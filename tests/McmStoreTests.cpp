#include <catch2/catch_test_macros.hpp>

#include <string>

#include "TestFiles.h"
#include "TestGlobals.h"
#include "mcm/McmLoader.h"
#include "mcm/McmStore.h"
#include "model/Definition.h"

using namespace mods_config;
using mcm::McmStore;
using model::SettingType;
using test_globals::TestGlobals;
using Type = model::ModSettingStorage::Type;

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

    model::Setting modSetting(const SettingType type, std::string name, const Type valueType)
    {
        return settingOf(type, model::ModSettingStorage{ .name = std::move(name), .type = valueType });
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
    const auto setting = settingOf(SettingType::Switcher, model::UnknownStorage{ .type = "PropertyValueBool" });

    const auto value = store.value(setting);
    CHECK_FALSE(value.available);
    CHECK(value.reason.empty());
    CHECK_FALSE(store.set(setting, true));
    CHECK_FALSE(store.value(setting).available);
}

TEST_CASE("McmStore: a ModSetting is read from the mod's defaults, and from the player's file over them")
{
    const test_files::TempFolder mcm;
    mcm.write("Config/MyMod/settings.ini", "[Main]\r\nbOn=1\r\nfStrength=1.0\r\niMode=1\r\nsName=Sanctuary Hills\r\n[Other]\r\nbOn=0\r\n");
    mcm.write("Settings/MyMod.ini", "[Main]\r\nfStrength=1.300000\r\n");
    mcm.write("Settings/Another.ini", "[Main]\r\nbOn=0\r\n");
    auto globals = someGlobals();
    McmStore store(globals, mcm.path);
    store.open({ .name = "MyMod", .displayName = {}, .frontPage = {}, .pages = {} });

    const auto on = store.value(modSetting(SettingType::Switcher, "bOn:Main", Type::Bool));
    CHECK(on.available);
    CHECK(on.value == model::Value(true));
    CHECK(store.value(modSetting(SettingType::Switcher, "bOn:Other", Type::Bool)).value == model::Value(false));
    CHECK(store.value(modSetting(SettingType::Slider, "fStrength:Main", Type::Float)).value == model::Value(1.3f));
    CHECK(store.value(modSetting(SettingType::Dropdown, "iMode:Main", Type::Int)).value == model::Value(1));
    CHECK(store.value(modSetting(SettingType::TextInput, "sName:Main", Type::String)).value == model::Value("Sanctuary Hills"s));

    // a slider of a whole number, and a text input of a number
    CHECK(store.value(modSetting(SettingType::Slider, "iMode:Main", Type::Int)).value == model::Value(1.0f));
    CHECK(store.value(modSetting(SettingType::TextInput, "iMode:Main", Type::Int)).value == model::Value("1"s));
}

TEST_CASE("McmStore: a ModSetting is read as MCM reads its files")
{
    const test_files::TempFolder mcm;
    mcm.write("Config/MyMod/settings.ini", "\xEF\xBB\xBF; a comment\nbBefore=1\n\n  [Main]  \n  bOn = 1 \n;bOff=1\nbTrue=true\nbEmpty=\niPart=7.5 or so\nno key\n");
    auto globals = someGlobals();
    McmStore store(globals, mcm.path);
    store.open({ .name = "MyMod", .displayName = {}, .frontPage = {}, .pages = {} });

    CHECK(store.value(modSetting(SettingType::Switcher, "bOn:Main", Type::Bool)).value == model::Value(true));

    // a bool is off at 0 alone
    CHECK(store.value(modSetting(SettingType::Switcher, "bTrue:Main", Type::Bool)).value == model::Value(true));
    CHECK(store.value(modSetting(SettingType::Switcher, "bEmpty:Main", Type::Bool)).value == model::Value(true));

    // a number is read up to what is no part of it
    CHECK(store.value(modSetting(SettingType::Stepper, "iPart:Main", Type::Int)).value == model::Value(7));

    // a comment, a key before the first section, and a name in another letter case
    CHECK_FALSE(store.value(modSetting(SettingType::Switcher, "bOff:Main", Type::Bool)).available);
    CHECK_FALSE(store.value(modSetting(SettingType::Switcher, "bBefore:Main", Type::Bool)).available);
    CHECK_FALSE(store.value(modSetting(SettingType::Switcher, "bon:main", Type::Bool)).available);
}

