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

TEST_CASE("ConfigSpecLoader: a choice's options are texts that store their place, or objects with the value each stores and an optional help")
{
    model::Translations translations;
    translations.add("$MyMod_Offhand", "Offhand");
    const auto mod = loaded(configSpecOf(R"(
        { "type": "dropdown", "valueType": "int", "text": "Location", "default": 1, "valueOptions": { "options": ["Head", "$MyMod_Offhand", "Primary hand"] },
          "storage": { "path": ["Main", "iLocation"] } },
        { "type": "dropdown", "valueType": "string", "text": "Mode", "default": "hybrid",
          "valueOptions": { "options": [
              { "value": "physical", "text": "Physical", "help": "Use direct VR interaction." },
              { "value": "hybrid", "text": { "token": "$MyMod_Hybrid", "fallback": "Hybrid" } }
          ] },
          "storage": { "path": ["Main", "sMode"] } },
        { "type": "stepper", "valueType": "float", "text": "Scale", "default": 1.0,
          "valueOptions": { "options": [ { "value": 0.5, "text": "Small" }, { "value": 1, "text": "Normal" } ] },
          "storage": { "path": ["Main", "fScale"] } }
    )"),
        translations);
    const auto settings = settingsOf(mod);
    REQUIRE(settings.size() == 3);
    using Options = std::vector<model::Option>;
    CHECK(settings[0].valueOptions.options == Options{ { 0, "Head" }, { 1, "Offhand" }, { 2, "Primary hand" } });
    CHECK(settings[0].defaultValue == model::Value(1));
    CHECK(settings[1].valueOptions.options == Options{ { std::string("physical"), "Physical", "Use direct VR interaction." }, { std::string("hybrid"), "Hybrid" } });
    CHECK(settings[1].defaultValue == model::Value(std::string("hybrid")));

    // a whole number is a value of a float
    CHECK(settings[2].valueOptions.options == Options{ { 0.5f, "Small" }, { 1.0f, "Normal" } });
}

TEST_CASE("ConfigSpecLoader: an option that is not one of its choice's type is left out")
{
    const auto mod = loaded(configSpecOf(R"(
        { "type": "dropdown", "valueType": "string", "text": "Mode", "default": "a",
          "valueOptions": { "options": [ "Short", { "value": 1, "text": "A number" }, { "text": "No value" }, 7, { "value": "a", "text": "A" } ] },
          "storage": { "path": ["Main", "sMode"] } },
        { "type": "stepper", "valueType": "int", "text": "Count", "default": 0,
          "valueOptions": { "options": [ "One", { "value": 1.5, "text": "Half" }, "Two" ] },
          "storage": { "path": ["Main", "iCount"] } }
    )"));
    const auto settings = settingsOf(mod);
    using Options = std::vector<model::Option>;
    CHECK(settings[0].valueOptions.options == Options{ { std::string("a"), "A" } });

    // a text keeps its place in the list
    CHECK(settings[1].valueOptions.options == Options{ { 0, "One" }, { 2, "Two" } });
}

TEST_CASE("ConfigSpecLoader: a choice has the options of the shared list it names, in any case")
{
    const auto mod = loaded(R"({
        "minSchemaVersion": 1, "modName": "MyMod", "applies": "live",
        "files": { "main": { "format": "toml", "root": "myGames", "path": "Mods_Config/MyMod/MyMod.toml" } },
        "defaultFile": "main",
        "sharedLists": {
            "hands": ["Left", "Right"],
            "Modes": [ { "value": "physical", "text": "Physical", "help": "Direct VR interaction." }, { "value": "hybrid", "text": "Hybrid" } ]
        },
        "content": [
            { "type": "stepper", "text": "Flashlight hand", "default": 0, "valueOptions": { "sharedOptions": "hands" }, "storage": { "path": ["Flashlight", "iHand"] } },
            { "type": "dropdown", "text": "Pip-Boy hand", "default": 1, "valueOptions": { "sharedOptions": "Hands" }, "storage": { "path": ["PipBoy", "iHand"] } },
            { "type": "dropdown", "valueType": "string", "text": "Mode", "default": "hybrid", "valueOptions": { "sharedOptions": "modes" }, "storage": { "path": ["Main", "sMode"] } },
            { "type": "dropdown", "text": "No such list", "default": 0, "valueOptions": { "sharedOptions": "feet" }, "storage": { "path": ["Main", "iFoot"] } },
            { "type": "dropdown", "text": "No options", "default": 0, "storage": { "path": ["Main", "iNone"] } }
        ]
    })");
    const auto settings = settingsOf(mod);
    REQUIRE(settings.size() == 5);
    using Options = std::vector<model::Option>;
    const Options hands{ { 0, "Left" }, { 1, "Right" } };
    CHECK(settings[0].valueOptions.options == hands);
    CHECK(settings[1].valueOptions.options == hands);
    CHECK(settings[2].valueOptions.options == Options{ { std::string("physical"), "Physical", "Direct VR interaction." }, { std::string("hybrid"), "Hybrid" } });
    CHECK(settings[3].valueOptions.options.empty());
    CHECK(settings[4].valueOptions.options.empty());
}

