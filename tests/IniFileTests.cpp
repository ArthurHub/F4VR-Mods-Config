#include <catch2/catch_test_macros.hpp>

#include <expected>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "TestFiles.h"
#include "common/CommonUtils.h"
#include "spec/IniFile.h"

using namespace mods_config;
using model::StoredType;
using spec::IniFile;
using Path = std::vector<std::string>;

namespace
{
    constexpr auto MOD_INI =
        "; My Mod's config\r\n"
        "iTop=1\r\n"
        "\r\n"
        "[Main]\r\n"
        "bEnabled = true\r\n"
        "bOn=1\r\n"
        "bShown = Yes\r\n"
        "iCount = 3\r\n"
        "fScale = 1.250\r\n"
        "fWhole = 2\r\n"
        "sEmpty =\r\n"
        "sName = C:\\Games ; not a comment\r\n"
        "; fScale = 9.0\r\n"
        "# sName = commented\r\n"
        "\r\n"
        "[Body]\r\n"
        "fX=-0.5\r\n";

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
     * An INI file of its own, read, with the given text.
     */
    struct ModFile
    {
        test_files::TempFolder folder;
        std::filesystem::path path = folder.path / "MyMod.ini";
        IniFile file{ path };

        explicit ModFile(const std::string& text = MOD_INI)
        {
            folder.write("MyMod.ini", text);
            const auto read = file.read();
            if (!read) {
                FAIL(read.error());
            }
        }

        /**
         * Writes the changes, which have to be written, and returns the file's text.
         */
        std::string write(const std::vector<IniFile::Change>& changes)
        {
            const auto written = writeChanges(file, changes);
            if (!written) {
                FAIL(written.error());
            }
            return readFile(path);
        }
    };
}

TEST_CASE("IniFile: a value is read by its section and its key, in the type of its setting")
{
    const ModFile mod;
    CHECK(mod.file.value(Path{ "Main", "bEnabled" }, StoredType::Bool) == model::Value(true));
    CHECK(mod.file.value(Path{ "Main", "bOn" }, StoredType::Bool) == model::Value(true));
    CHECK(mod.file.value(Path{ "Main", "bShown" }, StoredType::Bool) == model::Value(true));
    CHECK(mod.file.value(Path{ "Main", "iCount" }, StoredType::Int) == model::Value(3));
    CHECK(mod.file.value(Path{ "Main", "sEmpty" }, StoredType::String) == model::Value(std::string()));

    // a number, and a whole number read as a number
    const auto scale = mod.file.value(Path{ "Main", "fScale" }, StoredType::Float);
    REQUIRE(scale);
    CHECK(common::fEqual(std::get<float>(*scale), 1.25f));
    CHECK(mod.file.value(Path{ "Main", "fWhole" }, StoredType::Float) == model::Value(2.0f));
    const auto x = mod.file.value(Path{ "Body", "fX" }, StoredType::Float);
    REQUIRE(x);
    CHECK(common::fEqual(std::get<float>(*x), -0.5f));

    // a key before the first section is named by its key alone
    CHECK(mod.file.value(Path{ "iTop" }, StoredType::Int) == model::Value(1));

    // the names have no case
    CHECK(mod.file.value(Path{ "MAIN", "icount" }, StoredType::Int) == model::Value(3));
    CHECK(mod.file.contains(Path{ "body", "FX" }));
}

TEST_CASE("IniFile: a comment is a line that starts with ; or #, so a value can have either")
{
    const ModFile mod;
    CHECK(mod.file.value(Path{ "Main", "sName" }, StoredType::String) == model::Value(std::string("C:\\Games ; not a comment")));
    const auto scale = mod.file.value(Path{ "Main", "fScale" }, StoredType::Float);
    REQUIRE(scale);
    CHECK(common::fEqual(std::get<float>(*scale), 1.25f));

    const ModFile hash("[Main]\r\n  # iCount = 9\r\niCount = 3 # three\r\n");
    CHECK(hash.file.value(Path{ "Main", "iCount" }, StoredType::String) == model::Value(std::string("3 # three")));
    CHECK(hash.file.value(Path{ "Main", "iCount" }, StoredType::Int).error() == "iCount of [Main] is \"3 # three\" in MyMod.ini, which is not a whole number");
}

