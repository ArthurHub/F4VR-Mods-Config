#pragma once

#include <expected>
#include <filesystem>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

#include "model/ConfigDefinition.h"
#include "model/FileChoices.h"
#include "model/Translations.h"

namespace mods_config::spec
{
    // the version of the config spec schema the menu reads: a config spec whose `minSchemaVersion` is above it is not
    // available. It only grows, as a version adds fields and control types
    constexpr int SCHEMA_VERSION = 1;

    // where the config specs are, from the game's folder: a folder each, with its `spec.json`
    constexpr auto CONFIG_SPEC_FOLDER = R"(Data\ModsConfigFO4\Config)";

    /**
     * Whether a plugin is loaded, by its name: the game's check, or a test's.
     */
    using PluginCheck = std::function<bool(std::string_view name)>;

    /**
     * What a config spec's requirements and its conditions on a loaded plugin are answered with. With no check every
     * plugin is loaded.
     */
    struct PluginChecks
    {
        // a game plugin, by its file name, as `MyMod.esp`
        PluginCheck isGamePluginLoaded;

        // an F4SE plugin, by the name it registers with F4SE, as `F4VRBody`
        PluginCheck isF4sePluginLoaded;
    };

    // the folders a config spec's files and its choices of files are read from as it is read. With none, as in a test of
    // a config spec alone, a setting's path a file does not have, and a folder with no files, are not reported
    using FileFolders = model::FileFolders;

    std::expected<model::Mod, std::string> loadConfigSpec(std::string_view text, const model::Translations& translations = {}, const PluginChecks& plugins = {},
        const FileFolders& folders = {});
    std::expected<model::Mod, std::string> loadConfigSpecFile(const std::filesystem::path& file, const model::Translations& translations = {}, const PluginChecks& plugins = {},
        const FileFolders& folders = {});
    model::Mod notLoadedConfigSpec(const std::string& folder, const std::string& reason);
    void logConfigSpec(const model::Mod& mod);
    PluginChecks gamePluginChecks();
    FileFolders gameFileFolders();
    std::vector<model::Mod> loadConfigSpecs(const std::filesystem::path& configFolder, const model::Translations& translations = {}, const PluginChecks& plugins = {},
        const FileFolders& folders = {});
    std::vector<model::Mod> loadInstalledConfigSpecs(const model::Translations& translations = {});
    std::vector<model::Mod> withConfigSpecs(std::vector<model::Mod> mcmMods, std::vector<model::Mod> configSpecs);
}
