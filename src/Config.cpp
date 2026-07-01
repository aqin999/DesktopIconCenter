#include "Config.h"

#include <algorithm>
#include <cwctype>
#include <fstream>
#include <iterator>
#include <sstream>
#include <vector>

namespace
{
std::wstring Trim(std::wstring value)
{
    const auto isSpace = [](wchar_t ch) { return std::iswspace(ch) != 0; };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), [isSpace](wchar_t ch) { return !isSpace(ch); }));
    value.erase(std::find_if(value.rbegin(), value.rend(), [isSpace](wchar_t ch) { return !isSpace(ch); }).base(), value.end());
    return value;
}

std::wstring ToLower(std::wstring value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](wchar_t ch) {
        return static_cast<wchar_t>(std::towlower(ch));
    });
    return value;
}

std::wstring QuotePath(const std::filesystem::path& path)
{
    return L"\"" + path.wstring() + L"\"";
}

std::filesystem::path GetExePath()
{
    std::wstring buffer(MAX_PATH, L'\0');
    while (true)
    {
        const DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (length == 0)
        {
            return Config::GetExeDirectory() / L"DesktopIconCenter.exe";
        }

        if (length < buffer.size() - 1)
        {
            buffer.resize(length);
            return std::filesystem::path(buffer);
        }

        buffer.resize(buffer.size() * 2);
    }
}
}

Config::Config()
    : path_(GetExeDirectory() / L"config.ini")
{
}

bool Config::Load()
{
    EnsureFileExists();

    data_.enable = ReadBool(L"Enable", true);
    data_.delay = static_cast<unsigned int>(std::max(0, ReadInt(L"Delay", 200)));
    data_.autoStart = ReadBool(L"AutoStart", false);
    data_.moveMode = ReadString(L"MoveMode", L"Center");
    data_.centerX = ReadInt(L"CenterX", 960);
    data_.centerY = ReadInt(L"CenterY", 540);
    data_.gridSearchRadius = std::max(1, ReadInt(L"GridSearchRadius", 20));
    data_.ignoreFolders = ReadBool(L"IgnoreFolders", false);
    data_.ignoreShortcut = ReadBool(L"IgnoreShortcut", false);
    data_.allow360RealMouseDrag = ReadBool(L"Allow360RealMouseDrag", false);
    data_.allow360Reload = ReadBool(L"Allow360Reload", false);
    return true;
}

bool Config::Reload()
{
    return Load();
}

bool Config::Save() const
{
    try
    {
        std::filesystem::create_directories(path_.parent_path());
    }
    catch (...)
    {
        return false;
    }

    const auto fileName = path_.wstring();
    const bool ok =
        WritePrivateProfileStringW(L"General", L"Enable", BoolToText(data_.enable).c_str(), fileName.c_str()) &&
        WritePrivateProfileStringW(L"General", L"Delay", std::to_wstring(data_.delay).c_str(), fileName.c_str()) &&
        WritePrivateProfileStringW(L"General", L"AutoStart", BoolToText(data_.autoStart).c_str(), fileName.c_str()) &&
        WritePrivateProfileStringW(L"General", L"MoveMode", data_.moveMode.c_str(), fileName.c_str()) &&
        WritePrivateProfileStringW(L"General", L"CenterX", std::to_wstring(data_.centerX).c_str(), fileName.c_str()) &&
        WritePrivateProfileStringW(L"General", L"CenterY", std::to_wstring(data_.centerY).c_str(), fileName.c_str()) &&
        WritePrivateProfileStringW(L"General", L"GridSearchRadius", std::to_wstring(data_.gridSearchRadius).c_str(), fileName.c_str()) &&
        WritePrivateProfileStringW(L"General", L"IgnoreFolders", BoolToText(data_.ignoreFolders).c_str(), fileName.c_str()) &&
        WritePrivateProfileStringW(L"General", L"IgnoreShortcut", BoolToText(data_.ignoreShortcut).c_str(), fileName.c_str()) &&
        WritePrivateProfileStringW(L"General", L"Allow360RealMouseDrag", BoolToText(data_.allow360RealMouseDrag).c_str(), fileName.c_str()) &&
        WritePrivateProfileStringW(L"General", L"Allow360Reload", BoolToText(data_.allow360Reload).c_str(), fileName.c_str());

    WritePrivateProfileStringW(nullptr, nullptr, nullptr, fileName.c_str());
    return ok;
}

