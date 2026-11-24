#pragma once

#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "ConfigSpecReport.h"
#include "model/Categories.h"
#include "model/ConfigDefinition.h"
#include "model/Store.h"

namespace mods_config::ui
{
    class FullPanel;

    /**
     * The mods the menu lists, in order of their display names, with their categories and the last read of each config
     * spec's live copy. It groups the mods again as they or their categories change, and gives the rows of the
     * Categories page of the menu's own config spec and their options. Private to the panel, which sets and replaces the mods, as that is
     * panel work too; the menu sets the categories and the live copy reads through `FullPanel::modList()`.
     */
    class ModList
    {
    public:
        void setOwnMod(std::string modName);
        void setCategoryTable(model::CategoryTable table);
        void setCategoryChoices(model::CategoryChoices choices);
        void setCategoriesMinMods(int minMods);
        std::vector<model::Option> categoryOptions(model::StoreReader& store) const;
        std::string categoryPageContent() const;
        void liveCopyNotRead(std::string_view folder, std::string_view liveCopy, std::string_view time, std::string_view reason);
        void setLiveCopyStale(std::string_view folder, bool stale);
        std::optional<std::size_t> placeOf(std::string_view folder) const;

        const std::vector<model::Mod>& mods() const
        {
            return _mods;
        }

        // the mods by their categories, which the tree is grouped by: empty for a flat tree
        const std::vector<model::CategoryGroup>& groups() const
        {
            return _groups;
        }

        // the last read of the live copy of the config spec of a folder, and none while it was not read
        const LiveCopyRead* liveCopyRead(const std::string_view folder) const
        {
            const auto read = _liveCopyReads.find(folder);
            return read != _liveCopyReads.end() ? &read->second : nullptr;
        }

    private:
        friend class FullPanel;

        void setMods(std::vector<model::Mod> mods);
        std::optional<std::vector<std::size_t>> replace(model::Mod mod, std::size_t dropped, std::string_view time);
        bool isOwnMod(const model::Mod& mod) const;
        void group();

        // a mod the panel's store session holds, which can change it
        model::Mod& mod(const std::size_t place)
        {
            return _mods[place];
        }

        // the mods the panel's search reads the game's texts into: their order stays the mod list's
        std::vector<model::Mod>& changeableMods()
        {
            return _mods;
        }

        // the report changes what it shows of a read, as the ray is on its row
        LiveCopyRead* changeableRead(const std::string_view folder)
        {
            const auto read = _liveCopyReads.find(folder);
            return read != _liveCopyReads.end() ? &read->second : nullptr;
        }

        std::vector<model::Mod> _mods;

        // the `modName` of the menu's own config spec, whose Categories page has a row for each mod: none in a test
        // that has no own mod
        std::string _ownMod;

        // the shipped table of the categories, the category the player gave a mod, which is before the mod's own and
        // the table's, and from how many mods the tree is grouped: none while not told, whose tree is flat
        model::CategoryTable _categoryTable;
        model::CategoryChoices _categoryChoices;
        std::optional<int> _categoriesMinMods;

        std::vector<model::CategoryGroup> _groups;

        // the last read of each config spec's live copy, by its folder
        std::map<std::string, LiveCopyRead, std::less<>> _liveCopyReads;
    };
}
