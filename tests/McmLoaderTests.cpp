#include <catch2/catch_test_macros.hpp>

#include <algorithm>

#include "TestFiles.h"
#include "TestSwf.h"
#include "mcm/McmLoader.h"

using namespace mods_config;

namespace
{
    using Lines = std::vector<std::string>;

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

    std::string describe(const model::SettingType type)
    {
        switch (type) {
        case model::SettingType::Switcher:
            return "switcher";
        case model::SettingType::Slider:
            return "slider";
        case model::SettingType::Stepper:
            return "stepper";
        case model::SettingType::Dropdown:
            return "dropdown";
        case model::SettingType::TextInput:
            return "textinput";
        }
        return "?";
    }

    /**
     * The items as lines of "what: text", with the items of a group indented under it, so that a whole page is
     * compared in one check.
     */
    void describe(const std::vector<model::Item>& items, const std::string& indent, Lines& lines)
    {
        struct Describer
        {
            const std::string& indent;
            Lines& lines;

            void operator()(const model::Setting& setting) const
            {
                lines.push_back(indent + describe(setting.type) + ": " + setting.text);
            }

            void operator()(const model::Button& button) const
            {
                lines.push_back(indent + "button: " + button.text);
            }

            void operator()(const model::Hotkey& hotkey) const
            {
                lines.push_back(indent + "hotkey: " + hotkey.text);
            }

            void operator()(const model::Text& text) const
            {
                std::string words;
                for (const auto& line : text.lines) {
                    words += &line == &text.lines.front() ? "" : " / ";
                    for (const auto& run : line.runs) {
                        words += run.text;
                    }
                }
                lines.push_back(indent + "text: " + words);
            }

            void operator()(const model::Image& image) const
            {
                lines.push_back(indent + (image.pixels ? std::format("image of {} by {}", image.width, image.height) : "image"));
            }

            void operator()(const model::Spacer& spacer) const
            {
                lines.push_back(indent + (spacer.lines == 1 ? "spacer" : std::format("spacer of {} lines", spacer.lines)));
            }

            void operator()(const model::Placeholder& placeholder) const
            {
                lines.push_back(indent + "placeholder " + placeholder.type + ": " + placeholder.text);
            }

            void operator()(const model::Group& group) const
            {
                lines.push_back(indent + "group: " + group.text);
                describe(group.items, indent + "  ", lines);
            }
        };

        for (const auto& item : items) {
            std::visit(Describer{ indent, lines }, item);
        }
    }

    Lines describe(const model::Page& page)
    {
        Lines lines;
        describe(page.items, "", lines);
        return lines;
    }

    /**
     * The names of the mods, in order of name: a folder gives its files in no set order.
     */
    Lines namesOf(const std::vector<model::Mod>& mods)
    {
        Lines names;
        for (const auto& mod : mods) {
            names.push_back(mod.name);
        }
        std::ranges::sort(names);
        return names;
    }

    /**
     * A config with the given rows as its root content.
     */
    model::Page frontPageOf(const std::string_view rows)
    {
        return loaded(std::format(R"({{ "modName": "MyMod", "content": [{}] }})", rows)).frontPage;
    }

    /**
     * A condition as a text: the id of the setting that has to be on, and `all(...)`, `any(...)` and
     * `not(...)` around the conditions that are joined.
     */
    std::string describe(const model::Condition& condition)
    {
        using Kind = model::Condition::Kind;
        if (condition.kind == Kind::On) {
            return condition.setting;
        }
        std::string inner;
        for (const auto& joined : condition.conditions) {
            inner += (inner.empty() ? "" : ", ") + describe(joined);
        }
        return (condition.kind == Kind::All ? "all(" : condition.kind == Kind::Any ? "any(" : "not(") + inner + ")";
    }

    /**
     * What each item is shown by, in their order, and "always" for one with no condition.
     */
    Lines conditionsOf(const std::vector<model::Item>& items)
    {
        Lines lines;
        for (const auto& item : items) {
            const auto& visibleWhen = std::visit(
                [](const auto& row) -> const std::optional<model::Condition>& {
                    return row.visibleWhen;
                },
                item);
            lines.push_back(visibleWhen ? describe(*visibleWhen) : "always");
        }
        return lines;
    }

    /**
     * The ids of the settings among the items, in their order, with those of the groups.
     */
    Lines idsOf(const std::vector<model::Item>& items)
    {
        Lines ids;
        for (const auto& item : items) {
            if (const auto* setting = std::get_if<model::Setting>(&item)) {
                ids.push_back(setting->id);
            } else if (const auto* group = std::get_if<model::Group>(&item)) {
                std::ranges::copy(idsOf(group->items), std::back_inserter(ids));
            }
        }
        return ids;
    }

    /**
     * The ids the settings among the items have in their config, in their order, with those of the groups.
     */
    Lines sourceIdsOf(const std::vector<model::Item>& items)
    {
        Lines ids;
        for (const auto& item : items) {
            if (const auto* setting = std::get_if<model::Setting>(&item)) {
                ids.push_back(setting->sourceId);
            } else if (const auto* group = std::get_if<model::Group>(&item)) {
                std::ranges::copy(sourceIdsOf(group->items), std::back_inserter(ids));
            }
        }
        return ids;
    }
}

TEST_CASE("McmLoader: a config becomes a mod with its front page and its pages")
{
    const auto mod = loaded(R"({
        "modName": "MyMod",
        "displayName": "My Mod",
        "minMcmVersion": 2,
        "pluginRequirements": ["MyMod.esp"],
        "content": [
            { "type": "section", "text": "General" },
            {
                "type": "switcher", "text": "Enable the feature", "help": "Turns the feature on.",
                "groupControl": 2,
                "valueOptions": { "sourceType": "GlobalValue", "sourceForm": "MyMod.esp|1234" }
            },
            {
                "type": "slider", "text": "Strength", "id": "fStrength:Main",
                "groupCondition": 2,
                "valueOptions": { "sourceType": "ModSettingFloat", "min": 0, "max": 2, "step": 0.1 }
            }
        ],
        "pages": [
            {
                "pageDisplayName": "Advanced",
                "content": [
                    { "type": "dropdown", "text": "Mode", "id": "iMode:Main", "valueOptions": { "sourceType": "ModSettingInt", "options": ["Low", "High"] } },
                    { "type": "button", "text": "Reset everything", "action": { "type": "CallFunction", "form": "MyMod.esp|800", "function": "ResetAll" } }
                ]
            },
            { "pageDisplayName": "About", "content": [{ "type": "text", "text": "Made by me." }] }
        ]
    })");

    CHECK(mod.name == "MyMod");
    CHECK(mod.displayName == "My Mod");
    CHECK(mod.frontPage.name.empty());
    CHECK(describe(mod.frontPage) == Lines{ "group: General", "  switcher: Enable the feature", "  slider: Strength" });

    REQUIRE(mod.pages.size() == 2);
    CHECK(mod.pages[0].name == "Advanced");
    CHECK(describe(mod.pages[0]) == Lines{ "dropdown: Mode", "button: Reset everything" });
    CHECK(mod.pages[1].name == "About");
    CHECK(describe(mod.pages[1]) == Lines{ "text: Made by me." });
}

TEST_CASE("McmLoader: a mod whose required plugins are not all loaded is not available, and is its names and the reason alone")
{
    const auto config = R"({
        "modName": "MyMod",
        "displayName": "My Mod",
        "pluginRequirements": ["Fallout4.esm", "MyMod.esp", 7, "MyMod - Extra.esl", "MyMod - More.esp"],
        "content": [{ "type": "switcher", "text": "Enable the feature" }],
        "pages": [{ "pageDisplayName": "Advanced", "content": [] }]
    })";
    const auto loadedWith = [config](const std::vector<std::string>& loadedPlugins) {
        auto mod = mcm::loadConfig(config, {}, {}, {}, [&loadedPlugins](const std::string_view plugin) {
            return std::ranges::contains(loadedPlugins, plugin);
        });
        REQUIRE(mod);
        return std::move(*mod);
    };

    // every one is loaded, and an entry that is no text is passed over
    const auto all = loadedWith({ "Fallout4.esm", "MyMod.esp", "MyMod - Extra.esl", "MyMod - More.esp" });
    CHECK(all.notAvailable.empty());
    CHECK(all.pages.size() == 1);

    const auto one = loadedWith({ "Fallout4.esm", "MyMod - Extra.esl", "MyMod - More.esp" });
    CHECK(one.name == "MyMod");
    CHECK(one.displayName == "My Mod");
    CHECK(one.notAvailable == "its plugin MyMod.esp is not loaded");
    CHECK(one.frontPage.items.empty());
    CHECK(one.pages.empty());

    CHECK(loadedWith({ "Fallout4.esm", "MyMod - Extra.esl" }).notAvailable == "its plugins MyMod.esp and MyMod - More.esp are not loaded");
    CHECK(loadedWith({}).notAvailable == "its plugins Fallout4.esm, MyMod.esp, MyMod - Extra.esl and MyMod - More.esp are not loaded");

    // with no check, as for a config alone, every plugin is loaded
    CHECK(loaded(config).notAvailable.empty());
}

