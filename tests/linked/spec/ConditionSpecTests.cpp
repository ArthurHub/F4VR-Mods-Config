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

TEST_CASE("ConfigSpecLoader: a button is shown by its visibleWhen and enabled by its enabledWhen, with those of its sections")
{
    const auto mod = loaded(configSpecOf(R"(
        { "id": "a", "type": "switcher", "text": "A", "default": true, "storage": { "path": ["Main", "bA"] } },
        {
            "type": "section", "text": "Tools", "enabledWhen": { "setting": "a", "equals": true },
            "content": [
                { "type": "button", "text": "Hide the HUD", "action": { "type": "RunConsoleCommand", "command": "tm" },
                  "visibleWhen": { "setting": "a", "equals": false }, "enabledWhen": { "setting": "a", "notEquals": false } }
            ]
        }
    )"));
    using Condition = model::Condition;
    using Kind = Condition::Kind;
    const auto compared = [](const Kind kind, const bool on) {
        return Condition{ .kind = kind, .setting = "a", .conditions = {}, .offWhenNotAvailable = false, .values = { on } };
    };
    const auto& button = std::get<model::Button>(std::get<model::Group>(mod.frontPage.items[1]).items[0]);
    CHECK(button.visibleWhen == compared(Kind::Equals, false));
    CHECK(button.enabledWhen == Condition{ .kind = Kind::All, .setting = {}, .conditions = { compared(Kind::Equals, true), compared(Kind::NotEquals, false) } });
}

TEST_CASE("ConfigSpecLoader: page conditions cover child pages and their rows")
{
    const auto mod = loaded(R"({
        "minSchemaVersion": 1, "modName": "MyMod", "displayName": "My Mod", "applies": "save",
        "content": [ { "type": "text", "text": "Front" } ],
        "pages": [
            { "pageDisplayName": "Parent", "visibleWhen": { "clientState": "visible" }, "enabledWhen": { "clientState": "enabled" },
              "content": [ { "type": "text", "text": "Parent text" } ],
              "pages": [
                { "pageDisplayName": "Child", "visibleWhen": { "clientState": "childVisible" }, "enabledWhen": { "clientState": "childEnabled" },
                  "content": [ { "type": "button", "text": "Run", "action": { "type": "RunConsoleCommand", "command": "tm" } } ],
                  "pages": [ { "pageDisplayName": "Grandchild", "content": [ { "type": "text", "text": "Nested" } ] } ] }
              ] },
            { "pageDisplayName": "Other", "content": [ { "type": "text", "text": "Other" } ] }
        ]
    })");
    CHECK(mod.problems.empty());
    const auto gate = [](const std::string& name) {
        return model::Condition{ .kind = model::Condition::Kind::On, .setting = name, .source = model::Condition::Source::ClientState };
    };
    const auto& parent = mod.pages[0];
    const auto& child = parent.pages[0];
    CHECK(parent.visibleWhen == gate("visible"));
    CHECK(parent.enabledWhen == gate("enabled"));
    CHECK(std::get<model::Text>(parent.items[0]).visibleWhen == parent.visibleWhen);
    CHECK(child.visibleWhen == model::Condition{ .kind = model::Condition::Kind::All, .conditions = { gate("visible"), gate("childVisible") } });
    CHECK(child.enabledWhen == model::Condition{ .kind = model::Condition::Kind::All, .conditions = { gate("enabled"), gate("childEnabled") } });
    const auto& button = std::get<model::Button>(child.items[0]);
    CHECK(button.visibleWhen == child.visibleWhen);
    CHECK(button.enabledWhen == child.enabledWhen);
    CHECK(child.pages[0].visibleWhen == child.visibleWhen);
    CHECK(child.pages[0].enabledWhen == child.enabledWhen);
    CHECK_FALSE(mod.pages[1].visibleWhen);
    CHECK_FALSE(mod.pages[1].enabledWhen);
}

TEST_CASE("ConfigSpecLoader: malformed page conditions are reported and left out")
{
    const auto mod = loaded(R"({
        "minSchemaVersion": 1, "modName": "MyMod", "displayName": "My Mod", "applies": "save",
        "content": [ { "type": "text", "text": "Front" } ],
        "pages": [ { "pageDisplayName": "Page", "visibleWhen": true, "enabledWhen": {}, "content": [ { "type": "text", "text": "Shown" } ] } ]
    })");
    REQUIRE(mod.problems.size() == 2);
    CHECK_FALSE(mod.pages[0].visibleWhen);
    CHECK_FALSE(mod.pages[0].enabledWhen);
    CHECK(mod.problems[0].path == "pages[0].enabledWhen");
    CHECK(mod.problems[1].path == "pages[0].visibleWhen");
}

