#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include "HelpLine.h"
#include "Selection.h"
#include "model/Definition.h"

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
        // the selection a pressed breadcrumb goes to
        std::optional<Selection> pressed;

        // the Developer crumb of a package's report was pressed: it goes to the front page's Developer section
        bool developerPressed = false;

        // the button that was pressed, and the button the ray is on, for the help line
        TitleButton pressedButton = TitleButton::None;
        std::optional<PointedRow> pointed;
    };

    TitleBarDrawn drawTitleBar(const std::vector<model::Mod>& mods, const Selection& selection, bool helpShown, const model::Mod* report = nullptr);
}
