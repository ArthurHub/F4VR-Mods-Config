#include "TitleBar.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <span>
#include <string>
#include <string_view>
#include <variant>

#include <imgui.h>

#include "Drawing.h"
#include "PanelTargets.h"
#include "Theme.h"
#include "perf/Perf.h"

namespace
{
    using namespace mods_config;

    // what stands between two breadcrumbs: U+00BB, as its bytes in UTF-8
    constexpr auto CRUMB_SEPARATOR = "\xC2\xBB";

    // the crumb that stands for the pages between the mod and the page, when their crumbs do not fit: U+2026,
    // as its bytes in UTF-8
    constexpr auto PAGES_ELIDED = "\xE2\x80\xA6";

    // what Help says after its mark until the player first opens it
    constexpr auto HELP_LABEL = "Help";

    // the title bar's Help
    constexpr auto HELP_MARK = "?";

    // what the last crumb has after its name while the page has only its rows that match
    constexpr auto FILTERED = " (filtered)";

    // what the help line says of the title bar's buttons
    constexpr auto HELP_HELP = "How the menu works, with the buttons your config has.";
    constexpr auto MINIMIZE_HELP = "Hides the menu, as you left it. Restore on your offhand wrist shows it again.";
    constexpr auto CLOSE_HELP = "Closes the menu.";

    // the least the mod's breadcrumb is shortened to, beside the long name of a page:
    constexpr float MOD_CRUMB_MIN_WIDTH = theme::CONTROL_HEIGHT * 2.0f;

    float crumbWidth(const std::string_view text)
    {
        return controls::textWidth(text) + theme::SPACE_2 * 2.0f;
    }

    /**
     * A breadcrumb of the title bar, in the width it is given: a text that does not fit ends in an ellipsis.
     * Returns whether it was pressed, which only a link is. A suffix is after the text in the disabled color, dimmer
     * than a link, and the text shortens before it does.
     */
    bool crumb(const char* id, const std::string_view text, const ImVec2& min, const float width, const ui::CrumbLook look, const std::string_view suffix = {})
    {
        const auto max = ImVec2(min.x + width, min.y + theme::CONTROL_HEIGHT);
        ui::panelTargets().add(ui::TargetKind::Crumb, text, ImRect(min, max));
        bool pressed = false;
        auto color = theme::color::GREEN;
        if (look == ui::CrumbLook::Selected) {
            ImGui::GetWindowDrawList()->AddRectFilled(min, max, theme::color::GREEN, theme::RADIUS_FRAME);
            color = theme::color::ON_GREEN;
        } else if (look == ui::CrumbLook::Link) {
            ImGui::SetCursorScreenPos(min);
            pressed = ImGui::InvisibleButton(id, ImVec2(width, theme::CONTROL_HEIGHT));
            controls::drawRowState();
            if (!ImGui::IsItemHovered() && !ImGui::IsItemActive()) {
                color = theme::color::GREEN_MUTED;
            }
        }
        const float textLeft = min.x + theme::SPACE_2;
        const float suffixLeft = (std::min)(textLeft + controls::textWidth(text), max.x - theme::SPACE_2 - controls::textWidth(suffix));
        controls::drawText(textLeft, suffix.empty() ? max.x - theme::SPACE_1 : suffixLeft, min.y, theme::CONTROL_HEIGHT, color, text);
        if (!suffix.empty()) {
            controls::drawText(suffixLeft, max.x, min.y, theme::CONTROL_HEIGHT, theme::color::DISABLED, suffix);
        }
        return pressed;
    }

    /**
     * The ellipsis that stands for the pages between the mod and the page when their crumbs do not fit: muted, as a
     * link is while the ray is not on it, and not pressed.
     */
    void drawElided(const ImVec2& min)
    {
        controls::drawText(min.x + theme::SPACE_2, min.x + crumbWidth(PAGES_ELIDED) - theme::SPACE_1, min.y, theme::CONTROL_HEIGHT, theme::color::GREEN_MUTED, PAGES_ELIDED);
    }

