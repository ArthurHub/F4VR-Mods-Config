#pragma once

#include <expected>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include "model/Definition.h"

namespace mods_config::mcm
{
    std::expected<model::Mod, std::string> loadConfig(std::string_view text);
    std::vector<model::Mod> loadConfigs(const std::filesystem::path& configFolder);
    std::vector<model::Mod> loadInstalledMods();
}
