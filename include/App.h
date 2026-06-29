#pragma once

#include <Windows.h>

#include <filesystem>
#include <memory>
#include <string>

#include "Config.h"
#include "DesktopWatcher.h"
#include "ExplorerDesktop.h"
#include "IconManager.h"
#include "Logger.h"
#include "TrayIcon.h"

/*
应用主类说明：
01. App 是 DesktopIconCenter 的生命周期和消息循环管理者。
02. 它创建隐藏窗口，不显示任何主界面，符合后台运行要求。
03. 它初始化 COM、公共控件、配置、日志、托盘和桌面监听。
04. 它拥有 Config、Logger、DesktopWatcher、ExplorerDesktop、IconManager、TrayIcon。
05. 桌面监听线程检测到文件后，会把事件投递到隐藏窗口。
06. UI 线程收到事件后延迟指定毫秒，再调用 IconManager 移动图标。
07. 这样 Shell COM 操作都发生在已初始化 COM 的 UI 线程。
08. App 负责按配置忽略 desktop.ini、Thumbs.db、文件夹和快捷方式。
09. App 负责菜单命令：暂停、恢复、开机启动、打开配置、查看日志、退出。
10. App 负责保存 Enable 和 AutoStart 配置，实现无需重启 Reload。
11. 每次处理新增事件前都会 Reload 配置，确保修改 config.ini 生效。
12. 任务栏重启后，App 会重新添加托盘图标并刷新 Explorer 句柄。
13. Explorer 重启或桌面视图变化时，IconManager 每次都会重新获取视图。
14. App 不保存全局变量，窗口指针通过 GWLP_USERDATA 关联。
15. 所有成员资源在 Shutdown 或析构时按顺序释放。
16. 类不使用裸 new/delete，IconManager 通过 unique_ptr 管理。
17. 失败路径均写入日志，不弹出打扰用户的错误框。
18. 消息循环保持轻量，CPU 在空闲时为 0。
19. 当前设计支持单 EXE 直接运行，也支持 Visual Studio/CMake 构建。
20. 该类把文档中的后台工具行为串联成完整可运行程序。
*/
class App
{
public:
    App();
    ~App();

    App(const App&) = delete;
    App& operator=(const App&) = delete;

    bool Initialize(HINSTANCE instance);
    int Run();
    void Shutdown();

private:
    struct PostedDesktopEvent;

    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK SettingsWindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

    bool RegisterHiddenWindowClass();
    bool CreateHiddenWindow();
    bool StartWatcherIfNeeded();
    void StopWatcher();
    void PostDesktopEvent(const DesktopChangeEvent& event);
    void ProcessDesktopEvent(const DesktopChangeEvent& event);
    void HandleTrayCommand(UINT commandId);
    void OpenConfigFile();
    void OpenLogsFolder();
    void ShowSettingsDialog();
    void ApplyAutoStartSetting(bool enable);
    void ToggleAutoStart();
    void SetPaused(bool paused);
    std::filesystem::path GetDesktopPath() const;
    bool ShouldIgnoreEvent(const DesktopChangeEvent& event, const ConfigData& config) const;

private:
    static constexpr UINT DesktopEventMessage = WM_APP + 20;

    HINSTANCE instance_ = nullptr;
    HWND window_ = nullptr;
    UINT taskbarCreatedMessage_ = 0;
    bool comInitialized_ = false;
    bool shuttingDown_ = false;
    bool paused_ = false;

    Config config_;
    Logger logger_;
    DesktopWatcher watcher_;
    ExplorerDesktop explorerDesktop_;
    std::unique_ptr<IconManager> iconManager_;
    TrayIcon trayIcon_;
};
