#include "ConfigSpecStore.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <expected>
#include <format>
#include <memory>
#include <ranges>
#include <span>
#include <unordered_set>
#include <variant>
#include <vector>

#include "GameActions.h"
#include "GameValues.h"
#include "IniFile.h"
#include "TomlFile.h"
#include "common/CommonUtils.h"
#include "model/InputBindings.h"

using namespace mods_config;

namespace
{
    /**
     * Whether a setting is a text input that takes a number.
     */
    bool isNumberField(const model::Setting& setting)
    {
        return setting.type == model::SettingType::TextInput && setting.valueType != model::StoredType::String;
    }

    bool isChoice(const model::Setting& setting)
    {
        return setting.type == model::SettingType::Stepper || setting.type == model::SettingType::Dropdown;
    }

    /**
     * Whether a setting's number has limits: a slider and a text input that takes a number, whose most is
     * above its least.
     */
    bool hasLimits(const model::Setting& setting)
    {
        return (setting.type == model::SettingType::Slider || isNumberField(setting)) && setting.valueOptions.max > setting.valueOptions.min;
    }

    /**
     * Whether a number is inside the limits of its setting, its least and its most among them.
     */
    bool inLimits(const model::Setting& setting, const float number)
    {
        const auto& options = setting.valueOptions;
        return !hasLimits(setting) || ((number > options.min || common::fEqual(number, options.min)) && (number < options.max || common::fEqual(number, options.max)));
    }

    /**
     * Whether two values of a setting are the same one. Two numbers are, when they are as near as a number
     * and the text of it in a file.
     */
    bool isSameValue(const model::Value& value, const model::Value& other)
    {
        const auto* number = std::get_if<float>(&value);
        const auto* otherNumber = std::get_if<float>(&other);
        return number && otherNumber ? common::fEqual(*number, *otherNumber) : value == other;
    }

    /**
     * Whether a value is the value of one of the options of a choice.
     */
    bool isOption(const std::vector<model::Option>& options, const model::Value& value)
    {
        return std::ranges::any_of(options, [&value](const model::Option& option) {
            return isSameValue(option.value, value);
        });
    }

    /**
     * A value as a reason shows it: a text in quotes, and a number as short as it can be written.
     */
    std::string valueText(const model::Value& value)
    {
        if (const auto* on = std::get_if<bool>(&value)) {
            return *on ? "true" : "false";
        }
        if (const auto* whole = std::get_if<int>(&value)) {
            return std::format("{}", *whole);
        }
        if (const auto* number = std::get_if<float>(&value)) {
            return std::format("{}", *number);
        }
        return std::format("\"{}\"", std::get<std::string>(value));
    }

    /**
     * The path of a config spec's file from its root, as written in its config spec.
     */
    std::filesystem::path filePath(const model::File& file)
    {
        return std::filesystem::path(std::u8string(file.path.begin(), file.path.end()));
    }

    /**
     * A path as the menu compares one with another: absolute, normal, with the separators of Windows and in lower case,
     * as Windows matches a file's name with no regard to case.
     */
    std::string comparablePath(const std::filesystem::path& path)
    {
        std::error_code error;
        const auto text = std::filesystem::absolute(path, error).lexically_normal().make_preferred().u8string();
        return common::str_tolower(std::string(text.begin(), text.end()));
    }

    /**
     * Whether a path is in a folder, or in a folder in it.
     */
    bool isInFolder(const std::filesystem::path& path, const std::filesystem::path& folder)
    {
        auto folderText = comparablePath(folder);
        if (!folderText.ends_with('\\')) {
            folderText += '\\';
        }
        return comparablePath(path).starts_with(folderText);
    }

    /**
     * The name of a config spec's file, which names it in a reason, as a reason of its file does.
     */
    std::string fileName(const model::File& file)
    {
        const auto name = filePath(file).filename().u8string();
        return { name.begin(), name.end() };
    }

    /**
     * A value of the type a setting keeps, as its control has it: a slider has a number, also for a whole
     * number, a text input that takes a number has the text of it, and a binding has its table's text as the
     * framework writes it. None for a value its control does not show: a switcher of anything but true or false,
     * a slider of a text, and a binding of anything but a text.
     */
    std::optional<model::Value> controlValue(const model::Setting& setting, const model::Value& stored)
    {
        switch (setting.type) {
        case model::SettingType::Switcher:
            if (std::holds_alternative<bool>(stored)) {
                return stored;
            }
            break;
        case model::SettingType::Slider:
            if (const auto* whole = std::get_if<int>(&stored)) {
                return static_cast<float>(*whole);
            }
            if (std::holds_alternative<float>(stored)) {
                return stored;
            }
            break;
        case model::SettingType::Stepper:
        case model::SettingType::Dropdown:
            return stored;
        case model::SettingType::TextInput:
            if (const auto* whole = std::get_if<int>(&stored)) {
                return std::format("{}", *whole);
            }
            if (const auto* number = std::get_if<float>(&stored)) {
                return std::format("{}", *number);
            }
            return stored;
        case model::SettingType::InputBinding:
            if (const auto* text = std::get_if<std::string>(&stored)) {
                return model::canonicalBinding(*text);
            }
            break;
        }
        return std::nullopt;
    }

    /**
     * A value of a type that is no value in particular: false, 0, and an empty text.
     */
    model::Value zeroOf(const model::StoredType type)
    {
        switch (type) {
        case model::StoredType::Bool:
            return false;
        case model::StoredType::Int:
            return 0;
        case model::StoredType::Float:
            return 0.0f;
        case model::StoredType::String:
            break;
        }
        return std::string();
    }

    /**
     * The value a setting shows while it has none of its own: its default, or else its least, the first of its
     * options, or the zero of its type.
     */
    model::Value noneValue(const model::Setting& setting, const std::vector<model::Option>& options)
    {
        if (setting.defaultValue) {
            if (auto value = controlValue(setting, *setting.defaultValue)) {
                return std::move(*value);
            }
        }
        if (setting.type == model::SettingType::Slider) {
            return setting.valueOptions.min;
        }
        if (isChoice(setting) && !options.empty()) {
            return options.front().value;
        }
        return controlValue(setting, zeroOf(setting.valueType)).value_or(zeroOf(setting.valueType));
    }

