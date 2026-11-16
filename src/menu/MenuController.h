#pragma once

#include <chrono>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "api/NativeClients.h"
#include "mcm/McmStore.h"
#include "menu/MenuPort.h"
#include "model/Categories.h"
#include "model/ConfigDefinition.h"
#include "model/Translations.h"
#include "spec/ConfigSpecStore.h"
#include "spec/LiveCopies.h"
#include "ui/FullPanel.h"

namespace mods_config::menu
{
    /**
     * The config menu's decisions, with no game: what a press, a native client's request and a live copy's save do, in
     * the frame update, and in which order. It owns the stores and the Full panel, and asks the game side of everything
     * else through its port.
     */
    class MenuController
    {
    public:
        MenuController(MenuPort& port, MenuSetup setup);

        bool isOpen() const;
        bool isMinimized() const;
        bool isAdjusting() const;

        void open();
        void close();
        void askToClose();
        void setMinimized(bool minimized);
        void recenter();
        void back();
        void onFrameUpdate();

        void registerOwnOptions();

        // what a mod's client asks of the menu through the native API, which the frame update does
        bool requestOpen(std::string_view modName, std::string_view pageId);
        void requestMinimize();
        void requestRestore();
        void requestReadAgain(std::string_view modName);

        void setMods(std::vector<model::Mod> mods, model::Translations translations, model::CategoryTable categories);
        void readCategoryChoices();
        const std::vector<model::Mod>& mods() const;

        void select(const ui::Selection& selection);
        const ui::Selection& selection() const;

        // what the panel shows, for the devbench tool, which reads and drives it as the player's presses do
        ui::FullPanel& fullPanel()
        {
            return _fullPanel;
        }

        // the mods' native clients, which the native API registers
        api::NativeClients& nativeClients()
        {
            return _nativeClients;
        }

    private:
        void drawPanel();
        void applyPanelConfig();
        void pulse();
        std::vector<ui::FullPanel::QuickLaunchEntry> quickLaunchEntries();
        std::optional<ui::FullPanel::QuickLaunchEntry> frikFallbackEntry();
        std::string timeOfDay();
        void doLiveCopy(const std::string& folder, ui::LiveCopyAction action);
        void readLiveCopy(const std::string& folder);
        void readInstalled(const std::string& folder);
        void readChangedLiveCopies();
        void doClientRequests();
        void readSelectedAgain();
        void tellMenuEvent(model::ClientEvent event);
        void updateStores();
        void doAdjust();
        void endAdjust();

        MenuPort& _port;
        MenuSetup _setup;

        bool _open = false;
        bool _minimized = false;

        bool _minimizeRequested = false;
        bool _closeRequested = false;
        bool _askToCloseRequested = false;
        bool _openFrikRequested = false;

        // what clients asked through the native API: the selection to open the menu at, to show the menu again, and the
        // mod whose values to read again while it is shown
        std::optional<ui::Selection> _openRequest;
        bool _restoreRequested = false;
        std::string _readAgainRequest;

        // the Developer tools as a press on the front page changed them, which the frame update writes to the config
        std::optional<ui::DeveloperView> _developerToSave;

        // the Help page was shown for the first time, which the frame update writes to the config
        bool _helpOpened = false;

        /**
         * What a press on a row of a config spec's live copy in its report asked, by the config spec's folder, which
         * the frame update does.
         */
        struct LiveCopyRequest
        {
            std::string folder;
            ui::LiveCopyAction action = ui::LiveCopyAction::None;
        };

        std::optional<LiveCopyRequest> _liveCopyRequest;

        /**
         * The transform a press on Adjust in world tunes, which the frame update starts.
         */
        struct AdjustRequest
        {
            const model::Setting* setting = nullptr;
            model::Store* store = nullptr;
        };

        std::optional<AdjustRequest> _adjustRequest;

        // the texts of the game's language, which a config spec is read again with
        model::Translations _translations;

        // the live copies of the config specs' spec.json, and when the frame update last looked for a copy that was saved
        spec::LiveCopies _liveCopies;
        std::chrono::system_clock::time_point _liveCopiesChecked;

        // the values of the selected mod, which the panel asks for and sets: an MCM mod's, in the game
        mcm::McmStore _store;

        // and a config spec's mod's, in its files, from the Data folder and the folder of My Games
        spec::ConfigSpecStore _configSpecStore;

        // the native clients, which the config spec store asks
        api::NativeClients _nativeClients{ _configSpecStore.clientCalls() };

        // what the panel shows
        ui::FullPanel _fullPanel;

        // a Save of the mod's own config spec, which can write the player's categories the frame update reads again
        bool _categoriesSaved = false;

        // this controller put its handlers on the mod's own client, which keeps them for the whole run: only it adds the
        // options of the Categories rows there too, so a controller that lives shorter leaves nothing that points at it
        bool _ownsClient = false;
    };
}
