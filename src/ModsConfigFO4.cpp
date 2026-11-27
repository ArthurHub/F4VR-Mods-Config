#include "ModsConfigFO4.h"

#include "GameTranslations.h"
#include "devbench/MCDevBench.h"
#include "f4vr/F4VRUtils.h"
#include "mcm/McmLoader.h"
#include "model/Categories.h"
#include "perf/Perf.h"
#include "spec/ConfigSpecLoader.h"

#ifdef ENABLE_FALLOUT_VR
#include "api/FRIKApi.h"
#include "vrcf/VRControllersManager.h"

namespace
{
    /**
     * Whether a button is held down on a hand, on either hand for `Any`.
     */
    bool isHeldDown(const f4cf::vrcf::Hand hand, const vr::EVRButtonId button)
    {
        using namespace f4cf::vrcf;
        return hand == Hand::Any ? VRControllers.isPressHeldDown(Hand::Primary, button) || VRControllers.isPressHeldDown(Hand::Offhand, button)
                                 : VRControllers.isPressHeldDown(hand, button);
    }

    /**
     * Whether a binding's button is held on a hand together with its modifier, which is on that hand when the modifier
     * names none, as VRControllersManager::check reads it.
     */
    bool isHeldWithModifier(const f4cf::vrcf::InputBinding& binding, const f4cf::vrcf::Hand hand)
    {
        return isHeldDown(hand, binding.button) && isHeldDown(binding.modifier->hand.value_or(hand), binding.modifier->button);
    }
}
#endif

// This is the entry point to the mod.
extern "C" DLLEXPORT bool F4SEAPI F4SEPlugin_Query(const F4SE::QueryInterface* a_skse, F4SE::PluginInfo* a_info)
{
    return g_mod->onF4SEPluginQuery(a_skse, a_info);
}

// F4SE for flat Fallout 4 1.10.980 and later reads this in place of the query, before any of the DLL's code runs
extern "C" DLLEXPORT constinit auto F4SEPlugin_Version = f4cf::pluginVersionData(Version::PROJECT, { Version::MAJOR, Version::MINOR, Version::PATCH });

// This is the entry point to the mod.
extern "C" DLLEXPORT bool F4SEAPI F4SEPlugin_Load(const F4SE::LoadInterface* a_f4se)
{
    return g_mod->onF4SEPluginLoad(a_f4se);
}

#ifdef ENABLE_FALLOUT_VR
using namespace frik::api;

namespace
{
    // the FRIK API version that added the config overrides
    constexpr std::uint32_t FRIK_API_CONFIG_OVERRIDE_VERSION = 4;
    constexpr auto FRIK_INI_SECTION = "Fallout4VRBody";
    constexpr auto FRIK_OPEN_CONFIG_KEY = "sOpenConfigButton";

    // The owner of the menu's override of FRIK's gesture, which FRIK's row in the menu names. It is not the menu's
    // F4SE name, which owns its previews: the menu leaves those out of a setting's overrides, and a preview of the key
    // would replace this override, one per owner and key, and its Discard would clear it.
    constexpr auto FRIK_GESTURE_OWNER = "Mods Config FO4 VR";
}
#endif

namespace
{
    // the game's main menu, which CommonLibF4 has no class of
    constexpr std::string_view MAIN_MENU = "MainMenu";
}

namespace mods_config
{
    bool ModsConfigFO4::isPlayerLoaded()
    {
        const auto player = RE::PlayerCharacter::GetSingleton();
        return player && player->loadedData;
    }

    /**
     * The game's folders for the menu: MCM's, the Data folder and `My Games\Fallout4VR` for a config spec's files, the
     * config specs' and their live copies', in the mod's folder of My Games, and the categories the player gave mods,
     * beside the mod's config, as the mod's own config spec has it. And the game's checks of its plugins.
     */
    menu::MenuSetup ModsConfigFO4::menuSetup()
    {
        return { .mcm = R"(Data\MCM)",
            .data = "Data",
            .myGames = common::getRelativePathInDocuments(R"(\My Games\Fallout4VR)"),
            .configSpecs = spec::CONFIG_SPEC_FOLDER,
            .liveCopies = BASE_MOD_PATH + R"(\Dev)",
            .categories = BASE_MOD_PATH + R"(\Categories.toml)",
            .plugins = spec::gamePluginChecks() };
    }

    /**
     * Run F4SE plugin load and initialize the plugin given the init handle.
     */
    void ModsConfigFO4::onModLoaded(const F4SE::LoadInterface*)
    {
        devbench::setupDevBenchTool();
        // given to the menu as the framework connects the mod's client at kPostLoad
        _menu.registerOwnOptions();
    }

