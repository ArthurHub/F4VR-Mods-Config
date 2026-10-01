#pragma once

#include "Config.h"
#include "ModBase.h"

namespace mods_config
{
    class ModsConfigVR : public ModBase
    {
    public:
        ModsConfigVR()
            : ModBase(Settings(Version::PROJECT, Version::NAME, &g_config, 32, true))
        {}

    protected:
        virtual void onModLoaded(const F4SE::LoadInterface* f4SE) override;
        virtual void onGameLoaded() override;
        virtual void onGameSessionLoaded() override;
        virtual void onFrameUpdate() override;

    private:
    };

    // The ONE global to rule them ALL
    inline ModsConfigVR g_modsConfig;
}
