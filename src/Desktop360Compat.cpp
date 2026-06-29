#include "Desktop360Compat.h"

#include <ShlObj.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cwchar>
#include <cwctype>
#include <fstream>
#include <iterator>
#include <limits>
#include <optional>
#include <set>
#include <vector>

namespace
{
constexpr int Desktop360GroupId = 60000000;
constexpr int CellWidth = 76;
constexpr int CellHeight = 104;

struct ParsedValue
{
    wchar_t type = 0;
    int intValue = 0;
    std::wstring stringValue;
    size_t offset = 0;
    size_t length = 0;
};

struct ParsedStruct
{
    size_t offset = 0;
    size_t payloadOffset = 0;
    size_t payloadEnd = 0;
    std::vector<ParsedValue> scalarValues;
    std::vector<ParsedStruct> children;
};

std::wstring ToLower(std::wstring value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](wchar_t ch) {
        return static_cast<wchar_t>(std::towlower(ch));
    });
    return value;
}

std::wstring ReadUtf16Le(const std::vector<unsigned char>& data, size_t offset, size_t length)
{
    std::wstring result;
    result.reserve(length / sizeof(wchar_t));
    for (size_t i = 0; i + 1 < length; i += 2)
    {
        const auto ch = static_cast<wchar_t>(data[offset + i] | (data[offset + i + 1] << 8));
        result.push_back(ch);
    }
    return result;
}

uint32_t ReadUInt32Le(const std::vector<unsigned char>& data, size_t offset)
{
    return static_cast<uint32_t>(data[offset]) |
        (static_cast<uint32_t>(data[offset + 1]) << 8) |
        (static_cast<uint32_t>(data[offset + 2]) << 16) |
        (static_cast<uint32_t>(data[offset + 3]) << 24);
}

bool ParseStruct(const std::vector<unsigned char>& data, size_t offset, size_t end, ParsedStruct& parsed)
{
    if (offset + 5 > end || data[offset] != 'S')
    {
        return false;
    }

    const size_t payloadLength = ReadUInt32Le(data, offset + 1);
    const size_t payloadOffset = offset + 5;
    const size_t payloadEnd = payloadOffset + payloadLength;
    if (payloadEnd > end)
    {
        return false;
    }

    parsed = ParsedStruct {};
    parsed.offset = offset;
    parsed.payloadOffset = payloadOffset;
    parsed.payloadEnd = payloadEnd;

    size_t cursor = payloadOffset;
    while (cursor < payloadEnd)
    {
        if (cursor + 5 > payloadEnd)
        {
            return false;
        }

        const auto type = static_cast<wchar_t>(data[cursor]);
        const size_t valueLength = ReadUInt32Le(data, cursor + 1);
        const size_t valueOffset = cursor + 5;
        const size_t valueEnd = valueOffset + valueLength;
        if (valueEnd > payloadEnd)
        {
            return false;
        }

        if (type == L'S')
        {
            ParsedStruct child;
            if (!ParseStruct(data, cursor, payloadEnd, child))
            {
                return false;
            }
            parsed.children.push_back(std::move(child));
        }
        else
        {
            ParsedValue value;
            value.type = type;
            value.offset = cursor;
            value.length = valueLength;
            if (type == L'd' && valueLength == 4)
            {
                value.intValue = static_cast<int>(ReadUInt32Le(data, valueOffset));
            }
            else if (type == L's')
            {
                value.stringValue = ReadUtf16Le(data, valueOffset, valueLength);
            }
            parsed.scalarValues.push_back(std::move(value));
        }

        cursor = valueEnd;
    }

    return true;
}

std::optional<int> IntAt(const ParsedStruct& parsed, size_t index)
{
    size_t current = 0;
    for (const auto& value : parsed.scalarValues)
    {
        if (value.type != L'd')
        {
            continue;
        }
        if (current == index)
        {
            return value.intValue;
        }
        ++current;
    }
    return std::nullopt;
}

