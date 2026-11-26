#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "ConfigDefinition.h"
#include "Store.h"

namespace mods_config::model
{
    // a range of the bytes of a name: where a word of a query matched it, which the tree underlines
    using TextRange = f4cf::imgui::controls::TextRange;

    /**
     * What the player typed in the search field, as the words it is matched by: folded to lower case without
     * accents, and split at spaces and punctuation. A query with no word finds nothing.
     */
    class SearchQuery
    {
    public:
        SearchQuery() = default;
        explicit SearchQuery(std::string_view text);

        bool empty() const
        {
            return _words.empty();
        }

        const std::vector<std::u32string>& words() const
        {
            return _words;
        }

    private:
        std::vector<std::u32string> _words;
    };

    /**
     * A page the search shows in the tree, by its place among the pages it is under, with the pages under it that
     * it shows, in their order.
     */
    struct PageFound
    {
        std::size_t place = 0;

        // how many of its rows the search shows: the ones that match, or all of them for a page found by its name
        std::size_t count = 0;

        // a page that is disabled or not available is shown when its name matches, with no rows
        bool enabled = true;
        std::vector<PageFound> pages;
    };

    /**
     * A setting whose value hides a row the search shows, with the page it is on.
     */
    struct HidingSetting
    {
        const Setting* setting = nullptr;
        const Page* page = nullptr;
    };

    /**
     * A row the search shows that a condition hides now: the settings whose values hide it, each with a row the player
     * can show, and whether it is hidden while its one setting is off.
     */
    struct HiddenRow
    {
        std::vector<HidingSetting> by;
        bool whileOff = false;
    };

    /**
     * What the search found in a mod: how many rows of its front page it shows, and the pages it shows.
     */
    struct ModFound
    {
        std::size_t frontCount = 0;
        std::vector<PageFound> pages;

        // nothing under it matches, and its name does: every page of it is shown, and its row is closed until it is
        // selected
        bool byName = false;

        // how many rows of the mod the search shows, on every page
        std::size_t total = 0;

        // the items the search shows, of every page: its rows, the settings whose values show or hide them on their
        // page, and the sections they are in
        std::unordered_set<const Item*> rows;

        // the rows it shows that a condition hides now, which the page has dimmed, with what hides them
        std::unordered_map<const Item*, HiddenRow> hidden;
    };

    std::vector<TextRange> matchRanges(const SearchQuery& query, std::string_view name);
    std::optional<ModFound> searchMod(const Mod& mod, const SearchQuery& query, StoreReader* store);
}
