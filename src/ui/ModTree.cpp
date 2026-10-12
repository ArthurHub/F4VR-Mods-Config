#include "ModTree.h"

#include <imgui.h>

#include "Drawing.h"
#include "Theme.h"

namespace
{
    using namespace mods_config;

    // The tree's sizes that are not tokens of the design system, in the panel's pixels.
    // Its header:
    constexpr float HEADER_HEIGHT = 40.0f;
    // the room it keeps on its right, for the tip of its selected row and for its scroll bar:
    constexpr float RIGHT_ROOM = 20.0f + theme::SPACE_1 + theme::SCROLLBAR_SIZE + theme::SPACE_1;
    // and a row: the same width whether or not the scroll bar shows
    constexpr float ROW_WIDTH = theme::NAV_WIDTH - theme::SPACE_2 - RIGHT_ROOM;
}

namespace mods_config::ui
{
    /**
     * Draws the tree at the cursor of the current ImGui window. The header stays, and the rows scroll under it.
     */
    void ModTree::draw(const std::vector<model::Mod>& mods)
    {
        ImGui::PushStyleColor(ImGuiCol_ChildBg, theme::color::PLATE);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(theme::SPACE_2, theme::SPACE_2));
        const bool shown = ImGui::BeginChild("Mods", ImVec2(theme::NAV_WIDTH, 0.0f), ImGuiChildFlags_AlwaysUseWindowPadding);
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();

        // the rows of the tree are closer to each other than the rows of a page
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(theme::SPACE_3, theme::SPACE_1));
        if (shown) {
            const auto pos = ImGui::GetCursorScreenPos();
            const auto count = std::to_string(mods.size());
            ImGui::SetWindowFontScale(theme::FONT_SCALE_DETAIL);
            const float countLeft = pos.x + ROW_WIDTH - theme::SPACE_2 - ImGui::CalcTextSize(count.c_str()).x;
            drawText(pos.x + theme::SPACE_2, countLeft, pos.y, HEADER_HEIGHT, theme::color::GREEN_MUTED, "Mods");
            drawText(countLeft, pos.x + ROW_WIDTH, pos.y, HEADER_HEIGHT, theme::color::GREEN_MUTED, count.c_str());
            ImGui::SetWindowFontScale(theme::FONT_SCALE_BODY);
            ImGui::Dummy(ImVec2(ROW_WIDTH, HEADER_HEIGHT));

            if (ImGui::BeginChild("ModRows")) {
                for (const auto& mod : mods) {
                    const auto rowPos = ImGui::GetCursorScreenPos();
                    ImGui::Dummy(ImVec2(ROW_WIDTH, theme::CONTROL_HEIGHT));
                    drawText(rowPos.x + theme::SPACE_2, rowPos.x + ROW_WIDTH - theme::SPACE_3, rowPos.y, theme::CONTROL_HEIGHT, theme::color::GREEN, mod.displayName.c_str());
                }
            }
            ImGui::EndChild();
        }
        ImGui::PopStyleVar();
        ImGui::EndChild();
    }
}
