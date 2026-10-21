#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "TestFiles.h"
#include "spec/IniFile.h"
#include "spec/TomlFile.h"

using namespace mods_config;
using model::StoredType;
using spec::IniFile;
using spec::ModFile;
using spec::TomlFile;
using Path = std::vector<std::string>;

namespace
{
    constexpr auto MOD_TOML = "# My Mod\r\n[Main]\r\nbEnabled = true\r\niCount = 3\r\n";

    std::string readFile(const std::filesystem::path& path)
    {
        std::ostringstream text;
        text << std::ifstream(path, std::ios::binary).rdbuf();
        return text.str();
    }

    /**
     * A TOML file of its own, with the given text, read.
     */
    struct TestFile
    {
        test_files::TempFolder folder;
        std::filesystem::path path = folder.path / "MyMod.toml";
        TomlFile file{ path };

        explicit TestFile(const std::string& text = MOD_TOML)
        {
            folder.write("MyMod.toml", text);
            const auto read = file.read();
            if (!read) {
                FAIL(read.error());
            }
        }
    };
}

TEST_CASE("ModFile: prepare reads the file again and writes nothing, and commit writes the text it made")
{
    TestFile mod;
    mod.folder.write("MyMod.toml", "# My Mod\r\n[Main]\r\nbEnabled = true\r\niCount = 9\r\n");
    const std::vector<ModFile::Change> changes{ { { "Main", "bEnabled" }, false } };
    REQUIRE(mod.file.prepare(changes));
    CHECK(readFile(mod.path) == "# My Mod\r\n[Main]\r\nbEnabled = true\r\niCount = 9\r\n");
    CHECK(mod.file.value(Path{ "Main", "iCount" }, StoredType::Int) == model::Value(3));

    REQUIRE(mod.file.commit());
    CHECK(readFile(mod.path) == "# My Mod\r\n[Main]\r\nbEnabled = false\r\niCount = 9\r\n");
    CHECK(mod.file.value(Path{ "Main", "bEnabled" }, StoredType::Bool) == model::Value(false));
    CHECK(mod.file.value(Path{ "Main", "iCount" }, StoredType::Int) == model::Value(9));
}

TEST_CASE("ModFile: a file that is not there is created, with the folders on the way to it, and has only the values written")
{
    const test_files::TempFolder folder;
    const std::vector<ModFile::Change> changes{ { { "Main", "bEnabled" }, false }, { { "Main", "iCount" }, 4 } };

    TomlFile toml(folder.path / "Mods_Config" / "MyMod" / "MyMod.toml");
    REQUIRE(toml.prepare(changes));
    CHECK_FALSE(std::filesystem::exists(folder.path / "Mods_Config"));
    REQUIRE(toml.commit());
    CHECK(readFile(folder.path / "Mods_Config" / "MyMod" / "MyMod.toml") == "[Main]\nbEnabled = false\niCount = 4\n");
    CHECK(toml.value(Path{ "Main", "iCount" }, StoredType::Int) == model::Value(4));

    IniFile ini(folder.path / "MyMod" / "MyMod.ini");
    REQUIRE(ini.prepare(changes));
    REQUIRE(ini.commit());
    CHECK(readFile(folder.path / "MyMod" / "MyMod.ini") == "[Main]\r\nbEnabled=0\r\niCount=4\r\n");
}

TEST_CASE("ModFile: restore puts back the text commit replaced, and removes a file it created")
{
    TestFile mod;
    const std::vector<ModFile::Change> changes{ { { "Main", "bEnabled" }, false } };
    REQUIRE(mod.file.prepare(changes));
    REQUIRE(mod.file.commit());
    REQUIRE(mod.file.restore());
    CHECK(readFile(mod.path) == MOD_TOML);
    CHECK(mod.file.value(Path{ "Main", "bEnabled" }, StoredType::Bool) == model::Value(true));

    // a file that commit did not write is left as it is
    mod.folder.write("MyMod.toml", "[Main]\r\nbEnabled = false\r\n");
    REQUIRE(mod.file.prepare(changes));
    REQUIRE(mod.file.commit());
    REQUIRE(mod.file.restore());
    CHECK(readFile(mod.path) == "[Main]\r\nbEnabled = false\r\n");

    const test_files::TempFolder folder;
    TomlFile created(folder.path / "MyMod" / "MyMod.toml");
    REQUIRE(created.prepare(changes));
    REQUIRE(created.commit());
    REQUIRE(created.restore());
    CHECK_FALSE(std::filesystem::exists(folder.path / "MyMod" / "MyMod.toml"));
    CHECK_FALSE(created.contains(Path{ "Main", "bEnabled" }));
}

TEST_CASE("ModFile: a file that cannot have the values fails prepare, and one that cannot be written fails commit, as it was")
{
    TestFile mod;
    mod.folder.write("MyMod.toml", "[Main\r\n");
    const std::vector<ModFile::Change> changes{ { { "Main", "bEnabled" }, false } };
    CHECK_FALSE(mod.file.prepare(changes));

    mod.folder.write("MyMod.toml", MOD_TOML);
    REQUIRE(mod.file.prepare(changes));
    const auto committed = [&mod] {
        const test_files::ReadOnly readOnly(mod.path);
        return mod.file.commit();
    }();
    REQUIRE_FALSE(committed);
    CHECK(committed.error() == "MyMod.toml cannot be written");
    CHECK(readFile(mod.path) == MOD_TOML);
    CHECK(mod.file.value(Path{ "Main", "bEnabled" }, StoredType::Bool) == model::Value(true));

    // and restore does nothing then
    REQUIRE(mod.file.restore());
    CHECK(readFile(mod.path) == MOD_TOML);
}
