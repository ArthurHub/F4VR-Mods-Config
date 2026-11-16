#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include "Breadcrumbs.h"
#include "HelpLine.h"

namespace mods_config::ui
{
    // A button to the right of the title bar's breadcrumbs.
    enum class TitleButton : std::uint8_t
    {
        None,
        Help,
        Minimize,
        Close,
    };

    // The interactions reported by a frame of the title bar.
    struct TitleBarDrawn
    {
        // the place of the pressed breadcrumb among the crumbs, which only a link is
        std::optional<std::size_t> pressed;

        // the button that was pressed, and the button the ray is on, for the help line
        TitleButton pressedButton = TitleButton::None;
        std::optional<PointedRow> pointed;
    };

    TitleBarDrawn drawTitleBar(const std::vector<Crumb>& crumbs, bool highlightHelp);
}
