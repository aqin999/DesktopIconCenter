#pragma once

#include <Windows.h>

#include <optional>
#include <string>

/*
桌面 ListView 辅助类说明：
01. DesktopListView 封装 SysListView32 桌面图标控件句柄。
02. 文档要求涉及 LVM_FINDITEM、LVM_GETITEMCOUNT、LVM_GETITEMPOSITION。
03. 该类集中处理这些消息，避免散落到业务代码中。
04. 对无指针参数的消息直接使用 SendMessageTimeout。
05. 对指针参数的消息使用 Explorer 进程内远程缓冲区。
06. 远程缓冲区通过 VirtualAllocEx 和 VirtualFreeEx RAII 管理。
07. 所有发送都设置超时，避免 Explorer 无响应时卡死程序。
08. GetItemCount 用于快速确认 ListView 是否可用。
09. GetIconSpacing 读取当前桌面图标网格间距。
10. FindItemByName 用 LVM_FINDITEMW 辅助定位新增图标。
11. GetItemPosition 用 LVM_GETITEMPOSITION 读取图标坐标。
12. SetItemPosition 用 LVM_SETITEMPOSITION 提供兼容移动路径。
13. 主流程优先使用 IFolderView 移动，ListView 作为观测和兜底。
14. 类不负责查找窗口，窗口来源由 ExplorerDesktop 提供。
15. 类不保存进程句柄，每次调用即时打开并关闭。
16. 这样可以适应 Explorer 重启和权限变化。
17. 类不使用裸 new/delete，所有系统资源都有析构释放。
18. 坐标均保持 Win32 POINT/SIZE 原始语义。
19. 若消息失败会返回空值或 false，由上层记录日志。
20. 该类让文档要求的 ListView 交互有清晰、可审计的实现。
*/
class DesktopListView
{
public:
    DesktopListView() = default;
    explicit DesktopListView(HWND hwnd);

    void Attach(HWND hwnd) noexcept;
    HWND Hwnd() const noexcept;
    bool IsValid() const noexcept;

    int GetItemCount() const;
    SIZE GetIconSpacing() const;
    std::optional<POINT> GetItemPosition(int index) const;
    int FindItemByName(const std::wstring& name) const;
    bool SetItemPosition(int index, POINT point) const;

private:
    bool Send(UINT message, WPARAM wParam, LPARAM lParam, DWORD_PTR* result) const;
    HANDLE OpenListViewProcess() const;

private:
    HWND hwnd_ = nullptr;
};
