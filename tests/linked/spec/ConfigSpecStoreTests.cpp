#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <format>
#include <fstream>
#include <map>
#include <optional>
#include <ranges>
#include <sstream>
#include <string>
#include <variant>
#include <vector>

#include "TestActions.h"
#include "TestClients.h"
#include "TestFiles.h"
#include "TestGlobals.h"
#include "TestProperties.h"
#include "common/CommonUtils.h"
#include "model/Categories.h"
#include "model/Colors.h"
#include "model/Conditions.h"
#include "model/UnsavedPages.h"
#include "spec/ConfigSpecLoader.h"
#include "spec/ConfigSpecStore.h"
#include "spec/TomlFile.h"

using namespace mods_config;
using spec::ConfigSpecStore;

namespace
{
    constexpr auto MOD_FILE = R"(Mods_Config\MyMod\MyMod.toml)";

    // a setting of each control, with its default, kept in the file `main`
    constexpr auto SETTINGS = R"(
        { "type": "switcher", "valueType": "bool", "text": "Enable", "default": true, "storage": { "path": ["Main", "bEnable"] } },
        { "type": "slider", "valueType": "float", "text": "Scale", "default": 1.0, "valueOptions": { "min": 0.5, "max": 1.5, "step": 0.05 },
          "storage": { "path": ["Main", "fScale"] } },
        { "type": "slider", "valueType": "int", "text": "Opacity", "default": 200, "valueOptions": { "min": 0, "max": 255, "step": 1 },
          "storage": { "path": ["Main", "iOpacity"] } },
        { "type": "stepper", "valueType": "int", "text": "Hand", "default": 0, "valueOptions": { "options": ["Left", "Right"] },
          "storage": { "path": ["Main", "iHand"] } },
        { "type": "dropdown", "valueType": "string", "text": "Place", "default": "hip",
          "valueOptions": { "options": [ { "value": "hip", "text": "Hip" }, { "value": "chest", "text": "Chest" } ] },
          "storage": { "path": ["Main", "sPlace"] } },
        { "type": "textinput", "valueType": "string", "text": "Name", "default": "Me", "storage": { "path": ["Main", "sName"] } },
        { "type": "textinput", "valueType": "int", "text": "Count", "default": 3, "valueOptions": { "min": 1, "max": 9 },
          "storage": { "path": ["Main", "iCount"] } },
        { "type": "textinput", "valueType": "float", "text": "Speed", "default": 1.5, "storage": { "path": ["Main", "fSpeed"] } }
    )";

    // the file with a value of each setting, none of them its default
    constexpr auto MOD_TOML =
        "# My Mod\r\n"
        "[Main]\r\n"
        "bEnable = false\r\n"
        "fScale = 1.25\r\n"
        "iOpacity = 128\r\n"
        "iHand = 1\r\n"
        "sPlace = \"chest\"\r\n"
        "sName = \"You\"\r\n"
        "iCount = 5\r\n"
        "fSpeed = 2.5\r\n";

    /**
     * The whole text of a file.
     */
    std::string readFile(const std::filesystem::path& path)
    {
        std::ostringstream text;
        text << std::ifstream(path, std::ios::binary).rdbuf();
        return text.str();
    }

    /**
     * A config spec with the given content as its front page, a TOML file `main` in My Games that is its default,
     * and an INI file `ini` in Data.
     */
    model::Mod configSpecOf(const std::string& content)
    {
        auto mod = spec::loadConfigSpec(std::format(R"({{
            "minSchemaVersion": 1, "modName": "MyMod", "displayName": "My Mod", "applies": "live",
            "files": {{
                "main": {{ "format": "toml", "root": "myGames", "path": "Mods_Config/MyMod/MyMod.toml" }},
                "ini": {{ "format": "ini", "root": "data", "path": "MyMod.ini" }}
            }},
            "defaultFile": "main",
            "content": [ {} ]
        }})",
            content));
        if (!mod) {
            FAIL(mod.error());
        }
        return std::move(*mod);
    }

    /**
     * A config spec's mod and its store, with a Data folder and a My Games folder of their own, which have no files
     * until a test writes one.
     */
    struct Session
    {
        test_files::TempFolder data;
        test_files::TempFolder myGames;
        model::Mod mod;
        ConfigSpecStore store{ data.path, myGames.path };

        // each preview is told at once: the pacing has a test of its own
        explicit Session(const std::string& content = SETTINGS)
            : mod(configSpecOf(content))
        {
            store.setPreviewPacing(std::chrono::milliseconds(0), std::chrono::steady_clock::now);
        }

        /**
         * Writes the mod's TOML file and starts its session.
         */
        void open(const std::string& toml)
        {
            myGames.write(MOD_FILE, toml);
            store.open(mod);
        }

        const model::Setting& setting(const std::size_t row) const
        {
            return std::get<model::Setting>(mod.frontPage.items.at(row));
        }

        model::StoredValue value(const std::size_t row)
        {
            return store.value(setting(row));
        }

        /**
         * A setting of the activation sphere that is the first row, by its place among the sphere's keys, in the order of
         * its rows: its main ones and then those of its sections.
         */
        const model::Setting& sphereKey(const std::size_t place) const
        {
            std::vector<const model::Setting*> keys;
            const auto add = [&keys](const auto& self, const model::Group& group) -> void {
                for (const auto& item : group.items) {
                    if (const auto* key = std::get_if<model::Setting>(&item)) {
                        keys.push_back(key);
                    } else if (const auto* section = std::get_if<model::Group>(&item)) {
                        self(self, *section);
                    }
                }
            };
            add(add, std::get<model::Group>(mod.frontPage.items.at(0)));
            return *keys.at(place);
        }
    };
}

namespace
{
    std::string textOf(const model::Value& value)
    {
        return std::visit(
            []<typename T>(const T& v) {
                return std::format("{}", v);
            },
            value);
    }

    /**
     * A setting as its mod's client is told it: its id, its file and its path in it, its form and a property's
     * script and name, or its state.
     */
    std::string settingText(const model::ClientSetting& setting)
    {
        std::string where = setting.file ? setting.file->name + ":" : !setting.clientState.empty() ? "state " + setting.clientState : setting.form;
        for (const auto& name : setting.path) {
            where += (where.ends_with(':') ? "" : ".") + name;
        }
        if (!setting.property.empty()) {
            where += std::format(" {}.{}", setting.script, setting.property);
        }
        return setting.id + " " + where;
    }

    /**
     * The changes of `live` settings a store tells its mod's client, each as a line: its phase, its setting, and the
     * value it had before and has now. And each Save it tells, as a line of its revision and its files, and a line of
     * each setting, with its value before and the value written.
     */
    struct ToldChanges
    {
        std::vector<std::string> lines;
        std::vector<std::string> saves;
        std::string filePath;

        explicit ToldChanges(ConfigSpecStore& store)
        {
            store.clientCalls().addClient("MyMod",
                { .onChange =
                        [this](const model::SettingChange& change) {
                            lines.push_back(std::format("{} {} {} -> {}",
                                change.phase == model::ChangePhase::Discard ? "Discard" : "Preview",
                                settingText(change.setting),
                                textOf(change.previous),
                                textOf(change.value)));
                            filePath = change.setting.file ? change.setting.file->path : std::string();
                        },
                    .onSaved =
                        [this](const model::SaveBatch& batch) {
                            std::string files;
                            for (const auto& file : batch.files) {
                                files += " " + file.name;
                                filePath = file.path;
                            }
                            saves.push_back(std::format("Save {}:{}", batch.revision, files));
                            for (const auto& saved : batch.settings) {
                                saves.push_back(std::format("  {} {} -> {}", settingText(saved.setting), textOf(saved.before), textOf(saved.value)));
                            }
                        } });
        }
    };
}

TEST_CASE("ConfigSpecStore: a value is the file's, in the type of its control, and is clean")
{
    Session session;
    session.open(MOD_TOML);
    CHECK(session.value(0).value == model::Value(false));
    CHECK(session.value(1).value == model::Value(1.25f));
    // a slider of a whole number has a number
    CHECK(session.value(2).value == model::Value(128.0f));
    CHECK(session.value(3).value == model::Value(1));
    CHECK(session.value(4).value == model::Value("chest"s));
    CHECK(session.value(5).value == model::Value("You"s));
    // a text input of a number has its text
    CHECK(session.value(6).value == model::Value("5"s));
    CHECK(session.value(7).value == model::Value("2.5"s));
    for (std::size_t row = 0; row < 8; ++row) {
        const auto value = session.value(row);
        CHECK(value.available);
        CHECK_FALSE(value.invalid);
        CHECK(value.reason.empty());
        CHECK_FALSE(session.store.isDirty(session.setting(row)));
    }
    CHECK(session.store.dirtyCount() == 0);

    // a condition names a setting by its id
    CHECK(session.store.valueById("fScale:Main").value == model::Value(1.25f));
    CHECK_FALSE(session.store.valueById("missing").available);
}

TEST_CASE("ConfigSpecStore: a value the file does not have is its default, and is clean, as is every value of a file that is not there")
{
    Session session;
    session.open("[Main]\r\nbEnable = false\r\n");
    CHECK(session.value(0).value == model::Value(false));
    CHECK(session.value(1).value == model::Value(1.0f));
    CHECK(session.value(1).available);
    CHECK(session.value(6).value == model::Value("3"s));
    CHECK(session.store.dirtyCount() == 0);

    // setting the default leaves it clean, so a Save does not add it to the file
    CHECK(session.store.set(session.setting(1), 1.0f));
    CHECK_FALSE(session.store.isDirty(session.setting(1)));
    CHECK(session.store.set(session.setting(1), 1.1f));
    CHECK(session.store.isDirty(session.setting(1)));

    Session noFile;
    noFile.store.open(noFile.mod);
    for (std::size_t row = 0; row < 8; ++row) {
        CHECK(noFile.value(row).available);
        CHECK(noFile.value(row).value == *noFile.store.defaultValue(noFile.setting(row)));
    }
    CHECK(noFile.store.dirtyCount() == 0);
}

TEST_CASE("ConfigSpecStore: a set value is pending and dirty, and a value set back to the file's is clean")
{
    Session session;
    session.open(MOD_TOML);
    CHECK(session.store.set(session.setting(0), true));
    CHECK(session.store.set(session.setting(2), 200.4f));
    CHECK(session.store.set(session.setting(4), "hip"s));
    CHECK(session.value(0).value == model::Value(true));
    // a slider of a whole number is at a whole number
    CHECK(session.value(2).value == model::Value(200.0f));
    CHECK(session.store.isDirty(session.setting(0)));
    CHECK(session.store.dirtyCount() == 3);

    // a condition has the pending value
    CHECK(session.store.valueById("bEnable:Main").value == model::Value(true));

    CHECK(session.store.set(session.setting(0), false));
    CHECK_FALSE(session.store.isDirty(session.setting(0)));
    CHECK(session.store.set(session.setting(2), 128.2f));
    CHECK(session.store.dirtyCount() == 1);

    // the text of a number is written as short as it can be
    CHECK(session.store.set(session.setting(7), "02.50"s));
    CHECK_FALSE(session.store.isDirty(session.setting(7)));
    CHECK(session.store.set(session.setting(6), "7"s));
    CHECK(session.value(6).value == model::Value("7"s));
    CHECK(session.store.dirtyCount() == 2);
}

TEST_CASE("ConfigSpecStore: a value the player sets that is not one of its setting is pending and invalid, with the reason")
{
    Session session;
    session.open(MOD_TOML);
    // outside the limits of a slider and of a text input of a number, which is written as short as it can be
    CHECK(session.store.set(session.setting(1), 2.0f));
    CHECK(session.store.set(session.setting(6), "010"s));
    // a text that is no number of its field, as it is typed
    CHECK(session.store.set(session.setting(7), "fast"s));
    // no option of a choice
    CHECK(session.store.set(session.setting(4), "elbow"s));

    CHECK(session.value(1).invalid);
    CHECK(session.value(1).value == model::Value(2.0f));
    CHECK(session.value(1).reason == "Invalid: 2 is not from 0.5 to 1.5.");
    CHECK(session.value(6).value == model::Value("10"s));
    CHECK(session.value(6).reason == "Invalid: 10 is not from 1 to 9.");
    CHECK(session.value(7).value == model::Value("fast"s));
    CHECK(session.value(7).reason == R"(Invalid: "fast" is not a number.)");
    CHECK(session.value(4).reason == R"(Invalid: "elbow" is none of its options.)");
    CHECK(session.store.set(session.setting(6), "4.5"s));
    CHECK(session.value(6).reason == R"(Invalid: "4.5" is not a whole number.)");

    // each is dirty, and counted as invalid
    CHECK(session.store.isDirty(session.setting(1)));
    CHECK(session.store.dirtyCount() == 4);
    CHECK(session.store.invalidCount() == 4);

    // a valid value makes it valid, and the file's value makes it clean
    CHECK(session.store.set(session.setting(1), 1.0f));
    CHECK_FALSE(session.value(1).invalid);
    CHECK(session.value(1).reason.empty());
    CHECK(session.store.set(session.setting(6), "5"s));
    CHECK_FALSE(session.store.isDirty(session.setting(6)));
    CHECK_FALSE(session.value(6).invalid);
    CHECK(session.store.invalidCount() == 2);

    // a value of another type than its control shows is not set
    CHECK_FALSE(session.store.set(session.setting(0), 1));
    CHECK_FALSE(session.store.set(session.setting(3), "Left"s));
    CHECK_FALSE(session.store.set(session.setting(1), "1.0"s));
    CHECK(session.store.dirtyCount() == 3);

    // Discard takes them back
    session.store.discard();
    CHECK(session.store.invalidCount() == 0);
    CHECK_FALSE(session.value(7).invalid);
}

TEST_CASE("ConfigSpecStore: a text input of a text has the length its config spec allows")
{
    Session session(R"(
        { "type": "textinput", "valueType": "string", "text": "Name", "default": "Me",
          "valueOptions": { "allowEmpty": false, "maxLength": 4 }, "storage": { "path": ["Main", "sName"] } },
        { "type": "textinput", "valueType": "string", "text": "Code", "default": "abc",
          "valueOptions": { "minLength": 3 }, "storage": { "path": ["Main", "sCode"] } }
    )");
    session.open("[Main]\r\nsName = \"Preston\"\r\nsCode = \"abc\"\r\n");

    // a value of the file that is too long is invalid and clean, and does not block Save
    CHECK(session.value(0).invalid);
    CHECK(session.value(0).value == model::Value("Preston"s));
    CHECK(session.value(0).reason == R"(Invalid: Main.sName is "Preston" in MyMod.toml, which has 7 characters, and can have at most 4.)");
    CHECK(session.store.invalidCount() == 0);

    // an empty text where a text is needed, and one that is too short; a character of UTF-8 is one
    CHECK(session.store.set(session.setting(0), ""s));
    CHECK(session.value(0).reason == "Invalid: the text has 0 characters, and needs at least 1.");
    CHECK(session.store.set(session.setting(1), "\xC3\xA9t\xC3\xA9"s));
    CHECK_FALSE(session.value(1).invalid);
    CHECK(session.store.set(session.setting(1), "\xC3\xA9t"s));
    CHECK(session.value(1).reason == "Invalid: the text has 2 characters, and needs at least 3.");
    CHECK(session.store.invalidCount() == 2);

    CHECK(session.store.set(session.setting(0), "Nick"s));
    CHECK_FALSE(session.value(0).invalid);
    CHECK(session.store.invalidCount() == 1);
}

TEST_CASE("ConfigSpecStore: Save writes nothing while a value the player set is invalid")
{
    Session session;
    session.open(MOD_TOML);
    CHECK(session.store.set(session.setting(0), true));
    CHECK(session.store.set(session.setting(7), "fast"s));
    const auto saved = session.store.save();
    REQUIRE_FALSE(saved);
    CHECK(saved.error() == "1 value is not valid");
    CHECK(readFile(session.myGames.path / MOD_FILE) == MOD_TOML);
    CHECK(session.store.dirtyCount() == 2);
    // it is no failure of the Save, which the footer would tell
    CHECK(session.store.saveFailure().empty());

    CHECK(session.store.set(session.setting(7), "3"s));
    CHECK(session.store.save());
    CHECK(session.store.dirtyCount() == 0);
}

TEST_CASE("ConfigSpecStore: a value in the file that is not one of its setting is invalid, with the reason, and is clean")
{
    Session session;
    session.open(
        "[Main]\r\n"
        "bEnable = \"yes\"\r\n"
        "fScale = 2.0\r\n"
        "iHand = 5\r\n"
        "sPlace = \"elbow\"\r\n"
        "iCount = 12\r\n");

    // one of another type is the default
    const auto text = session.value(0);
    CHECK(text.available);
    CHECK(text.invalid);
    CHECK(text.value == model::Value(true));
    CHECK(text.reason == R"(Invalid: Main.bEnable is "yes" in MyMod.toml, which is not true or false.)");

    // one outside its limits is shown as it is
    const auto past = session.value(1);
    CHECK(past.invalid);
    CHECK(past.value == model::Value(2.0f));
    CHECK(past.reason == "Invalid: Main.fScale is 2 in MyMod.toml, which is not from 0.5 to 1.5.");
    CHECK(session.value(6).invalid);
    CHECK(session.value(6).value == model::Value("12"s));

    // and one that is no option of its choice
    CHECK(session.value(3).invalid);
    CHECK(session.value(3).value == model::Value(5));
    CHECK(session.value(4).reason == R"(Invalid: Main.sPlace is "elbow" in MyMod.toml, which is none of its options.)");

    CHECK_FALSE(session.value(2).invalid);
    CHECK(session.store.dirtyCount() == 0);

    // a set value replaces it, also the default of one of another type
    CHECK(session.store.set(session.setting(0), true));
    CHECK(session.store.isDirty(session.setting(0)));
    CHECK_FALSE(session.value(0).invalid);
    CHECK(session.value(0).reason.empty());
    CHECK(session.store.set(session.setting(1), 1.0f));
    CHECK_FALSE(session.value(1).invalid);
    CHECK(session.store.dirtyCount() == 2);
}

