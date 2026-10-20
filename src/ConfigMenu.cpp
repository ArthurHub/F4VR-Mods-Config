#include "ConfigMenu.h"

#include "Config.h"
#include "Theme.h"
#include "api/FRIKApi.h"
#include "common/MatrixUtils.h"
#include "f4sevr/PapyrusUtils.h"
#include "f4vr/F4VRUtils.h"
#include "imgui/ImGuiImage.h"
#include "imgui/ImGuiPointer.h"
#include "imgui/ImGuiSettings.h"
#include "vrcf/VRControllersHaptic.h"
#include "vrcf/VRControllersManager.h"
#include "vrui/UIManager.h"
#include "vrui/UIPanelStyle.h"

using namespace vrui;

namespace
{
    // The radius the panel is curved at toward the player, for its distance: a higher radius is a flatter
    // panel. At 1 the sides are as far from the player as the middle. It is a little flatter than that, as
    // tuned in the headset: 25 of the panel's own units at the shipped distance and scale, which are 20.
    constexpr float PANEL_CURVE_BY_DISTANCE = 1.25f;

    // The longest a press of the Back button is held to be Back, in seconds. On the button that moves the panel,
    // a press that is held longer moves it.
    constexpr float SHORT_PRESS_SECONDS = 0.3f;

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
        // a menu that was closed does not keep its place
        _fullPanel.reset();
        createPanel();
        createWristButtons();
        _store.menuOpened();
    }

    void ConfigMenu::close()
    {
        _minimizeRequested = false;
        _closeRequested = false;
        _askToCloseRequested = false;
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
        _imageTextures.clear();
        _packageTextures.clear();
        // the mod that is shown is left, which its scripts are told before they are told of the menu
        _fullPanel.reset();
        _store.menuClosed();
    }

    /**
     * Closes the menu as the player does, with the gesture or Close on the wrist: while the mod that is shown
     * has unsaved changes the panel's dialog asks first, and its Save and Discard close the menu. A panel that
     * is minimized is shown again for it.
     */
    void ConfigMenu::askToClose()
    {
        if (!isOpen()) {
            return;
        }
        // with nothing to ask the panel closes the menu with its action, which this frame update does
        _fullPanel.close();
        if (_fullPanel.isAsking()) {
            setMinimized(false);
        }
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

    void ConfigMenu::setMods(std::vector<model::Mod> mods)
    {
        // the textures are kept by the pixels of the mods that are replaced
        _imageTextures.clear();
        _fullPanel.setMods(std::move(mods));
    }

    const std::vector<model::Mod>& ConfigMenu::mods() const
    {
        return _fullPanel.mods();
    }

    /**
     * Shows a mod's front page or one of its pages, as a press on its row in the tree does.
     */
    void ConfigMenu::select(const ui::Selection& selection)
    {
        _fullPanel.select(selection);
    }

    const ui::Selection& ConfigMenu::selection() const
    {
        return _fullPanel.selection();
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
     * Goes one step back in the panel, which first ends the drag of a slider, and closes the menu from the
     * panel's front page.
     * Nothing while the menu is minimized: its panel is not shown then.
     */
    void ConfigMenu::back()
    {
        if (!isOpen() || isMinimized()) {
            return;
        }
        if (!_fullPanel.back()) {
            close();
        }
    }

    /**
     * Applies what a press asked for: closing the menu, which Close on the wrist asks first, minimizing it, or
     * minimizing it and opening FRIK's config.
     * It is done here, in the mod's frame update, and not where the press is seen: a wrist button's handler
     * runs while the UI manager iterates its elements, and the panel's content while its frame is built.
     * Back is seen here too. It is a short press of a button of the hand that points at the panel, either hand,
     * whose wand is hidden from the game then: the game does not get the press, and a press while the player
     * looks away is the game's.
     */
    void ConfigMenu::onFrameUpdate()
    {
        // every frame, so a change of the config shows while the menu is open
        imgui::pointer().setStyle(g_config.pointerStyle);
        if (isOpen() && g_config.panelChanged.exchange(false)) {
            applyPanelConfig();
        }

        const auto& pointer = imgui::pointer().state();
        if (isOpen() && pointer.canvas && g_config.backButton &&
            vrcf::VRControllers.isReleased(pointer.primaryHand ? vrcf::Hand::Primary : vrcf::Hand::Offhand, *g_config.backButton, SHORT_PRESS_SECONDS)) {
            // before the step: the menu's front page closes the menu, and the pointer is gone with it
            pulse();
            back();
        }

        if (_askToCloseRequested) {
            _askToCloseRequested = false;
            askToClose();
        }
        if (_openFrikRequested) {
            _openFrikRequested = false;
            setMinimized(true);
            // FRIK builds another config UI on every call, also when one is already open
            if (!frik::api::FRIKApi::inst->isConfigOpen()) {
                openFrikConfig();
            }
        } else if (_closeRequested) {
            close();
        } else if (_minimizeRequested) {
            _minimizeRequested = false;
            setMinimized(true);
        }
    }

    void ConfigMenu::createPanel()
    {
        // the size of the design system's body text: it is read as the first canvas draws, and later calls do nothing
        imgui::setFontSizePixels(theme::FONT_SIZE);
        _panel = std::make_shared<imgui::UIImGuiPanel>("ModsConfigVR_Panel", g_config.panelWidth, g_config.panelHeight);
        applyPanelConfig();
        // the menu's one ImGui panel: at this size it needs a texture of its own, and a dropdown's list or a dialog opens inside it
        _panel->setDedicated(true);
        _panel->setInteractive(true);
        _panel->setContent([this] {
            _fullPanel.setThumbstickSideValue(getThumbstickSideValue());
            _fullPanel.draw();
            if (_fullPanel.pressActed()) {
                pulse();
            }
        });
        _fullPanel.setQuickLaunch(quickLaunchEntries());
        _fullPanel.setMenuActions({ .minimize =
                                        [this] {
                                            _minimizeRequested = true;
                                        },
            .close =
                [this] {
                    _closeRequested = true;
                } });
        _fullPanel.setStore(model::Source::Mcm, &_store);
        _fullPanel.setStore(model::Source::Package, &_packageStore);
        _fullPanel.setImageDrawer({ .textureSize =
                                        [this](const std::string& texture) {
                                            return textureSize(texture);
                                        },
            .draw =
                [this](const model::Image& image, const ImVec2& min, const ImVec2& max) {
                    drawImage(image, min, max);
                } });
        // In front of the player, and left there: a panel that follows the head moves under the ray.
        // The player moves it by holding it with the config's move button. When they walk away from it,
        // it is put in front of them again. The menu is not closed for that, as it can hold changes that
        // are not saved yet.
        g_uiManager->attachPresetToWorldAtHMD(_panel, g_config.recenterDistance);
    }

    /**
     * Gives the panel what the config has for it: its look, its size and scale, the width of its tree, its place from where it was put in
     * front of the player, how it is moved: after a short press, where that is Back, and the controls its Help page tells. It is called when
     * the menu opens, and when the config is loaded again while the menu is open, so a change of the file shows at once. The place is from
     * where the panel is now, also after a hand moved it.
     */
    void ConfigMenu::applyPanelConfig()
    {
        _fullPanel.setControls(ui::menuControls({ .openMenu = g_config.openMenuBinding,
            .back = g_config.backButton,
            .move = g_config.moveButton,
            .recenter = g_config.recenterBinding,
            .leftHanded = f4vr::isLeftHandedMode() }));
        _panel->setStyle({
            // Opaque, both of them: these are picked to match the widgets beside the panel, and an alpha
            // below 255 blends a colour toward whatever is behind it, which reads as the wrong colour
            // rather than as a see-through one. The background is the only part that wants an alpha.
            .color = render::Color::rgba(10, 250, 120),
            .background = render::Color::rgba(15, 15, 15, g_config.panelBackgroundOpacity),
            .borderColor = render::Color::rgba(10, 250, 120),
            .borderThicknessUnits = 0.08f,
            .cornerRadiusUnits = 0.5f,
            .padding = UIPadding::all(0.3f),
        });
        // the canvas follows the size at its next frame, with as many pixels per unit
        _panel->setSize(g_config.panelWidth, g_config.panelHeight);
        _fullPanel.setTreeWidth(g_config.treeWidth * imgui::CANVAS_PIXELS_PER_UNIT);
        _panel->setScale(g_config.panelScale);
        _panel->setPosition(0, g_config.panelDistance, g_config.panelElevation);
        _panel->setRotation(common::MatrixUtils::getMatrixFromEulerAnglesDegrees(g_config.panelTilt, 0, 0));
        // the radius is in the panel's own units, so the curve is the same at any scale and distance
        _panel->setCurveRadius(PANEL_CURVE_BY_DISTANCE * g_config.panelDistance / g_config.panelScale);
        g_uiManager->setWorldMoveButton(g_config.moveButton);
        g_uiManager->setWorldMoveSmoothing(g_config.moveSmoothing);
        // Back and the move are on one button as shipped: a short press of it is Back, and a longer one moves
        g_uiManager->setWorldMoveHoldDelay(g_config.backButton == g_config.moveButton ? SHORT_PRESS_SECONDS : 0.0f);
    }

    /**
     * Gives the hand that points the config's pulse: in the frame its press does something in the panel, and
     * when its Back goes a step back or closes the menu.
     */
    void ConfigMenu::pulse()
    {
        if (!g_config.pressHaptic) {
            return;
        }
        vrcf::VRHaptics.trigger(imgui::pointer().state().primaryHand ? vrcf::Hand::Primary : vrcf::Hand::Offhand, *g_config.pressHaptic);
    }

    /**
     * How far the thumbstick of the hand that points at the panel is pushed to a side, which steps a slider or a stepper
     * of the panel: from -1 at the left to 1 at the right. None while no hand points, as the thumbstick is
     * the game's then, and none for a push that is more up or down, which scrolls.
     * Called while the panel's content is drawn, when the pointer's state is this frame's.
     */
    float ConfigMenu::getThumbstickSideValue()
    {
        const auto& pointer = imgui::pointer().state();
        if (!pointer.canvas) {
            return 0.0f;
        }
        const auto push = vrcf::VRControllers.getThumbstickValue(pointer.primaryHand ? vrcf::Hand::Primary : vrcf::Hand::Offhand);
        return std::abs(push.x) > std::abs(push.y) ? push.x : 0.0f;
    }

    /**
     * Draws an image of a mod's page in the panel: an MCM image with a texture that is made from its pixels
     * when it is first drawn, and a package's with its texture. Called while the panel's content is drawn.
     */
    void ConfigMenu::drawImage(const model::Image& image, const ImVec2& min, const ImVec2& max)
    {
        if (!image.pixels) {
            imgui::addImage(ImGui::GetWindowDrawList(), textureOf(image.texture), min, max);
            return;
        }
        const auto& pixels = *image.pixels;
        auto& texture = _imageTextures[&pixels];
        if (!texture) {
            texture = render::Texture::fromPixels(pixels.name, pixels.width, pixels.height, pixels.rgba);
        }
        imgui::addImage(ImGui::GetWindowDrawList(), *texture, min, max);
    }

    /**
     * The size of a package's texture in its pixels. None while the game has no view of it, as for a file
     * that is not there, whose reason the log has. Called while the panel's content is drawn, on the game thread.
     */
    std::optional<ImVec2> ConfigMenu::textureSize(const std::string& texture)
    {
        auto& loaded = textureOf(texture);
        if (!loaded.view() || loaded.width() == 0 || loaded.height() == 0) {
            return std::nullopt;
        }
        return ImVec2(static_cast<float>(loaded.width()), static_cast<float>(loaded.height()));
    }

    /**
     * A package's texture, by its path under `Data/Textures`, which the game loads the first time its view is asked
     * for: from a loose file or a BA2 archive, as a texture of a mesh is.
     */
    render::Texture& ConfigMenu::textureOf(const std::string& texture)
    {
        auto& loaded = _packageTextures[texture];
        if (!loaded) {
            // a package's path has `/` between its folders, and the game's has `\`
            auto path = R"(Data\Textures\)" + texture;
            std::ranges::replace(path, '/', '\\');
            loaded = render::Texture::load(path);
        }
        return *loaded;
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
            _askToCloseRequested = true;
        });

        _wristButtons = std::make_shared<UIContainer>("ModsConfigVR_WristButtons", UIContainerLayout::HorizontalCenter, WRIST_BUTTON_GAP);
        _wristButtons->addElement(_minimizeButton);
        _wristButtons->addElement(closeButton);

        g_uiManager->attachPresetToOffhandWrist(_wristButtons, WRIST_BUTTONS_OFFSET);
    }

    /**
     * The mods whose own config UI is opened from the menu's front page. FRIK's is the one there is, with
     * FRIK installed: this menu took its opening gesture.
     */
    std::vector<ui::FullPanel::QuickLaunchEntry> ConfigMenu::quickLaunchEntries()
    {
        std::vector<ui::FullPanel::QuickLaunchEntry> entries;
        if (frik::api::FRIKApi::inst) {
            entries.push_back({ .name = "FRIK Configuration",
                .help = "Opens FRIK's own configuration in the world: body height, arm length, and the weapon and Pip-Boy positions.",
                .open = [this] {
                    _openFrikRequested = true;
                } });
        }
        return entries;
    }
}
