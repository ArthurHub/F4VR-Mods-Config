#include "GameMenu.h"

#include <shellapi.h>

#include "Config.h"
#include "Theme.h"
#include "api/FRIKApi.h"
#include "common/MatrixUtils.h"
#include "f4vr/F4VRUtils.h"
#include "imgui/ImGuiImage.h"
#include "imgui/ImGuiKeyboard.h"
#include "imgui/ImGuiPointer.h"
#include "imgui/ImGuiSettings.h"
#include "ui/Drawing.h"
#include "vrcf/VRControllersHaptic.h"
#include "vrcf/VRControllersManager.h"
#include "vrui/UIButtonPanel.h"
#include "vrui/UIManager.h"
#include "vrui/UIPanelStyle.h"
#include "vrui/UITextPanel.h"

using namespace vrui;

namespace
{
    constexpr float WRIST_BUTTON_WIDTH = 3.0f;
    constexpr float WRIST_BUTTON_HEIGHT = 2.5f;
    constexpr float WRIST_BUTTON_TEXT_HEIGHT = 0.5f;
    constexpr float WRIST_TITLE_TEXT_HEIGHT = 0.75f;
    constexpr float WRIST_BUTTON_GAP = 0.5f;

    // where the wrist buttons lie, from the framework's place on the wrist: position, then degrees around x, y and z
    const RE::NiTransform WRIST_BUTTONS_OFFSET = common::MatrixUtils::getTransform(0, 0, 0, 0, 0, 0);

    // the icons of the wrist buttons, under the mod's `Data\Textures\ModsConfigFO4VR`
    constexpr auto RESTORE_ICON = R"(vrui\restore.dds)";
    constexpr auto EXIT_ICON = R"(vrui\exit.DDS)";
}

namespace mods_config
{
    /**
     * The game's folders for the menu: MCM's, the Data folder and `My Games\Fallout4VR` for a config spec's files, the
     * config specs' and their live copies', in the mod's folder of My Games, and the categories the player gave mods,
     * beside the mod's config, as the mod's own config spec has it. And the game's checks of its plugins.
     */
    menu::MenuSetup GameMenu::setup()
    {
        return { .mcm = R"(Data\MCM)",
            .data = "Data",
            .myGames = common::getRelativePathInDocuments(R"(\My Games\Fallout4VR)"),
            .configSpecs = spec::CONFIG_SPEC_FOLDER,
            .liveCopies = BASE_MOD_PATH + R"(\Dev)",
            .categories = BASE_MOD_PATH + R"(\Categories.toml)",
            .plugins = spec::gamePluginChecks() };
    }

    /**
     * Puts the menu's panel in front of the player, and left there: a panel that follows the head moves under the ray.
     * The player moves it by holding it with the config's move button. When they walk away from it, it is put in front
     * of them again. The menu is not closed for that, as it can hold changes that are not saved yet. The wrist buttons
     * are made with it, hidden.
     */
    void GameMenu::openPanel(menu::PanelHandlers handlers)
    {
        // the size of the design system's body text: it is read as the first canvas draws, and later calls do nothing
        imgui::setFontSizePixels(theme::FONT_SIZE);
        _panel = std::make_shared<imgui::UIImGuiPanel>("ModsConfigFO4_Panel", g_config.panelWidth, g_config.panelHeight);
        applyPanelLook();
        // the menu's one ImGui panel: at this size it needs a texture of its own, and a dropdown's list or a dialog opens inside it
        _panel->setDedicated(true);
        _panel->setInteractive(true);
        _panel->setContent(std::move(handlers.draw));
        g_uiManager->attachPresetToWorldAtHMD(_panel, g_config.recenterDistance);
        createWristButtons(std::move(handlers.restorePressed), std::move(handlers.exitPressed));
    }

    void GameMenu::closePanel()
    {
        g_uiManager->detachElement(_panel, true);
        g_uiManager->detachElement(_wristButtons, true);
        _panel.reset();
        _wristButtons.reset();
        _imageTextures.clear();
        _configSpecTextures.clear();
    }

    /**
     * Shows the panel again, put in front of the player wherever it was left, or hides it.
     */
    void GameMenu::setPanelShown(const bool shown)
    {
        if (shown) {
            g_uiManager->recenterWorldElement(_panel);
        }
        _panel->setVisibility(shown);
    }

    void GameMenu::setWristButtonsShown(const bool shown)
    {
        _wristButtons->setVisibility(shown);
    }

    void GameMenu::recenterPanel()
    {
        g_uiManager->recenterWorldElement(_panel);
    }