TEST_CASE("IniFile: a bool is 1, true, yes or on, and 0, false, no or off, in any case")
{
    const ModFile mod("[Main]\nb1=1\nb2=TRUE\nb3=yes\nb4=On\nb5=0\nb6=False\nb7=NO\nb8=off\nb9=2\n");
    for (const auto* key : { "b1", "b2", "b3", "b4" }) {
        CHECK(mod.file.value(Path{ "Main", key }, StoredType::Bool) == model::Value(true));
    }
    for (const auto* key : { "b5", "b6", "b7", "b8" }) {
        CHECK(mod.file.value(Path{ "Main", key }, StoredType::Bool) == model::Value(false));
    }
    CHECK(mod.file.value(Path{ "Main", "b9" }, StoredType::Bool).error() == "b9 of [Main] is \"2\" in MyMod.ini, which is not true or false");
}

TEST_CASE("IniFile: a whole number is in decimals or in hex, and is written in hex where the file has it so")
{
    ModFile mod("[Main]\r\niForm=0x000015DA\r\niMask=0Xff\r\niSigned=0x10\r\niLarge=0x80000000\r\niBad=0x-5\r\n");
    CHECK(mod.file.value(Path{ "Main", "iForm" }, StoredType::Int) == model::Value(0x15DA));
    CHECK(mod.file.value(Path{ "Main", "iMask" }, StoredType::Int) == model::Value(255));
    CHECK(mod.file.value(Path{ "Main", "iForm" }, StoredType::Float).has_value() == false);
    CHECK(mod.file.value(Path{ "Main", "iLarge" }, StoredType::Int).error() == "iLarge of [Main] is \"0x80000000\" in MyMod.ini, which is too large");
    CHECK(mod.file.value(Path{ "Main", "iBad" }, StoredType::Int).error() == "iBad of [Main] is \"0x-5\" in MyMod.ini, which is not a whole number");

    // with as many digits and in the case of the file's, and a negative number in decimals
    CHECK(mod.write({ { { "Main", "iForm" }, 0x2A }, { { "Main", "iMask" }, 0x1abc }, { { "Main", "iSigned" }, -3 } }) ==
          "[Main]\r\niForm=0x0000002A\r\niMask=0X1abc\r\niSigned=-3\r\niLarge=0x80000000\r\niBad=0x-5\r\n");
}

TEST_CASE("IniFile: a key the file has more than once has the value of its last, and a section the file has more than once is one")
{
    const ModFile mod("[Main]\r\niCount=1\r\n[Body]\r\nfX=0.5\r\n[Main]\r\niCount=2\r\nbOn=1\r\n");
    CHECK(mod.file.value(Path{ "Main", "iCount" }, StoredType::Int) == model::Value(2));
    CHECK(mod.file.value(Path{ "Main", "bOn" }, StoredType::Bool) == model::Value(true));
}

TEST_CASE("IniFile: a value the file does not have, or of another type, is not one, with the reason")
{
    const ModFile mod;
    CHECK(mod.file.value(Path{ "Main", "fMissing" }, StoredType::Float).error() == "fMissing of [Main] is not in MyMod.ini");
    CHECK(mod.file.value(Path{ "Other", "fScale" }, StoredType::Float).error() == "fScale of [Other] is not in MyMod.ini");
    CHECK(mod.file.value(Path{ "iCount" }, StoredType::Int).error() == "iCount is not in MyMod.ini");
    CHECK(mod.file.value(Path{ "Main", "fScale" }, StoredType::Int).error() == "fScale of [Main] is \"1.250\" in MyMod.ini, which is not a whole number");
    CHECK(mod.file.value(Path{ "Main", "sName" }, StoredType::Float).error() == "sName of [Main] is \"C:\\Games ; not a comment\" in MyMod.ini, which is not a number");
    CHECK(mod.file.value(Path{ "Main", "sEmpty" }, StoredType::Int).error() == "sEmpty of [Main] is \"\" in MyMod.ini, which is not a whole number");

    // a whole number has to fit in an int
    const ModFile large("[Main]\r\niLarge = 3000000000\r\n");
    CHECK(large.file.value(Path{ "Main", "iLarge" }, StoredType::Int).error() == "iLarge of [Main] is \"3000000000\" in MyMod.ini, which is too large");
    CHECK(large.file.value(Path{ "Main", "iLarge" }, StoredType::Float) == model::Value(3000000000.0f));
}

