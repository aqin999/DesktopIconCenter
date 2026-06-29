#include "IconManager.h"

#include "Desktop360Compat.h"
#include "DesktopListView.h"

#include <Shlwapi.h>
#include <wrl/client.h>

#include <algorithm>
#include <array>
#include <cwctype>
#include <iterator>
#include <memory>
#include <sstream>
#include <utility>

using Microsoft::WRL::ComPtr;

namespace
{
struct PidlDeleter
{
    void operator()(ITEMID_CHILD* pidl) const noexcept
    {
        if (pidl != nullptr)
        {
            CoTaskMemFree(pidl);
        }
    }
};

std::wstring ToLower(std::wstring value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](wchar_t ch) {
        return static_cast<wchar_t>(std::towlower(ch));
    });
    return value;
}

std::wstring NormalizePathText(std::filesystem::path path)
{
    auto text = path.wstring();
    std::replace(text.begin(), text.end(), L'/', L'\\');
    return ToLower(text);
}

std::wstring DisplayNameFromPidl(IShellFolder* folder, PCUITEMID_CHILD pidl, SHGDNF flags)
{
    if (folder == nullptr || pidl == nullptr)
    {
        return {};
    }

    STRRET strret {};
    if (FAILED(folder->GetDisplayNameOf(pidl, flags, &strret)))
    {
        return {};
    }

    wchar_t buffer[MAX_PATH * 4] = {};
    if (FAILED(StrRetToBufW(&strret, pidl, buffer, static_cast<UINT>(std::size(buffer)))))
    {
        return {};
    }

    return buffer;
}
}

struct IconManager::ShellIconInfo
{
    DesktopIconInfo icon;
    std::unique_ptr<ITEMID_CHILD, PidlDeleter> pidl;
};

IconManager::IconManager(ExplorerDesktop& explorerDesktop, Logger& logger)
    : explorerDesktop_(explorerDesktop), logger_(logger)
{
}

