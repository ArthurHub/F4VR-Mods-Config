#include "MenuController.h"

#include <chrono>
#include <ctime>
#include <filesystem>
#include <format>

#include "Config.h"
#include "imgui/UIImGuiPanel.h"
#include "perf/Perf.h"
#include "spec/ConfigSpecLoader.h"
#include "ui/MenuControls.h"

namespace
{
    // the id of the Config spec setting of the menu's own config spec, whose options the menu gives
    constexpr auto DEVELOPER_CONFIG_SPEC_ID = "developerConfigSpec";

    // the name FRIK's client of the native API registers under, whose quick launch entry replaces the menu's own
    constexpr auto FRIK_CLIENT_NAME = "FRIK";

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
}

namespace mods_config::menu
{
    /**
     * The menu, closed, with its stores in the setup's folders. A config spec is read from its live copy while its
     * author's report turns it on. A pending value a client hands the menu is set at once in the session of its mod,
     * while the menu shows it, but not while the dialog asks what to do with the unsaved changes.
     */
    MenuController::MenuController(MenuPort& port, MenuSetup setup)
        : _port(port),
          _setup(std::move(setup)),
          _liveCopies(_setup.configSpecs, _setup.liveCopies),
          _store(_setup.mcm),
          _configSpecStore(_setup.data, _setup.myGames)
    {
        _nativeClients.setTranslations(&_translations);
        _configSpecStore.setPageContentReading(&_translations, _setup.plugins);
        _nativeClients.setPendingValueAction([this](const std::string_view modName, const std::string_view id, const model::Value& value) -> std::expected<void, std::string> {
            if (_fullPanel.isAsking()) {
                return std::unexpected(std::string("the menu asks what to do with the unsaved changes"));
            }
            return _configSpecStore.setPendingValue(modName, id, value);
        });
        // the mod's own config spec has the Categories page, whose rows offer the names pending on it
        _fullPanel.modList().setOwnMod(std::string(Version::PROJECT));
        _fullPanel.setLiveCopyAction([this](const std::string& folder, const ui::LiveCopyAction action) {
            _liveCopyRequest = LiveCopyRequest{ .folder = folder, .action = action };
        });
    }

    /**
     * Adds the options of its own config spec's Config spec setting to the mod's client of the native API, as any
     * CommonFramework mod adds its own: none first, then each config spec the menu lists, by its modName and its name,
     * in their order, and the rows of its Categories page. The framework connects the client at kPostLoad. The client
     * keeps the handlers for the whole run, so only the controller that lives as long calls it, and only that one adds
     * the Categories rows' options in setMods.
     */
    void MenuController::registerOwnOptions()
    {
        _ownsClient = true;
        // a Save of the mod's own config spec can write the player's categories, which the tree is grouped by again
        // from the frame update: the Save is told while the panel is drawn, and the client hears only its own
        api::ModsConfigFO4Client::get().subscribeToSaves([this](const api::Saved&) {
            _categoriesSaved = true;
        });
        api::ModsConfigFO4Client::get().addOptions(DEVELOPER_CONFIG_SPEC_ID, [this] {
            std::vector<api::Option> options{ { .value = std::string(), .text = "None" } };
            for (const auto& mod : mods()) {
                if (mod.source == model::Source::ConfigSpec) {
                    options.push_back({ .value = mod.name, .text = mod.displayName });
                }
            }
            return options;
        });
        // the Categories page is a dynamic page, whose rows are the mods the menu lists as it is shown
        api::ModsConfigFO4Client::get().addPageContent(model::CATEGORIES_PAGE_ID, [this] {
            return std::optional<std::string>(_fullPanel.modList().categoryPageContent());
        });
    }

    bool MenuController::isOpen() const
    {
        return _open;
    }

    bool MenuController::isMinimized() const
    {
        return _open && _minimized;
    }

    bool MenuController::isAdjusting() const
    {
        return _port.isAdjusting();
    }

    /**
     * Opens the menu at its front page: the panel in front of the player, given its stores, its quick launch entries
     * and what its title bar and Adjust in world ask of the menu.
     */
    void MenuController::open()
    {
        if (isOpen()) {
            return;
        }
        logger::info("Open config menu");
        // a menu that was closed does not keep its place
        _fullPanel.reset();
        _port.openPanel({ .draw =
                              [this] {
                                  drawPanel();
                              },
            .restorePressed =
                [this] {
                    requestRestore();
                },
            .exitPressed =
                [this] {
                    _askToCloseRequested = true;
                } });
        _open = true;
        _minimized = false;
        applyPanelConfig();
        _fullPanel.setQuickLaunch(quickLaunchEntries());
        _fullPanel.setMenuActions({ .minimize =
                                        [this] {
                                            _minimizeRequested = true;
                                        },
            .close =
                [this] {
                    _closeRequested = true;
                },
            .adjustTransform =
                [this](const model::Setting& setting, model::Store& store) {
                    _adjustRequest = AdjustRequest{ .setting = &setting, .store = &store };
                } });
        _fullPanel.setStore(model::Source::Mcm, &_store);
        _fullPanel.setStore(model::Source::ConfigSpec, &_configSpecStore);
        _fullPanel.setImageDrawer(_port.imageDrawer());
        tellMenuEvent(model::ClientEvent::MenuOpened);
    }

