#pragma once

#include <optional>
#include <string_view>

#include "ConfigSpecReading.h"

// A config spec's setting rows: their type, value type, value options, default, display, storage and action, and
// the sets of their options, which are read once every setting is.
namespace mods_config::spec
{
    std::optional<model::SettingType> findSettingType(std::string_view type);
    model::Setting loadSetting(const Json& row, model::SettingType type, const Inherited& above, ConfigSpecReading& reading);
    void loadOptionSets(const Json& row, model::Setting& setting, ConfigSpecReading& reading);
    void checkSharedLists(ConfigSpecReading& reading);
}