MoveResult IconManager::MoveNewIconToCenter(const std::filesystem::path& filePath, const ConfigData& config)
{
    MoveResult result;
    const std::array<DWORD, 5> retryDelayMs { 0, 200, 500, 1000, 1000 };
    std::wstring lastReason = L"未知错误";

    for (size_t attempt = 0; attempt < retryDelayMs.size(); ++attempt)
    {
        if (retryDelayMs[attempt] > 0)
        {
            Sleep(retryDelayMs[attempt]);
        }

        explorerDesktop_.EnsureReady();
        DesktopListView listView(explorerDesktop_.GetListViewHwnd());
        const int listIndex = listView.FindItemByName(filePath.filename().wstring());
        if (listIndex >= 0)
        {
            logger_.Info(L"LVM_FINDITEM 定位到图标索引: " + std::to_wstring(listIndex));
        }

        ComPtr<IFolderView2> folderView;
        HRESULT hr = explorerDesktop_.GetDesktopFolderView(folderView.GetAddressOf());
        if (FAILED(hr))
        {
            lastReason = L"获取 IFolderView 失败: " + HResultText(hr);
            logger_.Warn(lastReason);
            continue;
        }

        std::vector<ShellIconInfo> icons;
        if (!EnumerateIcons(folderView.Get(), icons))
        {
            lastReason = L"枚举桌面图标失败";
            logger_.Warn(lastReason);
            continue;
        }

        const auto targetIndex = FindIconIndex(icons, filePath);
        if (!targetIndex.has_value())
        {
            lastReason = L"Explorer 尚未创建对应图标，等待重试: " + filePath.filename().wstring();
            logger_.Warn(lastReason);
            continue;
        }

        for (auto& item : icons)
        {
            item.icon.isTarget = false;
        }
        icons[*targetIndex].icon.isTarget = true;

        std::vector<DesktopIconInfo> publicIcons;
        publicIcons.reserve(icons.size());
        for (const auto& item : icons)
        {
            publicIcons.push_back(item.icon);
        }

        const RECT workArea = GetPrimaryWorkArea();
        const SIZE spacing = DetectIconSpacing();
        const POINT destination = gridManager_.FindNearestFreePosition(workArea, spacing, publicIcons, config);

        PCUITEMID_CHILD targetPidl = icons[*targetIndex].pidl.get();
        POINT mutableDestination = destination;
        hr = folderView->SelectAndPositionItems(1, &targetPidl, &mutableDestination, SVSI_POSITIONITEM);
        if (SUCCEEDED(hr))
        {
            if (listView.IsValid())
            {
                int moveIndex = listIndex;
                if (moveIndex < 0)
                {
                    moveIndex = listView.FindItemByName(filePath.stem().wstring());
                }
                if (moveIndex < 0 && *targetIndex < static_cast<size_t>(listView.GetItemCount()))
                {
                    moveIndex = static_cast<int>(*targetIndex);
                }

                if (moveIndex >= 0)
                {
                    if (listView.SetItemPosition(moveIndex, destination))
                    {
                        logger_.Info(L"LVM_SETITEMPOSITION 兜底移动索引 " + std::to_wstring(moveIndex) + L" -> (" +
                            std::to_wstring(destination.x) + L", " + std::to_wstring(destination.y) + L")");
                    }
                    RedrawWindow(listView.Hwnd(), nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);
                }
                else
                {
                    logger_.Warn(L"LVM_SETITEMPOSITION 兜底未找到 ListView 索引: " + filePath.filename().wstring());
                }
            }

            Desktop360Compat desktop360Compat(logger_);
            const Desktop360MoveResult desktop360Result = desktop360Compat.TryMoveIconToCenter(filePath, config);
            if (desktop360Result.attempted && !desktop360Result.success)
            {
                result.success = false;
                result.requestedPoint = destination;
                result.message = L"Explorer 已移动，但 360 桌面助手兼容移动失败: " + desktop360Result.message;
                return result;
            }

            result.success = true;
            result.requestedPoint = desktop360Result.attempted ? desktop360Result.requestedPoint : destination;
            result.message = L"移动成功: " + filePath.filename().wstring() + L" -> (" +
                std::to_wstring(result.requestedPoint.x) + L", " + std::to_wstring(result.requestedPoint.y) + L")";
            if (desktop360Result.attempted)
            {
                result.message += L"; " + desktop360Result.message;
            }

            if (listView.IsValid())
            {
                RedrawWindow(listView.Hwnd(), nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);
            }
            return result;
        }

        lastReason = L"移动图标失败: " + HResultText(hr);
        logger_.Warn(lastReason);
    }

    result.success = false;
    result.message = lastReason;
    return result;
}

bool IconManager::EnumerateIcons(IFolderView2* folderView, std::vector<ShellIconInfo>& icons) const
{
    if (folderView == nullptr)
    {
        return false;
    }

    ComPtr<IShellFolder> shellFolder;
    HRESULT hr = folderView->GetFolder(IID_PPV_ARGS(&shellFolder));
    if (FAILED(hr))
    {
        return false;
    }

    int count = 0;
    hr = folderView->ItemCount(SVGIO_ALLVIEW, &count);
    if (FAILED(hr))
    {
        return false;
    }

    icons.clear();
    icons.reserve(static_cast<size_t>(std::max(count, 0)));

    for (int index = 0; index < count; ++index)
    {
        PITEMID_CHILD rawPidl = nullptr;
        if (FAILED(folderView->Item(index, &rawPidl)) || rawPidl == nullptr)
        {
            continue;
        }

        ShellIconInfo item;
        item.pidl.reset(rawPidl);

        POINT position {};
        if (SUCCEEDED(folderView->GetItemPosition(item.pidl.get(), &position)))
        {
            item.icon.position = position;
        }

        item.icon.name = DisplayNameFromPidl(shellFolder.Get(), item.pidl.get(), SHGDN_INFOLDER);
        const auto parsingName = DisplayNameFromPidl(shellFolder.Get(), item.pidl.get(), SHGDN_FORPARSING);
        if (!parsingName.empty())
        {
            item.icon.parsingPath = parsingName;
        }

        icons.push_back(std::move(item));
    }

    return true;
}

