#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <format>
#include <functional>
#include <sstream>
#include <unordered_set>

#include "TestConfigSpecs.h"
#include "TestFiles.h"
#include "common/CommonUtils.h"
#include "f4vr/WandActivationSphere.h"
#include "model/Categories.h"
#include "model/Colors.h"
#include "model/InputBindings.h"
#include "model/OptionSets.h"
#include "model/TextMarkup.h"
#include "model/Transforms.h"
#include "spec/ConfigSpecLoader.h"

using namespace mods_config;
using namespace test_config_specs;

namespace
{
    /**
     * The texts of the items of a group, with the rows of each section that folds after its name.
     */
    Lines rowTexts(const model::Group& group)
    {
        Lines texts;
        for (const auto& item : group.items) {
            if (const auto* setting = std::get_if<model::Setting>(&item)) {
                texts.push_back(setting->text);
            } else if (const auto* placeholder = std::get_if<model::Placeholder>(&item)) {
                texts.push_back(placeholder->text);
            } else if (const auto* section = std::get_if<model::Group>(&item)) {
                texts.push_back(section->text + ":");
                std::ranges::copy(rowTexts(*section), std::back_inserter(texts));
            }
        }
        return texts;
    }
}

TEST_CASE("ConfigSpecLoader: an activation sphere is a group of its section's keys, its main rows and then Position and size, Activation and Visuals, folded")
{
    const auto mod = loaded(sphereSpec(R"({ "type": "cf.activationSphere", "text": "Body grab", "help": "Where the hand reaches.", "storage": { "path": ["Spheres", "Body"] } })"));
    CHECK(mod.problems.empty());
    REQUIRE(mod.frontPage.items.size() == 1);
    const auto& group = std::get<model::Group>(mod.frontPage.items[0]);
    CHECK(group.text == "Body grab");
    CHECK(group.help == "Where the hand reaches.");
    CHECK(group.role == model::GroupRole::ActivationSphere);
    CHECK_FALSE(group.collapsible);
    CHECK(rowTexts(group) == Lines{ "Activate",
                                 "Entry haptic",
                                 "Icons",
                                 "Position and size:",
                                 "Zone",
                                 "Activation:",
                                 "Primary haptic",
                                 "Secondary activate",
                                 "Secondary haptic",
                                 "Visuals:",
                                 "Show icon",
                                 "Icon",
                                 "Icon color",
                                 "Icon size",
                                 "Show binding icon",
                                 "Binding icons",
                                 "Binding icon position and size",
                                 "Binding icon color",
                                 "Show sphere",
                                 "Sphere style" });
    for (std::size_t place = 3; place < 6; ++place) {
        const auto& section = std::get<model::Group>(group.items[place]);
        CHECK(section.collapsible);
        CHECK(section.collapsed);
        CHECK(section.role == model::GroupRole::None);
    }

    // each key is a setting of its own, by its key and the section, with the control of its kind, and `live` with none above
    // it; Icons has no applies of its own
    const auto settings = settingsOf(mod);
    REQUIRE(settings.size() == 17);
    for (const auto& setting : settings) {
        CHECK(setting.applies == (model::setsOthers(setting) ? model::Applies::Unknown : model::Applies::Live));
        CHECK_FALSE(setting.help.empty());
    }
    CHECK(settings[0].id == "kPrimaryBinding:Spheres.Body");
    CHECK(settings[0].storage == fileValue("main", { "Spheres", "Body", "kPrimaryBinding" }));
    CHECK(settings[0].type == model::SettingType::InputBinding);
    CHECK(settings[0].valueOptions.suppress);
    CHECK(settings[1].type == model::SettingType::Haptic);
    CHECK(settings[2].type == model::SettingType::Stepper);
    CHECK(model::setsOthers(settings[2]));
    CHECK(settings[7].type == model::SettingType::Dropdown);
    // which icons are drawn, and when the icon, the binding icon and the sphere are: each option says what it means
    for (const auto place : { 2, 7, 11, 15 }) {
        REQUIRE(settings[place].valueOptions.options.size() == 4);
        for (const auto& option : settings[place].valueOptions.options) {
            CHECK_FALSE(option.help.empty());
        }
    }
    CHECK(settings[3].type == model::SettingType::Transform);
    CHECK(settings[8].type == model::SettingType::Dropdown);
    REQUIRE(settings[8].valueOptions.folderOptions);
    CHECK(settings[8].valueOptions.folderOptions->path == R"(Textures\MyMod\f4cf)");
    CHECK(settings[8].valueOptions.folderOptions->mask == "activation-icon-*.dds");
    CHECK(settings[9].type == model::SettingType::Color);
    CHECK(settings[10].type == model::SettingType::Slider);

    // what the framework reads for a key the file does not have
    const f4vr::WandActivationConfig framework;
    const auto text = [&settings](const std::size_t place) {
        return std::get<std::string>(*settings[place].defaultValue);
    };
    CHECK(text(0) == model::bindingText(framework.primary, model::TableForm::Toml));
    CHECK(text(1) == "Tick");
    CHECK(text(4) == "DoubleClick");
    CHECK(text(5) == model::bindingText(framework.secondary, model::TableForm::Toml));
    CHECK(text(6) == "DoubleClick");
    // the framework's icon, by its path in the mod's folder of Data
    CHECK(text(8) == R"(Textures\MyMod\)" + framework.iconStyle.textureOrDefault());
    CHECK(framework.showIcon == f4vr::parseActivationSphereVisibility(text(7), f4vr::ActivationSphereVisibility::Always));
    CHECK(framework.showSphere == f4vr::parseActivationSphereVisibility(text(15), f4vr::ActivationSphereVisibility::Always));
    CHECK(f4vr::findSphereStylePreset(text(16)) == framework.sphereStyle);
    CHECK(common::fEqual(std::get<float>(*settings[10].defaultValue), framework.iconStyle.size));
    const auto iconColor = model::colorOf(settings[9], *settings[9].defaultValue);
    REQUIRE(iconColor);
    CHECK(std::abs(iconColor->a - framework.iconStyle.color.a) < 0.01f);

    // every style the list has is one the framework has
    CHECK(settings[16].valueOptions.options.size() == 33);
    for (const auto& option : settings[16].valueOptions.options) {
        CHECK(f4vr::findSphereStylePreset(std::get<std::string>(option.value)));
    }
}

