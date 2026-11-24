#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <format>
#include <functional>
#include <sstream>
#include <unordered_set>

#include "TestConfigSpecs.h"
#include "TestFiles.h"
#include "common/CommonUtils.h"
#include "model/Categories.h"
#include "model/Colors.h"
#include "model/InputBindings.h"
#include "model/OptionSets.h"
#include "model/TextMarkup.h"
#include "model/Transforms.h"
#include "spec/ConfigSpecLoader.h"

using namespace mods_config;
using namespace test_config_specs;

namespace
{
    // a choice whose options set the three settings after it, of which one is a transform
    constexpr auto QUALITY = R"(
        { "type": "stepper", "valueType": "string", "id": "quality", "text": "Quality", "valueOptions": { "options": [
            { "value": "low", "text": "Low", "sets": { "fScale": 0.5, "zone": { "pos": [0, 0, 12] } } },
            { "value": "high", "text": "High", "sets": { "FSCALE": 1.5, "sPlace": "chest" } } ] } },
        { "type": "slider", "valueType": "float", "id": "fScale", "text": "Scale", "default": 1.0, "valueOptions": { "min": 0.5, "max": 1.5, "step": 0.05 },
          "storage": { "path": ["Main", "fScale"] } },
        { "type": "dropdown", "valueType": "string", "id": "sPlace", "text": "Place", "default": "hip",
          "valueOptions": { "options": [ { "value": "hip", "text": "Hip" }, { "value": "chest", "text": "Chest" } ] }, "storage": { "path": ["Main", "sPlace"] } },
        { "type": "cf.transform", "id": "zone", "text": "Zone", "default": { "scale": 1.0 }, "storage": { "path": ["Main", "tZone"] } })";
}

