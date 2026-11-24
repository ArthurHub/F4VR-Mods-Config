#include "SettingValues.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <format>

#include <nlohmann/json.hpp>

#include "Colors.h"
#include "FileChoices.h"
#include "Haptics.h"
#include "InputBindings.h"
#include "Transforms.h"
#include "common/CommonUtils.h"

namespace
{
    using namespace mods_config;

    /**
     * Whether a value is the value of one of the options of a choice.
     */
    bool isOption(const std::vector<model::Option>& options, const model::Value& value)
    {
        return std::ranges::any_of(options, [&value](const model::Option& option) {
            return model::sameValue(option.value, value);
        });
    }

    /**
     * The number of a value whose setting has one: a number, a whole number, and the text of one in a text input that
     * takes a number. None for any other value.
     */
    std::optional<float> numberOf(const model::StoredType valueType, const model::Value& value)
    {
        if (const auto* whole = std::get_if<int>(&value)) {
            return static_cast<float>(*whole);
        }
        if (const auto* number = std::get_if<float>(&value)) {
            return *number;
        }
        const auto* text = std::get_if<std::string>(&value);
        return text ? model::fieldNumber(valueType, *text) : std::nullopt;
    }

    /**
     * What is wrong with the value of a binding, a haptic, a color or a transform, which is a sentence of its own: the
     * framework's reason for a table that is none, the hand, the way or the button of a binding its setting does not
     * take, and a name of no haptic pattern. Empty for a value that is one, and for one that is no text.
     */
    std::string frameworkProblem(const model::SettingType type, const model::TableForm form, const model::ValueOptions& valueOptions, const model::Value& value)
    {
        const auto* text = std::get_if<std::string>(&value);
        if (!text) {
            return {};
        }
        switch (type) {
        case model::SettingType::InputBinding: {
            const auto binding = model::parseBinding(*text, form);
            return binding ? model::untakenPart(*binding, valueOptions) : binding.error();
        }
        case model::SettingType::Haptic:
            return model::findHaptic(value) ? std::string() : std::format("{} is not a haptic pattern, so none plays", *text);
        case model::SettingType::Color: {
            const auto color = model::parseColorTable(*text, form);
            return color ? std::string() : color.error();
        }
        case model::SettingType::Transform: {
            const auto transform = model::parseTransformTable(*text, form);
            return transform ? std::string() : transform.error();
        }
        default:
            return {};
        }
    }
}

namespace mods_config::model
{
    /**
     * Whether a setting type is one of CommonFramework's: a binding, a haptic, a color and a transform. Its value is
     * always a text, and what is wrong with one is a sentence of its own (`valueProblem`).
     */
    bool isFrameworkType(const SettingType type)
    {
        return type == SettingType::InputBinding || type == SettingType::Haptic || type == SettingType::Color || type == SettingType::Transform;
    }

    /**
     * The name of a framework type's value, as a config spec's report names it: `binding`, `haptic`, `color` and
     * `transform`. Empty for any other type.
     */
    std::string_view valueKindName(const SettingType type)
    {
        switch (type) {
        case SettingType::InputBinding:
            return "binding";
        case SettingType::Haptic:
            return "haptic";
        case SettingType::Color:
            return "color";
        case SettingType::Transform:
            return "transform";
        default:
            return {};
        }
    }

    /**
     * Whether a setting type's value is a table of its file: a binding, a color and a transform, kept in a TOML or an
     * INI file alone.
     */
    bool isTableType(const SettingType type)
    {
        return type == SettingType::InputBinding || type == SettingType::Color || type == SettingType::Transform;
    }

    /**
     * Whether a setting's value is the TOML text of an inline table of its file: a table type's in a TOML file. In an
     * INI file each is the text of its line, which is read and written as a text.
     */
    bool keptAsTable(const Setting& setting)
    {
        return isTableType(setting.type) && setting.tableForm == TableForm::Toml;
    }

