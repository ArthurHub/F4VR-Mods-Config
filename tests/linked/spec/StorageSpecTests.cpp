#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <format>
#include <functional>
#include <sstream>
#include <unordered_set>

#include "TestConfigSpecs.h"
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

TEST_CASE("ConfigSpecLoader: a setting is kept in the file its storage names, or in the nearest defaultFile above it")
{
    const auto mod = loaded(R"({
        "minSchemaVersion": 1, "modName": "MyMod", "applies": "live",
        "files": {
            "main": { "format": "toml", "root": "myGames", "path": "Mods_Config/MyMod/MyMod.toml" },
            "body": { "format": "toml", "root": "myGames", "path": "Mods_Config/MyMod/body.toml" },
            "advanced": { "format": "toml", "root": "data", "path": "F4SE/Plugins/MyMod/advanced.toml" }
        },
        "defaultFile": "main",
        "content": [
            { "type": "switcher", "text": "Config spec's", "storage": { "path": ["Main", "a"] } },
            { "type": "switcher", "text": "Own", "storage": { "file": "advanced", "path": ["Main", "b"] } },
            {
                "type": "section", "text": "Body", "defaultFile": "body",
                "content": [
                    { "type": "switcher", "text": "Section's", "storage": { "path": ["Main", "c"] } },
                    { "type": "section", "text": "Inner", "content": [ { "type": "switcher", "text": "Outer section's", "storage": { "path": ["Main", "d"] } } ] }
                ]
            }
        ],
        "pages": [
            {
                "pageDisplayName": "Advanced", "defaultFile": "advanced",
                "content": [ { "type": "switcher", "text": "Page's", "storage": { "path": ["Main", "e"] } } ],
                "pages": [ { "pageDisplayName": "Deeper", "content": [ { "type": "switcher", "text": "Page above's", "storage": { "path": ["Main", "f"] } } ] } ]
            }
        ]
    })");
    Lines files;
    for (const auto& setting : settingsOf(mod)) {
        files.push_back(setting.text + " " + std::get<model::FileStorage>(setting.storage).file);
    }
    CHECK(files == Lines{ "Config spec's main", "Own advanced", "Section's body", "Outer section's body", "Page's advanced", "Page above's advanced" });

    REQUIRE(mod.files.size() == 3);
    CHECK(mod.files.at("main") == model::File{ .format = model::FileFormat::Toml, .root = model::FileRoot::MyGames, .path = "Mods_Config/MyMod/MyMod.toml" });
    CHECK(mod.files.at("advanced") == model::File{ .format = model::FileFormat::Toml, .root = model::FileRoot::Data, .path = "F4SE/Plugins/MyMod/advanced.toml" });
}

