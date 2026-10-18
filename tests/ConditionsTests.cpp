#include <catch2/catch_test_macros.hpp>

#include <map>
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

    /**
     * A store of values by their ids, as a condition asks them. An id it has no value for is not available.
     */
    class ValuesStore final : public model::Store
    {
    public:
        explicit ValuesStore(std::map<std::string, model::Value, std::less<>> values)
            : _values(std::move(values))
        {}

        void open(const model::Mod&) override
        {}

        void close() override
        {}

        void pageShown() override
        {}

        model::StoredValue value(const model::Setting& setting) override
        {
            return valueById(setting.id);
        }

        model::StoredValue valueById(const std::string_view id) override
        {
            const auto found = _values.find(id);
            return found != _values.end() ? model::StoredValue{ .value = found->second, .available = true, .reason = {} } : model::StoredValue{};
        }

        bool set(const model::Setting&, const model::Value&) override
        {
            return false;
        }

        std::optional<model::Value> defaultValue(const model::Setting&) override
        {
            return std::nullopt;
        }

        bool savesAtOnce() const override
        {
            return true;
        }

        bool isDirty(const model::Setting&) const override
        {
            return false;
        }

        std::size_t dirtyCount() const override
        {
            return 0;
        }

        bool save() override
        {
            return true;
        }

        void discard() override
        {}

        model::StoredValue hotkey(const model::Hotkey&) override
        {
            return {};
        }

        bool run(const model::Action&) override
        {
            return false;
        }

    private:
        std::map<std::string, model::Value, std::less<>> _values;
    };

    Condition compared(const Kind kind, std::string id, std::vector<model::Value> values)
    {
        return { .kind = kind, .setting = std::move(id), .conditions = {}, .offWhenNotAvailable = false, .values = std::move(values) };
    }

    /**
     * A store with a bool, an int, a float and a text.
     */
    ValuesStore someValues()
    {
        return ValuesStore({ { "bOn", true }, { "iMode", 2 }, { "fScale", 1.4f }, { "sHand", std::string("left") } });
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
    mcm::McmStore store;
    store.open(mod);

    CHECK(model::holds(on("on"), &store));
    CHECK_FALSE(model::holds(on("off"), &store));

    // it is asked of the values the store has now
    globals.values.at({ PLUGIN, 0x801 }) = 1.0f;
    CHECK(model::holds(on("off"), &store));

    // a setting of a section of a page, which the store holds the value of
    const auto& held = std::get<model::Setting>(std::get<model::Group>(mod.pages[0].items[0]).items[0]);
    CHECK_FALSE(model::holds(on("held"), &store));
    store.set(held, true);
    CHECK(model::holds(on("held"), &store));
}

TEST_CASE("Conditions: a value that is no switcher is on at the number 1, as in MCM's menu")
{
    const auto mod = someMod();
    auto globals = someGlobals();
    mcm::McmStore store;
    store.open(mod);

    CHECK(model::holds(on("one"), &store));
    CHECK_FALSE(model::holds(on("two"), &store));
}

TEST_CASE("Conditions: an All holds with every one of its conditions, and an Any with one of them")
{
    const auto mod = someMod();
    auto globals = someGlobals();
    mcm::McmStore store;
    store.open(mod);

    CHECK(model::holds(joined(Kind::All, { on("on"), on("one") }), &store));
    CHECK_FALSE(model::holds(joined(Kind::All, { on("on"), on("off") }), &store));
    CHECK(model::holds(joined(Kind::Any, { on("off"), on("on") }), &store));
    CHECK_FALSE(model::holds(joined(Kind::Any, { on("off"), on("two") }), &store));

    // with none
    CHECK(model::holds(joined(Kind::All, {}), &store));
    CHECK_FALSE(model::holds(joined(Kind::Any, {}), &store));

    // one inside another
    CHECK(model::holds(joined(Kind::All, { on("on"), joined(Kind::Any, { on("off"), on("one") }) }), &store));
    CHECK_FALSE(model::holds(joined(Kind::Any, { on("off"), joined(Kind::All, { on("on"), on("off") }) }), &store));
}

TEST_CASE("Conditions: a Not holds while its condition does not")
{
    const auto mod = someMod();
    auto globals = someGlobals();
    mcm::McmStore store;
    store.open(mod);

    CHECK(model::holds(joined(Kind::Not, { on("off") }), &store));
    CHECK_FALSE(model::holds(joined(Kind::Not, { on("on") }), &store));
    CHECK_FALSE(model::holds(joined(Kind::Not, { joined(Kind::Any, { on("off"), on("on") }) }), &store));
}

TEST_CASE("Conditions: a condition that a value which is not available leaves open holds")
{
    const auto mod = someMod();
    auto globals = someGlobals();
    mcm::McmStore store;
    store.open(mod);

    // a global that is not in the game, and an id that no setting of the mod has
    CHECK(model::holds(on("missing"), &store));
    CHECK(model::holds(on("no such setting"), &store));
    CHECK(model::holds(joined(Kind::Not, { on("missing") }), &store));
    CHECK(model::holds(joined(Kind::All, { on("on"), on("missing") }), &store));
    CHECK(model::holds(joined(Kind::Any, { on("off"), on("missing") }), &store));

    // a value that is available settles it
    CHECK_FALSE(model::holds(joined(Kind::All, { on("missing"), on("off") }), &store));
    CHECK(model::holds(joined(Kind::Any, { on("missing"), on("on") }), &store));
    CHECK_FALSE(model::holds(joined(Kind::Not, { joined(Kind::Any, { on("missing"), on("on") }) }), &store));
}