TEST_CASE("ConfigSpecLoader: a setting and a button have their warnings, each a text with its condition, in their order")
{
    model::Translations translations;
    translations.add("$MyMod_TooHigh", "Trop haut");
    const auto mod = loaded(configSpecOf(R"(
        { "id": "height", "type": "slider", "text": "Height", "default": 1.3, "valueOptions": { "min": 0.5, "max": 2.0, "step": 0.01 },
          "storage": { "path": ["Main", "fHeight"] },
          "warnings": [
            { "when": { "setting": "height", "greaterThan": 1.8 }, "text": "$MyMod_TooHigh" },
            { "when": { "setting": "lowered", "equals": true }, "text": { "token": "$MyMod_Low", "fallback": "Lowered." } }
          ] },
        { "id": "lowered", "type": "switcher", "text": "Lowered", "default": false, "storage": { "path": ["Main", "bLowered"] } },
        { "type": "button", "text": "Rebuild", "action": { "type": "RunConsoleCommand", "command": "tm" },
          "warnings": [ { "when": { "setting": "lowered", "equals": true }, "text": "Load the save again after it." } ] }
    )"),
        translations);
    using Condition = model::Condition;
    using Kind = Condition::Kind;
    const auto& height = std::get<model::Setting>(mod.frontPage.items[0]);
    REQUIRE(height.warnings.size() == 2);
    CHECK(height.warnings[0].text == "Trop haut");
    CHECK(height.warnings[0].when == Condition{ .kind = Kind::GreaterThan, .setting = "height", .conditions = {}, .offWhenNotAvailable = false, .values = { 1.8f } });
    CHECK(height.warnings[1].text == "Lowered.");
    CHECK(height.warnings[1].when == Condition{ .kind = Kind::Equals, .setting = "lowered", .conditions = {}, .offWhenNotAvailable = false, .values = { true } });
    CHECK(std::get<model::Setting>(mod.frontPage.items[1]).warnings.empty());
    const auto& button = std::get<model::Button>(mod.frontPage.items[2]);
    REQUIRE(button.warnings.size() == 1);
    CHECK(button.warnings[0].text == "Load the save again after it.");
}

