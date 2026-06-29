#include "ExplorerDesktop.h"

#include <ExDisp.h>
#include <OleAuto.h>
#include <ShlGuid.h>
#include <ShlObj.h>
#include <wrl/client.h>

#include <iterator>

using Microsoft::WRL::ComPtr;

bool ExplorerDesktop::Refresh()
{
    progman_ = FindWindowW(L"Progman", nullptr);
    workerw_ = nullptr;
    defView_ = nullptr;
    listView_ = nullptr;

    if (progman_ != nullptr)
    {
        defView_ = FindDefViewInWindow(progman_);
        if (defView_ != nullptr)
        {
            listView_ = FindWindowExW(defView_, nullptr, L"SysListView32", nullptr);
            if (listView_ != nullptr)
            {
                return true;
            }
            defView_ = nullptr;
        }
    }

    struct EnumContext
    {
        HWND workerw = nullptr;
        HWND defView = nullptr;
    } context;

    EnumWindows(
        [](HWND hwnd, LPARAM parameter) -> BOOL {
            auto* ctx = reinterpret_cast<EnumContext*>(parameter);
            wchar_t className[64] = {};
            GetClassNameW(hwnd, className, static_cast<int>(std::size(className)));
            if (wcscmp(className, L"WorkerW") != 0)
            {
                return TRUE;
            }

            const HWND defView = ExplorerDesktop::FindDefViewInWindow(hwnd);
            if (defView == nullptr)
            {
                return TRUE;
            }

            ctx->workerw = hwnd;
            ctx->defView = defView;
            return FALSE;
        },
        reinterpret_cast<LPARAM>(&context));

    workerw_ = context.workerw;
    defView_ = context.defView;
    if (defView_ != nullptr)
    {
        listView_ = FindWindowExW(defView_, nullptr, L"SysListView32", nullptr);
    }

    return listView_ != nullptr;
}

bool ExplorerDesktop::EnsureReady()
{
    if (listView_ != nullptr && IsWindow(listView_))
    {
        return true;
    }
    return Refresh();
}

HWND ExplorerDesktop::GetProgmanHwnd() const noexcept
{
    return progman_;
}

HWND ExplorerDesktop::GetWorkerWHwnd() const noexcept
{
    return workerw_;
}

HWND ExplorerDesktop::GetDefViewHwnd() const noexcept
{
    return defView_;
}

HWND ExplorerDesktop::GetListViewHwnd() const noexcept
{
    return listView_;
}

HRESULT ExplorerDesktop::GetDesktopFolderView(IFolderView2** folderView) const
{
    if (folderView == nullptr)
    {
        return E_POINTER;
    }
    *folderView = nullptr;

    ComPtr<IShellWindows> shellWindows;
    HRESULT hr = CoCreateInstance(CLSID_ShellWindows, nullptr, CLSCTX_LOCAL_SERVER, IID_PPV_ARGS(shellWindows.GetAddressOf()));
    if (FAILED(hr))
    {
        return hr;
    }

    VARIANT desktopLocation;
    VARIANT empty;
    VariantInit(&desktopLocation);
    VariantInit(&empty);
    desktopLocation.vt = VT_I4;
    desktopLocation.lVal = CSIDL_DESKTOP;

    long hwnd = 0;
    ComPtr<IDispatch> dispatch;
    hr = shellWindows->FindWindowSW(&desktopLocation, &empty, SWC_DESKTOP, &hwnd, SWFO_NEEDDISPATCH, dispatch.GetAddressOf());
    VariantClear(&desktopLocation);
    VariantClear(&empty);
    if (FAILED(hr))
    {
        return hr;
    }
    if (dispatch.Get() == nullptr)
    {
        return E_FAIL;
    }

    ComPtr<IServiceProvider> serviceProvider;
    hr = dispatch.As(&serviceProvider);
    if (FAILED(hr))
    {
        return hr;
    }

    ComPtr<IShellBrowser> shellBrowser;
    hr = serviceProvider->QueryService(SID_STopLevelBrowser, IID_PPV_ARGS(shellBrowser.GetAddressOf()));
    if (FAILED(hr))
    {
        return hr;
    }

    ComPtr<IShellView> shellView;
    hr = shellBrowser->QueryActiveShellView(shellView.GetAddressOf());
    if (FAILED(hr))
    {
        return hr;
    }

    ComPtr<IFolderView2> view;
    hr = shellView.As(&view);
    if (FAILED(hr))
    {
        return hr;
    }

    *folderView = view.Detach();
    return S_OK;
}

HWND ExplorerDesktop::FindDefViewInWindow(HWND parent)
{
    if (parent == nullptr)
    {
        return nullptr;
    }

    HWND defView = FindWindowExW(parent, nullptr, L"SHELLDLL_DefView", nullptr);
    if (defView != nullptr)
    {
        return defView;
    }

    HWND child = nullptr;
    while ((child = FindWindowExW(parent, child, nullptr, nullptr)) != nullptr)
    {
        defView = FindWindowExW(child, nullptr, L"SHELLDLL_DefView", nullptr);
        if (defView != nullptr)
        {
            return defView;
        }
    }

    return nullptr;
}
