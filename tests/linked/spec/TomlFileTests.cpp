#include <catch2/catch_test_macros.hpp>

#include <expected>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "TestFiles.h"
#include "common/CommonUtils.h"
#include "spec/TomlFile.h"

using namespace mods_config;
using model::StoredType;
using spec::TomlFile;
using Path = std::vector<std::string>;

namespace
{
    constexpr auto MOD_TOML =
        "# My Mod's config\r\n"
        "\r\n"
        "[Main]\r\n"
        "bEnabled = true   # on by default\r\n"
        "iCount = 3\r\n"
        "fScale = 1.250\r\n"
        "fWhole = 2\r\n"
        "sName = 'C:\\Games'\r\n"
        "iFlags = 0x10\r\n"
        "\r\n"
        "# the body\r\n"
        "[Body.hand]\r\n"
        "fX = -0.5\r\n";

    std::string readFile(const std::filesystem::path& path)
    {
        std::ostringstream text;
        text << std::ifstream(path, std::ios::binary).rdbuf();
        return text.str();
    }

    /**
     * Writes values into a file as a Save does: its new text is made, and then written.
     */
    std::expected<void, std::string> writeChanges(spec::ModFile& file, const std::vector<spec::ModFile::Change>& changes)
    {
        if (auto prepared = file.prepare(changes); !prepared) {
            return prepared;
        }
        return file.commit();
    }

    /**
     * A TOML file of its own, read, with the given text.
     */
    struct ModFile
    {
        test_files::TempFolder folder;
        std::filesystem::path path = folder.path / "MyMod.toml";
        TomlFile file{ path };

        explicit ModFile(const std::string& text = MOD_TOML)
        {
            folder.write("MyMod.toml", text);
            const auto read = file.read();
            if (!read) {
                FAIL(read.error());
            }
        }
    };
}

TEST_CASE("TomlFile: a value is read by its path, in the type of its setting")
{
    const ModFile mod;
    CHECK(mod.file.value(Path{ "Main", "bEnabled" }, StoredType::Bool) == model::Value(true));
    CHECK(mod.file.value(Path{ "Main", "iCount" }, StoredType::Int) == model::Value(3));
    CHECK(mod.file.value(Path{ "Main", "sName" }, StoredType::String) == model::Value(std::string("C:\\Games")));
    CHECK(mod.file.value(Path{ "Main", "iFlags" }, StoredType::Int) == model::Value(16));

    // a number, and a whole number read as a number
    const auto scale = mod.file.value(Path{ "Main", "fScale" }, StoredType::Float);
    REQUIRE(scale);
    CHECK(common::fEqual(std::get<float>(*scale), 1.25f));
    CHECK(mod.file.value(Path{ "Main", "fWhole" }, StoredType::Float) == model::Value(2.0f));

    // a path of three
    const auto x = mod.file.value(Path{ "Body", "hand", "fX" }, StoredType::Float);
    REQUIRE(x);
    CHECK(common::fEqual(std::get<float>(*x), -0.5f));
}

TEST_CASE("TomlFile: a value the file does not have, or of another type, is not one, with the reason")
{
    const ModFile mod;
    CHECK(mod.file.value(Path{ "Main", "fMissing" }, StoredType::Float).error() == "Main.fMissing is not in MyMod.toml");
    CHECK(mod.file.value(Path{ "Other", "fScale" }, StoredType::Float).error() == "Other.fScale is not in MyMod.toml");
    CHECK(mod.file.value(Path{ "Main", "iCount" }, StoredType::String).error() == "Main.iCount is 3 in MyMod.toml, which is not a text");
    CHECK(mod.file.value(Path{ "Main", "fScale" }, StoredType::Int).error() == "Main.fScale is 1.25 in MyMod.toml, which is not a whole number");
    CHECK(mod.file.value(Path{ "Main", "sName" }, StoredType::Bool).error() == "Main.sName is \"C:\\Games\" in MyMod.toml, which is not true or false");
    CHECK(mod.file.value(Path{ "Body", "hand" }, StoredType::Float).error() == "Body.hand is { fX = -0.5 } in MyMod.toml, which is not a number");
    CHECK(mod.file.value(Path{ "Body", "hand", "fX" }, StoredType::Int).has_value() == false);

    // a whole number has to fit in an int
    const ModFile large("[Main]\r\niLarge = 3000000000\r\n");
    CHECK(large.file.value(Path{ "Main", "iLarge" }, StoredType::Int).error() == "Main.iLarge is 3000000000 in MyMod.toml, which is too large");
    CHECK(large.file.value(Path{ "Main", "iLarge" }, StoredType::Float) == model::Value(3000000000.0f));
}