std::optional<size_t> IconManager::FindIconIndex(const std::vector<ShellIconInfo>& icons, const std::filesystem::path& filePath) const
{
    const auto targetFullPath = NormalizePathText(filePath);
    const auto targetFileName = filePath.filename().wstring();
    const auto targetStem = filePath.stem().wstring();

    for (size_t index = 0; index < icons.size(); ++index)
    {
        const auto& icon = icons[index].icon;
        if (!icon.parsingPath.empty() && NormalizePathText(icon.parsingPath) == targetFullPath)
        {
            return index;
        }
    }

    for (size_t index = 0; index < icons.size(); ++index)
    {
        const auto& icon = icons[index].icon;
        if (!icon.parsingPath.empty() && FileNameEquals(icon.parsingPath.filename().wstring(), targetFileName))
        {
            return index;
        }
    }

    for (size_t index = 0; index < icons.size(); ++index)
    {
        const auto& icon = icons[index].icon;
        if (FileNameEquals(icon.name, targetFileName) || FileNameEquals(icon.name, targetStem))
        {
            return index;
        }
    }

    return std::nullopt;
}

SIZE IconManager::DetectIconSpacing() const
{
    DesktopListView listView(explorerDesktop_.GetListViewHwnd());
    SIZE spacing = listView.GetIconSpacing();

    if (spacing.cx <= 0 || spacing.cy <= 0)
    {
        spacing = SIZE { 96, 96 };
    }

    int systemHorizontal = 0;
    int systemVertical = 0;
    if (SystemParametersInfoW(SPI_ICONHORIZONTALSPACING, 0, &systemHorizontal, 0) && systemHorizontal > 0)
    {
        spacing.cx = systemHorizontal;
    }
    if (SystemParametersInfoW(SPI_ICONVERTICALSPACING, 0, &systemVertical, 0) && systemVertical > 0)
    {
        spacing.cy = systemVertical;
    }

    spacing.cx = std::max<LONG>(spacing.cx, 48);
    spacing.cy = std::max<LONG>(spacing.cy, 48);
    return spacing;
}

RECT IconManager::GetPrimaryWorkArea() const
{
    POINT origin { 0, 0 };
    HMONITOR monitor = MonitorFromPoint(origin, MONITOR_DEFAULTTOPRIMARY);
    MONITORINFO monitorInfo {};
    monitorInfo.cbSize = sizeof(monitorInfo);
    if (monitor != nullptr && GetMonitorInfoW(monitor, &monitorInfo))
    {
        return monitorInfo.rcWork;
    }

    RECT fallback {
        0,
        0,
        GetSystemMetrics(SM_CXSCREEN),
        GetSystemMetrics(SM_CYSCREEN)
    };
    return fallback;
}

bool IconManager::FileNameEquals(const std::wstring& left, const std::wstring& right)
{
    return ToLower(left) == ToLower(right);
}

std::wstring IconManager::HResultText(HRESULT hr)
{
    wchar_t* message = nullptr;
    const DWORD flags = FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS;
    const DWORD length = FormatMessageW(flags, nullptr, static_cast<DWORD>(hr), 0, reinterpret_cast<LPWSTR>(&message), 0, nullptr);

    std::wstring text;
    if (length != 0 && message != nullptr)
    {
        text.assign(message, length);
        while (!text.empty() && (text.back() == L'\r' || text.back() == L'\n' || text.back() == L'.' || text.back() == L' '))
        {
            text.pop_back();
        }
    }
    else
    {
        std::wstringstream stream;
        stream << L"HRESULT 0x" << std::hex << static_cast<unsigned long>(hr);
        text = stream.str();
    }

    if (message != nullptr)
    {
        LocalFree(message);
    }
    return text;
}
