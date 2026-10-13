#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

#include "TestEvents.h"
#include "TestFiles.h"
#include "TestGlobals.h"
#include "TestProperties.h"
#include "mcm/McmLoader.h"
#include "mcm/McmStore.h"
#include "model/Definition.h"

using namespace mods_config;
using mcm::McmStore;
using model::SettingType;
using test_events::TestEvents;
using test_globals::TestGlobals;
using test_properties::TestProperties;
using Type = model::ModSettingStorage::Type;

namespace
{
    using Events = std::vector<std::string>;

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

    model::Setting propertySetting(const SettingType type, std::string name, const Type valueType)
    {
        return settingOf(type, model::PropertyStorage{ .plugin = PLUGIN, .formId = 0x800, .script = {}, .name = std::move(name), .type = valueType });
    }

    /**
     * Properties of the form 0x800 with one of each type a script keeps: `bOn` is on, `fStrength` is 1.3,
     * `iMode` is 2 and `sName` is a text.
     */
    TestProperties someProperties()
    {
        TestProperties properties;
        properties.values = {
            { { PLUGIN, 0x800, "", "bOn" }, true },
            { { PLUGIN, 0x800, "", "fStrength" }, 1.3f },
            { { PLUGIN, 0x800, "", "iMode" }, 2 },
            { { PLUGIN, 0x800, "", "sName" }, "Sanctuary Hills"s },
        };
        return properties;
    }
}

TEST_CASE("McmStore: a switcher of a global is on at 1, and off at any other number")
{
    auto globals = someGlobals();
    McmStore store;

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
    McmStore store;
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
    McmStore store;
    const auto setting = globalSetting(SettingType::Dropdown, 0x801, { .options = { { .value = 0, .text = "Low" }, { .value = 1, .text = "High" } } });

    CHECK(store.set(setting, setting.valueOptions.options[1].value));
    CHECK(globals.values.at({ PLUGIN, 0x801 }) == 1.0f);
    CHECK(store.value(setting).value == model::Value(1));
}

TEST_CASE("McmStore: a value is read from its global every time, as another can change it")
{
    auto globals = someGlobals();
    McmStore store;
    const auto setting = globalSetting(SettingType::Switcher, 0x801);
    REQUIRE(store.value(setting).value == model::Value(false));

    globals.values[{ PLUGIN, 0x801 }] = 1.0f;
    CHECK(store.value(setting).value == model::Value(true));
}

TEST_CASE("McmStore: a slider has the number of its global, and a choice the option at that place")
{
    auto globals = someGlobals();
    McmStore store;

    CHECK(store.value(globalSetting(SettingType::Slider, 0x802)).value == model::Value(2.5f));
    CHECK(store.value(globalSetting(SettingType::Stepper, 0x803)).value == model::Value(2));
    CHECK(store.value(globalSetting(SettingType::Dropdown, 0x801)).value == model::Value(0));

    // a text is kept in no global
    CHECK_FALSE(store.value(globalSetting(SettingType::TextInput, 0x800)).available);
}

TEST_CASE("McmStore: a global that is not in the game is not available, with the reason")
{
    auto globals = someGlobals();
    McmStore store;
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
    McmStore store;
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
    McmStore store;
    const auto setting = settingOf(SettingType::Switcher, model::UnknownStorage{ .type = "PropertyValueColor" });

    const auto value = store.value(setting);
    CHECK_FALSE(value.available);
    CHECK(value.reason.empty());
    CHECK_FALSE(store.set(setting, true));
    CHECK_FALSE(store.value(setting).available);
}

