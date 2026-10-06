#include "ConfigMenu.h"

#include <array>
#include <format>

#include <imgui.h>

#include "Config.h"
#include "api/FRIKApi.h"
#include "common/MatrixUtils.h"
#include "f4sevr/PapyrusUtils.h"
#include "imgui/ImGuiPointer.h"
#include "vrui/UIManager.h"
#include "vrui/UIPanelStyle.h"

using namespace vrui;

namespace
{
    // the panel in vrui units, and where it stands relative to the HMD: forward, up, and tilted back
    constexpr float PANEL_WIDTH = 20.0f;
    constexpr float PANEL_HEIGHT = 15.0f;
    constexpr float PANEL_SCALE = 6.0f;
    constexpr float PANEL_FORWARD = 120.0f;
    constexpr float PANEL_UP = -20.0f;
    constexpr float PANEL_TILT_DEGREES = 5.0f;

    constexpr float WRIST_BUTTON_WIDTH = 3.0f;
    constexpr float WRIST_BUTTON_HEIGHT = 2.2f;
    constexpr float WRIST_BUTTON_GAP = 0.2f;

    // where the wrist buttons lie, from the framework's place on the wrist: position, then degrees around x, y and z
    const RE::NiTransform WRIST_BUTTONS_OFFSET = common::MatrixUtils::getTransform(0, 0, 0, 0, 0, 0);

    constexpr std::array COMBO_OPTIONS{ "Low", "Medium", "High", "Ultra" };
    constexpr int LIST_ROWS = 40;
    constexpr float LIST_HEIGHT_PIXELS = 220.0f;
    constexpr auto DISCARD_POPUP = "Discard changes?";

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

    void ConfigMenu::setMinimized(const bool minimized)
    {
        if (!isOpen()) {
            return;
        }
        _panel->setVisibility(!minimized);
        _minimizeButton->setMiddleText(minimized ? "RESUME" : "MINIMIZE");
    }

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
        // the menu's one ImGui panel: at this size it needs a texture of its own, and its combo and dialog open inside it
        _panel->setDedicated(true);
        _panel->setInteractive(true);
        _panel->setContent([this] {
            drawContent();
        });
        g_uiManager->attachElement(_panel, UIUtils::getHMDAttachNode());
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
     * Placeholder content until the mod list and the settings pages exist: one of each kind of control
     * the menu uses.
     */
    void ConfigMenu::drawContent()
    {
        ImGui::TextUnformatted("MODS CONFIG VR");
        ImGui::Separator();

        if (ImGui::Button("Button")) {
            ++_controls.buttonPresses;
        }
        ImGui::SameLine();
        ImGui::Text("pressed %d times", _controls.buttonPresses);

        ImGui::Checkbox("Checkbox", &_controls.checkbox);
        ImGui::SliderFloat("Slider", &_controls.slider, 0.0f, 2.0f);

        if (ImGui::BeginChild("List", ImVec2(0.0f, LIST_HEIGHT_PIXELS), ImGuiChildFlags_Border)) {
            for (int i = 0; i < LIST_ROWS; ++i) {
                if (ImGui::Selectable(std::format("Row {}", i + 1).c_str(), i == _controls.listRow)) {
                    _controls.listRow = i;
                }
            }
        }
        ImGui::EndChild();

        if (ImGui::Button("Discard")) {
            ImGui::OpenPopup(DISCARD_POPUP);
        }
        if (ImGui::BeginPopupModal(DISCARD_POPUP, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::TextUnformatted("All pending changes will be lost.");
            if (ImGui::Button("Discard")) {
                _controls.buttonPresses = 0;
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel")) {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        // at the panel's bottom edge, where its list has no room below it and opens upward
        ImGui::SetCursorPosY(ImGui::GetWindowHeight() - ImGui::GetFrameHeight());
        if (ImGui::BeginCombo("Combo", COMBO_OPTIONS[_controls.comboOption])) {
            for (int i = 0; i < static_cast<int>(COMBO_OPTIONS.size()); ++i) {
                if (ImGui::Selectable(COMBO_OPTIONS[i], i == _controls.comboOption)) {
                    _controls.comboOption = i;
                }
            }
            ImGui::EndCombo();
        }
    }
}
