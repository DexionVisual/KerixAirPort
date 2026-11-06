#include "theme.h"
#include "common.h"

namespace theme {

void apply() {
    ImGuiStyle& s = ImGui::GetStyle();

    s.WindowRounding    = 8.0f;
    s.ChildRounding     = 8.0f;
    s.FrameRounding     = 6.0f;
    s.GrabRounding      = 6.0f;
    s.PopupRounding     = 8.0f;
    s.ScrollbarRounding = 6.0f;
    s.TabRounding       = 6.0f;

    s.WindowBorderSize  = 0.0f;
    s.ChildBorderSize   = 0.0f;
    s.FrameBorderSize   = 0.0f;
    s.PopupBorderSize   = 0.0f;

    s.WindowPadding     = ImVec2(0, 0);
    s.FramePadding      = ImVec2(6, 3);
    s.ItemSpacing       = ImVec2(8, 5);
    s.ItemInnerSpacing  = ImVec2(6, 3);
    s.CellPadding       = ImVec2(6, 3);
    s.ScrollbarSize     = 10.0f;
    s.GrabMinSize       = 10.0f;

    ImVec4* c = s.Colors;
    auto col = [](ImU32 v) {
        return ImVec4(
            ((v >> IM_COL32_R_SHIFT) & 0xFF) / 255.0f,
            ((v >> IM_COL32_G_SHIFT) & 0xFF) / 255.0f,
            ((v >> IM_COL32_B_SHIFT) & 0xFF) / 255.0f,
            ((v >> IM_COL32_A_SHIFT) & 0xFF) / 255.0f);
    };

    constexpr ImU32 bg      = IM_COL32(0x0D, 0x0E, 0x14, 0xFA);
    constexpr ImU32 panel   = IM_COL32(0x13, 0x15, 0x1E, 0xFF);
    constexpr ImU32 frame   = IM_COL32(0x1A, 0x1D, 0x28, 0xFF);
    constexpr ImU32 frameH  = IM_COL32(0x22, 0x26, 0x35, 0xFF);
    constexpr ImU32 frameA  = IM_COL32(0x2A, 0x2F, 0x42, 0xFF);
    constexpr ImU32 text    = IM_COL32(0xE4, 0xE7, 0xF2, 0xFF);
    constexpr ImU32 textDim = IM_COL32(0x84, 0x8B, 0xA3, 0xFF);

    c[ImGuiCol_WindowBg]         = col(bg);
    c[ImGuiCol_ChildBg]          = col(IM_COL32(0, 0, 0, 0));
    c[ImGuiCol_PopupBg]          = col(bg);
    c[ImGuiCol_Text]             = col(text);
    c[ImGuiCol_TextDisabled]     = col(textDim);
    c[ImGuiCol_FrameBg]          = col(frame);
    c[ImGuiCol_FrameBgHovered]   = col(frameH);
    c[ImGuiCol_FrameBgActive]    = col(frameA);
    c[ImGuiCol_TitleBg]          = col(bg);
    c[ImGuiCol_TitleBgActive]    = col(bg);
    c[ImGuiCol_ScrollbarBg]      = col(IM_COL32(0, 0, 0, 0));
    c[ImGuiCol_ScrollbarGrab]    = col(frameA);
    c[ImGuiCol_ScrollbarGrabHovered] = col(IM_COL32(0x3A, 0x41, 0x5A, 0xFF));
    c[ImGuiCol_ScrollbarGrabActive]  = col(ACCENT_ROYAL);

    c[ImGuiCol_CheckMark]        = col(ACCENT_ROYAL);
    c[ImGuiCol_SliderGrab]       = col(ACCENT_ROYAL);
    c[ImGuiCol_SliderGrabActive] = col(ACCENT_VIOLET);
    c[ImGuiCol_Separator]        = col(IM_COL32(0x24, 0x28, 0x36, 0xFF));
    c[ImGuiCol_ResizeGrip]       = col(0);
    c[ImGuiCol_ResizeGripHovered]  = col(ACCENT_ROYAL);
    c[ImGuiCol_ResizeGripActive]   = col(ACCENT_VIOLET);

    c[ImGuiCol_Button]           = col(frame);
    c[ImGuiCol_ButtonHovered]    = col(frameH);
    c[ImGuiCol_ButtonActive]     = col(frameA);

    c[ImGuiCol_Header]           = col(IM_COL32(0, 0, 0, 0));
    c[ImGuiCol_HeaderHovered]    = col(frameH);
    c[ImGuiCol_HeaderActive]     = col(frameA);

    c[ImGuiCol_TableHeaderBg]    = col(panel);
    c[ImGuiCol_TableRowBg]       = col(0);
    c[ImGuiCol_TableRowBgAlt]    = col(IM_COL32(255, 255, 255, 6));


    c[ImGuiCol_NavCursor]        = col(ACCENT_ROYAL);
}

void glow(ImDrawList* dl, const ImVec2& mn, const ImVec2& mx, float rounding,
          ImU32 inner, ImU32 outer, int layers) {
    const ImVec4 ci = ImGui::ColorConvertU32ToFloat4(inner);
    const ImVec4 co = ImGui::ColorConvertU32ToFloat4(outer);

    for (int i = layers; i >= 1; --i) {
        const float t = (float)i / (float)layers;
        const float e = 1.8f + t * 16.0f;
        const float a = 0.34f * (1.0f - t) * (1.0f - t) + 0.025f;
        const ImVec4& base = (i % 2) ? ci : co;
        const ImU32 c = ImGui::ColorConvertFloat4ToU32(ImVec4(base.x, base.y, base.z, a));
        dl->AddRect(ImVec2(mn.x - e, mn.y - e), ImVec2(mx.x + e, mx.y + e),
                    c, rounding + e * 0.5f, 0, 2.4f);
    }


    dl->AddRect(mn, mx, inner, rounding, 0, 1.4f);
    dl->AddRect(ImVec2(mn.x - 1, mn.y - 1), ImVec2(mx.x + 1, mx.y + 1),
                (inner & 0x00FFFFFFu) | 0x50000000u, rounding, 0, 1.0f);
}

}
