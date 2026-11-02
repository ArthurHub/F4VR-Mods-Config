#pragma once

#include <string>

#include "Drawing.h"
#include "model/ConfigDefinition.h"

// A config spec's transform, as the design system's Transform has it: a frame with its position and its scale on one
// line and its rotation on the next.

namespace mods_config::ui
{
    std::string transformValueName(const model::Setting& setting, const model::Value& value);
    float drawTransform(const model::Setting& setting, float right, float top, const std::string* value, ValueLook look, bool open);
}