    /**
     * Whether a setting type is a choice of its options: a stepper, a dropdown, and a haptic, whose options are the
     * framework's patterns.
     */
    bool isChoice(const SettingType type)
    {
        return type == SettingType::Stepper || type == SettingType::Dropdown || type == SettingType::Haptic;
    }

    /**
     * Whether a setting is a text input that takes a number: one whose value is kept as an int or a float.
     */
    bool isNumberField(const SettingType type, const StoredType valueType)
    {
        return type == SettingType::TextInput && valueType != StoredType::String;
    }

    bool isNumberField(const Setting& setting)
    {
        return isNumberField(setting.type, setting.valueType);
    }

    /**
     * The number a text is, all of it, as a text input that takes one keeps it: a whole number has nothing but its
     * digits and a sign. None for a text that is no number of the field's type, and for an empty one.
     */
    std::optional<float> fieldNumber(const StoredType valueType, const std::string_view text)
    {
        const auto* end = text.data() + text.size();
        if (valueType == StoredType::Int) {
            int whole = 0;
            const auto [read, error] = std::from_chars(text.data(), end, whole);
            return error == std::errc() && read == end ? std::optional(static_cast<float>(whole)) : std::nullopt;
        }
        float number = 0.0f;
        const auto [read, error] = std::from_chars(text.data(), end, number);
        return error == std::errc() && read == end && std::isfinite(number) ? std::optional(number) : std::nullopt;
    }

    /**
     * Whether two values of a setting are the same one. Two ints are when they are equal: a float holds a whole number
     * exactly only up to 2^24, short of a form id or a packed color. Two other numbers are when they are as near as a
     * number and the text of it in a file, an int and a float too: an int slider's default is read as an int, and its
     * value is a float. Any other two are when they are equal. A table's text is compared as it is, so a caller compares
     * canonical values (`canonicalValue`).
     */
    bool sameValue(const Value& value, const Value& other)
    {
        if (std::holds_alternative<int>(value) && std::holds_alternative<int>(other)) {
            return value == other;
        }
        const auto number = [](const Value& given) -> std::optional<float> {
            if (const auto* whole = std::get_if<int>(&given)) {
                return static_cast<float>(*whole);
            }
            const auto* real = std::get_if<float>(&given);
            return real ? std::optional(*real) : std::nullopt;
        };
        const auto first = number(value);
        const auto second = number(other);
        return first && second ? common::fEqual(*first, *second) : value == other;
    }

    /**
     * A value in its canonical text, so two texts of one value are the same text: a binding's and a haptic's as the
     * framework writes them, and a color's and a transform's table with the keys it has, each in the form of its file.
     * None for a framework type's value that is no text. A value of any other type is as it is.
     */
    std::optional<Value> canonicalValue(const SettingType type, const TableForm form, const Value& value)
    {
        if (!isFrameworkType(type)) {
            return value;
        }
        const auto* text = std::get_if<std::string>(&value);
        if (!text) {
            return std::nullopt;
        }
        switch (type) {
        case SettingType::InputBinding:
            return canonicalBinding(*text, form);
        case SettingType::Haptic:
            return canonicalHaptic(*text);
        case SettingType::Color:
            return canonicalColor(*text, form);
        default:
            return canonicalTransform(*text, form);
        }
    }

    /**
     * Whether a value is of the type its setting's control shows: a bool of a switcher, a number of a slider, and a
     * text of a text input and of a binding, a color and a transform. Any value of a choice is, which shows it as long
     * as one of its options has it.
     */
    bool isControlValue(const SettingType type, const Value& value)
    {
        switch (type) {
        case SettingType::Switcher:
            return std::holds_alternative<bool>(value);
        case SettingType::Slider:
            return std::holds_alternative<float>(value);
        case SettingType::Stepper:
        case SettingType::Dropdown:
        case SettingType::Haptic:
            return true;
        default:
            return std::holds_alternative<std::string>(value);
        }
    }

