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
    };

    /**
     * The category of a mod, and what gave it: an empty name, of no source, for a mod with none.
     */
    struct ModCategory
    {
        std::string name;
        CategorySource source = CategorySource::None;

        bool operator==(const ModCategory&) const = default;
    };

    std::string categoryName(std::string_view text);
    std::expected<CategoryTable, std::string> readCategoryTable(std::string_view text);
    CategoryTable loadCategoryTable(const std::filesystem::path& file);
    ModCategory ownCategory(const Mod& mod, const CategoryTable& table);
}
