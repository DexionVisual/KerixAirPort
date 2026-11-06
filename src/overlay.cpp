#include "overlay.h"
#include "common.h"
#include "esp.h"
#include "menu.h"
#include "game/game.h"

#include <d3d11.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <imgui.h>
#include <backends/imgui_impl_dx11.h>
#include <backends/imgui_impl_dx12.h>
#include <backends/imgui_impl_win32.h>


extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace overlay {

namespace {

constexpr UINT kMaxBuffers = 8;
enum class Api { None, D3D11, D3D12 };

struct Slot {
    void* buffer = nullptr;
    union {
        D3D12_CPU_DESCRIPTOR_HANDLE rtv12;
        ID3D11RenderTargetView*    rtv11;
    };
};

struct Ctx {
    Api     api      = Api::None;
    ID3D12Device* dev12 = nullptr;
    ID3D11Device* dev11 = nullptr;
    IDXGISwapChain3* sc3 = nullptr;
    HWND    hwnd     = nullptr;
    UINT    bufCount = 0;
    DXGI_FORMAT fmt  = DXGI_FORMAT_UNKNOWN;
    UINT    width = 0, height = 0;


    ID3D12CommandQueue*        q12   = nullptr;
    ID3D12CommandAllocator*    a12   = nullptr;
    ID3D12GraphicsCommandList* l12   = nullptr;
    ID3D12Fence*               f12   = nullptr;
    HANDLE                     fEv12 = nullptr;
    UINT64                     fVal12 = 0;
    ID3D12DescriptorHeap*      rtvHeap12 = nullptr;
    UINT                       rtvInc12 = 0;
    ID3D12DescriptorHeap*      srvHeap12 = nullptr;
    UINT                       srvInc12 = 0;
    UINT                       srvNext = 0;


    ID3D11DeviceContext* ctx11 = nullptr;

    Slot slots[kMaxBuffers];

    bool     ready       = false;
    bool     imguiInit   = false;
    LONG_PTR origWndProc = 0;
};

Ctx g;
std::mutex g_initMtx;

void stage(const char* what) {
    FILE* f = nullptr;
    fopen_s(&f, "C:\\Users\\win11\\Desktop\\Kerix\\Airport\\kerix.log", "a");
    if (f) {
        SYSTEMTIME st;
        GetLocalTime(&st);
        fprintf(f, "[%02u:%02u:%02u] %-26s ok\r\n", st.wHour, st.wMinute, st.wSecond, what);
        fclose(f);
    }
}

LRESULT WINAPI wndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (ImGui::GetCurrentContext()) {
        if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wp, lp))
            return 1;

        if (msg == WM_KEYDOWN && wp == g_cfg.menuKey) {
            std::lock_guard<std::mutex> lk(g_cfgMtx);
            g_cfg.menuOpen = !g_cfg.menuOpen;
            return 1;
        }

        if (g_cfg.menuOpen) {

            switch (msg) {
            case WM_INPUT:
            case WM_LBUTTONDOWN: case WM_LBUTTONUP:
            case WM_RBUTTONDOWN: case WM_RBUTTONUP:
            case WM_MBUTTONDOWN: case WM_MBUTTONUP:
            case WM_MOUSEMOVE:  case WM_MOUSEWHEEL: case WM_MOUSEHWHEEL:
                return 1;
            default: break;
            }
        }
    }
    return CallWindowProcW(reinterpret_cast<WNDPROC>(g.origWndProc), hwnd, msg, wp, lp);
}


UINT currentBuffer(IDXGISwapChain* sc);

void srvAlloc(ImGui_ImplDX12_InitInfo*, D3D12_CPU_DESCRIPTOR_HANDLE* cpu, D3D12_GPU_DESCRIPTOR_HANDLE* gpu) {
    const D3D12_CPU_DESCRIPTOR_HANDLE baseCpu = g.srvHeap12->GetCPUDescriptorHandleForHeapStart();
    const D3D12_GPU_DESCRIPTOR_HANDLE baseGpu = g.srvHeap12->GetGPUDescriptorHandleForHeapStart();
    cpu->ptr = baseCpu.ptr + (UINT64)g.srvNext * g.srvInc12;
    gpu->ptr = baseGpu.ptr + (UINT64)g.srvNext * g.srvInc12;
    ++g.srvNext;
}
void srvFree(ImGui_ImplDX12_InitInfo*, D3D12_CPU_DESCRIPTOR_HANDLE, D3D12_GPU_DESCRIPTOR_HANDLE) {}

