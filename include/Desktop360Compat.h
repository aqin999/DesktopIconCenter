#pragma once

#include <Windows.h>

#include <filesystem>
#include <string>

#include "Config.h"
#include "Logger.h"

struct Desktop360MoveResult
{
    bool attempted = false;
    bool success = false;
    std::wstring message;
    POINT requestedPoint { 0, 0 };
};

class Desktop360Compat
{
public:
    explicit Desktop360Compat(Logger& logger);

    Desktop360MoveResult TryMoveIconToCenter(const std::filesystem::path& filePath, const ConfigData& config) const;

private:
    struct DtfItem;
    struct DtfDesktop;

    HWND Find360DesktopWindow() const;
    std::filesystem::path FindDtfDataFile() const;
    bool LoadDesktopData(const std::filesystem::path& dataFile, DtfDesktop& desktop) const;
    bool SendDragMessages(HWND hwnd, POINT source, POINT destination) const;

    static POINT OrderToPoint(int order, int rows, RECT workArea);
    static int FindNearestFreeOrder(POINT target, int rows, int columns, int sourceOrder, const DtfDesktop& desktop);
    static std::wstring NormalizePathText(std::filesystem::path path);
    static bool SamePath(const std::wstring& left, const std::filesystem::path& right);

private:
    Logger& logger_;
};
