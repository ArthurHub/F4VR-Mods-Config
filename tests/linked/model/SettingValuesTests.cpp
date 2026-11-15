#include <catch2/catch_test_macros.hpp>

#include <nlohmann/json.hpp>
#include <string>
#include <vector>

#include "model/ConfigDefinition.h"
#include "model/SettingValues.h"

using namespace mods_config;
using Json = nlohmann::json;
using Kind = model::ValueReadError::Kind;
using model::SettingType;
using model::StoredType;
using model::TableForm;

namespace
{
    /**
     * A setting of a type, kept as a value type and in a table form.
     */
    model::Setting settingOf(const SettingType type, const StoredType valueType = StoredType::String, const TableForm form = TableForm::Toml)
    {
        model::Setting setting{ .type = type };
        setting.valueType = valueType;
        setting.tableForm = form;
        return setting;
    }

    /**
     * What is wrong with a value of a setting, whose options are its own.
     */
    std::string problemOf(const model::Setting& setting, const model::Value& value)
    {
        return model::valueProblem(setting, setting.valueOptions.options, value);
    }

    /**
     * A JSON value read as a value of a setting.
     */
    std::expected<model::Value, model::ValueReadError> readOf(const model::Setting& setting, const Json& value)
    {
        return model::readJsonValue(setting.type, setting.valueType, setting.tableForm, setting.valueOptions, value);
    }
}

TEST_CASE("SettingValues: each setting type is or is not a framework type, a table and a choice")
{
    struct Row
    {
        SettingType type;
        bool framework;
        bool table;
        bool choice;
        std::string_view kind;
    };

    const std::vector<Row> rows{
        { SettingType::Switcher, false, false, false, "" },
        { SettingType::Slider, false, false, false, "" },
        { SettingType::Stepper, false, false, true, "" },
        { SettingType::Dropdown, false, false, true, "" },
        { SettingType::TextInput, false, false, false, "" },
        { SettingType::InputBinding, true, true, false, "binding" },
        { SettingType::Haptic, true, false, true, "haptic" },
        { SettingType::Color, true, true, false, "color" },
        { SettingType::Transform, true, true, false, "transform" },
    };
    for (const auto& row : rows) {
        INFO(static_cast<int>(row.type));
        CHECK(model::isFrameworkType(row.type) == row.framework);
        CHECK(model::isTableType(row.type) == row.table);
        CHECK(model::isChoice(row.type) == row.choice);
        CHECK(model::valueKindName(row.type) == row.kind);
    }
}

TEST_CASE("SettingValues: a table type is kept as a table in a TOML file, and as a text in an INI line")
{
    CHECK(model::keptAsTable(settingOf(SettingType::Color)));
    CHECK(model::keptAsTable(settingOf(SettingType::InputBinding)));
    CHECK_FALSE(model::keptAsTable(settingOf(SettingType::Transform, StoredType::String, TableForm::IniLine)));
    CHECK_FALSE(model::keptAsTable(settingOf(SettingType::Haptic)));
    CHECK_FALSE(model::keptAsTable(settingOf(SettingType::TextInput)));
}

TEST_CASE("SettingValues: two numbers are the same value when they are as near as a number, and any other two when equal")
{
    CHECK(model::sameValue(0.1f + 0.2f, 0.3f));
    CHECK_FALSE(model::sameValue(1.0f, 1.5f));
    CHECK(model::sameValue(3, 3));
    CHECK_FALSE(model::sameValue(3, 3.0f));
    CHECK(model::sameValue(std::string("a"), std::string("a")));
    CHECK_FALSE(model::sameValue(true, false));
    // a table's text is compared as it is: a caller compares canonical values
    CHECK_FALSE(model::sameValue(std::string("1,2,3"), std::string(" 1, 2 ,3 ")));
}

TEST_CASE("SettingValues: a framework type's value is in its canonical text, and any other value as it is")
{
    CHECK(model::canonicalValue(SettingType::InputBinding, TableForm::IniLine, std::string("main click b")) == model::Value(std::string("primary press menu")));
    CHECK(model::canonicalValue(SettingType::Haptic, TableForm::Toml, std::string("double click")) == model::Value(std::string("DoubleClick")));
    CHECK(model::canonicalValue(SettingType::Haptic, TableForm::Toml, std::string("off")) == model::Value(std::string("none")));
    CHECK(model::canonicalValue(SettingType::Color, TableForm::IniLine, std::string(" 1, 2 ,3 ")) == model::Value(std::string("1,2,3")));
    CHECK(model::canonicalValue(SettingType::Transform, TableForm::Toml, std::string("{scale=2}")) == model::Value(std::string("{ scale = 2.0 }")));

    // a text that is none is as it is, and a framework type's value that is no text has no canonical text
    CHECK(model::canonicalValue(SettingType::Color, TableForm::IniLine, std::string("1,2")) == model::Value(std::string("1,2")));
    CHECK_FALSE(model::canonicalValue(SettingType::Color, TableForm::Toml, 5));

    CHECK(model::canonicalValue(SettingType::Slider, TableForm::Toml, 2.5f) == model::Value(2.5f));
    CHECK(model::canonicalValue(SettingType::TextInput, TableForm::Toml, std::string(" a ")) == model::Value(std::string(" a ")));
}

