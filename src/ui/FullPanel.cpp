#include "FullPanel.h"

#include <algorithm>

#include <imgui.h>

#include "Drawing.h"
#include "Theme.h"
#include "common/CommonUtils.h"

namespace
{
    using namespace mods_config;

    // the menu's name: the first breadcrumb of the title bar, which stands for the menu's front page
    constexpr auto TITLE = "MODS CONFIG VR";

    // a button with a word on it, in the panel's pixels: a size of the design system that is not a token of it
    constexpr float BUTTON_WIDTH = 160.0f;

    /**
     * A row of the quick launch section, laid out as a setting's row: the mod's name after the gutter, and an
     * Open button in the control column. The whole row is pressed, so the button is only drawn.
     */
    bool quickLaunchRow(const char* name)
    {
        const auto pos = ImGui::GetCursorScreenPos();
        const float width = ImGui::GetContentRegionAvail().x;
        const bool pressed = ImGui::InvisibleButton(name, ImVec2(width, theme::CONTROL_HEIGHT));
        ui::drawRowState();

        // the button ends where a setting's control does, before the room of its Reset
        const float buttonRight = pos.x + width - theme::CONTROL_HEIGHT - theme::SPACE_2;
        const float buttonLeft = buttonRight - BUTTON_WIDTH;
        ui::drawOutline(ImVec2(buttonLeft, pos.y), ImVec2(buttonRight, pos.y + theme::CONTROL_HEIGHT), theme::color::FRAME_BORDER);
        const float labelLeft = buttonLeft + (BUTTON_WIDTH - ImGui::CalcTextSize("Open").x) * 0.5f;
        ui::drawText(labelLeft, buttonRight, pos.y, theme::CONTROL_HEIGHT, theme::color::GREEN, "Open");

        ui::drawText(pos.x + theme::SPACE_6 + theme::SPACE_3, buttonLeft - theme::SPACE_3, pos.y, theme::CONTROL_HEIGHT, theme::color::GREEN, name);
        return pressed;
    }
}

namespace mods_config::ui
{
    /**
     * The mods the menu lists, from every source they come from. They are kept in order of their display names.
     */
    void FullPanel::setMods(std::vector<model::Mod> mods)
    {
        std::ranges::sort(mods, common::CaseInsensitiveComparator(), &model::Mod::displayName);
        _mods = std::move(mods);
    }

    void FullPanel::setQuickLaunch(std::vector<QuickLaunchEntry> entries)
    {
        _quickLaunch = std::move(entries);
    }

    /**
     * Draws the panel into the current ImGui window: the title bar, and under it the tree of the mods with
     * the menu's front page beside it.
     */
    void FullPanel::draw()
    {
        if (!_styleApplied) {
            theme::applyStyle(ImGui::GetStyle());
            _styleApplied = true;
        }

        drawTitleBar();
        // the body stands a little further from the title bar than a row from a row
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + theme::SPACE_3 - ImGui::GetStyle().ItemSpacing.y);

        _modTree.draw(_mods);

        ImGui::SameLine(0.0f, theme::SPACE_4);
        if (ImGui::BeginChild("Content")) {
            drawFrontPage();
        }
        ImGui::EndChild();
    }

    /**
     * The title bar, with a line under it. The menu's name is its first breadcrumb and stands for the menu's
     * front page, so it is drawn selected while that page is open: a solid fill, as a selected row has.
     */
    void FullPanel::drawTitleBar() const
    {
        const auto pos = ImGui::GetCursorScreenPos();
        const float width = ImGui::GetContentRegionAvail().x;
        auto* drawList = ImGui::GetWindowDrawList();

        ImGui::SetWindowFontScale(theme::FONT_SCALE_TITLE);
        const float crumbTop = pos.y + (theme::TITLEBAR_HEIGHT - theme::CONTROL_HEIGHT) * 0.5f;
        const float crumbLeft = pos.x + theme::SPACE_2;
        const float crumbRight = crumbLeft + ImGui::CalcTextSize(TITLE).x + theme::SPACE_2 * 2.0f;
        drawList->AddRectFilled(ImVec2(crumbLeft, crumbTop), ImVec2(crumbRight, crumbTop + theme::CONTROL_HEIGHT), ImGui::GetColorU32(theme::color::GREEN), theme::RADIUS_FRAME);
        drawText(crumbLeft + theme::SPACE_2, crumbRight, crumbTop, theme::CONTROL_HEIGHT, theme::color::ON_GREEN, TITLE);
        ImGui::SetWindowFontScale(theme::FONT_SCALE_BODY);

        const float lineTop = pos.y + theme::TITLEBAR_HEIGHT - theme::BORDER_FRAME;
        drawList->AddRectFilled(ImVec2(pos.x, lineTop), ImVec2(pos.x + width, lineTop + theme::BORDER_FRAME), ImGui::GetColorU32(theme::color::RULE));
        ImGui::Dummy(ImVec2(width, theme::TITLEBAR_HEIGHT));
    }

    /**
     * The menu's front page, which is what the menu shows while no mod is selected: what the menu is, with its
     * version, and the quick launch section. The section is left out while it has no entry.
     */
    void FullPanel::drawFrontPage() const
    {
        // a page's own text is in from the edges of the page, as its rows' texts are
        ImGui::SetCursorPos(ImVec2(ImGui::GetCursorPosX() + theme::SPACE_2, ImGui::GetCursorPosY() + theme::SPACE_1));
        ImGui::PushStyleColor(ImGuiCol_Text, theme::color::GREEN_MUTED);
        ImGui::PushTextWrapPos(ImGui::GetContentRegionMax().x - theme::SPACE_2);
        ImGui::Text("Settings for the mods you have installed. Pick a mod on the left to see its settings.\nVersion %s", Version::NAME.data());
        ImGui::PopTextWrapPos();
        ImGui::PopStyleColor();

        if (_quickLaunch.empty()) {
            return;
        }
        ImGui::SeparatorText("Quick launch");
        for (const auto& entry : _quickLaunch) {
            if (quickLaunchRow(entry.name.c_str())) {
                entry.open();
            }
        }
    }
}
