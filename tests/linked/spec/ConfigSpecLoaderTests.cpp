#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <filesystem>
#include <format>
#include <fstream>
#include <functional>
#include <sstream>
#include <unordered_set>

#include <toml++/toml.hpp>

#include "TestConfigSpecs.h"
#include "TestFiles.h"
#include "common/CommonUtils.h"
#include "common/TomlEditor.h"
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
    /**
     * The items as lines of "what: text", with the items of a group indented under it, so that a whole page
     * is compared in one check.
     */
    void describe(const std::vector<model::Item>& items, const std::string& indent, Lines& lines)
    {
        for (const auto& item : items) {
            if (const auto* group = std::get_if<model::Group>(&item)) {
                lines.push_back(indent + "section: " + group->text);
                describe(group->items, indent + "  ", lines);
            } else if (const auto* setting = std::get_if<model::Setting>(&item)) {
                lines.push_back(indent + "setting " + setting->id + ": " + setting->text);
            } else if (const auto* placeholder = std::get_if<model::Placeholder>(&item)) {
                lines.push_back(indent + "placeholder " + placeholder->type + ": " + placeholder->text);
            } else {
                lines.push_back(indent + "?");
            }
        }
    }

    Lines describe(const model::Page& page)
    {
        Lines lines;
        describe(page.items, "", lines);
        return lines;
    }

    /**
     * Adds the ids of the settings of the activation spheres among items, with the pages under a page.
     */
    void addSphereKeys(const std::vector<model::Item>& items, const bool inSphere, std::unordered_set<std::string>& ids)
    {
        for (const auto& item : items) {
            if (const auto* setting = std::get_if<model::Setting>(&item); setting && inSphere) {
                ids.insert(setting->id);
            } else if (const auto* group = std::get_if<model::Group>(&item)) {
                addSphereKeys(group->items, inSphere || group->role == model::GroupRole::ActivationSphere, ids);
            }
        }
    }

    void addSphereKeys(const std::vector<model::Page>& pages, std::unordered_set<std::string>& ids)
    {
        for (const auto& page : pages) {
            addSphereKeys(page.items, false, ids);
            addSphereKeys(page.pages, ids);
        }
    }

    std::string readFile(const std::filesystem::path& path)
    {
        std::ostringstream text;
        text << std::ifstream(path, std::ios::binary).rdbuf();
        return text.str();
    }

    /**
     * Checks that a setting is kept in a value a TOML file has, and that its default is that value: a file
     * a mod ships has the values the mod has without their keys. A binding's table is the binding of its default.
     */
    void checkShippedDefault(const model::Setting& setting, const toml::table& shipped)
    {
        INFO(setting.id);
        toml::node_view<const toml::node> value(shipped);
        for (const auto& name : std::get<model::FileStorage>(setting.storage).path) {
            value = value[name];
        }
        REQUIRE(value);
        REQUIRE(setting.defaultValue);
        if (setting.type == model::SettingType::InputBinding) {
            CHECK(model::canonicalBinding(common::TomlEditor::Value(*value.node()).text(), model::TableForm::Toml) == std::get<std::string>(*setting.defaultValue));
        } else if (setting.type == model::SettingType::Color) {
            CHECK(model::canonicalColor(common::TomlEditor::Value(*value.node()).text(), model::TableForm::Toml) == std::get<std::string>(*setting.defaultValue));
        } else if (setting.type == model::SettingType::Transform) {
            CHECK(model::canonicalTransform(common::TomlEditor::Value(*value.node()).text(), model::TableForm::Toml) == std::get<std::string>(*setting.defaultValue));
        } else if (const auto* on = std::get_if<bool>(&*setting.defaultValue)) {
            CHECK(value.value<bool>() == *on);
        } else if (const auto* whole = std::get_if<int>(&*setting.defaultValue)) {
            CHECK(value.value<int>() == *whole);
        } else if (const auto* number = std::get_if<float>(&*setting.defaultValue)) {
            CHECK(common::fEqual(value.value<float>().value_or(NAN), *number));
        } else {
            CHECK(value.value<std::string>() == std::get<std::string>(*setting.defaultValue));
        }
    }
}

TEST_CASE("ConfigSpecLoader: a config spec is its mod, its front page and its pages, which nest, with their help")
{
    const auto mod = loaded(R"({
        "minSchemaVersion": 1,
        "modName": "MyMod",
        "displayName": "My Mod",
        "help": "Body tracking and holsters for VR.",
        "applies": "live",
        "content": [ { "type": "switcher", "text": "Holsters", "storage": { "file": "main", "path": ["Main", "bHolsters"] } } ],
        "pages": [
            {
                "pageDisplayName": "Body",
                "help": "Calibration, and where the body's parts are.",
                "content": [ { "type": "slider", "text": "Height", "storage": { "file": "main", "path": ["Body", "fHeight"] } } ],
                "pages": [ { "pageDisplayName": "Hands", "content": [] } ]
            },
            { "pageDisplayName": "Advanced", "content": [] }
        ]
    })");
    CHECK(mod.name == "MyMod");
    CHECK(mod.displayName == "My Mod");
    CHECK(mod.help == "Body tracking and holsters for VR.");
    CHECK(mod.source == model::Source::ConfigSpec);
    CHECK(mod.notAvailable.empty());

    CHECK(mod.frontPage.items.size() == 1);
    REQUIRE(mod.pages.size() == 2);
    CHECK(mod.pages[0].name == "Body");
    CHECK(mod.pages[0].help == "Calibration, and where the body's parts are.");
    CHECK(mod.pages[0].items.size() == 1);
    REQUIRE(mod.pages[0].pages.size() == 1);
    CHECK(mod.pages[0].pages[0].name == "Hands");
    CHECK(mod.pages[0].pages[0].help.empty());
    CHECK(mod.pages[1].name == "Advanced");
    CHECK(mod.pages[1].pages.empty());
}