std::optional<std::wstring> StringAt(const ParsedStruct& parsed, size_t index)
{
    size_t current = 0;
    for (const auto& value : parsed.scalarValues)
    {
        if (value.type != L's')
        {
            continue;
        }
        if (current == index)
        {
            return value.stringValue;
        }
        ++current;
    }
    return std::nullopt;
}

RECT GetWorkArea()
{
    RECT workArea {};
    if (!SystemParametersInfoW(SPI_GETWORKAREA, 0, &workArea, 0))
    {
        workArea.left = 0;
        workArea.top = 0;
        workArea.right = GetSystemMetrics(SM_CXSCREEN);
        workArea.bottom = GetSystemMetrics(SM_CYSCREEN);
    }
    return workArea;
}

bool IsShellExecuteVisible(HWND hwnd)
{
    if (hwnd == nullptr || !IsWindow(hwnd) || !IsWindowVisible(hwnd))
    {
        return false;
    }

    RECT rect {};
    if (!GetWindowRect(hwnd, &rect))
    {
        return false;
    }

    return (rect.right - rect.left) > 200 && (rect.bottom - rect.top) > 200;
}
}

struct Desktop360Compat::DtfItem
{
    std::wstring path;
    int order = -1;
};

struct Desktop360Compat::DtfDesktop
{
    RECT workArea {};
    std::vector<DtfItem> items;
};

Desktop360Compat::Desktop360Compat(Logger& logger)
    : logger_(logger)
{
}

Desktop360MoveResult Desktop360Compat::TryMoveIconToCenter(const std::filesystem::path& filePath, const ConfigData& config) const
{
    Desktop360MoveResult result;

    const HWND desktopWindow = Find360DesktopWindow();
    if (desktopWindow == nullptr)
    {
        return result;
    }

    result.attempted = true;
    const auto dataFile = FindDtfDataFile();
    if (dataFile.empty())
    {
        result.message = L"检测到 360 桌面助手，但未找到 DTFenceData.dtf";
        return result;
    }

    DtfDesktop desktop;
    std::optional<DtfItem> targetItem;
    for (int attempt = 0; attempt < 30; ++attempt)
    {
        if (attempt > 0)
        {
            Sleep(250);
        }

        desktop = DtfDesktop {};
        if (!LoadDesktopData(dataFile, desktop))
        {
            continue;
        }

        for (const auto& item : desktop.items)
        {
            if (SamePath(item.path, filePath))
            {
                targetItem = item;
                break;
            }
        }

        if (targetItem.has_value())
        {
            break;
        }
    }

    if (!targetItem.has_value())
    {
        result.message = L"检测到 360 桌面助手，但未在 DTFenceData.dtf 中找到新图标";
        return result;
    }

    RECT workArea = GetWorkArea();
    if (desktop.workArea.right > desktop.workArea.left && desktop.workArea.bottom > desktop.workArea.top)
    {
        workArea = desktop.workArea;
    }

    const int rows = std::max(1, (workArea.bottom - workArea.top) / CellHeight);
    const int columns = std::max(1, (workArea.right - workArea.left) / CellWidth);

    POINT targetPoint {
        workArea.left + (workArea.right - workArea.left) / 2,
        workArea.top + (workArea.bottom - workArea.top) / 2
    };

    const auto mode = ToLower(config.moveMode);
    if (mode == L"custom" || mode == L"fixed" || mode == L"manual")
    {
        targetPoint.x = config.centerX;
        targetPoint.y = config.centerY;
    }

    const int destinationOrder = FindNearestFreeOrder(targetPoint, rows, columns, targetItem->order, desktop);
    if (destinationOrder < 0)
    {
        result.message = L"检测到 360 桌面助手，但未找到可用目标网格";
        return result;
    }

    const POINT source = OrderToPoint(targetItem->order, rows, workArea);
    const POINT destination = OrderToPoint(destinationOrder, rows, workArea);
    result.requestedPoint = destination;

    if (destinationOrder == targetItem->order)
    {
        result.success = true;
        result.message = L"360 桌面助手图标已在目标网格附近";
        return result;
    }

    if (!SendDragMessages(desktopWindow, source, destination))
    {
        result.message = L"检测到 360 桌面助手，但发送拖拽消息失败";
        return result;
    }

    result.success = true;
    result.message = L"360 桌面助手兼容移动: " + filePath.filename().wstring() + L" -> (" +
        std::to_wstring(destination.x) + L", " + std::to_wstring(destination.y) + L")";
    logger_.Info(result.message);
    return result;
}