    /**
     * The number a text is, all of it, as a text input that takes one keeps it: a whole number has nothing
     * but its digits and a sign. None for a text that is no number of the field's type, and for an empty one.
     */
    std::optional<float> fieldNumber(const model::Setting& setting, const std::string& text)
    {
        const auto* end = text.data() + text.size();
        if (setting.valueType == model::StoredType::Int) {
            int whole = 0;
            const auto [read, error] = std::from_chars(text.data(), end, whole);
            return error == std::errc() && read == end ? std::optional(static_cast<float>(whole)) : std::nullopt;
        }
        float number = 0.0f;
        const auto [read, error] = std::from_chars(text.data(), end, number);
        return error == std::errc() && read == end && std::isfinite(number) ? std::optional(number) : std::nullopt;
    }

    /**
     * A value the player sets, as its control has it, if it is of the type its control shows: a bool of a
     * switcher, a number of a slider, a value of its type of a choice, and a text of a text input. A number of
     * a whole number setting is the nearest whole number, and the text of a number is written as short as it
     * can be. None for a value of another type. It can still be no value of its setting (`problemOf`).
     */
    std::optional<model::Value> typedValue(const model::Setting& setting, const model::Value& value)
    {
        switch (setting.type) {
        case model::SettingType::Switcher:
            if (std::holds_alternative<bool>(value)) {
                return value;
            }
            break;
        case model::SettingType::Slider:
            if (const auto* number = std::get_if<float>(&value)) {
                return setting.valueType == model::StoredType::Int ? std::round(*number) : *number;
            }
            break;
        case model::SettingType::Stepper:
        case model::SettingType::Dropdown:
            if (value.index() == zeroOf(setting.valueType).index()) {
                return value;
            }
            break;
        case model::SettingType::TextInput:
            if (const auto* text = std::get_if<std::string>(&value); text && isNumberField(setting)) {
                if (const auto number = fieldNumber(setting, *text)) {
                    return setting.valueType == model::StoredType::Int ? std::format("{}", static_cast<int>(*number)) : std::format("{}", *number);
                }
                return value;
            } else if (text) {
                return value;
            }
            break;
        case model::SettingType::InputBinding:
            if (const auto* text = std::get_if<std::string>(&value)) {
                return model::canonicalBinding(*text);
            }
            break;
        }
        return std::nullopt;
    }

    /**
     * What is wrong with a value of a setting, as its control has it, that is no value of it, as the end of a
     * sentence of the value: `is not from 0.5 to 1.5`. A text of a number that is none, a number outside the
     * limits, a text of a length its setting does not allow, and none of the options of a choice. A table that is
     * no binding has the framework's reason, a sentence of its own, and so does a binding of a hand, a way or a
     * button its setting does not take. Empty for a value that is one.
     */
    std::string problemOf(const model::Setting& setting, const std::vector<model::Option>& options, const model::Value& value)
    {
        if (setting.type == model::SettingType::InputBinding) {
            const auto binding = model::parseBinding(std::get<std::string>(value));
            return binding ? model::untakenPart(*binding, setting.valueOptions) : binding.error();
        }
        const auto& limits = setting.valueOptions;
        std::optional<float> number;
        if (setting.type == model::SettingType::Slider) {
            number = std::get<float>(value);
        } else if (isNumberField(setting)) {
            number = fieldNumber(setting, std::get<std::string>(value));
            if (!number) {
                return setting.valueType == model::StoredType::Int ? "is not a whole number" : "is not a number";
            }
        }
        if (number && !inLimits(setting, *number)) {
            return std::format("is not from {} to {}", limits.min, limits.max);
        }
        if (isChoice(setting) && !isOption(options, value)) {
            return "is none of its options";
        }
        if (setting.type == model::SettingType::TextInput && !isNumberField(setting)) {
            const int length = model::textLength(std::get<std::string>(value));
            if (length < limits.minLength) {
                return std::format("has {} character{}, and needs at least {}", length, length == 1 ? "" : "s", limits.minLength);
            }
            if (limits.maxLength > 0 && length > limits.maxLength) {
                return std::format("has {} characters, and can have at most {}", length, limits.maxLength);
            }
        }
        return {};
    }

    /**
     * Why a value the player set is no value of its setting, for the help line: the value and what is wrong
     * with it, as `Invalid: 12 is not from 1 to 9.` A text of a text input is `the text`, as it can be long, and a
     * binding has the framework's reason alone, as its row shows its text. Empty for a value that is one.
     */
    std::string typedReason(const model::Setting& setting, const std::vector<model::Option>& options, const model::Value& value)
    {
        const auto problem = problemOf(setting, options, value);
        if (problem.empty()) {
            return {};
        }
        if (setting.type == model::SettingType::InputBinding) {
            return std::format("Invalid: {}.", problem);
        }
        const auto* text = std::get_if<std::string>(&value);
        const bool isText = setting.type == model::SettingType::TextInput && !isNumberField(setting);
        const auto shown = isText ? std::string("the text") : isNumberField(setting) && fieldNumber(setting, *text) ? *text : valueText(value);
        return std::format("Invalid: {} {}.", shown, problem);
    }

    /**
     * A value as its file keeps it, in the type of its setting, from the value its control has: the number of
     * a slider of a whole number is the whole number, and the text of a text input that takes a number is
     * that number. The value is one of the setting, as a pending value is.
     */
    model::Value storedValue(const model::Setting& setting, const model::Value& value)
    {
        if (setting.type == model::SettingType::Slider && setting.valueType == model::StoredType::Int) {
            return static_cast<int>(std::round(std::get<float>(value)));
        }
        if (!isNumberField(setting)) {
            return value;
        }
        const auto& text = std::get<std::string>(value);
        if (setting.valueType == model::StoredType::Int) {
            int whole = 0;
            std::from_chars(text.data(), text.data() + text.size(), whole);
            return whole;
        }
        float number = 0.0f;
        std::from_chars(text.data(), text.data() + text.size(), number);
        return number;
    }

    /**
     * Why a value in a file, as its control has it, is not one of its setting, as `problemOf` says. Empty for
     * a value that is one. It names the value by its path and its file, as a reason of its file does, but for a
     * binding, which has the framework's reason alone, as its row shows its text.
     */
    std::string invalidReason(const model::Setting& setting, const std::vector<model::Option>& options, const model::Value& value, const std::string& path, const std::string& file)
    {
        const auto problem = problemOf(setting, options, value);
        if (problem.empty()) {
            return {};
        }
        if (setting.type == model::SettingType::InputBinding) {
            return std::format("Invalid: {}.", problem);
        }
        return std::format("Invalid: {} is {} in {}, which {}.", path, valueText(value), file, problem);
    }

