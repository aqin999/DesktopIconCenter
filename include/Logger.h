#pragma once

#include <filesystem>
#include <mutex>
#include <string>

/*
日志类说明：
01. Logger 负责将关键运行信息写入 logs/yyyy-MM-dd.log。
02. 日志目录默认由 App 指定为 EXE 同级的 logs 文件夹。
03. 写入采用 UTF-8 BOM，便于 Windows 记事本直接查看中文。
04. 类内部使用互斥锁，允许监听线程和 UI 线程同时写入。
05. 每条日志包含时间戳、级别和消息正文。
06. Info 用于普通流程，例如启动、监听、移动成功。
07. Warn 用于可恢复问题，例如图标暂时未出现。
08. Error 用于失败原因，例如 Explorer 句柄获取失败。
09. 写入失败不会抛出异常，避免影响后台工具稳定性。
10. 日志文件按自然日滚动，长期运行时会自动切换文件。
11. 日志类不缓存文件句柄，降低资源占用并便于查看。
12. 每次写入只打开、追加、关闭，适合本工具低频事件。
13. 类不使用裸 new/delete，全部依赖标准库和 Win32 RAII。
14. 路径使用 std::filesystem，兼容 Unicode 路径。
15. 消息文本使用 std::wstring，调用时无需手动转码。
16. Logger 不依赖 Config，避免初始化顺序耦合。
17. 日志目录创建失败时静默返回，由程序继续运行。
18. 该类不弹窗、不阻塞用户操作，符合后台工具定位。
19. 后续如需异步日志线程，可在此类内部扩展。
20. 当前实现优先简洁稳定，满足文档中的日志记录需求。
*/
class Logger
{
public:
    Logger() = default;
    explicit Logger(std::filesystem::path logDirectory);

    void Initialize(std::filesystem::path logDirectory);

    void Info(const std::wstring& message);
    void Warn(const std::wstring& message);
    void Error(const std::wstring& message);

private:
    void Write(const wchar_t* level, const std::wstring& message);
    std::filesystem::path CurrentLogFile() const;
    static std::wstring Timestamp();
    static std::string ToUtf8(const std::wstring& text);

private:
    std::filesystem::path logDirectory_;
    std::mutex mutex_;
};
