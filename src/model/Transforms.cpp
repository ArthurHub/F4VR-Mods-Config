#include "Transforms.h"

#include <cmath>
#include <format>

#include "ConfigDefinition.h"
#include "common/TomlEditor.h"
#include "config/ValueParsers.h"

namespace
{
    // a changed number is rounded to this many decimals' place, which is past any step, so a slider's step in a float
    // is written as -5.86 and not as -5.8600003
    constexpr double CHANGED_NUMBER_PLACE = 100000.0;

    /**
     * A number as the framework writes it: the shortest text that reads back as its float, with `.0` on a whole number,
     * so it stays a float.
     */
    std::string numberText(const float value)
    {
        auto text = std::format("{}", value);
        if (std::isfinite(value) && text.find_first_of(".eE") == std::string::npos) {
            text += ".0";
        }
        return text;
    }

    /**
     * The text of a key of three numbers: `pos = [0.0, 1.0, 0.0]`.
     */
    std::string threeText(const std::string_view key, const std::array<float, 3>& numbers)
    {
        return std::format("{} = [{}, {}, {}]", key, numberText(numbers[0]), numberText(numbers[1]), numberText(numbers[2]));
    }
}

namespace mods_config::model
{
    /**
     * The transform table of the TOML text of an inline table, as the framework reads it from its config, or the
     * framework's reason it is none, as `rot is not an array of three numbers`. A text that is no TOML value is none too.
     */
    std::expected<TransformTable, std::string> parseTransformTable(const std::string_view text)
    {
        std::string reason = "it is not a table, as { pos = [0, 0, 0], rot = [0, 0, 0], scale = 1 }";
        if (const auto document = common::parseToml(std::format("transform = {}", text)); document && document->size() == 1) {
            const auto& node = *document->get("transform");
            const auto read = config::parseTransform(node);
            if (read) {
                // each key the table has
                TransformTable table;
                const auto& keys = *node.as_table();
                if (keys.contains("pos")) {
                    table.pos = std::array{ read->x, read->y, read->z };
                }
                if (keys.contains("rot")) {
                    table.rot = std::array{ read->heading, read->roll, read->attitude };
                }
                if (keys.contains("scale")) {
                    table.scale = read->scale;
                }
                return table;
            }
            reason = read.error();
        }
        return std::unexpected(std::move(reason));
    }

    /**
     * A transform table as the TOML text of its inline table, with the keys it has, pos, rot and scale:
     * `{ pos = [0.0, 1.0, 0.0], scale = 1.5 }`. A table with no key is `{}`.
     */
    std::string transformText(const TransformTable& table)
    {
        std::string text;
        if (table.pos) {
            text += threeText("pos", *table.pos);
        }
        if (table.rot) {
            text += std::format("{}{}", text.empty() ? "" : ", ", threeText("rot", *table.rot));
        }
        if (table.scale) {
            text += std::format("{}scale = {}", text.empty() ? "" : ", ", numberText(*table.scale));
        }
        return text.empty() ? "{}" : std::format("{{ {} }}", text);
    }

    /**
     * The text of a transform as the menu writes it, so two texts of one transform are the same text: a file can write a
     * key's place, a whole number and its spaces in another way. A text that is no transform is as it is.
     */
    std::string canonicalTransform(const std::string& text)
    {
        const auto table = parseTransformTable(text);
        return table ? transformText(*table) : text;
    }

    /**
     * The number of an axis of a transform, as the framework reads it: a key the table leaves out is no move, no rotation
     * and a scale of 1.
     */
    float axisValue(const TransformTable& table, const std::size_t axis)
    {
        if (axis == TRANSFORM_SCALE) {
            return table.scale.value_or(1.0f);
        }
        const auto& part = axis < TRANSFORM_ROTATION ? table.pos : table.rot;
        return part ? (*part)[axis % 3] : 0.0f;
    }

    /**
     * Sets the number of an axis of a transform, which adds the key of its part to a table that leaves it out, with its
     * other axes at what the key's absence meant.
     */
    void setAxis(TransformTable& table, const std::size_t axis, const float value)
    {
        const auto number = static_cast<float>(std::round(static_cast<double>(value) * CHANGED_NUMBER_PLACE) / CHANGED_NUMBER_PLACE);
        if (axis == TRANSFORM_SCALE) {
            table.scale = number;
            return;
        }
        auto& part = axis < TRANSFORM_ROTATION ? table.pos : table.rot;
        if (!part) {
            part = std::array{ 0.0f, 0.0f, 0.0f };
        }
        (*part)[axis % 3] = number;
    }
}
