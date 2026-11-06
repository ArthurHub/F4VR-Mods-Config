#pragma once

#include <optional>
#include <vector>

#include "BindingEditor.h"
#include "Buttons.h"
#include "Choices.h"
#include "ColorEditor.h"
#include "HelpLine.h"
#include "PageImages.h"
#include "Sections.h"
#include "Sliders.h"
#include "TextInputs.h"
#include "TransformEditor.h"
#include "model/ConfigDefinition.h"
#include "model/Store.h"
#include "vrcf/VRControllersHaptic.h"

namespace mods_config::ui
{
    /**
     * What the drawing of a page tells.
     */
    struct PageDrawn
    {
        // the row the ray is on, for the help line: none in the gap between two rows, and on a row with no text.
        // The row of a dropdown while its list is open
        std::optional<PointedRow> pointed;

        // a press did something in this frame: it changed a value, took hold of a slider, opened the list of
        // a dropdown, started the edit of a text input, ran the action of a button, or folded or opened a
        // section. Or the thumbstick stepped a slider or a stepper
        bool pressActed = false;

        // the pattern a press on a haptic played, which is its feedback in place of the pulse
        std::optional<vrcf::HapticPattern> haptic;

        // the transform a press on Adjust in world tunes
        const model::Setting* adjust = nullptr;
    };

    /**
     * A page of a mod, in the content pane of the Full panel: its rows, in the order the mod gives them.
     * A section, a spacer, a text row and an image are drawn as they are, and a press on the heading of a
     * section that folds folds or opens it. A setting is drawn as its control,
     * with the value its store has for it, and a button as its text and a Run button, which runs its action in
     * the store, after its confirmation where it has one, and a text input as its text in a box, which a press edits. A hotkey is its text and the key the store has for it, which is only shown. Any other row is its text and what it is, until the commit that draws its control.
     */
    class ModPage
    {
    public:
        void setImageDrawer(ImageDrawer drawer);
        void setLeftHanded(bool leftHanded);
        PageDrawn draw(const model::Mod& mod, const model::Page& page, model::Store* store, float thumbstickSideValue);
        bool cancelDrag();
        bool closeList();
        bool cancelEdit();
        bool cancelBindingEdit();
        bool cancelColorEdit();
        bool cancelTransformEdit();
        void resumeTransformEdit();
        bool cancelConfirmation();
        void focus(const model::Item& item, const std::vector<const model::Group*>& sections);
        void reset();

        bool isFolded(const model::Group& group) const
        {
            return _sections.isFolded(group);
        }

        void setFolded(const model::Group& group, const bool folded)
        {
            _sections.setFolded(group, folded);
        }

    private:
        // with none an image from pixels keeps its room, and a texture is drawn as one the game does not load
        ImageDrawer _imageDrawer;

        // the player's handedness, which says what a button of a binding's hand is printed as
        bool _leftHanded = false;

        // the sliders of the page that is shown, and how the player edits one
        Sliders _sliders;

        // its steppers and dropdowns, and the list that is open
        Choices _choices;

        // its text inputs, and the one that is edited
        TextInputs _textInputs;

        // the editor of its bindings, and the one that is open
        BindingEditor _bindingEditor;

        // the editor of its colors, and the one that is open
        ColorEditor _colorEditor;

        // the editor of its transforms, and the one that is open
        TransformEditor _transformEditor;

        // its sections that fold, and whether each is folded
        Sections _sections;

        // its buttons, the confirmation one shows, and the results of their runs
        Buttons _buttons;

        // the item that has the focus ring, and none, and whether the page is scrolled to it as it is drawn next
        const model::Item* _focused = nullptr;
        bool _scrollToFocused = false;
    };
}
