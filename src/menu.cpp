#include "menu.h"
#include "common.h"
#include "theme.h"

#include <cstdio>

namespace menu {

namespace {

ImFont* g_bold = nullptr;

constexpr float kWinW  = 470.0f;
constexpr float kWinH  = 330.0f;
constexpr float kNavW  = 130.0f;
constexpr float kHeadH = 50.0f;
constexpr float kPad   = 14.0f;

struct NavItem { const char* label; int id; };
constexpr NavItem kNav[] = {
    { "esp",    0 },
    { "colors", 1 },
};


void drawHeader(ImDrawList* dl, const ImVec2& wp) {
    const ImVec2 tp(wp.x + kPad + 2.0f, wp.y + 12.0f);
    if (g_bold) dl->AddText(g_bold, 17.0f, tp, IM_COL32(236, 239, 250, 255), "KERIX");
    else        dl->AddText(tp, IM_COL32(236, 239, 250, 255), "KERIX");

    const ImVec2 ksz = ImGui::CalcTextSize("KERIX");
    dl->AddText(ImVec2(tp.x + ksz.x + 9.0f, tp.y + 2.0f), IM_COL32(0x84, 0x8B, 0xA3, 255), "airport");


    const float ly = wp.y + 42.0f;
    dl->AddRectFilledMultiColor(
        ImVec2(wp.x, ly), ImVec2(wp.x + kWinW * 0.62f, ly + 2.0f),
        ACCENT_ROYAL, ACCENT_VIOLET, ACCENT_VIOLET, ACCENT_ROYAL);
    dl->AddRectFilled(ImVec2(wp.x + kWinW * 0.62f, ly), ImVec2(wp.x + kWinW, ly + 2.0f),
                      IM_COL32(255, 255, 255, 14));
}

void navButton(const NavItem& it, bool active, float w) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float h = ImGui::GetTextLineHeight() + 10.0f;
    const ImVec2 p = ImGui::GetCursorScreenPos();

    ImGui::PushID(it.label);
    ImGui::InvisibleButton(it.label, ImVec2(w, h));
    const bool hovered = ImGui::IsItemHovered();
    const bool pressed = ImGui::IsItemClicked();
    ImGui::PopID();

    if (active) {
        dl->AddRectFilled(p, ImVec2(p.x + w, p.y + h), IM_COL32(0x41, 0x69, 0xE1, 0x30), 6.0f);
        dl->AddRectFilled(p, ImVec2(p.x + 3.0f, p.y + h), ACCENT_ROYAL, 2.0f);
    } else if (hovered) {
        dl->AddRectFilled(p, ImVec2(p.x + w, p.y + h), IM_COL32(255, 255, 255, 10), 6.0f);
    }
    dl->AddText(ImVec2(p.x + 13.0f, p.y + 5.0f),
                active ? IM_COL32(0xDB, 0xE2, 0xFF, 255) : IM_COL32(0x8D, 0x94, 0xAC, 255),
                it.label);

    ImGui::Spacing();
    if (pressed) g_cfg.menuTab = it.id;
}

void bodyEsp() {
    ImGui::Checkbox("enabled", &g_cfg.espEnabled);
    ImGui::Separator();
    ImGui::BeginDisabled(!g_cfg.espEnabled);
    ImGui::Checkbox("boxes", &g_cfg.box);
    ImGui::Checkbox("glow", &g_cfg.boxGlow);
    ImGui::Checkbox("names", &g_cfg.names);
    ImGui::Checkbox("snaplines", &g_cfg.snaplines);
    ImGui::Checkbox("disguise mark", &g_cfg.showDisguise);
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::SliderFloat("##range", &g_cfg.maxDistance, 20.0f, 400.0f, "range  %.0f m");
    ImGui::EndDisabled();
}

void bodyColors() {
    ImGui::ColorEdit4("police", g_cfg.colCop, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar);
    ImGui::ColorEdit4("smugglers", g_cfg.colSmg, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar);
    ImGui::ColorEdit4("disguised", g_cfg.colDisg, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar);
}

}

void setBoldFont(ImFont* f) { g_bold = f; }

void draw() {
    std::lock_guard<std::mutex> lk(g_cfgMtx);
    if (!g_cfg.menuOpen) return;

    theme::apply();

    ImGui::SetNextWindowPos(ImVec2(24, 24), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(kWinW, kWinH), ImGuiCond_Always);

    constexpr ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::Begin("##kerix", nullptr, flags);
    ImGui::PopStyleVar();


    {
        const ImVec2 p = ImGui::GetWindowPos();
        ImDrawList* bg = ImGui::GetBackgroundDrawList();
        theme::glow(bg, p, ImVec2(p.x + kWinW, p.y + kWinH), 9.0f,
                    ACCENT_ROYAL, ACCENT_VIOLET);
    }

    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 wp = ImGui::GetWindowPos();
    drawHeader(dl, wp);


    const float bodyTop = kHeadH + 8.0f;
    ImGui::SetCursorPos(ImVec2(kPad, bodyTop));
    ImGui::BeginChild("nav", ImVec2(kNavW - kPad, kWinH - bodyTop - kPad), ImGuiChildFlags_None);
    for (const auto& it : kNav)
        navButton(it, g_cfg.menuTab == it.id, kNavW - kPad - 10.0f);
    ImGui::EndChild();


    ImGui::SetCursorPos(ImVec2(kNavW + 4.0f, bodyTop + 2.0f));
    ImGui::BeginChild("body", ImVec2(kWinW - kNavW - 18.0f, kWinH - bodyTop - kPad),
                      ImGuiChildFlags_None);
    ImGui::PushItemWidth(-16.0f);
    switch (g_cfg.menuTab) {
    case 0: bodyEsp(); break;
    case 1: bodyColors(); break;
    }
    ImGui::PopItemWidth();
    ImGui::EndChild();

    ImGui::End();
}

}
