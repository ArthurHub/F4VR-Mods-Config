#pragma once

#include <cstddef>
#include <optional>

namespace mods_config::ui
{
    /**
     * What the Full panel shows: the menu's front page while no mod is selected, a mod's front page, or one of
     * the mod's pages. A mod and a page are named by their place in the lists the panel has.
     */
    struct Selection
    {
        std::optional<std::size_t> mod;

        // none for the mod's front page
        std::optional<std::size_t> page;

        bool operator==(const Selection&) const = default;
    };
}
