#pragma once

#include <string>

#include <imgui.h>
#include <imgui_internal.h>

#include "Drawing.h"
#include "model/Colors.h"
#include "model/ConfigDefinition.h"

// A config spec's color, as the design system's Color has it: a frame with the color as a swatch and as its numbers,
// which a press opens the editor of.

namespace mods_config::ui
{
    ImU32 colorU32(const render::Color& color, bool opaque = false);
    void drawCheckerboard(const ImRect& box);
    void drawSwatch(const ImRect& box, const render::Color& color, bool alpha);
    std::string colorValueName(const model::Setting& setting, const model::Value& value);
    float drawColor(const model::Setting& setting, float right, float top, const std::string* value, ValueLook look, bool open);
}
