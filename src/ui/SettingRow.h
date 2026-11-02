#pragma once

#include <optional>
#include <span>

#include "BindingEditor.h"
#include "Choices.h"
#include "ColorEditor.h"
#include "HelpLine.h"
#include "PageImages.h"
#include "PageRows.h"
#include "Sliders.h"
#include "TextInputs.h"
#include "TransformEditor.h"
#include "model/ConfigDefinition.h"
#include "model/Store.h"
#include "vrcf/VRControllersHaptic.h"

namespace mods_config::ui
{
    // The value store and control editors used by a setting row, owned by the menu and page.
    struct SettingRowContext
    {
        model::Store* store = nullptr;
        Sliders& sliders;
        Choices& choices;
        TextInputs& textInputs;
        BindingEditor& bindingEditor;
        ColorEditor& colorEditor;
        TransformEditor& transformEditor;

        // what a binding's icons are drawn with, and the player's handedness, which says what a button of a hand is
        // printed as
        const ImageDrawer& imageDrawer;
        bool leftHanded = false;

        // the binding settings of the mod, which a binding that is the same press as one of them names
        std::span<const model::Setting* const> bindings;
    };

    // Hover help and whether an input acted while drawing and editing a setting row.
    struct SettingRowDrawn
    {
        std::optional<PointedRow> pointed;
        bool pressActed = false;

        // the pattern a press played: a haptic's Play, or an option picked in its list. It is the press's feedback, in
        // place of the menu's pulse
        std::optional<vrcf::HapticPattern> haptic;
    };

    SettingRowDrawn drawSettingRow(const model::Setting& setting, const SettingRowContext& context);
}