TEST_CASE("SettingValues: a value is of the type its control shows")
{
    struct Row
    {
        SettingType type;
        model::Value value;
        bool shown;
    };

    const std::vector<Row> rows{
        { SettingType::Switcher, true, true },
        { SettingType::Switcher, 1, false },
        { SettingType::Slider, 1.0f, true },
        { SettingType::Slider, 1, false },
        { SettingType::Stepper, 1, true },
        { SettingType::Dropdown, std::string("a"), true },
        { SettingType::Haptic, std::string("Tick"), true },
        { SettingType::TextInput, std::string("a"), true },
        { SettingType::TextInput, 1, false },
        { SettingType::InputBinding, std::string("none"), true },
        { SettingType::Color, 1.0f, false },
        { SettingType::Transform, std::string("{}"), true },
    };
    for (const auto& row : rows) {
        INFO(static_cast<int>(row.type));
        CHECK(model::isControlValue(row.type, row.value) == row.shown);
    }
}

TEST_CASE("SettingValues: a number outside its limits, a text that is no number and a text of a length not allowed are invalid")
{
    auto slider = settingOf(SettingType::Slider, StoredType::Float);
    slider.valueOptions = { .min = 0.0f, .max = 10.0f };
    CHECK(problemOf(slider, 10.0f).empty());
    CHECK(problemOf(slider, 3).empty());
    CHECK(problemOf(slider, 10.5f) == "is not from 0 to 10");
    CHECK(problemOf(slider, -1) == "is not from 0 to 10");

    // a slider with no limits takes any number
    CHECK(problemOf(settingOf(SettingType::Slider, StoredType::Float), 99.0f).empty());

    // a number field's value is its text, or its number as a config spec writes it
    auto whole = settingOf(SettingType::TextInput, StoredType::Int);
    whole.valueOptions = { .min = 1.0f, .max = 9.0f };
    CHECK(problemOf(whole, std::string("5")).empty());
    CHECK(problemOf(whole, std::string("5.5")) == "is not a whole number");
    CHECK(problemOf(whole, std::string("12")) == "is not from 1 to 9");
    CHECK(problemOf(whole, 12) == "is not from 1 to 9");
    CHECK(problemOf(settingOf(SettingType::TextInput, StoredType::Float), std::string("a")) == "is not a number");

    auto text = settingOf(SettingType::TextInput);
    text.valueOptions = { .minLength = 1, .maxLength = 3 };
    CHECK(problemOf(text, std::string("abc")).empty());
    CHECK(problemOf(text, std::string("")) == "has 0 characters, and needs at least 1");
    CHECK(problemOf(text, std::string("abcd")) == "has 4 characters, and can have at most 3");

    // a check a value of another alternative has no part in passes
    CHECK(problemOf(slider, std::string("a")).empty());
    CHECK(problemOf(text, 5).empty());
}

TEST_CASE("SettingValues: a choice's value that is none of its options is invalid")
{
    auto dropdown = settingOf(SettingType::Dropdown, StoredType::Int);
    dropdown.valueOptions.options = { { .value = 1, .text = "One" }, { .value = 2, .text = "Two" } };
    CHECK(problemOf(dropdown, 2).empty());
    CHECK(problemOf(dropdown, 3) == "is none of its options");

    auto stepper = settingOf(SettingType::Stepper, StoredType::Float);
    stepper.valueOptions.options = { { .value = 0.3f, .text = "Low" } };
    CHECK(problemOf(stepper, 0.1f + 0.2f).empty());
}

TEST_CASE("SettingValues: a framework type's invalid value is a sentence of its own")
{
    CHECK(problemOf(settingOf(SettingType::Haptic), std::string("Tick")).empty());
    CHECK(problemOf(settingOf(SettingType::Haptic), std::string("Tik")) == "Tik is not a haptic pattern, so none plays");

    CHECK(problemOf(settingOf(SettingType::Color, StoredType::String, TableForm::IniLine), std::string("1,2,3")).empty());
    CHECK(problemOf(settingOf(SettingType::Color, StoredType::String, TableForm::IniLine), std::string("1,2")) ==
          "it is not r,g,b or r,g,b,a, each a whole number in 0..255, as 255,180,107");

    CHECK(problemOf(settingOf(SettingType::Transform), std::string("{ scale = 2 }")).empty());
    CHECK(problemOf(settingOf(SettingType::Transform), std::string("5")) == "it is not a table");

    auto binding = settingOf(SettingType::InputBinding, StoredType::String, TableForm::IniLine);
    CHECK(problemOf(binding, std::string("primary press trigger")).empty());
    CHECK(problemOf(binding, std::string("primary press thumbstik")).starts_with("'thumbstik' is not a button"));
    binding.valueOptions.hands = { vrcf::Hand::Offhand };
    CHECK_FALSE(problemOf(binding, std::string("primary press trigger")).empty());
}

