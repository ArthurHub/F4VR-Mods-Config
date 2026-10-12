#pragma once

#include "model/Definition.h"

namespace mods_config::ui
{
    /**
     * A page of a mod, in the content pane of the Full panel: its rows, in the order the mod gives them.
     * A section, a spacer and a text row are drawn as they are. Any other row is its text and what it is, until
     * the commit that draws its control.
     */
    class ModPage
    {
    public:
        void draw(const model::Page& page) const;
    };
}