TEST_CASE("McmStore: a property of a script is read every time, in the type its script keeps it in")
{
    auto properties = someProperties();
    McmStore store;

    const auto on = store.value(propertySetting(SettingType::Switcher, "bOn", Type::Bool));
    CHECK(on.available);
    CHECK(on.value == model::Value(true));
    CHECK(store.value(propertySetting(SettingType::Slider, "fStrength", Type::Float)).value == model::Value(1.3f));
    CHECK(store.value(propertySetting(SettingType::Dropdown, "iMode", Type::Int)).value == model::Value(2));
    CHECK(store.value(propertySetting(SettingType::TextInput, "sName", Type::String)).value == model::Value("Sanctuary Hills"s));

    // a slider of a whole number, a text input of a number, and a bool that is on at any number but 0
    CHECK(store.value(propertySetting(SettingType::Slider, "iMode", Type::Int)).value == model::Value(2.0f));
    CHECK(store.value(propertySetting(SettingType::TextInput, "iMode", Type::Int)).value == model::Value("2"s));
    CHECK(store.value(propertySetting(SettingType::Switcher, "iMode", Type::Bool)).value == model::Value(true));
    CHECK(store.value(propertySetting(SettingType::Switcher, "iMode", Type::Int)).value == model::Value(false));

    // a script changes it
    properties.values[{ PLUGIN, 0x800, "", "bOn" }] = false;
    CHECK(store.value(propertySetting(SettingType::Switcher, "bOn", Type::Bool)).value == model::Value(false));
}

TEST_CASE("McmStore: a property of a script is written at once, in the type of its row")
{
    auto properties = someProperties();
    McmStore store;

    CHECK(store.set(propertySetting(SettingType::Switcher, "bOn", Type::Bool), false));
    CHECK(store.set(propertySetting(SettingType::Slider, "fStrength", Type::Float), 0.5f));
    CHECK(store.set(propertySetting(SettingType::TextInput, "sName", Type::String), "Diamond City"s));
    CHECK(properties.values.at({ PLUGIN, 0x800, "", "bOn" }) == model::Value(false));
    CHECK(properties.values.at({ PLUGIN, 0x800, "", "fStrength" }) == model::Value(0.5f));
    CHECK(properties.values.at({ PLUGIN, 0x800, "", "sName" }) == model::Value("Diamond City"s));

    // a slider of a whole number is written as the nearest one
    const auto mode = propertySetting(SettingType::Slider, "iMode", Type::Int);
    CHECK(store.set(mode, 6.6f));
    CHECK(properties.values.at({ PLUGIN, 0x800, "", "iMode" }) == model::Value(7));
    CHECK(store.value(mode).value == model::Value(7.0f));

    // a text for a number is not written
    CHECK_FALSE(store.set(propertySetting(SettingType::TextInput, "fStrength", Type::Float), "strong"s));
    CHECK(properties.values.at({ PLUGIN, 0x800, "", "fStrength" }) == model::Value(0.5f));
}

TEST_CASE("McmStore: a property that is not in the game is not available, with the reason, and neither is a text for a number")
{
    auto properties = someProperties();
    McmStore store;

    const auto missing = propertySetting(SettingType::Switcher, "bMissing", Type::Bool);
    const auto value = store.value(missing);
    CHECK_FALSE(value.available);
    CHECK(value.value == model::Value(false));
    CHECK(value.reason == "Not available: the property bMissing of the form 800 of MyMod.esp is not in the game.");
    CHECK_FALSE(store.set(missing, true));
    CHECK_FALSE(properties.values.contains({ PLUGIN, 0x800, "", "bMissing" }));

    // the property of another script of the form
    auto ofScript = propertySetting(SettingType::Switcher, "bOn", Type::Bool);
    std::get<model::PropertyStorage>(ofScript.storage).script = "MyMod:Other";
    CHECK_FALSE(store.value(ofScript).available);

    const auto noNumber = store.value(propertySetting(SettingType::Slider, "sName", Type::Float));
    CHECK_FALSE(noNumber.available);
    CHECK(noNumber.reason == "Not available: the property sName of the form 800 of MyMod.esp is a text, which is no number.");
}

TEST_CASE("McmStore: a ModSetting is read from the mod's defaults, and from the player's file over them")
{
    const test_files::TempFolder mcm;
    mcm.write("Config/MyMod/settings.ini", "[Main]\r\nbOn=1\r\nfStrength=1.0\r\niMode=1\r\nsName=Sanctuary Hills\r\n[Other]\r\nbOn=0\r\n");
    mcm.write("Settings/MyMod.ini", "[Main]\r\nfStrength=1.300000\r\n");
    mcm.write("Settings/Another.ini", "[Main]\r\nbOn=0\r\n");
    auto globals = someGlobals();
    McmStore store(mcm.path);
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
    McmStore store(mcm.path);
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
    McmStore store(mcm.path);
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
    McmStore noFolder;
    noFolder.open({ .name = "MyMod", .displayName = {}, .frontPage = {}, .pages = {} });
    CHECK_FALSE(noFolder.value(modSetting(SettingType::TextInput, "sName:Main", Type::String)).available);
}