    /**
     * The width of Help until the player first opens it: `?` and `Help`, `space-2` apart, with `space-4` each side.
     */
    float highlightedHelpWidth()
    {
        return std::ceil(controls::textWidth(HELP_MARK) + theme::SPACE_2 + controls::textWidth(HELP_LABEL) + theme::SPACE_4 * 2.0f);
    }

    /**
     * Help until the player first opens it, so a new player finds it: `? Help` in the important style, with a ring
     * while it is pressed. Returns whether it was pressed and tells whether the ray is on it.
     */
    bool highlightedHelp(const char* id, const ImRect& box, bool& pointed)
    {
        ImGui::SetCursorScreenPos(box.Min);
        const bool pressed = ImGui::InvisibleButton(id, box.GetSize());
        pointed = ImGui::IsItemHovered();
        const bool held = ImGui::IsItemActive();
        if (pointed || held) {
            ImGui::GetWindowDrawList()->AddRectFilled(box.Min, box.Max, ImGui::GetColorU32(held ? theme::color::PRESSED : theme::color::HOVER), theme::RADIUS_FRAME);
        }
        controls::drawOutline(box.Min, box.Max, theme::color::ACCENT);
        if (held) {
            controls::drawButtonRing(box, theme::color::ACCENT);
        }
        const float textLeft = box.Min.x + theme::SPACE_4;
        controls::drawText(textLeft, box.Max.x, box.Min.y, theme::CONTROL_HEIGHT, theme::color::ACCENT, HELP_MARK);
        controls::drawText(textLeft + controls::textWidth(HELP_MARK) + theme::SPACE_2, box.Max.x, box.Min.y, theme::CONTROL_HEIGHT, theme::color::ACCENT, HELP_LABEL);
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
            controls::ICON_THICKNESS);
    }

    /**
     * The Close icon around a point: a cross in a box of 24, as the design system draws it.
     */
    void drawCloseIcon(const ImVec2& center)
    {
        constexpr float HALF = 6.0f;
        const auto color = ImGui::GetColorU32(theme::color::GREEN);
        auto* drawList = ImGui::GetWindowDrawList();
        drawList->AddLine(ImVec2(center.x - HALF, center.y - HALF), ImVec2(center.x + HALF, center.y + HALF), color, controls::ICON_THICKNESS);
        drawList->AddLine(ImVec2(center.x + HALF, center.y - HALF), ImVec2(center.x - HALF, center.y + HALF), color, controls::ICON_THICKNESS);
    }

}

namespace mods_config::ui
{
    /**
     * Draws the breadcrumbs and Help, Minimize and Close buttons. The crumbs after the menu's shorten to fit: a mod's
     * pages between it and the last collapse to an ellipsis, then the mod's crumb shortens, down to a least width, and
     * the last ends in an ellipsis. The Developer crumb before a report keeps its width. Help stands out while it is highlighted, and is wider, so the crumbs end sooner.
     * The last crumb of a filtered page says so after its name. Returns the place of the pressed crumb in the list,
     * the pressed button and the hovered help.
     */
    TitleBarDrawn drawTitleBar(const std::vector<Crumb>& crumbs, const bool highlightHelp)
    {
        F4CF_PERF_FUNCTION();
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
            const bool highlighted = highlightHelp && button.button == TitleButton::Help;
            const float buttonWidth = highlighted ? highlightedHelpWidth() : theme::CONTROL_HEIGHT;
            buttonLeft -= buttonWidth;
            const auto box = controls::controlBox(buttonLeft, buttonTop, buttonWidth);
            panelTargets().add(TargetKind::TitleButton, button.name, box);
            bool pointed = false;
            if (highlighted ? highlightedHelp(button.name, box, pointed) : controls::iconButton(button.name, box, pointed)) {
                bar.pressedButton = button.button;
            }
            if (pointed) {
                bar.pointed = PointedRow{ .text = button.name, .help = button.help };
            }
            if (button.button == TitleButton::Minimize) {
                drawMinimizeIcon(box.GetCenter());
            } else if (button.button == TitleButton::Close) {
                drawCloseIcon(box.GetCenter());
            } else if (!highlighted) {
                controls::drawTextIn(box, HELP_MARK, theme::color::GREEN, 0.5f);
            }
            buttonLeft -= theme::SPACE_2;
        }

