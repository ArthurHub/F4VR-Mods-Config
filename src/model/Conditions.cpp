#include "Conditions.h"

#include <algorithm>
#include <cctype>
#include <optional>
#include <variant>

#include "SettingValues.h"
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
     * Whether a comparison holds on the value of its setting. A value below or above another is a number:
     * a comparison of a bool or a text by them does not hold.
     */
    bool compare(const model::Condition& condition, const model::Value& value)
    {
        using Kind = model::Condition::Kind;
        const bool isOneOf = std::ranges::any_of(condition.values, [&value](const model::Value& other) {
            return model::sameValue(value, other);
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
     * available, as MCM's menu reads a hidden switcher. A state of the mod's client and a setting of the game's INI are
     * asked of the store as a setting's value is. With no store no value is available.
     */
    std::optional<bool> evaluate(const model::Condition& condition, model::StoreReader* store)
    {
        using Kind = model::Condition::Kind;
        using Source = model::Condition::Source;
        if (condition.kind != Kind::All && condition.kind != Kind::Any && condition.kind != Kind::Not) {
            const auto stored = !store                                       ? model::StoredValue{}
                                : condition.source == Source::ClientState    ? store->clientState(condition.setting)
                                : condition.source == Source::GameIniSetting ? store->gameIniSetting(condition.setting)
                                                                             : store->valueById(condition.setting);
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

    /**
     * Adds the ids of the settings a condition names, all of them, once each: a state of the mod's client and a setting
     * of the game's INI are none.
     */
    void addAllSettings(const model::Condition& condition, std::vector<std::string>& ids)
    {
        using Kind = model::Condition::Kind;
        if (condition.kind == Kind::All || condition.kind == Kind::Any || condition.kind == Kind::Not) {
            for (const auto& inner : condition.conditions) {
                addAllSettings(inner, ids);
            }
        } else if (condition.source == model::Condition::Source::Setting && std::ranges::find(ids, condition.setting) == ids.end()) {
            ids.push_back(condition.setting);
        }
    }

    /**
     * Adds the ids of the settings whose values decide a condition now, once each, as `decidingSettings` has them.
     */
    void addDecidingSettings(const model::Condition& condition, model::StoreReader* store, std::vector<std::string>& ids)
    {
        using Kind = model::Condition::Kind;
        // a comparison names its one setting, and a Not is decided by every setting under it
        if (condition.kind != Kind::All && condition.kind != Kind::Any) {
            addAllSettings(condition, ids);
            return;
        }
        // an All that holds and an Any that does not are decided by all their conditions, an All that does not by
        // those that do not hold, and an Any that holds by those that hold
        const bool holds = model::holds(condition, store);
        const bool byAll = holds == (condition.kind == Kind::All);
        for (const auto& inner : condition.conditions) {
            if (byAll || model::holds(inner, store) == holds) {
                addDecidingSettings(inner, store, ids);
            }
        }
    }
}

namespace mods_config::model
{
    /**
     * Whether a condition holds on the values the store has now. One that a value which is not available
     * leaves open holds: its row is shown, so nothing is hidden for a reason the player cannot change.
     * With no store no value is available.
     */
    bool holds(const Condition& condition, StoreReader* store)
    {
        return evaluate(condition, store).value_or(true);
    }

    /**
     * Whether a condition holds on the values the store has now, and does not where a value that is not
     * available leaves it open: a warning is not told of a value the player cannot see.
     */
    bool surelyHolds(const Condition& condition, StoreReader* store)
    {
        return evaluate(condition, store).value_or(false);
    }

    /**
     * The ids of the settings whose values decide a condition now, in the order it names them: the setting of a
     * comparison, every setting under a Not, and of an All and an Any those of the conditions that settle it, so a
     * change of one of them can turn it. A state of the mod's client and a setting of the game's INI are none.
     */
    std::vector<std::string> decidingSettings(const Condition& condition, StoreReader* store)
    {
        std::vector<std::string> ids;
        addDecidingSettings(condition, store, ids);
        return ids;
    }

    /**
     * The type of a setting of the game's INI, from the first letter of its name as the game reads it: `b` a bool,
     * `i` and `u` an int, `f` a float, and `s` a string, in either case. None for another letter, as a color's `r`
     * and `a`, which a condition does not compare.
     */
    std::optional<StoredType> gameIniSettingType(const std::string_view name)
    {
        switch (name.empty() ? '\0' : static_cast<char>(std::tolower(static_cast<unsigned char>(name.front())))) {
        case 'b':
            return StoredType::Bool;
        case 'i':
        case 'u':
            return StoredType::Int;
        case 'f':
            return StoredType::Float;
        case 's':
            return StoredType::String;
        default:
            return std::nullopt;
        }
    }
}
