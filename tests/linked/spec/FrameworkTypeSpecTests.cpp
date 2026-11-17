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

TEST_CASE("ConfigSpecLoader: a binding is the text of its table or its INI line, with its default an object the framework reads, and offers suppress")
{
    const auto mod = loaded(R"({
        "minSchemaVersion": 1, "modName": "MyMod", "applies": "live",
        "files": {
            "main": { "format": "toml", "root": "myGames", "path": "MyMod.toml" },
            "ini": { "format": "ini", "root": "myGames", "path": "MyMod.ini" }
        },
        "defaultFile": "main",
        "content": [
            { "type": "cf.inputBinding", "text": "Grab", "default": { "type": "tap", "button": "Trigger", "hand": "off" },
              "valueOptions": { "suppress": true }, "storage": { "path": ["Sphere", "kPrimaryBinding"] } },
            { "type": "cf.inputBinding", "text": "Open", "valueType": "string",
              "default": { "hand": "primary", "type": "longpress", "button": "thumbstick", "duration": 1.5, "modifier": { "hand": "offhand", "button": "thumbstick" } },
              "storage": { "path": ["Main", "kOpen"] } },
            { "type": "cf.inputBinding", "text": "Typo", "default": { "hand": "offhand", "type": "tap", "button": "thumbstik" }, "storage": { "path": ["Main", "kTypo"] } },
            { "type": "cf.inputBinding", "text": "Text", "valueType": "int", "default": "tap trigger", "storage": { "path": ["Main", "kText"] } },
            { "type": "cf.inputBinding", "text": "In an INI file", "default": { "hand": "primary", "type": "longpress", "button": "thumbstick", "duration": 1.5,
              "suppress": true, "modifier": { "hand": "offhand", "button": "grip" } }, "storage": { "file": "ini", "section": "Main", "key": "kBinding" } },
            { "type": "cf.inputBinding", "text": "Off in an INI file", "default": { "type": "none" }, "storage": { "file": "ini", "section": "Main", "key": "kOff" } },
            { "type": "cf.inputBinding", "text": "Slow in an INI file", "default": { "hand": "primary", "type": "axis", "direction": "up", "accelerate": false },
              "storage": { "file": "ini", "section": "Main", "key": "kSlow" } },
            { "type": "cf.inputBinding", "text": "In a state", "default": { "type": "none" }, "storage": { "clientState": "binding" } }
        ]
    })");
    const auto settings = settingsOf(mod);
    REQUIRE(settings.size() == 8);
    for (const auto& setting : settings) {
        CHECK(setting.type == model::SettingType::InputBinding);
        CHECK(setting.valueType == model::StoredType::String);
    }
    // the table as the framework writes it, with its words and the order of its keys
    CHECK(settings[0].defaultValue == model::Value(std::string(R"({ hand = "offhand", type = "tap", button = "trigger" })")));
    CHECK(settings[0].valueOptions.suppress);
    CHECK(settings[1].defaultValue ==
          model::Value(std::string(R"({ hand = "primary", type = "longpress", button = "thumbstick", duration = 1.5, modifier = { hand = "offhand", button = "thumbstick" } })")));
    CHECK_FALSE(settings[1].valueOptions.suppress);
    CHECK_FALSE(settings[2].defaultValue);
    CHECK_FALSE(settings[3].defaultValue);
    // in an INI file, its line in the grammar's own words, and an INI line has no accelerate
    CHECK(settings[0].tableForm == model::TableForm::Toml);
    CHECK(settings[4].tableForm == model::TableForm::IniLine);
    CHECK(settings[4].defaultValue == model::Value("primary longpress thumbstick 1.5 suppress +offhand:grip"s));
    CHECK(settings[5].defaultValue == model::Value("none"s));
    CHECK_FALSE(settings[6].defaultValue);
    CHECK(std::holds_alternative<model::UnknownStorage>(settings[7].storage));
    CHECK(problemLines(mod) ==
          Lines{ "E Typo: Its default is not a binding: 'thumbstik' is not a button, one of trigger, grip, a, b, thumbstick, menu, system. It has no Reset. [Typo] "
                 "content[2].default",
              "E Text: It has no default binding, an object as { \"hand\": \"offhand\", \"type\": \"tap\", \"button\": \"trigger\" }: it has no Reset. [Text] content[3]",
              "W Text: Its valueType \"int\" is not string, which a binding is: it is read as string. [Text] content[3].valueType",
              "E Slow in an INI file: Its default turns accelerate off, which the line of an INI file cannot have. It has no Reset. [Slow in an INI file] content[6].default",
              "E In a state: A binding is kept in a value of a TOML or an INI file, and it is not: it is not available. [In a state] content[7].storage" });
}