TEST_CASE("ConfigSpecLoader: a section has the rows of its own content, and a section can be in a section")
{
    const auto mod = loaded(configSpecOf(R"(
        { "type": "switcher", "text": "Enable", "storage": { "path": ["Main", "bEnable"] } },
        {
            "type": "section", "text": "Holsters",
            "content": [
                { "type": "switcher", "text": "Enable holsters", "storage": { "path": ["Holsters", "bEnabled"] } },
                {
                    "type": "section", "text": "Advanced",
                    "content": [ { "type": "slider", "text": "Reach", "storage": { "path": ["Holsters", "fReach"] } } ]
                },
                { "type": "slider", "text": "Size", "storage": { "path": ["Holsters", "fSize"] } }
            ]
        },
        { "type": "section", "text": "Empty", "content": [] }
    )"));
    CHECK(describe(mod.frontPage) == Lines{
                                         "setting bEnable:Main: Enable",
                                         "section: Holsters",
                                         "  setting bEnabled:Holsters: Enable holsters",
                                         "  section: Advanced",
                                         "    setting fReach:Holsters: Reach",
                                         "  setting fSize:Holsters: Size",
                                         "section: Empty",
                                     });
}

TEST_CASE("ConfigSpecLoader: a section folds with isCollapsible, starts folded with collapsed, and has its help")
{
    const auto mod = loaded(configSpecOf(R"(
        { "type": "section", "text": "Plain", "content": [] },
        { "type": "section", "text": "Folds", "help": "Opens and closes.", "isCollapsible": true, "content": [] },
        { "type": "section", "text": "Folded", "isCollapsible": true, "collapsed": true, "content": [] },
        { "type": "section", "text": "Collapsed alone", "collapsed": true, "content": [] },
        { "type": "section", "text": "Not bools", "isCollapsible": "yes", "collapsed": 1, "content": [] }
    )"));

    struct Fold
    {
        std::string help;
        bool collapsible = false;
        bool collapsed = false;

        bool operator==(const Fold&) const = default;
    };

    std::vector<Fold> folds;
    for (const auto& item : mod.frontPage.items) {
        const auto& group = std::get<model::Group>(item);
        folds.push_back({ .help = group.help, .collapsible = group.collapsible, .collapsed = group.collapsed });
    }
    // a section that does not fold could not be opened, so it does not start folded
    CHECK(folds == std::vector<Fold>{ {}, { .help = "Opens and closes.", .collapsible = true }, { .collapsible = true, .collapsed = true }, {}, {} });
}

TEST_CASE("ConfigSpecLoader: a section's maxOn limits its switchers, in its nested sections too, and a switcher under one counts by its countsAs")
{
    const auto mod = loaded(configSpecOf(R"(
        { "type": "section", "text": "Wheel", "maxOn": 3, "content": [
            { "type": "switcher", "text": "Aid", "default": true, "storage": { "path": ["Wheel", "bAid"] } },
            { "type": "section", "text": "Hands", "content": [
                { "type": "switcher", "text": "Gestures", "countsAs": 2, "default": false, "storage": { "path": ["Wheel", "bGestures"] } } ] } ] }
    )"));
    CHECK(problemLines(mod).empty());
    const auto& wheel = std::get<model::Group>(mod.frontPage.items.at(0));
    CHECK(wheel.maxOn == 3);
    CHECK(std::get<model::Setting>(wheel.items.at(0)).countsAs == 1);
    const auto& hands = std::get<model::Group>(wheel.items.at(1));
    CHECK(hands.maxOn == 0);
    CHECK(std::get<model::Setting>(hands.items.at(0)).countsAs == 2);
}

TEST_CASE(
    "ConfigSpecLoader: a maxOn that is no whole number of 1 or more, with no switchers, or under its switchers' defaults is an error, and a countsAs with no maxOn is ignored")
{
    const auto mod = loaded(configSpecOf(R"(
        { "type": "section", "text": "Zero", "maxOn": 0, "content": [ { "type": "switcher", "text": "A", "default": false, "storage": { "path": ["S", "bA"] } } ] },
        { "type": "section", "text": "Half", "maxOn": 1.5, "content": [ { "type": "switcher", "text": "B", "default": false, "storage": { "path": ["S", "bB"] } } ] },
        { "type": "section", "text": "Sliders", "maxOn": 2, "content": [ { "type": "slider", "text": "C", "default": 1.0, "storage": { "path": ["S", "fC"] } } ] },
        { "type": "section", "text": "Defaults", "maxOn": 2, "content": [
            { "type": "switcher", "text": "D", "default": true, "storage": { "path": ["S", "bD"] } },
            { "type": "switcher", "text": "E", "default": true, "countsAs": 2, "storage": { "path": ["S", "bE"] } } ] },
        { "type": "section", "text": "Counts", "maxOn": 4, "content": [
            { "type": "switcher", "text": "F", "default": false, "countsAs": 0, "storage": { "path": ["S", "bF"] } },
            { "type": "slider", "text": "G", "default": 1.0, "countsAs": 2, "storage": { "path": ["S", "fG"] } } ] },
        { "type": "switcher", "text": "H", "default": false, "countsAs": 2, "storage": { "path": ["S", "bH"] } }
    )"));
    CHECK(problemLines(mod) == Lines{
                                   "E Zero: Its maxOn 0 is not a whole number of 1 or more: it has no limit. [Zero] content[0].maxOn",
                                   "E Half: Its maxOn 1.5 is not a whole number of 1 or more: it has no limit. [Half] content[1].maxOn",
                                   "E Sliders: It has a maxOn and no switchers: it has nothing to limit. [Sliders] content[2].maxOn",
                                   "E Defaults: Its switchers' defaults take 3 places of its maxOn 2: Reset leaves it over its limit. [Defaults] content[3].maxOn",
                                   "E F: Its countsAs 0 is not a whole number of 1 or more: it counts 1. [F] content[4].content[0].countsAs",
                                   "W G: It has a countsAs and is no switcher: it is ignored. [G] content[4].content[1].countsAs",
                                   "W H: It has a countsAs and no section above it has a maxOn: it is ignored. [H] content[5].countsAs",
                               });
    CHECK(std::get<model::Group>(mod.frontPage.items.at(0)).maxOn == 0);
    CHECK(std::get<model::Group>(mod.frontPage.items.at(1)).maxOn == 0);
    // one with nothing to limit, or under its defaults, still limits
    CHECK(std::get<model::Group>(mod.frontPage.items.at(3)).maxOn == 2);
    CHECK(std::get<model::Setting>(mod.frontPage.items.at(5)).countsAs == 1);
}

TEST_CASE("ConfigSpecLoader: a switcher, a slider, a choice and a text input are settings, and any other row is a placeholder with its type")
{
    const auto mod = loaded(configSpecOf(R"(
        { "type": "switcher", "text": "Enable", "help": "Turns it on.", "valueType": "bool", "storage": { "path": ["Main", "bEnable"] } },
        { "type": "slider", "text": "Opacity", "valueType": "int", "valueOptions": { "min": 0, "max": 255, "step": 5 }, "storage": { "path": ["Main", "iOpacity"] } },
        { "type": "textinput", "text": "Name", "storage": { "path": ["Main", "sName"] } },
        { "type": "textinput", "text": "Scale", "valueType": "float", "storage": { "path": ["Main", "fScale"] } },
        { "type": "transform", "text": "Offset", "help": "Where it is." },
        { "type": "color", "text": "Tint" },
        { "type": "stepper", "text": "Hand", "valueOptions": { "options": ["Left", "Right"] }, "storage": { "path": ["Main", "iHand"] } },
        { "type": "dropdown", "text": "Where", "valueOptions": { "options": ["Head", "Hand"] }, "storage": { "path": ["Main", "iWhere"] } }
    )"));
    const auto& items = mod.frontPage.items;
    REQUIRE(items.size() == 8);

    const auto& enable = std::get<model::Setting>(items[0]);
    CHECK(enable.type == model::SettingType::Switcher);
    CHECK(enable.help == "Turns it on.");
    CHECK(enable.valueType == model::StoredType::Bool);
    CHECK(enable.sourceId.empty());

    const auto& opacity = std::get<model::Setting>(items[1]);
    CHECK(opacity.type == model::SettingType::Slider);
    CHECK(opacity.valueType == model::StoredType::Int);
    CHECK(common::fEqual(opacity.valueOptions.min, 0.0f));
    CHECK(common::fEqual(opacity.valueOptions.max, 255.0f));
    CHECK(common::fEqual(opacity.valueOptions.step, 5.0f));

    // with no valueType, the type of its control
    CHECK(std::get<model::Setting>(items[2]).valueType == model::StoredType::String);
    CHECK(std::get<model::Setting>(items[3]).valueType == model::StoredType::Float);

    const auto& offset = std::get<model::Placeholder>(items[4]);
    CHECK(offset.type == "transform");
    CHECK(offset.text == "Offset");
    CHECK(offset.help == "Where it is.");
    CHECK(std::get<model::Placeholder>(items[5]).type == "color");

    // a choice with no valueType stores the place of its option
    CHECK(std::get<model::Setting>(items[6]).type == model::SettingType::Stepper);
    CHECK(std::get<model::Setting>(items[6]).valueType == model::StoredType::Int);
    CHECK(std::get<model::Setting>(items[7]).type == model::SettingType::Dropdown);
    CHECK(std::get<model::Setting>(items[7]).valueType == model::StoredType::Int);
}

TEST_CASE("ConfigSpecLoader: HTML labels style every setting type after translation")
{
    model::Translations translations;
    translations.add("$MyMod_Label", "<font size='16' color='#FFBF33'><b><i><u>A styled &amp; label</u></i></b></font>");
    const std::vector<model::TextRun> expected{ { .text = "A styled & label", .size = 16, .color = 0xFFBF33, .bold = true, .italic = true, .underline = true } };
    for (const auto* type : { "switcher", "slider", "stepper", "dropdown", "textinput" }) {
        INFO(type);
        const bool switcher = std::string_view(type) == "switcher";
        const auto mod = loaded(configSpecOf(std::format(R"({{
            "type": "{}", "text": {{ "token": "$MyMod_Label", "fallback": "Unused" }}, "html": true,
            "valueType": "{}", "default": {}, "storage": {{ "path": ["Main", "value"] }},
            "valueOptions": {{ "min": 0, "max": 10, "step": 1, "options": ["Low", "High"] }}
        }})",
                                    type,
                                    switcher ? "bool" : "int",
                                    switcher ? "true" : "1")),
            translations);
        const auto setting = onlySetting(mod);
        CHECK(setting.text == "A styled & label");
        CHECK(setting.textRuns == expected);
        CHECK(setting.id == "value:Main");
    }
}

TEST_CASE("ConfigSpecLoader: button and unsupported labels take HTML only with a boolean true flag")
{
    const auto mod = loaded(configSpecOf(R"(
        { "type": "button", "text": "<p align='right'><b>First</b></p>Second<br>Third &amp; last", "html": true,
          "buttonText": "<b>Run</b>", "help": "<b>Help</b>",
          "action": { "type": "RunConsoleCommand", "command": "help showcase" } },
        { "type": "color", "text": "<u>Tint</u>", "html": true },
        { "type": "switcher", "text": "<b>Missing</b>", "default": true, "storage": { "path": ["Main", "a"] } },
        { "type": "slider", "text": "<i>False</i>", "html": false, "default": 1, "storage": { "path": ["Main", "b"] } },
        { "type": "textinput", "text": "<u>String</u>", "html": "true", "default": "", "storage": { "path": ["Main", "c"] } },
        { "type": "dropdown", "text": "<b>Number</b>", "html": 1, "default": 0,
          "valueOptions": { "options": ["Only"] }, "storage": { "path": ["Main", "d"] } }
    )"));
    const auto& items = mod.frontPage.items;
    REQUIRE(items.size() == 6);
    const auto& button = std::get<model::Button>(items[0]);
    CHECK(button.text == "First Second Third & last");
    REQUIRE_FALSE(button.textRuns.empty());
    CHECK(button.textRuns[0].bold);
    CHECK(button.buttonText == "<b>Run</b>");
    CHECK(button.help == "<b>Help</b>");
    const auto& placeholder = std::get<model::Placeholder>(items[1]);
    CHECK(placeholder.text == "Tint");
    REQUIRE(placeholder.textRuns.size() == 1);
    CHECK(placeholder.textRuns[0].underline);
    const Lines plain{ "<b>Missing</b>", "<i>False</i>", "<u>String</u>", "<b>Number</b>" };
    for (std::size_t i = 0; i < plain.size(); ++i) {
        const auto& setting = std::get<model::Setting>(items[i + 2]);
        CHECK(setting.text == plain[i]);
        CHECK(setting.textRuns.empty());
    }
}

TEST_CASE("ConfigSpecLoader: a text, a spacer and an image are rows that hold no value")
{
    model::Translations translations;
    translations.add("$MyMod_Note", "A translated note.");
    const auto mod = loaded(configSpecOf(R"(
        { "type": "text", "text": "A plain text with <b>a tag</b>.\nIts second line." },
        { "type": "text", "text": "<p align='center'>A <b>bold</b> word</p>", "html": true },
        { "type": "text", "text": "$MyMod_Note" },
        { "type": "text", "text": { "token": "$MyMod_Missing", "fallback": "A fallback." }, "html": "yes" },
        { "type": "spacer" },
        { "type": "spacer", "numLines": 3 },
        { "type": "spacer", "numLines": 100 },
        { "type": "spacer", "numLines": -2 },
        { "type": "image", "texture": "MyMod/config/logo.dds", "width": 512, "height": 128 },
        { "type": "image", "texture": "MyMod/config/logo.dds", "width": 256 },
        { "type": "image", "texture": "MyMod/config/logo.dds", "width": -5, "height": "big" },
        { "type": "image" }
    )"),
        translations);
    const auto& items = mod.frontPage.items;
    REQUIRE(items.size() == 12);

    // a text is plain where its html is not true, so a tag in it is shown as it is written
    CHECK(std::get<model::Text>(items[0]).lines == model::plainLines("A plain text with <b>a tag</b>.\nIts second line."));
    CHECK(std::get<model::Text>(items[1]).lines == model::parseMarkup("<p align='center'>A <b>bold</b> word</p>"));
    CHECK(std::get<model::Text>(items[2]).lines == model::plainLines("A translated note."));
    CHECK(std::get<model::Text>(items[3]).lines == model::plainLines("A fallback."));

    // one line without numLines, and from 1 to 20
    CHECK(std::get<model::Spacer>(items[4]).lines == 1);
    CHECK(std::get<model::Spacer>(items[5]).lines == 3);
    CHECK(std::get<model::Spacer>(items[6]).lines == 20);
    CHECK(std::get<model::Spacer>(items[7]).lines == 1);

    // a side that is not given, or is not more than 0, is 0: the texture's own, or of its aspect
    const auto image = [&items](const std::size_t place) {
        return std::get<model::Image>(items[place]);
    };
    CHECK_FALSE(image(8).pixels);
    CHECK(image(8).texture == "MyMod/config/logo.dds");
    CHECK(common::fEqual(image(8).width, 512.0f));
    CHECK(common::fEqual(image(8).height, 128.0f));
    CHECK(common::fEqual(image(9).width, 256.0f));
    CHECK(common::fEqual(image(9).height, 0.0f));
    CHECK(common::fEqual(image(10).width, 0.0f));
    CHECK(common::fEqual(image(10).height, 0.0f));
    CHECK(image(11).texture.empty());
}

TEST_CASE("ConfigSpecLoader: a config spec with no applies, or one that is not one, has settings whose applies is unknown")
{
    for (const auto* root : { "", R"("applies": "never",)" }) {
        const auto mod = loaded(std::format(R"({{
            "minSchemaVersion": 1, "modName": "MyMod", {}
            "files": {{ "main": {{ "format": "toml", "root": "myGames", "path": "MyMod.toml" }} }},
            "defaultFile": "main",
            "content": [ {{ "type": "switcher", "text": "A", "storage": {{ "path": ["Main", "a"] }} }} ]
        }})",
            root));
        REQUIRE(settingsOf(mod).size() == 1);
        CHECK(settingsOf(mod)[0].applies == model::Applies::Unknown);
    }
}

