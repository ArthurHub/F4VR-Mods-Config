#include "Drawing.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

#include <imgui_internal.h>

#include "PanelTargets.h"
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

    /**
     * A token of the design system as the framework's color.
     */
    f4cf::render::Color colorOf(const ImU32 color)
    {
        return f4cf::render::Color::rgba(static_cast<int>((color >> IM_COL32_R_SHIFT) & 0xFF),
            static_cast<int>((color >> IM_COL32_G_SHIFT) & 0xFF),
            static_cast<int>((color >> IM_COL32_B_SHIFT) & 0xFF),
            static_cast<int>((color >> IM_COL32_A_SHIFT) & 0xFF));
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
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + HEADING_TEXT_INSET);
        ImGui::PushStyleColor(ImGuiCol_Text, theme::color::GREEN_MUTED);
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - theme::SPACE_2);
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
        const ImU32 glow = (theme::color::GREEN & ~IM_COL32_A_MASK) | (GLOW_ALPHA << IM_COL32_A_SHIFT);
        auto* drawList = ImGui::GetWindowDrawList();
        for (float radius = RADIUS; radius > 0.0f; radius -= GLOW_STEP) {
            drawList->AddCircleFilled(center, radius, glow);
        }
        drawList->AddCircle(center, RADIUS, theme::color::GREEN, 0, 1.5f);
    }

    /**
     * The heading of a section of rows at the cursor: its muted text, `space-3` in as a row's text is in its frame,
     * and a line from it to a right edge, where the rows end. ImGui draws the line to the right of the window's
     * work area, past the rows into the room of the scroll bar, so that edge is the heading's while it is drawn.
     * The ranges of the text a search found are underlined.
     */
    void sectionHeading(const std::string_view text, const float right, const std::span<const model::TextRange> found)
    {
        auto* window = ImGui::GetCurrentWindow();
        const float workRight = window->WorkRect.Max.x;
        window->WorkRect.Max.x = right;
        // the style's padding is not a heading's
        ImGui::PushStyleVar(ImGuiStyleVar_SeparatorTextPadding, ImVec2(HEADING_TEXT_INSET, ImGui::GetStyle().SeparatorTextPadding.y));
        ImGui::PushStyleColor(ImGuiCol_Text, theme::color::GREEN_MUTED);
        // by its end: the text is a mod's, and ImGui would cut it at a "##"
        ImGui::SeparatorTextEx(0, text.data(), text.data() + text.size(), 0.0f);
        ImGui::PopStyleColor();
        ImGui::PopStyleVar();
        window->WorkRect.Max.x = workRight;
        const auto min = ImGui::GetItemRectMin();
        const float textLeft = min.x + HEADING_TEXT_INSET;
        controls::drawUnderlines(textLeft, textLeft + controls::textWidth(text), min.y, ImGui::GetItemRectSize().y, theme::color::GREEN_MUTED, text, found);
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
        const float markLeft = pos.x + HEADING_TEXT_INSET;
        drawSphereMark(ImVec2(markLeft + SPHERE_MARK_SIZE * 0.5f, middle));
        const float textLeft = markLeft + SPHERE_MARK_SIZE + theme::SPACE_1;
        const float textRight = (std::min)(textLeft + controls::textWidth(text), right);
        controls::drawText(textLeft, textRight, pos.y, height, theme::color::GREEN_MUTED, text);
        controls::drawUnderlines(textLeft, textRight, pos.y, height, theme::color::GREEN_MUTED, text, found);
        const float ruleLeft = std::ceil(textRight) + theme::SPACE_3;
        if (ruleLeft < right) {
            const float ruleTop = std::floor(middle - theme::BORDER_FRAME * 0.5f);
            ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(ruleLeft, ruleTop), ImVec2(right, ruleTop + theme::BORDER_FRAME), ImGui::GetColorU32(theme::color::RULE));
        }
    }

    /**
     * A button of the footer or of a row's editor in its box, as the framework's controls draw it, which devbench
     * finds by its label. Returns whether it was pressed.
     */
    bool panelButton(const char* label, const ImRect& box, const controls::ButtonLook look)
    {
        const bool pressed = controls::button(label, box, look);
        panelTargets().add(TargetKind::Button, label, box);
        return pressed;
    }

    /**
     * Begins the popup of an editor of a row, as the binding, the color and the transform editors are: of a width and a
     * height, centered on the row as far as the page lets it, under or over the row as `controls::popupTop` places it, padded and
     * outlined in `green`. Returns whether its content is drawn, which ends with `ImGui::EndPopup`.
     */
    bool beginRowEditor(const ImGuiID id, const ImRect& row, const float width, const float height)
    {
        const auto& page = ImGui::GetCurrentWindow()->InnerRect;
        const float left = std::clamp(std::floor(row.GetCenter().x - width * 0.5f), page.Min.x, (std::max)(page.Min.x, page.Max.x - width));
        ImGui::SetNextWindowPos(ImVec2(left, controls::popupTop(row, height, page)));
        ImGui::SetNextWindowSize(ImVec2(width, height));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(theme::SPACE_4, theme::SPACE_4));
        ImGui::PushStyleColor(ImGuiCol_Border, theme::color::GREEN);
        const bool shown = ImGui::BeginPopupEx(id,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar |
                ImGuiWindowFlags_NoScrollWithMouse);
        ImGui::PopStyleColor();
        ImGui::PopStyleVar();
        return shown;
    }

    /**
     * The buttons of a dialog, as the framework's controls lay them out, each of which devbench finds by its label.
     * Returns the place of the one that was pressed, and -1 for none.
     */
    int panelDialogButtons(const std::span<const controls::DialogButton> buttons)
    {
        std::vector<ImRect> boxes(buttons.size());
        const int pressed = controls::dialogButtons(buttons, boxes);
        for (std::size_t place = 0; place < buttons.size(); ++place) {
            panelTargets().add(TargetKind::Button, buttons[place].label, boxes[place]);
        }
        return pressed;
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
        return controls::tagWidth(word) + APPLIES_MARK_WIDTH + theme::SPACE_1;
    }

    /**
     * The tag of when a pending change takes effect, from its left in the middle of a height, as the design
     * system's TakesEffect has it: the mark of its applies and its word, all in amber, as the pending value is.
     */
    void drawAppliesTag(const float left, const float middle, const model::Applies applies, const std::string_view word)
    {
        const ImVec2 min(std::floor(left), std::floor(middle - controls::tagHeight() * 0.5f));
        const ImVec2 max(min.x + appliesTagWidth(word), min.y + controls::tagHeight());
        controls::drawOutline(min, max, theme::color::AMBER);
        const float markLeft = min.x + theme::BORDER_FRAME + theme::SPACE_2;
        drawAppliesMark(applies, ImVec2(markLeft + APPLIES_MARK_WIDTH * 0.5f, min.y + controls::tagHeight() * 0.5f));
        ImGui::PushFont(nullptr, theme::FONT_SIZE_DETAIL);
        controls::drawText(markLeft + APPLIES_MARK_WIDTH + theme::SPACE_1, max.x, min.y, controls::tagHeight(), theme::color::AMBER, word);
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
     * Counts that each have their mark before them, as `2 errors` after the invalid mark, ending at a right edge
     * in the current font and in the middle of a control's height from a top. Returns their left edge.
     */
    float drawMarkedCounts(const float right, const float top, const std::span<const MarkedCount> counts)
    {
        const auto markWidth = [](const CountMark mark) {
            return mark == CountMark::Pending ? controls::DOT_WIDTH : controls::MARK_WIDTH;
        };

        float width = 0.0f;
        for (const auto& count : counts) {
            width += (width > 0.0f ? theme::SPACE_2 * 2.0f : 0.0f) + markWidth(count.mark) + theme::SPACE_2 + controls::textWidth(count.text);
        }
        const float left = std::floor(right - width);
        const float middle = top + theme::CONTROL_HEIGHT * 0.5f;
        float x = left;
        for (const auto& count : counts) {
            const ImVec2 center(x + markWidth(count.mark) * 0.5f, middle);
            if (count.mark == CountMark::Invalid) {
                controls::drawErrorMark(center);
            } else if (count.mark == CountMark::Warning) {
                controls::drawWarningMark(center);
            } else {
                controls::drawDot(center, theme::color::AMBER);
            }
            x += markWidth(count.mark) + theme::SPACE_2;
            controls::drawText(x, right, top, theme::CONTROL_HEIGHT, count.color, count.text);
            x += controls::textWidth(count.text) + theme::SPACE_2 * 2.0f;
        }
        return left;
    }

    /**
     * A heading that folds, in a row at a place and in a width: an arrow that points right while it is folded
     * and down while it is open, its text, and a rule from it to the right edge, as a heading has. The arrow
     * starts where a heading's text does. Its text and its arrow are muted, under the ray too. The counts of what
     * it hides end the rule, in the detail size. A heading with the invalid mark or the warning mark has it before
     * its text, and its text and its arrow are red or amber. The ranges of the text a search found are underlined.
     */
    void drawFoldingHeading(const std::string_view text, const ImVec2& pos, const float width, const bool folded, const std::span<const MarkedCount> counts,
        const controls::Mark mark, const std::span<const model::TextRange> found)
    {
        // the room a rule keeps at the least, as the design system has it
        constexpr float RULE_MIN_WIDTH = theme::SPACE_4;
        auto color = theme::color::GREEN_MUTED;
        if (mark == controls::Mark::Error || mark == controls::Mark::Warning) {
            color = mark == controls::Mark::Error ? theme::color::RED : theme::color::AMBER;
        }
        const float arrowLeft = pos.x + HEADING_TEXT_INSET;
        controls::drawFoldArrow(ImVec2(arrowLeft, pos.y + (theme::CONTROL_HEIGHT - controls::FOLD_ARROW_SIZE) * 0.5f), !folded, color);
        float ruleRight = pos.x + width;
        if (!counts.empty()) {
            ImGui::PushFont(nullptr, theme::FONT_SIZE_DETAIL);
            ruleRight = drawMarkedCounts(ruleRight - theme::SPACE_3, pos.y, counts) - theme::SPACE_2;
            ImGui::PopFont();
        }
        float textLeft = arrowLeft + controls::FOLD_ARROW_SIZE + theme::SPACE_2;
        if (mark != controls::Mark::None) {
            controls::drawMark(mark, ImVec2(textLeft + controls::MARK_WIDTH * 0.5f, pos.y + theme::CONTROL_HEIGHT * 0.5f));
            textLeft += controls::MARK_WIDTH + theme::SPACE_2;
        }
        const float textRight = (std::min)(textLeft + controls::textWidth(text), ruleRight - RULE_MIN_WIDTH - theme::SPACE_3);
        controls::drawText(textLeft, textRight, pos.y, theme::CONTROL_HEIGHT, color, text);
        controls::drawUnderlines(textLeft, textRight, pos.y, theme::CONTROL_HEIGHT, color, text, found);
        const float ruleLeft = std::ceil(textRight) + theme::SPACE_3;
        if (ruleLeft < ruleRight) {
            const float ruleTop = std::floor(pos.y + (theme::CONTROL_HEIGHT - theme::BORDER_FRAME) * 0.5f);
            ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(ruleLeft, ruleTop), ImVec2(ruleRight, ruleTop + theme::BORDER_FRAME), ImGui::GetColorU32(theme::color::RULE));
        }
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
        const auto color = ImGui::GetColorU32(theme::color::AMBER);
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
            controls::drawDot(center, theme::color::AMBER);
            break;
        }
    }

    /**
     * The framework controls' style in the look of the design system: its tokens by role.
     */
    controls::ControlsStyle controlsStyle()
    {
        return { .accent = colorOf(theme::color::GREEN),
            .muted = colorOf(theme::color::GREEN_MUTED),
            .track = colorOf(theme::color::TRACK),
            .hover = colorOf(theme::color::HOVER),
            .pressed = colorOf(theme::color::PRESSED),
            .glow = colorOf(theme::color::GLOW),
            .rule = colorOf(theme::color::RULE),
            .frameBorder = colorOf(theme::color::FRAME_BORDER),
            .onAccent = colorOf(theme::color::ON_GREEN),
            .warning = colorOf(theme::color::AMBER),
            .onWarning = colorOf(theme::color::ON_AMBER),
            .error = colorOf(theme::color::RED),
            .onError = colorOf(theme::color::ON_RED),
            .popup = colorOf(theme::color::POPUP),
            .controlHeight = theme::CONTROL_HEIGHT,
            .frameRounding = theme::RADIUS_FRAME,
            .frameBorderSize = theme::BORDER_FRAME,
            .detailFontSize = theme::FONT_SIZE_DETAIL };
    }
}
