#include "McmStore.h"

#include <charconv>
#include <cmath>
#include <format>
#include <variant>

#include "common/CommonUtils.h"

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

    /**
     * The text of a ModSetting as the number MCM reads it as: a bool is off at `0` and on at any other
     * text, and a whole number and a number are read up to what is no part of one. None for a text that
     * starts with no number, and for one MCM keeps as a text.
     */
    std::optional<float> modSettingNumber(const model::ModSettingStorage::Type type, const std::string& text)
    {
        using Type = model::ModSettingStorage::Type;
        const auto* end = text.data() + text.size();
        switch (type) {
        case Type::Bool:
            return text != "0" ? 1.0f : 0.0f;
        case Type::Int: {
            int whole = 0;
            return std::from_chars(text.data(), end, whole).ec == std::errc() ? std::optional(static_cast<float>(whole)) : std::nullopt;
        }
        case Type::Float: {
            float number = 0.0f;
            return std::from_chars(text.data(), end, number).ec == std::errc() ? std::optional(number) : std::nullopt;
        }
        case Type::String:
            break;
        }
        return std::nullopt;
    }

    /**
     * The value of a setting that is a ModSetting, from the text MCM's files have for it. A text input has
     * the text as it is written. One that is in neither file is not available, and neither is one whose
     * text is no number for a setting that keeps one, with the reason.
     */
    model::StoredValue modSettingValue(const model::Setting& setting, const model::ModSettingStorage& modSetting, const std::string* text)
    {
        if (!text) {
            return { .value = valueWithNone(setting),
                .available = false,
                .reason = std::format("Not available: the ModSetting {} is in neither of MCM's files.", modSetting.name) };
        }
        if (setting.type == model::SettingType::TextInput) {
            return { .value = *text, .available = true, .reason = {} };
        }
        if (const auto number = modSettingNumber(modSetting.type, *text)) {
            if (auto value = fromNumber(setting, *number)) {
                return { .value = std::move(*value), .available = true, .reason = {} };
            }
        }
        return { .value = valueWithNone(setting),
            .available = false,
            .reason = std::format("Not available: the ModSetting {} is '{}' in MCM's files, which is no number.", modSetting.name, *text) };
    }

    /**
     * A value as the type its storage keeps it in, which MCM's function and a script's property take: a bool
     * is on at any number but 0, and a whole number is the nearest one. None for a number as a text, and a
     * text as a number.
     */
    std::optional<model::Value> typedValue(const model::StoredType type, const model::Value& value)
    {
        using Type = model::StoredType;
        if (type == Type::String) {
            return std::holds_alternative<std::string>(value) ? std::optional(value) : std::nullopt;
        }
        const auto number = toNumber(value);
        if (!number) {
            return std::nullopt;
        }
        switch (type) {
        case Type::Bool:
            return *number != 0.0f;
        case Type::Int:
            return static_cast<int>(std::lround(*number));
        default:
            return *number;
        }
    }

    /**
     * The value of a setting that is a property of a script, from the value the script has for it. A bool is
     * on at any number but 0, as MCM's menu reads it, and a text input has a number as its text. One that is
     * not in the game is not available, and neither is a text for a setting that keeps a number, with the reason.
     */
    model::StoredValue propertyValue(const model::Setting& setting, const model::PropertyStorage& property, const std::optional<model::Value>& stored)
    {
        if (!stored) {
            return { .value = valueWithNone(setting),
                .available = false,
                .reason = std::format("Not available: the property {} of the form {:X} of {} is not in the game.", property.name, property.formId, property.plugin) };
        }
        const auto number = toNumber(*stored);
        if (setting.type == model::SettingType::TextInput) {
            const auto* text = std::get_if<std::string>(&*stored);
            return { .value = text ? *text : std::format("{}", *number), .available = true, .reason = {} };
        }
        if (number) {
            const auto isBool = property.type == model::StoredType::Bool;
            if (auto value = fromNumber(setting, isBool && common::fNotEqual(*number, 0.0f) ? 1.0f : *number)) {
                return { .value = std::move(*value), .available = true, .reason = {} };
            }
        }
        return { .value = valueWithNone(setting),
            .available = false,
            .reason = std::format("Not available: the property {} of the form {:X} of {} is a text, which is no number.", property.name, property.formId, property.plugin) };
    }

    /**
     * Adds the settings of the items to the map, each by its id, with those of the groups among them.
     */
    void addSettings(const std::vector<model::Item>& items, std::map<std::string, const model::Setting*, std::less<>>& settings)
    {
        for (const auto& item : items) {
            if (const auto* setting = std::get_if<model::Setting>(&item)) {
                settings.try_emplace(setting->id, setting);
            } else if (const auto* group = std::get_if<model::Group>(&item)) {
                addSettings(group->items, settings);
            }
        }
    }
}

namespace mods_config::mcm
{
    McmStore::McmStore(std::filesystem::path mcmFolder)
        : _mcmFolder(std::move(mcmFolder))
    {}

    /**
     * Tells the scripts that the menu is opened, as MCM's menu does with `OnMCMOpen`.
     */
    void McmStore::menuOpened()
    {
        sendEvent("OnMCMOpen");
    }

