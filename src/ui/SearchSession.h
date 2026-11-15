#pragma once

#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "model/ConfigDefinition.h"
#include "model/Search.h"
#include "model/Store.h"

namespace mods_config::ui
{
    /**
     * The search of the panel's mods for the search field's query: the query, what it found in each mod, when that is
     * searched again, and whether the shown page has only its rows that match or is whole after Back. The panel says
     * what the field has, which mod is selected and what changed, and draws the tree and the page from what this found.
     */
    class SearchSession
    {
    public:
        using StoreOf = std::function<model::Store*(const model::Mod&)>;

        bool update(std::string_view text, std::vector<model::Mod>& mods, std::optional<std::size_t> selectedMod, const StoreOf& storeOf);
        void modReplaced();
        void pageShown();
        void pointWhileFiltered(const model::Item* item);
        const model::Item* showWhole();
        void reset();
        const model::ModFound* foundIn(std::size_t mod) const;
        bool showsRows(std::size_t mod, const model::Page& page) const;
        bool hiddenByConditions(std::size_t place, const model::Mod& mod, const model::Page& page) const;

        // the query as it was last searched, as it was typed
        const std::string& text() const
        {
            return _text;
        }

        // the words the query is matched by: empty for no search
        const model::SearchQuery& query() const
        {
            return _query;
        }

        // what the search found in each mod, by the mod's place, and none for a mod it did not find
        const std::vector<std::optional<model::ModFound>>& found() const
        {
            return _found;
        }

        // how many rows the search shows, of every mod
        std::size_t matches() const
        {
            return _matches;
        }

        // the page has only its rows that match: the query has words, and Back did not show the page whole. The panel
        // knows whether a page of a mod is shown at all
        bool filtered() const
        {
            return !_query.empty() && !_whole;
        }

    private:
        std::string _text;
        model::SearchQuery _query;
        std::vector<std::optional<model::ModFound>> _found;
        std::size_t _matches = 0;

        // the mod that was selected as every mod was searched, whose conditions were asked of its store
        std::optional<std::size_t> _searchedMod;

        // every mod is searched again in the next update, as a mod was replaced
        bool _stale = false;

        // the rows of the mods have the texts they take from the game, which are read once as a search starts
        bool _textsRead = false;

        // the page is whole after Back, until another page is shown or the query changes
        bool _whole = false;

        // the last item the ray was on while the page was filtered, which has the focus ring as the page is whole
        const model::Item* _pointed = nullptr;
    };
}
