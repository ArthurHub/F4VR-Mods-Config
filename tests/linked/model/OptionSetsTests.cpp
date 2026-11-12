#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstddef>
#include <string>

#include "common/CommonUtils.h"
#include "model/ConfigDefinition.h"
#include "model/OptionSets.h"
#include "model/Transforms.h"

using namespace mods_config;
using namespace std::string_literals;

namespace
{
    /**
     * A setting of a type, in a form of a table and of a type of value, as an option sets it.
     */
    model::Setting settingOf(const model::SettingType type, const model::TableForm form = model::TableForm::Toml, const model::StoredType valueType = model::StoredType::String)
    {
        model::Setting setting;
        setting.type = type;
        setting.tableForm = form;
        setting.valueType = valueType;
        return setting;
    }
}

TEST_CASE("OptionSets: a choice sets others where it is kept in them")
{
    auto choice = settingOf(model::SettingType::Stepper);
    CHECK_FALSE(model::setsOthers(choice));
    choice.storage = model::SetsStorage{};
    CHECK(model::setsOthers(choice));
}

TEST_CASE("OptionSets: a transform has an option's value where it has the parts the option sets, and a part it leaves out is zero")
{
    const auto transform = settingOf(model::SettingType::Transform);
    const model::Value beside("{ pos = [0.6, 0.0, 0.0] }"s);
    CHECK(model::hasSetValue(transform, "{ pos = [0.6, 0.0, 0.0], scale = 2.0 }"s, beside));
    CHECK(model::hasSetValue(transform, "{ scale = 2.0, pos = [0.6000001, 0, 0] }"s, beside));
    CHECK_FALSE(model::hasSetValue(transform, "{ pos = [0.0, 0.0, 0.0], scale = 2.0 }"s, beside));
    CHECK(model::hasSetValue(transform, "{ scale = 2.0 }"s, "{ pos = [0.0, 0.0, 0.0] }"s));
    CHECK_FALSE(model::hasSetValue(transform, "{ pos = [0.6, 0.0, 0.0] }"s, "{ scale = 2.0 }"s));
    CHECK(model::hasSetValue(transform, "{}"s, "{ scale = 1.0 }"s));

    // a line of an INI file has every part, and a text that is no transform has none
    CHECK(model::hasSetValue(settingOf(model::SettingType::Transform, model::TableForm::IniLine), "0.6,0,0;0,0,0;2"s, beside));
    CHECK_FALSE(model::hasSetValue(transform, "not a table"s, beside));
}

TEST_CASE("OptionSets: a color has an option's value where it has the parts the option sets")
{
    const auto color = settingOf(model::SettingType::Color);
    CHECK(model::hasSetValue(color, "{ rgb = [1, 2, 3], alpha = 100 }"s, "{ alpha = 100 }"s));
    CHECK_FALSE(model::hasSetValue(color, "{ rgb = [1, 2, 3] }"s, "{ alpha = 100 }"s));
    CHECK(model::hasSetValue(color, "{ rgb = [1, 2, 3], alpha = 100 }"s, "{ rgb = [1, 2, 3] }"s));
}

TEST_CASE("OptionSets: any other value is the option's where it is the same, and two numbers where they are near")
{
    const auto slider = settingOf(model::SettingType::Slider, model::TableForm::Toml, model::StoredType::Float);
    CHECK(model::hasSetValue(slider, 1.0f, 1));
    CHECK(model::hasSetValue(slider, 0.30000001f, 0.3f));
    CHECK_FALSE(model::hasSetValue(slider, 0.4f, 0.3f));
    const auto dropdown = settingOf(model::SettingType::Dropdown);
    CHECK(model::hasSetValue(dropdown, "never"s, "never"s));
    CHECK_FALSE(model::hasSetValue(dropdown, "Never"s, "never"s));

    // a binding is a whole value
    const auto binding = settingOf(model::SettingType::InputBinding);
    CHECK_FALSE(model::hasSetValue(binding, R"({ hand = "primary", type = "press", button = "trigger" })"s, R"({ type = "press" })"s));
}

TEST_CASE("OptionSets: an option sets the parts of a transform and a color it names, which keep the others, in the setting's form")
{
    const auto transform = settingOf(model::SettingType::Transform);
    CHECK(model::withSetValue(transform, "{ pos = [1.0, 2.0, 3.0], scale = 2.0 }"s, "{ pos = [0.6, 0.0, 0.0] }"s) == model::Value("{ pos = [0.6, 0.0, 0.0], scale = 2.0 }"s));
    CHECK(model::withSetValue(transform, "not a table"s, "{ pos = [0.6, 0.0, 0.0] }"s) == model::Value("{ pos = [0.6, 0.0, 0.0] }"s));
    const auto line = model::withSetValue(settingOf(model::SettingType::Transform, model::TableForm::IniLine), "1,2,3;4,5,6;2"s, "{ pos = [0.6, 0.0, 0.0] }"s);
    const auto table = model::parseTransformTable(std::get<std::string>(line), model::TableForm::IniLine);
    REQUIRE(table);
    for (std::size_t axis = 0; axis < model::TRANSFORM_AXES; ++axis) {
        CHECK(common::fEqual(model::axisValue(*table, axis), std::array{ 0.6f, 0.0f, 0.0f, 4.0f, 5.0f, 6.0f, 2.0f }[axis]));
    }

    const auto color = settingOf(model::SettingType::Color);
    CHECK(model::withSetValue(color, "{ rgb = [1, 2, 3], alpha = 10 }"s, "{ alpha = 100 }"s) == model::Value("{ rgb = [1, 2, 3], alpha = 100 }"s));

    // any other value is the option's, a binding's whole
    CHECK(model::withSetValue(settingOf(model::SettingType::Dropdown), "always"s, "never"s) == model::Value("never"s));
    const model::Value binding(R"({ hand = "offhand", type = "tap", button = "grip" })"s);
    CHECK(model::withSetValue(settingOf(model::SettingType::InputBinding), R"({ type = "press" })"s, binding) == binding);
}

TEST_CASE("OptionSets: a choice with none of its options has a value of its type that is no option")
{
    auto text = settingOf(model::SettingType::Stepper);
    text.valueOptions.options = { { .value = "low"s, .text = "Low" }, { .value = "high"s, .text = "High" } };
    CHECK(model::blankChoiceValue(text) == model::Value(""s));

    auto whole = settingOf(model::SettingType::Dropdown, model::TableForm::Toml, model::StoredType::Int);
    whole.valueOptions.options = { { .value = -1, .text = "Off" }, { .value = 2, .text = "Two" } };
    CHECK(model::blankChoiceValue(whole) == model::Value(-2));

    auto number = settingOf(model::SettingType::Stepper, model::TableForm::Toml, model::StoredType::Float);
    number.valueOptions.options = { { .value = 0.5f, .text = "Half" } };
    CHECK(model::blankChoiceValue(number) == model::Value(-1.0f));
}
