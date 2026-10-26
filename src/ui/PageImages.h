#pragma once

#include <functional>
#include <optional>
#include <string>

#include <imgui.h>

#include "model/ConfigDefinition.h"

namespace mods_config::ui
{
    // The menu's callbacks for drawing images and reading texture sizes, without game access in the UI.
    struct ImageDrawer
    {
        // the size of a config spec's texture, or none when the game does not load it
        std::function<std::optional<ImVec2>(const std::string& texture)> textureSize;

        // why a config spec's texture is not loaded, as its file is not found, or empty when that is not known
        std::function<std::string(const std::string& texture)> textureError;

        // draws an image's pixels or texture in the given rectangle
        std::function<void(const model::Image& image, const ImVec2& min, const ImVec2& max)> draw;

        // draws an icon of the mod's own, by its path under `Data\Textures`, in the given rectangle, with its colors
        // multiplied by the tint, as a binding's icon is drawn in the color of its text
        std::function<void(const std::string& texture, const ImVec2& min, const ImVec2& max, ImU32 tint)> drawIcon;
    };

    void drawImage(const model::Image& image, const ImageDrawer& drawer, float width);
    bool drawTexture(const model::Image& image, const ImageDrawer& drawer, float width, std::string& reason);
}
