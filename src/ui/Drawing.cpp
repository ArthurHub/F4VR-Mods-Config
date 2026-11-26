#include "Drawing.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

#include <imgui_internal.h>

#include "Theme.h"
#include "imgui/ImGuiFonts.h"

namespace
{
    using namespace mods_config;

    // The size of a font tag that is drawn as the panel's body text. A config's sizes are those of MCM's menu,
    // where the ordinary text is about this size, so a text is as much larger or smaller than the rows as there.
    constexpr float MARKUP_BODY_SIZE = 20.0f;

    // the text of the Restart mark
    constexpr auto RESTART_TEXT = "Restart";

    /**
     * The face of the canvas font a run is drawn in: bold for a `<b>`, italic for an `<i>`, and bold for a text
     * that is both, as the panel has no face for both. The current font where the face is not loaded.
     */
    ImFont* fontOf(const model::TextRun& run)
    {
        ImFont* face = nullptr;
        if (run.bold) {
            face = f4cf::imgui::canvasFont(f4cf::imgui::FontStyle::Bold);
        } else if (run.italic) {
            face = f4cf::imgui::canvasFont(f4cf::imgui::FontStyle::Italic);
        }
        return face ? face : ImGui::GetFont();
    }

    /**
     * The piece of a run of a config's text, in a font size: in its own color, or else in the given one, and in
     * its face.
     */
    controls::TextPiece pieceOf(const model::TextRun& run, const float fontSize, const ImU32 color)
    {
        return { .text = run.text,
            .color = run.color ? IM_COL32(*run.color >> 16, (*run.color >> 8) & 0xFF, *run.color & 0xFF, 255) : color,
            .font = fontOf(run),
            .size = fontSize,
            .underline = run.underline };
    }
}

