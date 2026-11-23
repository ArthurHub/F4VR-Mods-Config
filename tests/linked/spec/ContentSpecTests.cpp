#include <catch2/catch_test_macros.hpp>

#include <string>
#include <string_view>

#include "TestConfigSpecs.h"
#include "spec/ConfigSpecLoader.h"
#include "spec/PageContentReader.h"

using namespace mods_config;
using namespace test_config_specs;

namespace
{
    // A config spec with a setting and a section on its front page, and two pages with an id: `parent`, which passes
    // its file, `applies`, `omitDefault` and `visibleWhen` to `wheelItems` under it, which adds an `enabledWhen`
    constexpr auto WHEEL_SPEC = R"({
        "minSchemaVersion": 1, "modName": "MyMod", "applies": "save",
        "files": {
            "main": { "format": "toml", "root": "myGames", "path": "MyMod.toml" },
            "wheel": { "format": "toml", "root": "myGames", "path": "Wheel.toml" }
        },
        "defaultFile": "main",
        "content": [
            { "id": "master", "type": "switcher", "text": "Master", "default": true, "storage": { "path": ["Main", "bMaster"] } },
            { "id": "tools", "type": "section", "text": "Tools", "content": [
                { "id": "run", "type": "button", "text": "Run", "action": { "type": "RunConsoleCommand", "command": "tm" } } ] }
        ],
        "pages": [
            { "pageDisplayName": "Parent", "id": "parent", "defaultFile": "wheel", "applies": "live", "omitDefault": true,
              "visibleWhen": { "setting": "master", "equals": true },
              "pages": [ { "pageDisplayName": "Wheel items", "id": "wheelItems", "enabledWhen": { "clientState": "ready" } } ] },
            { "pageDisplayName": "Plain" }
        ]
    })";

    /**
     * A switcher row of a client's content with an id, kept at `Items`, its id.
     */
    std::string switcherRow(const std::string_view id)
    {
        return std::format(R"({{ "id": "{0}", "type": "switcher", "text": "{0}", "default": false, "storage": {{ "path": ["Items", "{0}"] }} }})", id);
    }

    /**
     * The content a page has to be given.
     */
    spec::PageContent given(model::Mod& mod, const std::string_view pageId, const std::string_view text)
    {
        REQUIRE(mod.pageContent);
        auto content = spec::readPageContent(*mod.pageContent, pageId, text);
        if (!content) {
            FAIL(content.error());
        }
        return std::move(*content);
    }

    Lines problemLines(const spec::PageContent& content)
    {
        Lines lines;
        for (const auto& problem : content.problems) {
            lines.push_back(problemLine(problem));
        }
        return lines;
    }
}

TEST_CASE("PageContentReader: the rows a client gives inherit the page's file, applies, omitDefault and conditions")
{
    auto mod = loaded(WHEEL_SPEC);
    const auto content = given(mod, "WHEELITEMS", std::format("[ {} ]", switcherRow("item")));
    CHECK(content.problems.empty());
    REQUIRE(content.items.size() == 1);
    const auto& setting = std::get<model::Setting>(content.items[0]);
    const auto& page = mod.pages[0].pages[0];
    CHECK(setting.storage == fileValue("wheel", { "Items", "item" }));
    CHECK(setting.applies == model::Applies::Live);
    CHECK(setting.omitDefault);
    CHECK(setting.visibleWhen == page.visibleWhen);
    CHECK(setting.enabledWhen == page.enabledWhen);
    CHECK(page.enabledWhen != page.visibleWhen);
}

TEST_CASE("PageContentReader: a problem of a client's row says where it is, from the client's call")
{
    auto mod = loaded(WHEEL_SPEC);
    const auto content = given(mod, "wheelItems", R"([
        { "type": "text", "text": "Pick up to eight." },
        { "id": "lost", "type": "switcher", "text": "Lost", "default": false, "storage": { "file": "nowhere", "path": ["Items", "bLost"] } }
    ])");
    CHECK(problemLines(content) == Lines{ "E Lost: It names no file of its config spec, or no value of it: it is not available. [Parent \xE2\x80\xBA Wheel items \xE2\x80\xBA "
                                          "Lost] giveContent(\"wheelItems\")[1].storage" });
    REQUIRE(content.problems.size() == 1);
    CHECK(content.problems[0].place == model::RowPlace{ .page = { 0, 0 }, .item = { 1 } });
    CHECK(std::holds_alternative<model::UnknownStorage>(std::get<model::Setting>(content.items[1]).storage));
}