TEST_CASE("ConfigSpecLoader: a value is named by its path, or by its section and its key as an INI key is")
{
    const auto mod = loaded(configSpecOf(R"(
        { "type": "switcher", "text": "Path", "storage": { "path": ["body", "hands", "bLeft"] } },
        { "type": "switcher", "text": "Key", "storage": { "section": "Holsters", "key": "bEnabled" } },
        { "type": "switcher", "text": "Root key", "storage": { "path": ["bTop"] } },
        { "type": "switcher", "text": "File in another case", "storage": { "file": "MAIN", "path": ["Main", "bCase"] } }
    )"));
    const auto settings = settingsOf(mod);
    REQUIRE(settings.size() == 4);
    CHECK(settings[0].storage == fileValue("main", { "body", "hands", "bLeft" }));
    CHECK(settings[1].storage == fileValue("main", { "Holsters", "bEnabled" }));
    CHECK(settings[2].storage == fileValue("main", { "bTop" }));
    CHECK(settings[3].storage == fileValue("main", { "Main", "bCase" }));
}

TEST_CASE("ConfigSpecLoader: a setting is kept in a global, or in a property of a script in the type of its value")
{
    const auto mod = loaded(configSpecOf(R"(
        { "type": "slider", "text": "Time of day", "storage": { "global": "Fallout4.esm|38" } },
        { "type": "switcher", "text": "Global with 0x", "storage": { "global": "MyMod.esp|0x1234" } },
        { "type": "slider", "valueType": "int", "text": "Property", "storage": { "form": "MyMod.esp|800", "scriptName": "MyMod:Config", "propertyName": "iCount" } },
        { "type": "switcher", "text": "First script", "storage": { "form": "MyMod.esp|800", "propertyName": "bOn" } }
    )"));
    const auto settings = settingsOf(mod);
    REQUIRE(settings.size() == 4);
    CHECK(settings[0].storage == model::Storage(model::GlobalStorage{ .plugin = "Fallout4.esm", .formId = 0x38 }));
    CHECK(settings[1].storage == model::Storage(model::GlobalStorage{ .plugin = "MyMod.esp", .formId = 0x1234 }));
    CHECK(settings[2].storage ==
          model::Storage(model::PropertyStorage{ .plugin = "MyMod.esp", .formId = 0x800, .script = "MyMod:Config", .name = "iCount", .type = model::StoredType::Int }));
    CHECK(settings[3].storage == model::Storage(model::PropertyStorage{ .plugin = "MyMod.esp", .formId = 0x800, .script = {}, .name = "bOn", .type = model::StoredType::Bool }));

    // its id is its form, and a property's its name and its form
    CHECK(settings[0].id == "Fallout4.esm|38");
    CHECK(settings[1].id == "MyMod.esp|1234");
    CHECK(settings[2].id == "iCount:MyMod.esp|800");
    CHECK(settings[3].id == "bOn:MyMod.esp|800");
}

TEST_CASE("ConfigSpecLoader: a setting is kept in a state of its mod's client, by its name, which is its id")
{
    const auto mod = loaded(configSpecOf(R"(
        { "type": "dropdown", "valueType": "int", "text": "Pip-Boy", "default": 0, "valueOptions": { "options": ["Wrist", "Projected"] },
          "storage": { "clientState": "pipboyType" } },
        { "type": "slider", "text": "Height", "readOnly": true, "storage": { "clientState": "height" } },
        { "type": "switcher", "text": "No name", "default": true, "storage": { "clientState": "" } },
        { "type": "switcher", "text": "With an action", "default": true, "storage": { "clientState": "bOn" },
          "action": { "type": "RunConsoleCommand", "command": "tm" } }
    )"));
    const auto settings = settingsOf(mod);
    REQUIRE(settings.size() == 4);
    CHECK(settings[0].storage == model::Storage(model::ClientStateStorage{ .name = "pipboyType" }));
    CHECK(settings[0].id == "pipboyType");
    CHECK(settings[1].storage == model::Storage(model::ClientStateStorage{ .name = "height" }));
    CHECK(settings[2].storage == model::Storage(model::UnknownStorage{ .type = "clientState" }));
    CHECK(settings[2].id.empty());

    // an action runs before the client applies what a Save wrote, which is a warning, and it still runs
    CHECK(std::holds_alternative<model::ConsoleCommand>(settings[3].action));
    REQUIRE(mod.problems.size() == 2);
    CHECK(mod.problems[0].severity == model::Severity::Error);
    CHECK(mod.problems[0].message == "Its clientState names no state of its mod's client: it is not available.");
    CHECK(mod.problems[0].path == "content[2].storage");
    CHECK(mod.problems[1].severity == model::Severity::Warning);
    CHECK(mod.problems[1].message.starts_with("It is kept in a state of its mod's client"));
    CHECK(mod.problems[1].path == "content[3].action");
}

TEST_CASE("ConfigSpecLoader: a setting that names no file of its config spec, or no value, is kept where the menu does not read")
{
    const auto mod = loaded(R"({
        "minSchemaVersion": 1, "modName": "MyMod", "applies": "live",
        "files": {
            "main": { "format": "toml", "root": "myGames", "path": "MyMod.toml" },
            "ini": { "format": "ini", "root": "myGames", "path": "MyMod.ini" }
        },
        "content": [
            { "type": "switcher", "text": "No default file", "storage": { "path": ["Main", "a"] } },
            { "type": "switcher", "text": "No such file", "storage": { "file": "other", "path": ["Main", "b"] } },
            { "type": "switcher", "text": "No path", "storage": { "file": "main" } },
            { "type": "switcher", "text": "Empty name", "storage": { "file": "main", "path": ["Main", ""] } },
            { "type": "switcher", "text": "Path of three in an INI file", "storage": { "file": "ini", "path": ["Main", "Sub", "c"] } },
            { "type": "switcher", "text": "No storage" },
            { "type": "switcher", "text": "Global by its name", "storage": { "global": "GameHour" } },
            { "type": "switcher", "text": "Global of no text", "storage": { "global": 56 } },
            { "type": "switcher", "text": "Property with no name", "storage": { "form": "MyMod.esp|800" } },
            { "type": "switcher", "text": "Property with no id", "storage": { "form": "MyMod.esp", "propertyName": "bOn" } }
        ]
    })");
    Lines storages;
    for (const auto& setting : settingsOf(mod)) {
        const auto* unknown = std::get_if<model::UnknownStorage>(&setting.storage);
        storages.push_back(setting.text + (unknown ? " unknown " + unknown->type : " read"));
        // and it has no id from where its value is kept
        CHECK(setting.id.empty());
    }
    CHECK(storages == Lines{ "No default file unknown file",
                          "No such file unknown file",
                          "No path unknown file",
                          "Empty name unknown file",
                          "Path of three in an INI file unknown file",
                          "No storage unknown ",
                          "Global by its name unknown global",
                          "Global of no text unknown global",
                          "Property with no name unknown property",
                          "Property with no id unknown property" });
}

TEST_CASE("ConfigSpecLoader: a file that is not written as the schema has it is left out")
{
    const auto mod = loaded(R"({
        "minSchemaVersion": 1, "modName": "MyMod", "applies": "live",
        "files": {
            "toml": { "format": "toml", "root": "myGames", "path": "Mods_Config/MyMod/MyMod.toml" },
            "ini": { "format": "ini", "root": "data", "path": "F4SE/Plugins/MyMod.INI" },
            "json": { "format": "json", "root": "myGames", "path": "MyMod\\calibration.json" },
            "noFormat": { "root": "myGames", "path": "MyMod.toml" },
            "otherFormat": { "format": "yaml", "root": "myGames", "path": "MyMod.yaml" },
            "otherRoot": { "format": "toml", "root": "documents", "path": "MyMod.toml" },
            "absolute": { "format": "toml", "root": "data", "path": "C:/Games/MyMod.toml" },
            "fromTheTop": { "format": "toml", "root": "data", "path": "/MyMod.toml" },
            "goesUp": { "format": "toml", "root": "data", "path": "F4SE/../../MyMod.toml" },
            "otherExtension": { "format": "toml", "root": "data", "path": "MyMod.ini" },
            "noPath": { "format": "toml", "root": "data" },
            "notAnObject": "MyMod.toml"
        },
        "content": [ { "type": "switcher", "text": "Left out", "storage": { "file": "goesUp", "path": ["Main", "a"] } } ]
    })");
    Lines names;
    for (const auto& [name, file] : mod.files) {
        names.push_back(name);
    }
    CHECK(names == Lines{ "ini", "toml" });
    CHECK(mod.files.at("ini").format == model::FileFormat::Ini);
    CHECK(mod.files.at("toml").format == model::FileFormat::Toml);
    CHECK(std::get<model::UnknownStorage>(onlySetting(mod).storage).type == "file");
}

TEST_CASE("ConfigSpecLoader: a file can take its path from a state of its mod's client, and one that names no state is read from its path")
{
    const auto mod = loaded(R"({
        "minSchemaVersion": 1, "modName": "MyMod", "applies": "live",
        "files": {
            "main": { "format": "toml", "root": "myGames", "path": "Mods_Config/MyMod/MyMod.toml", "pathFromClientState": "configFile" },
            "noName": { "format": "ini", "root": "data", "path": "MyMod.ini", "pathFromClientState": 5 }
        },
        "content": [ { "type": "spacer" } ]
    })");
    CHECK(mod.files.at("main").pathFromClientState == "configFile");
    CHECK(mod.files.at("main").path == "Mods_Config/MyMod/MyMod.toml");
    CHECK(mod.files.at("noName").pathFromClientState.empty());
    REQUIRE(mod.problems.size() == 1);
    CHECK(mod.problems[0].severity == model::Severity::Warning);
    CHECK(mod.problems[0].path == "files.noName.pathFromClientState");
}
