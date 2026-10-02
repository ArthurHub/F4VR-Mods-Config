#include "Config.h"

using namespace common;

namespace
{
    const char* DEFAULT_SECTION = Version::PROJECT.data();
}

namespace mods_config
{
    void Config::loadIniConfigInternal(const CSimpleIniA& ini)
    {
        // both thumbsticks pressed in and held, the gesture FRIK uses for its own config
        openMenuBinding = getInputBindingValue(ini,
            DEFAULT_SECTION,
            "sOpenMenuBinding",
            vrcf::InputBinding{
                .hand = vrcf::Hand::Primary,
                .type = vrcf::ActivationType::LongPress,
                .button = vr::k_EButton_SteamVR_Touchpad,
                .modifier = vrcf::InputModifier{ vr::k_EButton_SteamVR_Touchpad, vrcf::Hand::Offhand },
                .duration = 1.5f,
            });
    }
}
