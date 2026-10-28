#include "ConfigMenu.h"

#include <chrono>
#include <ctime>
#include <filesystem>
#include <format>

#include <shellapi.h>

#include "Config.h"
#include "Theme.h"
#include "api/FRIKApi.h"
#include "common/MatrixUtils.h"
#include "f4sevr/PapyrusUtils.h"
#include "f4vr/F4VRUtils.h"
#include "imgui/ImGuiImage.h"
#include "imgui/ImGuiPointer.h"
#include "imgui/ImGuiSettings.h"
#include "spec/ConfigSpecLoader.h"
#include "vrcf/VRControllersHaptic.h"
#include "vrcf/VRControllersManager.h"
#include "vrui/UIManager.h"
#include "vrui/UIPanelStyle.h"
#include "vrui/UITextPanel.h"

using namespace vrui;

namespace
{
    // The radius the panel is curved at toward the player, for its distance: a higher radius is a flatter
    // panel. At 1 the sides are as far from the player as the middle. It is a little flatter than that, as
    // tuned in the headset: 25 of the panel's own units at the shipped distance and scale, which are 20.
    constexpr float PANEL_CURVE_BY_DISTANCE = 1.25f;

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

    // the id of the Config spec setting of the menu's own config spec, whose options the menu gives
    constexpr auto DEVELOPER_CONFIG_SPEC_ID = "developerConfigSpec";

    // the name FRIK's client of the native API registers under, whose quick launch entry replaces the menu's own
    constexpr auto FRIK_CLIENT_NAME = "FRIK";

    // the folder of the live copies of the config specs' spec.json, in the mod's folder of My Games
    const auto LIVE_COPIES_FOLDER = mods_config::BASE_MOD_PATH + R"(\Dev)";

    // how often the frame update looks for a live copy that was saved
    constexpr auto LIVE_COPIES_CHECK = std::chrono::milliseconds(500);

    /**
     * Where the live copy of the config spec of a folder is, as its report says it: from `My Games`.
     */
    std::string shownLiveCopy(const std::string& folder)
    {
        return std::format(R"(My Games\Fallout4VR\Mods_Config\{}\Dev\{}\spec.json)", Version::PROJECT, folder);
    }

    /**
     * The time of day now, as a read of a live copy says when it was: `10:42:05`.
     */
    std::string timeOfDay()
    {
        const auto now = std::time(nullptr);
        std::tm local{};
        localtime_s(&local, &now);
        return std::format("{:02}:{:02}:{:02}", local.tm_hour, local.tm_min, local.tm_sec);
    }

    /**
     * The places of the pages on the way to the page of an id, under the pages given, with no regard to case. None
     * for an id no page has.
     */
    std::optional<std::vector<std::size_t>> pagePath(const std::vector<mods_config::model::Page>& pages, const std::string& id)
    {
        for (std::size_t place = 0; place < pages.size(); ++place) {
            if (common::str_tolower(pages[place].id) == id) {
                return std::vector{ place };
            }
            if (auto under = pagePath(pages[place].pages, id)) {
                under->insert(under->begin(), place);
                return under;
            }
        }
        return std::nullopt;
    }

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
    /**
     * The menu, closed. A config spec is read from its live copy while its author's report turns it on.
     */
    ConfigMenu::ConfigMenu()
        : _liveCopies(spec::CONFIG_SPEC_FOLDER, LIVE_COPIES_FOLDER)
    {
        _nativeClients.setTranslations(&_translations);
        _fullPanel.setLiveCopyAction([this](const std::string& folder, const ui::LiveCopyAction action) {
            _liveCopyRequest = LiveCopyRequest{ .folder = folder, .action = action };
        });
    }

