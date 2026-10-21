#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "TestFiles.h"
#include "spec/LiveCopies.h"

using namespace mods_config;
using spec::LiveCopies;
using Folders = std::vector<std::string>;

namespace
{
    constexpr auto INSTALLED = R"({ "modName": "MyMod" })";

    std::string readFile(const std::filesystem::path& path)
    {
        std::ostringstream text;
        text << std::ifstream(path, std::ios::binary).rdbuf();
        return text.str();
    }

    /**
     * The installed config specs and their live copies, each in a folder of the test's own, with MyMod installed.
     */
    struct TestCopies
    {
        test_files::TempFolder folder;
        LiveCopies copies{ folder.path / "Config", folder.path / "Dev" };

        TestCopies()
        {
            folder.write("Config/MyMod/spec.json", INSTALLED);
        }

        /**
         * Writes the live copy of a config spec, as its author saves it, a while ago: one that was written just now is
         * not read yet.
         */
        void save(const std::string& text) const
        {
            const auto copy = copies.copyOf("MyMod");
            std::ofstream(copy, std::ios::binary | std::ios::trunc) << text;
            std::filesystem::last_write_time(copy, std::filesystem::file_time_type::clock::now() - std::chrono::seconds(1));
        }
    };
}

TEST_CASE("A live copy is made of the installed spec.json", "[live-copies]")
{
    TestCopies test;
    CHECK_FALSE(test.copies.isOn("MyMod"));

    REQUIRE(test.copies.turnOn("MyMod"));
    CHECK(test.copies.isOn("MyMod"));
    CHECK(test.copies.copyOf("MyMod") == test.folder.path / "Dev" / "MyMod" / "spec.json");
    CHECK(readFile(test.copies.copyOf("MyMod")) == INSTALLED);
}

TEST_CASE("A live copy that is there keeps its author's edits", "[live-copies]")
{
    TestCopies test;
    test.folder.write("Dev/MyMod/spec.json", R"({ "modName": "Edited" })");

    REQUIRE(test.copies.turnOn("MyMod"));
    CHECK(readFile(test.copies.copyOf("MyMod")) == R"({ "modName": "Edited" })");

    // off keeps the copy, so on again reads the same edits
    test.copies.turnOff("MyMod");
    CHECK_FALSE(test.copies.isOn("MyMod"));
    CHECK(std::filesystem::exists(test.copies.copyOf("MyMod")));
    REQUIRE(test.copies.turnOn("MyMod"));
    CHECK(readFile(test.copies.copyOf("MyMod")) == R"({ "modName": "Edited" })");
}

TEST_CASE("A config spec with no installed spec.json has no live copy", "[live-copies]")
{
    TestCopies test;
    const auto on = test.copies.turnOn("Gone");
    REQUIRE_FALSE(on);
    CHECK(on.error().starts_with("its spec.json cannot be copied"));
    CHECK_FALSE(test.copies.isOn("Gone"));
}

TEST_CASE("A live copy is read again as it is saved", "[live-copies]")
{
    TestCopies test;
    REQUIRE(test.copies.turnOn("MyMod"));
    test.save(INSTALLED);

    // a copy that was not read since it was turned on is read
    CHECK(test.copies.changed() == Folders{ "MyMod" });
    test.copies.markRead("MyMod");
    CHECK(test.copies.changed().empty());

    // a save of the copy, and none after it is read
    std::filesystem::last_write_time(test.copies.copyOf("MyMod"), std::filesystem::file_time_type::clock::now() - std::chrono::milliseconds(500));
    CHECK(test.copies.changed() == Folders{ "MyMod" });
    test.copies.markRead("MyMod");
    CHECK(test.copies.changed().empty());

    // a copy that was saved just now is not read yet, as its editor can still be writing it
    std::filesystem::last_write_time(test.copies.copyOf("MyMod"), std::filesystem::file_time_type::clock::now());
    CHECK(test.copies.changed().empty());

    // nor one that is off
    test.save(INSTALLED);
    test.copies.turnOff("MyMod");
    CHECK(test.copies.changed().empty());
}

TEST_CASE("A live copy is stale while the installed spec.json is newer, with another text, until it is overwritten", "[live-copies]")
{
    TestCopies test;
    REQUIRE(test.copies.turnOn("MyMod"));
    const auto installed = test.folder.path / "Config" / "MyMod" / "spec.json";
    const auto copy = test.copies.copyOf("MyMod");
    const auto now = std::filesystem::file_time_type::clock::now();

    // a copy that was just made is not
    CHECK_FALSE(test.copies.isStale("MyMod"));

    // the author's edit of the copy is newer, and is kept
    test.folder.write("Dev/MyMod/spec.json", R"({ "modName": "Edited" })");
    std::filesystem::last_write_time(copy, now + std::chrono::seconds(5));
    CHECK_FALSE(test.copies.isStale("MyMod"));

    // a newer installed file with the same text is not, and one with another text is
    std::filesystem::last_write_time(installed, now);
    test.folder.write("Dev/MyMod/spec.json", INSTALLED);
    std::filesystem::last_write_time(copy, now - std::chrono::seconds(5));
    CHECK_FALSE(test.copies.isStale("MyMod"));
    test.folder.write("Config/MyMod/spec.json", R"({ "modName": "Updated" })");
    std::filesystem::last_write_time(installed, now);
    CHECK(test.copies.isStale("MyMod"));

    // overwritten, the copy is the installed file's
    REQUIRE(test.copies.overwrite("MyMod"));
    CHECK(readFile(copy) == R"({ "modName": "Updated" })");
    CHECK_FALSE(test.copies.isStale("MyMod"));
}