TEST_CASE("McmStore: a ModSetting is read as its mod is selected, and not again until it is selected again")
{
    const test_files::TempFolder mcm;
    mcm.write("Config/MyMod/settings.ini", "[Main]\nbOn=0\n");
    auto globals = someGlobals();
    McmStore store(mcm.path);
    const model::Mod mod{ .name = "MyMod", .displayName = {}, .frontPage = {}, .pages = {} };
    const auto setting = modSetting(SettingType::Switcher, "bOn:Main", Type::Bool);
    store.open(mod);
    REQUIRE(store.value(setting).value == model::Value(false));

    // MCM writes the player's file as a script sets the value
    mcm.write("Settings/MyMod.ini", "[Main]\nbOn=1\n");
    CHECK(store.value(setting).value == model::Value(false));
    store.open(mod);
    CHECK(store.value(setting).value == model::Value(true));

    // one that was set here is read again too: a test has no MCM that writes the file
    CHECK(store.set(setting, false));
    CHECK(store.value(setting).value == model::Value(false));
    store.open(mod);
    CHECK(store.value(setting).value == model::Value(true));
}

TEST_CASE("McmStore: a ModSetting that is set has the value at once, as the type MCM keeps it in")
{
    const test_files::TempFolder mcm;
    mcm.write("Config/MyMod/settings.ini", "[Main]\nbOn=0\nfStrength=1.0\niMode=0\niCount=1\nbChoice=0\nsName=Sanctuary Hills\n");
    auto globals = someGlobals();
    McmStore store(mcm.path);
    store.open({ .name = "MyMod", .displayName = {}, .frontPage = {}, .pages = {} });

    const auto on = modSetting(SettingType::Switcher, "bOn:Main", Type::Bool);
    const auto strength = modSetting(SettingType::Slider, "fStrength:Main", Type::Float);
    const auto mode = modSetting(SettingType::Dropdown, "iMode:Main", Type::Int);
    CHECK(store.set(on, true));
    CHECK(store.set(strength, 1.3f));
    CHECK(store.set(mode, 2));
    CHECK(store.value(on).value == model::Value(true));
    CHECK(store.value(strength).value == model::Value(1.3f));
    CHECK(store.value(mode).value == model::Value(2));

    // a slider of a whole number has the nearest one, and a choice of a bool is on at any option but the first
    const auto count = modSetting(SettingType::Slider, "iCount:Main", Type::Int);
    const auto choice = modSetting(SettingType::Stepper, "bChoice:Main", Type::Bool);
    const auto name = modSetting(SettingType::TextInput, "sName:Main", Type::String);
    CHECK(store.set(count, 6.6f));
    CHECK(store.set(choice, 2));
    CHECK(store.set(name, "Diamond City"s));
    CHECK(store.value(count).value == model::Value(7.0f));
    CHECK(store.value(choice).value == model::Value(1));
    CHECK(store.value(name).value == model::Value("Diamond City"s));
}

TEST_CASE("McmStore: a ModSetting MCM does not have is not set, and neither is a value of another kind")
{
    const test_files::TempFolder mcm;
    mcm.write("Config/MyMod/settings.ini", "[Main]\nfStrength=1.0\nsName=Sanctuary Hills\n");
    auto globals = someGlobals();
    McmStore store(mcm.path);
    store.open({ .name = "MyMod", .displayName = {}, .frontPage = {}, .pages = {} });

    // MCM has the ModSettings of its files alone
    const auto missing = modSetting(SettingType::Switcher, "bMissing:Main", Type::Bool);
    CHECK_FALSE(store.set(missing, true));
    CHECK_FALSE(store.value(missing).available);

    // a text for a number, and a number for a text
    CHECK_FALSE(store.set(modSetting(SettingType::TextInput, "fStrength:Main", Type::Float), "strong"s));
    CHECK_FALSE(store.set(modSetting(SettingType::Slider, "sName:Main", Type::String), 2.0f));
    CHECK(store.value(modSetting(SettingType::Slider, "fStrength:Main", Type::Float)).value == model::Value(1.0f));
}