TEST_CASE("McmLoader: a mod with no displayName is shown by its modName")
{
    CHECK(loaded(R"({ "modName": "MyMod" })").displayName == "MyMod");
    CHECK(loaded(R"({ "modName": "MyMod", "displayName": "" })").displayName == "MyMod");
    CHECK(loaded(R"({ "modName": "MyMod", "displayName": 7 })").displayName == "MyMod");
}

TEST_CASE("McmLoader: a config with no content has an empty front page, and one with no pages has none")
{
    const auto mod = loaded(R"({ "modName": "MyMod" })");
    CHECK(mod.frontPage.items.empty());
    CHECK(mod.pages.empty());

    // a config whose settings are all on its pages
    const auto pagesOnly = loaded(R"({ "modName": "MyMod", "pages": [{ "pageDisplayName": "Main", "content": [{ "type": "spacer" }] }] })");
    CHECK(pagesOnly.frontPage.items.empty());
    REQUIRE(pagesOnly.pages.size() == 1);
    CHECK(describe(pagesOnly.pages[0]) == Lines{ "spacer" });

    // a page with no content is still a page of the mod
    const auto emptyPage = loaded(R"({ "modName": "MyMod", "pages": [{ "pageDisplayName": "Soon" }] })");
    REQUIRE(emptyPage.pages.size() == 1);
    CHECK(emptyPage.pages[0].name == "Soon");
    CHECK(emptyPage.pages[0].items.empty());
}

TEST_CASE("McmLoader: every control the menu knows becomes its item")
{
    const auto page = frontPageOf(R"(
        { "type": "switcher", "text": "A switcher" },
        { "type": "slider", "text": "A slider" },
        { "type": "stepper", "text": "A stepper" },
        { "type": "dropdown", "text": "A dropdown" },
        { "type": "dropdownFiles", "text": "A dropdown of files" },
        { "type": "textinput", "text": "A text" },
        { "type": "textinputInt", "text": "A whole number" },
        { "type": "textinputFloat", "text": "A number" },
        { "type": "button", "text": "A button" },
        { "type": "text", "text": "Some words" },
        { "type": "image", "libName": "MyMod", "className": "logo" },
        { "type": "spacer", "numLines": 2 }
    )");

    CHECK(describe(page) == Lines{
                                "switcher: A switcher",
                                "slider: A slider",
                                "stepper: A stepper",
                                "dropdown: A dropdown",
                                "dropdown: A dropdown of files",
                                "textinput: A text",
                                "textinput: A whole number",
                                "textinput: A number",
                                "button: A button",
                                "text: Some words",
                                "image",
                                "spacer of 2 lines",
                            });

    // what a text input takes, which no other setting has
    using Type = model::StoredType;
    const auto valueTypeOf = [&page](const std::size_t place) {
        return std::get<model::Setting>(page.items[place]).valueType;
    };
    CHECK(valueTypeOf(0) == Type::String);
    CHECK(valueTypeOf(5) == Type::String);
    CHECK(valueTypeOf(6) == Type::Int);
    CHECK(valueTypeOf(7) == Type::Float);
}

TEST_CASE("McmLoader: a spacer is as high as its numLines, and one line without it")
{
    const auto page = frontPageOf(R"(
        { "type": "spacer" },
        { "type": "spacer", "numLines": 1 },
        { "type": "spacer", "numLines": 3 },
        { "type": "spacer", "numLines": 2.6 },
        { "type": "spacer", "numLines": 0 },
        { "type": "spacer", "numLines": -4 },
        { "type": "spacer", "numLines": "3" },
        { "type": "spacer", "numLines": 100000 }
    )");

    CHECK(describe(page) == Lines{ "spacer", "spacer", "spacer of 3 lines", "spacer of 2 lines", "spacer", "spacer", "spacer", "spacer of 20 lines" });
}

TEST_CASE("McmLoader: a text row with html has its tags read, and one without shows them")
{
    const auto page = frontPageOf(R"(
        { "type": "text", "text": "<p align='center'><font size='24'>My Mod</font></p>What it does.<br>And how.", "html": true },
        { "type": "text", "text": "One line\nand the next" },
        { "type": "text", "text": "A <b>plain</b> text", "html": false },
        { "type": "text", "text": "A <b>plain</b> text", "html": "true" },
        { "type": "text", "text": "A <b>plain</b> text", "html": "false" },
        { "type": "text", "html": true },
        { "type": "text", "text": "<font color='#AAAAAA' size='16'>A note</font>", "html": 1 },
        { "type": "text", "text": "A <b>plain</b> text", "html": 0 }
    )");

    // a flag that is a text is set by any text, as Flash has it, also `"false"`
    CHECK(describe(page) == Lines{ "text: My Mod / What it does. / And how.",
                                "text: One line / and the next",
                                "text: A <b>plain</b> text",
                                "text: A plain text",
                                "text: A plain text",
                                "text: ",
                                "text: A note",
                                "text: A <b>plain</b> text" });

    const auto& title = std::get<model::Text>(page.items[0]).lines[0];
    CHECK(title.align == model::TextAlign::Center);
    REQUIRE(title.runs.size() == 1);
    CHECK(title.runs[0].size == 24);
}

TEST_CASE("McmLoader: a slider has its least, its most and its step")
{
    const auto page = frontPageOf(R"(
        { "type": "slider", "text": "Strength", "valueOptions": { "sourceType": "ModSettingFloat", "min": 0.5, "max": 2, "step": 0.1 } },
        { "type": "slider", "text": "No range", "valueOptions": { "sourceType": "ModSettingFloat" } },
        { "type": "slider", "text": "No valueOptions" },
        { "type": "slider", "text": "Not numbers", "valueOptions": { "min": "1", "max": null, "step": [5] } }
    )");

    const auto& strength = std::get<model::Setting>(page.items[0]).valueOptions;
    CHECK(strength.min == 0.5f);
    CHECK(strength.max == 2.0f);
    CHECK(strength.step == 0.1f);
    CHECK(strength.options.empty());

    // what a slider has without them
    for (std::size_t i = 1; i < page.items.size(); ++i) {
        const auto& valueOptions = std::get<model::Setting>(page.items[i]).valueOptions;
        CHECK(valueOptions.min == 0.0f);
        CHECK(valueOptions.max == 0.0f);
        CHECK(valueOptions.step == 1.0f);
    }
}

TEST_CASE("McmLoader: the options of a choice have their place in the list as their value")
{
    model::Translations translations;
    translations.add("$MyMod_High", "Haut");

    const auto mod = mcm::loadConfig(R"({ "modName": "MyMod", "content": [
        { "type": "dropdown", "text": "Mode", "valueOptions": { "sourceType": "ModSettingInt", "options": ["Low", "$MyMod_High", 7, "Last"] } },
        { "type": "stepper", "text": "Quality", "valueOptions": { "options": ["Off", "On"] } },
        { "type": "dropdown", "text": "No options", "valueOptions": { "sourceType": "ModSettingInt" } },
        { "type": "stepper", "text": "Options that are no list", "valueOptions": { "options": "Low" } }
    ] })",
        translations);
    REQUIRE(mod);
    const auto optionsOf = [&mod](const std::size_t row) {
        return std::get<model::Setting>(mod->frontPage.items[row]).valueOptions.options;
    };
    using Options = std::vector<model::Option>;

    // an option's text is translated, and one that is no text keeps its place
    CHECK(optionsOf(0) == Options{ { 0, "Low" }, { 1, "Haut" }, { 2, "" }, { 3, "Last" } });
    CHECK(optionsOf(1) == Options{ { 0, "Off" }, { 1, "On" } });
    CHECK(optionsOf(2).empty());
    CHECK(optionsOf(3).empty());
}

TEST_CASE("McmLoader: a dropdownFiles has None and the files of its folder that match its mask, in the order of their names")
{
    const test_files::TempFolder folder;
    folder.write("Data/Interface/Presets/b.xml", "");
    folder.write("Data/Interface/Presets/A.xml", "");
    folder.write("Data/Interface/Presets/c.txt", "");
    folder.write("Data/Interface/Presets/Sub/d.xml", "");
    folder.write("Data/MCM/Config/MyMod/config.json", R"({ "modName": "MyMod", "content": [
        { "type": "dropdownFiles", "text": "Masked", "id": "sPreset:Main", "valueOptions": { "sourceType": "ModSettingString", "path": "Data/Interface/Presets", "mask": "*.xml" } },
        { "type": "dropdownFiles", "text": "No mask", "valueOptions": { "path": "Data\\Interface\\Presets" } },
        { "type": "dropdownFiles", "text": "In a global", "valueOptions": { "sourceType": "GlobalValue", "sourceForm": "MyMod.esp|800", "path": "Data/Interface/Presets", "mask": "*.xml" } },
        { "type": "dropdownFiles", "text": "No such folder", "valueOptions": { "path": "Data/Interface/NoSuchFolder" } },
        { "type": "dropdownFiles", "text": "No path" }
    ] })");

    const auto mods = mcm::loadConfigs(folder.path / "Data" / "MCM" / "Config");
    REQUIRE(mods.size() == 1);
    const auto& items = mods[0].frontPage.items;
    const auto optionsOf = [&items](const std::size_t row) {
        return std::get<model::Setting>(items[row]).valueOptions.options;
    };
    using Options = std::vector<model::Option>;

    CHECK(std::get<model::Setting>(items[0]).type == model::SettingType::Dropdown);
    // a storage that keeps a text keeps the name of the file, and a folder in the folder is no file
    CHECK(optionsOf(0) == Options{ { ""s, "None" }, { "A.xml"s, "A.xml" }, { "b.xml"s, "b.xml" } });
    CHECK(optionsOf(1) == Options{ { ""s, "None" }, { "A.xml"s, "A.xml" }, { "b.xml"s, "b.xml" }, { "c.txt"s, "c.txt" } });
    // any other keeps the place of the option, as a dropdown does
    CHECK(optionsOf(2) == Options{ { 0, "None" }, { 1, "A.xml" }, { 2, "b.xml" } });
    CHECK(optionsOf(3) == Options{ { ""s, "None" } });
    CHECK(optionsOf(4) == Options{ { ""s, "None" } });
}