    /**
     * What is wrong with a value of a setting, as the end of a sentence of the value: `is not from 0.5 to 1.5`. A text
     * of a number that is none, a number outside the limits, a text of a length its setting does not allow, and none of
     * the options of a choice, which of a choice of files names its folder. A choice that takes a typed value
     * (`typedOption`) takes one that is none of its options, of a length it allows. A framework type's is a sentence of its own
     * (`isFrameworkType`): the framework's reason for a table, a binding's untaken part, and a name of no haptic pattern.
     * A number field's value is its number or its text. A check a value of another alternative has no part in passes.
     * Empty for a value that is one.
     */
    std::string valueProblem(const SettingType type, const StoredType valueType, const TableForm form, const ValueOptions& valueOptions, const std::vector<Option>& options,
        const Value& value)
    {
        if (isFrameworkType(type)) {
            return frameworkProblem(type, form, valueOptions, value);
        }
        const bool numberField = isNumberField(type, valueType);
        if (type == SettingType::Slider || numberField) {
            const auto number = numberOf(valueType, value);
            if (!number && numberField && std::holds_alternative<std::string>(value)) {
                return valueType == StoredType::Int ? "is not a whole number" : "is not a number";
            }
            const auto& limits = valueOptions;
            const bool hasLimits = limits.max > limits.min;
            if (number && hasLimits && !((*number > limits.min || common::fEqual(*number, limits.min)) && (*number < limits.max || common::fEqual(*number, limits.max)))) {
                return std::format("is not from {} to {}", valueOptions.min, valueOptions.max);
            }
        }
        const bool typed = isChoice(type) && !isOption(options, value);
        if (typed && !valueOptions.typedOption) {
            return valueOptions.folderOptions ? std::format("is not in {}", folderText(*valueOptions.folderOptions)) : "is none of its options";
        }
        const auto* text = std::get_if<std::string>(&value);
        if (((type == SettingType::TextInput && !numberField) || typed) && text) {
            const int length = textLength(*text);
            if (length < valueOptions.minLength) {
                return std::format("has {} character{}, and needs at least {}", length, length == 1 ? "" : "s", valueOptions.minLength);
            }
            if (valueOptions.maxLength > 0 && length > valueOptions.maxLength) {
                return std::format("has {} characters, and can have at most {}", length, valueOptions.maxLength);
            }
        }
        return {};
    }

    std::string valueProblem(const Setting& setting, const std::vector<Option>& options, const Value& value)
    {
        return valueProblem(setting.type, setting.valueType, setting.tableForm, setting.valueOptions, options, value);
    }

    /**
     * A JSON value in a value type: a JSON bool for a bool, a whole number for an int, any number for a float, and a
     * text for a string. None for a JSON value of another type.
     */
    std::optional<Value> jsonValue(const nlohmann::json& value, const StoredType valueType)
    {
        if (valueType == StoredType::Bool && value.is_boolean()) {
            return value.get<bool>();
        }
        if (valueType == StoredType::Int && value.is_number_integer()) {
            return value.get<int>();
        }
        if (valueType == StoredType::Float && value.is_number()) {
            return value.get<float>();
        }
        if (valueType == StoredType::String && value.is_string()) {
            return value.get<std::string>();
        }
        return std::nullopt;
    }

    /**
     * A value of the game in a stored type, as Papyrus casts it: a bool is on at any number but 0, a whole number is
     * the nearest one, and a bool is 1 and 0 as a number. None for a text as a number and a number as a text: a store
     * that reads a text as a number reads it first.
     */
    std::optional<Value> asStoredType(const Value& value, const StoredType type)
    {
        const bool isText = std::holds_alternative<std::string>(value);
        if (type == StoredType::String || isText) {
            return type == StoredType::String && isText ? std::optional(value) : std::nullopt;
        }
        const auto* on = std::get_if<bool>(&value);
        const auto* whole = std::get_if<int>(&value);
        const float number = on ? (*on ? 1.0f : 0.0f) : whole ? static_cast<float>(*whole) : std::get<float>(value);
        switch (type) {
        case StoredType::Bool:
            return common::fNotEqual(number, 0.0f);
        case StoredType::Int:
            return static_cast<int>(std::lround(number));
        default:
            return number;
        }
    }

