#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <string>
#include <vector>

#include "TestActions.h"
#include "TestEvents.h"
#include "TestFiles.h"
#include "TestGlobals.h"
#include "TestProperties.h"
#include "mcm/McmLoader.h"
#include "mcm/McmStore.h"
#include "model/Conditions.h"
#include "model/ConfigDefinition.h"

using namespace mods_config;
using mcm::McmStore;
using model::SettingType;
using test_actions::TestActions;
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

TEST_CASE("McmStore: a switcher waits for a Save, which writes it to its global as 1 and 0")
{
    auto globals = someGlobals();
    McmStore store;
    const auto setting = globalSetting(SettingType::Switcher, 0x801);

    CHECK(store.set(setting, true));
    CHECK(globals.values.at({ PLUGIN, 0x801 }) == 0.0f);
    CHECK(store.value(setting).value == model::Value(true));
    CHECK(store.isDirty(setting));
    CHECK(store.save().has_value());
    CHECK(globals.values.at({ PLUGIN, 0x801 }) == 1.0f);
    CHECK_FALSE(store.isDirty(setting));

    CHECK(store.set(setting, false));
    CHECK(store.save().has_value());
    CHECK(globals.values.at({ PLUGIN, 0x801 }) == 0.0f);
    CHECK(store.value(setting).value == model::Value(false));
}

TEST_CASE("McmStore: a choice is written to its global as the place of its option")
{
    auto globals = someGlobals();
    McmStore store;
    const auto setting = globalSetting(SettingType::Dropdown, 0x801, { .options = { { .value = 0, .text = "Low" }, { .value = 1, .text = "High" } } });

    CHECK(store.set(setting, setting.valueOptions.options[1].value));
    CHECK(store.save().has_value());
    CHECK(globals.values.at({ PLUGIN, 0x801 }) == 1.0f);
    CHECK(store.value(setting).value == model::Value(1));
}

TEST_CASE("McmStore: a change waits for a Save, a value set back is no change, and Discard takes the changes back")
{
    auto globals = someGlobals();
    McmStore store;
    const auto on = globalSetting(SettingType::Switcher, 0x801);
    const auto strength = globalSetting(SettingType::Slider, 0x802);

    CHECK(store.dirtyCount() == 0);
    CHECK_FALSE(store.lastSave());
    CHECK(store.set(on, true));
    CHECK(store.set(strength, 1.5f));
    CHECK(store.dirtyCount() == 2);

    // set back to the value of its global
    CHECK(store.set(strength, 2.5f));
    CHECK_FALSE(store.isDirty(strength));
    CHECK(store.dirtyCount() == 1);

    store.discard();
    CHECK(store.dirtyCount() == 0);
    CHECK(store.value(on).value == model::Value(false));
    CHECK(globals.values.at({ PLUGIN, 0x801 }) == 0.0f);

    // a Save, which says it wrote no change that takes effect after a restart, until the next change
    CHECK(store.set(on, true));
    CHECK(store.save().has_value());
    CHECK(store.lastSave() == 0u);
    CHECK(store.set(strength, 1.5f));
    CHECK_FALSE(store.lastSave());
}

TEST_CASE("McmStore: a change whose global comes to have its value, as a script sets it, is no change any more")
{
    auto globals = someGlobals();
    McmStore store;
    const auto on = globalSetting(SettingType::Switcher, 0x801);
    const auto strength = globalSetting(SettingType::Slider, 0x802);
    CHECK(store.set(on, true));
    CHECK(store.set(strength, 1.5f));

    globals.values[{ PLUGIN, 0x801 }] = 1.0f;
    globals.values[{ PLUGIN, 0x802 }] = 2.0f;
    store.startFrame();
    CHECK_FALSE(store.isDirty(on));

    // one whose global the script set to another value stays the player's, and a Save writes it
    CHECK(store.isDirty(strength));
    CHECK(store.value(strength).value == model::Value(1.5f));
    CHECK(store.save().has_value());
    CHECK(globals.values.at({ PLUGIN, 0x802 }) == 1.5f);
}

TEST_CASE("McmStore: a Save writes nothing while a global of a change is not in the game, and keeps every change")
{
    auto globals = someGlobals();
    TestEvents events;
    McmStore store;
    auto on = globalSetting(SettingType::Switcher, 0x801);
    on.sourceId = "bOn";
    const auto strength = globalSetting(SettingType::Slider, 0x802);
    CHECK(store.set(on, true));
    CHECK(store.set(strength, 1.5f));
    events.sent.clear();

    globals.values.erase({ PLUGIN, 0x802 });
    const auto saved = store.save();
    REQUIRE_FALSE(saved);
    CHECK(saved.error() == "the global 802 of MyMod.esp is not in the game");
    CHECK(store.saveFailure() == saved.error());
    CHECK(globals.values.at({ PLUGIN, 0x801 }) == 0.0f);
    CHECK(store.dirtyCount() == 2);
    CHECK(events.sent.empty());

    // until the next change
    CHECK(store.set(on, false));
    CHECK(store.saveFailure().empty());
}

