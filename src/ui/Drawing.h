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
     * A mark that is a piece of a text, with no text: the warning mark, or the invalid mark.
     */
    enum class TextMark : std::uint8_t
    {
        None,
        Warning,
        Invalid,
    };

    /**
     * A piece of a text that is drawn in one color, or a mark.
     */
    struct TextPiece
    {
        std::string_view text;
        ImU32 color = 0;
        TextMark mark = TextMark::None;
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

    /**
     * How a button of the footer and of the dialog looks, as the design system's Button has it.
     */
    enum class ButtonLook : std::uint8_t
    {
        // Save: a solid green
        Primary,
        // Discard: an outline
        Secondary,
        // Cancel: no outline until the ray is on it
        Quiet,
    };

    // the track of a slider, and the box of a stepper's option, in the panel's pixels: not a token of the
    // design system
    constexpr float TRACK_WIDTH = 280.0f;

    // the strokes of a drawn icon, as the design system draws its icons in a box of 24
    constexpr float ICON_THICKNESS = 2.5f;

    // the box of the arrow of a row that opens and closes, as a mod's row of the tree and a section that folds
    constexpr float FOLD_ARROW_SIZE = 14.0f;

    // the box of the warning mark and of the invalid mark, as the design system draws them
    constexpr float WARNING_MARK_WIDTH = 20.0f;

    float mcmScale();
    float textWidth(std::string_view text);
    void drawText(float left, float right, float top, float height, ImU32 color, std::string_view text);
    float drawTextRuns(float left, float right, float top, float height, ImU32 color, std::span<const model::TextRun> runs);
    float drawTextLines(std::span<const model::TextLine> lines, const ImVec2& pos, float width, ImU32 color);
    int countTextLines(std::span<const TextPiece> pieces, float width);
    void drawTextPieces(std::span<const TextPiece> pieces, const ImVec2& pos, float width, int maxLines);
    void sectionHeading(std::string_view text, float right);
    void drawOutline(const ImVec2& min, const ImVec2& max, ImU32 color);
    void drawDashedOutline(const ImVec2& min, const ImVec2& max, ImU32 color);
    void drawRowState();
    ImRect controlBox(float left, float top, float width);
    void drawFrame(const ImRect& box, ValueLook look = ValueLook::Saved);
    void drawButtonFrame(const ImRect& box, bool pointed, bool held);
    bool button(const char* label, const ImRect& box, ButtonLook look);
    void drawTextIn(const ImRect& box, std::string_view text, ImU32 color, float align, float padding = 0.0f);
    ImU32 valueColor(ValueLook look);
    void drawPendingMark(const ImVec2& center);
    void drawInvalidMark(const ImVec2& center);
    void drawWarningMark(const ImVec2& center);
    void drawFoldArrow(const ImVec2& topLeft, bool open, ImU32 color);
}
