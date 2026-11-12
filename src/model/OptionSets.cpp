#include "OptionSets.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <optional>
#include <ranges>
#include <string>
#include <variant>

#include "Colors.h"
#include "Transforms.h"
#include "common/CommonUtils.h"

namespace
{
    using namespace mods_config::model;

    /**
     * A value as a number: an int or a float. None for a bool and a text.
     */
    std::optional<float> numberOf(const Value& value)
    {
        if (const auto* whole = std::get_if<int>(&value)) {
            return static_cast<float>(*whole);
        }
        const auto* number = std::get_if<float>(&value);
        return number ? std::optional(*number) : std::nullopt;
    }

    /**
     * Whether a part of a table that is set is the part a setting has: one that is not set is any.
     */
    template <std::size_t N>
    bool hasPart(const std::optional<std::array<float, N>>& set, const std::optional<std::array<float, N>>& current)
    {
        if (!set) {
            return true;
        }
        const auto have = current.value_or(std::array<float, N>{});
        return std::ranges::all_of(std::views::iota(std::size_t{ 0 }, N), [&set, &have](const std::size_t i) {
            return common::fEqual((*set)[i], have[i]);
        });
    }

    /**
     * The text of a value as a setting of a transform or a color has it, and none for any other.
     */
    const std::string* tableText(const Setting& setting, const Value& value)
    {
        const bool isTable = setting.type == SettingType::Transform || setting.type == SettingType::Color;
        return isTable ? std::get_if<std::string>(&value) : nullptr;
    }
}

namespace mods_config::model
{
    /**
     * Whether a setting is a choice whose options set other settings, which has no value of its own.
     */
    bool setsOthers(const Setting& setting)
    {
        return std::holds_alternative<SetsStorage>(setting.storage);
    }

    /**
     * Whether a setting has the value an option sets on it: two numbers that are as near as a number and the text of it
     * in a file, and the parts of a transform or a color the option sets, in the setting's form of a table. A part the
     * setting's table leaves out is the framework's zero. A text that is no table has none of them.
     */
    bool hasSetValue(const Setting& target, const Value& current, const Value& set)
    {
        const auto* setText = tableText(target, set);
        const auto* currentText = tableText(target, current);
        if (setText && currentText && target.type == SettingType::Transform) {
            const auto wanted = parseTransformTable(*setText, TableForm::Toml);
            const auto have = parseTransformTable(*currentText, target.tableForm);
            return wanted && have && hasPart(wanted->pos, have->pos) && hasPart(wanted->rot, have->rot) &&
                   (!wanted->scale || common::fEqual(*wanted->scale, have->scale.value_or(1.0f)));
        }
        if (setText && currentText) {
            const auto wanted = parseColorTable(*setText, TableForm::Toml);
            const auto have = parseColorTable(*currentText, target.tableForm);
            return wanted && have && (!wanted->rgb || wanted->rgb == have->rgb) && (!wanted->alpha || wanted->alpha == have->alpha);
        }
        const auto number = numberOf(set);
        const auto other = numberOf(current);
        if (number && other) {
            return common::fEqual(*number, *other);
        }
        return set == current;
    }

    /**
     * The value a setting gets as an option sets one on it: the option's, and of a transform or a color the setting's
     * table with the parts the option sets, in the setting's form. A current value that is no table is replaced.
     */
    Value withSetValue(const Setting& target, const Value& current, const Value& set)
    {
        const auto* setText = tableText(target, set);
        if (!setText) {
            return set;
        }
        const auto* currentText = tableText(target, current);
        if (target.type == SettingType::Transform) {
            const auto wanted = parseTransformTable(*setText, TableForm::Toml);
            auto table = currentText ? parseTransformTable(*currentText, target.tableForm).value_or(TransformTable{}) : TransformTable{};
            if (!wanted) {
                return current;
            }
            table.pos = wanted->pos ? wanted->pos : table.pos;
            table.rot = wanted->rot ? wanted->rot : table.rot;
            table.scale = wanted->scale ? wanted->scale : table.scale;
            return transformText(table, target.tableForm);
        }
        const auto wanted = parseColorTable(*setText, TableForm::Toml);
        auto table = currentText ? parseColorTable(*currentText, target.tableForm).value_or(ColorTable{}) : ColorTable{};
        if (!wanted) {
            return current;
        }
        table.rgb = wanted->rgb ? wanted->rgb : table.rgb;
        table.alpha = wanted->alpha ? wanted->alpha : table.alpha;
        return colorText(table, target.tableForm);
    }

    /**
     * The value a choice that sets other settings has while none of its options is theirs: one of its type that is no
     * option, so the row shows none. An empty text, and a number below its options.
     */
    Value blankChoiceValue(const Setting& choice)
    {
        const auto& options = choice.valueOptions.options;
        if (choice.valueType == StoredType::Int) {
            int least = 0;
            for (const auto& option : options) {
                if (const auto* whole = std::get_if<int>(&option.value)) {
                    least = (std::min)(least, *whole);
                }
            }
            return least - 1;
        }
        if (choice.valueType == StoredType::Float) {
            float least = 0.0f;
            for (const auto& option : options) {
                if (const auto number = numberOf(option.value)) {
                    least = (std::min)(least, *number);
                }
            }
            return least - 1.0f;
        }
        if (choice.valueType == StoredType::Bool) {
            return false;
        }
        return std::string();
    }
}