TEST_CASE("McmModSettings: a ModSetting that is set has the text MCM writes to its file")
{
    const test_files::TempFolder mcm;
    mcm.write("Config/MyMod/settings.ini", "[Main]\nbOn=0\nfStrength=1.0\niMode=0\nsName=Sanctuary Hills\n");
    mcm::McmModSettings settings;
    settings.load(mcm.path, "MyMod");

    CHECK(settings.set("bOn:Main", true));
    CHECK(settings.set("fStrength:Main", 1.3f));
    CHECK(settings.set("iMode:Main", 2));
    CHECK(settings.set("sName:Main", "Diamond City"s));
    CHECK(*settings.find("bOn:Main") == "1");
    CHECK(*settings.find("fStrength:Main") == "1.300000");
    CHECK(*settings.find("iMode:Main") == "2");
    CHECK(*settings.find("sName:Main") == "Diamond City");

    CHECK_FALSE(settings.set("bMissing:Main", true));
    CHECK_FALSE(settings.find("bMissing:Main"));
}

TEST_CASE("McmStore: the ModSettings of the MCM showcase have the values of its settings file")
{
    const auto mods = mcm::loadConfigs(MODS_CONFIG_MCM_SHOWCASE_DIR);
    REQUIRE(mods.size() == 1);
    auto globals = someGlobals();
    McmStore store(std::filesystem::path(MODS_CONFIG_MCM_SHOWCASE_DIR).parent_path());
    store.open(mods[0]);

    CHECK(store.valueById("bSwitcher:Main").value == model::Value(true));
    CHECK(store.valueById("fSlider:Main").value == model::Value(50.0f));
    CHECK(store.valueById("iDropdown:Main").value == model::Value(1));
    CHECK(store.valueById("fTenth:Sliders").value == model::Value(1.3f));
    CHECK(store.valueById("iCount:Sliders").value == model::Value(5.0f));
    CHECK_FALSE(store.valueById("bMissing:Switchers").available);
}

TEST_CASE("McmStore: a mod that is shown and left sends MCM's events of it")
{
    auto globals = someGlobals();
    TestEvents events;
    McmStore store;
    const model::Mod first{ .name = "MyMod", .displayName = {}, .frontPage = {}, .pages = {} };
    const model::Mod second{ .name = "Other", .displayName = {}, .frontPage = {}, .pages = {} };

    store.menuOpened();
    store.open(first);
    CHECK(events.sent == Events{ "OnMCMOpen", "OnMCMMenuOpen", "OnMCMMenuOpen|MyMod" });

    // another mod: the one before it is left first
    events.sent.clear();
    store.open(second);
    CHECK(events.sent == Events{ "OnMCMMenuClose|MyMod", "OnMCMMenuOpen", "OnMCMMenuOpen|Other" });

    events.sent.clear();
    store.close();
    store.menuClosed();
    CHECK(events.sent == Events{ "OnMCMMenuClose|Other", "OnMCMClose" });

    // no mod is left with none shown
    events.sent.clear();
    store.close();
    CHECK(events.sent.empty());
}

TEST_CASE("McmStore: a setting that is set sends MCM's event, with its mod and the id of its row")
{
    auto globals = someGlobals();
    TestEvents events;
    McmStore store;
    store.open(model::Mod{ .name = "MyMod", .displayName = {}, .frontPage = {}, .pages = {} });
    events.sent.clear();

    auto withId = globalSetting(SettingType::Switcher, 0x801);
    withId.sourceId = "bOn";
    CHECK(store.set(withId, true));
    CHECK(events.sent == Events{ "OnMCMSettingChange(MyMod, bOn)", "OnMCMSettingChange|MyMod(MyMod, bOn)" });

    // a row with no id in its config, as MCM's menu sends none for it
    events.sent.clear();
    CHECK(store.set(globalSetting(SettingType::Switcher, 0x801), false));
    CHECK(events.sent.empty());

    // a value that is not written
    auto missing = globalSetting(SettingType::Switcher, 0x1234);
    missing.sourceId = "bMissing";
    CHECK_FALSE(store.set(missing, true));
    CHECK(events.sent.empty());
}
