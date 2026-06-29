#include "GridManager.h"

#include <algorithm>
#include <cmath>
#include <cwctype>
#include <limits>
#include <set>

namespace
{
std::wstring ToLower(std::wstring value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](wchar_t ch) {
        return static_cast<wchar_t>(std::towlower(ch));
    });
    return value;
}

int RoundToInt(double value)
{
    return static_cast<int>(std::llround(value));
}

bool RoughlyInside(const RECT& rect, POINT point, SIZE spacing)
{
    return point.x >= rect.left - spacing.cx && point.x <= rect.right + spacing.cx &&
           point.y >= rect.top - spacing.cy && point.y <= rect.bottom + spacing.cy;
}
}

POINT GridManager::FindNearestFreePosition(
    const RECT& workArea,
    SIZE iconSpacing,
    const std::vector<DesktopIconInfo>& icons,
    const ConfigData& config) const
{
    iconSpacing.cx = std::max<LONG>(iconSpacing.cx, 48);
    iconSpacing.cy = std::max<LONG>(iconSpacing.cy, 48);

    POINT desired {
        (workArea.left + workArea.right) / 2,
        (workArea.top + workArea.bottom) / 2
    };

    const auto mode = ToLower(config.moveMode);
    if (mode == L"custom" || mode == L"fixed" || mode == L"manual")
    {
        desired.x = config.centerX;
        desired.y = config.centerY;
    }

    bool hasBase = false;
    int baseX = workArea.left + std::max<LONG>(8, iconSpacing.cx / 4);
    int baseY = workArea.top + std::max<LONG>(8, iconSpacing.cy / 4);

    for (const auto& icon : icons)
    {
        if (!RoughlyInside(workArea, icon.position, iconSpacing))
        {
            continue;
        }

        if (!hasBase)
        {
            baseX = icon.position.x;
            baseY = icon.position.y;
            hasBase = true;
        }
        else
        {
            baseX = std::min(baseX, icon.position.x);
            baseY = std::min(baseY, icon.position.y);
        }
    }

    auto toCell = [&](POINT point) {
        return std::pair<int, int> {
            RoundToInt(static_cast<double>(point.x - baseX) / static_cast<double>(iconSpacing.cx)),
            RoundToInt(static_cast<double>(point.y - baseY) / static_cast<double>(iconSpacing.cy))
        };
    };

    auto toPoint = [&](int column, int row) {
        return POINT {
            baseX + column * iconSpacing.cx,
            baseY + row * iconSpacing.cy
        };
    };

    std::set<std::pair<int, int>> occupied;
    for (const auto& icon : icons)
    {
        if (icon.isTarget)
        {
            continue;
        }

        if (!RoughlyInside(workArea, icon.position, iconSpacing))
        {
            continue;
        }

        occupied.insert(toCell(icon.position));
    }

    const auto [centerColumn, centerRow] = toCell(desired);
    const int radius = std::clamp(config.gridSearchRadius, 1, 200);

    POINT fallback = toPoint(centerColumn, centerRow);
    if (!IsInsideWorkArea(workArea, fallback, iconSpacing))
    {
        fallback.x = std::clamp(fallback.x, workArea.left, std::max(workArea.left, workArea.right - 32));
        fallback.y = std::clamp(fallback.y, workArea.top, std::max(workArea.top, workArea.bottom - 32));
    }

    for (int currentRadius = 0; currentRadius <= radius; ++currentRadius)
    {
        bool found = false;
        POINT bestPoint = fallback;
        long long bestDistance = std::numeric_limits<long long>::max();

        for (int dx = -currentRadius; dx <= currentRadius; ++dx)
        {
            for (int dy = -currentRadius; dy <= currentRadius; ++dy)
            {
                if (std::max(std::abs(dx), std::abs(dy)) != currentRadius)
                {
                    continue;
                }

                const int column = centerColumn + dx;
                const int row = centerRow + dy;
                const auto cell = std::pair<int, int> { column, row };
                if (occupied.find(cell) != occupied.end())
                {
                    continue;
                }

                const POINT candidate = toPoint(column, row);
                if (!IsInsideWorkArea(workArea, candidate, iconSpacing))
                {
                    continue;
                }

                const long long distanceX = static_cast<long long>(candidate.x) - desired.x;
                const long long distanceY = static_cast<long long>(candidate.y) - desired.y;
                const long long distance = distanceX * distanceX + distanceY * distanceY;
                if (!found || distance < bestDistance)
                {
                    found = true;
                    bestDistance = distance;
                    bestPoint = candidate;
                }
            }
        }

        if (found)
        {
            return bestPoint;
        }
    }

    return fallback;
}

bool GridManager::IsInsideWorkArea(const RECT& workArea, POINT point, SIZE iconSpacing) noexcept
{
    const LONG safeWidth = std::min<LONG>(iconSpacing.cx, 64);
    const LONG safeHeight = std::min<LONG>(iconSpacing.cy, 64);
    return point.x >= workArea.left && point.y >= workArea.top &&
           point.x + safeWidth <= workArea.right && point.y + safeHeight <= workArea.bottom;
}

bool GridManager::SameCell(POINT a, POINT b, SIZE iconSpacing) noexcept
{
    iconSpacing.cx = std::max<LONG>(iconSpacing.cx, 48);
    iconSpacing.cy = std::max<LONG>(iconSpacing.cy, 48);
    return std::abs(a.x - b.x) < iconSpacing.cx / 2 && std::abs(a.y - b.y) < iconSpacing.cy / 2;
}
