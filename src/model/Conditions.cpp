#include "Conditions.h"

#include <optional>
#include <variant>

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
     * Whether a condition holds, and none where a value that would tell is not available. So a setting that
     * is off hides a row that needs it and another whose value is not available, and a setting that is on
     * shows a row that needs it or such another.
     */
    std::optional<bool> evaluate(const model::Condition& condition, model::Store& store)
    {
        using Kind = model::Condition::Kind;
        if (condition.kind == Kind::On) {
            const auto stored = store.valueById(condition.setting);
            return stored.available ? std::optional(isOn(stored.value)) : std::nullopt;
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
     */
    bool holds(const Condition& condition, Store& store)
    {
        return evaluate(condition, store).value_or(true);
    }
}
