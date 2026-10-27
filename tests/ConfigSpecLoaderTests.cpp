#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <format>
#include <fstream>
#include <functional>
#include <sstream>
#include <unordered_set>

#include <toml++/toml.hpp>

#include "TestFiles.h"
#include "common/CommonUtils.h"
#include "common/TomlEditor.h"
#include "model/InputBindings.h"
#include "model/TextMarkup.h"
#include "spec/ConfigSpecLoader.h"

using namespace mods_config;

namespace
{
    using Lines = std::vector<std::string>;

    /**
     * A config spec that has to load.
     */
    model::Mod loaded(const std::string_view text, const model::Translations& translations = {})
    {
        auto mod = spec::loadConfigSpec(text, translations);
        if (!mod) {
            FAIL(mod.error());
        }
        return std::move(*mod);
    }

    /**
     * A config spec with the given content as its front page, a file `main` that is its default, and the given
     * root `applies`.
     */
    std::string configSpecOf(const std::string& content, const std::string& applies = "live")
    {
        return std::format(R"({{
            "minSchemaVersion": 1, "modName": "MyMod", "displayName": "My Mod", "applies": "{}",
            "files": {{ "main": {{ "format": "toml", "root": "myGames", "path": "Mods_Config/MyMod/MyMod.toml" }} }},
            "defaultFile": "main",
            "content": [ {} ]
        }})",
            applies,
            content);
    }

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
     * Every setting of a page and of the pages under it, in order, with those of its sections.
     */
    void forEachSetting(const model::Page& page, const std::function<void(const model::Setting&)>& visit)
    {
        const std::function<void(const std::vector<model::Item>&)> inItems = [&](const std::vector<model::Item>& items) {
            for (const auto& item : items) {
                if (const auto* setting = std::get_if<model::Setting>(&item)) {
                    visit(*setting);
                } else if (const auto* group = std::get_if<model::Group>(&item)) {
                    inItems(group->items);
                }
            }
        };
        inItems(page.items);
        for (const auto& under : page.pages) {
            forEachSetting(under, visit);
        }
    }

    /**
     * The settings of a mod by their ids, in order.
     */
    std::vector<model::Setting> settingsOf(const model::Mod& mod)
    {
        std::vector<model::Setting> settings;
        const auto add = [&settings](const model::Setting& setting) {
            settings.push_back(setting);
        };
        forEachSetting(mod.frontPage, add);
        for (const auto& page : mod.pages) {
            forEachSetting(page, add);
        }
        return settings;
    }

    model::Setting onlySetting(const model::Mod& mod)
    {
        const auto settings = settingsOf(mod);
        REQUIRE(settings.size() == 1);
        return settings[0];
    }

    model::Storage fileValue(const std::string& file, Lines path)
    {
        return model::FileStorage{ .file = file, .path = std::move(path) };
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
            CHECK(model::canonicalBinding(common::TomlEditor::Value(*value.node()).text()) == std::get<std::string>(*setting.defaultValue));
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

TEST_CASE("ConfigSpecLoader: a button calls a function with its typed params, or runs a console command, with the text on its button")
{
    model::Translations translations;
    translations.add("$MyMod_Reset", "Reset");
    const auto mod = loaded(configSpecOf(R"(
        { "type": "button", "text": "Reset everything", "help": "Puts every value back.", "buttonText": "$MyMod_Reset",
          "action": { "type": "CallFunction", "form": "MyMod.esp|0x800", "scriptName": "MyMod:Config", "function": "ResetAll",
                      "params": [4, -2, 4.0, 1e2, true, "a text", "{i}5", "{value}"] } },
        { "type": "button", "text": "Notify",
          "action": { "type": "CallGlobalFunction", "script": "Debug", "function": "Notification", "params": ["Done"] } },
        { "type": "button", "text": "Hide the HUD", "action": { "type": "RunConsoleCommand", "command": "tm" } }
    )"),
        translations);
    const auto& items = mod.frontPage.items;
    REQUIRE(items.size() == 3);

    // a number with no point is a whole number, and a text is never read for a type
    const auto& reset = std::get<model::Button>(items[0]);
    CHECK(reset.text == "Reset everything");
    CHECK(reset.help == "Puts every value back.");
    CHECK(reset.buttonText == "Reset");
    CHECK(reset.action == model::Action(model::CallFunction{ .plugin = "MyMod.esp",
                              .formId = 0x800,
                              .script = "MyMod:Config",
                              .function = "ResetAll",
                              .arguments = { 4, -2, 4.0f, 100.0f, true, std::string("a text"), std::string("{i}5"), std::string("{value}") },
                              .valueArguments = {} }));

    // with no buttonText, the button is Run
    const auto& notify = std::get<model::Button>(items[1]);
    CHECK(notify.buttonText.empty());
    CHECK(
        notify.action ==
        model::Action(model::CallFunction{ .plugin = {}, .formId = 0, .script = "Debug", .function = "Notification", .arguments = { std::string("Done") }, .valueArguments = {} }));

    CHECK(std::get<model::Button>(items[2]).action == model::Action(model::ConsoleCommand{ "tm" }));
}

TEST_CASE("ConfigSpecLoader: OpenURL loads a desktop web link without a native client")
{
    const auto mod = loaded(configSpecOf(R"(
        { "type": "button", "text": "Documentation", "buttonText": "Open",
          "action": { "type": "OpenURL", "url": "https://example.com/docs?q=a%20b#buttons" } }
    )"));
    REQUIRE(mod.problems.empty());
    REQUIRE(mod.frontPage.items.size() == 1);
    const auto& button = std::get<model::Button>(mod.frontPage.items[0]);
    CHECK(button.buttonText == "Open");
    CHECK(button.action == model::Action(model::WebLink{ "https://example.com/docs?q=a%20b#buttons" }));
}

TEST_CASE("ConfigSpecLoader: OpenURL is only available for buttons, so saving a setting never opens a browser")
{
    const auto mod = loaded(configSpecOf(R"(
        { "type": "switcher", "valueType": "bool", "text": "Enabled", "default": true,
          "storage": { "path": ["Main", "bEnabled"] }, "action": { "type": "OpenURL", "url": "https://example.com" } }
    )"));
    REQUIRE(mod.problems.size() == 1);
    const auto& action = std::get<model::Setting>(mod.frontPage.items[0]).action;
    REQUIRE(std::holds_alternative<model::UnknownAction>(action));
    CHECK(std::get<model::UnknownAction>(action).reason == "Not available: OpenURL is only available for buttons.");
}

TEST_CASE("ConfigSpecLoader: a setting's action has the value written for each parameter that is {value}")
{
    const auto mod = loaded(configSpecOf(R"(
        { "type": "switcher", "text": "Automation", "storage": { "global": "MyMod.esp|1234" },
          "action": { "type": "CallFunction", "form": "MyMod.esp|800", "function": "OnChanged", "params": ["{value}", 2, "a {value}", "{value}"] } },
        { "type": "slider", "valueType": "int", "text": "Count", "storage": { "path": ["Main", "iCount"] },
          "action": { "type": "CallGlobalFunction", "script": "MyMod:Config", "function": "SetCount", "params": ["{value}"] } },
        { "type": "switcher", "text": "HUD", "storage": { "path": ["Main", "bHud"] }, "action": { "type": "RunConsoleCommand", "command": "tm" } },
        { "type": "switcher", "text": "No action", "storage": { "path": ["Main", "bNone"] } },
        { "type": "switcher", "text": "Native", "storage": { "path": ["Main", "bNative"] },
          "action": { "type": "CallNativeFunction", "function": "apply", "params": { "on": "{value}", "mode": "{value} as text" } } }
    )"));
    const auto settings = settingsOf(mod);
    REQUIRE(settings.size() == 5);

    // only a parameter that is exactly {value}, in the setting's type
    const auto* automation = std::get_if<model::CallFunction>(&settings[0].action);
    REQUIRE(automation);
    CHECK(automation->function == "OnChanged");
    CHECK(automation->arguments == std::vector<model::Value>{ "{value}"s, 2, "a {value}"s, "{value}"s });
    CHECK(automation->valueArguments == std::vector<model::ValueArgument>{ { .place = 0, .text = "{value}", .type = model::StoredType::Bool },
                                            { .place = 3, .text = "{value}", .type = model::StoredType::Bool } });
    const auto* count = std::get_if<model::CallFunction>(&settings[1].action);
    REQUIRE(count);
    CHECK(count->valueArguments == std::vector<model::ValueArgument>{ { .place = 0, .text = "{value}", .type = model::StoredType::Int } });

    CHECK(settings[2].action == model::Action(model::ConsoleCommand{ "tm" }));
    CHECK(std::holds_alternative<model::NoAction>(settings[3].action));
    const auto* native = std::get_if<model::NativeFunction>(&settings[4].action);
    REQUIRE(native);
    CHECK(native->function == "apply");
    CHECK(native->arguments == std::vector<model::NamedArgument>{ { .name = "mode", .value = "{value} as text"s }, { .name = "on", .value = "{value}"s } });
    CHECK(native->valueArguments == std::vector<std::size_t>{ 1 });
}

TEST_CASE("ConfigSpecLoader: a button runs a function of its mod's DLL with its named parameters, and asks its confirmation first")
{
    const auto mod = loaded(configSpecOf(R"(
        { "type": "button", "text": "Calibrate", "buttonText": "Calibrate",
          "action": { "type": "CallNativeFunction", "function": "calibrate", "params": { "mode": "height", "samples": 30, "scale": 1.5, "includeScale": true } },
          "confirmation": { "title": "Calibrate?", "message": "Stand straight and look ahead.", "confirm": "Start", "dangerous": true } },
        { "type": "button", "text": "No params", "action": { "type": "CallNativeFunction", "function": "reset" }, "confirmation": { "title": "Reset?" } },
        { "type": "button", "text": "Not an object", "action": { "type": "RunConsoleCommand", "command": "tm" }, "confirmation": "Sure?" }
    )"));
    const auto& calibrate = std::get<model::Button>(mod.frontPage.items[0]);
    // the parameters by their names
    CHECK(calibrate.action == model::Action(model::NativeFunction{ .function = "calibrate",
                                  .arguments = { { .name = "includeScale", .value = true },
                                      { .name = "mode", .value = "height"s },
                                      { .name = "samples", .value = 30 },
                                      { .name = "scale", .value = 1.5f } },
                                  .valueArguments = {} }));
    REQUIRE(calibrate.confirmation);
    CHECK(calibrate.confirmation->title == "Calibrate?");
    CHECK(calibrate.confirmation->message == "Stand straight and look ahead.");
    CHECK(calibrate.confirmation->confirm == "Start");
    CHECK(calibrate.confirmation->dangerous);

    const auto& reset = std::get<model::Button>(mod.frontPage.items[1]);
    CHECK(reset.action == model::Action(model::NativeFunction{ .function = "reset", .arguments = {}, .valueArguments = {} }));
    REQUIRE(reset.confirmation);
    CHECK(reset.confirmation->message.empty());
    CHECK_FALSE(reset.confirmation->dangerous);

    // a confirmation that is no object is an error, and the button asks none
    CHECK_FALSE(std::get<model::Button>(mod.frontPage.items[2]).confirmation);
    REQUIRE(mod.problems.size() == 1);
    CHECK(mod.problems[0].message == "Its confirmation is not an object: the button runs with none.");
}

TEST_CASE("ConfigSpecLoader: a button whose action the menu does not run, or is not written right, has the reason")
{
    const auto mod = loaded(configSpecOf(R"(
        { "type": "button", "text": "No native function", "action": { "type": "CallNativeFunction" } },
        { "type": "button", "text": "Native params a list", "action": { "type": "CallNativeFunction", "function": "calibrate", "params": ["height"] } },
        { "type": "button", "text": "Native list param", "action": { "type": "CallNativeFunction", "function": "calibrate", "params": { "modes": ["height"] } } },
        { "type": "button", "text": "Flash", "action": { "type": "CallExternalFunction", "function": "Flash" } },
        { "type": "button", "text": "No type", "action": { "function": "ResetAll" } },
        { "type": "button", "text": "No action" },
        { "type": "button", "text": "No form", "action": { "type": "CallFunction", "function": "ResetAll" } },
        { "type": "button", "text": "No id", "action": { "type": "CallFunction", "form": "MyMod.esp", "function": "ResetAll" } },
        { "type": "button", "text": "No function", "action": { "type": "CallFunction", "form": "MyMod.esp|800" } },
        { "type": "button", "text": "No script", "action": { "type": "CallGlobalFunction", "function": "Notification" } },
        { "type": "button", "text": "Params not a list", "action": { "type": "CallGlobalFunction", "script": "Debug", "function": "Trace", "params": "a" } },
        { "type": "button", "text": "An object", "action": { "type": "CallGlobalFunction", "script": "Debug", "function": "Trace", "params": ["a", { "b": 1 }] } },
        { "type": "button", "text": "Too large", "action": { "type": "CallGlobalFunction", "script": "Debug", "function": "Trace", "params": [2147483648] } },
        { "type": "button", "text": "No command", "action": { "type": "RunConsoleCommand", "command": "" } }
    )"));
    std::vector<std::string> reasons;
    for (const auto& item : mod.frontPage.items) {
        const auto& action = std::get<model::Button>(item).action;
        REQUIRE(std::holds_alternative<model::UnknownAction>(action));
        reasons.push_back(std::get<model::UnknownAction>(action).reason);
    }
    CHECK(reasons == Lines{
                         "Not available: its CallNativeFunction names no function.",
                         "Not available: the params of its CallNativeFunction are not an object of named values.",
                         "Not available: the parameter 'modes' of its CallNativeFunction is not a whole number of Papyrus, a number, a bool or a text.",
                         "Not available: the menu does not run a CallExternalFunction.",
                         "Not available: the action has no type.",
                         "Not available: the button has no action.",
                         "Not available: its CallFunction names no form.",
                         "Not available: the form 'MyMod.esp' of its CallFunction is not a plugin and an id in hex.",
                         "Not available: its CallFunction names no function.",
                         "Not available: its CallGlobalFunction names no script.",
                         "Not available: the params of its CallGlobalFunction are not a list.",
                         "Not available: parameter 2 of its CallGlobalFunction is not a whole number of Papyrus, a number, a bool or a text.",
                         "Not available: parameter 1 of its CallGlobalFunction is not a whole number of Papyrus, a number, a bool or a text.",
                         "Not available: its RunConsoleCommand has no command.",
                     });
}

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
        return model::Condition{ .kind = model::Condition::Kind::On, .setting = name, .clientState = true };
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

TEST_CASE("ConfigSpecLoader: a choice's options are texts that store their place, or objects with the value each stores")
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
    CHECK(settings[1].valueOptions.options == Options{ { std::string("physical"), "Physical" }, { std::string("hybrid"), "Hybrid" } });
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
            "Modes": [ { "value": "physical", "text": "Physical" }, { "value": "hybrid", "text": "Hybrid" } ]
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
    CHECK(settings[2].valueOptions.options == Options{ { std::string("physical"), "Physical" }, { std::string("hybrid"), "Hybrid" } });
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

TEST_CASE("ConfigSpecLoader: a config spec whose required plugins are not all loaded is not available, and its reason names each that is not")
{
    const auto configSpecRequiring = [](const std::string& game, const std::string& f4se) {
        return std::format(R"({{
            "minSchemaVersion": 1, "modName": "MyMod", "displayName": "My Mod", "help": "Its help.", "applies": "live",
            "pluginRequirements": [{}], "f4sePluginRequirements": [{}],
            "content": [ {{ "type": "spacer" }} ]
        }})",
            game,
            f4se);
    };
    const spec::PluginChecks plugins{ .isGamePluginLoaded =
                                          [](const std::string_view plugin) {
                                              return plugin == "MyMod.esp";
                                          },
        .isF4sePluginLoaded =
            [](const std::string_view plugin) {
                return plugin == "F4VRBody";
            } };
    const auto reason = [&plugins](const std::string& text) {
        const auto mod = spec::loadConfigSpec(text, {}, plugins);
        REQUIRE(mod);
        return mod->notAvailable;
    };

    // every one is loaded, and a requirement that is no text is passed over
    const auto mod = spec::loadConfigSpec(configSpecRequiring(R"("MyMod.esp", 5)", R"("F4VRBody")"), {}, plugins);
    REQUIRE(mod);
    CHECK(mod->notAvailable.empty());
    CHECK(mod->frontPage.items.size() == 1);
    // its F4SE plugins are kept, which a Save sends its F4SE message to
    CHECK(mod->f4sePlugins == std::vector<std::string>{ "F4VRBody" });
    CHECK(reason(configSpecRequiring("", "")).empty());

    CHECK(reason(configSpecRequiring(R"("MyMod.esp", "SS2.esm")", "")) == "its plugin SS2.esm is not loaded");
    CHECK(reason(configSpecRequiring(R"("A.esp", "MyMod.esp", "B.esm", "C.esl")", "")) == "its plugins A.esp, B.esm and C.esl are not loaded");
    CHECK(reason(configSpecRequiring("", R"("F4VRBody", "MyMod")")) == "its F4SE plugin MyMod is not loaded");
    CHECK(reason(configSpecRequiring("", R"("A", "B")")) == "its F4SE plugins A and B are not loaded");
    CHECK(reason(configSpecRequiring(R"("SS2.esm")", R"("MyMod")")) == "its plugin SS2.esm and its F4SE plugin MyMod are not loaded");

    // one that is not available is its names and the reason alone, also one whose config spec is not right
    const auto missing = spec::loadConfigSpec(configSpecRequiring(R"("SS2.esm")", ""), {}, plugins);
    REQUIRE(missing);
    CHECK(missing->name == "MyMod");
    CHECK(missing->displayName == "My Mod");
    CHECK(missing->help.empty());
    CHECK(missing->frontPage.items.empty());
    CHECK(missing->source == model::Source::ConfigSpec);
    const auto noApplies = spec::loadConfigSpec(R"({ "minSchemaVersion": 1, "modName": "MyMod", "pluginRequirements": ["SS2.esm"], "content": [] })", {}, plugins);
    REQUIRE(noApplies);
    CHECK(noApplies->notAvailable == "its plugin SS2.esm is not loaded");

    // with no checks every plugin is loaded
    CHECK(loaded(configSpecRequiring(R"("SS2.esm")", R"("MyMod")")).notAvailable.empty());
}

TEST_CASE("ConfigSpecLoader: a config spec's minSchemaVersion is required, and one that needs a newer schema than the menu reads is not available, with the version")
{
    const auto configSpecOfVersion = [](const std::string& version) {
        return std::format(R"({{ "minSchemaVersion": {}, "modName": "MyMod", "displayName": "My Mod", "applies": "live", "content": [ {{ "type": "spacer" }} ] }})", version);
    };
    // the menu's own version and an older one are read
    for (const auto version : { spec::SCHEMA_VERSION, 1 }) {
        INFO(version);
        const auto mod = loaded(configSpecOfVersion(std::to_string(version)));
        CHECK(mod.notAvailable.empty());
        CHECK(mod.frontPage.items.size() == 1);
    }

    // it is required, and is a whole number from 1
    for (const auto& version : { "0"s, "-1"s, "\"9\""s, "1.5"s, "null"s }) {
        INFO(version);
        CHECK(spec::loadConfigSpec(configSpecOfVersion(version)).error() == "No minSchemaVersion of a whole number from 1");
    }
    CHECK(spec::loadConfigSpec(R"({ "modName": "MyMod", "applies": "live", "content": [] })").error() == "No minSchemaVersion of a whole number from 1");

    const auto newer = spec::loadConfigSpec(configSpecOfVersion(std::to_string(spec::SCHEMA_VERSION + 1)));
    REQUIRE(newer);
    CHECK(newer->notAvailable == std::format("its spec.json needs a newer Mods Config FO4 VR, one that reads schema version {}", spec::SCHEMA_VERSION + 1));
    CHECK(newer->displayName == "My Mod");
    CHECK(newer->frontPage.items.empty());

    // it is told before a plugin that is not loaded, and of a config spec that is not read as this version reads one
    const auto noApplies = spec::loadConfigSpec(R"({ "minSchemaVersion": 99, "modName": "MyMod", "pluginRequirements": ["SS2.esm"] })",
        {},
        spec::PluginChecks{ .isGamePluginLoaded =
                                [](std::string_view) {
                                    return false;
                                },
            .isF4sePluginLoaded = {} });
    REQUIRE(noApplies);
    CHECK(noApplies->notAvailable == "its spec.json needs a newer Mods Config FO4 VR, one that reads schema version 99");
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
    CHECK(names == Lines{ "ini", "json", "toml" });
    CHECK(mod.files.at("ini").format == model::FileFormat::Ini);
    CHECK(mod.files.at("json").format == model::FileFormat::Json);
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
    CHECK(mod.pages[0].name == "Most Used");
    const auto& examples = mod.pages[1];
    CHECK(examples.name == "Detailed Examples");
    CHECK(examples.pages.size() == 22);

    // the file whose path goes up out of its root is left out
    Lines files;
    for (const auto& [name, file] : mod.files) {
        files.push_back(name);
    }
    CHECK(files == Lines{ "ini", "invalid", "player", "showcase" });

    // a page has pages under it, two levels deep
    const auto pages = std::ranges::find(examples.pages, "Pages", &model::Page::name);
    REQUIRE(pages != examples.pages.end());
    REQUIRE(pages->pages.size() == 2);
    CHECK(pages->pages[0].pages.size() == 1);

    // the settings of the shipped file, but for those with no default: a choice with no options has none either,
    // and a read-only setting needs none
    const auto shipped = toml::parse(readFile(std::filesystem::path(MODS_CONFIG_SPEC_SHOWCASE_DIR) / "ModsConfigFO4_Showcase" / "showcase.toml"));
    Lines withNoDefault;
    std::size_t checked = 0;
    std::size_t readOnly = 0;
    for (const auto& setting : settingsOf(mod)) {
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
                               "No options",
                               "No such shared list",
                               "A default that is no option" });
    CHECK(checked >= 30);
    CHECK(readOnly == 9);
}

namespace
{
    /**
     * A problem as one line: E or W, its row, its message, where it is, and its path.
     */
    std::string problemLine(const model::Problem& problem)
    {
        return std::format("{} {}: {} [{}] {}", problem.severity == model::Severity::Error ? "E" : "W", problem.row, problem.message, problem.where, problem.path);
    }

    Lines problemLines(const model::Mod& mod)
    {
        Lines lines;
        for (const auto& problem : mod.problems) {
            lines.push_back(problemLine(problem));
        }
        return lines;
    }
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
                                   "E spec.json: The file \"bad\" is left out: its format 'yaml' is not ini, json or toml. Its settings are not available. [] files.bad",
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
        "sharedLists": { "hands": [ { "value": 0, "text": "Left", "shorttext": "L" } ] },
        "content": [
            { "type": "stepper", "text": "Hand", "default": 0, "valueOptions": { "sharedOptions": "hands", "wrap": true }, "storage": { "path": ["a"], "fil": "main" } },
            { "type": "dropdown", "valueType": "string", "text": "Place", "default": "hip",
              "valueOptions": { "options": [ { "value": "hip", "text": "Hip", "shortText": "H", "help": "At the hip.", "icon": "hip.dds" } ] },
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
                                   "W spec.json: Unknown field \"shorttext\": it is not read. Did you mean \"shortText\"? [] sharedLists.hands[0].shorttext" });
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
    CHECK(elsewhere == Lines{ "files.outside", "pages[1].pages[20].content[0].content[2].text" });
    CHECK(errors >= 20);
    CHECK(warnings >= 10);
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

TEST_CASE("ConfigSpecLoader: a binding is the text of its table, with its default an object the framework reads, and offers suppress")
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
            { "type": "cf.inputBinding", "text": "In an INI file", "default": { "type": "none" }, "storage": { "file": "ini", "section": "Main", "key": "kBinding" } }
        ]
    })");
    const auto settings = settingsOf(mod);
    REQUIRE(settings.size() == 5);
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
    CHECK(std::holds_alternative<model::UnknownStorage>(settings[4].storage));
    CHECK(problemLines(mod) ==
          Lines{ "E Typo: Its default is not a binding: 'thumbstik' is not a button, one of trigger, grip, a, b, thumbstick, menu, system. It has no Reset. [Typo] "
                 "content[2].default",
              "E Text: It has no default binding, an object as { \"hand\": \"offhand\", \"type\": \"tap\", \"button\": \"trigger\" }: it has no Reset. [Text] content[3]",
              "W Text: Its valueType \"int\" is not string, which a binding is: it is read as string. [Text] content[3].valueType",
              "E In an INI file: A binding is kept in a value of a TOML file, and it is not: it is not available. [In an INI file] content[4].storage" });
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
    CHECK(problemLines(*mod) == Lines{ "W Typo: Its file \"main\" has no value Mian.fScale: it shows its default, and a Save adds it. [Typo] content[1].storage",
                                    "W Shown: Its file \"main\" has no value Main.fShown: it shows none. [Shown] content[2].storage",
                                    "W Off: Its file \"ini\" has no value bOff of [Main]: it shows its default, and a Save adds it. [Off] content[4].storage" });

    // with no folders, the files are not read
    const auto alone = spec::loadConfigSpec(text);
    REQUIRE(alone);
    CHECK(alone->problems.empty());
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
        return model::Condition{ .kind = kind, .setting = name, .conditions = {}, .offWhenNotAvailable = false, .values = std::move(values), .clientState = true };
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
