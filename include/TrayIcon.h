#pragma once

#include <Windows.h>
#include <Shellapi.h>

#include <string>

/*
托盘图标类说明：
01. TrayIcon 负责系统托盘图标、提示文本和右键菜单。
02. 后台工具不显示主窗口，托盘是用户主要交互入口。
03. 菜单包含暂停监听、恢复监听、开机启动、打开配置、查看日志、退出。
04. 菜单命令通过 WM_COMMAND 发送给 App 的隐藏窗口。
05. 类内部只保存 NOTIFYICONDATAW 和状态位。
06. Create 会添加托盘图标并设置 NOTIFYICON_VERSION_4。
07. Destroy 会从系统托盘移除图标，避免退出后残留。
08. Recreate 用于 Explorer/任务栏重启后的自动恢复。
09. ShowMenu 每次动态创建菜单，确保状态勾选即时更新。
10. AutoStart 状态以菜单勾选显示。
11. Paused 状态决定显示“暂停监听”还是“恢复监听”。
12. 托盘提示文本会显示当前运行或暂停状态。
13. 图标优先加载资源 app.ico，失败时使用系统默认图标。
14. 类不直接修改配置，不直接启动或停止监听。
15. 业务处理全部交给 App，保持 UI 层职责清晰。
16. 类不使用裸 new/delete，菜单句柄创建后立即销毁。
17. 所有字符串均为 Unicode 中文文本。
18. TrackPopupMenu 前调用 SetForegroundWindow，避免菜单不消失。
19. Shell_NotifyIcon 失败时返回 false，由上层记录日志。
20. 该类满足文档中托盘驻留和右键菜单需求。
*/
class TrayIcon
{
public:
    static constexpr UINT MessageId = WM_APP + 10;

    TrayIcon() = default;
    ~TrayIcon();

    TrayIcon(const TrayIcon&) = delete;
    TrayIcon& operator=(const TrayIcon&) = delete;

    bool Create(HWND ownerWindow, HINSTANCE instance, bool paused, bool autoStart);
    void Destroy();
    bool Recreate();

    void SetPaused(bool paused);
    void SetAutoStart(bool autoStart);
    void ShowMenu(POINT screenPoint) const;

private:
    void UpdateTip();

private:
    NOTIFYICONDATAW data_ {};
    HINSTANCE instance_ = nullptr;
    bool added_ = false;
    bool paused_ = false;
    bool autoStart_ = false;
};
