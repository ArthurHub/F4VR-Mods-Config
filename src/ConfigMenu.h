#pragma once

#include <memory>
#include <unordered_map>
#include <vector>

#include "imgui/UIImGuiPanel.h"
#include "model/Definition.h"
#include "render/Texture.h"
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
        bool isOpen() const;
        bool isMinimized() const;

        void open();
        void close();
        void setMinimized(bool minimized);
        void recenter();
        void onFrameUpdate();

        void setMods(std::vector<model::Mod> mods);
        const std::vector<model::Mod>& mods() const;

        void select(const ui::Selection& selection);
        const ui::Selection& selection() const;

    private:
        void createPanel();
        void applyPanelConfig() const;
        void createWristButtons();
        void drawImage(const model::ImagePixels& pixels, const ImVec2& min, const ImVec2& max);
        std::vector<ui::FullPanel::QuickLaunchEntry> quickLaunchEntries();

        std::shared_ptr<imgui::UIImGuiPanel> _panel;
        std::shared_ptr<vrui::UIContainer> _wristButtons;
        std::shared_ptr<vrui::UIButtonPanel> _minimizeButton;
        bool _closeRequested = false;
        bool _openFrikRequested = false;

        // what the panel shows
        ui::FullPanel _fullPanel;

        // The textures of the images that were drawn, each by the pixels of the mod it is made from. They are
        // on the GPU while the menu is open, and are made again from the pixels when it is opened again
        std::unordered_map<const model::ImagePixels*, std::shared_ptr<render::Texture>> _imageTextures;
    };
}