    /**
     * Whether a setting is kept in the game, in a global or in a property of a script, which the game keeps
     * in the save.
     */
    bool isInGame(const model::Storage& storage)
    {
        return std::holds_alternative<model::GlobalStorage>(storage) || std::holds_alternative<model::PropertyStorage>(storage);
    }

    /**
     * A form as a config spec names it, by its plugin and its id in hex: `Fallout4.esm|38`.
     */
    std::string formText(const std::string& plugin, const std::uint32_t formId)
    {
        return std::format("{}|{:X}", plugin, formId);
    }

    /**
     * The value a setting's mod's client is told a setting had before its change, in the setting's type: its baseline,
     * or its default where its file has no value of its type, and else the value it has now, which is stored.
     */
    model::Value clientBaseline(const model::Setting& setting, const std::optional<model::Value>& baseline, const model::Value& stored)
    {
        return baseline ? storedValue(setting, *baseline) : setting.defaultValue.value_or(stored);
    }

    /**
     * A global or a property as a reason names it: `the global Fallout4.esm|38`.
     */
    std::string gameName(const model::Storage& storage)
    {
        if (const auto* global = std::get_if<model::GlobalStorage>(&storage)) {
            return "the global " + formText(global->plugin, global->formId);
        }
        const auto& property = std::get<model::PropertyStorage>(storage);
        return std::format("the property {} of {}", property.name, formText(property.plugin, property.formId));
    }

    /**
     * Where a setting that is saved keeps its value, as a key that stays the same as its config spec is read
     * again: the name of its file in the config spec with its path, or its global or its property.
     */
    std::string storageKey(const model::Setting& setting)
    {
        const auto* file = std::get_if<model::FileStorage>(&setting.storage);
        if (!file) {
            return gameName(setting.storage);
        }
        auto key = file->file;
        for (const auto& name : file->path) {
            key += '/' + name;
        }
        return key;
    }

    /**
     * A value of the game in the type of a setting's value: a bool is on at any number but 0, as Papyrus casts
     * a number to a bool, a whole number is the nearest one, and a bool is 1 and 0 as a number. None for a
     * text as a number, and for a number as a text.
     */
    std::optional<model::Value> asType(const model::Value& value, const model::StoredType type)
    {
        const bool isText = std::holds_alternative<std::string>(value);
        if (type == model::StoredType::String || isText) {
            return type == model::StoredType::String && isText ? std::optional(value) : std::nullopt;
        }
        const auto* on = std::get_if<bool>(&value);
        const auto* whole = std::get_if<int>(&value);
        const float number = on ? (*on ? 1.0f : 0.0f) : whole ? static_cast<float>(*whole) : std::get<float>(value);
        switch (type) {
        case model::StoredType::Bool:
            return common::fNotEqual(number, 0.0f);
        case model::StoredType::Int:
            return static_cast<int>(std::lround(number));
        default:
            return number;
        }
    }

    /**
     * The value a setting keeps in the game now, in the type of its value, or why it is not available, as
     * `the global MyMod.esp|800 is not in the game`: its global or its property is not in the game, or keeps
     * no value of the setting's type, as a global, which keeps a number, does not for a setting of a text.
     */
    std::expected<model::Value, std::string> readGame(const model::Setting& setting)
    {
        std::optional<model::Value> read;
        if (const auto* global = std::get_if<model::GlobalStorage>(&setting.storage)) {
            if (const auto number = getGlobal(*global)) {
                read = *number;
            }
        } else {
            read = getProperty(std::get<model::PropertyStorage>(setting.storage));
        }
        if (!read) {
            return std::unexpected(std::format("{} is not in the game", gameName(setting.storage)));
        }
        auto typed = asType(*read, setting.valueType);
        if (!typed) {
            return std::unexpected(std::format("{} is {}, which is no value of the setting's valueType", gameName(setting.storage), valueText(*read)));
        }
        return std::move(*typed);
    }

    /**
     * Writes a value into the game, where a storage keeps it, in the type of its setting's value: a global
     * keeps a number, which is 1 and 0 for a bool. Returns false for a global or a property that is not in the
     * game.
     */
    bool writeGame(const model::Storage& storage, const model::Value& stored)
    {
        if (const auto* global = std::get_if<model::GlobalStorage>(&storage)) {
            const auto number = asType(stored, model::StoredType::Float);
            return number && setGlobal(*global, std::get<float>(*number));
        }
        return setProperty(std::get<model::PropertyStorage>(storage), stored);
    }

    /**
     * Runs the action of a setting that Save wrote: a `CallFunction`, a `CallGlobalFunction` and a
     * `CallNativeFunction` have the value written in place of each of their `{value}` parameters, as the file or the
     * game keeps it, and a `RunConsoleCommand` runs its line. A function of the mod's DLL runs by its mod's name.
     */
    void runAction(const model::Setting& setting, const model::Value& written, const model::ClientCalls& functions, const std::string& modName)
    {
        if (const auto* native = std::get_if<model::NativeFunction>(&setting.action)) {
            auto arguments = native->arguments;
            for (const auto place : native->valueArguments) {
                arguments[place].value = storedValue(setting, written);
            }
            functions.call(modName, native->function, arguments);
            return;
        }
        if (const auto* call = std::get_if<model::CallFunction>(&setting.action)) {
            auto filled = *call;
            for (const auto& argument : filled.valueArguments) {
                filled.arguments[argument.place] = storedValue(setting, written);
            }
            filled.valueArguments.clear();
            callFunction(filled);
        } else if (const auto* command = std::get_if<model::ConsoleCommand>(&setting.action)) {
            runConsoleCommand(*command);
        }
    }

    /**
     * Adds the settings of the items to the list, with those of the groups among them.
     */
    void addSettings(const std::vector<model::Item>& items, std::vector<const model::Setting*>& settings)
    {
        for (const auto& item : items) {
            if (const auto* setting = std::get_if<model::Setting>(&item)) {
                settings.push_back(setting);
            } else if (const auto* group = std::get_if<model::Group>(&item)) {
                addSettings(group->items, settings);
            }
        }
    }

    /**
     * Adds the settings of pages to the list, with those of the pages under them.
     */
    void addSettings(const std::vector<model::Page>& pages, std::vector<const model::Setting*>& settings)
    {
        for (const auto& page : pages) {
            addSettings(page.items, settings);
            addSettings(page.pages, settings);
        }
    }