TEST_CASE("ConfigSpecLoader: a sphere's power-armor zone has a row while its file has the key, or its config spec gives it a default")
{
    const test_files::TempFolder myGames;
    myGames.write("MyMod.toml",
        "[Body]\r\n"
        "tZonePA = { scale = 24.0 }\r\n"
        "[Head]\r\n"
        "tZone = { scale = 18.0 }\r\n"
        "[Hand]\r\n"
        "tZone = { scale = 18.0 }\r\n"
        "[Foot]\r\n"
        "tZonePA = { scale = 24.0 }\r\n");
    const auto mod = spec::loadConfigSpec(sphereSpec(R"({ "type": "cf.activationSphere", "text": "Body", "storage": { "path": ["Body"] } },
        { "type": "cf.activationSphere", "text": "Head", "storage": { "path": ["Head"] }, "keys": { "tZonePA": { "text": "Named" } } },
        { "type": "cf.activationSphere", "text": "Hand", "storage": { "path": ["Hand"] }, "keys": { "tZonePA": { "default": { "scale": 2.0 } } } },
        { "type": "cf.activationSphere", "text": "Foot", "storage": { "path": ["Foot"] }, "keys": { "tZonePA": false } })"),
        {},
        {},
        { .data = {}, .myGames = myGames.path });
    REQUIRE(mod);
    const auto hasRow = [&mod](const std::string& section) {
        return std::ranges::any_of(settingsOf(*mod), [&section](const model::Setting& setting) {
            return setting.id == "tZonePA:" + section;
        });
    };
    CHECK(hasRow("Body"));
    CHECK_FALSE(hasRow("Head"));
    CHECK(hasRow("Hand"));
    CHECK_FALSE(hasRow("Foot"));

    // what the framework reads with no key is the zone, which is no value to reset it to
    const auto settings = settingsOf(*mod);
    const auto zone = [&settings](const std::string& id) {
        return std::ranges::find(settings, id, &model::Setting::id);
    };
    CHECK_FALSE(zone("tZonePA:Body")->defaultValue);
    CHECK(zone("tZonePA:Hand")->defaultValue == model::Value("{ scale = 2.0 }"s));
    CHECK(problemLines(*mod) == Lines{ "W Head: Its file \"main\" has no tZonePA and it gives it no default: it has no row. [Head] content[1].keys.tZonePA" });
}

