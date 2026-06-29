#include "TrayIcon.h"

#include "resource.h"
#include "Version.h"

#include <iterator>
#include <strsafe.h>

TrayIcon::~TrayIcon()
{
    Destroy();
}

bool TrayIcon::Create(HWND ownerWindow, HINSTANCE instance, bool paused, bool autoStart)
{
    Destroy();

    instance_ = instance;
    paused_ = paused;
    autoStart_ = autoStart;

    data_ = NOTIFYICONDATAW {};
    data_.cbSize = sizeof(data_);
    data_.hWnd = ownerWindow;
    data_.uID = 1;
    data_.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP | NIF_SHOWTIP;
    data_.uCallbackMessage = MessageId;
    data_.hIcon = static_cast<HICON>(LoadImageW(instance_, MAKEINTRESOURCEW(IDI_APP_ICON), IMAGE_ICON, 32, 32, LR_DEFAULTCOLOR | LR_SHARED));
    if (data_.hIcon == nullptr)
    {
        data_.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    }
    UpdateTip();

    if (!Shell_NotifyIconW(NIM_ADD, &data_))
    {
        return false;
    }

    data_.uVersion = NOTIFYICON_VERSION_4;
    Shell_NotifyIconW(NIM_SETVERSION, &data_);
    added_ = true;
    return true;
}

void TrayIcon::Destroy()
{
    if (added_)
    {
        Shell_NotifyIconW(NIM_DELETE, &data_);
        added_ = false;
    }
}

bool TrayIcon::Recreate()
{
    if (data_.hWnd == nullptr)
    {
        return false;
    }
    return Create(data_.hWnd, instance_, paused_, autoStart_);
}

void TrayIcon::SetPaused(bool paused)
{
    paused_ = paused;
    UpdateTip();
    if (added_)
    {
        Shell_NotifyIconW(NIM_MODIFY, &data_);
    }
}

void TrayIcon::SetAutoStart(bool autoStart)
{
    autoStart_ = autoStart;
}

void TrayIcon::ShowMenu(POINT screenPoint) const
{
    HMENU menu = CreatePopupMenu();
    if (menu == nullptr)
    {
        return;
    }

    AppendMenuW(menu, MF_STRING | MF_DISABLED, 0, L"DesktopIconCenter v" DESKTOP_ICON_CENTER_VERSION);
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);

    if (paused_)
    {
        AppendMenuW(menu, MF_STRING, ID_TRAY_RESUME, L"恢复监听");
    }
    else
    {
        AppendMenuW(menu, MF_STRING, ID_TRAY_PAUSE, L"暂停监听");
    }

    AppendMenuW(menu, MF_STRING, ID_TRAY_SETTINGS, L"设置...");
    AppendMenuW(menu, MF_STRING | (autoStart_ ? MF_CHECKED : MF_UNCHECKED), ID_TRAY_AUTOSTART, L"开机启动");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, ID_TRAY_OPEN_CONFIG, L"打开配置");
    AppendMenuW(menu, MF_STRING, ID_TRAY_RELOAD_CONFIG, L"重新加载配置");
    AppendMenuW(menu, MF_STRING, ID_TRAY_OPEN_LOGS, L"查看日志");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, ID_TRAY_EXIT, L"退出 DesktopIconCenter");
    SetMenuDefaultItem(menu, ID_TRAY_EXIT, FALSE);

    SetForegroundWindow(data_.hWnd);
    TrackPopupMenu(menu, TPM_RIGHTBUTTON | TPM_BOTTOMALIGN, screenPoint.x, screenPoint.y, 0, data_.hWnd, nullptr);
    PostMessageW(data_.hWnd, WM_NULL, 0, 0);
    DestroyMenu(menu);
}

void TrayIcon::UpdateTip()
{
    const wchar_t* state = paused_ ? L"已暂停" : L"正在监听桌面";
    std::wstring tip = L"DesktopIconCenter v" DESKTOP_ICON_CENTER_VERSION L" - ";
    tip += state;
    StringCchCopyW(data_.szTip, std::size(data_.szTip), tip.c_str());
}
