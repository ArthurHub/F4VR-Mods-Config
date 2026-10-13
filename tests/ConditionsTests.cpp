#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

#include "TestGlobals.h"
#include "mcm/McmStore.h"
#include "model/Conditions.h"

using namespace mods_config;
using model::Condition;
using Kind = model::Condition::Kind;

namespace
{
    constexpr auto PLUGIN = "MyMod.esp";

    Condition on(std::string id)
    {
        return { .kind = Kind::On, .setting = std::move(id), .conditions = {} };
    }

    Condition joined(const Kind kind, std::vector<Condition> conditions)
    {
        return { .kind = kind, .setting = {}, .conditions = std::move(conditions) };
    }

    model::Item globalSetting(std::string id, const model::SettingType type, const std::uint32_t formId)
    {
        return model::Setting{ .id = std::move(id),
            .type = type,
            .text = {},
            .help = {},
            .valueOptions = {},
            .storage = model::GlobalStorage{ .plugin = PLUGIN, .formId = formId } };
    }

    /**
     * A mod with a switcher that is on and one that is off, a switcher whose global is not in the game, a
     * slider at 1 and one at 2, and in a section of a page a switcher that is kept nowhere.
     */
    model::Mod someMod()
    {
        model::Mod mod{ .name = "MyMod", .displayName = "My Mod", .frontPage = {}, .pages = {} };
        mod.frontPage.items.push_back(globalSetting("on", model::SettingType::Switcher, 0x800));
        mod.frontPage.items.push_back(globalSetting("off", model::SettingType::Switcher, 0x801));
        mod.frontPage.items.push_back(globalSetting("missing", model::SettingType::Switcher, 0x1234));
        mod.frontPage.items.push_back(globalSetting("one", model::SettingType::Slider, 0x800));
        mod.frontPage.items.push_back(globalSetting("two", model::SettingType::Slider, 0x802));

        model::Group group{ .text = "A section", .items = {}, .visibleWhen = {} };
        group.items.push_back(model::Setting{ .id = "held", .type = model::SettingType::Switcher, .text = {}, .help = {}, .valueOptions = {}, .storage = {} });
        mod.pages.push_back({ .name = "A page", .items = { std::move(group) } });
        return mod;
    }

    test_globals::TestGlobals someGlobals()
    {
        test_globals::TestGlobals globals;
        globals.values = { { { PLUGIN, 0x800 }, 1.0f }, { { PLUGIN, 0x801 }, 0.0f }, { { PLUGIN, 0x802 }, 2.0f } };
        return globals;
    }
}

TEST_CASE("Conditions: a condition on a setting holds while the setting is on")
{
    const auto mod = someMod();
    auto globals = someGlobals();
    mcm::McmStore store(globals);
    store.open(mod);

    CHECK(model::holds(on("on"), store));
    CHECK_FALSE(model::holds(on("off"), store));

    // it is asked of the values the store has now
    globals.values.at({ PLUGIN, 0x801 }) = 1.0f;
    CHECK(model::holds(on("off"), store));

    // a setting of a section of a page, which the store holds the value of
    const auto& held = std::get<model::Setting>(std::get<model::Group>(mod.pages[0].items[0]).items[0]);
    CHECK_FALSE(model::holds(on("held"), store));
    store.set(held, true);
    CHECK(model::holds(on("held"), store));
}

TEST_CASE("Conditions: a value that is no switcher is on at the number 1, as in MCM's menu")
{
    const auto mod = someMod();
    auto globals = someGlobals();
    mcm::McmStore store(globals);
    store.open(mod);

    CHECK(model::holds(on("one"), store));
    CHECK_FALSE(model::holds(on("two"), store));
}

TEST_CASE("Conditions: an All holds with every one of its conditions, and an Any with one of them")
{
    const auto mod = someMod();
    auto globals = someGlobals();
    mcm::McmStore store(globals);
    store.open(mod);

    CHECK(model::holds(joined(Kind::All, { on("on"), on("one") }), store));
    CHECK_FALSE(model::holds(joined(Kind::All, { on("on"), on("off") }), store));
    CHECK(model::holds(joined(Kind::Any, { on("off"), on("on") }), store));
    CHECK_FALSE(model::holds(joined(Kind::Any, { on("off"), on("two") }), store));

    // with none
    CHECK(model::holds(joined(Kind::All, {}), store));
    CHECK_FALSE(model::holds(joined(Kind::Any, {}), store));

    // one inside another
    CHECK(model::holds(joined(Kind::All, { on("on"), joined(Kind::Any, { on("off"), on("one") }) }), store));
    CHECK_FALSE(model::holds(joined(Kind::Any, { on("off"), joined(Kind::All, { on("on"), on("off") }) }), store));
}

TEST_CASE("Conditions: a Not holds while its condition does not")
{
    const auto mod = someMod();
    auto globals = someGlobals();
    mcm::McmStore store(globals);
    store.open(mod);

    CHECK(model::holds(joined(Kind::Not, { on("off") }), store));
    CHECK_FALSE(model::holds(joined(Kind::Not, { on("on") }), store));
    CHECK_FALSE(model::holds(joined(Kind::Not, { joined(Kind::Any, { on("off"), on("on") }) }), store));
}

TEST_CASE("Conditions: a condition that a value which is not available leaves open holds")
{
    const auto mod = someMod();
    auto globals = someGlobals();
    mcm::McmStore store(globals);
    store.open(mod);

    // a global that is not in the game, and an id that no setting of the mod has
    CHECK(model::holds(on("missing"), store));
    CHECK(model::holds(on("no such setting"), store));
    CHECK(model::holds(joined(Kind::Not, { on("missing") }), store));
    CHECK(model::holds(joined(Kind::All, { on("on"), on("missing") }), store));
    CHECK(model::holds(joined(Kind::Any, { on("off"), on("missing") }), store));

    // a value that is available settles it
    CHECK_FALSE(model::holds(joined(Kind::All, { on("missing"), on("off") }), store));
    CHECK(model::holds(joined(Kind::Any, { on("missing"), on("on") }), store));
    CHECK_FALSE(model::holds(joined(Kind::Not, { joined(Kind::Any, { on("missing"), on("on") }) }), store));
}

TEST_CASE("Conditions: the settings of an id are those of the mod whose session it is")
{
    const auto mod = someMod();
    model::Mod other{ .name = "Other", .displayName = "Other", .frontPage = {}, .pages = {} };
    other.frontPage.items.push_back(globalSetting("off", model::SettingType::Switcher, 0x800));
    auto globals = someGlobals();
    mcm::McmStore store(globals);

    store.open(mod);
    CHECK_FALSE(store.valueById("off").value == model::Value(true));
    store.open(other);
    CHECK(store.valueById("off").value == model::Value(true));
    CHECK_FALSE(store.valueById("on").available);
}
