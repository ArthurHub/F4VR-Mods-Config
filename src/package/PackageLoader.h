#pragma once

#include <expected>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include "model/Definition.h"
#include "model/Translations.h"

namespace mods_config::package
{
    std::expected<model::Mod, std::string> loadPackage(std::string_view text, const model::Translations& translations = {});
    std::vector<model::Mod> loadPackages(const std::filesystem::path& configFolder, const model::Translations& translations = {});
    std::vector<model::Mod> loadInstalledPackages(const model::Translations& translations = {});
}
