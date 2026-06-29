#include "DesktopWatcher.h"

#include <iterator>
#include <utility>
#include <vector>

DesktopWatcher::~DesktopWatcher()
{
    Stop();
}

bool DesktopWatcher::Start(const std::filesystem::path& desktopPath, Callback callback)
{
    if (running_.load())
    {
        return true;
    }

    desktopPath_ = desktopPath;
    callback_ = std::move(callback);
    stopRequested_.store(false);
    running_.store(true);

    try
    {
        worker_ = std::thread(&DesktopWatcher::ThreadProc, this);
        return true;
    }
    catch (...)
    {
        running_.store(false);
        return false;
    }
}

void DesktopWatcher::Stop()
{
    stopRequested_.store(true);

    const HANDLE handle = static_cast<HANDLE>(directoryHandle_.load());
    if (handle != nullptr && handle != INVALID_HANDLE_VALUE)
    {
        CancelIoEx(handle, nullptr);
    }

    const HANDLE stopEvent = static_cast<HANDLE>(stopEvent_.load());
    if (stopEvent != nullptr && stopEvent != INVALID_HANDLE_VALUE)
    {
        SetEvent(stopEvent);
    }

    if (worker_.joinable())
    {
        worker_.join();
    }

    running_.store(false);
}

bool DesktopWatcher::IsRunning() const noexcept
{
    return running_.load();
}

void DesktopWatcher::ThreadProc()
{
    HANDLE directory = CreateFileW(
        desktopPath_.wstring().c_str(),
        FILE_LIST_DIRECTORY,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr,
        OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
        nullptr);

    if (directory == INVALID_HANDLE_VALUE)
    {
        directoryHandle_.store(nullptr);
        running_.store(false);
        return;
    }

    directoryHandle_.store(directory);
    HANDLE stopEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    HANDLE changeEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (stopEvent == nullptr || changeEvent == nullptr)
    {
        if (stopEvent != nullptr)
        {
            CloseHandle(stopEvent);
        }
        if (changeEvent != nullptr)
        {
            CloseHandle(changeEvent);
        }
        directoryHandle_.store(nullptr);
        CloseHandle(directory);
        running_.store(false);
        return;
    }

    stopEvent_.store(stopEvent);
    std::vector<BYTE> buffer(64 * 1024);

    while (!IsStopRequested())
    {
        DWORD bytesReturned = 0;
        OVERLAPPED overlapped {};
        overlapped.hEvent = changeEvent;
        ResetEvent(changeEvent);

        const BOOL ok = ReadDirectoryChangesW(
            directory,
            buffer.data(),
            static_cast<DWORD>(buffer.size()),
            FALSE,
            FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME,
            nullptr,
            &overlapped,
            nullptr);

        if (!ok && GetLastError() != ERROR_IO_PENDING)
        {
            if (!IsStopRequested())
            {
                Sleep(500);
            }
            continue;
        }

        HANDLE waitHandles[] = { stopEvent, changeEvent };
        const DWORD waitResult = WaitForMultipleObjects(static_cast<DWORD>(std::size(waitHandles)), waitHandles, FALSE, INFINITE);
        if (waitResult == WAIT_OBJECT_0)
        {
            CancelIoEx(directory, &overlapped);
            GetOverlappedResult(directory, &overlapped, &bytesReturned, FALSE);
            break;
        }

        if (waitResult != WAIT_OBJECT_0 + 1)
        {
            CancelIoEx(directory, &overlapped);
            continue;
        }

        if (!GetOverlappedResult(directory, &overlapped, &bytesReturned, FALSE))
        {
            if (GetLastError() != ERROR_OPERATION_ABORTED && !IsStopRequested())
            {
                Sleep(500);
            }
            continue;
        }

        if (bytesReturned == 0)
        {
            continue;
        }

        DWORD offset = 0;
        while (offset < bytesReturned)
        {
            const auto* info = reinterpret_cast<const FILE_NOTIFY_INFORMATION*>(buffer.data() + offset);
            std::wstring fileName(info->FileName, info->FileNameLength / sizeof(wchar_t));

            if (info->Action == FILE_ACTION_ADDED || info->Action == FILE_ACTION_RENAMED_NEW_NAME)
            {
                const auto fullPath = desktopPath_ / fileName;
                const DWORD attributes = GetFileAttributesW(fullPath.wstring().c_str());

                DesktopChangeEvent event;
                event.fullPath = fullPath;
                event.fileName = std::move(fileName);
                event.action = info->Action;
                event.isDirectory = (attributes != INVALID_FILE_ATTRIBUTES) && ((attributes & FILE_ATTRIBUTE_DIRECTORY) != 0);

                if (callback_)
                {
                    callback_(event);
                }
            }

            if (info->NextEntryOffset == 0)
            {
                break;
            }
            offset += info->NextEntryOffset;
        }
    }

    directoryHandle_.store(nullptr);
    stopEvent_.store(nullptr);
    CloseHandle(changeEvent);
    CloseHandle(stopEvent);
    CloseHandle(directory);
    running_.store(false);
}

bool DesktopWatcher::IsStopRequested() const noexcept
{
    return stopRequested_.load();
}
