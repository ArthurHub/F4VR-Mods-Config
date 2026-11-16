#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <format>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "Config.h"
#include "TestFiles.h"
#include "TestMenuPort.h"
#include "common/CommonUtils.h"
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

    // the installed spec.json of My Mod, in its folder of the config specs' folder
    constexpr auto MY_MOD_SPEC = R"(Data\ModsConfigFO4\Config\MyMod\spec.json)";

    /**
     * My Mod's config spec with another name shown, as its author edits it.
     */
    std::string editedMyMod()
    {
        std::string text = MY_MOD;
        const std::string name = R"("displayName": "My Mod")";
        text.replace(text.find(name), name.size(), R"("displayName": "My Mod Edited")");
        return text;
    }

    std::string readFile(const std::filesystem::path& path)
    {
        std::ostringstream text;
        text << std::ifstream(path, std::ios::binary).rdbuf();
        return text.str();
    }

    model::Mod configSpecOf(const char* json)
    {
        auto mod = spec::loadConfigSpec(json);
        if (!mod) {
            FAIL(mod.error());
        }
        return std::move(*mod);
    }

    /**
     * A client of the native API that gives nothing and does nothing but the one function it is given, which counts
     * its runs.
     */
    struct NullClient final : api::IClient
    {
        std::string function;
        int runs = 0;

        explicit NullClient(std::string name = {})
            : function(std::move(name))
        {}

        bool hasFunction(const char* name) override
        {
            return name == function;
        }

        bool runFunction(const char* name, api::IFunctionCall*) override
        {
            if (name != function) {
                return false;
            }
            ++runs;
            return true;
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
     * My Mod in My Games, whose Scale is 1.25, My Mod's installed spec.json, and the mods My Mod and Other.
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
            folder.write(MY_MOD_SPEC, MY_MOD);
            auto myMod = configSpecOf(MY_MOD);
            myMod.folder = "MyMod";
            controller.setMods({ std::move(myMod), configSpecOf(OTHER_MOD) }, {}, {});
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

        const model::Mod& myMod() const
        {
            return controller.mods()[indexOf("MyMod")];
        }

        std::filesystem::path liveCopy() const
        {
            return folder.path / "Dev" / "MyMod" / "spec.json";
        }

        /**
         * Writes My Mod's live copy as its author saves it, a while ago: one that was written just now is not read
         * yet.
         */
        void writeLiveCopy(const std::string& text) const
        {
            std::ofstream(liveCopy(), std::ios::binary | std::ios::trunc) << text;
            std::filesystem::last_write_time(liveCopy(), std::filesystem::file_time_type::clock::now() - std::chrono::seconds(1));
        }

        /**
         * Saves My Mod's live copy, with the menu's next look for saved copies due.
         */
        void saveLiveCopy(const std::string& text)
        {
            writeLiveCopy(text);
            port.time += std::chrono::seconds(1);
        }

        /**
         * A press on a row of My Mod's live copy in its report, done in the frame update.
         */
        void pressLiveCopy(const ui::LiveCopyAction action)
        {
            REQUIRE(controller.fullPanel().openReport(indexOf("MyMod")));
            REQUIRE(controller.fullPanel().pressLiveCopy(action));
            frame();
        }

        const ui::LiveCopyRead& liveCopyRead()
        {
            const auto* read = controller.fullPanel().modList().liveCopyRead("MyMod");
            REQUIRE(read);
            return *read;
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
    CHECK(common::fEqual(menu.scale(), 1.25f));

    // while it is shown
    menu.writeScale("0.75");
    menu.controller.requestRestore();
    menu.frame();
    CHECK(menu.port.take().empty());
    CHECK(common::fEqual(menu.scale(), 1.25f));

    // Restore on the wrist
    menu.minimized();
    menu.port.handlers.restorePressed();
    menu.frame();
    CHECK_FALSE(menu.controller.isMinimized());
    CHECK(menu.port.take() == Calls{ "showPanel", "hideWristButtons" });
    CHECK(common::fEqual(menu.scale(), 0.75f));
}

TEST_CASE("MenuController: a client's request to read again reads the values of the mod that is shown, by its name in any case, and of no other")
{
    Menu menu;
    menu.openAtMyMod();
    menu.writeScale("0.75");

    menu.controller.requestReadAgain("Other");
    menu.frame();
    CHECK(common::fEqual(menu.scale(), 1.25f));

    menu.controller.requestReadAgain("MYMOD");
    menu.frame();
    CHECK(common::fEqual(menu.scale(), 0.75f));

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

TEST_CASE("MenuController: a live copy turned on is made of the installed spec.json and read in place of the mod")
{
    Menu menu;
    menu.controller.open();
    menu.pressLiveCopy(ui::LiveCopyAction::TurnOn);
    CHECK(readFile(menu.liveCopy()) == MY_MOD);
    CHECK(menu.myMod().liveCopy == std::format(R"(My Games\Fallout4VR\Mods_Config\{}\Dev\MyMod\spec.json)", Version::PROJECT));
    CHECK_FALSE(menu.liveCopyRead().failed);
    CHECK_FALSE(menu.liveCopyRead().stale);

    // off reads the installed spec.json again
    menu.saveLiveCopy(editedMyMod());
    menu.frame();
    REQUIRE(menu.myMod().displayName == "My Mod Edited");
    menu.pressLiveCopy(ui::LiveCopyAction::TurnOff);
    CHECK(menu.myMod().displayName == "My Mod");
    CHECK(menu.myMod().liveCopy.empty());
}

TEST_CASE("MenuController: a saved live copy is read again and replaces its mod in the list, which drops its unsaved changes")
{
    Menu menu;
    menu.controller.open();
    menu.pressLiveCopy(ui::LiveCopyAction::TurnOn);
    menu.controller.select({ .mod = menu.indexOf("MyMod") });
    menu.changeScale();
    REQUIRE(common::fEqual(menu.scale(), 0.5f));

    // the menu looks for saved copies every half second
    menu.writeLiveCopy(editedMyMod());
    menu.port.time += std::chrono::milliseconds(400);
    menu.frame();
    CHECK(menu.myMod().displayName == "My Mod");

    menu.port.time += std::chrono::milliseconds(100);
    menu.frame();
    CHECK(menu.myMod().displayName == "My Mod Edited");
    CHECK_FALSE(menu.liveCopyRead().failed);
    CHECK(menu.controller.selection().mod == menu.indexOf("MyMod"));
    CHECK(common::fEqual(menu.scale(), 1.25f));

    // once: it is not read again until it is saved again
    menu.changeScale();
    menu.port.time += std::chrono::seconds(1);
    menu.frame();
    CHECK(common::fEqual(menu.scale(), 0.5f));
}

TEST_CASE("MenuController: a saved live copy is not read while Adjust in world tunes a transform, and is read after it")
{
    Menu menu;
    menu.controller.open();
    menu.pressLiveCopy(ui::LiveCopyAction::TurnOn);
    menu.port.adjusting = true;
    menu.saveLiveCopy(editedMyMod());
    menu.frame();
    CHECK(menu.myMod().displayName == "My Mod");

    menu.port.adjusting = false;
    menu.frame();
    CHECK(menu.myMod().displayName == "My Mod Edited");
}

TEST_CASE("MenuController: a live copy that cannot be read leaves its mod as it was read last, with the reason in its report")
{
    Menu menu;
    menu.controller.open();
    menu.pressLiveCopy(ui::LiveCopyAction::TurnOn);
    menu.saveLiveCopy(editedMyMod());
    menu.frame();
    REQUIRE(menu.myMod().displayName == "My Mod Edited");

    menu.saveLiveCopy("{ not json");
    menu.frame();
    CHECK(menu.liveCopyRead().failed);
    CHECK(menu.myMod().displayName == "My Mod Edited");
}

TEST_CASE("MenuController: Overwrite copies the installed spec.json over the live copy and reads it, and does nothing while the copy is off")
{
    Menu menu;
    menu.controller.open();
    menu.pressLiveCopy(ui::LiveCopyAction::Overwrite);
    CHECK_FALSE(std::filesystem::exists(menu.liveCopy()));

    menu.pressLiveCopy(ui::LiveCopyAction::TurnOn);
    menu.saveLiveCopy(editedMyMod());
    menu.frame();
    REQUIRE(menu.myMod().displayName == "My Mod Edited");
    menu.pressLiveCopy(ui::LiveCopyAction::Overwrite);
    CHECK(readFile(menu.liveCopy()) == MY_MOD);
    CHECK(menu.myMod().displayName == "My Mod");
}

TEST_CASE("MenuController: Reload reads the live copy again, and its report tells an installed spec.json that is newer and different")
{
    Menu menu;
    menu.controller.open();
    menu.pressLiveCopy(ui::LiveCopyAction::TurnOn);
    std::filesystem::last_write_time(menu.liveCopy(), std::filesystem::file_time_type::clock::now() - std::chrono::seconds(2));
    menu.folder.write(MY_MOD_SPEC, editedMyMod());

    menu.pressLiveCopy(ui::LiveCopyAction::Reload);
    CHECK(menu.liveCopyRead().stale);
    CHECK(menu.myMod().displayName == "My Mod");
}

TEST_CASE("MenuController: Open and Show give the live copy to the desktop's shell")
{
    Menu menu;
    menu.controller.open();
    menu.pressLiveCopy(ui::LiveCopyAction::TurnOn);
    menu.port.take();

    menu.pressLiveCopy(ui::LiveCopyAction::Open);
    menu.pressLiveCopy(ui::LiveCopyAction::Show);
    CHECK(menu.port.take() == Calls{ "openFile " + menu.liveCopy().string(), "showInFolder " + menu.liveCopy().string() });
}

TEST_CASE("MenuController: the quick launch list has each client's entry with its texts translated, by name in any case, and a press runs its function")
{
    Menu menu;
    model::Translations translations;
    translations.add("$AlphaOpen", "alpha UI");
    menu.controller.setMods({ configSpecOf(MY_MOD) }, std::move(translations), {});
    NullClient zulu("openUi");
    NullClient alpha("openUi");
    REQUIRE(menu.controller.nativeClients().registerClient("Zulu", api::MODS_CONFIG_FO4_API_VERSION, &zulu));
    REQUIRE(menu.controller.nativeClients().registerClient("Alpha", api::MODS_CONFIG_FO4_API_VERSION, &alpha));
    REQUIRE(menu.controller.nativeClients().addQuickLaunch(&zulu, "Zulu UI", "Opens Zulu's UI.", "Open", "openUi"));
    REQUIRE(menu.controller.nativeClients().addQuickLaunch(&alpha, "$AlphaOpen", "", "", "openUi"));
    REQUIRE(menu.controller.nativeClients().addQuickLaunch(&alpha, "Broken", "", "", "missing"));
    menu.controller.open();

    const auto& entries = menu.controller.fullPanel().quickLaunch();
    std::vector<std::string> names;
    std::ranges::transform(entries, std::back_inserter(names), &ui::QuickLaunchEntry::name);
    CHECK(names == std::vector<std::string>{ "alpha UI", "Broken", "Zulu UI" });
    CHECK(entries[0].notAvailable.empty());
    CHECK_FALSE(entries[1].notAvailable.empty());
    CHECK(entries[2].help == "Opens Zulu's UI.");
    CHECK(entries[2].buttonText == "Open");

    CHECK_FALSE(entries[2].open().failed);
    CHECK(zulu.runs == 1);
    CHECK(alpha.runs == 0);
}

TEST_CASE("MenuController: the quick launch list has the menu's own entry for FRIK while FRIK is installed and its client added none")
{
    Menu menu;
    const auto hasFrikEntry = [&menu] {
        menu.controller.close();
        menu.controller.open();
        const auto& entries = menu.controller.fullPanel().quickLaunch();
        return std::ranges::any_of(entries, [](const ui::QuickLaunchEntry& entry) {
            return entry.name == "FRIK Configuration";
        });
    };
    CHECK_FALSE(hasFrikEntry());

    menu.port.frikInstalled = true;
    CHECK(hasFrikEntry());

    NullClient frik("openConfig");
    REQUIRE(menu.controller.nativeClients().registerClient("frik", api::MODS_CONFIG_FO4_API_VERSION, &frik));
    REQUIRE(menu.controller.nativeClients().addQuickLaunch(&frik, "FRIK", "", "", "openConfig"));
    CHECK_FALSE(hasFrikEntry());
}

TEST_CASE("MenuController: the Help page shown for the first time while Help stands out is written to the config in the frame update")
{
    Menu menu;
    g_config.highlightHelp = true;
    menu.controller.open();
    menu.port.take();
    REQUIRE(menu.controller.fullPanel().showHelp(true));
    CHECK(menu.port.take().empty());
    menu.frame();
    CHECK(menu.port.take() == Calls{ "saveHelpOpened" });

    // once
    menu.controller.fullPanel().showHelp(false);
    menu.controller.fullPanel().showHelp(true);
    menu.frame();
    CHECK(menu.port.take().empty());
    g_config.highlightHelp = false;
}

TEST_CASE("MenuController: a change of the Developer section's filter or fold is written to the config in the frame update")
{
    const auto before = g_config.developer;
    g_config.developer = { .enabled = true };
    Menu menu;
    menu.controller.open();
    menu.port.take();
    REQUIRE(menu.controller.fullPanel().changeDeveloper({ .enabled = true, .folded = true }));
    CHECK(menu.port.take().empty());
    menu.frame();
    CHECK(menu.port.take() == Calls{ "saveDeveloper folded" });
    menu.frame();
    CHECK(menu.port.take().empty());
    g_config.developer = before;
}

namespace
{
    /**
     * The menu whose own options and Save handler are on the mod's own client of the native API. The client is one for
     * the whole run and keeps its handlers, so this menu lives as long, and its tests share it.
     */
    Menu& lastingMenu()
    {
        static Menu menu;
        static const bool registered = [] {
            menu.controller.registerOwnOptions();
            return true;
        }();
        (void)registered;
        menu.controller.close();
        menu.controller.setMods({ configSpecOf(MY_MOD), configSpecOf(OTHER_MOD) }, {}, {});
        std::filesystem::remove(menu.folder.path / "Categories.toml");
        menu.controller.readCategoryChoices();
        // a Save other tests told the client is still waiting for the frame update, which would read the file again
        menu.frame();
        menu.port.take();
        return menu;
    }

    /**
     * The options a client gives, each as its value and its text.
     */
    struct RecordedOptions final : api::IOptions
    {
        std::vector<std::string> given;

        void add(const api::SettingValue* value, const char* text) override
        {
            given.push_back(std::string(value->text ? value->text : "") + " " + (text ? text : ""));
        }
    };
}

TEST_CASE("MenuController: the Config spec setting of the menu's own config spec has none, then each config spec the menu lists")
{
    lastingMenu();
    const api::SettingView setting{ .id = "developerConfigSpec", .valueType = api::ValueType::String };
    RecordedOptions options;
    REQUIRE(static_cast<api::IClient&>(api::ModsConfigFO4Client::get()).giveOptions(&setting, &options));
    CHECK(options.given == std::vector<std::string>{ " None", "MyMod My Mod", "Other Other" });
}

TEST_CASE("MenuController: a Categories row of the menu's own config spec offers the categories the player gave mods")
{
    // the menus of the tests before set mods too, and only the lasting one, which owns the client, adds their options
    auto& menu = lastingMenu();
    menu.folder.write("Categories.toml", "MyMod = \"Gameplay\"\n");
    menu.controller.readCategoryChoices();
    const api::SettingView setting{ .id = "category.MyMod", .valueType = api::ValueType::String };
    RecordedOptions options;
    REQUIRE(static_cast<api::IClient&>(api::ModsConfigFO4Client::get()).giveOptions(&setting, &options));
    CHECK(std::ranges::find(options.given, "Gameplay Gameplay") != options.given.end());
}

TEST_CASE("MenuController: a Save of the menu's own config spec reads the player's categories again, which group the tree")
{
    auto& menu = lastingMenu();
    menu.controller.open();
    const auto hasGameplay = [&menu] {
        return std::ranges::any_of(menu.controller.fullPanel().modList().groups(), [](const model::CategoryGroup& group) {
            return group.name == "Gameplay";
        });
    };
    menu.folder.write("Categories.toml", "MyMod = \"Gameplay\"\n");
    menu.frame();
    CHECK_FALSE(hasGameplay());

    const api::SaveBatch batch{};
    static_cast<api::IClient&>(api::ModsConfigFO4Client::get()).onSaved(&batch);
    CHECK_FALSE(hasGameplay());
    menu.frame();
    CHECK(hasGameplay());
}