TEST_CASE("McmLoader: a setting is kept in the global its row names, or nowhere")
{
    const auto page = frontPageOf(R"(
        { "type": "switcher", "text": "A global", "valueOptions": { "sourceType": "GlobalValue", "sourceForm": "MyMod.esp|1234" } },
        { "type": "slider", "text": "Its id in any case, with 0x", "valueOptions": { "sourceType": "GlobalValue", "sourceForm": "My Mod.esl | 0x00aBc ", "min": 1, "max": 10 } },
        { "type": "switcher", "text": "No valueOptions" },
        { "type": "stepper", "text": "No sourceType", "valueOptions": { "options": ["Off", "On"] } },
        { "type": "switcher", "text": "A ModSetting", "id": "bOn:Main", "valueOptions": { "sourceType": "ModSettingBool" } },
        { "type": "switcher", "text": "No sourceForm", "valueOptions": { "sourceType": "GlobalValue" } },
        { "type": "switcher", "text": "No id", "valueOptions": { "sourceType": "GlobalValue", "sourceForm": "MyMod.esp" } },
        { "type": "switcher", "text": "No plugin", "valueOptions": { "sourceType": "GlobalValue", "sourceForm": "|1234" } },
        { "type": "switcher", "text": "An id that is not hex", "valueOptions": { "sourceType": "GlobalValue", "sourceForm": "MyMod.esp|12G4" } }
    )");
    const auto storageOf = [&page](const std::size_t row) {
        return std::get<model::Setting>(page.items[row]).storage;
    };

    CHECK(storageOf(0) == model::Storage(model::GlobalStorage{ .plugin = "MyMod.esp", .formId = 0x1234 }));
    CHECK(storageOf(1) == model::Storage(model::GlobalStorage{ .plugin = "My Mod.esl", .formId = 0xABC }));

    // the store holds the value of a row that names no source
    CHECK(storageOf(2) == model::Storage(model::NoStorage{}));
    CHECK(storageOf(3) == model::Storage(model::NoStorage{}));

    CHECK(storageOf(4) == model::Storage(model::ModSettingStorage{ .name = "bOn:Main", .type = model::ModSettingStorage::Type::Bool }));

    // a global that is not written as one
    for (std::size_t row = 5; row < page.items.size(); ++row) {
        CHECK(storageOf(row) == model::Storage(model::UnknownStorage{ .type = "GlobalValue" }));
    }
}

TEST_CASE("McmLoader: a ModSetting with no id is kept nowhere, as it names no setting")
{
    const auto page = frontPageOf(R"(
        { "type": "switcher", "text": "Unfold settings", "valueOptions": { "sourceType": "ModSettingBool" } },
        { "type": "slider", "text": "A property", "valueOptions": { "sourceType": "PropertyValueFloat" } }
    )");

    CHECK(std::get<model::Setting>(page.items[0]).storage == model::Storage(model::NoStorage{}));
    CHECK(std::get<model::Setting>(page.items[1]).storage == model::Storage(model::UnknownStorage{ .type = "PropertyValueFloat" }));
}

TEST_CASE("McmLoader: a property of a script is named by its form and its name, and typed by its sourceType")
{
    using Type = model::StoredType;
    const auto page = frontPageOf(R"(
        { "type": "slider", "text": "A float", "valueOptions": { "sourceType": "PropertyValueFloat", "sourceForm": "MyMod.esp|800", "propertyName": "fStrength" } },
        { "type": "switcher", "text": "A bool of a script", "valueOptions": { "sourceType": "PropertyValueBool", "sourceForm": "MyMod.esp|0x801", "scriptName": "MyMod:Main", "propertyName": "bOn" } },
        { "type": "dropdown", "text": "An int", "valueOptions": { "sourceType": "PropertyValueInt", "sourceForm": "MyMod.esp|800", "propertyName": "iMode" } },
        { "type": "textinput", "text": "A string", "valueOptions": { "sourceType": "PropertyValueString", "sourceForm": "MyMod.esp|800", "propertyName": "sName" } },
        { "type": "switcher", "text": "No propertyName", "valueOptions": { "sourceType": "PropertyValueBool", "sourceForm": "MyMod.esp|800" } },
        { "type": "switcher", "text": "No sourceForm", "valueOptions": { "sourceType": "PropertyValueBool", "propertyName": "bOn" } },
        { "type": "switcher", "text": "An id that is not hex", "valueOptions": { "sourceType": "PropertyValueBool", "sourceForm": "MyMod.esp|8G0", "propertyName": "bOn" } },
        { "type": "switcher", "text": "No type of MCM", "valueOptions": { "sourceType": "PropertyValueColor", "sourceForm": "MyMod.esp|800", "propertyName": "bOn" } }
    )");
    const auto storageOf = [&page](const std::size_t row) {
        return std::get<model::Setting>(page.items[row]).storage;
    };

    CHECK(storageOf(0) == model::Storage(model::PropertyStorage{ .plugin = "MyMod.esp", .formId = 0x800, .script = {}, .name = "fStrength", .type = Type::Float }));
    CHECK(storageOf(1) == model::Storage(model::PropertyStorage{ .plugin = "MyMod.esp", .formId = 0x801, .script = "MyMod:Main", .name = "bOn", .type = Type::Bool }));
    CHECK(storageOf(2) == model::Storage(model::PropertyStorage{ .plugin = "MyMod.esp", .formId = 0x800, .script = {}, .name = "iMode", .type = Type::Int }));
    CHECK(storageOf(3) == model::Storage(model::PropertyStorage{ .plugin = "MyMod.esp", .formId = 0x800, .script = {}, .name = "sName", .type = Type::String }));

    // a property that is not written as one
    for (std::size_t row = 4; row < 7; ++row) {
        CHECK(storageOf(row) == model::Storage(model::UnknownStorage{ .type = "PropertyValueBool" }));
    }
    CHECK(storageOf(7) == model::Storage(model::UnknownStorage{ .type = "PropertyValueColor" }));
}

TEST_CASE("McmLoader: a ModSetting is named by the id of its row, and typed by its sourceType")
{
    using Type = model::ModSettingStorage::Type;
    const auto page = frontPageOf(R"(
        { "type": "switcher", "text": "A bool", "id": "bOn:Main", "valueOptions": { "sourceType": "ModSettingBool" } },
        { "type": "dropdown", "text": "An int", "id": "iMode:Main", "valueOptions": { "sourceType": "ModSettingInt" } },
        { "type": "slider", "text": "A float", "id": "fStrength:Main", "valueOptions": { "sourceType": "ModSettingFloat" } },
        { "type": "textinput", "text": "A string", "id": "sName:Main", "valueOptions": { "sourceType": "ModSettingString" } },
        { "type": "switcher", "text": "The id of another row", "id": "bOn:Main", "valueOptions": { "sourceType": "ModSettingBool" } },
        { "type": "switcher", "text": "No type of MCM", "id": "bOther:Main", "valueOptions": { "sourceType": "ModSettingColor" } }
    )");
    const auto storageOf = [&page](const std::size_t row) {
        return std::get<model::Setting>(page.items[row]).storage;
    };

    CHECK(storageOf(0) == model::Storage(model::ModSettingStorage{ .name = "bOn:Main", .type = Type::Bool }));
    CHECK(storageOf(1) == model::Storage(model::ModSettingStorage{ .name = "iMode:Main", .type = Type::Int }));
    CHECK(storageOf(2) == model::Storage(model::ModSettingStorage{ .name = "fStrength:Main", .type = Type::Float }));
    CHECK(storageOf(3) == model::Storage(model::ModSettingStorage{ .name = "sName:Main", .type = Type::String }));

    // the row has an id of its place, and still the ModSetting its config names
    CHECK(std::get<model::Setting>(page.items[4]).id == "main/5");
    CHECK(storageOf(4) == storageOf(0));

    CHECK(storageOf(5) == model::Storage(model::UnknownStorage{ .type = "ModSettingColor" }));
}

