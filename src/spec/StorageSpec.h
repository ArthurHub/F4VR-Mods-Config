#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "ConfigSpecReading.h"

// Where a config spec's values are kept: its `files`, the values of those that are there, and the `storage` of a
// setting, a file's value, a global, a property of a script or a state of its mod's client, which its id comes from.
namespace mods_config::spec
{
    std::map<std::string, model::File> loadFiles(const Json& configSpec, ConfigSpecReading& reading);
    std::map<std::string, std::unique_ptr<ModFile>> readFiles(const std::map<std::string, model::File>& files, const FileFolders& folders);
    std::string pathProblem(const std::string& path);
    std::optional<std::pair<std::string, std::uint32_t>> parseForm(const std::string& text);
    model::Storage loadStorage(const Json& row, const std::string& defaultFile, model::StoredType valueType, ConfigSpecReading& reading);
    std::string idOf(const model::Storage& storage);
}
