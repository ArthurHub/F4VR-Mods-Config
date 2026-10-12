#pragma once

#include <functional>
#include <optional>

#include <imgui.h>

#include "HelpLine.h"
#include "model/Definition.h"

namespace mods_config::ui
{
    /**
     * Draws the pixels of an image in a rectangle of the current ImGui window. The panel knows nothing of the
     * game, whose renderer an image is drawn by, so the menu gives the panel what draws one.
     */
    using ImageDrawer = std::function<void(const model::ImagePixels& pixels, const ImVec2& min, const ImVec2& max)>;

    /**
     * A page of a mod, in the content pane of the Full panel: its rows, in the order the mod gives them.
     * A section, a spacer, a text row and an image are drawn as they are. Any other row is its text and what
     * it is, until the commit that draws its control.
     */
    class ModPage
    {
    public:
        void setImageDrawer(ImageDrawer drawer);
        std::optional<PointedRow> draw(const model::Page& page) const;

    private:
        // none draws no image, and an image keeps its room
        ImageDrawer _imageDrawer;
    };
}
