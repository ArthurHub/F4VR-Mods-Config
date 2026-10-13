#include <catch2/catch_test_macros.hpp>

#include <string>

#include "TestGlobals.h"
#include "mcm/McmStore.h"
#include "model/Definition.h"

using namespace mods_config;
using mcm::McmStore;
using model::SettingType;
using test_globals::TestGlobals;

namespace
{
    constexpr auto PLUGIN = "MyMod.esp";

    model::Setting settingOf(const SettingType type, model::Storage storage, model::ValueOptions valueOptions = {})
    {
        return { .type = type, .text = "A setting", .help = {}, .valueOptions = std::move(valueOptions), .storage = std::move(storage) };
    }

    model::Setting globalSetting(const SettingType type, const std::uint32_t formId, model::ValueOptions valueOptions = {})
    {
        return settingOf(type, model::GlobalStorage{ .plugin = PLUGIN, .formId = formId }, std::move(valueOptions));
    }

    /**
     * Globals with one of each number a test reads: 0x800 is 1, 0x801 is 0, 0x802 is 2.5 and 0x803 is 2.
     */
    TestGlobals someGlobals()
    {
        TestGlobals globals;
        globals.values = { { { PLUGIN, 0x800 }, 1.0f }, { { PLUGIN, 0x801 }, 0.0f }, { { PLUGIN, 0x802 }, 2.5f }, { { PLUGIN, 0x803 }, 2.0f } };
        return globals;
    }
}

TEST_CASE("McmStore: a switcher of a global is on at 1, and off at any other number")
{
    auto globals = someGlobals();
    McmStore store(globals);

    const auto on = store.value(globalSetting(SettingType::Switcher, 0x800));
    CHECK(on.available);
    CHECK(on.value == model::Value(true));
    CHECK(store.value(globalSetting(SettingType::Switcher, 0x801)).value == model::Value(false));

    // as MCM's own menu has it
    CHECK(store.value(globalSetting(SettingType::Switcher, 0x803)).value == model::Value(false));
}

TEST_CASE("McmStore: a switcher is written to its global at once, as 1 and 0")
{
    auto globals = someGlobals();
    McmStore store(globals);
    const auto setting = globalSetting(SettingType::Switcher, 0x801);

    CHECK(store.set(setting, true));
    CHECK(globals.values.at({ PLUGIN, 0x801 }) == 1.0f);
    CHECK(store.value(setting).value == model::Value(true));

    CHECK(store.set(setting, false));
    CHECK(globals.values.at({ PLUGIN, 0x801 }) == 0.0f);
    CHECK(store.value(setting).value == model::Value(false));
}

TEST_CASE("McmStore: a choice is written to its global as the place of its option")
{
    auto globals = someGlobals();
    McmStore store(globals);
    const auto setting = globalSetting(SettingType::Dropdown, 0x801, { .options = { { .value = 0, .text = "Low" }, { .value = 1, .text = "High" } } });

    CHECK(store.set(setting, setting.valueOptions.options[1].value));
    CHECK(globals.values.at({ PLUGIN, 0x801 }) == 1.0f);
    CHECK(store.value(setting).value == model::Value(1));
}

TEST_CASE("McmStore: a value is read from its global every time, as another can change it")
{
    auto globals = someGlobals();
    McmStore store(globals);
    const auto setting = globalSetting(SettingType::Switcher, 0x801);
    REQUIRE(store.value(setting).value == model::Value(false));

    globals.values[{ PLUGIN, 0x801 }] = 1.0f;
    CHECK(store.value(setting).value == model::Value(true));
}

TEST_CASE("McmStore: a slider has the number of its global, and a choice the option at that place")
{
    auto globals = someGlobals();
    McmStore store(globals);

    CHECK(store.value(globalSetting(SettingType::Slider, 0x802)).value == model::Value(2.5f));
    CHECK(store.value(globalSetting(SettingType::Stepper, 0x803)).value == model::Value(2));
    CHECK(store.value(globalSetting(SettingType::Dropdown, 0x801)).value == model::Value(0));

    // a text is kept in no global
    CHECK_FALSE(store.value(globalSetting(SettingType::TextInput, 0x800)).available);
}

TEST_CASE("McmStore: a global that is not in the game is not available, with the reason")
{
    auto globals = someGlobals();
    McmStore store(globals);
    const auto setting = globalSetting(SettingType::Switcher, 0x1234);

    const auto value = store.value(setting);
    CHECK_FALSE(value.available);
    CHECK(value.value == model::Value(false));
    CHECK(value.reason == "Not available: the global 1234 of MyMod.esp is not in the game.");

    CHECK_FALSE(store.set(setting, true));
    CHECK_FALSE(globals.values.contains({ PLUGIN, 0x1234 }));
}

TEST_CASE("McmStore: a setting with no storage is held while its mod is selected, from the value it has with none")
{
    auto globals = someGlobals();
    McmStore store(globals);
    const auto switcher = settingOf(SettingType::Switcher, model::NoStorage{});
    const auto slider = settingOf(SettingType::Slider, model::NoStorage{}, { .min = 0.5f, .max = 2.0f, .step = 0.1f, .options = {} });
    const auto stepper = settingOf(SettingType::Stepper, model::NoStorage{}, { .options = { { .value = 0, .text = "Low" }, { .value = 1, .text = "High" } } });

    CHECK(store.value(switcher).available);
    CHECK(store.value(switcher).value == model::Value(false));
    CHECK(store.value(slider).value == model::Value(0.5f));
    CHECK(store.value(stepper).value == model::Value(0));

    CHECK(store.set(switcher, true));
    CHECK(store.value(switcher).value == model::Value(true));
    // each setting has its own
    CHECK(store.value(settingOf(SettingType::Switcher, model::NoStorage{})).value == model::Value(false));

    // the mod is selected again
    store.open({});
    CHECK(store.value(switcher).value == model::Value(false));
}

TEST_CASE("McmStore: a value of a storage the menu does not read is not available, and is not set")
{
    auto globals = someGlobals();
    McmStore store(globals);
    const auto setting = settingOf(SettingType::Switcher, model::UnknownStorage{ .type = "ModSettingBool" });

    const auto value = store.value(setting);
    CHECK_FALSE(value.available);
    CHECK(value.reason.empty());
    CHECK_FALSE(store.set(setting, true));
    CHECK_FALSE(store.value(setting).available);
}
