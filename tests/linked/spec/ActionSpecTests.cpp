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

TEST_CASE("ConfigSpecLoader: a button has a style and an icon, and an OpenURL has the link icon with none")
{
    const auto mod = loaded(configSpecOf(R"(
        { "type": "button", "text": "Calibrate", "style": "important", "icon": "target", "action": { "type": "RunConsoleCommand", "command": "tm" } },
        { "type": "button", "text": "Plain", "action": { "type": "RunConsoleCommand", "command": "tm" } },
        { "type": "button", "text": "Docs", "action": { "type": "OpenURL", "url": "https://example.com" } },
        { "type": "button", "text": "Site", "icon": "external", "action": { "type": "OpenURL", "url": "https://example.com" } }
    )"));
    CHECK(mod.problems.empty());
    const auto button = [&mod](const std::size_t place) -> const model::Button& {
        return std::get<model::Button>(mod.frontPage.items[place]);
    };
    CHECK(button(0).style == model::ButtonStyle::Important);
    CHECK(button(0).icon == model::ButtonIcon::Target);
    CHECK(button(1).style == model::ButtonStyle::None);
    CHECK(button(1).icon == model::ButtonIcon::None);
    CHECK(button(2).icon == model::ButtonIcon::Link);
    CHECK(button(3).icon == model::ButtonIcon::External);
}

TEST_CASE("ConfigSpecLoader: each icon of a button is read by its name")
{
    constexpr std::array
        NAMES{ "play", "stop", "bolt", "target", "check", "window", "external", "link", "gear", "import", "export", "copy", "eye", "hand", "sound", "pin", "refresh", "trash" };
    std::string rows;
    for (const auto* name : NAMES) {
        rows += std::format(R"({}{{ "type": "button", "text": "{}", "icon": "{}", "action": {{ "type": "RunConsoleCommand", "command": "tm" }} }})",
            rows.empty() ? "" : ",",
            name,
            name);
    }
    const auto mod = loaded(configSpecOf(rows));
    CHECK(mod.problems.empty());
    REQUIRE(mod.frontPage.items.size() == NAMES.size());
    for (std::size_t place = 0; place < NAMES.size(); ++place) {
        CAPTURE(NAMES[place]);
        CHECK(static_cast<std::size_t>(std::get<model::Button>(mod.frontPage.items[place]).icon) == place + 1);
    }
}

TEST_CASE("ConfigSpecLoader: a dangerous button always asks first, in red, and a dangerous confirmation makes its button dangerous")
{
    const auto mod = loaded(configSpecOf(R"(
        { "type": "button", "text": "Delete every holster", "help": "It cannot be taken back.", "buttonText": "Delete", "style": "dangerous",
          "action": { "type": "RunConsoleCommand", "command": "tm" } },
        { "type": "button", "text": "<b>Wipe</b>", "html": true, "style": "dangerous", "action": { "type": "RunConsoleCommand", "command": "tm" } },
        { "type": "button", "text": "Own", "style": "dangerous", "action": { "type": "RunConsoleCommand", "command": "tm" },
          "confirmation": { "title": "Are you sure?", "message": "Really.", "confirm": "Yes" } },
        { "type": "button", "text": "Red", "action": { "type": "RunConsoleCommand", "command": "tm" }, "confirmation": { "title": "Reset?", "dangerous": true } },
        { "type": "button", "text": "Both", "style": "important", "action": { "type": "RunConsoleCommand", "command": "tm" }, "confirmation": { "dangerous": true } },
        { "type": "button", "text": "Quiet", "style": "important", "action": { "type": "RunConsoleCommand", "command": "tm" } }
    )"));
    CHECK(mod.problems.empty());
    const auto button = [&mod](const std::size_t place) -> const model::Button& {
        return std::get<model::Button>(mod.frontPage.items[place]);
    };

    // with no confirmation it asks with its text and its help, and its confirming button says what it does
    REQUIRE(button(0).confirmation);
    CHECK(button(0).style == model::ButtonStyle::Dangerous);
    CHECK(button(0).confirmation->title == "Delete every holster?");
    CHECK(button(0).confirmation->message == "It cannot be taken back.");
    CHECK(button(0).confirmation->confirm.empty());
    CHECK(button(0).confirmation->dangerous);

    // the plain text of a label with tags, and no message with no help
    REQUIRE(button(1).confirmation);
    CHECK(button(1).confirmation->title == "Wipe?");
    CHECK(button(1).confirmation->message.empty());

    // its own confirmation stays, in red
    REQUIRE(button(2).confirmation);
    CHECK(button(2).confirmation->title == "Are you sure?");
    CHECK(button(2).confirmation->confirm == "Yes");
    CHECK(button(2).confirmation->dangerous);

    // a dangerous confirmation makes it dangerous, also over another style
    CHECK(button(3).style == model::ButtonStyle::Dangerous);
    CHECK(button(4).style == model::ButtonStyle::Dangerous);

    CHECK(button(5).style == model::ButtonStyle::Important);
    CHECK_FALSE(button(5).confirmation);
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

TEST_CASE("ConfigSpecLoader: a style or an icon that is none the menu has is a warning, and the button is drawn without it")
{
    const auto mod = loaded(configSpecOf(R"(
        { "type": "button", "text": "Loud", "style": "loud", "icon": "rocket", "action": { "type": "RunConsoleCommand", "command": "tm" } },
        { "type": "button", "text": "Number", "style": 1, "action": { "type": "RunConsoleCommand", "command": "tm" } },
        { "type": "button", "text": "Docs", "icon": "rocket", "action": { "type": "OpenURL", "url": "https://example.com" } }
    )"));
    const auto& loud = std::get<model::Button>(mod.frontPage.items[0]);
    CHECK(loud.style == model::ButtonStyle::None);
    CHECK(loud.icon == model::ButtonIcon::None);
    CHECK(std::get<model::Button>(mod.frontPage.items[1]).style == model::ButtonStyle::None);
    CHECK(std::get<model::Button>(mod.frontPage.items[2]).icon == model::ButtonIcon::Link);
    CHECK(problemLines(mod) == Lines{ R"(W Loud: Its icon "rocket" is none the menu has: the button is drawn without it. [Loud] content[0].icon)",
                                   R"(W Loud: Its style "loud" is none the menu has: the button is drawn without it. [Loud] content[0].style)",
                                   R"(W Number: Its style 1 is none the menu has: the button is drawn without it. [Number] content[1].style)",
                                   R"(W Docs: Its icon "rocket" is none the menu has: the button is drawn without it. [Docs] content[2].icon)" });
}