    /**
     * A file of a config spec as the session read it. A file that is not there has neither its values nor a
     * reason: its settings have their defaults.
     */
    struct FileOfSession
    {
        // the file, read. None for a file that is not there or is not available
        const spec::ModFile* file = nullptr;

        // why its settings are not available, and empty for a file that is read or is not there
        std::string notAvailable;
    };

    /**
     * Why a setting kept in a storage the store does not read is not available.
     */
    std::string unknownStorageReason(const model::UnknownStorage& storage)
    {
        if (storage.type == "global") {
            return "Not available: its global is not a plugin and an id in hex, as MyMod.esp|800.";
        }
        if (storage.type == "property") {
            return "Not available: its property names no form as a plugin and an id in hex, as MyMod.esp|800, or has no propertyName.";
        }
        if (storage.type == "binding") {
            return "Not available: a binding is kept in a value of a TOML file.";
        }
        return storage.type == "file" ? "Not available: the setting names no file of its config spec, or no value of it." : "Not available: the setting has no storage.";
    }
}

namespace mods_config::spec
{
    ConfigSpecStore::ConfigSpecStore(std::filesystem::path dataFolder, std::filesystem::path myGamesFolder)
        : _dataFolder(std::move(dataFolder)),
          _myGamesFolder(std::move(myGamesFolder))
    {}

    /**
     * Starts the session of a mod, after it ends the one of the mod before it: its files, its globals and its
     * properties are read now, and the value of each setting is its baseline. One that is not in the game is
     * not available, with the reason.
     * A file that is not there has none of its values, and a key the file does not have is its setting's
     * default: both are clean, and the first Save that changes one of them writes it. A file that cannot be
     * read, or is no TOML, has its settings not available, with the reason, and so has a JSON file, which the
     * menu does not read. A value that is not of its setting's type, outside its limits or no option of its choice is
     * invalid, with the reason: of the game, one that is no value of the setting's type is not available.
     * A choice that reads its options in the game reads them now, and is not available while its list is not
     * in the game.
     * A read-only setting is never invalid: it has the value of its file as it is, none where the file has
     * none, and is not available where the file's value is not of its type.
     */
    void ConfigSpecStore::open(const model::Mod& mod)
    {
        close();
        _modName = mod.name;
        _modDisplayName = mod.displayName;
        _clientStates.clear();
        _mod = &mod;
        _clientCalls.send(_modName, model::ClientEvent::ModShown);
        readValues(mod);
    }

    /**
     * Reads the values of the mod's files and of the game as the baseline of its settings, with why one is not
     * available or is invalid, as `open` has it.
     */
    void ConfigSpecStore::readValues(const model::Mod& mod)
    {
        std::map<std::string, FileOfSession, std::less<>> files;
        for (const auto& [name, file] : mod.files) {
            auto& read = files[name];
            if (file.format == model::FileFormat::Json) {
                read.notAvailable = "Not available: the menu does not read JSON files yet.";
                continue;
            }
            const auto path = _filePaths.insert_or_assign(name, pathOf(file)).first->second;
            auto& modFile = _files[name];
            modFile = file.format == model::FileFormat::Toml ? std::unique_ptr<ModFile>(std::make_unique<TomlFile>(path)) : std::make_unique<IniFile>(path);
            if (!std::filesystem::exists(path)) {
                logger::debug("The file '{}' of '{}' is not there: its settings have their defaults", path.string(), mod.name);
                continue;
            }
            if (const auto result = modFile->read(); !result) {
                logger::warn("The file '{}' of '{}' cannot be read: {}", path.string(), mod.name, result.error());
                read.notAvailable = std::format("Not available: {}", result.error());
                continue;
            }
            read.file = modFile.get();
        }

        _options.read(mod, &_clientCalls);
        addSettings(mod.frontPage.items, _settingList);
        addSettings(mod.pages, _settingList);
        for (const auto* setting : _settingList) {
            _settings.try_emplace(setting->id, setting);
            auto& entry = _entries[setting];
            const auto& options = _options.of(*setting);
            if (!controlValue(*setting, zeroOf(setting->valueType))) {
                entry.notAvailable = "Not available: its control does not show a value of its valueType.";
                continue;
            }
            if (const auto reason = _options.notAvailable(*setting); !reason.empty()) {
                entry.notAvailable = reason;
                continue;
            }
            if (const auto* unknown = std::get_if<model::UnknownStorage>(&setting->storage)) {
                entry.notAvailable = unknownStorageReason(*unknown);
                continue;
            }
            if (isInGame(setting->storage)) {
                auto stored = readGame(*setting);
                if (!stored) {
                    entry.notAvailable = std::format("Not available: {}.", stored.error());
                    continue;
                }
                entry.baseline = controlValue(*setting, *stored);
                if (!setting->readOnly) {
                    entry.invalid = invalidReason(*setting, options, *entry.baseline, gameName(setting->storage), "the game");
                }
                continue;
            }
            const auto& storage = std::get<model::FileStorage>(setting->storage);
            const auto file = files.find(storage.file);
            if (file == files.end() || !file->second.notAvailable.empty()) {
                entry.notAvailable = file != files.end() ? file->second.notAvailable : "Not available: the setting names no file of its config spec.";
                continue;
            }
            const auto* modFile = file->second.file;
            if (!modFile || !modFile->contains(storage.path)) {
                if (setting->readOnly) {
                    continue;
                }
                if (!setting->defaultValue) {
                    entry.notAvailable = "Not available: the file has no value of the setting, and its config spec gives it no default.";
                    continue;
                }
                entry.baseline = controlValue(*setting, *setting->defaultValue);
                continue;
            }
            // a binding is the text of its table, which its control reads
            auto stored = setting->type != model::SettingType::InputBinding ? modFile->value(storage.path, setting->valueType)
                                                                            : modFile->tableText(storage.path).transform([](std::string text) {
                                                                                  return model::Value(std::move(text));
                                                                              });
            if (!stored) {
                if (setting->readOnly) {
                    entry.notAvailable = std::format("Not available: {}.", stored.error());
                } else {
                    entry.invalid = std::format("Invalid: {}.", stored.error());
                }
                continue;
            }
            entry.baseline = controlValue(*setting, *stored);
            if (!setting->readOnly) {
                entry.invalid = invalidReason(*setting, options, *entry.baseline, modFile->pathName(storage.path), fileName(mod.files.at(storage.file)));
            }
        }
        logger::debug("Read {} settings of '{}', of {} files and of the game", _settingList.size(), mod.name, files.size());
    }

