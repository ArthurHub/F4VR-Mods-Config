#include "Drawing.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include <imgui_internal.h>

#include "Theme.h"

namespace
{
    using namespace mods_config;

    // The size of a font tag that is drawn as the panel's body text. A config's sizes are those of MCM's menu,
    // where the ordinary text is about this size, so a text is as much larger or smaller than the rows as there.
    constexpr float MARKUP_BODY_SIZE = 20.0f;

    /**
     * A word of a line of text with the spaces after it, which a line is wrapped between.
     */
    struct Word
    {
        std::string_view text;

        // with its spaces, and without them
        float width = 0.0f;
        float inkWidth = 0.0f;

        float fontSize = 0.0f;
        ImU32 color = 0;
    };

    float widthOf(const std::string_view text, const float fontSize)
    {
        return text.empty() ? 0.0f : ImGui::GetFont()->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, text.data(), text.data() + text.size()).x;
    }

    /**
     * Adds the words of a text in one size and one color to those of its line.
     */
    void addWords(const std::string_view text, const float fontSize, const ImU32 color, std::vector<Word>& words)
    {
        for (std::size_t start = 0; start < text.size();) {
            const auto inkEnd = (std::min)(text.find(' ', start), text.size());
            const auto end = (std::min)(text.find_first_not_of(' ', inkEnd), text.size());
            words.push_back({ .text = text.substr(start, end - start),
                .width = widthOf(text.substr(start, end - start), fontSize),
                .inkWidth = widthOf(text.substr(start, inkEnd - start), fontSize),
                .fontSize = fontSize,
                .color = color });
            start = end;
        }
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
            addWords(piece.text, ImGui::GetFontSize(), piece.color, words);
        }
        return words;
    }

    /**
     * Draws words on one line, set in its room by the line's align. Words of a smaller size stand on the
     * bottom of the line. Returns the height of the line.
     */
    float drawWords(const std::span<const Word> words, const model::TextAlign align, const ImVec2& pos, const float width)
    {
        float height = 0.0f;
        float lineWidth = 0.0f;
        for (const auto& word : words) {
            height = (std::max)(height, word.fontSize);
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
        for (const auto& word : words) {
            ImGui::GetWindowDrawList()->AddText(ImGui::GetFont(),
                word.fontSize,
                ImVec2(std::floor(x), std::floor(pos.y + height - word.fontSize)),
                word.color,
                word.text.data(),
                word.text.data() + word.text.size());
            x += word.width;
        }
        return height;
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
        ImGui::RenderTextEllipsis(ImGui::GetWindowDrawList(),
            ImVec2(left, y),
            ImVec2(right, y + ImGui::GetFontSize()),
            right,
            right,
            text.data(),
            text.data() + text.size(),
            nullptr);
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
     * color, or else in the given color, and an empty line is as high as a line of body text.
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
                addWords(run.text, fontSize, run.color ? IM_COL32(*run.color >> 16, (*run.color >> 8) & 0xFF, *run.color & 0xFF, 255) : color, words);
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
        const auto* font = ImGui::GetFont();
        const float fontSize = ImGui::GetFontSize();
        const float fontScale = fontSize / font->FontSize;

        for (std::size_t line = 0; line < lineCount; ++line) {
            const float y = pos.y + static_cast<float>(line) * fontSize;
            const auto first = starts[line];
            if (line + 1 < lineCount || lineCount == starts.size()) {
                const auto end = line + 1 < starts.size() ? starts[line + 1] : words.size();
                drawWords(std::span<const Word>(words).subspan(first, end - first), model::TextAlign::Left, ImVec2(pos.x, y), width);
                continue;
            }

            // the last line of a text that is cut: the words that leave room for the ellipsis, and the first in any case
            const float room = width - font->EllipsisWidth * fontScale;
            auto last = first;
            float lineWidth = words[first].width;
            while (last + 1 < words.size() && lineWidth + words[last + 1].inkWidth <= room) {
                lineWidth += words[++last].width;
            }
            drawWords(std::span<const Word>(words).subspan(first, last + 1 - first), model::TextAlign::Left, ImVec2(pos.x, y), width);

            ImVec2 at(std::floor(pos.x + lineWidth - words[last].width + words[last].inkWidth), std::floor(y));
            for (int i = 0; i < font->EllipsisCharCount; ++i) {
                font->RenderChar(ImGui::GetWindowDrawList(), fontSize, at, words[last].color, font->EllipsisChar);
                at.x += font->EllipsisCharStep * fontScale;
            }
        }
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
            ImDrawFlags_None,
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
     * The frame of a control: the fill of a track, with an outline.
     */
    void drawFrame(const ImRect& box)
    {
        ImGui::GetWindowDrawList()->AddRectFilled(box.Min, box.Max, ImGui::GetColorU32(theme::color::TRACK), theme::RADIUS_FRAME);
        drawOutline(box.Min, box.Max, theme::color::FRAME_BORDER);
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
}
