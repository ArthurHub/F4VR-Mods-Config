#pragma once

#include <functional>
#include <optional>

#include <imgui.h>

#include "HelpLine.h"
#include "model/Definition.h"
#include "model/Store.h"

namespace mods_config::ui
{
    /**
     * Draws the pixels of an image in a rectangle of the current ImGui window. The panel knows nothing of the
     * game, whose renderer an image is drawn by, so the menu gives the panel what draws one.
     */
    using ImageDrawer = std::function<void(const model::ImagePixels& pixels, const ImVec2& min, const ImVec2& max)>;

    /**
     * What the drawing of a page tells.
     */
    struct PageDrawn
    {
        // the row the ray is on, for the help line: none in the gap between two rows, and on a row with no text
        std::optional<PointedRow> pointed;

        // a press changed a value in this frame
        bool changed = false;
    };

    /**
     * A page of a mod, in the content pane of the Full panel: its rows, in the order the mod gives them.
     * A section, a spacer, a text row and an image are drawn as they are. A setting is drawn as its control,
     * with the value its store has for it. Any other row is its text and what it is, until the commit that
     * draws its control.
     */
    class ModPage
    {
    public:
        void setImageDrawer(ImageDrawer drawer);
        PageDrawn draw(const model::Page& page, model::Store* store) const;

    private:
        // none draws no image, and an image keeps its room
        ImageDrawer _imageDrawer;
    };
}