TEST_CASE("ConfigSpecLoader: a config spec that cannot be read says why")
{
    CHECK(spec::loadConfigSpec("{ not json").error().starts_with("Not valid JSON"));
    CHECK(spec::loadConfigSpec("[]").error() == "Not a JSON object");
    CHECK(spec::loadConfigSpec(R"({ "applies": "live", "content": [] })").error() == "No modName");
}

TEST_CASE("ConfigSpecLoader: a mod with no displayName is named by its modName, and comments are skipped")
{
    const auto mod = loaded(R"({
        // a comment
        "minSchemaVersion": 1, "modName": "MyMod", "applies": "save", "content": []
    })");
    CHECK(mod.displayName == "MyMod");
    CHECK(mod.help.empty());
}

TEST_CASE("ConfigSpecLoader: a text is plain, a token, or a token with a fallback")
{
    model::Translations translations;
    translations.add("$MyMod_Name", "Mon mod");
    translations.add("$MyMod_Height", "Hauteur");

    const auto mod = loaded(R"({
        "minSchemaVersion": 1, "modName": "MyMod", "applies": "live",
        "displayName": "$MyMod_Name",
        "help": { "token": "$MyMod_Help", "fallback": "Body tracking." },
        "files": { "main": { "format": "toml", "root": "myGames", "path": "MyMod.toml" } },
        "defaultFile": "main",
        "content": [
            { "type": "section", "text": { "token": "$MyMod_Body", "fallback": "Body" }, "content": [
                { "type": "slider", "text": { "token": "$MyMod_Height", "fallback": "Height" }, "help": "Plain help.", "storage": { "path": ["Body", "fHeight"] } },
                { "type": "slider", "text": "$MyMod_Missing", "storage": { "path": ["Body", "fWidth"] } },
                { "type": "slider", "text": 12, "storage": { "path": ["Body", "fDepth"] } }
            ] }
        ],
        "pages": [ { "pageDisplayName": { "token": "$MyMod_Advanced", "fallback": "Advanced" }, "help": "$MyMod_Missing", "content": [] } ]
    })",
        translations);
    CHECK(mod.displayName == "Mon mod");
    CHECK(mod.help == "Body tracking.");
    CHECK(describe(mod.frontPage) == Lines{ "section: Body", "  setting fHeight:Body: Hauteur", "  setting fWidth:Body: $MyMod_Missing", "  setting fDepth:Body: " });
    CHECK(std::get<model::Setting>(std::get<model::Group>(mod.frontPage.items[0]).items[0]).help == "Plain help.");
    CHECK(mod.pages[0].name == "Advanced");
    CHECK(mod.pages[0].help == "$MyMod_Missing");
}

