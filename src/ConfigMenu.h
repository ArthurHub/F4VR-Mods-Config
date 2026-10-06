#pragma once

#include <memory>

#include "imgui/UIImGuiPanel.h"
#include "vrui/UIButtonPanel.h"
#include "vrui/UIContainer.h"

namespace mods_config
{
    /**
     * The config menu: one ImGui panel in front of the player, and a row of buttons on the offhand that
     * stays while the panel is minimized.
     */
    class ConfigMenu
    {
    public:
        /**
         * The values of the placeholder controls.
         */
        struct ControlValues
        {
            int buttonPresses = 0;
            bool checkbox = true;
            float slider = 1.25f;
            int comboOption = 0;
            int listRow = 0;
        };

        bool isOpen() const;
        bool isMinimized() const;

        void open();
        void close();

        /**
         * Hide or show the panel. The offhand buttons stay either way.
         */
        void setMinimized(bool minimized);

        /**
         * Applies what a button press asked for: closing the menu, or minimizing it and opening FRIK's config.
         * It is done here, in the mod's frame update, and not in the press handler, which runs while the
         * UI manager iterates its elements.
         */
        void onFrameUpdate();

        const ControlValues& controls() const
        {
            return _controls;
        }

    private:
        void createPanel();
        void createWristButtons();
        void drawContent();

        std::shared_ptr<imgui::UIImGuiPanel> _panel;
        std::shared_ptr<vrui::UIContainer> _wristButtons;
        std::shared_ptr<vrui::UIButtonPanel> _minimizeButton;
        bool _closeRequested = false;
        bool _openFrikRequested = false;

        ControlValues _controls;
    };
}