TEST_CASE("ConfigSpecStore: a setting of a file that cannot be read, or of no file is not available, with the reason")
{
    // a game with no globals
    const test_globals::TestGlobals globals;
    Session session(R"(
        { "type": "switcher", "valueType": "bool", "text": "Enable", "default": true, "storage": { "path": ["Main", "bEnable"] } },
        { "type": "switcher", "valueType": "bool", "text": "Global", "default": true, "storage": { "global": "MyMod.esp|800" } },
        { "type": "switcher", "valueType": "int", "text": "Of a number", "default": 1, "storage": { "path": ["Main", "iOn"] } }
    )");
    session.open("[Main\r\nbEnable = true\r\n");
    const auto broken = session.value(0);
    CHECK_FALSE(broken.available);
    CHECK(broken.reason.starts_with("Not available: "));
    CHECK(broken.reason.find("MyMod.toml") != std::string::npos);
    CHECK(session.value(1).reason == "Not available: the global MyMod.esp|800 is not in the game.");
    CHECK(session.value(2).reason == "Not available: its control does not show a value of its valueType.");
    for (std::size_t row = 0; row < 3; ++row) {
        CHECK_FALSE(session.value(row).available);
        CHECK_FALSE(session.store.set(session.setting(row), false));
    }
    CHECK(session.store.dirtyCount() == 0);
}

TEST_CASE("ConfigSpecStore: a session that ends forgets its pending values, and the next reads the file again")
{
    Session session;
    session.open(MOD_TOML);
    CHECK(session.store.set(session.setting(0), true));
    session.store.close();
    CHECK(session.store.dirtyCount() == 0);
    CHECK_FALSE(session.value(0).available);

    session.open("[Main]\r\nbEnable = true\r\n");
    CHECK(session.value(0).value == model::Value(true));
    CHECK_FALSE(session.store.isDirty(session.setting(0)));

    // another page of the mod reads nothing again: the baseline is the one of the session's start
    CHECK(session.store.set(session.setting(1), 0.5f));
    session.myGames.write(MOD_FILE, "[Main]\r\nbEnable = false\r\n");
    session.store.pageShown();
    CHECK(session.value(0).value == model::Value(true));
    CHECK(session.store.isDirty(session.setting(1)));
}

TEST_CASE("ConfigSpecStore: Save writes the dirty settings in the types of the file, with its layout kept, and they are clean with the value written")
{
    Session session;
    session.open(MOD_TOML);
    CHECK(session.store.set(session.setting(0), true));
    CHECK(session.store.set(session.setting(1), 0.75f));
    CHECK(session.store.set(session.setting(2), 200.0f));
    CHECK(session.store.set(session.setting(4), "hip"s));
    CHECK(session.store.set(session.setting(6), "7"s));
    CHECK(session.store.set(session.setting(7), "3"s));
    CHECK(session.store.save());

    // a slider of a whole number is a whole number, and the text of a number its number
    CHECK(readFile(session.myGames.path / MOD_FILE) ==
          "# My Mod\r\n"
          "[Main]\r\n"
          "bEnable = true\r\n"
          "fScale = 0.75\r\n"
          "iOpacity = 200\r\n"
          "iHand = 1\r\n"
          "sPlace = \"hip\"\r\n"
          "sName = \"You\"\r\n"
          "iCount = 7\r\n"
          "fSpeed = 3.0\r\n");
    CHECK(session.store.dirtyCount() == 0);
    CHECK(session.value(1).value == model::Value(0.75f));
    CHECK(session.value(6).value == model::Value("7"s));

    // the value written is the baseline: a change back to the file's value is clean
    CHECK(session.store.set(session.setting(0), false));
    CHECK(session.store.isDirty(session.setting(0)));
    CHECK(session.store.set(session.setting(0), true));
    CHECK(session.store.dirtyCount() == 0);
}

TEST_CASE("ConfigSpecStore: a binding is its table's text as the framework writes it, invalid with the framework's reason, and Save writes it as the table")
{
    Session session(R"(
        { "type": "cf.inputBinding", "text": "Grab", "default": { "hand": "offhand", "type": "tap", "button": "trigger" },
          "storage": { "path": ["Sphere", "kPrimary"] } },
        { "type": "cf.inputBinding", "text": "Typo", "default": { "hand": "offhand", "type": "tap", "button": "trigger" },
          "storage": { "path": ["Sphere", "kTypo"] } },
        { "type": "cf.inputBinding", "text": "Missing", "default": { "type": "none" }, "storage": { "path": ["Sphere", "kMissing"] } }
    )");
    session.open(
        "[Sphere]\r\n"
        "kPrimary = { button = \"Trigger\", hand = \"off\", type = \"tap\" }   # grab\r\n"
        "kTypo = { hand = \"offhand\", type = \"tap\", button = \"thumbstik\" }\r\n");

    // the file's words and order are the framework's, so the default is the same value
    const auto grab = session.value(0);
    CHECK(grab.value == model::Value(R"({ hand = "offhand", type = "tap", button = "trigger" })"s));
    CHECK_FALSE(grab.invalid);
    CHECK(session.store.defaultValue(session.setting(0)) == grab.value);

    // a table the framework cannot read is shown as the file has it
    const auto typo = session.value(1);
    CHECK(typo.value == model::Value(R"({ hand = "offhand", type = "tap", button = "thumbstik" })"s));
    CHECK(typo.invalid);
    CHECK(typo.reason == "Invalid: 'thumbstik' is not a button, one of trigger, grip, a, b, thumbstick, menu, system.");
    CHECK(session.value(2).value == model::Value(R"({ type = "none" })"s));

    // a value the player sets is the framework's text too, and one that is no binding is invalid
    CHECK(session.store.set(session.setting(0), R"({ type = "longpress", hand = "primary", button = "a" })"s));
    CHECK(session.value(0).value == model::Value(R"({ hand = "primary", type = "longpress", button = "a" })"s));
    CHECK(session.store.set(session.setting(2), R"({ hand = "offhand" })"s));
    CHECK(session.value(2).invalid);
    CHECK(session.value(2).reason == "Invalid: The type is missing, one of tap, press, longpress, double, hold, release, touch, axis, none.");
    CHECK_FALSE(session.store.set(session.setting(0), 1.0f));
    session.store.discard(session.setting(2));

    CHECK(session.store.set(session.setting(1), R"({ type = "none" })"s));
    REQUIRE(session.store.save());
    CHECK(readFile(session.myGames.path / MOD_FILE) ==
          "[Sphere]\r\n"
          "kPrimary = { hand = \"primary\", type = \"longpress\", button = \"a\" }   # grab\r\n"
          "kTypo = { type = \"none\" }\r\n");
    CHECK(session.store.dirtyCount() == 0);
}

TEST_CASE("ConfigSpecStore: a color is its table's text with the keys it has, invalid with the framework's reason, and Save writes it as the table")
{
    Session session(R"(
        { "type": "cf.color", "text": "Ray", "default": { "rgb": [190, 255, 190], "alpha": 200 }, "storage": { "path": ["Pointer", "cRay"] } },
        { "type": "cf.color", "text": "Mark", "default": { "rgb": [215, 255, 215], "alpha": 180 }, "storage": { "path": ["Pointer", "cMark"] } },
        { "type": "cf.color", "text": "Out", "default": { "rgb": [215, 255, 215], "alpha": 100 }, "storage": { "path": ["Pointer", "cOut"] } }
    )");
    session.open(
        "[Pointer]\r\n"
        "cRay = { alpha = 200, rgb = [190,255,190] }   # ray\r\n"
        "cMark = { alpha = 60 }\r\n"
        "cOut = { rgb = [300, 0, 0] }\r\n");

    // the file's keys and spaces are the menu's, so the default is the same value, and one key alone stays alone
    CHECK(session.value(0).value == model::Value("{ rgb = [190, 255, 190], alpha = 200 }"s));
    CHECK(session.store.defaultValue(session.setting(0)) == session.value(0).value);
    CHECK(session.value(1).value == model::Value("{ alpha = 60 }"s));
    CHECK_FALSE(session.value(1).invalid);
    // its channels are the framework's: the keys it has over its default's
    CHECK(model::colorOf(session.setting(1), session.value(1).value) == render::Color::rgba(215, 255, 215, 60));

    // a table the framework cannot read is shown as the file has it
    const auto out = session.value(2);
    CHECK(out.value == model::Value("{ rgb = [300, 0, 0] }"s));
    CHECK(out.invalid);
    CHECK(out.reason == "Invalid: rgb has a number that is not a whole number in 0..255.");
    CHECK(session.store.dirtyCount() == 0);

    // a value the player sets is the menu's text too, and one that is no color is invalid
    CHECK(session.store.set(session.setting(1), "{alpha=60,rgb=[1,2,3]}"s));
    CHECK(session.value(1).value == model::Value("{ rgb = [1, 2, 3], alpha = 60 }"s));
    CHECK(session.store.set(session.setting(0), "{ colour = [1, 2, 3] }"s));
    CHECK(session.value(0).invalid);
    CHECK(session.value(0).reason == "Invalid: it has the key 'colour', which is not rgb or alpha.");
    session.store.discard(session.setting(0));
    CHECK_FALSE(session.store.set(session.setting(0), 1.0f));

    CHECK(session.store.set(session.setting(2), "{ rgb = [255, 0, 0] }"s));
    REQUIRE(session.store.save());
    CHECK(readFile(session.myGames.path / MOD_FILE) ==
          "[Pointer]\r\n"
          "cRay = { alpha = 200, rgb = [190,255,190] }   # ray\r\n"
          "cMark = { rgb = [1, 2, 3], alpha = 60 }\r\n"
          "cOut = { rgb = [255, 0, 0] }\r\n");
    CHECK(session.store.dirtyCount() == 0);
}

TEST_CASE("ConfigSpecStore: a transform is its table's text with the keys it has, invalid with the framework's reason, and Save writes it as the table")
{
    Session session(R"(
        { "type": "cf.transform", "text": "Mesh", "default": { "pos": [1, 2, 3], "scale": 1.5 }, "storage": { "path": ["Hand", "tMesh"] } },
        { "type": "cf.transform", "text": "Tilt", "default": { "rot": [0, 0, 0] }, "storage": { "path": ["Hand", "tTilt"] } },
        { "type": "cf.transform", "text": "Out", "default": {}, "storage": { "path": ["Hand", "tOut"] } }
    )");
    session.open(
        "[Hand]\r\n"
        "tMesh = { scale = 1.5, pos = [1,2,3] }   # mesh\r\n"
        "tTilt = { rot = [0.0, -5.0, 0.0] }\r\n"
        "tOut = { pos = [0, 0], scale = 1 }\r\n");

    // the file's keys, numbers and spaces are the menu's, so the default is the same value, and one key alone stays alone
    CHECK(session.value(0).value == model::Value("{ pos = [1.0, 2.0, 3.0], scale = 1.5 }"s));
    CHECK(session.store.defaultValue(session.setting(0)) == session.value(0).value);
    CHECK(session.value(1).value == model::Value("{ rot = [0.0, -5.0, 0.0] }"s));
    CHECK_FALSE(session.value(1).invalid);

    // a table the framework cannot read is shown as the file has it
    const auto out = session.value(2);
    CHECK(out.value == model::Value("{ pos = [0, 0], scale = 1 }"s));
    CHECK(out.invalid);
    CHECK(out.reason == "Invalid: pos is not an array of three numbers.");
    CHECK(session.store.dirtyCount() == 0);

    // a value the player sets is the menu's text too, and one that is no transform is invalid
    CHECK(session.store.set(session.setting(1), "{rot=[0,1,0],pos=[1,2,3]}"s));
    CHECK(session.value(1).value == model::Value("{ pos = [1.0, 2.0, 3.0], rot = [0.0, 1.0, 0.0] }"s));
    CHECK(session.store.set(session.setting(0), "{ position = [1, 2, 3] }"s));
    CHECK(session.value(0).invalid);
    CHECK(session.value(0).reason == "Invalid: it has the key 'position', which is not pos, rot or scale.");
    session.store.discard(session.setting(0));
    CHECK_FALSE(session.store.set(session.setting(0), 1.0f));

    CHECK(session.store.set(session.setting(2), "{ pos = [0.5, 0.0, 0.0] }"s));
    REQUIRE(session.store.save());
    CHECK(readFile(session.myGames.path / MOD_FILE) ==
          "[Hand]\r\n"
          "tMesh = { scale = 1.5, pos = [1,2,3] }   # mesh\r\n"
          "tTilt = { pos = [1.0, 2.0, 3.0], rot = [0.0, 1.0, 0.0] }\r\n"
          "tOut = { pos = [0.5, 0.0, 0.0] }\r\n");
    CHECK(session.store.dirtyCount() == 0);
}

TEST_CASE("ConfigSpecStore: a sphere's key the file leaves out is its default, and each key is saved alone")
{
    Session session(R"({ "type": "cf.activationSphere", "text": "Body", "storage": { "path": ["Body"] } })");
    session.open(
        "[Body]\r\n"
        "tZone = { pos = [1.0, 2.0, 3.0], scale = 20.0 }\r\n"
        "sEntryHaptic = \"Click\"  # entry\r\n");
    constexpr std::size_t ICON_SIZE = 10;
    CHECK(session.store.value(session.sphereKey(1)).value == model::Value("Click"s));
    CHECK(session.store.value(session.sphereKey(ICON_SIZE)).value == model::Value(1.0f));
    CHECK(session.store.dirtyCount() == 0);

    CHECK(session.store.set(session.sphereKey(ICON_SIZE), 2.5f));
    REQUIRE(session.store.save());
    CHECK(readFile(session.myGames.path / MOD_FILE) ==
          "[Body]\r\n"
          "tZone = { pos = [1.0, 2.0, 3.0], scale = 20.0 }\r\n"
          "sEntryHaptic = \"Click\"  # entry\r\n"
          "fIconSize = 2.5\r\n");
}

TEST_CASE("ConfigSpecStore: a sphere's binding set to None hides its haptic, and an icon and a sphere that are never shown disable their rows")
{
    Session session(R"({ "type": "cf.activationSphere", "text": "Body", "storage": { "path": ["Body"] } })");
    session.open(
        "[Body]\r\n"
        "kSecondaryBinding = { hand = \"primary\", type = \"tap\", button = \"grip\" }\r\n"
        "sShowIcon = \"always\"\r\n");
    constexpr std::size_t PRIMARY = 0;
    constexpr std::size_t SHOW_ICON = 7;
    constexpr std::size_t PRIMARY_HAPTIC = 4;
    constexpr std::size_t SECONDARY = 5;
    constexpr std::size_t SECONDARY_HAPTIC = 6;
    constexpr std::size_t ICON = 8;
    constexpr std::size_t SPHERE_STYLE = 16;
    const auto shown = [&session](const std::size_t place) {
        return model::holds(*session.sphereKey(place).visibleWhen, &session.store);
    };
    const auto enabled = [&session](const std::size_t place) {
        return model::holds(*session.sphereKey(place).enabledWhen, &session.store);
    };

    CHECK(shown(PRIMARY_HAPTIC));
    CHECK(shown(SECONDARY_HAPTIC));
    CHECK(session.store.set(session.sphereKey(SECONDARY), R"({ type = "none" })"s));
    CHECK_FALSE(shown(SECONDARY_HAPTIC));
    CHECK(session.store.set(session.sphereKey(PRIMARY), R"({ type = "off" })"s));
    CHECK_FALSE(shown(PRIMARY_HAPTIC));

    CHECK(enabled(ICON));
    CHECK(session.store.set(session.sphereKey(SHOW_ICON), "never"s));
    CHECK_FALSE(enabled(ICON));

    // as the framework's default, as the file has no sShowSphere
    CHECK_FALSE(enabled(SPHERE_STYLE));
}

