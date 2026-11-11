#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <string>
#include <vector>

#include "model/Categories.h"

using namespace mods_config;
using model::CategorySource;
using model::ModCategory;

namespace
{
    /**
     * A table that has to read.
     */
    model::CategoryTable table(const std::string_view text)
    {
        auto read = model::readCategoryTable(text);
        if (!read) {
            FAIL(read.error());
        }
        return std::move(*read);
    }

    model::Mod mod(std::string name, std::string category = {})
    {
        model::Mod mod{ .name = std::move(name), .displayName = {}, .frontPage = {}, .pages = {} };
        mod.category = std::move(category);
        return mod;
    }
}

TEST_CASE("Categories: a category's name is the text without the spaces around it, and a `$` token is none")
{
    CHECK(model::categoryName("Gameplay") == "Gameplay");
    CHECK(model::categoryName("  Weapons and Armor ") == "Weapons and Armor");
    CHECK(model::categoryName("").empty());
    CHECK(model::categoryName("   ").empty());
    CHECK(model::categoryName("$Gameplay").empty());
    CHECK(model::categoryName(" $Gameplay").empty());
}

TEST_CASE("Categories: the table is read with its names in their order and its mods by their modName in any case")
{
    const auto read = table(R"({
        // comments are skipped
        "categories": ["Gameplay", " Interface ", "Settlements"],
        "mods": { "SS2": "Settlements", "FallUI": "interface", "MyMod": "My Own" }
    })");

    CHECK(read.names == std::vector<std::string>{ "Gameplay", "Interface", "Settlements" });
    CHECK(read.mods.size() == 3);
    CHECK(read.mods.at("ss2") == "Settlements");
    // a shipped name is spelled as the table's names spell it, and any other name is taken as it is
    CHECK(read.mods.at("fallui") == "Interface");
    CHECK(read.mods.at("mymod") == "My Own");
}

TEST_CASE("Categories: an entry of the table that is no name, and one that is there already in another case, are left out")
{
    const auto read = table(R"({
        "categories": ["Gameplay", 3, "", "$Token", "GAMEPLAY", "Combat"],
        "mods": { "One": "Gameplay", "Two": 3, "Three": "", "Four": "$Token", "one": "Combat" }
    })");

    CHECK(read.names == std::vector<std::string>{ "Gameplay", "Combat" });
    CHECK(read.mods.size() == 1);
    // the first of two entries that differ only by their case is kept
    CHECK(read.mods.at("one") == "Gameplay");
}

TEST_CASE("Categories: a table with no names or no mods is an empty one, and a text that is no JSON object is an error")
{
    const auto empty = table("{}");
    CHECK(empty.names.empty());
    CHECK(empty.mods.empty());

    const auto wrong = table(R"({ "categories": "Gameplay", "mods": ["SS2"] })");
    CHECK(wrong.names.empty());
    CHECK(wrong.mods.empty());

    CHECK(!model::readCategoryTable("{ not json"));
    CHECK(!model::readCategoryTable("[]"));
}

TEST_CASE("Categories: a table whose file is not there is an empty one")
{
    const auto missing = model::loadCategoryTable("no-such-folder/Categories.json");
    CHECK(missing.names.empty());
    CHECK(missing.mods.empty());
}

TEST_CASE("Categories: a mod's category is its own, then the table's for its modName in any case, and else none")
{
    const auto known = table(R"({ "categories": ["Gameplay", "Settlements"], "mods": { "SS2": "Settlements" } })");

    CHECK(model::ownCategory(mod("SS2"), known) == ModCategory{ .name = "Settlements", .source = CategorySource::Table });
    CHECK(model::ownCategory(mod("ss2"), known) == ModCategory{ .name = "Settlements", .source = CategorySource::Table });
    // its own category is before the table's
    CHECK(model::ownCategory(mod("SS2", "Gameplay"), known) == ModCategory{ .name = "Gameplay", .source = CategorySource::Mod });
    CHECK(model::ownCategory(mod("Other"), known) == ModCategory{});
    CHECK(model::ownCategory(mod("Other"), {}) == ModCategory{});
}

TEST_CASE("Categories: a mod's own category is spelled as the table spells a shipped name, and as the mod writes any other")
{
    const auto known = table(R"({ "categories": ["Weapons and Armor"] })");

    CHECK(model::ownCategory(mod("MyMod", "weapons AND armor"), known).name == "Weapons and Armor");
    CHECK(model::ownCategory(mod("MyMod", "my own"), known).name == "my own");
}

TEST_CASE("Categories: the shipped table has the ten shipped names, and every mod of it is in one of them")
{
    const auto shipped = model::loadCategoryTable(MODS_CONFIG_CATEGORY_TABLE);

    CHECK(shipped.names == std::vector<std::string>{ "Gameplay", "Combat", "Weapons and Armor", "Settlements", "Characters", "Interface", "Visuals", "Audio", "VR", "Utilities" });
    CHECK(!shipped.mods.empty());
    for (const auto& [modName, category] : shipped.mods) {
        INFO(modName << ": " << category);
        CHECK(std::ranges::find(shipped.names, category) != shipped.names.end());
    }
}

TEST_CASE("Categories: a list is grouped from a number of mods, and any list with 0")
{
    CHECK(model::isGrouped(30, 30));
    CHECK(model::isGrouped(31, 30));
    CHECK(!model::isGrouped(29, 30));
    CHECK(model::isGrouped(1, 0));
    CHECK(model::isGrouped(0, 0));
}

TEST_CASE("Categories: the mods are grouped by their categories in the order of the names, with the mods of none last")
{
    const auto known = table(R"({ "categories": ["Interface", "Settlements"], "mods": { "SS2": "Settlements" } })");
    const std::vector<model::Mod> mods{ mod("Alpha", "settlements"), mod("Beta"), mod("Gamma", "interface"), mod("SS2"), mod("Delta", "combat"), mod("Epsilon", "Combat") };

    const auto groups = model::groupByCategory(mods, known);

    REQUIRE(groups.size() == 4);
    // any other name is as the first of its mods writes it
    CHECK(groups[0] == model::CategoryGroup{ .name = "combat", .mods = { 4, 5 } });
    // a shipped name is as the table spells it
    CHECK(groups[1] == model::CategoryGroup{ .name = "Interface", .mods = { 2 } });
    // the mod's own and the table's are one category
    CHECK(groups[2] == model::CategoryGroup{ .name = "Settlements", .mods = { 0, 3 } });
    CHECK(groups[3] == model::CategoryGroup{ .name = "", .mods = { 1 } });
}

TEST_CASE("Categories: mods that all have a category have no group of none, and no mods have no groups")
{
    const auto groups = model::groupByCategory({ mod("Alpha", "Combat"), mod("Beta", "Combat") }, {});
    REQUIRE(groups.size() == 1);
    CHECK(groups[0] == model::CategoryGroup{ .name = "Combat", .mods = { 0, 1 } });

    CHECK(model::groupByCategory({}, {}).empty());
}