TEST_CASE("McmLoader: a setting has the id of its row, or one from its page and its place")
{
    const auto mod = loaded(R"({
        "modName": "MyMod",
        "content": [
            { "type": "section", "text": "General" },
            { "type": "switcher", "text": "With no id" },
            { "type": "slider", "text": "With an id", "id": "fStrength:Main" },
            { "type": "button", "text": "No setting", "id": "aButton" },
            { "type": "switcher", "text": "The same id again", "id": "fStrength:Main" }
        ],
        "pages": [
            { "pageDisplayName": "Advanced", "content": [{ "type": "switcher", "text": "With no id" }, { "type": "switcher", "text": "An id of the front page", "id": "fStrength:Main" }] },
            { "pageDisplayName": "More", "content": [{ "type": "text", "text": "Some text" }, { "type": "dropdown", "text": "With no id" }] }
        ]
    })");

    CHECK(idsOf(mod.frontPage.items) == Lines{ "main/2", "fStrength:Main", "main/5" });
    CHECK(idsOf(mod.pages[0].items) == Lines{ "page1/1", "page1/2" });
    CHECK(idsOf(mod.pages[1].items) == Lines{ "page2/2" });

    // the id of its row is kept as it is, for MCM's event of a change
    CHECK(sourceIdsOf(mod.frontPage.items) == Lines{ "", "fStrength:Main", "fStrength:Main" });
}

TEST_CASE("McmLoader: a row of a group is shown while the setting that controls the group is on")
{
    const auto page = frontPageOf(R"(
        { "type": "text", "text": "A row before its control", "groupCondition": 2 },
        { "type": "switcher", "text": "Enable the feature", "groupControl": 2 },
        { "type": "slider", "text": "Strength", "id": "fStrength:Main", "groupCondition": 2 },
        { "type": "switcher", "text": "Unfold", "id": "bUnfold:Main", "groupControl": 3, "groupCondition": 2 },
        { "type": "button", "text": "A button", "groupCondition": 3 },
        { "type": "image", "groupCondition": 3 },
        { "type": "spacer", "groupCondition": 3 },
        { "type": "hotkey", "text": "A hotkey", "groupCondition": 3 },
        { "type": "switcher", "text": "With no condition" }
    )");

    CHECK(conditionsOf(page.items) == Lines{ "main/2", "always", "main/2", "main/2", "bUnfold:Main", "bUnfold:Main", "bUnfold:Main", "bUnfold:Main", "always" });
}

TEST_CASE("McmLoader: a condition joins groups with AND, OR, ONLY and a list, and one can be inside another")
{
    const auto page = frontPageOf(R"(
        { "type": "switcher", "text": "One", "id": "one", "groupControl": 1 },
        { "type": "switcher", "text": "Two", "id": "two", "groupControl": 2 },
        { "type": "switcher", "text": "Three", "id": "three", "groupControl": 3 },
        { "type": "text", "text": "Both", "groupCondition": { "AND": [1, 2] } },
        { "type": "text", "text": "Either", "groupCondition": { "OR": [1, 2] } },
        { "type": "text", "text": "A list is either", "groupCondition": [2, 3] },
        { "type": "text", "text": "These and no other", "groupCondition": { "ONLY": [1, 3] } },
        { "type": "text", "text": "One inside another", "groupCondition": { "AND": [1, { "OR": [2, 3] }] } },
        { "type": "text", "text": "One group", "groupCondition": { "AND": [3] } }
    )");

    CHECK(conditionsOf(page.items) ==
          Lines{ "always", "always", "always", "all(one, two)", "any(one, two)", "any(two, three)", "all(one, three, not(any(two)))", "all(one, any(two, three))", "all(three)" });
}

TEST_CASE("McmLoader: a group that no row controls is never on, and the group 0 always is")
{
    const auto page = frontPageOf(R"(
        { "type": "switcher", "text": "One", "id": "one", "groupControl": 1 },
        { "type": "button", "text": "A row with no value controls no group", "groupControl": 5 },
        { "type": "text", "text": "No such group", "groupCondition": 4 },
        { "type": "text", "text": "A group of a button", "groupCondition": 5 },
        { "type": "text", "text": "One and no such group", "groupCondition": { "AND": [1, 4] } },
        { "type": "text", "text": "The group 0", "groupCondition": 0 },
        { "type": "text", "text": "One and the group 0", "groupCondition": { "AND": [1, 0] } },
        { "type": "text", "text": "No operator MCM knows", "groupCondition": { "XOR": [1, 4] } },
        { "type": "text", "text": "Not a condition", "groupCondition": "one" }
    )");

    // any() never holds, and all() always does
    CHECK(conditionsOf(page.items) == Lines{ "always", "always", "any()", "any()", "all(one, any())", "always", "all(one, all())", "always", "always" });
}

TEST_CASE("McmLoader: a group that two rows control is on with the later one")
{
    const auto page = frontPageOf(R"(
        { "type": "switcher", "text": "First", "id": "first", "groupControl": 1 },
        { "type": "switcher", "text": "Second", "id": "second", "groupControl": 1 },
        { "type": "text", "text": "A row of the group", "groupCondition": 1 }
    )");

    CHECK(conditionsOf(page.items).back() == "second");
}

TEST_CASE("McmLoader: a hidden switcher is a switcher that is never shown, and controls its group")
{
    const auto page = frontPageOf(R"(
        { "type": "hiddenSwitcher", "groupControl": 1, "valueOptions": { "sourceType": "GlobalValue", "sourceForm": "MyMod.esp|1752" } },
        { "type": "text", "text": "A row of its group", "groupCondition": 1 },
        { "type": "switcher", "text": "Unfold", "id": "bUnfold:Main", "groupControl": 2 },
        { "type": "hiddenSwitcher", "id": "bBusy:Main", "groupControl": 3, "groupCondition": 2, "valueOptions": { "sourceType": "ModSettingBool" } },
        { "type": "text", "text": "A row of the group of one with a condition", "groupCondition": 3 },
        { "type": "text", "text": "A row of the group of a switcher", "groupCondition": 2 }
    )");

    // any() never holds: its own condition does not show it
    CHECK(conditionsOf(page.items) == Lines{ "any()", "main/1", "always", "any()", "bBusy:Main", "bUnfold:Main" });

    // the group of a hidden switcher is off while its value is not available, and the one of a switcher open
    CHECK(std::get<model::Text>(page.items[1]).visibleWhen->offWhenNotAvailable);
    CHECK(std::get<model::Text>(page.items[4]).visibleWhen->offWhenNotAvailable);
    CHECK_FALSE(std::get<model::Text>(page.items[5]).visibleWhen->offWhenNotAvailable);

    const auto& first = std::get<model::Setting>(page.items[0]);
    CHECK(first.type == model::SettingType::Switcher);
    CHECK(first.storage == model::Storage(model::GlobalStorage{ .plugin = "MyMod.esp", .formId = 0x1752 }));
    CHECK(std::get<model::Setting>(page.items[3]).storage == model::Storage(model::ModSettingStorage{ .name = "bBusy:Main", .type = model::StoredType::Bool }));
}

TEST_CASE("McmLoader: the groups of a page are its own, and a section's condition is of its heading alone")
{
    const auto mod = loaded(R"({
        "modName": "MyMod",
        "content": [
            { "type": "switcher", "text": "Unfold", "id": "front", "groupControl": 1 },
            { "type": "section", "text": "A section of the group", "groupCondition": 1 },
            { "type": "text", "text": "A row of the group", "groupCondition": 1 },
            { "type": "text", "text": "A row that is always shown" }
        ],
        "pages": [
            {
                "pageDisplayName": "Advanced",
                "content": [{ "type": "switcher", "text": "Unfold", "id": "advanced", "groupControl": 1 }, { "type": "text", "text": "A row of the group", "groupCondition": 1 }]
            },
            { "pageDisplayName": "With no control", "content": [{ "type": "text", "text": "A row of a group", "groupCondition": 1 }] }
        ]
    })");

    CHECK(conditionsOf(mod.frontPage.items) == Lines{ "always", "front" });
    CHECK(conditionsOf(std::get<model::Group>(mod.frontPage.items[1]).items) == Lines{ "front", "always" });
    CHECK(conditionsOf(mod.pages[0].items) == Lines{ "always", "advanced" });
    CHECK(conditionsOf(mod.pages[1].items) == Lines{ "any()" });
}

TEST_CASE("McmLoader: a control the menu does not support is a placeholder with its type")
{
    const auto page = frontPageOf(R"(
        { "type": "color", "text": "Tint", "id": "iTint:Main" },
        { "type": "positioner", "text": "Place of the widget" },
        { "type": "keyinput", "text": "Keys" },
        { "type": "switch", "text": "A name MCM's menu does not have" },
        { "type": "Switcher", "text": "A type in another letter case" },
        { "text": "A row with no type" },
        { "type": 3, "text": "A type that is not a text" },
        "not a row"
    )");

    CHECK(describe(page) == Lines{
                                "placeholder color: Tint",
                                "placeholder positioner: Place of the widget",
                                "placeholder keyinput: Keys",
                                "placeholder switch: A name MCM's menu does not have",
                                "placeholder Switcher: A type in another letter case",
                                "placeholder : A row with no type",
                                "placeholder : A type that is not a text",
                                "placeholder : ",
                            });
}