TEST_CASE("McmStore: a Save writes every change in the order of the config, then runs the action of each row and sends its event in that order")
{
    auto globals = someGlobals();
    TestEvents events;
    TestActions actions;
    model::Mod mod{ .name = "MyMod", .displayName = {}, .frontPage = {}, .pages = {} };
    auto first = globalSetting(SettingType::Switcher, 0x801);
    first.sourceId = "bFirst";
    first.action = model::CallFunction{ .plugin = PLUGIN, .formId = 0x800, .script = {}, .function = "First", .arguments = {}, .valueArguments = {} };
    mod.frontPage.items.emplace_back(first);
    auto second = globalSetting(SettingType::Slider, 0x802);
    second.sourceId = "fSecond";
    model::Page page;
    page.name = "Second";
    page.items.emplace_back(second);
    mod.pages.push_back(std::move(page));
    McmStore store;
    store.open(mod);
    const auto& firstSetting = std::get<model::Setting>(mod.frontPage.items[0]);
    const auto& secondSetting = std::get<model::Setting>(mod.pages[0].items[0]);

    // set in the other order
    CHECK(store.set(secondSetting, 1.5f));
    CHECK(store.set(firstSetting, true));
    events.sent.clear();
    CHECK(store.save().has_value());
    CHECK(globals.values.at({ PLUGIN, 0x801 }) == 1.0f);
    CHECK(globals.values.at({ PLUGIN, 0x802 }) == 1.5f);
    CHECK(
        events.sent ==
        Events{ "OnMCMSettingChange(MyMod, bFirst)", "OnMCMSettingChange|MyMod(MyMod, bFirst)", "OnMCMSettingChange(MyMod, fSecond)", "OnMCMSettingChange|MyMod(MyMod, fSecond)" });
    REQUIRE(actions.called.size() == 1);
    CHECK(actions.called[0].function == "First");
    CHECK(store.dirtyCount() == 0);
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

TEST_CASE("McmStore: a setting with no storage is held while its mod is selected, from the value it has with none, and is never dirty")
{
    auto globals = someGlobals();
    TestActions actions;
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
    CHECK_FALSE(store.isDirty(switcher));
    CHECK(store.dirtyCount() == 0);
    // each setting has its own
    CHECK(store.value(settingOf(SettingType::Switcher, model::NoStorage{})).value == model::Value(false));

    // nothing writes it, so its action runs as it is set
    auto withAction = settingOf(SettingType::Switcher, model::NoStorage{});
    withAction.action = model::CallFunction{ .plugin = PLUGIN, .formId = 0x800, .script = {}, .function = "Unfold", .arguments = {}, .valueArguments = {} };
    CHECK(store.set(withAction, true));
    CHECK(actions.called.size() == 1);

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

TEST_CASE("McmStore: a property of a script is written by Save, in the type of its row")
{
    auto properties = someProperties();
    McmStore store;

    const auto on = propertySetting(SettingType::Switcher, "bOn", Type::Bool);
    const auto strength = propertySetting(SettingType::Slider, "fStrength", Type::Float);
    const auto name = propertySetting(SettingType::TextInput, "sName", Type::String);
    CHECK(store.set(on, false));
    CHECK(store.set(strength, 0.5f));
    CHECK(store.set(name, "Diamond City"s));
    CHECK(properties.values.at({ PLUGIN, 0x800, "", "bOn" }) == model::Value(true));
    CHECK(store.save().has_value());
    CHECK(properties.values.at({ PLUGIN, 0x800, "", "bOn" }) == model::Value(false));
    CHECK(properties.values.at({ PLUGIN, 0x800, "", "fStrength" }) == model::Value(0.5f));
    CHECK(properties.values.at({ PLUGIN, 0x800, "", "sName" }) == model::Value("Diamond City"s));

    // a slider of a whole number is written as the nearest one
    const auto mode = propertySetting(SettingType::Slider, "iMode", Type::Int);
    CHECK(store.set(mode, 6.6f));
    CHECK(store.value(mode).value == model::Value(7.0f));
    CHECK(store.save().has_value());
    CHECK(properties.values.at({ PLUGIN, 0x800, "", "iMode" }) == model::Value(7));

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

    // a comment, a key before the first section, and a name in another letter case are not read: off, as
    // MCM has a ModSetting that is in no file
    CHECK(store.value(modSetting(SettingType::Switcher, "bOff:Main", Type::Bool)).value == model::Value(false));
    CHECK(store.value(modSetting(SettingType::Switcher, "bBefore:Main", Type::Bool)).value == model::Value(false));
    CHECK(store.value(modSetting(SettingType::Switcher, "bon:main", Type::Bool)).value == model::Value(false));
}

TEST_CASE("McmStore: a ModSetting that is in neither file is what MCM reads for it, and one that is no number is not available")
{
    const test_files::TempFolder mcm;
    mcm.write("Config/MyMod/settings.ini", "[Main]\nfStrength=strong\nsName=Sanctuary Hills\n");
    auto globals = someGlobals();
    McmStore store(mcm.path);
    store.open({ .name = "MyMod", .displayName = {}, .frontPage = {}, .pages = {} });

    // off, -1 for a number, which is no option of a choice, and an empty text
    const auto missing = store.value(modSetting(SettingType::Switcher, "bMissing:Main", Type::Bool));
    CHECK(missing.available);
    CHECK(missing.value == model::Value(false));
    CHECK(store.value(modSetting(SettingType::Dropdown, "iMissing:Main", Type::Int)).value == model::Value(-1));
    CHECK(store.value(modSetting(SettingType::Slider, "fMissing:Main", Type::Float)).value == model::Value(-1.0f));
    CHECK(store.value(modSetting(SettingType::TextInput, "sMissing:Main", Type::String)).value == model::Value(""s));

    const auto noNumber = store.value(modSetting(SettingType::Slider, "fStrength:Main", Type::Float));
    CHECK_FALSE(noNumber.available);
    CHECK(noNumber.reason == "Not available: the ModSetting fStrength:Main is 'strong' in MCM's files, which is no number.");

    // a slider of a text
    CHECK_FALSE(store.value(modSetting(SettingType::Slider, "sName:Main", Type::String)).available);

    // a mod with no files, and a store with no folder of MCM
    store.open({ .name = "Another", .displayName = {}, .frontPage = {}, .pages = {} });
    CHECK(store.value(modSetting(SettingType::TextInput, "sName:Main", Type::String)).value == model::Value(""s));
    McmStore noFolder;
    noFolder.open({ .name = "MyMod", .displayName = {}, .frontPage = {}, .pages = {} });
    CHECK(noFolder.value(modSetting(SettingType::TextInput, "sName:Main", Type::String)).value == model::Value(""s));
}

TEST_CASE("McmStore: a ModSetting is read as its mod is selected, and from the player's file again as another of its pages is shown")
{
    const test_files::TempFolder mcm;
    mcm.write("Config/MyMod/settings.ini", "[Main]\nbOn=0\nfStrength=1.0\n");
    auto globals = someGlobals();
    McmStore store(mcm.path);
    const auto on = modSetting(SettingType::Switcher, "bOn:Main", Type::Bool);
    const auto strength = modSetting(SettingType::Slider, "fStrength:Main", Type::Float);
    store.open({ .name = "MyMod", .displayName = {}, .frontPage = {}, .pages = {} });
    REQUIRE(store.value(on).value == model::Value(false));

    // MCM writes the player's file as a script sets the value
    mcm.write("Settings/MyMod.ini", "[Main]\nbOn=1\n");
    CHECK(store.value(on).value == model::Value(false));
    store.pageShown();
    CHECK(store.value(on).value == model::Value(true));

    // over the mod's defaults, in place of what was read before
    mcm.write("Settings/MyMod.ini", "[Main]\nfStrength=1.300000\n");
    store.pageShown();
    CHECK(store.value(on).value == model::Value(false));
    CHECK(store.value(strength).value == model::Value(1.3f));

    // a change stays the player's, and one that was saved is read again: a test has no MCM that writes the file
    CHECK(store.set(on, true));
    store.pageShown();
    CHECK(store.value(on).value == model::Value(true));
    CHECK(store.save().has_value());
    CHECK(store.value(on).value == model::Value(true));
    store.pageShown();
    CHECK(store.value(on).value == model::Value(false));

    // a ModSetting that is held is in neither file, and is kept
    const auto missing = modSetting(SettingType::Switcher, "bMissing:Main", Type::Bool);
    CHECK(store.set(missing, true));
    store.pageShown();
    CHECK(store.value(missing).value == model::Value(true));
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

TEST_CASE("McmStore: a ModSetting MCM does not have is held while its mod is selected, and a value of another kind is not set")
{
    const test_files::TempFolder mcm;
    mcm.write("Config/MyMod/settings.ini", "[Main]\nfStrength=1.0\nsName=Sanctuary Hills\n");
    auto globals = someGlobals();
    McmStore store(mcm.path);
    store.open({ .name = "MyMod", .displayName = {}, .frontPage = {}, .pages = {} });

    // MCM has the ModSettings of its files alone, and its menu holds another, as Sim Settlements 2 has a
    // switcher that shows a group. It is off again as the mod is selected again
    const auto missing = modSetting(SettingType::Switcher, "bMissing:Main", Type::Bool);
    CHECK(store.set(missing, true));
    CHECK(store.value(missing).value == model::Value(true));
    store.open({ .name = "MyMod", .displayName = {}, .frontPage = {}, .pages = {} });
    CHECK(store.value(missing).value == model::Value(false));

    // a text for a number, and a number for a text
    CHECK_FALSE(store.set(modSetting(SettingType::TextInput, "fStrength:Main", Type::Float), "strong"s));
    CHECK_FALSE(store.set(modSetting(SettingType::Slider, "sName:Main", Type::String), 2.0f));
    CHECK(store.value(modSetting(SettingType::Slider, "fStrength:Main", Type::Float)).value == model::Value(1.0f));
}

TEST_CASE("McmStore: a text input that takes a number has the text of its number, as MCM's menu shows it")
{
    const test_files::TempFolder mcm;
    mcm.write("Config/MyMod/settings.ini", "[Main]\nfStrength=3.1400\nfTiny=0.0000004\niCount=42\nsWord=strong\n");
    auto globals = someGlobals();
    auto properties = someProperties();
    McmStore store(mcm.path);
    store.open({ .name = "MyMod", .displayName = {}, .frontPage = {}, .pages = {} });
    const auto field = [](model::Storage storage, const Type valueType) {
        auto setting = settingOf(SettingType::TextInput, std::move(storage));
        setting.valueType = valueType;
        return setting;
    };

    // a number has 6 decimals at most
    CHECK(store.value(field(model::ModSettingStorage{ .name = "fStrength:Main", .type = Type::Float }, Type::Float)).value == model::Value("3.14"s));
    CHECK(store.value(field(model::ModSettingStorage{ .name = "fTiny:Main", .type = Type::Float }, Type::Float)).value == model::Value("0"s));
    CHECK(store.value(field(model::ModSettingStorage{ .name = "iCount:Main", .type = Type::Int }, Type::Int)).value == model::Value("42"s));
    CHECK_FALSE(store.value(field(model::ModSettingStorage{ .name = "sWord:Main", .type = Type::Float }, Type::Float)).available);
    CHECK(store.defaultValue(field(model::ModSettingStorage{ .name = "fStrength:Main", .type = Type::Float }, Type::Float)) == model::Value("3.14"s));

    // a global, a property of a script, and one kept nowhere, which is 0 until it is set
    CHECK(store.value(field(model::GlobalStorage{ .plugin = PLUGIN, .formId = 0x802 }, Type::Float)).value == model::Value("2.5"s));
    CHECK(store.value(field(model::GlobalStorage{ .plugin = PLUGIN, .formId = 0x802 }, Type::Int)).value == model::Value("2"s));
    CHECK(store.value(field(model::PropertyStorage{ .plugin = PLUGIN, .formId = 0x800, .script = {}, .name = "fStrength", .type = Type::Float }, Type::Float)).value ==
          model::Value("1.3"s));
    CHECK(store.value(field(model::NoStorage{}, Type::Int)).value == model::Value("0"s));
}

TEST_CASE("McmStore: a text input that takes a number is written as the number that is typed, and a text that is no number is not")
{
    const test_files::TempFolder mcm;
    mcm.write("Config/MyMod/settings.ini", "[Main]\nfStrength=1.0\niCount=5\n");
    auto globals = someGlobals();
    TestEvents events;
    TestActions actions;
    McmStore store(mcm.path);
    store.open({ .name = "MyMod", .displayName = {}, .frontPage = {}, .pages = {} });
    const auto field = [](model::Storage storage, const Type valueType) {
        auto setting = settingOf(SettingType::TextInput, std::move(storage));
        setting.valueType = valueType;
        setting.sourceId = "field";
        return setting;
    };
    auto strength = field(model::ModSettingStorage{ .name = "fStrength:Main", .type = Type::Float }, Type::Float);
    strength.action = model::CallFunction{ .plugin = PLUGIN,
        .formId = 0x800,
        .script = {},
        .function = "SetStrength",
        .arguments = { 0.0f },
        .valueArguments = { { .place = 0, .text = "{value}", .type = Type::Float } } };
    const auto count = field(model::ModSettingStorage{ .name = "iCount:Main", .type = Type::Int }, Type::Int);
    events.sent.clear();

    // as MCM's menu shows it, and its action gets the number
    CHECK(store.set(strength, "12.50"s));
    CHECK(store.value(strength).value == model::Value("12.5"s));
    CHECK(store.set(count, "-7"s));
    CHECK(store.value(count).value == model::Value("-7"s));
    CHECK(store.save().has_value());
    REQUIRE(actions.called.size() == 1);
    CHECK(actions.called[0].arguments == std::vector<model::Value>{ 12.5f });

    // nothing is written, run or sent for a text that is no number of the field's type
    events.sent.clear();
    for (const auto* text : { "", "-", "1-2", "abc", "1e" }) {
        CHECK_FALSE(store.set(strength, std::string(text)));
    }
    CHECK_FALSE(store.set(count, "1.5"s));
    CHECK(store.value(strength).value == model::Value("12.5"s));
    CHECK(store.value(count).value == model::Value("-7"s));
    CHECK(actions.called.size() == 1);
    CHECK(events.sent.empty());

    // a global keeps it as a number
    const auto global = field(model::GlobalStorage{ .plugin = PLUGIN, .formId = 0x801 }, Type::Float);
    CHECK(store.set(global, "0.25"s));
    CHECK(store.save().has_value());
    CHECK(globals.values.at({ PLUGIN, 0x801 }) == 0.25f);
    CHECK(store.value(global).value == model::Value("0.25"s));
}

TEST_CASE("McmStore: a choice of files keeps the name of its file, and an empty text for None")
{
    const test_files::TempFolder mcm;
    mcm.write("Config/MyMod/settings.ini", "[Main]\nsPreset=b.xml\n");
    auto properties = someProperties();
    TestActions actions;
    McmStore store(mcm.path);
    store.open({ .name = "MyMod", .displayName = {}, .frontPage = {}, .pages = {} });
    const model::ValueOptions files{ .options = { { .value = ""s, .text = "None" }, { .value = "A.xml"s, .text = "A.xml" }, { .value = "b.xml"s, .text = "b.xml" } } };

    // a ModSetting, which has its default from the mod's file
    auto preset = settingOf(SettingType::Dropdown, model::ModSettingStorage{ .name = "sPreset:Main", .type = Type::String }, files);
    preset.action = model::CallFunction{ .plugin = PLUGIN,
        .formId = 0x800,
        .script = {},
        .function = "Load",
        .arguments = { ""s, ""s },
        .valueArguments = { { .place = 0, .text = "{value}", .type = Type::String }, { .place = 1, .text = "Loading {value}!", .type = Type::String } } };
    CHECK(store.value(preset).available);
    CHECK(store.value(preset).value == model::Value("b.xml"s));
    CHECK(store.defaultValue(preset) == model::Value("b.xml"s));
    CHECK(store.set(preset, "A.xml"s));
    CHECK(store.value(preset).value == model::Value("A.xml"s));
    CHECK(store.save().has_value());
    CHECK(store.set(preset, ""s));
    CHECK(store.value(preset).value == model::Value(""s));
    CHECK(store.save().has_value());

    // the action of the row has the name of the file in its arguments
    REQUIRE(actions.called.size() == 2);
    CHECK(actions.called[0].arguments == std::vector<model::Value>{ "A.xml"s, "Loading A.xml!"s });

    // a property of a script, whose name is no file: no option is chosen
    const auto name = settingOf(SettingType::Dropdown, model::PropertyStorage{ .plugin = PLUGIN, .formId = 0x800, .script = {}, .name = "sName", .type = Type::String }, files);
    CHECK(store.value(name).value == model::Value("Sanctuary Hills"s));
    CHECK(store.set(name, "b.xml"s));
    CHECK(store.save().has_value());
    CHECK(properties.values.at({ PLUGIN, 0x800, "", "sName" }) == model::Value("b.xml"s));

    // one that is kept nowhere is at None
    CHECK(store.value(settingOf(SettingType::Dropdown, model::NoStorage{}, files)).value == model::Value(""s));
}

TEST_CASE("McmStore: a choice reads its options in the game as its mod is selected and as another page is shown, and is not available while its list is not in the game")
{
    auto globals = someGlobals();
    TestProperties lists;
    lists.lists[{ PLUGIN, 0x900, "", "" }] = { "Sanctuary", "Red Rocket" };
    lists.lists[{ PLUGIN, 0x800, "MyMod:Config", "Names" }] = { "Nate", "Nora" };
    model::Mod mod{ .name = "MyMod", .displayName = {}, .frontPage = {}, .pages = {} };
    mod.frontPage.items.push_back(globalSetting(SettingType::Dropdown, 0x800, { .gameOptions = model::GameOptions{ .plugin = PLUGIN, .formId = 0x900 } }));
    mod.frontPage.items.push_back(
        globalSetting(SettingType::Stepper, 0x801, { .gameOptions = model::GameOptions{ .plugin = PLUGIN, .formId = 0x800, .script = "MyMod:Config", .property = "Names" } }));
    mod.frontPage.items.push_back(globalSetting(SettingType::Dropdown, 0x803, { .gameOptions = model::GameOptions{ .plugin = PLUGIN, .formId = 0x901 } }));
    const auto settingAt = [&mod](const std::size_t row) -> const model::Setting& {
        return std::get<model::Setting>(mod.frontPage.items[row]);
    };
    McmStore store;
    using Options = std::vector<model::Option>;

    // none until the mod is selected, and then each option has its place as its value, as MCM stores it
    CHECK(store.options(settingAt(0)).empty());
    store.open(mod);
    CHECK(store.options(settingAt(0)) == Options{ { 0, "Sanctuary" }, { 1, "Red Rocket" } });
    CHECK(store.options(settingAt(1)) == Options{ { 0, "Nate" }, { 1, "Nora" } });
    CHECK(store.value(settingAt(0)).value == model::Value(1));
    CHECK(store.set(settingAt(1), 1));
    CHECK(store.save().has_value());
    CHECK(globals.values.at({ PLUGIN, 0x801 }) == 1.0f);

    const auto missing = store.value(settingAt(2));
    CHECK_FALSE(missing.available);
    CHECK(missing.reason == "Not available: the FormList MyMod.esp|901 of its options is not in the game.");

    // a script can change its list, which is read again as another page is shown
    lists.lists[{ PLUGIN, 0x900, "", "" }].push_back("Abernathy Farm");
    CHECK(store.options(settingAt(0)).size() == 2);
    store.pageShown();
    CHECK(store.options(settingAt(0)).size() == 3);

    store.close();
    CHECK(store.options(settingAt(0)).empty());
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

TEST_CASE("McmStore: the default of a ModSetting is the value of the mod's file, whatever the player's file has")
{
    const test_files::TempFolder mcm;
    mcm.write("Config/MyMod/settings.ini", "[Main]\nbOn=1\nfStrength=1.0\niMode=1\nsName=Sanctuary Hills\nfWord=strong\n");
    mcm.write("Settings/MyMod.ini", "[Main]\nbOn=0\nfStrength=1.300000\nbPlayer=1\n");
    auto globals = someGlobals();
    McmStore store(mcm.path);
    store.open({ .name = "MyMod", .displayName = {}, .frontPage = {}, .pages = {} });

    const auto on = modSetting(SettingType::Switcher, "bOn:Main", Type::Bool);
    const auto strength = modSetting(SettingType::Slider, "fStrength:Main", Type::Float);
    CHECK(store.value(on).value == model::Value(false));
    CHECK(store.defaultValue(on) == model::Value(true));
    CHECK(store.defaultValue(strength) == model::Value(1.0f));
    CHECK(store.defaultValue(modSetting(SettingType::Dropdown, "iMode:Main", Type::Int)) == model::Value(1));
    CHECK(store.defaultValue(modSetting(SettingType::TextInput, "sName:Main", Type::String)) == model::Value("Sanctuary Hills"s));

    // it stays as the value is set, to another and to the default
    CHECK(store.set(strength, 0.5f));
    CHECK(store.defaultValue(strength) == model::Value(1.0f));
    CHECK(store.set(strength, 1.0f));
    CHECK(store.value(strength).value == model::Value(1.0f));
    CHECK(store.defaultValue(strength) == model::Value(1.0f));

    // one the player's file has alone, one that is in neither file, and one whose default is no number
    CHECK(store.value(modSetting(SettingType::Switcher, "bPlayer:Main", Type::Bool)).available);
    CHECK_FALSE(store.defaultValue(modSetting(SettingType::Switcher, "bPlayer:Main", Type::Bool)));
    CHECK_FALSE(store.defaultValue(modSetting(SettingType::Switcher, "bMissing:Main", Type::Bool)));
    CHECK_FALSE(store.defaultValue(modSetting(SettingType::Slider, "fWord:Main", Type::Float)));

    // the mod that is left has none
    store.close();
    CHECK_FALSE(store.defaultValue(on));
}

TEST_CASE("McmStore: a global, a property of a script and a setting that is kept nowhere have no default")
{
    auto globals = someGlobals();
    auto properties = someProperties();
    McmStore store;
    store.open({ .name = "MyMod", .displayName = {}, .frontPage = {}, .pages = {} });

    CHECK_FALSE(store.defaultValue(globalSetting(SettingType::Switcher, 0x800)));
    CHECK_FALSE(store.defaultValue(propertySetting(SettingType::Switcher, "bOn", Type::Bool)));
    CHECK_FALSE(store.defaultValue(settingOf(SettingType::Switcher, model::NoStorage{})));
}

TEST_CASE("McmStore: a hotkey has the key the player's file has for it, by the names of MCM's menu")
{
    const test_files::TempFolder mcm;
    mcm.write("Config/MyMod/keybinds.json", R"({
        // MCM's reader skips a comment
        "modName": "MyMod",
        "keybinds": [
            { "id": "OpenMenu", "desc": "Open the menu", "action": { "type": "CallFunction", "form": "MyMod.esp|800", "function": "Open" } },
            { "id": "Console", "desc": "A command", "action": { "type": "RunConsoleCommand", "command": "tgm" } },
            { "id": "Mouse", "desc": "A button of the mouse", "action": { "type": "CallGlobalFunction", "script": "Debug", "function": "Trace" } },
            { "id": "Unnamed", "desc": "A key with no name", "action": { "type": "SendEvent", "form": "MyMod.esp|800" } },
            { "id": "TwoKeys", "desc": "Two keys", "action": { "type": "RunConsoleCommand", "command": "tgm" } },
            { "id": "NoKey", "desc": "No key", "action": { "type": "RunConsoleCommand", "command": "tgm" } }
        ]
    })");
    mcm.write("Settings/Keybinds.json", R"({ "version": 1, "keybinds": [
        { "keycode": 116, "modifiers": 0, "modName": "MyMod", "id": "OpenMenu" },
        { "keycode": 96, "modifiers": 7, "modName": "mymod", "id": "console" },
        { "keycode": 260, "modifiers": 2, "modName": "MyMod", "id": "Mouse" },
        { "keycode": 270, "modifiers": 1, "modName": "MyMod", "id": "Unnamed" },
        { "keycode": 90, "modifiers": 0, "modName": "MyMod", "id": "TwoKeys" },
        { "keycode": 65, "modifiers": 4, "modName": "MyMod", "id": "TwoKeys" },
        { "keycode": 66, "modifiers": 0, "modName": "Another", "id": "NoKey" }
    ] })");
    McmStore store(mcm.path);
    store.open({ .name = "MyMod", .displayName = {}, .frontPage = {}, .pages = {} });
    const auto keyOf = [&store](std::string id) {
        const auto key = store.hotkey({ .id = std::move(id), .text = "A hotkey" });
        CHECK(key.available);
        CHECK(key.reason.empty());
        return std::get<std::string>(key.value);
    };

    CHECK(keyOf("OpenMenu") == "F5");
    // a mod and a hotkey are known in any letter case, and the keys that are held are in the order of MCM's menu
    CHECK(keyOf("Console") == "Ctrl-Alt-Shift-NumPad0");
    CHECK(keyOf("openmenu") == "F5");
    CHECK(keyOf("Mouse") == "Ctrl-Mouse5");
    // a key MCM's menu has no name for is its code
    CHECK(keyOf("Unnamed") == "Shift-Key 270");
    // the lower of two keys, as MCM's menu shows it
    CHECK(keyOf("TwoKeys") == "Alt-A");
    // the key of another mod's hotkey of that id is not this one's
    CHECK(keyOf("NoKey") == "None");
}

TEST_CASE("McmStore: a hotkey the mod's keybinds file does not have is not available, with the reason")
{
    const test_files::TempFolder mcm;
    mcm.write("Config/MyMod/keybinds.json", R"({ "modName": "MyMod", "keybinds": [
        { "id": "OpenMenu", "desc": "Open the menu", "action": { "type": "RunConsoleCommand", "command": "tgm" } },
        { "id": "NoAction", "desc": "With no action" },
        { "id": "OtherAction", "desc": "With an action MCM does not have", "action": { "type": "NoSuchAction" } },
        "not a hotkey"
    ] })");
    mcm.write("Config/Renamed/keybinds.json", R"({ "modName": "AnotherName", "keybinds": [
        { "id": "OpenMenu", "desc": "Open the menu", "action": { "type": "RunConsoleCommand", "command": "tgm" } }
    ] })");
    mcm.write("Config/Broken/keybinds.json", "{ not JSON");
    mcm.write("Settings/Keybinds.json", R"({ "version": 1, "keybinds": [
        { "keycode": 116, "modifiers": 0, "modName": "MyMod", "id": "OpenMenu" },
        { "keycode": 117, "modifiers": 0, "modName": "MyMod", "id": "Missing" },
        { "keycode": 118, "modifiers": 0, "modName": "MyMod", "id": "NoAction" },
        { "keycode": 119, "modifiers": 0, "modName": "Renamed", "id": "OpenMenu" },
        { "keycode": 120, "modifiers": 0, "modName": "Broken", "id": "OpenMenu" },
        { "keycode": 121, "modifiers": 0, "modName": "NoFile", "id": "OpenMenu" }
    ] })");
    McmStore store(mcm.path);
    const auto open = [&store](std::string name) {
        store.open({ .name = std::move(name), .displayName = {}, .frontPage = {}, .pages = {} });
    };
    const auto reasonOf = [&store](std::string id) {
        const auto key = store.hotkey({ .id = std::move(id), .text = "A hotkey" });
        CHECK_FALSE(key.available);
        CHECK(key.value == model::Value("None"s));
        return key.reason;
    };

    // MCM binds no key to a hotkey the mod's file does not have, and drops the key the player's file has for it
    open("MyMod");
    CHECK(store.hotkey({ .id = "OpenMenu", .text = "A hotkey" }).available);
    CHECK(reasonOf("Missing") == "Not available: the hotkey Missing is not in the keybinds.json of the mod.");
    CHECK(reasonOf("NoAction") == "Not available: the hotkey NoAction is not in the keybinds.json of the mod.");
    CHECK(reasonOf("OtherAction") == "Not available: the hotkey OtherAction is not in the keybinds.json of the mod.");
    CHECK(reasonOf("") == "Not available: the hotkey has no id.");

    // a file that names another mod, one that is no JSON, and none
    for (const auto* mod : { "Renamed", "Broken", "NoFile" }) {
        open(mod);
        CHECK(reasonOf("OpenMenu") == "Not available: the hotkey OpenMenu is not in the keybinds.json of the mod.");
    }

    // and with no mod selected, and with no folder of MCM
    store.close();
    CHECK_FALSE(store.hotkey({ .id = "OpenMenu", .text = "A hotkey" }).available);
    McmStore noFolder;
    noFolder.open({ .name = "MyMod", .displayName = {}, .frontPage = {}, .pages = {} });
    CHECK_FALSE(noFolder.hotkey({ .id = "OpenMenu", .text = "A hotkey" }).available);
}

