#include "PageContentReader.h"

#include <algorithm>
#include <format>
#include <ranges>
#include <utility>

#include "ContentSpec.h"
#include "StorageSpec.h"
#include "common/CommonUtils.h"

namespace mods_config::spec
{
    /**
     * Drops the ids the content a mod's client last gave a page added, by the page's id in lower case, so the page's
     * content can be read again.
     */
    void PageContentReader::forget(const std::string& pageId)
    {
        const auto found = given.find(pageId);
        if (found == given.end()) {
            return;
        }
        for (const auto& id : found->second.settings) {
            settings.erase(id);
        }
        for (const auto& id : found->second.items) {
            itemIds.erase(id);
        }
        given.erase(found);
    }

    /**
     * Reads the content a mod's client gives a page, by the page's `id`, against its loaded config spec: `text` is the
     * page's `content` array in the config spec's format, read as the config spec's rows are. The rows inherit the
     * page's `defaultFile`, `applies` and `omitDefault`, join its conditions, can name any setting of the mod, and their
     * ids are unique across it, apart from those of the content the page was last given, which are dropped first. The
     * config spec's files are read as they are now. The error is why the page is not available: no page has the id,
     * or the text is not a JSON array.
     */
    std::expected<PageContent, std::string> readPageContent(PageContentReader& reader, const std::string_view pageId, const std::string_view text,
        const model::Translations& translations, const PluginChecks& plugins, const FileFolders& folders)
    {
        const auto id = common::str_tolower(std::string(pageId));
        const auto page = reader.pages.find(id);
        if (page == reader.pages.end()) {
            return std::unexpected(std::format("its config spec has no page with the id \"{}\"", pageId));
        }
        reader.forget(id);
        Json rows;
        try {
            rows = Json::parse(text, nullptr, true, true);
        } catch (const Json::parse_error&) {
            rows = nullptr;
        }
        if (!rows.is_array()) {
            return std::unexpected("its mod's client gave content that is not a JSON array"s);
        }

        ConfigSpecReading reading{ .translations = translations,
            .files = reader.files,
            .modName = reader.modName,
            .sharedLists = reader.sharedLists.is_object() ? &reader.sharedLists : nullptr,
            .sharedPresetLists = reader.sharedPresetLists.is_object() ? &reader.sharedPresetLists : nullptr,
            .plugins = plugins,
            .folders = folders,
            .fileValues = {},
            .settings = reader.settings,
            .problems = {},
            .at = { .path = {}, .pages = page->second.names, .row = {}, .pagePlaces = page->second.places, .itemPlaces = {} },
            .spheres = {},
            .itemIds = reader.itemIds };
        reading.fileValues = readFiles(reading.files, folders);
        auto items = loadRows(rows, "", page->second.inherited, reading);
        loadRowsConditions(rows, "", items, page->second.conditions, reading);
        loadRowsIds(rows, "", items, reading);

        auto& given = reader.given[id];
        for (const auto& key : reading.settings | std::views::keys) {
            if (!reader.settings.contains(key)) {
                given.settings.push_back(key);
            }
        }
        for (const auto& key : reading.itemIds) {
            if (!reader.itemIds.contains(key)) {
                given.items.push_back(key);
            }
        }
        reader.settings = std::move(reading.settings);
        reader.itemIds = std::move(reading.itemIds);

        // the conditions are read after every row, and their problems are put in their places, under the client's call
        std::ranges::stable_sort(reading.problems, isBefore);
        for (auto& problem : reading.problems) {
            problem.path = std::format("giveContent(\"{}\")", pageId) + problem.path;
        }
        return PageContent{ .items = std::move(items), .problems = std::move(reading.problems) };
    }
}
