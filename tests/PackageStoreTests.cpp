#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <format>
#include <fstream>
#include <sstream>
#include <string>
#include <variant>
#include <vector>

#include "TestFiles.h"
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
     * and an INI file `ini` in Data.
     */
    model::Mod packageOf(const std::string& content)
    {
        auto mod = package::loadPackage(std::format(R"({{
            "modName": "MyMod", "displayName": "My Mod", "applies": "live",
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

TEST_CASE("PackageStore: a value that is not one of its setting is not set")
{
    Session session;
    session.open(MOD_TOML);
    // outside the limits of a slider and of a text input of a number
    CHECK_FALSE(session.store.set(session.setting(1), 2.0f));
    CHECK_FALSE(session.store.set(session.setting(6), "10"s));
    // a text that is no number of its field
    CHECK_FALSE(session.store.set(session.setting(6), "4.5"s));
    CHECK_FALSE(session.store.set(session.setting(7), "fast"s));
    CHECK_FALSE(session.store.set(session.setting(7), ""s));
    // no option of a choice, and a value of another type
    CHECK_FALSE(session.store.set(session.setting(3), 2));
    CHECK_FALSE(session.store.set(session.setting(4), "elbow"s));
    CHECK_FALSE(session.store.set(session.setting(0), 1));
    CHECK(session.store.dirtyCount() == 0);
    CHECK(session.value(1).value == model::Value(1.25f));
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
    Session session(R"(
        { "type": "switcher", "valueType": "bool", "text": "Enable", "default": true, "storage": { "path": ["Main", "bEnable"] } },
        { "type": "switcher", "valueType": "bool", "text": "INI", "default": true, "storage": { "file": "ini", "section": "Main", "key": "bIni" } },
        { "type": "switcher", "valueType": "bool", "text": "Global", "default": true, "storage": { "global": "MyMod.esp|800" } },
        { "type": "switcher", "valueType": "int", "text": "Of a number", "default": 1, "storage": { "path": ["Main", "iOn"] } }
    )");
    session.open("[Main\r\nbEnable = true\r\n");
    const auto broken = session.value(0);
    CHECK_FALSE(broken.available);
    CHECK(broken.reason.starts_with("Not available: "));
    CHECK(broken.reason.find("MyMod.toml") != std::string::npos);
    CHECK(session.value(1).reason == "Not available: the menu does not read INI files yet.");
    CHECK(session.value(2).reason == "Not available: the menu does not read a package's setting that is kept in a global yet.");
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

TEST_CASE("PackageStore: a file that cannot be written keeps its settings dirty, and Save says so")
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

TEST_CASE("PackageStore: the default of a setting is its package's, in the type of its control")
{
    Session session;
    session.open(MOD_TOML);
    CHECK(session.store.defaultValue(session.setting(0)) == model::Value(true));
    CHECK(session.store.defaultValue(session.setting(2)) == model::Value(200.0f));
    CHECK(session.store.defaultValue(session.setting(6)) == model::Value("3"s));
    CHECK_FALSE(session.store.hotkey({}).available);
    CHECK_FALSE(session.store.run(model::NoAction{}));
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
