#include <catch2/catch_test_macros.hpp>

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
                lines.push_back(indent + "text: " + text.text);
            }

            void operator()(const model::Image&) const
            {
                lines.push_back(indent + "image");
            }

            void operator()(const model::Spacer&) const
            {
                lines.push_back(indent + "spacer");
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
                                "spacer",
                            });
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
