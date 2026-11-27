#pragma once

#include <memory>
#include <string>
#include <unordered_map>

#include "AdjustInWorld.h"
#include "imgui/UIImGuiPanel.h"
#include "menu/MenuPort.h"
#include "render/Texture.h"
#include "vrui/UIContainer.h"

namespace mods_config
{
    /**
     * The game side of the config menu, which its controller asks through its port: one ImGui panel that is put in
     * front of the player and stays in the world, where the player can move it, a row of buttons on the offhand that
     * shows while the panel is minimized, Adjust in world, the textures of images, the controllers, the shell and FRIK.
     */
    class GameMenu final : public menu::MenuPort
    {
    public:
        void openPanel(menu::PanelHandlers handlers) override;
        void closePanel() override;
        void setPanelShown(bool shown) override;
        void setWristButtonsShown(bool shown) override;
        void recenterPanel() override;
        void applyPanelLook() override;
        bool backPressed() override;
        bool isLeftHanded() override;
        bool isGameMenuTakingInput() override;
        void playHaptic(const vrcf::HapticPattern& pattern) override;
        void startAdjust(const model::Setting& setting, model::Store& store) override;
        bool isAdjusting() const override;
        bool adjustFrame() override;
        void cancelAdjust() override;
        void stopAdjust() override;
        ui::ImageDrawer imageDrawer() override;
        void dropImageTextures() override;
        void openFile(const std::filesystem::path& file) override;
        void showInFolder(const std::filesystem::path& file) override;
        bool isFrikInstalled() override;
        bool isFrikConfigOpen() override;
        void openFrikConfig() override;
        void saveDeveloper(const ui::DeveloperView& view) override;
        void saveHelpOpened() override;
        std::chrono::system_clock::time_point now() override;

    private:
        void createWristButtons(std::function<void()> restorePressed, std::function<void()> exitPressed);
        static vrcf::Hand pointingHand();
        void drawImage(const model::Image& image, const ImVec2& min, const ImVec2& max);
        std::optional<ImVec2> textureSize(const std::string& texture);
        render::Texture& textureOf(const std::string& texture);

        std::shared_ptr<imgui::UIImGuiPanel> _panel;
        std::shared_ptr<vrui::UIContainer> _wristButtons;

        // the tuning of a transform in the world, with the panel minimized
        AdjustInWorld _adjust;

        // The textures of the images that were drawn, each by the pixels of the mod it is made from. They are
        // on the GPU while the menu is open, and are made again from the pixels when it is opened again
        std::unordered_map<const model::ImagePixels*, std::shared_ptr<render::Texture>> _imageTextures;

        // the textures of config specs that were drawn, by their paths under `Data/Textures`, which the game loads
        // as they are first drawn. They are given back to the game as the menu closes
        std::unordered_map<std::string, std::shared_ptr<render::Texture>> _configSpecTextures;
    };
}