    /**
     * Where a file of the mod is: the path the mod's client answers for its `pathFromClientState`, as a mod that loads
     * another file at runtime has it, while that is a file of its format under the game's Data folder or `My Games`. Its
     * root and its path otherwise: with no state, no client, a client that does not answer it, and another path, which
     * the log names. The client is asked as the mod is selected and as its values are read again.
     */
    std::filesystem::path ConfigSpecStore::pathOf(const model::File& file) const
    {
        auto declared = (file.root == model::FileRoot::Data ? _dataFolder : _myGamesFolder) / filePath(file);
        if (file.pathFromClientState.empty()) {
            return declared;
        }
        const auto answer = _clientCalls.state(_modName, file.pathFromClientState);
        const auto* text = answer ? std::get_if<std::string>(&*answer) : nullptr;
        if (!text) {
            logger::debug("The client of '{}' gives no path in its state '{}': its file is '{}'", _modName, file.pathFromClientState, declared.string());
            return declared;
        }
        std::filesystem::path given(std::u8string(text->begin(), text->end()));
        const auto extension = given.extension().u8string();
        const auto format = file.format == model::FileFormat::Toml ? ".toml" : file.format == model::FileFormat::Ini ? ".ini" : ".json";
        if (!given.is_absolute() || common::str_tolower(std::string(extension.begin(), extension.end())) != format ||
            !(isInFolder(given, _dataFolder) || isInFolder(given, _myGamesFolder))) {
            logger::warn("The client of '{}' gives the path '{}' in its state '{}', which is no {} file under the game's Data folder or My Games: its file is '{}'",
                _modName,
                *text,
                file.pathFromClientState,
                format,
                declared.string());
            return declared;
        }
        logger::debug("The client of '{}' gives its file '{}' in its state '{}'", _modName, *text, file.pathFromClientState);
        return given;
    }

    /**
     * Ends the session of the mod: its values are forgotten, also the pending ones, why a Save failed and the
     * last Save, and the game has the baseline of each setting that is previewed again. The settings that take
     * effect after a restart that its Saves wrote are kept.
     * It does not read the mod, which can be gone by now: the menu's mods are replaced before it is called.
     */
    void ConfigSpecStore::close()
    {
        rollBack();
        if (!_modName.empty()) {
            showPage(nullptr);
            _clientCalls.send(_modName, model::ClientEvent::ModLeft);
        }
        _modName.clear();
        _modDisplayName.clear();
        _mod = nullptr;
        _files.clear();
        _filePaths.clear();
        _options.clear();
        _settingList.clear();
        _settings.clear();
        _entries.clear();
        _saveFailure.clear();
        _lastSave.reset();
    }

    /**
     * Nothing: the session's baseline is read as the mod is selected, and a value that changed in a file since
     * is kept by the next Save, unless the player changed the same setting.
     */
    void ConfigSpecStore::pageShown()
    {}

    /**
     * The value of a setting: its pending value, or else its baseline. One that is not available has the
     * reason, and so has one that is invalid: a pending value the player set so, or the file's value, which is
     * the setting's default where it is not of its type. A read-only setting whose file has no value of it is
     * not available, with no reason: it has no value to show.
     */
    model::StoredValue ConfigSpecStore::value(const model::Setting& setting)
    {
        const auto none = noneValue(setting, _options.of(setting));
        const auto found = _entries.find(&setting);
        if (found == _entries.end()) {
            return { .value = none, .available = false, .invalid = false, .reason = {} };
        }
        const auto& entry = found->second;
        if (!entry.notAvailable.empty()) {
            return { .value = none, .available = false, .invalid = false, .reason = entry.notAvailable };
        }
        if (entry.pending) {
            return { .value = *entry.pending, .available = true, .invalid = !entry.pendingInvalid.empty(), .reason = entry.pendingInvalid };
        }
        if (setting.readOnly && !entry.baseline) {
            return { .value = none, .available = false, .invalid = false, .reason = {} };
        }
        return { .value = entry.baseline.value_or(none), .available = true, .invalid = !entry.invalid.empty(), .reason = entry.invalid };
    }

    /**
     * The value of the setting of an id, which a condition names its setting by.
     */
    model::StoredValue ConfigSpecStore::valueById(const std::string_view id)
    {
        const auto found = _settings.find(id);
        return found != _settings.end() ? value(*found->second) : model::StoredValue{};
    }

    /**
     * Sets the pending value of a setting. A value that is the baseline leaves the setting clean, so the Save
     * does not write it. One that is no value of the setting, as a number outside its limits or a text that is
     * no number, is pending and invalid, with the reason, and Save writes nothing while it is. A `live` setting
     * of the game is previewed: the game has its value at once, but for an invalid one, which leaves the
     * preview before it. A value that is set ends the last Save, and its failure.
     * Returns false for a value that is not set: one of a setting that is read-only or not available, and one
     * that is not of the type its control shows.
     */
    bool ConfigSpecStore::set(const model::Setting& setting, const model::Value& value)
    {
        const auto found = _entries.find(&setting);
        if (setting.readOnly || found == _entries.end() || !found->second.notAvailable.empty()) {
            return false;
        }
        auto typed = typedValue(setting, value);
        if (!typed) {
            logger::debug("A value of another type than '{}' has is not set", setting.text);
            return false;
        }
        auto& entry = found->second;
        _saveFailure.clear();
        _lastSave.reset();
        if (entry.baseline && isSameValue(*typed, *entry.baseline)) {
            entry.pending.reset();
            entry.pendingInvalid.clear();
        } else {
            entry.pendingInvalid = typedReason(setting, _options.of(setting), *typed);
            entry.pending = std::move(*typed);
        }
        if (setting.applies == model::Applies::Live && isInGame(setting.storage) && entry.pendingInvalid.empty()) {
            preview(setting, entry);
        }
        previewToClient(setting, entry);
        return true;
    }

    /**
     * The default of a setting, as its config spec gives it, as its control has it.
     */
    std::optional<model::Value> ConfigSpecStore::defaultValue(const model::Setting& setting)
    {
        return setting.defaultValue ? controlValue(setting, *setting.defaultValue) : std::nullopt;
    }

