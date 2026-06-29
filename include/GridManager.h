#pragma once

#include <Windows.h>

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "Config.h"

struct DesktopIconInfo
{
    std::wstring name;
    std::filesystem::path parsingPath;
    POINT position { 0, 0 };
    bool isTarget = false;
};

/*
网格管理类说明：
01. GridManager 负责把“屏幕中心”转换为可用桌面图标网格位置。
02. 桌面图标不能直接固定到 960,540，否则会覆盖已有图标。
03. 类会根据当前图标间距建立逻辑网格。
04. 网格基准优先从已有图标坐标推导，尽量贴合 Explorer 排列。
05. 如果桌面没有可用图标，则使用显示器工作区左上角作为基准。
06. 搜索目标默认是主显示器工作区中心。
07. 当 MoveMode 为 Custom/Fixed/Manual 时可使用 CenterX/CenterY。
08. 已有图标会映射到占用单元，目标图标自身会被排除。
09. 搜索方式是按半径扩散的螺旋/环形搜索。
10. 同一半径内选择距离目标中心最近的候选单元。
11. 搜索半径由 GridSearchRadius 控制，并做安全限制。
12. 结果坐标会限制在显示器工作区内，避免图标跑到任务栏区域。
13. 坐标单位保持与 Explorer ListView/IFolderView 一致的像素坐标。
14. 类不访问 Explorer，也不依赖 Win32 消息，便于独立测试。
15. 类不修改输入图标列表，函数保持无副作用。
16. 类不使用裸 new/delete，容器由标准库管理。
17. 该算法兼容多分辨率和多显示器的后续扩展。
18. 当前版本默认以主显示器为中心点来源。
19. 若搜索失败，会返回目标中心附近的网格点作为最后兜底。
20. 该类是避免覆盖已有桌面图标的核心实现。
*/
class GridManager
{
public:
    POINT FindNearestFreePosition(
        const RECT& workArea,
        SIZE iconSpacing,
        const std::vector<DesktopIconInfo>& icons,
        const ConfigData& config) const;

private:
    static bool IsInsideWorkArea(const RECT& workArea, POINT point, SIZE iconSpacing) noexcept;
    static bool SameCell(POINT a, POINT b, SIZE iconSpacing) noexcept;
};
