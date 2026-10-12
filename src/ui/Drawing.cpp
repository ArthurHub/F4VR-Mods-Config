#include "Drawing.h"

#include <imgui_internal.h>

#include "Theme.h"

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