TEST_CASE("ConfigSpecLoader: the config spec of every folder in the config specs' folder is loaded")
{
    const test_files::TempFolder folder;
    folder.write("MyMod/spec.json", configSpecOf(""));
    folder.write("Other/spec.json", R"({ "minSchemaVersion": 1, "modName": "Other", "applies": "save", "content": [] })");
    folder.write("Broken/spec.json", "{ not json");
    folder.write("NoApplies/spec.json", R"({ "minSchemaVersion": 1, "modName": "NoApplies", "content": [] })");
    folder.write("NotAConfigSpec/readme.txt", "nothing");
    folder.write("spec.json", "{}");

    auto mods = spec::loadConfigSpecs(folder.path);
    std::ranges::sort(mods, {}, &model::Mod::name);
    Lines names;
    for (const auto& mod : mods) {
        names.push_back(mod.name + (mod.notAvailable.empty() ? "" : ": " + mod.notAvailable));
        CHECK(mod.source == model::Source::ConfigSpec);
    }
    // one that cannot be read is named by its folder
    CHECK(names == Lines{ "Broken: its spec.json cannot be read", "MyMod", "NoApplies", "Other" });
}

TEST_CASE("ConfigSpecLoader: two config specs with one modName, in any case, are both not available")
{
    const test_files::TempFolder folder;
    folder.write("First/spec.json", R"({ "minSchemaVersion": 1, "modName": "MyMod", "displayName": "My Mod", "applies": "live", "content": [ { "type": "spacer" } ] })");
    folder.write("Second/spec.json", R"({ "minSchemaVersion": 1, "modName": "MYMOD", "displayName": "My Other Mod", "applies": "live", "content": [] })");
    folder.write("Third/spec.json", R"({ "minSchemaVersion": 1, "modName": "Other", "applies": "live", "content": [] })");

    auto mods = spec::loadConfigSpecs(folder.path);
    std::ranges::sort(mods, {}, &model::Mod::displayName);
    REQUIRE(mods.size() == 3);
    CHECK(mods[0].displayName == "My Mod");
    CHECK(mods[0].notAvailable == "another config spec has its modName");
    CHECK(mods[0].frontPage.items.empty());
    CHECK(mods[1].displayName == "My Other Mod");
    CHECK(mods[1].notAvailable == "another config spec has its modName");
    CHECK(mods[2].notAvailable.empty());
}