TEST_CASE("ConfigSpecStore: a sphere's icon, mesh and texture are choices of the framework's files in the mod's folders, written by their paths in Data")
{
    Session session(R"({ "type": "cf.activationSphere", "text": "Body", "storage": { "path": ["Body"] }, "keys": { "sSphereNif": true, "sSphereTexture": true } })");
    for (const auto* file : { "activation-icon-circle.dds", "activation-icon-hand.dds", "activation-icon-ring.dds", "activation-sphere.dds", "debug-sphere.dds" }) {
        session.data.write(std::format(R"(Textures\MyMod\f4cf\{})", file), "");
    }
    session.data.write(R"(Meshes\MyMod\f4cf\activation-sphere.nif)", "");
    session.open("[Body]\r\nsIcon = 'Textures\\MyMod\\f4cf\\activation-icon-ring.dds'\r\n");
    constexpr std::size_t ICON = 8;
    constexpr std::size_t MESH = 17;
    constexpr std::size_t TEXTURE = 18;
    REQUIRE(session.sphereKey(MESH).text == "Sphere mesh");
    REQUIRE(session.sphereKey(TEXTURE).text == "Sphere texture");

    // the icons alone, and the file's icon is its option
    const auto& icons = session.store.options(session.sphereKey(ICON));
    REQUIRE(icons.size() == 3);
    CHECK(icons[0] == model::Option{ .value = R"(Textures\MyMod\f4cf\activation-icon-circle.dds)"s, .text = "activation-icon-circle.dds" });
    CHECK(session.store.value(session.sphereKey(ICON)).value == icons[2].value);
    CHECK_FALSE(session.store.value(session.sphereKey(ICON)).invalid);

    // the style's own first, which the file leaves out, and the textures of a sphere alone
    CHECK(session.store.options(session.sphereKey(MESH)) ==
          std::vector<model::Option>{ { .value = ""s, .text = "Style's mesh" }, { .value = R"(Meshes\MyMod\f4cf\activation-sphere.nif)"s, .text = "activation-sphere.nif" } });
    const auto& textures = session.store.options(session.sphereKey(TEXTURE));
    REQUIRE(textures.size() == 3);
    CHECK(textures[0].text == "Style's texture");
    CHECK(textures[2].value == model::Value(R"(Textures\MyMod\f4cf\debug-sphere.dds)"s));
    CHECK(session.store.value(session.sphereKey(TEXTURE)).value == model::Value(""s));

    CHECK(session.store.set(session.sphereKey(TEXTURE), textures[2].value));
    REQUIRE(session.store.save());
    CHECK(readFile(session.myGames.path / MOD_FILE) ==
          "[Body]\r\n"
          "sIcon = 'Textures\\MyMod\\f4cf\\activation-icon-ring.dds'\r\n"
          "sSphereTexture = \"Textures\\\\MyMod\\\\f4cf\\\\debug-sphere.dds\"\r\n");
}

TEST_CASE("ConfigSpecStore: a sphere's Icons sets when its icon and its binding icon show, and moves the binding icon beside the icon for both")
{
    Session session(R"({ "type": "cf.activationSphere", "text": "Body", "storage": { "path": ["Body"] } })");
    session.open(
        "[Body]\r\n"
        "sShowIcon = \"Always\"\r\n"
        "tBindingIcon = { scale = 0.8 }\r\n");
    constexpr std::size_t ICONS = 2;
    constexpr std::size_t SHOW_ICON = 7;
    constexpr std::size_t SHOW_BINDING_ICON = 11;
    constexpr std::size_t BINDING_ICON = 13;
    const auto value = [&session](const std::size_t place) {
        return session.store.value(session.sphereKey(place)).value;
    };

    // an icon shown always is none of its options, and its default is the framework's icon alone
    CHECK(value(ICONS) == model::Value(""s));
    CHECK(session.store.defaultValue(session.sphereKey(ICONS)) == model::Value("icon"s));

    CHECK(session.store.set(session.sphereKey(ICONS), "both"s));
    CHECK(value(SHOW_ICON) == model::Value("wheninside"s));
    CHECK(value(SHOW_BINDING_ICON) == model::Value("wheninside"s));
    CHECK(value(BINDING_ICON) == model::Value("{ pos = [0.65, -0.65, 0.0], scale = 1.0 }"s));
    CHECK(value(ICONS) == model::Value("both"s));

    // the binding icon moved and sized on its own row keeps the option
    CHECK(session.store.set(session.sphereKey(BINDING_ICON), "{ pos = [0.9, -0.7, 0.0], scale = 1.3 }"s));
    CHECK(value(ICONS) == model::Value("both"s));

    // the binding alone is in the icon's place
    CHECK(session.store.set(session.sphereKey(ICONS), "binding"s));
    CHECK(value(SHOW_ICON) == model::Value("never"s));
    CHECK(value(BINDING_ICON) == model::Value("{ pos = [0.0, 0.0, 0.0], scale = 1.25 }"s));
    CHECK(value(ICONS) == model::Value("binding"s));
}

TEST_CASE("ConfigSpecStore: a sphere's Icons has the mod's defaults as an option, which is its default")
{
    Session session(R"({ "type": "cf.activationSphere", "text": "Body", "storage": { "path": ["Body"] }, "keys": {
        "sShowBindingIcon": { "default": "wheninside" }, "tBindingIcon": { "default": { "pos": [0.8, -0.7, 0.0], "scale": 1.4 } } } })");
    session.open(
        "[Body]\r\n"
        "sShowBindingIcon = \"WhenInside\"\r\n"
        "tBindingIcon = { pos = [0.8, -0.7, 0.0], scale = 1.4 }\r\n");
    constexpr std::size_t ICONS = 2;
    constexpr std::size_t BINDING_ICON = 13;

    CHECK(session.store.value(session.sphereKey(ICONS)).value == model::Value("both"s));
    CHECK(session.store.defaultValue(session.sphereKey(ICONS)) == model::Value("both"s));

    // back from the binding alone, the binding icon is at the mod's place again
    CHECK(session.store.set(session.sphereKey(ICONS), "binding"s));
    CHECK(session.store.set(session.sphereKey(ICONS), "both"s));
    CHECK(session.store.value(session.sphereKey(BINDING_ICON)).value == model::Value("{ pos = [0.8, -0.7, 0.0], scale = 1.4 }"s));
}

TEST_CASE("ConfigSpecStore: a sphere's dropdown reads a word as the framework does, with no regard to case and separators, and with its other words")
{
    Session session(R"({ "type": "cf.activationSphere", "text": "Body", "storage": { "path": ["Body"] }, "keys": { "sSphereOrientation": true } })");
    session.open(
        "[Body]\r\n"
        "sShowIcon = \"Off\"\r\n"
        "sShowSphere = \"inside\"\r\n"
        "sSphereStyle = \"Cyan_Subtle\"\r\n"
        "sSphereOrientation = \"head\"\r\n");
    constexpr std::size_t SHOW_ICON = 7;
    constexpr std::size_t ICON = 8;
    constexpr std::size_t SHOW_SPHERE = 15;
    constexpr std::size_t SPHERE_STYLE = 16;
    constexpr std::size_t SPHERE_ORIENTATION = 17;
    const auto word = [&session](const std::size_t place) {
        const auto stored = session.store.value(session.sphereKey(place));
        CHECK_FALSE(stored.invalid);
        return stored.value;
    };
    CHECK(word(SHOW_ICON) == model::Value("never"s));
    CHECK(word(SHOW_SPHERE) == model::Value("wheninside"s));
    CHECK(word(SPHERE_STYLE) == model::Value("cyan-subtle"s));
    CHECK(word(SPHERE_ORIENTATION) == model::Value("hmd"s));
    CHECK(session.store.dirtyCount() == 0);

    // a condition has the word as the option has it
    CHECK_FALSE(model::holds(*session.sphereKey(ICON).enabledWhen, &session.store));

    // the option of the file's word is no change
    CHECK(session.store.set(session.sphereKey(SHOW_ICON), "never"s));
    CHECK(session.store.dirtyCount() == 0);

    // a word the framework does not read is as the file has it, and is invalid
    Session typo(R"({ "type": "cf.activationSphere", "text": "Body", "storage": { "path": ["Body"] } })");
    typo.open(
        "[Body]\r\n"
        "sShowIcon = \"sometimes\"\r\n"
        "sSphereStyle = \"cyan-loud\"\r\n");
    for (const auto& [place, text] : { std::pair{ SHOW_ICON, "sometimes"s }, std::pair{ SPHERE_STYLE, "cyan-loud"s } }) {
        const auto stored = typo.store.value(typo.sphereKey(place));
        CHECK(stored.value == model::Value(text));
        CHECK(stored.invalid);
    }

    // which bindings have an icon, with the framework's other words for them
    constexpr std::size_t BINDING_ICONS = 12;
    for (const auto& [synonym, option] : { std::pair{ "Both", "all"s }, std::pair{ "hand", "inside"s }, std::pair{ "When_Inside", "inside"s } }) {
        Session icons(R"({ "type": "cf.activationSphere", "text": "Body", "storage": { "path": ["Body"] } })");
        icons.open(std::format("[Body]\r\nsBindingIcons = \"{}\"\r\n", synonym));
        const auto stored = icons.store.value(icons.sphereKey(BINDING_ICONS));
        CHECK(stored.value == model::Value(option));
        CHECK_FALSE(stored.invalid);
    }
}

TEST_CASE("ConfigSpecStore: a haptic is its pattern's name as the framework writes it, and a name it does not have is invalid")
{
    Session session(R"(
        { "type": "cf.haptic", "text": "Entry", "default": "Tick", "storage": { "path": ["Sphere", "sEntry"] } },
        { "type": "cf.haptic", "text": "Off", "default": "Tick", "storage": { "path": ["Sphere", "sOff"] } },
        { "type": "cf.haptic", "text": "Typo", "default": "Tick", "storage": { "path": ["Sphere", "sTypo"] } },
        { "type": "cf.haptic", "text": "Missing", "default": "none", "storage": { "path": ["Sphere", "sMissing"] } }
    )");
    session.open(
        "[Sphere]\r\n"
        "sEntry = \"double click\"   # entry\r\n"
        "sOff = \"off\"\r\n"
        "sTypo = \"Tik\"\r\n");

    // the file's text is matched as the framework reads it, and is clean
    CHECK(session.value(0).value == model::Value("DoubleClick"s));
    CHECK_FALSE(session.value(0).invalid);
    CHECK(session.value(1).value == model::Value("none"s));
    CHECK(session.store.dirtyCount() == 0);

    // a name the framework does not have is shown as the file has it
    const auto typo = session.value(2);
    CHECK(typo.value == model::Value("Tik"s));
    CHECK(typo.invalid);
    CHECK(typo.reason == "Invalid: Tik is not a haptic pattern, so none plays.");
    CHECK(session.value(3).value == model::Value("none"s));
    CHECK(session.store.options(session.setting(0)).size() == 19);

    // a value the player sets is the library's name too, and one that is none is invalid
    CHECK(session.store.set(session.setting(3), "ramp-up"s));
    CHECK(session.value(3).value == model::Value("RampUp"s));
    CHECK(session.store.set(session.setting(1), "Bzz"s));
    CHECK(session.value(1).invalid);
    session.store.discard(session.setting(1));
    CHECK_FALSE(session.store.set(session.setting(0), 1.0f));

    CHECK(session.store.set(session.setting(2), "Success"s));
    REQUIRE(session.store.save());
    CHECK(readFile(session.myGames.path / MOD_FILE) ==
          "[Sphere]\r\n"
          "sEntry = \"double click\"   # entry\r\n"
          "sOff = \"off\"\r\n"
          "sTypo = \"Success\"\r\n"
          "sMissing = \"RampUp\"\r\n");
    CHECK(session.store.dirtyCount() == 0);
}

TEST_CASE("ConfigSpecStore: a binding of a hand, a type or a button its setting does not take is invalid, with what it takes")
{
    Session session(R"(
        { "type": "cf.inputBinding", "text": "Back", "default": { "hand": "any", "type": "tap", "button": "grip" },
          "valueOptions": { "hands": ["any"], "types": ["tap", "axis"], "buttons": ["grip", "thumbstick"] }, "storage": { "path": ["Menu", "kBack"] } }
    )");
    session.open("[Menu]\r\nkBack = { hand = \"primary\", type = \"tap\", button = \"grip\" }\r\n");
    CHECK(session.value(0).invalid);
    CHECK(session.value(0).reason == "Invalid: 'primary' is not a hand this binding takes, one of any.");

    CHECK(session.store.set(session.setting(0), R"({ hand = "any", type = "press", button = "grip" })"s));
    CHECK(session.value(0).reason == "Invalid: 'press' is not a way this binding takes, one of tap, axis.");
    CHECK(session.store.set(session.setting(0), R"({ hand = "any", type = "tap", button = "menu" })"s));
    CHECK(session.value(0).reason == "Invalid: 'menu' is not a button this binding takes, one of grip, thumbstick.");
    CHECK_FALSE(session.store.save());

    // a push is of its axis's button, and a binding that is off is taken
    CHECK(session.store.set(session.setting(0), R"({ hand = "any", type = "axis", axis = "trigger", direction = "up" })"s));
    CHECK(session.value(0).reason == "Invalid: 'trigger' is not a button this binding takes, one of grip, thumbstick.");
    CHECK(session.store.set(session.setting(0), R"({ hand = "any", type = "axis", direction = "up" })"s));
    CHECK_FALSE(session.value(0).invalid);
    CHECK(session.store.set(session.setting(0), R"({ type = "none" })"s));
    CHECK_FALSE(session.value(0).invalid);
    CHECK(session.store.save());
}

TEST_CASE("ConfigSpecStore: Save keeps a value that changed in the file since the session started, but for one the player changed, and adds a key the file does not have")
{
    Session session;
    session.open("[Main]\r\nbEnable = false\r\nfScale = 1.25\r\n");
    CHECK(session.store.set(session.setting(0), true));
    CHECK(session.store.set(session.setting(5), "Them"s));
    session.myGames.write(MOD_FILE, "[Main]\r\nbEnable = false\r\nfScale = 1.0\r\niHand = 1\r\n");
    CHECK(session.store.save());
    CHECK(readFile(session.myGames.path / MOD_FILE) == "[Main]\r\nbEnable = true\r\nfScale = 1.0\r\niHand = 1\r\nsName = \"Them\"\r\n");

    // an invalid value of the file that is written is valid
    Session invalid;
    invalid.open("[Main]\r\nfScale = 2.0\r\n");
    CHECK(invalid.value(1).invalid);
    CHECK(invalid.store.set(invalid.setting(1), 1.5f));
    CHECK(invalid.store.save());
    CHECK_FALSE(invalid.value(1).invalid);
    CHECK(invalid.value(1).reason.empty());
}

TEST_CASE("ConfigSpecStore: a file that is not written right keeps its settings dirty, and Save says so")
{
    Session session;
    session.open(MOD_TOML);
    CHECK(session.store.set(session.setting(0), true));
    session.myGames.write(MOD_FILE, "[Main\r\nbEnable = false\r\n");
    CHECK_FALSE(session.store.save());
    CHECK(session.store.isDirty(session.setting(0)));
    CHECK(readFile(session.myGames.path / MOD_FILE) == "[Main\r\nbEnable = false\r\n");

    // and is written by a Save once it can be
    session.myGames.write(MOD_FILE, MOD_TOML);
    CHECK(session.store.save());
    CHECK(session.store.dirtyCount() == 0);
}

namespace
{
    // a setting of the INI file and one of the TOML file
    constexpr auto TWO_FILES = R"(
        { "type": "switcher", "valueType": "bool", "text": "On", "default": false, "storage": { "file": "ini", "section": "Main", "key": "bOn" } },
        { "type": "switcher", "valueType": "bool", "text": "Enable", "default": true, "storage": { "path": ["Main", "bEnable"] } }
    )";
}

TEST_CASE("ConfigSpecStore: a Save that fails leaves every file as it was, and keeps every setting dirty")
{
    SECTION("a file that cannot have its values writes no other file")
    {
        Session session(TWO_FILES);
        session.data.write("MyMod.ini", "[Main]\r\nbOn=0\r\n");
        session.open("[Main]\r\nbEnable = false\r\n");
        CHECK(session.store.set(session.setting(0), true));
        CHECK(session.store.set(session.setting(1), true));
        session.myGames.write(MOD_FILE, "[Main\r\n");
        CHECK_FALSE(session.store.save());
        CHECK(readFile(session.data.path / "MyMod.ini") == "[Main]\r\nbOn=0\r\n");
        CHECK(session.store.dirtyCount() == 2);
    }

    SECTION("a file that cannot be written has the files written before it put back")
    {
        Session session(TWO_FILES);
        session.data.write("MyMod.ini", "[Main]\r\nbOn=0\r\n");
        session.open("[Main]\r\nbEnable = false\r\n");
        CHECK(session.store.set(session.setting(0), true));
        CHECK(session.store.set(session.setting(1), true));

        // the files are written in the order of their names, so the INI file `ini` is written before `main`
        const auto toml = session.myGames.path / MOD_FILE;
        {
            const test_files::ReadOnly readOnly(toml);
            CHECK_FALSE(session.store.save());
        }
        CHECK(readFile(session.data.path / "MyMod.ini") == "[Main]\r\nbOn=0\r\n");
        CHECK(readFile(toml) == "[Main]\r\nbEnable = false\r\n");
        CHECK(session.store.dirtyCount() == 2);

        // and the next Save writes both
        CHECK(session.store.save());
        CHECK(readFile(session.data.path / "MyMod.ini") == "[Main]\r\nbOn=1\r\n");
        CHECK(readFile(toml) == "[Main]\r\nbEnable = true\r\n");
        CHECK(session.store.dirtyCount() == 0);
    }

    SECTION("a file the Save created is removed")
    {
        Session session(TWO_FILES);
        session.open("[Main]\r\nbEnable = false\r\n");
        CHECK(session.store.set(session.setting(0), true));
        CHECK(session.store.set(session.setting(1), true));
        const auto toml = session.myGames.path / MOD_FILE;
        {
            const test_files::ReadOnly readOnly(toml);
            CHECK_FALSE(session.store.save());
        }
        CHECK_FALSE(std::filesystem::exists(session.data.path / "MyMod.ini"));
        CHECK(session.store.dirtyCount() == 2);
    }
}

