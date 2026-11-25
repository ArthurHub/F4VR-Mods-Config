#pragma once

#include <chrono>
#include <filesystem>
#include <functional>

#include "model/ConfigDefinition.h"
#include "model/Store.h"
#include "spec/ConfigSpecLoader.h"
#include "ui/Developer.h"
#include "ui/PageImages.h"
#include "vrcf/VRControllersHaptic.h"

namespace mods_config::menu
{
    /**
     * Where the menu controller reads and writes, and the checks of the plugins a config spec asks about: the game's
     * folders in the mod, and a test's own folders in a test.
     */
    struct MenuSetup
    {
        // MCM's folder of the MCM mods' configs and settings, `Data\MCM`
        std::filesystem::path mcm;

        // the roots a config spec's files are kept under: the game's Data folder, and `My Games\Fallout4VR`
        std::filesystem::path data;
        std::filesystem::path myGames;

        // the installed config specs' folders, `Data\ModsConfigFO4\Config`, and the folder of their live copies
        std::filesystem::path configSpecs;
        std::filesystem::path liveCopies;

        // the categories the player gave mods, which the mod's own config spec writes
        std::filesystem::path categories;

        spec::PluginChecks plugins;
    };

    /**
     * What the game does for the panel while it is in the world: draw its content, and the presses of Restore and Exit
     * on the wrist. Each is called while the game builds a frame or iterates its UI elements, so it only draws or asks.
     */
    struct PanelHandlers
    {
        std::function<void()> draw;
        std::function<void()> restorePressed;
        std::function<void()> exitPressed;
    };

    /**
     * The game side of the menu, which the menu controller asks of everything that is not game-free: the panel in the
     * world and the wrist buttons, the controllers and their haptics, Adjust in world, the textures of images, the
     * desktop's shell, FRIK, the writes of the mod's config, and the clock. The mod's adapter does it in the game, and a
     * test's records it.
     */
    class MenuPort
    {
    public:
        virtual ~MenuPort() = default;

        // the panel in front of the player, with the wrist buttons hidden, and the panel's look from the config
        virtual void openPanel(PanelHandlers handlers) = 0;

        // takes the panel and the wrist buttons out of the world, and gives back the textures of their images
        virtual void closePanel() = 0;

        // shows the panel again, in front of the player, or hides it
        virtual void setPanelShown(bool shown) = 0;

        virtual void setWristButtonsShown(bool shown) = 0;
        virtual void recenterPanel() = 0;

        // gives the panel in the world what the config has for its look, its size and place, and its move
        virtual void applyPanelLook() = 0;

        // a short press of Back in this frame, on the hand that points at the panel
        virtual bool backPressed() = 0;

        virtual bool isLeftHanded() = 0;
        virtual bool isGameMenuTakingInput() = 0;

        // plays a pattern on the hand that points at the panel
        virtual void playHaptic(const vrcf::HapticPattern& pattern) = 0;

        // Adjust in world, which tunes a transform of the shown mod in its store: started, and ended by Done when
        // a frame of it returns true, or by Cancel, which gives the value back, or stopped as the menu closes
        virtual void startAdjust(const model::Setting& setting, model::Store& store) = 0;
        virtual bool isAdjusting() const = 0;
        virtual bool adjustFrame() = 0;
        virtual void cancelAdjust() = 0;
        virtual void stopAdjust() = 0;

        // what the panel draws its images and icons with, and the textures of the pixels of mods that are replaced
        virtual ui::ImageDrawer imageDrawer() = 0;
        virtual void dropImageTextures() = 0;

        // opens a file in the desktop's editor for its type, or its folder in Explorer, with the file selected
        virtual void openFile(const std::filesystem::path& file) = 0;
        virtual void showInFolder(const std::filesystem::path& file) = 0;

        virtual bool isFrikInstalled() = 0;
        virtual bool isFrikConfigOpen() = 0;
        virtual void openFrikConfig() = 0;

        // writes what the front page changed to the mod's config, at once
        virtual void saveDeveloper(const ui::DeveloperView& view) = 0;
        virtual void saveHelpOpened() = 0;

        virtual std::chrono::system_clock::time_point now() = 0;
    };
}
