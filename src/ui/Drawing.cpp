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
     * Adds the words of a run to those of its line.
     */
    void addWords(const model::TextRun& run, const ImU32 color, std::vector<Word>& words)
    {
        const float fontSize = ImGui::GetFontSize() * (run.size ? static_cast<float>(*run.size) / MARKUP_BODY_SIZE : 1.0f);
        const std::string_view text = run.text;
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
                addWords(run, run.color ? IM_COL32(*run.color >> 16, (*run.color >> 8) & 0xFF, *run.color & 0xFF, 255) : color, words);
            }
            if (words.empty()) {
                y += ImGui::GetFontSize();
                continue;
            }

            std::size_t first = 0;
            float lineWidth = 0.0f;
            for (std::size_t i = 0; i < words.size(); ++i) {
                if (i > first && lineWidth + words[i].inkWidth > width) {
                    y += drawWords(std::span<const Word>(words).subspan(first, i - first), line.align, ImVec2(pos.x, y), width);
                    first = i;
                    lineWidth = 0.0f;
                }
                lineWidth += words[i].width;
            }
            y += drawWords(std::span<const Word>(words).subspan(first), line.align, ImVec2(pos.x, y), width);
        }
        return y - pos.y;
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
}
