#include "HelpPage.h"

#include <array>
#include <optional>
#include <string>
#include <string_view>

#include <imgui.h>

#include "Drawing.h"
#include "Theme.h"

namespace
{
    using namespace mods_config;

    /**
     * A line of the page as the help line has a row's help, `Name: text`, wrapped to the page's width, after the mark
     * of when a pending change takes effect, for a line that tells one.
     */
    void drawHelpText(const std::string_view name, const std::string_view text, const std::optional<model::Applies> mark = std::nullopt)
    {
        const auto pos = ImGui::GetCursorScreenPos();
        const float markRoom = mark ? ui::pendingMarkWidth(*mark) + theme::SPACE_2 : 0.0f;
        const float left = pos.x + ui::HEADING_TEXT_INSET;
        const float width = ImGui::GetContentRegionAvail().x - ui::HEADING_TEXT_INSET - theme::SPACE_2 - markRoom;
        const auto label = std::string(name) + ": ";
        const std::array pieces{ ui::TextPiece{ .text = label, .color = theme::color::GREEN }, ui::TextPiece{ .text = text, .color = theme::color::GREEN_MUTED } };
        const int lines = ui::countTextLines(pieces, width);
        if (mark) {
            ui::drawAppliesMark(*mark, ImVec2(left + ui::pendingMarkWidth(*mark) * 0.5f, pos.y + ImGui::GetFontSize() * 0.5f));
        }
        ui::drawTextPieces(pieces, ImVec2(left + markRoom, pos.y), width, lines);
        ImGui::Dummy(ImVec2(width, static_cast<float>(lines) * ImGui::GetFontSize()));
    }
}

namespace mods_config::ui
{
    /**
     * Draws the Help page's introduction and control descriptions, wrapped to the page's width, then what Save does
     * and what each mark of when a change takes effect means, as the design system's FullPanelHelp has it.
     * Control names are green and their descriptions use the muted text color.
     */
    void drawHelpPage(const std::vector<ControlHelp>& controls)
    {
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + theme::SPACE_1);
        drawPageText("How the menu is used, with the buttons your config has.");
        sectionHeading("Controls", ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x);
        for (const auto& control : controls) {
            drawHelpText(control.name, control.text);
        }

        sectionHeading("Saving", ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x);
        drawHelpText("Save",
            "Writes the changes of the mod you are on, and the mod acts on them in its own way. A mod can say when: a changed row then has its mark, and the "
            "help line its tag.");
        drawHelpText("Live", "As you change it. Discard takes it back.", model::Applies::Live);
        drawHelpText("Save", "After Save.", model::Applies::Save);
        drawHelpText("Restart", "After the game is restarted.", model::Applies::Restart);
        drawHelpText("No mark", "The mod does not say, as no MCM mod does. Save writes it all the same.", model::Applies::Unknown);
    }

}
