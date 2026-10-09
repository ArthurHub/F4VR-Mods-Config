#include "ConfigMenu.h"

#include <algorithm>

#include <imgui.h>

#include "Config.h"
#include "Theme.h"
#include "api/FRIKApi.h"
#include "common/CommonUtils.h"
#include "common/MatrixUtils.h"
#include "f4sevr/PapyrusUtils.h"
#include "imgui/ImGuiPointer.h"
#include "vrui/UIManager.h"
#include "vrui/UIPanelStyle.h"

using namespace vrui;

namespace
{
    // the panel in vrui units, and where it is put from the HMD: forward, up, and tilted back
    constexpr float PANEL_WIDTH = 24.0f;
    constexpr float PANEL_HEIGHT = 15.0f;
    constexpr float PANEL_SCALE = 6.0f;
    constexpr float PANEL_FORWARD = 120.0f;
    constexpr float PANEL_UP = -20.0f;
    constexpr float PANEL_TILT_DEGREES = 5.0f;

    // The radius the panel is curved at toward the player, in the panel's own units: a higher radius is a
    // flatter panel. At PANEL_FORWARD / PANEL_SCALE, which is 20, the sides are as far from the player as
    // the middle. It is a little flatter than that, as tuned in the headset.
    constexpr float PANEL_CURVE_RADIUS = 25.0f;

    constexpr float WRIST_BUTTON_WIDTH = 3.0f;
    constexpr float WRIST_BUTTON_HEIGHT = 2.2f;
    constexpr float WRIST_BUTTON_GAP = 0.2f;

    // where the wrist buttons lie, from the framework's place on the wrist: position, then degrees around x, y and z
    const RE::NiTransform WRIST_BUTTONS_OFFSET = common::MatrixUtils::getTransform(0, 0, 0, 0, 0, 0);

    /**
     * Open FRIK's main config through the Papyrus function its holotape calls: FRIK's API has no call
     * for it. The function takes no arguments, which is an empty array.
     */
    void openFrikConfig()
    {
        logger::info("Open FRIK main config");
        const auto vm = F4SEVR::getGameVM()->m_virtualMachine;
        F4SEVR::VMArray<F4SEVR::VMVariable> arguments;
        F4SEVR::VMValue packedArguments;
        F4SEVR::PackValue(&packedArguments, &arguments, vm);

        const F4SEVR::BSFixedString script("FRIK:FRIK");
        const F4SEVR::BSFixedString function("OpenMainConfigurationMode");
        F4SEVR::CallGlobalFunctionNoWait_Internal(vm, 0, 0, &script, &function, &packedArguments);
    }
}

namespace mods_config
{
    bool ConfigMenu::isOpen() const
    {
        return _panel != nullptr;
    }

    bool ConfigMenu::isMinimized() const
    {
        return _panel && !_panel->isVisible();
    }

    void ConfigMenu::open()
    {
        if (isOpen()) {
            return;
        }
        logger::info("Open config menu");
        createPanel();
        createWristButtons();
    }

    void ConfigMenu::close()
    {
        _closeRequested = false;
        _openFrikRequested = false;
        if (!isOpen()) {
            return;
        }
        logger::info("Close config menu");
        g_uiManager->detachElement(_panel, true);
        g_uiManager->detachElement(_wristButtons, true);
        _panel.reset();
        _wristButtons.reset();
        _minimizeButton.reset();
    }

    /**
     * Hide or show the panel. The offhand buttons stay either way.
     * A panel that is shown again is put in front of the player, wherever it was left.
     */
    void ConfigMenu::setMinimized(const bool minimized)
    {
        if (!isOpen()) {
            return;
        }
        if (!minimized) {
            g_uiManager->recenterWorldElement(_panel);
        }
        _panel->setVisibility(!minimized);
        _minimizeButton->setMiddleText(minimized ? "RESUME" : "MINIMIZE");
    }

    /**
     * The mods the menu lists, from every source they come from. They are kept in order of their display names.
     */
    void ConfigMenu::setMods(std::vector<model::Mod> mods)
    {
        std::ranges::sort(mods, common::CaseInsensitiveComparator(), &model::Mod::displayName);
        _mods = std::move(mods);
    }

    /**
     * Put the panel in front of the player again, where they look now.
     * Nothing while the menu is minimized: Resume does it then.
     */
    void ConfigMenu::recenter()
    {
        if (!isOpen() || isMinimized()) {
            return;
        }
        logger::info("Recenter config menu");
        g_uiManager->recenterWorldElement(_panel);
    }

    /**
     * Applies what a button press asked for: closing the menu, or minimizing it and opening FRIK's config.
     * It is done here, in the mod's frame update, and not in the press handler, which runs while the
     * UI manager iterates its elements.
     */
    void ConfigMenu::onFrameUpdate()
    {
        // every frame, so a change of the config shows while the menu is open
        imgui::pointer().setStyle(g_config.pointerStyle);

        if (_openFrikRequested) {
            _openFrikRequested = false;
            setMinimized(true);
            // FRIK builds another config UI on every call, also when one is already open
            if (!frik::api::FRIKApi::inst->isConfigOpen()) {
                openFrikConfig();
            }
        } else if (_closeRequested) {
            close();
        }
    }