TEST_CASE("ConfigSpecLoader: a mod with an MCM menu and a config spec is listed by its config spec alone")
{
    const auto modOf = [](std::string name, const model::Source source) {
        return model::Mod{ .name = std::move(name), .displayName = {}, .frontPage = {}, .pages = {}, .source = source };
    };
    std::vector<model::Mod> mcmMods;
    mcmMods.push_back(modOf("FRIK", model::Source::Mcm));
    mcmMods.push_back(modOf("Other", model::Source::Mcm));
    mcmMods.push_back(modOf("Broken", model::Source::Mcm));
    std::vector<model::Mod> configSpecs;
    configSpecs.push_back(modOf("frik", model::Source::ConfigSpec));
    configSpecs.push_back(modOf("Mine", model::Source::ConfigSpec));
    // one that is not available too
    configSpecs.push_back(modOf("Broken", model::Source::ConfigSpec));
    configSpecs.back().notAvailable = "its spec.json cannot be read";

    Lines names;
    for (const auto& mod : spec::withConfigSpecs(std::move(mcmMods), std::move(configSpecs))) {
        names.push_back(mod.name + (mod.source == model::Source::Mcm ? " (MCM)" : ""));
    }
    CHECK(names == Lines{ "Other (MCM)", "frik", "Mine", "Broken" });
}

TEST_CASE("ConfigSpecLoader: with no config specs' folder there are no mods")
{
    const test_files::TempFolder folder;
    CHECK(spec::loadConfigSpecs(folder.path / "missing").empty());
}

TEST_CASE("ConfigSpecLoader: the mod's own config spec edits keys of its shipped TOML, with the defaults it ships")
{
    const auto mods = spec::loadConfigSpecs(MODS_CONFIG_SPECS_DIR);
    REQUIRE(mods.size() == 1);
    const auto& mod = mods[0];
    CHECK(mod.notAvailable.empty());
    CHECK(mod.name == "ModsConfigFO4VR");
    // the file the menu loaded, which the framework's client answers
    CHECK(mod.files.at("config") == model::File{ .format = model::FileFormat::Toml,
                                        .root = model::FileRoot::MyGames,
                                        .path = "Mods_Config/ModsConfigFO4VR/ModsConfigFO4VR.toml",
                                        .pathFromClientState = "configFile" });

    const auto shipped = toml::parse(readFile(MODS_CONFIG_SHIPPED_CONFIG));
    std::unordered_set<std::string> ids;
    for (const auto& setting : settingsOf(mod)) {
        INFO(setting.id);
        CHECK(ids.insert(setting.id).second);
        CHECK_FALSE(setting.text.empty());
        CHECK_FALSE(setting.help.empty());
        const auto& storage = std::get<model::FileStorage>(setting.storage);
        CHECK(storage.file == "config");
        CHECK(storage.path.size() == 2);
        checkShippedDefault(setting, shipped);
    }
    CHECK(ids.size() >= 10);
}

