#include "ConditionSpec.h"

#include <algorithm>
#include <array>
#include <format>
#include <utility>

#include "common/CommonUtils.h"
#include "model/Conditions.h"
#include "model/SettingValues.h"

using namespace std::string_literals;

namespace mods_config::spec
{
    namespace
    {
        // the fields of a warning of a setting or a button, as the schema has them (docs/design/schema.md)
        constexpr std::array<std::string_view, 2> WARNING_FIELDS{ "when", "text" };

        using Kind = model::Condition::Kind;
        using OperatorName = std::pair<std::string_view, Kind>;

        // what a condition compares a setting's value by, by its key
        constexpr std::array OPERATORS{
            OperatorName{ "equals", Kind::Equals },
            OperatorName{ "notEquals", Kind::NotEquals },
            OperatorName{ "lessThan", Kind::LessThan },
            OperatorName{ "lessThanOrEqual", Kind::LessThanOrEqual },
            OperatorName{ "greaterThan", Kind::GreaterThan },
            OperatorName{ "greaterThanOrEqual", Kind::GreaterThanOrEqual },
            OperatorName{ "oneOf", Kind::OneOf },
            OperatorName{ "notOneOf", Kind::NotOneOf },
        };

        /**
         * Whether a JSON value can be compared with the value of a setting of a type: a bool with a bool, a
         * number with an int and a float, and a text with a string. There is no conversion between them.
         */
        bool isComparable(const Json& value, const model::StoredType type)
        {
            switch (type) {
            case model::StoredType::Bool:
                return value.is_boolean();
            case model::StoredType::Int:
            case model::StoredType::Float:
                return value.is_number();
            default:
                return value.is_string();
            }
        }

        /**
         * A JSON value that a condition compares with, in its own type: a whole number is an int, and any other
         * number a float.
         */
        model::Value conditionValue(const Json& value)
        {
            if (value.is_boolean()) {
                return value.get<bool>();
            }
            if (value.is_number_integer()) {
                return value.get<int>();
            }
            if (value.is_number()) {
                return value.get<float>();
            }
            return value.get<std::string>();
        }

        /**
         * A condition on the value of a setting: its `setting` and one operator, as `{ "setting": "iMode",
         * "oneOf": [1, 2] }`. It names the setting by its id in any case, and has the id as the config spec gives it.
         * Or the reason it is not one: a setting the config spec does not have, a value of another type than the
         * setting's, an order of a value that is no number, and a `oneOf` or a `notOneOf` of a setting that is
         * neither a text nor a choice.
         */
        std::expected<model::Condition, std::string> loadComparison(const Json& condition, const ConfigSpecReading& reading)
        {
            const auto named = getText(condition, "setting");
            const auto found = reading.settings.find(common::str_tolower(named));
            if (found == reading.settings.end()) {
                return std::unexpected(std::format("it names no setting '{}'", named));
            }
            const auto& setting = found->second;
            const auto op = std::ranges::find_if(OPERATORS, [&condition](const OperatorName& name) {
                return condition.contains(name.first);
            });
            if (op == OPERATORS.end()) {
                return std::unexpected(std::format("it has no operator for the setting '{}'", setting.id));
            }

            const auto kind = op->second;
            const bool isList = kind == Kind::OneOf || kind == Kind::NotOneOf;
            const bool isOrder = kind != Kind::Equals && kind != Kind::NotEquals && !isList;
            const bool isNumber = setting.valueType == model::StoredType::Int || setting.valueType == model::StoredType::Float;
            if (isOrder && !isNumber) {
                return std::unexpected(std::format("{} compares numbers, and '{}' is no number", op->first, setting.id));
            }
            if (isList && setting.valueType != model::StoredType::String && !model::isChoice(setting.type)) {
                return std::unexpected(std::format("{} is of a text or a choice, and '{}' is neither", op->first, setting.id));
            }
            const auto& operand = condition.at(op->first);
            if (isList && !operand.is_array()) {
                return std::unexpected(std::format("its {} is not a list", op->first));
            }

            model::Condition comparison{ .kind = kind, .setting = setting.id, .conditions = {}, .offWhenNotAvailable = false, .values = {} };
            for (const auto& value : isList ? operand : Json::array({ operand })) {
                if (!isComparable(value, setting.valueType)) {
                    return std::unexpected(std::format("its {} has a value that is not of the type of '{}'", op->first, setting.id));
                }
                comparison.values.push_back(conditionValue(value));
            }
            return comparison;
        }

