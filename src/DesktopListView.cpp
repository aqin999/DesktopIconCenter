#include "DesktopListView.h"

#include <CommCtrl.h>

#include <vector>

namespace
{
class UniqueHandle
{
public:
    explicit UniqueHandle(HANDLE handle = nullptr) noexcept
        : handle_(handle)
    {
    }

    ~UniqueHandle()
    {
        if (handle_ != nullptr && handle_ != INVALID_HANDLE_VALUE)
        {
            CloseHandle(handle_);
        }
    }

    UniqueHandle(const UniqueHandle&) = delete;
    UniqueHandle& operator=(const UniqueHandle&) = delete;

    HANDLE get() const noexcept
    {
        return handle_;
    }

    explicit operator bool() const noexcept
    {
        return handle_ != nullptr && handle_ != INVALID_HANDLE_VALUE;
    }

private:
    HANDLE handle_ = nullptr;
};

class RemoteMemory
{
public:
    RemoteMemory(HANDLE process, SIZE_T size)
        : process_(process), size_(size)
    {
        memory_ = VirtualAllocEx(process_, nullptr, size_, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    }

    ~RemoteMemory()
    {
        if (memory_ != nullptr)
        {
            VirtualFreeEx(process_, memory_, 0, MEM_RELEASE);
        }
    }

    RemoteMemory(const RemoteMemory&) = delete;
    RemoteMemory& operator=(const RemoteMemory&) = delete;

    LPVOID get() const noexcept
    {
        return memory_;
    }

    bool Write(const void* data, SIZE_T size) const
    {
        SIZE_T written = 0;
        return memory_ != nullptr && WriteProcessMemory(process_, memory_, data, size, &written) && written == size;
    }

    bool Read(void* data, SIZE_T size) const
    {
        SIZE_T read = 0;
        return memory_ != nullptr && ReadProcessMemory(process_, memory_, data, size, &read) && read == size;
    }

private:
    HANDLE process_ = nullptr;
    LPVOID memory_ = nullptr;
    SIZE_T size_ = 0;
};
}

DesktopListView::DesktopListView(HWND hwnd)
    : hwnd_(hwnd)
{
}

void DesktopListView::Attach(HWND hwnd) noexcept
{
    hwnd_ = hwnd;
}

HWND DesktopListView::Hwnd() const noexcept
{
    return hwnd_;
}

bool DesktopListView::IsValid() const noexcept
{
    return hwnd_ != nullptr && IsWindow(hwnd_);
}

int DesktopListView::GetItemCount() const
{
    DWORD_PTR result = 0;
    if (!Send(LVM_GETITEMCOUNT, 0, 0, &result))
    {
        return 0;
    }
    return static_cast<int>(result);
}

SIZE DesktopListView::GetIconSpacing() const
{
    DWORD_PTR result = 0;
    if (!Send(LVM_GETITEMSPACING, FALSE, 0, &result))
    {
        return SIZE { 96, 96 };
    }

    SIZE spacing { static_cast<LONG>(LOWORD(result)), static_cast<LONG>(HIWORD(result)) };
    if (spacing.cx <= 0 || spacing.cy <= 0)
    {
        spacing = SIZE { 96, 96 };
    }
    return spacing;
}

std::optional<POINT> DesktopListView::GetItemPosition(int index) const
{
    if (!IsValid() || index < 0)
    {
        return std::nullopt;
    }

    UniqueHandle process(OpenListViewProcess());
    if (!process)
    {
        return std::nullopt;
    }

    RemoteMemory remotePoint(process.get(), sizeof(POINT));
    if (remotePoint.get() == nullptr)
    {
        return std::nullopt;
    }

    DWORD_PTR result = 0;
    if (!Send(LVM_GETITEMPOSITION, static_cast<WPARAM>(index), reinterpret_cast<LPARAM>(remotePoint.get()), &result))
    {
        return std::nullopt;
    }

    POINT point {};
    if (!remotePoint.Read(&point, sizeof(point)))
    {
        return std::nullopt;
    }

    return point;
}

int DesktopListView::FindItemByName(const std::wstring& name) const
{
    if (!IsValid() || name.empty())
    {
        return -1;
    }

    UniqueHandle process(OpenListViewProcess());
    if (!process)
    {
        return -1;
    }

    const SIZE_T textBytes = (name.size() + 1) * sizeof(wchar_t);
    RemoteMemory remoteText(process.get(), textBytes);
    RemoteMemory remoteInfo(process.get(), sizeof(LVFINDINFOW));
    if (remoteText.get() == nullptr || remoteInfo.get() == nullptr)
    {
        return -1;
    }

    if (!remoteText.Write(name.c_str(), textBytes))
    {
        return -1;
    }

    LVFINDINFOW info {};
    info.flags = LVFI_STRING;
    info.psz = reinterpret_cast<LPCWSTR>(remoteText.get());
    if (!remoteInfo.Write(&info, sizeof(info)))
    {
        return -1;
    }

    DWORD_PTR result = 0;
    if (!Send(LVM_FINDITEMW, static_cast<WPARAM>(-1), reinterpret_cast<LPARAM>(remoteInfo.get()), &result))
    {
        return -1;
    }

    return static_cast<int>(result);
}

bool DesktopListView::SetItemPosition(int index, POINT point) const
{
    if (!IsValid() || index < 0)
    {
        return false;
    }

    DWORD_PTR result = 0;
    return Send(LVM_SETITEMPOSITION, static_cast<WPARAM>(index), MAKELPARAM(point.x, point.y), &result);
}

bool DesktopListView::Send(UINT message, WPARAM wParam, LPARAM lParam, DWORD_PTR* result) const
{
    if (!IsValid())
    {
        return false;
    }

    DWORD_PTR localResult = 0;
    const BOOL ok = SendMessageTimeoutW(
        hwnd_,
        message,
        wParam,
        lParam,
        SMTO_ABORTIFHUNG | SMTO_BLOCK,
        1000,
        &localResult);

    if (result != nullptr)
    {
        *result = localResult;
    }

    return ok != FALSE;
}

HANDLE DesktopListView::OpenListViewProcess() const
{
    if (!IsValid())
    {
        return nullptr;
    }

    DWORD processId = 0;
    GetWindowThreadProcessId(hwnd_, &processId);
    if (processId == 0)
    {
        return nullptr;
    }

    return OpenProcess(
        PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_VM_OPERATION | PROCESS_VM_READ | PROCESS_VM_WRITE,
        FALSE,
        processId);
}
