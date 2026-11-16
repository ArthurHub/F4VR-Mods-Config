#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <filesystem>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "Config.h"
#include "TestFiles.h"
#include "TestMenuPort.h"
#include "menu/MenuController.h"
#include "spec/ConfigSpecLoader.h"

using namespace mods_config;
using test_menu_port::Calls;
using test_menu_port::RecordingMenuPort;

namespace
{
    // a config spec with a setting kept in a TOML file in My Games, and a page with a page under it
    constexpr auto MY_MOD = R"({
        "minSchemaVersion": 1, "modName": "MyMod", "displayName": "My Mod",
        "files": { "main": { "format": "toml", "root": "myGames", "path": "Mods_Config/MyMod/MyMod.toml" } },
        "defaultFile": "main",
        "content": [ { "type": "slider", "id": "scale", "valueType": "float", "text": "Scale", "default": 1.0,
                       "valueOptions": { "min": 0.5, "max": 1.5, "step": 0.25 }, "storage": { "path": ["Main", "fScale"] } } ],
        "pages": [ { "pageDisplayName": "General", "id": "general", "content": [],
                     "pages": [ { "pageDisplayName": "Advanced", "id": "advanced", "content": [] } ] } ]
    })";

    constexpr auto OTHER_MOD = R"({ "minSchemaVersion": 1, "modName": "Other", "displayName": "Other", "content": [] })";

    constexpr auto MOD_TOML = R"(Mods_Config\MyMod\MyMod.toml)";

    model::Mod configSpecOf(const char* json)
    {
        auto mod = spec::loadConfigSpec(json);
        if (!mod) {
            FAIL(mod.error());
        }
        return std::move(*mod);
    }

    /**
     * A client of the native API that gives nothing and does nothing, which only registers.
     */
    struct NullClient final : api::IClient
    {
        bool hasFunction(const char*) override
        {
            return false;
        }

        bool runFunction(const char*, api::IFunctionCall*) override
        {
            return false;
        }

        bool answerState(const char*, api::SettingValue*) override
        {
            return false;
        }

        bool giveOptions(const api::SettingView*, api::IOptions*) override
        {
            return false;
        }

        void onEvent(const api::MenuEvent*) override
        {}

        void onChange(const api::SettingChange*) override
        {}

        void onSaved(const api::SaveBatch*) override
        {}
    };

    /**
     * Gives the config's pulse of a press while it lives, and none again after it.
     */
    struct PressHaptic
    {
        PressHaptic()
        {
            g_config.pressHaptic = vrcf::HapticPattern{};
        }

        PressHaptic(const PressHaptic&) = delete;
        PressHaptic& operator=(const PressHaptic&) = delete;

        ~PressHaptic()
        {
            g_config.pressHaptic.reset();
        }
    };

    /**
     * The menu controller over the recording game side, with the folders of a test of its own: the TOML file of
     * My Mod in My Games, whose Scale is 1.25, and the mods My Mod and Other.
     */
    struct Menu
    {
        test_files::TempFolder folder;
        RecordingMenuPort port;
        menu::MenuController controller{ port,
            { .mcm = folder.path / "Data" / "MCM",
                .data = folder.path / "Data",
                .myGames = folder.path / "MyGames",
                .configSpecs = folder.path / "Data" / "ModsConfigFO4" / "Config",
                .liveCopies = folder.path / "Dev",
                .categories = folder.path / "Categories.toml" } };

        Menu()
        {
            writeScale("1.25");
            controller.setMods({ configSpecOf(MY_MOD), configSpecOf(OTHER_MOD) }, {}, {});
            port.take();
        }

        void writeScale(const std::string& scale) const
        {
            folder.write(std::filesystem::path("MyGames") / MOD_TOML, "[Main]\nfScale = " + scale + "\n");
        }

        std::size_t indexOf(const std::string& name) const
        {
            const auto& mods = controller.mods();
            return static_cast<std::size_t>(std::ranges::find(mods, name, &model::Mod::name) - mods.begin());
        }

        /**
         * The menu open at My Mod's front page, with the calls of that taken.
         */
        void openAtMyMod()
        {
            controller.open();
            controller.select({ .mod = indexOf("MyMod") });
            port.take();
        }

        /**
         * The menu open and minimized, with the calls of that taken.
         */
        void minimized()
        {
            if (!controller.isOpen()) {
                controller.open();
            }
            controller.requestMinimize();
            controller.onFrameUpdate();
            REQUIRE(controller.isMinimized());
            port.take();
        }

        /**
         * Scale of My Mod, as the store of the mod that is shown has it.
         */
        float scale()
        {
            auto* store = controller.fullPanel().selectedStore();
            REQUIRE(store);
            const auto& setting = std::get<model::Setting>(controller.mods()[indexOf("MyMod")].frontPage.items[0]);
            return std::get<float>(store->value(setting).value);
        }

        /**
         * Changes Scale of My Mod, which is a change that is not saved.
         */
        void changeScale()
        {
            auto* store = controller.fullPanel().selectedStore();
            REQUIRE(store);
            const auto& setting = std::get<model::Setting>(controller.mods()[indexOf("MyMod")].frontPage.items[0]);
            REQUIRE(store->set(setting, 0.5f));
        }

        void frame()
        {
            controller.onFrameUpdate();
        }
    };
}

