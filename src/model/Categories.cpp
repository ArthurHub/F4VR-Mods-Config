#include "Categories.h"

#include <algorithm>
#include <format>
#include <fstream>
#include <sstream>

#include <nlohmann/json.hpp>

#include "common/CommonUtils.h"

using namespace std::string_literals;

namespace
{
    using Json = nlohmann::json;

    /**
     * Whether two names are the same category: a category is known by its name without regard to case.
     */
    bool sameName(const std::string_view one, const std::string_view other)
    {
        return common::str_tolower(std::string(one)) == common::str_tolower(std::string(other));
    }
}

namespace mods_config::model
{
    /**
     * A name as the table spells it, for a shipped name written in another case: any other name as it is.
     */
    std::string CategoryTable::spelled(const std::string_view name) const
    {
        const auto shipped = std::ranges::find_if(names, [name](const std::string& known) {
            return sameName(known, name);
        });
        return shipped != names.end() ? *shipped : std::string(name);
    }

    /**
     * The name of a category as a config writes it, without the spaces around it. It is a plain name, so a `$`
     * token is no name: empty for one, and for no text.
     */
    std::string categoryName(const std::string_view text)
    {
        auto name = common::trim(std::string(text));
        return name.starts_with('$') ? std::string() : name;
    }

    /**
     * Reads the shipped table: `categories`, the shipped names, and `mods`, the category of a mod by its `modName`.
     * A name that is no text, a second spelling of a shipped name, and a mod that is listed again in another case
     * are left out, with a warning in the log. Comments are skipped, as in a config spec.
     * The error says why the table cannot be read.
     */
    std::expected<CategoryTable, std::string> readCategoryTable(const std::string_view text)
    {
        Json json;
        try {
            json = Json::parse(text, nullptr, true, true);
        } catch (const Json::parse_error& e) {
            return std::unexpected(std::format("Not valid JSON: {}", e.what()));
        }
        if (!json.is_object()) {
            return std::unexpected("Not a JSON object"s);
        }

        CategoryTable table;
        if (const auto names = json.find("categories"); names != json.end() && names->is_array()) {
            for (const auto& entry : *names) {
                const auto name = entry.is_string() ? categoryName(entry.get<std::string>()) : std::string();
                if (name.empty() || table.spelled(name) != name) {
                    logger::warn("Category table: the name {} is left out", entry.dump());
                    continue;
                }
                table.names.push_back(name);
            }
        }
        if (const auto mods = json.find("mods"); mods != json.end() && mods->is_object()) {
            for (const auto& [modName, entry] : mods->items()) {
                const auto name = entry.is_string() ? categoryName(entry.get<std::string>()) : std::string();
                if (name.empty() || !table.mods.try_emplace(common::str_tolower(modName), table.spelled(name)).second) {
                    logger::warn("Category table: the mod '{}' is left out", modName);
                }
            }
        }
        return table;
    }

    /**
     * Reads the shipped table from its file. A file that is not there, or cannot be read, is an empty table, with
     * the reason in the log: every mod then has its own category alone.
     */
    CategoryTable loadCategoryTable(const std::filesystem::path& file)
    {
        std::ifstream stream(file, std::ios::binary);
        if (!stream) {
            logger::warn("Category table '{}' is not found: no mod has a category from it", file.string());
            return {};
        }
        std::ostringstream text;
        text << stream.rdbuf();
        auto table = readCategoryTable(text.str());
        if (!table) {
            logger::error("Category table '{}' cannot be read: {}", file.string(), table.error());
            return {};
        }
        logger::info("Loaded category table from '{}': {} names, {} mods", file.string(), table->names.size(), table->mods.size());
        return std::move(*table);
    }

    /**
     * The category a mod has without the player's choice: the one of its own config, and else the table's for its
     * `modName`, without regard to case. A shipped name is spelled as the table spells it.
     */
    ModCategory ownCategory(const Mod& mod, const CategoryTable& table)
    {
        if (!mod.category.empty()) {
            return { .name = table.spelled(mod.category), .source = CategorySource::Mod };
        }
        if (const auto listed = table.mods.find(common::str_tolower(mod.name)); listed != table.mods.end()) {
            return { .name = listed->second, .source = CategorySource::Table };
        }
        return {};
    }
}
