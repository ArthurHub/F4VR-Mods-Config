#pragma once

#include <vector>

#include "model/Definition.h"

namespace mods_config::ui
{
    /**
     * The tree of the mods, on the left of the Full panel: a plate that keeps its place and its width whatever
     * is shown beside it, with the number of the mods in its header and their rows under it.
     */
    class ModTree
    {
    public:
        void draw(const std::vector<model::Mod>& mods);
    };
}
