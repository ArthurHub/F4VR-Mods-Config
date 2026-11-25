#pragma once

#include <imgui.h>

#include "Drawing.h"
#include "imgui/controls/Sliders.h"
#include "model/ConfigDefinition.h"

// A setting's slider on the framework's: its range, its id, and its value's text.
namespace mods_config::ui
{
    controls::SliderRange sliderRange(const model::Setting& setting);
    ImGuiID sliderId(const model::Setting& setting);
    controls::ValueText valueBoxText(const model::Setting& setting, float value);
}