TEST_CASE("ConfigSpecLoader: a choice reads its options in the game from a FormList or a property, by their place or their text")
{
    const auto mod = loaded(configSpecOf(R"(
        { "type": "dropdown", "text": "Settlement", "default": 3, "valueOptions": { "listFromForm": "MyMod.esp|0x900" }, "storage": { "path": ["Main", "a"] } },
        { "type": "stepper", "valueType": "string", "text": "Name", "default": "Sanctuary",
          "valueOptions": { "listFromProperty": { "form": "MyMod.esp|800", "scriptName": "MyMod:Config", "propertyName": "Names" } }, "storage": { "path": ["Main", "b"] } },
        { "type": "dropdown", "text": "Any script", "default": 0, "valueOptions": { "listFromProperty": { "form": "MyMod.esp|800", "propertyName": "Names" } },
          "storage": { "path": ["Main", "c"] } },
        { "type": "dropdown", "text": "Own first", "default": 0, "valueOptions": { "options": ["Own"], "listFromForm": "MyMod.esp|900" }, "storage": { "path": ["Main", "d"] } },
        { "type": "dropdown", "valueType": "float", "text": "Of a float", "default": 0.0, "valueOptions": { "listFromForm": "MyMod.esp|900" }, "storage": { "path": ["Main", "e"] } },
        { "type": "dropdown", "text": "No form", "default": 0, "valueOptions": { "listFromForm": "MyMod.esp" }, "storage": { "path": ["Main", "f"] } },
        { "type": "stepper", "text": "No name", "default": 0, "valueOptions": { "listFromProperty": { "form": "MyMod.esp|800" } }, "storage": { "path": ["Main", "g"] } }
    )"));
    const auto settings = settingsOf(mod);
    REQUIRE(settings.size() == 7);

    CHECK(settings[0].valueOptions.gameOptions == model::GameOptions{ .plugin = "MyMod.esp", .formId = 0x900, .script = {}, .property = {}, .storesText = false });
    CHECK(settings[0].valueOptions.options.empty());
    CHECK(settings[1].valueOptions.gameOptions == model::GameOptions{ .plugin = "MyMod.esp", .formId = 0x800, .script = "MyMod:Config", .property = "Names", .storesText = true });
    CHECK(settings[2].valueOptions.gameOptions->script.empty());

    // its default is any value of its type, as its options are not known until the game is read
    CHECK(settings[0].defaultValue == model::Value(3));
    CHECK(settings[1].defaultValue == model::Value(std::string("Sanctuary")));

    // its own options come first, and a list that is not one has no options
    CHECK_FALSE(settings[3].valueOptions.gameOptions);
    CHECK(settings[3].valueOptions.options.size() == 1);
    for (std::size_t i = 4; i < settings.size(); ++i) {
        INFO(settings[i].text);
        CHECK_FALSE(settings[i].valueOptions.gameOptions);
        CHECK(settings[i].valueOptions.options.empty());
    }
}

TEST_CASE("ConfigSpecLoader: a choice with listFromClientCallback takes its options from its mod's client, with any default of its type")
{
    const auto mod = loaded(configSpecOf(R"(
        { "type": "dropdown", "valueType": "string", "text": "Config spec", "default": "anything", "valueOptions": { "listFromClientCallback": true },
          "storage": { "path": ["a"] } },
        { "type": "stepper", "valueType": "int", "text": "Not true", "default": 0, "valueOptions": { "listFromClientCallback": false },
          "storage": { "path": ["b"] } }
    )"));
    const auto& client = std::get<model::Setting>(mod.frontPage.items[0]);
    CHECK(client.valueOptions.clientOptions);
    CHECK(client.valueOptions.options.empty());
    CHECK(client.defaultValue == model::Value("anything"s));
    CHECK_FALSE(std::get<model::Setting>(mod.frontPage.items[1]).valueOptions.clientOptions);
    CHECK(problemLines(mod) == Lines{ "E Not true: It has no list of options, or names no shared list of its config spec: it has no options. [Not true] content[1].valueOptions" });
}

TEST_CASE("ConfigSpecLoader: a dropdown of a string lists the files of a folder, whose default is in it, and a folder with no files is a warning")
{
    const test_files::TempFolder folder;
    folder.write("Data/Textures/MyMod/Gobos/1_Solid.DDS", "");
    const auto text = configSpecOf(R"(
        { "type": "dropdown", "valueType": "string", "text": "Gobo", "default": "data\\Textures\\MyMod\\Gobos\\1_Solid.DDS",
          "valueOptions": { "listFromFolder": { "path": "Textures/MyMod/Gobos", "mask": "*.dds", "noneText": "No gobo" } }, "storage": { "path": ["Main", "a"] } },
        { "type": "dropdown", "valueType": "string", "text": "Presets", "default": "Mine.json",
          "valueOptions": { "listFromFolder": { "root": "myGames", "path": "Presets" } }, "storage": { "path": ["Main", "b"] } },
        { "type": "dropdown", "valueType": "string", "text": "Missing", "default": "data/Textures/MyMod/Gobos/9_Laser.dds",
          "valueOptions": { "listFromFolder": { "path": "Textures/MyMod/Gobos", "mask": "*.dds" } }, "storage": { "path": ["Main", "c"] } },
        { "type": "dropdown", "valueType": "string", "text": "Elsewhere", "default": "Textures\\Effects\\Gobo.dds",
          "valueOptions": { "listFromFolder": { "path": "Textures/MyMod/Gobos" } }, "storage": { "path": ["Main", "d"] } },
        { "type": "stepper", "valueType": "string", "text": "Stepper", "default": "a",
          "valueOptions": { "listFromFolder": { "path": "Textures" } }, "storage": { "path": ["Main", "e"] } },
        { "type": "dropdown", "valueType": "int", "text": "Int", "default": 0,
          "valueOptions": { "listFromFolder": { "path": "Textures" } }, "storage": { "path": ["Main", "f"] } },
        { "type": "dropdown", "valueType": "string", "text": "Up", "default": "a",
          "valueOptions": { "listFromFolder": { "path": "../Textures" } }, "storage": { "path": ["Main", "g"] } },
        { "type": "dropdown", "valueType": "string", "text": "Root", "default": "a",
          "valueOptions": { "listFromFolder": { "root": "game", "path": "Textures", "nonetext": "None" } }, "storage": { "path": ["Main", "h"] } }
    )");
    const auto mod = spec::loadConfigSpec(text, {}, {}, { .data = folder.path / "Data", .myGames = folder.path / "MyGames" });
    REQUIRE(mod);
    const auto settings = settingsOf(*mod);
    REQUIRE(settings.size() == 8);

    // its options are listed by its store: the loader keeps its folder, and its default as it is written
    CHECK(settings[0].valueOptions.folderOptions == model::FolderOptions{ .root = model::FileRoot::Data, .path = "Textures/MyMod/Gobos", .mask = "*.dds", .noneText = "No gobo" });
    CHECK(settings[0].valueOptions.options.empty());
    CHECK(settings[0].defaultValue == model::Value(R"(data\Textures\MyMod\Gobos\1_Solid.DDS)"s));
    CHECK(settings[1].valueOptions.folderOptions->root == model::FileRoot::MyGames);
    CHECK(settings[2].defaultValue);
    CHECK_FALSE(settings[3].defaultValue);
    for (std::size_t i = 4; i < settings.size(); ++i) {
        INFO(settings[i].text);
        CHECK_FALSE(settings[i].valueOptions.folderOptions);
    }
    CHECK(problemLines(*mod) ==
          Lines{ "W Presets: Presets is not there, or has no files: the setting is not available until it has one. [Presets] content[1].valueOptions.listFromFolder",
              "W Missing: Its default 9_Laser.dds is not in Textures\\MyMod\\Gobos: it is invalid until it is. [Missing] content[2].default",
              "E Elsewhere: Its default \"Textures\\Effects\\Gobo.dds\" is not in its folder Textures\\MyMod\\Gobos: it has no Reset. [Elsewhere] content[3].default",
              "E Stepper: A listFromFolder is of a dropdown whose valueType is string: it has no options. [Stepper] content[4].valueOptions.listFromFolder",
              "E Int: A listFromFolder is of a dropdown whose valueType is string: it has no options. [Int] content[5].valueOptions.listFromFolder",
              "E Up: Its folder is left out: its path '../Textures' goes up out of its folder. It has no options. [Up] content[6].valueOptions.listFromFolder",
              "E Root: Its root 'game' is not data or myGames: it has no options. [Root] content[7].valueOptions.listFromFolder",
              "W Root: Unknown field \"nonetext\": it is not read. Did you mean \"noneText\"? [Root] content[7].valueOptions.listFromFolder.nonetext" });

    // with no folders, a folder is not listed
    const auto alone = spec::loadConfigSpec(text);
    REQUIRE(alone);
    CHECK(alone->problems.size() == 6);
}
