#include "HelpPage.h"

#include <array>

#include <imgui.h>

#include "Drawing.h"
#include "Theme.h"

namespace mods_config::ui
{
    /**
     * Draws the Help page's introduction and control descriptions, wrapped to the page's width.
     * Control names are green and their descriptions use the muted text color.
     */
    void drawHelpPage(const std::vector<ControlHelp>& controls)
    {
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + theme::SPACE_1);
        drawPageText("How the menu is used, with the buttons your config has.");
        ImGui::SeparatorText("Controls");

        const float width = ImGui::GetContentRegionAvail().x - theme::SPACE_2 * 2.0f;
        for (const auto& control : controls) {
            const auto name = control.name + ": ";
            const std::array pieces{ TextPiece{ .text = name, .color = theme::color::GREEN }, TextPiece{ .text = control.text, .color = theme::color::GREEN_MUTED } };
            const int lines = countTextLines(pieces, width);
            const auto pos = ImGui::GetCursorScreenPos();
            drawTextPieces(pieces, ImVec2(pos.x + theme::SPACE_2, pos.y), width, lines);
            ImGui::Dummy(ImVec2(width, static_cast<float>(lines) * ImGui::GetFontSize()));
        }
    }

}
