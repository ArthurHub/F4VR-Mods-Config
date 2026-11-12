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

    // The size of a dialog, which is not a token of the design system, in the panel's pixels. Its width:
    constexpr float DIALOG_WIDTH = 576.0f;
    // and the least a button of it is, which is wider for a longer word:
    constexpr float DIALOG_BUTTON_MIN_WIDTH = 112.0f;

    // the ring around a button, outside its box
    constexpr float BUTTON_RING = 2.0f;

    // the room of the mark of the chosen option, before the text of every option of a list
    constexpr float CHECK_SIZE = 20.0f;
    constexpr float CHECK_THICKNESS = 2.5f;

    /**
     * The mark of the chosen option of a list, in a box of its size from a corner.
     */
    void drawCheck(const ImVec2& min, const ImU32 color)
    {
        // as the design system draws it, in a box of 24
        constexpr float SCALE = CHECK_SIZE / 24.0f;
        const std::array points{ ImVec2(min.x + 4.0f * SCALE, min.y + 12.5f * SCALE),
            ImVec2(min.x + 9.5f * SCALE, min.y + 18.0f * SCALE),
            ImVec2(min.x + 20.0f * SCALE, min.y + 6.5f * SCALE) };
        ImGui::GetWindowDrawList()->AddPolyline(points.data(), static_cast<int>(points.size()), ImGui::GetColorU32(color), CHECK_THICKNESS);
    }

    // The size of a font tag that is drawn as the panel's body text. A config's sizes are those of MCM's menu,
    // where the ordinary text is about this size, so a text is as much larger or smaller than the rows as there.
    constexpr float MARKUP_BODY_SIZE = 20.0f;

    // The line under an underlined text, in its font size: its top is below the baseline by Roboto's underline
    // position, and it is as thick as its underline, which are 150 and 100 of the 2400 units ImGui sizes the font by.
    constexpr float UNDERLINE_OFFSET = 0.0625f;
    constexpr float UNDERLINE_THICKNESS = 0.042f;

    // the line of the search under a match, at the bottom of the text, as the design system's Search has it
    constexpr float SEARCH_UNDERLINE = 2.0f;

    // the text of the Restart mark, and its height: a line of the detail size of 22 inside its outline, as the
    // design system's tag has it
    constexpr auto RESTART_TEXT = "Restart";
    constexpr float RESTART_MARK_HEIGHT = 22.0f + theme::BORDER_FRAME * 2.0f;

    /**
     * How a piece of text looks: its font size, its color, and its face.
     */
    struct Look
    {
        float fontSize = 0.0f;
        ImU32 color = 0;
        ImFont* font = nullptr;
        bool underline = false;
    };

    /**
     * A word of a line of text with the spaces after it, which a line is wrapped between.
     */
    struct Word
    {
        std::string_view text;

        // with its spaces, and without them
        float width = 0.0f;
        float inkWidth = 0.0f;
        Look look;

        // the mark it is, with no text
        ui::TextMark mark = ui::TextMark::None;
    };

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

    float widthOf(const std::string_view text, const Look& look)
    {
        return text.empty() ? 0.0f : look.font->CalcTextSizeA(look.fontSize, FLT_MAX, 0.0f, text.data(), text.data() + text.size()).x;
    }

    /**
     * Adds the words of a text in one look to those of its line.
     */
    void addWords(const std::string_view text, const Look& look, std::vector<Word>& words)
    {
        for (std::size_t start = 0; start < text.size();) {
            const auto inkEnd = (std::min)(text.find(' ', start), text.size());
            const auto end = (std::min)(text.find_first_not_of(' ', inkEnd), text.size());
            words.push_back({ .text = text.substr(start, end - start),
                .width = widthOf(text.substr(start, end - start), look),
                .inkWidth = widthOf(text.substr(start, inkEnd - start), look),
                .look = look });
            start = end;
        }
    }

    /**
     * The look of a run of a config's text, in a font size: in its own color, or else in the given one, and in
     * its face.
     */
    Look lookOf(const model::TextRun& run, const float fontSize, const ImU32 color)
    {
        return { .fontSize = fontSize,
            .color = run.color ? IM_COL32(*run.color >> 16, (*run.color >> 8) & 0xFF, *run.color & 0xFF, 255) : color,
            .font = fontOf(run),
            .underline = run.underline };
    }

    /**
     * Where the lines of words start when they are wrapped in a width: the place of the first word of each.
     * A word that is wider than the width is a line by itself.
     */
    std::vector<std::size_t> wrapWords(const std::span<const Word> words, const float width)
    {
        std::vector<std::size_t> starts;
        float lineWidth = 0.0f;
        for (std::size_t i = 0; i < words.size(); ++i) {
            if (i == 0 || lineWidth + words[i].inkWidth > width) {
                starts.push_back(i);
                lineWidth = 0.0f;
            }
            lineWidth += words[i].width;
        }
        return starts;
    }

    /**
     * The words of pieces of text that are one text, in the current font size. A mark is a word of its own,
     * with the room of a space after it.
     */
    std::vector<Word> wordsOf(const std::span<const ui::TextPiece> pieces)
    {
        std::vector<Word> words;
        for (const auto& piece : pieces) {
            const Look look{ .fontSize = ImGui::GetFontSize(), .color = piece.color, .font = ImGui::GetFont(), .underline = false };
            if (piece.mark != ui::TextMark::None) {
                words.push_back({ .text = {}, .width = ui::WARNING_MARK_WIDTH + theme::SPACE_2, .inkWidth = ui::WARNING_MARK_WIDTH, .look = look, .mark = piece.mark });
            } else {
                addWords(piece.text, look, words);
            }
        }
        return words;
    }

    /**
     * Draws words on one line, set in its room by the line's align. Words of a smaller size stand on the
     * bottom of the line. An underlined word has its line under it and under the spaces after it, but not
     * under the spaces that end the line. A mark is in the middle of the line's height.
     * Returns the height of the line.
     */
    float drawWords(const std::span<const Word> words, const model::TextAlign align, const ImVec2& pos, const float width)
    {
        float height = 0.0f;
        float lineWidth = 0.0f;
        for (const auto& word : words) {
            height = (std::max)(height, word.look.fontSize);
            lineWidth += &word == &words.back() ? word.inkWidth : word.width;
        }
        if (!ImGui::IsRectVisible(pos, ImVec2(pos.x + width, pos.y + height))) {
            return height;
        }

        const float room = (std::max)(width - lineWidth, 0.0f);
        float x = pos.x;
        if (align != model::TextAlign::Left) {
            x += align == model::TextAlign::Center ? room * 0.5f : room;
        }
        auto* drawList = ImGui::GetWindowDrawList();
        for (const auto& word : words) {
            const auto& look = word.look;
            if (word.mark != ui::TextMark::None) {
                const ImVec2 center(x + ui::WARNING_MARK_WIDTH * 0.5f, pos.y + height * 0.5f);
                ui::drawMark(word.mark, center);
                x += word.width;
                continue;
            }
            const ImVec2 at(std::floor(x), std::floor(pos.y + height - look.fontSize));
            drawList->AddText(look.font, look.fontSize, at, look.color, word.text.data(), word.text.data() + word.text.size());
            if (look.underline) {
                // to where the next word starts before it is set on a whole pixel, so the lines of two words meet
                const float end = x + (&word == &words.back() ? word.inkWidth : word.width);
                const float top = at.y + look.font->GetFontBaked(look.fontSize)->Ascent + look.fontSize * UNDERLINE_OFFSET;
                drawList->AddRectFilled(ImVec2(at.x, top), ImVec2(end, top + (std::max)(look.fontSize * UNDERLINE_THICKNESS, 1.0f)), look.color);
            }
            x += word.width;
        }
        return height;
    }

    /**
     * Draws the words from the first on one line, as many as leave room for an ellipsis in the width, and the
     * first in any case, and the ellipsis after them in the size and color of the last.
     */
    void drawCutLine(const std::span<const Word> words, const ImVec2& pos, const float width)
    {
        const auto& first = words.front().look;
        const float room = width - first.font->GetFontBaked(first.fontSize)->GetCharAdvance(first.font->EllipsisChar);
        std::size_t last = 0;
        float lineWidth = words.front().width;
        while (last + 1 < words.size() && lineWidth + words[last + 1].inkWidth <= room) {
            lineWidth += words[++last].width;
        }
        const auto drawn = words.first(last + 1);
        const float height = drawWords(drawn, model::TextAlign::Left, pos, width);

        const auto& end = words[last];
        const auto& look = end.look;
        const ImVec2 at(std::floor(pos.x + lineWidth - end.width + end.inkWidth), std::floor(pos.y + height - look.fontSize));
        look.font->RenderChar(ImGui::GetWindowDrawList(), look.fontSize, at, look.color, look.font->EllipsisChar);
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
     * The width of a line of text in the current font size.
     */
    float textWidth(const std::string_view text)
    {
        return ImGui::CalcTextSize(text.data(), text.data() + text.size()).x;
    }

    /**
     * One line of text in the current font size, centered in a height. It ends in an ellipsis where it does
     * not fit before the right edge.
     */
    void drawText(const float left, const float right, const float top, const float height, const ImU32 color, const std::string_view text)
    {
        if (text.empty()) {
            return;
        }
        const float y = top + (height - ImGui::GetFontSize()) * 0.5f;
        ImGui::PushStyleColor(ImGuiCol_Text, color);
        ImGui::RenderTextEllipsis(ImGui::GetWindowDrawList(), ImVec2(left, y), ImVec2(right, y + ImGui::GetFontSize()), right, text.data(), text.data() + text.size(), nullptr);
        ImGui::PopStyleColor();
    }

    /**
     * The line of the search under ranges of the bytes of a line of text that `drawText` drew from `left`: 2 px at
     * the bottom of the text, in its color. A text that ends in an ellipsis has no line past the letters before it.
     */
    void drawUnderlines(const float left, const float right, const float top, const float height, const ImU32 color, const std::string_view text,
        const std::span<const model::TextRange> ranges)
    {
        if (ranges.empty()) {
            return;
        }
        const float bottom = top + (height + ImGui::GetFontSize()) * 0.5f;
        const float shown = textWidth(text) <= right - left ? right : right - textWidth("\xE2\x80\xA6");
        auto* drawList = ImGui::GetWindowDrawList();
        for (const auto& range : ranges) {
            const float start = left + textWidth(text.substr(0, range.begin));
            const float end = (std::min)(left + textWidth(text.substr(0, range.end)), shown);
            if (end > start) {
                drawList->AddRectFilled(ImVec2(start, bottom - SEARCH_UNDERLINE), ImVec2(end, bottom), ImGui::GetColorU32(color));
            }
        }
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
        std::vector<Word> words;
        float y = pos.y;
        for (const auto& line : lines) {
            words.clear();
            for (const auto& run : line.runs) {
                const float fontSize = run.size ? static_cast<float>(*run.size) * mcmScale() : ImGui::GetFontSize();
                addWords(run.text, lookOf(run, fontSize, color), words);
            }
            if (words.empty()) {
                y += ImGui::GetFontSize();
                continue;
            }

            const auto starts = wrapWords(words, width);
            for (std::size_t i = 0; i < starts.size(); ++i) {
                const auto end = i + 1 < starts.size() ? starts[i + 1] : words.size();
                y += drawWords(std::span<const Word>(words).subspan(starts[i], end - starts[i]), line.align, ImVec2(pos.x, y), width);
            }
        }
        return y - pos.y;
    }

    /**
     * The number of lines that pieces of text take as one text, wrapped in a width in the current font size.
     */
    int countTextLines(const std::span<const TextPiece> pieces, const float width)
    {
        return static_cast<int>(wrapWords(wordsOf(pieces), width).size());
    }

    /**
     * Pieces of text as one text in the current font size, each in its color, wrapped in a width between
     * the words. A text that needs more lines than it is given ends its last line in an ellipsis.
     */
    void drawTextPieces(const std::span<const TextPiece> pieces, const ImVec2& pos, const float width, const int maxLines)
    {
        const auto words = wordsOf(pieces);
        const auto starts = wrapWords(words, width);
        const auto lineCount = (std::min)(starts.size(), static_cast<std::size_t>((std::max)(maxLines, 0)));
        for (std::size_t line = 0; line < lineCount; ++line) {
            const ImVec2 at(pos.x, pos.y + static_cast<float>(line) * ImGui::GetFontSize());
            const auto first = starts[line];
            if (line + 1 < lineCount || lineCount == starts.size()) {
                const auto end = line + 1 < starts.size() ? starts[line + 1] : words.size();
                drawWords(std::span<const Word>(words).subspan(first, end - first), model::TextAlign::Left, at, width);
            } else {
                drawCutLine(std::span<const Word>(words).subspan(first), at, width);
            }
        }
    }

    /**
     * One line of text in pieces of a size, a color and a face each, centered in a height, as `drawText` draws
     * a line. A piece is never larger than the current font size, so it stays in its row, and one with no color
     * is in the given color. It ends in an ellipsis where it does not fit before the right edge.
     * Returns the width it takes, which is not more than its room.
     */
    float drawTextRuns(const float left, const float right, const float top, const float height, const ImU32 color, const std::span<const model::TextRun> runs)
    {
        std::vector<Word> words;
        for (const auto& run : runs) {
            const float fontSize = run.size ? (std::min)(static_cast<float>(*run.size) * mcmScale(), ImGui::GetFontSize()) : ImGui::GetFontSize();
            auto look = lookOf(run, fontSize, color);
            // in the alpha of a disabled row
            look.color = ImGui::GetColorU32(look.color);
            addWords(run.text, look, words);
        }
        if (words.empty()) {
            return 0.0f;
        }

        float lineHeight = 0.0f;
        float lineWidth = 0.0f;
        for (const auto& word : words) {
            lineHeight = (std::max)(lineHeight, word.look.fontSize);
            lineWidth += &word == &words.back() ? word.inkWidth : word.width;
        }
        const ImVec2 pos(left, top + (height - lineHeight) * 0.5f);
        ImGui::PushClipRect(ImVec2(left, top), ImVec2(right, top + height), true);
        if (lineWidth <= right - left) {
            drawWords(words, model::TextAlign::Left, pos, right - left);
        } else {
            drawCutLine(words, pos, right - left);
        }
        ImGui::PopClipRect();
        return (std::min)(lineWidth, right - left);
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
        drawUnderlines(textLeft, textLeft + textWidth(text), min.y, ImGui::GetItemRectSize().y, theme::color::GREEN_MUTED, text, found);
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
        const float textRight = (std::min)(textLeft + textWidth(text), right);
        drawText(textLeft, textRight, pos.y, height, theme::color::GREEN_MUTED, text);
        drawUnderlines(textLeft, textRight, pos.y, height, theme::color::GREEN_MUTED, text, found);
        const float ruleLeft = std::ceil(textRight) + theme::SPACE_3;
        if (ruleLeft < right) {
            const float ruleTop = std::floor(middle - theme::BORDER_FRAME * 0.5f);
            ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(ruleLeft, ruleTop), ImVec2(right, ruleTop + theme::BORDER_FRAME), ImGui::GetColorU32(theme::color::RULE));
        }
    }

    /**
     * The line of a section in a section, along the left of its rows: from under its heading down to the
     * bottom of its last row, where it turns toward the rows with a rounded corner, and stops at a right edge,
     * short of the rows. It shows what the section covers, so a row after it is seen to be the outer section's.
     */
    void drawSectionLine(const float left, const float top, const float bottom, const float right)
    {
        const float x = left + theme::BORDER_FRAME * 0.5f;
        const float y = bottom - theme::BORDER_FRAME * 0.5f;
        const float radius = std::clamp(right - x, 0.0f, theme::RADIUS_FRAME);
        auto* drawList = ImGui::GetWindowDrawList();
        drawList->PathLineTo(ImVec2(x, top));
        drawList->PathArcTo(ImVec2(x + radius, y - radius), radius, IM_PI, IM_PI * 0.5f);
        drawList->PathLineTo(ImVec2(right, y));
        drawList->PathStroke(ImGui::GetColorU32(theme::color::RULE), ImDrawFlags_None, theme::BORDER_FRAME);
    }

    /**
     * An outline along the inside of a rectangle's edge, as a control and a row under the ray have.
     */
    void drawOutline(const ImVec2& min, const ImVec2& max, const ImU32 color)
    {
        const float inset = theme::BORDER_FRAME * 0.5f;
        ImGui::GetWindowDrawList()->AddRect(ImVec2(min.x + inset, min.y + inset),
            ImVec2(max.x - inset, max.y - inset),
            ImGui::GetColorU32(color),
            theme::RADIUS_FRAME,
            theme::BORDER_FRAME);
    }

    /**
     * A dashed outline inside a rectangle, as the box of a texture the game does not load has it: dashes three
     * times as long as the outline is thick, with gaps of twice it, from each corner along its side.
     */
    void drawDashedOutline(const ImVec2& min, const ImVec2& max, const ImU32 color)
    {
        const float inset = theme::BORDER_FRAME * 0.5f;
        const std::array corners{ ImVec2(min.x + inset, min.y + inset),
            ImVec2(max.x - inset, min.y + inset),
            ImVec2(max.x - inset, max.y - inset),
            ImVec2(min.x + inset, max.y - inset) };
        const float dash = theme::BORDER_FRAME * 3.0f;
        const float gap = theme::BORDER_FRAME * 2.0f;
        auto* drawList = ImGui::GetWindowDrawList();
        for (std::size_t side = 0; side < corners.size(); ++side) {
            const auto& from = corners[side];
            const auto& to = corners[(side + 1) % corners.size()];
            const float length = std::hypot(to.x - from.x, to.y - from.y);
            for (float at = 0.0f; at < length; at += dash + gap) {
                const float end = (std::min)(at + dash, length);
                drawList->AddLine(ImLerp(from, to, at / length), ImLerp(from, to, end / length), ImGui::GetColorU32(color), theme::BORDER_FRAME);
            }
        }
    }

    /**
     * The bar of the selected row: a solid fill that ends in a tip inside the row, which starts
     * `SELECTED_BAR_TIP_DEPTH` from its right edge and points at the edge's middle. It is one shape, so no seam
     * shows between the bar and its tip.
     */
    void drawSelectedBar(const ImVec2& min, const ImVec2& max)
    {
        auto* drawList = ImGui::GetWindowDrawList();
        drawList->PathArcToFast(ImVec2(min.x + theme::RADIUS_FRAME, min.y + theme::RADIUS_FRAME), theme::RADIUS_FRAME, 6, 9);
        drawList->PathLineTo(ImVec2(max.x - SELECTED_BAR_TIP_DEPTH, min.y));
        drawList->PathLineTo(ImVec2(max.x, (min.y + max.y) * 0.5f));
        drawList->PathLineTo(ImVec2(max.x - SELECTED_BAR_TIP_DEPTH, max.y));
        drawList->PathArcToFast(ImVec2(min.x + theme::RADIUS_FRAME, max.y - theme::RADIUS_FRAME), theme::RADIUS_FRAME, 3, 6);
        drawList->PathFillConvex(theme::color::GREEN);
    }

    /**
     * How the row that was just added looks while the ray is on it: a tint with an outline, and a stronger one
     * while it is pressed. Call it before the row's content is drawn, which then lies over it.
     */
    void drawRowState()
    {
        const bool pressed = ImGui::IsItemActive();
        if (!pressed && !ImGui::IsItemHovered()) {
            return;
        }
        const auto min = ImGui::GetItemRectMin();
        const auto max = ImGui::GetItemRectMax();
        ImGui::GetWindowDrawList()->AddRectFilled(min, max, ImGui::GetColorU32(pressed ? theme::color::PRESSED : theme::color::HOVER), theme::RADIUS_FRAME);
        drawOutline(min, max, pressed ? theme::color::GLOW : theme::color::FRAME_BORDER);
    }

    /**
     * An option of the list that is open, at the cursor: the mark of the chosen one, and its text, with a muted text
     * after it, as a file's extension. The chosen option is a solid fill, and any other has the look of a row under the
     * ray. Returns whether it was pressed.
     */
    bool listOption(const std::string_view text, const bool chosen, const float width, const std::string_view suffix)
    {
        const auto pos = ImGui::GetCursorScreenPos();
        const ImVec2 max(pos.x + width, pos.y + theme::CONTROL_HEIGHT);
        const bool pressed = ImGui::InvisibleButton("option", ImVec2(width, theme::CONTROL_HEIGHT));
        auto color = theme::color::GREEN;
        // on the chosen option's solid fill, the muted text is in the color on green at 60%
        auto muted = theme::color::GREEN_MUTED;
        if (chosen) {
            ImGui::GetWindowDrawList()->AddRectFilled(pos, max, ImGui::GetColorU32(theme::color::GREEN), theme::RADIUS_FRAME);
            drawCheck(ImVec2(pos.x + theme::SPACE_3, pos.y + (theme::CONTROL_HEIGHT - CHECK_SIZE) * 0.5f), theme::color::ON_GREEN);
            color = theme::color::ON_GREEN;
            muted = (theme::color::ON_GREEN & ~IM_COL32_A_MASK) | (153u << IM_COL32_A_SHIFT);
        } else {
            drawRowState();
        }
        drawTextWithSuffix(pos.x + theme::SPACE_3 + CHECK_SIZE + theme::SPACE_3, max.x - theme::SPACE_4, pos.y, theme::CONTROL_HEIGHT, color, text, muted, suffix);
        return pressed;
    }

    /**
     * The entry of the list that is open that does something other than pick, as typing a new value, at the cursor: its
     * text where an option's is, in `green-muted`, and in `green` with the look of a row under the ray. Returns whether
     * it was pressed.
     */
    bool listAction(const std::string_view text, const float width)
    {
        const auto pos = ImGui::GetCursorScreenPos();
        const bool pressed = ImGui::InvisibleButton("action", ImVec2(width, theme::CONTROL_HEIGHT));
        drawRowState();
        const auto color = ImGui::IsItemHovered() || ImGui::IsItemActive() ? theme::color::GREEN : theme::color::GREEN_MUTED;
        drawText(pos.x + theme::SPACE_3 + CHECK_SIZE + theme::SPACE_3, pos.x + width - theme::SPACE_4, pos.y, theme::CONTROL_HEIGHT, color, text);
        return pressed;
    }

    /**
     * One line of text, as `drawText` draws it, with a text after it in another color, as a file's extension, which
     * keeps its room: the text ends in an ellipsis before it where both do not fit.
     */
    void drawTextWithSuffix(const float left, const float right, const float top, const float height, const ImU32 color, const std::string_view text, const ImU32 suffixColor,
        const std::string_view suffix)
    {
        if (suffix.empty()) {
            drawText(left, right, top, height, color, text);
            return;
        }
        const float textRight = (std::max)(left, right - textWidth(suffix));
        drawText(left, textRight, top, height, color, text);
        drawText((std::min)(left + textWidth(text), textRight), right, top, height, suffixColor, suffix);
    }

    /**
     * The box of a control that is as high as a row, from its left edge.
     */
    ImRect controlBox(const float left, const float top, const float width)
    {
        return { left, top, left + width, top + theme::CONTROL_HEIGHT };
    }

    /**
     * The frame of a control: the fill of a track, with an outline, which is red around an invalid value.
     */
    void drawFrame(const ImRect& box, const ValueLook look)
    {
        ImGui::GetWindowDrawList()->AddRectFilled(box.Min, box.Max, ImGui::GetColorU32(theme::color::TRACK), theme::RADIUS_FRAME);
        drawOutline(box.Min, box.Max, look == ValueLook::Invalid ? theme::color::RED : theme::color::FRAME_BORDER);
    }

    /**
     * A switcher that ends at the right: an Off and an On cell, with the one of its value marked in the color
     * of its value's look. On is a solid fill, and Off a ring, so a solid fill always says on. One with no
     * value has neither marked. Returns its left edge.
     */
    float drawSwitcher(const float right, const float top, const std::optional<bool> on, const ValueLook look)
    {
        const auto box = controlBox(right - TOGGLE_CELL_WIDTH * 2.0f, top, TOGGLE_CELL_WIDTH * 2.0f);
        const auto offCell = controlBox(box.Min.x, top, TOGGLE_CELL_WIDTH);
        const auto onCell = controlBox(offCell.Max.x, top, TOGGLE_CELL_WIDTH);
        const auto color = valueColor(look);
        drawFrame(box);
        if (on == true) {
            ImGui::GetWindowDrawList()->AddRectFilled(onCell.Min, onCell.Max, ImGui::GetColorU32(color), theme::RADIUS_FRAME, ImDrawFlags_RoundCornersRight);
        } else if (on == false) {
            ImGui::GetWindowDrawList()->AddRectFilled(offCell.Min, offCell.Max, ImGui::GetColorU32(theme::color::PRESSED), theme::RADIUS_FRAME, ImDrawFlags_RoundCornersLeft);
            drawOutline(offCell.Min, offCell.Max, color);
        }
        const auto onText = look == ValueLook::Pending ? theme::color::ON_AMBER : look == ValueLook::Invalid ? theme::color::ON_RED : theme::color::ON_GREEN;
        drawTextIn(offCell, "Off", on == false ? color : theme::color::GREEN_MUTED, 0.5f);
        drawTextIn(onCell, "On", on == true ? onText : theme::color::GREEN_MUTED, 0.5f);
        return box.Min.x;
    }

    /**
     * The frame of a button that is a part of a row, as a button of a stepper: an outline, and the look of a
     * control under the ray while the ray is on it, with a stronger one while the trigger is held on it.
     */
    void drawButtonFrame(const ImRect& box, const bool pointed, const bool held)
    {
        if (pointed || held) {
            ImGui::GetWindowDrawList()->AddRectFilled(box.Min, box.Max, ImGui::GetColorU32(held ? theme::color::PRESSED : theme::color::HOVER), theme::RADIUS_FRAME);
            drawOutline(box.Min, box.Max, held ? theme::color::GLOW : theme::color::GREEN);
        } else {
            drawOutline(box.Min, box.Max, theme::color::FRAME_BORDER);
        }
    }

    /**
     * The ring around a button, outside its box, as a dangerous one has it under the ray and a button of a row's style
     * while it is pressed.
     */
    void drawButtonRing(const ImRect& box, const ImU32 color)
    {
        ImGui::GetWindowDrawList()->AddRect(ImVec2(box.Min.x - BUTTON_RING, box.Min.y - BUTTON_RING),
            ImVec2(box.Max.x + BUTTON_RING, box.Max.y + BUTTON_RING),
            ImGui::GetColorU32(color),
            theme::RADIUS_FRAME + BUTTON_RING,
            0,
            BUTTON_RING);
    }

    /**
     * Adds an icon button in its box and draws its frame; the caller draws the icon.
     * Returns whether it was pressed and tells whether the ray is on it.
     */
    bool iconButton(const char* id, const ImRect& box, bool& pointed)
    {
        ImGui::SetCursorScreenPos(box.Min);
        const bool pressed = ImGui::InvisibleButton(id, box.GetSize());
        pointed = ImGui::IsItemHovered();
        drawButtonFrame(box, pointed, ImGui::IsItemActive());
        return pressed;
    }

    /**
     * A button of the footer or of the dialog, in its box. The primary button is a solid green, with its text
     * in the color on green, a lighter green under the ray, and an outline while it is pressed. The others
     * have the look of a button that is a part of a row, and a quiet one has no outline while the ray is not
     * on it. Inside `BeginDisabled` it is faded and is not pressed. Returns whether it was pressed.
     */
    bool button(const char* label, const ImRect& box, const ButtonLook look)
    {
        ImGui::SetCursorScreenPos(box.Min);
        const bool pressed = ImGui::InvisibleButton(label, box.GetSize());
        const bool pointed = ImGui::IsItemHovered();
        const bool held = ImGui::IsItemActive();
        auto* drawList = ImGui::GetWindowDrawList();
        if (look == ButtonLook::Primary) {
            drawList->AddRectFilled(box.Min, box.Max, ImGui::GetColorU32(pointed && !held ? theme::color::GLOW : theme::color::GREEN), theme::RADIUS_FRAME);
            if (held) {
                drawOutline(box.Min, box.Max, theme::color::GLOW);
            }
        } else if (look == ButtonLook::Danger) {
            drawList->AddRectFilled(box.Min, box.Max, ImGui::GetColorU32(theme::color::RED), theme::RADIUS_FRAME);
            if (pointed || held) {
                drawButtonRing(box, theme::color::RED);
            }
        } else if (look == ButtonLook::Secondary || pointed || held) {
            drawButtonFrame(box, pointed, held);
        }
        const auto textColor = look == ButtonLook::Primary ? theme::color::ON_GREEN : look == ButtonLook::Danger ? theme::color::ON_RED : theme::color::GREEN;
        drawTextIn(box, label, textColor, 0.5f);
        return pressed;
    }

    /**
     * Begins a dialog, as the design system's ConfirmDialog has it: a modal popup of ImGui in the middle of the panel,
     * which dims the panel behind it and takes every press, with a border of its color. It is opened while it is
     * asked, and closed as it is not, as after Back: ImGui opens and closes a popup inside a frame. Returns whether
     * its content is drawn, which ends with `ImGui::EndPopup`.
     */
    bool beginDialog(const char* id, const bool asked, const ImU32 border)
    {
        const auto popup = ImGui::GetID(id);
        if (asked && !ImGui::IsPopupOpen(popup, ImGuiPopupFlags_None)) {
            ImGui::OpenPopupEx(popup);
        }
        if (!ImGui::IsPopupOpen(popup, ImGuiPopupFlags_None)) {
            return false;
        }

        ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(DIALOG_WIDTH, 0.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(theme::SPACE_6, theme::SPACE_6));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(theme::SPACE_2, theme::SPACE_4));
        ImGui::PushStyleColor(ImGuiCol_Border, border);
        const bool shown = ImGui::BeginPopupModal(id,
            nullptr,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize);
        ImGui::PopStyleColor();
        ImGui::PopStyleVar(2);
        if (!shown) {
            return false;
        }
        if (!asked) {
            ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
            return false;
        }
        return true;
    }

    /**
     * The buttons of a dialog, from its right in their order, a little further from its text than that is from its
     * title. A button is as wide as its word with its padding, and no less than the least. Returns the place of the
     * one that was pressed, and -1 for none.
     */
    int dialogButtons(const std::span<const DialogButton> buttons)
    {
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + theme::SPACE_2);
        const auto pos = ImGui::GetCursorScreenPos();
        float right = pos.x + ImGui::GetContentRegionAvail().x;
        int pressed = -1;
        for (int place = 0; place < static_cast<int>(buttons.size()); ++place) {
            const auto& dialogButton = buttons[place];
            const float width = (std::max)(DIALOG_BUTTON_MIN_WIDTH, std::ceil(textWidth(dialogButton.label) + theme::SPACE_4 * 2.0f));
            const auto box = controlBox(right - width, pos.y, width);
            right -= width + theme::SPACE_2;
            ImGui::BeginDisabled(dialogButton.disabled);
            if (button(std::string(dialogButton.label).c_str(), box, dialogButton.look)) {
                pressed = place;
            }
            ImGui::EndDisabled();
        }
        ImGui::SetCursorScreenPos(pos);
        ImGui::Dummy(ImVec2(ImGui::GetContentRegionAvail().x, theme::CONTROL_HEIGHT));
        return pressed;
    }

    /**
     * One line of text in a box, in from its sides by a padding. The align sets it in that room: 0 at the
     * left, 0.5 in the middle and 1 at the right. A text that is wider than the room ends in an ellipsis.
     */
    void drawTextIn(const ImRect& box, const std::string_view text, const ImU32 color, const float align, const float padding)
    {
        const float room = box.GetWidth() - padding * 2.0f;
        // on a whole pixel to the left, so a text that just fits its room gets no ellipsis
        const float left = std::floor(box.Min.x + padding + (std::max)(room - textWidth(text), 0.0f) * align);
        drawText(left, box.Max.x - padding, box.Min.y, box.GetHeight(), color, text);
    }

    /**
     * The color a value is drawn in, with the fill and the grab of a slider: amber while it waits for a Save,
     * and red while it is invalid.
     */
    ImU32 valueColor(const ValueLook look)
    {
        switch (look) {
        case ValueLook::Pending:
            return theme::color::AMBER;
        case ValueLook::Invalid:
            return theme::color::RED;
        case ValueLook::Saved:
            break;
        }
        return theme::color::GREEN;
    }

    /**
     * The width of a tag: its text in the detail size, with the room inside its outline.
     */
    float tagWidth(const std::string_view text)
    {
        ImGui::PushFont(nullptr, theme::FONT_SIZE_DETAIL);
        const float width = textWidth(text);
        ImGui::PopFont();
        return std::ceil(width + (theme::SPACE_2 + theme::BORDER_FRAME) * 2.0f);
    }

    /**
     * A tag, from its left in the middle of a height: a short text in the detail size and the muted color, in an
     * outline of a frame's border, as the design system's tag has it.
     */
    void drawTag(const float left, const float middle, const std::string_view text)
    {
        const ImVec2 min(std::floor(left), std::floor(middle - RESTART_MARK_HEIGHT * 0.5f));
        const ImVec2 max(min.x + tagWidth(text), min.y + RESTART_MARK_HEIGHT);
        drawOutline(min, max, theme::color::FRAME_BORDER);
        ImGui::PushFont(nullptr, theme::FONT_SIZE_DETAIL);
        drawText(min.x + theme::BORDER_FRAME + theme::SPACE_2, max.x, min.y, RESTART_MARK_HEIGHT, theme::color::GREEN_MUTED, text);
        ImGui::PopFont();
    }

    /**
     * The width of the tag of when a pending change takes effect: its mark and its word.
     */
    float appliesTagWidth(const std::string_view word)
    {
        return tagWidth(word) + APPLIES_MARK_WIDTH + theme::SPACE_1;
    }

    /**
     * The tag of when a pending change takes effect, from its left in the middle of a height, as the design
     * system's TakesEffect has it: the mark of its applies and its word, all in amber, as the pending value is.
     */
    void drawAppliesTag(const float left, const float middle, const model::Applies applies, const std::string_view word)
    {
        const ImVec2 min(std::floor(left), std::floor(middle - RESTART_MARK_HEIGHT * 0.5f));
        const ImVec2 max(min.x + appliesTagWidth(word), min.y + RESTART_MARK_HEIGHT);
        drawOutline(min, max, theme::color::AMBER);
        const float markLeft = min.x + theme::BORDER_FRAME + theme::SPACE_2;
        drawAppliesMark(applies, ImVec2(markLeft + APPLIES_MARK_WIDTH * 0.5f, min.y + RESTART_MARK_HEIGHT * 0.5f));
        ImGui::PushFont(nullptr, theme::FONT_SIZE_DETAIL);
        drawText(markLeft + APPLIES_MARK_WIDTH + theme::SPACE_1, max.x, min.y, RESTART_MARK_HEIGHT, theme::color::AMBER, word);
        ImGui::PopFont();
    }

    /**
     * The width of the Restart mark, the tag of a setting that takes effect after the game is restarted.
     */
    float restartMarkWidth()
    {
        return tagWidth(RESTART_TEXT);
    }

    void drawRestartMark(const float left, const float middle)
    {
        drawTag(left, middle, RESTART_TEXT);
    }

    /**
     * Counts that each have their mark before them, as `2 errors` after the invalid mark, ending at a right edge
     * in the current font and in the middle of a control's height from a top. Returns their left edge.
     */
    float drawMarkedCounts(const float right, const float top, const std::span<const MarkedCount> counts)
    {
        const auto markWidth = [](const CountMark mark) {
            return mark == CountMark::Pending ? PENDING_MARK_WIDTH : WARNING_MARK_WIDTH;
        };

        float width = 0.0f;
        for (const auto& count : counts) {
            width += (width > 0.0f ? theme::SPACE_2 * 2.0f : 0.0f) + markWidth(count.mark) + theme::SPACE_2 + textWidth(count.text);
        }
        const float left = std::floor(right - width);
        const float middle = top + theme::CONTROL_HEIGHT * 0.5f;
        float x = left;
        for (const auto& count : counts) {
            const ImVec2 center(x + markWidth(count.mark) * 0.5f, middle);
            if (count.mark == CountMark::Invalid) {
                drawInvalidMark(center);
            } else if (count.mark == CountMark::Warning) {
                drawWarningMark(center);
            } else {
                drawPendingMark(center);
            }
            x += markWidth(count.mark) + theme::SPACE_2;
            drawText(x, right, top, theme::CONTROL_HEIGHT, count.color, count.text);
            x += textWidth(count.text) + theme::SPACE_2 * 2.0f;
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
    void drawFoldingHeading(const std::string_view text, const ImVec2& pos, const float width, const bool folded, const std::span<const MarkedCount> counts, const TextMark mark,
        const std::span<const model::TextRange> found)
    {
        // the room a rule keeps at the least, as the design system has it
        constexpr float RULE_MIN_WIDTH = theme::SPACE_4;
        auto color = theme::color::GREEN_MUTED;
        if (mark == TextMark::Invalid || mark == TextMark::Warning) {
            color = mark == TextMark::Invalid ? theme::color::RED : theme::color::AMBER;
        }
        const float arrowLeft = pos.x + HEADING_TEXT_INSET;
        drawFoldArrow(ImVec2(arrowLeft, pos.y + (theme::CONTROL_HEIGHT - FOLD_ARROW_SIZE) * 0.5f), !folded, color);
        float ruleRight = pos.x + width;
        if (!counts.empty()) {
            ImGui::PushFont(nullptr, theme::FONT_SIZE_DETAIL);
            ruleRight = drawMarkedCounts(ruleRight - theme::SPACE_3, pos.y, counts) - theme::SPACE_2;
            ImGui::PopFont();
        }
        float textLeft = arrowLeft + FOLD_ARROW_SIZE + theme::SPACE_2;
        if (mark != TextMark::None) {
            drawMark(mark, ImVec2(textLeft + WARNING_MARK_WIDTH * 0.5f, pos.y + theme::CONTROL_HEIGHT * 0.5f));
            textLeft += WARNING_MARK_WIDTH + theme::SPACE_2;
        }
        const float textRight = (std::min)(textLeft + textWidth(text), ruleRight - RULE_MIN_WIDTH - theme::SPACE_3);
        drawText(textLeft, textRight, pos.y, theme::CONTROL_HEIGHT, color, text);
        drawUnderlines(textLeft, textRight, pos.y, theme::CONTROL_HEIGHT, color, text, found);
        const float ruleLeft = std::ceil(textRight) + theme::SPACE_3;
        if (ruleLeft < ruleRight) {
            const float ruleTop = std::floor(pos.y + (theme::CONTROL_HEIGHT - theme::BORDER_FRAME) * 0.5f);
            ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(ruleLeft, ruleTop), ImVec2(ruleRight, ruleTop + theme::BORDER_FRAME), ImGui::GetColorU32(theme::color::RULE));
        }
    }

    /**
     * The mark of a value that waits for a Save, around a point: a dot, amber but on a fill whose text has
     * another color, as the selected row of the tree.
     */
    void drawPendingMark(const ImVec2& center, const ImU32 color)
    {
        constexpr float RADIUS = 5.0f;
        ImGui::GetWindowDrawList()->AddCircleFilled(center, RADIUS, ImGui::GetColorU32(color));
    }

    /**
     * The width of the pending mark of a change by when it takes effect: the dot where it is not known.
     */
    float pendingMarkWidth(const model::Applies applies)
    {
        return applies == model::Applies::Unknown ? PENDING_MARK_WIDTH : APPLIES_MARK_WIDTH;
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
            drawPendingMark(center);
            break;
        }
    }

    /**
     * The mark of an invalid value, around a point: a red triangle with an `!` in it, as the design system
     * draws it in a box of 20 by 18.
     */
    void drawInvalidMark(const ImVec2& center)
    {
        const ImVec2 origin(std::floor(center.x - 10.0f), std::floor(center.y - 9.0f));
        const auto at = [&origin](const float x, const float y) {
            return ImVec2(origin.x + x, origin.y + y);
        };
        auto* drawList = ImGui::GetWindowDrawList();
        drawList->AddTriangleFilled(at(10.0f, 1.0f), at(19.0f, 17.0f), at(1.0f, 17.0f), ImGui::GetColorU32(theme::color::RED));
        const auto bang = ImGui::GetColorU32(theme::color::ON_RED);
        drawList->AddRectFilled(at(9.0f, 6.0f), at(11.0f, 12.0f), bang);
        drawList->AddRectFilled(at(9.0f, 13.4f), at(11.0f, 15.4f), bang);
    }

    /**
     * The mark of a warning, around a point: a hollow amber triangle with an `!` in it, as the design system
     * draws it in a box of 20 by 18. It is not the invalid mark, which is filled red.
     */
    void drawWarningMark(const ImVec2& center)
    {
        constexpr float THICKNESS = 2.0f;
        const ImVec2 origin(std::floor(center.x - 10.0f), std::floor(center.y - 9.0f));
        const auto at = [&origin](const float x, const float y) {
            return ImVec2(origin.x + x, origin.y + y);
        };
        auto* drawList = ImGui::GetWindowDrawList();
        const auto color = ImGui::GetColorU32(theme::color::AMBER);
        drawList->AddTriangle(at(10.0f, 1.5f), at(18.5f, 16.5f), at(1.5f, 16.5f), color, THICKNESS);
        drawList->AddRectFilled(at(9.0f, 6.0f), at(11.0f, 12.0f), color);
        drawList->AddRectFilled(at(9.0f, 13.0f), at(11.0f, 15.0f), color);
    }

    /**
     * The mark of a result that is there, around a point: a green ring with an `i` in it, as the design system
     * draws it in a box of 20.
     */
    void drawInfoMark(const ImVec2& center)
    {
        const ImVec2 origin(std::floor(center.x - 10.0f), std::floor(center.y - 10.0f));
        const auto at = [&origin](const float x, const float y) {
            return ImVec2(origin.x + x, origin.y + y);
        };
        auto* drawList = ImGui::GetWindowDrawList();
        const auto color = ImGui::GetColorU32(theme::color::GREEN);
        drawList->AddCircle(at(10.0f, 10.0f), 8.5f, color, 0, 2.0f);
        drawList->AddRectFilled(at(9.0f, 8.5f), at(11.0f, 15.0f), color);
        drawList->AddCircleFilled(at(10.0f, 5.8f), 1.3f, color);
    }

    /**
     * The Reset icon around a point: most of a circle, open at its left, with an arrowhead at its upper end, as the
     * design system draws it in a box of 24, at a part of that size. The stroke keeps its width.
     */
    void drawResetIcon(const ImVec2& center, const float scale)
    {
        // the circle from its lower left end around its right to its upper left end, in radians
        constexpr float RADIUS = 7.5f;
        constexpr float ARC_START = 2.744f;
        constexpr float ARC_END = -2.396f;
        const auto at = [&center, scale](const float x, const float y) {
            return ImVec2(center.x + (x - 12.0f) * scale, center.y + (y - 12.0f) * scale);
        };
        const auto color = ImGui::GetColorU32(theme::color::GREEN);
        auto* drawList = ImGui::GetWindowDrawList();
        drawList->PathArcTo(at(12.1f, 12.1f), RADIUS * scale, ARC_START, ARC_END);
        drawList->PathLineTo(at(4.0f, 10.0f));
        drawList->PathStroke(color, ICON_THICKNESS);
        const std::array head{ at(4.0f, 5.0f), at(4.0f, 10.0f), at(9.0f, 10.0f) };
        drawList->AddPolyline(head.data(), static_cast<int>(head.size()), color, ICON_THICKNESS);
    }

    /**
     * The Discard icon around a point, an undo: an arrowhead to the left, whose tail turns back down in a half circle,
     * in amber as the pending change it takes back. As the design system draws it in a box of 24, at a part of that
     * size. The stroke keeps its width.
     */
    void drawDiscardIcon(const ImVec2& center, const float scale)
    {
        const auto at = [&center, scale](const float x, const float y) {
            return ImVec2(center.x + (x - 12.0f) * scale, center.y + (y - 12.0f) * scale);
        };
        const auto color = ImGui::GetColorU32(theme::color::AMBER);
        auto* drawList = ImGui::GetWindowDrawList();
        drawList->PathLineTo(at(4.0f, 9.0f));
        drawList->PathArcTo(at(14.0f, 14.0f), 5.0f * scale, -IM_PI / 2.0f, IM_PI / 2.0f);
        drawList->PathLineTo(at(8.0f, 19.0f));
        drawList->PathStroke(color, ICON_THICKNESS);
        const std::array head{ at(9.0f, 4.0f), at(4.0f, 9.0f), at(9.0f, 14.0f) };
        drawList->AddPolyline(head.data(), static_cast<int>(head.size()), color, ICON_THICKNESS);
    }

    /**
     * A mark of a text around a point: the warning mark, the invalid mark or the info mark, and nothing for none.
     */
    void drawMark(const TextMark mark, const ImVec2& center)
    {
        switch (mark) {
        case TextMark::Warning:
            drawWarningMark(center);
            break;
        case TextMark::Invalid:
            drawInvalidMark(center);
            break;
        case TextMark::Info:
            drawInfoMark(center);
            break;
        default:
            break;
        }
    }

    /**
     * The arrow of a row that opens and closes, in its box from its top left corner: it points down while
     * the row is open, and right while it is closed, as the design system draws it in a box of 14.
     */
    void drawFoldArrow(const ImVec2& topLeft, const bool open, const ImU32 color)
    {
        const auto at = [&topLeft](const float x, const float y) {
            return ImVec2(topLeft.x + x, topLeft.y + y);
        };
        if (open) {
            ImGui::GetWindowDrawList()->AddTriangleFilled(at(2.0f, 4.0f), at(12.0f, 4.0f), at(7.0f, 11.0f), ImGui::GetColorU32(color));
        } else {
            ImGui::GetWindowDrawList()->AddTriangleFilled(at(4.0f, 2.0f), at(11.0f, 7.0f), at(4.0f, 12.0f), ImGui::GetColorU32(color));
        }
    }

    /**
     * A filled arrow around a point, as the buttons of a stepper and a dropdown have, and a slider's smaller.
     */
    void drawControlArrow(const ImVec2& center, const ControlArrow arrow, const float scale)
    {
        // the corners of each arrow in its box, as the design system draws them
        static constexpr std::array<std::array<ImVec2, 3>, 3> CORNERS{ {
            { ImVec2(10.0f, 2.0f), ImVec2(3.0f, 7.0f), ImVec2(10.0f, 12.0f) },
            { ImVec2(4.0f, 2.0f), ImVec2(11.0f, 7.0f), ImVec2(4.0f, 12.0f) },
            { ImVec2(2.0f, 4.0f), ImVec2(12.0f, 4.0f), ImVec2(7.0f, 11.0f) },
        } };
        const auto& corners = CORNERS[static_cast<std::size_t>(arrow)];
        const ImVec2 origin(center.x - ARROW_SIZE * scale * 0.5f, center.y - ARROW_SIZE * scale * 0.5f);
        const auto at = [&origin, scale](const ImVec2& corner) {
            return ImVec2(origin.x + corner.x * scale, origin.y + corner.y * scale);
        };
        ImGui::GetWindowDrawList()->AddTriangleFilled(at(corners[0]), at(corners[1]), at(corners[2]), ImGui::GetColorU32(theme::color::GREEN));
    }

    /**
     * A button with an arrow, as a stepper and a slider have at their ends. One that goes nowhere is disabled. Any
     * other has the look of a control under the ray while the ray is on it, and a stronger one while the trigger is
     * held on it.
     */
    void drawStepButton(const ImRect& box, const ControlArrow arrow, const bool enabled, const bool pointed, const bool held, const float arrowScale)
    {
        ImGui::BeginDisabled(!enabled);
        drawButtonFrame(box, enabled && pointed, enabled && held);
        drawControlArrow(box.GetCenter(), arrow, arrowScale);
        ImGui::EndDisabled();
    }
}
