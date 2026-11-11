#pragma once

#include <cstdint>
#include <expected>
#include <filesystem>
#include <map>
#include <string>
#include <string_view>
#include <vector>

#include "ConfigDefinition.h"

// The categories a long list of mods is grouped by. See docs/design/schema.md, Category.

namespace mods_config::model
{
    class Store;

    // what the mods with no category are under, last
    constexpr auto NO_CATEGORY = "No category";

    // the page of the mod's own config spec that the menu fills with a row for each mod, the file of the config spec
    // the player's choices are kept in, and what the id of a mod's row starts with, before its `modName`
    constexpr auto CATEGORIES_PAGE_ID = "categories";
    constexpr auto CATEGORIES_FILE = "categories";
    constexpr auto CATEGORY_SETTING_ID = "category.";

    // the section of that page the rows of the mods are in
    constexpr auto CATEGORY_ROWS_ID = "categoryMods";

    // the most characters of a name the player types, which a row of the tree has room for
    constexpr int CATEGORY_NAME_MAX_LENGTH = 24;

    // where the shipped table is, from the game's folder
    constexpr auto CATEGORY_TABLE_FILE = R"(Data\ModsConfigFO4\Categories.json)";

    /**
     * The table the mod ships: the names of the shipped categories, and the category of each published mod that
     * its author gave none.
     */
    struct CategoryTable
    {
        // the shipped names, in the table's order, as it spells them
        std::vector<std::string> names;

        // the category of a mod, by its `modName` in lower case
        std::map<std::string, std::string> mods;

        std::string spelled(std::string_view name) const;
    };

    /**
     * What gave a mod its category.
     */
    enum class CategorySource : std::uint8_t
    {
        None,
        // the `category` of the mod's own config
        Mod,
        // the shipped table
        Table,
        // the player's choice, which can be no category
        Player,
    };

    // the category the player gave a mod, by the mod's `modName` in lower case: an empty name for no category
    using CategoryChoices = std::map<std::string, std::string>;

    /**
     * The category of a mod, and what gave it: an empty name, of no source, for a mod with none.
     */
    struct ModCategory
    {
        std::string name;
        CategorySource source = CategorySource::None;

        bool operator==(const ModCategory&) const = default;
    };

    /**
     * A category of a grouped list of mods: its name as it is shown, empty for the mods with none, and the places of
     * its mods in the list, in the list's order.
     */
    struct CategoryGroup
    {
        std::string name;
        std::vector<std::size_t> mods;

        bool operator==(const CategoryGroup&) const = default;
    };

    std::string categoryName(std::string_view text);
    std::string categoryKey(std::string_view name);
    std::expected<CategoryTable, std::string> readCategoryTable(std::string_view text);
    CategoryTable loadCategoryTable(const std::filesystem::path& file);
    std::expected<CategoryChoices, std::string> readCategoryChoices(std::string_view text);
    CategoryChoices loadCategoryChoices(const std::filesystem::path& file);
    ModCategory ownCategory(const Mod& mod, const CategoryTable& table);
    ModCategory categoryOf(const Mod& mod, const CategoryTable& table, const CategoryChoices& choices);
    bool isGrouped(std::size_t modCount, int minMods);
    std::vector<CategoryGroup> groupByCategory(const std::vector<Mod>& mods, const CategoryTable& table, const CategoryChoices& choices = {});
    std::vector<std::string> categoryNames(const std::vector<Mod>& mods, const CategoryTable& table, const CategoryChoices& choices);
    std::vector<Option> categoryOptions(const std::vector<Mod>& mods, const CategoryTable& table, const CategoryChoices& choices);
    bool addCategoryRows(Mod& own, const std::vector<Mod>& mods, const CategoryTable& table);
    CategoryChoices withPendingChoices(CategoryChoices choices, const Mod& own, Store& store);
}
