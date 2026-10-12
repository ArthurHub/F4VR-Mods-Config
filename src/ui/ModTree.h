#pragma once

#include <optional>
#include <vector>

#include "Selection.h"
#include "model/Definition.h"

namespace mods_config::ui
{
    /**
     * The tree of the mods, on the left of the Full panel: a plate that keeps its place and its width whatever
     * is shown beside it, with the number of the mods in its header and their rows under it. The selected mod
     * is open, with a row for each of its pages.
     */
    class ModTree
    {
    public:
        std::optional<Selection> draw(const std::vector<model::Mod>& mods, const Selection& selection);
        void showSelection();
        void showTop();

    private:
        // what the rows scroll to when they are drawn next: the selected row, or their top
        bool _showSelection = false;
        bool _showTop = false;
    };
}
