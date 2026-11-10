#include <catch2/catch_test_macros.hpp>

#include <string>
#include <unordered_set>
#include <variant>
#include <vector>

#include "mcm/McmStore.h"
#include "model/Search.h"

using namespace mods_config;
using model::SearchQuery;
using model::TextRange;

namespace
{
    model::Item setting(std::string text)
    {
        return model::Setting{ .id = text, .type = model::SettingType::Switcher, .text = std::move(text), .help = {}, .valueOptions = {}, .storage = {} };
    }

    model::Item section(std::string text, std::vector<model::Item> items)
    {
        return model::Group{ .text = std::move(text), .items = std::move(items), .visibleWhen = {} };
    }

    model::Page page(std::string name, std::vector<model::Item> items, std::vector<model::Page> pages = {})
    {
        return { .name = std::move(name), .items = std::move(items), .pages = std::move(pages) };
    }

    /**
     * A mod with a setting on its front page, a page of settings with a section, a page that no row of matches
     * by its name, a page with a page under it, and a page with a button, a hotkey and a text.
     */
    model::Mod flashlight()
    {
        model::Mod mod{ .name = "Flashlight", .displayName = "Immersive Flashlight", .frontPage = {}, .pages = {} };
        mod.frontPage.items.push_back(setting("Enabled"));
        mod.pages.push_back(page("Beam", { setting("Beam width"), setting("Intensity"), section("Advanced", { setting("Beam angle"), setting("Fade") }) }));
        mod.pages.push_back(page("Colors", { setting("Red"), setting("Green") }));
        mod.pages.push_back(page("Body stow", { setting("Holster the light on the hip") }, { page("Hip", { setting("Offset") }) }));

        model::Page rows = page("Rows", {});
        rows.items.push_back(model::Button{ .text = "Reset all", .textRuns = {}, .help = {} });
        rows.items.push_back(model::Hotkey{ .id = "toggle", .text = "Toggle the light", .textRuns = {}, .help = {} });
        rows.items.push_back(model::Text{ .lines = { model::TextLine{ .align = model::TextAlign::Left, .runs = { model::TextRun{ .text = "Reset text" } } } } });
        mod.pages.push_back(std::move(rows));
        return mod;
    }

    std::vector<std::size_t> placesOf(const std::vector<model::PageFound>& pages)
    {
        std::vector<std::size_t> places;
        for (const auto& found : pages) {
            places.push_back(found.place);
        }
        return places;
    }
}

TEST_CASE("Search: a query is its words, in lower case without accents, split at spaces and punctuation")
{
    const SearchQuery query("  Hol, SIZ-Écran ");
    CHECK(query.words() == std::vector<std::u32string>{ U"hol", U"siz", U"ecran" });
    CHECK(SearchQuery(" ,. ").empty());
    CHECK(SearchQuery().empty());
}

TEST_CASE("Search: each word of the query starts a word of the name, in any order, and its letters are underlined")
{
    CHECK(model::matchRanges(SearchQuery("hol siz"), "Holster size") == std::vector<TextRange>{ { 0, 3 }, { 8, 11 } });
    CHECK(model::matchRanges(SearchQuery("siz hol"), "Holster size") == std::vector<TextRange>{ { 0, 3 }, { 8, 11 } });
    CHECK(model::matchRanges(SearchQuery("ster"), "Holster size").empty());

    // a word starts after punctuation
    CHECK(model::matchRanges(SearchQuery("hand"), "Two-handed hold") == std::vector<TextRange>{ { 4, 8 } });
    CHECK(model::matchRanges(SearchQuery("s"), "FRIK's") == std::vector<TextRange>{ { 5, 6 } });
}

