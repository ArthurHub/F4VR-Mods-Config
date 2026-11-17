#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <format>
#include <functional>
#include <sstream>
#include <unordered_set>

#include "TestConfigSpecs.h"
#include "common/CommonUtils.h"
#include "model/Categories.h"
#include "model/Colors.h"
#include "model/InputBindings.h"
#include "model/OptionSets.h"
#include "model/TextMarkup.h"
#include "model/Transforms.h"
#include "spec/ConfigSpecLoader.h"

using namespace mods_config;
using namespace test_config_specs;

TEST_CASE("ConfigSpecLoader: a config spec whose required plugins are not all loaded is not available, and its reason names each that is not")
{
    const auto configSpecRequiring = [](const std::string& game, const std::string& f4se) {
        return std::format(R"({{
            "minSchemaVersion": 1, "modName": "MyMod", "displayName": "My Mod", "help": "Its help.", "applies": "live",
            "pluginRequirements": [{}], "f4sePluginRequirements": [{}],
            "content": [ {{ "type": "spacer" }} ]
        }})",
            game,
            f4se);
    };
    const spec::PluginChecks plugins{ .isGamePluginLoaded =
                                          [](const std::string_view plugin) {
                                              return plugin == "MyMod.esp";
                                          },
        .isF4sePluginLoaded =
            [](const std::string_view plugin) {
                return plugin == "F4VRBody";
            } };
    const auto reason = [&plugins](const std::string& text) {
        const auto mod = spec::loadConfigSpec(text, {}, plugins);
        REQUIRE(mod);
        return mod->notAvailable;
    };

    // every one is loaded, and a requirement that is no text is passed over
    const auto mod = spec::loadConfigSpec(configSpecRequiring(R"("MyMod.esp", 5)", R"("F4VRBody")"), {}, plugins);
    REQUIRE(mod);
    CHECK(mod->notAvailable.empty());
    CHECK(mod->frontPage.items.size() == 1);
    // its F4SE plugins are kept, which a Save sends its F4SE message to
    CHECK(mod->f4sePlugins == std::vector<std::string>{ "F4VRBody" });
    CHECK(reason(configSpecRequiring("", "")).empty());

    CHECK(reason(configSpecRequiring(R"("MyMod.esp", "SS2.esm")", "")) == "its plugin SS2.esm is not loaded");
    CHECK(reason(configSpecRequiring(R"("A.esp", "MyMod.esp", "B.esm", "C.esl")", "")) == "its plugins A.esp, B.esm and C.esl are not loaded");
    CHECK(reason(configSpecRequiring("", R"("F4VRBody", "MyMod")")) == "its F4SE plugin MyMod is not loaded");
    CHECK(reason(configSpecRequiring("", R"("A", "B")")) == "its F4SE plugins A and B are not loaded");
    CHECK(reason(configSpecRequiring(R"("SS2.esm")", R"("MyMod")")) == "its plugin SS2.esm and its F4SE plugin MyMod are not loaded");

    // one that is not available is its names and the reason alone, also one whose config spec is not right
    const auto missing = spec::loadConfigSpec(configSpecRequiring(R"("SS2.esm")", ""), {}, plugins);
    REQUIRE(missing);
    CHECK(missing->name == "MyMod");
    CHECK(missing->displayName == "My Mod");
    CHECK(missing->help.empty());
    CHECK(missing->frontPage.items.empty());
    CHECK(missing->source == model::Source::ConfigSpec);
    const auto noApplies = spec::loadConfigSpec(R"({ "minSchemaVersion": 1, "modName": "MyMod", "pluginRequirements": ["SS2.esm"], "content": [] })", {}, plugins);
    REQUIRE(noApplies);
    CHECK(noApplies->notAvailable == "its plugin SS2.esm is not loaded");

    // with no checks every plugin is loaded
    CHECK(loaded(configSpecRequiring(R"("SS2.esm")", R"("MyMod")")).notAvailable.empty());
}

TEST_CASE("ConfigSpecLoader: a config spec's minSchemaVersion is required, and one that needs a newer schema than the menu reads is not available, with the version")
{
    const auto configSpecOfVersion = [](const std::string& version) {
        return std::format(R"({{ "minSchemaVersion": {}, "modName": "MyMod", "displayName": "My Mod", "applies": "live", "content": [ {{ "type": "spacer" }} ] }})", version);
    };
    // the menu's own version and an older one are read
    for (const auto version : { spec::SCHEMA_VERSION, 1 }) {
        INFO(version);
        const auto mod = loaded(configSpecOfVersion(std::to_string(version)));
        CHECK(mod.notAvailable.empty());
        CHECK(mod.frontPage.items.size() == 1);
    }

    // it is required, and is a whole number from 1
    for (const auto& version : { "0"s, "-1"s, "\"9\""s, "1.5"s, "null"s }) {
        INFO(version);
        CHECK(spec::loadConfigSpec(configSpecOfVersion(version)).error() == "No minSchemaVersion of a whole number from 1");
    }
    CHECK(spec::loadConfigSpec(R"({ "modName": "MyMod", "applies": "live", "content": [] })").error() == "No minSchemaVersion of a whole number from 1");

    const auto newer = spec::loadConfigSpec(configSpecOfVersion(std::to_string(spec::SCHEMA_VERSION + 1)));
    REQUIRE(newer);
    CHECK(newer->notAvailable == std::format("its spec.json needs a newer Mods Config FO4 VR, one that reads schema version {}", spec::SCHEMA_VERSION + 1));
    CHECK(newer->displayName == "My Mod");
    CHECK(newer->frontPage.items.empty());

    // it is told before a plugin that is not loaded, and of a config spec that is not read as this version reads one
    const auto noApplies = spec::loadConfigSpec(R"({ "minSchemaVersion": 99, "modName": "MyMod", "pluginRequirements": ["SS2.esm"] })",
        {},
        spec::PluginChecks{ .isGamePluginLoaded =
                                [](std::string_view) {
                                    return false;
                                },
            .isF4sePluginLoaded = {} });
    REQUIRE(noApplies);
    CHECK(noApplies->notAvailable == "its spec.json needs a newer Mods Config FO4 VR, one that reads schema version 99");
}