TEST_CASE("ConfigSpecLoader: a sphere whose file has no section of it is a warning, unless the file omits its defaults, which its keys then do")
{
    const test_files::TempFolder myGames;
    myGames.write("MyMod.toml", "[Other]\r\nx = 1\r\n");
    const auto sphere = R"({ "type": "cf.activationSphere", "text": "Body", "storage": { "path": ["Body"] } })";
    const auto plain = spec::loadConfigSpec(sphereSpec(sphere), {}, {}, { .data = {}, .myGames = myGames.path });
    REQUIRE(plain);
    CHECK(problemLines(*plain) ==
          Lines{ "W Body: Its file \"main\" has no section Body: each key shows its default, and a Save writes a key once it is changed. [Body] content[0].storage" });

    auto omitting = sphereSpec(sphere);
    const std::string file = R"("path": "MyMod.toml" })";
    omitting.replace(omitting.find(file), file.size(), R"("path": "MyMod.toml", "omitDefault": true })");
    const auto omitted = spec::loadConfigSpec(omitting, {}, {}, { .data = {}, .myGames = myGames.path });
    REQUIRE(omitted);
    CHECK(omitted->problems.empty());
    // each key with a default omits it; the zone, which has none, keeps its value
    for (const auto& setting : settingsOf(*omitted)) {
        INFO(setting.id);
        CHECK(setting.omitDefault == (setting.defaultValue.has_value() && !model::setsOthers(setting)));
    }

    // a sphere's own omitDefault, in a file that keeps its defaults, is its keys'
    const auto own = spec::loadConfigSpec(sphereSpec(R"({ "type": "cf.activationSphere", "text": "Body", "omitDefault": true, "storage": { "path": ["Body"] } })"),
        {},
        {},
        { .data = {}, .myGames = myGames.path });
    REQUIRE(own);
    CHECK(own->problems.empty());
    for (const auto& setting : settingsOf(*own)) {
        INFO(setting.id);
        CHECK(setting.omitDefault == (setting.defaultValue.has_value() && !model::setsOthers(setting)));
    }
}

