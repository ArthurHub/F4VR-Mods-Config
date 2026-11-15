#include "SearchSession.h"

#include <algorithm>
#include <unordered_set>
#include <utility>

namespace
{
    using namespace mods_config;

    /**
     * Whether a search shows a row of a list of items: a section it shows is one of them, with its rows.
     */
    bool hasFound(const std::vector<model::Item>& items, const std::unordered_set<const model::Item*>& found)
    {
        return std::ranges::any_of(items, [&found](const model::Item& item) {
            return found.contains(&item);
        });
    }
}

namespace mods_config::ui
{
    /**
     * Searches the mods for the search field's text, before the panel draws. Every mod is searched as the text changes,
     * as another mod is selected and after a mod was replaced; the selected mod in each update too, as its conditions
     * are asked of its store and its values change. Another mod is searched with no store, so its conditions hold. As a
     * search first starts after a reset, every available mod's store reads the texts its rows take from the game, which
     * only the selected mod has otherwise, so they are found by them too. A new text shows the page filtered again.
     * Returns whether the text changed: the panel scrolls the tree to its top and shows the page again.
     */
    bool SearchSession::update(const std::string_view text, std::vector<model::Mod>& mods, const std::optional<std::size_t> selectedMod, const StoreOf& storeOf)
    {
        const bool changed = text != _text;
        if (changed) {
            _text = text;
            _query = model::SearchQuery(text);
            _stale = true;
            _whole = false;
        }
        if (_query.empty()) {
            _found.clear();
            _matches = 0;
            return changed;
        }
        if (!_textsRead) {
            for (auto& mod : mods) {
                if (auto* const store = storeOf(mod); store && mod.notAvailable.empty()) {
                    store->readTexts(mod);
                }
            }
            _textsRead = true;
        }
        const auto storeIfSelected = [&](const std::size_t place) {
            return selectedMod == place ? storeOf(mods[place]) : nullptr;
        };
        if (_stale || _searchedMod != selectedMod) {
            _found.assign(mods.size(), std::nullopt);
            for (std::size_t place = 0; place < mods.size(); ++place) {
                _found[place] = model::searchMod(mods[place], _query, storeIfSelected(place));
            }
            _searchedMod = selectedMod;
            _stale = false;
        } else if (selectedMod) {
            _found[*selectedMod] = model::searchMod(mods[*selectedMod], _query, storeIfSelected(*selectedMod));
        }
        _matches = 0;
        for (const auto& found : _found) {
            _matches += found ? found->total : 0;
        }
        return changed;
    }

    /**
     * A mod was replaced, as its config spec was read again, and the mods sorted again: every mod is searched again, and
     * the row pointed while filtered, which was of the mod before, is forgotten.
     */
    void SearchSession::modReplaced()
    {
        _stale = true;
        _pointed = nullptr;
    }

    /**
     * Another page is shown, which is filtered while the query has words, until Back.
     */
    void SearchSession::pageShown()
    {
        _whole = false;
        _pointed = nullptr;
    }

    /**
     * The item the ray is on while the page is filtered, which keeps the focus ring as the page is shown whole.
     */
    void SearchSession::pointWhileFiltered(const model::Item* item)
    {
        if (item) {
            _pointed = item;
        }
    }

    /**
     * Shows the page whole, as Back does, until another page is shown or the query changes. Returns the last item the
     * ray was on while it was filtered, which the panel gives the focus ring, and none to show the page from its top.
     */
    const model::Item* SearchSession::showWhole()
    {
        _whole = true;
        return std::exchange(_pointed, nullptr);
    }

    /**
     * Forgets the search, as the menu opens and as the mods are set: no query, nothing found, and the texts from the
     * game are read again as the next search starts.
     */
    void SearchSession::reset()
    {
        *this = SearchSession();
    }

    /**
     * What the search found in a mod, by its place: none while it found nothing there or there is no search.
     */
    const model::ModFound* SearchSession::foundIn(const std::size_t mod) const
    {
        return mod < _found.size() && _found[mod] ? &*_found[mod] : nullptr;
    }

    /**
     * Whether the search shows a row of a page of a mod, by the mod's place.
     */
    bool SearchSession::showsRows(const std::size_t mod, const model::Page& page) const
    {
        const auto* found = foundIn(mod);
        return found && hasFound(page.items, found->rows);
    }

    /**
     * Whether rows of a page of the selected mod, by the mod's place, match the query though the values of other
     * settings hide them all: the search shows none of them, and searched with no store, whose conditions hold, it would.
     */
    bool SearchSession::hiddenByConditions(const std::size_t place, const model::Mod& mod, const model::Page& page) const
    {
        if (_query.empty() || showsRows(place, page)) {
            return false;
        }
        const auto found = model::searchMod(mod, _query, nullptr);
        return found && hasFound(page.items, found->rows);
    }
}