    /**
     * Tells the scripts that the menu is closed, as MCM's menu does with `OnMCMClose`. The menu ends the
     * session of its mod before it.
     */
    void McmStore::menuClosed()
    {
        sendEvent("OnMCMClose");
    }

    /**
     * Starts the session of a mod, after it ends the one of the mod before it: the settings of an id are the
     * mod's. Its ModSettings are read from MCM's files now, and not again while it is selected: MCM writes
     * the player's file on every set, so the files have what MCM has.
     * The scripts are told that the mod is shown, as MCM's menu tells them for a press on the mod's row.
     */
    void McmStore::open(const model::Mod& mod)
    {
        close();
        _modName = mod.name;
        _modSettings.load(_mcmFolder, mod.name);
        logger::debug("Read {} ModSettings of '{}' from MCM's files", _modSettings.size(), mod.name);
        addSettings(mod.frontPage.items, _settings);
        for (const auto& page : mod.pages) {
            addSettings(page.items, _settings);
        }
        sendEvent("OnMCMMenuOpen");
        sendEvent("OnMCMMenuOpen|" + _modName);
    }

    /**
     * Ends the session of the mod: its values are forgotten, and the scripts are told that it is left, which
     * they are not with no mod.
     * It does not read the mod, which can be gone by now: the menu's mods are replaced before it is called.
     */
    void McmStore::close()
    {
        if (!_modName.empty()) {
            sendEvent("OnMCMMenuClose|" + _modName);
        }
        _modName.clear();
        _modSettings.load({}, {});
        _held.clear();
        _settings.clear();
    }

    /**
     * The value of a setting, read from its storage now: another mod or the console can change a global at
     * any time. A ModSetting is the one of the mod that is selected, as its files had it then or as it was
     * set since.
     * A property of a script is read from the script now too.
     * A global that is not in the game is not available, with the reason, and neither is a ModSetting that
     * is in no file, a property that is not in the game, and a value of a storage the store does not read.
     */
    model::StoredValue McmStore::value(const model::Setting& setting)
    {
        if (std::holds_alternative<model::NoStorage>(setting.storage)) {
            const auto held = _held.find(&setting);
            return { .value = held != _held.end() ? held->second : valueWithNone(setting), .available = true, .reason = {} };
        }
        if (const auto* global = std::get_if<model::GlobalStorage>(&setting.storage)) {
            const auto number = getGlobal(*global);
            if (!number) {
                return { .value = valueWithNone(setting),
                    .available = false,
                    .reason = std::format("Not available: the global {:X} of {} is not in the game.", global->formId, global->plugin) };
            }
            if (auto value = fromNumber(setting, *number)) {
                return { .value = std::move(*value), .available = true, .reason = {} };
            }
        }
        if (const auto* modSetting = std::get_if<model::ModSettingStorage>(&setting.storage)) {
            return modSettingValue(setting, *modSetting, _modSettings.find(modSetting->name));
        }
        if (const auto* property = std::get_if<model::PropertyStorage>(&setting.storage)) {
            return propertyValue(setting, *property, getProperty(*property));
        }
        return { .value = valueWithNone(setting), .available = false, .reason = {} };
    }

    /**
     * The value of the setting of an id, which a condition names its setting by.
     */
    model::StoredValue McmStore::valueById(const std::string_view id)
    {
        const auto found = _settings.find(id);
        return found != _settings.end() ? value(*found->second) : model::StoredValue{};
    }

    /**
     * Sets a value, at once, and then tells the scripts that the setting changed, as MCM's menu does: with
     * the name of the mod and the id of the row, for a row that has one in its config.
     * Returns false for a value that is not written, which no script is told of.
     */
    bool McmStore::set(const model::Setting& setting, const model::Value& value)
    {
        if (!write(setting, value)) {
            return false;
        }
        if (!setting.sourceId.empty()) {
            sendEvent("OnMCMSettingChange", { _modName, setting.sourceId });
            sendEvent("OnMCMSettingChange|" + _modName, { _modName, setting.sourceId });
        }
        return true;
    }

    /**
     * Writes a value where the setting's storage says. Returns false for a value that is not written: one of
     * a storage the store does not write, of a global or a property that is not in the game, or of a
     * ModSetting that is in neither of MCM's files, which MCM does not have.
     */
    bool McmStore::write(const model::Setting& setting, const model::Value& value)
    {
        if (std::holds_alternative<model::NoStorage>(setting.storage)) {
            _held.insert_or_assign(&setting, value);
            return true;
        }
        if (const auto* global = std::get_if<model::GlobalStorage>(&setting.storage)) {
            const auto number = toNumber(value);
            if (number && setGlobal(*global, *number)) {
                logger::debug("Set the global {:X} of {} to {} ('{}')", global->formId, global->plugin, *number, setting.text);
                return true;
            }
        }
        if (const auto* modSetting = std::get_if<model::ModSettingStorage>(&setting.storage)) {
            const auto typed = typedValue(modSetting->type, value);
            return typed && _modSettings.set(modSetting->name, *typed);
        }
        if (const auto* property = std::get_if<model::PropertyStorage>(&setting.storage)) {
            const auto typed = typedValue(property->type, value);
            if (typed && setProperty(*property, *typed)) {
                logger::debug("Set the property {} of the form {:X} of {} ('{}')", property->name, property->formId, property->plugin, setting.text);
                return true;
            }
        }
        return false;
    }
}
