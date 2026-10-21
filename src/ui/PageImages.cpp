#include "PageImages.h"

#include <algorithm>
#include <cmath>

#include <imgui.h>

#include "Drawing.h"
#include "Theme.h"

namespace
{
    using namespace mods_config;

    /**
     * Resolves package and texture dimensions, preserving the aspect ratio for an unspecified side.
     * Missing textures use the available width and a control's height; wider images shrink to fit.
     */
    ImVec2 textureSize(const model::Image& image, const std::optional<ImVec2>& own, const float room)
    {
        ImVec2 size(image.width, image.height);
        if (!own) {
            size = ImVec2(size.x > 0.0f ? size.x : room, size.y > 0.0f ? size.y : theme::CONTROL_HEIGHT);
        } else if (size.x <= 0.0f && size.y <= 0.0f) {
            size = *own;
        } else if (size.x <= 0.0f) {
            size.x = size.y * own->x / own->y;
        } else if (size.y <= 0.0f) {
            size.y = size.x * own->y / own->x;
        }
        if (size.x > room) {
            size = ImVec2(room, size.y * room / size.x);
        }
        return ImVec2(std::round(size.x), std::round(size.y));
    }

}

namespace mods_config::ui
{
    /**
     * An image at the cursor, in the middle of a row's width. It is as large as in MCM's menu beside the rows'
     * text, and smaller where the row is not as wide. The row is as high as the image.
     */
    void drawImage(const model::Image& image, const ImageDrawer& drawer, const float width)
    {
        const auto pos = ImGui::GetCursorScreenPos();
        const float scale = (std::min)(mcmScale(), width / image.width);
        const ImVec2 size(std::round(image.width * scale), std::round(image.height * scale));
        ImGui::Dummy(ImVec2(width, size.y));
        if (drawer.draw && ImGui::IsItemVisible()) {
            const ImVec2 topLeft(pos.x + std::floor((width - size.x) * 0.5f), pos.y);
            drawer.draw(image, topLeft, ImVec2(topLeft.x + size.x, topLeft.y + size.y));
        }
    }

    /**
     * A package's texture at the cursor, in the middle of a row's width, with room above and below it. A
     * texture the game does not load is a box of its size with a dashed outline, that names it.
     */
    void drawTexture(const model::Image& image, const ImageDrawer& drawer, const float width)
    {
        const auto own = drawer.textureSize ? drawer.textureSize(image.texture) : std::nullopt;
        const auto pos = ImGui::GetCursorScreenPos();
        const auto size = textureSize(image, own, width);
        ImGui::Dummy(ImVec2(width, size.y + theme::SPACE_1 * 2.0f));
        if (!ImGui::IsItemVisible()) {
            return;
        }
        const ImVec2 topLeft(pos.x + std::floor((width - size.x) * 0.5f), pos.y + theme::SPACE_1);
        const ImVec2 bottomRight(topLeft.x + size.x, topLeft.y + size.y);
        if (own && drawer.draw) {
            drawer.draw(image, topLeft, bottomRight);
            return;
        }
        drawDashedOutline(topLeft, bottomRight, theme::color::FRAME_BORDER);
        ImGui::PushFont(nullptr, theme::FONT_SIZE_DETAIL);
        const auto text = "Missing: " + image.texture;
        const float left = (std::max)(topLeft.x + theme::SPACE_2, std::floor((topLeft.x + bottomRight.x - textWidth(text)) * 0.5f));
        drawText(left, bottomRight.x - theme::SPACE_2, topLeft.y, size.y, theme::color::GREEN_MUTED, text);
        ImGui::PopFont();
    }

}
