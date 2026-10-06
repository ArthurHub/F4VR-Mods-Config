#include "Config.h"

#include "vrcf/InputBindingParser.h"
#include "vrui/UIManager.h"

using namespace common;

namespace
{
    const char* DEFAULT_SECTION = Version::PROJECT.data();
    const char* PANEL_SECTION = "Panel";
    const char* POINTER_SECTION = "Pointer";
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

        // a word that is not a button, as "none" is, leaves the panel not moved
        moveButton = vrcf::parseButton(ini.GetValue(PANEL_SECTION, "sMoveButton", "grip"));
        moveSmoothing = static_cast<float>(ini.GetDoubleValue(PANEL_SECTION, "fMoveSmoothing", vrui::WORLD_MOVE_SMOOTH_SECONDS));

        // the offhand thumbstick pressed in and held
        recenterBinding = getInputBindingValue(ini,
            PANEL_SECTION,
            "sRecenterBinding",
            vrcf::InputBinding{
                .hand = vrcf::Hand::Offhand,
                .type = vrcf::ActivationType::LongPress,
                .button = vr::k_EButton_SteamVR_Touchpad,
            });

        recenterDistance = static_cast<float>(ini.GetDoubleValue(PANEL_SECTION, "fRecenterDistance", 350.0));

        // a key that is missing keeps the framework's default look
        const imgui::PointerStyle defaults;
        pointerStyle.rayOffset = getTransformValue(ini, POINTER_SECTION, "tRayOffset", defaults.rayOffset);
        pointerStyle.rayColor = getColorValue(ini, POINTER_SECTION, "sRayColor", defaults.rayColor);
        pointerStyle.rayWidth = static_cast<float>(ini.GetDoubleValue(POINTER_SECTION, "fRayWidth", defaults.rayWidth));
        pointerStyle.rayMaxLength = static_cast<float>(ini.GetDoubleValue(POINTER_SECTION, "fRayMaxLength", defaults.rayMaxLength));
        pointerStyle.rayFade = static_cast<float>(ini.GetDoubleValue(POINTER_SECTION, "fRayFade", defaults.rayFade));
        pointerStyle.markColor = getColorValue(ini, POINTER_SECTION, "sMarkColor", defaults.markColor);
        pointerStyle.markSize = static_cast<float>(ini.GetDoubleValue(POINTER_SECTION, "fMarkSize", defaults.markSize));
        pointerStyle.markBorderColor = getColorValue(ini, POINTER_SECTION, "sMarkBorderColor", defaults.markBorderColor);
        pointerStyle.markBorderWidth = static_cast<float>(ini.GetDoubleValue(POINTER_SECTION, "fMarkBorderWidth", defaults.markBorderWidth));
        pointerStyle.scrollSpeed = static_cast<float>(ini.GetDoubleValue(POINTER_SECTION, "fScrollSpeed", defaults.scrollSpeed));
    }
}
