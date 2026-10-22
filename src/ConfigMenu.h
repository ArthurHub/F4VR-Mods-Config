#pragma once

#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "api/NativeClients.h"
#include "common/CommonUtils.h"
#include "imgui/UIImGuiPanel.h"
#include "mcm/McmStore.h"
#include "model/ConfigDefinition.h"
#include "model/Translations.h"
#include "render/Texture.h"
#include "spec/ConfigSpecStore.h"
#include "spec/LiveCopies.h"
#include "ui/FullPanel.h"
#include "vrui/UIButtonPanel.h"
#include "vrui/UIContainer.h"

namespace mods_config
{
    /**
     * The config menu: one ImGui panel that is put in front of the player and stays in the world, where the
     * player can move it, and a row of buttons on the offhand that stays while the panel is minimized.
     */
    class ConfigMenu
    {
    public:
        ConfigMenu();

        bool isOpen() const;
        bool isMinimized() const;

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

        void setMods(std::vector<model::Mod> mods, model::Translations translations);
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
        void createPanel();
        void applyPanelConfig();
        void createWristButtons();
        static void pulse();
        static float getThumbstickSideValue();
        void drawImage(const model::Image& image, const ImVec2& min, const ImVec2& max);
        std::optional<ImVec2> textureSize(const std::string& texture);
        render::Texture& textureOf(const std::string& texture);
        std::vector<ui::FullPanel::QuickLaunchEntry> quickLaunchEntries();
        void doLiveCopy(const std::string& folder, ui::LiveCopyAction action);
        void readLiveCopy(const std::string& folder);
        void readInstalled(const std::string& folder);
        void readChangedLiveCopies();
        void doClientRequests();
        void readSelectedAgain();

        std::shared_ptr<imgui::UIImGuiPanel> _panel;
        std::shared_ptr<vrui::UIContainer> _wristButtons;
        std::shared_ptr<vrui::UIButtonPanel> _minimizeButton;
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

        // the texts of the game's language, which a config spec is read again with
        model::Translations _translations;

        // the live copies of the config specs' spec.json, and when the frame update last looked for a copy that was saved
        spec::LiveCopies _liveCopies;
        std::chrono::steady_clock::time_point _liveCopiesChecked;

        // the values of the selected mod, which the panel asks for and sets: an MCM mod's, in the game
        mcm::McmStore _store{ R"(Data\MCM)" };

        // and a config spec's mod's, in its files, from the game's Data folder and the game's folder of My Games
        spec::ConfigSpecStore _configSpecStore{ "Data", common::getRelativePathInDocuments(R"(\My Games\Fallout4VR)") };

        // the native clients, which the config spec store asks
        api::NativeClients _nativeClients{ _configSpecStore.clientCalls() };

        // what the panel shows
        ui::FullPanel _fullPanel;

        // The textures of the images that were drawn, each by the pixels of the mod it is made from. They are
        // on the GPU while the menu is open, and are made again from the pixels when it is opened again
        std::unordered_map<const model::ImagePixels*, std::shared_ptr<render::Texture>> _imageTextures;

        // the textures of config specs that were drawn, by their paths under `Data/Textures`, which the game loads
        // as they are first drawn. They are given back to the game as the menu closes
        std::unordered_map<std::string, std::shared_ptr<render::Texture>> _configSpecTextures;
    };
}
