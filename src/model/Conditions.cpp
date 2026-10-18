#include "Conditions.h"

#include <algorithm>
#include <optional>
#include <variant>

#include "common/CommonUtils.h"

using namespace mods_config;

namespace
{
    /**
     * Whether a value is on: a switcher that is on, and a number that is 1, as MCM's menu has a group on
     * while the value of its control is 1. A text is never on.
     */
    bool isOn(const model::Value& value)
    {
        if (const auto* on = std::get_if<bool>(&value)) {
            return *on;
        }
        if (const auto* whole = std::get_if<int>(&value)) {
            return *whole == 1;
        }
        const auto* number = std::get_if<float>(&value);
        return number && *number == 1.0f;
    }

    /**
     * A value as a number: an int or a float. None for a bool and a text.
     */
    std::optional<float> numberOf(const model::Value& value)
    {
        if (const auto* whole = std::get_if<int>(&value)) {
            return static_cast<float>(*whole);
        }
        const auto* number = std::get_if<float>(&value);
        return number ? std::optional(*number) : std::nullopt;
    }

    /**
     * Whether two values are the same: two of one type that are equal, and two numbers, an int and a float
     * too, that are as near as a number and the text of it in a file. A text is never a number.
     */
    bool isEqual(const model::Value& value, const model::Value& other)
    {
        if (std::holds_alternative<int>(value) && std::holds_alternative<int>(other)) {
            return value == other;
        }
        const auto number = numberOf(value);
        const auto otherNumber = numberOf(other);
        if (number && otherNumber) {
            return common::fEqual(*number, *otherNumber);
        }
        return value == other;
    }

    /**
     * Whether a comparison holds on the value of its setting. A value below or above another is a number:
     * a comparison of a bool or a text by them does not hold.
     */
    bool compare(const model::Condition& condition, const model::Value& value)
    {
        using Kind = model::Condition::Kind;
        const bool isOneOf = std::ranges::any_of(condition.values, [&value](const model::Value& other) {
            return isEqual(value, other);
        });
        if (condition.kind == Kind::Equals || condition.kind == Kind::OneOf) {
            return isOneOf;
        }
        if (condition.kind == Kind::NotEquals || condition.kind == Kind::NotOneOf) {
            return !isOneOf;
        }

        const auto number = numberOf(value);
        const auto other = condition.values.empty() ? std::nullopt : numberOf(condition.values.front());
        if (!number || !other) {
            return false;
        }
        const bool equal = common::fEqual(*number, *other);
        switch (condition.kind) {
        case Kind::LessThan:
            return *number < *other && !equal;
        case Kind::LessThanOrEqual:
            return *number < *other || equal;
        case Kind::GreaterThan:
            return *number > *other && !equal;
        case Kind::GreaterThanOrEqual:
            return *number > *other || equal;
        default:
            return false;
        }
    }

    /**
     * Whether a condition holds, and none where a value that would tell is not available. So a setting that
     * is off hides a row that needs it and another whose value is not available, and a setting that is on
     * shows a row that needs it or such another. The value of a setting no row shows is off where it is not
     * available, as MCM's menu reads a hidden switcher. With no store no value is available.
     */
    std::optional<bool> evaluate(const model::Condition& condition, model::Store* store)
    {
        using Kind = model::Condition::Kind;
        if (condition.kind != Kind::All && condition.kind != Kind::Any && condition.kind != Kind::Not) {
            const auto stored = store ? store->valueById(condition.setting) : model::StoredValue{};
            if (!stored.available) {
                return condition.offWhenNotAvailable ? std::optional(false) : std::nullopt;
            }
            return condition.kind == Kind::On ? isOn(stored.value) : compare(condition, stored.value);
        }
        if (condition.kind == Kind::Not) {
            const auto inner = condition.conditions.empty() ? std::nullopt : evaluate(condition.conditions.front(), store);
            return inner ? std::optional(!*inner) : std::nullopt;
        }

        // an All is settled by one that does not hold, and an Any by one that holds
        const bool settling = condition.kind == Kind::Any;
        bool known = true;
        for (const auto& inner : condition.conditions) {
            const auto result = evaluate(inner, store);
            if (result == settling) {
                return settling;
            }
            known = known && result.has_value();
        }
        return known ? std::optional(!settling) : std::nullopt;
    }
}

namespace mods_config::model
{
    /**
     * Whether a condition holds on the values the store has now. One that a value which is not available
     * leaves open holds: its row is shown, so nothing is hidden for a reason the player cannot change.
     * With no store no value is available.
     */
    bool holds(const Condition& condition, Store* store)
    {
        return evaluate(condition, store).value_or(true);
    }
}
