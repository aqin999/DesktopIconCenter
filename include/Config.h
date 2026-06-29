#pragma once

#include <Windows.h>

#include <filesystem>
#include <string>

struct ConfigData
{
    bool enable = true;
    unsigned int delay = 200;
    bool autoStart = false;
    std::wstring moveMode = L"Center";
    int centerX = 960;
    int centerY = 540;
    int gridSearchRadius = 20;
    bool ignoreFolders = false;
    bool ignoreShortcut = false;
};

/*
配置类说明：
01. Config 负责读取、保存和重载 config.ini。
02. 配置文件默认位于 EXE 所在目录，便于单文件分发。
03. 首次启动时如果不存在配置文件，会自动写入默认值。
04. 类内部只保存一份 ConfigData 快照，调用 Reload 可刷新。
05. 读取布尔值时兼容 true/false、1/0、yes/no、on/off。
06. Delay 用于等待 Explorer 创建桌面图标，默认 200 毫秒。
07. MoveMode 默认 Center，后续可扩展 Custom 等模式。
08. CenterX 和 CenterY 作为自定义中心点的备用字段保留。
09. GridSearchRadius 控制网格搜索半径，避免无限搜索。
10. IgnoreFolders 用于按需忽略新建文件夹事件。
11. IgnoreShortcut 用于按需忽略 .lnk 快捷方式事件。
12. AutoStart 会同步到 HKCU Run 启动项。
13. 所有路径均使用 std::filesystem::path 管理。
14. 所有文本均使用 std::wstring，匹配 Unicode 工程要求。
15. 类不持有窗口句柄，不依赖 UI 线程。
16. 类不使用裸 new/delete，所有资源由 RAII 管理。
17. 保存配置时使用 Win32 Profile API，保持 INI 格式简单。
18. 注册表写入只影响当前用户，不需要管理员权限。
19. 失败时返回 false，由上层写入日志并继续运行。
20. 该类是程序运行配置的唯一入口，便于后续扩展。
*/
class Config
{
public:
    Config();

    bool Load();
    bool Reload();
    bool Save() const;

    const ConfigData& Data() const noexcept;
    const std::filesystem::path& Path() const noexcept;

    void SetEnable(bool value) noexcept;
    void SetAutoStart(bool value) noexcept;

    bool ApplyAutoStart(bool enable) const;
    bool IsAutoStartEnabled() const;

    static std::filesystem::path GetExeDirectory();

private:
    void EnsureFileExists() const;
    bool ReadBool(const wchar_t* key, bool defaultValue) const;
    int ReadInt(const wchar_t* key, int defaultValue) const;
    std::wstring ReadString(const wchar_t* key, const wchar_t* defaultValue) const;
    static std::wstring BoolToText(bool value);

private:
    std::filesystem::path path_;
    ConfigData data_;
};
