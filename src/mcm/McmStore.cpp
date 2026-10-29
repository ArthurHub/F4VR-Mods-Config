#include "McmStore.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <format>
#include <optional>
#include <ranges>
#include <string>
#include <variant>

#include "common/CommonUtils.h"

using namespace mods_config;

namespace
{
    // the least and the most a whole number of Papyrus is
    constexpr double MIN_WHOLE = -2147483648.0;
    constexpr double MAX_WHOLE = 2147483647.0;

    // the decimals a number of a text input has at most, as MCM's menu shows it
    constexpr double NUMBER_DECIMALS = 1e6;

    /**
     * Whether a setting is a text input that takes a number: a `textinputInt` or a `textinputFloat`.
     */
    bool isNumberField(const model::Setting& setting)
    {
        return setting.type == model::SettingType::TextInput && setting.valueType != model::StoredType::String;
    }

    /**
     * A number as the text of a text input that takes one, as MCM's menu shows it: a whole number is cut at
     * its point, and a number has 6 decimals at most, written as short as it can be.
     */
    std::string numberText(const model::Setting& setting, const double number)
    {
        if (setting.valueType == model::StoredType::Int) {
            return std::format("{}", static_cast<int>(std::clamp(number, MIN_WHOLE, MAX_WHOLE)));
        }
        return std::format("{}", static_cast<float>(std::round(number * NUMBER_DECIMALS) / NUMBER_DECIMALS));
    }

    /**
     * The number a text is, all of it: none for a text with anything else in it, and for an empty one.
     */
    std::optional<double> textNumber(const std::string& text)
    {
        double number = 0.0;
        const auto* end = text.data() + text.size();
        const auto [read, error] = std::from_chars(text.data(), end, number);
        return error == std::errc() && read == end && std::isfinite(number) ? std::optional(number) : std::nullopt;
    }

    /**
     * The text a text input that takes a number is set to: the number that is typed, as MCM's menu shows it.
     * None for a text that is no number of the field's type: a whole number has no point.
     */
    std::optional<std::string> typedNumberText(const model::Setting& setting, const model::Value& value)
    {
        const auto* text = std::get_if<std::string>(&value);
        const auto number = text ? textNumber(*text) : std::nullopt;
        if (!number || (setting.valueType == model::StoredType::Int && text->find_first_not_of("-0123456789") != std::string::npos)) {
            return std::nullopt;
        }
        return numberText(setting, *number);
    }

    /**
     * The value a setting has with none: a switcher is off, a slider is at its least, and a stepper and a
     * dropdown are at their first option. A text input that takes a number is 0.
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
        return isNumberField(setting) ? std::string("0") : std::string();
    }

    /**
     * Whether the value of a setting is a text as its storage has it: the one of a text input that takes a
     * text, and of a choice whose options are texts, as the files of a `dropdownFiles` are for a storage that
     * keeps a text. A text input that takes a number has the text of the number its storage keeps.
     */
    bool keepsText(const model::Setting& setting)
    {
        const auto& options = setting.valueOptions.options;
        return (setting.type == model::SettingType::TextInput && !isNumberField(setting)) || (!options.empty() && std::holds_alternative<std::string>(options.front().value));
    }

    /**
     * The number of a global as the value of a setting, as MCM's menu reads it: a switcher is on at 1 and
     * off at any other number, and a stepper and a dropdown have the option whose place is the number.
     * A text input that takes a number has its text. None for a setting that keeps no number.
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
            if (isNumberField(setting)) {
                return numberText(setting, number);
            }
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
     * The text of a ModSetting that is in neither of MCM's files, as what MCM reads for it: off, -1 for a
     * number, and an empty text. A setting of a choice then shows no option.
     */
    std::string unknownModSettingText(const model::StoredType type)
    {
        switch (type) {
        case model::StoredType::Bool:
            return "0";
        case model::StoredType::Int:
        case model::StoredType::Float:
            return "-1";
        case model::StoredType::String:
            break;
        }
        return {};
    }

