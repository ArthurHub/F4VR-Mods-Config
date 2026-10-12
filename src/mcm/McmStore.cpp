#include "McmStore.h"

#include <cmath>
#include <format>
#include <variant>

using namespace mods_config;

namespace
{
    /**
     * The value a setting has with none: a switcher is off, a slider is at its least, and a stepper and a
     * dropdown are at their first option.
     */
    model::Value valueWithNone(const model::Setting& setting)
    {
        switch (setting.type) {
        case model::SettingType::Switcher:
            return false;
        case model::SettingType::Slider:
            return setting.valueOptions.min;
        case model::SettingType::Stepper:
        case model::SettingType::Dropdown:
            return setting.valueOptions.options.empty() ? model::Value(0) : setting.valueOptions.options.front().value;
        case model::SettingType::TextInput:
            break;
        }
        return std::string();
    }

    /**
     * The number of a global as the value of a setting, as MCM's menu reads it: a switcher is on at 1 and
     * off at any other number, and a stepper and a dropdown have the option whose place is the number.
     * None for a setting that keeps no number.
     */
    std::optional<model::Value> fromNumber(const model::Setting& setting, const float number)
    {
        switch (setting.type) {
        case model::SettingType::Switcher:
            return number == 1.0f;
        case model::SettingType::Slider:
            return number;
        case model::SettingType::Stepper:
        case model::SettingType::Dropdown:
            return static_cast<int>(std::lround(number));
        case model::SettingType::TextInput:
            break;
        }
        return std::nullopt;
    }

    /**
     * A value as the number a global keeps it as: 1 and 0 for a switcher. None for a text.
     */
    std::optional<float> toNumber(const model::Value& value)
    {
        if (const auto* on = std::get_if<bool>(&value)) {
            return *on ? 1.0f : 0.0f;
        }
        if (const auto* whole = std::get_if<int>(&value)) {
            return static_cast<float>(*whole);
        }
        if (const auto* number = std::get_if<float>(&value)) {
            return *number;
        }
        return std::nullopt;
    }
}

namespace mods_config::mcm
{
    McmStore::McmStore(Globals& globals)
        : _globals(globals)
    {}

    /**
     * Starts the session of a mod: the values the store held for the mod before it are forgotten.
     */
    void McmStore::open(const model::Mod&)
    {
        _held.clear();
    }

    /**
     * The value of a setting, read from its storage now: another mod or the console can change a global at
     * any time. A global that is not in the game is not available, with the reason, and neither is a value
     * of a storage the store does not read.
     */
    model::StoredValue McmStore::value(const model::Setting& setting)
    {
        if (std::holds_alternative<model::NoStorage>(setting.storage)) {
            const auto held = _held.find(&setting);
            return { .value = held != _held.end() ? held->second : valueWithNone(setting), .available = true, .reason = {} };
        }
        if (const auto* global = std::get_if<model::GlobalStorage>(&setting.storage)) {
            const auto number = _globals.get(*global);
            if (!number) {
                return { .value = valueWithNone(setting),
                    .available = false,
                    .reason = std::format("Not available: the global {:X} of {} is not in the game.", global->formId, global->plugin) };
            }
            if (auto value = fromNumber(setting, *number)) {
                return { .value = std::move(*value), .available = true, .reason = {} };
            }
        }
        return { .value = valueWithNone(setting), .available = false, .reason = {} };
    }

    /**
     * Writes a value where the setting's storage says, at once. Returns false for a value that is not
     * written: one of a storage the store does not write, or of a global that is not in the game.
     */
    bool McmStore::set(const model::Setting& setting, const model::Value& value)
    {
        if (std::holds_alternative<model::NoStorage>(setting.storage)) {
            _held.insert_or_assign(&setting, value);
            return true;
        }
        if (const auto* global = std::get_if<model::GlobalStorage>(&setting.storage)) {
            const auto number = toNumber(value);
            if (number && _globals.set(*global, *number)) {
                logger::debug("Set the global {:X} of {} to {} ('{}')", global->formId, global->plugin, *number, setting.text);
                return true;
            }
        }
        return false;
    }
}
