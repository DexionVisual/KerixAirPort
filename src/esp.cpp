#include "esp.h"
#include "common.h"
#include "theme.h"

#include <imgui_internal.h>

namespace esp {

std::vector<game::PlayerData>& lastPlayers() {
    static std::vector<game::PlayerData> v;
    return v;
}

namespace {

ImU32 teamColor(const game::PlayerData& p) {
    if (p.disguised && g_cfg.showDisguise)
        return ImGui::ColorConvertFloat4ToU32(ImVec4(g_cfg.colDisg[0], g_cfg.colDisg[1], g_cfg.colDisg[2], 1.0f));
    return ImGui::ColorConvertFloat4ToU32(ImVec4(
        p.isAgent ? g_cfg.colCop[0] : g_cfg.colSmg[0],
        p.isAgent ? g_cfg.colCop[1] : g_cfg.colSmg[1],
        p.isAgent ? g_cfg.colCop[2] : g_cfg.colSmg[2],
        1.0f));
}

const char* teamTag(const game::PlayerData& p) {
    if (p.disguised && g_cfg.showDisguise) return "SMG*";
    return p.isAgent ? "COP" : "SMG";
}

void cornerBox(ImDrawList* dl, float x, float y, float w, float h, ImU32 col) {
    const float len = ImClamp(w * 0.28f, 3.0f, 14.0f);
    const float th  = 1.6f;


    dl->AddRect(ImVec2(x - 1, y - 1), ImVec2(x + w + 1, y + h + 1), IM_COL32(0, 0, 0, 170), 0, 0, th + 1.2f);


    const float rx = x + w, by = y + h;
    dl->AddLine(ImVec2(x, y),         ImVec2(x + len, y),        col, th);
    dl->AddLine(ImVec2(x, y),         ImVec2(x, y + len),        col, th);
    dl->AddLine(ImVec2(rx - len, y),  ImVec2(rx, y),             col, th);
    dl->AddLine(ImVec2(rx, y),        ImVec2(rx, y + len),       col, th);
    dl->AddLine(ImVec2(x, by),        ImVec2(x + len, by),       col, th);
    dl->AddLine(ImVec2(x, by - len),  ImVec2(x, by),             col, th);
    dl->AddLine(ImVec2(rx - len, by), ImVec2(rx, by),            col, th);
    dl->AddLine(ImVec2(rx, by - len), ImVec2(rx, by),            col, th);
}

void boxGlow(ImDrawList* dl, float x, float y, float w, float h, ImU32 col) {
    const ImVec4 c = ImGui::ColorConvertU32ToFloat4(col);
    for (int i = 4; i >= 1; --i) {
        const float t = (float)i / 4.0f;
        const float e = 1.5f + t * 7.0f;
        const float a = 0.22f * (1.0f - t) * (1.0f - t);
        dl->AddRect(ImVec2(x - e, y - e), ImVec2(x + w + e, y + h + e),
                    ImGui::ColorConvertFloat4ToU32(ImVec4(c.x, c.y, c.z, a)), 3.0f, 0, 2.0f);
    }
}

}

void draw(const std::vector<game::PlayerData>& players) {
    if (!g_cfg.espEnabled || players.empty()) return;

    ImDrawList* dl = ImGui::GetForegroundDrawList();

    std::lock_guard<std::mutex> lk(g_cfgMtx);

    for (const auto& p : players) {
        if (!p.valid || !p.onScreen || !p.alive) continue;
        if (p.dist > g_cfg.maxDistance) continue;

        const ImU32 col = teamColor(p);

        const float top = p.head[1];
        const float bot = p.feet[1];
        const float h = ImMax(bot - top, 8.0f);
        const float w = ImMax(h * 0.46f, 8.0f);
        const float x = (p.feet[0] + p.head[0]) * 0.5f - w * 0.5f;
        const float y = top;

        if (g_cfg.boxGlow)
            boxGlow(dl, x, y, w, h, col);
        if (g_cfg.box)
            cornerBox(dl, x, y, w, h, col);

        const float cx = x + w * 0.5f;

        if (g_cfg.names && p.name[0]) {
            const ImVec2 ts = ImGui::CalcTextSize(p.name);
            dl->AddText(ImVec2(cx - ts.x * 0.5f + 1, y - ts.y - 5 + 1), IM_COL32(0, 0, 0, 190), p.name);
            dl->AddText(ImVec2(cx - ts.x * 0.5f, y - ts.y - 5), IM_COL32(235, 238, 247, 255), p.name);
        }


        {
            const char* tag = teamTag(p);
            const ImVec2 ts = ImGui::CalcTextSize(tag);
            dl->AddText(ImVec2(x + w + 3.0f, y + 1.0f), (col & 0x00FFFFFFu) | 0xC8000000u, tag);
        }

        if (g_cfg.snaplines) {
            const ImVec2 disp = ImGui::GetIO().DisplaySize;
            dl->AddLine(ImVec2(disp.x * 0.5f, disp.y), ImVec2(cx, bot),
                        (col & 0x00FFFFFFu) | 0x90000000u, 1.0f);
        }
    }
}

}
