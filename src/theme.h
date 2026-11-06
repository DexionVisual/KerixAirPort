#pragma once
#include <imgui.h>

namespace theme {

void apply();


void glow(ImDrawList* dl, const ImVec2& mn, const ImVec2& mx, float rounding,
          ImU32 inner, ImU32 outer, int layers = 7);

}