        ImGui::PushFont(nullptr, theme::FONT_SIZE_TITLE);
        // on whole pixels, so a crumb is exactly as wide as the text it was measured for
        ImVec2 at(std::floor(pos.x + theme::SPACE_2), buttonTop);
        const float right = buttonLeft;
        const float gap = theme::SPACE_1;
        const float separatorWidth = controls::textWidth(CRUMB_SEPARATOR);
        const auto drawSeparator = [&at, right, separatorWidth, gap] {
            controls::drawText(at.x, right, at.y, theme::CONTROL_HEIGHT, theme::color::GREEN_MUTED, CRUMB_SEPARATOR);
            at.x += separatorWidth + gap;
        };

        const auto& menu = crumbs.front();
        if (crumb("menu", menu.text, at, crumbWidth(menu.text), menu.look)) {
            bar.pressed = 0;
        }
        at.x += crumbWidth(menu.text) + gap;

        if (crumbs.size() > 1) {
            // the second crumb, as a mod's, and the crumbs after it, as its pages'
            const auto& second = crumbs[1];
            const std::span<const Crumb> pages(crumbs.begin() + 2, crumbs.end());

            // the room of a crumb after another: the gap after the one before, the separator, and a gap
            const float before = gap + separatorWidth + gap;
            const std::string_view suffix = crumbs.back().filtered ? FILTERED : "";
            const float suffixWidth = suffix.empty() ? 0.0f : controls::textWidth(suffix);
            float pagesWidth = pages.empty() ? 0.0f : suffixWidth;
            for (const auto& page : pages) {
                pagesWidth += before + crumbWidth(page.text);
            }
            const float secondRoom = right - at.x - separatorWidth - gap;
            const bool elided = pages.size() > 1 && crumbWidth(second.text) + pagesWidth > secondRoom;
            if (elided) {
                pagesWidth = before + crumbWidth(PAGES_ELIDED) + before + crumbWidth(pages.back().text) + suffixWidth;
            }
            // a mod's crumb, and the last, shorten, down to a least width; the Developer crumb before a report does not
            const bool shortens = pages.empty() || std::holds_alternative<Selection>(*second.target);
            const float secondFullWidth = crumbWidth(second.text) + (pages.empty() ? suffixWidth : 0.0f);
            const float secondWidth = shortens ? (std::min)(secondFullWidth, (std::max)(secondRoom - pagesWidth, MOD_CRUMB_MIN_WIDTH)) : secondFullWidth;

            drawSeparator();
            if (crumb("second", second.text, at, secondWidth, second.look, pages.empty() ? suffix : "")) {
                bar.pressed = 1;
            }
            at.x += secondWidth + gap;

            if (elided) {
                drawSeparator();
                drawElided(at);
                at.x += crumbWidth(PAGES_ELIDED) + gap;
            }
            for (std::size_t level = elided ? pages.size() - 1 : 0; level < pages.size(); ++level) {
                const auto& page = pages[level];
                const bool last = level + 1 == pages.size();
                const float pageWidth = last ? (std::min)(crumbWidth(page.text) + suffixWidth, right - at.x - separatorWidth - gap) : crumbWidth(page.text);
                drawSeparator();
                ImGui::PushID(static_cast<int>(level));
                if (crumb("page", page.text, at, pageWidth, page.look, last ? suffix : "")) {
                    bar.pressed = level + 2;
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
