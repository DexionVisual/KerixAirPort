#include "hooks.h"
#include "overlay.h"
#include <MinHook.h>
#include <kiero.h>
#include <dxgi1_4.h>

namespace hooks {

using PresentFn = HRESULT(WINAPI*)(IDXGISwapChain*, UINT, UINT);

PresentFn  g_origPresent = nullptr;
int        g_mode        = 0;

static HRESULT WINAPI hkPresent(IDXGISwapChain* sc, UINT sync, UINT flags) {
    overlay::frame(sc, sync, flags);
    if (!g_origPresent) return E_FAIL;
    return g_origPresent(sc, sync, flags);
}


using CreateSwapFn1  = HRESULT(WINAPI*)(IDXGIFactory*, IUnknown*, DXGI_SWAP_CHAIN_DESC*, IDXGISwapChain**);
using CreateSwapFnH  = HRESULT(WINAPI*)(IDXGIFactory*, IUnknown*, HWND, DXGI_SWAP_CHAIN_DESC1*, const DXGI_SWAP_CHAIN_FULLSCREEN_DESC*, IDXGISwapChain1**);
using CreateSwapFnCW = HRESULT(WINAPI*)(IDXGIFactory*, IUnknown*, IUnknown*, DXGI_SWAP_CHAIN_DESC1*, const DXGI_SWAP_CHAIN_FULLSCREEN_DESC*, IDXGISwapChain1**);

static CreateSwapFn1  g_origCreate1  = nullptr;
static CreateSwapFnH  g_origCreateH  = nullptr;
static CreateSwapFnCW g_origCreateCW = nullptr;
static bool           g_presentHooked = false;

static bool patch_present(IDXGISwapChain* sc) {
    if (g_presentHooked || !sc) return false;
    void** vtbl = *reinterpret_cast<void***>(sc);
    if (MH_CreateHook(vtbl[8], &hkPresent, reinterpret_cast<void**>(&g_origPresent)) != MH_OK)
        return false;
    if (MH_EnableHook(vtbl[8]) != MH_OK) return false;
    g_presentHooked = true;
    return true;
}

static void capture_swapchain(IDXGISwapChain* sc) {
    if (!sc || g_presentHooked) return;
    patch_present(sc);
}

static HRESULT WINAPI hkCreateSwapChain(IDXGIFactory* f, IUnknown* dev, DXGI_SWAP_CHAIN_DESC* desc, IDXGISwapChain** out) {
    HRESULT hr = g_origCreate1(f, dev, desc, out);
    if (SUCCEEDED(hr)) capture_swapchain(*out);
    return hr;
}
static HRESULT WINAPI hkCreateSwapChainForHwnd(IDXGIFactory* f, IUnknown* dev, HWND hwnd, DXGI_SWAP_CHAIN_DESC1* desc, const DXGI_SWAP_CHAIN_FULLSCREEN_DESC* full, IDXGISwapChain1** out) {
    HRESULT hr = g_origCreateH(f, dev, hwnd, desc, full, out);
    if (SUCCEEDED(hr)) capture_swapchain(*out);
    return hr;
}
static HRESULT WINAPI hkCreateSwapChainForCoreWindow(IDXGIFactory* f, IUnknown* dev, IUnknown* wnd, DXGI_SWAP_CHAIN_DESC1* desc, const DXGI_SWAP_CHAIN_FULLSCREEN_DESC* full, IDXGISwapChain1** out) {
    HRESULT hr = g_origCreateCW(f, dev, wnd, desc, full, out);
    if (SUCCEEDED(hr)) capture_swapchain(*out);
    return hr;
}

static bool install_dxgi_fallback() {
    IDXGIFactory* factory = nullptr;
    if (FAILED(CreateDXGIFactory1(__uuidof(IDXGIFactory), reinterpret_cast<void**>(&factory))))
        return false;

    void** vtbl = *reinterpret_cast<void***>(factory);
    factory->Release();

    struct Slot { void* target; void* detour; void** orig; };
    Slot slots[] = {
        { vtbl[10], &hkCreateSwapChain,              reinterpret_cast<void**>(&g_origCreate1) },
        { vtbl[15], &hkCreateSwapChainForHwnd,       reinterpret_cast<void**>(&g_origCreateH) },
        { vtbl[16], &hkCreateSwapChainForCoreWindow, reinterpret_cast<void**>(&g_origCreateCW) },
    };
    int active = 0;
    for (auto& s : slots) {
        if (MH_CreateHook(s.target, s.detour, s.orig) == MH_OK && MH_EnableHook(s.target) == MH_OK)
            ++active;
    }
    if (!active) return false;
    g_mode = 2;
    return true;
}

bool kiero_try() {


    const kiero::RenderType::Enum api = GetModuleHandleA("d3d12.dll")
        ? kiero::RenderType::D3D12
        : kiero::RenderType::D3D11;

    kiero::Status::Enum st = kiero::init(api);
    if (st != kiero::Status::Success)
        return false;


    const uint16_t presentIdx = (api == kiero::RenderType::D3D12) ? 140 : 8;
    st = kiero::bind(presentIdx, reinterpret_cast<void**>(&g_origPresent), &hkPresent);
    return st == kiero::Status::Success;
}

bool install() {


    MH_Initialize();
    if (kiero_try()) {
        g_mode = 1;
        return true;
    }

    kiero::shutdown();
    return install_dxgi_fallback();
}

void uninstall() {
    if (g_mode == 1) {
        kiero::unbind(140);
        kiero::shutdown();
    } else if (g_mode == 2) {
        if (g_presentHooked) {
            IDXGIFactory* factory = nullptr;
            if (SUCCEEDED(CreateDXGIFactory1(__uuidof(IDXGIFactory), reinterpret_cast<void**>(&factory)))) {
                void** vtbl = *reinterpret_cast<void***>(factory);
                factory->Release();
                MH_DisableHook(vtbl[10]); MH_DisableHook(vtbl[11]); MH_DisableHook(vtbl[12]);
            }
        }
    }
    MH_Uninitialize();
    g_mode = 0;
}

}
