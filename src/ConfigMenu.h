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
        bool isOpen() const;
        bool isMinimized() const;

        void open();
        void close();

        /**
         * Hide or show the panel. The offhand buttons stay either way.
         */
        void setMinimized(bool minimized);

        /**
         * Call after the UI manager's frame update: what a button press asked for, closing the menu or
         * minimizing it and opening FRIK's config, is applied here, once the manager is done iterating its elements.
         */
        void onFrameUpdate();

    private:
        void createPanel();
        void createWristButtons();
        void drawContent();

        std::shared_ptr<imgui::UIImGuiPanel> _panel;
        std::shared_ptr<vrui::UIContainer> _wristButtons;
        std::shared_ptr<vrui::UIButtonPanel> _minimizeButton;
        bool _closeRequested = false;
        bool _openFrikRequested = false;

        // values of the placeholder controls
        int _buttonPresses = 0;
        bool _checkbox = true;
        float _slider = 1.25f;
        int _comboOption = 0;
        int _listRow = 0;
    };
}
