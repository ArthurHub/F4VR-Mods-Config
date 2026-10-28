#include "TitleBar.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <string>
#include <string_view>

#include <imgui.h>

#include "Drawing.h"
#include "Theme.h"
#include "common/CommonUtils.h"

namespace
{
    using namespace mods_config;

    // the menu's name: the first breadcrumb of the title bar, which stands for the menu's front page
    constexpr auto TITLE = "MODS CONFIG FO4";

    // what stands between two breadcrumbs: U+00BB, as its bytes in UTF-8
    constexpr auto CRUMB_SEPARATOR = "\xC2\xBB";

    // the crumb that stands for the pages between the mod and the page, when their crumbs do not fit: U+2026,
    // as its bytes in UTF-8
    constexpr auto PAGES_ELIDED = "\xE2\x80\xA6";

    // the breadcrumb of the Help page, after the menu's
    constexpr auto HELP_CRUMB = "Help";

    // the breadcrumb of the front page's Developer section, before a config spec's report
    constexpr auto DEVELOPER_CRUMB = "Developer";

    // what the help line says of the title bar's buttons
    constexpr auto HELP_HELP = "What each control of the menu does, with the buttons your config has for it.";
    constexpr auto MINIMIZE_HELP = "Hides the menu, as you left it. Restore on your offhand wrist shows it again.";
    constexpr auto CLOSE_HELP = "Closes the menu.";

    // the least the mod's breadcrumb is shortened to, beside the long name of a page:
    constexpr float MOD_CRUMB_MIN_WIDTH = theme::CONTROL_HEIGHT * 2.0f;

    enum class CrumbLook : std::uint8_t
    {
        // a crumb before the last, which is pressed to go there
        Link,
        // the last crumb: where the player is
        Current,
        // the menu's crumb while the menu's front page is open: a solid fill, as a selected row has
        Selected,
        // the ellipsis that stands for the pages between the mod and the page, which is not pressed
        Elided,
    };

    float crumbWidth(const std::string_view text)
    {
        return ui::textWidth(text) + theme::SPACE_2 * 2.0f;
    }

    /**
     * A breadcrumb of the title bar, in the width it is given: a text that does not fit ends in an ellipsis.
     * Returns whether it was pressed, which only a link is. The crumb of the pages that do not fit is muted, as a
     * link is while the ray is not on it.
     */
    bool crumb(const char* id, const std::string_view text, const ImVec2& min, const float width, const CrumbLook look)
    {
        const auto max = ImVec2(min.x + width, min.y + theme::CONTROL_HEIGHT);
        bool pressed = false;
        auto color = theme::color::GREEN;
        if (look == CrumbLook::Selected) {
            ImGui::GetWindowDrawList()->AddRectFilled(min, max, theme::color::GREEN, theme::RADIUS_FRAME);
            color = theme::color::ON_GREEN;
        } else if (look == CrumbLook::Link) {
            ImGui::SetCursorScreenPos(min);
            pressed = ImGui::InvisibleButton(id, ImVec2(width, theme::CONTROL_HEIGHT));
            ui::drawRowState();
            if (!ImGui::IsItemHovered() && !ImGui::IsItemActive()) {
                color = theme::color::GREEN_MUTED;
            }
        } else if (look == CrumbLook::Elided) {
            color = theme::color::GREEN_MUTED;
        }
        ui::drawText(min.x + theme::SPACE_2, max.x - theme::SPACE_1, min.y, theme::CONTROL_HEIGHT, color, text);
        return pressed;
    }

    /**
     * The Minimize icon around a point: a line at the bottom of a box of 24, as the design system draws it.
     */
    void drawMinimizeIcon(const ImVec2& center)
    {
        ImGui::GetWindowDrawList()->AddLine(ImVec2(center.x - 6.0f, center.y + 5.0f),
            ImVec2(center.x + 6.0f, center.y + 5.0f),
            ImGui::GetColorU32(theme::color::GREEN),
            ui::ICON_THICKNESS);
    }

    /**
     * The Close icon around a point: a cross in a box of 24, as the design system draws it.
     */
    void drawCloseIcon(const ImVec2& center)
    {
        constexpr float HALF = 6.0f;
        const auto color = ImGui::GetColorU32(theme::color::GREEN);
        auto* drawList = ImGui::GetWindowDrawList();
        drawList->AddLine(ImVec2(center.x - HALF, center.y - HALF), ImVec2(center.x + HALF, center.y + HALF), color, ui::ICON_THICKNESS);
        drawList->AddLine(ImVec2(center.x + HALF, center.y - HALF), ImVec2(center.x - HALF, center.y + HALF), color, ui::ICON_THICKNESS);
    }

}

