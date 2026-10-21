#pragma once

#include <expected>
#include <filesystem>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

#include "model/ConfigDefinition.h"
#include "model/Translations.h"

namespace mods_config::mcm
{
    /**
     * Whether a game plugin, as `SS2.esm`, is loaded: the game's check, or a test's. With none every plugin is.
     */
    using PluginCheck = std::function<bool(std::string_view plugin)>;

    std::expected<model::Mod, std::string> loadConfig(std::string_view text, const model::Translations& translations = {}, const std::filesystem::path& interfaceFolder = {},
        const std::filesystem::path& configFolder = {}, const PluginCheck& isPluginLoaded = {});
    std::vector<model::Mod> loadConfigs(const std::filesystem::path& configFolder, const model::Translations& translations = {}, const std::filesystem::path& interfaceFolder = {},
        const PluginCheck& isPluginLoaded = {});
    std::vector<model::Mod> loadInstalledMods(const model::Translations& translations = {});
}