TEST_CASE("Search: case and accents are ignored in Latin, Greek and Cyrillic, and the range is the name's own bytes")
{
    CHECK(model::matchRanges(SearchQuery("ecran"), "Écran") == std::vector<TextRange>{ { 0, 6 } });
    CHECK(model::matchRanges(SearchQuery("LUMIERE"), "Réglage de la lumière") == std::vector<TextRange>{ { 15, 23 } });
    CHECK(model::matchRanges(SearchQuery("zolc"), "Żółć") == std::vector<TextRange>{ { 0, 8 } });
    CHECK(model::matchRanges(SearchQuery("φως"), "ΦΩΣ") == std::vector<TextRange>{ { 0, 6 } });
    CHECK(model::matchRanges(SearchQuery("мир прив"), "ПРИВЕТ мир") == std::vector<TextRange>{ { 0, 8 }, { 13, 19 } });
    CHECK(model::matchRanges(SearchQuery("елк"), "Ёлка") == std::vector<TextRange>{ { 0, 6 } });

    // an accent that is a character of its own belongs to the letter before it
    CHECK(model::matchRanges(SearchQuery("ecran"),
              "e\xCC\x81"
              "cran") == std::vector<TextRange>{ { 0, 7 } });
}

TEST_CASE("Search: a word of Chinese, Japanese or Korean matches anywhere in the name")
{
    CHECK(model::matchRanges(SearchQuery("設定"), "画面設定") == std::vector<TextRange>{ { 6, 12 } });
    CHECK(model::matchRanges(SearchQuery("音量"), "画面設定").empty());
}

TEST_CASE("Search: a row matches by its own name, and by the names above it with a word of its own")
{
    const auto mod = flashlight();

    auto found = model::searchMod(mod, SearchQuery("intens"), nullptr);
    REQUIRE(found);
    CHECK_FALSE(found->byName);
    CHECK(found->frontCount == 0);
    CHECK(placesOf(found->pages) == std::vector<std::size_t>{ 0 });
    CHECK(found->pages[0].count == 1);
    CHECK(found->total == 1);

    // the mod's name and the section's narrow a row down, and a row needs one word of its own
    found = model::searchMod(mod, SearchQuery("flash adv fade"), nullptr);
    REQUIRE(found);
    CHECK(placesOf(found->pages) == std::vector<std::size_t>{ 0 });
    CHECK(found->pages[0].count == 1);

    CHECK_FALSE(model::searchMod(mod, SearchQuery("holz"), nullptr));
}

TEST_CASE("Search: the most specific match wins: a page or a section shows all its rows only when none of them matches")
{
    const auto mod = flashlight();

    // the page Beam matches, and so do two of its rows: only they are shown
    auto found = model::searchMod(mod, SearchQuery("beam"), nullptr);
    REQUIRE(found);
    CHECK(placesOf(found->pages) == std::vector<std::size_t>{ 0 });
    CHECK(found->pages[0].count == 2);

    // no row of Colors matches: the page shows them all
    found = model::searchMod(mod, SearchQuery("colors"), nullptr);
    REQUIRE(found);
    CHECK(placesOf(found->pages) == std::vector<std::size_t>{ 1 });
    CHECK(found->pages[0].count == 2);

    // nor of the section Advanced
    found = model::searchMod(mod, SearchQuery("advanced"), nullptr);
    REQUIRE(found);
    CHECK(found->pages[0].count == 2);

    // a page found by its name has its pages too, all of them
    found = model::searchMod(mod, SearchQuery("body"), nullptr);
    REQUIRE(found);
    REQUIRE(placesOf(found->pages) == std::vector<std::size_t>{ 2 });
    CHECK(found->pages[0].count == 1);
    REQUIRE(found->pages[0].pages.size() == 1);
    CHECK(found->pages[0].pages[0].count == 1);
    CHECK(found->total == 2);

    // a page under a page that matches is shown under it, and the page above with none of its rows
    found = model::searchMod(mod, SearchQuery("offset"), nullptr);
    REQUIRE(found);
    REQUIRE(placesOf(found->pages) == std::vector<std::size_t>{ 2 });
    CHECK(found->pages[0].count == 0);
    CHECK(placesOf(found->pages[0].pages) == std::vector<std::size_t>{ 0 });
}