    /**
     * Gives the ray the config's look, and the panel its look, its size and scale, its place from where it was put in
     * front of the player, and how it is moved: after a short press, where that is Back. The place is from where the
     * panel is now, also after a hand moved it. The framework's keyboard takes the panel's see-through and Back.
     */
    void GameMenu::applyPanelLook()
    {
        imgui::pointer().setStyle(g_config.pointerStyle);
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
        _panel->setScale(g_config.panelScale);
        _panel->setPosition(0, g_config.panelDistance, g_config.panelElevation);
        _panel->setRotation(common::MatrixUtils::getMatrixFromEulerAnglesDegrees(g_config.panelTilt, 0, 0));
        // the radius is in the panel's own units, so the curve is the same at any scale and distance; 0 is flat
        _panel->setCurveRadius(g_config.panelCurve * g_config.panelDistance / g_config.panelScale);
        g_uiManager->setWorldMoveBinding(g_config.moveBinding);
        g_uiManager->setWorldMoveSmoothing(g_config.moveSmoothing);

        // the framework's keyboard is already the panel's green: it takes the panel's see-through, and Back closes it
        auto keyboardStyle = imgui::keyboard().style();
        keyboardStyle.background = render::Color::rgba(15, 15, 15, g_config.panelBackgroundOpacity);
        imgui::keyboard().setStyle(keyboardStyle);
        imgui::keyboard().setCloseBinding(g_config.backBinding);

        // the framework's controls in the look of the design system
        controls::get().setStyle(ui::controlsStyle());
    }

    /**
     * A short press of Back on the hand that points at the panel, either hand, whose wand is hidden from the game then:
     * the game does not get the press, and a press while the player looks away is the game's. While the framework's
     * keyboard is open, Back is its close binding, which closes it as Done does, keeping the text, and not a step of
     * the menu.
     */
    bool GameMenu::backPressed()
    {
        return !imgui::keyboard().isOpen() && imgui::pointer().state().canvas && vrcf::VRControllers.check(g_config.backBinding, pointingHand());
    }

    bool GameMenu::isLeftHanded()
    {
        return RE::VRSettings::IsLeftHandedMode();
    }

    bool GameMenu::isGameMenuTakingInput()
    {
        return f4vr::isGameMenuTakingInput();
    }

    void GameMenu::playHaptic(const vrcf::HapticPattern& pattern)
    {
        vrcf::VRHaptics.trigger(pointingHand(), pattern);
    }

    void GameMenu::startAdjust(const model::Setting& setting, model::Store& store)
    {
        _adjust.start(setting, store);
    }

    bool GameMenu::isAdjusting() const
    {
        return _adjust.isActive();
    }

    bool GameMenu::adjustFrame()
    {
        return _adjust.onFrameUpdate();
    }

    void GameMenu::cancelAdjust()
    {
        _adjust.cancel();
    }

    void GameMenu::stopAdjust()
    {
        _adjust.stop();
    }

    /**
     * What the panel draws its images and icons with: an MCM image's pixels, and a config spec's textures, which the
     * game loads.
     */
    ui::ImageDrawer GameMenu::imageDrawer()
    {
        return { .textureSize =
                     [this](const std::string& texture) {
                         return textureSize(texture);
                     },
            .textureError =
                [this](const std::string& texture) {
                    return textureOf(texture).error();
                },
            .draw =
                [this](const model::Image& image, const ImVec2& min, const ImVec2& max) {
                    drawImage(image, min, max);
                },
            .drawIcon =
                [this](const std::string& texture, const ImVec2& min, const ImVec2& max, const ImU32 tint) {
                    imgui::addImage(ImGui::GetWindowDrawList(), textureOf(texture), min, max, tint);
                } };
    }

    void GameMenu::dropImageTextures()
    {
        _imageTextures.clear();
    }