    bool ConfigSpecStore::isDirty(const model::Setting& setting) const
    {
        const auto found = _entries.find(&setting);
        return found != _entries.end() && found->second.pending.has_value();
    }

    std::size_t ConfigSpecStore::dirtyCount() const
    {
        return static_cast<std::size_t>(std::ranges::count_if(_entries, [](const auto& entry) {
            return entry.second.pending.has_value();
        }));
    }

    std::size_t ConfigSpecStore::invalidCount() const
    {
        return static_cast<std::size_t>(std::ranges::count_if(_entries, [](const auto& entry) {
            return entry.second.pending && !entry.second.pendingInvalid.empty();
        }));
    }

    /**
     * When a change of a setting takes effect, as its `applies` says, also a `live` one of a file, which no native
     * client previews yet: its mod can act on the change in its own way. None for a read-only setting, which is
     * not set.
     */
    std::optional<model::Applies> ConfigSpecStore::applies(const model::Setting& setting) const
    {
        if (setting.readOnly) {
            return std::nullopt;
        }
        return setting.applies;
    }

    std::size_t ConfigSpecStore::dirtyRestartCount() const
    {
        return static_cast<std::size_t>(std::ranges::count_if(_entries, [this](const auto& entry) {
            return entry.second.pending && applies(*entry.first) == model::Applies::Restart;
        }));
    }

    std::size_t ConfigSpecStore::savedRestartCount(const model::Mod& mod) const
    {
        const auto found = _savedForRestart.find(mod.name);
        return found != _savedForRestart.end() ? found->second.size() : 0;
    }

    /**
     * Writes the dirty settings into their files, each file in one write, in the order of the config spec, and a
     * setting then has the value written as its baseline. A file is read again as it is written, so a value
     * that changed in it since the session started stays, but for one the player changed too, and a file that
     * is not there is created. The dirty settings of the game are written after every file, and the action of
     * each setting that was written runs then, in the order of the config spec, with the value written. The ones
     * written that take effect after a restart are kept for the mod, and how many they are is the last Save.
     * Then the mod's client is told once what the Save wrote: each setting, with its value before, and the files,
     * and each F4SE plugin of its config spec is sent the F4SE message of the Save.
     * Nothing is written before every file has its new text, checked to read back, and every global and
     * property is in the game, and the files already written are put back when a write fails: a Save that
     * fails changes no file and no value of the game, and keeps every setting dirty.
     * Returns the reason, which is in the log too, for a Save that does not write every dirty setting, as
     * `showcase.toml cannot be written`. While a dirty setting is invalid nothing is written, and the reason
     * is not a failure of the Save: the UI does not save then.
     */
    std::expected<void, std::string> ConfigSpecStore::save()
    {
        if (const auto invalid = invalidCount(); invalid > 0) {
            logger::warn("The changes of '{}' are not saved, as {} of them are not valid", _modName, invalid);
            return std::unexpected(std::format("{} value{} not valid", invalid, invalid == 1 ? " is" : "s are"));
        }
        _saveFailure.clear();
        _lastSave.reset();
        const auto failed = [this](const std::string& reason) {
            _saveFailure = reason;
            return std::unexpected(reason);
        };

        // the files that have dirty settings, with their names in the config spec and the settings
        struct FileSave
        {
            std::string_view name;
            ModFile* file;
            std::vector<const model::Setting*> settings;
        };

        std::vector<FileSave> saves;
        for (const auto& [name, file] : _files) {
            std::vector<ModFile::Change> changes;
            std::vector<const model::Setting*> settings;
            for (const auto* setting : _settingList) {
                const auto& entry = _entries.at(setting);
                const auto* storage = std::get_if<model::FileStorage>(&setting->storage);
                if (entry.pending && storage && storage->file == name) {
                    changes.push_back({ .path = storage->path, .value = storedValue(*setting, *entry.pending), .table = setting->type == model::SettingType::InputBinding });
                    settings.push_back(setting);
                }
            }
            if (changes.empty()) {
                continue;
            }
            if (const auto result = file->prepare(changes); !result) {
                logger::warn("The changes of '{}' are not saved, as its file '{}' cannot have them: {}", _modName, name, result.error());
                return failed(result.error());
            }
            saves.push_back({ .name = name, .file = file.get(), .settings = std::move(settings) });
        }
        for (const auto* setting : _settingList) {
            if (_entries.at(setting).pending && isInGame(setting->storage)) {
                if (const auto stored = readGame(*setting); !stored) {
                    logger::warn("The changes of '{}' are not saved, as a setting of the game is not available: {}", _modName, stored.error());
                    return failed(stored.error());
                }
            }
        }

        for (std::size_t next = 0; next < saves.size(); ++next) {
            if (const auto result = saves[next].file->commit(); !result) {
                logger::warn("The changes of '{}' are not saved, as its file '{}' cannot be written: {}", _modName, saves[next].name, result.error());
                for (const auto& written : std::span(saves).first(next) | std::views::reverse) {
                    if (const auto restored = written.file->restore(); !restored) {
                        logger::error("The file '{}' of '{}' is not put back as it was: {}", written.name, _modName, restored.error());
                    }
                }
                return failed(result.error());
            }
        }

        std::expected<void, std::string> saved;
        // each setting written, with the value it had before, which its mod's client is told
        std::unordered_map<const model::Setting*, model::Value> written;
        const auto wrote = [this, &written](const model::Setting* setting) {
            auto& entry = _entries.at(setting);
            written.emplace(setting, clientBaseline(*setting, entry.baseline, storedValue(*setting, *entry.pending)));
            entry.baseline = std::move(entry.pending);
            entry.pending.reset();
            entry.invalid.clear();
            // the file has the value the client shows now, which the batch tells it
            _clientPreviews.erase(setting);
        };
        for (const auto& fileSave : saves) {
            std::ranges::for_each(fileSave.settings, wrote);
        }
        for (const auto* setting : _settingList) {
            const auto& entry = _entries.at(setting);
            if (!entry.pending || !isInGame(setting->storage)) {
                continue;
            }
            if (!writeGame(setting->storage, storedValue(*setting, *entry.pending))) {
                logger::warn("The change of '{}' to {} is not saved: it is not in the game", _modName, gameName(setting->storage));
                if (saved) {
                    saved = std::unexpected(std::format("{} is not in the game", gameName(setting->storage)));
                }
                continue;
            }
            _previews.erase(setting);
            wrote(setting);
        }

        std::size_t restarts = 0;
        model::SaveBatch batch;
        for (const auto* setting : _settingList) {
            const auto before = written.find(setting);
            if (before == written.end()) {
                continue;
            }
            const auto& value = *_entries.at(setting).baseline;
            runAction(*setting, value, _clientCalls, _modName);
            if (applies(*setting) == model::Applies::Restart) {
                _savedForRestart[_modName].insert(storageKey(*setting));
                ++restarts;
            }
            batch.settings.push_back({ .setting = clientSetting(*setting), .before = before->second, .value = storedValue(*setting, value) });
        }
        _lastSave = restarts;
        logger::info("Saved {} changes of '{}', {} of them take effect after a restart", written.size(), _modName, restarts);
        if (!written.empty()) {
            for (const auto& fileSave : saves) {
                batch.files.push_back(clientFile(std::string(fileSave.name)));
            }
            batch.revision = ++_saveRevisions[_modName];
            _clientCalls.sendSave(_modName, batch);
            sendSavedMessage(_modName, _mod->f4sePlugins);
        }
        if (!saved) {
            return failed(saved.error());
        }
        return {};
    }