HWND Desktop360Compat::Find360DesktopWindow() const
{
    struct EnumContext
    {
        HWND found = nullptr;
    } context;

    EnumWindows(
        [](HWND hwnd, LPARAM parameter) -> BOOL {
            auto* ctx = reinterpret_cast<EnumContext*>(parameter);
            if (ctx->found != nullptr)
            {
                return FALSE;
            }

            EnumChildWindows(
                hwnd,
                [](HWND child, LPARAM childParameter) -> BOOL {
                    auto* childCtx = reinterpret_cast<EnumContext*>(childParameter);
                    wchar_t className[128] = {};
                    GetClassNameW(child, className, static_cast<int>(std::size(className)));
                    if (wcscmp(className, L"360DirectUICls") != 0)
                    {
                        return TRUE;
                    }

                    wchar_t title[128] = {};
                    GetWindowTextW(child, title, static_cast<int>(std::size(title)));
                    if (wcsstr(title, L"360DTFenceLite") == nullptr)
                    {
                        return TRUE;
                    }

                    if (IsShellExecuteVisible(child))
                    {
                        childCtx->found = child;
                        return FALSE;
                    }
                    return TRUE;
                },
                reinterpret_cast<LPARAM>(ctx));

            return ctx->found == nullptr;
        },
        reinterpret_cast<LPARAM>(&context));

    return context.found;
}

std::filesystem::path Desktop360Compat::FindDtfDataFile() const
{
    PWSTR roaming = nullptr;
    const HRESULT hr = SHGetKnownFolderPath(FOLDERID_RoamingAppData, KF_FLAG_DEFAULT, nullptr, &roaming);
    if (FAILED(hr) || roaming == nullptr)
    {
        return {};
    }

    const std::filesystem::path appData(roaming);
    CoTaskMemFree(roaming);

    const std::array<std::filesystem::path, 2> candidates {
        appData / L"360DesktopLite" / L"DTFence" / L"DTFenceData.dtf",
        appData / L"DesktopLite" / L"DTFence" / L"DTFenceData.dtf"
    };

    for (const auto& candidate : candidates)
    {
        std::error_code error;
        if (std::filesystem::exists(candidate, error))
        {
            return candidate;
        }
    }

    return {};
}

bool Desktop360Compat::LoadDesktopData(const std::filesystem::path& dataFile, DtfDesktop& desktop) const
{
    std::ifstream stream(dataFile, std::ios::binary);
    if (!stream)
    {
        return false;
    }

    std::vector<unsigned char> data(
        (std::istreambuf_iterator<char>(stream)),
        std::istreambuf_iterator<char>());
    if (data.empty())
    {
        return false;
    }

    size_t cursor = 0;
    while (cursor + 5 <= data.size())
    {
        ParsedStruct group;
        if (!ParseStruct(data, cursor, data.size(), group))
        {
            return false;
        }

        const size_t payloadLength = ReadUInt32Le(data, cursor + 1);
        cursor = group.payloadOffset + payloadLength;

        const auto groupId = IntAt(group, 0);
        const auto groupName = StringAt(group, 0);
        if (!groupId.has_value() || !groupName.has_value() || *groupId != Desktop360GroupId)
        {
            continue;
        }

        RECT workArea = GetWorkArea();
        const auto top = IntAt(group, 1);
        const auto bottom = IntAt(group, 2);
        const auto left = IntAt(group, 3);
        const auto right = IntAt(group, 4);
        if (left.has_value() && top.has_value() && right.has_value() && bottom.has_value() &&
            *right > *left && *bottom > *top)
        {
            workArea.left = *left;
            workArea.top = *top;
            workArea.right = *right;
            workArea.bottom = *bottom;
        }

        desktop.workArea = workArea;
        desktop.items.clear();

        for (const auto& record : group.children)
        {
            const auto path = StringAt(record, 0);
            const auto order = IntAt(record, 6);
            if (!path.has_value() || !order.has_value())
            {
                continue;
            }

            DtfItem item;
            item.path = *path;
            item.order = *order;
            if (item.order >= 0)
            {
                desktop.items.push_back(std::move(item));
            }
        }

        return true;
    }

    return false;
}

