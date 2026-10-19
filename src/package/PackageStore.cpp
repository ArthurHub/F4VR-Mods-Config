#include "PackageStore.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <format>
#include <memory>
#include <ranges>
#include <variant>
#include <vector>

#include "TomlFile.h"
#include "common/CommonUtils.h"

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
     * Whether a value is the value of an option of a choice.
     */
    bool isOption(const model::Setting& setting, const model::Value& value)
    {
        return std::ranges::any_of(setting.valueOptions.options, [&value](const model::Option& option) {
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
     * A path as TOML writes it as a dotted key, `Panel.fScale`, for a reason.
     */
    std::string pathText(const std::vector<std::string>& path)
    {
        return path | std::views::join_with('.') | std::ranges::to<std::string>();
    }

    /**
     * The path of a package's file from its root, as written in its package.
     */
    std::filesystem::path filePath(const model::File& file)
    {
        return std::filesystem::path(std::u8string(file.path.begin(), file.path.end()));
    }

    /**
     * The name of a package's file, which names it in a reason, as a reason of a TOML file does.
     */
    std::string fileName(const model::File& file)
    {
        const auto name = filePath(file).filename().u8string();
        return { name.begin(), name.end() };
    }

    /**
     * A value of the type a setting keeps, as its control has it: a slider has a number, also for a whole
     * number, and a text input that takes a number has the text of it. None for a value its control does not
     * show: a switcher of anything but true or false, and a slider of a text.
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
     * The value a setting shows while it has none of its own: its default, or else its least, its first option,
     * or the zero of its type.
     */
    model::Value noneValue(const model::Setting& setting)
    {
        if (setting.defaultValue) {
            if (auto value = controlValue(setting, *setting.defaultValue)) {
                return std::move(*value);
            }
        }
        if (setting.type == model::SettingType::Slider) {
            return setting.valueOptions.min;
        }
        if (isChoice(setting) && !setting.valueOptions.options.empty()) {
            return setting.valueOptions.options.front().value;
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
     * A value the player sets, as its control has it, if it is one of its setting: inside the limits of a
     * number, and an option of a choice. A number of a whole number setting is the nearest whole number, and
     * the text of a number is written as short as it can be. None for a value that is not one.
     */
    std::optional<model::Value> acceptedValue(const model::Setting& setting, const model::Value& value)
    {
        switch (setting.type) {
        case model::SettingType::Switcher:
            if (std::holds_alternative<bool>(value)) {
                return value;
            }
            break;
        case model::SettingType::Slider:
            if (const auto* number = std::get_if<float>(&value)) {
                const float stored = setting.valueType == model::StoredType::Int ? std::round(*number) : *number;
                if (inLimits(setting, stored)) {
                    return stored;
                }
            }
            break;
        case model::SettingType::Stepper:
        case model::SettingType::Dropdown:
            if (isOption(setting, value)) {
                return value;
            }
            break;
        case model::SettingType::TextInput:
            if (const auto* text = std::get_if<std::string>(&value); text && !isNumberField(setting)) {
                return value;
            } else if (text) {
                const auto number = fieldNumber(setting, *text);
                if (number && inLimits(setting, *number)) {
                    return setting.valueType == model::StoredType::Int ? std::format("{}", static_cast<int>(*number)) : std::format("{}", *number);
                }
            }
            break;
        }
        return std::nullopt;
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
     * Why a value in a file, as its control has it, is not one of its setting: a number outside the limits,
     * or no option of a choice. Empty for a value that is one. It names the value by its path and its file,
     * as a reason of a TOML file does.
     */
    std::string invalidReason(const model::Setting& setting, const model::Value& value, const std::string& path, const std::string& file)
    {
        const auto number = setting.type == model::SettingType::Slider ? std::optional(std::get<float>(value))
                            : isNumberField(setting)                   ? fieldNumber(setting, std::get<std::string>(value))
                                                                       : std::nullopt;
        if (number && !inLimits(setting, *number)) {
            return std::format("Invalid: {} is {} in {}, which is not from {} to {}.", path, valueText(value), file, setting.valueOptions.min, setting.valueOptions.max);
        }
        if (isChoice(setting) && !isOption(setting, value)) {
            return std::format("Invalid: {} is {} in {}, which is none of its options.", path, valueText(value), file);
        }
        return {};
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
     * A file of a package as the session read it. A file that is not there has neither a TOML file nor a
     * reason: its settings have their defaults.
     */
    struct FileOfSession
    {
        std::unique_ptr<package::TomlFile> toml;

        // why its settings are not available, and empty for a file that is read or is not there
        std::string notAvailable;
    };

    /**
     * Why a setting kept in a storage the store does not read is not available.
     */
    std::string unknownStorageReason(const model::UnknownStorage& storage)
    {
        if (storage.type == "global" || storage.type == "property") {
            return std::format("Not available: the menu does not read a package's setting that is kept in a {} yet.", storage.type);
        }
        return storage.type == "file" ? "Not available: the setting names no file of its package, or no value of it." : "Not available: the setting has no storage.";
    }
}

namespace mods_config::package
{
    PackageStore::PackageStore(std::filesystem::path dataFolder, std::filesystem::path myGamesFolder)
        : _dataFolder(std::move(dataFolder)),
          _myGamesFolder(std::move(myGamesFolder))
    {}

    /**
     * Starts the session of a mod, after it ends the one of the mod before it: its files are read now, and
     * the value of each setting is its baseline.
     * A file that is not there has none of its values, and a key the file does not have is its setting's
     * default: both are clean, and the first Save that changes one of them writes it. A file that cannot be
     * read, or is no TOML, has its settings not available, with the reason, and so has a file the menu does
     * not read. A value that is not of its setting's type, outside its limits or no option of its choice is
     * invalid, with the reason.
     * A read-only setting is never invalid: it has the value of its file as it is, none where the file has
     * none, and is not available where the file's value is not of its type.
     */
    void PackageStore::open(const model::Mod& mod)
    {
        close();
        _modName = mod.name;

        std::map<std::string, FileOfSession, std::less<>> files;
        for (const auto& [name, file] : mod.files) {
            auto& read = files[name];
            if (file.format != model::FileFormat::Toml) {
                read.notAvailable = std::format("Not available: the menu does not read {} files yet.", file.format == model::FileFormat::Ini ? "INI" : "JSON");
                continue;
            }
            const auto& root = file.root == model::FileRoot::Data ? _dataFolder : _myGamesFolder;
            const auto path = root / filePath(file);
            _tomlFiles.emplace(name, path);
            if (!std::filesystem::exists(path)) {
                logger::debug("The file '{}' of '{}' is not there: its settings have their defaults", path.string(), mod.name);
                continue;
            }
            read.toml = std::make_unique<TomlFile>(path);
            if (const auto result = read.toml->read(); !result) {
                logger::warn("The file '{}' of '{}' cannot be read: {}", path.string(), mod.name, result.error());
                read.notAvailable = std::format("Not available: {}", result.error());
                read.toml.reset();
            }
        }

        addSettings(mod.frontPage.items, _settingList);
        addSettings(mod.pages, _settingList);
        for (const auto* setting : _settingList) {
            _settings.try_emplace(setting->id, setting);
            auto& entry = _entries[setting];
            if (!controlValue(*setting, zeroOf(setting->valueType))) {
                entry.notAvailable = "Not available: its control does not show a value of its valueType.";
                continue;
            }
            if (const auto* unknown = std::get_if<model::UnknownStorage>(&setting->storage)) {
                entry.notAvailable = unknownStorageReason(*unknown);
                continue;
            }
            const auto& storage = std::get<model::FileStorage>(setting->storage);
            const auto file = files.find(storage.file);
            if (file == files.end() || !file->second.notAvailable.empty()) {
                entry.notAvailable = file != files.end() ? file->second.notAvailable : "Not available: the setting names no file of its package.";
                continue;
            }
            const auto* toml = file->second.toml.get();
            if (!toml || !toml->contains(storage.path)) {
                if (setting->readOnly) {
                    continue;
                }
                if (!setting->defaultValue) {
                    entry.notAvailable = "Not available: the file has no value of the setting, and its package gives it no default.";
                    continue;
                }
                entry.baseline = controlValue(*setting, *setting->defaultValue);
                continue;
            }
            auto stored = toml->value(storage.path, setting->valueType);
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
                entry.invalid = invalidReason(*setting, *entry.baseline, pathText(storage.path), fileName(mod.files.at(storage.file)));
            }
        }
        logger::debug("Read {} settings of '{}' from {} files", _settingList.size(), mod.name, files.size());
    }

    /**
     * Ends the session of the mod: its values are forgotten, also the pending ones.
     * It does not read the mod, which can be gone by now: the menu's mods are replaced before it is called.
     */
    void PackageStore::close()
    {
        _modName.clear();
        _tomlFiles.clear();
        _settingList.clear();
        _settings.clear();
        _entries.clear();
    }

    /**
     * Nothing: the session's baseline is read as the mod is selected, and a value that changed in a file since
     * is kept by the next Save, unless the player changed the same setting.
     */
    void PackageStore::pageShown()
    {}

    /**
     * The value of a setting: its pending value, or else its baseline. One that is not available has the
     * reason, and so has one that is invalid, which is the setting's default where the file's value is not of
     * its type. A read-only setting whose file has no value of it is not available, with no reason: it has
     * no value to show.
     */
    model::StoredValue PackageStore::value(const model::Setting& setting)
    {
        const auto found = _entries.find(&setting);
        if (found == _entries.end()) {
            return { .value = noneValue(setting), .available = false, .invalid = false, .reason = {} };
        }
        const auto& entry = found->second;
        if (!entry.notAvailable.empty()) {
            return { .value = noneValue(setting), .available = false, .invalid = false, .reason = entry.notAvailable };
        }
        if (entry.pending) {
            return { .value = *entry.pending, .available = true, .invalid = false, .reason = {} };
        }
        if (setting.readOnly && !entry.baseline) {
            return { .value = noneValue(setting), .available = false, .invalid = false, .reason = {} };
        }
        return { .value = entry.baseline.value_or(noneValue(setting)), .available = true, .invalid = !entry.invalid.empty(), .reason = entry.invalid };
    }

    /**
     * The value of the setting of an id, which a condition names its setting by.
     */
    model::StoredValue PackageStore::valueById(const std::string_view id)
    {
        const auto found = _settings.find(id);
        return found != _settings.end() ? value(*found->second) : model::StoredValue{};
    }

    /**
     * Sets the pending value of a setting. A value that is the baseline leaves the setting clean, so the Save
     * does not write it. Returns false for a value that is not set: one of a setting that is read-only or not
     * available, and one that is not a value of the setting, as a number outside its limits.
     */
    bool PackageStore::set(const model::Setting& setting, const model::Value& value)
    {
        const auto found = _entries.find(&setting);
        if (setting.readOnly || found == _entries.end() || !found->second.notAvailable.empty()) {
            return false;
        }
        auto accepted = acceptedValue(setting, value);
        if (!accepted) {
            logger::debug("A value that is not one of '{}' is not set", setting.text);
            return false;
        }
        auto& entry = found->second;
        if (entry.baseline && isSameValue(*accepted, *entry.baseline)) {
            entry.pending.reset();
        } else {
            entry.pending = std::move(*accepted);
        }
        return true;
    }

    /**
     * The default of a setting, as its package gives it, as its control has it.
     */
    std::optional<model::Value> PackageStore::defaultValue(const model::Setting& setting)
    {
        return setting.defaultValue ? controlValue(setting, *setting.defaultValue) : std::nullopt;
    }

    bool PackageStore::isDirty(const model::Setting& setting) const
    {
        const auto found = _entries.find(&setting);
        return found != _entries.end() && found->second.pending.has_value();
    }

    std::size_t PackageStore::dirtyCount() const
    {
        return static_cast<std::size_t>(std::ranges::count_if(_entries, [](const auto& entry) {
            return entry.second.pending.has_value();
        }));
    }

    /**
     * Writes the dirty settings into their files, each file in one write, in the order of the package, and a
     * setting then has the value written as its baseline. A file is read again as it is written, so a value
     * that changed in it since the session started stays, but for one the player changed too.
     * A file that cannot be written keeps its settings dirty, with the reason in the log, and the other files
     * are still written. Returns whether every dirty setting is written.
     */
    bool PackageStore::save()
    {
        bool saved = true;
        std::size_t written = 0;
        for (const auto& [name, path] : _tomlFiles) {
            std::vector<TomlFile::Change> changes;
            std::vector<Entry*> entries;
            for (const auto* setting : _settingList) {
                auto& entry = _entries.at(setting);
                const auto* storage = std::get_if<model::FileStorage>(&setting->storage);
                if (entry.pending && storage && storage->file == name) {
                    changes.push_back({ .path = storage->path, .value = storedValue(*setting, *entry.pending) });
                    entries.push_back(&entry);
                }
            }
            if (changes.empty()) {
                continue;
            }
            if (const auto result = TomlFile(path).write(changes); !result) {
                logger::warn("The changes of '{}' to '{}' are not saved: {}", _modName, path.string(), result.error());
                saved = false;
                continue;
            }
            for (auto* entry : entries) {
                entry->baseline = std::move(entry->pending);
                entry->pending.reset();
                entry->invalid.clear();
            }
            written += changes.size();
        }
        logger::info("Saved {} changes of '{}'", written, _modName);
        return saved;
    }

    /**
     * Takes back the pending values: each setting has its baseline again, also an invalid one of its file.
     */
    void PackageStore::discard()
    {
        for (auto& entry : _entries | std::views::values) {
            entry.pending.reset();
        }
    }

    /**
     * A package has no hotkeys.
     */
    model::StoredValue PackageStore::hotkey(const model::Hotkey&)
    {
        return { .value = std::string(), .available = false, .invalid = false, .reason = "Not available: a package has no hotkeys." };
    }

    /**
     * Runs no action: a package's button runs none yet.
     */
    bool PackageStore::run(const model::Action&)
    {
        return false;
    }
}