TEST_CASE("ConfigSpecLoader: a warning with no text, or whose when is not a condition, is left out")
{
    const auto mod = loaded(configSpecOf(R"(
        { "id": "on", "type": "switcher", "text": "On", "default": true, "storage": { "path": ["Main", "bOn"] },
          "warnings": [
            { "when": { "setting": "on", "equals": true } },
            { "when": { "setting": "on", "equals": true }, "text": "" },
            { "text": "No when." },
            { "when": { "setting": "missing", "equals": true }, "text": "No such setting." },
            { "when": { "setting": "on", "equals": 1 }, "text": "Another type." },
            "Not an object.",
            { "when": { "setting": "on", "equals": true }, "text": "Kept." }
          ] },
        { "type": "switcher", "text": "Not a list", "default": true, "storage": { "path": ["Main", "bOther"] }, "warnings": { "text": "A warning." } }
    )"));
    const auto& on = std::get<model::Setting>(mod.frontPage.items[0]);
    REQUIRE(on.warnings.size() == 1);
    CHECK(on.warnings[0].text == "Kept.");
    CHECK(std::get<model::Setting>(mod.frontPage.items[1]).warnings.empty());
}

TEST_CASE("ConfigSpecLoader: a row is shown by its visibleWhen and a setting enabled by its enabledWhen, with every operator")
{
    const auto mod = loaded(configSpecOf(R"(
        { "id": "enable", "type": "switcher", "text": "Enable", "default": true, "storage": { "path": ["Main", "bEnable"] } },
        { "type": "slider", "text": "Height", "default": 1.0, "valueOptions": { "min": 0.5, "max": 1.5, "step": 0.1 }, "storage": { "path": ["Main", "fHeight"] },
          "visibleWhen": { "setting": "Enable", "equals": true }, "enabledWhen": { "setting": "mode", "notEquals": "off" } },
        { "type": "slider", "text": "A", "default": 1.0, "storage": { "path": ["Main", "fA"] }, "visibleWhen": { "setting": "fHeight:Main", "lessThan": 1.4 } },
        { "type": "slider", "text": "B", "default": 1.0, "storage": { "path": ["Main", "fB"] }, "visibleWhen": { "setting": "fHeight:Main", "lessThanOrEqual": 1 } },
        { "type": "slider", "text": "C", "default": 1.0, "storage": { "path": ["Main", "fC"] }, "visibleWhen": { "setting": "fHeight:Main", "greaterThan": 1 } },
        { "type": "slider", "text": "D", "default": 1.0, "storage": { "path": ["Main", "fD"] }, "visibleWhen": { "setting": "fHeight:Main", "greaterThanOrEqual": 1.5 } },
        { "type": "switcher", "text": "E", "default": true, "storage": { "path": ["Main", "bE"] }, "visibleWhen": { "setting": "mode", "oneOf": ["physical", "hybrid"] } },
        { "type": "switcher", "text": "F", "default": true, "storage": { "path": ["Main", "bF"] }, "visibleWhen": { "setting": "iHand:Main", "notOneOf": [1] } },
        { "type": "text", "text": "A text", "visibleWhen": { "all": [ { "setting": "enable", "equals": true }, { "not": { "any": [] } } ] } },
        { "id": "mode", "type": "dropdown", "valueType": "string", "text": "Mode", "default": "physical",
          "valueOptions": { "options": [ { "value": "physical", "text": "Physical" }, { "value": "off", "text": "Off" } ] }, "storage": { "path": ["Main", "sMode"] } },
        { "type": "stepper", "text": "Hand", "default": 0, "valueOptions": { "options": ["Left", "Right"] }, "storage": { "path": ["Main", "iHand"] } }
    )"));
    const auto& items = mod.frontPage.items;
    using Condition = model::Condition;
    using Kind = Condition::Kind;
    const auto compared = [](const Kind kind, std::string id, std::vector<model::Value> values) {
        return Condition{ .kind = kind, .setting = std::move(id), .conditions = {}, .offWhenNotAvailable = false, .values = std::move(values) };
    };

    // a setting is named by its id in any case, also one after the row, and has its id as the config spec gives it
    const auto& height = std::get<model::Setting>(items[1]);
    CHECK(height.visibleWhen == compared(Kind::Equals, "enable", { true }));
    CHECK(height.enabledWhen == compared(Kind::NotEquals, "mode", { std::string("off") }));
    CHECK_FALSE(std::get<model::Setting>(items[0]).visibleWhen);
    CHECK_FALSE(std::get<model::Setting>(items[0]).enabledWhen);

    CHECK(std::get<model::Setting>(items[2]).visibleWhen == compared(Kind::LessThan, "fHeight:Main", { 1.4f }));
    CHECK(std::get<model::Setting>(items[3]).visibleWhen == compared(Kind::LessThanOrEqual, "fHeight:Main", { 1 }));
    CHECK(std::get<model::Setting>(items[4]).visibleWhen == compared(Kind::GreaterThan, "fHeight:Main", { 1 }));
    CHECK(std::get<model::Setting>(items[5]).visibleWhen == compared(Kind::GreaterThanOrEqual, "fHeight:Main", { 1.5f }));
    CHECK(std::get<model::Setting>(items[6]).visibleWhen == compared(Kind::OneOf, "mode", { std::string("physical"), std::string("hybrid") }));
    CHECK(std::get<model::Setting>(items[7]).visibleWhen == compared(Kind::NotOneOf, "iHand:Main", { 1 }));

    const Condition joined{ .kind = Kind::All,
        .setting = {},
        .conditions = { compared(Kind::Equals, "enable", { true }),
            Condition{ .kind = Kind::Not, .setting = {}, .conditions = { Condition{ .kind = Kind::Any, .setting = {}, .conditions = {} } } } } };
    CHECK(std::get<model::Text>(items[8]).visibleWhen == joined);
}

TEST_CASE("ConfigSpecLoader: a section's conditions are joined with those of every row in it, at every level")
{
    const auto mod = loaded(configSpecOf(R"(
        { "id": "a", "type": "switcher", "text": "A", "default": true, "storage": { "path": ["Main", "bA"] } },
        { "id": "b", "type": "switcher", "text": "B", "default": true, "storage": { "path": ["Main", "bB"] } },
        {
            "type": "section", "text": "Outer", "visibleWhen": { "setting": "a", "equals": true }, "enabledWhen": { "setting": "b", "equals": true },
            "content": [
                { "type": "switcher", "text": "In the outer", "default": true, "storage": { "path": ["Main", "bOuter"] } },
                {
                    "type": "section", "text": "Inner", "visibleWhen": { "setting": "b", "equals": false },
                    "content": [
                        { "type": "switcher", "text": "In the inner", "default": true, "storage": { "path": ["Main", "bInner"] },
                          "enabledWhen": { "setting": "a", "equals": false } }
                    ]
                }
            ]
        }
    )"));
    using Condition = model::Condition;
    using Kind = Condition::Kind;
    const auto equals = [](std::string id, const bool on) {
        return Condition{ .kind = Kind::Equals, .setting = std::move(id), .conditions = {}, .offWhenNotAvailable = false, .values = { on } };
    };
    const auto all = [](Condition first, Condition second) {
        return Condition{ .kind = Kind::All, .setting = {}, .conditions = { std::move(first), std::move(second) } };
    };

    const auto& outer = std::get<model::Group>(mod.frontPage.items[2]);
    CHECK(outer.visibleWhen == equals("a", true));
    const auto& inOuter = std::get<model::Setting>(outer.items[0]);
    CHECK(inOuter.visibleWhen == equals("a", true));
    CHECK(inOuter.enabledWhen == equals("b", true));

    const auto& inner = std::get<model::Group>(outer.items[1]);
    CHECK(inner.visibleWhen == all(equals("a", true), equals("b", false)));
    const auto& inInner = std::get<model::Setting>(inner.items[0]);
    CHECK(inInner.visibleWhen == all(equals("a", true), equals("b", false)));
    CHECK(inInner.enabledWhen == all(equals("b", true), equals("a", false)));
}

TEST_CASE("ConfigSpecLoader: a condition that is not one is left out as a whole, so its row is shown and enabled")
{
    const auto mod = loaded(configSpecOf(R"(
        { "id": "on", "type": "switcher", "text": "On", "default": true, "storage": { "path": ["Main", "bOn"] } },
        { "id": "name", "type": "textinput", "text": "Name", "default": "", "storage": { "path": ["Main", "sName"] } },
        { "type": "text", "text": "No such setting", "visibleWhen": { "setting": "missing", "equals": true } },
        { "type": "text", "text": "Another type", "visibleWhen": { "setting": "on", "equals": 1 } },
        { "type": "text", "text": "A text in order", "visibleWhen": { "setting": "name", "lessThan": 3 } },
        { "type": "text", "text": "A oneOf of a bool", "visibleWhen": { "setting": "on", "oneOf": [true] } },
        { "type": "text", "text": "A oneOf of no list", "visibleWhen": { "setting": "name", "oneOf": "a" } },
        { "type": "text", "text": "Two operators", "visibleWhen": { "setting": "on", "equals": true, "notEquals": false } },
        { "type": "text", "text": "No operator", "visibleWhen": { "setting": "on" } },
        { "type": "text", "text": "An unknown key", "visibleWhen": { "setting": "on", "is": true } },
        { "type": "text", "text": "No condition", "visibleWhen": { "when": true } },
        { "type": "text", "text": "Not an object", "visibleWhen": true },
        { "type": "text", "text": "A part that is not one", "visibleWhen": { "any": [ { "setting": "on", "equals": true }, { "setting": "missing", "equals": true } ] } },
        { "type": "switcher", "text": "Not enabled by it", "default": true, "storage": { "path": ["Main", "bOther"] }, "enabledWhen": { "not": { "plugin": "" } } }
    )"));
    const auto& items = mod.frontPage.items;
    REQUIRE(items.size() == 14);
    for (std::size_t place = 2; place < 13; ++place) {
        INFO(place);
        const auto& text = std::get<model::Text>(items[place]);
        CHECK_FALSE(text.visibleWhen);
    }
    CHECK_FALSE(std::get<model::Setting>(items[13]).enabledWhen);
}

TEST_CASE("ConfigSpecLoader: a condition on a plugin is answered as the config spec is read, and holds while the plugin is loaded")
{
    const auto text = configSpecOf(R"(
        { "type": "text", "text": "Game plugin", "visibleWhen": { "plugin": "MyMod - ENB Patch.esp" } },
        { "type": "text", "text": "No game plugin", "visibleWhen": { "not": { "plugin": "Missing.esp" } } },
        { "type": "text", "text": "F4SE plugin", "visibleWhen": { "f4sePlugin": "F4VRBody" } },
        { "type": "text", "text": "No F4SE plugin", "visibleWhen": { "f4sePlugin": "Missing" } }
    )");
    const spec::PluginChecks plugins{ .isGamePluginLoaded =
                                          [](const std::string_view plugin) {
                                              return plugin == "MyMod - ENB Patch.esp";
                                          },
        .isF4sePluginLoaded =
            [](const std::string_view plugin) {
                return plugin == "F4VRBody";
            } };
    auto mod = spec::loadConfigSpec(text, {}, plugins);
    REQUIRE(mod);

    // an All of none holds, and an Any of none does not
    using Condition = model::Condition;
    using Kind = Condition::Kind;
    const Condition holds{ .kind = Kind::All, .setting = {}, .conditions = {} };
    const Condition fails{ .kind = Kind::Any, .setting = {}, .conditions = {} };
    const auto& items = mod->frontPage.items;
    CHECK(std::get<model::Text>(items[0]).visibleWhen == holds);
    CHECK(std::get<model::Text>(items[1]).visibleWhen == Condition{ .kind = Kind::Not, .setting = {}, .conditions = { fails } });
    CHECK(std::get<model::Text>(items[2]).visibleWhen == holds);
    CHECK(std::get<model::Text>(items[3]).visibleWhen == fails);

    // with no checks every plugin is loaded
    const auto unchecked = loaded(text);
    CHECK(std::get<model::Text>(unchecked.frontPage.items[3]).visibleWhen == holds);
}

TEST_CASE("ConfigSpecLoader: a condition asks a state of the mod's client by its name, on, or with an operator of a value of any type")
{
    const auto mod = loaded(configSpecOf(R"(
        { "type": "button", "text": "On", "action": { "type": "RunConsoleCommand", "command": "tm" }, "enabledWhen": { "clientState": "ready" } },
        { "type": "button", "text": "Equals", "action": { "type": "RunConsoleCommand", "command": "tm" },
          "enabledWhen": { "clientState": "calibrating", "equals": false },
          "warnings": [ { "text": "Stand still.", "when": { "not": { "clientState": "calibrating", "notEquals": true } } } ] },
        { "type": "text", "text": "Order", "visibleWhen": { "clientState": "Level", "greaterThanOrEqual": 2.5 } },
        { "type": "text", "text": "List", "visibleWhen": { "clientState": "mode", "oneOf": ["walk", "run"] } },
        { "type": "text", "text": "No name", "visibleWhen": { "clientState": "" } },
        { "type": "text", "text": "Order of a text", "visibleWhen": { "clientState": "mode", "lessThan": "run" } },
        { "type": "text", "text": "No list", "visibleWhen": { "clientState": "mode", "oneOf": "walk" } },
        { "type": "text", "text": "No operator", "visibleWhen": { "clientState": "mode", "is": "walk" } }
    )"));
    const auto& items = mod.frontPage.items;
    using Kind = model::Condition::Kind;
    const auto stateOf = [](const std::string& name, const Kind kind, std::vector<model::Value> values) {
        return model::Condition{ .kind = kind,
            .setting = name,
            .conditions = {},
            .offWhenNotAvailable = false,
            .values = std::move(values),
            .source = model::Condition::Source::ClientState };
    };
    CHECK(std::get<model::Button>(items[0]).enabledWhen == stateOf("ready", Kind::On, {}));
    const auto& equals = std::get<model::Button>(items[1]);
    CHECK(equals.enabledWhen == stateOf("calibrating", Kind::Equals, { false }));
    REQUIRE(equals.warnings.size() == 1);
    CHECK(equals.warnings[0].when.conditions == std::vector{ stateOf("calibrating", Kind::NotEquals, { true }) });
    CHECK(std::get<model::Text>(items[2]).visibleWhen == stateOf("Level", Kind::GreaterThanOrEqual, { 2.5f }));
    CHECK(std::get<model::Text>(items[3]).visibleWhen == stateOf("mode", Kind::OneOf, { "walk"s, "run"s }));

    // one that is not written right is left out, and reported
    for (std::size_t place = 4; place < items.size(); ++place) {
        CHECK_FALSE(std::get<model::Text>(items[place]).visibleWhen);
    }
    std::vector<std::string> problems;
    for (const auto& problem : mod.problems) {
        problems.push_back(problem.message);
    }
    CHECK(problems == Lines{
                          "Its visibleWhen is left out: its clientState names no state. The row is shown and enabled as if it had none.",
                          "Its visibleWhen is left out: lessThan compares numbers, and its value is no number. The row is shown and enabled as if it had none.",
                          "Its visibleWhen is left out: its oneOf is not a list. The row is shown and enabled as if it had none.",
                          "Its visibleWhen is left out: it has no operator for the state 'mode'. The row is shown and enabled as if it had none.",
                      });
}

TEST_CASE("ConfigSpecLoader: a condition asks a setting of the game's INI by its name, whose first letter tells its type")
{
    const auto mod = loaded(configSpecOf(R"(
        { "type": "text", "text": "On", "visibleWhen": { "gameIniSetting": "bUseWandDirectionalMovement:VRInput" } },
        { "type": "text", "text": "Order", "visibleWhen": { "gameIniSetting": "iShadowMapResolution:Display", "lessThan": 2048 } },
        { "type": "text", "text": "Float", "visibleWhen": { "gameIniSetting": "fMouseHeadingSensitivity:Controls", "greaterThan": 0.5 } },
        { "type": "text", "text": "Unsigned", "visibleWhen": { "gameIniSetting": "uGridsToLoad:General", "oneOf": [5, 7] } },
        { "type": "text", "text": "Text", "visibleWhen": { "gameIniSetting": "sLanguage:General", "notOneOf": ["en"] } },
        { "type": "text", "text": "No name", "visibleWhen": { "gameIniSetting": "" } },
        { "type": "text", "text": "Color", "visibleWhen": { "gameIniSetting": "rDefaultColor:Interface", "equals": 1 } },
        { "type": "text", "text": "Order of a bool", "visibleWhen": { "gameIniSetting": "bShow:Main", "lessThan": 1 } },
        { "type": "text", "text": "List of a float", "visibleWhen": { "gameIniSetting": "fScale:Main", "oneOf": [1.0] } },
        { "type": "text", "text": "Other type", "visibleWhen": { "gameIniSetting": "bShow:Main", "equals": 1 } },
        { "type": "text", "text": "No operator", "visibleWhen": { "gameIniSetting": "bShow:Main", "is": true } }
    )"));
    const auto& items = mod.frontPage.items;
    using Kind = model::Condition::Kind;
    const auto settingOf = [](const std::string& name, const Kind kind, std::vector<model::Value> values) {
        return model::Condition{ .kind = kind,
            .setting = name,
            .conditions = {},
            .offWhenNotAvailable = false,
            .values = std::move(values),
            .source = model::Condition::Source::GameIniSetting };
    };
    CHECK(std::get<model::Text>(items[0]).visibleWhen == settingOf("bUseWandDirectionalMovement:VRInput", Kind::On, {}));
    CHECK(std::get<model::Text>(items[1]).visibleWhen == settingOf("iShadowMapResolution:Display", Kind::LessThan, { 2048 }));
    CHECK(std::get<model::Text>(items[2]).visibleWhen == settingOf("fMouseHeadingSensitivity:Controls", Kind::GreaterThan, { 0.5f }));
    CHECK(std::get<model::Text>(items[3]).visibleWhen == settingOf("uGridsToLoad:General", Kind::OneOf, { 5, 7 }));
    CHECK(std::get<model::Text>(items[4]).visibleWhen == settingOf("sLanguage:General", Kind::NotOneOf, { "en"s }));

    // one that is not written right, or does not fit the setting's type, is left out, and reported
    for (std::size_t place = 5; place < items.size(); ++place) {
        CHECK_FALSE(std::get<model::Text>(items[place]).visibleWhen);
    }
    std::vector<std::string> problems;
    for (const auto& problem : mod.problems) {
        problems.push_back(problem.message);
    }
    const auto leftOut = [](const std::string& reason) {
        return std::format("Its visibleWhen is left out: {}. The row is shown and enabled as if it had none.", reason);
    };
    CHECK(problems == Lines{
                          leftOut("its gameIniSetting names no setting"),
                          leftOut("its gameIniSetting 'rDefaultColor:Interface' is of no type a condition compares: the name of one starts with b, i, u, f or s"),
                          leftOut("lessThan compares numbers, and 'bShow:Main' is no number"),
                          leftOut("oneOf is of a text or a whole number, and 'fScale:Main' is neither"),
                          leftOut("its equals has a value that is not of the type of 'bShow:Main'"),
                          leftOut("it has no operator for the game's INI setting 'bShow:Main'"),
                      });
}
