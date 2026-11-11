#include "Categories.h"

#include <algorithm>
#include <format>
#include <fstream>
#include <ranges>
#include <sstream>

#include <nlohmann/json.hpp>

#include "common/CommonUtils.h"
#include "common/TomlEditor.h"

using namespace std::string_literals;

namespace
{
    using Json = nlohmann::json;

    /**
     * Whether two names are the same category.
     */
    bool sameName(const std::string_view one, const std::string_view other)
    {
        return mods_config::model::categoryKey(one) == mods_config::model::categoryKey(other);
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
     * What a category is known by: its name in lower case, as its name is without regard to case.
     */
    std::string categoryKey(const std::string_view name)
    {
        return common::str_tolower(std::string(name));
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

    /**
     * Reads the player's choices: a TOML text with a key for each mod the player gave a category, named by the mod's
     * `modName`, whose value is the category's name, and an empty text for no category. A value that is no text is
     * left out. The error says why the text cannot be read.
     */
    std::expected<CategoryChoices, std::string> readCategoryChoices(const std::string_view text)
    {
        const auto table = common::parseToml(text);
        if (!table) {
            return std::unexpected(table.error());
        }
        CategoryChoices choices;
        for (const auto& [modName, value] : *table) {
            if (const auto* name = value.as_string()) {
                choices.try_emplace(common::str_tolower(std::string(modName.str())), categoryName(name->get()));
            }
        }
        return choices;
    }

    /**
     * Reads the player's choices from their file. A file that is not there, as before the player's first choice, or
     * that cannot be read, has none, with the reason of one that cannot in the log.
     */
    CategoryChoices loadCategoryChoices(const std::filesystem::path& file)
    {
        std::ifstream stream(file, std::ios::binary);
        if (!stream) {
            return {};
        }
        std::ostringstream text;
        text << stream.rdbuf();
        auto choices = readCategoryChoices(text.str());
        if (!choices) {
            logger::error("The categories of '{}' cannot be read: {}", file.string(), choices.error());
            return {};
        }
        return std::move(*choices);
    }

    /**
     * The category of a mod: the player's choice for its `modName`, without regard to case, which can be no category,
     * and else the one it has without it (`ownCategory`). A shipped name is spelled as the table spells it.
     */
    ModCategory categoryOf(const Mod& mod, const CategoryTable& table, const CategoryChoices& choices)
    {
        if (const auto chosen = choices.find(common::str_tolower(mod.name)); chosen != choices.end()) {
            return { .name = table.spelled(chosen->second), .source = CategorySource::Player };
        }
        return ownCategory(mod, table);
    }

    /**
     * Whether a list of so many mods is grouped by category: one of at least `minMods` mods, and any list with 0.
     */
    bool isGrouped(const std::size_t modCount, const int minMods)
    {
        return minMods <= 0 || modCount >= static_cast<std::size_t>(minMods);
    }

    /**
     * The mods of a list by their categories: the categories in the order of their names, without regard to case,
     * and the mods with none last, each with its mods in the list's order. A category is known by its name without
     * regard to case, and is spelled as the first of its mods writes it, a shipped name as the table spells it.
     * A category with no mod is not there.
     */
    std::vector<CategoryGroup> groupByCategory(const std::vector<Mod>& mods, const CategoryTable& table, const CategoryChoices& choices)
    {
        std::map<std::string, CategoryGroup> byName;
        for (std::size_t place = 0; place < mods.size(); ++place) {
            auto name = categoryOf(mods[place], table, choices).name;
            auto& group = byName[categoryKey(name)];
            if (group.mods.empty()) {
                group.name = std::move(name);
            }
            group.mods.push_back(place);
        }

        std::vector<CategoryGroup> groups;
        groups.reserve(byName.size());
        for (auto& [key, group] : byName) {
            if (!key.empty()) {
                groups.push_back(std::move(group));
            }
        }
        if (const auto none = byName.find(std::string()); none != byName.end()) {
            groups.push_back(std::move(none->second));
        }
        return groups;
    }

    /**
     * The names a mod can be given, in the order of their names without regard to case: the shipped ones, and the
     * others in use, the category each mod has and the one it has without the player's choice, which Reset gives it.
     * A name is there once, a shipped one as the table spells it and any other as the first mod that has it writes it.
     */
    std::vector<std::string> categoryNames(const std::vector<Mod>& mods, const CategoryTable& table, const CategoryChoices& choices)
    {
        std::map<std::string, std::string> byKey;
        const auto add = [&byKey](const std::string& name) {
            if (!name.empty()) {
                byKey.try_emplace(categoryKey(name), name);
            }
        };
        std::ranges::for_each(table.names, add);
        for (const auto& mod : mods) {
            add(ownCategory(mod, table).name);
            add(categoryOf(mod, table, choices).name);
        }
        std::vector<std::string> names;
        names.reserve(byKey.size());
        for (auto& name : byKey | std::views::values) {
            names.push_back(std::move(name));
        }
        return names;
    }

    /**
     * The options of a mod's row of the Categories page: each name a mod can be given, and no category last, whose
     * value is an empty text.
     */
    std::vector<Option> categoryOptions(const std::vector<Mod>& mods, const CategoryTable& table, const CategoryChoices& choices)
    {
        std::vector<Option> options;
        for (auto& name : categoryNames(mods, table, choices)) {
            options.push_back({ .value = name, .text = name, .help = {} });
        }
        options.push_back({ .value = std::string(), .text = NO_CATEGORY, .help = "The mod is under no category, last in the list." });
        return options;
    }

    /**
     * Fills the Categories page of the mod's own config spec, the page of the id `categories`, with a section of a row
     * for each mod, the mod itself too, in the order of their display names: a dropdown of the names, kept in the
     * config spec's file `categories` under the mod's `modName`. Its default is the category the mod has without the
     * player's choice, which its help names, so Reset gives it, and a Save keeps no value that is it. Its options are
     * its mod's client's, by its id, as the names in use change. A search does not find a row by the mod's name.
     * The rows it made before are replaced. False for a config spec with no such page, which is left as it is.
     */
    bool addCategoryRows(Mod& own, const std::vector<Mod>& mods, const CategoryTable& table)
    {
        constexpr auto SECTION_ID = "categoryMods";
        const auto page = std::ranges::find(own.pages, CATEGORIES_PAGE_ID, &Page::id);
        if (page == own.pages.end()) {
            return false;
        }
        std::erase_if(page->items, [SECTION_ID](const Item& item) {
            const auto* group = std::get_if<Group>(&item);
            return group && group->id == SECTION_ID;
        });

        std::vector<const Mod*> listed{ &own };
        for (const auto& mod : mods) {
            if (categoryKey(mod.name) != categoryKey(own.name)) {
                listed.push_back(&mod);
            }
        }
        std::ranges::sort(listed, common::CaseInsensitiveComparator(), &Mod::displayName);

        Group section;
        section.text = "Mods";
        section.id = SECTION_ID;
        for (const auto* mod : listed) {
            const auto [name, source] = ownCategory(*mod, table);
            Setting row;
            row.id = CATEGORY_SETTING_ID + mod->name;
            row.type = SettingType::Dropdown;
            row.text = mod->displayName;
            row.help = source == CategorySource::Mod     ? std::format("Reset gives {}, the mod's own category.", name)
                       : source == CategorySource::Table ? std::format("Reset gives {}, from the table of known mods.", name)
                                                         : std::format("Reset gives {}: the mod has no category of its own.", NO_CATEGORY);
            row.valueOptions.clientOptions = true;
            row.storage = FileStorage{ .file = CATEGORIES_FILE, .path = { mod->name } };
            row.valueType = StoredType::String;
            row.applies = Applies::Save;
            row.defaultValue = name;
            row.omitDefault = true;
            row.notSearched = true;
            section.items.emplace_back(std::move(row));
        }
        page->items.emplace_back(std::move(section));
        return true;
    }
}
