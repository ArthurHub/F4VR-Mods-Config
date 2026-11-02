#pragma once

#include <imgui.h>

#include "model/ConfigDefinition.h"

namespace mods_config::ui
{
    // the box of a button's icon, which the design system's ButtonRow draws in
    constexpr float BUTTON_ICON_SIZE = 20.0f;

    void drawButtonIcon(model::ButtonIcon icon, const ImVec2& center, ImU32 color);
}
