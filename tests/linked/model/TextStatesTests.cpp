#include <catch2/catch_test_macros.hpp>

#include "TestClients.h"
#include "TestIniSettings.h"
#include "model/Conditions.h"
#include "model/TextMarkup.h"
#include "model/TextStates.h"
#include "spec/ConfigSpecLoader.h"
#include "spec/ConfigSpecStore.h"

using namespace mods_config;

namespace
{
    struct Session
    {
        model::Mod mod{ .name = "MyMod", .displayName = "My Mod" };
        spec::ConfigSpecStore store{ {}, {} };

        Session()
        {
            store.open(mod);
        }
    };
}

TEST_CASE("TextStates: text inserts string, int, float and bool states, including empty and false values")
{
    Session session;
    session.store.clientCalls().addClient("MyMod",
        test_clients::TestClient{ .states = { { "version",
                                                  [] {
                                                      return std::optional<model::Value>("1.2.3"s);
                                                  } },
                                      { "count",
                                          [] {
                                              return std::optional<model::Value>(12);
                                          } },
                                      { "scale",
                                          [] {
                                              return std::optional<model::Value>(1.25f);
                                          } },
                                      { "ready",
                                          [] {
                                              return std::optional<model::Value>(true);
                                          } },
                                      { "busy",
                                          [] {
                                              return std::optional<model::Value>(false);
                                          } },
                                      { "empty",
                                          [] {
                                              return std::optional<model::Value>(std::string());
                                          } } } }
            .calls());
    const auto text = model::plainLines("Version {clientState:version}: {clientState:count}, {clientState:scale}, {clientState:ready}, {clientState:busy}, [{clientState:empty}]");
    const auto resolved = model::resolveClientText(text, &session.store);
    REQUIRE(resolved);
    CHECK(resolved->at(0).runs.at(0).text == "Version 1.2.3: 12, 1.25, true, false, []");
    CHECK(text[0].runs[0].text.starts_with("Version {clientState:version}"));
}

TEST_CASE("TextStates: a missing state or client shows Unknown with its name, and static or malformed text stays literal")
{
    Session session;
    const auto text = model::plainLines("Version {clientState:version}: {clientState:status}");
    CHECK(model::resolveClientText(text, &session.store)->at(0).runs.at(0).text == "Version Unknown (version): Unknown (status)");
    CHECK(model::resolveClientText(text, nullptr)->at(0).runs.at(0).text == "Version Unknown (version): Unknown (status)");
    session.store.clientCalls().addClient("MyMod", test_clients::TestClient{}.calls());
    session.store.startFrame();
    CHECK(model::resolveClientText(text, &session.store)->at(0).runs.at(0).text == "Version Unknown (version): Unknown (status)");
    CHECK_FALSE(model::resolveClientText(model::plainLines("Ordinary {braces} and {value}"), &session.store));
    const auto malformed = model::plainLines("Empty {clientState:}, incomplete {clientState:version");
    CHECK(*model::resolveClientText(malformed, &session.store) == malformed);
}

TEST_CASE("TextStates: a row's label and help insert states on one line, and a label's runs keep their look")
{
    Session session;
    session.store.clientCalls().addClient("MyMod",
        test_clients::TestClient{ .states = { { "profile",
                                      [] {
                                          return std::optional<model::Value>("Seated\r\nLeft"s);
                                      } } } }
            .calls());
    CHECK(model::resolveClientText("Profile: {clientState:profile}", &session.store) == "Profile: Seated Left");
    CHECK(model::resolveClientText("Plain {text}", &session.store) == "Plain {text}");

    const auto runs = model::markupRuns("<b>Profile</b> {clientState:profile}");
    const auto resolved = model::resolveClientRuns(runs, &session.store);
    REQUIRE(resolved.size() == runs.size());
    CHECK(resolved[0].bold);
    CHECK(resolved[0].text == "Profile");
    CHECK(resolved[1].text == " Seated Left");
}

