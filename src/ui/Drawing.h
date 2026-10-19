#pragma once

#include <cstdint>
#include <span>
#include <string_view>

#include <imgui.h>
#include <imgui_internal.h>

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

    /**
     * How the value of a setting looks: as it is saved, pending a Save in amber, or invalid in red.
     */
    enum class ValueLook : std::uint8_t
    {
        Saved,
        Pending,
        Invalid,
    };

    // the track of a slider, and the box of a stepper's option, in the panel's pixels: not a token of the
    // design system
    constexpr float TRACK_WIDTH = 280.0f;

    // the strokes of a drawn icon, as the design system draws its icons in a box of 24
    constexpr float ICON_THICKNESS = 2.5f;

    float mcmScale();
    float textWidth(std::string_view text);
    void drawText(float left, float right, float top, float height, ImU32 color, std::string_view text);
    void drawTextRuns(float left, float right, float top, float height, ImU32 color, std::span<const model::TextRun> runs);
    float drawTextLines(std::span<const model::TextLine> lines, const ImVec2& pos, float width, ImU32 color);
    int countTextLines(std::span<const TextPiece> pieces, float width);
    void drawTextPieces(std::span<const TextPiece> pieces, const ImVec2& pos, float width, int maxLines);
    void sectionHeading(std::string_view text);
    void drawOutline(const ImVec2& min, const ImVec2& max, ImU32 color);
    void drawDashedOutline(const ImVec2& min, const ImVec2& max, ImU32 color);
    void drawRowState();
    ImRect controlBox(float left, float top, float width);
    void drawFrame(const ImRect& box, ValueLook look = ValueLook::Saved);
    void drawButtonFrame(const ImRect& box, bool pointed, bool held);
    void drawTextIn(const ImRect& box, std::string_view text, ImU32 color, float align, float padding = 0.0f);
    ImU32 valueColor(ValueLook look);
    void drawPendingMark(const ImVec2& center);
    void drawInvalidMark(const ImVec2& center);
}
