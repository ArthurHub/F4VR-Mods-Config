#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

#include "TestFiles.h"
#include "model/FileChoices.h"

using namespace mods_config;

namespace
{
    const model::FolderOptions GOBOS{ .root = model::FileRoot::Data, .path = "Textures/MyMod/Gobos", .mask = "*.dds", .noneText = {} };
}

TEST_CASE("FileChoices: a folder lists its files that match the mask, alphabetical with no regard to case, and no folder in it")
{
    const test_files::TempFolder folder;
    folder.write("b.DDS", "");
    folder.write("A.dds", "");
    folder.write("c.txt", "");
    folder.write("sub.dds/d.dds", "");
    CHECK(model::listFiles(folder.path, "*.dds") == std::vector<std::string>{ "A.dds", "b.DDS" });
    CHECK(model::listFiles(folder.path, "") == std::vector<std::string>{ "A.dds", "b.DDS", "c.txt" });
    CHECK(model::listFiles(folder.path / "missing", "*").empty());
}

TEST_CASE("FileChoices: a value is in the folder as a bare name, or as a path of the folder in any spelling")
{
    CHECK(model::isInFolder("6_Incandescent.DDS", GOBOS));
    CHECK(model::isInFolder(R"(data\Textures\MyMod\Gobos\6_Incandescent.DDS)", GOBOS));
    CHECK(model::isInFolder("textures/mymod/gobos/6_incandescent.dds", GOBOS));
    CHECK_FALSE(model::isInFolder(R"(data\Textures\Effects\Gobos\Vanilla.dds)", GOBOS));
    CHECK_FALSE(model::isInFolder(R"(Textures\MyMod\Gobos\Old\6_Incandescent.DDS)", GOBOS));

    // `data\` is of the Data folder alone
    auto myGames = GOBOS;
    myGames.root = model::FileRoot::MyGames;
    CHECK_FALSE(model::isInFolder(R"(data\Textures\MyMod\Gobos\a.dds)", myGames));
}

TEST_CASE("FileChoices: a file is shown with no extension where the mask names one, and else with its extension apart")
{
    const auto fixed = model::shownFileName("6_Incandescent.DDS", "*.dds");
    CHECK(fixed.name == "6_Incandescent");
    CHECK(fixed.extension.empty());
    CHECK(model::shownFileName("activation-icon-ring.dds", "activation-icon-*.dds").name == "activation-icon-ring");
    const auto any = model::shownFileName("Preset.v2.json", "*");
    CHECK(any.name == "Preset.v2");
    CHECK(any.extension == ".json");
    CHECK(model::shownFileName("README", "").extension.empty());
    CHECK(model::shownFileName("a.dds", "*.d?s").extension == ".dds");
}

TEST_CASE("FileChoices: a config spec's options are its default's path with each file's name, and a value names one in any spelling")
{
    model::Setting setting;
    setting.valueOptions.folderOptions = GOBOS;
    setting.defaultValue = std::string(R"(data\Textures\MyMod\Gobos\6_Incandescent.DDS)");
    const std::vector<std::string> files{ "1_Solid.DDS", "6_Incandescent.DDS" };
    const auto options = model::fileOptions(setting, files);
    REQUIRE(options.size() == 2);
    CHECK(options[1] == model::Option{ .value = std::string(R"(data\Textures\MyMod\Gobos\6_Incandescent.DDS)"), .text = "6_Incandescent.DDS" });

    CHECK(model::findFileOption(options, GOBOS, "textures/mymod/gobos/1_solid.dds") == 0);
    CHECK(model::findFileOption(options, GOBOS, "1_Solid.DDS") == 0);
    CHECK_FALSE(model::findFileOption(options, GOBOS, "9_Laser.dds"));
    CHECK_FALSE(model::findFileOption(options, GOBOS, R"(Textures\Effects\1_Solid.DDS)"));

    // a bare default has bare names, one with none the folder's path, and the option that is no file is first
    setting.defaultValue = std::string("6_Incandescent.DDS");
    CHECK(model::fileOptions(setting, files)[0].value == model::Value(std::string("1_Solid.DDS")));
    setting.defaultValue.reset();
    setting.valueOptions.folderOptions->noneText = "No gobo";
    const auto withNone = model::fileOptions(setting, files);
    CHECK(withNone[0] == model::Option{ .value = std::string(), .text = "No gobo" });
    CHECK(withNone[1].value == model::Value(std::string("Textures/MyMod/Gobos/1_Solid.DDS")));
    CHECK(model::findFileOption(withNone, GOBOS, "") == 0);
}