TEST_CASE("McmStore: the key of a hotkey is read as its mod is selected, and with no player's file it has none")
{
    const test_files::TempFolder mcm;
    mcm.write("Config/MyMod/keybinds.json", R"({ "modName": "MyMod", "keybinds": [
        { "id": "OpenMenu", "desc": "Open the menu", "action": { "type": "RunConsoleCommand", "command": "tgm" } }
    ] })");
    const model::Mod mod{ .name = "MyMod", .displayName = {}, .frontPage = {}, .pages = {} };
    const model::Hotkey hotkey{ .id = "OpenMenu", .text = "Open the menu" };
    McmStore store(mcm.path);
    store.open(mod);
    CHECK(store.hotkey(hotkey).available);
    CHECK(store.hotkey(hotkey).value == model::Value("None"s));

    // MCM writes the file as its own menu closes
    mcm.write("Settings/Keybinds.json", R"({ "version": 1, "keybinds": [{ "keycode": 71, "modifiers": 0, "modName": "MyMod", "id": "OpenMenu" }] })");
    CHECK(store.hotkey(hotkey).value == model::Value("None"s));
    store.open(mod);
    CHECK(store.hotkey(hotkey).value == model::Value("G"s));

    // a file of a version MCM does not read has no keys
    mcm.write("Settings/Keybinds.json", R"({ "version": 0, "keybinds": [{ "keycode": 71, "modifiers": 0, "modName": "MyMod", "id": "OpenMenu" }] })");
    store.open(mod);
    CHECK(store.hotkey(hotkey).value == model::Value("None"s));
}