TEST_CASE("ConfigSpecLoader: the config spec showcase is loaded, and its shipped file has the default of each of its settings")
{
    const auto mods = spec::loadConfigSpecs(MODS_CONFIG_SPEC_SHOWCASE_DIR);
    REQUIRE(mods.size() == 1);
    const auto& mod = mods[0];
    CHECK(mod.notAvailable.empty());
    CHECK(mod.name == "ModsConfigFO4_Showcase");
    CHECK(mod.displayName == "Mods Config FO4: Showcase");
    CHECK_FALSE(mod.help.empty());
    CHECK_FALSE(mod.frontPage.items.empty());
    REQUIRE(mod.pages.size() == 2);
    CHECK(mod.pages[0].name == "General");
    const auto& examples = mod.pages[1];
    CHECK(examples.name == "Detailed Examples");
    CHECK(examples.pages.size() == 27);

    // the file whose path goes up out of its root is left out
    Lines files;
    for (const auto& [name, file] : mod.files) {
        files.push_back(name);
    }
    CHECK(files == Lines{ "developer", "ini", "invalid", "player", "showcase" });
    CHECK(mod.files.at("developer").omitDefault);

    // a page has pages under it, two levels deep
    const auto pages = std::ranges::find(examples.pages, "Pages", &model::Page::name);
    REQUIRE(pages != examples.pages.end());
    REQUIRE(pages->pages.size() == 2);
    CHECK(pages->pages[0].pages.size() == 1);

    // the settings of the shipped file, but for those with no default: a choice with no options has none either,
    // and a read-only setting needs none. The default of an activation sphere's key is the framework's, and a sphere
    // leaves out most of its keys, as a mod's does. A choice that sets other settings has no value of its own, and one
    // that omits its default is not in its file at its default
    const auto shipped = toml::parse(readFile(std::filesystem::path(MODS_CONFIG_SPEC_SHOWCASE_DIR) / "ModsConfigFO4_Showcase" / "showcase.toml"));
    Lines withNoDefault;
    std::size_t checked = 0;
    std::size_t readOnly = 0;
    Lines omitted;
    std::unordered_set<std::string> sphereKeys;
    addSphereKeys(mod.pages, sphereKeys);
    for (const auto& setting : settingsOf(mod)) {
        if (sphereKeys.contains(setting.id) || model::setsOthers(setting)) {
            continue;
        }
        if (setting.omitDefault) {
            omitted.push_back(setting.text);
            continue;
        }
        if (setting.readOnly) {
            ++readOnly;
            continue;
        }
        if (!setting.defaultValue) {
            withNoDefault.push_back(setting.text);
            continue;
        }
        const auto* storage = std::get_if<model::FileStorage>(&setting.storage);
        if (storage && storage->file == "showcase") {
            checkShippedDefault(setting, shipped);
            ++checked;
        }
    }
    CHECK(withNoDefault == Lines{ "No default",
                               "A default of another type",
                               "A default outside its range",
                               "A binding's default it does not take",
                               "A slow binding in an INI file",
                               "A haptic's default that is no pattern",
                               "A color's default that is no color",
                               "A transform's default that is no transform",
                               "No options",
                               "No such shared list",
                               "A default that is no option" });
    CHECK(checked >= 30);
    CHECK(readOnly == 10);
    CHECK(omitted == Lines{ "Kept only while changed", "Optional number", "Changed number", "Default switcher", "Changed color" });
}

TEST_CASE("ConfigSpecLoader: a problem names its row, the pages and the row it is on, and its path in spec.json")
{
    const auto mod = loaded(R"({
        "minSchemaVersion": 1, "modName": "MyMod", "applies": "live",
        "files": { "main": { "format": "toml", "root": "myGames", "path": "MyMod.toml" }, "bad": { "format": "yaml", "root": "data", "path": "a.yaml" } },
        "defaultFile": "main",
        "content": [ { "type": "switcher", "text": "On the front page", "default": true } ],
        "pages": [
            {
                "pageDisplayName": "Beam", "visibleWen": true,
                "content": [ { "type": "section", "text": "Shape", "content": [ { "type": "slider", "text": "Beam angle", "default": 1.0 } ] } ],
                "pages": [ { "pageDisplayName": "Color", "content": [ { "type": "spacer", "lines": 2 } ] } ]
            }
        ]
    })");
    CHECK(problemLines(mod) == Lines{ "E On the front page: It has no storage: it is not available. [On the front page] content[0]",
                                   "E spec.json: The file \"bad\" is left out: its format 'yaml' is not ini or toml. Its settings are not available. [] files.bad",
                                   "E Beam angle: It has no storage: it is not available. [Beam \xE2\x80\xBA Beam angle] pages[0].content[0].content[0]",
                                   "W spacer: Unknown field \"lines\": it is not read. [Beam \xE2\x80\xBA Color \xE2\x80\xBA spacer] pages[0].pages[0].content[0].lines",
                                   "W Beam: Unknown field \"visibleWen\": it is not read. Did you mean \"visibleWhen\"? [Beam] pages[0].visibleWen" });
}

TEST_CASE("ConfigSpecLoader: a field the schema does not have is a warning, with the field it is near, and one the menu does not read yet is none")
{
    const auto mod = loaded(R"({
        "minSchemaVersion": 1, "modName": "MyMod", "applies": "live", "category": "Visuals", "specVersion": 3, "Help": "Near help",
        "files": { "main": { "format": "toml", "root": "myGames", "path": "MyMod.toml", "encoding": "utf-8" } },
        "defaultFile": "main",
        "sharedLists": { "hands": [ { "value": 0, "text": "Left", "texts": "L" } ] },
        "content": [
            { "type": "stepper", "text": "Hand", "default": 0, "valueOptions": { "sharedOptions": "hands", "wrap": true }, "storage": { "path": ["a"], "fil": "main" } },
            { "type": "dropdown", "valueType": "string", "text": "Place", "default": "hip",
              "valueOptions": { "options": [ { "value": "hip", "text": "Hip", "help": "At the hip.", "icon": "hip.dds" } ] },
              "display": { "decimal": 2 }, "storage": { "path": ["b"] },
              "warnings": [ { "when": { "setting": "a", "equals": 1 }, "text": "Left-handed.", "severity": "high" } ] },
            { "type": "button", "text": "Reset", "action": { "type": "RunConsoleCommand", "command": "tm", "comand": "tm" },
              "confirmation": { "title": "Sure?", "dangerous": true, "dangerus": true } },
            { "type": "text", "text": { "token": "$Missing", "fallback": "Shown", "fallbak": "x" } }
        ]
    })");
    CHECK(problemLines(mod) == Lines{ "W spec.json: Unknown field \"Help\": it is not read. Did you mean \"help\"? [] Help",
                                   "W Hand: Unknown field \"fil\": it is not read. Did you mean \"file\"? [Hand] content[0].storage.fil",
                                   "W Hand: Unknown field \"wrap\": it is not read. [Hand] content[0].valueOptions.wrap",
                                   "W Place: Unknown field \"decimal\": it is not read. Did you mean \"decimals\"? [Place] content[1].display.decimal",
                                   "W Place: Unknown field \"icon\": it is not read. [Place] content[1].valueOptions.options[0].icon",
                                   "W Place: Unknown field \"severity\": it is not read. [Place] content[1].warnings[0].severity",
                                   "W Reset: Unknown field \"comand\": it is not read. Did you mean \"command\"? [Reset] content[2].action.comand",
                                   "W Reset: Unknown field \"dangerus\": it is not read. Did you mean \"dangerous\"? [Reset] content[2].confirmation.dangerus",
                                   "W Shown: Unknown field \"fallbak\": it is not read. Did you mean \"fallback\"? [Shown] content[3].text.fallbak",
                                   "W spec.json: Unknown field \"encoding\": it is not read. [] files.main.encoding",
                                   "W spec.json: Unknown field \"texts\": it is not read. Did you mean \"text\"? [] sharedLists.hands[0].texts" });
}

