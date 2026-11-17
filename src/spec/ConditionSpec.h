#pragma once

#include <optional>
#include <vector>

#include "ConfigSpecReading.h"

// The conditions of a config spec's rows (`visibleWhen`, `enabledWhen`) and the warnings of its settings and buttons,
// read once every setting is, as a condition can name a setting after it.
namespace mods_config::spec
{
    std::optional<model::Condition> loadRowCondition(const Json& row, const char* key, ConfigSpecReading& reading);
    std::vector<model::Warning> loadWarnings(const Json& row, ConfigSpecReading& reading);
    std::optional<model::Condition> joinConditions(const std::optional<model::Condition>& above, std::optional<model::Condition> own);
}
