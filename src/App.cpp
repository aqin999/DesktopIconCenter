#include "App.h"

#include "resource.h"
#include "Version.h"

#include <CommCtrl.h>
#include <ShlObj.h>
#include <Shellapi.h>

#include <algorithm>
#include <cwctype>

struct App::PostedDesktopEvent
{
    DesktopChangeEvent event;
};

namespace
{
const wchar_t* HiddenWindowClassName()
{
    return L"DesktopIconCenterHiddenWindow";
}

const wchar_t* SettingsWindowClassName()
{
    return L"DesktopIconCenterSettingsWindow";
}

constexpr int SettingsAutoStartCheckboxId = 51001;

struct SettingsDialogState
{
    App* app = nullptr;
    HWND checkbox = nullptr;
    bool done = false;
};

std::wstring ToLower(std::wstring value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](wchar_t ch) {
        return static_cast<wchar_t>(std::towlower(ch));
    });
    return value;
}

bool IsShellExecuteSuccess(HINSTANCE instance)
{
    return reinterpret_cast<INT_PTR>(instance) > 32;
}

void SetDefaultGuiFont(HWND hwnd)
{
    SendMessageW(hwnd, WM_SETFONT, reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)), TRUE);
}
}

App::App() = default;

App::~App()
{
    Shutdown();
}

bool App::Initialize(HINSTANCE instance)
{
    instance_ = instance;
    logger_.Initialize(Config::GetExeDirectory() / L"logs");
    logger_.Info(L"程序启动");

    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (SUCCEEDED(hr))
    {
        comInitialized_ = true;
    }
    else if (hr == RPC_E_CHANGED_MODE)
    {
        logger_.Warn(L"COM 已由外部初始化为其它模式，继续尝试运行");
    }
    else
    {
        logger_.Error(L"COM 初始化失败");
        return false;
    }

    INITCOMMONCONTROLSEX controls {};
    controls.dwSize = sizeof(controls);
    controls.dwICC = ICC_LISTVIEW_CLASSES;
    InitCommonControlsEx(&controls);

    config_.Load();
    paused_ = !config_.Data().enable;
    if (!config_.ApplyAutoStart(config_.Data().autoStart))
    {
        logger_.Warn(L"同步开机启动配置失败");
    }

    taskbarCreatedMessage_ = RegisterWindowMessageW(L"TaskbarCreated");

    if (!RegisterHiddenWindowClass() || !CreateHiddenWindow())
    {
        logger_.Error(L"创建隐藏窗口失败");
        return false;
    }

    iconManager_ = std::make_unique<IconManager>(explorerDesktop_, logger_);
    if (!explorerDesktop_.Refresh())
    {
        logger_.Warn(L"启动时未能定位桌面 ListView，后续事件会自动重试");
    }

    if (!trayIcon_.Create(window_, instance_, paused_, config_.Data().autoStart))
    {
        logger_.Warn(L"创建系统托盘图标失败");
    }

    StartWatcherIfNeeded();
    return true;
}

int App::Run()
{
    MSG message {};
    while (GetMessageW(&message, nullptr, 0, 0) > 0)
    {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    return static_cast<int>(message.wParam);
}

void App::Shutdown()
{
    if (shuttingDown_)
    {
        return;
    }
    shuttingDown_ = true;

    StopWatcher();
    trayIcon_.Destroy();
    iconManager_.reset();

    if (window_ != nullptr && IsWindow(window_))
    {
        HWND window = window_;
        window_ = nullptr;
        DestroyWindow(window);
    }

    if (comInitialized_)
    {
        CoUninitialize();
        comInitialized_ = false;
    }

    logger_.Info(L"程序退出");
}

LRESULT CALLBACK App::WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    App* app = nullptr;

    if (message == WM_NCCREATE)
    {
        const auto* createStruct = reinterpret_cast<CREATESTRUCTW*>(lParam);
        app = static_cast<App*>(createStruct->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app));
        app->window_ = hwnd;
    }
    else
    {
        app = reinterpret_cast<App*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }

    if (app != nullptr)
    {
        return app->HandleMessage(hwnd, message, wParam, lParam);
    }

    return DefWindowProcW(hwnd, message, wParam, lParam);
}

