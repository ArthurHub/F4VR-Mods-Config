#include "Config.h"

#include "vrcf/InputBindingParser.h"
#include "vrui/UIManager.h"

using namespace common;

namespace
{
    const char* DEFAULT_SECTION = Version::PROJECT.data();
    const char* PANEL_SECTION = "Panel";
    const char* POINTER_SECTION = "Pointer";
    const char* DEVELOPER_SECTION = "Developer";

    // which packages the Developer section lists, by the text the config has for each
    constexpr std::array<std::pair<std::string_view, mods_config::ui::ShowPackages>, 3> SHOW_PACKAGES{ {
        { "attention", mods_config::ui::ShowPackages::NeedingAttention },
        { "all", mods_config::ui::ShowPackages::All },
        { "one", mods_config::ui::ShowPackages::OnlyOne },
    } };
}

namespace mods_config
{
    void Config::loadConfigInternal(const toml::table& config)
    {
        // both thumbsticks pressed in and held, the gesture FRIK uses for its own config
        openMenuBinding = getInputBindingValue(config,
            DEFAULT_SECTION,
            "kOpenMenuBinding",
            vrcf::InputBinding{
                .hand = vrcf::Hand::Primary,
                .type = vrcf::ActivationType::LongPress,
                .button = vr::k_EButton_SteamVR_Touchpad,
                .modifier = vrcf::InputModifier{ vr::k_EButton_SteamVR_Touchpad, vrcf::Hand::Offhand },
                .duration = 1.5f,
            });

        // a word that is not a button, as "none" is, leaves the menu with no Back
        backButton = vrcf::parseButton(config[DEFAULT_SECTION]["sBackButton"].value_or("grip"sv));

        // a panel has a size and stands in front of the player, whatever the file says
        const auto panel = config[PANEL_SECTION];
        panelScale = (std::max)(panel["fScale"].value_or(6.0f), 1.0f);
        panelDistance = (std::max)(panel["fDistance"].value_or(120.0f), 20.0f);
        panelElevation = panel["fHeight"].value_or(-20.0f);
        panelTilt = panel["fTilt"].value_or(5.0f);
        // a panel smaller than this has no room for the tree of the mods beside a page, and for the title bar and the footer
        panelWidth = (std::max)(panel["fPanelWidth"].value_or(30.0f), 16.0f);
        panelHeight = (std::max)(panel["fPanelHeight"].value_or(15.0f), 10.0f);
        // a tree narrower than this cuts most names short, and the page beside it keeps room for its controls
        treeWidth = std::clamp(panel["fTreeWidth"].value_or(8.0f), 4.0f, panelWidth - 9.0f);
        panelBackgroundOpacity = static_cast<std::uint8_t>(std::clamp(panel["iBackgroundOpacity"].value_or(217), 0, 255));

        // a word that is not a button, as "none" is, leaves the panel not moved
        moveButton = vrcf::parseButton(panel["sMoveButton"].value_or("grip"sv));
        moveSmoothing = panel["fMoveSmoothing"].value_or(vrui::WORLD_MOVE_SMOOTH_SECONDS);

        // the offhand thumbstick pressed in and held
        recenterBinding = getInputBindingValue(config,
            PANEL_SECTION,
            "kRecenterBinding",
            vrcf::InputBinding{
                .hand = vrcf::Hand::Offhand,
                .type = vrcf::ActivationType::LongPress,
                .button = vr::k_EButton_SteamVR_Touchpad,
            });

        recenterDistance = panel["fRecenterDistance"].value_or(350.0f);

        // a key that is missing keeps the framework's default look
        const imgui::PointerStyle defaults;
        const auto pointer = config[POINTER_SECTION];
        pointerStyle.rayOffset = getTransformValue(config, POINTER_SECTION, "tRayOffset", defaults.rayOffset);
        pointerStyle.rayColor = getColorValue(config, POINTER_SECTION, "cRayColor", defaults.rayColor);
        pointerStyle.rayWidth = pointer["fRayWidth"].value_or(defaults.rayWidth);
        pointerStyle.rayMaxLength = pointer["fRayMaxLength"].value_or(defaults.rayMaxLength);
        pointerStyle.rayFade = pointer["fRayFade"].value_or(defaults.rayFade);
        pointerStyle.markColor = getColorValue(config, POINTER_SECTION, "cMarkColor", defaults.markColor);
        pointerStyle.markSize = pointer["fMarkSize"].value_or(defaults.markSize);
        pointerStyle.markBorderColor = getColorValue(config, POINTER_SECTION, "cMarkBorderColor", defaults.markBorderColor);
        pointerStyle.markBorderWidth = pointer["fMarkBorderWidth"].value_or(defaults.markBorderWidth);
        pointerStyle.scrollSpeed = pointer["fScrollSpeed"].value_or(defaults.scrollSpeed);

        // "none" is no pulse, and so is a word that is not a pattern
        pressHaptic = vrcf::parseHapticPattern(pointer["sPressHaptic"].value_or("Tick"sv));

        // a word that is not one of the filter's lists the packages that need attention
        const auto developerSection = config[DEVELOPER_SECTION];
        developer.enabled = developerSection["bDeveloperTools"].value_or(false);
        const auto show = std::ranges::find(SHOW_PACKAGES, developerSection["sShowPackages"].value_or("attention"sv), &std::pair<std::string_view, ui::ShowPackages>::first);
        developer.show = show != SHOW_PACKAGES.end() ? show->second : ui::ShowPackages::NeedingAttention;
        developer.package = developerSection["sPackage"].value_or(""s);
        developer.folded = developerSection["bFolded"].value_or(false);

        // last, so the menu gives its panel the values when all of them are read
        panelChanged = true;
    }

    /**
     * Keeps the Developer section's filter and its fold, as a press on the menu's front page changed them, and
     * writes them to the file at once: the front page has no Save.
     */
    void Config::saveDeveloper(const ui::DeveloperView& view)
    {
        developer = view;
        const auto show = std::ranges::find(SHOW_PACKAGES, view.show, &std::pair<std::string_view, ui::ShowPackages>::second);
        saveConfigValues(DEVELOPER_SECTION, { { "sShowPackages", std::string(show->first) }, { "bFolded", view.folded } });
    }
}