void flushGpu12() {
    if (!g.q12 || !g.f12) return;
    ++g.fVal12;
    g.q12->Signal(g.f12, g.fVal12);
    if (g.f12->GetCompletedValue() < g.fVal12) {
        g.f12->SetEventOnCompletion(g.fVal12, g.fEv12);
        WaitForSingleObject(g.fEv12, 2000);
    }
}

bool initDx12(ID3D12Device* device, IDXGISwapChain* sc) {
    g.dev12 = device;

    D3D12_COMMAND_QUEUE_DESC qd{};
    qd.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    if (FAILED(device->CreateCommandQueue(&qd, __uuidof(ID3D12CommandQueue), reinterpret_cast<void**>(&g.q12))))
        return false;
    if (FAILED(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, __uuidof(ID3D12CommandAllocator), reinterpret_cast<void**>(&g.a12))))
        return false;
    if (FAILED(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, g.a12, nullptr, __uuidof(ID3D12GraphicsCommandList), reinterpret_cast<void**>(&g.l12))))
        return false;
    g.l12->Close();
    if (FAILED(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, __uuidof(ID3D12Fence), reinterpret_cast<void**>(&g.f12))))
        return false;
    g.fEv12 = CreateEventW(nullptr, FALSE, FALSE, nullptr);

    D3D12_DESCRIPTOR_HEAP_DESC hd{};
    hd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    hd.NumDescriptors = 64;
    hd.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    if (FAILED(device->CreateDescriptorHeap(&hd, __uuidof(ID3D12DescriptorHeap), reinterpret_cast<void**>(&g.srvHeap12))))
        return false;
    g.srvInc12 = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    ImGui_ImplWin32_Init(g.hwnd);
    ImGui_ImplDX12_InitInfo info;
    info.Device = device;
    info.CommandQueue = g.q12;
    info.NumFramesInFlight = g.bufCount;
    info.RTVFormat = g.fmt;
    info.DSVFormat = DXGI_FORMAT_UNKNOWN;
    info.SrvDescriptorHeap = g.srvHeap12;
    info.SrvDescriptorAllocFn = srvAlloc;
    info.SrvDescriptorFreeFn = srvFree;
    if (!ImGui_ImplDX12_Init(&info)) {
        ImGui_ImplWin32_Shutdown();
        return false;
    }
    return true;
}

void renderDx12(IDXGISwapChain* sc) {
    ImGui_ImplDX12_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    esp::draw(esp::lastPlayers());
    menu::draw();
    ImGui::Render();

    g.a12->Reset();
    g.l12->Reset(g.a12, nullptr);

    ID3D12DescriptorHeap* heaps[] = { g.srvHeap12 };
    g.l12->SetDescriptorHeaps(1, heaps);

    const UINT idx = currentBuffer(sc);
    if (!g.slots[idx].buffer) return;

    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = static_cast<ID3D12Resource*>(g.slots[idx].buffer);
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    barrier.Transition.StateAfter  = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    g.l12->ResourceBarrier(1, &barrier);

    D3D12_CPU_DESCRIPTOR_HANDLE rtv = g.slots[idx].rtv12;
    g.l12->OMSetRenderTargets(1, &rtv, FALSE, nullptr);

    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), g.l12);

    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.StateAfter  = D3D12_RESOURCE_STATE_PRESENT;
    g.l12->ResourceBarrier(1, &barrier);
    g.l12->Close();

    g.q12->ExecuteCommandLists(1, reinterpret_cast<ID3D12CommandList* const*>(&g.l12));
}


bool initDx11(ID3D11Device* device, ID3D11DeviceContext* context) {
    g.dev11 = device;
    g.ctx11 = context;
    ImGui_ImplWin32_Init(g.hwnd);
    if (!ImGui_ImplDX11_Init(device, context)) {
        ImGui_ImplWin32_Shutdown();
        return false;
    }
    return true;
}

