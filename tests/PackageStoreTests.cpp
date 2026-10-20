#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <format>
#include <fstream>
#include <sstream>
#include <string>
#include <variant>
#include <vector>

#include "TestActions.h"
#include "TestFiles.h"
#include "TestGlobals.h"
#include "TestProperties.h"
#include "common/CommonUtils.h"
#include "package/PackageLoader.h"
#include "package/PackageStore.h"

using namespace mods_config;
using package::PackageStore;

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
     * A package with the given content as its front page, a TOML file `main` in My Games that is its default,
     * an INI file `ini` in Data, and a JSON file `json` in Data.
     */
    model::Mod packageOf(const std::string& content)
    {
        auto mod = package::loadPackage(std::format(R"({{
            "modName": "MyMod", "displayName": "My Mod", "applies": "live",
            "files": {{
                "main": {{ "format": "toml", "root": "myGames", "path": "Mods_Config/MyMod/MyMod.toml" }},
                "ini": {{ "format": "ini", "root": "data", "path": "MyMod.ini" }},
                "json": {{ "format": "json", "root": "data", "path": "MyMod.json" }}
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
     * A package's mod and its store, with a Data folder and a My Games folder of their own, which have no files
     * until a test writes one.
     */
    struct Session
    {
        test_files::TempFolder data;
        test_files::TempFolder myGames;
        model::Mod mod;
        PackageStore store{ data.path, myGames.path };

        explicit Session(const std::string& content = SETTINGS)
            : mod(packageOf(content))
        {}

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
    };
}

TEST_CASE("PackageStore: a value is the file's, in the type of its control, and is clean")
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
    CHECK_FALSE(session.store.savesAtOnce());
    CHECK(session.store.dirtyCount() == 0);

    // a condition names a setting by its id
    CHECK(session.store.valueById("fScale:Main").value == model::Value(1.25f));
    CHECK_FALSE(session.store.valueById("missing").available);
}

TEST_CASE("PackageStore: a value the file does not have is its default, and is clean, as is every value of a file that is not there")
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

TEST_CASE("PackageStore: a set value is pending and dirty, and a value set back to the file's is clean")
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

TEST_CASE("PackageStore: a value the player sets that is not one of its setting is pending and invalid, with the reason")
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

TEST_CASE("PackageStore: a text input of a text has the length its package allows")
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

TEST_CASE("PackageStore: Save writes nothing while a value the player set is invalid")
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

TEST_CASE("PackageStore: a value in the file that is not one of its setting is invalid, with the reason, and is clean")
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

TEST_CASE("PackageStore: a setting of a file that cannot be read, of a file the menu does not read, or of no file is not available, with the reason")
{
    // a game with no globals
    const test_globals::TestGlobals globals;
    Session session(R"(
        { "type": "switcher", "valueType": "bool", "text": "Enable", "default": true, "storage": { "path": ["Main", "bEnable"] } },
        { "type": "switcher", "valueType": "bool", "text": "JSON", "default": true, "storage": { "file": "json", "path": ["Main", "bJson"] } },
        { "type": "switcher", "valueType": "bool", "text": "Global", "default": true, "storage": { "global": "MyMod.esp|800" } },
        { "type": "switcher", "valueType": "int", "text": "Of a number", "default": 1, "storage": { "path": ["Main", "iOn"] } }
    )");
    session.open("[Main\r\nbEnable = true\r\n");
    const auto broken = session.value(0);
    CHECK_FALSE(broken.available);
    CHECK(broken.reason.starts_with("Not available: "));
    CHECK(broken.reason.find("MyMod.toml") != std::string::npos);
    CHECK(session.value(1).reason == "Not available: the menu does not read JSON files yet.");
    CHECK(session.value(2).reason == "Not available: the global MyMod.esp|800 is not in the game.");
    CHECK(session.value(3).reason == "Not available: its control does not show a value of its valueType.");
    for (std::size_t row = 0; row < 4; ++row) {
        CHECK_FALSE(session.value(row).available);
        CHECK_FALSE(session.store.set(session.setting(row), false));
    }
    CHECK(session.store.dirtyCount() == 0);
}

TEST_CASE("PackageStore: a session that ends forgets its pending values, and the next reads the file again")
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

TEST_CASE("PackageStore: Save writes the dirty settings in the types of the file, with its layout kept, and they are clean with the value written")
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

TEST_CASE("PackageStore: Save keeps a value that changed in the file since the session started, but for one the player changed, and adds a key the file does not have")
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

TEST_CASE("PackageStore: a file that is not written right keeps its settings dirty, and Save says so")
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

TEST_CASE("PackageStore: a Save that fails leaves every file as it was, and keeps every setting dirty")
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

TEST_CASE("PackageStore: Save creates a file that is not there, with the folders on the way to it, and with only the settings it changed")
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

TEST_CASE("PackageStore: a setting of an INI file has the file's value or is invalid, and Save writes it with the file's layout kept")
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
    CHECK(session.value(3).value == model::Value("You ; and me"s));
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

TEST_CASE("PackageStore: every setting of the package showcase's INI file is available, valid and clean with the shipped file")
{
    const test_globals::TestGlobals globals;
    const test_properties::TestProperties properties;
    const auto mods = package::loadPackages(MODS_CONFIG_PACKAGE_SHOWCASE_DIR);
    REQUIRE(mods.size() == 1);

    // the showcase's game Data folder, which has its INI file
    const test_files::TempFolder myGames;
    PackageStore store(std::filesystem::path(MODS_CONFIG_PACKAGE_SHOWCASE_DIR).parent_path().parent_path(), myGames.path);
    store.open(mods[0]);

    std::size_t checked = 0;
    const auto checkItems = [&store, &checked](const std::vector<model::Item>& items, const auto& self) -> void {
        for (const auto& item : items) {
            if (const auto* group = std::get_if<model::Group>(&item)) {
                self(group->items, self);
            } else if (const auto* setting = std::get_if<model::Setting>(&item)) {
                const auto* storage = std::get_if<model::FileStorage>(&setting->storage);
                if (!storage || storage->file != "ini") {
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

TEST_CASE("PackageStore: Discard takes back every pending value, and writes nothing")
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

TEST_CASE("PackageStore: a setting of a global or a property has the game's value as its baseline, and one that is not in the game is not available")
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

TEST_CASE("PackageStore: a live setting of the game is previewed, and has its baseline again as it is set back, discarded, or its session ends")
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

TEST_CASE("PackageStore: Save writes the game's values after the files, then runs the action of each setting it wrote, with its value")
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

    // in the order of the package, with the value in its type
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

TEST_CASE("PackageStore: a Save whose file cannot be written writes nothing into the game")
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

TEST_CASE("PackageStore: the default of a setting is its package's, in the type of its control")
{
    Session session;
    session.open(MOD_TOML);
    CHECK(session.store.defaultValue(session.setting(0)) == model::Value(true));
    CHECK(session.store.defaultValue(session.setting(2)) == model::Value(200.0f));
    CHECK(session.store.defaultValue(session.setting(6)) == model::Value("3"s));
    CHECK_FALSE(session.store.hotkey({}).available);
}

TEST_CASE("PackageStore: a choice reads its options in the game as its mod is selected, and is not available while its list is not in the game")
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

TEST_CASE("PackageStore: a button calls its function or runs its console command at once")
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

    CHECK(session.store.run(action(0)));
    REQUIRE(actions.called.size() == 1);
    CHECK(actions.called[0].function == "ResetAll");
    CHECK(actions.called[0].arguments == std::vector<model::Value>{ 1 });

    CHECK(session.store.run(action(1)));
    CHECK(actions.commands == std::vector{ model::ConsoleCommand{ "tm" } });

    // an action the menu does not run, a row with none, and a function whose form is not in the game
    CHECK_FALSE(session.store.run(action(2)));
    CHECK_FALSE(session.store.run(model::NoAction{}));
    actions.inGame = false;
    CHECK_FALSE(session.store.run(action(0)));
    CHECK(actions.called.size() == 1);

    // nothing waits for a Save
    CHECK(session.store.dirtyCount() == 0);
}

TEST_CASE("PackageStore: a read-only setting has its file's value as it is, none where the file has none, and is not set")
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

TEST_CASE("PackageStore: every setting of the mod's own package is available, valid and clean with the shipped config")
{
    const auto mods = package::loadPackages(MODS_CONFIG_PACKAGES_DIR);
    REQUIRE(mods.size() == 1);
    const test_files::TempFolder myGames;
    const auto folder = myGames.path / "Mods_Config" / "ModsConfigVR";
    std::filesystem::create_directories(folder);
    std::filesystem::copy_file(MODS_CONFIG_SHIPPED_CONFIG, folder / "ModsConfigVR.toml");
    PackageStore store({}, myGames.path);
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