    /**
     * Adds the options of its own config spec's Config spec setting to the mod's client of the native API, as any
     * CommonFramework mod adds its own: none first, then each config spec the menu lists, by its modName and its name,
     * in their order. The framework connects the client at kPostLoad.
     */
    void ConfigMenu::registerOwnOptions()
    {
        api::ModsConfigFO4Client::get().addOptions(DEVELOPER_CONFIG_SPEC_ID, [this] {
            std::vector<api::Option> options{ { .value = std::string(), .text = "None" } };
            for (const auto& mod : mods()) {
                if (mod.source == model::Source::ConfigSpec) {
                    options.push_back({ .value = mod.name, .text = mod.displayName });
                }
            }
            return options;
        });
    }

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
        _configSpecStore.clientCalls().sendToAll(model::ClientEvent::MenuOpened);
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
        _imageTextures.clear();
        _configSpecTextures.clear();
        // the mod that is shown is left, which its scripts and its client are told before they are told of the menu
        _fullPanel.reset();
        _store.menuClosed();
        _configSpecStore.clientCalls().sendToAll(model::ClientEvent::MenuClosed);
    }

    /**
     * Closes the menu as the player does, with the gesture or Exit on the wrist: while the mod that is shown
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
     * Hide the panel and show the offhand buttons in its place, or the other way round.
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
        _wristButtons->setVisibility(minimized);
    }

    /**
     * The mods the menu lists, and the texts of the game's language they were read with, which a config spec is read
     * again with from its live copy.
     */
    void ConfigMenu::setMods(std::vector<model::Mod> mods, model::Translations translations)
    {
        // the textures are kept by the pixels of the mods that are replaced
        _imageTextures.clear();
        _fullPanel.setMods(std::move(mods));
        _translations = std::move(translations);
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
     * Nothing while the menu is minimized: Restore does it then.
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
     * Applies what a press asked for: closing the menu, which Exit on the wrist asks first, minimizing it,
     * minimizing it and opening FRIK's config, or what a row of a config spec's live copy asks. And reads the live
     * copies that were saved, also while the menu is closed.
     * It is done here, in the mod's frame update, and not where the press is seen: a wrist button's handler
     * runs while the UI manager iterates its elements, and the panel's content while its frame is built.
     * Back is seen here too. It is a short press of a button of the hand that points at the panel, either hand,
     * whose wand is hidden from the game then: the game does not get the press, and a press while the player
     * looks away is the game's.
     */
    void ConfigMenu::onFrameUpdate()
    {
        // the pause menu's events of a Save wait for MCM's file, also after the menu is closed
        _store.frameUpdate();
        // every frame, so a change of the config shows while the menu is open
        imgui::pointer().setStyle(g_config.pointerStyle);
        if (_developerToSave) {
            g_config.saveDeveloper(*_developerToSave);
            _developerToSave.reset();
        }
        if (_liveCopyRequest) {
            const auto request = std::move(*_liveCopyRequest);
            _liveCopyRequest.reset();
            doLiveCopy(request.folder, request.action);
        }
        readChangedLiveCopies();
        doClientRequests();
        if (isOpen() && g_config.panelChanged.exchange(false)) {
            applyPanelConfig();
        }

        const auto& pointer = imgui::pointer().state();
        if (isOpen() && pointer.canvas && vrcf::VRControllers.check(g_config.backBinding, pointer.primaryHand ? vrcf::Hand::Primary : vrcf::Hand::Offhand)) {
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

    /**
     * Asks the menu to open at a mod, and at a page of it by its `id`, for a mod's client: at the mod's front page with
     * no page, and at the menu's front page with no mod. Both are matched with no regard to case. The frame update
     * opens it, shows it if it is minimized, and selects it, which asks first while the mod that is shown has unsaved
     * changes. Returns false for a mod or a page that is not there, with nothing asked.
     */
    bool ConfigMenu::requestOpen(const std::string_view modName, const std::string_view pageId)
    {
        ui::Selection selection;
        if (!modName.empty()) {
            const auto& mods = _fullPanel.mods();
            const auto name = common::str_tolower(std::string(modName));
            const auto mod = std::ranges::find_if(mods, [&name](const model::Mod& each) {
                return common::str_tolower(each.name) == name;
            });
            if (mod == mods.end()) {
                logger::warn("A client asks to open the menu at the mod '{}', which it does not have", modName);
                return false;
            }
            selection.mod = static_cast<std::size_t>(mod - mods.begin());
            if (!pageId.empty()) {
                auto path = pagePath(mod->pages, common::str_tolower(std::string(pageId)));
                if (!path) {
                    logger::warn("A client asks to open the menu at the page '{}' of '{}', which it does not have", pageId, modName);
                    return false;
                }
                selection.page = std::move(*path);
            }
        }
        _openRequest = std::move(selection);
        return true;
    }

    /**
     * Asks the menu to minimize, for a mod's client that opens its own UI. Nothing while it is closed.
     */
    void ConfigMenu::requestMinimize()
    {
        _minimizeRequested = isOpen();
    }

    /**
     * Asks the menu to show itself again where it was, for a mod's client whose UI closed: only while it is minimized,
     * so a menu the player showed or closed is left as it is. Its values are read again then.
     */
    void ConfigMenu::requestRestore()
    {
        _restoreRequested = true;
    }

    /**
     * Asks the menu to read the values of a mod again while it is shown, for its client, which changed them.
     */
    void ConfigMenu::requestReadAgain(const std::string_view modName)
    {
        _readAgainRequest = modName;
    }

    /**
     * Does what the mods' clients asked through the native API since the frame before: opens the menu at a mod and a
     * page, shows it again with the values of the mod that is shown read again, or reads them again alone.
     */
    void ConfigMenu::doClientRequests()
    {
        if (_openRequest) {
            const auto selection = std::move(*_openRequest);
            _openRequest.reset();
            if (!isOpen()) {
                open();
            } else if (isMinimized()) {
                setMinimized(false);
            }
            _fullPanel.select(selection);
        }
        if (_restoreRequested) {
            _restoreRequested = false;
            if (isMinimized()) {
                setMinimized(false);
                readSelectedAgain();
            }
        }
        if (!_readAgainRequest.empty()) {
            const auto& selection = _fullPanel.selection();
            if (isOpen() && selection.mod && common::str_tolower(_fullPanel.mods()[*selection.mod].name) == common::str_tolower(_readAgainRequest)) {
                readSelectedAgain();
            }
            _readAgainRequest.clear();
        }
    }

    /**
     * Reads the values of the mod that is shown again, in its store.
     */
    void ConfigMenu::readSelectedAgain()
    {
        if (auto* const store = _fullPanel.selectedStore()) {
            store->readAgain();
        }
    }

    void ConfigMenu::createPanel()
    {
        // the size of the design system's body text: it is read as the first canvas draws, and later calls do nothing
        imgui::setFontSizePixels(theme::FONT_SIZE);
        _panel = std::make_shared<imgui::UIImGuiPanel>("ModsConfigFO4_Panel", g_config.panelWidth, g_config.panelHeight);
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
        _fullPanel.setStore(model::Source::ConfigSpec, &_configSpecStore);
        _fullPanel.setImageDrawer({ .textureSize =
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
                } });
        // In front of the player, and left there: a panel that follows the head moves under the ray.
        // The player moves it by holding it with the config's move button. When they walk away from it,
        // it is put in front of them again. The menu is not closed for that, as it can hold changes that
        // are not saved yet.
        g_uiManager->attachPresetToWorldAtHMD(_panel, g_config.recenterDistance);
    }

    /**
     * Gives the panel what the config has for it: its look, its size and scale, the width of its tree, its place from where it was put in
     * front of the player, how it is moved: after a short press, where that is Back, the controls its Help page tells, the player's
     * handedness, which a binding's buttons are printed by, and the Developer
     * tools, whose filter and fold a press on the front page writes back from the frame update. It is called when
     * the menu opens, and when the config is loaded again while the menu is open, so a change of the file shows at once. The place is from
     * where the panel is now, also after a hand moved it.
     */
    void ConfigMenu::applyPanelConfig()
    {
        _fullPanel.setDeveloper(g_config.developer, [this](const ui::DeveloperView& view) {
            _developerToSave = view;
        });
        _fullPanel.setControls(ui::menuControls({ .openMenu = g_config.openMenuBinding,
            .back = g_config.backBinding,
            .move = g_config.moveBinding,
            .recenter = g_config.recenterBinding,
            .leftHanded = f4vr::isLeftHandedMode() }));
        _fullPanel.setLeftHanded(f4vr::isLeftHandedMode());
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
        g_uiManager->setWorldMoveBinding(g_config.moveBinding);
        g_uiManager->setWorldMoveSmoothing(g_config.moveSmoothing);
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
     * when it is first drawn, and a config spec's with its texture. Called while the panel's content is drawn.
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
     * The size of a config spec's texture in its pixels. None while the game has no view of it, as for a file
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
     * A config spec's texture, by its path under `Data/Textures`, which the game loads the first time its view is asked
     * for: from a loose file or a BA2 archive, as a texture of a mesh is.
     */
    render::Texture& ConfigMenu::textureOf(const std::string& texture)
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

    /**
     * The buttons on the offhand wrist under the menu's title, which show only while the panel is minimized: Restore
     * shows the panel again, and Exit closes the menu, which asks first while the mod that is shown has unsaved changes.
     */
    void ConfigMenu::createWristButtons()
    {
        const auto restoreButton = std::make_shared<UIButtonPanel>("ModsConfigFO4_RestoreButton", WRIST_BUTTON_WIDTH, WRIST_BUTTON_HEIGHT);
        restoreButton->setTextHeight(WRIST_BUTTON_TEXT_HEIGHT);
        restoreButton->setImage(RESTORE_ICON);
        restoreButton->setBottomText("RESTORE");
        restoreButton->setOnPressHandler([this](UIButtonPanel*) {
            setMinimized(false);
        });

        const auto exitButton = std::make_shared<UIButtonPanel>("ModsConfigFO4_ExitButton", WRIST_BUTTON_WIDTH, WRIST_BUTTON_HEIGHT);
        exitButton->setTextHeight(WRIST_BUTTON_TEXT_HEIGHT);
        exitButton->setImage(EXIT_ICON);
        exitButton->setBottomText("EXIT");
        exitButton->setOnPressHandler([this](UIButtonPanel*) {
            _askToCloseRequested = true;
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
     * Does what a row of a config spec's live copy in its report asks: on copies its spec.json where there is no
     * copy, and reads the copy, off reads its installed spec.json again, Reload reads the copy again, Overwrite copies
     * the installed spec.json over the copy and reads it, Open opens it in the desktop's editor for `.json`, and Show
     * opens its folder in Explorer, with the copy selected. A copy that cannot be made leaves the switch off, and one
     * that cannot be overwritten is as it was, with the reason in the log.
     */
    void ConfigMenu::doLiveCopy(const std::string& folder, const ui::LiveCopyAction action)
    {
        const auto copy = _liveCopies.copyOf(folder);
        switch (action) {
        case ui::LiveCopyAction::TurnOn:
            if (const auto on = _liveCopies.turnOn(folder); !on) {
                logger::warn("Config spec '{}': its live copy is not read: {}", folder, on.error());
                return;
            }
            logger::info("Config spec '{}' is read from its live copy '{}'", folder, shownLiveCopy(folder));
            readLiveCopy(folder);
            break;
        case ui::LiveCopyAction::TurnOff:
            _liveCopies.turnOff(folder);
            logger::info("Config spec '{}' is read from its installed spec.json", folder);
            readInstalled(folder);
            break;
        case ui::LiveCopyAction::Reload:
            if (_liveCopies.isOn(folder)) {
                readLiveCopy(folder);
            }
            break;
        case ui::LiveCopyAction::Overwrite:
            if (!_liveCopies.isOn(folder)) {
                break;
            }
            if (const auto copied = _liveCopies.overwrite(folder); !copied) {
                logger::warn("Config spec '{}': its live copy is not overwritten: {}", folder, copied.error());
                break;
            }
            logger::info("Config spec '{}': its live copy is overwritten with its installed spec.json", folder);
            readLiveCopy(folder);
            break;
        case ui::LiveCopyAction::Open:
            ShellExecuteW(nullptr, L"open", copy.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
            break;
        case ui::LiveCopyAction::Show: {
            const auto select = L"/select,\"" + copy.wstring() + L"\"";
            ShellExecuteW(nullptr, L"open", L"explorer.exe", select.c_str(), nullptr, SW_SHOWNORMAL);
            break;
        }
        default:
            break;
        }
    }

    /**
     * Reads a config spec from its live copy in place of the mod the menu has of it, which drops its unsaved
     * changes. A copy that cannot be read leaves the mod as it was read last, with the reason as its error. Either
     * way the panel is told whether the installed spec.json is newer than the copy.
     */
    void ConfigMenu::readLiveCopy(const std::string& folder)
    {
        _liveCopies.markRead(folder);
        auto mod = spec::loadConfigSpecFile(_liveCopies.copyOf(folder), _translations, spec::gamePluginChecks(), spec::gameFileFolders());
        if (!mod) {
            logger::warn("Config spec '{}': its live copy cannot be read: {}", folder, mod.error());
            _fullPanel.liveCopyNotRead(folder, shownLiveCopy(folder), timeOfDay(), mod.error());
        } else {
            mod->folder = folder;
            mod->liveCopy = shownLiveCopy(folder);
            spec::logConfigSpec(*mod);
            const auto dropped = _fullPanel.replaceMod(std::move(*mod), timeOfDay());
            logger::info("Config spec '{}' is read from its live copy, which dropped {} unsaved changes", folder, dropped);
        }
        const bool stale = _liveCopies.isStale(folder);
        if (stale) {
            logger::warn("Config spec '{}': its installed spec.json is newer than its live copy", folder);
        }
        _fullPanel.setLiveCopyStale(folder, stale);
    }

    /**
     * Reads a config spec from its installed spec.json again, in place of the mod the menu has of it, which drops its
     * unsaved changes. One that cannot be read is a mod that is not available, as it is when the game starts.
     */
    void ConfigMenu::readInstalled(const std::string& folder)
    {
        const auto file = std::filesystem::path(spec::CONFIG_SPEC_FOLDER) / std::u8string(folder.begin(), folder.end()) / "spec.json";
        auto mod = spec::loadConfigSpecFile(file, _translations, spec::gamePluginChecks(), spec::gameFileFolders());
        if (!mod) {
            mod = spec::notLoadedConfigSpec(folder, mod.error());
        }
        mod->folder = folder;
        spec::logConfigSpec(*mod);
        _fullPanel.replaceMod(std::move(*mod), timeOfDay());
    }

    /**
     * Reads the live copies that were saved since they were read, every little while: a look at each copy that is
     * read is a check of the time it was written.
     */
    void ConfigMenu::readChangedLiveCopies()
    {
        const auto now = std::chrono::steady_clock::now();
        if (now - _liveCopiesChecked < LIVE_COPIES_CHECK) {
            return;
        }
        _liveCopiesChecked = now;
        for (const auto& folder : _liveCopies.changed()) {
            logger::info("Config spec '{}': its live copy was saved", folder);
            readLiveCopy(folder);
        }
    }

    /**
     * The entries of the quick launch section of the menu's front page, by the names they show, as the mods are listed:
     * the entry each client added, with its texts translated, and the menu's own for an older FRIK. A press on a
     * client's entry runs its function, as one that opens the mod's own UI and minimizes the menu, and one whose
     * function does not run is disabled, with the reason.
     */
    std::vector<ui::FullPanel::QuickLaunchEntry> ConfigMenu::quickLaunchEntries()
    {
        std::vector<ui::FullPanel::QuickLaunchEntry> entries;
        if (auto frik = frikFallbackEntry()) {
            entries.push_back(std::move(*frik));
        }
        // the clients' entries, whose function opens the mod's own UI and minimizes the menu, or does a quick action
        const auto& calls = _configSpecStore.clientCalls();
        for (const auto& entry : calls.quickLaunch()) {
            auto name = _translations.translate(entry.text);
            auto notAvailable = calls.notRunnable(entry.modName, name, entry.function);
            entries.push_back({ .name = std::move(name),
                .help = _translations.translate(entry.help),
                .open =
                    [&calls, entry] {
                        auto result = calls.call(entry.modName, entry.function, {});
                        return ui::QuickLaunchResult{ .failed = !result.done, .message = std::move(result.message) };
                    },
                .buttonText = _translations.translate(entry.buttonText),
                .notAvailable = std::move(notAvailable) });
        }
        // as the mods are listed
        std::ranges::sort(entries, [](const auto& entry, const auto& other) {
            return common::str_tolower(entry.name) < common::str_tolower(other.name);
        });
        return entries;
    }

    /**
     * The menu's own quick launch entry for FRIK, while FRIK is installed and its client added none, as a FRIK of before
     * the native API: this menu took FRIK's opening gesture, so its config is opened from here. A press minimizes the
     * menu and opens FRIK's config in the frame update, and the menu does not come back by itself. None for a FRIK whose
     * client added its own.
     */
    std::optional<ui::FullPanel::QuickLaunchEntry> ConfigMenu::frikFallbackEntry()
    {
        if (!frik::api::FRIKApi::inst) {
            return std::nullopt;
        }
        const bool frikAddedEntry = std::ranges::any_of(_configSpecStore.clientCalls().quickLaunch(), [](const model::ClientQuickLaunch& entry) {
            return common::str_tolower(entry.modName) == common::str_tolower(FRIK_CLIENT_NAME);
        });
        if (frikAddedEntry) {
            return std::nullopt;
        }
        return ui::FullPanel::QuickLaunchEntry{ .name = "FRIK Configuration",
            .help = "Opens FRIK's own configuration in the world: body height, arm length, and the weapon and Pip-Boy positions.",
            .open =
                [this] {
                    _openFrikRequested = true;
                    return ui::QuickLaunchResult{};
                },
            .buttonText = {},
            .notAvailable = {} };
    }
}
