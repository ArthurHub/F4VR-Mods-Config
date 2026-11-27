#pragma once

#include "menu/MenuPort.h"

namespace mods_config
{
    /**
     * The game side of the menu on flat Fallout 4, as GameMenu is in VR: the panel on the screen. It has no panel yet,
     * so the menu does not show; what VR alone has (the wrist buttons, haptics, adjusting in the world, FRIK) does
     * nothing here.
     */
    class FlatGameMenu final : public menu::MenuPort
    {
    public:
        void openPanel(menu::PanelHandlers) override
        {}

        void closePanel() override
        {}

        void setPanelShown(bool) override
        {}

        void setWristButtonsShown(bool) override
        {}

        void recenterPanel() override
        {}

        void applyPanelLook() override
        {}

        bool backPressed() override
        {
            return false;
        }

        bool isLeftHanded() override
        {
            return false;
        }

        bool isGameMenuTakingInput() override
        {
            return false;
        }

        void playHaptic(const vrcf::HapticPattern&) override
        {}

        void startAdjust(const model::Setting&, model::Store&) override
        {}

        bool isAdjusting() const override
        {
            return false;
        }

        bool adjustFrame() override
        {
            return false;
        }

        void cancelAdjust() override
        {}

        void stopAdjust() override
        {}

        ui::ImageDrawer imageDrawer() override
        {
            return {};
        }

        void dropImageTextures() override
        {}

        void openFile(const std::filesystem::path&) override
        {}

        void showInFolder(const std::filesystem::path&) override
        {}

        bool isFrikInstalled() override
        {
            return false;
        }

        bool isFrikConfigOpen() override
        {
            return false;
        }

        void openFrikConfig() override
        {}

        void saveDeveloper(const ui::DeveloperView&) override
        {}

        void saveHelpOpened() override
        {}

        std::chrono::system_clock::time_point now() override
        {
            return std::chrono::system_clock::now();
        }
    };
}