TEST_CASE("ConfigSpecLoader: a color is the text of its table or its INI line, with its default an object the framework reads, and can be RGB only")
{
    const auto mod = loaded(R"({
        "minSchemaVersion": 1, "modName": "MyMod", "applies": "live",
        "files": {
            "main": { "format": "toml", "root": "myGames", "path": "MyMod.toml" },
            "ini": { "format": "ini", "root": "myGames", "path": "MyMod.ini" }
        },
        "defaultFile": "main",
        "content": [
            { "type": "cf.color", "text": "Ray", "default": { "alpha": 200, "rgb": [190, 255, 190] }, "storage": { "path": ["Pointer", "cRay"] } },
            { "type": "cf.color", "text": "Light", "valueType": "int", "default": { "rgb": [255, 214, 170] }, "valueOptions": { "alpha": false },
              "storage": { "path": ["Light", "cColor"] } },
            { "type": "cf.color", "text": "Out of range", "default": { "rgb": [300, 0, 0] }, "storage": { "path": ["Light", "cOut"] } },
            { "type": "cf.color", "text": "Text", "default": "red", "storage": { "path": ["Light", "cText"] } },
            { "type": "cf.color", "text": "In an INI file", "default": { "rgb": [190, 255, 190], "alpha": 10 }, "storage": { "file": "ini", "section": "Main", "key": "cColor" } },
            { "type": "cf.color", "text": "RGB in an INI file", "default": { "rgb": [255, 214, 170] }, "storage": { "file": "ini", "section": "Main", "key": "cLight" } },
            { "type": "cf.color", "text": "In a state", "default": { "alpha": 10 }, "storage": { "clientState": "color" } }
        ]
    })");
    const auto settings = settingsOf(mod);
    REQUIRE(settings.size() == 7);
    for (const auto& setting : settings) {
        CHECK(setting.type == model::SettingType::Color);
        CHECK(setting.valueType == model::StoredType::String);
    }
    // the table with the keys it has, rgb before alpha
    CHECK(settings[0].defaultValue == model::Value("{ rgb = [190, 255, 190], alpha = 200 }"s));
    CHECK(settings[0].valueOptions.alpha);
    CHECK(settings[1].defaultValue == model::Value("{ rgb = [255, 214, 170] }"s));
    CHECK_FALSE(settings[1].valueOptions.alpha);
    CHECK_FALSE(settings[2].defaultValue);
    CHECK_FALSE(settings[3].defaultValue);
    // in an INI file, its line, with an alpha where the table has one
    CHECK(settings[4].tableForm == model::TableForm::IniLine);
    CHECK(settings[4].defaultValue == model::Value("190,255,190,10"s));
    CHECK(settings[5].defaultValue == model::Value("255,214,170"s));
    CHECK(std::holds_alternative<model::UnknownStorage>(settings[6].storage));
    CHECK(problemLines(mod) ==
          Lines{ "W Light: Its valueType \"int\" is not string, which a color is: it is read as string. [Light] content[1].valueType",
              "E Out of range: Its default is not a color: rgb has a number that is not a whole number in 0..255. It has no Reset. [Out of range] content[2].default",
              "E Text: It has no default color, an object as { \"rgb\": [255, 255, 255], \"alpha\": 255 }: it has no Reset. [Text] content[3]",
              "E In a state: A color is kept in a value of a TOML or an INI file, and it is not: it is not available. [In a state] content[6].storage" });
}