TEST_CASE("ConfigSpecStore: Save creates a file that is not there, with the folders on the way to it, and with only the settings it changed")
{
    Session session;
    session.store.open(session.mod);

    // a setting set back to its default changes nothing, so no file is created
    CHECK(session.store.set(session.setting(0), false));
    CHECK(session.store.set(session.setting(0), true));
    CHECK(session.store.save());
    CHECK_FALSE(std::filesystem::exists(session.myGames.path / "Mods_Config"));

    CHECK(session.store.set(session.setting(0), false));
    CHECK(session.store.set(session.setting(6), "7"s));
    CHECK(session.store.save());
    CHECK(readFile(session.myGames.path / MOD_FILE) == "[Main]\nbEnable = false\niCount = 7\n");
    CHECK(session.store.dirtyCount() == 0);
    CHECK(session.value(6).value == model::Value("7"s));

    // and the next Save writes into the file it created
    CHECK(session.store.set(session.setting(1), 0.75f));
    CHECK(session.store.save());
    CHECK(readFile(session.myGames.path / MOD_FILE) == "[Main]\nbEnable = false\niCount = 7\nfScale = 0.75\n");

    Session ini(TWO_FILES);
    ini.open(MOD_TOML);
    CHECK(ini.store.set(ini.setting(0), true));
    CHECK(ini.store.save());
    CHECK(readFile(ini.data.path / "MyMod.ini") == "[Main]\r\nbOn=1\r\n");
}

TEST_CASE("ConfigSpecStore: a setting of an INI file has the file's value or is invalid, and Save writes it with the file's layout kept")
{
    Session session(R"(
        { "type": "switcher", "valueType": "bool", "text": "On", "default": false, "storage": { "file": "ini", "section": "Main", "key": "bOn" } },
        { "type": "slider", "valueType": "int", "text": "Count", "default": 3, "valueOptions": { "min": 0, "max": 10, "step": 1 },
          "storage": { "file": "ini", "section": "Main", "key": "iCount" } },
        { "type": "slider", "valueType": "float", "text": "Scale", "default": 1.0, "valueOptions": { "min": 0.5, "max": 1.5, "step": 0.05 },
          "storage": { "file": "ini", "section": "Main", "key": "fScale" } },
        { "type": "textinput", "valueType": "string", "text": "Name", "default": "Me", "storage": { "file": "ini", "section": "Main", "key": "sName" } },
        { "type": "switcher", "valueType": "bool", "text": "New", "default": true, "storage": { "file": "ini", "section": "Main", "key": "bNew" } }
    )");
    session.data.write("MyMod.ini", "; My Mod\r\n[Main]\r\nbOn=1\r\niCount = 12\r\nfScale = abc\r\nsName = You ; and me\r\n");
    session.store.open(session.mod);
    CHECK(session.value(0).value == model::Value(true));
    CHECK(session.value(1).value == model::Value(12.0f));
    CHECK(session.value(1).reason == "Invalid: iCount of [Main] is 12 in MyMod.ini, which is not from 0 to 10.");
    CHECK(session.value(2).reason == R"(Invalid: fScale of [Main] is "abc" in MyMod.ini, which is not a number.)");
    CHECK(session.value(3).value == model::Value("You"s));
    // a key the file does not have is its default
    CHECK(session.value(4).value == model::Value(true));
    CHECK_FALSE(session.value(4).invalid);

    CHECK(session.store.set(session.setting(0), false));
    CHECK(session.store.set(session.setting(1), 4.0f));
    CHECK(session.store.set(session.setting(2), 0.75f));
    CHECK(session.store.set(session.setting(4), false));
    CHECK(session.store.save());
    CHECK(readFile(session.data.path / "MyMod.ini") == "; My Mod\r\n[Main]\r\nbOn=0\r\niCount = 4\r\nfScale = 0.75\r\nsName = You ; and me\r\nbNew = 0\r\n");
    CHECK(session.store.dirtyCount() == 0);
    CHECK_FALSE(session.value(1).invalid);
}

TEST_CASE("ConfigSpecStore: a binding, a color and a transform of an INI file are its lines, invalid with the reason, and Save writes the menu's line")
{
    Session session(R"(
        { "type": "cf.inputBinding", "text": "Grab", "default": { "hand": "offhand", "type": "tap", "button": "trigger" },
          "storage": { "file": "ini", "section": "Main", "key": "sGrab" } },
        { "type": "cf.inputBinding", "text": "Open", "default": { "hand": "primary", "type": "tap", "button": "a" }, "storage": { "file": "ini", "section": "Main", "key": "sOpen" } },
        { "type": "cf.color", "text": "Ray", "default": { "rgb": [190, 255, 190], "alpha": 200 }, "storage": { "file": "ini", "section": "Main", "key": "sRay" } },
        { "type": "cf.color", "text": "Empty", "default": { "rgb": [255, 255, 255] }, "storage": { "file": "ini", "section": "Main", "key": "sEmpty" } },
        { "type": "cf.transform", "text": "Mesh", "default": { "scale": 1.0 }, "storage": { "file": "ini", "section": "Main", "key": "sMesh" } },
        { "type": "cf.transform", "text": "New", "default": { "pos": [0, 0, 12] }, "valueOptions": { "rotation": false },
          "storage": { "file": "ini", "section": "Main", "key": "sNew" } }
    )");
    session.data.write("MyMod.ini", "[Main]\r\nsGrab = off  tap Trigger\r\nsOpen =\r\nsRay = 255, 0, 0\r\nsEmpty =\r\nsMesh = 1,2,3;0,90 ; mesh\r\n");
    session.store.open(session.mod);

    // the file's line in the menu's words, and an empty binding is off
    CHECK(session.value(0).value == model::Value("offhand tap trigger"s));
    CHECK(session.store.defaultValue(session.setting(0)) == session.value(0).value);
    CHECK(session.value(1).value == model::Value("none"s));
    CHECK_FALSE(session.value(1).invalid);
    // a color with no alpha has its default's
    CHECK(session.value(2).value == model::Value("255,0,0"s));
    CHECK(model::colorOf(session.setting(2), session.value(2).value) == render::Color::rgba(255, 0, 0, 200));
    // an empty color is invalid, and so is a transform short of its scale, with no comment in its value
    CHECK(session.value(3).invalid);
    CHECK(session.value(3).reason == "Invalid: it is not r,g,b or r,g,b,a, each a whole number in 0..255, as 255,180,107.");
    CHECK(session.value(4).value == model::Value("1,2,3;0,90"s));
    CHECK(session.value(4).reason == "Invalid: it is not the seven numbers x,y,z;heading,roll,attitude;scale, as 0,0,12;0,0,0;1.");
    // a key the file does not have is its default's line
    CHECK(session.value(5).value == model::Value("0.0,0.0,12.0;0.0,0.0,0.0;1.0"s));
    CHECK(session.store.dirtyCount() == 0);

    // a value the player sets is the menu's line too, and a table is no line. A value is written where the file's was
    CHECK(session.store.set(session.setting(1), "main longpress b 1.5 suppress +offhand:grip"s));
    CHECK(session.value(1).value == model::Value("primary longpress menu 1.5 suppress +offhand:grip"s));
    CHECK(session.store.set(session.setting(2), "{ rgb = [1, 2, 3] }"s));
    CHECK(session.value(2).invalid);
    CHECK(session.store.set(session.setting(2), "1,2,3,4"s));
    CHECK(session.store.set(session.setting(3), "0,0,0"s));
    CHECK(session.store.set(session.setting(5), "0,0,10;0,0,0;1"s));
    REQUIRE(session.store.save());
    CHECK(readFile(session.data.path / "MyMod.ini") ==
          "[Main]\r\n"
          "sGrab = off  tap Trigger\r\n"
          "sOpen =primary longpress menu 1.5 suppress +offhand:grip\r\n"
          "sRay = 1,2,3,4\r\n"
          "sEmpty =0,0,0\r\n"
          "sMesh = 1,2,3;0,90 ; mesh\r\n"
          "sNew = 0.0,0.0,10.0;0.0,0.0,0.0;1.0\r\n");
    CHECK(session.store.dirtyCount() == 0);
}

TEST_CASE("ConfigSpecStore: every setting of the config spec showcase's INI file is available, valid and clean with the shipped file")
{
    const test_globals::TestGlobals globals;
    const test_properties::TestProperties properties;
    const auto mods = spec::loadConfigSpecs(MODS_CONFIG_SPEC_SHOWCASE_DIR);
    REQUIRE(mods.size() == 1);

    // the showcase's game Data folder, which has its INI file
    const test_files::TempFolder myGames;
    ConfigSpecStore store(std::filesystem::path(MODS_CONFIG_SPEC_SHOWCASE_DIR).parent_path().parent_path(), myGames.path);
    store.open(mods[0]);

    std::size_t checked = 0;
    const auto checkItems = [&store, &checked](const std::vector<model::Item>& items, const auto& self) -> void {
        for (const auto& item : items) {
            if (const auto* group = std::get_if<model::Group>(&item)) {
                self(group->items, self);
            } else if (const auto* setting = std::get_if<model::Setting>(&item)) {
                // a setting with no default is a mistake of the Mistakes page, as a default an INI line cannot have
                const auto* storage = std::get_if<model::FileStorage>(&setting->storage);
                if (!storage || storage->file != "ini" || !setting->defaultValue) {
                    continue;
                }
                const auto value = store.value(*setting);
                INFO(setting->id << ": " << value.reason);
                CHECK(value.available);
                CHECK_FALSE(value.invalid);
                CHECK(value.value == *store.defaultValue(*setting));
                ++checked;
            }
        }
    };
    const auto checkPages = [&checkItems](const std::vector<model::Page>& pages, const auto& self) -> void {
        for (const auto& page : pages) {
            checkItems(page.items, checkItems);
            self(page.pages, self);
        }
    };
    checkItems(mods[0].frontPage.items, checkItems);
    checkPages(mods[0].pages, checkPages);
    CHECK(checked >= 4);
    CHECK(store.dirtyCount() == 0);
}

TEST_CASE("ConfigSpecStore: a setting's own Discard takes back its change alone, and its saved value is its baseline")
{
    Session session;
    session.open("[Main]\r\nbEnable = false\r\nfScale = 2.0\r\n");
    CHECK(session.store.set(session.setting(0), true));
    CHECK(session.store.set(session.setting(1), 1.0f));
    CHECK(session.store.savedValue(session.setting(0)) == model::Value(false));

    // an invalid value of the file is the saved value too
    CHECK(session.store.savedValue(session.setting(1)) == model::Value(2.0f));

    session.store.discard(session.setting(0));
    CHECK_FALSE(session.store.isDirty(session.setting(0)));
    CHECK(session.value(0).value == model::Value(false));
    CHECK(session.store.isDirty(session.setting(1)));
    CHECK_FALSE(session.store.savedValue(session.setting(0)));

    // a clean setting has nothing to take back
    session.store.discard(session.setting(0));
    CHECK(session.store.dirtyCount() == 1);
    session.store.discard(session.setting(1));
    CHECK(session.value(1).invalid);
    CHECK(session.store.dirtyCount() == 0);
}

TEST_CASE("ConfigSpecStore: Discard takes back every pending value, and writes nothing")
{
    Session session;
    session.open("[Main]\r\nbEnable = false\r\nfScale = 2.0\r\n");
    CHECK(session.store.set(session.setting(0), true));
    CHECK(session.store.set(session.setting(1), 1.0f));
    CHECK(session.store.dirtyCount() == 2);
    session.store.discard();
    CHECK(session.store.dirtyCount() == 0);
    CHECK(session.value(0).value == model::Value(false));

    // an invalid value of the file is invalid again
    CHECK(session.value(1).invalid);
    CHECK(session.value(1).value == model::Value(2.0f));

    // a Save then has nothing to write
    CHECK(session.store.save());
    CHECK(readFile(session.myGames.path / MOD_FILE) == "[Main]\r\nbEnable = false\r\nfScale = 2.0\r\n");
}

namespace
{
    // a setting of each control kept in the game: two globals and two properties of a script, with an action
    constexpr auto GAME_SETTINGS = R"(
        { "type": "slider", "valueType": "float", "text": "Time of day", "default": 12.0, "valueOptions": { "min": 0, "max": 24, "step": 0.5 },
          "storage": { "global": "Fallout4.esm|38" } },
        { "type": "switcher", "valueType": "bool", "text": "Automation", "default": true, "storage": { "global": "MyMod.esp|801" }, "applies": "save",
          "action": { "type": "CallFunction", "form": "MyMod.esp|800", "function": "OnAutomationChanged", "params": ["{value}", 1] } },
        { "type": "stepper", "valueType": "int", "text": "Mode", "default": 0, "valueOptions": { "options": ["Off", "Low", "High"] },
          "storage": { "form": "MyMod.esp|800", "scriptName": "MyMod:Config", "propertyName": "iMode" } },
        { "type": "textinput", "valueType": "string", "text": "Name", "default": "Me",
          "storage": { "form": "MyMod.esp|800", "propertyName": "sName" } }
    )";

    /**
     * The game of a test, with each global and property of `GAME_SETTINGS` in it.
     */
    struct TestGame
    {
        test_globals::TestGlobals globals;
        test_properties::TestProperties properties;
        test_actions::TestActions actions;

        TestGame()
        {
            globals.values[{ "Fallout4.esm", 0x38 }] = 8.5f;
            globals.values[{ "MyMod.esp", 0x801 }] = 0.0f;
            properties.values[{ "MyMod.esp", 0x800, "MyMod:Config", "iMode" }] = 2;
            properties.values[{ "MyMod.esp", 0x800, "", "sName" }] = "You"s;
        }

        float hour() const
        {
            return globals.values.at({ "Fallout4.esm", 0x38 });
        }

        model::Value mode() const
        {
            return properties.values.at({ "MyMod.esp", 0x800, "MyMod:Config", "iMode" });
        }
    };
}

TEST_CASE("ConfigSpecStore: a setting of a global or a property has the game's value as its baseline, and one that is not in the game is not available")
{
    TestGame game;
    Session session(GAME_SETTINGS);
    session.open(MOD_TOML);
    CHECK(session.value(0).value == model::Value(8.5f));
    // a global of a switcher is on at any number but 0
    CHECK(session.value(1).value == model::Value(false));
    CHECK(session.value(2).value == model::Value(2));
    CHECK(session.value(3).value == model::Value("You"s));
    for (std::size_t row = 0; row < 4; ++row) {
        CHECK(session.value(row).available);
        CHECK_FALSE(session.value(row).invalid);
        CHECK_FALSE(session.store.isDirty(session.setting(row)));
    }

    // the baseline is the one of the session's start: the game reads nothing again for another page
    game.globals.values[{ "Fallout4.esm", 0x38 }] = 9.0f;
    session.store.pageShown();
    CHECK(session.value(0).value == model::Value(8.5f));

    // a value that is no value of its setting is invalid, and one of another type is not available
    game.globals.values[{ "Fallout4.esm", 0x38 }] = 30.0f;
    game.properties.values[{ "MyMod.esp", 0x800, "", "sName" }] = 5;
    game.globals.values.erase({ "MyMod.esp", 0x801 });
    session.store.open(session.mod);
    CHECK(session.value(0).invalid);
    CHECK(session.value(0).reason == "Invalid: the global Fallout4.esm|38 is 30 in the game, which is not from 0 to 24.");
    CHECK(session.value(1).reason == "Not available: the global MyMod.esp|801 is not in the game.");
    CHECK(session.value(3).reason == "Not available: the property sName of MyMod.esp|800 is 5, which is no value of the setting's valueType.");
    CHECK_FALSE(session.value(1).available);
    CHECK_FALSE(session.value(3).available);
    CHECK_FALSE(session.store.set(session.setting(1), true));
}

TEST_CASE("ConfigSpecStore: a live setting of the game is previewed, and has its baseline again as it is set back, discarded, or its session ends")
{
    TestGame game;
    Session session(GAME_SETTINGS);
    session.open(MOD_TOML);

    // the game has a pending value at once, and it is still dirty
    CHECK(session.store.set(session.setting(0), 20.0f));
    CHECK(common::fEqual(game.hour(), 20.0f));
    CHECK(session.store.set(session.setting(2), 0));
    CHECK(game.mode() == model::Value(0));
    CHECK(session.store.dirtyCount() == 2);

    // set back to its baseline, the game has it again
    CHECK(session.store.set(session.setting(0), 8.5f));
    CHECK(common::fEqual(game.hour(), 8.5f));
    CHECK(session.store.dirtyCount() == 1);

    // its own Discard writes its baseline back, and leaves the other's change
    CHECK(session.store.set(session.setting(0), 6.0f));
    session.store.discard(session.setting(0));
    CHECK(common::fEqual(game.hour(), 8.5f));
    CHECK(game.mode() == model::Value(0));
    CHECK(session.store.dirtyCount() == 1);

    CHECK(session.store.set(session.setting(0), 6.0f));
    session.store.discard();
    CHECK(common::fEqual(game.hour(), 8.5f));
    CHECK(game.mode() == model::Value(2));
    CHECK(session.store.dirtyCount() == 0);

    CHECK(session.store.set(session.setting(0), 6.0f));
    session.store.close();
    CHECK(common::fEqual(game.hour(), 8.5f));

    // an invalid value is not previewed: the game keeps the last valid one, and has the baseline again as it ends
    session.store.open(session.mod);
    CHECK(session.store.set(session.setting(0), 6.0f));
    CHECK(session.store.set(session.setting(0), 30.0f));
    CHECK(session.value(0).invalid);
    CHECK(common::fEqual(game.hour(), 6.0f));
    session.store.discard();
    CHECK(common::fEqual(game.hour(), 8.5f));
    CHECK(session.store.set(session.setting(0), 30.0f));
    CHECK(common::fEqual(game.hour(), 8.5f));
    session.store.close();

    // a setting that is not live waits for a Save
    session.store.open(session.mod);
    CHECK(session.store.set(session.setting(1), true));
    CHECK(common::fEqual(game.globals.values.at({ "MyMod.esp", 0x801 }), 0.0f));
    session.store.close();
    CHECK(game.actions.called.empty());
}

