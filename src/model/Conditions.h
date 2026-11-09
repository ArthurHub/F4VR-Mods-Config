#pragma once

#include <optional>
#include <string_view>

#include "ConfigDefinition.h"
#include "Store.h"

namespace mods_config::model
{
    bool holds(const Condition& condition, Store* store);
    bool surelyHolds(const Condition& condition, Store* store);
    std::optional<StoredType> gameIniSettingType(std::string_view name);
}