void renderDx11(IDXGISwapChain* sc) {
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    esp::draw(esp::lastPlayers());
    menu::draw();
    ImGui::Render();

    const UINT idx = currentBuffer(sc);
    if (g.slots[idx].rtv11) {
        g.ctx11->OMSetRenderTargets(1, &g.slots[idx].rtv11, nullptr);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    }
}


UINT currentBuffer(IDXGISwapChain* sc) {
    if (g.sc3) return g.sc3->GetCurrentBackBufferIndex();
    for (UINT i = 0; i < g.bufCount && i < kMaxBuffers; ++i) {
        ID3D12Resource* res = nullptr;
        if (SUCCEEDED(sc->GetBuffer(i, __uuidof(ID3D12Resource), reinterpret_cast<void**>(&res)))) {
            void* ptr = res;
            res->Release();
            if (g.slots[i].buffer == ptr) return i;
        }
    }
    return 0;
}

void releaseTargets() {
    if (g.api == Api::D3D12) flushGpu12();
    for (UINT i = 0; i < kMaxBuffers; ++i) {
        if (g.api == Api::D3D11 && g.slots[i].rtv11)
            g.slots[i].rtv11->Release();
        g.slots[i] = Slot{};
    }
    if (g.rtvHeap12) { g.rtvHeap12->Release(); g.rtvHeap12 = nullptr; }
}

void ensureTargets(IDXGISwapChain* sc) {
    DXGI_SWAP_CHAIN_DESC desc{};
    if (FAILED(sc->GetDesc(&desc))) return;

    if (desc.BufferDesc.Format != g.fmt || desc.BufferCount != g.bufCount ||
        desc.OutputWindow != g.hwnd || desc.BufferDesc.Width != g.width || desc.BufferDesc.Height != g.height) {
        releaseTargets();
        g.fmt      = desc.BufferDesc.Format;
        g.bufCount = desc.BufferCount;
        g.hwnd     = desc.OutputWindow;
        g.width    = desc.BufferDesc.Width;
        g.height   = desc.BufferDesc.Height;
    }

    if (g.api == Api::D3D12 && !g.rtvHeap12) {
        D3D12_DESCRIPTOR_HEAP_DESC hd{};
        hd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        hd.NumDescriptors = kMaxBuffers;
        hd.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        if (FAILED(g.dev12->CreateDescriptorHeap(&hd, __uuidof(ID3D12DescriptorHeap), reinterpret_cast<void**>(&g.rtvHeap12))))
            return;
        g.rtvInc12 = g.dev12->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    }


    D3D12_CPU_DESCRIPTOR_HANDLE base12 = g.rtvHeap12 ? g.rtvHeap12->GetCPUDescriptorHandleForHeapStart() : D3D12_CPU_DESCRIPTOR_HANDLE{};
    for (UINT i = 0; i < g.bufCount && i < kMaxBuffers; ++i) {
        ID3D12Resource* res12 = nullptr;
        ID3D11Resource* res11 = nullptr;
        if (g.api == Api::D3D12) {
            if (FAILED(sc->GetBuffer(i, __uuidof(ID3D12Resource), reinterpret_cast<void**>(&res12))))
                continue;
        } else if (g.api == Api::D3D11) {
            if (FAILED(sc->GetBuffer(i, __uuidof(ID3D11Resource), reinterpret_cast<void**>(&res11))))
                continue;
        }
        void* ptr = res12 ? (void*)res12 : (void*)res11;
        if (res12) res12->Release();
        if (res11) res11->Release();
        if (g.slots[i].buffer == ptr) continue;

        g.slots[i].buffer = ptr;
        if (g.api == Api::D3D12) {
            g.slots[i].rtv12.ptr = base12.ptr + (UINT64)i * g.rtvInc12;
            if (SUCCEEDED(sc->GetBuffer(i, __uuidof(ID3D12Resource), reinterpret_cast<void**>(&res12)))) {
                g.dev12->CreateRenderTargetView(res12, nullptr, g.slots[i].rtv12);
                res12->Release();
            }
        } else if (g.api == Api::D3D11) {
            if (SUCCEEDED(sc->GetBuffer(i, __uuidof(ID3D11Resource), reinterpret_cast<void**>(&res11)))) {
                g.dev11->CreateRenderTargetView(res11, nullptr, &g.slots[i].rtv11);
                res11->Release();
            }
        }
    }
}