    /**
     * A JSON value as the TOML text of the same value: an object is an inline table and an array an array, whose keys
     * and texts JSON writes as TOML writes them, and a bool and a number are themselves. A null is none, which no TOML
     * value reads.
     */
    std::string tomlText(const nlohmann::json& value)
    {
        if (value.is_object()) {
            std::string text;
            for (const auto& [key, item] : value.items()) {
                text += std::format("{}{} = {}", text.empty() ? "{ " : ", ", nlohmann::json(key).dump(), tomlText(item));
            }
            return text.empty() ? "{}" : text + " }";
        }
        if (value.is_array()) {
            std::string text;
            for (const auto& item : value) {
                text += std::format("{}{}", text.empty() ? "[" : ", ", tomlText(item));
            }
            return text.empty() ? "[]" : text + "]";
        }
        return value.is_null() ? std::string() : value.dump();
    }

    /**
     * A setting's value from a config spec's JSON, as its `default` and an option's set value write it. A table is a
     * JSON object, as `{ "rgb": [190, 255, 190] }`, which the framework reads as the table of its config, and is the
     * text of `form`: the TOML text of the table, with the keys it has, or an INI file's line. A haptic is the name of
     * its pattern, in its canonical text. Any other is a value of its `valueType` that is one of its values (`valueProblem`):
     * any value is one of a choice that reads its options in the game, from its mod's client or from a folder, which
     * are not known until then. A null is no value, of any type.
     */
    std::expected<Value, ValueReadError> readJsonValue(const SettingType type, const StoredType valueType, const TableForm form, const ValueOptions& valueOptions,
        const nlohmann::json& value)
    {
        using Kind = ValueReadError::Kind;
        const auto fail = [](const Kind kind, std::string reason = {}) {
            return std::unexpected(ValueReadError{ .kind = kind, .reason = std::move(reason) });
        };
        switch (type) {
        case SettingType::InputBinding: {
            if (!value.is_object()) {
                return fail(Kind::NotOfType);
            }
            const auto binding = parseBinding(tomlText(value), TableForm::Toml);
            if (!binding) {
                return fail(Kind::NotValue, binding.error());
            }
            if (auto untaken = untakenPart(*binding, valueOptions); !untaken.empty()) {
                return fail(Kind::Untaken, std::move(untaken));
            }
            if (form == TableForm::IniLine && !binding->accelerate) {
                return fail(Kind::IniAccelerate);
            }
            return bindingText(*binding, form);
        }
        case SettingType::Color: {
            if (!value.is_object()) {
                return fail(Kind::NotOfType);
            }
            const auto color = parseColorTable(tomlText(value), TableForm::Toml);
            return color ? std::expected<Value, ValueReadError>(colorText(*color, form)) : fail(Kind::NotValue, color.error());
        }
        case SettingType::Transform: {
            if (!value.is_object()) {
                return fail(Kind::NotOfType);
            }
            const auto transform = parseTransformTable(tomlText(value), TableForm::Toml);
            return transform ? std::expected<Value, ValueReadError>(transformText(*transform, form)) : fail(Kind::NotValue, transform.error());
        }
        case SettingType::Haptic: {
            if (!value.is_string()) {
                return fail(Kind::NotOfType);
            }
            Value haptic = canonicalHaptic(value.get<std::string>());
            return findHaptic(haptic) ? std::expected<Value, ValueReadError>(std::move(haptic)) : fail(Kind::NotValue);
        }
        default:
            break;
        }
        auto typed = jsonValue(value, valueType);
        if (!typed) {
            return fail(Kind::NotOfType);
        }
        if (isChoice(type) && (valueOptions.gameOptions || valueOptions.clientOptions || valueOptions.folderOptions)) {
            return std::move(*typed);
        }
        if (auto problem = valueProblem(type, valueType, form, valueOptions, valueOptions.options, *typed); !problem.empty()) {
            return fail(Kind::NotValue, std::move(problem));
        }
        return std::move(*typed);
    }
}