TEST_CASE("TomlFile: a file that is not there or is no TOML has no values, with the reason")
{
    const test_files::TempFolder folder;
    TomlFile missing(folder.path / "Missing.toml");
    CHECK(missing.read().error() == "there is no file Missing.toml");
    CHECK(missing.value(Path{ "Main", "bEnabled" }, StoredType::Bool).error() == "Main.bEnabled is not in Missing.toml");

    folder.write("Broken.toml", "[Main]\nbEnabled = yes\n");
    TomlFile broken(folder.path / "Broken.toml");
    const auto read = broken.read();
    REQUIRE_FALSE(read);
    CHECK(read.error().contains("Broken.toml"));
    CHECK(read.error().contains("line 2"));
    CHECK_FALSE(broken.value(Path{ "Main", "bEnabled" }, StoredType::Bool));
}

TEST_CASE("TomlFile: a write changes the values alone, and keeps the comments, the blank lines and the order")
{
    ModFile mod;
    const std::vector<TomlFile::Change> changes{
        { { "Main", "bEnabled" }, false },
        { { "Main", "fScale" }, 0.8f },
        { { "Main", "sName" }, std::string("D:\\Games") },
        { { "Body", "hand", "fX" }, 1.5f },
    };
    const auto written = writeChanges(mod.file, changes);
    if (!written) {
        FAIL(written.error());
    }
    CHECK(readFile(mod.path) ==
          "# My Mod's config\r\n"
          "\r\n"
          "[Main]\r\n"
          "bEnabled = false   # on by default\r\n"
          "iCount = 3\r\n"
          "fScale = 0.8\r\n"
          "fWhole = 2\r\n"
          "sName = 'D:\\Games'\r\n"
          "iFlags = 0x10\r\n"
          "\r\n"
          "# the body\r\n"
          "[Body.hand]\r\n"
          "fX = 1.5\r\n");

    // the values are the written ones
    CHECK(mod.file.value(Path{ "Main", "bEnabled" }, StoredType::Bool) == model::Value(false));
    CHECK(mod.file.value(Path{ "Main", "sName" }, StoredType::String) == model::Value(std::string("D:\\Games")));
}

TEST_CASE("TomlFile: a table is read as its text, and a change of its text is written as the table")
{
    ModFile mod(
        "[Sphere]\r\n"
        "# the binding\r\n"
        "kPrimaryBinding = { hand = \"off\", type = \"tap\",   button = \"trigger\" }   # tap\r\n"
        "fRadius = 5.0\r\n");
    CHECK(mod.file.tableText(Path{ "Sphere", "kPrimaryBinding" }) == std::string(R"({ hand = "off", type = "tap", button = "trigger" })"));
    // any value, as a number, is its TOML text
    CHECK(mod.file.tableText(Path{ "Sphere", "fRadius" }) == std::string("5.0"));
    CHECK_FALSE(mod.file.tableText(Path{ "Sphere", "kMissing" }));

    const std::vector<TomlFile::Change> changes{
        { .path = { "Sphere", "kPrimaryBinding" }, .value = std::string(R"({ hand = "primary", type = "longpress", button = "a", duration = 1.5 })"), .table = true },
        { .path = { "Sphere", "kSecondaryBinding" }, .value = std::string(R"({ type = "none" })"), .table = true },
    };
    REQUIRE(writeChanges(mod.file, changes));
    CHECK(readFile(mod.path) ==
          "[Sphere]\r\n"
          "# the binding\r\n"
          "kPrimaryBinding = { hand = \"primary\", type = \"longpress\", button = \"a\", duration = 1.5 }   # tap\r\n"
          "fRadius = 5.0\r\n"
          "kSecondaryBinding = { type = \"none\" }\r\n");
}

TEST_CASE("TomlFile: a value the file does not have is added, in its section or in a new one")
{
    ModFile mod;
    const std::vector<TomlFile::Change> changes{
        { { "Main", "iNew" }, 7 },
        { { "Extra", "bOn" }, true },
    };
    REQUIRE(writeChanges(mod.file, changes));
    const auto text = readFile(mod.path);
    CHECK(text.contains("iFlags = 0x10\r\niNew = 7\r\n"));
    CHECK(text.ends_with("[Extra]\r\nbOn = true\r\n"));
    CHECK(mod.file.value(Path{ "Main", "iNew" }, StoredType::Int) == model::Value(7));
    CHECK(mod.file.value(Path{ "Extra", "bOn" }, StoredType::Bool) == model::Value(true));
}

