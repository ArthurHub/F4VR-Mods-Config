#pragma once

#include <span>
#include <string_view>

#include <imgui.h>

#include "model/Definition.h"

// What the parts of the panel draw with, in the look of the design system.

namespace mods_config::ui
{
    /**
     * A piece of a text that is drawn in one color.
     */
    struct TextPiece
    {
        std::string_view text;
        ImU32 color = 0;
    };

    float mcmScale();
    float textWidth(std::string_view text);
    void drawText(float left, float right, float top, float height, ImU32 color, std::string_view text);
    float drawTextLines(std::span<const model::TextLine> lines, const ImVec2& pos, float width, ImU32 color);
    int countTextLines(std::span<const TextPiece> pieces, float width);
    void drawTextPieces(std::span<const TextPiece> pieces, const ImVec2& pos, float width, int maxLines);
    void sectionHeading(std::string_view text);
    void drawOutline(const ImVec2& min, const ImVec2& max, ImU32 color);
    void drawRowState();
}
