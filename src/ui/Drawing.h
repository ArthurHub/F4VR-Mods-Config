#pragma once

#include <imgui.h>

// What the parts of the panel draw with, in the look of the design system.

namespace mods_config::ui
{
    void drawText(float left, float right, float top, float height, ImU32 color, const char* text);
    void drawOutline(const ImVec2& min, const ImVec2& max, ImU32 color);
    void drawRowState();
}
