#include "ModsConfigVR.h"

#include "api/FRIKApi.h"
#include "devbench/MCDevBench.h"
#include "vrcf/VRControllersManager.h"

// This is the entry point to the mod.
extern "C" DLLEXPORT bool F4SEAPI F4SEPlugin_Query(const F4SE::QueryInterface* a_skse, F4SE::PluginInfo* a_info)
{
    return g_mod->onF4SEPluginQuery(a_skse, a_info);
}

// This is the entry point to the mod.
extern "C" DLLEXPORT bool F4SEAPI F4SEPlugin_Load(const F4SE::LoadInterface* a_f4se)
{
    return g_mod->onF4SEPluginLoad(a_f4se);
}

using namespace frik::api;

namespace
{
    // the FRIK API version that added the config overrides
    constexpr std::uint32_t FRIK_API_CONFIG_OVERRIDE_VERSION = 4;
    constexpr auto FRIK_INI_SECTION = "Fallout4VRBody";
    constexpr auto FRIK_OPEN_CONFIG_KEY = "sOpenConfigButton";
}

namespace mods_config
{
    bool ModsConfigVR::isPlayerLoaded()
    {
        const auto player = RE::PlayerCharacter::GetSingleton();
        return player && player->loadedData;
    }

    /**
     * Run F4SE plugin load and initialize the plugin given the init handle.
     */
    void ModsConfigVR::onModLoaded(const F4SE::LoadInterface*)
    {
        devbench::setupDevBenchTool();
    }

    /**
     * On game fully loaded initialize things that should be initialized only once.
     */
    void ModsConfigVR::onGameLoaded()
    {
        takeOpenGestureFromFRIK();
    }

    /**
     * Game session can be initialized multiple times as it is fired on new game and save loaded events.
     */
    void ModsConfigVR::onGameSessionLoaded()
    {
        _menu.close();
    }

    /**
     * On every frame if player is initialized.
     */
    void ModsConfigVR::onFrameUpdate()
    {
        if (!isPlayerLoaded()) {
            logger::sample(3000, "no player data - noop");
            return;
        }

        if (vrcf::VRControllers.check(g_config.openMenuBinding)) {
            if (_menu.isOpen()) {
                _menu.close();
            } else {
                _menu.open();
            }
        }

        _menu.onFrameUpdate();
    }

    /**
     * The menu opens with the gesture FRIK uses for its own config, so with FRIK installed its binding is
     * turned off for the session. FRIK is not required: without it only the gesture of this mod exists.
     */
    void ModsConfigVR::takeOpenGestureFromFRIK()
    {
        // the oldest API with isConfigOpen, which the menu's FRIK button asks
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
        _frikGestureTaken = FRIKApi::inst->setConfigValueOverride(Version::PROJECT.data(), FRIK_INI_SECTION, FRIK_OPEN_CONFIG_KEY, "none");
        logger::info("Take the open config gesture from FRIK: {}", _frikGestureTaken ? "done" : "failed");
    }
}