    void MenuController::close()
    {
        _minimizeRequested = false;
        _closeRequested = false;
        _askToCloseRequested = false;
        _openFrikRequested = false;
        _adjustRequest.reset();
        if (!isOpen()) {
            return;
        }
        logger::info("Close config menu");
        // before the session it writes into ends, which discards its value
        _port.stopAdjust();
        _port.closePanel();
        _open = false;
        _minimized = false;
        // the mod that is shown is left, which its scripts and its client are told before they are told of the menu
        _fullPanel.reset();
        tellMenuEvent(model::ClientEvent::MenuClosed);
    }

    /**
     * Closes the menu as the player does, with the gesture or Exit on the wrist: while the mod that is shown
     * has unsaved changes the panel's dialog asks first, and its Save and Discard close the menu. A panel that
     * is minimized is shown again for it.
     */
    void MenuController::askToClose()
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
     * A panel that is shown again is put in front of the player, wherever it was left. The ray is on no row of a
     * minimized panel, except while Adjust in world tunes the row's transform.
     */
    void MenuController::setMinimized(const bool minimized)
    {
        if (!isOpen()) {
            return;
        }
        if (minimized && !isAdjusting()) {
            _fullPanel.point(nullptr);
        }
        _port.setPanelShown(!minimized);
        _port.setWristButtonsShown(minimized && !isAdjusting());
        _minimized = minimized;
    }

    /**
     * The mods the menu lists, the texts of the game's language they were read with, which a config spec is read
     * again with from its live copy, and the shipped table of their categories.
     */
    void MenuController::setMods(std::vector<model::Mod> mods, model::Translations translations, model::CategoryTable categories)
    {
        // the textures are kept by the pixels of the mods that are replaced
        _port.dropImageTextures();

        // the rows of the Categories page of the mod's own config spec, which its client gives, take their options
        // from its own client, which only the controller that owns the client adds them to
        if (_ownsClient) {
            for (const auto& mod : mods) {
                api::ModsConfigFO4Client::get().addOptions(model::CATEGORY_SETTING_ID + mod.name, [this] {
                    std::vector<api::Option> options;
                    for (const auto& option : _fullPanel.modList().categoryOptions(_configSpecStore)) {
                        options.push_back({ .value = std::get<std::string>(option.value), .text = option.text });
                    }
                    return options;
                });
            }
        }
        _fullPanel.modList().setCategoryTable(std::move(categories));
        _fullPanel.setMods(std::move(mods));
        readCategoryChoices();
        _translations = std::move(translations);
    }

    /**
     * Reads the categories the player gave mods from their file, beside the mod's config, and has the tree grouped by
     * them: at first, and after a Save wrote the file.
     */
    void MenuController::readCategoryChoices()
    {
        _fullPanel.modList().setCategoryChoices(model::loadCategoryChoices(_setup.categories));
    }

    const std::vector<model::Mod>& MenuController::mods() const
    {
        return _fullPanel.mods();
    }

    /**
     * Shows a mod's front page or one of its pages, as a press on its row in the tree does.
     */
    void MenuController::select(const ui::Selection& selection)
    {
        _fullPanel.select(selection);
    }

    const ui::Selection& MenuController::selection() const
    {
        return _fullPanel.selection();
    }

    /**
     * Put the panel in front of the player again, where they look now.
     * Nothing while the menu is minimized: Restore does it then.
     */
    void MenuController::recenter()
    {
        if (!isOpen() || isMinimized()) {
            return;
        }
        logger::info("Recenter config menu");
        _port.recenterPanel();
    }

