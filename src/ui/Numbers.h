#pragma once

#include <optional>
#include <string>

#include "model/Definition.h"

// How the number of a setting is written, in its control and in the help line, as its display says: the
// design system's NumberDisplay.

namespace mods_config::ui
{
    bool isPercent(const model::Setting& setting);
    std::string numberText(const model::Setting& setting, float value, bool withUnit = true);
    std::string percentDetail(const model::Setting& setting, float value);
    std::optional<float> fieldNumber(const std::string& text);
    std::string fieldText(const model::Setting& setting, const std::string& text);
}