    void ConfigMenu::createPanel()
    {
        _panel = std::make_shared<imgui::UIImGuiPanel>("ModsConfigVR_Panel", PANEL_WIDTH, PANEL_HEIGHT);
        _panel->setStyle({
            // Opaque, both of them: these are picked to match the widgets beside the panel, and an alpha
            // below 255 blends a colour toward whatever is behind it, which reads as the wrong colour
            // rather than as a see-through one. The background is the only part that wants an alpha.
            .color = render::Color::rgba(10, 250, 120),
            .background = render::Color::rgba(15, 15, 15, 166),
            .borderColor = render::Color::rgba(10, 250, 120),
            .borderThicknessUnits = 0.08f,
            .cornerRadiusUnits = 0.5f,
            .padding = UIPadding::all(0.3f),
        });
        _panel->setScale(PANEL_SCALE);
        _panel->setPosition(0, PANEL_FORWARD, PANEL_UP);
        _panel->setRotation(common::MatrixUtils::getMatrixFromEulerAnglesDegrees(PANEL_TILT_DEGREES, 0, 0));
        _panel->setCurveRadius(PANEL_CURVE_RADIUS);
        // the menu's one ImGui panel: at this size it needs a texture of its own, and a dropdown's list or a dialog opens inside it
        _panel->setDedicated(true);
        _panel->setInteractive(true);
        _panel->setContent([this] {
            drawContent();
        });
        // In front of the player, and left there: a panel that follows the head moves under the ray.
        // The player moves it by holding it with the config's move button. When they walk away from it,
        // it is put in front of them again. The menu is not closed for that, as it can hold changes that
        // are not saved yet.
        g_uiManager->setWorldMoveButton(g_config.moveButton);
        g_uiManager->setWorldMoveSmoothing(g_config.moveSmoothing);
        g_uiManager->attachPresetToWorldAtHMD(_panel, g_config.recenterDistance);
    }

    void ConfigMenu::createWristButtons()
    {
        _minimizeButton = std::make_shared<UIButtonPanel>("ModsConfigVR_MinimizeButton", WRIST_BUTTON_WIDTH, WRIST_BUTTON_HEIGHT);
        _minimizeButton->setTopText("CLICK");
        _minimizeButton->setMiddleText("MINIMIZE");
        _minimizeButton->setOnPressHandler([this](UIButtonPanel*) {
            setMinimized(!isMinimized());
        });

        const auto closeButton = std::make_shared<UIButtonPanel>("ModsConfigVR_CloseButton", WRIST_BUTTON_WIDTH, WRIST_BUTTON_HEIGHT);
        closeButton->setTopText("CLICK");
        closeButton->setMiddleText("CLOSE");
        closeButton->setOnPressHandler([this](UIButtonPanel*) {
            _closeRequested = true;
        });

        _wristButtons = std::make_shared<UIContainer>("ModsConfigVR_WristButtons", UIContainerLayout::HorizontalCenter, WRIST_BUTTON_GAP);
        _wristButtons->addElement(_minimizeButton);
        _wristButtons->addElement(closeButton);

        // this menu opens with the gesture taken from FRIK, so FRIK's own config is opened from here
        if (frik::api::FRIKApi::inst) {
            const auto frikButton = std::make_shared<UIButtonPanel>("ModsConfigVR_FrikButton", WRIST_BUTTON_WIDTH, WRIST_BUTTON_HEIGHT);
            frikButton->setTopText("CLICK");
            frikButton->setMiddleText("FRIK");
            frikButton->setOnPressHandler([this](UIButtonPanel*) {
                _openFrikRequested = true;
            });
            _wristButtons->addElement(frikButton);
        }

        g_uiManager->attachPresetToOffhandWrist(_wristButtons, WRIST_BUTTONS_OFFSET);
    }

    /**
     * The Full panel: the title, the list of the mods on the left, and beside it the menu's front page.
     */
    void ConfigMenu::drawContent() const
    {
        ImGui::TextUnformatted("MODS CONFIG VR");
        ImGui::Separator();

        // the list keeps its place and its width, whatever is shown beside it
        if (ImGui::BeginChild("Mods", ImVec2(theme::NAV_WIDTH, 0.0f))) {
            drawModList();
        }
        ImGui::EndChild();

        ImGui::SameLine(0.0f, theme::SPACE_4);
        if (ImGui::BeginChild("Content")) {
            drawFrontPage();
        }
        ImGui::EndChild();
    }

    /**
     * The mods, under a header with their number. The header stays, and the rows scroll under it.
     */
    void ConfigMenu::drawModList() const
    {
        const auto count = std::to_string(_mods.size());
        ImGui::TextDisabled("Mods");
        ImGui::SameLine(ImGui::GetContentRegionMax().x - ImGui::CalcTextSize(count.c_str()).x);
        ImGui::TextDisabled("%s", count.c_str());

        if (ImGui::BeginChild("ModRows")) {
            for (const auto& mod : _mods) {
                ImGui::TextUnformatted(mod.displayName.c_str());
            }
        }
        ImGui::EndChild();
    }

    /**
     * The menu's front page, which is what the menu shows while no mod is selected.
     */
    void ConfigMenu::drawFrontPage() const
    {
        ImGui::TextWrapped("Settings for the mods you have installed. Pick a mod on the left to see its settings.");
        ImGui::TextDisabled("Version %s", Version::NAME.data());
    }
}
