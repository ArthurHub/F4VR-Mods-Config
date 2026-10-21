#pragma once

#include <cstddef>
#include <optional>

#include "HelpLine.h"
#include "model/Definition.h"

namespace mods_config::ui
{
    /**
     * What a frame of a package's report did: the row the ray is on, for the help line, the problem whose row
     * a press asks to show, by its place among the package's problems, and whether a press did something.
     */
    struct ReportDrawn
    {
        std::optional<PointedRow> pointed;
        std::optional<std::size_t> pressed;
        bool pressActed = false;
    };

    /**
     * A package's report, for its author, as the design system's FullPanelPackageReport has it: where its
     * config.json is read from, and its problems, its errors and then its warnings, each under a heading that
     * folds. A problem on a row that the menu shows has an arrow, and a press on it asks to show the row.
     * The headings stay as the author left them until the menu is closed.
     */
    class PackageReport
    {
    public:
        ReportDrawn draw(const model::Mod& mod);
        void reset();

    private:
        // whether the headings of the errors and of the warnings are folded
        bool _errorsFolded = false;
        bool _warningsFolded = false;
    };
}
