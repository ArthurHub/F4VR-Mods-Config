#pragma once

#include <functional>
#include <optional>
#include <string>

#include <imgui.h>

#include "Choices.h"
#include "HelpLine.h"
#include "Sliders.h"
#include "TextInputs.h"
#include "model/Definition.h"
#include "model/Store.h"

namespace mods_config::ui
{
    /**
     * What draws the images of a page. The panel knows nothing of the game, whose renderer an image is drawn
     * by and which loads a package's texture, so the menu gives the panel what draws one.
     */
    struct ImageDrawer
    {
        // the size of a package's texture, by its path under `Data/Textures`, in its pixels. None for one the
        // game does not load, as one that is not there
        std::function<std::optional<ImVec2>(const std::string& texture)> textureSize;

        // draws an image in a rectangle of the current ImGui window: its pixels, or else its texture
        std::function<void(const model::Image& image, const ImVec2& min, const ImVec2& max)> draw;
    };

    /**
     * What the drawing of a page tells.
     */
    struct PageDrawn
    {
        // the row the ray is on, for the help line: none in the gap between two rows, and on a row with no text.
        // The row of a dropdown while its list is open
        std::optional<PointedRow> pointed;

        // a press did something in this frame: it changed a value, took hold of a slider, opened the list of
        // a dropdown, started the edit of a text input, or ran the action of a button. Or the thumbstick stepped a slider or a stepper
        bool pressActed = false;
    };

    /**
     * A page of a mod, in the content pane of the Full panel: its rows, in the order the mod gives them.
     * A section, a spacer, a text row and an image are drawn as they are. A setting is drawn as its control,
     * with the value its store has for it, and a button as its text and a Run button, which runs its action in
     * the store, and a text input as its text in a box, which a press edits. A hotkey is its text and the key the store has for it, which is only shown. Any other row is its text and what it is, until the commit that draws its control.
     */
    class ModPage
    {
    public:
        void setImageDrawer(ImageDrawer drawer);
        PageDrawn draw(const model::Page& page, model::Store* store, float thumbstickSideValue);
        bool cancelDrag();
        bool closeList();
        bool cancelEdit();
        void reset();

    private:
        // with none an image from pixels keeps its room, and a texture is drawn as one the game does not load
        ImageDrawer _imageDrawer;

        // the sliders of the page that is shown, and how the player edits one
        Sliders _sliders;

        // its steppers and dropdowns, and the list that is open
        Choices _choices;

        // its text inputs, and the one that is edited
        TextInputs _textInputs;
    };
}
