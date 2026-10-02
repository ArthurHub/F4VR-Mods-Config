#pragma once

#include "Config.h"
#include "ConfigMenu.h"
#include "ModBase.h"

namespace mods_config
{
    class ModsConfigVR : public ModBase
    {
    public:
        ModsConfigVR()
            : ModBase(Settings(Version::PROJECT, Version::NAME, &g_config, 32, true))
        {}

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

        // FRIK's own open-config binding is turned off for the session
        bool _frikGestureTaken = false;
    };

    // The ONE global to rule them ALL
    inline ModsConfigVR g_modsConfig;
}