TEST_CASE("ConfigSpecLoader: a color's presets are its own or a shared list's, with their names, and one that is no color is left out")
{
    const auto mod = loaded(R"({
        "minSchemaVersion": 1, "modName": "MyMod", "applies": "live",
        "files": { "main": { "format": "toml", "root": "myGames", "path": "MyMod.toml" } },
        "defaultFile": "main",
        "sharedPresetLists": {
            "Lights": [ { "rgb": [255, 180, 107], "text": "Warm white" }, { "rgb": [255, 228, 206] }, { "alpha": 10 } ]
        },
        "content": [
            { "type": "cf.color", "text": "Ray", "default": { "rgb": [190, 255, 190], "alpha": 200 }, "storage": { "path": ["Pointer", "cRay"] },
              "valueOptions": { "presets": [ { "rgb": [255, 0, 0], "alpha": 100, "text": "Faint red" }, "blue", { "rgb": [300, 0, 0] }, { "rgb": [0, 0, 255], "name": "Blue" } ] } },
            { "type": "cf.color", "text": "Light", "default": { "rgb": [255, 214, 170] }, "storage": { "path": ["Light", "cColor"] },
              "valueOptions": { "alpha": false, "sharedPresets": "lights" } },
            { "type": "cf.color", "text": "Both", "default": { "rgb": [255, 214, 170] }, "storage": { "path": ["Light", "cBoth"] },
              "valueOptions": { "presets": [ { "rgb": [0, 0, 0] } ], "sharedPresets": "Lights" } },
            { "type": "cf.color", "text": "No list", "default": { "rgb": [255, 214, 170] }, "storage": { "path": ["Light", "cNoList"] },
              "valueOptions": { "sharedPresets": "Lamps" } },
            { "type": "cf.color", "text": "None", "default": { "rgb": [255, 214, 170] }, "storage": { "path": ["Light", "cNone"] } }
        ]
    })");
    const auto settings = settingsOf(mod);
    REQUIRE(settings.size() == 5);
    const auto presetsOf = [](const model::Setting& setting) {
        Lines lines;
        for (const auto& preset : setting.valueOptions.presets) {
            lines.push_back(std::format("{} {} {} {}{}", preset.rgb[0], preset.rgb[1], preset.rgb[2], preset.alpha ? std::format("{} ", *preset.alpha) : "", preset.text));
        }
        return lines;
    };
    CHECK(presetsOf(settings[0]) == Lines{ "255 0 0 100 Faint red", "0 0 255 " });
    // a shared list is named in any case
    CHECK(presetsOf(settings[1]) == Lines{ "255 180 107 Warm white", "255 228 206 " });
    CHECK(presetsOf(settings[2]) == Lines{ "0 0 0 " });
    CHECK(settings[3].valueOptions.presets.empty());
    CHECK(settings[4].valueOptions.presets.empty());
    CHECK(problemLines(mod) ==
          Lines{ "E Ray: Preset 2 is left out: it is not an object as { \"rgb\": [255, 180, 107], \"text\": \"Warm white\" }. [Ray] content[0].valueOptions.presets[1]",
              "E Ray: Preset 3 is left out: rgb has a number that is not a whole number in 0..255. [Ray] content[0].valueOptions.presets[2]",
              "W Ray: Unknown field \"name\": it is not read. [Ray] content[0].valueOptions.presets[3].name",
              "E Light: Preset 3 of the shared list \"lights\" is left out: it has no rgb. [Light] content[1].valueOptions",
              "W Both: It has its own presets and names a shared list of them too: its own presets are used. [Both] content[2].valueOptions.sharedPresets",
              "W No list: It names \"Lamps\", which is no list of the config spec's sharedPresetLists: it has no presets. [No list] content[3].valueOptions.sharedPresets" });
}