TEST_CASE("IniFile: a file that is not there has no values, with the reason")
{
    const test_files::TempFolder folder;
    IniFile missing(folder.path / "Missing.ini");
    CHECK(missing.read().error() == "there is no file Missing.ini");
    CHECK(missing.value(Path{ "Main", "bEnabled" }, StoredType::Bool).error() == "bEnabled of [Main] is not in Missing.ini");
}

TEST_CASE("IniFile: a file that starts with the mark of UTF-8 has it kept, and its first line read")
{
    ModFile mod("\xEF\xBB\xBF[Main]\r\niCount=3\r\n");
    CHECK(mod.file.value(Path{ "Main", "iCount" }, StoredType::Int) == model::Value(3));
    CHECK(mod.write({ { { "Main", "iCount" }, 4 } }) == "\xEF\xBB\xBF[Main]\r\niCount=4\r\n");
}

TEST_CASE("IniFile: a write changes the values alone, and keeps the comments, the blank lines, the order and the spaces")
{
    ModFile mod;
    const auto text = mod.write({
        { { "Main", "bEnabled" }, false },
        { { "Main", "fScale" }, 0.8f },
        { { "Main", "sName" }, std::string("D:\\Games") },
        { { "Main", "sEmpty" }, std::string("full") },
        { { "Body", "fX" }, 1.5f },
        { { "iTop" }, 2 },
    });
    CHECK(text ==
          "; My Mod's config\r\n"
          "iTop=2\r\n"
          "\r\n"
          "[Main]\r\n"
          "bEnabled = false\r\n"
          "bOn=1\r\n"
          "bShown = Yes\r\n"
          "iCount = 3\r\n"
          "fScale = 0.8\r\n"
          "fWhole = 2\r\n"
          "sEmpty =full\r\n"
          "sName = D:\\Games\r\n"
          "; fScale = 9.0\r\n"
          "# sName = commented\r\n"
          "\r\n"
          "[Body]\r\n"
          "fX=1.5\r\n");

    // the values are the written ones
    CHECK(mod.file.value(Path{ "Main", "bEnabled" }, StoredType::Bool) == model::Value(false));
    CHECK(mod.file.value(Path{ "Main", "sName" }, StoredType::String) == model::Value(std::string("D:\\Games")));
}

TEST_CASE("IniFile: a bool is written in the words and the case of the file's, and a number with a decimal point")
{
    ModFile mod("[Main]\nbA=true\nbB=1\nbC=Yes\nbD=OFF\nbE=maybe\nfA=1.5\nfB=0.5\n");
    const auto text = mod.write({
        { { "Main", "bA" }, false },
        { { "Main", "bB" }, false },
        { { "Main", "bC" }, false },
        { { "Main", "bD" }, true },
        { { "Main", "bE" }, true },
        { { "Main", "fA" }, 3.0f },
        { { "Main", "fB" }, 0.1f },
    });
    // a file of \n has \n
    CHECK(text == "[Main]\nbA=false\nbB=0\nbC=No\nbD=ON\nbE=1\nfA=3.0\nfB=0.1\n");
}