    /**
     * On game fully loaded initialize things that should be initialized only once.
     */
    void ModsConfigFO4::onGameLoaded()
    {
#ifdef ENABLE_FALLOUT_VR
        takeOpenGestureFromFRIK();
#endif

        // A loading screen and the main menu close the menu without asking, before the world it showed its values in is
        // left: a config spec's unsaved changes are discarded and its live previews put back, and an MCM mod's changes
        // stay, as they are made already. The event comes on the game thread, between frames.
        _gameMenus.init([this](const std::string& menu, const bool opened) {
            if (opened && (menu == RE::LoadingMenu::MENU_NAME || menu == MAIN_MENU) && _menu.isOpen()) {
                logger::info("{} opened: close config menu without asking", menu);
                _menu.close();
            }
        });

        // once: what a mod's menu holds does not change while the game runs
        const auto translations = loadGameTranslations();
        auto mods = spec::withConfigSpecs(mcm::loadInstalledMods(translations), spec::loadInstalledConfigSpecs(translations));
        _menu.setMods(std::move(mods), translations, model::loadCategoryTable(model::CATEGORY_TABLE_FILE));
    }

    /**
     * Game session can be initialized multiple times as it is fired on new game and save loaded events.
     */
    void ModsConfigFO4::onGameSessionLoaded()
    {
        _menu.close();
    }

    /**
     * On every frame if player is initialized.
     */
    void ModsConfigFO4::onFrameUpdate()
    {
        F4CF_PERF_FUNCTION();
        if (!isPlayerLoaded()) {
            logger::sample(3000, "no player data - noop");
            return;
        }

#ifdef ENABLE_FALLOUT_VR
        // the gesture shows a minimized menu again, as Restore on the wrist does. While a transform is tuned in the world
        // its sticks are the tuning's, and Done and Cancel end it
        const bool gesture = !_menu.isAdjusting() && vrcf::VRControllers.check(g_config.openMenuBinding);
        closeFavoritesOfOpenGesture(gesture);
        if (gesture) {
            if (_menu.isMinimized()) {
                _menu.requestRestore();
            } else if (_menu.isOpen()) {
                _menu.askToClose();
            } else {
                _menu.open();
            }
        }

        // Not while the button of the open gesture is held as well. With the defaults that is both thumbsticks,
        // the gesture that closes the menu: without this the panel jumps before it closes.
        const auto& openBinding = g_config.openMenuBinding;
        if (!vrcf::VRControllers.isPressHeldDown(openBinding.hand, openBinding.button) && vrcf::VRControllers.check(g_config.recenterBinding)) {
            _menu.recenter();
        }
#endif

        _menu.onFrameUpdate();
    }

#ifdef ENABLE_FALLOUT_VR
    /**
     * Closes the game's favorites, which the press of the primary thumbstick in the default open gesture opens. They are
     * closed in every frame both buttons of the gesture are held, in either order, as the game shows them a frame or two
     * after the press, and when the gesture fires: a gesture with no modifier has only that, so its own button still opens them.
     */
    void ModsConfigFO4::closeFavoritesOfOpenGesture(const bool gestureFired)
    {
        const auto& binding = g_config.openMenuBinding;
        const bool bothHeld = binding.modifier && binding.type != vrcf::ActivationType::Disabled && binding.type != vrcf::ActivationType::AxisDirection &&
                              (binding.hand == vrcf::Hand::Any ? isHeldWithModifier(binding, vrcf::Hand::Primary) || isHeldWithModifier(binding, vrcf::Hand::Offhand)
                                                               : isHeldWithModifier(binding, binding.hand));
        if ((gestureFired || bothHeld) && _gameMenus.isFavoritesMenuOpen()) {
            logger::info("Close the game's favorites, opened by the open gesture");
            f4vr::closeFavoriteMenu();
        }
    }

    /**
     * The menu opens with the gesture FRIK uses for its own config, so with FRIK installed its binding is
     * turned off for the session. FRIK is not required: without it only the gesture of this mod exists.
     */
    void ModsConfigFO4::takeOpenGestureFromFRIK()
    {
        // the oldest API with isConfigOpen, which the menu's quick launch entry for FRIK asks
        const int err = FRIKApi::initialize(2);
        if (err != 0) {
            logger::info("FRIK API is not available (error {}), the menu works without it", err);
            return;
        }
        logger::info("FRIK (v{}) API (v{}) init successful", FRIKApi::inst->getModVersion(), FRIKApi::inst->getVersion());

        if (FRIKApi::inst->getVersion() < FRIK_API_CONFIG_OVERRIDE_VERSION) {
            logger::warn("FRIK API v{} has no config overrides: FRIK's config opens with the same gesture as this menu", FRIKApi::inst->getVersion());
            return;
        }
        _frikGestureTaken = FRIKApi::inst->setConfigValueOverride(FRIK_GESTURE_OWNER, FRIK_INI_SECTION, FRIK_OPEN_CONFIG_KEY, "none");
        logger::info("Take the open config gesture from FRIK: {}", _frikGestureTaken ? "done" : "failed");
    }
#endif
}