TEST_CASE("ConfigSpecLoader: a token with no translation and no fallback is a warning, and is shown as it is written")
{
    model::Translations translations;
    translations.add("$MyMod_Known", "Known");
    const auto mod = loaded(configSpecOf(R"(
        { "type": "text", "text": "$MyMod_Known" },
        { "type": "text", "text": { "token": "$MyMod_Fallback", "fallback": "With a fallback" } },
        { "type": "text", "text": "Plain" },
        { "type": "switcher", "text": "$MyMod_Missing", "help": "$MyMod_MissingHelp", "default": true, "storage": { "path": ["a"] } }
    )"),
        translations);
    CHECK(std::get<model::Setting>(mod.frontPage.items[3]).text == "$MyMod_Missing");
    CHECK(problemLines(mod) ==
          Lines{ "W $MyMod_Missing: The token $MyMod_MissingHelp has no translation and no fallback: it is shown as it is written. [$MyMod_Missing] content[3].help",
              "W $MyMod_Missing: The token $MyMod_Missing has no translation and no fallback: it is shown as it is written. [$MyMod_Missing] content[3].text" });
}

TEST_CASE("ConfigSpecLoader: a config spec's problems are in the order of its spec.json, its conditions with its rows")
{
    std::string rows;
    for (int place = 0; place < 11; ++place) {
        rows += std::format(R"({{ "type": "spacer"{} }},)", place == 2 || place == 10 ? R"(, "visibleWhen": { "setting": "none", "equals": 1 })" : "");
    }
    rows += R"({ "type": "spacer", "height": 1 })";
    Lines paths;
    for (const auto& problem : loaded(configSpecOf(rows)).problems) {
        paths.push_back(problem.path);
    }
    CHECK(paths == Lines{ "content[2].visibleWhen", "content[10].visibleWhen", "content[11].height" });
}

TEST_CASE("ConfigSpecLoader: a config spec that cannot be read, and two with one modName, have their problem")
{
    const test_files::TempFolder folder;
    folder.write("Broken/spec.json", "{ not json");
    folder.write("First/spec.json", R"({ "minSchemaVersion": 1, "modName": "MyMod", "applies": "live", "content": [] })");
    folder.write("Second/spec.json", R"({ "minSchemaVersion": 1, "modName": "mymod", "applies": "live", "content": [] })");

    auto mods = spec::loadConfigSpecs(folder.path);
    std::ranges::sort(mods, {}, &model::Mod::name);
    REQUIRE(mods.size() == 3);
    REQUIRE(mods[0].problems.size() == 1);
    CHECK(mods[0].problems[0].row == "spec.json");
    CHECK(mods[0].problems[0].message.starts_with("It cannot be read: Not valid JSON: "));
    for (const std::size_t place : { 1u, 2u }) {
        CHECK(problemLines(mods[place]) == Lines{ std::format("E spec.json: Another config spec has the modName \"{}\": neither is read. [] modName", mods[place].name) });
    }
}

TEST_CASE("ConfigSpecLoader: the mod's own config spec has no problems, and the config spec showcase's are on its Mistakes page")
{
    const auto own = spec::loadConfigSpecs(MODS_CONFIG_SPECS_DIR);
    REQUIRE(own.size() == 1);
    CHECK(problemLines(own[0]).empty());

    const auto showcase = spec::loadConfigSpecs(MODS_CONFIG_SPEC_SHOWCASE_DIR);
    REQUIRE(showcase.size() == 1);
    Lines elsewhere;
    std::size_t errors = 0;
    std::size_t warnings = 0;
    for (const auto& problem : showcase[0].problems) {
        if (!problem.where.starts_with("Detailed Examples \xE2\x80\xBA Mistakes \xE2\x80\xBA ")) {
            elsewhere.push_back(problem.path);
        }
        (problem.severity == model::Severity::Error ? errors : warnings) += 1;
    }
    // its file that goes up out of its folder, and its text of a token with no fallback, which the Texts page shows
    CHECK(elsewhere == Lines{ "files.outside", "pages[1].pages[25].content[0].content[2].text" });
    CHECK(errors >= 20);
    CHECK(warnings >= 10);
}

TEST_CASE("ConfigSpecLoader: a problem on a row has the row's place, by its page and its items, and the folder of a config spec is kept")
{
    const test_files::TempFolder folder;
    folder.write("MyFolder/spec.json", R"({
        "minSchemaVersion": 1, "modName": "MyMod", "applies": "live", "Typo": 1,
        "content": [ { "type": "spacer" }, { "type": "spacer", "lines": 1 } ],
        "pages": [
            { "pageDisplayName": "One", "content": [ { "type": "spacer" } ] },
            {
                "pageDisplayName": "Two", "visibleWen": 1,
                "content": [ { "type": "spacer" } ],
                "pages": [ { "pageDisplayName": "Under", "content": [
                    { "type": "text", "text": "A" },
                    { "type": "section", "text": "Section", "content": [ { "type": "spacer" }, { "type": "switcher", "text": "No storage", "default": true } ] }
                ] } ]
            }
        ]
    })");
    const auto mods = spec::loadConfigSpecs(folder.path);
    REQUIRE(mods.size() == 1);
    CHECK(mods[0].folder == "MyFolder");
    Lines places;
    for (const auto& problem : mods[0].problems) {
        std::string line = problem.path + ":";
        if (problem.place) {
            for (const auto page : problem.place->page) {
                line += std::format(" p{}", page);
            }
            for (const auto item : problem.place->item) {
                line += std::format(" i{}", item);
            }
        } else {
            line += " none";
        }
        places.push_back(line);
    }
    CHECK(places == Lines{ "Typo: none", "content[1].lines: i1", "pages[1].pages[0].content[1].content[1]: p1 p0 i1 i1", "pages[1].visibleWen: none" });
}