TEST_CASE("TomlFile: a write reads the file again, and keeps a change someone else made since it was read")
{
    ModFile mod;
    mod.folder.write("MyMod.toml", "[Main]\r\nbEnabled = true\r\niCount = 9\r\n");
    const std::vector<TomlFile::Change> changes{ { { "Main", "bEnabled" }, false } };
    REQUIRE(writeChanges(mod.file, changes));
    CHECK(readFile(mod.path) == "[Main]\r\nbEnabled = false\r\niCount = 9\r\n");
    CHECK(mod.file.value(Path{ "Main", "iCount" }, StoredType::Int) == model::Value(9));
}

TEST_CASE("TomlFile: a file that has every value already is not written")
{
    ModFile mod;
    const std::vector<TomlFile::Change> changes{
        { { "Main", "fScale" }, 1.25f },
        { { "Main", "iFlags" }, 16 },
    };

    // a file that cannot be written: the write succeeds only when it does not write
    const test_files::ReadOnly readOnly(mod.path);
    CHECK(writeChanges(mod.file, changes));
    CHECK(readFile(mod.path) == MOD_TOML);
}

TEST_CASE("TomlFile: a write that cannot be made in full writes nothing, with the reason")
{
    SECTION("a value that cannot be written")
    {
        ModFile mod;
        const std::vector<TomlFile::Change> changes{
            { { "Main", "bEnabled" }, false },
            { { "Main", "iCount", "x" }, 1 },
        };
        const auto written = writeChanges(mod.file, changes);
        REQUIRE_FALSE(written);
        CHECK(written.error().contains("Main.iCount.x"));
        CHECK(readFile(mod.path) == MOD_TOML);
    }

    SECTION("a file that is no TOML")
    {
        ModFile mod;
        mod.folder.write("MyMod.toml", "[Main]\nbEnabled = yes\n");
        const std::vector<TomlFile::Change> changes{ { { "Main", "bEnabled" }, false } };
        const auto written = writeChanges(mod.file, changes);
        REQUIRE_FALSE(written);
        CHECK(written.error().contains("line 2"));
        CHECK(readFile(mod.path) == "[Main]\nbEnabled = yes\n");
    }
}

TEST_CASE("TomlFile: a change that removes its key takes its line out, and keeps the rest of the file")
{
    ModFile mod("# the categories\r\nSS2 = \"Gameplay\"\r\n\"My Mod\" = \"Combat\"   # moved\r\nFallUI = \"\"\r\n\r\n[Main]\r\niCount = 3\r\nfScale = 1.5\r\n");
    const std::vector<TomlFile::Change> changes{
        { .path = { "My Mod" }, .value = std::string("Combat"), .table = false, .remove = true },
        { .path = { "Main", "iCount" }, .value = 3, .table = false, .remove = true },
        { .path = { "SS2" }, .value = std::string("Settlements") },
        // a key the file does not have stays so
        { .path = { "Missing" }, .value = std::string("Gameplay"), .table = false, .remove = true },
    };

    REQUIRE(writeChanges(mod.file, changes));

    CHECK(readFile(mod.path) == "# the categories\r\nSS2 = \"Settlements\"\r\nFallUI = \"\"\r\n\r\n[Main]\r\nfScale = 1.5\r\n");
    CHECK(!mod.file.contains(Path{ "My Mod" }));
    CHECK(!mod.file.contains(Path{ "Main", "iCount" }));
    CHECK(!mod.file.contains(Path{ "Missing" }));
    CHECK(mod.file.value(Path{ "SS2" }, StoredType::String) == model::Value(std::string("Settlements")));
}

TEST_CASE("TomlFile: a key that is not alone on its lines is not removed, and nothing is written")
{
    const std::string text = "hand = { x = 1, y = 2 }\r\n";
    ModFile mod(text);
    const std::vector<TomlFile::Change> changes{ { .path = { "hand", "x" }, .value = 1, .table = false, .remove = true } };

    const auto written = writeChanges(mod.file, changes);

    REQUIRE(!written);
    CHECK(written.error().contains("hand.x cannot be removed"));
    CHECK(readFile(mod.path) == text);
}