TEST_CASE("Conditions: the value of a setting no row shows is off where it is not available")
{
    const auto mod = someMod();
    auto globals = someGlobals();
    mcm::McmStore store;
    store.open(mod);

    // as MCM's menu reads a hidden switcher
    auto hidden = on("missing");
    hidden.offWhenNotAvailable = true;
    CHECK_FALSE(model::holds(hidden, &store));
    CHECK(model::holds(joined(Kind::Not, { hidden }), &store));

    // one whose value is available is read as any other
    auto shown = on("on");
    shown.offWhenNotAvailable = true;
    CHECK(model::holds(shown, &store));
}

TEST_CASE("Conditions: the settings of an id are those of the mod whose session it is")
{
    const auto mod = someMod();
    model::Mod other{ .name = "Other", .displayName = "Other", .frontPage = {}, .pages = {} };
    other.frontPage.items.push_back(globalSetting("off", model::SettingType::Switcher, 0x800));
    auto globals = someGlobals();
    mcm::McmStore store;

    store.open(mod);
    CHECK_FALSE(store.valueById("off").value == model::Value(true));
    store.open(other);
    CHECK(store.valueById("off").value == model::Value(true));
    CHECK_FALSE(store.valueById("on").available);
}

TEST_CASE("Conditions: an Equals and a NotEquals compare a value of any type, and a number with a number of either type")
{
    auto store = someValues();
    CHECK(model::holds(compared(Kind::Equals, "bOn", { true }), &store));
    CHECK_FALSE(model::holds(compared(Kind::Equals, "bOn", { false }), &store));
    CHECK(model::holds(compared(Kind::Equals, "iMode", { 2 }), &store));
    CHECK(model::holds(compared(Kind::Equals, "iMode", { 2.0f }), &store));
    CHECK(model::holds(compared(Kind::Equals, "sHand", { std::string("left") }), &store));
    CHECK_FALSE(model::holds(compared(Kind::Equals, "sHand", { std::string("Left") }), &store));
    CHECK(model::holds(compared(Kind::NotEquals, "iMode", { 1 }), &store));
    CHECK_FALSE(model::holds(compared(Kind::NotEquals, "sHand", { std::string("left") }), &store));

    // a number is as near as a number and the text of it in a file
    CHECK(model::holds(compared(Kind::Equals, "fScale", { 1.4000001f }), &store));

    // a text is never a number, and a bool never a whole number
    CHECK_FALSE(model::holds(compared(Kind::Equals, "iMode", { std::string("2") }), &store));
    CHECK_FALSE(model::holds(compared(Kind::Equals, "bOn", { 1 }), &store));
}

TEST_CASE("Conditions: an order holds on a number below or above its value, and an equal one only for its OrEqual")
{
    auto store = someValues();
    CHECK(model::holds(compared(Kind::LessThan, "iMode", { 3 }), &store));
    CHECK_FALSE(model::holds(compared(Kind::LessThan, "iMode", { 2 }), &store));
    CHECK(model::holds(compared(Kind::LessThanOrEqual, "iMode", { 2 }), &store));
    CHECK(model::holds(compared(Kind::GreaterThan, "fScale", { 1 }), &store));
    CHECK_FALSE(model::holds(compared(Kind::GreaterThan, "fScale", { 1.4f }), &store));
    CHECK(model::holds(compared(Kind::GreaterThanOrEqual, "fScale", { 1.4f }), &store));
    CHECK_FALSE(model::holds(compared(Kind::GreaterThanOrEqual, "fScale", { 1.5f }), &store));

    // a number that is as near to its value is not below or above it
    CHECK_FALSE(model::holds(compared(Kind::LessThan, "fScale", { 1.4000001f }), &store));

    // a text and a bool are no number
    CHECK_FALSE(model::holds(compared(Kind::GreaterThan, "sHand", { 0 }), &store));
    CHECK_FALSE(model::holds(compared(Kind::LessThan, "bOn", { 2 }), &store));
}

TEST_CASE("Conditions: a OneOf holds on one of its values, and a NotOneOf on none of them")
{
    auto store = someValues();
    CHECK(model::holds(compared(Kind::OneOf, "sHand", { std::string("right"), std::string("left") }), &store));
    CHECK_FALSE(model::holds(compared(Kind::OneOf, "sHand", { std::string("right") }), &store));
    CHECK(model::holds(compared(Kind::OneOf, "iMode", { 0, 2 }), &store));
    CHECK(model::holds(compared(Kind::NotOneOf, "iMode", { 0, 1 }), &store));
    CHECK_FALSE(model::holds(compared(Kind::NotOneOf, "iMode", { 1, 2 }), &store));

    // with none
    CHECK_FALSE(model::holds(compared(Kind::OneOf, "iMode", {}), &store));
    CHECK(model::holds(compared(Kind::NotOneOf, "iMode", {}), &store));
}

TEST_CASE("Conditions: a comparison that a value which is not available leaves open holds, and so does one with no store")
{
    auto store = someValues();
    CHECK(model::holds(compared(Kind::Equals, "missing", { true }), &store));
    CHECK(model::holds(compared(Kind::NotEquals, "missing", { true }), &store));
    CHECK(model::holds(joined(Kind::Not, { compared(Kind::LessThan, "missing", { 1 }) }), &store));
    CHECK_FALSE(model::holds(joined(Kind::All, { compared(Kind::Equals, "missing", { 1 }), compared(Kind::Equals, "bOn", { false }) }), &store));

    // with no store no value is available, and a condition that needs none is asked as any other
    CHECK(model::holds(compared(Kind::Equals, "bOn", { false }), nullptr));
    CHECK(model::holds(on("bOn"), nullptr));
    CHECK_FALSE(model::holds(joined(Kind::Any, {}), nullptr));
    CHECK(model::holds(joined(Kind::Not, { joined(Kind::Any, {}) }), nullptr));
}