TEST_CASE("ConfigSpecLoader: a sphere's keys name a row, hide it or show one that has none, and each is a setting of the fields it gives")
{
    const auto mod =
        loaded(sphereSpec(R"({ "type": "cf.activationSphere", "text": "Body", "applies": "save", "storage": { "path": ["Body"] }, "visibleWhen": { "plugin": "Some.esp" },
        "keys": {
            "kPrimaryBinding": { "text": "Take the light", "help": "Takes it.", "valueOptions": { "suppress": false, "hands": ["offhand"] }, "default": { "hand": "offhand", "type": "tap", "button": "grip" } },
            "tZone": { "valueOptions": { "rotation": false, "scale": { "max": 60 } } },
            "tZonePA": { "default": { "scale": 2.0 } },
            "sShowIcon": false,
            "sIcon": true,
            "kSecondaryBinding": false,
            "fSphereGlow": { "applies": "restart" },
            "sShowSphere": { "enabledWhen": { "setting": "tZone:Body", "notEquals": "{ scale = 1.0 }" } }
        } },
        { "type": "cf.activationSphere", "text": "Head", "storage": { "path": ["Head"] }, "keys": { "tZone": false, "sShowSphere": false, "cSphereColor": true } })"));
    CHECK(mod.problems.empty());
    REQUIRE(mod.frontPage.items.size() == 2);
    const auto& body = std::get<model::Group>(mod.frontPage.items[0]);
    CHECK(rowTexts(body) == Lines{ "Take the light",
                                "Entry haptic",
                                "Position and size:",
                                "Zone",
                                "Zone in power armor",
                                "Activation:",
                                "Primary haptic",
                                "Secondary haptic",
                                "Visuals:",
                                "Show binding icon",
                                "Binding icons",
                                "Binding icon position and size",
                                "Binding icon color",
                                "Show sphere",
                                "Sphere style",
                                "Sphere glow" });
    const auto settings = settingsOf(mod);
    const auto setting = [&settings](const std::string& text) -> const model::Setting& {
        return *std::ranges::find(settings, text, &model::Setting::text);
    };

    // a key's own fields, over the key's and with its own `valueOptions` added to them
    const auto& primary = setting("Take the light");
    CHECK(primary.help == "Takes it.");
    CHECK_FALSE(primary.valueOptions.suppress);
    CHECK(primary.valueOptions.hands == std::vector{ vrcf::Hand::Offhand });
    CHECK(primary.defaultValue == model::Value(model::canonicalBinding(R"({ hand = "offhand", type = "tap", button = "grip" })", model::TableForm::Toml)));
    CHECK(primary.applies == model::Applies::Save);
    CHECK(setting("Sphere glow").applies == model::Applies::Restart);
    const auto& zone = setting("Zone").valueOptions.transformAxes;
    CHECK_FALSE(zone[model::TRANSFORM_ROTATION].on);
    CHECK(common::fEqual(zone[model::TRANSFORM_SCALE].max, 60.0f));

    // a power-armor zone with a default of its own is as any setting
    CHECK(setting("Zone in power armor").defaultValue == model::Value("{ scale = 2.0 }"s));

    // a condition of a key is joined with the one the sphere gives it and the section's
    CHECK(setting("Sphere style").enabledWhen);
    REQUIRE(setting("Show sphere").enabledWhen);
    CHECK(setting("Show sphere").enabledWhen->setting == "tZone:Body");
    REQUIRE(setting("Primary haptic").visibleWhen);
    CHECK(setting("Primary haptic").visibleWhen->kind == model::Condition::Kind::All);

    // with the sphere's control hidden its look is hidden, also a key that is named, and a section with no row is left out
    const auto& head = std::get<model::Group>(mod.frontPage.items[1]);
    CHECK(rowTexts(head) == Lines{ "Activate",
                                "Entry haptic",
                                "Icons",
                                "Activation:",
                                "Primary haptic",
                                "Secondary activate",
                                "Secondary haptic",
                                "Visuals:",
                                "Show icon",
                                "Icon",
                                "Icon color",
                                "Icon size",
                                "Show binding icon",
                                "Binding icons",
                                "Binding icon position and size",
                                "Binding icon color" });
}

