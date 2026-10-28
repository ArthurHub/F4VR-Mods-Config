#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>

#include <imgui.h>
#include <imgui_internal.h>

#include "Theme.h"
#include "model/ConfigDefinition.h"

// What the parts of the panel draw with, in the look of the design system.

namespace mods_config::ui
{
    /**
     * A mark that is a piece of a text, with no text: the warning mark, the invalid mark, or the info mark of a
     * result.
     */
    enum class TextMark : std::uint8_t
    {
        None,
        Warning,
        Invalid,
        Info,
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
     * The mark before a count: the invalid mark, the warning mark, or the pending mark.
     */
    enum class CountMark : std::uint8_t
    {
        Invalid,
        Warning,
        Pending,
    };

    /**
     * A count with its mark, as `2 errors` in red after the invalid mark.
     */
    struct MarkedCount
    {
        CountMark mark = CountMark::Invalid;
        std::string text;
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
        // the confirming button of a dangerous confirmation: a solid red
        Danger,
    };

    /**
     * A button of a dialog: its text, its look, and whether it is disabled.
     */
    struct DialogButton
    {
        std::string_view label;
        ButtonLook look = ButtonLook::Quiet;
        bool disabled = false;
    };

    // the track of a slider, and the box of a stepper's option, in the panel's pixels: not a token of the
    // design system
    constexpr float TRACK_WIDTH = 280.0f;

    // the box of a dropdown, which fills the room of a control in its row: the control column without the room of
    // Reset. A binding's frame is as wide
    constexpr float DROPDOWN_WIDTH = theme::CONTROL_WIDTH - theme::CONTROL_HEIGHT - theme::SPACE_2;

    // a cell of a switcher, Off or On, which has two, in the panel's pixels: not a token of the design system
    constexpr float TOGGLE_CELL_WIDTH = 88.0f;

    // the box the arrow of a stepper's button and of a dropdown is drawn in
    constexpr float ARROW_SIZE = 14.0f;

    /**
     * Where an arrow of a stepper's button and of a dropdown points.
     */
    enum class ControlArrow : std::uint8_t
    {
        Left,
        Right,
        Down,
    };

    // the strokes of a drawn icon, as the design system draws its icons in a box of 24
    constexpr float ICON_THICKNESS = 2.5f;

    // the box of the arrow of a row that opens and closes, as a mod's row of the tree and a section that folds
    constexpr float FOLD_ARROW_SIZE = 14.0f;

    // the box of the warning mark and of the invalid mark, as the design system draws them
    constexpr float WARNING_MARK_WIDTH = 20.0f;

    // the box of the pending mark, a dot, as the design system draws it
    constexpr float PENDING_MARK_WIDTH = 10.0f;

    // the box of the mark of when a pending change takes effect, which is drawn in place of the dot
    constexpr float APPLIES_MARK_WIDTH = 16.0f;

    // how far the selected-row bar's tip extends past its right edge
    constexpr float SELECTED_BAR_TIP_DEPTH = 20.0f;

    // where a heading's text starts in its row, as a row's text does in its frame, so a heading lines up with the
    // rows beside it: and where the page's texts start, in from the content's left
    constexpr float HEADING_TEXT_INSET = theme::SPACE_3;

    float mcmScale();
    float textWidth(std::string_view text);
    void drawPageText(std::string_view text);
    void drawText(float left, float right, float top, float height, ImU32 color, std::string_view text);
    float drawTextRuns(float left, float right, float top, float height, ImU32 color, std::span<const model::TextRun> runs);
    float drawTextLines(std::span<const model::TextLine> lines, const ImVec2& pos, float width, ImU32 color);
    int countTextLines(std::span<const TextPiece> pieces, float width);
    void drawTextPieces(std::span<const TextPiece> pieces, const ImVec2& pos, float width, int maxLines);
    void sectionHeading(std::string_view text, float right);
    void drawSectionLine(float left, float top, float bottom, float right);
    void drawOutline(const ImVec2& min, const ImVec2& max, ImU32 color);
    void drawDashedOutline(const ImVec2& min, const ImVec2& max, ImU32 color);
    void drawSelectedBar(const ImVec2& min, const ImVec2& max);
    void drawRowState();
    bool listOption(std::string_view text, bool chosen, float width);
    ImRect controlBox(float left, float top, float width);
    void drawFrame(const ImRect& box, ValueLook look = ValueLook::Saved);
    float drawSwitcher(float right, float top, std::optional<bool> on, ValueLook look);
    void drawButtonFrame(const ImRect& box, bool pointed, bool held);
    bool iconButton(const char* id, const ImRect& box, bool& pointed);
    bool button(const char* label, const ImRect& box, ButtonLook look);
    bool beginDialog(const char* id, bool asked, ImU32 border);
    int dialogButtons(std::span<const DialogButton> buttons);
    void drawTextIn(const ImRect& box, std::string_view text, ImU32 color, float align, float padding = 0.0f);
    ImU32 valueColor(ValueLook look);
    float tagWidth(std::string_view text);
    void drawTag(float left, float middle, std::string_view text);
    float appliesTagWidth(std::string_view word);
    void drawAppliesTag(float left, float middle, model::Applies applies, std::string_view word);
    float restartMarkWidth();
    void drawRestartMark(float left, float middle);
    float drawMarkedCounts(float right, float top, std::span<const MarkedCount> counts);
    void drawFoldingHeading(std::string_view text, const ImVec2& pos, float width, bool folded, std::span<const MarkedCount> counts, TextMark mark = TextMark::None);
    void drawPendingMark(const ImVec2& center);
    float pendingMarkWidth(model::Applies applies);
    void drawAppliesMark(model::Applies applies, const ImVec2& center);
    void drawInvalidMark(const ImVec2& center);
    void drawWarningMark(const ImVec2& center);
    void drawInfoMark(const ImVec2& center);
    void drawResetIcon(const ImVec2& center, float scale = 1.0f);
    void drawDiscardIcon(const ImVec2& center, float scale = 1.0f);
    void drawMark(TextMark mark, const ImVec2& center);
    void drawFoldArrow(const ImVec2& topLeft, bool open, ImU32 color);
    void drawControlArrow(const ImVec2& center, ControlArrow arrow);
}