namespace mods_config::ui
{
    /**
     * Draws the breadcrumbs and Help, Minimize and Close buttons, with overflowing pages collapsed to an
     * ellipsis and long names shortened. A config spec's report, shown over the menu's front page, has the crumbs
     * of the menu, of the Developer section, and of the config spec's name. Returns the pressed selection,
     * Developer crumb or button and the hovered help.
     */
    TitleBarDrawn drawTitleBar(const std::vector<model::Mod>& mods, const Selection& selection, const bool helpShown, const model::Mod* report)
    {
        const auto pos = ImGui::GetCursorScreenPos();
        const float width = ImGui::GetContentRegionAvail().x;
        TitleBarDrawn bar;

        // the buttons from the right of the bar, before the crumbs, which are in the title's size
        struct Button
        {
            TitleButton button;
            const char* name;
            const char* help;
        };

        constexpr std::array BUTTONS{ Button{ TitleButton::Close, "Close", CLOSE_HELP },
            Button{ TitleButton::Minimize, "Minimize", MINIMIZE_HELP },
            Button{ TitleButton::Help, "Help", HELP_HELP } };
        const float buttonTop = std::floor(pos.y + (theme::TITLEBAR_HEIGHT - theme::CONTROL_HEIGHT) * 0.5f);
        float buttonLeft = std::floor(pos.x + width - theme::SPACE_2);
        for (const auto& button : BUTTONS) {
            buttonLeft -= theme::CONTROL_HEIGHT;
            const auto box = controlBox(buttonLeft, buttonTop, theme::CONTROL_HEIGHT);
            bool pointed = false;
            if (iconButton(button.name, box, pointed)) {
                bar.pressedButton = button.button;
            }
            if (pointed) {
                bar.pointed = PointedRow{ .text = button.name, .help = button.help };
            }
            if (button.button == TitleButton::Help) {
                drawTextIn(box, "?", theme::color::GREEN, 0.5f);
            } else if (button.button == TitleButton::Minimize) {
                drawMinimizeIcon(box.GetCenter());
            } else {
                drawCloseIcon(box.GetCenter());
            }
            buttonLeft -= theme::SPACE_2;
        }

        ImGui::PushFont(nullptr, theme::FONT_SIZE_TITLE);
        // on whole pixels, so a crumb is exactly as wide as the text it was measured for
        ImVec2 at(std::floor(pos.x + theme::SPACE_2), buttonTop);
        const float right = buttonLeft;
        const float gap = theme::SPACE_1;
        const float separatorWidth = textWidth(CRUMB_SEPARATOR);
        const auto drawSeparator = [&at, right, separatorWidth, gap] {
            drawText(at.x, right, at.y, theme::CONTROL_HEIGHT, theme::color::GREEN_MUTED, CRUMB_SEPARATOR);
            at.x += separatorWidth + gap;
        };
        const auto* mod = selection.mod && !helpShown ? &mods[*selection.mod] : nullptr;

        if (crumb("menu", TITLE, at, crumbWidth(TITLE), mod || helpShown || report ? CrumbLook::Link : CrumbLook::Selected)) {
            bar.pressed = Selection{};
        }
        at.x += crumbWidth(TITLE) + gap;

        if (helpShown) {
            drawSeparator();
            crumb("help", HELP_CRUMB, at, (std::min)(crumbWidth(HELP_CRUMB), right - at.x), CrumbLook::Current);
        } else if (report) {
            drawSeparator();
            if (crumb("developer", DEVELOPER_CRUMB, at, crumbWidth(DEVELOPER_CRUMB), CrumbLook::Link)) {
                bar.developerPressed = true;
            }
            at.x += crumbWidth(DEVELOPER_CRUMB) + gap;
            drawSeparator();
            crumb("report", report->displayName, at, (std::min)(crumbWidth(report->displayName), right - at.x), CrumbLook::Current);
        } else if (mod) {
            // the pages on the way to the selected page, by their names: MCM's own list sets a page in under
            // another by spaces before its name, which are the tree's
            std::vector<std::string> pageNames;
            const auto* pages = &mod->pages;
            for (const auto place : selection.page) {
                pageNames.push_back(common::trim((*pages)[place].name));
                pages = &(*pages)[place].pages;
            }

            // the room of a crumb after another: the gap after the one before, the separator, and a gap
            const float before = gap + separatorWidth + gap;
            float pagesWidth = 0.0f;
            for (const auto& name : pageNames) {
                pagesWidth += before + crumbWidth(name);
            }
            const float modRoom = right - at.x - separatorWidth - gap;
            const bool elided = pageNames.size() > 1 && crumbWidth(mod->displayName) + pagesWidth > modRoom;
            if (elided) {
                pagesWidth = before + crumbWidth(PAGES_ELIDED) + before + crumbWidth(pageNames.back());
            }
            const float modWidth = (std::min)(crumbWidth(mod->displayName), (std::max)(modRoom - pagesWidth, MOD_CRUMB_MIN_WIDTH));

            drawSeparator();
            if (crumb("mod", mod->displayName, at, modWidth, pageNames.empty() ? CrumbLook::Current : CrumbLook::Link)) {
                bar.pressed = Selection{ .mod = selection.mod, .page = {} };
            }
            at.x += modWidth + gap;

            if (elided) {
                drawSeparator();
                crumb("elided", PAGES_ELIDED, at, crumbWidth(PAGES_ELIDED), CrumbLook::Elided);
                at.x += crumbWidth(PAGES_ELIDED) + gap;
            }
            for (std::size_t level = elided ? pageNames.size() - 1 : 0; level < pageNames.size(); ++level) {
                const bool last = level + 1 == pageNames.size();
                const float pageWidth = last ? (std::min)(crumbWidth(pageNames[level]), right - at.x - separatorWidth - gap) : crumbWidth(pageNames[level]);
                drawSeparator();
                ImGui::PushID(static_cast<int>(level));
                if (crumb("page", pageNames[level], at, pageWidth, last ? CrumbLook::Current : CrumbLook::Link)) {
                    bar.pressed = Selection{ .mod = selection.mod, .page = { selection.page.begin(), selection.page.begin() + static_cast<std::ptrdiff_t>(level + 1) } };
                }
                ImGui::PopID();
                at.x += pageWidth + gap;
            }
        }
        ImGui::PopFont();

        const float lineTop = pos.y + theme::TITLEBAR_HEIGHT - theme::BORDER_FRAME;
        ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(pos.x, lineTop), ImVec2(pos.x + width, lineTop + theme::BORDER_FRAME), ImGui::GetColorU32(theme::color::RULE));
        // the crumbs moved the cursor: the bar takes its room from where it began
        ImGui::SetCursorScreenPos(pos);
        ImGui::Dummy(ImVec2(width, theme::TITLEBAR_HEIGHT));
        return bar;
    }

}
