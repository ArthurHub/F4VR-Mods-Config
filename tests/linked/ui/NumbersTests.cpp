#include <catch2/catch_test_macros.hpp>

#include <string>
#include <utility>

#include "ui/Numbers.h"

using namespace mods_config;

namespace
{
    /**
     * A slider of a range and a step, shown as a display says.
     */
    model::Setting sliderOf(const float min, const float max, const float step, model::NumberDisplay display = {})
    {
        return model::Setting{ .type = model::SettingType::Slider, .valueOptions = { .min = min, .max = max, .step = step, .options = {} }, .display = std::move(display) };
    }

    /**
     * A text input of a number, shown as a display says.
     */
    model::Setting fieldOf(model::NumberDisplay display, const model::StoredType type = model::StoredType::Float)
    {
        return model::Setting{ .type = model::SettingType::TextInput, .valueOptions = { .min = 0.0f, .max = 10.0f }, .valueType = type, .display = std::move(display) };
    }
}

TEST_CASE("Numbers: a number has its display's decimals, or else as many as its step has")
{
    CHECK(ui::numberText(sliderOf(0.0f, 2.0f, 0.01f), 1.25f) == "1.25");
    CHECK(ui::numberText(sliderOf(0.0f, 24.0f, 1.0f), 8.0f) == "8");
    // three at most from a step
    CHECK(ui::numberText(sliderOf(0.0f, 1.0f, 0.002f), 0.12345f) == "0.123");
    CHECK(ui::numberText(sliderOf(0.0f, 50.0f, 1.0f, { .decimals = 1 }), 12.0f) == "12.0");
    CHECK(ui::numberText(sliderOf(0.0f, 1.0f, 0.001f, { .decimals = 0 }), 0.75f) == "1");
    // a negative number has a minus, and one that is rounded to zero has none
    CHECK(ui::numberText(sliderOf(-1.0f, 1.0f, 0.05f), -0.5f) == "-0.50");
    CHECK(ui::numberText(sliderOf(-1.0f, 1.0f, 0.01f), -0.001f) == "0.00");
}

TEST_CASE("Numbers: a unit follows its number, with a space but for a degree, a percent and a prime")
{
    CHECK(ui::numberText(sliderOf(0.5f, 2.5f, 0.01f, { .unit = "m" }), 1.78f) == "1.78 m");
    CHECK(ui::numberText(sliderOf(0.0f, 90.0f, 1.0f, { .unit = "°" }), 40.0f) == "40°");
    CHECK(ui::numberText(sliderOf(0.0f, 100.0f, 1.0f, { .unit = "%" }), 40.0f) == "40%");
    CHECK(ui::numberText(sliderOf(0.0f, 12.0f, 1.0f, { .unit = "′" }), 6.0f) == "6′");
    // the number alone, as a box with no room for its unit has it
    CHECK(ui::numberText(sliderOf(0.5f, 2.5f, 0.01f, { .unit = "m" }), 1.78f, false) == "1.78");
}

TEST_CASE("Numbers: a percentage is the place of a number in its range, in whole percents unless its decimals say more")
{
    const auto opacity = sliderOf(0.0f, 255.0f, 1.0f, { .percent = true });
    CHECK(ui::isPercent(opacity));
    CHECK(ui::numberText(opacity, 217.0f) == "85%");
    CHECK(ui::numberText(opacity, 0.0f) == "0%");
    CHECK(ui::numberText(opacity, 255.0f) == "100%");
    CHECK(ui::percentDetail(opacity, 217.0f) == "85% (217 of 255)");
    CHECK(ui::numberText(sliderOf(0.0f, 255.0f, 1.0f, { .decimals = 1, .percent = true }), 217.0f) == "85.1%");
    // from a least that is not 0, and with no other unit
    CHECK(ui::numberText(sliderOf(0.5f, 1.5f, 0.05f, { .unit = "m", .percent = true }), 1.0f) == "50%");

    // a number with no range is no percentage
    const auto noRange = sliderOf(0.0f, 0.0f, 1.0f, { .percent = true });
    CHECK_FALSE(ui::isPercent(noRange));
    CHECK(ui::numberText(noRange, 3.0f) == "3");
}

TEST_CASE("Numbers: a text input of a number with a display shows it as the display writes it, and any other text as it is")
{
    CHECK(ui::fieldText(fieldOf({ .decimals = 2 }), "1.5") == "1.50");
    CHECK(ui::fieldText(fieldOf({ .unit = "h" }), "8") == "8 h");
    CHECK(ui::fieldText(fieldOf({ .unit = "h" }, model::StoredType::Int), "-3") == "-3 h");
    CHECK(ui::fieldText(fieldOf({ .percent = true }), "2.5") == "25%");
    // with no display it is the text its store has, as MCM's menu writes a number
    CHECK(ui::fieldText(fieldOf({}), "3.140000") == "3.140000");
    // a text that is no number, and a text input of a text
    CHECK(ui::fieldText(fieldOf({ .unit = "h" }), "") == "");
    CHECK(ui::fieldText(fieldOf({ .unit = "h" }), "fast") == "fast");
    CHECK(ui::fieldText(fieldOf({ .unit = "h" }, model::StoredType::String), "12") == "12");
}