bool initCommon(IDXGISwapChain* sc, void* hwnd) {
    g.hwnd = reinterpret_cast<HWND>(hwnd);
    sc->QueryInterface(__uuidof(IDXGISwapChain3), reinterpret_cast<void**>(&g.sc3));

    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;


    char winDir[MAX_PATH];
    GetWindowsDirectoryA(winDir, MAX_PATH);
    std::string fonts = winDir;
    io.Fonts->AddFontFromFileTTF((fonts + "\\Fonts\\segoeui.ttf").c_str(), 15.0f);
    ImFont* bold = io.Fonts->AddFontFromFileTTF((fonts + "\\Fonts\\segoeuib.ttf").c_str(), 15.0f);
    menu::setBoldFont(bold);

    g.origWndProc = SetWindowLongPtrW(g.hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(wndProc));
    g.imguiInit = true;
    return true;
}

void shutdownOverlay() {
    if (!g.imguiInit) return;
    if (g.api == Api::D3D12) flushGpu12();
    if (g.hwnd && g.origWndProc)
        SetWindowLongPtrW(g.hwnd, GWLP_WNDPROC, g.origWndProc);
    if (g.api == Api::D3D12) ImGui_ImplDX12_Shutdown();
    if (g.api == Api::D3D11) ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    releaseTargets();
    if (g.srvHeap12) g.srvHeap12->Release();
    if (g.l12) g.l12->Release();
    if (g.a12) g.a12->Release();
    if (g.f12) g.f12->Release();
    if (g.q12) g.q12->Release();
    if (g.sc3) g.sc3->Release();
    if (g.fEv12) CloseHandle(g.fEv12);

    g = Ctx{};
}

}

void frame(IDXGISwapChain* sc, UINT sync, UINT flags) {
    if (flags & DXGI_PRESENT_TEST) return;

    if (!g.ready) {
        std::lock_guard<std::mutex> lk(g_initMtx);
        if (g.ready) return;

        DXGI_SWAP_CHAIN_DESC desc{};
        if (FAILED(sc->GetDesc(&desc))) return;
        g.hwnd = desc.OutputWindow;
        g.bufCount = desc.BufferCount;
        g.fmt = desc.BufferDesc.Format;
        g.width = desc.BufferDesc.Width;
        g.height = desc.BufferDesc.Height;


        ID3D12Device* d12 = nullptr;
        ID3D11Device* d11 = nullptr;
        ID3D11DeviceContext* d11ctx = nullptr;
        if (SUCCEEDED(sc->GetDevice(__uuidof(ID3D12Device), reinterpret_cast<void**>(&d12))))
            g.api = Api::D3D12;
        else if (SUCCEEDED(sc->GetDevice(__uuidof(ID3D11Device), reinterpret_cast<void**>(&d11)))) {
            d11->GetImmediateContext(&d11ctx);
            g.api = Api::D3D11;
        } else
            return;

        initCommon(sc, desc.OutputWindow);
        stage(g.api == Api::D3D12 ? "imgui dx12 backend" : "imgui dx11 backend");
        bool ok = (g.api == Api::D3D12) ? initDx12(d12, sc) : initDx11(d11, d11ctx);
        if (d12) d12->Release();
        if (d11) d11->Release();
        if (d11ctx) d11ctx->Release();
        if (!ok) {
            shutdownOverlay();
            return;
        }
        ensureTargets(sc);
        g.ready = true;
        stage("overlay ready");
    }

    ensureTargets(sc);

    if (!g_cfg.menuOpen && !g_cfg.espEnabled)
        return;


    if (g_cfg.espEnabled && game::ready())
        game::build_snapshot(esp::lastPlayers(), (float)g.width, (float)g.height);
    else
        esp::lastPlayers().clear();

    if (g.api == Api::D3D12) {
        flushGpu12();
        renderDx12(sc);
    } else if (g.api == Api::D3D11) {
        renderDx11(sc);
    }
    static bool loggedFirst = false;
    if (!loggedFirst) { loggedFirst = true; stage("first frame drawn"); }
}

}
