#pragma once

#include <Windows.h>

#include <atomic>
#include <filesystem>
#include <functional>
#include <string>
#include <thread>

struct DesktopChangeEvent
{
    std::filesystem::path fullPath;
    std::wstring fileName;
    DWORD action = 0;
    bool isDirectory = false;
};

/*
桌面监听类说明：
01. DesktopWatcher 负责用 ReadDirectoryChangesW 监听桌面目录。
02. 监听目标由 App 传入，默认是当前用户 Desktop 文件夹。
03. 监听事件包含 FILE_ACTION_ADDED 和 FILE_ACTION_RENAMED_NEW_NAME。
04. 监听范围包含文件、文件夹和快捷方式，具体忽略规则由 App 决定。
05. 类内部维护一个工作线程，避免阻塞 UI 消息循环。
06. Stop 会调用 CancelIoEx 取消阻塞中的 ReadDirectoryChangesW。
07. 目录句柄使用原子指针暴露给 Stop，避免长时间等待。
08. 工作线程退出后由 std::thread::join 回收。
09. 事件以 DesktopChangeEvent 传递，包含完整路径和文件名。
10. 回调函数不直接操作窗口，由 App 转投递到 UI 线程。
11. 监听不递归子目录，符合“新建桌面项目”的需求。
12. 共享模式包含 read/write/delete，避免影响 Explorer 创建文件。
13. 句柄打开使用 FILE_FLAG_BACKUP_SEMANTICS 以支持目录句柄。
14. 类不保存 Config，运行状态由 App 控制，职责单一。
15. 类不使用裸 new/delete，线程和缓冲区均由 RAII 管理。
16. 监听失败会结束线程，上层可根据日志或菜单重新启动。
17. 解析通知缓冲区时严格按 NextEntryOffset 遍历。
18. 文件属性读取失败时默认按非目录处理，避免误忽略。
19. CPU 占用接近 0，因为线程大部分时间阻塞在系统调用。
20. 该类是后台检测新增桌面项目的唯一入口。
*/
class DesktopWatcher
{
public:
    using Callback = std::function<void(const DesktopChangeEvent&)>;

    DesktopWatcher() = default;
    ~DesktopWatcher();

    DesktopWatcher(const DesktopWatcher&) = delete;
    DesktopWatcher& operator=(const DesktopWatcher&) = delete;

    bool Start(const std::filesystem::path& desktopPath, Callback callback);
    void Stop();
    bool IsRunning() const noexcept;

private:
    void ThreadProc();
    bool IsStopRequested() const noexcept;

private:
    std::filesystem::path desktopPath_;
    Callback callback_;
    std::thread worker_;
    std::atomic_bool stopRequested_ { false };
    std::atomic_bool running_ { false };
    std::atomic<void*> directoryHandle_ { nullptr };
    std::atomic<void*> stopEvent_ { nullptr };
};