TEST_CASE("ConfigSpecStore: Save writes the game's values after the files, then runs the action of each setting it wrote, with its value")
{
    TestGame game;
    Session session(std::string(GAME_SETTINGS) + "," + R"(
        { "type": "slider", "valueType": "int", "text": "Count", "default": 3, "valueOptions": { "min": 0, "max": 9 }, "storage": { "path": ["Main", "iCount"] },
          "action": { "type": "CallGlobalFunction", "script": "MyMod:Config", "function": "SetCount", "params": ["{value}"] } },
        { "type": "switcher", "valueType": "bool", "text": "HUD", "default": true, "storage": { "path": ["Main", "bHud"] },
          "action": { "type": "RunConsoleCommand", "command": "tm" } }
    )");
    session.open("[Main]\r\niCount = 5\r\nbHud = true\r\n");
    CHECK(session.store.set(session.setting(1), true));
    CHECK(session.store.set(session.setting(3), "Them"s));
    CHECK(session.store.set(session.setting(4), 7.0f));
    // a change that is set back runs nothing
    CHECK(session.store.set(session.setting(5), false));
    CHECK(session.store.set(session.setting(5), true));
    CHECK(session.store.save());

    // a global keeps a bool as 1
    CHECK(common::fEqual(game.globals.values.at({ "MyMod.esp", 0x801 }), 1.0f));
    CHECK(game.properties.values.at({ "MyMod.esp", 0x800, "", "sName" }) == model::Value("Them"s));
    CHECK(readFile(session.myGames.path / MOD_FILE) == "[Main]\r\niCount = 7\r\nbHud = true\r\n");
    CHECK(session.store.dirtyCount() == 0);
    CHECK(session.value(1).value == model::Value(true));

    // in the order of the config spec, with the value in its type
    REQUIRE(game.actions.called.size() == 2);
    CHECK(game.actions.called[0].function == "OnAutomationChanged");
    CHECK(game.actions.called[0].arguments == std::vector<model::Value>{ true, 1 });
    CHECK(game.actions.called[0].valueArguments.empty());
    CHECK(game.actions.called[1].function == "SetCount");
    CHECK(game.actions.called[1].arguments == std::vector<model::Value>{ 7 });
    CHECK(game.actions.commands.empty());

    // a setting that is not written again runs nothing
    CHECK(session.store.save());
    CHECK(game.actions.called.size() == 2);
}

TEST_CASE("ConfigSpecStore: a Save whose file cannot be written writes nothing into the game")
{
    TestGame game;
    Session session(std::string(GAME_SETTINGS) + "," + R"(
        { "type": "switcher", "valueType": "bool", "text": "Enable", "default": true, "storage": { "path": ["Main", "bEnable"] } }
    )");
    session.open(MOD_TOML);
    CHECK(session.store.set(session.setting(1), true));
    CHECK(session.store.set(session.setting(4), true));
    session.myGames.write(MOD_FILE, "[Main\r\n");
    CHECK_FALSE(session.store.save());
    CHECK(common::fEqual(game.globals.values.at({ "MyMod.esp", 0x801 }), 0.0f));
    CHECK(session.store.dirtyCount() == 2);
    CHECK(game.actions.called.empty());

    // nor does one with a global that is not in the game since the session started, which writes no file
    session.myGames.write(MOD_FILE, MOD_TOML);
    game.globals.values.erase({ "MyMod.esp", 0x801 });
    CHECK_FALSE(session.store.save());
    CHECK(readFile(session.myGames.path / MOD_FILE) == MOD_TOML);
    CHECK(session.store.dirtyCount() == 2);
    CHECK(game.actions.called.empty());
}

TEST_CASE("ConfigSpecStore: the default of a setting is its config spec's, in the type of its control")
{
    Session session;
    session.open(MOD_TOML);
    CHECK(session.store.defaultValue(session.setting(0)) == model::Value(true));
    CHECK(session.store.defaultValue(session.setting(2)) == model::Value(200.0f));
    CHECK(session.store.defaultValue(session.setting(6)) == model::Value("3"s));
    CHECK_FALSE(session.store.hotkey({}).available);
}

TEST_CASE("ConfigSpecStore: a choice reads its options in the game as its mod is selected, and is not available while its list is not in the game")
{
    test_properties::TestProperties lists;
    lists.lists[{ "MyMod.esp", 0x900, "", "" }] = { "Sanctuary", "Red Rocket" };
    lists.lists[{ "MyMod.esp", 0x800, "", "Names" }] = { "Nate", "Nora" };
    Session session(R"(
        { "type": "dropdown", "text": "Settlement", "default": 0, "valueOptions": { "listFromForm": "MyMod.esp|900" }, "storage": { "path": ["Main", "iPlace"] } },
        { "type": "stepper", "valueType": "string", "text": "Name", "default": "Nate",
          "valueOptions": { "listFromProperty": { "form": "MyMod.esp|800", "propertyName": "Names" } }, "storage": { "path": ["Main", "sName"] } },
        { "type": "dropdown", "text": "Missing", "default": 0, "valueOptions": { "listFromForm": "MyMod.esp|901" }, "storage": { "path": ["Main", "iMissing"] } }
    )");
    session.open("[Main]\r\niPlace = 1\r\nsName = \"Shaun\"\r\n");
    using Options = std::vector<model::Option>;

    // a choice of an int has the place of its option, and one of a string its text
    CHECK(session.store.options(session.setting(0)) == Options{ { 0, "Sanctuary" }, { 1, "Red Rocket" } });
    CHECK(session.store.options(session.setting(1)) == Options{ { "Nate"s, "Nate" }, { "Nora"s, "Nora" } });
    CHECK(session.value(0).value == model::Value(1));

    // a value of the file that is none of the options read is invalid, and so is one that is set
    CHECK(session.value(1).invalid);
    CHECK(session.value(1).reason == "Invalid: Main.sName is \"Shaun\" in MyMod.toml, which is none of its options.");
    CHECK(session.store.set(session.setting(1), "Nora"s));
    CHECK_FALSE(session.value(1).invalid);
    CHECK(session.store.set(session.setting(0), 2));
    CHECK(session.value(0).invalid);

    CHECK_FALSE(session.value(2).available);
    CHECK(session.value(2).reason == "Not available: the FormList MyMod.esp|901 of its options is not in the game.");
    CHECK(session.store.options(session.setting(2)).empty());
    CHECK_FALSE(session.store.set(session.setting(2), 0));

    // the options are of the session's start, as its values are: a list that changes is read by the next session
    lists.lists[{ "MyMod.esp", 0x900, "", "" }].push_back("Abernathy Farm");
    session.store.pageShown();
    CHECK(session.store.options(session.setting(0)).size() == 2);
    session.store.open(session.mod);
    CHECK(session.store.options(session.setting(0)).size() == 3);
}

TEST_CASE("ConfigSpecStore: OpenURL launches immediately and reports a failed browser launch")
{
    test_actions::TestActions actions;
    Session session(R"(
        { "type": "button", "text": "Documentation", "action": { "type": "OpenURL", "url": "https://example.com/help" } }
    )");
    session.open(MOD_TOML);
    const auto& action = std::get<model::Button>(session.mod.frontPage.items[0]).action;
    CHECK(session.store.notRunnable(action).empty());
    const auto opened = session.store.run(action);
    CHECK(opened.ran);
    CHECK_FALSE(opened.failed);
    CHECK(opened.message == "Opened in your desktop browser.");
    CHECK(actions.links == std::vector{ model::WebLink{ "https://example.com/help" } });
    CHECK(session.store.dirtyCount() == 0);
    CHECK(actions.called.empty());
    CHECK(actions.commands.empty());

    actions.browserAvailable = false;
    const auto failed = session.store.run(action);
    CHECK(failed.ran);
    CHECK(failed.failed);
    CHECK(failed.message == "Could not open the link in your desktop browser.");
    CHECK(actions.links.size() == 2);
}

TEST_CASE("ConfigSpecStore: a button calls its function or runs its console command at once")
{
    test_actions::TestActions actions;
    Session session(R"(
        { "type": "button", "text": "Reset", "action": { "type": "CallFunction", "form": "MyMod.esp|800", "function": "ResetAll", "params": [1] } },
        { "type": "button", "text": "Hide the HUD", "action": { "type": "RunConsoleCommand", "command": "tm" } },
        { "type": "button", "text": "Calibrate", "action": { "type": "CallNativeFunction", "function": "calibrate" } }
    )");
    session.open(MOD_TOML);
    const auto action = [&session](const std::size_t row) {
        return std::get<model::Button>(session.mod.frontPage.items.at(row)).action;
    };

    CHECK(session.store.run(action(0)).ran);
    REQUIRE(actions.called.size() == 1);
    CHECK(actions.called[0].function == "ResetAll");
    CHECK(actions.called[0].arguments == std::vector<model::Value>{ 1 });

    CHECK(session.store.run(action(1)).ran);
    CHECK(actions.commands == std::vector{ model::ConsoleCommand{ "tm" } });

    // an action the menu does not run, a row with none, and a function whose form is not in the game
    CHECK_FALSE(session.store.run(action(2)).ran);
    CHECK_FALSE(session.store.run(model::NoAction{}).ran);
    actions.inGame = false;
    CHECK_FALSE(session.store.run(action(0)).ran);
    CHECK(actions.called.size() == 1);

    // nothing waits for a Save
    CHECK(session.store.dirtyCount() == 0);
}

TEST_CASE("ConfigSpecStore: a read-only setting has its file's value as it is, none where the file has none, and is not set")
{
    Session session(R"(
        { "type": "slider", "valueType": "float", "text": "Height", "readOnly": true, "valueOptions": { "min": 1.0, "max": 2.5, "step": 0.01 },
          "storage": { "path": ["Main", "fHeight"] } },
        { "type": "slider", "valueType": "float", "text": "Past", "readOnly": true, "valueOptions": { "min": 0.0, "max": 1.0 },
          "storage": { "path": ["Main", "fPast"] } },
        { "type": "dropdown", "valueType": "string", "text": "Place", "readOnly": true,
          "valueOptions": { "options": [ { "value": "hip", "text": "Hip" } ] }, "storage": { "path": ["Main", "sPlace"] } },
        { "type": "switcher", "valueType": "bool", "text": "Calibrated", "readOnly": true, "storage": { "path": ["Main", "bCalibrated"] } },
        { "type": "switcher", "valueType": "bool", "text": "Text", "readOnly": true, "storage": { "path": ["Main", "bText"] } }
    )");
    session.open("[Main]\r\nfHeight = 1.78\r\nfPast = 2.0\r\nsPlace = \"elbow\"\r\nbText = \"yes\"\r\n");

    const auto height = session.value(0);
    CHECK(height.available);
    CHECK(height.value == model::Value(1.78f));

    // outside its limits and none of its options, as the file has it, and never invalid
    for (const std::size_t row : { 1u, 2u }) {
        INFO(session.setting(row).text);
        const auto value = session.value(row);
        CHECK(value.available);
        CHECK_FALSE(value.invalid);
        CHECK(value.reason.empty());
    }
    CHECK(session.value(1).value == model::Value(2.0f));
    CHECK(session.value(2).value == model::Value("elbow"s));

    // none where the file has none, with no reason, as the row shows none
    const auto none = session.value(3);
    CHECK_FALSE(none.available);
    CHECK(none.reason.empty());
    CHECK_FALSE(session.store.valueById("bCalibrated:Main").available);

    // one of another type is no value it shows
    const auto text = session.value(4);
    CHECK_FALSE(text.available);
    CHECK_FALSE(text.invalid);
    CHECK(text.reason == R"(Not available: Main.bText is "yes" in MyMod.toml, which is not true or false.)");

    // it is not set, has no default, and is never dirty
    CHECK_FALSE(session.store.set(session.setting(0), 1.5f));
    CHECK_FALSE(session.store.set(session.setting(3), true));
    CHECK_FALSE(session.store.defaultValue(session.setting(0)));
    CHECK(session.store.dirtyCount() == 0);
    CHECK(session.value(0).value == model::Value(1.78f));
}

namespace
{
    // a setting of each applies, of a file and of the game, and a read-only one
    constexpr auto APPLIES_SETTINGS = R"(
        { "type": "switcher", "valueType": "bool", "text": "Live", "default": true, "storage": { "path": ["Main", "bLive"] } },
        { "type": "switcher", "valueType": "bool", "text": "Save", "default": true, "applies": "save", "storage": { "path": ["Main", "bSave"] } },
        { "type": "switcher", "valueType": "bool", "text": "Restart", "default": true, "applies": "restart", "storage": { "path": ["Main", "bRestart"] } },
        { "type": "switcher", "valueType": "bool", "text": "Also restart", "default": true, "applies": "restart", "storage": { "path": ["Main", "bAlso"] } },
        { "type": "slider", "valueType": "float", "text": "Time of day", "default": 12.0, "valueOptions": { "min": 0, "max": 24, "step": 0.5 },
          "storage": { "global": "Fallout4.esm|38" } },
        { "type": "switcher", "valueType": "bool", "text": "Read", "readOnly": true, "applies": "restart", "storage": { "path": ["Main", "bRead"] } }
    )";
}

TEST_CASE("ConfigSpecStore: a setting takes effect as its applies says, also a live one of a file, and a read-only one never")
{
    TestGame game;
    Session session(APPLIES_SETTINGS);
    session.open("[Main]\r\n");
    CHECK(session.store.applies(session.setting(0)) == model::Applies::Live);
    CHECK(session.store.applies(session.setting(1)) == model::Applies::Save);
    CHECK(session.store.applies(session.setting(2)) == model::Applies::Restart);
    CHECK(session.store.applies(session.setting(4)) == model::Applies::Live);
    CHECK_FALSE(session.store.applies(session.setting(5)));

    // a live one of a file is not in the game, so a change of it is only pending
    CHECK(session.store.set(session.setting(0), false));
    CHECK(session.store.isDirty(session.setting(0)));
}

TEST_CASE("ConfigSpecStore: a live setting of a file is previewed to its mod's client, and a Discard tells it its baseline again")
{
    Session session;
    ToldChanges told(session.store);
    session.open(MOD_TOML);

    // each valid pending value, in the setting's type, after the value told before it
    CHECK(session.store.set(session.setting(1), 1.4f));
    CHECK(session.store.set(session.setting(1), 1.3f));
    CHECK(session.store.set(session.setting(2), 100.0f));
    CHECK(told.lines == std::vector<std::string>{ "Preview fScale:Main main:Main.fScale 1.25 -> 1.4",
                            "Preview fScale:Main main:Main.fScale 1.4 -> 1.3",
                            "Preview iOpacity:Main main:Main.iOpacity 128 -> 100" });
    CHECK(told.filePath.ends_with(R"(Mods_Config\MyMod\MyMod.toml)"));

    // set back, it is a Discard to the baseline
    told.lines.clear();
    CHECK(session.store.set(session.setting(1), 1.25f));
    CHECK(told.lines == std::vector<std::string>{ "Discard fScale:Main main:Main.fScale 1.3 -> 1.25" });

    // and so is its own Discard
    told.lines.clear();
    CHECK(session.store.set(session.setting(1), 1.4f));
    session.store.discard(session.setting(1));
    CHECK(told.lines == std::vector<std::string>{ "Preview fScale:Main main:Main.fScale 1.25 -> 1.4", "Discard fScale:Main main:Main.fScale 1.4 -> 1.25" });

    // an invalid value is not told, and the client keeps the preview before it
    told.lines.clear();
    CHECK(session.store.set(session.setting(6), "x"s));
    CHECK(told.lines.empty());

    // Discard, and the end of the session, tell each preview's baseline
    session.store.discard();
    CHECK(told.lines == std::vector<std::string>{ "Discard iOpacity:Main main:Main.iOpacity 100 -> 128" });
    told.lines.clear();
    CHECK(session.store.set(session.setting(0), true));
    session.store.close();
    CHECK(told.lines == std::vector<std::string>{ "Preview bEnable:Main main:Main.bEnable false -> true", "Discard bEnable:Main main:Main.bEnable true -> false" });
}

