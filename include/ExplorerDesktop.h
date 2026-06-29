#pragma once

#include <Windows.h>
#include <ShObjIdl.h>

/*
Explorer 桌面定位类说明：
01. ExplorerDesktop 负责定位 Explorer 管理的桌面窗口层级。
02. 主要查找 Progman、WorkerW、SHELLDLL_DefView、SysListView32。
03. Windows 10 和 Windows 11 都可能把 DefView 放在不同 WorkerW 下。
04. Refresh 会重新枚举窗口，用于 Explorer 重启后的恢复。
05. EnsureReady 会检查已有句柄是否仍然有效，无效时自动刷新。
06. GetListViewHwnd 返回桌面图标 ListView 句柄。
07. ListView 句柄用于读取间距、辅助定位和兼容旧方案。
08. 实际移动优先使用官方 Shell COM 的 IFolderView 接口。
09. GetDesktopFolderView 通过 IShellWindows 获取桌面 ShellView。
10. Shell COM 路径比盲目跨进程内存操作更稳定可维护。
11. 类不持有 COM 对象，只在调用时即时获取，避免 Explorer 重启后悬空。
12. 所有窗口句柄都只作为缓存，使用前必须 IsWindow 验证。
13. 查找窗口时不会发送创建 WorkerW 的未文档化消息。
14. 仅基于当前 Explorer 已存在的公开窗口类名做定位。
15. SendMessageTimeout 的使用放在 DesktopListView，避免 Explorer 卡死。
16. 类不依赖配置文件，也不直接写日志，保持低耦合。
17. 类不使用裸 new/delete，枚举上下文放在栈上。
18. 多显示器下桌面 ListView 仍由 Explorer 统一管理。
19. 后续如需指定显示器，可在 IconManager/GridManager 扩展。
20. 该类是程序与 Explorer 桌面视图交互的基础设施。
*/
class ExplorerDesktop
{
public:
    ExplorerDesktop() = default;

    bool Refresh();
    bool EnsureReady();

    HWND GetProgmanHwnd() const noexcept;
    HWND GetWorkerWHwnd() const noexcept;
    HWND GetDefViewHwnd() const noexcept;
    HWND GetListViewHwnd() const noexcept;

    HRESULT GetDesktopFolderView(IFolderView2** folderView) const;

private:
    static HWND FindDefViewInWindow(HWND parent);

private:
    HWND progman_ = nullptr;
    HWND workerw_ = nullptr;
    HWND defView_ = nullptr;
    HWND listView_ = nullptr;
};
