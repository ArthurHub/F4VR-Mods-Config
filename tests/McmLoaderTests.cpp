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
                                "textinput: A text",
                                "textinput: A whole number",
                                "textinput: A number",
                                "button: A button",
                                "text: Some words",
                                "image",
                                "spacer of 2 lines",
                            });
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
        { "type": "text", "html": true },
        { "type": "text", "text": "<font color='#AAAAAA' size='16'>A note</font>", "html": 1 },
        { "type": "text", "text": "A <b>plain</b> text", "html": 0 }
    )");

    CHECK(describe(page) == Lines{ "text: My Mod / What it does. / And how.",
                                "text: One line / and the next",
                                "text: A <b>plain</b> text",
                                "text: A <b>plain</b> text",
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

TEST_CASE("McmLoader: a control the menu does not support is a placeholder with its type")
{
    const auto page = frontPageOf(R"(
        { "type": "hotkey", "text": "Open the menu", "id": "openMenu" },
        { "type": "dropdownFiles", "text": "Preset" },
        { "type": "Switcher", "text": "A type in another letter case" },
        { "text": "A row with no type" },
        { "type": 3, "text": "A type that is not a text" },
        "not a row"
    )");

    CHECK(describe(page) == Lines{
                                "placeholder hotkey: Open the menu",
                                "placeholder dropdownFiles: Preset",
                                "placeholder Switcher: A type in another letter case",
                                "placeholder : A row with no type",
                                "placeholder : A type that is not a text",
                                "placeholder : ",
                            });
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
    CHECK(describe(mod->pages[0]) == Lines{ "placeholder hotkey: Actif" });

    // with no translations every text is as it is written
    CHECK(loaded(config).displayName == "$MyMod_Name");
}

TEST_CASE("McmLoader: the mod's own MCM showcase is loaded")
{
    const auto mods = mcm::loadConfigs(MODS_CONFIG_MCM_SHOWCASE_DIR);

    REQUIRE(mods.size() == 1);
    CHECK(mods[0].name == "ModsConfigVR_McmShowcase");
    CHECK(mods[0].displayName == "Mods Config VR - MCM Showcase");
    CHECK_FALSE(mods[0].frontPage.items.empty());
    CHECK(mods[0].pages.size() == 13);

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

TEST_CASE("McmLoader: a config that cannot be read is left out, and the others are loaded")
{
    const test_files::TempFolder folder;
    folder.write("Broken/config.json", R"({ "modName": "Broken", )");
    folder.write("NoName/config.json", R"({ "displayName": "No name" })");
    folder.write("Good/config.json", R"({ "modName": "Good" })");

    CHECK(namesOf(mcm::loadConfigs(folder.path)) == Lines{ "Good" });
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
    // a control the menu does not support has its help too
    CHECK(std::get<model::Placeholder>(rows[3]).help == "One line and the next");
    CHECK(std::get<model::Setting>(rows[4]).help.empty());
    CHECK(std::get<model::Setting>(rows[5]).help.empty());
}