TEST_CASE("ConfigSpecStore: a preview is told to the client at most once in its interval, and the last value once it is past")
{
    Session session;
    ToldChanges told(session.store);
    auto now = std::chrono::steady_clock::time_point{};
    session.store.setPreviewPacing(ConfigSpecStore::PREVIEW_INTERVAL, [&now] {
        return now;
    });
    session.open(MOD_TOML);
    const auto& scale = session.setting(1);

    // the first is told at once, and a drag within the interval is held, with its last value
    CHECK(session.store.set(scale, 1.4f));
    now += std::chrono::milliseconds(20);
    CHECK(session.store.set(scale, 1.3f));
    CHECK(session.store.set(scale, 1.2f));
    session.store.frameUpdate();
    CHECK(told.lines == std::vector<std::string>{ "Preview fScale:Main main:Main.fScale 1.25 -> 1.4" });

    // the frame past the interval tells it, after the value told before
    now += std::chrono::milliseconds(30);
    session.store.frameUpdate();
    session.store.frameUpdate();
    CHECK(told.lines == std::vector<std::string>{ "Preview fScale:Main main:Main.fScale 1.25 -> 1.4", "Preview fScale:Main main:Main.fScale 1.4 -> 1.2" });

    // a change past the interval is told at once
    told.lines.clear();
    now += std::chrono::milliseconds(60);
    CHECK(session.store.set(scale, 1.1f));
    CHECK(told.lines == std::vector<std::string>{ "Preview fScale:Main main:Main.fScale 1.2 -> 1.1" });

    // a value set back to the one told drops what is held, and a Discard is told at once, from the value told
    told.lines.clear();
    CHECK(session.store.set(scale, 1.3f));
    CHECK(session.store.set(scale, 1.1f));
    now += std::chrono::milliseconds(60);
    session.store.frameUpdate();
    CHECK(told.lines.empty());
    CHECK(session.store.set(scale, 1.4f));
    CHECK(session.store.set(scale, 1.25f));
    session.store.frameUpdate();
    CHECK(told.lines == std::vector<std::string>{ "Preview fScale:Main main:Main.fScale 1.1 -> 1.4", "Discard fScale:Main main:Main.fScale 1.4 -> 1.25" });
}

TEST_CASE("ConfigSpecStore: a client sets a pending value by its setting's id as the player's edit, which is told back to it")
{
    Session session;
    ToldChanges told(session.store);
    CHECK(session.store.setPendingValue("MyMod", "fScale:Main", 1.4f).error() == "the menu does not show its mod");
    session.open(MOD_TOML);

    // in the setting's valueType, also a whole number of a slider, and its mod's name with no regard to case
    CHECK(session.store.setPendingValue("mymod", "fScale:Main", 1.4f));
    CHECK(session.store.setPendingValue("MyMod", "iOpacity:Main", 100));
    CHECK(session.value(1).value == model::Value(1.4f));
    CHECK(session.value(2).value == model::Value(100.0f));
    CHECK(session.store.dirtyCount() == 2);
    CHECK(told.lines == std::vector<std::string>{ "Preview fScale:Main main:Main.fScale 1.25 -> 1.4", "Preview iOpacity:Main main:Main.iOpacity 128 -> 100" });

    // a value told already is not told again, and the baseline makes it clean, with a Discard
    told.lines.clear();
    CHECK(session.store.setPendingValue("MyMod", "fScale:Main", 1.4f));
    CHECK(told.lines.empty());
    CHECK(session.store.setPendingValue("MyMod", "fScale:Main", 1.25f));
    CHECK_FALSE(session.store.isDirty(session.setting(1)));
    CHECK(told.lines == std::vector<std::string>{ "Discard fScale:Main main:Main.fScale 1.4 -> 1.25" });

    // one that is no value of its setting is refused, with nothing changed, as are another mod and an id of none
    told.lines.clear();
    CHECK(session.store.setPendingValue("Other", "fScale:Main", 1.4f).error() == "the menu does not show its mod");
    CHECK(session.store.setPendingValue("MyMod", "fNone:Main", 1.4f).error() == "its config spec has no setting of that id");
    CHECK(session.store.setPendingValue("MyMod", "fScale:Main", 1).error() == "1 is not of the setting's valueType");
    CHECK(session.store.setPendingValue("MyMod", "fScale:Main", 2.0f).error() == "Invalid: 2 is not from 0.5 to 1.5.");
    CHECK(session.store.setPendingValue("MyMod", "sPlace:Main", "back"s).error() == "Invalid: \"back\" is none of its options.");
    CHECK(session.store.setPendingValue("MyMod", "iCount:Main", 12).error() == "Invalid: 12 is not from 1 to 9.");
    CHECK(told.lines.empty());
    CHECK(session.store.dirtyCount() == 1);
    CHECK(session.store.invalidCount() == 0);
}

TEST_CASE("ConfigSpecStore: a client's pending value of a color is its table's text, and one of a read-only setting is refused")
{
    Session session(R"(
        { "type": "cf.color", "text": "Ray", "default": { "rgb": [190, 255, 190] }, "storage": { "path": ["Pointer", "cRay"] } },
        { "type": "slider", "valueType": "float", "text": "Height", "readOnly": true, "valueOptions": { "min": 1.0, "max": 2.5 },
          "storage": { "path": ["Pointer", "fHeight"] } }
    )");
    session.open("[Pointer]\r\ncRay = { rgb = [190, 255, 190] }\r\nfHeight = 1.78\r\n");

    // the menu's text of the table, as a player's color is
    CHECK(session.store.setPendingValue("MyMod", "cRay:Pointer", "{rgb=[1,2,3]}"s));
    CHECK(session.value(0).value == model::Value("{ rgb = [1, 2, 3] }"s));
    CHECK(session.store.setPendingValue("MyMod", "cRay:Pointer", "{ colour = [1, 2, 3] }"s).error() == "Invalid: it has the key 'colour', which is not rgb or alpha.");
    CHECK(session.value(0).value == model::Value("{ rgb = [1, 2, 3] }"s));

    CHECK(session.store.setPendingValue("MyMod", "fHeight:Pointer", 2.0f).error() == "the setting is read-only");
}

TEST_CASE("ConfigSpecStore: a live global or property is told to the mod's client after the game has its value, and so is its Discard")
{
    TestGame game;
    Session session(GAME_SETTINGS);
    ToldChanges told(session.store);
    std::vector<float> hours;
    session.store.clientCalls().addClient("Other", {});
    // the game's hour as the client is told each change: the store writes it first
    const auto check = [&told, &game, &hours] {
        hours.push_back(game.hour());
    };
    session.open(MOD_TOML);

    CHECK(session.store.set(session.setting(0), 18.0f));
    check();
    CHECK(session.store.set(session.setting(2), 1));
    CHECK(game.mode() == model::Value(1));
    // one that is not live, as Automation, is not told
    CHECK(session.store.set(session.setting(1), true));
    session.store.discard();
    check();
    // a Discard of each, in no order
    REQUIRE(told.lines.size() == 4);
    std::ranges::sort(told.lines.begin() + 2, told.lines.end());
    CHECK(told.lines == std::vector<std::string>{ "Preview Fallout4.esm|38 Fallout4.esm|38 8.5 -> 18",
                            "Preview iMode:MyMod.esp|800 MyMod.esp|800 MyMod:Config.iMode 2 -> 1",
                            "Discard Fallout4.esm|38 Fallout4.esm|38 18 -> 8.5",
                            "Discard iMode:MyMod.esp|800 MyMod.esp|800 MyMod:Config.iMode 1 -> 2" });
    CHECK(hours == std::vector<float>{ 18.0f, 8.5f });
}

TEST_CASE("ConfigSpecStore: a file takes its path from a state of its mod's client while that is a file of its format under its folders")
{
    constexpr auto OTHER_FILE = R"(Mods_Config\MyMod\MyMod_Other.toml)";
    Session session;
    session.mod.files.at("main").pathFromClientState = "configFile";
    std::optional<model::Value> answer;
    std::vector<std::string> told;
    session.store.clientCalls().addClient("MyMod",
        { .state =
                [&answer](const std::string_view name) {
                    return name == "configFile" ? answer : std::nullopt;
                },
            .onSaved =
                [&told](const model::SaveBatch& batch) {
                    for (const auto& file : batch.files) {
                        told.push_back(file.path);
                    }
                } });
    session.myGames.write(OTHER_FILE, "[Main]\r\nfScale = 1.4\r\n");
    const auto other = (session.myGames.path / OTHER_FILE).string();

    // the client answers the file it loaded, in any letter case
    answer = common::str_tolower(other);
    session.open(MOD_TOML);
    CHECK(session.value(1).value == model::Value(1.4f));
    CHECK(session.store.set(session.setting(1), 1.3f));
    REQUIRE(session.store.save());
    CHECK(readFile(session.myGames.path / OTHER_FILE) == "[Main]\r\nfScale = 1.3\r\n");
    CHECK(readFile(session.myGames.path / MOD_FILE) == MOD_TOML);
    REQUIRE(told.size() == 1);
    CHECK(common::str_tolower(told[0]) == common::str_tolower(other));

    // its declared path with no answer, a path outside its folders, and one of another format
    const auto declared = [&session] {
        session.store.close();
        session.store.open(session.mod);
        return session.value(1).value == model::Value(1.25f);
    };
    answer.reset();
    CHECK(declared());
    answer = (session.data.path.parent_path() / "MyMod.toml").string();
    CHECK(declared());
    answer = (session.myGames.path / R"(Mods_Config\MyMod\MyMod.ini)").string();
    CHECK(declared());
    answer = "Mods_Config/MyMod/MyMod_Other.toml";
    CHECK(declared());

    // it is asked again as the values are read again
    answer = other;
    session.store.readAgain();
    CHECK(session.value(1).value == model::Value(1.3f));
}

TEST_CASE("ConfigSpecStore: a Save that wrote something sends its F4SE message to each F4SE plugin of its config spec, after the client's batch")
{
    TestGame game;
    Session session;
    session.mod.f4sePlugins = { "MyPlugin", "F4VRBody" };
    // how many messages were sent as the client is told the batch
    std::vector<std::size_t> sentAtBatch;
    session.store.clientCalls().addClient("MyMod", { .onSaved = [&](const model::SaveBatch&) {
        sentAtBatch.push_back(game.actions.savedMessages.size());
    } });
    session.open(MOD_TOML);

    REQUIRE(session.store.save());
    CHECK(game.actions.savedMessages.empty());

    CHECK(session.store.set(session.setting(1), 1.4f));
    REQUIRE(session.store.save());
    CHECK(game.actions.savedMessages == std::vector<std::string>{ "MyMod > MyPlugin", "MyMod > F4VRBody" });
    CHECK(sentAtBatch == std::vector<std::size_t>{ 0 });
}

TEST_CASE("ConfigSpecStore: a Save tells the client once what it wrote, of any applies, and no Discard of a preview it wrote")
{
    TestGame game;
    Session session(R"(
        { "type": "switcher", "valueType": "bool", "text": "Enable", "default": true, "storage": { "path": ["Main", "bEnable"] } },
        { "type": "slider", "valueType": "float", "text": "Scale", "default": 1.0, "applies": "save", "valueOptions": { "min": 0.5, "max": 1.5, "step": 0.05 },
          "storage": { "path": ["Main", "fScale"] } },
        { "type": "switcher", "valueType": "bool", "text": "Automation", "default": true, "applies": "restart", "storage": { "global": "MyMod.esp|801" } }
    )");
    ToldChanges told(session.store);
    session.open("[Main]\r\nbEnable = false\r\nfScale = 1.25\r\n");

    // one that is not live is not previewed
    CHECK(session.store.set(session.setting(1), 1.4f));
    CHECK(told.lines.empty());
    CHECK(session.store.set(session.setting(0), true));
    CHECK(session.store.set(session.setting(2), true));
    REQUIRE(session.store.save());
    CHECK(told.saves == std::vector<std::string>{ "Save 1: main",
                            "  bEnable:Main main:Main.bEnable false -> true",
                            "  fScale:Main main:Main.fScale 1.25 -> 1.4",
                            "  MyMod.esp|801 MyMod.esp|801 false -> true" });
    CHECK(told.filePath.ends_with(R"(Mods_Config\MyMod\MyMod.toml)"));

    // the preview it wrote ends with it, with no Discard
    session.store.close();
    CHECK(told.lines == std::vector<std::string>{ "Preview bEnable:Main main:Main.bEnable false -> true" });

    // the next Save of the mod counts on, one of the game alone writes no file, and one that writes nothing tells nothing
    session.store.open(session.mod);
    told.saves.clear();
    CHECK(session.store.set(session.setting(2), false));
    REQUIRE(session.store.save());
    REQUIRE(session.store.save());
    CHECK(told.saves == std::vector<std::string>{ "Save 2:", "  MyMod.esp|801 MyMod.esp|801 true -> false" });
}

TEST_CASE("ConfigSpecStore: the changes that take effect after a restart are counted while pending, by the last Save, and for the mod until the game is restarted")
{
    TestGame game;
    Session session(APPLIES_SETTINGS);
    session.open("[Main]\r\n");
    CHECK_FALSE(session.store.lastSave());
    CHECK(session.store.savedRestartCount(session.mod) == 0);

    CHECK(session.store.set(session.setting(1), false));
    CHECK(session.store.set(session.setting(2), false));
    CHECK(session.store.dirtyCount() == 2);
    CHECK(session.store.dirtyRestartCount() == 1);

    CHECK(session.store.save());
    CHECK(session.store.dirtyRestartCount() == 0);
    CHECK(session.store.lastSave() == 1u);
    CHECK(session.store.savedRestartCount(session.mod) == 1);

    // the next change ends the last Save, and a Save of no restart says none
    CHECK(session.store.set(session.setting(1), true));
    CHECK_FALSE(session.store.lastSave());
    CHECK(session.store.save());
    CHECK(session.store.lastSave() == 0u);

    // a setting that is set back and saved again counts once, and still counts
    CHECK(session.store.set(session.setting(2), true));
    CHECK(session.store.set(session.setting(3), false));
    CHECK(session.store.save());
    CHECK(session.store.lastSave() == 2u);
    CHECK(session.store.savedRestartCount(session.mod) == 2);

    // Discard ends the last Save, and so does the end of the session, which keeps the mod's count
    CHECK(session.store.set(session.setting(1), false));
    session.store.discard();
    CHECK_FALSE(session.store.lastSave());
    CHECK(session.store.set(session.setting(1), false));
    CHECK(session.store.save());
    session.store.close();
    CHECK_FALSE(session.store.lastSave());
    CHECK(session.store.savedRestartCount(session.mod) == 2);
    session.store.open(session.mod);
    CHECK(session.store.savedRestartCount(session.mod) == 2);

    // a mod of another name has none
    auto other = session.mod;
    other.name = "OtherMod";
    CHECK(session.store.savedRestartCount(other) == 0);
}

TEST_CASE("ConfigSpecStore: a Save that fails is no last Save, and counts no change for a restart")
{
    TestGame game;
    Session session(APPLIES_SETTINGS);
    session.open("[Main]\r\n");
    CHECK(session.store.set(session.setting(2), false));
    {
        const test_files::ReadOnly readOnly(session.myGames.path / MOD_FILE);
        CHECK_FALSE(session.store.save());
    }
    CHECK_FALSE(session.store.lastSave());
    CHECK(session.store.savedRestartCount(session.mod) == 0);
    CHECK(session.store.dirtyRestartCount() == 1);
}

TEST_CASE("ConfigSpecStore: every setting of the mod's own config spec is available, valid and clean with the shipped config")
{
    const auto mods = spec::loadConfigSpecs(MODS_CONFIG_SPECS_DIR);
    REQUIRE(mods.size() == 1);
    const test_files::TempFolder myGames;
    const auto folder = myGames.path / "Mods_Config" / "ModsConfigFO4VR";
    std::filesystem::create_directories(folder);
    std::filesystem::copy_file(MODS_CONFIG_SHIPPED_CONFIG, folder / "ModsConfigFO4VR.toml");
    // the menu gives the options of its Config spec setting: none, and the config specs it lists
    ConfigSpecStore store({}, myGames.path);
    store.clientCalls().addClient("ModsConfigFO4VR",
        test_clients::TestClient{ .options = { { "developerConfigSpec",
                                      [](const model::Setting&) {
                                          return std::vector<model::Option>{ { .value = std::string(), .text = "None" }, { .value = "MyMod"s, .text = "My Mod" } };
                                      } } } }
            .calls());
    store.open(mods[0]);

    std::size_t checked = 0;
    const auto check = [&store, &checked](const std::vector<model::Item>& items, const auto& self) -> void {
        for (const auto& item : items) {
            if (const auto* group = std::get_if<model::Group>(&item)) {
                self(group->items, self);
            } else if (const auto* setting = std::get_if<model::Setting>(&item)) {
                const auto value = store.value(*setting);
                INFO(setting->id << ": " << value.reason);
                CHECK(value.available);
                CHECK_FALSE(value.invalid);
                CHECK(value.value == *store.defaultValue(*setting));
                ++checked;
            }
        }
    };
    check(mods[0].frontPage.items, check);
    for (const auto& page : mods[0].pages) {
        check(page.items, check);
    }
    CHECK(checked >= 10);
    CHECK(store.dirtyCount() == 0);
}

