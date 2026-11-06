#include "common.h"
#include "hooks.h"
#include "game/game.h"
#include "game/il2cpp.h"

#include <thread>

Config        g_cfg;
std::mutex    g_cfgMtx;


static void log_step(const char* what, bool ok) {
    FILE* f = nullptr;
    fopen_s(&f, "C:\\Users\\win11\\Desktop\\Kerix\\Airport\\kerix.log", "a");
    if (f) {
        SYSTEMTIME st;
        GetLocalTime(&st);
        fprintf(f, "[%02u:%02u:%02u] %-26s %s\r\n", st.wHour, st.wMinute, st.wSecond,
                what, ok ? "ok" : "fail");
        fclose(f);
    }
}

static DWORD WINAPI main_thread(LPVOID) {
    bool ok = false;

    for (int i = 0; i < 300; ++i) {
        if (il2cpp::init() && game::try_init()) { ok = true; break; }
        Sleep(500);
    }
    log_step("game attach", ok);


    ok = false;
    for (int i = 0; i < 300; ++i) {
        const bool d3d12 = GetModuleHandleA("d3d12.dll") != nullptr;
        const bool d3d11 = GetModuleHandleA("d3d11.dll") != nullptr;
        if (d3d12 || d3d11) {
            log_step(d3d12 ? "render api d3d12" : "render api d3d11", true);
            log_step("render hook", ok = hooks::install());
            break;
        }
        Sleep(500);
    }
    log_step("render hook done", ok);
    return 0;
}

BOOL APIENTRY DllMain(HMODULE mod, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(mod);
        CreateThread(nullptr, 0, main_thread, nullptr, 0, nullptr);
    } else if (reason == DLL_PROCESS_DETACH) {
        game::shutdown();
        hooks::uninstall();
    }
    return TRUE;
}
