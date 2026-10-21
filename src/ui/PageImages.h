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

        // draws an image's pixels or texture in the given rectangle
        std::function<void(const model::Image& image, const ImVec2& min, const ImVec2& max)> draw;
    };

    void drawImage(const model::Image& image, const ImageDrawer& drawer, float width);
    void drawTexture(const model::Image& image, const ImageDrawer& drawer, float width);
}