TEST_CASE("McmStore: a ModSetting that is in neither file is not available, and neither is one that is no number")
{
    const test_files::TempFolder mcm;
    mcm.write("Config/MyMod/settings.ini", "[Main]\nfStrength=strong\nsName=Sanctuary Hills\n");
    auto globals = someGlobals();
    McmStore store(globals, mcm.path);
    store.open({ .name = "MyMod", .displayName = {}, .frontPage = {}, .pages = {} });

    const auto missing = store.value(modSetting(SettingType::Switcher, "bMissing:Main", Type::Bool));
    CHECK_FALSE(missing.available);
    CHECK(missing.value == model::Value(false));
    CHECK(missing.reason == "Not available: the ModSetting bMissing:Main is in neither of MCM's files.");

    const auto noNumber = store.value(modSetting(SettingType::Slider, "fStrength:Main", Type::Float));
    CHECK_FALSE(noNumber.available);
    CHECK(noNumber.reason == "Not available: the ModSetting fStrength:Main is 'strong' in MCM's files, which is no number.");

    // a slider of a text
    CHECK_FALSE(store.value(modSetting(SettingType::Slider, "sName:Main", Type::String)).available);

    // a mod with no files, and a store with no folder of MCM
    store.open({ .name = "Another", .displayName = {}, .frontPage = {}, .pages = {} });
    CHECK_FALSE(store.value(modSetting(SettingType::TextInput, "sName:Main", Type::String)).available);
    McmStore noFolder(globals);
    noFolder.open({ .name = "MyMod", .displayName = {}, .frontPage = {}, .pages = {} });
    CHECK_FALSE(noFolder.value(modSetting(SettingType::TextInput, "sName:Main", Type::String)).available);
}

TEST_CASE("McmStore: a ModSetting is read as its mod is selected, and is not written")
{
    const test_files::TempFolder mcm;
    mcm.write("Config/MyMod/settings.ini", "[Main]\nbOn=0\n");
    auto globals = someGlobals();
    McmStore store(globals, mcm.path);
    const model::Mod mod{ .name = "MyMod", .displayName = {}, .frontPage = {}, .pages = {} };
    const auto setting = modSetting(SettingType::Switcher, "bOn:Main", Type::Bool);
    store.open(mod);
    REQUIRE(store.value(setting).value == model::Value(false));

    CHECK_FALSE(store.set(setting, true));
    CHECK(store.value(setting).value == model::Value(false));

    // MCM writes the player's file as a script sets the value
    mcm.write("Settings/MyMod.ini", "[Main]\nbOn=1\n");
    CHECK(store.value(setting).value == model::Value(false));
    store.open(mod);
    CHECK(store.value(setting).value == model::Value(true));
}

TEST_CASE("McmStore: the ModSettings of the MCM showcase have the values of its settings file")
{
    const auto mods = mcm::loadConfigs(MODS_CONFIG_MCM_SHOWCASE_DIR);
    REQUIRE(mods.size() == 1);
    auto globals = someGlobals();
    McmStore store(globals, std::filesystem::path(MODS_CONFIG_MCM_SHOWCASE_DIR).parent_path());
    store.open(mods[0]);

    CHECK(store.valueById("bSwitcher:Main").value == model::Value(true));
    CHECK(store.valueById("fSlider:Main").value == model::Value(50.0f));
    CHECK(store.valueById("iDropdown:Main").value == model::Value(1));
    CHECK(store.valueById("fTenth:Sliders").value == model::Value(1.3f));
    CHECK(store.valueById("iCount:Sliders").value == model::Value(5.0f));
    CHECK_FALSE(store.valueById("bMissing:Switchers").available);
}
