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

TEST_CASE("PackageLoader: a switcher, a slider and a text input are settings, and any other row is a placeholder with its type")
{
    const auto mod = loaded(packageOf(R"(
        { "type": "switcher", "text": "Enable", "help": "Turns it on.", "valueType": "bool", "storage": { "path": ["Main", "bEnable"] } },
        { "type": "slider", "text": "Opacity", "valueType": "int", "valueOptions": { "min": 0, "max": 255, "step": 5 }, "storage": { "path": ["Main", "iOpacity"] } },
        { "type": "textinput", "text": "Name", "storage": { "path": ["Main", "sName"] } },
        { "type": "textinput", "text": "Scale", "valueType": "float", "storage": { "path": ["Main", "fScale"] } },
        { "type": "dropdown", "text": "Mode", "help": "Low or high." },
        { "type": "spacer" }
    )"));
    const auto& items = mod.frontPage.items;
    REQUIRE(items.size() == 6);

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

    const auto& mode = std::get<model::Placeholder>(items[4]);
    CHECK(mode.type == "dropdown");
    CHECK(mode.text == "Mode");
    CHECK(mode.help == "Low or high.");
    CHECK(std::get<model::Placeholder>(items[5]).type == "spacer");
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
            { "type": "switcher", "text": "Global", "storage": { "global": "MyMod.esp|1234" } },
            { "type": "switcher", "text": "Property", "storage": { "form": "MyMod.esp|800", "propertyName": "bOn" } }
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
                          "Global unknown global",
                          "Property unknown property" });
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
        REQUIRE(storage.path.size() == 2);
        const auto shippedValue = shipped[storage.path[0]][storage.path[1]];
        REQUIRE(shippedValue);

        // the default is the value of the shipped file, which the mod has without its key
        REQUIRE(setting.defaultValue);
        if (const auto* number = std::get_if<int>(&*setting.defaultValue)) {
            CHECK(shippedValue.value<int>() == *number);
        } else {
            CHECK(common::fEqual(shippedValue.value<float>().value_or(NAN), std::get<float>(*setting.defaultValue)));
        }
    }
    CHECK(ids.size() >= 10);
}