TEST_CASE("TextStates: translated styled text keeps formatting and inserts state strings without markup or recursive expansion")
{
    model::Translations translations;
    translations.add("$MyMod_Version", "<p align='right'><b>Version {clientState:version}</b></p>");
    const auto mod = spec::loadConfigSpec(R"({
        "minSchemaVersion": 1, "modName": "MyMod", "displayName": "My Mod", "applies": "save",
        "content": [ { "type": "text", "text": "$MyMod_Version", "html": true } ]
    })",
        translations);
    REQUIRE(mod);
    const auto& lines = std::get<model::Text>(mod->frontPage.items[0]).lines;
    Session session;
    session.store.clientCalls().addClient("MyMod",
        test_clients::TestClient{ .states = { { "version",
                                      [] {
                                          return std::optional<model::Value>("<i>1.2.3 & {clientState:other}</i>\nsecond line"s);
                                      } } } }
            .calls());
    const auto resolved = model::resolveClientText(lines, &session.store);
    REQUIRE(resolved);
    REQUIRE(resolved->size() == 2);
    CHECK(resolved->at(0).align == model::TextAlign::Right);
    CHECK(resolved->at(1).align == model::TextAlign::Right);
    CHECK(resolved->at(0).runs.at(0).bold);
    CHECK(resolved->at(1).runs.at(0).bold);
    CHECK_FALSE(resolved->at(0).runs.at(0).italic);
    CHECK(resolved->at(0).runs.at(0).text == "Version <i>1.2.3 & {clientState:other}</i>");
    CHECK(resolved->at(1).runs.at(0).text == "second line");
    CHECK(lines[0].runs[0].text == "Version {clientState:version}");
}

TEST_CASE("TextStates: repeated text and conditions share one state query per frame, and refresh next frame")
{
    Session session;
    std::string version = "1.2.3";
    int asked = 0;
    session.store.clientCalls().addClient("MyMod",
        test_clients::TestClient{ .states = { { "version",
                                      [&] {
                                          ++asked;
                                          return std::optional<model::Value>(version);
                                      } } } }
            .calls());
    const auto text = model::plainLines("{clientState:VERSION} / {clientState:version}");
    const model::Condition condition{ .kind = model::Condition::Kind::Equals, .setting = "version", .values = { "1.2.3"s }, .source = model::Condition::Source::ClientState };
    CHECK(model::resolveClientText(text, &session.store)->at(0).runs.at(0).text == "1.2.3 / 1.2.3");
    CHECK(model::holds(condition, &session.store));
    CHECK(asked == 1);
    version = "1.2.4";
    CHECK(model::resolveClientText(text, &session.store)->at(0).runs.at(0).text == "1.2.3 / 1.2.3");
    CHECK(asked == 1);
    session.store.startFrame();
    CHECK(model::resolveClientText(text, &session.store)->at(0).runs.at(0).text == "1.2.4 / 1.2.4");
    CHECK_FALSE(model::holds(condition, &session.store));
    CHECK(asked == 2);
}

TEST_CASE("TextStates: a setting of the game's INI is inserted and asked by a condition, by its name in any case, as the game has it now")
{
    Session session;
    test_ini_settings::TestIniSettings ini;
    ini.values = { { "busewanddirectionalmovement:vrinput", true }, { "ishadowmapresolution:display", 1024 }, { "sname:general", "Vault"s } };
    const auto text = model::plainLines(
        "Wand {gameIniSetting:bUseWandDirectionalMovement:VRInput}, shadows {gameIniSetting:iShadowMapResolution:Display}, "
        "{gameIniSetting:sName:General} {clientState:none}, {gameIniSetting:fMissing:Main}, {gameIniSetting:}");
    CHECK(model::resolveClientText(text, &session.store)->at(0).runs.at(0).text == "Wand true, shadows 1024, Vault Unknown (none), Unknown (fMissing:Main), {gameIniSetting:}");
    CHECK(model::resolveClientText("Help: {gameIniSetting:iShadowMapResolution:Display}", &session.store) == "Help: 1024");

    using Source = model::Condition::Source;
    const model::Condition on{ .kind = model::Condition::Kind::On, .setting = "BUSEWANDDIRECTIONALMOVEMENT:VRINPUT", .source = Source::GameIniSetting };
    const model::Condition low{ .kind = model::Condition::Kind::LessThan, .setting = "iShadowMapResolution:Display", .values = { 2048 }, .source = Source::GameIniSetting };
    const model::Condition missing{ .kind = model::Condition::Kind::Equals, .setting = "bMissing:Main", .values = { true }, .source = Source::GameIniSetting };
    CHECK(model::holds(on, &session.store));
    CHECK(model::holds(low, &session.store));

    // a change the game's settings menu makes is seen as the setting is next asked
    ini.values["ishadowmapresolution:display"] = 2048;
    CHECK_FALSE(model::holds(low, &session.store));
    CHECK(model::resolveClientText("{gameIniSetting:iShadowMapResolution:Display}", &session.store) == "2048");

    // one the game does not have leaves the condition open: the row is shown, and a warning is not told
    CHECK(model::holds(missing, &session.store));
    CHECK_FALSE(model::surelyHolds(missing, &session.store));
}
