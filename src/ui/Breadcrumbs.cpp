#include "Breadcrumbs.h"

#include "common/CommonUtils.h"

namespace
{
    // the menu's name: the first breadcrumb of the title bar, which stands for the menu's front page
    constexpr auto TITLE = "MODS CONFIG FO4";

    // the breadcrumb of the Help page, after the menu's
    constexpr auto HELP_CRUMB = "Help";

    // the breadcrumb of the front page's Developer section, before a config spec's report
    constexpr auto DEVELOPER_CRUMB = "Developer";
}

namespace mods_config::ui
{
    /**
     * The breadcrumbs of the title bar, from the menu's to where the player is. The Help page has the menu's and its
     * own over the selection it keeps, and a config spec's report, shown over the menu's front page, the menu's, the
     * Developer section's and its own. A mod has its own and one for each page on the way to the selected page, by
     * its name without the spaces MCM's list sets a page in by. Each crumb before the last is a link to where it
     * stands for, and the last says while the page is filtered to its rows that match.
     */
    std::vector<Crumb> breadcrumbsOf(const std::vector<model::Mod>& mods, const Selection& selection, const bool helpShown, const model::Mod* report, const bool filtered)
    {
        std::vector<Crumb> crumbs;
        crumbs.push_back({ .text = TITLE, .look = CrumbLook::Link, .target = Selection{} });
        if (helpShown) {
            crumbs.push_back({ .text = HELP_CRUMB, .look = CrumbLook::Current });
        } else if (report) {
            crumbs.push_back({ .text = DEVELOPER_CRUMB, .look = CrumbLook::Link, .target = ToDeveloperSection{} });
            crumbs.push_back({ .text = report->displayName, .look = CrumbLook::Current });
        } else if (selection.mod) {
            crumbs.push_back({ .text = mods[*selection.mod].displayName, .look = CrumbLook::Link, .target = Selection{ .mod = selection.mod, .page = {} } });
            const auto* pages = &mods[*selection.mod].pages;
            for (std::size_t level = 0; level < selection.page.size(); ++level) {
                const auto& page = (*pages)[selection.page[level]];
                crumbs.push_back({ .text = common::trim(page.name),
                    .look = CrumbLook::Link,
                    .target = Selection{ .mod = selection.mod, .page = { selection.page.begin(), selection.page.begin() + static_cast<std::ptrdiff_t>(level + 1) } } });
                pages = &page.pages;
            }
            crumbs.back().filtered = filtered;
        }
        // the menu's alone is its front page, which is open
        crumbs.back().look = crumbs.size() == 1 ? CrumbLook::Selected : CrumbLook::Current;
        crumbs.back().target.reset();
        return crumbs;
    }
}