        /**
         * A condition on a state of the mod's client: its `clientState`, and one operator, as `{ "clientState":
         * "calibrating", "equals": false }`, or none, which holds while the state is on. Its type is the client's, so a
         * value of any type is compared, but an order is of a number and a `oneOf` and a `notOneOf` of a list. Or the
         * reason it is not one.
         */
        std::expected<model::Condition, std::string> loadClientState(const Json& condition)
        {
            const auto named = condition.at("clientState");
            if (!named.is_string() || named.get_ref<const std::string&>().empty()) {
                return std::unexpected("its clientState names no state"s);
            }
            model::Condition state{ .kind = Kind::On,
                .setting = named.get<std::string>(),
                .conditions = {},
                .offWhenNotAvailable = false,
                .values = {},
                .source = model::Condition::Source::ClientState };
            if (condition.size() == 1) {
                return state;
            }
            const auto op = std::ranges::find_if(OPERATORS, [&condition](const OperatorName& name) {
                return condition.contains(name.first);
            });
            if (op == OPERATORS.end()) {
                return std::unexpected(std::format("it has no operator for the state '{}'", state.setting));
            }
            state.kind = op->second;
            const bool isList = state.kind == Kind::OneOf || state.kind == Kind::NotOneOf;
            const bool isOrder = state.kind != Kind::Equals && state.kind != Kind::NotEquals && !isList;
            const auto& operand = condition.at(op->first);
            if (isList && !operand.is_array()) {
                return std::unexpected(std::format("its {} is not a list", op->first));
            }
            for (const auto& value : isList ? operand : Json::array({ operand })) {
                if (!value.is_boolean() && !value.is_number() && !value.is_string()) {
                    return std::unexpected(std::format("its {} has a value that is not a bool, a number or a text", op->first));
                }
                if (isOrder && !value.is_number()) {
                    return std::unexpected(std::format("{} compares numbers, and its value is no number", op->first));
                }
                state.values.push_back(conditionValue(value));
            }
            return state;
        }

        /**
         * A condition on a setting of the game's INI: its `gameIniSetting`, by the name the game gives it, and one operator,
         * as `{ "gameIniSetting": "iShadowMapResolution:Display", "lessThan": 2048 }`, or none, which holds while it is on.
         * Its type is told by the first letter of its name, so it is checked as a setting's is: an order is of a number,
         * a `oneOf` and a `notOneOf` of a text or a whole number, and a value of another type is not compared. Or the reason
         * it is not one. Whether the game has the setting is asked as the page is drawn.
         */
        std::expected<model::Condition, std::string> loadGameIniSetting(const Json& condition)
        {
            const auto named = condition.at("gameIniSetting");
            if (!named.is_string() || named.get_ref<const std::string&>().empty()) {
                return std::unexpected("its gameIniSetting names no setting"s);
            }
            const auto& name = named.get_ref<const std::string&>();
            const auto type = model::gameIniSettingType(name);
            if (!type) {
                return std::unexpected(std::format("its gameIniSetting '{}' is of no type a condition compares: the name of one starts with b, i, u, f or s", name));
            }
            model::Condition setting{ .kind = Kind::On,
                .setting = name,
                .conditions = {},
                .offWhenNotAvailable = false,
                .values = {},
                .source = model::Condition::Source::GameIniSetting };
            if (condition.size() == 1) {
                return setting;
            }
            const auto op = std::ranges::find_if(OPERATORS, [&condition](const OperatorName& operatorName) {
                return condition.contains(operatorName.first);
            });
            if (op == OPERATORS.end()) {
                return std::unexpected(std::format("it has no operator for the game's INI setting '{}'", name));
            }
            setting.kind = op->second;
            const bool isList = setting.kind == Kind::OneOf || setting.kind == Kind::NotOneOf;
            const bool isOrder = setting.kind != Kind::Equals && setting.kind != Kind::NotEquals && !isList;
            if (isOrder && *type != model::StoredType::Int && *type != model::StoredType::Float) {
                return std::unexpected(std::format("{} compares numbers, and '{}' is no number", op->first, name));
            }
            if (isList && *type != model::StoredType::String && *type != model::StoredType::Int) {
                return std::unexpected(std::format("{} is of a text or a whole number, and '{}' is neither", op->first, name));
            }
            const auto& operand = condition.at(op->first);
            if (isList && !operand.is_array()) {
                return std::unexpected(std::format("its {} is not a list", op->first));
            }
            for (const auto& value : isList ? operand : Json::array({ operand })) {
                if (!isComparable(value, *type)) {
                    return std::unexpected(std::format("its {} has a value that is not of the type of '{}'", op->first, name));
                }
                setting.values.push_back(conditionValue(value));
            }
            return setting;
        }

