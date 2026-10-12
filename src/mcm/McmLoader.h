#pragma once

#include <expected>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include "model/Definition.h"
#include "model/Translations.h"

namespace mods_config::mcm
{
    std::expected<model::Mod, std::string> loadConfig(std::string_view text, const model::Translations& translations = {}, const std::filesystem::path& interfaceFolder = {},
        const std::filesystem::path& configFolder = {});
    std::vector<model::Mod> loadConfigs(const std::filesystem::path& configFolder, const model::Translations& translations = {}, const std::filesystem::path& interfaceFolder = {});
    std::vector<model::Mod> loadInstalledMods(const model::Translations& translations = {});
}
