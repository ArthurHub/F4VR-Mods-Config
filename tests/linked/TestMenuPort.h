#pragma once

#include <chrono>
#include <filesystem>
#include <string>
#include <vector>

#include "menu/MenuPort.h"

// A double of the menu's game side for the menu controller's tests, which records what the controller asks of it.
namespace test_menu_port
{
    using namespace mods_config;
    using Calls = std::vector<std::string>;

    /**
     * A game side that records each call that does something in the game as a line of text, in order, and answers each
     * question with a field the test sets. It keeps the handlers of the panel it was asked to open, so a test can press
     * Restore and Exit on the wrist. Adjust in world is active from its start until it is cancelled or stopped, or the
     * test sets it.
     */
    class RecordingMenuPort final : public menu::MenuPort
    {
    public:
        Calls calls;

        // the answers to the controller's questions
        bool backIsPressed = false;
        float thumbstick = 0.0f;
        bool leftHanded = false;
        bool gameMenuTakingInput = false;
        bool adjusting = false;
        bool adjustEnds = false;
        bool frikInstalled = false;
        bool frikConfigOpen = false;
        std::chrono::system_clock::time_point time = std::chrono::system_clock::now();

        menu::PanelHandlers handlers;

        void openPanel(menu::PanelHandlers panelHandlers) override
        {
            handlers = std::move(panelHandlers);
            calls.emplace_back("openPanel");
        }

        void closePanel() override
        {
            handlers = {};
            calls.emplace_back("closePanel");
        }

        void setPanelShown(const bool shown) override
        {
            calls.push_back(shown ? "showPanel" : "hidePanel");
        }

        void setWristButtonsShown(const bool shown) override
        {
            calls.push_back(shown ? "showWristButtons" : "hideWristButtons");
        }

        void recenterPanel() override
        {
            calls.emplace_back("recenterPanel");
        }

        void applyPanelLook() override
        {
            calls.emplace_back("applyPanelLook");
        }

        bool backPressed() override
        {
            return std::exchange(backIsPressed, false);
        }

        float thumbstickSideValue() override
        {
            return thumbstick;
        }

        bool isLeftHanded() override
        {
            return leftHanded;
        }

        bool isGameMenuTakingInput() override
        {
            return gameMenuTakingInput;
        }

        void playHaptic(const vrcf::HapticPattern&) override
        {
            calls.emplace_back("playHaptic");
        }

        void startAdjust(const model::Setting& setting, model::Store&) override
        {
            adjusting = true;
            calls.push_back("startAdjust " + setting.id);
        }

        bool isAdjusting() const override
        {
            return adjusting;
        }

        bool adjustFrame() override
        {
            if (adjustEnds) {
                adjusting = false;
                calls.emplace_back("adjustDone");
            }
            return adjustEnds;
        }

        void cancelAdjust() override
        {
            adjusting = false;
            calls.emplace_back("cancelAdjust");
        }

        void stopAdjust() override
        {
            if (adjusting) {
                adjusting = false;
                calls.emplace_back("stopAdjust");
            }
        }

        ui::ImageDrawer imageDrawer() override
        {
            return {};
        }

        void dropImageTextures() override
        {
            calls.emplace_back("dropImageTextures");
        }

        void openFile(const std::filesystem::path& file) override
        {
            calls.push_back("openFile " + file.string());
        }

        void showInFolder(const std::filesystem::path& file) override
        {
            calls.push_back("showInFolder " + file.string());
        }

        bool isFrikInstalled() override
        {
            return frikInstalled;
        }

        bool isFrikConfigOpen() override
        {
            return frikConfigOpen;
        }

        void openFrikConfig() override
        {
            calls.emplace_back("openFrikConfig");
        }

        void saveDeveloper(const ui::DeveloperView& view) override
        {
            calls.push_back(std::string("saveDeveloper ") + (view.folded ? "folded" : "open"));
        }

        void saveHelpOpened() override
        {
            calls.emplace_back("saveHelpOpened");
        }

        std::chrono::system_clock::time_point now() override
        {
            return time;
        }

        /**
         * The calls recorded since the last take, which are cleared.
         */
        Calls take()
        {
            return std::exchange(calls, {});
        }
    };
}
