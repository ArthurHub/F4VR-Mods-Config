#include "Transforms.h"

#include <format>
#include <optional>
#include <string_view>
#include <vector>

#include "Theme.h"
#include "common/CommonUtils.h"
#include "model/Transforms.h"

namespace
{
    using namespace mods_config;

    // the room of `pos` and of `rot`, so the numbers line up, and the height of a line of the frame, in the panel's pixels
    constexpr float KEY_WIDTH = 30.0f;
    constexpr float LINE_HEIGHT = 18.0f;

    /**
     * A number of a transform as its row writes it: two decimals at most, and none past what it needs. One that rounds to
     * zero has no minus.
     */
    std::string shortNumber(const float value)
    {
        auto text = std::format("{:.2f}", value);
        text.erase(text.find_last_not_of('0') + 1);
        if (text.ends_with('.')) {
            text.pop_back();
        }
        return text == "-0" ? "0" : text;
    }

    /**
     * The three numbers of a part of a transform, as `9.37, -5.86, -5.13`, each with a unit after it.
     */
    std::string threeNumbers(const model::TransformTable& table, const std::size_t first, const std::string_view unit)
    {
        const auto number = [&](const std::size_t axis) {
            return shortNumber(model::axisValue(table, first + axis)) + std::string(unit);
        };
        return std::format("{}, {}, {}", number(0), number(1), number(2));
    }

    /**
     * A line of a transform's frame: the key of its part, with none for the scale alone, its numbers, and whether the
     * value leaves out the key of its part, which is drawn as what it means, in `green-muted`.
     */
    struct Line
    {
        std::string_view key;
        std::string numbers;
        bool leftOut = false;
    };

    /**
     * The lines of a transform's frame: the position and the rotation, of the parts that are on.
     */
    std::vector<Line> linesOf(const model::Setting& setting, const model::TransformTable& table)
    {
        const auto& axes = setting.valueOptions.transformAxes;
        std::vector<Line> lines;
        if (axes[model::TRANSFORM_POSITION].on) {
            lines.push_back({ .key = "pos", .numbers = threeNumbers(table, model::TRANSFORM_POSITION, ""), .leftOut = !table.pos });
        }
        if (axes[model::TRANSFORM_ROTATION].on) {
            lines.push_back({ .key = "rot", .numbers = threeNumbers(table, model::TRANSFORM_ROTATION, "°"), .leftOut = !table.rot });
        }
        return lines;
    }

    /**
     * The scale of a transform as its frame writes it, `×1.5`: none for a scale of 1, as a line of numbers has nothing to
     * say of it, and none for a scale that is off. With no line of numbers it is the whole of the frame, so it is always
     * written.
     */
    std::optional<std::string> scaleText(const model::Setting& setting, const model::TransformTable& table, const bool alone)
    {
        const float scale = model::axisValue(table, model::TRANSFORM_SCALE);
        if (!setting.valueOptions.transformAxes[model::TRANSFORM_SCALE].on || (!alone && common::fEqual(scale, 1.0f))) {
            return std::nullopt;
        }
        return std::format("×{}", shortNumber(scale));
    }
}

namespace mods_config::ui
{
    /**
     * A value of a transform setting as the help line names it, its default or its saved value: the parts that are on, as
     * `pos 9.37, -5.86, -5.13; rot 20.72°, 0°, 90°; ×1.5`. Empty for a value that is no transform.
     */
    std::string transformValueName(const model::Setting& setting, const model::Value& value)
    {
        const auto* text = std::get_if<std::string>(&value);
        const auto table = text ? model::parseTransformTable(*text, setting.tableForm) : std::unexpected(std::string());
        if (!table) {
            return {};
        }
        std::string name;
        const auto lines = linesOf(setting, *table);
        for (const auto& line : lines) {
            name += std::format("{}{} {}", name.empty() ? "" : "; ", line.key, line.numbers);
        }
        if (const auto scale = scaleText(setting, *table, lines.empty())) {
            name += std::format("{}{}", name.empty() ? "" : "; ", *scale);
        }
        return name;
    }

    /**
     * A transform that ends at the right, in a frame as wide as a dropdown's: its numbers, on a line for its position and
     * a line for its rotation, in the detail size and the color of its value's look, and the down arrow, as a press opens
     * its editor, with the outline in the full color while it is open. The scale follows the numbers of the first line. A
     * part that is off is left out. A value that is no transform is its text, as the file has it. One with no value has an
     * empty frame. Returns its left edge.
     */
    float drawTransform(const model::Setting& setting, const float right, const float top, const std::string* value, const ValueLook look, const bool open)
    {
        const auto box = controlBox(right - DROPDOWN_WIDTH, top, DROPDOWN_WIDTH);
        const float arrowLeft = box.Max.x - theme::SPACE_4 - ARROW_SIZE;
        const float textLeft = box.Min.x + theme::SPACE_4;
        const float textRight = arrowLeft - theme::SPACE_2;
        drawFrame(box, look);
        if (open) {
            drawOutline(box.Min, box.Max, theme::color::GREEN);
        }
        drawControlArrow(ImVec2(arrowLeft + ARROW_SIZE * 0.5f, box.GetCenter().y), ControlArrow::Down);
        if (!value) {
            return box.Min.x;
        }
        const auto table = model::parseTransformTable(*value, setting.tableForm);
        if (!table) {
            drawText(textLeft, textRight, top, theme::CONTROL_HEIGHT, valueColor(look), *value);
            return box.Min.x;
        }

        const auto lines = linesOf(setting, *table);
        const auto scale = scaleText(setting, *table, lines.empty());
        const float count = lines.empty() ? 1.0f : static_cast<float>(lines.size());
        const float linesTop = box.GetCenter().y - count * LINE_HEIGHT * 0.5f;
        ImGui::PushFont(nullptr, theme::FONT_SIZE_DETAIL);
        for (std::size_t i = 0; i < lines.size(); ++i) {
            const auto& line = lines[i];
            const float lineTop = linesTop + static_cast<float>(i) * LINE_HEIGHT;
            const float numbersLeft = textLeft + KEY_WIDTH;
            drawText(textLeft, numbersLeft, lineTop, LINE_HEIGHT, theme::color::GREEN_MUTED, line.key);
            drawText(numbersLeft, textRight, lineTop, LINE_HEIGHT, line.leftOut ? theme::color::GREEN_MUTED : valueColor(look), line.numbers);
            if (i == 0 && scale) {
                drawText(numbersLeft + textWidth(line.numbers) + theme::SPACE_3, textRight, lineTop, LINE_HEIGHT, valueColor(look), *scale);
            }
        }
        if (lines.empty() && scale) {
            drawText(textLeft, textRight, linesTop, LINE_HEIGHT, table->scale ? valueColor(look) : theme::color::GREEN_MUTED, *scale);
        }
        ImGui::PopFont();
        return box.Min.x;
    }
}
