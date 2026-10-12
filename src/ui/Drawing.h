#pragma once

#include <span>
#include <string_view>

#include <imgui.h>

#include "model/Definition.h"

// What the parts of the panel draw with, in the look of the design system.

namespace mods_config::ui
{
    float textWidth(std::string_view text);
    void drawText(float left, float right, float top, float height, ImU32 color, std::string_view text);
    float drawTextLines(std::span<const model::TextLine> lines, const ImVec2& pos, float width, ImU32 color);
    void sectionHeading(std::string_view text);
    void drawOutline(const ImVec2& min, const ImVec2& max, ImU32 color);
    void drawRowState();
}