LRESULT CALLBACK App::SettingsWindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    auto* state = reinterpret_cast<SettingsDialogState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

    if (message == WM_NCCREATE)
    {
        const auto* createStruct = reinterpret_cast<CREATESTRUCTW*>(lParam);
        state = static_cast<SettingsDialogState*>(createStruct->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));
    }

    switch (message)
    {
    case WM_CREATE:
    {
        if (state == nullptr || state->app == nullptr)
        {
            return -1;
        }

        HWND title = CreateWindowExW(
            0,
            L"STATIC",
            L"启动设置",
            WS_CHILD | WS_VISIBLE,
            22,
            18,
            360,
            24,
            hwnd,
            nullptr,
            state->app->instance_,
            nullptr);
        SetDefaultGuiFont(title);

        state->checkbox = CreateWindowExW(
            0,
            L"BUTTON",
            L"随 Windows 开机自动启动 DesktopIconCenter",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX,
            22,
            52,
            360,
            28,
            hwnd,
            reinterpret_cast<HMENU>(SettingsAutoStartCheckboxId),
            state->app->instance_,
            nullptr);
        SetDefaultGuiFont(state->checkbox);
        SendMessageW(
            state->checkbox,
            BM_SETCHECK,
            state->app->config_.Data().autoStart ? BST_CHECKED : BST_UNCHECKED,
            0);

        HWND saveButton = CreateWindowExW(
            0,
            L"BUTTON",
            L"保存",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
            210,
            104,
            78,
            28,
            hwnd,
            reinterpret_cast<HMENU>(IDOK),
            state->app->instance_,
            nullptr);
        SetDefaultGuiFont(saveButton);

        HWND cancelButton = CreateWindowExW(
            0,
            L"BUTTON",
            L"取消",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
            300,
            104,
            78,
            28,
            hwnd,
            reinterpret_cast<HMENU>(IDCANCEL),
            state->app->instance_,
            nullptr);
        SetDefaultGuiFont(cancelButton);

        return 0;
    }

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK)
        {
            if (state != nullptr && state->app != nullptr)
            {
                const bool enable = SendDlgItemMessageW(hwnd, SettingsAutoStartCheckboxId, BM_GETCHECK, 0, 0) == BST_CHECKED;
                state->app->ApplyAutoStartSetting(enable);
            }
            DestroyWindow(hwnd);
            return 0;
        }

        if (LOWORD(wParam) == IDCANCEL)
        {
            DestroyWindow(hwnd);
            return 0;
        }
        break;

    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;

    case WM_DESTROY:
        if (state != nullptr)
        {
            state->done = true;
        }
        return 0;

    default:
        break;
    }

    return DefWindowProcW(hwnd, message, wParam, lParam);
}

LRESULT App::HandleMessage(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (taskbarCreatedMessage_ != 0 && message == taskbarCreatedMessage_)
    {
        trayIcon_.Recreate();
        explorerDesktop_.Refresh();
        StartWatcherIfNeeded();
        logger_.Info(L"检测到任务栏或 Explorer 重启，已恢复托盘和桌面句柄");
        return 0;
    }

    if (message == TrayIcon::MessageId)
    {
        const UINT trayEvent = LOWORD(lParam);
        if (trayEvent == WM_RBUTTONUP || trayEvent == WM_CONTEXTMENU || trayEvent == NIN_KEYSELECT)
        {
            POINT point {};
            GetCursorPos(&point);
            trayIcon_.ShowMenu(point);
            return 0;
        }

        if (trayEvent == WM_LBUTTONDBLCLK)
        {
            OpenConfigFile();
            return 0;
        }
    }

    switch (message)
    {
    case WM_COMMAND:
        HandleTrayCommand(LOWORD(wParam));
        return 0;

    case DesktopEventMessage:
    {
        std::unique_ptr<PostedDesktopEvent> posted(reinterpret_cast<PostedDesktopEvent*>(lParam));
        if (posted)
        {
            ProcessDesktopEvent(posted->event);
        }
        return 0;
    }

    case WM_DESTROY:
        window_ = nullptr;
        Shutdown();
        PostQuitMessage(0);
        return 0;

    default:
        break;
    }

    return DefWindowProcW(hwnd, message, wParam, lParam);
}

bool App::RegisterHiddenWindowClass()
{
    WNDCLASSEXW windowClass {};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.lpfnWndProc = App::WindowProc;
    windowClass.hInstance = instance_;
    windowClass.hIcon = LoadIconW(instance_, MAKEINTRESOURCEW(IDI_APP_ICON));
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.lpszClassName = HiddenWindowClassName();

    const ATOM atom = RegisterClassExW(&windowClass);
    if (atom != 0)
    {
        return true;
    }

    return GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
}

bool App::CreateHiddenWindow()
{
    window_ = CreateWindowExW(
        0,
        HiddenWindowClassName(),
        L"DesktopIconCenter",
        WS_OVERLAPPED,
        0,
        0,
        0,
        0,
        nullptr,
        nullptr,
        instance_,
        this);

    return window_ != nullptr;
}

