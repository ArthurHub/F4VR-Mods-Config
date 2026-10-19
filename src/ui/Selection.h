#pragma once

#include <cstddef>
#include <optional>
#include <vector>

namespace mods_config::ui
{
    /**
     * What the Full panel shows: the menu's front page while no mod is selected, a mod's front page, or one of
     * the mod's pages. A mod and a page are named by their place in the lists the panel has.
     */
    struct Selection
    {
        std::optional<std::size_t> mod;

        // the page, as its place in the mod's pages, then in the pages of each page on the way to it: one place
        // for a page under the mod, two for a page under one of those. Empty for the mod's front page
        std::vector<std::size_t> page;

        bool operator==(const Selection&) const = default;
    };
}