TEST_CASE(
    "ConfigSpecLoader: a section, a sphere and a button have the id their client is told, a sphere its section's with none, and one a setting or an item before has is an error")
{
    const auto mod = loaded(sphereSpec(R"(
        { "type": "section", "id": "holsters", "text": "Holsters", "content": [
            { "type": "button", "id": "test", "text": "Test", "action": { "type": "RunConsoleCommand", "command": "tm" } }
        ] },
        { "type": "cf.activationSphere", "text": "Body grab", "storage": { "path": ["Spheres", "Body"] } },
        { "type": "cf.activationSphere", "id": "head", "text": "Head", "storage": { "path": ["Head"] } },
        { "type": "section", "text": "No id", "content": [] },
        { "type": "section", "id": "BENABLED:MAIN", "text": "A setting's", "content": [] },
        { "type": "button", "id": "Holsters", "text": "A section's", "action": { "type": "RunConsoleCommand", "command": "tm" } },
        { "type": "switcher", "text": "On", "default": false, "storage": { "path": ["Main", "bEnabled"] } })"));
    const auto& items = mod.frontPage.items;
    REQUIRE(items.size() == 7);
    const auto& holsters = std::get<model::Group>(items[0]);
    CHECK(holsters.id == "holsters");
    CHECK(std::get<model::Button>(holsters.items[0]).id == "test");
    CHECK(std::get<model::Group>(items[1]).id == "Spheres.Body");
    CHECK(std::get<model::Group>(items[2]).id == "head");
    CHECK(std::get<model::Group>(items[3]).id.empty());
    CHECK(std::get<model::Group>(items[4]).id.empty());
    CHECK(std::get<model::Button>(items[5]).id.empty());
    CHECK(std::get<model::Setting>(items[6]).id == "bEnabled:Main");
    CHECK(problemLines(mod) ==
          Lines{ "E A setting's: The setting \"bEnabled:Main\" has its id: the setting keeps it, and this tells its mod's client nothing. Give it an id of its own. [A setting's] "
                 "content[4]",
              "E A section's: A section or a button before it has the id \"Holsters\": this tells its mod's client nothing. Give it an id of its own. [A section's] content[5]" });
}

TEST_CASE("ConfigSpecLoader: a config spec's category is read as it is written, also of a mod that is not available, and one that is no plain name is a warning")
{
    const auto named = loaded(R"({ "minSchemaVersion": 1, "modName": "MyMod", "category": " Gameplay " })");
    CHECK(named.category == "Gameplay");
    CHECK(named.problems.empty());

    const auto none = loaded(R"({ "minSchemaVersion": 1, "modName": "MyMod" })");
    CHECK(none.category.empty());
    CHECK(none.problems.empty());

    for (const auto* category : { R"("$Gameplay")", "3", R"("  ")" }) {
        const auto mod = loaded(std::format(R"({{ "minSchemaVersion": 1, "modName": "MyMod", "category": {} }})", category));
        CHECK(mod.category.empty());
        REQUIRE(mod.problems.size() == 1);
        CHECK(mod.problems[0].severity == model::Severity::Warning);
        CHECK(mod.problems[0].path == "category");
    }

    const auto newer = loaded(R"({ "minSchemaVersion": 99, "modName": "MyMod", "category": "Combat" })");
    CHECK(!newer.notAvailable.empty());
    CHECK(newer.category == "Combat");
}

TEST_CASE("ConfigSpecLoader: the mod's own config spec has the Categories page the menu fills, and the file of the player's categories")
{
    auto mods = spec::loadConfigSpecs(MODS_CONFIG_SPECS_DIR);
    REQUIRE(mods.size() == 1);
    auto& mod = mods[0];
    CHECK(mod.problems.empty());
    CHECK(mod.category == "Interface");
    CHECK(mod.files.at(model::CATEGORIES_FILE) ==
          model::File{ .format = model::FileFormat::Toml, .root = model::FileRoot::MyGames, .path = "Mods_Config/ModsConfigFO4VR/Categories.toml", .pathFromClientState = {} });

    REQUIRE(model::addCategoryRows(mod, {}, {}));
    const auto page = std::ranges::find(mod.pages, model::CATEGORIES_PAGE_ID, &model::Page::id);
    REQUIRE(page != mod.pages.end());
    CHECK(page->name == "Categories");
    // its text, and the section of the rows, which has the mod's own
    REQUIRE(page->items.size() == 2);
    CHECK(std::get<model::Group>(page->items[1]).items.size() == 1);
}

TEST_CASE("ConfigSpecLoader: a condition and an option's sets on the front page name a setting on a page")
{
    const auto mod = loaded(R"({
        "minSchemaVersion": 1, "modName": "MyMod", "displayName": "My Mod", "applies": "live",
        "files": { "main": { "format": "toml", "root": "myGames", "path": "Mods_Config/MyMod/MyMod.toml" } },
        "defaultFile": "main",
        "content": [
            { "type": "slider", "valueType": "float", "id": "fHeight", "text": "Height", "default": 1.0, "valueOptions": { "min": 0.5, "max": 1.5, "step": 0.05 },
              "storage": { "path": ["Main", "fHeight"] }, "visibleWhen": { "setting": "bHolsters", "equals": true } },
            { "type": "stepper", "valueType": "string", "id": "quality", "text": "Quality", "valueOptions": { "options": [
                { "value": "on", "text": "On", "sets": { "bHolsters": true } },
                { "value": "off", "text": "Off", "sets": { "bHolsters": false } } ] } } ],
        "pages": [ { "pageDisplayName": "Body", "content": [
            { "type": "switcher", "id": "bHolsters", "text": "Holsters", "default": false, "storage": { "path": ["Main", "bHolsters"] } } ] } ]
    })");
    CHECK(mod.problems.empty());
    const auto& height = std::get<model::Setting>(mod.frontPage.items[0]);
    REQUIRE(height.visibleWhen);
    CHECK(height.visibleWhen->setting == "bHolsters");
    const auto& quality = std::get<model::Setting>(mod.frontPage.items[1]);
    REQUIRE(quality.valueOptions.options.size() == 2);
    CHECK(setOn(quality.valueOptions.options[0], "bHolsters") == model::Value(true));
    CHECK(setOn(quality.valueOptions.options[1], "bHolsters") == model::Value(false));
}