TEST_CASE("McmStore: the hotkeys of the MCM showcase are in its keybinds file, but for the one that shows one that is not")
{
    const auto mods = mcm::loadConfigs(MODS_CONFIG_MCM_SHOWCASE_DIR);
    REQUIRE(mods.size() == 1);
    McmStore store(std::filesystem::path(MODS_CONFIG_MCM_SHOWCASE_DIR).parent_path());
    store.open(mods[0]);

    CHECK(store.hotkey({ .id = "TestHotkey", .text = {} }).available);
    CHECK(store.hotkey({ .id = "TestHotkeyA", .text = {} }).available);
    CHECK(store.hotkey({ .id = "TestHotkeyB", .text = {} }).available);
    CHECK_FALSE(store.hotkey({ .id = "TestHotkeyMissing", .text = {} }).available);
}

TEST_CASE("McmStore: a hidden switcher of the MCM showcase that is on shows the row of its group, and one that is off does not")
{
    const auto mods = mcm::loadConfigs(MODS_CONFIG_MCM_SHOWCASE_DIR);
    REQUIRE(mods.size() == 1);
    McmStore store(std::filesystem::path(MODS_CONFIG_MCM_SHOWCASE_DIR).parent_path());
    store.open(mods[0]);

    const auto page = std::ranges::find(mods[0].pages, "Buttons, text input and others", &model::Page::name);
    REQUIRE(page != mods[0].pages.end());
    const auto section = std::ranges::find_if(page->items, [](const model::Item& item) {
        const auto* group = std::get_if<model::Group>(&item);
        return group && group->text == "Hidden switchers";
    });
    REQUIRE(section != page->items.end());

    // the two hidden switchers, and the text row of the group of each
    const auto& rows = std::get<model::Group>(*section).items;
    REQUIRE(rows.size() == 4);
    CHECK_FALSE(model::holds(*std::get<model::Setting>(rows[0]).visibleWhen, &store));
    CHECK_FALSE(model::holds(*std::get<model::Setting>(rows[1]).visibleWhen, &store));
    CHECK(model::holds(*std::get<model::Text>(rows[2]).visibleWhen, &store));
    CHECK_FALSE(model::holds(*std::get<model::Text>(rows[3]).visibleWhen, &store));
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
    CHECK(store.valueById("bMissing:Switchers").value == model::Value(false));
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

TEST_CASE("McmStore: a setting that is saved sends MCM's event, with its mod and the id of its row")
{
    auto globals = someGlobals();
    TestEvents events;
    McmStore store;
    store.open(model::Mod{ .name = "MyMod", .displayName = {}, .frontPage = {}, .pages = {} });
    events.sent.clear();

    auto withId = globalSetting(SettingType::Switcher, 0x801);
    withId.sourceId = "bOn";
    CHECK(store.set(withId, true));
    CHECK(events.sent.empty());
    CHECK(store.save().has_value());
    CHECK(events.sent == Events{ "OnMCMSettingChange(MyMod, bOn)", "OnMCMSettingChange|MyMod(MyMod, bOn)" });

    // a row with no id in its config, as MCM's menu sends none for it
    events.sent.clear();
    const auto noId = globalSetting(SettingType::Switcher, 0x801);
    CHECK(store.set(noId, false));
    CHECK(store.save().has_value());
    CHECK(events.sent.empty());

    // a value that is not written
    auto missing = globalSetting(SettingType::Switcher, 0x1234);
    missing.sourceId = "bMissing";
    CHECK_FALSE(store.set(missing, true));
    CHECK(events.sent.empty());
}

TEST_CASE("McmStore: a Save that wrote a ModSetting tells that the pause menu opened and closed, and only that one")
{
    const test_files::TempFolder mcm;
    mcm.write("Config/MyMod/settings.ini", "[Main]\nbOn=0\n");
    auto globals = someGlobals();
    TestEvents events;
    McmStore store(mcm.path);
    store.open({ .name = "MyMod", .displayName = {}, .frontPage = {}, .pages = {} });

    // a global, and a ModSetting MCM does not have, which is held here: no file is written
    const auto global = globalSetting(SettingType::Switcher, 0x801);
    const auto missing = modSetting(SettingType::Switcher, "bMissing:Main", Type::Bool);
    CHECK(store.set(global, true));
    CHECK(store.set(missing, true));
    events.sent.clear();
    CHECK(store.save().has_value());
    CHECK(events.sent.empty());

    const auto on = modSetting(SettingType::Switcher, "bOn:Main", Type::Bool);
    CHECK(store.set(on, true));
    CHECK(store.save().has_value());
    CHECK(events.sent == Events{ "PauseMenu(opening)", "PauseMenu(closing)" });

    // the menu closes with nothing more
    events.sent.clear();
    store.close();
    store.menuClosed();
    CHECK(events.sent == Events{ "OnMCMMenuClose|MyMod", "OnMCMClose" });
}

TEST_CASE("McmStore: the action of a button calls its function, and no other action is run")
{
    TestActions actions;
    TestEvents events;
    McmStore store;
    store.open(model::Mod{ .name = "MyMod", .displayName = {}, .frontPage = {}, .pages = {} });
    events.sent.clear();

    const model::CallFunction call{ .plugin = PLUGIN, .formId = 0x800, .script = "MyMod:Main", .function = "Apply", .arguments = { 40, true, "a text"s } };
    CHECK(store.run(call).ran);
    const model::CallFunction global{ .plugin = {}, .formId = 0, .script = "Debug", .function = "Notification", .arguments = { "Hi"s } };
    CHECK(store.run(global).ran);
    CHECK(actions.called == std::vector{ call, global });

    // a button is no setting: MCM's menu sends no event for it
    CHECK(events.sent.empty());

    CHECK_FALSE(store.run(model::NoAction{}).ran);
    CHECK_FALSE(store.run(model::UnknownAction{ .type = "CallExternalFunction", .reason = {} }).ran);
    CHECK(actions.called.size() == 2);

    // a function of a form that is not in the game
    actions.inGame = false;
    CHECK_FALSE(store.run(call).ran);
    CHECK(actions.called.size() == 2);
}

TEST_CASE("McmStore: a setting that is saved runs the action of its row, with the value in its arguments")
{
    auto globals = someGlobals();
    TestActions actions;
    McmStore store;
    store.open(model::Mod{ .name = "MyMod", .displayName = {}, .frontPage = {}, .pages = {} });

    using Arguments = std::vector<model::Value>;
    const model::CallFunction call{ .plugin = PLUGIN,
        .formId = 0x800,
        .script = {},
        .function = "Apply",
        .arguments = { ""s, ""s, ""s, ""s, "kept"s },
        .valueArguments = { { .place = 0, .text = "{value}", .type = Type::Float },
            { .place = 1, .text = "{value}", .type = Type::Bool },
            { .place = 2, .text = "{value}", .type = Type::Int },
            { .place = 3, .text = "Is {value} of {value}", .type = Type::String } } };

    // a switcher is 1 and 0, in the type of each argument. Only the first `{value}` of a text is the value
    auto switcher = globalSetting(SettingType::Switcher, 0x801);
    switcher.action = call;
    CHECK(store.set(switcher, true));
    CHECK(store.save().has_value());
    REQUIRE(actions.called.size() == 1);
    CHECK(actions.called[0].function == "Apply");
    CHECK(actions.called[0].arguments == Arguments{ 1.0f, true, 1, "Is 1 of {value}"s, "kept"s });
    CHECK(actions.called[0].valueArguments.empty());
    CHECK(store.set(switcher, false));
    CHECK(store.save().has_value());
    CHECK(actions.called[1].arguments == Arguments{ 0.0f, false, 0, "Is 0 of {value}"s, "kept"s });

    // a number, which a whole number cuts at its point, and the place of a choice's option
    auto slider = globalSetting(SettingType::Slider, 0x802);
    slider.action = call;
    CHECK(store.set(slider, 1.75f));
    CHECK(store.save().has_value());
    CHECK(actions.called[2].arguments == Arguments{ 1.75f, true, 1, "Is 1.75 of {value}"s, "kept"s });
    auto choice = globalSetting(SettingType::Dropdown, 0x803);
    choice.action = call;
    CHECK(store.set(choice, 1));
    CHECK(store.save().has_value());
    CHECK(actions.called[3].arguments == Arguments{ 1.0f, true, 1, "Is 1 of {value}"s, "kept"s });

    // a text, which is 0 as a number, of a setting with no storage, which runs it as it is set
    auto text = settingOf(SettingType::TextInput, model::NoStorage{});
    text.action = call;
    CHECK(store.set(text, "Sanctuary"s));
    CHECK(actions.called[4].arguments == Arguments{ 0.0f, false, 0, "Is Sanctuary of {value}"s, "kept"s });

    // a global function, which has no form, the same
    auto global = globalSetting(SettingType::Switcher, 0x801);
    global.action = model::CallFunction{ .plugin = {},
        .formId = 0,
        .script = "M8rJourney",
        .function = "SetQuestRunning",
        .arguments = { ""s },
        .valueArguments = { { .place = 0, .text = "{value}", .type = Type::Bool } } };
    CHECK(store.set(global, true));
    CHECK(store.save().has_value());
    REQUIRE(actions.called.size() == 6);
    CHECK(actions.called[5].script == "M8rJourney");
    CHECK(actions.called[5].arguments == Arguments{ true });
}

TEST_CASE("McmStore: no action is run for a value that is not set, nor one the store does not run")
{
    auto globals = someGlobals();
    TestActions actions;
    TestEvents events;
    McmStore store;
    store.open(model::Mod{ .name = "MyMod", .displayName = {}, .frontPage = {}, .pages = {} });
    events.sent.clear();

    const model::CallFunction call{ .plugin = PLUGIN, .formId = 0x800, .script = {}, .function = "Apply", .arguments = {}, .valueArguments = {} };
    auto missing = globalSetting(SettingType::Switcher, 0x1234);
    missing.action = call;
    CHECK_FALSE(store.set(missing, true));
    CHECK(actions.called.empty());

    auto other = globalSetting(SettingType::Switcher, 0x801);
    other.action = model::UnknownAction{ .type = "CallExternalFunction", .reason = {} };
    CHECK(store.set(other, true));
    CHECK(store.save().has_value());
    CHECK(actions.called.empty());

    // a function of a form that is not in the game: the value is set, and its event is sent
    actions.inGame = false;
    auto withId = globalSetting(SettingType::Switcher, 0x803);
    withId.sourceId = "bOn";
    withId.action = call;
    CHECK(store.set(withId, true));
    CHECK(store.save().has_value());
    CHECK(actions.called.empty());
    CHECK(globals.values.at({ PLUGIN, 0x803 }) == 1.0f);
    CHECK(events.sent == Events{ "OnMCMSettingChange(MyMod, bOn)", "OnMCMSettingChange|MyMod(MyMod, bOn)" });
}
