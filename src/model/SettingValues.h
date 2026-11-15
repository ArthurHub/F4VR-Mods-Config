#pragma once

#include <cstdint>
#include <expected>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json_fwd.hpp>

#include "ConfigDefinition.h"

// The setting value rules: what a value of each setting type is, answered by setting type for any setting. Whether two
// values are the same, a value's canonical text, why a value is no value of its setting, how a value is read from a
// config spec's JSON, and whether a type is a choice or kept as a table. Each answer is one switch over the setting
// types, which asks the type's own module (Colors, Transforms, InputBindings, Haptics) for the rules of its values.
namespace mods_config::model
{
    /**
     * Why a JSON value is no value of a setting, for its reader to report in its own sentence.
     */
    struct ValueReadError
    {
        enum class Kind : std::uint8_t
        {
            // a JSON value that is not of the setting's type: no object for a table, no text for a haptic, and no
            // value of the `valueType` of any other
            NotOfType,

            // a value of the type that is no value of the setting: with the framework's reason for a table, no reason
            // for a haptic, and the setting's problem for any other
            NotValue,

            // a binding of a hand, a way or a button the setting does not take, with which of them
            Untaken,

            // a binding that turns `accelerate` off, which the line of an INI file cannot have
            IniAccelerate,
        };

        Kind kind = Kind::NotOfType;
        std::string reason;
    };

    bool isFrameworkType(SettingType type);
    std::string_view valueKindName(SettingType type);
    bool isTableType(SettingType type);
    bool keptAsTable(const Setting& setting);
    bool isChoice(SettingType type);
    bool isNumberField(SettingType type, StoredType valueType);
    bool isNumberField(const Setting& setting);
    std::optional<float> fieldNumber(StoredType valueType, std::string_view text);
    bool sameValue(const Value& value, const Value& other);
    std::optional<Value> canonicalValue(SettingType type, TableForm form, const Value& value);
    bool isControlValue(SettingType type, const Value& value);
    std::string valueProblem(SettingType type, StoredType valueType, TableForm form, const ValueOptions& valueOptions, const std::vector<Option>& options, const Value& value);
    std::string valueProblem(const Setting& setting, const std::vector<Option>& options, const Value& value);
    std::optional<Value> jsonValue(const nlohmann::json& value, StoredType valueType);
    std::string tomlText(const nlohmann::json& value);
    std::expected<Value, ValueReadError> readJsonValue(SettingType type, StoredType valueType, TableForm form, const ValueOptions& valueOptions, const nlohmann::json& value);
}
