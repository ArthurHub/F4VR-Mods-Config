#include "Drawing.h"

#include <algorithm>
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

    // The line under an underlined text, in its font size: its top is below the baseline by Roboto's underline
    // position, and it is as thick as its underline, which are 150 and 100 of the 2400 units ImGui sizes the font by.
    constexpr float UNDERLINE_OFFSET = 0.0625f;
    constexpr float UNDERLINE_THICKNESS = 0.042f;

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
     * The words of pieces of text that are one text, in the current font size.
     */
    std::vector<Word> wordsOf(const std::span<const ui::TextPiece> pieces)
    {
        std::vector<Word> words;
        for (const auto& piece : pieces) {
            addWords(piece.text, { .fontSize = ImGui::GetFontSize(), .color = piece.color, .font = ImGui::GetFont(), .underline = false }, words);
        }
        return words;
    }

    /**
     * Draws words on one line, set in its room by the line's align. Words of a smaller size stand on the
     * bottom of the line. An underlined word has its line under it and under the spaces after it, but not
     * under the spaces that end the line. Returns the height of the line.
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
     */
    void drawTextRuns(const float left, const float right, const float top, const float height, const ImU32 color, const std::span<const model::TextRun> runs)
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
            return;
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
    }

    /**
     * The heading of a section of rows: its text, and a line from it to the right edge.
     */
    void sectionHeading(const std::string_view text)
    {
        // by its end: the text is a mod's, and ImGui would cut it at a "##"
        ImGui::SeparatorTextEx(0, text.data(), text.data() + text.size(), 0.0f);
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
     * The mark of a value that waits for a Save, around a point: an amber dot.
     */
    void drawPendingMark(const ImVec2& center)
    {
        constexpr float RADIUS = 5.0f;
        ImGui::GetWindowDrawList()->AddCircleFilled(center, RADIUS, ImGui::GetColorU32(theme::color::AMBER));
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
}