TEST_CASE("MenuController: a client's request opens the menu at a mod's page, by the mod's name and the page's id in any case")
{
    Menu menu;
    CHECK(menu.controller.requestOpen("MYMOD", "Advanced"));
    CHECK_FALSE(menu.controller.isOpen());
    menu.frame();
    CHECK(menu.controller.isOpen());
    CHECK(menu.port.take() == Calls{ "openPanel" });
    CHECK(menu.controller.selection() == ui::Selection{ .mod = menu.indexOf("MyMod"), .page = { 0, 0 } });

    // at a minimized menu, which is shown again, and at the menu's front page with no mod
    menu.minimized();
    CHECK(menu.controller.requestOpen("", ""));
    menu.frame();
    CHECK(menu.port.take() == Calls{ "showPanel", "hideWristButtons" });
    CHECK(menu.controller.selection() == ui::Selection{});
}

TEST_CASE("MenuController: a client's request to open at a mod or a page the menu does not have is refused, and asks nothing")
{
    Menu menu;
    CHECK_FALSE(menu.controller.requestOpen("NoMod", ""));
    CHECK_FALSE(menu.controller.requestOpen("MyMod", "nopage"));
    menu.frame();
    CHECK_FALSE(menu.controller.isOpen());
    CHECK(menu.port.take().empty());
}

TEST_CASE("MenuController: a client's request to minimize hides the panel for the wrist buttons, and is ignored while the menu is closed")
{
    Menu menu;
    menu.controller.requestMinimize();
    menu.controller.open();
    menu.port.take();
    menu.frame();
    CHECK_FALSE(menu.controller.isMinimized());
    CHECK(menu.port.take().empty());

    menu.controller.requestMinimize();
    menu.frame();
    CHECK(menu.controller.isMinimized());
    CHECK(menu.port.take() == Calls{ "hidePanel", "showWristButtons" });
}

TEST_CASE("MenuController: Restore shows a minimized menu again and reads the shown mod's values again, and does nothing while it is shown")
{
    Menu menu;
    menu.openAtMyMod();
    CHECK(menu.scale() == 1.25f);

    // while it is shown
    menu.writeScale("0.75");
    menu.controller.requestRestore();
    menu.frame();
    CHECK(menu.port.take().empty());
    CHECK(menu.scale() == 1.25f);

    // Restore on the wrist
    menu.minimized();
    menu.port.handlers.restorePressed();
    menu.frame();
    CHECK_FALSE(menu.controller.isMinimized());
    CHECK(menu.port.take() == Calls{ "showPanel", "hideWristButtons" });
    CHECK(menu.scale() == 0.75f);
}

TEST_CASE("MenuController: a client's request to read again reads the values of the mod that is shown, by its name in any case, and of no other")
{
    Menu menu;
    menu.openAtMyMod();
    menu.writeScale("0.75");

    menu.controller.requestReadAgain("Other");
    menu.frame();
    CHECK(menu.scale() == 1.25f);

    menu.controller.requestReadAgain("MYMOD");
    menu.frame();
    CHECK(menu.scale() == 0.75f);

    // nothing while the menu is closed
    menu.controller.close();
    menu.controller.requestReadAgain("MyMod");
    menu.frame();
    CHECK_FALSE(menu.controller.isOpen());
}

TEST_CASE("MenuController: a client's request to open or to restore cancels Adjust in world, and shows the panel again")
{
    Menu menu;
    menu.openAtMyMod();
    menu.minimized();

    SECTION("open")
    {
        menu.port.adjusting = true;
        CHECK(menu.controller.requestOpen("Other", ""));
        menu.frame();
        CHECK(menu.port.take() == Calls{ "cancelAdjust", "showPanel", "hideWristButtons" });
        CHECK(menu.controller.selection() == ui::Selection{ .mod = menu.indexOf("Other") });
    }

    SECTION("restore")
    {
        menu.port.adjusting = true;
        menu.controller.requestRestore();
        menu.frame();
        CHECK(menu.port.take() == Calls{ "cancelAdjust", "showPanel", "hideWristButtons" });
        CHECK_FALSE(menu.controller.isMinimized());
    }

    SECTION("a request to read again does not")
    {
        menu.port.adjusting = true;
        menu.controller.requestReadAgain("MyMod");
        menu.frame();
        CHECK(menu.port.take().empty());
        CHECK(menu.controller.isAdjusting());
    }
}