TEST_CASE("ConfigSpecLoader: a sphere's binding icon has its rows after the icon's, with the framework's defaults")
{
    const auto mod = loaded(sphereSpec(R"({ "type": "cf.activationSphere", "text": "Body", "storage": { "path": ["Body"] } },
        { "type": "cf.activationSphere", "text": "Head", "storage": { "path": ["Head"] }, "keys": { "sShowBindingIcon": false, "cBindingIconColor": true } },
        { "type": "cf.activationSphere", "text": "Hand", "storage": { "path": ["Hand"] }, "keys": { "cBindingIconColor": false } })"));
    CHECK(mod.problems.empty());
    REQUIRE(mod.frontPage.items.size() == 3);
    const auto visuals = [&mod](const std::size_t sphere) {
        const auto& group = std::get<model::Group>(mod.frontPage.items[sphere]);
        return rowTexts(std::get<model::Group>(group.items.back()));
    };

    // the binding icon's rows are after the icon's
    CHECK(visuals(0) == Lines{ "Show icon",
                            "Icon",
                            "Icon color",
                            "Icon size",
                            "Show binding icon",
                            "Binding icons",
                            "Binding icon position and size",
                            "Binding icon color",
                            "Show sphere",
                            "Sphere style" });
    // hiding the key that says when it is shown hides its look, also a key that is named, and Icons, so Show icon is a
    // main row
    CHECK(visuals(1) == Lines{ "Icon", "Icon color", "Icon size", "Show sphere", "Sphere style" });
    CHECK(visuals(2) ==
          Lines{ "Show icon", "Icon", "Icon color", "Icon size", "Show binding icon", "Binding icons", "Binding icon position and size", "Show sphere", "Sphere style" });

    const auto settings = settingsOf(mod);
    const auto setting = [&settings](const std::string& text) -> const model::Setting& {
        return *std::ranges::find(settings, text, &model::Setting::text);
    };
    const auto text = [&setting](const std::string& row) {
        return std::get<std::string>(*setting(row).defaultValue);
    };

    // what the framework reads for a key the file does not have
    const f4vr::WandActivationConfig framework;
    CHECK(framework.showBindingIcon == f4vr::parseActivationSphereVisibility(text("Show binding icon"), f4vr::ActivationSphereVisibility::Always));
    CHECK(framework.bindingIconStyle.bindings == f4vr::parseActivationBindingIcons(text("Binding icons"), f4vr::ActivationBindingIcons::All));
    CHECK(setting("Binding icons").valueOptions.sphereWords == model::SphereWords::BindingIcons);
    for (const auto& option : setting("Binding icons").valueOptions.options) {
        const auto& word = std::get<std::string>(option.value);
        CHECK(f4vr::parseActivationBindingIcons(word, f4vr::ActivationBindingIcons::All) == f4vr::parseActivationBindingIcons(word, f4vr::ActivationBindingIcons::Primary));
    }
    const auto position = model::parseTransformTable(text("Binding icon position and size"), model::TableForm::Toml);
    REQUIRE(position);
    CHECK(position->pos == std::array{ 0.0f, 0.0f, 0.0f });
    CHECK(common::fEqual(*position->scale, framework.bindingIconStyle.transform.scale));
    CHECK_FALSE(setting("Binding icon position and size").valueOptions.transformAxes[model::TRANSFORM_ROTATION].on);
    const auto color = model::colorOf(setting("Binding icon color"), *setting("Binding icon color").defaultValue);
    REQUIRE(color);
    CHECK(std::abs(color->a - framework.bindingIconStyle.color.a) < 0.01f);

    // its look is disabled while it is never shown
    for (const auto* row : { "Binding icons", "Binding icon position and size", "Binding icon color" }) {
        REQUIRE(setting(row).enabledWhen);
        CHECK(setting(row).enabledWhen->setting == "sShowBindingIcon:Body");
    }
}

TEST_CASE("ConfigSpecLoader: a sphere's Icons picks its icon, its binding icon or both, and its keys hide it, name it or give it options of their own")
{
    const auto mod = loaded(sphereSpec(R"({ "type": "cf.activationSphere", "text": "Body", "storage": { "path": ["Body"] } },
        { "type": "cf.activationSphere", "text": "Head", "storage": { "path": ["Head"] }, "keys": { "icons": false } },
        { "type": "cf.activationSphere", "text": "Hand", "storage": { "path": ["Hand"] }, "keys": { "tBindingIcon": false,
            "icons": { "text": "Marks", "help": "Which marks show.", "valueOptions": { "options": [
                { "value": "on", "text": "On", "sets": { "sShowIcon:Hand": "always", "sShowBindingIcon:Hand": "always" } } ] } } } },
        { "type": "cf.activationSphere", "text": "Foot", "storage": { "path": ["Foot"] }, "keys": { "tBindingIcon": false } },
        { "type": "cf.activationSphere", "text": "Arm", "storage": { "path": ["Arm"] }, "keys": { "sShowBindingIcon": { "default": "wheninside" },
            "tBindingIcon": { "default": { "pos": [1.0, -1.0, 0.0], "scale": 1.5 } }, "fIconSize": { "default": 2.0 } } },
        { "type": "cf.activationSphere", "text": "Leg", "storage": { "path": ["Leg"] }, "keys": { "sShowIcon": { "default": "never" },
            "sShowBindingIcon": { "default": "wheninside" }, "tBindingIcon": { "default": { "pos": [0.0, 0.0, 0.0], "scale": 2.0 } } } },
        { "type": "cf.activationSphere", "text": "Eye", "storage": { "path": ["Eye"] }, "keys": { "sShowIcon": { "default": "always" } } })"));
    CHECK(mod.problems.empty());
    const auto settings = settingsOf(mod);
    const auto& icons = *std::ranges::find(settings, "Icons", &model::Setting::text);
    CHECK(icons.id == "icons:Body");
    CHECK(model::setsOthers(icons));
    const auto words = [](const model::Setting& choice, const std::size_t option) {
        Lines texts;
        for (const auto& set : choice.valueOptions.options[option].sets) {
            texts.push_back(std::format("{} = {}", set.setting, std::get<std::string>(set.value)));
        }
        for (const auto& set : choice.valueOptions.options[option].alsoSets) {
            texts.push_back(std::format("also {} = {}", set.setting, std::get<std::string>(set.value)));
        }
        std::ranges::sort(texts);
        return texts;
    };
    REQUIRE(icons.valueOptions.options.size() == 4);
    CHECK(words(icons, 0) == Lines{ "sShowBindingIcon:Body = never", "sShowIcon:Body = never" });
    CHECK(words(icons, 1) == Lines{ "sShowBindingIcon:Body = never", "sShowIcon:Body = wheninside" });
    // the binding alone is in the icon's place, and with the icon beside it
    CHECK(words(icons, 2) == Lines{ "also tBindingIcon:Body = { pos = [0.0, 0.0, 0.0], scale = 1.25 }", "sShowBindingIcon:Body = wheninside", "sShowIcon:Body = never" });
    CHECK(words(icons, 3) == Lines{ "also tBindingIcon:Body = { pos = [0.65, -0.65, 0.0], scale = 1.0 }", "sShowBindingIcon:Body = wheninside", "sShowIcon:Body = wheninside" });

    // the mod's defaults: both shown is its binding icon's default, and the binding alone goes with its icon size
    const auto& arm = std::get<model::Setting>(std::get<model::Group>(mod.frontPage.items[4]).items[2]);
    REQUIRE(arm.valueOptions.options.size() == 4);
    CHECK(words(arm, 2) == Lines{ "also tBindingIcon:Arm = { pos = [0.0, 0.0, 0.0], scale = 2.5 }", "sShowBindingIcon:Arm = wheninside", "sShowIcon:Arm = never" });
    CHECK(words(arm, 3) == Lines{ "also tBindingIcon:Arm = { pos = [1.0, -1.0, 0.0], scale = 1.5 }", "sShowBindingIcon:Arm = wheninside", "sShowIcon:Arm = wheninside" });

    // the binding alone as the mod's default keeps its size
    const auto& leg = std::get<model::Setting>(std::get<model::Group>(mod.frontPage.items[5]).items[2]);
    REQUIRE(leg.valueOptions.options.size() == 4);
    CHECK(words(leg, 2) == Lines{ "also tBindingIcon:Leg = { pos = [0.0, 0.0, 0.0], scale = 2.0 }", "sShowBindingIcon:Leg = wheninside", "sShowIcon:Leg = never" });

    // an icon the mod shows always is turned on always, and its help says so
    const auto& eye = std::get<model::Setting>(std::get<model::Group>(mod.frontPage.items[6]).items[2]);
    REQUIRE(eye.valueOptions.options.size() == 4);
    CHECK(words(eye, 1) == Lines{ "sShowBindingIcon:Eye = never", "sShowIcon:Eye = always" });
    CHECK(eye.valueOptions.options[1].help == "Its icon shows all the time.");
    CHECK(words(eye, 3) == Lines{ "also tBindingIcon:Eye = { pos = [0.65, -0.65, 0.0], scale = 1.0 }", "sShowBindingIcon:Eye = wheninside", "sShowIcon:Eye = always" });
    CHECK(eye.valueOptions.options[3].help == "Its icon shows all the time, and the icons of its bindings beside it while a hand is in the zone.");

    // hidden, Show icon is a main row again
    const auto& head = std::get<model::Group>(mod.frontPage.items[1]);
    CHECK(std::get<model::Setting>(head.items[2]).text == "Show icon");

    // named, with options of its own
    const auto& hand = std::get<model::Group>(mod.frontPage.items[2]);
    const auto& marks = std::get<model::Setting>(hand.items[2]);
    CHECK(marks.text == "Marks");
    CHECK(marks.help == "Which marks show.");
    REQUIRE(marks.valueOptions.options.size() == 1);
    CHECK(marks.valueOptions.options[0].sets.size() == 2);

    // with the binding icon's place hidden, no option sets it
    const auto& foot = std::get<model::Setting>(std::get<model::Group>(mod.frontPage.items[3]).items[2]);
    REQUIRE(foot.valueOptions.options.size() == 4);
    CHECK(foot.valueOptions.options[2].alsoSets.empty());
    CHECK(foot.valueOptions.options[3].alsoSets.empty());
}

TEST_CASE("ConfigSpecLoader: what a sphere gets wrong is reported where it is in spec.json, and on the row it is on")
{
    const auto mod = loaded(sphereSpec(R"({ "type": "cf.activationSphere", "text": "Body", "stowage": true, "storage": { "path": ["Body"] },
        "keys": {
            "kPrimaryBinding": false,
            "sIcn": true,
            "sEntryHaptic": 1,
            "aSphereFalloff": true,
            "tZone": { "storage": { "path": ["Other"] }, "default": 5 }
        } },
        { "type": "cf.activationSphere", "text": "In an INI file", "storage": { "file": "ini", "section": "Sphere", "key": "Zone" } },
        { "type": "cf.activationSphere", "text": "Nowhere", "keys": 1 })"));
    CHECK(problemLines(mod) ==
          Lines{ "W Sphere falloff: The menu has no row of the type \"cf.falloff\": it is shown as one it does not have. [Sphere falloff] content[0].keys.aSphereFalloff",
              "E Body: The primary binding cannot be hidden: it is shown. [Body] content[0].keys.kPrimaryBinding",
              "W Body: Its value 1 is not true, false or an object: it is shown. [Body] content[0].keys.sEntryHaptic",
              "W Body: Unknown field \"sIcn\": it is not read. Did you mean \"sIcon\"? [Body] content[0].keys.sIcn",
              "E Zone: It has no default transform, an object as { \"pos\": [0, 0, 0], \"rot\": [0, 0, 0], \"scale\": 1 }: it has no Reset. [Zone] content[0].keys.tZone",
              "W Body: Unknown field \"storage\": it is not read. [Body] content[0].keys.tZone.storage",
              "W Body: Unknown field \"stowage\": it is not read. Did you mean \"storage\"? [Body] content[0].stowage",
              "E In an INI file: An activation sphere is a section of a TOML file, and it is not: it has no rows. [In an INI file] content[1].storage",
              "E Nowhere: It has no storage: it is not available. [Nowhere] content[2]" });
    REQUIRE(mod.frontPage.items.size() == 3);
    CHECK(rowTexts(std::get<model::Group>(mod.frontPage.items[1])).empty());
    CHECK(rowTexts(std::get<model::Group>(mod.frontPage.items[2])).empty());
    const auto zone = std::ranges::find_if(mod.problems, [](const model::Problem& problem) {
        return problem.row == "Zone";
    });
    REQUIRE(zone != mod.problems.end());
    CHECK(zone->place == model::RowPlace{ .page = {}, .item = { 0, 3, 0 } });
}