TEST_CASE("McmLoader: a hotkey has the id of its row, which MCM knows it by")
{
    const auto page = frontPageOf(R"(
        { "type": "hotkey", "text": "Open the menu", "id": "openMenu", "valueOptions": { "allowModifierKeys": true } },
        { "type": "hotkey", "text": "With no id" }
    )");

    CHECK(describe(page) == Lines{ "hotkey: Open the menu", "hotkey: With no id" });
    CHECK(std::get<model::Hotkey>(page.items[0]).id == "openMenu");
    CHECK(std::get<model::Hotkey>(page.items[1]).id.empty());
}

TEST_CASE("McmLoader: a text that is missing or is not a text is empty")
{
    const auto page = frontPageOf(R"(
        { "type": "switcher" },
        { "type": "button", "text": 12 },
        { "type": "section", "text": null }
    )");

    CHECK(describe(page) == Lines{ "switcher: ", "button: ", "group: " });
}

TEST_CASE("McmLoader: the rows after a section are its group, up to the next section")
{
    const auto page = frontPageOf(R"(
        { "type": "text", "text": "Before any section" },
        { "type": "switcher", "text": "On the page itself" },
        { "type": "section", "text": "First" },
        { "type": "slider", "text": "In the first" },
        { "type": "spacer" },
        { "type": "section", "text": "Empty" },
        { "type": "section", "text": "Last" },
        { "type": "button", "text": "In the last" }
    )");

    CHECK(describe(page) == Lines{
                                "text: Before any section",
                                "switcher: On the page itself",
                                "group: First",
                                "  slider: In the first",
                                "  spacer",
                                "group: Empty",
                                "group: Last",
                                "  button: In the last",
                            });
}

TEST_CASE("McmLoader: each page groups its own rows")
{
    const auto mod = loaded(R"({
        "modName": "MyMod",
        "content": [{ "type": "section", "text": "On the front page" }],
        "pages": [{ "pageDisplayName": "Main", "content": [{ "type": "switcher", "text": "Not in that section" }] }]
    })");

    CHECK(describe(mod.frontPage) == Lines{ "group: On the front page" });
    CHECK(describe(mod.pages[0]) == Lines{ "switcher: Not in that section" });
}

TEST_CASE("McmLoader: comments are skipped, as MCM's own reader skips them")
{
    const auto mod = loaded(R"({
        // the mod's identity
        "modName": "MyMod",
        /* its rows */
        "content": [{ "type": "switcher", "text": "Enabled" }]
    })");

    CHECK(describe(mod.frontPage) == Lines{ "switcher: Enabled" });
}

TEST_CASE("McmLoader: a file that starts with a byte order mark is read")
{
    CHECK(loaded("\xEF\xBB\xBF{ \"modName\": \"MyMod\" }").name == "MyMod");
}

TEST_CASE("McmLoader: a config that cannot be read says why")
{
    const auto notJson = mcm::loadConfig(R"({ "modName": "MyMod", "content": [ )");
    REQUIRE_FALSE(notJson);
    CHECK(notJson.error().starts_with("Not valid JSON: "));

    const auto empty = mcm::loadConfig("");
    REQUIRE_FALSE(empty);
    CHECK(empty.error().starts_with("Not valid JSON: "));

    // MCM's reader takes no comma after the last item either
    CHECK_FALSE(mcm::loadConfig(R"({ "modName": "MyMod", })"));

    const auto notObject = mcm::loadConfig(R"(["MyMod"])");
    REQUIRE_FALSE(notObject);
    CHECK(notObject.error() == "Not a JSON object");

    const auto noName = mcm::loadConfig(R"({ "displayName": "My Mod", "content": [] })");
    REQUIRE_FALSE(noName);
    CHECK(noName.error() == "No modName");
}

TEST_CASE("McmLoader: the config of every folder in MCM's config folder is loaded")
{
    const test_files::TempFolder folder;
    folder.write("AutoEat/config.json", R"({ "modName": "AutoEat", "content": [{ "type": "switcher", "text": "Enabled" }] })");
    folder.write("SS2/config.json", R"({ "modName": "SS2", "displayName": "Sim Settlements 2" })");
    // MCM keeps a mod's other files beside its config
    folder.write("SS2/settings.ini", "[Main] bEnabled=1");
    folder.write("SS2/keybinds.json", R"({ "modName": "SS2", "keybinds": [] })");

    const auto mods = mcm::loadConfigs(folder.path);

    CHECK(namesOf(mods) == Lines{ "AutoEat", "SS2" });
    const auto autoEat = std::ranges::find(mods, "AutoEat", &model::Mod::name);
    REQUIRE(autoEat != mods.end());
    CHECK(describe(autoEat->frontPage) == Lines{ "switcher: Enabled" });
}

TEST_CASE("McmLoader: a text that is a token is shown in the game's language")
{
    model::Translations translations;
    translations.add("$MyMod_Name", "Mon Mod");
    translations.add("$MyMod_Page", "Combat");
    translations.add("$MyMod_Section", "Notifications");
    translations.add("$MyMod_Enabled", "Actif");

    const auto config = R"({
        "modName": "$MyMod_Name",
        "displayName": "$MyMod_Name",
        "content": [
            { "type": "section", "text": "$MyMod_Section" },
            { "type": "switcher", "text": "$MyMod_Enabled" },
            { "type": "button", "text": "$MyMod_Missing" },
            { "type": "text", "text": "Costs $5" }
        ],
        "pages": [{ "pageDisplayName": "$MyMod_Page", "content": [{ "type": "hotkey", "text": "$MyMod_Enabled" }] }]
    })";
    const auto mod = mcm::loadConfig(config, translations);
    REQUIRE(mod);

    // the mod's name is what the mod is known by, and is not a text to show
    CHECK(mod->name == "$MyMod_Name");
    CHECK(mod->displayName == "Mon Mod");
    // a token that no file has stays as it is written, as in MCM
    CHECK(describe(mod->frontPage) == Lines{ "group: Notifications", "  switcher: Actif", "  button: $MyMod_Missing", "  text: Costs $5" });
    REQUIRE(mod->pages.size() == 1);
    CHECK(mod->pages[0].name == "Combat");
    CHECK(describe(mod->pages[0]) == Lines{ "hotkey: Actif" });

    // with no translations every text is as it is written
    CHECK(loaded(config).displayName == "$MyMod_Name");
}

TEST_CASE("McmLoader: the mod's own MCM showcase is loaded")
{
    const auto mods = mcm::loadConfigs(MODS_CONFIG_MCM_SHOWCASE_DIR);

    REQUIRE(mods.size() == 1);
    CHECK(mods[0].name == "ModsConfigFO4_McmShowcase");
    CHECK(mods[0].displayName == "Mods Config FO4: MCM Compatibility");
    CHECK_FALSE(mods[0].frontPage.items.empty());
    CHECK(mods[0].pages.size() == 10);

    // its images are read from its lib.swf, but for the one of a class the file does not have
    Lines images;
    for (const auto& page : mods[0].pages) {
        for (const auto& line : describe(page)) {
            // a row of a section is set in under it
            if (const auto row = line.substr(line.find_first_not_of(' ')); row.starts_with("image")) {
                images.push_back(row);
            }
        }
    }
    CHECK(images == Lines{ "image of 256 by 64", "image of 690 by 43.125", "image of 64 by 64", "image of 256 by 64", "image" });
}

TEST_CASE("McmLoader: the dropdownFiles of the MCM showcase list the files of its own config folder, in the game it is installed in")
{
    // the showcase is a Data folder: a game has it in its own folder, which a path of a config is from
    const test_files::TempFolder game;
    std::filesystem::copy(std::filesystem::path(MODS_CONFIG_MCM_SHOWCASE_DIR).parent_path().parent_path(), game.path / "Data", std::filesystem::copy_options::recursive);
    const auto mods = mcm::loadConfigs(game.path / "Data" / "MCM" / "Config");
    REQUIRE(mods.size() == 1);

    const auto page = std::ranges::find(mods[0].pages, "Steppers and dropdowns", &model::Page::name);
    REQUIRE(page != mods[0].pages.end());
    const auto section = std::ranges::find_if(page->items, [](const model::Item& item) {
        const auto* group = std::get_if<model::Group>(&item);
        return group && group->text == "Dropdowns of files";
    });
    REQUIRE(section != page->items.end());
    const auto& rows = std::get<model::Group>(*section).items;
    REQUIRE(rows.size() == 3);
    const auto textsOf = [&rows](const std::size_t row) {
        Lines texts;
        for (const auto& option : std::get<model::Setting>(rows[row]).valueOptions.options) {
            texts.push_back(option.text);
        }
        return texts;
    };
    CHECK(textsOf(0) == Lines{ "None", "config.json", "keybinds.json" });
    CHECK(textsOf(1) == Lines{ "None", "config.json", "keybinds.json", "lib.swf", "settings.ini" });
    CHECK(textsOf(2) == Lines{ "None" });
}

