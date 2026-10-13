#pragma once

#include <functional>
#include <optional>

#include <imgui.h>

#include "Choices.h"
#include "HelpLine.h"
#include "Sliders.h"
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
        // the row the ray is on, for the help line: none in the gap between two rows, and on a row with no text.
        // The row of a dropdown while its list is open
        std::optional<PointedRow> pointed;

        // a press did something in this frame: it changed a value, took hold of a slider, or opened the list
        // of a dropdown. Or the thumbstick stepped a slider or a stepper
        bool pressActed = false;
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
        PageDrawn draw(const model::Page& page, model::Store* store, float thumbstickSideValue);
        bool cancelDrag();
        bool closeList();
        void reset();

    private:
        // none draws no image, and an image keeps its room
        ImageDrawer _imageDrawer;

        // the sliders of the page that is shown, and how the player edits one
        Sliders _sliders;

        // its steppers and dropdowns, and the list that is open
        Choices _choices;
    };
}
