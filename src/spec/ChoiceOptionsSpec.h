#pragma once

#include <optional>
#include <string>

#include "ConfigSpecReading.h"

// The options of a config spec's choices, a stepper's and a dropdown's: their own, a shared list's, the game's, a
// folder's files, or their mod's client's, and the config spec's shared lists of them.
namespace mods_config::spec
{
    std::optional<model::Value> getValue(const Json& value, const char* key, model::StoredType type);
    void loadChoiceOptions(model::SettingType type, model::StoredType valueType, const Json* options, model::ValueOptions& valueOptions, ConfigSpecReading& reading);
    bool isFolderDefault(const std::string& value, const model::FolderOptions& folder, ConfigSpecReading& reading);
    void checkSharedLists(ConfigSpecReading& reading);
}