    /**
     * Takes back the pending values: each setting has its baseline again, also an invalid one of its file,
     * and the game has the baseline of each setting that is previewed again. The last Save is over, also one
     * that failed.
     */
    void ConfigSpecStore::discard()
    {
        rollBack();
        _saveFailure.clear();
        _lastSave.reset();
        for (auto& entry : _entries | std::views::values) {
            entry.pending.reset();
            entry.pendingInvalid.clear();
        }
    }

    /**
     * Takes back the pending value of one setting: it has its baseline again, also an invalid one of its file, the game
     * has the baseline of one it has a preview of, and the mod's client is told the Discard of one it was told the
     * preview of. It ends the last Save, and its failure, as a set does.
     */
    void ConfigSpecStore::discard(const model::Setting& setting)
    {
        const auto found = _entries.find(&setting);
        if (found == _entries.end() || !found->second.pending) {
            return;
        }
        auto& entry = found->second;
        _saveFailure.clear();
        _lastSave.reset();
        entry.pending.reset();
        entry.pendingInvalid.clear();
        if (_previews.contains(&setting)) {
            preview(setting, entry);
        }
        previewToClient(setting, entry);
    }

    /**
     * The baseline of a dirty setting: none for a clean one, and for one whose file has a value of another type.
     */
    std::optional<model::Value> ConfigSpecStore::savedValue(const model::Setting& setting)
    {
        const auto found = _entries.find(&setting);
        return found != _entries.end() && found->second.pending ? found->second.baseline : std::nullopt;
    }

    /**
     * A config spec has no hotkeys.
     */
    model::StoredValue ConfigSpecStore::hotkey(const model::Hotkey&)
    {
        return { .value = std::string(), .available = false, .invalid = false, .reason = "Not available: a config spec has no hotkeys." };
    }

    /**
     * Runs the action of a button at once: a `CallFunction` and a `CallGlobalFunction` call their function,
     * which Papyrus runs on its own time, and a `RunConsoleCommand` runs its line in the console.
     * Returns false for a row with no action, for an action the store does not run, and for a function whose
     * form or script is not in the game.
     */
    /**
     * The value of a state of the mod's client, which it asks once a frame. Not available while the client does not
     * answer it, and for a mod with no client.
     */
    model::StoredValue ConfigSpecStore::clientState(const std::string_view name)
    {
        auto key = common::str_tolower(std::string(name));
        auto found = _clientStates.find(key);
        if (found == _clientStates.end()) {
            found = _clientStates.emplace(std::move(key), _clientCalls.state(_modName, name)).first;
        }
        if (!found->second) {
            return { .value = {}, .available = false, .invalid = false, .reason = "Not available: its mod's client does not answer it." };
        }
        return { .value = *found->second, .available = true, .invalid = false, .reason = {} };
    }

    /**
     * Tells the mod's client that a page of its config spec that has an `id` is left and another is shown, as the
     * player goes to it. Nothing for the page that is shown already.
     */
    void ConfigSpecStore::showPage(const model::Page* page)
    {
        const std::string pageId = page ? page->id : std::string();
        if (_modName.empty() || pageId == _pageId) {
            return;
        }
        if (!_pageId.empty()) {
            _clientCalls.send(_modName, model::ClientEvent::PageLeft, _pageId);
        }
        _pageId = pageId;
        if (!_pageId.empty()) {
            _clientCalls.send(_modName, model::ClientEvent::PageShown, _pageId);
        }
    }

    /**
     * Reads the values of the mod's files and of the game again, as its own UI can have changed them while the menu
     * was minimized. A clean setting takes its new value as its baseline. A pending one stays the player's, as Save
     * keeps it, and is clean where the new value is the player's. One the game has a preview of keeps its baseline:
     * the game has the preview.
     */
    void ConfigSpecStore::readAgain()
    {
        if (!_mod) {
            return;
        }
        auto before = std::move(_entries);
        _entries.clear();
        _files.clear();
        _filePaths.clear();
        _settingList.clear();
        _settings.clear();
        _options.clear();
        readValues(*_mod);
        for (auto& [setting, entry] : _entries) {
            const auto found = before.find(setting);
            if (found == before.end() || !entry.notAvailable.empty()) {
                continue;
            }
            if (_previews.contains(setting)) {
                entry.baseline = found->second.baseline;
            }
            const auto& pending = found->second.pending;
            if (pending && !(entry.baseline && isSameValue(*pending, *entry.baseline))) {
                entry.pending = pending;
                entry.pendingInvalid = found->second.pendingInvalid;
            }
        }
        // a setting the client shows that is clean now, as its file has the player's value, ends its preview
        for (auto preview = _clientPreviews.begin(); preview != _clientPreviews.end();) {
            const auto entry = _entries.find(preview->first);
            if (entry == _entries.end() || !entry->second.pending) {
                discardOnClient(preview->second);
                preview = _clientPreviews.erase(preview);
            } else {
                ++preview;
            }
        }
        logger::info("Read the values of '{}' again", _modName);
    }

    /**
     * Forgets the states of the mod's client asked in the frame before, so each is asked again.
     */
    void ConfigSpecStore::startFrame()
    {
        _clientStates.clear();
    }

