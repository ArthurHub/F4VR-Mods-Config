#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <string>
#include <vector>

#include "model/Categories.h"
#include "model/ChoiceOptions.h"

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

TEST_CASE("Categories: the shipped table has the twelve shipped names, and every mod of it is in one of them")
{
    const auto shipped = model::loadCategoryTable(MODS_CONFIG_CATEGORY_TABLE);

    const std::vector<std::string>
        names{ "Gameplay", "Survival", "Combat", "Weapons and Armor", "Settlements", "Characters", "Interface", "Visuals", "Audio", "Performance", "VR", "Utilities" };
    CHECK(shipped.names == names);
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

TEST_CASE("Categories: the player's choices are read by their modName in any case, with an empty text for no category")
{
    const auto choices = model::readCategoryChoices("SS2 = \"Gameplay\"\n\"My Mod\" = \"\"\nOther = 3\nSpaced = \" Combat \"\n");

    REQUIRE(choices);
    CHECK(*choices == model::CategoryChoices{ { "ss2", "Gameplay" }, { "my mod", "" }, { "spaced", "Combat" } });

    CHECK(model::readCategoryChoices("")->empty());
    CHECK(!model::readCategoryChoices("SS2 = "));
    CHECK(model::loadCategoryChoices("no-such-folder/Categories.toml").empty());
}

TEST_CASE("Categories: the player's choice is before a mod's own category and the table's, and can be no category")
{
    const auto known = table(R"({ "categories": ["Gameplay", "Settlements"], "mods": { "SS2": "Settlements" } })");
    const model::CategoryChoices choices{ { "ss2", "gameplay" }, { "mymod", "" }, { "other", "My Own" } };

    // a shipped name is spelled as the table spells it
    CHECK(model::categoryOf(mod("SS2"), known, choices) == ModCategory{ .name = "Gameplay", .source = CategorySource::Player });
    CHECK(model::categoryOf(mod("MyMod", "Settlements"), known, choices) == ModCategory{ .name = "", .source = CategorySource::Player });
    CHECK(model::categoryOf(mod("Other"), known, choices) == ModCategory{ .name = "My Own", .source = CategorySource::Player });
    // a mod with no choice has its own
    CHECK(model::categoryOf(mod("Untouched", "Settlements"), known, choices) == ModCategory{ .name = "Settlements", .source = CategorySource::Mod });
    CHECK(model::categoryOf(mod("SS2"), known, {}) == ModCategory{ .name = "Settlements", .source = CategorySource::Table });

    const auto groups = model::groupByCategory({ mod("SS2"), mod("MyMod", "Settlements"), mod("Untouched", "Settlements") }, known, choices);
    REQUIRE(groups.size() == 3);
    CHECK(groups[0] == model::CategoryGroup{ .name = "Gameplay", .mods = { 0 } });
    CHECK(groups[1] == model::CategoryGroup{ .name = "Settlements", .mods = { 2 } });
    CHECK(groups[2] == model::CategoryGroup{ .name = "", .mods = { 1 } });
}

TEST_CASE("Categories: the names a mod can be given are the shipped ones and the others in use, in the order of their names, and no category is the last option")
{
    const auto known = table(R"({ "categories": ["Gameplay", "Settlements"], "mods": { "SS2": "Settlements" } })");
    const std::vector<model::Mod> mods{ mod("SS2"), mod("Alpha", "Zeta"), mod("Beta", "gameplay"), mod("Gamma", "zeta"), mod("Delta") };
    const model::CategoryChoices choices{ { "alpha", "My Own" }, { "delta", "" }, { "gone", "Not Listed" } };

    // Zeta is in use as the category Alpha has without the player's choice, and the choice of a mod that is not
    // listed is not
    CHECK(model::categoryNames(mods, known, choices) == std::vector<std::string>{ "Gameplay", "My Own", "Settlements", "Zeta" });

    const auto options = model::categoryOptions(mods, known, choices);
    REQUIRE(options.size() == 5);
    CHECK(options[0].value == model::Value(std::string("Gameplay")));
    CHECK(options[0].text == "Gameplay");
    CHECK(options[4].value == model::Value(std::string()));
    CHECK(options[4].text == "No category");
}

TEST_CASE("Categories: the Categories page is filled with a row for each mod, in the order of their names, kept under its modName")
{
    const auto known = table(R"({ "categories": ["Interface", "Settlements"], "mods": { "SS2": "Settlements" } })");
    auto own = mod("ModsConfigFO4VR", "interface");
    own.displayName = "Mods Config FO4 VR";
    own.pages.push_back({ .name = "Panel", .items = {} });
    own.pages.push_back({ .name = "Categories", .items = { model::Text{} }, .pages = {}, .id = "categories" });
    auto ss2 = mod("SS2");
    ss2.displayName = "Sim Settlements 2";
    auto alpha = mod("Alpha");
    alpha.displayName = "alpha";
    // the mod's own is among the mods the menu lists, and has one row
    const std::vector<model::Mod> mods{ ss2, own, alpha };

    REQUIRE(model::addCategoryRows(own, mods, known));

    const auto& page = own.pages[1];
    REQUIRE(page.items.size() == 2);
    const auto& section = std::get<model::Group>(page.items[1]);
    CHECK(section.text == "Mods");
    REQUIRE(section.items.size() == 3);

    const auto& first = std::get<model::Setting>(section.items[0]);
    CHECK(first.id == "category.Alpha");
    CHECK(first.text == "alpha");
    CHECK(first.type == model::SettingType::Dropdown);
    CHECK(first.storage == model::Storage(model::FileStorage{ .file = "categories", .path = { "Alpha" } }));
    CHECK(first.defaultValue == model::Value(std::string()));
    CHECK(first.help == "Reset gives No category: the mod has no category of its own.");
    CHECK(first.valueOptions.clientOptions);
    CHECK(first.valueOptions.typedOption);
    CHECK(first.valueOptions.maxLength == 24);
    CHECK(first.applies == model::Applies::Save);
    CHECK(first.omitDefault);
    CHECK(first.notSearched);

    const auto& second = std::get<model::Setting>(section.items[1]);
    CHECK(second.id == "category.ModsConfigFO4VR");
    CHECK(second.defaultValue == model::Value(std::string("Interface")));
    CHECK(second.help == "Reset gives Interface, the mod's own category.");

    const auto& third = std::get<model::Setting>(section.items[2]);
    CHECK(third.id == "category.SS2");
    CHECK(third.defaultValue == model::Value(std::string("Settlements")));
    CHECK(third.help == "Reset gives Settlements, from the table of known mods.");

    // filled again, its rows are replaced
    REQUIRE(model::addCategoryRows(own, { ss2 }, known));
    REQUIRE(own.pages[1].items.size() == 2);
    CHECK(std::get<model::Group>(own.pages[1].items[1]).items.size() == 2);

    // a config spec with no such page is left as it is
    auto other = mod("Other");
    CHECK(!model::addCategoryRows(other, mods, known));
}

TEST_CASE("Categories: a typed name is trimmed, takes the spelling of an option it is in another case, and an empty one is none")
{
    const auto options = model::categoryOptions({}, *model::readCategoryTable(R"({ "categories": ["Gameplay"] })"), {});
    const auto typed = [&options](const std::string_view text) {
        return model::typedOptionValue(text, options);
    };
    CHECK(typed("  Mods I Like ") == model::Value(std::string("Mods I Like")));
    CHECK(typed("gAMEPLAY") == model::Value(std::string("Gameplay")));
    // the text of the option of no category gives no category
    CHECK(typed("no category") == model::Value(std::string()));
    CHECK_FALSE(typed("   "));
    CHECK_FALSE(typed(""));
}