TEST_CASE("McmLoader: an image is read from the lib.swf of the mod its row names")
{
    constexpr test_swf::Pixel RED{ 255, 0, 0, 255 };
    test_swf::SwfBuilder swf;
    swf.bitmap(1, 2, 1, { RED, RED });
    swf.bitmap(2, 1, 1, { RED });
    swf.bitmapShape(3, 2, 1380, 200);
    swf.sprite(4, { 3 });
    swf.bitmapShape(5, 2, 100, 800);
    swf.sprite(6, { 5 });
    swf.symbol(1, "Logo");
    swf.symbol(4, "Wide");
    swf.symbol(6, "High");
    const auto bytes = swf.build();

    const test_files::TempFolder folder;
    folder.write("Images/lib.swf", std::string(bytes.begin(), bytes.end()));
    folder.write("MyMod/config.json", R"({ "modName": "MyMod", "content": [
        { "type": "image", "libName": "Images", "className": "Logo" },
        { "type": "image", "libName": "Images", "className": "Wide" },
        { "type": "image", "libName": "Images", "className": "High" },
        { "type": "image", "libName": "Images", "className": "NoSuchClass" },
        { "type": "image", "libName": "NoSuchMod", "className": "Logo" },
        { "type": "image" }
    ] })");

    const auto mods = mcm::loadConfigs(folder.path);

    REQUIRE(mods.size() == 1);
    // an image that is wider or higher than MCM's menu has room for is shown smaller, as it is there
    CHECK(describe(mods[0].frontPage) == Lines{ "image of 2 by 1", "image of 690 by 100", "image of 50 by 400", "image", "image", "image" });
    const auto& logo = std::get<model::Image>(mods[0].frontPage.items[0]);
    CHECK(logo.pixels->name == "Logo");
    CHECK(logo.pixels->rgba == std::vector<std::uint8_t>{ 255, 0, 0, 255, 255, 0, 0, 255 });
}

TEST_CASE("McmLoader: an M8r intro is the image of the class of the file its code loads")
{
    constexpr test_swf::Pixel RED{ 255, 0, 0, 255 };
    // a logo as M8r's EmbeddedImage has it: the class of the file is code, with its bitmap beside it
    const auto logoOf = [](const int width) {
        test_swf::SwfBuilder swf;
        swf.bitmap(1, width, 3, std::vector(static_cast<std::size_t>(width) * 3, RED));
        swf.symbol(1, "M8r.View.McmIntros.MyMod_bitmap");
        swf.symbol(0, "M8r.View.McmIntros.MyMod");
        const auto bytes = swf.build();
        return std::string(bytes.begin(), bytes.end());
    };

    const test_files::TempFolder folder;
    folder.write("MyMod/logo.swf", logoOf(6));
    folder.write("MyMod/other.swf", logoOf(9));
    folder.write("MyMod/config.json", R"({ "modName": "MyMod", "content": [
        { "type": "image", "libName": "MyMod", "className": "M8r.View.McmIntros.M8rIntro" },
        { "type": "image", "libName": "MyMod", "className": "M8r.View.McmIntros.M8rIntroLight" },
        { "type": "image", "libName": "MyMod", "className": "M8r.View.McmIntros.M8rIntroLight", "m8rIntro": { "file": "other.swf" } },
        { "type": "image", "libName": "MyMod", "className": "M8r.View.McmIntros.M8rIntroLight", "m8rIntro": { "mouseEnabled": true } },
        { "type": "image", "libName": "NoSuchMod", "className": "M8r.View.McmIntros.M8rIntro" }
    ] })");

    const auto mods = mcm::loadConfigs(folder.path);

    REQUIRE(mods.size() == 1);
    // at 2/3 of the bitmap's size, as EmbeddedImage shows it. An intro whose row names no file shows none
    CHECK(describe(mods[0].frontPage) == Lines{ "image of 4 by 2", "image of 4 by 2", "image of 6 by 2", "image", "image" });
}

TEST_CASE("McmLoader: a row with sharedOptions has the options of that list of the config")
{
    const auto mod = loaded(R"({ "modName": "MyMod",
        "sharedLists": { "weather": ["$Clear", "Rain"], "empty": [] },
        "content": [
            { "type": "dropdown", "text": "Weather", "valueOptions": { "sharedOptions": "weather", "options": ["Own"] } }
        ],
        "pages": [{ "pageDisplayName": "Page", "content": [
            { "type": "stepper", "text": "Weather", "valueOptions": { "sharedOptions": "weather" } },
            { "type": "dropdown", "text": "No such list", "valueOptions": { "sharedOptions": "noSuchList", "options": ["Own"] } },
            { "type": "dropdown", "text": "Own", "valueOptions": { "options": ["Own"] } }
        ] }]
    })");

    const auto optionsOf = [](const model::Item& item) {
        Lines texts;
        for (const auto& option : std::get<model::Setting>(item).valueOptions.options) {
            texts.push_back(option.text);
        }
        return texts;
    };
    // in place of the row's own, and none for a list that is not there, as in MCM's menu
    CHECK(optionsOf(mod.frontPage.items[0]) == Lines{ "$Clear", "Rain" });
    CHECK(optionsOf(mod.pages[0].items[0]) == Lines{ "$Clear", "Rain" });
    CHECK(optionsOf(mod.pages[0].items[1]).empty());
    CHECK(optionsOf(mod.pages[0].items[2]) == Lines{ "Own" });
}

TEST_CASE("McmLoader: a stepper and a dropdown read their options in the game from a FormList or a property, in MCM's order")
{
    const auto mod = loaded(R"({ "modName": "MyMod",
        "sharedLists": { "weather": ["Clear", "Rain"] },
        "content": [
            { "type": "dropdown", "text": "Settlement", "valueOptions": { "sourceType": "GlobalValue", "sourceForm": "MyMod.esp|801",
                "listFromForm": "MyMod.esp|0x900", "options": ["Own"] } },
            { "type": "stepper", "text": "Name", "valueOptions": {
                "listFromProperty": { "form": "MyMod.esp|800", "scriptName": "MyMod:Config", "propertyName": "Names" } } },
            { "type": "dropdown", "text": "Both", "valueOptions": { "listFromForm": "MyMod.esp|900",
                "listFromProperty": { "form": "MyMod.esp|800", "propertyName": "Names" } } },
            { "type": "dropdown", "text": "Shared", "valueOptions": { "sharedOptions": "weather", "listFromForm": "MyMod.esp|900" } },
            { "type": "dropdown", "text": "No form", "valueOptions": { "listFromForm": "MyMod.esp", "options": ["Own"] } },
            { "type": "stepper", "text": "No name", "valueOptions": { "listFromProperty": { "form": "MyMod.esp|800" }, "options": ["Own"] } },
            { "type": "switcher", "text": "Not a choice", "valueOptions": { "listFromForm": "MyMod.esp|900" } }
        ]
    })");
    const auto valueOptionsOf = [&mod](const std::size_t row) {
        return std::get<model::Setting>(mod.frontPage.items[row]).valueOptions;
    };
    using Options = std::vector<model::Option>;

    // in place of the row's own options, and an option's value is its place, as MCM stores it
    const auto formList = valueOptionsOf(0);
    CHECK(formList.gameOptions == model::GameOptions{ .plugin = "MyMod.esp", .formId = 0x900, .script = {}, .property = {}, .storesText = false });
    CHECK(formList.options.empty());
    CHECK(valueOptionsOf(1).gameOptions == model::GameOptions{ .plugin = "MyMod.esp", .formId = 0x800, .script = "MyMod:Config", .property = "Names", .storesText = false });

    // a FormList comes before a property, and a shared list before both
    CHECK(valueOptionsOf(2).gameOptions->property.empty());
    CHECK_FALSE(valueOptionsOf(3).gameOptions);
    CHECK(valueOptionsOf(3).options == Options{ { 0, "Clear" }, { 1, "Rain" } });

    // a list that is not written so leaves no options, as MCM's menu has none of it
    CHECK_FALSE(valueOptionsOf(4).gameOptions);
    CHECK(valueOptionsOf(4).options.empty());
    CHECK_FALSE(valueOptionsOf(5).gameOptions);
    CHECK(valueOptionsOf(5).options.empty());
    CHECK_FALSE(valueOptionsOf(6).gameOptions);
}

TEST_CASE("McmLoader: the text of a row that is not a text row is shown without its tags, as MCM sets it as html")
{
    model::Translations translations;
    translations.add("$Settings", "Settings");
    const auto mod = mcm::loadConfig(R"({ "modName": "MyMod", "content": [
        { "type": "section", "text": "<font size='20'><b>$Settings</b></font>" },
        { "type": "switcher", "text": "<font size='15' face='$DebugTextFont'><u>Show Advanced</u></font>", "html": true },
        { "type": "button", "text": "Ammo &amp; guns" },
        { "type": "switcher", "text": "    - Indented" }
    ] })",
        translations);
    REQUIRE(mod);

    const auto& section = std::get<model::Group>(mod->frontPage.items[0]);
    // a token of the game's inside the tags is translated
    CHECK(section.text == "Settings");
    CHECK(std::get<model::Setting>(section.items[0]).text == "Show Advanced");
    CHECK(std::get<model::Button>(section.items[1]).text == "Ammo & guns");
    // the spaces a row is set in by stay
    CHECK(std::get<model::Setting>(section.items[2]).text == "    - Indented");
}

