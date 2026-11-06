#include <catch2/catch_test_macros.hpp>

#include "TestProperties.h"
#include "mcm/McmGameTexts.h"
#include "mcm/McmLoader.h"

using namespace mods_config;

namespace
{
    /**
     * A config that has to load.
     */
    model::Mod loaded(const std::string_view text)
    {
        auto mod = mcm::loadConfig(text);
        if (!mod) {
            FAIL(mod.error());
        }
        return std::move(*mod);
    }

    /**
     * The text of a text row, its lines joined with ` / `.
     */
    std::string linesOf(const model::Item& item)
    {
        std::string words;
        for (const auto& line : std::get<model::Text>(item).lines) {
            words += words.empty() ? "" : " / ";
            for (const auto& run : line.runs) {
                words += run.text;
            }
        }
        return words;
    }
}

TEST_CASE("McmGameTexts: a row shows the text the game has for it, and keeps its own where the game has none")
{
    test_properties::TestProperties game;
    auto mod = loaded(R"({
        "modName": "MyMod",
        "content": [
            { "type": "section", "text": "MyMod.esp|10", "textFromFormName": true },
            { "type": "button", "text": "MyMod.esp|20", "textFromFormDescription": true, "help": "MyMod.esp|10", "helpFromFormName": true },
            {
                "type": "switcher", "text": "Fallback",
                "textFromStringProperty": { "form": "MyMod.esp|40", "scriptName": "MyQuest", "propertyName": "Title" },
                "helpFromStringArrayProperty": { "form": "MyMod.esp|40", "scriptName": "MyQuest", "propertyName": "Helps", "index": 1 }
            },
            { "type": "text", "text": "MyMod.esp|50", "textFromFormName": true, "html": true },
            { "type": "text", "text": "MyMod.esp|60", "textFromFormName": true },
            { "type": "hotkey", "text": "MyMod.esp|70", "textFromFormName": true }
        ]
    })");
    game.formTexts[{ "MyMod.esp", 0x10, false }] = "Settlements";
    game.formTexts[{ "MyMod.esp", 0x20, true }] = "Builds <b>everything</b>.";
    game.formTexts[{ "MyMod.esp", 0x50, false }] = "First<br>Second";
    game.formTexts[{ "MyMod.esp", 0x60, false }] = "First<br>Second";
    game.values[{ "MyMod.esp", 0x40, "MyQuest", "Title" }] = std::string("From the script");
    game.lists[{ "MyMod.esp", 0x40, "MyQuest", "Helps" }] = { "Zero", "One" };

    mcm::readGameTexts(mod);

    const auto& group = std::get<model::Group>(mod.frontPage.items[0]);
    CHECK(group.text == "Settlements");
    const auto& rows = group.items;
    const auto& button = std::get<model::Button>(rows[0]);
    CHECK(button.text == "Builds everything.");
    REQUIRE(button.textRuns.size() == 3);
    CHECK(button.textRuns[1].bold);
    CHECK(button.help == "Settlements");
    const auto& setting = std::get<model::Setting>(rows[1]);
    CHECK(setting.text == "From the script");
    CHECK(setting.help == "One");

    // a text row takes tags with its html, as its own text does
    CHECK(linesOf(rows[2]) == "First / Second");
    CHECK(linesOf(rows[3]) == "First<br>Second");

    // a form with no name keeps the row's own text
    CHECK(std::get<model::Hotkey>(rows[4]).text == "MyMod.esp|70");

    // a property that is no text keeps what the row shows, and so does a place the array does not have
    const auto& read = std::get<model::Setting>(std::get<model::Group>(mod.frontPage.items[0]).items[1]);
    game.values[{ "MyMod.esp", 0x40, "MyQuest", "Title" }] = 7;
    game.lists[{ "MyMod.esp", 0x40, "MyQuest", "Helps" }] = { "Zero" };
    mcm::readGameTexts(mod);
    CHECK(read.text == "From the script");
    CHECK(read.help == "One");

    // a property is read again as it changes, and its empty text is the row's, as in MCM's menu
    game.values[{ "MyMod.esp", 0x40, "MyQuest", "Title" }] = std::string();
    mcm::readGameTexts(mod);
    CHECK(read.text.empty());
}
