#pragma once

#include <expected>
#include <string>
#include <string_view>
#include <vector>

#include "ConfigSpecLoader.h"
#include "model/ConfigDefinition.h"
#include "model/Translations.h"

// The content of a config spec's page that its mod's client gives, read against the loaded config spec. The reader a
// config spec keeps is `model::Mod::pageContent`; its insides are the loader's own.
namespace mods_config::spec
{
    class PageContentReader;

    /**
     * The rows a mod's client gave a page, and what is wrong in them, each with its path from `giveContent("<id>")`.
     */
    struct PageContent
    {
        std::vector<model::Item> items;
        std::vector<model::Problem> problems;
    };

    std::expected<PageContent, std::string> readPageContent(PageContentReader& reader, std::string_view pageId, std::string_view text, const model::Translations& translations = {},
        const PluginChecks& plugins = {}, const FileFolders& folders = {});
}