        /**
         * A condition of a config spec, or the reason it is not one: a comparison of a setting's value, `all` and
         * `any` of a list of conditions, `not` of one, a state of the mod's client, a setting of the game's INI, or whether
         * a `plugin` or an `f4sePlugin` is loaded.
         * Whether a plugin is loaded does not change while the game runs, so it is answered here, as an `all` of
         * none, which holds, or an `any` of none, which does not.
         * A condition with a part that is not one is not one as a whole.
         */
        std::expected<model::Condition, std::string> loadCondition(const Json& condition, const ConfigSpecReading& reading)
        {
            if (!condition.is_object()) {
                return std::unexpected("a condition is not an object"s);
            }
            if (condition.size() == 2 && condition.contains("setting")) {
                return loadComparison(condition, reading);
            }
            if (condition.size() <= 2 && condition.contains("clientState")) {
                return loadClientState(condition);
            }
            if (condition.size() <= 2 && condition.contains("gameIniSetting")) {
                return loadGameIniSetting(condition);
            }
            if (condition.size() != 1) {
                return std::unexpected(std::format("{} is not one condition", condition.dump()));
            }

            const auto first = condition.begin();
            const auto& key = first.key();
            const auto& value = first.value();
            if (key == "all" || key == "any") {
                if (!value.is_array()) {
                    return std::unexpected(std::format("its {} is not a list", key));
                }
                model::Condition joined{ .kind = key == "all" ? Kind::All : Kind::Any, .setting = {}, .conditions = {} };
                for (const auto& inner : value) {
                    auto loaded = loadCondition(inner, reading);
                    if (!loaded) {
                        return loaded;
                    }
                    joined.conditions.push_back(std::move(*loaded));
                }
                return joined;
            }
            if (key == "not") {
                auto inner = loadCondition(value, reading);
                if (!inner) {
                    return inner;
                }
                return model::Condition{ .kind = Kind::Not, .setting = {}, .conditions = { std::move(*inner) } };
            }
            if (key == "plugin" || key == "f4sePlugin") {
                if (!value.is_string() || value.get_ref<const std::string&>().empty()) {
                    return std::unexpected(std::format("its {} names no plugin", key));
                }
                const auto& check = key == "plugin" ? reading.plugins.isGamePluginLoaded : reading.plugins.isF4sePluginLoaded;
                const bool loaded = !check || check(value.get_ref<const std::string&>());
                return model::Condition{ .kind = loaded ? Kind::All : Kind::Any, .setting = {}, .conditions = {} };
            }
            if (key == "setting") {
                return std::unexpected(std::format("it has no operator for the setting '{}'", value.is_string() ? value.get<std::string>() : value.dump()));
            }
            return std::unexpected(std::format("'{}' is no condition", key));
        }
    }

    /**
     * The condition of a key of a row, as `visibleWhen`. None for a row with none, and for one that is not a
     * condition, which is reported: the row is then shown and enabled as if it had none, so the mistake is
     * seen and nothing is hidden by it.
     */
    std::optional<model::Condition> loadRowCondition(const Json& row, const char* key, ConfigSpecReading& reading)
    {
        const auto found = row.find(key);
        if (found == row.end()) {
            return std::nullopt;
        }
        auto condition = loadCondition(*found, reading);
        if (!condition) {
            const Into into(reading, key);
            reading.report(model::Severity::Warning, std::format("Its {} is left out: {}. The row is shown and enabled as if it had none.", key, condition.error()));
            return std::nullopt;
        }
        return std::move(*condition);
    }

    /**
     * The `warnings` of a setting or a button: a text each, which the row tells while its `when` holds. One
     * with no text, and one whose `when` is not a condition, is left out, and is reported.
     */
    std::vector<model::Warning> loadWarnings(const Json& row, ConfigSpecReading& reading)
    {
        std::vector<model::Warning> warnings;
        const auto found = row.find("warnings");
        if (found == row.end()) {
            return warnings;
        }
        const Into into(reading, "warnings");
        if (!found->is_array()) {
            reading.report(model::Severity::Error, "Its warnings are not a list: they are left out.");
            return warnings;
        }
        for (std::size_t place = 0; place < found->size(); ++place) {
            const auto& warning = (*found)[place];
            const Into intoWarning(reading, "", place);
            checkFields(warning, WARNING_FIELDS, reading);
            auto text = warning.is_object() ? readShownText(warning, "text", reading) : std::string();
            if (text.empty()) {
                reading.report(model::Severity::Error, std::format("Warning {} has no text: it is left out.", place + 1));
                continue;
            }
            const auto when = warning.find("when");
            auto condition = when != warning.end() ? loadCondition(*when, reading) : std::unexpected("it has no when"s);
            if (!condition) {
                reading.report(model::Severity::Error, std::format("Warning {} is left out: {}.", place + 1, condition.error()));
                continue;
            }
            warnings.push_back(model::Warning{ .when = std::move(*condition), .text = std::move(text) });
        }
        return warnings;
    }

    /**
     * A condition that needs both: an All of the two, or the one there is.
     */
    std::optional<model::Condition> joinConditions(const std::optional<model::Condition>& above, std::optional<model::Condition> own)
    {
        if (!above || !own) {
            return above ? above : own;
        }
        return model::Condition{ .kind = Kind::All, .setting = {}, .conditions = { *above, std::move(*own) } };
    }
}
