#pragma once

#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "Selection.h"
#include "imgui/controls/TitleBars.h"
#include "model/ConfigDefinition.h"

namespace mods_config::ui
{
    /**
     * Where a press on the Developer crumb over a config spec's report goes: the menu's front page, scrolled to its
     * Developer section.
     */
    struct ToDeveloperSection
    {
        bool operator==(const ToDeveloperSection&) const = default;
    };

    // where a press on a crumb goes
    using CrumbTarget = std::variant<Selection, ToDeveloperSection>;

    // how a breadcrumb is drawn, and whether it is pressed, as the framework's title bar has it: the menu's crumb is
    // Selected while the menu's front page is open
    using CrumbLook = f4cf::imgui::controls::CrumbLook;

    /**
     * A breadcrumb of the title bar: its text, its look, and where a press goes, which only a link has.
     */
    struct Crumb
    {
        std::string text;
        CrumbLook look = CrumbLook::Link;
        std::optional<CrumbTarget> target;

        // the page has only its rows that match the search, which the last crumb says after its name
        bool filtered = false;
    };

    std::vector<Crumb> breadcrumbsOf(const std::vector<model::Mod>& mods, const Selection& selection, bool helpShown, const model::Mod* report, bool filtered);
}
