#pragma once

#include "ConfigSpecReading.h"

// The parts of a config spec's settings of the framework's types (`cf.inputBinding`, `cf.haptic`, `cf.color`,
// `cf.transform`) that only they have: their value options, and the config spec's shared lists of color presets.
namespace mods_config::spec
{
    void loadFrameworkValueOptions(model::SettingType type, const Json* options, model::ValueOptions& valueOptions, ConfigSpecReading& reading);
    void checkSharedPresetLists(ConfigSpecReading& reading);
}
