#pragma once

#include <optional>
#include <vector>

#include "HelpLine.h"
#include "Selection.h"
#include "Theme.h"
#include "model/Definition.h"

namespace mods_config::ui
{
    /**
     * What the drawing of the tree tells.
     */
    struct TreeDrawn
    {
        // what a pressed row selects
        std::optional<Selection> pressed;

        // the row the ray is on, for the help line: a mod that is not available, which says why. None on any
        // other row, which has no help
        std::optional<PointedRow> pointed;
    };

    /**
     * The tree of the mods, on the left of the Full panel: a plate that keeps its place and its width whatever
     * is shown beside it, with the number of the mods in its header and their rows under it. The selected mod
     * is open, with a row for each of its pages.
     */
    class ModTree
    {
    public:
        TreeDrawn draw(const std::vector<model::Mod>& mods, const Selection& selection, float height);
        void showSelection();
        void showTop();
        void setWidth(float width);

    private:
        // the plate's width, in the panel's pixels
        float _width = theme::NAV_WIDTH;

        // what the rows scroll to when they are drawn next: the selected row, or their top
        bool _showSelection = false;
        bool _showTop = false;
    };
}