bool Desktop360Compat::SendDragMessages(HWND hwnd, POINT source, POINT destination) const
{
    if (hwnd == nullptr || !IsWindow(hwnd))
    {
        return false;
    }

    POINT clientSource = source;
    POINT clientDestination = destination;
    ScreenToClient(hwnd, &clientSource);
    ScreenToClient(hwnd, &clientDestination);

    const auto makePoint = [](POINT point) -> LPARAM {
        return MAKELPARAM(static_cast<short>(point.x), static_cast<short>(point.y));
    };

    if (!PostMessageW(hwnd, WM_MOUSEMOVE, 0, makePoint(clientSource)))
    {
        return false;
    }
    Sleep(20);
    if (!PostMessageW(hwnd, WM_LBUTTONDOWN, MK_LBUTTON, makePoint(clientSource)))
    {
        return false;
    }
    Sleep(40);

    constexpr int steps = 24;
    for (int step = 1; step <= steps; ++step)
    {
        POINT current {
            clientSource.x + ((clientDestination.x - clientSource.x) * step) / steps,
            clientSource.y + ((clientDestination.y - clientSource.y) * step) / steps
        };
        PostMessageW(hwnd, WM_MOUSEMOVE, MK_LBUTTON, makePoint(current));
        Sleep(12);
    }

    Sleep(40);
    if (!PostMessageW(hwnd, WM_LBUTTONUP, 0, makePoint(clientDestination)))
    {
        return false;
    }
    PostMessageW(hwnd, WM_MOUSEMOVE, 0, makePoint(clientDestination));
    return true;
}

POINT Desktop360Compat::OrderToPoint(int order, int rows, RECT workArea)
{
    rows = std::max(1, rows);
    const int column = std::max(0, order) / rows;
    const int row = std::max(0, order) % rows;
    return POINT {
        workArea.left + (CellWidth / 2) + column * CellWidth,
        workArea.top + (CellHeight / 2) + row * CellHeight
    };
}

int Desktop360Compat::FindNearestFreeOrder(POINT target, int rows, int columns, int sourceOrder, const DtfDesktop& desktop)
{
    std::set<int> usedOrders;
    for (const auto& item : desktop.items)
    {
        if (item.order >= 0 && item.order != sourceOrder)
        {
            usedOrders.insert(item.order);
        }
    }

    int bestOrder = -1;
    long long bestDistance = std::numeric_limits<long long>::max();
    const RECT workArea = desktop.workArea.right > desktop.workArea.left ? desktop.workArea : GetWorkArea();

    for (int column = 0; column < columns; ++column)
    {
        for (int row = 0; row < rows; ++row)
        {
            const int order = column * rows + row;
            if (usedOrders.find(order) != usedOrders.end())
            {
                continue;
            }

            const POINT point = OrderToPoint(order, rows, workArea);
            const long long dx = static_cast<long long>(point.x) - target.x;
            const long long dy = static_cast<long long>(point.y) - target.y;
            const long long distance = dx * dx + dy * dy;
            if (distance < bestDistance)
            {
                bestDistance = distance;
                bestOrder = order;
            }
        }
    }

    return bestOrder;
}

std::wstring Desktop360Compat::NormalizePathText(std::filesystem::path path)
{
    auto text = path.wstring();
    std::replace(text.begin(), text.end(), L'/', L'\\');
    return ToLower(text);
}

bool Desktop360Compat::SamePath(const std::wstring& left, const std::filesystem::path& right)
{
    return NormalizePathText(left) == NormalizePathText(right);
}