TEST_CASE("ConfigSpecLoader: a color has 12 presets at the most, and the rest are left out")
{
    std::string presets;
    for (int i = 0; i < 14; ++i) {
        presets += std::format("{}{{ \"rgb\": [{}, 0, 0] }}", presets.empty() ? "" : ", ", i);
    }
    const auto mod = loaded(std::format(R"({{
        "minSchemaVersion": 1, "modName": "MyMod", "applies": "live",
        "files": {{ "main": {{ "format": "toml", "root": "myGames", "path": "MyMod.toml" }} }},
        "defaultFile": "main",
        "content": [
            {{ "type": "cf.color", "text": "Ray", "default": {{ "rgb": [190, 255, 190] }}, "storage": {{ "path": ["Pointer", "cRay"] }},
              "valueOptions": {{ "presets": [ {} ] }} }}
        ]
    }})",
        presets));
    const auto settings = settingsOf(mod);
    REQUIRE(settings.size() == 1);
    REQUIRE(settings[0].valueOptions.presets.size() == 12);
    CHECK(settings[0].valueOptions.presets.back().rgb[0] == 11);
    CHECK(problemLines(mod) == Lines{ "W Ray: It has 14 presets, and its editor shows 12 at the most: the rest are left out. [Ray] content[0].valueOptions" });
}

TEST_CASE("ConfigSpecLoader: a transform is the text of its table or its INI line, with its default an object the framework reads")
{
    const auto mod = loaded(R"({
        "minSchemaVersion": 1, "modName": "MyMod", "applies": "live",
        "files": {
            "main": { "format": "toml", "root": "myGames", "path": "MyMod.toml" },
            "ini": { "format": "ini", "root": "myGames", "path": "MyMod.ini" }
        },
        "defaultFile": "main",
        "content": [
            { "type": "cf.transform", "text": "Mesh", "default": { "scale": 1.5, "pos": [1, 2, 3], "rot": [0, 90, 0] }, "storage": { "path": ["Hand", "tMesh"] } },
            { "type": "cf.transform", "text": "Zone", "valueType": "int", "default": { "pos": [0.0, 0.0, 12.0] }, "storage": { "path": ["Hand", "tZone"] } },
            { "type": "cf.transform", "text": "Two numbers", "default": { "pos": [0, 0] }, "storage": { "path": ["Hand", "tTwo"] } },
            { "type": "cf.transform", "text": "Text", "default": "0,0,0;0,0,0;1", "storage": { "path": ["Hand", "tText"] } },
            { "type": "cf.transform", "text": "In an INI file", "default": { "pos": [0, 0, 12.5] }, "storage": { "file": "ini", "section": "Main", "key": "tMesh" } },
            { "type": "cf.transform", "text": "In a state", "default": {}, "storage": { "clientState": "transform" } }
        ]
    })");
    const auto settings = settingsOf(mod);
    REQUIRE(settings.size() == 6);
    for (const auto& setting : settings) {
        CHECK(setting.type == model::SettingType::Transform);
        CHECK(setting.valueType == model::StoredType::String);
    }
    // the table with the keys it has, pos, rot and scale
    CHECK(settings[0].defaultValue == model::Value("{ pos = [1.0, 2.0, 3.0], rot = [0.0, 90.0, 0.0], scale = 1.5 }"s));
    CHECK(settings[1].defaultValue == model::Value("{ pos = [0.0, 0.0, 12.0] }"s));
    CHECK_FALSE(settings[2].defaultValue);
    CHECK_FALSE(settings[3].defaultValue);
    // in an INI file, its line of all seven numbers, a key the table leaves out as the framework reads it
    CHECK(settings[4].tableForm == model::TableForm::IniLine);
    CHECK(settings[4].defaultValue == model::Value("0.0,0.0,12.5;0.0,0.0,0.0;1.0"s));
    CHECK(std::holds_alternative<model::UnknownStorage>(settings[5].storage));
    CHECK(problemLines(mod) == Lines{ "W Zone: Its valueType \"int\" is not string, which a transform is: it is read as string. [Zone] content[1].valueType",
                                   "E Two numbers: Its default is not a transform: pos is not an array of three numbers. It has no Reset. [Two numbers] content[2].default",
                                   "E Text: It has no default transform, an object as { \"pos\": [0, 0, 0], \"rot\": [0, 0, 0], \"scale\": 1 }: it has no Reset. [Text] content[3]",
                                   "E In a state: A transform is kept in a value of a TOML or an INI file, and it is not: it is not available. [In a state] content[5].storage" });
}