TEST_CASE("ConfigSpecStore: a choice takes its options from its mod's client as its mod is selected, and is not available with no callback")
{
    Session session(R"(
        { "type": "dropdown", "valueType": "string", "id": "ConfigSpec", "text": "Config spec", "default": "", "valueOptions": { "listFromClientCallback": true },
          "storage": { "path": ["Main", "sConfigSpec"] } },
        { "type": "stepper", "valueType": "int", "text": "Other", "default": 0, "valueOptions": { "listFromClientCallback": true },
          "storage": { "path": ["Main", "iOther"] } }
    )");
    ConfigSpecStore store(session.data.path, session.myGames.path);
    int calls = 0;
    // its mod and its id in another case
    store.clientCalls().addClient("mymod",
        test_clients::TestClient{ .options = { { "configspec",
                                      [&calls](const model::Setting& setting) {
                                          ++calls;
                                          CHECK(setting.text == "Config spec");
                                          return std::vector<model::Option>{ { .value = std::string(), .text = "None" }, { .value = "FRIK"s, .text = "FRIK" } };
                                      } } } }
            .calls());
    session.myGames.write(MOD_FILE, "[Main]\r\nsConfigSpec = \"FRIK\"\r\niOther = 0\r\n");
    store.open(session.mod);
    CHECK(calls == 1);
    CHECK(store.options(session.setting(0)).size() == 2);
    CHECK(store.value(session.setting(0)).value == model::Value("FRIK"s));
    CHECK_FALSE(store.value(session.setting(0)).invalid);

    // a value that is none of them is invalid, as one of a game's list is
    CHECK(store.set(session.setting(0), "Other"s));
    CHECK(store.value(session.setting(0)).invalid);

    // a setting no callback is registered for has none, and is not available
    const auto other = store.value(session.setting(1));
    CHECK_FALSE(other.available);
    CHECK(other.reason == "Not available: its mod's client gives no options for it.");

    // a store with no callbacks gives none
    ConfigSpecStore alone(session.data.path, session.myGames.path);
    alone.open(session.mod);
    CHECK_FALSE(alone.value(session.setting(0)).available);
}

TEST_CASE("ConfigSpecStore: values read again take a clean setting's new value, keep a pending one, and keep the baseline of a preview")
{
    test_globals::TestGlobals globals;
    globals.values[{ "MyMod.esp", 0x800 }] = 1.0f;
    Session session(R"(
        { "type": "switcher", "valueType": "bool", "text": "Clean", "default": true, "storage": { "path": ["Main", "bClean"] } },
        { "type": "switcher", "valueType": "bool", "text": "Pending", "default": true, "storage": { "path": ["Main", "bPending"] } },
        { "type": "switcher", "valueType": "bool", "text": "Made the same", "default": true, "storage": { "path": ["Main", "bSame"] } },
        { "type": "slider", "valueType": "float", "text": "Previewed", "default": 1.0, "valueOptions": { "min": 0.0, "max": 5.0, "step": 0.5 },
          "storage": { "global": "MyMod.esp|800" } }
    )");
    session.open("[Main]\r\nbClean = false\r\nbPending = false\r\nbSame = false\r\n");
    CHECK(session.store.set(session.setting(1), true));
    CHECK(session.store.set(session.setting(2), true));
    // a live global has its pending value in the game, as a preview
    CHECK(session.store.set(session.setting(3), 2.5f));
    CHECK(globals.values.at({ "MyMod.esp", 0x800 }) == 2.5f);

    // the mod's own UI writes its file
    session.myGames.write(MOD_FILE, "[Main]\r\nbClean = true\r\nbPending = false\r\nbSame = true\r\n");
    session.store.readAgain();
    CHECK(session.value(0).value == model::Value(true));
    CHECK_FALSE(session.store.isDirty(session.setting(0)));
    CHECK(session.value(1).value == model::Value(true));
    CHECK(session.store.isDirty(session.setting(1)));
    // the player's value is the file's now
    CHECK(session.value(2).value == model::Value(true));
    CHECK_FALSE(session.store.isDirty(session.setting(2)));
    // the game has the preview, so the baseline is the one before it, which Discard writes back
    CHECK(session.value(3).value == model::Value(2.5f));
    CHECK(session.store.isDirty(session.setting(3)));
    session.store.discard();
    CHECK(globals.values.at({ "MyMod.esp", 0x800 }) == 1.0f);
    CHECK(session.store.dirtyCount() == 0);
}

namespace
{
    // a setting kept in a state of the mod's client, of each kind: a live one, one that waits for a Save, and a read-only one
    constexpr auto STATE_SETTINGS = R"(
        { "type": "dropdown", "valueType": "int", "text": "Pip-Boy", "default": 0, "valueOptions": { "options": ["Wrist", "Projected"] },
          "storage": { "clientState": "pipboyType" } },
        { "type": "slider", "valueType": "float", "text": "Scale", "default": 1.0, "applies": "save", "valueOptions": { "min": 0.5, "max": 1.5, "step": 0.05 },
          "storage": { "clientState": "scale" } },
        { "type": "slider", "valueType": "float", "text": "Height", "readOnly": true, "storage": { "clientState": "height" } }
    )";

    /**
     * The client of the mod `MyMod`, whose states are the values a test gives it, and which keeps the changes and the
     * Saves it is told, each as a line, as `ToldChanges` does.
     */
    struct StateClient
    {
        std::map<std::string, model::Value, std::less<>> states;
        std::vector<std::string> told;

        explicit StateClient(ConfigSpecStore& store)
        {
            store.clientCalls().addClient("MyMod",
                { .state =
                        [this](const std::string_view name) {
                            const auto found = states.find(name);
                            return found != states.end() ? std::optional(found->second) : std::nullopt;
                        },
                    .onChange =
                        [this](const model::SettingChange& change) {
                            told.push_back(std::format("{} {} {} -> {}",
                                change.phase == model::ChangePhase::Discard ? "Discard" : "Preview",
                                settingText(change.setting),
                                textOf(change.previous),
                                textOf(change.value)));
                        },
                    .onSaved =
                        [this](const model::SaveBatch& batch) {
                            told.push_back(std::format("Save {}", batch.revision));
                            for (const auto& saved : batch.settings) {
                                told.push_back(std::format("  {} {} -> {}", settingText(saved.setting), textOf(saved.before), textOf(saved.value)));
                            }
                        } });
        }
    };
}

TEST_CASE("ConfigSpecStore: a setting of a state of its mod's client has the client's answer as its baseline, and one it does not answer is not available")
{
    Session session(STATE_SETTINGS);
    session.open(MOD_TOML);
    CHECK(session.value(0).reason == "Not available: needs My Mod's DLL, which is not loaded.");
    CHECK_FALSE(session.value(0).available);
    CHECK_FALSE(session.store.set(session.setting(0), 1));

    StateClient client(session.store);
    client.states = { { "pipboyType", 1 }, { "scale", 1.25f }, { "height", 1.8f } };
    session.store.open(session.mod);
    CHECK(session.value(0).value == model::Value(1));
    CHECK(session.value(1).value == model::Value(1.25f));
    CHECK(session.value(2).value == model::Value(1.8f));
    for (std::size_t row = 0; row < 3; ++row) {
        CHECK(session.value(row).available);
        CHECK_FALSE(session.value(row).invalid);
        CHECK_FALSE(session.store.isDirty(session.setting(row)));
    }
    // a read-only one is not set
    CHECK_FALSE(session.store.set(session.setting(2), 1.7f));

    // a whole number is a number, and an answer that is no value of its setting is invalid, as one of another type,
    // which a read-only one does not show
    client.states = { { "pipboyType", 5 }, { "scale", 1 }, { "height", "tall"s } };
    session.store.open(session.mod);
    CHECK(session.value(0).invalid);
    CHECK(session.value(0).reason.starts_with("Invalid: the state pipboyType is 5 in its mod's DLL, which "));
    CHECK(session.value(1).value == model::Value(1.0f));
    CHECK_FALSE(session.value(1).invalid);
    CHECK(session.value(2).reason == "Not available: the state height of its mod's client is \"tall\", which is no value of the setting's valueType.");
    client.states["scale"] = "big"s;
    session.store.open(session.mod);
    CHECK(session.value(1).reason == "Invalid: the state scale of its mod's client is \"big\", which is no value of the setting's valueType.");

    // one the client does not answer, and every one of a client that is cut off
    client.states.erase("pipboyType");
    session.store.open(session.mod);
    CHECK(session.value(0).reason == "Not available: My Mod's DLL does not answer its state 'pipboyType'.");
    session.store.clientCalls().cutOff("MyMod");
    session.store.open(session.mod);
    CHECK(session.value(1).reason == "Not available: its mod's client failed, and is cut off until the game is restarted.");
    session.store.close();
}

TEST_CASE("ConfigSpecStore: a setting of a state is written by its client alone, as a live one is previewed and discarded, and as a Save tells it")
{
    Session session(STATE_SETTINGS);
    StateClient client(session.store);
    client.states = { { "pipboyType", 0 }, { "scale", 1.0f }, { "height", 1.8f } };
    session.open(MOD_TOML);

    // a live one is told as it is set and set back, and one that is not live waits for a Save
    CHECK(session.store.set(session.setting(0), 1));
    CHECK(session.store.set(session.setting(1), 1.25f));
    session.store.discard(session.setting(0));
    CHECK(session.store.set(session.setting(0), 1));
    CHECK(client.told ==
          std::vector<std::string>{ "Preview pipboyType state pipboyType 0 -> 1", "Discard pipboyType state pipboyType 1 -> 0", "Preview pipboyType state pipboyType 0 -> 1" });

    // a Save tells the client what it wrote, with no Discard of the preview, and writes no file
    client.told.clear();
    REQUIRE(session.store.save());
    CHECK(client.told == std::vector<std::string>{ "Save 1", "  pipboyType state pipboyType 0 -> 1", "  scale state scale 1 -> 1.25" });
    CHECK(readFile(session.myGames.path / MOD_FILE) == MOD_TOML);
    CHECK(session.store.dirtyCount() == 0);
    CHECK(session.value(0).value == model::Value(1));

    // a Save with no client to tell writes nothing, and keeps the change dirty
    CHECK(session.store.set(session.setting(1), 1.5f));
    session.store.clientCalls().cutOff("MyMod");
    const auto saved = session.store.save();
    REQUIRE_FALSE(saved);
    CHECK(saved.error() == "My Mod's DLL is not loaded, or its client is cut off");
    CHECK(session.store.isDirty(session.setting(1)));
    session.store.discard();
}

TEST_CASE("ConfigSpecStore: a state read again keeps the baseline of its preview, and a clean one takes the client's new value")
{
    Session session(STATE_SETTINGS);
    StateClient client(session.store);
    client.states = { { "pipboyType", 0 }, { "scale", 1.0f }, { "height", 1.8f } };
    session.open(MOD_TOML);
    CHECK(session.store.set(session.setting(0), 1));

    // the client applied the preview, and the mod changed another value itself
    client.states["pipboyType"] = 1;
    client.states["scale"] = 1.4f;
    session.store.readAgain();
    CHECK(session.value(1).value == model::Value(1.4f));
    CHECK_FALSE(session.store.isDirty(session.setting(1)));
    CHECK(session.value(0).value == model::Value(1));
    CHECK(session.store.isDirty(session.setting(0)));

    // its Discard gives the client the value before the preview
    client.told.clear();
    session.store.discard();
    CHECK(client.told == std::vector<std::string>{ "Discard pipboyType state pipboyType 1 -> 0" });
}

namespace
{
    // a choice of the gobos of a folder of Data, and one of a folder of My Games with the option that is no file
    constexpr auto FILE_SETTINGS = R"(
        { "type": "dropdown", "valueType": "string", "text": "Gobo", "default": "data\\Textures\\MyMod\\Gobos\\2_Rings.DDS",
          "valueOptions": { "listFromFolder": { "path": "Textures/MyMod/Gobos", "mask": "*.dds" } }, "storage": { "path": ["Main", "sGobo"] } },
        { "type": "dropdown", "valueType": "string", "text": "Preset", "default": "",
          "valueOptions": { "listFromFolder": { "root": "myGames", "path": "Presets", "noneText": "No preset" } }, "storage": { "path": ["Main", "sPreset"] } }
    )";
}

TEST_CASE("ConfigSpecStore: a choice of files has its folder's files, matches a value in any spelling, and writes a pick in its default's form")
{
    Session session(FILE_SETTINGS);
    session.data.write(R"(Textures\MyMod\Gobos\1_Solid.DDS)", "");
    session.data.write(R"(Textures\MyMod\Gobos\2_Rings.DDS)", "");
    session.data.write(R"(Textures\MyMod\Gobos\readme.txt)", "");
    session.open("[Main]\r\nsGobo = 'Textures/MyMod/Gobos/1_solid.dds'\r\n");
    const auto& gobo = session.setting(0);
    const auto& options = session.store.options(gobo);
    REQUIRE(options.size() == 2);
    CHECK(options[0] == model::Option{ .value = R"(data\Textures\MyMod\Gobos\1_Solid.DDS)"s, .text = "1_Solid.DDS" });
    CHECK(options[1].text == "2_Rings.DDS");

    // the file's spelling is its option's, and is clean
    CHECK(session.value(0).value == options[0].value);
    CHECK_FALSE(session.value(0).invalid);
    CHECK(session.store.dirtyCount() == 0);
    CHECK(session.store.defaultValue(gobo) == options[1].value);

    CHECK(session.store.set(gobo, options[1].value));
    REQUIRE(session.store.save());
    spec::TomlFile file(session.myGames.path / MOD_FILE);
    REQUIRE(file.read());
    CHECK(file.value(std::vector<std::string>{ "Main", "sGobo" }, model::StoredType::String) == model::Value(R"(data\Textures\MyMod\Gobos\2_Rings.DDS)"s));

    // a folder of My Games with no files has the option that is no file alone
    CHECK(session.store.options(session.setting(1)) == std::vector<model::Option>{ { .value = ""s, .text = "No preset" } });
    CHECK(session.value(1).available);

    // it is listed again as the values are read again
    session.myGames.write(R"(Presets\Mine.json)", "");
    session.store.readAgain();
    CHECK(session.store.options(session.setting(1)).size() == 2);
}

TEST_CASE("ConfigSpecStore: a value of a choice of files that its folder does not have is invalid, and one whose folder has no files is not available")
{
    Session session(FILE_SETTINGS);
    session.data.write(R"(Textures\MyMod\Gobos\2_Rings.DDS)", "");
    session.open("[Main]\r\nsGobo = 'data\\Textures\\Effects\\Gobos\\Vanilla.dds'\r\n");
    CHECK(session.value(0).invalid);
    CHECK(session.value(0).reason == R"(Invalid: Main.sGobo is "data\Textures\Effects\Gobos\Vanilla.dds" in MyMod.toml, which is not in Textures\MyMod\Gobos.)");

    // a name the folder does not have is invalid as the player sets it
    CHECK(session.store.set(session.setting(0), R"(data\Textures\MyMod\Gobos\9_Laser.dds)"s));
    CHECK(session.value(0).reason == R"(Invalid: "data\Textures\MyMod\Gobos\9_Laser.dds" is not in Textures\MyMod\Gobos.)");
    session.store.discard();

    Session empty(FILE_SETTINGS);
    empty.open("");
    CHECK_FALSE(empty.value(0).available);
    CHECK(empty.value(0).reason == R"(Not available: no *.dds files in Textures\MyMod\Gobos.)");
}

TEST_CASE("ConfigSpecStore: the client is told the id of the row the ray is on and of each group around it, innermost first, as it changes, and none as the mod is left")
{
    const auto mod = spec::loadConfigSpec(R"({
        "minSchemaVersion": 1, "modName": "MyMod",
        "files": { "main": { "format": "toml", "root": "myGames", "path": "MyMod.toml" } },
        "defaultFile": "main",
        "content": [ { "type": "text", "text": "Front" } ],
        "pages": [ { "pageDisplayName": "Outer", "pages": [ { "pageDisplayName": "Inner", "content": [
            { "type": "section", "id": "holsters", "text": "Holsters", "content": [
                { "type": "section", "text": "No id", "content": [
                    { "type": "switcher", "text": "Left", "default": false, "storage": { "path": ["Holsters", "bLeft"] } },
                    { "type": "text", "text": "A note" }
                ] }
            ] },
            { "type": "cf.activationSphere", "text": "Body grab", "storage": { "path": ["BodyGrab"] } }
        ] } ] } ]
    })");
    REQUIRE(mod);
    std::vector<std::vector<std::string>> told;
    ConfigSpecStore store({}, {});
    store.clientCalls().addClient("MyMod", test_clients::TestClient{ .onPointed = [&told](const std::vector<std::string>& ids) {
        told.push_back(ids);
    } }.calls());
    store.open(*mod);

    const auto& items = mod->pages[0].pages[0].items;
    const auto& holsters = items[0];
    const auto& noId = std::get<model::Group>(holsters).items[0];
    const auto& left = std::get<model::Group>(noId).items[0];
    const auto& note = std::get<model::Group>(noId).items[1];
    // the sphere's zone is in the section the loader makes of its zone keys
    const model::Item* zone = nullptr;
    const auto findZone = [&zone](const std::vector<model::Item>& found, const auto& self) -> void {
        for (const auto& item : found) {
            if (const auto* group = std::get_if<model::Group>(&item)) {
                self(group->items, self);
            } else if (const auto* setting = std::get_if<model::Setting>(&item); setting && setting->id == "tZone:BodyGrab") {
                zone = &item;
            }
        }
    };
    findZone(std::get<model::Group>(items[1]).items, findZone);
    REQUIRE(zone);
    const model::Item notOfTheMod = model::Text{};

    store.showPointed(&left);
    store.showPointed(&note);
    // the heading of the group is in no other group with an id, so it tells what the row in it told: nothing new
    store.showPointed(&holsters);
    store.showPointed(zone);
    store.showPointed(&notOfTheMod);
    store.showPointed(nullptr);
    store.showPointed(zone);
    store.close();
    CHECK(told ==
          std::vector<std::vector<std::string>>{ { "bLeft:Holsters", "holsters" }, { "holsters" }, { "tZone:BodyGrab", "BodyGrab" }, {}, { "tZone:BodyGrab", "BodyGrab" }, {} });
}

