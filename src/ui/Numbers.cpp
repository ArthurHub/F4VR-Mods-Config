#include "Numbers.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <format>
#include <string_view>

namespace
{
    using namespace mods_config;

    // the most decimals a number has from its step
    constexpr int MAX_STEP_DECIMALS = 3;

    // units that follow the number with no space between: `40°`, and `1.78 m` for any other
    constexpr std::array<std::string_view, 4> JOINED_UNITS{ "°", "%", "′", "″" };

    /**
     * A number rounded to its decimals. One that rounds to zero has no minus, as `-0.00`.
     */
    std::string withDecimals(const float value, const int decimals)
    {
        auto text = std::format("{:.{}f}", value, decimals);
        if (text.starts_with('-') && text.find_first_not_of("0.", 1) == std::string::npos) {
            text.erase(0, 1);
        }
        return text;
    }

    /**
     * A number of a setting as it is kept, with no display: a slider's with the decimals of its step, and a text
     * input's as short as it is written.
     */
    std::string storedText(const model::Setting& setting, const float value)
    {
        return setting.type == model::SettingType::TextInput ? std::format("{}", value) : withDecimals(value, ui::stepDecimals(setting.valueOptions.step));
    }
}

namespace mods_config::ui
{
    /**
     * As many decimals as a step has, three at most: 0.05 has two, and 1 none.
     */
    int stepDecimals(const float step)
    {
        int decimals = 0;
        for (float scaled = std::abs(step); decimals < MAX_STEP_DECIMALS && std::abs(scaled - std::round(scaled)) > 0.001f; scaled *= 10.0f) {
            ++decimals;
        }
        return decimals;
    }

    /**
     * Whether a setting's number is written as a percentage of its range: its display says so, and it has a
     * range, whose most is above its least.
     */
    bool isPercent(const model::Setting& setting)
    {
        return setting.display.percent && setting.valueOptions.max > setting.valueOptions.min;
    }

    /**
     * A number of a setting as its display writes it: with its display's decimals, or else a slider's with as
     * many as its step has and a text input's as short as it is written, and its unit after it. A unit that
     * starts with a degree, a percent or a prime follows the number with no space, and any other with one. A
     * percentage is the place of the number in its range, in whole percents unless its decimals say more, and
     * has no other unit. The number is rounded for showing only.
     */
    std::string numberText(const model::Setting& setting, const float value, const bool withUnit)
    {
        const auto& display = setting.display;
        if (isPercent(setting)) {
            const auto& options = setting.valueOptions;
            return withDecimals((value - options.min) / (options.max - options.min) * 100.0f, display.decimals.value_or(0)) + "%";
        }
        auto text = display.decimals ? withDecimals(value, *display.decimals) : storedText(setting, value);
        if (!withUnit || display.unit.empty()) {
            return text;
        }
        const bool joined = std::ranges::any_of(JOINED_UNITS, [&display](const std::string_view unit) {
            return display.unit.starts_with(unit);
        });
        return std::format("{}{}{}", text, joined ? "" : " ", display.unit);
    }

    /**
     * A percentage with the number it is of, for the help line: `85% (217 of 255)`. The number and the most
     * are written as they are kept.
     */
    std::string percentDetail(const model::Setting& setting, const float value)
    {
        return std::format("{} ({} of {})", numberText(setting, value), storedText(setting, value), storedText(setting, setting.valueOptions.max));
    }

    /**
     * The number a text is, all of it. None for a text that is no number, and for an empty one.
     */
    std::optional<float> fieldNumber(const std::string& text)
    {
        float number = 0.0f;
        const auto* end = text.data() + text.size();
        const auto [read, error] = std::from_chars(text.data(), end, number);
        return error == std::errc() && read == end && !text.empty() ? std::optional(number) : std::nullopt;
    }

    /**
     * The text of a text input as its box shows it: a number with a display as the display writes it, and any
     * other text as it is, as a number with no display is, which is written as its store has it.
     */
    std::string fieldText(const model::Setting& setting, const std::string& text)
    {
        if (setting.valueType == model::StoredType::String || setting.display == model::NumberDisplay{}) {
            return text;
        }
        const auto number = fieldNumber(text);
        return number ? numberText(setting, *number) : text;
    }
}