TEST_CASE("ConfigSpecLoader: a transform's parts are on with their own ranges, or off, or have the range its config spec gives, of a part or of an axis")
{
    const auto mod = loaded(R"({
        "minSchemaVersion": 1, "modName": "MyMod", "applies": "live",
        "files": { "main": { "format": "toml", "root": "myGames", "path": "MyMod.toml" } },
        "defaultFile": "main",
        "content": [
            { "type": "cf.transform", "text": "Own", "default": {}, "storage": { "path": ["Own"] } },
            { "type": "cf.transform", "text": "Ranges", "default": {}, "storage": { "path": ["Ranges"] },
              "valueOptions": { "position": { "min": -10, "max": 10, "step": 0.1, "z": { "min": 0, "max": 40 } }, "rotation": false, "scale": { "min": 0.5, "max": 2.0 } } },
            { "type": "cf.transform", "text": "Rotation", "default": {}, "storage": { "path": ["Rotation"] },
              "valueOptions": { "position": false, "rotation": { "roll": { "step": 5 }, "heading": { "max": -200 } }, "scale": false } },
            { "type": "cf.transform", "text": "Mistakes", "default": {}, "storage": { "path": ["Mistakes"] },
              "valueOptions": { "position": { "min": 5, "max": 1 }, "rotation": { "step": 0 }, "scale": 3 } },
            { "type": "cf.transform", "text": "Unknown", "default": {}, "storage": { "path": ["Unknown"] },
              "valueOptions": { "position": { "stpe": 1, "w": {} }, "rotation": { "yaw": 1, "roll": { "z": 1 } } } },
            { "type": "cf.transform", "text": "None on", "default": {}, "storage": { "path": ["None"] },
              "valueOptions": { "position": false, "rotation": false, "scale": false } }
        ]
    })");
    const auto settings = settingsOf(mod);
    REQUIRE(settings.size() == 6);
    const auto range = [&settings](const std::size_t setting, const std::size_t axis) {
        const auto& found = settings[setting].valueOptions.transformAxes[axis];
        return std::array{ found.min, found.max, found.step };
    };
    using Range = std::array<float, 3>;
    constexpr Range POSITION{ -50.0f, 50.0f, 1.0f };
    constexpr Range ROTATION{ -180.0f, 180.0f, 1.0f };
    constexpr Range SCALE{ 0.1f, 3.0f, 0.1f };

    // left out, each part is on with its own range
    for (std::size_t axis = 0; axis < model::TRANSFORM_AXES; ++axis) {
        CHECK(settings[0].valueOptions.transformAxes[axis].on);
        CHECK(range(0, axis) == (axis < 3 ? POSITION : axis < 6 ? ROTATION : SCALE));
    }

    // a part's range, an axis's over it, and a part that is off
    CHECK(range(1, 0) == Range{ -10.0f, 10.0f, 0.1f });
    CHECK(range(1, 2) == Range{ 0.0f, 40.0f, 0.1f });
    CHECK_FALSE(settings[1].valueOptions.transformAxes[model::TRANSFORM_ROTATION].on);
    CHECK_FALSE(settings[1].valueOptions.transformAxes[model::TRANSFORM_ROTATION + 2].on);
    CHECK(range(1, model::TRANSFORM_SCALE) == Range{ 0.5f, 2.0f, 0.1f });

    CHECK_FALSE(settings[2].valueOptions.transformAxes[0].on);
    CHECK(settings[2].valueOptions.transformAxes[model::TRANSFORM_ROTATION].on);
    CHECK(range(2, model::TRANSFORM_ROTATION) == ROTATION);
    CHECK(range(2, model::TRANSFORM_ROTATION + 1) == Range{ -180.0f, 180.0f, 5.0f });

    // what is wrong keeps the range of what it is in
    CHECK(range(3, 0) == POSITION);
    CHECK(range(3, model::TRANSFORM_ROTATION) == ROTATION);
    CHECK(settings[3].valueOptions.transformAxes[model::TRANSFORM_SCALE].on);
    CHECK(range(3, model::TRANSFORM_SCALE) == SCALE);

    // with no part on it is not available
    CHECK(std::holds_alternative<model::UnknownStorage>(settings[5].storage));

    constexpr auto NO_RANGE = "Its range is none: its min has to be below its max, and its step above 0. It has the range of what it is in.";
    CHECK(problemLines(mod) == Lines{ std::format("E Rotation: {} [Rotation] content[2].valueOptions.rotation.heading", NO_RANGE),
                                   std::format("E Mistakes: {} [Mistakes] content[3].valueOptions.position", NO_RANGE),
                                   std::format("E Mistakes: {} [Mistakes] content[3].valueOptions.rotation", NO_RANGE),
                                   "W Mistakes: Its scale is not true, false or a range: it is on, with its own range. [Mistakes] content[3].valueOptions.scale",
                                   "W Unknown: Unknown field \"stpe\": it is not read. [Unknown] content[4].valueOptions.position.stpe",
                                   "W Unknown: Unknown field \"w\": it is not read. [Unknown] content[4].valueOptions.position.w",
                                   "W Unknown: Unknown field \"z\": it is not read. [Unknown] content[4].valueOptions.rotation.roll.z",
                                   "W Unknown: Unknown field \"yaw\": it is not read. [Unknown] content[4].valueOptions.rotation.yaw",
                                   "E None on: Its position, its rotation and its scale are all off: it is not available. [None on] content[5].valueOptions" });
}