bool App::StartWatcherIfNeeded()
{
    if (paused_ || !config_.Data().enable)
    {
        trayIcon_.SetPaused(true);
        return false;
    }

    if (watcher_.IsRunning())
    {
        return true;
    }

    const auto desktopPath = GetDesktopPath();
    if (desktopPath.empty())
    {
        logger_.Error(L"无法获取当前用户桌面路径");
        return false;
    }

    const bool ok = watcher_.Start(desktopPath, [this](const DesktopChangeEvent& changeEvent) {
        PostDesktopEvent(changeEvent);
    });

    if (ok)
    {
        logger_.Info(L"监听开始: " + desktopPath.wstring());
    }
    else
    {
        logger_.Error(L"监听启动失败: " + desktopPath.wstring());
    }
    return ok;
}

void App::StopWatcher()
{
    if (watcher_.IsRunning())
    {
        watcher_.Stop();
        logger_.Info(L"监听停止");
    }
}

void App::PostDesktopEvent(const DesktopChangeEvent& changeEvent)
{
    if (shuttingDown_ || window_ == nullptr)
    {
        return;
    }

    auto posted = std::make_unique<PostedDesktopEvent>();
    posted->event = changeEvent;

    if (!PostMessageW(window_, DesktopEventMessage, 0, reinterpret_cast<LPARAM>(posted.get())))
    {
        logger_.Warn(L"投递桌面事件失败: " + changeEvent.fileName);
        return;
    }

    posted.release();
}

void App::ProcessDesktopEvent(const DesktopChangeEvent& changeEvent)
{
    config_.Reload();
    const auto& data = config_.Data();
    trayIcon_.SetAutoStart(data.autoStart);

    if (!data.enable)
    {
        paused_ = true;
        trayIcon_.SetPaused(true);
        StopWatcher();
        return;
    }

    if (paused_)
    {
        return;
    }

    if (ShouldIgnoreEvent(changeEvent, data))
    {
        return;
    }

    logger_.Info(L"检测新文件: " + changeEvent.fullPath.wstring());

    const DWORD delay = std::min<DWORD>(data.delay, 10000);
    if (delay > 0)
    {
        Sleep(delay);
    }

    if (!iconManager_)
    {
        logger_.Error(L"图标管理器未初始化");
        return;
    }

    const MoveResult result = iconManager_->MoveNewIconToCenter(changeEvent.fullPath, data);
    if (result.success)
    {
        logger_.Info(result.message);
    }
    else
    {
        logger_.Error(L"移动失败: " + result.message);
    }
}

void App::HandleTrayCommand(UINT commandId)
{
    switch (commandId)
    {
    case ID_TRAY_PAUSE:
        SetPaused(true);
        break;

    case ID_TRAY_RESUME:
        SetPaused(false);
        break;

    case ID_TRAY_AUTOSTART:
        ToggleAutoStart();
        break;

    case ID_TRAY_SETTINGS:
        ShowSettingsDialog();
        break;

    case ID_TRAY_OPEN_CONFIG:
        OpenConfigFile();
        break;

    case ID_TRAY_OPEN_LOGS:
        OpenLogsFolder();
        break;

    case ID_TRAY_RELOAD_CONFIG:
        config_.Reload();
        paused_ = !config_.Data().enable;
        trayIcon_.SetPaused(paused_);
        trayIcon_.SetAutoStart(config_.Data().autoStart);
        config_.ApplyAutoStart(config_.Data().autoStart);
        if (paused_)
        {
            StopWatcher();
        }
        else
        {
            StartWatcherIfNeeded();
        }
        logger_.Info(L"配置已重新加载");
        break;

    case ID_TRAY_EXIT:
        logger_.Info(L"收到退出命令");
        if (window_ != nullptr)
        {
            DestroyWindow(window_);
        }
        else
        {
            Shutdown();
            PostQuitMessage(0);
        }
        break;

    default:
        break;
    }
}

