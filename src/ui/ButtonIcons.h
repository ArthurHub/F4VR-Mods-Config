#pragma once

#include "imgui/controls/Icons.h"
#include "model/ConfigDefinition.h"

namespace mods_config::ui
{
    // the box of a button's icon, which the design system's ButtonRow draws in
    constexpr float BUTTON_ICON_SIZE = 20.0f;

    f4cf::imgui::controls::Icon buttonIcon(model::ButtonIcon icon);
}
