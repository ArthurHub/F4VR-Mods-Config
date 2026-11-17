#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "ConfigSpecReading.h"

// What a config spec needs before the rest of it is read: the schema version it is written in, and the game and
// F4SE plugins it requires, with the running game's checks of them.
namespace mods_config::spec
{
    bool isGamePluginLoaded(std::string_view plugin);
    bool isF4sePluginLoaded(std::string_view plugin);
    std::vector<std::string> requiredPlugins(const Json& configSpec, const char* key);
    std::string findMissingRequirements(const Json& configSpec, const PluginChecks& plugins);
    std::optional<std::int64_t> findSchemaVersion(const Json& configSpec);
}