TEST_CASE("McmLoader: the text of a row that is not a text row has the size, the color and the face of its tags in its pieces")
{
    model::Translations translations;
    translations.add("$Modes", "Show modes");
    const auto mod = mcm::loadConfig(R"({ "modName": "MyMod", "content": [
        { "type": "switcher", "text": "<font size='16' color='#AAAAAA'>$Modes</font>" },
        { "type": "button", "text": "Spawn <font color='#FF0000'>now</font><br>please" },
        { "type": "hotkey", "text": "<b>Bold</b> &amp; <u>underlined</u>" },
        { "type": "colorPicker", "text": "<font size='15'>Tint</font>" },
        { "type": "switcher", "text": "<p>Plain</p>" }
    ] })",
        translations);
    REQUIRE(mod);
    const auto& items = mod->frontPage.items;

    // a token of the game's that is a piece by itself is translated
    const auto& setting = std::get<model::Setting>(items[0]);
    CHECK(setting.text == "Show modes");
    CHECK(setting.textRuns == std::vector<model::TextRun>{ { .text = "Show modes", .size = 16, .color = 0xAAAAAA } });

    // a line break is a space, as in its plain text
    CHECK(std::get<model::Button>(items[1]).textRuns == std::vector<model::TextRun>{ { .text = "Spawn ", .size = {}, .color = {} },
                                                            { .text = "now", .size = {}, .color = 0xFF0000 },
                                                            { .text = " ", .size = {}, .color = {} },
                                                            { .text = "please", .size = {}, .color = {} } });

    CHECK(std::get<model::Hotkey>(items[2]).textRuns == std::vector<model::TextRun>{ { .text = "Bold", .size = {}, .color = {}, .bold = true },
                                                            { .text = " & ", .size = {}, .color = {} },
                                                            { .text = "underlined", .size = {}, .color = {}, .underline = true } });
    CHECK(std::get<model::Placeholder>(items[3]).textRuns == std::vector<model::TextRun>{ { .text = "Tint", .size = 15, .color = {} } });

    // tags that give no size, no color and no face leave the text plain
    CHECK(std::get<model::Setting>(items[4]).textRuns.empty());
}

TEST_CASE("McmLoader: the configs of a folder are translated")
{
    const test_files::TempFolder folder;
    folder.write("AutoEat/config.json", R"({ "modName": "AutoEat", "displayName": "$AutoEat" })");
    model::Translations translations;
    translations.add("$AutoEat", "Auto Eat");

    const auto mods = mcm::loadConfigs(folder.path, translations);

    REQUIRE(mods.size() == 1);
    CHECK(mods[0].displayName == "Auto Eat");
}

TEST_CASE("McmLoader: a config that cannot be read is a mod that is not available, named by its folder")
{
    const test_files::TempFolder folder;
    folder.write("Broken Mod/config.json", R"({ "modName": "Broken", )");
    folder.write("NoName/config.json", R"({ "displayName": "No name" })");
    folder.write("Good/config.json", R"({ "modName": "Good", "content": [{ "type": "switcher", "text": "Enabled" }] })");

    const auto mods = mcm::loadConfigs(folder.path);

    CHECK(namesOf(mods) == Lines{ "Broken Mod", "Good", "NoName" });
    for (const auto& mod : mods) {
        if (mod.name == "Good") {
            CHECK(mod.notAvailable.empty());
            CHECK(describe(mod.frontPage) == Lines{ "switcher: Enabled" });
        } else {
            CHECK(mod.displayName == mod.name);
            CHECK(mod.notAvailable == "its config cannot be read");
            CHECK(mod.frontPage.items.empty());
            CHECK(mod.pages.empty());
        }
    }
}

TEST_CASE("McmLoader: the configs of a folder are checked for their required plugins")
{
    const test_files::TempFolder folder;
    folder.write("Loaded/config.json", R"({ "modName": "Loaded", "pluginRequirements": ["Loaded.esp"] })");
    folder.write("Missing/config.json", R"({ "modName": "Missing", "pluginRequirements": ["Missing.esp"] })");

    const auto mods = mcm::loadConfigs(folder.path, {}, {}, [](const std::string_view plugin) {
        return plugin == "Loaded.esp";
    });

    REQUIRE(namesOf(mods) == Lines{ "Loaded", "Missing" });
    const auto missing = std::ranges::find(mods, "Missing", &model::Mod::name);
    CHECK(missing->notAvailable == "its plugin Missing.esp is not loaded");
    CHECK(std::ranges::find(mods, "Loaded", &model::Mod::name)->notAvailable.empty());
}

TEST_CASE("McmLoader: what is in the config folder and is not a mod's config is passed over")
{
    const test_files::TempFolder folder;
    folder.write("Good/config.json", R"({ "modName": "Good" })");
    // a folder with no config, a file beside the folders, and a config one folder too deep
    folder.write("NoConfig/settings.ini", "[Main]");
    folder.write("config.json", R"({ "modName": "Beside" })");
    folder.write("Outer/Inner/config.json", R"({ "modName": "TooDeep" })");

    CHECK(namesOf(mcm::loadConfigs(folder.path)) == Lines{ "Good" });
}

TEST_CASE("McmLoader: no MCM mod is listed while MCM itself is not loaded")
{
    // the tests have no MCM in their process
    CHECK(mcm::loadInstalledMods().empty());
}

TEST_CASE("McmLoader: with no config folder there are no mods")
{
    const test_files::TempFolder folder;

    CHECK(mcm::loadConfigs(folder.path / "MCM" / "Config").empty());
    // an empty one
    CHECK(mcm::loadConfigs(folder.path).empty());
}

TEST_CASE("McmLoader: a row has its help, in the game's language and without its tags")
{
    model::Translations translations;
    translations.add("$MyMod_StrengthHelp", "La force de la fonction.");

    const auto mod = mcm::loadConfig(R"({ "modName": "MyMod", "content": [
        { "type": "switcher", "text": "Enabled", "help": "Turns the feature on." },
        { "type": "slider", "text": "Strength", "help": "$MyMod_StrengthHelp" },
        { "type": "button", "text": "Reset", "help": "<p>Puts every value back. </p><p>It asks first. <font color='#aaaaaa'>(Default: ON)</font></p>" },
        { "type": "hotkey", "text": "Open the menu", "help": "One line<br>and the next" },
        { "type": "color", "text": "Tint", "help": "The color of the light." },
        { "type": "stepper", "text": "No help" },
        { "type": "dropdown", "text": "A help that is no text", "help": 7 }
    ] })",
        translations);
    REQUIRE(mod);
    const auto& rows = mod->frontPage.items;

    CHECK(std::get<model::Setting>(rows[0]).help == "Turns the feature on.");
    CHECK(std::get<model::Setting>(rows[1]).help == "La force de la fonction.");
    // a help is one line of plain text, with or without html on its row
    CHECK(std::get<model::Button>(rows[2]).help == "Puts every value back. It asks first. (Default: ON)");
    CHECK(std::get<model::Hotkey>(rows[3]).help == "One line and the next");
    // a control the menu does not support has its help too
    CHECK(std::get<model::Placeholder>(rows[4]).help == "The color of the light.");
    CHECK(std::get<model::Setting>(rows[5]).help.empty());
    CHECK(std::get<model::Setting>(rows[6]).help.empty());
}

TEST_CASE("McmLoader: a button has the function its action calls, with its parameters in their types")
{
    const auto page = frontPageOf(R"(
        { "type": "button", "text": "With no parameters", "action": { "type": "CallFunction", "form": "MyMod.esp|800", "function": "ResetAll" } },
        {
            "type": "button", "text": "With a script and parameters",
            "action": {
                "type": "CallFunction", "form": "MyMod.esp|0x1F", "scriptName": "MyMod:Main", "function": "Apply",
                "params": ["{i}40", "{i}-1", "{f}1.5", "{b}1", "{b}0", "{b}true", "{b}false", "{b}", "a text", "{s}kept", true, 4, 1.5, 2.0]
            }
        },
        {
            "type": "button", "text": "With what is no number",
            "action": { "type": "CallFunction", "form": "MyMod.esp|800", "function": "Apply", "params": ["{i}many", "{i}1.9", "{f}", "{i}99999999999", null, [1]] }
        }
    )");

    const auto actionOf = [&page](const std::size_t place) {
        return std::get<model::Button>(page.items[place]).action;
    };
    using Arguments = std::vector<model::Value>;

    CHECK(actionOf(0) == model::Action(model::CallFunction{ .plugin = "MyMod.esp", .formId = 0x800, .script = {}, .function = "ResetAll", .arguments = {} }));

    // a whole number, a number and a bool by the start of a text. A JSON number is a number, also a whole one
    const auto typed = std::get<model::CallFunction>(actionOf(1));
    CHECK(typed.formId == 0x1F);
    CHECK(typed.script == "MyMod:Main");
    CHECK(typed.function == "Apply");
    CHECK(typed.arguments == Arguments{ 40, -1, 1.5f, true, false, true, false, false, "a text"s, "{s}kept"s, true, 4.0f, 1.5f, 2.0f });

    // a text that is no number is 0, a whole number is cut at its point and kept in what Papyrus has, and
    // what is no bool, number or text is an empty text
    CHECK(std::get<model::CallFunction>(actionOf(2)).arguments == Arguments{ 0, 1, 0.0f, 2147483647, ""s, ""s });
}

