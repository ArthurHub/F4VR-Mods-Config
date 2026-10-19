#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <functional>
#include <sstream>
#include <unordered_set>

#include <toml++/toml.hpp>

#include "TestFiles.h"
#include "common/CommonUtils.h"
#include "model/TextMarkup.h"
#include "package/PackageLoader.h"

using namespace mods_config;

namespace
{
    using Lines = std::vector<std::string>;

    /**
     * A package that has to load.
     */
    model::Mod loaded(const std::string_view text, const model::Translations& translations = {})
    {
        auto mod = package::loadPackage(text, translations);
        if (!mod) {
            FAIL(mod.error());
        }
        return std::move(*mod);
    }

    /**
     * A package with the given content as its front page, a file `main` that is its default, and the given
     * root `applies`.
     */
    std::string packageOf(const std::string& content, const std::string& applies = "live")
    {
        return std::format(R"({{
            "modName": "MyMod", "displayName": "My Mod", "applies": "{}",
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
     * a mod ships has the values the mod has without their keys.
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
        if (const auto* on = std::get_if<bool>(&*setting.defaultValue)) {
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

TEST_CASE("PackageLoader: a package is its mod, its front page and its pages, which nest, with their help")
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
    CHECK(mod.source == model::Source::Package);
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

TEST_CASE("PackageLoader: a section has the rows of its own content, and a section can be in a section")
{
    const auto mod = loaded(packageOf(R"(
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

TEST_CASE("PackageLoader: a section folds with isCollapsible, starts folded with collapsed, and has its help")
{
    const auto mod = loaded(packageOf(R"(
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

TEST_CASE("PackageLoader: a switcher, a slider, a choice and a text input are settings, and any other row is a placeholder with its type")
{
    const auto mod = loaded(packageOf(R"(
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

TEST_CASE("PackageLoader: HTML labels style every setting type after translation")
{
    model::Translations translations;
    translations.add("$MyMod_Label", "<font size='16' color='#FFBF33'><b><i><u>A styled &amp; label</u></i></b></font>");
    const std::vector<model::TextRun> expected{ { .text = "A styled & label", .size = 16, .color = 0xFFBF33, .bold = true, .italic = true, .underline = true } };
    for (const auto* type : { "switcher", "slider", "stepper", "dropdown", "textinput" }) {
        INFO(type);
        const bool switcher = std::string_view(type) == "switcher";
        const auto mod = loaded(packageOf(std::format(R"({{
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

TEST_CASE("PackageLoader: button and unsupported labels take HTML only with a boolean true flag")
{
    const auto mod = loaded(packageOf(R"(
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

TEST_CASE("PackageLoader: a text, a spacer and an image are rows that hold no value")
{
    model::Translations translations;
    translations.add("$MyMod_Note", "A translated note.");
    const auto mod = loaded(packageOf(R"(
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

TEST_CASE("PackageLoader: a button calls a function with its typed params, or runs a console command, with the text on its button")
{
    model::Translations translations;
    translations.add("$MyMod_Reset", "Reset");
    const auto mod = loaded(packageOf(R"(
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

TEST_CASE("PackageLoader: a setting's action has the value written for each parameter that is {value}")
{
    const auto mod = loaded(packageOf(R"(
        { "type": "switcher", "text": "Automation", "storage": { "global": "MyMod.esp|1234" },
          "action": { "type": "CallFunction", "form": "MyMod.esp|800", "function": "OnChanged", "params": ["{value}", 2, "a {value}", "{value}"] } },
        { "type": "slider", "valueType": "int", "text": "Count", "storage": { "path": ["Main", "iCount"] },
          "action": { "type": "CallGlobalFunction", "script": "MyMod:Config", "function": "SetCount", "params": ["{value}"] } },
        { "type": "switcher", "text": "HUD", "storage": { "path": ["Main", "bHud"] }, "action": { "type": "RunConsoleCommand", "command": "tm" } },
        { "type": "switcher", "text": "No action", "storage": { "path": ["Main", "bNone"] } },
        { "type": "switcher", "text": "Native", "storage": { "path": ["Main", "bNative"] }, "action": { "type": "CallNativeFunction", "function": "apply" } }
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
    CHECK(std::holds_alternative<model::UnknownAction>(settings[4].action));
}

TEST_CASE("PackageLoader: a button whose action the menu does not run, or is not written right, has the reason")
{
    const auto mod = loaded(packageOf(R"(
        { "type": "button", "text": "Calibrate", "action": { "type": "CallNativeFunction", "function": "calibrate", "params": { "mode": "height" } } },
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
                         "Not available: the mod's DLL has not given the menu its functions.",
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

TEST_CASE("PackageLoader: a button is shown by its visibleWhen and enabled by its enabledWhen, with those of its sections")
{
    const auto mod = loaded(packageOf(R"(
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

TEST_CASE("PackageLoader: a setting and a button have their warnings, each a text with its condition, in their order")
{
    model::Translations translations;
    translations.add("$MyMod_TooHigh", "Trop haut");
    const auto mod = loaded(packageOf(R"(
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

TEST_CASE("PackageLoader: a warning with no text, or whose when is not a condition, is left out")
{
    const auto mod = loaded(packageOf(R"(
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

TEST_CASE("PackageLoader: a choice's options are texts that store their place, or objects with the value each stores")
{
    model::Translations translations;
    translations.add("$MyMod_Offhand", "Offhand");
    const auto mod = loaded(packageOf(R"(
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

TEST_CASE("PackageLoader: an option that is not one of its choice's type is left out")
{
    const auto mod = loaded(packageOf(R"(
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

TEST_CASE("PackageLoader: a choice has the options of the shared list it names, in any case")
{
    const auto mod = loaded(R"({
        "modName": "MyMod", "applies": "live",
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

TEST_CASE("PackageLoader: a choice reads its options in the game from a FormList or a property, by their place or their text")
{
    const auto mod = loaded(packageOf(R"(
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

TEST_CASE("PackageLoader: a row is shown by its visibleWhen and a setting enabled by its enabledWhen, with every operator")
{
    const auto mod = loaded(packageOf(R"(
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

    // a setting is named by its id in any case, also one after the row, and has its id as the package gives it
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

TEST_CASE("PackageLoader: a section's conditions are joined with those of every row in it, at every level")
{
    const auto mod = loaded(packageOf(R"(
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

TEST_CASE("PackageLoader: a condition that is not one is left out as a whole, so its row is shown and enabled")
{
    const auto mod = loaded(packageOf(R"(
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

TEST_CASE("PackageLoader: a condition on a plugin is answered as the package is read, and holds while the plugin is loaded")
{
    const auto text = packageOf(R"(
        { "type": "text", "text": "Game plugin", "visibleWhen": { "plugin": "MyMod - ENB Patch.esp" } },
        { "type": "text", "text": "No game plugin", "visibleWhen": { "not": { "plugin": "Missing.esp" } } },
        { "type": "text", "text": "F4SE plugin", "visibleWhen": { "f4sePlugin": "F4VRBody" } },
        { "type": "text", "text": "No F4SE plugin", "visibleWhen": { "f4sePlugin": "Missing" } }
    )");
    const package::PluginChecks plugins{ .isGamePluginLoaded =
                                             [](const std::string_view plugin) {
                                                 return plugin == "MyMod - ENB Patch.esp";
                                             },
        .isF4sePluginLoaded =
            [](const std::string_view plugin) {
                return plugin == "F4VRBody";
            } };
    auto mod = package::loadPackage(text, {}, plugins);
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

TEST_CASE("PackageLoader: a slider and a switcher with no valueType have the type of their control")
{
    const auto mod = loaded(packageOf(R"(
        { "type": "switcher", "text": "Enable", "storage": { "path": ["Main", "bEnable"] } },
        { "type": "slider", "text": "Scale", "valueType": "number", "storage": { "path": ["Main", "fScale"] } }
    )"));
    CHECK(std::get<model::Setting>(mod.frontPage.items[0]).valueType == model::StoredType::Bool);
    CHECK(std::get<model::Setting>(mod.frontPage.items[1]).valueType == model::StoredType::Float);
}

TEST_CASE("PackageLoader: a setting is kept in the file its storage names, or in the nearest defaultFile above it")
{
    const auto mod = loaded(R"({
        "modName": "MyMod", "applies": "live",
        "files": {
            "main": { "format": "toml", "root": "myGames", "path": "Mods_Config/MyMod/MyMod.toml" },
            "body": { "format": "toml", "root": "myGames", "path": "Mods_Config/MyMod/body.toml" },
            "advanced": { "format": "toml", "root": "data", "path": "F4SE/Plugins/MyMod/advanced.toml" }
        },
        "defaultFile": "main",
        "content": [
            { "type": "switcher", "text": "Package's", "storage": { "path": ["Main", "a"] } },
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
    CHECK(files == Lines{ "Package's main", "Own advanced", "Section's body", "Outer section's body", "Page's advanced", "Page above's advanced" });

    REQUIRE(mod.files.size() == 3);
    CHECK(mod.files.at("main") == model::File{ .format = model::FileFormat::Toml, .root = model::FileRoot::MyGames, .path = "Mods_Config/MyMod/MyMod.toml" });
    CHECK(mod.files.at("advanced") == model::File{ .format = model::FileFormat::Toml, .root = model::FileRoot::Data, .path = "F4SE/Plugins/MyMod/advanced.toml" });
}

TEST_CASE("PackageLoader: a value is named by its path, or by its section and its key as an INI key is")
{
    const auto mod = loaded(packageOf(R"(
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

TEST_CASE("PackageLoader: a setting is kept in a global, or in a property of a script in the type of its value")
{
    const auto mod = loaded(packageOf(R"(
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

    // it has no id from where its value is kept
    for (const auto& setting : settings) {
        CHECK(setting.id.empty());
    }
}

TEST_CASE("PackageLoader: a setting that names no file of its package, or no value, is kept where the menu does not read")
{
    const auto mod = loaded(R"({
        "modName": "MyMod", "applies": "live",
        "files": { "main": { "format": "toml", "root": "myGames", "path": "MyMod.toml" } },
        "content": [
            { "type": "switcher", "text": "No default file", "storage": { "path": ["Main", "a"] } },
            { "type": "switcher", "text": "No such file", "storage": { "file": "other", "path": ["Main", "b"] } },
            { "type": "switcher", "text": "No path", "storage": { "file": "main" } },
            { "type": "switcher", "text": "Empty name", "storage": { "file": "main", "path": ["Main", ""] } },
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
                          "No storage unknown ",
                          "Global by its name unknown global",
                          "Global of no text unknown global",
                          "Property with no name unknown property",
                          "Property with no id unknown property" });
}

TEST_CASE("PackageLoader: a file that is not written as the schema has it is left out")
{
    const auto mod = loaded(R"({
        "modName": "MyMod", "applies": "live",
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

TEST_CASE("PackageLoader: a setting has its default in the type of its value, and none of another type")
{
    const auto mod = loaded(packageOf(R"(
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

TEST_CASE("PackageLoader: a default that is not one of its setting's values is none, so the setting has no Reset")
{
    const auto mod = loaded(packageOf(R"(
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

TEST_CASE("PackageLoader: a read-only setting has no default, as it has no Reset")
{
    const auto mod = loaded(packageOf(R"(
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

TEST_CASE("PackageLoader: a number's display has its decimals from 0 to 6, its unit, which is a text, and whether it is a percentage")
{
    model::Translations translations;
    translations.add("$MyMod_Meters", "meters");
    const auto mod = loaded(packageOf(R"(
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

TEST_CASE("PackageLoader: a setting has its author's id, or one from where its value is kept")
{
    const auto mod = loaded(packageOf(R"(
        { "id": "playerHeight", "type": "slider", "text": "Height", "storage": { "path": ["Body", "fHeight"] } },
        { "type": "switcher", "text": "Two names", "storage": { "path": ["Holsters", "bEnabled"] } },
        { "type": "switcher", "text": "Key", "storage": { "section": "Holsters", "key": "bLeft" } },
        { "type": "switcher", "text": "Three names", "storage": { "path": ["body", "hands", "bLeft"] } },
        { "type": "switcher", "text": "One name", "storage": { "path": ["bTop"] } }
    )"));
    Lines ids;
    for (const auto& setting : settingsOf(mod)) {
        ids.push_back(setting.id);
    }
    CHECK(ids == Lines{ "playerHeight", "bEnabled:Holsters", "bLeft:Holsters", "bLeft:body.hands", "bTop" });
}

TEST_CASE("PackageLoader: two settings whose ids are the same, in any case, are both kept")
{
    const auto mod = loaded(packageOf(R"(
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

TEST_CASE("PackageLoader: a setting takes its own applies, or else the nearest above it")
{
    const auto mod = loaded(R"({
        "modName": "MyMod", "applies": "live",
        "files": { "main": { "format": "toml", "root": "myGames", "path": "MyMod.toml" } },
        "defaultFile": "main",
        "content": [
            { "type": "switcher", "text": "Package's", "storage": { "path": ["Main", "a"] } },
            { "type": "switcher", "text": "Own", "applies": "restart", "storage": { "path": ["Main", "b"] } },
            {
                "type": "section", "text": "Hooks", "applies": "save",
                "content": [
                    { "type": "switcher", "text": "Section's", "storage": { "path": ["Main", "c"] } },
                    { "type": "section", "text": "Inner", "content": [ { "type": "switcher", "text": "Outer section's", "storage": { "path": ["Main", "d"] } } ] },
                    { "type": "switcher", "text": "Own in a section", "applies": "live", "storage": { "path": ["Main", "e"] } }
                ]
            },
            { "type": "switcher", "text": "Not one", "applies": "never", "storage": { "path": ["Main", "f"] } }
        ],
        "pages": [
            {
                "pageDisplayName": "Advanced", "applies": "restart",
                "content": [ { "type": "switcher", "text": "Page's", "storage": { "path": ["Main", "g"] } } ],
                "pages": [ { "pageDisplayName": "Deeper", "content": [ { "type": "switcher", "text": "Page above's", "storage": { "path": ["Main", "h"] } } ] } ]
            }
        ]
    })");
    const auto name = [](const std::optional<model::Applies> applies) {
        if (!applies) {
            return "none";
        }
        switch (*applies) {
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
    CHECK(applies ==
          Lines{ "Package's live", "Own restart", "Section's save", "Outer section's save", "Own in a section live", "Not one live", "Page's restart", "Page above's restart" });
}

TEST_CASE("PackageLoader: a package that cannot be read says why")
{
    CHECK(package::loadPackage("{ not json").error().starts_with("Not valid JSON"));
    CHECK(package::loadPackage("[]").error() == "Not a JSON object");
    CHECK(package::loadPackage(R"({ "applies": "live", "content": [] })").error() == "No modName");
    // applies is required at the root, and has to be one of the three
    CHECK(package::loadPackage(R"({ "modName": "MyMod", "content": [] })").error() == "No applies of live, save or restart");
    CHECK(package::loadPackage(R"({ "modName": "MyMod", "applies": "never", "content": [] })").error() == "No applies of live, save or restart");
}

TEST_CASE("PackageLoader: a mod with no displayName is named by its modName, and comments are skipped")
{
    const auto mod = loaded(R"({
        // a comment
        "modName": "MyMod", "applies": "save", "content": []
    })");
    CHECK(mod.displayName == "MyMod");
    CHECK(mod.help.empty());
}

TEST_CASE("PackageLoader: a text is plain, a token, or a token with a fallback")
{
    model::Translations translations;
    translations.add("$MyMod_Name", "Mon mod");
    translations.add("$MyMod_Height", "Hauteur");

    const auto mod = loaded(R"({
        "modName": "MyMod", "applies": "live",
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

TEST_CASE("PackageLoader: the package of every folder in the packages' folder is loaded")
{
    const test_files::TempFolder folder;
    folder.write("MyMod/config.json", packageOf(""));
    folder.write("Other/config.json", R"({ "modName": "Other", "applies": "save", "content": [] })");
    folder.write("Broken/config.json", "{ not json");
    folder.write("NoApplies/config.json", R"({ "modName": "NoApplies", "content": [] })");
    folder.write("NotAPackage/readme.txt", "nothing");
    folder.write("config.json", "{}");

    auto mods = package::loadPackages(folder.path);
    std::ranges::sort(mods, {}, &model::Mod::name);
    Lines names;
    for (const auto& mod : mods) {
        names.push_back(mod.name + (mod.notAvailable.empty() ? "" : ": " + mod.notAvailable));
        CHECK(mod.source == model::Source::Package);
    }
    // one that cannot be read is named by its folder
    CHECK(names == Lines{ "Broken: its package cannot be read", "MyMod", "NoApplies: its package cannot be read", "Other" });
}

TEST_CASE("PackageLoader: two packages with one modName, in any case, are both not available")
{
    const test_files::TempFolder folder;
    folder.write("First/config.json", R"({ "modName": "MyMod", "displayName": "My Mod", "applies": "live", "content": [ { "type": "spacer" } ] })");
    folder.write("Second/config.json", R"({ "modName": "MYMOD", "displayName": "My Other Mod", "applies": "live", "content": [] })");
    folder.write("Third/config.json", R"({ "modName": "Other", "applies": "live", "content": [] })");

    auto mods = package::loadPackages(folder.path);
    std::ranges::sort(mods, {}, &model::Mod::displayName);
    REQUIRE(mods.size() == 3);
    CHECK(mods[0].displayName == "My Mod");
    CHECK(mods[0].notAvailable == "its package has the modName of another package");
    CHECK(mods[0].frontPage.items.empty());
    CHECK(mods[1].displayName == "My Other Mod");
    CHECK(mods[1].notAvailable == "its package has the modName of another package");
    CHECK(mods[2].notAvailable.empty());
}

TEST_CASE("PackageLoader: with no packages' folder there are no mods")
{
    const test_files::TempFolder folder;
    CHECK(package::loadPackages(folder.path / "missing").empty());
}

TEST_CASE("PackageLoader: the mod's own package edits keys of its shipped TOML, with the defaults it ships")
{
    const auto mods = package::loadPackages(MODS_CONFIG_PACKAGES_DIR);
    REQUIRE(mods.size() == 1);
    const auto& mod = mods[0];
    CHECK(mod.notAvailable.empty());
    CHECK(mod.name == "ModsConfigVR");
    CHECK(mod.files.at("config") == model::File{ .format = model::FileFormat::Toml, .root = model::FileRoot::MyGames, .path = "Mods_Config/ModsConfigVR/ModsConfigVR.toml" });

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

TEST_CASE("PackageLoader: the package showcase is loaded, and its shipped file has the default of each of its settings")
{
    const auto mods = package::loadPackages(MODS_CONFIG_PACKAGE_SHOWCASE_DIR);
    REQUIRE(mods.size() == 1);
    const auto& mod = mods[0];
    CHECK(mod.notAvailable.empty());
    CHECK(mod.name == "ModsConfigVR_Showcase");
    CHECK(mod.displayName == "Mods Config VR: Showcase");
    CHECK_FALSE(mod.help.empty());
    CHECK_FALSE(mod.frontPage.items.empty());
    REQUIRE(mod.pages.size() == 3);
    CHECK(mod.pages[0].name == "Simple");
    CHECK(mod.pages[1].name == "Advanced");
    const auto& examples = mod.pages[2];
    CHECK(examples.name == "Detailed Examples");
    CHECK(examples.pages.size() == 21);

    // the file whose path goes up out of its root is left out
    Lines files;
    for (const auto& [name, file] : mod.files) {
        files.push_back(name);
    }
    CHECK(files == Lines{ "ini", "invalid", "player", "showcase" });

    // a page has pages under it, two levels deep
    const auto pages = std::ranges::find(examples.pages, "Pages", &model::Page::name);
    REQUIRE(pages != examples.pages.end());
    REQUIRE(pages->pages.size() == 1);
    CHECK(pages->pages[0].pages.size() == 1);

    // the settings of the shipped file, but for those with no default: a choice with no options has none either,
    // and a read-only setting needs none
    const auto shipped = toml::parse(readFile(std::filesystem::path(MODS_CONFIG_PACKAGE_SHOWCASE_DIR) / "ModsConfigVR_Showcase" / "showcase.toml"));
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
    CHECK(withNoDefault == Lines{ "No default", "A default of another type", "A default outside its range", "No options", "No such shared list", "A default that is no option" });
    CHECK(checked >= 30);
    CHECK(readOnly == 9);
}
