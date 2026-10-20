#pragma once

#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "HelpLine.h"

namespace mods_config::ui
{
    // A quick launch entry that opens a mod's own config UI.
    struct QuickLaunchEntry
    {
        std::string name;

        // what the help line says while the ray is on the entry
        std::string help;
        std::function<void()> open;
    };

    // A mod's notice data, read from the model and its store by the panel.
    struct FrontPageMod
    {
        std::string_view displayName;
        std::size_t savedRestartCount = 0;
        std::string_view notAvailable;
    };

    // The quick launch interactions reported by a frame of the front page.
    struct FrontPageDrawn
    {
        std::optional<std::size_t> pressed;
        std::optional<PointedRow> pointed;
    };

    FrontPageDrawn drawFrontPage(const std::vector<QuickLaunchEntry>& entries, const std::vector<FrontPageMod>& mods);
}
