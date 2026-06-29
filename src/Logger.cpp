#include "Logger.h"

#include <Windows.h>

#include <iomanip>
#include <sstream>
#include <utility>
#include <vector>

Logger::Logger(std::filesystem::path logDirectory)
    : logDirectory_(std::move(logDirectory))
{
}

void Logger::Initialize(std::filesystem::path logDirectory)
{
    std::lock_guard<std::mutex> lock(mutex_);
    logDirectory_ = std::move(logDirectory);

    try
    {
        std::filesystem::create_directories(logDirectory_);
    }
    catch (...)
    {
    }
}

void Logger::Info(const std::wstring& message)
{
    Write(L"INFO", message);
}

void Logger::Warn(const std::wstring& message)
{
    Write(L"WARN", message);
}

void Logger::Error(const std::wstring& message)
{
    Write(L"ERROR", message);
}

void Logger::Write(const wchar_t* level, const std::wstring& message)
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (logDirectory_.empty())
    {
        return;
    }

    try
    {
        std::filesystem::create_directories(logDirectory_);
    }
    catch (...)
    {
        return;
    }

    const auto line = Timestamp() + L" [" + level + L"] " + message + L"\r\n";
    const auto utf8 = ToUtf8(line);
    const auto filePath = CurrentLogFile();

    HANDLE file = CreateFileW(
        filePath.wstring().c_str(),
        FILE_APPEND_DATA,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr,
        OPEN_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);

    if (file == INVALID_HANDLE_VALUE)
    {
        return;
    }

    LARGE_INTEGER size {};
    if (GetFileSizeEx(file, &size) && size.QuadPart == 0)
    {
        constexpr unsigned char bom[] = { 0xEF, 0xBB, 0xBF };
        DWORD written = 0;
        WriteFile(file, bom, sizeof(bom), &written, nullptr);
    }

    DWORD written = 0;
    WriteFile(file, utf8.data(), static_cast<DWORD>(utf8.size()), &written, nullptr);
    CloseHandle(file);
}

std::filesystem::path Logger::CurrentLogFile() const
{
    SYSTEMTIME time {};
    GetLocalTime(&time);

    wchar_t name[32] = {};
    swprintf_s(name, L"%04u-%02u-%02u.log", time.wYear, time.wMonth, time.wDay);
    return logDirectory_ / name;
}

std::wstring Logger::Timestamp()
{
    SYSTEMTIME time {};
    GetLocalTime(&time);

    wchar_t buffer[64] = {};
    swprintf_s(
        buffer,
        L"%04u-%02u-%02u %02u:%02u:%02u.%03u",
        time.wYear,
        time.wMonth,
        time.wDay,
        time.wHour,
        time.wMinute,
        time.wSecond,
        time.wMilliseconds);
    return buffer;
}

std::string Logger::ToUtf8(const std::wstring& text)
{
    if (text.empty())
    {
        return {};
    }

    const int required = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
    if (required <= 0)
    {
        return {};
    }

    std::string result(static_cast<size_t>(required), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), result.data(), required, nullptr, nullptr);
    return result;
}
