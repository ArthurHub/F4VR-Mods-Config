#include "FrontPage.h"

#include <algorithm>
#include <format>
#include <string_view>
#include <utility>

#include <imgui.h>

#include "Drawing.h"
#include "PageRows.h"
#include "Theme.h"
#include "Version.h"

namespace
{
    using namespace mods_config;

    // the width of a quick launch button, in the panel's pixels
    constexpr float BUTTON_WIDTH = 160.0f;

    // the headings of the sections, and their help
    constexpr auto QUICK_LAUNCH = "Quick launch";
    constexpr auto QUICK_LAUNCH_HELP = "Mods with a configuration of their own, which opens from here.";
    constexpr auto NOTICES = "Notices";
    constexpr auto NOTICES_HELP = "What needs your attention: a restart of the game, or a mod that is not available.";

    /**
     * A heading that folds at the cursor, as wide as the page: a press folds or opens it. Tells its text and
     * help while the ray is on it, and whether a press did something.
     */
    void foldingHeading(const char* text, const char* help, bool& folded, ui::FrontPageDrawn& drawn)
    {
        const auto pos = ImGui::GetCursorScreenPos();
        const float width = ui::rowWidth();
        const auto row = ui::addFoldingRow(text, folded);
        drawn.pressActed = drawn.pressActed || row.pressed;
        ui::drawFoldingHeading(text, pos, width, folded, row.pointed || row.held, {});
        if (row.pointed) {
            drawn.pointed = ui::PointedRow{ .text = text, .help = help };
        }
    }

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
     * A row of the quick launch section, laid out as a setting's row: set in a level under its heading, the mod's
     * name, and its button in the control column, which says its own text or Open. The whole row is pressed, so the
     * button is only drawn. A disabled row is drawn so, and a press on it does nothing.
     * Tells whether the ray is on the row.
     */
    bool quickLaunchRow(const ui::QuickLaunchEntry& entry, bool& pointed)
    {
        ImGui::Indent(theme::SPACE_3);
        const auto pos = ImGui::GetCursorScreenPos();
        const float width = ui::rowWidth();
        const bool enabled = entry.notAvailable.empty();
        ImGui::BeginDisabled(!enabled);
        const bool pressed = ImGui::InvisibleButton(entry.name.c_str(), ImVec2(width, theme::CONTROL_HEIGHT));
        pointed = ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled);
        ui::drawRowState();

        // the button ends where a setting's control does, before the room of its Reset
        const float buttonRight = pos.x + width - theme::CONTROL_HEIGHT - theme::SPACE_2;
        const float buttonLeft = buttonRight - BUTTON_WIDTH;
        ui::drawOutline(ImVec2(buttonLeft, pos.y), ImVec2(buttonRight, pos.y + theme::CONTROL_HEIGHT), theme::color::FRAME_BORDER);
        const std::string_view label = entry.buttonText.empty() ? std::string_view("Open") : std::string_view(entry.buttonText);
        ui::drawTextIn(ImRect(buttonLeft, pos.y, buttonRight, pos.y + theme::CONTROL_HEIGHT), label, theme::color::GREEN, 0.5f, theme::SPACE_3);

        ui::drawText(pos.x + theme::SPACE_3, buttonLeft - theme::SPACE_3, pos.y, theme::CONTROL_HEIGHT, theme::color::GREEN, entry.name);
        ImGui::EndDisabled();
        ImGui::Unindent(theme::SPACE_3);
        return pressed && enabled;
    }

}

namespace mods_config::ui
{
    /**
     * The quick launch entries the front page lists, each with what opens it.
     */
    void FrontPage::setQuickLaunch(std::vector<QuickLaunchEntry> entries)
    {
        _quickLaunch = std::move(entries);
    }

    /**
     * Draws the menu's introduction, quick launch entries, restart notices and unavailable mods.
     * Empty sections are omitted. The heading of each section folds: a press on it folds or opens it, and a
     * press on a quick launch entry opens it.
     * Returns the row the ray is on, and whether a press did something.
     */
    FrontPageDrawn FrontPage::draw(const std::vector<FrontPageMod>& mods)
    {
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + theme::SPACE_1);
        drawPageText(std::format("Settings for the mods you have installed. Pick a mod on the left to see its settings.\nVersion {}", Version::NAME));

        FrontPageDrawn drawn;
        if (!_quickLaunch.empty()) {
            foldingHeading(QUICK_LAUNCH, QUICK_LAUNCH_HELP, _quickLaunchFolded, drawn);
        }
        if (!_quickLaunchFolded) {
            for (const auto& entry : _quickLaunch) {
                bool onRow = false;
                if (quickLaunchRow(entry, onRow)) {
                    _result = entry.open();
                    _resultOf = entry.name;
                    drawn.pressActed = true;
                }
                if (!onRow) {
                    continue;
                }
                drawn.pointed = PointedRow{ .text = entry.name, .help = entry.notAvailable.empty() ? entry.help : entry.notAvailable };
                if (_resultOf == entry.name && !_result.message.empty()) {
                    if (_result.failed) {
                        drawn.pointed->help = _result.message;
                        drawn.pointed->invalid = true;
                    } else {
                        drawn.pointed->result = _result.message;
                    }
                }
            }
        }

        const bool hasNotices = std::ranges::any_of(mods, [](const FrontPageMod& mod) {
            return mod.savedRestartCount > 0 || !mod.notAvailable.empty();
        });
        if (!hasNotices) {
            return drawn;
        }
        foldingHeading(NOTICES, NOTICES_HELP, _noticesFolded, drawn);
        if (_noticesFolded) {
            return drawn;
        }
        for (const auto& mod : mods) {
            const auto restarts = mod.savedRestartCount;
            if (restarts > 0) {
                restartNotice(std::format("Restart the game: {} has {} saved change{} effect then.", mod.displayName, restarts, restarts == 1 ? " that takes" : "s that take"));
            }
        }
        for (const auto& mod : mods) {
            if (!mod.notAvailable.empty()) {
                drawPageText(std::format("{} is not available: {}.", mod.displayName, mod.notAvailable));
            }
        }
        return drawn;
    }

    /**
     * Opens the sections again, and forgets the result of the last press on an entry, as the menu is closed.
     */
    void FrontPage::reset()
    {
        _quickLaunchFolded = false;
        _noticesFolded = false;
        _resultOf.clear();
        _result = {};
    }
}
