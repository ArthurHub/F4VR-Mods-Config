#pragma once

#include "Config.h"
#include "ConfigMenu.h"
#include "ModBase.h"
#include "f4vr/GameMenusHandler.h"

namespace mods_config
{
    class ModsConfigFO4 : public ModBase
    {
    public:
        ModsConfigFO4()
            : ModBase(Settings(Version::PROJECT, Version::NAME, &g_config, 64, true))
        {
            // the buttons on the hand and Adjust in world draw vrui text: its font is built in the background at load,
            // or the first minimize stalls a frame for ~0.1 s
            _settings.preloadRendering = true;
        }

        static bool isPlayerLoaded();

        ConfigMenu& menu()
        {
            return _menu;
        }

        bool isFrikGestureTaken() const
        {
            return _frikGestureTaken;
        }

    protected:
        virtual void onModLoaded(const F4SE::LoadInterface* f4SE) override;
        virtual void onGameLoaded() override;
        virtual void onGameSessionLoaded() override;
        virtual void onFrameUpdate() override;

    private:
        void takeOpenGestureFromFRIK();

        ConfigMenu _menu;

        // the game's menus opening and closing, as the loading screen that closes the menu
        f4vr::GameMenusHandler _gameMenus;

        // FRIK's own open-config binding is turned off for the session
        bool _frikGestureTaken = false;
    };

    // The ONE global to rule them ALL
    inline ModsConfigFO4 g_modsConfig;
}