void App::OpenConfigFile()
{
    config_.Save();
    const auto path = config_.Path().wstring();
    const auto parameters = L"\"" + path + L"\"";

    HINSTANCE result = ShellExecuteW(window_, L"open", L"notepad.exe", parameters.c_str(), nullptr, SW_SHOWNORMAL);
    if (!IsShellExecuteSuccess(result))
    {
        ShellExecuteW(window_, L"open", path.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    }
}

void App::OpenLogsFolder()
{
    const auto logDirectory = Config::GetExeDirectory() / L"logs";
    try
    {
        std::filesystem::create_directories(logDirectory);
    }
    catch (...)
    {
    }

    ShellExecuteW(window_, L"open", logDirectory.wstring().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

void App::ShowSettingsDialog()
{
    config_.Reload();
    trayIcon_.SetAutoStart(config_.Data().autoStart);

    WNDCLASSEXW windowClass {};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.lpfnWndProc = App::SettingsWindowProc;
    windowClass.hInstance = instance_;
    windowClass.hIcon = LoadIconW(instance_, MAKEINTRESOURCEW(IDI_APP_ICON));
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    windowClass.lpszClassName = SettingsWindowClassName();
    RegisterClassExW(&windowClass);

    constexpr int width = 420;
    constexpr int height = 176;
    RECT workArea {};
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &workArea, 0);
    const int x = workArea.left + ((workArea.right - workArea.left) - width) / 2;
    const int y = workArea.top + ((workArea.bottom - workArea.top) - height) / 2;

    SettingsDialogState state;
    state.app = this;

    HWND dialog = CreateWindowExW(
        WS_EX_DLGMODALFRAME | WS_EX_TOPMOST,
        SettingsWindowClassName(),
            L"DesktopIconCenter v" DESKTOP_ICON_CENTER_VERSION L" 设置",
        WS_CAPTION | WS_SYSMENU | WS_POPUP,
        x,
        y,
        width,
        height,
        window_,
        nullptr,
        instance_,
        &state);

    if (dialog == nullptr)
    {
        logger_.Error(L"创建设置窗口失败");
        return;
    }

    if (window_ != nullptr)
    {
        EnableWindow(window_, FALSE);
    }

    ShowWindow(dialog, SW_SHOWNORMAL);
    UpdateWindow(dialog);

    MSG message {};
    while (!state.done && GetMessageW(&message, nullptr, 0, 0) > 0)
    {
        if (!IsDialogMessageW(dialog, &message))
        {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
    }

    if (window_ != nullptr)
    {
        EnableWindow(window_, TRUE);
        SetForegroundWindow(window_);
    }
}

void App::ApplyAutoStartSetting(bool enable)
{
    config_.Reload();
    config_.SetAutoStart(enable);

    if (config_.ApplyAutoStart(enable))
    {
        config_.Save();
        trayIcon_.SetAutoStart(enable);
        logger_.Info(enable ? L"设置窗口：已启用开机启动" : L"设置窗口：已关闭开机启动");
        return;
    }

    logger_.Error(L"设置窗口：修改开机启动失败");
    MessageBoxW(window_, L"修改开机启动失败，请查看日志。", L"DesktopIconCenter", MB_OK | MB_ICONERROR);
}

void App::ToggleAutoStart()
{
    config_.Reload();
    const bool newValue = !config_.Data().autoStart;
    config_.SetAutoStart(newValue);

    if (config_.ApplyAutoStart(newValue))
    {
        config_.Save();
        trayIcon_.SetAutoStart(newValue);
        logger_.Info(newValue ? L"已启用开机启动" : L"已关闭开机启动");
    }
    else
    {
        logger_.Error(L"修改开机启动失败");
    }
}

void App::SetPaused(bool paused)
{
    paused_ = paused;
    config_.SetEnable(!paused);
    config_.Save();
    trayIcon_.SetPaused(paused);

    if (paused)
    {
        StopWatcher();
        logger_.Info(L"已暂停监听");
    }
    else
    {
        logger_.Info(L"已恢复监听");
        StartWatcherIfNeeded();
    }
}

std::filesystem::path App::GetDesktopPath() const
{
    PWSTR path = nullptr;
    const HRESULT hr = SHGetKnownFolderPath(FOLDERID_Desktop, KF_FLAG_DEFAULT, nullptr, &path);
    if (FAILED(hr) || path == nullptr)
    {
        return {};
    }

    std::filesystem::path result(path);
    CoTaskMemFree(path);
    return result;
}

bool App::ShouldIgnoreEvent(const DesktopChangeEvent& changeEvent, const ConfigData& config) const
{
    if (changeEvent.action != FILE_ACTION_ADDED && changeEvent.action != FILE_ACTION_RENAMED_NEW_NAME)
    {
        return true;
    }

    const auto lowerName = ToLower(changeEvent.fileName);
    if (lowerName == L"desktop.ini" || lowerName == L"thumbs.db")
    {
        return true;
    }

    if (config.ignoreFolders && changeEvent.isDirectory)
    {
        return true;
    }

    if (config.ignoreShortcut && ToLower(changeEvent.fullPath.extension().wstring()) == L".lnk")
    {
        return true;
    }

    return false;
}