TEST_CASE("McmLoader: a button has the global function its action calls, with no form")
{
    const auto page = frontPageOf(R"(
        { "type": "button", "text": "Notify", "action": { "type": "CallGlobalFunction", "script": "Debug", "function": "Notification", "params": ["Hi", "{i}2"] } },
        { "type": "button", "text": "A form is not read", "action": { "type": "CallGlobalFunction", "form": "MyMod.esp|800", "scriptName": "MyMod:Main", "script": "MyMod:Utils", "function": "Apply" } }
    )");

    const auto actionOf = [&page](const std::size_t place) {
        return std::get<model::Button>(page.items[place]).action;
    };
    using Arguments = std::vector<model::Value>;

    CHECK(actionOf(0) ==
          model::Action(model::CallFunction{ .plugin = {}, .formId = 0, .script = "Debug", .function = "Notification", .arguments = Arguments{ "Hi"s, 2 }, .valueArguments = {} }));
    CHECK(actionOf(1) == model::Action(model::CallFunction{ .plugin = {}, .formId = 0, .script = "MyMod:Utils", .function = "Apply", .arguments = {}, .valueArguments = {} }));
}

TEST_CASE("McmLoader: a button with no action runs nothing, and one with another action is not run, with the reason")
{
    const auto page = frontPageOf(R"(
        { "type": "button", "text": "No action" },
        { "type": "button", "text": "An action that is no object", "action": "CallFunction" },
        { "type": "button", "text": "No form", "action": { "type": "CallFunction", "function": "ResetAll" } },
        { "type": "button", "text": "A form that is no id", "action": { "type": "CallFunction", "form": "MyMod.esp|Quest", "function": "ResetAll" } },
        { "type": "button", "text": "No function", "action": { "type": "CallFunction", "form": "MyMod.esp|800" } },
        { "type": "button", "text": "A global function with no script", "action": { "type": "CallGlobalFunction", "function": "Notification" } },
        { "type": "button", "text": "A global function with no name", "action": { "type": "CallGlobalFunction", "script": "Debug" } },
        { "type": "button", "text": "A Flash function", "action": { "type": "CallExternalFunction", "plugin": "MyPlugin", "function": "Apply" } },
        { "type": "button", "text": "A hotkey's action", "action": { "type": "RunConsoleCommand", "command": "tgm" } },
        { "type": "button", "text": "No type", "action": { "function": "ResetAll" } }
    )");

    const auto actionOf = [&page](const std::size_t place) {
        return std::get<model::Button>(page.items[place]).action;
    };
    for (std::size_t place = 0; place < 7; ++place) {
        CHECK(actionOf(place) == model::Action(model::NoAction{}));
    }
    CHECK(actionOf(7) == model::Action(model::UnknownAction{ .type = "CallExternalFunction",
                             .reason = "Not available: a CallExternalFunction calls a Flash function of an F4SE plugin, and the menu has no Flash." }));
    CHECK(actionOf(8) == model::Action(model::UnknownAction{ .type = "RunConsoleCommand", .reason = "Not available: the menu does not run a RunConsoleCommand." }));
    CHECK(actionOf(9) == model::Action(model::UnknownAction{ .type = {}, .reason = "Not available: the action has no type." }));
}

TEST_CASE("McmLoader: a value row has the function its action calls, with the parameters its value is put in")
{
    const auto page = frontPageOf(R"(
        {
            "type": "slider", "text": "A global", "valueOptions": { "sourceType": "GlobalValue", "sourceForm": "MyMod.esp|801" },
            "action": {
                "type": "CallFunction", "form": "MyMod.esp|800", "function": "Apply",
                "params": ["{value}", "{i}{value}", "{b}{value}", "{f}{value}", "Now {value}", "{i}7", "kept"]
            }
        },
        {
            "type": "switcher", "text": "A ModSetting", "id": "bOn:Main", "valueOptions": { "sourceType": "ModSettingBool" },
            "action": { "type": "CallFunction", "form": "MyMod.esp|800", "function": "Apply", "params": ["{value}"] }
        },
        {
            "type": "dropdown", "text": "A property",
            "valueOptions": { "sourceType": "PropertyValueInt", "sourceForm": "MyMod.esp|800", "propertyName": "iMode", "options": ["Low", "High"] },
            "action": { "type": "CallFunction", "form": "MyMod.esp|800", "function": "Apply", "params": ["{value}"] }
        },
        {
            "type": "switcher", "text": "Kept nowhere",
            "action": { "type": "CallFunction", "form": "MyMod.esp|800", "function": "Apply", "params": ["{value}"] }
        },
        { "type": "switcher", "text": "No action" },
        { "type": "button", "text": "A button", "action": { "type": "CallFunction", "form": "MyMod.esp|800", "function": "Apply", "params": ["{value}"] } }
    )");

    const auto callOf = [&page](const std::size_t place) {
        return std::get<model::CallFunction>(std::get<model::Setting>(page.items[place]).action);
    };
    using Arguments = std::vector<model::Value>;
    using ValueArguments = std::vector<model::ValueArgument>;
    using Type = model::StoredType;

    // a parameter with the value has its place kept by an empty text. `{value}` alone is in the type of the
    // row's storage, which is a number for a global, and the start of a text gives it another
    const auto global = callOf(0);
    CHECK(global.function == "Apply");
    CHECK(global.arguments == Arguments{ ""s, ""s, ""s, ""s, ""s, 7, "kept"s });
    CHECK(global.valueArguments == ValueArguments{ { .place = 0, .text = "{value}", .type = Type::Float },
                                       { .place = 1, .text = "{value}", .type = Type::Int },
                                       { .place = 2, .text = "{value}", .type = Type::Bool },
                                       { .place = 3, .text = "{value}", .type = Type::Float },
                                       { .place = 4, .text = "Now {value}", .type = Type::String } });

    // a ModSetting and a property are what they are kept as, and a row that is kept nowhere is a text
    CHECK(callOf(1).valueArguments == ValueArguments{ { .place = 0, .text = "{value}", .type = Type::Bool } });
    CHECK(callOf(2).valueArguments == ValueArguments{ { .place = 0, .text = "{value}", .type = Type::Int } });
    CHECK(callOf(3).valueArguments == ValueArguments{ { .place = 0, .text = "{value}", .type = Type::String } });

    CHECK(std::get<model::Setting>(page.items[4]).action == model::Action(model::NoAction{}));

    // a button has no value: its `{value}` is a text as it is written
    const auto button = std::get<model::CallFunction>(std::get<model::Button>(page.items[5]).action);
    CHECK(button.arguments == Arguments{ "{value}"s });
    CHECK(button.valueArguments.empty());
}

TEST_CASE("McmLoader: a value row has the global function its action calls, with the value, and another action is not run")
{
    // as JOURNEY has them
    const auto page = frontPageOf(R"(
        {
            "type": "switcher", "text": "Running", "id": "bQuestIsRunning:JOURNEY", "valueOptions": { "sourceType": "ModSettingBool" },
            "action": { "type": "CallGlobalFunction", "script": "M8rJourney", "function": "SetQuestRunning", "params": ["{b}{value}"] }
        },
        {
            "type": "dropdown", "text": "Travel mode", "valueOptions": { "sourceType": "GlobalValue", "sourceForm": "Journey.esp|5BAE", "options": ["A", "B"] },
            "action": { "type": "CallGlobalFunction", "script": "M8rJourney", "function": "SetTravelMode", "params": ["{i}{value}"] }
        },
        {
            "type": "switcher", "text": "A Flash function", "valueOptions": { "sourceType": "GlobalValue", "sourceForm": "MyMod.esp|801" },
            "action": { "type": "CallExternalFunction", "plugin": "MyPlugin", "function": "Apply", "params": ["{value}"] }
        }
    )");

    const auto actionOf = [&page](const std::size_t place) {
        return std::get<model::Setting>(page.items[place]).action;
    };
    using Arguments = std::vector<model::Value>;
    using ValueArguments = std::vector<model::ValueArgument>;
    using Type = model::StoredType;

    CHECK(actionOf(0) == model::Action(model::CallFunction{ .plugin = {},
                             .formId = 0,
                             .script = "M8rJourney",
                             .function = "SetQuestRunning",
                             .arguments = Arguments{ ""s },
                             .valueArguments = ValueArguments{ { .place = 0, .text = "{value}", .type = Type::Bool } } }));
    CHECK(std::get<model::CallFunction>(actionOf(1)).valueArguments == ValueArguments{ { .place = 0, .text = "{value}", .type = Type::Int } });

    // its row is set as any other, and nothing runs
    CHECK(std::get<model::UnknownAction>(actionOf(2)).type == "CallExternalFunction");
}
