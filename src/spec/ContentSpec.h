#pragma once

#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "ConfigSpecReading.h"
#include "PageContentReader.h"

// The rows of a `content`, of a page, a section or the front page, or given by a mod's client: their items, read
// first, and their conditions and ids, read once every setting is, as a condition can name a setting after it.
namespace mods_config::spec
{
    /**
     * What a row is shown and enabled by: its own conditions, and those of the sections it is in.
     */
    struct RowConditions
    {
        std::optional<model::Condition> visibleWhen;
        std::optional<model::Condition> enabledWhen;
    };

    std::vector<model::Item> loadItems(const Json& container, const Inherited& above, ConfigSpecReading& reading);
    std::vector<model::Item> loadRows(const Json& rows, std::string_view key, const Inherited& above, ConfigSpecReading& reading);
    void loadConditions(const Json& container, std::vector<model::Item>& items, const RowConditions& above, ConfigSpecReading& reading);
    void loadRowsConditions(const Json& rows, std::string_view key, std::vector<model::Item>& items, const RowConditions& above, ConfigSpecReading& reading);
    void loadItemIds(const Json& container, std::vector<model::Item>& items, ConfigSpecReading& reading);
    void loadRowsIds(const Json& rows, std::string_view key, std::vector<model::Item>& items, ConfigSpecReading& reading);
    void hideColorParts(std::vector<model::Item>& items, const std::set<std::string, std::less<>>& parts);

    /**
     * What the rows of a page's content get from the page, as a mod's client gives them: what they inherit, the
     * page's conditions they join, and where the page is, which their problems name.
     */
    struct ContentPage
    {
        Inherited inherited;
        RowConditions conditions;
        std::vector<std::string> names;
        std::vector<std::size_t> places;
    };

    /**
     * What a config spec keeps once it is loaded, to read a page's content its mod's client gives against it. The
     * ids, in lower case, are the config spec's and those of the content given so far, each page's apart, so a page
     * read again drops its own first.
     */
    class PageContentReader
    {
    public:
        std::string modName;
        std::map<std::string, model::File> files;

        // the `sharedLists` and the `sharedPresetLists` of the config spec's root: null for none
        Json sharedLists;
        Json sharedPresetLists;

        std::unordered_map<std::string, SettingInfo> settings;
        std::set<std::string, std::less<>> itemIds;

        // the pages with an `id`, by their id in lower case: the first of two with one id
        std::map<std::string, ContentPage> pages;

        /**
         * The ids the content a mod's client gave a page added, in lower case.
         */
        struct GivenIds
        {
            std::vector<std::string> settings;
            std::vector<std::string> items;
        };

        // by the page's id in lower case
        std::map<std::string, GivenIds> given;

        void forget(const std::string& pageId);
    };
}