    /**
     * Why an action does not run: a function of the mod's DLL whose client is not there, is cut off, or did not
     * register it. Empty for any other.
     */
    std::string ConfigSpecStore::notRunnable(const model::Action& action) const
    {
        const auto* native = std::get_if<model::NativeFunction>(&action);
        return native ? _clientCalls.notRunnable(_modName, _modDisplayName, native->function) : std::string();
    }

    /**
     * Runs an action at once: a script, console command, desktop web link or function of the mod's DLL.
     * A browser launch or native function that fails gives a message for the button's help line.
     */
    model::ActionResult ConfigSpecStore::run(const model::Action& action)
    {
        if (const auto* call = std::get_if<model::CallFunction>(&action)) {
            return { .ran = callFunction(*call), .failed = false, .message = {} };
        }
        if (const auto* command = std::get_if<model::ConsoleCommand>(&action)) {
            return { .ran = runConsoleCommand(*command), .failed = false, .message = {} };
        }
        if (const auto* link = std::get_if<model::WebLink>(&action)) {
            const bool opened = openWebLink(*link);
            return { .ran = true, .failed = !opened, .message = opened ? "Opened in your desktop browser." : "Could not open the link in your desktop browser." };
        }
        if (const auto* native = std::get_if<model::NativeFunction>(&action); native && notRunnable(action).empty()) {
            auto result = _clientCalls.call(_modName, native->function, native->arguments);
            return { .ran = true, .failed = !result.done, .message = std::move(result.message) };
        }
        return {};
    }

    /**
     * Writes the value a `live` setting of the game has now into the game: its pending value, as a preview,
     * or its baseline, as it is set back. The baseline of one that is previewed is kept, which a Discard
     * writes back, and so does the end of the session.
     */
    void ConfigSpecStore::preview(const model::Setting& setting, const Entry& entry)
    {
        const auto& shown = entry.pending ? *entry.pending : *entry.baseline;
        if (!writeGame(setting.storage, storedValue(setting, shown))) {
            logger::warn("The change of '{}' to {} is not previewed: it is not in the game", _modName, gameName(setting.storage));
            return;
        }
        if (entry.pending) {
            _previews.insert_or_assign(&setting, Preview{ .storage = setting.storage, .baseline = storedValue(setting, *entry.baseline) });
        } else {
            _previews.erase(&setting);
        }
    }

    /**
     * Tells the mod's client the change of a `live` setting: its valid pending value, as a Preview, with the value it was
     * told before, its baseline at first, and a Discard back to the baseline as the setting is clean again. A setting of
     * the game is in it already, as the store previews it first, and the client can do what more it needs. An invalid
     * pending value leaves the client with the preview before it. Nothing for a setting that is not `live`.
     */
    void ConfigSpecStore::previewToClient(const model::Setting& setting, const Entry& entry)
    {
        const auto* storage = std::get_if<model::FileStorage>(&setting.storage);
        if (setting.applies != model::Applies::Live || (!storage && !isInGame(setting.storage)) || !_mod) {
            return;
        }
        const auto found = _clientPreviews.find(&setting);
        if (!entry.pending) {
            if (found != _clientPreviews.end()) {
                discardOnClient(found->second);
                _clientPreviews.erase(found);
            }
            return;
        }
        if (!entry.pendingInvalid.empty()) {
            return;
        }
        auto value = storedValue(setting, *entry.pending);
        if (found != _clientPreviews.end()) {
            auto& change = found->second.change;
            if (isSameValue(change.value, value)) {
                return;
            }
            change.previous = std::exchange(change.value, std::move(value));
            change.phase = model::ChangePhase::Preview;
            _clientCalls.sendChange(_modName, change);
            return;
        }
        const auto baseline = clientBaseline(setting, entry.baseline, value);
        ClientPreview preview{ .change = { .setting = clientSetting(setting), .previous = baseline, .value = std::move(value), .phase = model::ChangePhase::Preview },
            .baseline = baseline };
        _clientCalls.sendChange(_modName, preview.change);
        _clientPreviews.emplace(&setting, std::move(preview));
    }

    /**
     * A setting as the mod's client is told it: its id and type, and where it is kept, in a file of the mod or in the
     * game.
     */
    model::ClientSetting ConfigSpecStore::clientSetting(const model::Setting& setting) const
    {
        model::ClientSetting told{ .id = setting.id, .valueType = setting.valueType };
        if (const auto* storage = std::get_if<model::FileStorage>(&setting.storage)) {
            told.file = clientFile(storage->file);
            told.path = storage->path;
        } else if (const auto* global = std::get_if<model::GlobalStorage>(&setting.storage)) {
            told.form = formText(global->plugin, global->formId);
        } else if (const auto* property = std::get_if<model::PropertyStorage>(&setting.storage)) {
            told.form = formText(property->plugin, property->formId);
            told.script = property->script;
            told.property = property->name;
        }
        return told;
    }

    /**
     * A file of the mod as its client is told it: its name in the config spec, its format and its absolute path, from
     * the game's Data folder or from `My Games`. One the config spec does not have has no path.
     */
    model::ClientFile ConfigSpecStore::clientFile(const std::string& name) const
    {
        const auto file = _mod->files.find(name);
        if (file == _mod->files.end()) {
            return { .name = name };
        }
        const auto found = _filePaths.find(name);
        auto path = found != _filePaths.end() ? found->second : pathOf(file->second);
        // the API's texts are UTF-8
        const auto text = path.make_preferred().u8string();
        return { .name = name, .format = file->second.format, .path = std::string(text.begin(), text.end()) };
    }

    /**
     * Tells the mod's client that a setting it shows a preview of is back at its baseline.
     */
    void ConfigSpecStore::discardOnClient(ClientPreview& preview) const
    {
        preview.change.previous = std::exchange(preview.change.value, preview.baseline);
        preview.change.phase = model::ChangePhase::Discard;
        _clientCalls.sendChange(_modName, preview.change);
    }

    /**
     * Writes the baseline of each setting of the game that is previewed back into the game, and tells the mod's client
     * that each `live` setting it shows a preview of is back at its baseline, so no preview outlives the change it shows.
     */
    void ConfigSpecStore::rollBack()
    {
        // the game has the baseline before the client is told it
        for (const auto& preview : _previews | std::views::values) {
            writeGame(preview.storage, preview.baseline);
        }
        _previews.clear();
        for (auto& preview : _clientPreviews | std::views::values) {
            discardOnClient(preview);
        }
        _clientPreviews.clear();
    }
}
