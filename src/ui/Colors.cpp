#include "Colors.h"

#include <algorithm>

#include "Theme.h"

namespace
{
    using namespace mods_config;

    // the swatch of the row, and the squares of the checkerboard under a color's alpha
    constexpr float SWATCH_WIDTH = 64.0f;
    constexpr float SWATCH_HEIGHT = 32.0f;
    constexpr float CHECKER_SIZE = 8.0f;
}

namespace mods_config::ui
{
    /**
     * A color as ImGui draws it, at its alpha, or opaque.
     */
    ImU32 colorU32(const render::Color& color, const bool opaque)
    {
        return ImGui::ColorConvertFloat4ToU32(ImVec4(color.r, color.g, color.b, opaque ? 1.0f : color.a));
    }

    /**
     * A checkerboard of 8 px squares in a box, `rule` on `glass`, which a color's alpha is drawn over so its opacity
     * shows.
     */
    void drawCheckerboard(const ImRect& box)
    {
        auto* drawList = ImGui::GetWindowDrawList();
        drawList->AddRectFilled(box.Min, box.Max, ImGui::GetColorU32(theme::color::GLASS));
        int row = 0;
        for (float y = box.Min.y; y < box.Max.y; y += CHECKER_SIZE, ++row) {
            int column = 0;
            for (float x = box.Min.x; x < box.Max.x; x += CHECKER_SIZE, ++column) {
                if ((row + column) % 2 == 0) {
                    drawList->AddRectFilled(ImVec2(x, y),
                        ImVec2((std::min)(x + CHECKER_SIZE, box.Max.x), (std::min)(y + CHECKER_SIZE, box.Max.y)),
                        ImGui::GetColorU32(theme::color::RULE));
                }
            }
        }
    }

    /**
     * A color's swatch in a box, outlined in `frame-border`: its left half the color opaque, and its right half the color
     * at its alpha over a checkerboard, so its opacity shows. A color that is RGB only is opaque all over. A disabled row
     * fades it as the rest of the row.
     */
    void drawSwatch(const ImRect& box, const render::Color& color, const bool alpha)
    {
        auto* drawList = ImGui::GetWindowDrawList();
        const float middle = std::floor(box.GetCenter().x);
        drawList->AddRectFilled(box.Min, ImVec2(alpha ? middle : box.Max.x, box.Max.y), ImGui::GetColorU32(colorU32(color, true)));
        if (alpha) {
            const ImRect half(ImVec2(middle, box.Min.y), box.Max);
            drawCheckerboard(half);
            drawList->AddRectFilled(half.Min, half.Max, ImGui::GetColorU32(colorU32(color)));
        }
        drawList->AddRect(box.Min, box.Max, ImGui::GetColorU32(theme::color::FRAME_BORDER), theme::RADIUS_GRAB, 0, theme::BORDER_FRAME);
    }

    /**
     * A value of a color setting as the help line names it, its default or its saved value: its numbers, as
     * `190, 255, 190, alpha 200`. Empty for a value that is no color.
     */
    std::string colorValueName(const model::Setting& setting, const model::Value& value)
    {
        const auto color = model::colorOf(setting, value);
        return color ? model::colorNumbers(*color, setting.valueOptions.alpha) : std::string();
    }

    /**
     * A color that ends at the right, in a frame as wide as a dropdown's: its swatch, its numbers in the color of its
     * value's look, and the down arrow, as a press opens its editor, with the outline in the full color while it is open.
     * A value that is no color is its text, as the file has it, with no swatch, in the frame of an invalid value. One with
     * no value has an empty frame. Returns its left edge.
     */
    float drawColor(const model::Setting& setting, const float right, const float top, const std::string* value, const ValueLook look, const bool open)
    {
        const auto box = controlBox(right - DROPDOWN_WIDTH, top, DROPDOWN_WIDTH);
        const float arrowLeft = box.Max.x - theme::SPACE_4 - ARROW_SIZE;
        drawFrame(box, look);
        if (open) {
            drawOutline(box.Min, box.Max, theme::color::GREEN);
        }
        float textLeft = box.Min.x + theme::SPACE_4;
        std::string text;
        if (value) {
            const auto color = model::colorOf(setting, *value);
            text = color ? model::colorNumbers(*color, setting.valueOptions.alpha) : *value;
            if (color) {
                const float swatchLeft = box.Min.x + theme::SPACE_2;
                const float swatchTop = box.GetCenter().y - SWATCH_HEIGHT * 0.5f;
                drawSwatch(ImRect(swatchLeft, swatchTop, swatchLeft + SWATCH_WIDTH, swatchTop + SWATCH_HEIGHT), *color, setting.valueOptions.alpha);
                textLeft = swatchLeft + SWATCH_WIDTH + theme::SPACE_3;
            }
        }
        drawText(textLeft, arrowLeft - theme::SPACE_2, top, theme::CONTROL_HEIGHT, valueColor(look), text);
        drawControlArrow(ImVec2(arrowLeft + ARROW_SIZE * 0.5f, box.GetCenter().y), ControlArrow::Down);
        return box.Min.x;
    }
}