TEST_CASE("ConfigSpecLoader: a slider and a switcher with no valueType have the type of their control")
{
    const auto mod = loaded(configSpecOf(R"(
        { "type": "switcher", "text": "Enable", "storage": { "path": ["Main", "bEnable"] } },
        { "type": "slider", "text": "Scale", "valueType": "number", "storage": { "path": ["Main", "fScale"] } }
    )"));
    CHECK(std::get<model::Setting>(mod.frontPage.items[0]).valueType == model::StoredType::Bool);
    CHECK(std::get<model::Setting>(mod.frontPage.items[1]).valueType == model::StoredType::Float);
}

TEST_CASE("ConfigSpecLoader: a setting has its default in the type of its value, and none of another type")
{
    const auto mod = loaded(configSpecOf(R"(
        { "type": "switcher", "text": "Bool", "default": true, "storage": { "path": ["Main", "a"] } },
        { "type": "slider", "valueType": "int", "text": "Int", "default": 217, "storage": { "path": ["Main", "b"] } },
        { "type": "slider", "text": "Float", "default": 0.5, "storage": { "path": ["Main", "c"] } },
        { "type": "slider", "text": "Whole float", "default": 2, "storage": { "path": ["Main", "d"] } },
        { "type": "textinput", "text": "String", "default": "Tick", "storage": { "path": ["Main", "e"] } },
        { "type": "switcher", "text": "Missing", "storage": { "path": ["Main", "f"] } },
        { "type": "switcher", "text": "Number for a bool", "default": 1, "storage": { "path": ["Main", "g"] } },
        { "type": "slider", "valueType": "int", "text": "Point for an int", "default": 2.5, "storage": { "path": ["Main", "h"] } },
        { "type": "textinput", "text": "Number for a string", "default": 3, "storage": { "path": ["Main", "i"] } }
    )"));
    const auto settings = settingsOf(mod);
    REQUIRE(settings.size() == 9);
    CHECK(settings[0].defaultValue == model::Value(true));
    CHECK(settings[1].defaultValue == model::Value(217));
    CHECK(common::fEqual(std::get<float>(settings[2].defaultValue.value()), 0.5f));
    CHECK(common::fEqual(std::get<float>(settings[3].defaultValue.value()), 2.0f));
    CHECK(settings[4].defaultValue == model::Value(std::string("Tick")));
    for (std::size_t i = 5; i < settings.size(); ++i) {
        INFO(settings[i].text);
        CHECK_FALSE(settings[i].defaultValue);
    }
}

TEST_CASE("ConfigSpecLoader: a default that is not one of its setting's values is none, so the setting has no Reset")
{
    const auto mod = loaded(configSpecOf(R"(
        { "type": "slider", "text": "At the least", "default": 0.5, "valueOptions": { "min": 0.5, "max": 1.5 }, "storage": { "path": ["Main", "a"] } },
        { "type": "slider", "valueType": "int", "text": "At the most", "default": 255, "valueOptions": { "min": 0, "max": 255 }, "storage": { "path": ["Main", "b"] } },
        { "type": "slider", "text": "No range", "default": 7.0, "storage": { "path": ["Main", "c"] } },
        { "type": "stepper", "valueType": "float", "text": "An option", "default": 1,
          "valueOptions": { "options": [ { "value": 0.5, "text": "Small" }, { "value": 1.0, "text": "Normal" } ] }, "storage": { "path": ["Main", "d"] } },
        { "type": "slider", "text": "Below the least", "default": 0.4, "valueOptions": { "min": 0.5, "max": 1.5 }, "storage": { "path": ["Main", "e"] } },
        { "type": "textinput", "valueType": "int", "text": "Above the most", "default": 10, "valueOptions": { "min": 1, "max": 9 }, "storage": { "path": ["Main", "f"] } },
        { "type": "dropdown", "valueType": "string", "text": "No option", "default": "d",
          "valueOptions": { "options": [ { "value": "a", "text": "A" } ] }, "storage": { "path": ["Main", "g"] } },
        { "type": "stepper", "text": "Past the last", "default": 2, "valueOptions": { "options": ["Left", "Right"] }, "storage": { "path": ["Main", "h"] } },
        { "type": "dropdown", "text": "No options", "default": 0, "storage": { "path": ["Main", "i"] } }
    )"));
    const auto settings = settingsOf(mod);
    REQUIRE(settings.size() == 9);
    for (std::size_t i = 0; i < settings.size(); ++i) {
        INFO(settings[i].text);
        CHECK(settings[i].defaultValue.has_value() == (i < 4));
    }
}

TEST_CASE("ConfigSpecLoader: a text input of a text has its least and its most characters, and a default of a length it allows")
{
    const auto mod = loaded(configSpecOf(R"(
        { "type": "textinput", "text": "Limits", "default": "abc", "valueOptions": { "minLength": 2, "maxLength": 8 }, "storage": { "path": ["Main", "a"] } },
        { "type": "textinput", "text": "Not empty", "default": "a", "valueOptions": { "allowEmpty": false }, "storage": { "path": ["Main", "b"] } },
        { "type": "textinput", "text": "Not a length", "default": "", "valueOptions": { "minLength": -1, "maxLength": "8" }, "storage": { "path": ["Main", "c"] } },
        { "type": "textinput", "valueType": "int", "text": "A number", "default": 5, "valueOptions": { "maxLength": 2 }, "storage": { "path": ["Main", "d"] } },
        { "type": "textinput", "text": "Empty", "default": "", "valueOptions": { "allowEmpty": false }, "storage": { "path": ["Main", "e"] } },
        { "type": "textinput", "text": "Too long", "default": "abc", "valueOptions": { "maxLength": 2 }, "storage": { "path": ["Main", "f"] } }
    )"));
    const auto settings = settingsOf(mod);
    REQUIRE(settings.size() == 6);
    CHECK(settings[0].valueOptions.minLength == 2);
    CHECK(settings[0].valueOptions.maxLength == 8);
    // allowEmpty of false is a least of one
    CHECK(settings[1].valueOptions.minLength == 1);
    CHECK(settings[1].valueOptions.maxLength == 0);
    CHECK(settings[2].valueOptions.minLength == 0);
    CHECK(settings[2].valueOptions.maxLength == 0);
    // a text input of a number has its least and its most instead
    CHECK(settings[3].valueOptions.maxLength == 0);
    for (std::size_t i = 0; i < settings.size(); ++i) {
        INFO(settings[i].text);
        CHECK(settings[i].defaultValue.has_value() == (i < 4));
    }
}

TEST_CASE("ConfigSpecLoader: a read-only setting has no default, as it has no Reset")
{
    const auto mod = loaded(configSpecOf(R"(
        { "type": "slider", "text": "Measured", "readOnly": true, "storage": { "path": ["Main", "a"] } },
        { "type": "switcher", "text": "With a default", "readOnly": true, "default": true, "storage": { "path": ["Main", "b"] } },
        { "type": "switcher", "text": "Edited", "readOnly": false, "default": true, "storage": { "path": ["Main", "c"] } }
    )"));
    const auto settings = settingsOf(mod);
    REQUIRE(settings.size() == 3);
    CHECK(settings[0].readOnly);
    CHECK_FALSE(settings[0].defaultValue);
    CHECK(settings[1].readOnly);
    CHECK_FALSE(settings[1].defaultValue);
    CHECK_FALSE(settings[2].readOnly);
    CHECK(settings[2].defaultValue == model::Value(true));
}

TEST_CASE("ConfigSpecLoader: a number's display has its decimals from 0 to 6, its unit, which is a text, and whether it is a percentage")
{
    model::Translations translations;
    translations.add("$MyMod_Meters", "meters");
    const auto mod = loaded(configSpecOf(R"(
        { "type": "slider", "text": "Height", "default": 1.0, "display": { "decimals": 2, "unit": "m" }, "storage": { "path": ["Main", "a"] } },
        { "type": "slider", "text": "Opacity", "default": 1.0, "display": { "percent": true }, "storage": { "path": ["Main", "b"] } },
        { "type": "textinput", "valueType": "float", "text": "Reach", "default": 1.0, "display": { "unit": { "token": "$MyMod_Meters", "fallback": "m" } },
          "storage": { "path": ["Main", "c"] } },
        { "type": "slider", "text": "Too many", "default": 1.0, "display": { "decimals": 7 }, "storage": { "path": ["Main", "d"] } },
        { "type": "slider", "text": "A point", "default": 1.0, "display": { "decimals": 1.5 }, "storage": { "path": ["Main", "e"] } },
        { "type": "slider", "text": "None", "default": 1.0, "storage": { "path": ["Main", "f"] } }
    )"),
        translations);
    const auto settings = settingsOf(mod);
    REQUIRE(settings.size() == 6);
    CHECK(settings[0].display == model::NumberDisplay{ .decimals = 2, .unit = "m", .percent = false });
    CHECK(settings[1].display == model::NumberDisplay{ .decimals = {}, .unit = {}, .percent = true });
    CHECK(settings[2].display == model::NumberDisplay{ .decimals = {}, .unit = "meters", .percent = false });
    for (std::size_t i = 3; i < settings.size(); ++i) {
        INFO(settings[i].text);
        CHECK(settings[i].display == model::NumberDisplay{});
    }
}

TEST_CASE("ConfigSpecLoader: a setting has its author's id, or one from where its value is kept")
{
    const auto mod = loaded(configSpecOf(R"(
        { "id": "playerHeight", "type": "slider", "text": "Height", "storage": { "path": ["Body", "fHeight"] } },
        { "type": "switcher", "text": "Two names", "storage": { "path": ["Holsters", "bEnabled"] } },
        { "type": "switcher", "text": "Key", "storage": { "section": "Holsters", "key": "bLeft" } },
        { "type": "switcher", "text": "Three names", "storage": { "path": ["body", "hands", "bLeft"] } },
        { "type": "switcher", "text": "One name", "storage": { "path": ["bTop"] } },
        { "type": "slider", "text": "Time of day", "storage": { "global": "Fallout4.esm|38" } },
        { "type": "switcher", "text": "A property", "storage": { "form": "MyMod.esp|800", "scriptName": "MyMod:Config", "propertyName": "bOn" } }
    )"));
    Lines ids;
    for (const auto& setting : settingsOf(mod)) {
        ids.push_back(setting.id);
    }
    CHECK(ids == Lines{ "playerHeight", "bEnabled:Holsters", "bLeft:Holsters", "bLeft:body.hands", "bTop", "Fallout4.esm|38", "bOn:MyMod.esp|800" });
}

TEST_CASE("ConfigSpecLoader: two settings whose ids are the same, in any case, are both kept")
{
    const auto mod = loaded(configSpecOf(R"(
        { "type": "switcher", "text": "First", "storage": { "path": ["Main", "bOn"] } },
        { "id": "BON:MAIN", "type": "switcher", "text": "Second", "storage": { "path": ["Other", "bOn"] } },
        { "id": "own", "type": "switcher", "text": "Third", "storage": { "path": ["Main", "bOn"] } }
    )"));
    Lines ids;
    for (const auto& setting : settingsOf(mod)) {
        ids.push_back(setting.id);
    }
    CHECK(ids == Lines{ "bOn:Main", "BON:MAIN", "own" });
}

TEST_CASE("ConfigSpecLoader: a setting takes its own applies, or else the nearest above it")
{
    const auto mod = loaded(R"({
        "minSchemaVersion": 1, "modName": "MyMod", "applies": "live",
        "files": { "main": { "format": "toml", "root": "myGames", "path": "MyMod.toml" } },
        "defaultFile": "main",
        "content": [
            { "type": "switcher", "text": "Config spec's", "storage": { "path": ["Main", "a"] } },
            { "type": "switcher", "text": "Own", "applies": "restart", "storage": { "path": ["Main", "b"] } },
            {
                "type": "section", "text": "Hooks", "applies": "save",
                "content": [
                    { "type": "switcher", "text": "Section's", "storage": { "path": ["Main", "c"] } },
                    { "type": "section", "text": "Inner", "content": [ { "type": "switcher", "text": "Outer section's", "storage": { "path": ["Main", "d"] } } ] },
                    { "type": "switcher", "text": "Own in a section", "applies": "live", "storage": { "path": ["Main", "e"] } }
                ]
            },
            { "type": "switcher", "text": "Not one", "applies": "never", "storage": { "path": ["Main", "f"] } },
            { "type": "switcher", "text": "Own unknown", "applies": "unknown", "storage": { "path": ["Main", "i"] } }
        ],
        "pages": [
            {
                "pageDisplayName": "Advanced", "applies": "restart",
                "content": [ { "type": "switcher", "text": "Page's", "storage": { "path": ["Main", "g"] } } ],
                "pages": [ { "pageDisplayName": "Deeper", "content": [ { "type": "switcher", "text": "Page above's", "storage": { "path": ["Main", "h"] } } ] } ]
            }
        ]
    })");
    const auto name = [](const model::Applies applies) {
        switch (applies) {
        case model::Applies::Unknown:
            return "unknown";
        case model::Applies::Live:
            return "live";
        case model::Applies::Save:
            return "save";
        case model::Applies::Restart:
            return "restart";
        }
        return "?";
    };
    Lines applies;
    for (const auto& setting : settingsOf(mod)) {
        applies.push_back(setting.text + " " + name(setting.applies));
    }
    CHECK(applies == Lines{ "Config spec's live",
                         "Own restart",
                         "Section's save",
                         "Outer section's save",
                         "Own in a section live",
                         "Not one live",
                         "Own unknown unknown",
                         "Page's restart",
                         "Page above's restart" });
}

TEST_CASE("ConfigSpecLoader: a valueType that is no type is a warning, and the setting has the type of its control")
{
    const auto mod = loaded(configSpecOf(R"(
        { "type": "textinput", "valueType": "flot", "text": "Height", "default": "1.5", "storage": { "path": ["Main", "fHeight"] } },
        { "type": "slider", "valueType": 2, "text": "Scale", "default": 1.0, "storage": { "path": ["Main", "fScale"] } }
    )"));
    CHECK(problemLines(mod) == Lines{ "W Height: Its valueType \"flot\" is not bool, int, float or string: it is read as string. [Height] content[0].valueType",
                                   "W Scale: Its valueType 2 is not bool, int, float or string: it is read as float. [Scale] content[1].valueType" });
    const auto settings = settingsOf(mod);
    REQUIRE(settings.size() == 2);
    CHECK(settings[0].valueType == model::StoredType::String);
    CHECK(settings[1].valueType == model::StoredType::Float);
}

TEST_CASE("ConfigSpecLoader: with the folders of its files, a setting whose path its file does not have is a warning")
{
    const test_files::TempFolder folder;
    folder.write("MyGames/Mods_Config/MyMod/MyMod.toml", "[Main]\nfScale = 1.0\n");
    folder.write("Data/MyMod.ini", "[Main]\nbOn=1\n");
    const auto text = R"({
        "minSchemaVersion": 1, "modName": "MyMod", "applies": "save",
        "files": {
            "main": { "format": "toml", "root": "myGames", "path": "Mods_Config/MyMod/MyMod.toml" },
            "ini": { "format": "ini", "root": "data", "path": "MyMod.ini" },
            "later": { "format": "toml", "root": "myGames", "path": "Mods_Config/MyMod/later.toml" }
        },
        "defaultFile": "main",
        "content": [
            { "type": "slider", "text": "Scale", "default": 1.0, "storage": { "path": ["Main", "fScale"] } },
            { "type": "slider", "text": "Typo", "default": 1.0, "storage": { "path": ["Mian", "fScale"] } },
            { "type": "slider", "text": "Shown", "readOnly": true, "storage": { "path": ["Main", "fShown"] } },
            { "type": "switcher", "text": "On", "default": true, "storage": { "file": "ini", "section": "Main", "key": "bOn" } },
            { "type": "switcher", "text": "Off", "default": true, "storage": { "file": "ini", "section": "Main", "key": "bOff" } },
            { "type": "switcher", "text": "Not there yet", "default": true, "storage": { "file": "later", "path": ["bOn"] } }
        ]
    })";
    const auto mod = spec::loadConfigSpec(text, {}, {}, { .data = folder.path / "Data", .myGames = folder.path / "MyGames" });
    REQUIRE(mod);
    CHECK(
        problemLines(*mod) == Lines{ "W Typo: Its file \"main\" has no value Mian.fScale: it shows its default, which a Save writes once it is changed. [Typo] content[1].storage",
                                  "W Shown: Its file \"main\" has no value Main.fShown: it shows none. [Shown] content[2].storage",
                                  "W Off: Its file \"ini\" has no value bOff of [Main]: it shows its default, which a Save writes once it is changed. [Off] content[4].storage" });

    // with no folders, the files are not read
    const auto alone = spec::loadConfigSpec(text);
    REQUIRE(alone);
    CHECK(alone->problems.empty());
}

TEST_CASE("ConfigSpecLoader: a setting omits its default by its own omitDefault or its file's, and its file missing its value is then no warning")
{
    const test_files::TempFolder folder;
    folder.write("MyGames/MyMod.toml", "[Main]\nfScale = 1.0\n");
    folder.write("MyGames/MyMod_Developer.toml", "");
    const auto text = R"({
        "minSchemaVersion": 1, "modName": "MyMod", "applies": "live",
        "files": {
            "main": { "format": "toml", "root": "myGames", "path": "MyMod.toml" },
            "dev": { "format": "toml", "root": "myGames", "path": "MyMod_Developer.toml", "omitDefault": true }
        },
        "defaultFile": "main",
        "content": [
            { "type": "slider", "text": "Own", "default": 1.0, "omitDefault": true, "storage": { "path": ["Main", "fMissing"] } },
            { "type": "slider", "text": "Dev", "default": 1.0, "storage": { "file": "dev", "path": ["fA"] } },
            { "type": "slider", "text": "Kept", "default": 1.0, "omitDefault": false, "storage": { "file": "dev", "path": ["fB"] } },
            { "type": "slider", "text": "Plain", "default": 1.0, "storage": { "path": ["Main", "fScale"] } },
            { "type": "slider", "text": "Shown", "readOnly": true, "storage": { "file": "dev", "path": ["fShown"] } },
            { "type": "slider", "text": "Global", "default": 1.0, "storage": { "global": "MyMod.esp|800" } }
        ]
    })";
    const auto mod = spec::loadConfigSpec(text, {}, {}, { .data = folder.path / "Data", .myGames = folder.path / "MyGames" });
    REQUIRE(mod);
    CHECK(mod->files.at("dev").omitDefault);
    CHECK_FALSE(mod->files.at("main").omitDefault);

    // a read-only setting and one kept elsewhere never omit a default: the file's is ignored for them
    const auto settings = settingsOf(*mod);
    REQUIRE(settings.size() == 6);
    CHECK(settings[0].omitDefault);
    CHECK(settings[1].omitDefault);
    CHECK_FALSE(settings[2].omitDefault);
    CHECK_FALSE(settings[3].omitDefault);
    CHECK_FALSE(settings[4].omitDefault);
    CHECK_FALSE(settings[5].omitDefault);
    CHECK(problemLines(*mod) == Lines{ "W Kept: Its file \"dev\" has no value fB: it shows its default, which a Save writes once it is changed. [Kept] content[2].storage",
                                    "W Shown: Its file \"dev\" has no value fShown: it shows none. [Shown] content[4].storage" });
}

TEST_CASE("ConfigSpecLoader: an omitDefault of a setting that cannot omit its default, or that is not true or false, is reported and left out")
{
    const auto mod = loaded(configSpecOf(R"(
        { "type": "slider", "text": "Global", "default": 1.0, "omitDefault": true, "storage": { "global": "MyMod.esp|800" } },
        { "type": "slider", "text": "Shown", "readOnly": true, "omitDefault": true, "storage": { "path": ["Main", "fShown"] } },
        { "type": "switcher", "text": "No default", "omitDefault": true, "storage": { "path": ["Main", "bNo"] } },
        { "type": "slider", "text": "Word", "default": 1.0, "omitDefault": "yes", "storage": { "path": ["Main", "fWord"] } }
    )"));
    for (const auto& setting : settingsOf(mod)) {
        CHECK_FALSE(setting.omitDefault);
    }
    CHECK(problemLines(mod) == Lines{ "W Global: Its omitDefault is ignored: it is not kept in a file. [Global] content[0].omitDefault",
                                   "W Shown: Its omitDefault is ignored: it is read-only. [Shown] content[1].omitDefault",
                                   "E No default: It has no default of the type of its value: it has no Reset. [No default] content[2]",
                                   "W No default: Its omitDefault is ignored: it has no default. [No default] content[2].omitDefault",
                                   "W Word: Its omitDefault is not true or false: it is left out. [Word] content[3].omitDefault" });

    // a choice whose options set other settings: they omit their own defaults or not
    auto quality = std::string(QUALITY);
    const std::string text = R"("text": "Quality",)";
    quality.replace(quality.find(text), text.size(), R"("text": "Quality", "omitDefault": true,)");
    const auto sets = loaded(configSpecOf(quality));
    CHECK(problemLines(sets) ==
          Lines{ "W Quality: Its omitDefault is ignored: its options set other settings, which omit their own defaults or not. [Quality] content[0].omitDefault" });
}

TEST_CASE("ConfigSpecLoader: the root, a page and a section pass omitDefault down, the nearest first, over the file's")
{
    const auto mod = loaded(R"({
        "minSchemaVersion": 1, "modName": "MyMod", "applies": "live", "omitDefault": true,
        "files": {
            "main": { "format": "toml", "root": "myGames", "path": "MyMod.toml" },
            "dev": { "format": "toml", "root": "myGames", "path": "Dev.toml", "omitDefault": true }
        },
        "defaultFile": "main",
        "content": [ { "type": "switcher", "text": "Root", "default": true, "storage": { "path": ["a"] } } ],
        "pages": [ {
            "pageDisplayName": "Page", "omitDefault": false,
            "content": [
                { "type": "switcher", "text": "Page", "default": true, "storage": { "path": ["b"] } },
                { "type": "section", "text": "Section", "omitDefault": true, "content": [
                    { "type": "switcher", "text": "Section", "default": true, "storage": { "path": ["c"] } },
                    { "type": "switcher", "text": "Own", "default": true, "omitDefault": false, "storage": { "path": ["d"] } },
                    { "type": "switcher", "text": "Read-only", "readOnly": true, "storage": { "path": ["e"] } }
                ] },
                { "type": "switcher", "text": "Dev file", "default": true, "storage": { "file": "dev", "path": ["f"] } }
            ]
        } ]
    })");
    // one from above is ignored where the setting cannot omit its default, with no warning
    CHECK(mod.problems.empty());
    Lines omitting;
    for (const auto& setting : settingsOf(mod)) {
        if (setting.omitDefault) {
            omitting.push_back(setting.text);
        }
    }
    CHECK(omitting == Lines{ "Root", "Section" });
}

TEST_CASE("ConfigSpecLoader: a file's omitDefault that is not true or false is reported and left out")
{
    const auto mod = loaded(R"({
        "minSchemaVersion": 1, "modName": "MyMod", "applies": "live",
        "files": { "main": { "format": "toml", "root": "myGames", "path": "MyMod.toml", "omitDefault": 1 } },
        "content": [ { "type": "spacer" } ]
    })");
    CHECK_FALSE(mod.files.at("main").omitDefault);
    CHECK(problemLines(mod) == Lines{ "W spec.json: Its omitDefault is not true or false: it is left out. [] files.main.omitDefault" });
}

TEST_CASE("ConfigSpecLoader: a stepper whose options set other settings is kept in them, with no default or applies of its own")
{
    const auto mod = loaded(configSpecOf(QUALITY));
    CHECK(mod.problems.empty());
    const auto& quality = std::get<model::Setting>(mod.frontPage.items[0]);
    CHECK(model::setsOthers(quality));
    CHECK(quality.applies == model::Applies::Unknown);
    CHECK_FALSE(quality.defaultValue);
    REQUIRE(quality.valueOptions.options.size() == 2);

    // each value is the setting's own, named by its id as its config spec has it, which is read in any case; a transform
    // has the parts it names
    const auto& low = quality.valueOptions.options[0];
    CHECK(low.sets.size() == 2);
    CHECK(setOn(low, "fScale") == model::Value(0.5f));
    CHECK(setOn(low, "zone") == model::Value("{ pos = [0.0, 0.0, 12.0] }"s));
    const auto& high = quality.valueOptions.options[1];
    CHECK(setOn(high, "fScale") == model::Value(1.5f));
    CHECK(setOn(high, "sPlace") == model::Value("chest"s));
}

TEST_CASE("ConfigSpecLoader: what an option sets that is not a setting's value is left out and reported, and so is a choice's own storage")
{
    const auto mod = loaded(configSpecOf(R"(
        { "type": "stepper", "valueType": "string", "id": "quality", "text": "Quality", "default": "low", "applies": "save", "valueOptions": { "options": [
            { "value": "low", "text": "Low", "sets": { "fScale": 2.5, "sPlace": "belt", "nothing": 1, "other": "a", "zone": 3 } },
            { "value": "high", "text": "High", "sets": { "fScale": "high" } },
            { "value": "none", "text": "None" } ] } },
        { "type": "stepper", "valueType": "string", "id": "other", "text": "Other", "valueOptions": { "options": [
            { "value": "a", "text": "A", "sets": { "fScale": 1.0 } } ] } },
        { "type": "dropdown", "valueType": "string", "text": "Kept", "default": "a", "storage": { "path": ["Main", "sKept"] }, "valueOptions": { "options": [
            { "value": "a", "text": "A", "sets": { "fScale": 1.0 } } ] } },
        { "type": "slider", "valueType": "float", "id": "fScale", "text": "Scale", "default": 1.0, "valueOptions": { "min": 0.5, "max": 1.5, "step": 0.05 },
          "storage": { "path": ["Main", "fScale"] } },
        { "type": "dropdown", "valueType": "string", "id": "sPlace", "text": "Place", "default": "hip",
          "valueOptions": { "options": [ { "value": "hip", "text": "Hip" }, { "value": "chest", "text": "Chest" } ] }, "storage": { "path": ["Main", "sPlace"] } },
        { "type": "cf.transform", "id": "zone", "text": "Zone", "default": { "scale": 1.0 }, "storage": { "path": ["Main", "tZone"] } })"));
    Lines messages;
    for (const auto& problem : mod.problems) {
        messages.push_back(problem.message);
    }
    std::ranges::sort(messages);
    Lines expected{ "Its default is left out: it is the option whose sets are the defaults of their settings.",
        "Its applies is left out: each setting its options set takes effect as its own says.",
        "It has a storage, and options that set other settings: its options' sets are ignored.",
        "Its value 2.5 is not one of the setting's values: outside its limits, of a length it does not allow, or none of its options: it is left out.",
        "\"nothing\" names no setting: it is left out.",
        "\"other\" is a choice that sets other settings, which no option sets: it is left out.",
        "Its value \"belt\" is not one of the setting's values: outside its limits, of a length it does not allow, or none of its options: it is left out.",
        "Its value 3 is not a table, an object as the setting's default is: it is left out.",
        "Its value \"high\" is not of the setting's type: it is left out.",
        "It sets no setting: picking it changes nothing, and the row never shows it." };
    std::ranges::sort(expected);
    CHECK(messages == expected);

    // the options keep what is right, and a choice with a storage is as any
    const auto settings = settingsOf(mod);
    CHECK(settings[0].valueOptions.options[0].sets.empty());
    CHECK(settings[0].valueOptions.options.size() == 3);
    CHECK_FALSE(model::setsOthers(settings[2]));
    CHECK(std::holds_alternative<model::FileStorage>(settings[2].storage));
}

TEST_CASE("ConfigSpecLoader: an option's alsoSets are read as its sets are, and an option with alsoSets alone is never the value")
{
    const auto mod = loaded(configSpecOf(R"(
        { "type": "stepper", "valueType": "string", "id": "quality", "text": "Quality", "valueOptions": { "options": [
            { "value": "low", "text": "Low", "sets": { "fScale": 0.5 }, "alsoSets": { "zone": { "pos": [0, 0, 12] }, "nothing": 1 } },
            { "value": "near", "text": "Near", "alsoSets": { "zone": { "pos": [0, 0, 4] } } } ] } },
        { "type": "slider", "valueType": "float", "id": "fScale", "text": "Scale", "default": 1.0, "valueOptions": { "min": 0.5, "max": 1.5, "step": 0.05 },
          "storage": { "path": ["Main", "fScale"] } },
        { "type": "cf.transform", "id": "zone", "text": "Zone", "default": { "scale": 1.0 }, "storage": { "path": ["Main", "tZone"] } })"));
    Lines messages;
    for (const auto& problem : mod.problems) {
        messages.push_back(problem.message);
    }
    std::ranges::sort(messages);
    CHECK(messages == Lines{ "\"nothing\" names no setting: it is left out.", "It has no sets: the row never shows it, and picking it sets only its alsoSets." });

    // a choice of alsoSets alone is kept in the settings too
    const auto& quality = std::get<model::Setting>(mod.frontPage.items[0]);
    CHECK(model::setsOthers(quality));
    const auto& low = quality.valueOptions.options[0];
    CHECK(setOn(low, "fScale") == model::Value(0.5f));
    REQUIRE(low.alsoSets.size() == 1);
    CHECK(low.alsoSets[0] == model::OptionSet{ .setting = "zone", .value = "{ pos = [0.0, 0.0, 12.0] }"s });
    const auto& closer = quality.valueOptions.options[1];
    CHECK(closer.sets.empty());
    CHECK(closer.alsoSets.size() == 1);
}

TEST_CASE("ConfigSpecLoader: a cf.color with parts is kept in the sliders they name, which are hidden, and has an alpha with an a")
{
    const auto mod = loaded(configSpecOf(R"(
        { "type": "cf.color", "id": "glow", "text": "Glow", "parts": { "r": "GLOWR", "g": "glowG", "b": "glowB" } },
        { "type": "cf.color", "id": "tint", "text": "Tint", "parts": { "r": "tintR", "g": "tintG", "b": "tintB", "a": "tintA" } },
        { "type": "slider", "valueType": "int", "default": 0, "id": "glowR", "text": "Red", "valueOptions": { "min": 0, "max": 255 }, "storage": { "path": ["Main", "a"] } },
        { "type": "slider", "valueType": "int", "default": 0, "id": "glowG", "text": "Green", "valueOptions": { "min": 0, "max": 255 }, "storage": { "path": ["Main", "b"] } },
        { "type": "slider", "valueType": "int", "default": 0, "id": "glowB", "text": "Blue", "valueOptions": { "min": 0, "max": 255 }, "storage": { "path": ["Main", "c"] } },
        { "type": "slider", "default": 0.5, "id": "tintR", "text": "Red", "valueOptions": { "min": 0, "max": 1 }, "storage": { "path": ["Main", "d"] } },
        { "type": "slider", "default": 0.5, "id": "tintG", "text": "Green", "valueOptions": { "min": 0, "max": 1 }, "storage": { "path": ["Main", "e"] } },
        { "type": "slider", "default": 0.5, "id": "tintB", "text": "Blue", "valueOptions": { "min": 0, "max": 1 }, "storage": { "path": ["Main", "f"] } },
        { "type": "slider", "default": 0.5, "id": "tintA", "text": "Alpha", "valueOptions": { "min": 0, "max": 1 }, "storage": { "path": ["Main", "g"] } }
    )"));
    CHECK(mod.problems.empty());
    const auto settings = settingsOf(mod);
    REQUIRE(settings.size() == 9);
    CHECK(std::get<model::ColorPartsStorage>(settings[0].storage).parts == std::vector<std::string>{ "glowR", "glowG", "glowB" });
    CHECK_FALSE(settings[0].valueOptions.alpha);
    CHECK(settings[0].tableForm == model::TableForm::IniLine);
    CHECK_FALSE(settings[0].visibleWhen);
    CHECK(std::get<model::ColorPartsStorage>(settings[1].storage).parts.size() == 4);
    CHECK(settings[1].valueOptions.alpha);
    for (std::size_t part = 2; part < settings.size(); ++part) {
        INFO(settings[part].id);
        REQUIRE(settings[part].visibleWhen);
        CHECK(settings[part].visibleWhen->kind == model::Condition::Kind::Any);
        CHECK(settings[part].visibleWhen->conditions.empty());
    }
}

TEST_CASE("ConfigSpecLoader: parts that name no slider with a range are reported, and the color is not available")
{
    const auto mod = loaded(configSpecOf(R"(
        { "type": "cf.color", "text": "No blue", "default": { "rgb": [1, 2, 3] }, "applies": "live", "parts": { "r": "red", "g": "green" } },
        { "type": "cf.color", "text": "Wrong parts", "parts": { "r": "nothing", "g": "flag", "b": "flat", "x": "red" } },
        { "type": "cf.color", "text": "Shared", "parts": { "r": "red", "g": "green", "b": "blue" } },
        { "type": "cf.color", "text": "Again", "parts": { "r": "red", "g": "green", "b": "blue" } },
        { "type": "cf.color", "text": "Kept", "default": { "rgb": [1, 2, 3] }, "parts": { "r": "red", "g": "green", "b": "blue" }, "storage": { "path": ["Main", "cKept"] } },
        { "type": "slider", "default": 0.5, "text": "Not a color", "parts": { "r": "red" }, "storage": { "path": ["Main", "h"] } },
        { "type": "switcher", "default": false, "id": "flag", "text": "Flag", "storage": { "path": ["Main", "a"] } },
        { "type": "slider", "default": 0.5, "id": "flat", "text": "Flat", "storage": { "path": ["Main", "b"] } },
        { "type": "slider", "default": 0, "id": "red", "text": "Red", "valueOptions": { "min": 0, "max": 255 }, "storage": { "path": ["Main", "c"] } },
        { "type": "slider", "default": 0, "id": "green", "text": "Green", "valueOptions": { "min": 0, "max": 255 }, "storage": { "path": ["Main", "d"] } },
        { "type": "slider", "default": 0, "id": "blue", "text": "Blue", "valueOptions": { "min": 0, "max": 255 }, "storage": { "path": ["Main", "e"] } }
    )"));
    for (const auto& line : problemLines(mod)) {
        UNSCOPED_INFO(line);
    }
    Lines messages;
    for (const auto& problem : mod.problems) {
        messages.push_back(problem.message);
    }
    std::ranges::sort(messages);
    Lines expected{ "Its default is left out: it is the color of its sliders' defaults.",
        "Its applies is left out: each of its sliders takes effect as its own says.",
        "It has no part \"b\": it is not available.",
        "\"nothing\" names no setting: it is not available.",
        "\"flag\" is not a slider: it is not available.",
        "\"flat\" is a slider with no range from its min to its max: it is not available.",
        "Unknown field \"x\": it is not read.",
        "\"red\" is a part of another color: it is not available.",
        "\"green\" is a part of another color: it is not available.",
        "\"blue\" is a part of another color: it is not available.",
        "It has a storage, and parts, the sliders it is kept in: its parts are ignored.",
        "Only a cf.color has parts, the sliders it is kept in: its parts are ignored." };
    std::ranges::sort(expected);
    CHECK(messages == expected);

    const auto settings = settingsOf(mod);
    for (const auto place : { 0, 1, 3 }) {
        INFO(settings[place].text);
        CHECK(std::get<model::UnknownStorage>(settings[place].storage).type == "color parts");
    }
    CHECK(std::holds_alternative<model::ColorPartsStorage>(settings[2].storage));
    CHECK(std::holds_alternative<model::FileStorage>(settings[4].storage));
}
