#pragma once

#include <memory>
#include <vector>

#include "imgui/UIImGuiPanel.h"
#include "model/Definition.h"
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
        bool isOpen() const;
        bool isMinimized() const;

        void open();
        void close();
        void setMinimized(bool minimized);
        void recenter();
        void onFrameUpdate();

        void setMods(std::vector<model::Mod> mods);

        const std::vector<model::Mod>& mods() const
        {
            return _mods;
        }

    private:
        void createPanel();
        void createWristButtons();
        void drawContent();
        void drawModList() const;
        void drawFrontPage();
        void drawQuickLaunch();

        std::shared_ptr<imgui::UIImGuiPanel> _panel;
        std::shared_ptr<vrui::UIContainer> _wristButtons;
        std::shared_ptr<vrui::UIButtonPanel> _minimizeButton;
        bool _closeRequested = false;
        bool _openFrikRequested = false;

        // the mods the menu lists, in order of their display names
        std::vector<model::Mod> _mods;
    };
}
