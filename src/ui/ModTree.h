#pragma once

#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

#include "HelpLine.h"
#include "SearchField.h"
#include "Selection.h"
#include "Theme.h"
#include "model/Categories.h"
#include "model/ConfigDefinition.h"
#include "model/Search.h"
#include "model/Store.h"

namespace mods_config::ui
{
    /**
     * What the drawing of the tree tells.
     */
    struct TreeDrawn
    {
        // what a pressed row selects
        std::optional<Selection> pressed;

        // the row the ray is on, for the help line: a mod that is not available, which says why, or a mod or a
        // page with a help, or the search field and its Clear. None on any other row, which has no help
        std::optional<PointedRow> pointed;

        // a press on the search field did something: it started the edit, or Clear emptied the query
        bool searchActed = false;

        // the ray is on the search field or its Clear
        bool searchPointed = false;
    };

    /**
     * What a search found, which the tree shows in place of every mod: the query, what it found in each mod, by the
     * mod's place, none for a mod in which nothing matches, and how many rows it found in all.
     */
    struct TreeSearch
    {
        const model::SearchQuery* query = nullptr;
        const std::vector<std::optional<model::ModFound>>* found = nullptr;
        std::size_t matches = 0;
    };

    /**
     * The tree of the mods, on the left of the Full panel: a plate that keeps its place and its width whatever
     * is shown beside it, with the search field in its header and the rows of the mods under it. The selected mod
     * is open, with a row for each of its pages, and so is a page of it with pages, while it or one of them is
     * selected, a level further in. While the field has a query, the rows are the mods and pages it found.
     * A grouped tree has its categories as its top level, each a row that a press opens and closes, with its mods
     * under it, a level further in. The categories the player opened stay open while the game runs.
     */
    class ModTree
    {
    public:
        TreeDrawn draw(const std::vector<model::Mod>& mods, const Selection& selection, float height, model::Store* store, const TreeSearch* search,
            const std::vector<model::CategoryGroup>* categories = nullptr);
        void showSelection();
        void showTop();
        void setWidth(float width);

        // the search field's query, as it is typed: empty for none
        const std::string& query() const
        {
            return _search.query();
        }

        // ends the edit of the search field, for Back, with the query kept: false while it is not edited
        bool endSearchEdit()
        {
            return _search.endEdit();
        }

        // empties the search field's query, as the menu opens
        void clearSearch()
        {
            _search.clear();
        }

    private:
        void drawCategories(const std::vector<model::Mod>& mods, const std::vector<model::CategoryGroup>& categories, const Selection& selection, float rowWidth, TreeDrawn& drawn,
            model::Store* store);
        float drawMod(const std::vector<model::Mod>& mods, std::size_t modIndex, int level, const Selection& selection, float rowWidth, TreeDrawn& drawn,
            model::Store* store) const;
        void drawPages(const std::vector<model::Page>& pages, std::size_t modIndex, int level, const Selection& selection, std::vector<std::size_t>& path, float rowWidth,
            TreeDrawn& drawn, model::Store* store) const;
        void drawFound(const std::vector<model::Mod>& mods, const Selection& selection, const TreeSearch& search, float rowWidth, TreeDrawn& drawn) const;
        void drawFoundPages(const std::vector<model::Page>& pages, const std::vector<model::PageFound>& found, std::size_t modIndex, const Selection& selection,
            const model::SearchQuery& query, std::vector<std::size_t>& path, float rowWidth, TreeDrawn& drawn) const;

        // the header: the search field
        SearchField _search;

        // the plate's width, in the panel's pixels
        float _width = theme::NAV_WIDTH;

        // what the rows scroll to when they are drawn next: the selected row, or their top
        bool _showSelection = false;
        bool _showTop = false;

        // the categories that are open, by their names in lower case, the mods with none by an empty one
        std::unordered_set<std::string> _openCategories;

        // the category a press opened, whose first mods are scrolled to when they are drawn next: none for none
        std::optional<std::string> _openedCategory;
    };
}