namespace
{
    // two choices of a text, kept at the root of the TOML file
    constexpr auto CHOICES = R"(
        { "type": "dropdown", "valueType": "string", "text": "First", "default": "a",
          "valueOptions": { "options": [ { "value": "a", "text": "A" }, { "value": "b", "text": "B" } ] }, "storage": { "path": ["first"] } },
        { "type": "dropdown", "valueType": "string", "text": "Second", "default": "a",
          "valueOptions": { "options": [ { "value": "a", "text": "A" }, { "value": "b", "text": "B" } ] }, "storage": { "path": ["second"] } }
    )";

    /**
     * A session of two choices that keep no value that is their default.
     */
    struct OmitSession : Session
    {
        OmitSession()
            : Session(CHOICES)
        {
            for (auto& item : mod.frontPage.items) {
                std::get<model::Setting>(item).omitDefault = true;
            }
        }
    };
}

TEST_CASE("ConfigSpecStore: a Save keeps no value that is the default of a setting that keeps none: its key is removed")
{
    OmitSession s;
    s.open("# choices\nfirst = \"b\"\nsecond = \"b\"\n");
    CHECK(s.value(0).value == model::Value(std::string("b")));

    // set to its default, it is dirty, and the Save removes its key
    REQUIRE(s.store.set(s.setting(0), std::string("a")));
    CHECK(s.store.isDirty(s.setting(0)));
    REQUIRE(s.store.save());

    CHECK(readFile(s.myGames.path / MOD_FILE) == "# choices\nsecond = \"b\"\n");
    CHECK(!s.store.isDirty(s.setting(0)));
    CHECK(s.value(0).value == model::Value(std::string("a")));
    CHECK(s.value(0).available);
    CHECK(!s.value(0).invalid);

    // and another value is written again
    REQUIRE(s.store.set(s.setting(0), std::string("b")));
    REQUIRE(s.store.save());
    CHECK(readFile(s.myGames.path / MOD_FILE) == "# choices\nsecond = \"b\"\nfirst = \"b\"\n");
}

TEST_CASE("ConfigSpecStore: a Save removes the key of a clean setting whose file has its default, where it keeps none")
{
    OmitSession s;
    s.open("first = \"a\"\nsecond = \"a\"\n");
    CHECK(s.store.dirtyCount() == 0);

    REQUIRE(s.store.set(s.setting(1), std::string("b")));
    REQUIRE(s.store.save());

    CHECK(readFile(s.myGames.path / MOD_FILE) == "second = \"b\"\n");
    CHECK(s.store.dirtyCount() == 0);
}

TEST_CASE("ConfigSpecStore: a setting that keeps its default has it written, as any setting has")
{
    Session s(CHOICES);
    s.open("first = \"b\"\n");

    REQUIRE(s.store.set(s.setting(0), std::string("a")));
    REQUIRE(s.store.save());

    CHECK(readFile(s.myGames.path / MOD_FILE) == "first = \"a\"\n");
}

TEST_CASE("ConfigSpecStore: a row of the Categories page of the mod's own config spec keeps a mod's category in Categories.toml, and none that is the mod's own")
{
    auto mods = spec::loadConfigSpecs(MODS_CONFIG_SPECS_DIR);
    REQUIRE(mods.size() == 1);
    auto& own = mods[0];
    const auto table = model::readCategoryTable(R"({ "categories": ["Gameplay", "Settlements"], "mods": { "SS2": "Settlements" } })");
    REQUIRE(table);
    const std::vector<model::Mod> listed{ model::Mod{ .name = "SS2", .displayName = "Sim Settlements 2", .frontPage = {}, .pages = {} } };
    REQUIRE(model::addCategoryRows(own, listed, *table));

    const test_files::TempFolder data;
    const test_files::TempFolder myGames;
    const auto file = myGames.path / "Mods_Config" / "ModsConfigFO4VR" / "Categories.toml";
    ConfigSpecStore store{ data.path, myGames.path };
    // the menu's own client gives the names, as the menu does
    const auto names = [&](const model::Setting&) {
        return model::categoryOptions(listed, *table, model::loadCategoryChoices(file));
    };
    store.clientCalls().addClient("ModsConfigFO4VR", test_clients::TestClient{ .options = { { "category.SS2", names }, { "category.ModsConfigFO4VR", names } } }.calls());
    store.open(own);

    const auto page = std::ranges::find(own.pages, model::CATEGORIES_PAGE_ID, &model::Page::id);
    REQUIRE(page != own.pages.end());
    const auto& rows = std::get<model::Group>(page->items.back()).items;
    const auto row = std::ranges::find_if(rows, [](const model::Item& item) {
        return std::get<model::Setting>(item).id == "category.SS2";
    });
    REQUIRE(row != rows.end());
    const auto& ss2 = std::get<model::Setting>(*row);

    // with no file, the mod has the category it has without the player's choice
    auto value = store.value(ss2);
    CHECK(value.available);
    CHECK(!value.invalid);
    CHECK(value.value == model::Value(std::string("Settlements")));
    CHECK(store.options(ss2).size() == 3);

    // another category is kept under the mod's modName
    REQUIRE(store.set(ss2, std::string("Gameplay")));
    REQUIRE(store.save());
    CHECK(model::loadCategoryChoices(file) == model::CategoryChoices{ { "ss2", "Gameplay" } });

    // and so is no category, as an empty text
    REQUIRE(store.set(ss2, std::string()));
    REQUIRE(store.save());
    CHECK(model::loadCategoryChoices(file) == model::CategoryChoices{ { "ss2", "" } });

    // Reset gives the mod's own, which is not kept
    const auto reset = store.defaultValue(ss2);
    REQUIRE(reset);
    CHECK(*reset == model::Value(std::string("Settlements")));
    REQUIRE(store.set(ss2, *reset));
    CHECK(store.isDirty(ss2));
    REQUIRE(store.save());
    CHECK(model::loadCategoryChoices(file).empty());
    CHECK(store.dirtyCount() == 0);
}

TEST_CASE("ConfigSpecStore: a choice with a typed entry has its options read again as it is set, discarded and read again, so its client can offer a pending value")
{
    Session session(R"(
        { "type": "dropdown", "valueType": "string", "id": "first", "text": "First", "default": "", "valueOptions": { "listFromClientCallback": true },
          "storage": { "path": ["Main", "sFirst"] } },
        { "type": "dropdown", "valueType": "string", "id": "second", "text": "Second", "default": "", "valueOptions": { "listFromClientCallback": true },
          "storage": { "path": ["Main", "sSecond"] } }
    )");
    for (auto& item : session.mod.frontPage.items) {
        std::get<model::Setting>(item).valueOptions.typedOption = true;
    }
    auto& store = session.store;
    // the client offers its names and the values of both choices, as the menu offers the pending categories
    const auto names = [&session](const model::Setting&) {
        std::vector<model::Option> options{ { .value = std::string(), .text = "None" }, { .value = "Combat"s, .text = "Combat" } };
        for (const auto* id : { "first", "second" }) {
            const auto stored = session.store.valueById(id);
            const auto* text = std::get_if<std::string>(&stored.value);
            if (text && std::ranges::find(options, model::Value(*text), &model::Option::value) == options.end()) {
                options.push_back({ .value = *text, .text = *text });
            }
        }
        return options;
    };
    store.clientCalls().addClient("MyMod", test_clients::TestClient{ .options = { { "first", names }, { "second", names } } }.calls());
    session.open("[Main]\r\nsFirst = \"Combat\"\r\n");
    CHECK(store.options(session.setting(1)).size() == 2);

    // a typed value is one of the options of both, and so not invalid
    REQUIRE(store.set(session.setting(0), "Mods I Like"s));
    CHECK_FALSE(session.value(0).invalid);
    REQUIRE(store.options(session.setting(1)).size() == 3);
    CHECK(store.options(session.setting(1)).back().text == "Mods I Like");

    // the values read again keep it
    store.readAgain();
    CHECK(store.options(session.setting(1)).size() == 3);
    CHECK(session.value(0).value == model::Value("Mods I Like"s));
    CHECK_FALSE(session.value(0).invalid);

    // discarded, it is offered no more, and neither is one after a Discard of all
    store.discard(session.setting(0));
    CHECK(store.options(session.setting(1)).size() == 2);
    REQUIRE(store.set(session.setting(1), "Another"s));
    CHECK(store.options(session.setting(0)).size() == 3);
    store.discard();
    CHECK(store.options(session.setting(0)).size() == 2);
}

TEST_CASE("ConfigSpecStore: a name typed in a row of the Categories page is offered by the others at once, as the menu's own client gives them")
{
    auto mods = spec::loadConfigSpecs(MODS_CONFIG_SPECS_DIR);
    REQUIRE(mods.size() == 1);
    auto& own = mods[0];
    const auto table = model::readCategoryTable(R"({ "categories": ["Gameplay", "Settlements"], "mods": { "SS2": "Settlements" } })");
    REQUIRE(table);
    const std::vector<model::Mod> listed{ model::Mod{ .name = "SS2", .displayName = "Sim Settlements 2", .frontPage = {}, .pages = {} } };
    REQUIRE(model::addCategoryRows(own, listed, *table));

    const test_files::TempFolder data;
    const test_files::TempFolder myGames;
    ConfigSpecStore store{ data.path, myGames.path };
    const auto names = [&](const model::Setting&) {
        return model::categoryOptions(listed, *table, model::withPendingChoices({}, own, store));
    };
    store.clientCalls().addClient("ModsConfigFO4VR", test_clients::TestClient{ .options = { { "category.SS2", names }, { "category.ModsConfigFO4VR", names } } }.calls());
    store.open(own);

    const auto& rows = std::get<model::Group>(std::ranges::find(own.pages, model::CATEGORIES_PAGE_ID, &model::Page::id)->items.back()).items;
    const auto& first = std::get<model::Setting>(rows.at(0));
    const auto& second = std::get<model::Setting>(rows.at(1));
    REQUIRE(first.id == "category.ModsConfigFO4VR");
    REQUIRE(second.id == "category.SS2");
    CHECK(model::withPendingChoices({}, own, store).empty());

    REQUIRE(store.set(second, "Mods I Like"s));
    CHECK(model::withPendingChoices({ { "ss2", "Gameplay" } }, own, store) == model::CategoryChoices{ { "ss2", "Mods I Like" } });
    CHECK_FALSE(store.value(second).invalid);
    const auto texts = store.options(first) | std::views::transform(&model::Option::text) | std::ranges::to<std::vector>();
    CHECK(texts == std::vector<std::string>{ "Gameplay", "Mods I Like", "Settlements", "No category" });
}

namespace
{
    // a choice whose options set the three settings after it, of which one is a transform: Low is their defaults
    constexpr auto QUALITY = R"(
        { "type": "stepper", "valueType": "string", "id": "quality", "text": "Quality", "valueOptions": { "options": [
            { "value": "low", "text": "Low", "sets": { "fScale": 1.0, "sPlace": "hip" } },
            { "value": "high", "text": "High", "sets": { "fScale": 1.5, "sPlace": "chest" } },
            { "value": "far", "text": "Far", "sets": { "zone": { "pos": [0, 0, 12] } } } ] } },
        { "type": "slider", "valueType": "float", "id": "fScale", "text": "Scale", "default": 1.0, "valueOptions": { "min": 0.5, "max": 1.5, "step": 0.05 },
          "storage": { "path": ["Main", "fScale"] } },
        { "type": "dropdown", "valueType": "string", "id": "sPlace", "text": "Place", "default": "hip",
          "valueOptions": { "options": [ { "value": "hip", "text": "Hip" }, { "value": "chest", "text": "Chest" } ] }, "storage": { "path": ["Main", "sPlace"] } },
        { "type": "cf.transform", "id": "zone", "text": "Zone", "default": { "scale": 1.0 }, "storage": { "path": ["Main", "tZone"] } })";
}

TEST_CASE("ConfigSpecStore: a choice that sets other settings is the first option whose values they have, and none while no option's are")
{
    Session session(QUALITY);
    session.open(
        "[Main]\r\n"
        "fScale = 1.5\r\n"
        "sPlace = \"chest\"\r\n"
        "tZone = { scale = 2.0 }\r\n");
    const auto& quality = session.setting(0);
    CHECK(session.value(0).value == model::Value("high"s));
    CHECK_FALSE(session.store.isDirty(quality));
    // its default is the option of their defaults
    CHECK(session.store.defaultValue(quality) == model::Value("low"s));

    // a setting changed on its own row can leave it with none, which says why
    CHECK(session.store.set(session.setting(1), 0.75f));
    const auto blank = session.value(0);
    CHECK(blank.available);
    CHECK_FALSE(blank.invalid);
    CHECK(blank.value == model::Value(""s));
    CHECK(blank.reason == "None of its options: their settings are set otherwise.");
    CHECK(session.store.isDirty(quality));
    CHECK(session.store.savedValue(quality) == model::Value("high"s));
}

TEST_CASE("ConfigSpecStore: a pick of a choice that sets other settings sets each, of a transform the parts it names, and its Discard takes them back")
{
    Session session(QUALITY);
    session.open(
        "[Main]\r\n"
        "fScale = 1.5\r\n"
        "sPlace = \"chest\"\r\n"
        "tZone = { scale = 2.0 }\r\n");
    const auto& quality = session.setting(0);
    CHECK(session.store.set(quality, "low"s));
    CHECK(session.value(1).value == model::Value(1.0f));
    CHECK(session.value(2).value == model::Value("hip"s));
    CHECK(session.value(0).value == model::Value("low"s));
    CHECK(session.store.isDirty(quality));
    CHECK(session.store.savedValue(quality) == model::Value("high"s));

    // the settings count, and the choice does not
    CHECK(session.store.dirtyCount() == 2);
    CHECK(model::unsavedChanges(session.mod.frontPage, session.store, false) == 2);

    // a transform keeps the parts the option does not name; the first option whose values the settings have is the value
    CHECK(session.store.set(quality, "far"s));
    CHECK(session.value(3).value == model::Value("{ pos = [0.0, 0.0, 12.0], scale = 2.0 }"s));
    CHECK(session.value(0).value == model::Value("low"s));

    // no option is no pick
    CHECK_FALSE(session.store.set(quality, "medium"s));

    session.store.discard(quality);
    CHECK(session.store.dirtyCount() == 0);
    CHECK(session.value(0).value == model::Value("high"s));
    CHECK(session.value(3).value == model::Value("{ scale = 2.0 }"s));

    // a Save writes the settings, and nothing of the choice
    CHECK(session.store.set(quality, "low"s));
    REQUIRE(session.store.save());
    CHECK(readFile(session.myGames.path / MOD_FILE) ==
          "[Main]\r\n"
          "fScale = 1.0\r\n"
          "sPlace = \"hip\"\r\n"
          "tZone = { scale = 2.0 }\r\n");
}

TEST_CASE("ConfigSpecStore: a pick sets an option's alsoSets too, which a player tunes on their own rows and keeps the option, and its Discard takes them back")
{
    Session session(R"(
        { "type": "stepper", "valueType": "string", "id": "quality", "text": "Quality", "valueOptions": { "options": [
            { "value": "low", "text": "Low", "sets": { "fScale": 1.0 }, "alsoSets": { "zone": { "pos": [0, 0, 6] } } },
            { "value": "high", "text": "High", "sets": { "fScale": 1.5 }, "alsoSets": { "zone": { "pos": [0, 0, 12] } } } ] } },
        { "type": "slider", "valueType": "float", "id": "fScale", "text": "Scale", "default": 1.0, "valueOptions": { "min": 0.5, "max": 1.5, "step": 0.05 },
          "storage": { "path": ["Main", "fScale"] } },
        { "type": "cf.transform", "id": "zone", "text": "Zone", "default": { "scale": 1.0 }, "storage": { "path": ["Main", "tZone"] } })");
    session.open(
        "[Main]\r\n"
        "fScale = 1.0\r\n"
        "tZone = { scale = 2.0 }\r\n");
    const auto& quality = session.setting(0);

    // the zone is no option's, and the value is read from the scale alone
    CHECK(session.value(0).value == model::Value("low"s));

    CHECK(session.store.set(quality, "high"s));
    CHECK(session.value(1).value == model::Value(1.5f));
    CHECK(session.value(2).value == model::Value("{ pos = [0.0, 0.0, 12.0], scale = 2.0 }"s));
    CHECK(session.value(0).value == model::Value("high"s));

    // the zone tuned on its own row keeps the option, and the choice is pending with it
    CHECK(session.store.set(session.setting(2), "{ pos = [0.0, 0.0, 10.0], scale = 2.0 }"s));
    CHECK(session.value(0).value == model::Value("high"s));
    CHECK(session.store.isDirty(quality));

    session.store.discard(quality);
    CHECK(session.store.dirtyCount() == 0);
    CHECK(session.value(2).value == model::Value("{ scale = 2.0 }"s));
}

TEST_CASE("ConfigSpecStore: a mod's client hands back a pick of a choice that sets other settings as the player picks one")
{
    Session session(QUALITY);
    session.open("[Main]\r\n");
    REQUIRE(session.store.setPendingValue("MyMod", "quality", "high"s));
    CHECK(session.value(1).value == model::Value(1.5f));
    CHECK(session.value(2).value == model::Value("chest"s));
    CHECK(session.value(0).value == model::Value("high"s));
}