TEST_CASE("ConfigSpecLoader: a haptic is the name of a pattern, with None and the framework's patterns as its options")
{
    const auto mod = loaded(R"({
        "minSchemaVersion": 1, "modName": "MyMod", "applies": "live",
        "files": { "main": { "format": "toml", "root": "myGames", "path": "MyMod.toml" } },
        "defaultFile": "main",
        "content": [
            { "type": "cf.haptic", "text": "Entry", "default": "double click", "storage": { "path": ["Sphere", "sEntryHaptic"] } },
            { "type": "cf.haptic", "text": "Off", "valueType": "int", "default": "off", "storage": { "path": ["Sphere", "sOff"] } },
            { "type": "cf.haptic", "text": "Typo", "default": "Tik", "storage": { "path": ["Sphere", "sTypo"] } },
            { "type": "cf.haptic", "text": "No text", "default": 2, "storage": { "path": ["Sphere", "sNoText"] } }
        ]
    })");
    const auto settings = settingsOf(mod);
    REQUIRE(settings.size() == 4);
    for (const auto& setting : settings) {
        CHECK(setting.type == model::SettingType::Haptic);
        CHECK(setting.valueType == model::StoredType::String);
        REQUIRE(setting.valueOptions.options.size() == 19);
        CHECK(setting.valueOptions.options.front() == model::Option{ .value = "none"s, .text = "None" });
        CHECK(setting.valueOptions.options[3] == model::Option{ .value = "DoubleClick"s, .text = "Double click" });
    }
    // a default is read as the framework reads it, and written as the library names it
    CHECK(settings[0].defaultValue == model::Value("DoubleClick"s));
    CHECK(settings[1].defaultValue == model::Value("none"s));
    CHECK_FALSE(settings[2].defaultValue);
    CHECK_FALSE(settings[3].defaultValue);
    CHECK(problemLines(mod) == Lines{ "W Off: Its valueType \"int\" is not string, which a haptic is: it is read as string. [Off] content[1].valueType",
                                   "E Typo: Its default \"Tik\" is not a haptic pattern of the framework: it has no Reset. [Typo] content[2].default",
                                   "E No text: It has no default haptic, a text as \"Tick\" or \"none\": it has no Reset. [No text] content[3]" });
}

