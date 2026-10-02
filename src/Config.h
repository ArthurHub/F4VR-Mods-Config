#pragma once

#include "ConfigBase.h"
#include "Resources.h"
#include "common/CommonUtils.h"
#include "vrcf/VRControllersManager.h"

namespace mods_config
{
    static const auto BASE_MOD_PATH = BASE_PATH + "\\" + std::string(Version::PROJECT);
    static const auto INI_PATH = BASE_MOD_PATH + "\\" + std::string(Version::PROJECT) + ".ini";

    class Config : public ConfigBase
    {
    public:
        explicit Config()
            : ConfigBase(Version::PROJECT, INI_PATH, IDR_CONFIG_INI)
        {}

        // Opens and closes the config menu
        vrcf::InputBinding openMenuBinding;

    protected:
        virtual void loadIniConfigInternal(const CSimpleIniA& ini) override;
    };

    // Global singleton for easy access
    inline Config g_config;
}
