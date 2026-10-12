#pragma once

#include "model/Definition.h"

namespace mods_config::ui
{
    /**
     * A page of a mod, in the content pane of the Full panel: its rows, in the order the mod gives them.
     * A row is its text and what it is, until the commit that draws its control.
     */
    class ModPage
    {
    public:
        void draw(const model::Page& page) const;
    };
}