namespace mods_config::ui
{
    /**
     * Draws muted text at the cursor, inset from the page's edges and wrapped to its remaining width. It
     * starts where a heading's text does.
     */
    void drawPageText(const std::string_view text)
    {
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + controls::headingTextInset());
        ImGui::PushStyleColor(ImGuiCol_Text, colors().muted);
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - look().space2);
        ImGui::TextUnformatted(text.data(), text.data() + text.size());
        ImGui::PopTextWrapPos();
        ImGui::PopStyleColor();
    }

    /**
     * How much larger the panel draws a size of MCM's menu, so that it is as large beside the rows' text as
     * it is there.
     */
    float mcmScale()
    {
        return ImGui::GetFontSize() / MARKUP_BODY_SIZE;
    }

    /**
     * Lines of text in a width, each wrapped between its words where it is wider. A run is in its own size and
     * color, or else in the given color, and in its face. An empty line is as high as a line of body text.
     * Returns the height of them all.
     */
    float drawTextLines(const std::span<const model::TextLine> lines, const ImVec2& pos, const float width, const ImU32 color)
    {
        std::vector<controls::TextPiece> pieces;
        float y = pos.y;
        for (const auto& line : lines) {
            pieces.clear();
            for (const auto& run : line.runs) {
                const float fontSize = run.size ? static_cast<float>(*run.size) * mcmScale() : ImGui::GetFontSize();
                pieces.push_back(pieceOf(run, fontSize, color));
            }
            y += controls::drawWrappedText(pieces, line.align, ImVec2(pos.x, y), width);
        }
        return y - pos.y;
    }

    /**
     * One line of text in pieces of a size, a color and a face each, centered in a height, as `controls::drawText` draws
     * a line. A piece is never larger than the current font size, so it stays in its row, and one with no color
     * is in the given color. It ends in an ellipsis where it does not fit before the right edge.
     * Returns the width it takes, which is not more than its room.
     */
    float drawTextRuns(const float left, const float right, const float top, const float height, const ImU32 color, const std::span<const model::TextRun> runs)
    {
        std::vector<controls::TextPiece> pieces;
        for (const auto& run : runs) {
            const float fontSize = run.size ? (std::min)(static_cast<float>(*run.size) * mcmScale(), ImGui::GetFontSize()) : ImGui::GetFontSize();
            auto piece = pieceOf(run, fontSize, color);
            // in the alpha of a disabled row
            piece.color = ImGui::GetColorU32(piece.color);
            pieces.push_back(piece);
        }
        return controls::drawTextLine(left, right, top, height, pieces);
    }

    /**
     * An activation sphere's mark, around its center: a ring over a glow that fades from its middle to the ring, in
     * circles that each add to it, as ImGui has no radial fill.
     */
    static void drawSphereMark(const ImVec2& center)
    {
        constexpr float RADIUS = 7.0f;
        constexpr float GLOW_STEP = 1.0f;
        constexpr ImU32 GLOW_ALPHA = 46;
        const ImU32 glow = (colors().accent & ~IM_COL32_A_MASK) | (GLOW_ALPHA << IM_COL32_A_SHIFT);
        auto* drawList = ImGui::GetWindowDrawList();
        for (float radius = RADIUS; radius > 0.0f; radius -= GLOW_STEP) {
            drawList->AddCircleFilled(center, radius, glow);
        }
        drawList->AddCircle(center, RADIUS, colors().accent, 0, 1.5f);
    }

    /**
     * The heading of an activation sphere at the cursor, as high as any heading: the sphere's mark, its muted text and a
     * line from it to a right edge. It is drawn here, as ImGui's heading has a line before a text that is this far in.
     * The ranges of the text a search found are underlined.
     */
    void sphereHeading(const std::string_view text, const float right, const std::span<const model::TextRange> found)
    {
        const auto pos = ImGui::GetCursorScreenPos();
        const float height = ImGui::GetFontSize() + ImGui::GetStyle().SeparatorTextPadding.y * 2.0f;
        ImGui::Dummy(ImVec2(right - pos.x, height));
        const float middle = pos.y + height * 0.5f;
        const float markLeft = pos.x + controls::headingTextInset();
        drawSphereMark(ImVec2(markLeft + SPHERE_MARK_SIZE * 0.5f, middle));
        const float textLeft = markLeft + SPHERE_MARK_SIZE + look().space1;
        const float textRight = (std::min)(textLeft + controls::textWidth(text), right);
        controls::drawText(textLeft, textRight, pos.y, height, colors().muted, text);
        controls::drawUnderlines(textLeft, textRight, pos.y, height, colors().muted, text, found);
        const float ruleLeft = std::ceil(textRight) + look().space3;
        if (ruleLeft < right) {
            const float ruleTop = std::floor(middle - look().frameBorderSize * 0.5f);
            ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(ruleLeft, ruleTop), ImVec2(right, ruleTop + look().frameBorderSize), ImGui::GetColorU32(colors().rule));
        }
    }

    /**
     * The box of a dropdown, which fills the room of a control in its row: the control column without the room of
     * Reset. A binding's frame is as wide.
     */
    float dropdownWidth()
    {
        return theme::CONTROL_WIDTH - look().controlHeight - look().space2;
    }

    /**
     * The tone of a value by its look: the warning while it waits for a Save, and the error while it is invalid.
     */
    controls::Tone toneOf(const ValueLook look)
    {
        switch (look) {
        case ValueLook::Pending:
            return controls::Tone::Warning;
        case ValueLook::Invalid:
            return controls::Tone::Error;
        case ValueLook::Saved:
            break;
        }
        return controls::Tone::Normal;
    }

    /**
     * The color a value is drawn in, with the fill and the grab of a slider: amber while it waits for a Save,
     * and red while it is invalid.
     */
    ImU32 valueColor(const ValueLook look)
    {
        return controls::get().tone(toneOf(look)).color;
    }

    /**
     * The width of the tag of when a pending change takes effect: its mark and its word.
     */
    float appliesTagWidth(const std::string_view word)
    {
        return controls::tagWidth(word) + APPLIES_MARK_WIDTH + look().space1;
    }

    /**
     * The tag of when a pending change takes effect, from its left in the middle of a height, as the design
     * system's TakesEffect has it: the mark of its applies and its word, all in amber, as the pending value is.
     */
    void drawAppliesTag(const float left, const float middle, const model::Applies applies, const std::string_view word)
    {
        const ImVec2 min(std::floor(left), std::floor(middle - controls::tagHeight() * 0.5f));
        const ImVec2 max(min.x + appliesTagWidth(word), min.y + controls::tagHeight());
        controls::drawOutline(min, max, colors().warning);
        const float markLeft = min.x + look().frameBorderSize + look().space2;
        drawAppliesMark(applies, ImVec2(markLeft + APPLIES_MARK_WIDTH * 0.5f, min.y + controls::tagHeight() * 0.5f));
        ImGui::PushFont(nullptr, look().detailFontSize);
        controls::drawText(markLeft + APPLIES_MARK_WIDTH + look().space1, max.x, min.y, controls::tagHeight(), colors().warning, word);
        ImGui::PopFont();
    }

    /**
     * The width of the Restart mark, the tag of a setting that takes effect after the game is restarted.
     */
    float restartMarkWidth()
    {
        return controls::tagWidth(RESTART_TEXT);
    }

    void drawRestartMark(const float left, const float middle)
    {
        controls::drawTag(left, middle, RESTART_TEXT);
    }

    /**
     * The width of the pending mark of a change by when it takes effect: the dot where it is not known.
     */
    float pendingMarkWidth(const model::Applies applies)
    {
        return applies == model::Applies::Unknown ? controls::DOT_WIDTH : APPLIES_MARK_WIDTH;
    }

    /**
     * The pending mark of a change by when it takes effect, around a point, in amber, as the design system's
     * TakesEffect draws it in a box of 16: a broadcast for `live`, a disk for `save`, a power mark for `restart`,
     * and the plain dot where it is not known.
     */
    void drawAppliesMark(const model::Applies applies, const ImVec2& center)
    {
        constexpr float THICKNESS = 2.0f;
        const ImVec2 origin(std::floor(center.x - 8.0f), std::floor(center.y - 8.0f));
        const auto at = [&origin](const float x, const float y) {
            return ImVec2(origin.x + x, origin.y + y);
        };
        auto* drawList = ImGui::GetWindowDrawList();
        const auto color = ImGui::GetColorU32(colors().warning);
        switch (applies) {
        case model::Applies::Live: {
            // a dot with an arc on each side of it
            constexpr float SPREAD = 0.95f;
            drawList->AddCircleFilled(at(8.0f, 8.0f), 2.2f, color);
            drawList->PathArcTo(at(8.0f, 8.0f), 5.6f, IM_PI - SPREAD, IM_PI + SPREAD);
            drawList->PathStroke(color, ImDrawFlags_None, THICKNESS);
            drawList->PathArcTo(at(8.0f, 8.0f), 5.6f, -SPREAD, SPREAD);
            drawList->PathStroke(color, ImDrawFlags_None, THICKNESS);
            break;
        }
        case model::Applies::Save: {
            // a disk: its outline with a corner cut, its shutter and its label
            const std::array outline{ at(2.0f, 2.0f), at(11.5f, 2.0f), at(14.0f, 4.5f), at(14.0f, 14.0f), at(2.0f, 14.0f) };
            drawList->AddPolyline(outline.data(), static_cast<int>(outline.size()), color, ImDrawFlags_Closed, THICKNESS);
            drawList->AddRectFilled(at(5.0f, 2.0f), at(10.0f, 5.5f), color);
            drawList->AddRectFilled(at(4.5f, 9.0f), at(11.5f, 14.0f), color);
            break;
        }
        case model::Applies::Restart: {
            // a power mark: an arc open at the top, and a line down into it
            constexpr float GAP_FROM = -1.03f;
            constexpr float GAP_TO = 1.03f + IM_PI;
            drawList->PathArcTo(at(8.0f, 8.8f), 5.8f, GAP_FROM, GAP_TO);
            drawList->PathStroke(color, ImDrawFlags_None, THICKNESS);
            drawList->AddLine(at(8.0f, 1.2f), at(8.0f, 8.0f), color, THICKNESS);
            break;
        }
        case model::Applies::Unknown:
            controls::drawDot(center, colors().warning);
            break;
        }
    }
}