TEST_CASE("IniFile: a key the file does not have is added after the last value of its section, or in a new section")
{
    SECTION("in its section, with the = of the line before it, and in a new section at the end")
    {
        ModFile mod;
        const auto text = mod.write({
            { { "Main", "iNew" }, 7 },
            { { "Body", "fY" }, 0.25f },
            { { "Extra", "bOn" }, true },
            { { "Extra", "sName" }, std::string("Me") },
        });
        CHECK(text.contains("sName = C:\\Games ; not a comment\r\niNew = 7\r\n; fScale = 9.0\r\n"));
        CHECK(text.ends_with("[Body]\r\nfX=-0.5\r\nfY=0.25\r\n\r\n[Extra]\r\nbOn=1\r\nsName=Me\r\n"));
        CHECK(mod.file.value(Path{ "Main", "iNew" }, StoredType::Int) == model::Value(7));
        CHECK(mod.file.value(Path{ "Extra", "bOn" }, StoredType::Bool) == model::Value(true));
    }

    SECTION("after the heading of a section with no values, and before the first section")
    {
        ModFile mod("; comment\n[Main]\n; nothing yet\n\n[Body]\nfX=1.0\n");
        CHECK(mod.write({ { { "Main", "iCount" }, 3 }, { { "iTop" }, 1 } }) == "iTop=1\n; comment\n[Main]\niCount=3\n; nothing yet\n\n[Body]\nfX=1.0\n");
    }

    SECTION("in a file whose last line has no line break")
    {
        ModFile mod("[Main]\r\niCount=3");
        CHECK(mod.write({ { { "Main", "bOn" }, false }, { { "Body", "fX" }, 0.5f } }) == "[Main]\r\niCount=3\r\nbOn=0\r\n\r\n[Body]\r\nfX=0.5\r\n");
    }
}

TEST_CASE("IniFile: a write sets each line of a key the file has more than once")
{
    ModFile mod("[Main]\r\niCount=1\r\n[Main]\r\niCount = 2\r\n");
    CHECK(mod.write({ { { "main", "ICOUNT" }, 5 } }) == "[Main]\r\niCount=5\r\n[Main]\r\niCount = 5\r\n");
}

TEST_CASE("IniFile: a write reads the file again, and keeps a change someone else made since it was read")
{
    ModFile mod;
    mod.folder.write("MyMod.ini", "[Main]\r\nbEnabled=1\r\niCount=9\r\n");
    CHECK(mod.write({ { { "Main", "bEnabled" }, false } }) == "[Main]\r\nbEnabled=0\r\niCount=9\r\n");
    CHECK(mod.file.value(Path{ "Main", "iCount" }, StoredType::Int) == model::Value(9));
}

TEST_CASE("IniFile: a file that has every value already is not written")
{
    ModFile mod;
    const std::vector<IniFile::Change> changes{
        { { "Main", "fScale" }, 1.25f },
        { { "Main", "bShown" }, true },
    };

    // a file that cannot be written: the write succeeds only when it does not write
    const test_files::ReadOnly readOnly(mod.path);
    CHECK(writeChanges(mod.file, changes));
    CHECK(readFile(mod.path) == MOD_INI);
}

TEST_CASE("IniFile: a write that cannot be made in full writes nothing, with the reason")
{
    SECTION("a text that does not read back as itself")
    {
        ModFile mod;
        const std::vector<IniFile::Change> changes{
            { { "Main", "bEnabled" }, false },
            { { "Main", "sName" }, std::string("two\r\nlines") },
        };
        const auto written = writeChanges(mod.file, changes);
        REQUIRE_FALSE(written);
        CHECK(written.error() == "sName of [Main] cannot be written as \"two\r\nlines\" in MyMod.ini");
        CHECK(readFile(mod.path) == MOD_INI);

        const std::vector<IniFile::Change> spaces{ { { "Main", "sName" }, std::string(" padded ") } };
        CHECK_FALSE(writeChanges(mod.file, spaces));
        CHECK(readFile(mod.path) == MOD_INI);
    }

    SECTION("a path of more than a section and a key")
    {
        ModFile mod;
        const std::vector<IniFile::Change> changes{ { { "Main", "Sub", "iCount" }, 1 } };
        CHECK(writeChanges(mod.file, changes).error() == "Main.Sub.iCount is no key of MyMod.ini, which has a section and a key");
        CHECK(readFile(mod.path) == MOD_INI);
    }
}