const ConfigData& Config::Data() const noexcept
{
    return data_;
}

const std::filesystem::path& Config::Path() const noexcept
{
    return path_;
}

void Config::SetEnable(bool value) noexcept
{
    data_.enable = value;
}

void Config::SetAutoStart(bool value) noexcept
{
    data_.autoStart = value;
}

bool Config::ApplyAutoStart(bool enable) const
{
    HKEY key = nullptr;
    const auto status = RegCreateKeyExW(
        HKEY_CURRENT_USER,
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",
        0,
        nullptr,
        REG_OPTION_NON_VOLATILE,
        KEY_SET_VALUE,
        nullptr,
        &key,
        nullptr);

    if (status != ERROR_SUCCESS)
    {
        return false;
    }

    const auto closeKey = [](HKEY value) {
        if (value != nullptr)
        {
            RegCloseKey(value);
        }
    };

    bool result = false;
    if (enable)
    {
        const auto command = QuotePath(GetExePath());
        result = RegSetValueExW(
            key,
            L"DesktopIconCenter",
            0,
            REG_SZ,
            reinterpret_cast<const BYTE*>(command.c_str()),
            static_cast<DWORD>((command.size() + 1) * sizeof(wchar_t))) == ERROR_SUCCESS;
    }
    else
    {
        const auto deleteStatus = RegDeleteValueW(key, L"DesktopIconCenter");
        result = (deleteStatus == ERROR_SUCCESS || deleteStatus == ERROR_FILE_NOT_FOUND);
    }

    closeKey(key);
    return result;
}

bool Config::IsAutoStartEnabled() const
{
    wchar_t buffer[MAX_PATH * 2] = {};
    DWORD size = sizeof(buffer);
    const auto status = RegGetValueW(
        HKEY_CURRENT_USER,
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",
        L"DesktopIconCenter",
        RRF_RT_REG_SZ,
        nullptr,
        buffer,
        &size);
    return status == ERROR_SUCCESS && buffer[0] != L'\0';
}

std::filesystem::path Config::GetExeDirectory()
{
    std::wstring buffer(MAX_PATH, L'\0');
    DWORD length = 0;

    while (true)
    {
        length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (length == 0)
        {
            return std::filesystem::current_path();
        }

        if (length < buffer.size() - 1)
        {
            buffer.resize(length);
            break;
        }

        buffer.resize(buffer.size() * 2);
    }

    return std::filesystem::path(buffer).parent_path();
}

void Config::EnsureFileExists() const
{
    if (std::filesystem::exists(path_))
    {
        return;
    }

    try
    {
        std::filesystem::create_directories(path_.parent_path());
        std::ofstream file(path_, std::ios::binary | std::ios::trunc);
        file << "[General]\r\n"
             << "Enable=true\r\n"
             << "Delay=200\r\n"
             << "AutoStart=false\r\n"
             << "MoveMode=Center\r\n"
             << "CenterX=960\r\n"
             << "CenterY=540\r\n"
             << "GridSearchRadius=20\r\n"
             << "IgnoreFolders=false\r\n"
             << "IgnoreShortcut=false\r\n"
             << "Allow360RealMouseDrag=false\r\n"
             << "Allow360Reload=false\r\n";
    }
    catch (...)
    {
    }
}

bool Config::ReadBool(const wchar_t* key, bool defaultValue) const
{
    const auto text = ToLower(Trim(ReadString(key, defaultValue ? L"true" : L"false")));
    if (text == L"1" || text == L"true" || text == L"yes" || text == L"on")
    {
        return true;
    }

    if (text == L"0" || text == L"false" || text == L"no" || text == L"off")
    {
        return false;
    }

    return defaultValue;
}

int Config::ReadInt(const wchar_t* key, int defaultValue) const
{
    const auto text = Trim(ReadString(key, std::to_wstring(defaultValue).c_str()));
    try
    {
        return std::stoi(text);
    }
    catch (...)
    {
        return defaultValue;
    }
}

std::wstring Config::ReadString(const wchar_t* key, const wchar_t* defaultValue) const
{
    wchar_t buffer[512] = {};
    GetPrivateProfileStringW(L"General", key, defaultValue, buffer, static_cast<DWORD>(std::size(buffer)), path_.wstring().c_str());
    return buffer;
}

std::wstring Config::BoolToText(bool value)
{
    return value ? L"true" : L"false";
}
