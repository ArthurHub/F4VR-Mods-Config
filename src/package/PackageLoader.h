#pragma once

#include <expected>
#include <filesystem>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

#include "model/Definition.h"
#include "model/Translations.h"

namespace mods_config::package
{
    // the version of the package schema the menu reads: a package whose `minSchemaVersion` is above it is not
    // available. It only grows, as a version adds fields and control types
    constexpr int SCHEMA_VERSION = 1;

    /**
     * Whether a plugin is loaded, by its name: the game's check, or a test's.
     */
    using PluginCheck = std::function<bool(std::string_view name)>;

    /**
     * What a package's requirements and its conditions on a loaded plugin are answered with. With no check every
     * plugin is loaded.
     */
    struct PluginChecks
    {
        // a game plugin, by its file name, as `MyMod.esp`
        PluginCheck isGamePluginLoaded;

        // an F4SE plugin, by the name it registers with F4SE, as `F4VRBody`
        PluginCheck isF4sePluginLoaded;
    };

    std::expected<model::Mod, std::string> loadPackage(std::string_view text, const model::Translations& translations = {}, const PluginChecks& plugins = {});
    std::vector<model::Mod> loadPackages(const std::filesystem::path& configFolder, const model::Translations& translations = {}, const PluginChecks& plugins = {});
    std::vector<model::Mod> loadInstalledPackages(const model::Translations& translations = {});
}
