#pragma once

#include <memory>

#include "imgui/UIImGuiPanel.h"
#include "vrui/UIButtonPanel.h"
#include "vrui/UIContainer.h"

namespace mods_config
{
    /**
     * The config menu: one ImGui panel in front of the player, and two buttons on the offhand that stay
     * while the panel is minimized.
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
         * Call after the UI manager's frame update: a close asked for by a button press is applied here,
         * once the manager is done iterating its elements.
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

        // values of the placeholder controls
        int _buttonPresses = 0;
        bool _checkbox = true;
        float _slider = 1.25f;
        int _comboOption = 0;
        int _listRow = 0;
    };
}