    /**
     * The value of a setting that is a ModSetting, from the text MCM's files have for it. A setting whose
     * value is a text has the text as it is written. One whose text is no number for a setting that keeps one
     * is not available, with the reason.
     */
    model::StoredValue modSettingValue(const model::Setting& setting, const model::ModSettingStorage& modSetting, const std::string& text)
    {
        if (keepsText(setting)) {
            return { .value = text, .available = true, .reason = {} };
        }
        if (const auto number = modSettingNumber(modSetting.type, text)) {
            if (auto value = fromNumber(setting, *number)) {
                return { .value = std::move(*value), .available = true, .reason = {} };
            }
        }
        return { .value = valueWithNone(setting),
            .available = false,
            .reason = std::format("Not available: the ModSetting {} is '{}' in MCM's files, which is no number.", modSetting.name, text) };
    }

    /**
     * A value as the type its storage keeps it in, which MCM's function and a script's property take: a bool
     * is on at any number but 0, and a whole number is the nearest one. A text is read as a number, as a text
     * input that takes one gives it. None for a number as a text, and a text that is no number as a number.
     */
    std::optional<model::Value> typedValue(const model::StoredType type, const model::Value& value)
    {
        using Type = model::StoredType;
        if (type == Type::String) {
            return std::holds_alternative<std::string>(value) ? std::optional(value) : std::nullopt;
        }
        auto number = toNumber(value);
        if (const auto* text = std::get_if<std::string>(&value); text && !number) {
            if (const auto read = textNumber(*text)) {
                number = static_cast<float>(*read);
            }
        }
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
     * on at any number but 0, as MCM's menu reads it, and a setting whose value is a text has a number as its
     * text. One that is not in the game is not available, and neither is a text for a setting that keeps a
     * number, with the reason.
     */
    model::StoredValue propertyValue(const model::Setting& setting, const model::PropertyStorage& property, const std::optional<model::Value>& stored)
    {
        if (!stored) {
            return { .value = valueWithNone(setting),
                .available = false,
                .reason = std::format("Not available: the property {} of the form {:X} of {} is not in the game.", property.name, property.formId, property.plugin) };
        }
        const auto number = toNumber(*stored);
        if (keepsText(setting)) {
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
     * A value as the text MCM's menu puts in a parameter: a switcher is `1` and `0`, a choice is the place
     * of its option, and a number is written as short as it can be.
     */
    std::string valueText(const model::Value& value)
    {
        if (const auto* on = std::get_if<bool>(&value)) {
            return *on ? "1" : "0";
        }
        if (const auto* whole = std::get_if<int>(&value)) {
            return std::format("{}", *whole);
        }
        if (const auto* number = std::get_if<float>(&value)) {
            return std::format("{}", *number);
        }
        return std::get<std::string>(value);
    }

    constexpr std::string_view VALUE_TOKEN = "{value}";

    /**
     * An argument of a call with the value its row is set to: the value is put in the argument's text, in
     * place of its first `{value}`, and the text is given in the argument's type, as MCM's menu does.
     * A text that is no number is 0, a whole number is cut at its point, and a bool is on at any number but 0.
     */
    model::Value filledArgument(const model::ValueArgument& argument, const model::Value& value)
    {
        auto text = argument.text;
        if (const auto token = text.find(VALUE_TOKEN); token != std::string::npos) {
            text.replace(token, VALUE_TOKEN.size(), valueText(value));
        }
        if (argument.type == model::StoredType::String) {
            return text;
        }
        double number = 0.0;
        const auto* end = text.data() + text.size();
        if (const auto [read, error] = std::from_chars(text.data(), end, number); error != std::errc() || read != end) {
            number = 0.0;
        }
        switch (argument.type) {
        case model::StoredType::Bool:
            return common::fNotEqual(static_cast<float>(number), 0.0f);
        case model::StoredType::Int:
            return static_cast<int>(std::clamp(number, MIN_WHOLE, MAX_WHOLE));
        default:
            return static_cast<float>(number);
        }
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

    /**
     * Whether two values of a setting are the same: two numbers that are as near as a number MCM writes.
     */
    bool isSameValue(const model::Value& value, const model::Value& other)
    {
        const auto* number = std::get_if<float>(&value);
        const auto* otherNumber = std::get_if<float>(&other);
        return number && otherNumber ? common::fEqual(*number, *otherNumber) : value == other;
    }

    /**
     * Where a setting keeps its value, as the log names it for a change that is not written.
     */
    std::string storageName(const model::Setting& setting)
    {
        if (const auto* global = std::get_if<model::GlobalStorage>(&setting.storage)) {
            return std::format("the global {:X} of {}", global->formId, global->plugin);
        }
        if (const auto* property = std::get_if<model::PropertyStorage>(&setting.storage)) {
            return std::format("the property {} of the form {:X} of {}", property->name, property->formId, property->plugin);
        }
        if (const auto* modSetting = std::get_if<model::ModSettingStorage>(&setting.storage)) {
            return std::format("the ModSetting {}", modSetting->name);
        }
        return std::format("'{}'", setting.text);
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
     * mod's. Its ModSettings are read from MCM's files now, and the player's file again as another of its pages
     * is shown: MCM writes it on every set, so the files have what MCM has. Its hotkeys are read now alone: MCM
     * writes their file as its own menu closes. The options a choice reads in the game are read now, and again
     * as another page is shown.
     * The scripts are told that the mod is shown, as MCM's menu tells them for a press on the mod's row.
     */
    void McmStore::open(const model::Mod& mod)
    {
        close();
        _modName = mod.name;
        _modSettings.load(_mcmFolder, mod.name);
        _keybinds.load(_mcmFolder, mod.name);
        _options.read(mod);
        logger::debug("Read {} ModSettings and {} hotkeys of '{}' from MCM's files", _modSettings.size(), _keybinds.size(), mod.name);
        addSettings(mod.frontPage.items, _settings);
        for (const auto& page : mod.pages) {
            addSettings(page.items, _settings);
        }
        sendEvent("OnMCMMenuOpen");
        sendEvent("OnMCMMenuOpen|" + _modName);
    }

    /**
     * Ends the session of the mod: its values are forgotten, and the scripts are told that it is left, which they are
     * not with no mod. A change the player made stays, as the panel asks to Save or Discard before it leaves a mod with
     * one: one that is still there is kept, as by a Save, so a DLL that reads MCM's file is told of a ModSetting it
     * wrote.
     * It does not read the mod, which can be gone by now: the menu's mods are replaced before it is called.
     */
    void McmStore::close()
    {
        if (!_changes.empty()) {
            logger::info("The session of '{}' ends with {} changes, which are kept", _modName, _changes.size());
        }
        waitForPauseMenu();
        if (!_modName.empty()) {
            sendEvent("OnMCMMenuClose|" + _modName);
        }
        _modName.clear();
        _modSettings.load({}, {});
        _keybinds.load({}, {});
        _options.clear();
        _held.clear();
        _settings.clear();
        _changes.clear();
        _lastSave.reset();
    }

    /**
     * Reads the player's file of the ModSettings of the mod that is selected again, as another of its pages is
     * shown, so a value a script set since shows there: MCM writes the file on every set, also one of a script.
     * The options a choice reads in the game are read again too, as a script can change its list.
     * Nothing with no mod selected.
     */
    void McmStore::pageShown()
    {
        if (_modName.empty()) {
            return;
        }
        _options.readAgain();
        if (const auto changed = _modSettings.readAgain()) {
            logger::debug("Read the ModSettings of '{}' from MCM's files again: {} changed", _modName, changed);
        }
    }

    /**
     * The value of a setting, as its storage has it now: a change is written as it is made.
     */
    model::StoredValue McmStore::value(const model::Setting& setting)
    {
        return stored(setting);
    }

    /**
     * The value a setting's storage has, read now: another mod or the console can change a global at any
     * time. A ModSetting is the one of the mod that is selected, as its files had it when they were last read,
     * or as a Save wrote it since.
     * A property of a script is read from the script now too.
     * A ModSetting that is in neither of MCM's files is what MCM reads for it, until it is set and held.
     * A global that is not in the game is not available, with the reason, and neither is a property that is
     * not in the game, a value of a storage the store does not read, and a choice whose list of options is not
     * in the game.
     */
    model::StoredValue McmStore::stored(const model::Setting& setting)
    {
        if (const auto reason = _options.notAvailable(setting); !reason.empty()) {
            return { .value = valueWithNone(setting), .available = false, .reason = std::string(reason) };
        }
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
            if (const auto* text = _modSettings.find(modSetting->name)) {
                return modSettingValue(setting, *modSetting, *text);
            }
            if (const auto held = _held.find(&setting); held != _held.end()) {
                return { .value = held->second, .available = true, .reason = {} };
            }
            const auto unknown = unknownModSettingText(modSetting->type);
            return modSettingValue(setting, *modSetting, unknown);
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
     * Sets a setting, as MCM's menu does: its value is written where its storage says, as it reads it back, then
     * the action of its row runs and the scripts are told it changed. A slider of a whole number has the nearest one.
     * A value that is the one its storage has changes nothing. The setting is dirty while its value is not the one
     * it had before the player first changed it, which Discard writes back.
     * A setting with no storage is held, with the action of its row and MCM's event, and is never dirty, as nothing
     * writes it, and so is a ModSetting that is in neither of MCM's files, which MCM does not set: as Sim Settlements
     * 2's switchers that show a group.
     * A text input that takes a number is set to the text of the number that is typed, as MCM's menu shows it.
     * A value that is set ends the last Save.
     * Returns false for a value that is not set: one of a storage that is not available or that the store does
     * not write, one its storage does not keep, and a text that is no number, for a text input that takes one.
     */
    bool McmStore::set(const model::Setting& setting, const model::Value& typed)
    {
        auto value = typed;
        if (isNumberField(setting)) {
            auto text = typedNumberText(setting, typed);
            if (!text) {
                logger::debug("A text that is no number is not set to '{}'", setting.text);
                return false;
            }
            value = std::move(*text);
        }
        const auto* modSetting = std::get_if<model::ModSettingStorage>(&setting.storage);
        if (std::holds_alternative<model::NoStorage>(setting.storage) || (modSetting && !_modSettings.find(modSetting->name))) {
            write(setting, value);
            written(setting, value);
            return true;
        }
        auto current = stored(setting);
        auto read = current.available ? readBack(setting, value) : std::nullopt;
        if (!read) {
            return false;
        }
        if (isSameValue(*read, current.value)) {
            return true;
        }
        if (!change(setting, *read)) {
            return false;
        }
        _lastSave.reset();
        const auto before = std::ranges::find(_changes, &setting, &Change::setting);
        auto baseline = before != _changes.end() ? std::move(before->baseline) : std::move(current.value);
        if (before != _changes.end()) {
            _changes.erase(before);
        }
        if (!isSameValue(*read, baseline)) {
            _changes.push_back({ .setting = &setting, .baseline = std::move(baseline) });
        }
        return true;
    }

    /**
     * Whether the player changed a setting from the value it had before, which a Save keeps.
     */
    bool McmStore::isDirty(const model::Setting& setting) const
    {
        return std::ranges::contains(_changes, &setting, &Change::setting);
    }

    /**
     * Keeps the changes, which are written already: their values are the settings' baselines now. Then, where the
     * session wrote a ModSetting since its last Save, a DLL that reads its MCM file as the pause menu closes is told
     * that it did, as it is when MCM's menu, which is in the pause menu, is left: once MCM wrote the file, which it
     * does on Papyrus's time, see frameUpdate. The DLL is told at a Save alone, as the events go to every listener
     * of the game's menus.
     */
    std::expected<void, std::string> McmStore::save()
    {
        logger::info("Saved {} changes of '{}'", _changes.size(), _modName);
        _changes.clear();
        waitForPauseMenu();
        _lastSave = 0;
        return {};
    }

    /**
     * Has the pause menu's events wait for MCM's file to have each ModSetting the session wrote since its last Save.
     * Nothing for a session that wrote none.
     */
    void McmStore::waitForPauseMenu()
    {
        if (_modSettingsWritten.empty()) {
            return;
        }
        if (!_pauseMenuWait) {
            _pauseMenuWait.emplace();
        }
        _pauseMenuWait->framesLeft = PAUSE_MENU_WAIT_FRAMES;
        auto& file = _pauseMenuWait->files[_modSettings.playerFile()];
        for (auto& [name, text] : _modSettingsWritten) {
            file.insert_or_assign(name, std::move(text));
        }
        _modSettingsWritten.clear();
    }

    /**
     * Tells the game's listeners of its menus that the pause menu opened and closed, after a Save wrote a ModSetting,
     * once MCM's player file has every ModSetting the Saves wrote: MCM writes it as it runs the call that sets one,
     * which is queued to Papyrus, and a DLL reads its MCM file as the pause menu closes. After
     * `PAUSE_MENU_WAIT_FRAMES` frames they are sent anyway, as for a value MCM had already and does not write again.
     * The menu calls it every frame, also while it is closed.
     */
    void McmStore::frameUpdate()
    {
        if (!_pauseMenuWait) {
            return;
        }
        const bool inFile = std::ranges::all_of(_pauseMenuWait->files, [](const auto& file) {
            return McmModSettings::fileHas(file.first, file.second);
        });
        if (!inFile && --_pauseMenuWait->framesLeft > 0) {
            return;
        }
        if (!inFile) {
            logger::warn("MCM's file does not have the ModSettings a Save wrote after {} frames: the pause menu's events are sent anyway", PAUSE_MENU_WAIT_FRAMES);
        }
        _pauseMenuWait.reset();
        sendPauseMenuEvents();
    }

    /**
     * Sets the settings the player changed back to the values they had before, in the reverse of the order each was
     * last changed, as a change is made: each is written, the action of its row runs and the scripts are told. So a
     * preset that is set back runs its script with the preset it had, which sets its rows back. What a script did
     * that the value set back does not undo stays. The last Save is over.
     */
    void McmStore::discard()
    {
        auto changes = std::move(_changes);
        _changes.clear();
        for (const auto& [setting, baseline] : changes | std::views::reverse) {
            change(*setting, baseline);
        }
        _lastSave.reset();
    }

    /**
     * Sets one setting the player changed back to the value it had before, as discard does. It ends the last Save, as
     * a set does.
     */
    void McmStore::discard(const model::Setting& setting)
    {
        const auto found = std::ranges::find(_changes, &setting, &Change::setting);
        if (found == _changes.end()) {
            return;
        }
        const auto baseline = std::move(found->baseline);
        _changes.erase(found);
        change(setting, baseline);
        _lastSave.reset();
    }

    /**
     * The value a dirty setting had before the player changed it, which its Discard writes back. None for a clean one.
     */
    std::optional<model::Value> McmStore::savedValue(const model::Setting& setting)
    {
        const auto found = std::ranges::find(_changes, &setting, &Change::setting);
        return found != _changes.end() ? std::optional(found->baseline) : std::nullopt;
    }

    /**
     * A frame of the panel starts: a setting whose storage has the value it had before the player changed it, as a
     * script set it back, is no change any more.
     */
    void McmStore::startFrame()
    {
        std::erase_if(_changes, [this](const Change& change) {
            const auto current = stored(*change.setting);
            return current.available && isSameValue(current.value, change.baseline);
        });
    }

    /**
     * The default of a setting: for a ModSetting, the value the mod's own `settings.ini` has for it, read as
     * the value of the player's file is. MCM has no default for a global and a property of a script.
     * None for a ModSetting the mod's file does not have, and for one whose text there is no value of the setting.
     */
    std::optional<model::Value> McmStore::defaultValue(const model::Setting& setting)
    {
        const auto* modSetting = std::get_if<model::ModSettingStorage>(&setting.storage);
        const auto* text = modSetting ? _modSettings.findDefault(modSetting->name) : nullptr;
        if (!text) {
            return std::nullopt;
        }
        auto stored = modSettingValue(setting, *modSetting, *text);
        return stored.available ? std::optional(std::move(stored.value)) : std::nullopt;
    }

    /**
     * The key a hotkey of the mod that is selected is bound to, as MCM's files had it when the mod was
     * selected: MCM writes the player's file as its own menu closes, where a key is bound.
     * A hotkey the mod's `keybinds.json` does not have is not available, with the reason: MCM binds no key to it.
     */
    model::StoredValue McmStore::hotkey(const model::Hotkey& hotkey)
    {
        if (const auto* key = _keybinds.find(hotkey.id)) {
            return { .value = *key, .available = true, .reason = {} };
        }
        return { .value = keyName(0, 0),
            .available = false,
            .reason =
                hotkey.id.empty() ? "Not available: the hotkey has no id."s : std::format("Not available: the hotkey {} is not in the keybinds.json of the mod.", hotkey.id) };
    }

    /**
     * Runs the action of a button, as MCM's menu does for a press on it: a `CallFunction` and a
     * `CallGlobalFunction` call their function, which Papyrus runs on its own time, so nothing is known of
     * how it ends.
     * Returns false for a row with no action, for an action the store does not run, and for a function whose
     * form or script is not in the game.
     */
    model::ActionResult McmStore::run(const model::Action& action)
    {
        const auto* call = std::get_if<model::CallFunction>(&action);
        return { .ran = call && callFunction(*call), .failed = false, .message = {} };
    }

    /**
     * Makes a change of a setting, as MCM's menu does: its value is written, then the action of its row runs and the
     * scripts are told. A ModSetting written is kept for the pause menu's events of the next Save. Returns false for a
     * value that is not written, as of a global or a property that is no longer in the game: nothing else is done.
     */
    bool McmStore::change(const model::Setting& setting, const model::Value& value)
    {
        if (!write(setting, value)) {
            logger::warn("A change of '{}' of '{}' is not written: {} is not in the game", setting.text, _modName, storageName(setting));
            return false;
        }
        written(setting, value);
        const auto* modSetting = std::get_if<model::ModSettingStorage>(&setting.storage);
        if (const auto* text = modSetting ? _modSettings.find(modSetting->name) : nullptr) {
            _modSettingsWritten.insert_or_assign(modSetting->name, *text);
        }
        return true;
    }

    /**
     * Writes a value where the setting's storage says. Returns false for a value that is not written: one of
     * a storage the store does not write, or of a global or a property that is not in the game.
     * A ModSetting that is in neither of MCM's files is held here while its mod is selected, as MCM's menu
     * holds it: MCM has only those of its files, and sets no other.
     */
    bool McmStore::write(const model::Setting& setting, const model::Value& value)
    {
        if (std::holds_alternative<model::NoStorage>(setting.storage)) {
            _held.insert_or_assign(&setting, value);
            return true;
        }
        if (const auto* global = std::get_if<model::GlobalStorage>(&setting.storage)) {
            const auto number = typedValue(model::StoredType::Float, value);
            if (number && setGlobal(*global, std::get<float>(*number))) {
                logger::debug("Set the global {:X} of {} to {} ('{}')", global->formId, global->plugin, std::get<float>(*number), setting.text);
                return true;
            }
        }
        if (const auto* modSetting = std::get_if<model::ModSettingStorage>(&setting.storage)) {
            if (!_modSettings.find(modSetting->name)) {
                // MCM sets no such ModSetting, and its menu keeps the value while it is open
                _held.insert_or_assign(&setting, value);
                logger::debug("Held the ModSetting {} of '{}', which is in neither of MCM's files ('{}')", modSetting->name, _modName, setting.text);
                return true;
            }
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

    /**
     * The value a setting has once a value is written to its storage, as the storage keeps it and the row reads
     * it: a bool of a choice is its first option or its second, and a whole number of a slider the nearest one.
     * None for a value its storage does not keep, as a text for a number, and for a storage the store does not
     * write.
     */
    std::optional<model::Value> McmStore::readBack(const model::Setting& setting, const model::Value& value) const
    {
        if (std::holds_alternative<model::GlobalStorage>(setting.storage)) {
            const auto number = typedValue(model::StoredType::Float, value);
            return number ? fromNumber(setting, std::get<float>(*number)) : std::nullopt;
        }
        if (const auto* modSetting = std::get_if<model::ModSettingStorage>(&setting.storage)) {
            const auto typed = typedValue(modSetting->type, value);
            if (!typed || keepsText(setting)) {
                return typed;
            }
            const auto number = toNumber(*typed);
            return number ? fromNumber(setting, *number) : std::nullopt;
        }
        if (const auto* property = std::get_if<model::PropertyStorage>(&setting.storage)) {
            const auto typed = typedValue(property->type, value);
            auto read = typed ? propertyValue(setting, *property, typed) : model::StoredValue{};
            return read.available ? std::optional(std::move(read.value)) : std::nullopt;
        }
        return std::nullopt;
    }

    /**
     * What follows the write of a value, in the order of MCM's menu: the action of its row runs with the value,
     * and then the scripts are told that the setting changed, with the name of the mod and the id of the row,
     * for a row that has one in its config.
     */
    void McmStore::written(const model::Setting& setting, const model::Value& value) const
    {
        if (const auto* call = std::get_if<model::CallFunction>(&setting.action)) {
            auto filled = *call;
            for (const auto& argument : filled.valueArguments) {
                filled.arguments[argument.place] = filledArgument(argument, value);
            }
            filled.valueArguments.clear();
            callFunction(filled);
        }
        if (!setting.sourceId.empty()) {
            sendEvent("OnMCMSettingChange", { _modName, setting.sourceId });
            sendEvent("OnMCMSettingChange|" + _modName, { _modName, setting.sourceId });
        }
    }
}
