#pragma once
#include <windows.h>
#include <cstdint>
#include <atomic>
#include <mutex>
#include <vector>
#include <string>
#include <imgui.h>

struct Vec3 {
    float x, y, z;
};

struct Config {

    bool    espEnabled     = true;
    bool    box            = true;
    bool    boxGlow        = true;
    bool    names          = true;
    bool    snaplines      = false;
    bool    showDisguise   = true;
    float   maxDistance    = 200.0f;

    float   colCop[4]      = { 0.255f, 0.412f, 0.882f, 1.0f };
    float   colSmg[4]      = { 1.0f, 0.624f, 0.262f, 1.0f };
    float   colDisg[4]     = { 0.706f, 0.361f, 1.0f, 1.0f };

    bool    menuOpen       = true;
    int     menuTab        = 0;
    UINT    menuKey        = VK_INSERT;
};

extern Config        g_cfg;
extern std::mutex    g_cfgMtx;


constexpr ImU32 ACCENT_ROYAL  = IM_COL32(0x41, 0x69, 0xE1, 255);
constexpr ImU32 ACCENT_VIOLET = IM_COL32(0x7C, 0x4D, 0xE0, 255);
constexpr ImU32 ACCENT_DEEP   = IM_COL32(0x2B, 0x3A, 0x8C, 255);
