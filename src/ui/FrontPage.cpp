#include "FrontPage.h"

#include <format>
#include <string_view>

#include <imgui.h>

#include "Drawing.h"
#include "Theme.h"
#include "Version.h"

namespace
{
    using namespace mods_config;

    // the width of a quick launch button, in the panel's pixels
    constexpr float BUTTON_WIDTH = 160.0f;

    /**
     * A notice of the menu's front page at the cursor that the game has to be restarted: the Restart mark, and
     * after it the text in green, wrapped in the rest of the page's width. It is in from the edges of the page,
     * as its texts are.
     */
    void restartNotice(const std::string_view text)
    {
        const auto pos = ImGui::GetCursorScreenPos();
        ui::drawRestartMark(pos.x + theme::SPACE_2, pos.y + ImGui::GetFontSize() * 0.5f);
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + theme::SPACE_2 + ui::restartMarkWidth() + theme::SPACE_2);
        ImGui::PushStyleColor(ImGuiCol_Text, theme::color::GREEN);
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - theme::SPACE_2);
        ImGui::TextUnformatted(text.data(), text.data() + text.size());
        ImGui::PopTextWrapPos();
        ImGui::PopStyleColor();
    }

    /**
     * A row of the quick launch section, laid out as a setting's row: the mod's name after the gutter, and an
     * Open button in the control column. The whole row is pressed, so the button is only drawn.
     * Tells whether the ray is on the row.
     */
    bool quickLaunchRow(const char* name, bool& pointed)
    {
        const auto pos = ImGui::GetCursorScreenPos();
        const float width = ImGui::GetContentRegionAvail().x;
        const bool pressed = ImGui::InvisibleButton(name, ImVec2(width, theme::CONTROL_HEIGHT));
        pointed = ImGui::IsItemHovered();
        ui::drawRowState();

        // the button ends where a setting's control does, before the room of its Reset
        const float buttonRight = pos.x + width - theme::CONTROL_HEIGHT - theme::SPACE_2;
        const float buttonLeft = buttonRight - BUTTON_WIDTH;
        ui::drawOutline(ImVec2(buttonLeft, pos.y), ImVec2(buttonRight, pos.y + theme::CONTROL_HEIGHT), theme::color::FRAME_BORDER);
        const float labelLeft = buttonLeft + (BUTTON_WIDTH - ui::textWidth("Open")) * 0.5f;
        ui::drawText(labelLeft, buttonRight, pos.y, theme::CONTROL_HEIGHT, theme::color::GREEN, "Open");

        ui::drawText(pos.x + theme::SPACE_6 + theme::SPACE_3, buttonLeft - theme::SPACE_3, pos.y, theme::CONTROL_HEIGHT, theme::color::GREEN, name);
        return pressed;
    }

}

namespace mods_config::ui
{
    /**
     * Draws the menu's introduction, quick launch entries, restart notices and unavailable mods.
     * Empty sections are omitted. Returns the pressed entry and the row the ray is on.
     */
    FrontPageDrawn drawFrontPage(const std::vector<QuickLaunchEntry>& entries, const std::vector<FrontPageMod>& mods)
    {
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + theme::SPACE_1);
        drawPageText(std::format("Settings for the mods you have installed. Pick a mod on the left to see its settings.\nVersion {}", Version::NAME));

        FrontPageDrawn drawn;
        if (!entries.empty()) {
            ImGui::SeparatorText("Quick launch");
        }
        for (std::size_t index = 0; index < entries.size(); ++index) {
            const auto& entry = entries[index];
            bool onRow = false;
            if (quickLaunchRow(entry.name.c_str(), onRow)) {
                drawn.pressed = index;
            }
            if (onRow) {
                drawn.pointed = PointedRow{ .text = entry.name, .help = entry.help };
            }
        }

        bool hasNotices = false;
        const auto notices = [&hasNotices] {
            if (!hasNotices) {
                ImGui::SeparatorText("Notices");
                hasNotices = true;
            }
        };
        for (const auto& mod : mods) {
            const auto restarts = mod.savedRestartCount;
            if (restarts > 0) {
                notices();
                restartNotice(std::format("Restart the game: {} has {} saved change{} effect then.", mod.displayName, restarts, restarts == 1 ? " that takes" : "s that take"));
            }
        }
        for (const auto& mod : mods) {
            if (!mod.notAvailable.empty()) {
                notices();
                drawPageText(std::format("{} is not available: {}.", mod.displayName, mod.notAvailable));
            }
        }
        return drawn;
    }

}
