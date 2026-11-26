#include "TitleBar.h"

#include <array>
#include <string_view>
#include <variant>

#include "Drawing.h"
#include "Theme.h"
#include "imgui/controls/TitleBars.h"
#include "perf/Perf.h"

namespace
{
    using namespace mods_config;

    // the title bar's Help, and what it says after its mark until the player first opens it
    constexpr auto HELP_MARK = "?";
    constexpr auto HELP_LABEL = "Help";

    // what the last crumb has after its name while the page has only its rows that match
    constexpr auto FILTERED = " (filtered)";

    // what the help line says of the title bar's buttons
    constexpr auto HELP_HELP = "How the menu works, with the buttons your config has.";
    constexpr auto MINIMIZE_HELP = "Hides the menu, as you left it. Restore on your offhand wrist shows it again.";
    constexpr auto CLOSE_HELP = "Closes the menu.";

    /**
     * A title bar's button of the panel: the framework's button, which the bar draws, what it is, and what the help
     * line says of it.
     */
    struct PanelButton
    {
        ui::TitleButton button;
        const char* help;
    };

    // from the right of the bar, as the framework's buttons are
    constexpr std::array PANEL_BUTTONS{ PanelButton{ ui::TitleButton::Close, CLOSE_HELP },
        PanelButton{ ui::TitleButton::Minimize, MINIMIZE_HELP },
        PanelButton{ ui::TitleButton::Help, HELP_HELP } };
}

namespace mods_config::ui
{
    /**
     * Draws the breadcrumbs and the Help, Minimize and Close buttons, as the framework's title bar. The crumbs after the
     * menu's shorten to fit; the Developer crumb before a report keeps its width. Help stands out while it is
     * highlighted, and is wider, so the crumbs end sooner. The last crumb of a filtered page says so after its name.
     * Returns the place of the pressed crumb in the list, the pressed button and the hovered help.
     */
    TitleBarDrawn drawTitleBar(const std::vector<Crumb>& crumbs, const bool highlightHelp)
    {
        F4CF_PERF_FUNCTION();
        std::vector<controls::Crumb> barCrumbs;
        barCrumbs.reserve(crumbs.size());
        for (const auto& crumb : crumbs) {
            // a crumb that goes to a section of the front page, as Developer before a report, keeps its width
            const bool shortens = !crumb.target || std::holds_alternative<Selection>(*crumb.target);
            barCrumbs.push_back({ .text = crumb.text, .look = crumb.look, .shortens = shortens });
        }
        const std::array<controls::TitleButton, 3> buttons{ controls::TitleButton{ .name = "Close", .icon = controls::Icon::Cross },
            controls::TitleButton{ .name = "Minimize", .icon = controls::Icon::Minimize },
            controls::TitleButton{ .name = "Help", .text = HELP_MARK, .highlightText = highlightHelp ? HELP_LABEL : "" } };
        // the menu's crumb alone never says it is filtered
        const bool filtered = crumbs.size() > 1 && crumbs.back().filtered;

        const auto bar = controls::titleBar({ .crumbs = barCrumbs, .buttons = buttons, .suffix = filtered ? FILTERED : "", .height = theme::TITLEBAR_HEIGHT });
        TitleBarDrawn drawn{ .pressed = bar.pressedCrumb };
        if (bar.pressedButton) {
            drawn.pressedButton = PANEL_BUTTONS[*bar.pressedButton].button;
        }
        if (bar.pointedButton) {
            drawn.pointed = PointedRow{ .text = buttons[*bar.pointedButton].name, .help = PANEL_BUTTONS[*bar.pointedButton].help };
        }
        return drawn;
    }
}