    /**
     * Goes one step back in the panel, which first ends the drag of a slider, and closes the menu from the
     * panel's front page.
     * Nothing while the menu is minimized: its panel is not shown then.
     */
    void MenuController::back()
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
     * Back is seen here too, on the hand that points at the panel. Opening FRIK's config is done before a close asked
     * in the same frame, which is left for the next frame, and a close before a minimize.
     */
    void MenuController::onFrameUpdate()
    {
        F4CF_PERF_FUNCTION();
        updateStores();
        if (_developerToSave) {
            _port.saveDeveloper(*_developerToSave);
            _developerToSave.reset();
        }
        if (_helpOpened) {
            _helpOpened = false;
            _port.saveHelpOpened();
        }
        if (_liveCopyRequest) {
            const auto request = std::move(*_liveCopyRequest);
            _liveCopyRequest.reset();
            doLiveCopy(request.folder, request.action);
        }
        // a read replaces the mod that is tuned
        if (!isAdjusting()) {
            readChangedLiveCopies();
        }
        doClientRequests();
        doAdjust();
        if (isOpen() && g_config.panelChanged.exchange(false)) {
            applyPanelConfig();
            _port.applyPanelLook();
        }
        if (std::exchange(_categoriesSaved, false)) {
            readCategoryChoices();
        }

        if (isOpen() && _port.backPressed()) {
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
            if (!_port.isFrikConfigOpen()) {
                _port.openFrikConfig();
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
    bool MenuController::requestOpen(const std::string_view modName, const std::string_view pageId)
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
    void MenuController::requestMinimize()
    {
        _minimizeRequested = isOpen();
    }

    /**
     * Asks the menu to show itself again where it was, as Restore on the wrist and the opening gesture do, and a mod's
     * client whose UI closed: only while it is minimized, so a menu the player showed or closed is left as it is. Its
     * values are read again then, as the mod's own UI can have changed them.
     */
    void MenuController::requestRestore()
    {
        _restoreRequested = true;
    }

    /**
     * Asks the menu to close, for a mod's client: as the player's Exit does, which asks first while the mod that is
     * shown has unsaved changes, or at once with discardChanges, which drops them. Nothing while it is closed.
     */
    void MenuController::requestClose(const bool discardChanges)
    {
        if (!isOpen()) {
            return;
        }
        if (discardChanges) {
            _closeRequested = true;
        } else {
            _askToCloseRequested = true;
        }
    }

    /**
     * Asks the menu to read the values of a mod again while it is shown, for its client, which changed them.
     */
    void MenuController::requestReadAgain(const std::string_view modName)
    {
        _readAgainRequest = modName;
    }

    /**
     * Does what the mods' clients asked through the native API since the frame before: opens the menu at a mod and a
     * page, shows it again with the values of the mod that is shown read again, or reads them again alone.
     */
    void MenuController::doClientRequests()
    {
        F4CF_PERF_FUNCTION();
        // the panel is shown for them, and a selection can leave the mod that is tuned
        if (isAdjusting() && (_openRequest || _restoreRequested)) {
            _port.cancelAdjust();
            endAdjust();
            _restoreRequested = false;
        }
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
     * Starts the tuning a press on Adjust in world asked for, with the panel minimized, and tunes while it lasts. A game
     * menu that opens ends it as Cancel does, as the menu takes the controllers it is tuned with.
     */
    void MenuController::doAdjust()
    {
        F4CF_PERF_FUNCTION();
        if (_adjustRequest) {
            const auto request = *_adjustRequest;
            _adjustRequest.reset();
            if (isOpen() && !isAdjusting() && !_port.isGameMenuTakingInput()) {
                _port.startAdjust(*request.setting, *request.store);
                setMinimized(true);
            }
        }
        if (!isAdjusting()) {
            return;
        }
        if (_port.isGameMenuTakingInput()) {
            _port.cancelAdjust();
            endAdjust();
        } else if (_port.adjustFrame()) {
            endAdjust();
        }
    }

    /**
     * Shows the panel again after the tuning ended, with the editor of the transform open on the value it left.
     */
    void MenuController::endAdjust()
    {
        setMinimized(false);
        _fullPanel.resumeTransformEdit();
    }

    /**
     * Reads the values of the mod that is shown again, in its store, and the rows its client gives the page that is shown.
     */
    void MenuController::readSelectedAgain()
    {
        _fullPanel.readAgain();
    }

    /**
     * Tells every store and every mod's client that the menu opened or closed: the MCM mods' scripts with MCM's own
     * events, and the native clients with theirs. A menu that closes has left its mod before.
     */
    void MenuController::tellMenuEvent(const model::ClientEvent event)
    {
        if (event == model::ClientEvent::MenuOpened) {
            _store.menuOpened();
        } else {
            _store.menuClosed();
        }
        _configSpecStore.clientCalls().sendToAll(event);
    }

    /**
     * The stores' work of each frame, open menu or closed.
     */
    void MenuController::updateStores()
    {
        // the pause menu's events of a Save wait for MCM's file, also after the menu is closed
        _store.frameUpdate();
        // the last value of a preview its mod's client was not told yet, as previews are told at most every 50 ms
        _configSpecStore.frameUpdate();
    }

    /**
     * One frame of the panel's content, which the game draws: a press that did something gets its feedback on the
     * pointing hand, the pattern a haptic's press played or else the pulse.
     */
    void MenuController::drawPanel()
    {
        _fullPanel.draw();
        if (const auto played = _fullPanel.hapticPlayed()) {
            _port.playHaptic(*played);
        } else if (_fullPanel.pressActed()) {
            pulse();
        }
    }

    /**
     * Gives the Full panel what the config has for it: the controls its Help page tells, the player's handedness, which
     * a binding's buttons are printed by, whether Help stands out, which the Help page shown for the first time turns
     * off from the frame update, the Developer tools, whose filter and fold a press on the front page writes back from
     * the frame update, the width of its tree and the number of mods it groups from. Called when the menu opens and
     * when the config is loaded again while it is open, with the panel's look in the world.
     */
    void MenuController::applyPanelConfig()
    {
        _fullPanel.setDeveloper(g_config.developer, [this](const ui::DeveloperView& view) {
            _developerToSave = view;
        });
        _fullPanel.setHighlightHelp(g_config.highlightHelp, [this] {
            _helpOpened = true;
        });
        const bool leftHanded = _port.isLeftHanded();
        _fullPanel.setControls(ui::menuControls(
            { .openMenu = g_config.openMenuBinding, .back = g_config.backBinding, .move = g_config.moveBinding, .recenter = g_config.recenterBinding, .leftHanded = leftHanded }));
        _fullPanel.setLeftHanded(leftHanded);
        _fullPanel.setTreeWidth(g_config.treeWidth * imgui::CANVAS_PIXELS_PER_UNIT);
        _fullPanel.modList().setCategoriesMinMods(g_config.categoriesMinMods);
    }

    /**
     * Gives the hand that points the config's pulse, none when the config has none: in the frame its press does
     * something in the panel, and when its Back goes a step back or closes the menu.
     */
    void MenuController::pulse()
    {
        if (g_config.pressHaptic) {
            _port.playHaptic(*g_config.pressHaptic);
        }
    }

    /**
     * The time of day now, as a read of a live copy says when it was: `10:42:05`.
     */
    std::string MenuController::timeOfDay()
    {
        const auto now = std::chrono::system_clock::to_time_t(_port.now());
        std::tm local{};
        localtime_s(&local, &now);
        return std::format("{:02}:{:02}:{:02}", local.tm_hour, local.tm_min, local.tm_sec);
    }

    /**
     * Does what a row of a config spec's live copy in its report asks: on copies its spec.json where there is no
     * copy, and reads the copy, off reads its installed spec.json again, Reload reads the copy again, Overwrite copies
     * the installed spec.json over the copy and reads it, Open opens it in the desktop's editor for `.json`, and Show
     * opens its folder in Explorer, with the copy selected. A copy that cannot be made leaves the switch off, and one
     * that cannot be overwritten is as it was, with the reason in the log.
     */
    void MenuController::doLiveCopy(const std::string& folder, const ui::LiveCopyAction action)
    {
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
            _port.openFile(_liveCopies.copyOf(folder));
            break;
        case ui::LiveCopyAction::Show:
            _port.showInFolder(_liveCopies.copyOf(folder));
            break;
        default:
            break;
        }
    }

    /**
     * Reads a config spec from its live copy in place of the mod the menu has of it, which drops its unsaved
     * changes. A copy that cannot be read leaves the mod as it was read last, with the reason as its error. Either
     * way the panel is told whether the installed spec.json is newer than the copy.
     */
    void MenuController::readLiveCopy(const std::string& folder)
    {
        _liveCopies.markRead(folder);
        auto mod = spec::loadConfigSpecFile(_liveCopies.copyOf(folder), _translations, _setup.plugins, { .data = _setup.data, .myGames = _setup.myGames });
        if (!mod) {
            logger::warn("Config spec '{}': its live copy cannot be read: {}", folder, mod.error());
            _fullPanel.modList().liveCopyNotRead(folder, shownLiveCopy(folder), timeOfDay(), mod.error());
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
        _fullPanel.modList().setLiveCopyStale(folder, stale);
    }

    /**
     * Reads a config spec from its installed spec.json again, in place of the mod the menu has of it, which drops its
     * unsaved changes. One that cannot be read is a mod that is not available, as it is when the game starts.
     */
    void MenuController::readInstalled(const std::string& folder)
    {
        const auto file = _setup.configSpecs / std::u8string(folder.begin(), folder.end()) / "spec.json";
        auto mod = spec::loadConfigSpecFile(file, _translations, _setup.plugins, { .data = _setup.data, .myGames = _setup.myGames });
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
    void MenuController::readChangedLiveCopies()
    {
        F4CF_PERF_FUNCTION();
        const auto now = _port.now();
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
    std::vector<ui::FullPanel::QuickLaunchEntry> MenuController::quickLaunchEntries()
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
    std::optional<ui::FullPanel::QuickLaunchEntry> MenuController::frikFallbackEntry()
    {
        if (!_port.isFrikInstalled()) {
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