TEST_CASE("MenuController: Adjust in world ends with Done, and is cancelled by a game menu that takes the controllers, with the panel shown again")
{
    Menu menu;
    menu.openAtMyMod();
    menu.minimized();
    menu.port.adjusting = true;

    menu.frame();
    CHECK(menu.port.take().empty());

    SECTION("done")
    {
        menu.port.adjustEnds = true;
        menu.frame();
        CHECK(menu.port.take() == Calls{ "adjustDone", "showPanel", "hideWristButtons" });
    }

    SECTION("a game menu")
    {
        menu.port.gameMenuTakingInput = true;
        menu.frame();
        CHECK(menu.port.take() == Calls{ "cancelAdjust", "showPanel", "hideWristButtons" });
    }
}

TEST_CASE("MenuController: opening FRIK's config is done before a close asked in the same frame, which closes the menu in the next frame")
{
    Menu menu;
    menu.port.frikInstalled = true;
    menu.controller.open();
    menu.port.take();
    const auto& entries = menu.controller.fullPanel().quickLaunch();
    const auto frik = std::ranges::find(entries, std::string("FRIK Configuration"), &ui::QuickLaunchEntry::name);
    REQUIRE(frik != entries.end());

    frik->open();
    menu.port.handlers.exitPressed();
    menu.frame();
    CHECK(menu.controller.isOpen());
    CHECK(menu.port.take() == Calls{ "hidePanel", "showWristButtons", "openFrikConfig" });

    menu.frame();
    CHECK_FALSE(menu.controller.isOpen());
    CHECK(menu.port.take() == Calls{ "closePanel" });

    // FRIK's config that is open already is not opened again
    menu.controller.open();
    menu.port.frikConfigOpen = true;
    menu.port.take();
    menu.controller.fullPanel().quickLaunch().front().open();
    menu.frame();
    CHECK(menu.port.take() == Calls{ "hidePanel", "showWristButtons" });
}

TEST_CASE("MenuController: a close is done before a minimize asked in the same frame")
{
    Menu menu;
    menu.controller.open();
    menu.port.take();
    menu.controller.requestMinimize();
    menu.port.handlers.exitPressed();
    menu.frame();
    CHECK_FALSE(menu.controller.isOpen());
    CHECK(menu.port.take() == Calls{ "closePanel" });
}

TEST_CASE("MenuController: Back steps back in the panel with the config's pulse first, and closes the menu from its front page")
{
    const PressHaptic haptic;
    Menu menu;
    menu.controller.open();
    menu.controller.select({ .mod = menu.indexOf("MyMod"), .page = { 0 } });
    menu.port.take();

    menu.port.backIsPressed = true;
    menu.frame();
    CHECK(menu.controller.isOpen());
    CHECK(menu.port.take() == Calls{ "playHaptic" });
    CHECK(menu.controller.selection() != ui::Selection{ .mod = menu.indexOf("MyMod"), .page = { 0 } });

    // back to the front page, then closed from it
    while (menu.controller.selection() != ui::Selection{}) {
        menu.port.backIsPressed = true;
        menu.frame();
        REQUIRE(menu.controller.isOpen());
    }
    menu.port.take();
    menu.port.backIsPressed = true;
    menu.frame();
    CHECK_FALSE(menu.controller.isOpen());
    CHECK(menu.port.take() == Calls{ "playHaptic", "closePanel" });
}

TEST_CASE("MenuController: Exit on the wrist with unsaved changes shows the minimized panel again for the dialog, which refuses a client's pending value")
{
    Menu menu;
    NullClient client;
    REQUIRE(menu.controller.nativeClients().registerClient("MyMod", api::MODS_CONFIG_FO4_API_VERSION, &client));
    menu.openAtMyMod();
    menu.changeScale();
    const api::SettingValue scale{ .type = api::ValueType::Float, .floatValue = 0.75f };
    CHECK(menu.controller.nativeClients().setPendingValue(&client, "scale", &scale));

    menu.minimized();
    menu.port.handlers.exitPressed();
    menu.frame();
    CHECK(menu.controller.isOpen());
    CHECK(menu.controller.fullPanel().isAsking());
    CHECK(menu.port.take() == Calls{ "showPanel", "hideWristButtons" });
    CHECK_FALSE(menu.controller.nativeClients().setPendingValue(&client, "scale", &scale));
}