TEST_CASE("PageContentReader: a client's row can name a setting of the config spec, in a condition and in an option's sets")
{
    auto mod = loaded(WHEEL_SPEC);
    const auto content = given(mod, "wheelItems", R"([
        { "id": "preset", "type": "dropdown", "valueType": "string", "text": "Preset", "visibleWhen": { "setting": "MASTER", "equals": false },
          "valueOptions": { "options": [
            { "value": "off", "text": "Off", "sets": { "master": false } },
            { "value": "on", "text": "On", "sets": { "master": true } } ] } },
        { "id": "ghost", "type": "switcher", "text": "Ghost", "default": false, "storage": { "path": ["Items", "bGhost"] },
          "visibleWhen": { "setting": "nothing", "equals": true } }
    ])");
    const auto& preset = std::get<model::Setting>(content.items[0]);
    REQUIRE(preset.valueOptions.options.size() == 2);
    CHECK(setOn(preset.valueOptions.options[1], "master") == model::Value(true));
    REQUIRE(preset.visibleWhen);
    REQUIRE(preset.visibleWhen->kind == model::Condition::Kind::All);
    CHECK(preset.visibleWhen->conditions[1].setting == "master");
    CHECK(problemLines(content).size() == 1);
    CHECK(content.problems[0].row == "Ghost");
}

TEST_CASE("PageContentReader: a client's row with an id of the config spec is an error")
{
    auto mod = loaded(WHEEL_SPEC);
    const auto content = given(mod,
        "wheelItems",
        std::format(R"([ {}, {},
        {{ "id": "MASTER", "type": "section", "text": "Section A", "content": [ {} ] }},
        {{ "id": "run", "type": "section", "text": "Section B", "content": [ {} ] }} ])",
            switcherRow("master"),
            switcherRow("tools"),
            switcherRow("a"),
            switcherRow("b")));
    CHECK(
        problemLines(content) ==
        Lines{ "E master: A setting before it has the id \"master\": a condition names that one. Give one of them an id of its own. [Parent \xE2\x80\xBA Wheel items \xE2\x80\xBA "
               "master] "
               "giveContent(\"wheelItems\")[0]",
            "E tools: A section or a button has the id \"tools\": no condition names this setting. Give it an id of its own. [Parent \xE2\x80\xBA Wheel items \xE2\x80\xBA tools] "
            "giveContent(\"wheelItems\")[1]",
            "E Section A: The setting \"master\" has its id: the setting keeps it, and this tells its mod's client nothing. Give it an id of its own. [Parent \xE2\x80\xBA Wheel "
            "items \xE2\x80\xBA Section A] giveContent(\"wheelItems\")[2]",
            "E Section B: A section or a button before it has the id \"run\": this tells its mod's client nothing. Give it an id of its own. [Parent \xE2\x80\xBA Wheel items "
            "\xE2\x80\xBA Section B] giveContent(\"wheelItems\")[3]" });
}

TEST_CASE("PageContentReader: an id is unique across the pages a client gives content, and a page read again drops its own")
{
    auto mod = loaded(WHEEL_SPEC);
    const auto row = std::format("[ {} ]", switcherRow("item"));
    CHECK(given(mod, "parent", row).problems.empty());
    CHECK(given(mod, "parent", row).problems.empty());
    CHECK(given(mod, "wheelItems", row).problems.size() == 1);

    // a row of one page can name a row of another, until that page is given other rows
    const auto naming = R"([ { "type": "text", "text": "Named", "visibleWhen": { "setting": "item", "equals": true } } ])";
    CHECK(given(mod, "wheelItems", naming).problems.empty());
    CHECK(given(mod, "parent", "[]").items.empty());
    CHECK(given(mod, "wheelItems", naming).problems.size() == 1);
    CHECK(given(mod, "wheelItems", row).problems.empty());
}

TEST_CASE("PageContentReader: a page is not available for text that is not a JSON array, and for no page with the id")
{
    auto mod = loaded(WHEEL_SPEC);
    REQUIRE(mod.pageContent);
    CHECK(spec::readPageContent(*mod.pageContent, "wheelItems", "{}").error() == "its mod's client gave content that is not a JSON array");
    CHECK(spec::readPageContent(*mod.pageContent, "wheelItems", "not json").error() == "its mod's client gave content that is not a JSON array");
    CHECK(spec::readPageContent(*mod.pageContent, "plain", "[]").error() == "its config spec has no page with the id \"plain\"");

    // a page whose text is not an array has no rows, so the ids of its last ones are free
    CHECK(given(mod, "parent", std::format("[ {} ]", switcherRow("item"))).problems.empty());
    CHECK_FALSE(spec::readPageContent(*mod.pageContent, "parent", "{}"));
    CHECK(given(mod, "wheelItems", std::format("[ {} ]", switcherRow("item"))).problems.empty());
}

TEST_CASE("PageContentReader: a config spec that is not available has no reader")
{
    CHECK_FALSE(loaded(R"({ "minSchemaVersion": 99, "modName": "MyMod" })").pageContent);
    CHECK_FALSE(spec::notLoadedConfigSpec("MyMod", "no").pageContent);
}