TEST_CASE("SettingValues: a table is read from a JSON object as the text of its form")
{
    const Json color = { { "rgb", { 1, 2, 3 } } };
    CHECK(readOf(settingOf(SettingType::Color, StoredType::String, TableForm::IniLine), color) == model::Value(std::string("1,2,3")));
    CHECK(readOf(settingOf(SettingType::Color), color) == model::Value(std::string("{ rgb = [1, 2, 3] }")));

    const Json transform = { { "scale", 2 } };
    CHECK(readOf(settingOf(SettingType::Transform), transform) == model::Value(std::string("{ scale = 2.0 }")));

    const Json binding = { { "hand", "primary" }, { "type", "press" }, { "button", "trigger" } };
    CHECK(readOf(settingOf(SettingType::InputBinding, StoredType::String, TableForm::IniLine), binding) == model::Value(std::string("primary press trigger")));

    // no object is not of the type, and an object that is no table has the framework's reason
    for (const auto type : { SettingType::InputBinding, SettingType::Color, SettingType::Transform }) {
        INFO(static_cast<int>(type));
        CHECK(readOf(settingOf(type), Json("{ scale = 2 }")).error().kind == Kind::NotOfType);
        CHECK(readOf(settingOf(type), Json()).error().kind == Kind::NotOfType);
    }
    const auto notColor = readOf(settingOf(SettingType::Color), Json{ { "rgb", { 1, 2 } } });
    REQUIRE_FALSE(notColor);
    CHECK(notColor.error().kind == Kind::NotValue);
    CHECK_FALSE(notColor.error().reason.empty());
}

TEST_CASE("SettingValues: a binding read from JSON is one its setting takes, and one its INI line can have")
{
    auto setting = settingOf(SettingType::InputBinding, StoredType::String, TableForm::IniLine);
    setting.valueOptions.hands = { vrcf::Hand::Offhand };
    const auto untaken = readOf(setting, Json{ { "hand", "primary" }, { "type", "press" }, { "button", "trigger" } });
    REQUIRE_FALSE(untaken);
    CHECK(untaken.error().kind == Kind::Untaken);
    CHECK_FALSE(untaken.error().reason.empty());

    const Json held = { { "hand", "primary" }, { "type", "axis" }, { "axis", "thumbstick" }, { "direction", "up" }, { "accelerate", false } };
    CHECK(readOf(settingOf(SettingType::InputBinding, StoredType::String, TableForm::IniLine), held).error().kind == Kind::IniAccelerate);
    CHECK(readOf(settingOf(SettingType::InputBinding), held));
}

TEST_CASE("SettingValues: a haptic is read from JSON as its pattern's name, in its canonical text")
{
    const auto setting = settingOf(SettingType::Haptic);
    CHECK(readOf(setting, Json("double click")) == model::Value(std::string("DoubleClick")));
    CHECK(readOf(setting, Json("none")) == model::Value(std::string("none")));
    CHECK(readOf(setting, Json("Tik")).error().kind == Kind::NotValue);
    CHECK(readOf(setting, Json(5)).error().kind == Kind::NotOfType);
}

TEST_CASE("SettingValues: any other value is read from JSON in its value type, and is one of its setting's values")
{
    CHECK(readOf(settingOf(SettingType::Switcher, StoredType::Bool), Json(true)) == model::Value(true));
    CHECK(readOf(settingOf(SettingType::Switcher, StoredType::Bool), Json(1)).error().kind == Kind::NotOfType);
    CHECK(readOf(settingOf(SettingType::Slider, StoredType::Float), Json(2)) == model::Value(2.0f));
    CHECK(readOf(settingOf(SettingType::TextInput, StoredType::Int), Json(2.5)).error().kind == Kind::NotOfType);
    CHECK(readOf(settingOf(SettingType::TextInput), Json()).error().kind == Kind::NotOfType);

    auto slider = settingOf(SettingType::Slider, StoredType::Float);
    slider.valueOptions = { .min = 0.0f, .max = 10.0f };
    const auto outside = readOf(slider, Json(11));
    REQUIRE_FALSE(outside);
    CHECK(outside.error().kind == Kind::NotValue);
    CHECK(outside.error().reason == "is not from 0 to 10");

    // any value is one of a choice whose options are not known until the game runs
    auto dropdown = settingOf(SettingType::Dropdown, StoredType::String);
    dropdown.valueOptions.options = { { .value = std::string("a"), .text = "A" } };
    CHECK(readOf(dropdown, Json("b")).error().kind == Kind::NotValue);
    dropdown.valueOptions.clientOptions = true;
    CHECK(readOf(dropdown, Json("b")) == model::Value(std::string("b")));
}

TEST_CASE("SettingValues: a JSON value is the TOML text of the same value")
{
    CHECK(model::tomlText(Json{ { "rgb", { 1, 2, 3 } }, { "alpha", 5 } }) == R"({ "alpha" = 5, "rgb" = [1, 2, 3] })");
    CHECK(model::tomlText(Json::object()) == "{}");
    CHECK(model::tomlText(Json("a")) == R"("a")");
    CHECK(model::tomlText(Json()).empty());
}