TEST_CASE("Search: a mod found by its name alone has every page and every row")
{
    const auto mod = flashlight();
    const auto found = model::searchMod(mod, SearchQuery("immersive"), nullptr);
    REQUIRE(found);
    CHECK(found->byName);
    CHECK(found->frontCount == 1);
    CHECK(placesOf(found->pages) == std::vector<std::size_t>{ 0, 1, 2, 3 });
    // every setting, the button and the hotkey, and not the text
    CHECK(found->total == 11);
}

TEST_CASE("Search: buttons and hotkeys are found, and a text is not")
{
    const auto mod = flashlight();
    auto found = model::searchMod(mod, SearchQuery("reset"), nullptr);
    REQUIRE(found);
    CHECK(placesOf(found->pages) == std::vector<std::size_t>{ 3 });
    CHECK(found->pages[0].count == 1);

    found = model::searchMod(mod, SearchQuery("toggle"), nullptr);
    REQUIRE(found);
    CHECK(found->pages[0].count == 1);
}

TEST_CASE("Search: a mod that is not available is found by its name alone, and a disabled page by its name")
{
    model::Mod missing{ .name = "SS2", .displayName = "Sim Settlements", .frontPage = {}, .pages = {}, .notAvailable = "its plugin SS2.esm is not loaded" };
    const auto found = model::searchMod(missing, SearchQuery("sim"), nullptr);
    REQUIRE(found);
    CHECK(found->byName);
    CHECK(found->total == 0);

    auto mod = flashlight();
    mod.pages[1].notAvailable = "its plugin is not loaded";
    auto colors = model::searchMod(mod, SearchQuery("colors"), nullptr);
    REQUIRE(colors);
    REQUIRE(colors->pages.size() == 1);
    CHECK_FALSE(colors->pages[0].enabled);
    CHECK(colors->pages[0].count == 0);

    // its rows are not searched
    CHECK_FALSE(model::searchMod(mod, SearchQuery("red"), nullptr));
}

TEST_CASE("Search: a row a condition hides is not found with the store, and is with none")
{
    auto mod = flashlight();
    mod.frontPage.items.push_back(setting("Show advanced"));
    std::get<model::Setting>(mod.pages[0].items[1]).visibleWhen = model::Condition{ .kind = model::Condition::Kind::On, .setting = "Show advanced", .conditions = {} };

    mcm::McmStore store;
    store.open(mod);
    CHECK_FALSE(model::searchMod(mod, SearchQuery("intens"), &store));

    const auto found = model::searchMod(mod, SearchQuery("intens"), nullptr);
    REQUIRE(found);
    CHECK(found->pages[0].count == 1);
}

TEST_CASE("Search: the items it shows are its rows and the sections they are in, which a page has alone while searching")
{
    const auto mod = flashlight();
    const auto& beam = mod.pages[0].items;
    const auto& advanced = std::get<model::Group>(beam[2]).items;

    // a row in a section, and the section
    auto found = model::searchMod(mod, SearchQuery("fade"), nullptr);
    REQUIRE(found);
    CHECK(found->rows == std::unordered_set<const model::Item*>{ &beam[2], &advanced[1] });

    // a section found by its heading has all its rows
    found = model::searchMod(mod, SearchQuery("advanced"), nullptr);
    REQUIRE(found);
    CHECK(found->rows == std::unordered_set<const model::Item*>{ &beam[2], &advanced[0], &advanced[1] });

    // a page found by its name has all its rows, and a text row is not one
    found = model::searchMod(mod, SearchQuery("rows"), nullptr);
    REQUIRE(found);
    const auto& rows = mod.pages[3].items;
    CHECK(found->rows == std::unordered_set<const model::Item*>{ &rows[0], &rows[1] });
}