TEST_CASE("ConfigSpecLoader: a binding takes the hands, the types and the buttons its lists name, as the framework names them")
{
    const auto mod = loaded(R"({
        "minSchemaVersion": 1, "modName": "MyMod", "applies": "live",
        "files": { "main": { "format": "toml", "root": "myGames", "path": "MyMod.toml" } },
        "defaultFile": "main",
        "content": [
            { "type": "cf.inputBinding", "text": "Back", "default": { "hand": "any", "type": "tap", "button": "grip" },
              "valueOptions": { "hands": ["any", "Primary"], "types": ["tap", "longpress"], "buttons": ["grip", "b"] }, "storage": { "path": ["Main", "kBack"] } },
            { "type": "cf.inputBinding", "text": "Any", "default": { "hand": "primary", "type": "axis", "direction": "up" }, "storage": { "path": ["Main", "kAny"] } },
            { "type": "cf.inputBinding", "text": "Words", "default": { "hand": "primary", "type": "press", "button": "grip" },
              "valueOptions": { "buttons": ["grip", "trigr", 3], "types": "press" }, "storage": { "path": ["Main", "kWords"] } },
            { "type": "cf.inputBinding", "text": "Untaken", "default": { "hand": "primary", "type": "tap", "button": "grip" },
              "valueOptions": { "types": ["press"] }, "storage": { "path": ["Main", "kUntaken"] } },
            { "type": "cf.inputBinding", "text": "Off", "default": { "type": "none" }, "valueOptions": { "types": ["press"] }, "storage": { "path": ["Main", "kOff"] } }
        ]
    })");
    const auto settings = settingsOf(mod);
    REQUIRE(settings.size() == 5);
    CHECK(settings[0].valueOptions.hands == std::vector{ vrcf::Hand::Any, vrcf::Hand::Primary });
    CHECK(settings[0].valueOptions.types == std::vector{ vrcf::ActivationType::Tap, vrcf::ActivationType::LongPress });
    CHECK(settings[0].valueOptions.buttons == std::vector{ vr::k_EButton_Grip, vr::k_EButton_ApplicationMenu });
    CHECK(settings[1].valueOptions.hands.empty());
    CHECK(settings[1].valueOptions.types.empty());
    CHECK(settings[1].valueOptions.buttons.empty());
    // a word that is none is left out, and a value that is no list takes any
    CHECK(settings[2].valueOptions.buttons == std::vector{ vr::k_EButton_Grip });
    CHECK(settings[2].valueOptions.types.empty());
    CHECK(settings[2].defaultValue);
    // a default it does not take is none, and a binding that is off is always taken
    CHECK_FALSE(settings[3].defaultValue);
    CHECK(settings[4].defaultValue);
    CHECK(problemLines(mod) ==
          Lines{ "W Words: \"trigr\" is not a button, as the framework names it: it is left out. [Words] content[2].valueOptions.buttons[1]",
              "W Words: 3 is not a button, as the framework names it: it is left out. [Words] content[2].valueOptions.buttons[2]",
              "W Words: It is not a list of words: the binding takes any type. [Words] content[2].valueOptions.types",
              "E Untaken: Its default is not a binding it takes: 'tap' is not a way this binding takes, one of press. It has no Reset. [Untaken] content[3].default" });
}