    void GameMenu::openFile(const std::filesystem::path& file)
    {
        ShellExecuteW(nullptr, L"open", file.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    }

    void GameMenu::showInFolder(const std::filesystem::path& file)
    {
        const auto select = L"/select,\"" + file.wstring() + L"\"";
        ShellExecuteW(nullptr, L"open", L"explorer.exe", select.c_str(), nullptr, SW_SHOWNORMAL);
    }

    bool GameMenu::isFrikInstalled()
    {
        return frik::api::FRIKApi::inst != nullptr;
    }

    bool GameMenu::isFrikConfigOpen()
    {
        return frik::api::FRIKApi::inst->isConfigOpen();
    }

    /**
     * Open FRIK's main config through the Papyrus function its holotape calls: FRIK's API has no call
     * for it. The function takes no arguments.
     */
    void GameMenu::openFrikConfig()
    {
        logger::info("Open FRIK main config");
        f4vr::callPapyrusGlobalFunction("FRIK:FRIK", "OpenMainConfigurationMode");
    }

    void GameMenu::saveDeveloper(const ui::DeveloperView& view)
    {
        g_config.saveDeveloper(view);
    }

    void GameMenu::saveHelpOpened()
    {
        g_config.saveHelpOpened();
    }

    std::chrono::system_clock::time_point GameMenu::now()
    {
        return std::chrono::system_clock::now();
    }

    /**
     * The buttons on the offhand wrist under the menu's title, which show only while the panel is minimized: Restore
     * shows the panel again, and Exit closes the menu, which asks first while the mod that is shown has unsaved changes.
     */
    void GameMenu::createWristButtons(std::function<void()> restorePressed, std::function<void()> exitPressed)
    {
        const auto restoreButton = std::make_shared<UIButtonPanel>("ModsConfigFO4_RestoreButton", WRIST_BUTTON_WIDTH, WRIST_BUTTON_HEIGHT);
        restoreButton->setTextHeight(WRIST_BUTTON_TEXT_HEIGHT);
        restoreButton->setImage(RESTORE_ICON);
        restoreButton->setBottomText("RESTORE");
        restoreButton->setOnPressHandler([restorePressed = std::move(restorePressed)](UIButtonPanel*) {
            restorePressed();
        });

        const auto exitButton = std::make_shared<UIButtonPanel>("ModsConfigFO4_ExitButton", WRIST_BUTTON_WIDTH, WRIST_BUTTON_HEIGHT);
        exitButton->setTextHeight(WRIST_BUTTON_TEXT_HEIGHT);
        exitButton->setImage(EXIT_ICON);
        exitButton->setBottomText("EXIT");
        exitButton->setOnPressHandler([exitPressed = std::move(exitPressed)](UIButtonPanel*) {
            exitPressed();
        });

        const auto buttons = std::make_shared<UIContainer>("ModsConfigFO4_WristButtonRow", UIContainerLayout::HorizontalCenter, WRIST_BUTTON_GAP);
        buttons->addElement(restoreButton);
        buttons->addElement(exitButton);

        const auto title = std::make_shared<UITextPanel>("ModsConfigFO4_WristTitle");
        title->setStyle(UIPanelStyle{ .color = F4VR_PANEL_STYLE.color });
        title->setTextHeight(WRIST_TITLE_TEXT_HEIGHT);
        title->setContent([](std::vector<TextRow>& rows) {
            rows.emplace_back("MODS CONFIG", std::nullopt, render::TextDecoration::Underline);
        });

        // the title over the buttons: a column stacks its rows from the bottom up
        _wristButtons = std::make_shared<UIContainer>("ModsConfigFO4_WristButtons", UIContainerLayout::VerticalUp, WRIST_BUTTON_GAP);
        _wristButtons->addElement(buttons);
        _wristButtons->addElement(title);
        _wristButtons->setVisibility(false);

        g_uiManager->attachPresetToOffhandWrist(_wristButtons, WRIST_BUTTONS_OFFSET);
    }

    /**
     * The hand that points at the panel, which gets the pulse of its press, plays the pattern a haptic's press plays,
     * and has Back and the thumbstick.
     */
    vrcf::Hand GameMenu::pointingHand()
    {
        return imgui::pointer().state().primaryHand ? vrcf::Hand::Primary : vrcf::Hand::Offhand;
    }

    /**
     * Draws an image of a mod's page in the panel: an MCM image with a texture that is made from its pixels
     * when it is first drawn, and a config spec's with its texture. Called while the panel's content is drawn.
     */
    void GameMenu::drawImage(const model::Image& image, const ImVec2& min, const ImVec2& max)
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
     * The size of a config spec's texture in its pixels. None while the game has no view of it, as for a file
     * that is not there, whose reason the log has. Called while the panel's content is drawn, on the game thread.
     */
    std::optional<ImVec2> GameMenu::textureSize(const std::string& texture)
    {
        auto& loaded = textureOf(texture);
        if (!loaded.view() || loaded.width() == 0 || loaded.height() == 0) {
            return std::nullopt;
        }
        return ImVec2(static_cast<float>(loaded.width()), static_cast<float>(loaded.height()));
    }

    /**
     * A config spec's texture, by its path under `Data/Textures`, which the game loads the first time its view is asked
     * for: from a loose file or a BA2 archive, as a texture of a mesh is.
     */
    render::Texture& GameMenu::textureOf(const std::string& texture)
    {
        auto& loaded = _configSpecTextures[texture];
        if (!loaded) {
            // a config spec's path has `/` between its folders, and the game's has `\`
            auto path = R"(Data\Textures\)" + texture;
            std::ranges::replace(path, '/', '\\');
            loaded = render::Texture::load(path);
        }
        return *loaded;
    }
}
