#pragma once

#include <Windows.h>

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "Config.h"
#include "ExplorerDesktop.h"
#include "GridManager.h"
#include "Logger.h"

struct MoveResult
{
    bool success = false;
    std::wstring message;
    POINT requestedPoint { 0, 0 };
};

/*
图标管理类说明：
01. IconManager 负责定位新增桌面图标并移动到中心空位。
02. 类同时使用 ExplorerDesktop、DesktopListView 和 Shell IFolderView。
03. ListView 用于满足 LVM_FINDITEM 等文档要求并读取网格间距。
04. IFolderView 用于官方支持的枚举、读取位置和移动图标。
05. 新增图标定位优先按文件系统解析路径的文件名匹配。
06. 当 Explorer 隐藏扩展名时，解析路径仍可包含真实文件名。
07. 定位失败会按 200ms、500ms、1000ms 等间隔重试。
08. 重试最多 5 次，避免无限等待。
09. 每次重试都会重新获取 ShellView，以兼容 Explorer 重启。
10. 枚举时读取所有桌面图标的名称、解析路径和坐标。
11. 移动前调用 GridManager 计算中央附近最近空位。
12. 移动只针对目标图标，不改变其它图标位置。
13. 成功和失败原因会返回给 App 写日志。
14. 类要求调用线程已经初始化 COM，App 在 UI 线程完成该操作。
15. 类不弹窗、不阻塞消息循环超过必要的短重试时间。
16. 类不使用裸 new/delete，PIDL 用 unique_ptr 自定义释放器管理。
17. 类对 Explorer 句柄失效有容错，失败后可等待下一次事件恢复。
18. 多显示器中心计算使用主显示器工作区，后续可扩展配置。
19. 若 Shell COM 失败，不会尝试不稳定的未验证 Hack。
20. 该类是实现“新增图标自动移到中央空位”的业务核心。
*/
class IconManager
{
public:
    IconManager(ExplorerDesktop& explorerDesktop, Logger& logger);

    MoveResult MoveNewIconToCenter(const std::filesystem::path& filePath, const ConfigData& config);

private:
    struct ShellIconInfo;

    bool EnumerateIcons(IFolderView2* folderView, std::vector<ShellIconInfo>& icons) const;
    std::optional<size_t> FindIconIndex(const std::vector<ShellIconInfo>& icons, const std::filesystem::path& filePath) const;
    SIZE DetectIconSpacing() const;
    RECT GetPrimaryWorkArea() const;
    static bool FileNameEquals(const std::wstring& left, const std::wstring& right);
    static std::wstring HResultText(HRESULT hr);

private:
    ExplorerDesktop& explorerDesktop_;
    Logger& logger_;
    GridManager gridManager_;
};
