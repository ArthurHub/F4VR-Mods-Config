#include "PackageLoader.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <cstdint>
#include <format>
#include <fstream>
#include <map>
#include <optional>
#include <sstream>
#include <unordered_map>

#include <nlohmann/json.hpp>

#include "common/CommonUtils.h"
#include "model/TextMarkup.h"

using namespace mods_config;

namespace
{
    using Json = nlohmann::json;

    // where the packages are, from the game's folder: a folder each, with its `config.json`
    constexpr auto PACKAGE_CONFIG_FOLDER = R"(Data\ModsConfigVR\Config)";

    // the most lines a spacer is high, whatever its package says, as in an MCM config
    constexpr float MAX_SPACER_LINES = 20.0f;

    /**
     * The text of a key. It is empty when the value has no such key, or the key holds something else than a text.
     */
    std::string getText(const Json& value, const char* key)
    {
        const auto found = value.find(key);
        return found != value.end() && found->is_string() ? found->get<std::string>() : std::string();
    }

    /**
     * A text of a key that the player sees, in the game's language. It is plain text, a `$` token, or a token
     * with the text that is shown where the language has none, as `{ "token": "$MyMod_Height", "fallback":
     * "Height" }`. Empty for a key that is none of them.
     */
    std::string getShownText(const Json& value, const char* key, const model::Translations& translations)
    {
        const auto found = value.find(key);
        if (found == value.end()) {
            return {};
        }
        if (found->is_string()) {
            return translations.translate(found->get<std::string>());
        }
        return found->is_object() ? translations.translate(getText(*found, "token"), getText(*found, "fallback")) : std::string();
    }

    /**
     * A number of a key, or else the given one.
     */
    float getNumber(const Json& value, const char* key, const float otherwise)
    {
        const auto found = value.find(key);
        return found != value.end() && found->is_number() ? found->get<float>() : otherwise;
    }

    /**
     * A length of a key: a whole number above 0, or else 0, which is no limit.
     */
    int getLength(const Json& value, const char* key)
    {
        const auto found = value.find(key);
        return found != value.end() && found->is_number_integer() ? (std::max)(found->get<int>(), 0) : 0;
    }

    /**
     * Whether a key is true. False for a value with no such key, or with one that holds something else than a bool.
     */
    bool getBool(const Json& value, const char* key)
    {
        const auto found = value.find(key);
        return found != value.end() && found->is_boolean() && found->get<bool>();
    }

    /**
     * A row's translated label, without tags where `html` is true, for search and the help line.
     */
    std::string getLabel(const Json& row, const model::Translations& translations)
    {
        auto text = getShownText(row, "text", translations);
        return getBool(row, "html") ? model::removeMarkup(text) : text;
    }

    /**
     * A label's styled pieces on one line where `html` is true. Other labels are drawn plain.
     */
    std::vector<model::TextRun> getLabelRuns(const Json& row, const model::Translations& translations)
    {
        return getBool(row, "html") ? model::markupRuns(getShownText(row, "text", translations)) : std::vector<model::TextRun>{};
    }

    /**
     * The `applies` of a package, a page, a section or a setting. None for one that has none, and for one
     * that is not `live`, `save` or `restart`.
     */
    std::optional<model::Applies> findApplies(const Json& value)
    {
        const auto text = getText(value, "applies");
        if (text == "live") {
            return model::Applies::Live;
        }
        if (text == "save") {
            return model::Applies::Save;
        }
        return text == "restart" ? std::optional(model::Applies::Restart) : std::nullopt;
    }

    /**
     * What the items of a page or a section get from above them: the file a setting is kept in where its
     * storage names none, and when its change takes effect.
     */
    struct Inherited
    {
        std::string defaultFile;
        model::Applies applies = model::Applies::Live;
    };

    /**
     * What a page, a section or a setting has for what is under it: its own `defaultFile` and `applies`, or
     * else what it gets from above. An `applies` that is not one is passed over, with the reason in the log.
     */
    Inherited inherit(const Json& value, const Inherited& above)
    {
        Inherited own = above;
        if (auto file = getText(value, "defaultFile"); !file.empty()) {
            own.defaultFile = std::move(file);
        }
        if (const auto applies = findApplies(value)) {
            own.applies = *applies;
        } else if (value.contains("applies")) {
            logger::warn("The applies of '{}' is not live, save or restart", getText(value, "text"));
        }
        return own;
    }

    using FormatName = std::pair<std::string_view, model::FileFormat>;

    // what a file of a package is written in, by its `format`, which is also its extension
    constexpr std::array FILE_FORMATS{
        FormatName{ "ini", model::FileFormat::Ini },
        FormatName{ "json", model::FileFormat::Json },
        FormatName{ "toml", model::FileFormat::Toml },
    };

    /**
     * A file of a package's `files`, or the reason it is not one: a `format` of `ini`, `json` or `toml`,
     * which its extension is, a `root` of `data` or `myGames`, and a `path` from that root. The path is
     * checked as it is written: one that is absolute, or goes up with `..`, could be any file of the system.
     */
    std::expected<model::File, std::string> loadFile(const Json& declared)
    {
        const auto formatText = getText(declared, "format");
        const auto format = std::ranges::find(FILE_FORMATS, formatText, &FormatName::first);
        if (format == FILE_FORMATS.end()) {
            return std::unexpected(std::format("its format '{}' is not ini, json or toml", formatText));
        }
        const auto rootText = getText(declared, "root");
        if (rootText != "data" && rootText != "myGames") {
            return std::unexpected(std::format("its root '{}' is not data or myGames", rootText));
        }
        auto path = getText(declared, "path");
        const std::filesystem::path checked(std::u8string(path.begin(), path.end()));
        if (path.empty() || checked.has_root_name() || checked.has_root_directory()) {
            return std::unexpected(std::format("its path '{}' is not one from its root", path));
        }
        if (std::ranges::any_of(checked, [](const std::filesystem::path& part) {
                return part == "..";
            })) {
            return std::unexpected(std::format("its path '{}' goes up with ..", path));
        }
        const auto extension = checked.extension().u8string();
        if (common::str_tolower(std::string(extension.begin(), extension.end())) != "." + formatText) {
            return std::unexpected(std::format("its path '{}' does not end in .{}", path, formatText));
        }
        return model::File{ .format = format->second, .root = rootText == "data" ? model::FileRoot::Data : model::FileRoot::MyGames, .path = std::move(path) };
    }

    /**
     * The files of a package, by the names its `files` gives them. A file that is not written as one is
     * left out, with the reason in the log, so its settings are not available.
     */
    std::map<std::string, model::File> loadFiles(const Json& package)
    {
        std::map<std::string, model::File> files;
        const auto found = package.find("files");
        if (found == package.end() || !found->is_object()) {
            return files;
        }
        for (const auto& [name, declared] : found->items()) {
            auto file = declared.is_object() ? loadFile(declared) : std::unexpected("it is not an object"s);
            if (file) {
                files.emplace(name, std::move(*file));
            } else {
                logger::warn("The file '{}' of package '{}' is left out: {}", name, getText(package, "modName"), file.error());
            }
        }
        return files;
    }

    /**
     * What a condition knows of the setting it names: its id as its package has it, and what its value is.
     */
    struct SettingInfo
    {
        std::string id;
        model::SettingType type = model::SettingType::Switcher;
        model::StoredType valueType = model::StoredType::Bool;
    };

    /**
     * What a package is read with: the game's translations, its files, its shared lists, the plugins its
     * conditions ask of, and its settings so far by their ids in lower case, as ids are compared without
     * regard to case.
     */
    struct PackageReading
    {
        const model::Translations& translations;
        const std::map<std::string, model::File>& files;

        // the `sharedLists` of its root. None for a package with none
        const Json* sharedLists = nullptr;
        const package::PluginChecks& plugins;
        std::unordered_map<std::string, SettingInfo> settings;
    };

    /**
     * The name a package gives one of its files, as the package declares it: a storage can name it in
     * another case. None for a name of no file.
     */
    std::optional<std::string> findFile(const std::map<std::string, model::File>& files, const std::string& name)
    {
        const auto lower = common::str_tolower(name);
        const auto found = std::ranges::find_if(files, [&lower](const auto& file) {
            return common::str_tolower(file.first) == lower;
        });
        return found != files.end() ? std::optional(found->first) : std::nullopt;
    }

    /**
     * A form of a game plugin, written as its plugin and its id in hex, as `MyMod.esp|800`, which can start
     * with `0x`. None for a text that is not written so.
     */
    std::optional<std::pair<std::string, std::uint32_t>> parseForm(const std::string& text)
    {
        const auto separator = text.find('|');
        auto plugin = common::trim(text.substr(0, separator));
        auto id = separator == std::string::npos ? std::string() : common::trim(text.substr(separator + 1));
        if (id.starts_with("0x") || id.starts_with("0X")) {
            id.erase(0, 2);
        }
        std::uint32_t formId = 0;
        const auto [end, error] = std::from_chars(id.data(), id.data() + id.size(), formId, 16);
        if (plugin.empty() || id.empty() || error != std::errc() || end != id.data() + id.size()) {
            return std::nullopt;
        }
        return std::pair(std::move(plugin), formId);
    }

    /**
     * Where a setting keeps its value: a global, as `{ "global": "MyMod.esp|1234" }`, a property of a script,
     * by its `form`, its `propertyName` and its `scriptName`, which is optional, in the setting's `valueType`,
     * or a value of the file its `storage` names, or else of the nearest `defaultFile` above it. The value of a
     * file is named by its `path`, or by its `section` and its `key`, as an INI key is. A storage that is not
     * written as one, as a form that is not a plugin and an id in hex, a property with no name, or no file of
     * the package or no value of it, as a path of more than two in an INI file, is one the menu does not read,
     * with the reason in the log.
     */
    model::Storage loadStorage(const Json& row, const std::string& defaultFile, const model::StoredType valueType, const PackageReading& reading)
    {
        const auto storage = row.find("storage");
        if (storage == row.end() || !storage->is_object()) {
            logger::warn("The setting '{}' has no storage", getText(row, "text"));
            return model::UnknownStorage{};
        }
        if (storage->contains("global")) {
            const auto global = getText(*storage, "global");
            if (auto form = parseForm(global)) {
                return model::GlobalStorage{ .plugin = std::move(form->first), .formId = form->second };
            }
            logger::warn("The global '{}' of the setting '{}' is not a plugin and an id in hex", global, getText(row, "text"));
            return model::UnknownStorage{ .type = "global" };
        }
        if (storage->contains("form")) {
            const auto formText = getText(*storage, "form");
            auto form = parseForm(formText);
            auto name = getText(*storage, "propertyName");
            if (form && !name.empty()) {
                return model::PropertyStorage{ .plugin = std::move(form->first),
                    .formId = form->second,
                    .script = getText(*storage, "scriptName"),
                    .name = std::move(name),
                    .type = valueType };
            }
            logger::warn("The property '{}' of the form '{}' of the setting '{}' is not a name, and a plugin and an id in hex", name, formText, getText(row, "text"));
            return model::UnknownStorage{ .type = "property" };
        }

        std::vector<std::string> path;
        if (const auto names = storage->find("path"); names != storage->end() && names->is_array()) {
            for (const auto& name : *names) {
                if (!name.is_string() || name.get_ref<const std::string&>().empty()) {
                    path.clear();
                    break;
                }
                path.push_back(name.get<std::string>());
            }
        } else if (auto key = getText(*storage, "key"); !key.empty()) {
            auto section = getText(*storage, "section");
            path = section.empty() ? std::vector{ std::move(key) } : std::vector{ std::move(section), std::move(key) };
        }

        auto fileName = getText(*storage, "file");
        const auto file = findFile(reading.files, fileName.empty() ? defaultFile : fileName);
        if (!file || path.empty() || (reading.files.at(*file).format == model::FileFormat::Ini && path.size() > 2)) {
            logger::warn("The setting '{}' names no file of its package, or no value of it", getText(row, "text"));
            return model::UnknownStorage{ .type = "file" };
        }
        return model::FileStorage{ .file = *file, .path = std::move(path) };
    }

    /**
     * The id a setting gets from where its value is kept: the last name of its path, and the names before it
     * joined by dots, as `fScale:Panel` for `Panel`, `fScale`. A path of two is named as MCM names a key of an
     * INI file, so a mod that moves from INI to TOML keeps its ids. A path of one is its name alone. Empty for
     * a storage the menu does not read.
     */
    std::string idOf(const model::Storage& storage)
    {
        const auto* file = std::get_if<model::FileStorage>(&storage);
        if (!file || file->path.empty()) {
            return {};
        }
        std::string before;
        for (std::size_t i = 0; i + 1 < file->path.size(); ++i) {
            before += (i == 0 ? "" : ".") + file->path[i];
        }
        return before.empty() ? file->path.back() : std::format("{}:{}", file->path.back(), before);
    }

    using SettingTypeName = std::pair<std::string_view, model::SettingType>;

    // the rows that edit a value, by their `type`
    constexpr std::array SETTING_TYPES{
        SettingTypeName{ "switcher", model::SettingType::Switcher },
        SettingTypeName{ "slider", model::SettingType::Slider },
        SettingTypeName{ "stepper", model::SettingType::Stepper },
        SettingTypeName{ "dropdown", model::SettingType::Dropdown },
        SettingTypeName{ "textinput", model::SettingType::TextInput },
    };

    /**
     * Whether a setting is a choice: a stepper or a dropdown.
     */
    bool isChoice(const model::SettingType type)
    {
        return type == model::SettingType::Stepper || type == model::SettingType::Dropdown;
    }

    using ValueTypeName = std::pair<std::string_view, model::StoredType>;

    // what a setting's value is, by its `valueType`
    constexpr std::array VALUE_TYPES{
        ValueTypeName{ "bool", model::StoredType::Bool },
        ValueTypeName{ "int", model::StoredType::Int },
        ValueTypeName{ "float", model::StoredType::Float },
        ValueTypeName{ "string", model::StoredType::String },
    };

    /**
     * What a setting's value is, by its `valueType`. One with none, or with one that is not a type, has the
     * type of its control: a bool for a switcher, a number for a slider, the place of an option for a choice,
     * and a text for a text input.
     */
    model::StoredType loadValueType(const Json& row, const model::SettingType type)
    {
        const auto found = std::ranges::find(VALUE_TYPES, getText(row, "valueType"), &ValueTypeName::first);
        if (found != VALUE_TYPES.end()) {
            return found->second;
        }
        if (type == model::SettingType::Switcher) {
            return model::StoredType::Bool;
        }
        if (isChoice(type)) {
            return model::StoredType::Int;
        }
        return type == model::SettingType::Slider ? model::StoredType::Float : model::StoredType::String;
    }

    /**
     * The value of a key in a type: a JSON bool for a bool, a whole number for an int, any number for a
     * float, and a text for a string. None for a key that has no value of the type.
     */
    std::optional<model::Value> getValue(const Json& value, const char* key, const model::StoredType type)
    {
        const auto found = value.find(key);
        if (found == value.end()) {
            return std::nullopt;
        }
        if (type == model::StoredType::Bool && found->is_boolean()) {
            return found->get<bool>();
        }
        if (type == model::StoredType::Int && found->is_number_integer()) {
            return found->get<int>();
        }
        if (type == model::StoredType::Float && found->is_number()) {
            return found->get<float>();
        }
        if (type == model::StoredType::String && found->is_string()) {
            return found->get<std::string>();
        }
        return std::nullopt;
    }

    /**
     * The shared list of a package by its name, which a choice can name in another case. None for a name of
     * no list.
     */
    const Json* findSharedList(const Json* sharedLists, const std::string& name)
    {
        if (!sharedLists) {
            return nullptr;
        }
        const auto lower = common::str_tolower(name);
        for (const auto& [listName, list] : sharedLists->items()) {
            if (common::str_tolower(listName) == lower) {
                return &list;
            }
        }
        return nullptr;
    }

    /**
     * The options of a choice, in the type of its value. A text is an option of the short form, whose value
     * is its place in the list, for a choice of an int. An object is an option with its own `value` and its
     * `text`. Any other option, and one whose value is not of the choice's type, is left out, with the reason
     * in the log.
     */
    std::vector<model::Option> loadOptions(const Json& list, const Json& row, const model::StoredType valueType, const PackageReading& reading)
    {
        std::vector<model::Option> options;
        for (std::size_t place = 0; place < list.size(); ++place) {
            const auto& option = list[place];
            if (option.is_string() && valueType == model::StoredType::Int) {
                options.push_back({ .value = static_cast<int>(place), .text = reading.translations.translate(option.get<std::string>()) });
                continue;
            }
            auto value = option.is_object() ? getValue(option, "value", valueType) : std::nullopt;
            if (!value) {
                logger::warn("Option {} of '{}' is left out: it is not a text of a choice of an int, or an object with a value of its type", place + 1, getText(row, "text"));
                continue;
            }
            options.push_back({ .value = std::move(*value), .text = getShownText(option, "text", reading.translations) });
        }
        return options;
    }

    /**
     * Where a choice reads its options in the game: a `listFromForm` names a FormList, as `MyMod.esp|900`, and a
     * `listFromProperty` a property of a script, by its `form`, its `propertyName` and its `scriptName`, which
     * is optional. A choice of an int stores the place of an option, and one of a string its text. None for a
     * choice with neither, and with the reason in the log for a list that is not written so, and for a choice
     * of another type.
     */
    std::optional<model::GameOptions> loadGameOptions(const Json& valueOptions, const Json& row, const model::StoredType valueType)
    {
        model::GameOptions list{ .storesText = valueType == model::StoredType::String };
        const auto property = valueOptions.find("listFromProperty");
        if (valueOptions.contains("listFromForm")) {
            const auto formList = getText(valueOptions, "listFromForm");
            auto form = parseForm(formList);
            if (!form) {
                logger::warn("The FormList '{}' of the choice '{}' is not a plugin and an id in hex", formList, getText(row, "text"));
                return std::nullopt;
            }
            list.plugin = std::move(form->first);
            list.formId = form->second;
        } else if (property != valueOptions.end() && property->is_object()) {
            const auto formText = getText(*property, "form");
            auto form = parseForm(formText);
            list.property = getText(*property, "propertyName");
            if (!form || list.property.empty()) {
                logger::warn("The property '{}' of the form '{}' that the choice '{}' lists is not a name, and a plugin and an id in hex",
                    list.property,
                    formText,
                    getText(row, "text"));
                return std::nullopt;
            }
            list.plugin = std::move(form->first);
            list.formId = form->second;
            list.script = getText(*property, "scriptName");
        } else {
            return std::nullopt;
        }
        if (valueType != model::StoredType::Int && valueType != model::StoredType::String) {
            logger::warn("The choice '{}' lists its options in the game, so its valueType is int or string", getText(row, "text"));
            return std::nullopt;
        }
        return list;
    }

    /**
     * The `valueOptions` of a setting: the least, the most and the step of a number, the fewest and the most
     * characters of a text input of a text, and the options of a choice: its own `options`, the shared list
     * its `sharedOptions` names, or where it reads them in the game. An `allowEmpty` of false is a least of
     * one character. A length that is no whole number above 0 is none.
     * A choice with none of them has no options, with the reason in the log, so it is disabled.
     */
    model::ValueOptions loadValueOptions(const Json& row, const model::SettingType type, const model::StoredType valueType, const PackageReading& reading)
    {
        model::ValueOptions valueOptions;
        const auto found = row.find("valueOptions");
        const bool hasOptions = found != row.end() && found->is_object();
        if (hasOptions) {
            valueOptions.min = getNumber(*found, "min", valueOptions.min);
            valueOptions.max = getNumber(*found, "max", valueOptions.max);
            valueOptions.step = getNumber(*found, "step", valueOptions.step);
        }
        if (hasOptions && type == model::SettingType::TextInput && valueType == model::StoredType::String) {
            valueOptions.minLength = getLength(*found, "minLength");
            valueOptions.maxLength = getLength(*found, "maxLength");
            const auto allowEmpty = found->find("allowEmpty");
            if (allowEmpty != found->end() && allowEmpty->is_boolean() && !allowEmpty->get<bool>()) {
                valueOptions.minLength = (std::max)(valueOptions.minLength, 1);
            }
        }
        if (!isChoice(type)) {
            return valueOptions;
        }

        const Json* list = nullptr;
        if (hasOptions) {
            const auto own = found->find("options");
            list = own != found->end() ? &*own : findSharedList(reading.sharedLists, getText(*found, "sharedOptions"));
        }
        if (list && list->is_array()) {
            valueOptions.options = loadOptions(*list, row, valueType, reading);
            return valueOptions;
        }
        if (hasOptions) {
            valueOptions.gameOptions = loadGameOptions(*found, row, valueType);
        }
        if (!valueOptions.gameOptions) {
            logger::warn("The choice '{}' has no list of options, or names no shared list of its package", getText(row, "text"));
        }
        return valueOptions;
    }

    /**
     * Whether two values of a setting are the same one: two numbers are when they are as near as a number and
     * the text of it in a file.
     */
    bool isSameValue(const model::Value& value, const model::Value& other)
    {
        const auto* number = std::get_if<float>(&value);
        const auto* otherNumber = std::get_if<float>(&other);
        return number && otherNumber ? common::fEqual(*number, *otherNumber) : value == other;
    }

    /**
     * Whether a value of a setting's type is one of its values: an option of a choice, a number inside
     * the limits of a setting that has them, its least and its most among them, and a text of the length its
     * setting allows. Any value is one of a choice that reads its options in the game, which are not known
     * until then.
     */
    bool isValueOf(const model::Value& value, const model::SettingType type, const model::ValueOptions& options)
    {
        if (isChoice(type)) {
            return options.gameOptions || std::ranges::any_of(options.options, [&value](const model::Option& option) {
                return isSameValue(option.value, value);
            });
        }
        if (const auto* text = std::get_if<std::string>(&value)) {
            const int length = model::textLength(*text);
            return length >= options.minLength && (options.maxLength == 0 || length <= options.maxLength);
        }
        const auto* whole = std::get_if<int>(&value);
        const auto* point = std::get_if<float>(&value);
        if ((!whole && !point) || !(options.max > options.min)) {
            return true;
        }
        const float number = whole ? static_cast<float>(*whole) : *point;
        return (number > options.min || common::fEqual(number, options.min)) && (number < options.max || common::fEqual(number, options.max));
    }

    /**
     * The `default` of a setting, in its value's type. None for a setting with no default of its type, or
     * with one that is not one of its values: outside its limits, of a length it does not allow, or none of
     * its options. Both are errors of the package that the log has, and the setting then has no Reset.
     */
    std::optional<model::Value> loadDefault(const Json& row, const model::SettingType type, const model::StoredType valueType, const model::ValueOptions& options)
    {
        auto value = getValue(row, "default", valueType);
        if (!value) {
            logger::warn("The setting '{}' has no default of the type of its value", getText(row, "text"));
        } else if (!isValueOf(*value, type, options)) {
            logger::warn("The default of the setting '{}' is not one of its values: it is outside its limits, of a length it does not allow, or none of its options",
                getText(row, "text"));
            value.reset();
        }
        return value;
    }

    // the most decimals a number's display can have, as MCM's menu writes a number at the most
    constexpr int MAX_DECIMALS = 6;

    /**
     * The `display` of a setting: its decimals, its unit, which is a text the player sees, and whether it is a
     * percentage. Decimals that are no whole number from 0 to 6 are none, with the reason in the log.
     */
    model::NumberDisplay loadDisplay(const Json& row, const model::Translations& translations)
    {
        const auto found = row.find("display");
        if (found == row.end() || !found->is_object()) {
            return {};
        }
        model::NumberDisplay display{ .decimals = {}, .unit = getShownText(*found, "unit", translations), .percent = getBool(*found, "percent") };
        if (const auto decimals = found->find("decimals"); decimals != found->end()) {
            if (decimals->is_number_integer() && decimals->get<int>() >= 0 && decimals->get<int>() <= MAX_DECIMALS) {
                display.decimals = decimals->get<int>();
            } else {
                logger::warn("The decimals of the setting '{}' are not a whole number from 0 to {}", getText(row, "text"), MAX_DECIMALS);
            }
        }
        return display;
    }

    // the least and the most a whole number of Papyrus is
    constexpr double MIN_WHOLE = -2147483648.0;
    constexpr double MAX_WHOLE = 2147483647.0;

    /**
     * A parameter of a call, in its own type as a JSON value: a number written with no point or exponent is a
     * whole number, any other number is a number, a bool is a bool and a text is a text, which is never read
     * for a type. None for any other value, and for a whole number that Papyrus does not have.
     */
    std::optional<model::Value> loadArgument(const Json& param)
    {
        if (param.is_boolean()) {
            return param.get<bool>();
        }
        if (param.is_number_integer()) {
            const auto whole = param.get<double>();
            return whole >= MIN_WHOLE && whole <= MAX_WHOLE ? std::optional<model::Value>(static_cast<int>(whole)) : std::nullopt;
        }
        if (param.is_number()) {
            return param.get<float>();
        }
        return param.is_string() ? std::optional<model::Value>(param.get<std::string>()) : std::nullopt;
    }

    /**
     * The function a `CallFunction` or a `CallGlobalFunction` calls, with its `params`, or why it is not one.
     * A `CallFunction` names the form of its script in `form`, as `MyMod.esp|800`, and can name the script
     * in `scriptName`. A `CallGlobalFunction` names its script in `script`, and has no form.
     */
    std::expected<model::CallFunction, std::string> loadCall(const Json& action, const std::string& type)
    {
        model::CallFunction call{ .plugin = {}, .formId = 0, .script = {}, .function = getText(action, "function"), .arguments = {}, .valueArguments = {} };
        if (call.function.empty()) {
            return std::unexpected(std::format("its {} names no function", type));
        }
        if (type == "CallGlobalFunction") {
            call.script = getText(action, "script");
            if (call.script.empty()) {
                return std::unexpected("its CallGlobalFunction names no script"s);
            }
        } else {
            const auto formText = getText(action, "form");
            auto form = parseForm(formText);
            if (formText.empty()) {
                return std::unexpected("its CallFunction names no form"s);
            }
            if (!form) {
                return std::unexpected(std::format("the form '{}' of its CallFunction is not a plugin and an id in hex", formText));
            }
            call.plugin = std::move(form->first);
            call.formId = form->second;
            call.script = getText(action, "scriptName");
        }

        const auto params = action.find("params");
        if (params == action.end()) {
            return call;
        }
        if (!params->is_array()) {
            return std::unexpected(std::format("the params of its {} are not a list", type));
        }
        for (std::size_t place = 0; place < params->size(); ++place) {
            auto argument = loadArgument((*params)[place]);
            if (!argument) {
                return std::unexpected(std::format("parameter {} of its {} is not a whole number of Papyrus, a number, a bool or a text", place + 1, type));
            }
            call.arguments.push_back(std::move(*argument));
        }
        return call;
    }

    /**
     * An action of a button or a setting that is not written right, which the menu does not run: the help line
     * of a button has the mistake as its reason, and the log has it too.
     */
    model::UnknownAction mistakenAction(const Json& row, std::string type, const std::string& mistake)
    {
        logger::warn("The action of '{}' is not run: {}", getText(row, "text"), mistake);
        return model::UnknownAction{ .type = std::move(type), .reason = std::format("Not available: {}.", mistake) };
    }

    /**
     * What a button runs, by the `type` of its `action`: a `CallFunction` and a `CallGlobalFunction`, with
     * their `params`, and a `RunConsoleCommand`, with its `command`. A `CallNativeFunction` runs a function
     * that the mod's DLL gives the menu, and the menu takes none from a DLL. A button with no action, any
     * other action, and one that is not written right are actions the menu does not run, with the reason.
     */
    model::Action loadAction(const Json& row)
    {
        const auto action = row.find("action");
        if (action == row.end() || !action->is_object()) {
            return mistakenAction(row, {}, "the button has no action");
        }
        auto type = getText(*action, "type");
        if (type == "CallFunction" || type == "CallGlobalFunction") {
            auto call = loadCall(*action, type);
            return call ? model::Action(std::move(*call)) : mistakenAction(row, std::move(type), call.error());
        }
        if (type == "RunConsoleCommand") {
            auto command = getText(*action, "command");
            return command.empty() ? model::Action(mistakenAction(row, std::move(type), "its RunConsoleCommand has no command")) : model::ConsoleCommand{ std::move(command) };
        }
        if (type.empty()) {
            return mistakenAction(row, {}, "the action has no type");
        }
        logger::info("'{}' has an action '{}', which the menu does not run", getText(row, "text"), type);
        auto reason =
            type == "CallNativeFunction" ? "Not available: the mod's DLL has not given the menu its functions."s : std::format("Not available: the menu does not run a {}.", type);
        return model::UnknownAction{ .type = std::move(type), .reason = std::move(reason) };
    }

    /**
     * A button: its text, its action and the text on its button, `buttonText`, which is `Run` without one. Its
     * conditions and its warnings are read once every setting of the package is (`loadConditions`).
     */
    model::Button loadButton(const Json& row, const PackageReading& reading)
    {
        return model::Button{ .text = getLabel(row, reading.translations),
            .textRuns = getLabelRuns(row, reading.translations),
            .help = getShownText(row, "help", reading.translations),
            .buttonText = getShownText(row, "buttonText", reading.translations),
            .action = loadAction(row),
            .visibleWhen = {},
            .enabledWhen = {},
            .warnings = {} };
    }

    // the parameter of a setting's action that is its value
    constexpr auto VALUE_PARAMETER = "{value}";

    /**
     * What a setting runs as Save writes its value: an action as a button's, of which a parameter that is
     * exactly `"{value}"` is the value written, in the setting's `valueType`. A setting with no `action` runs
     * nothing, and so does one whose action the menu does not run, with the reason in the log.
     */
    model::Action loadSettingAction(const Json& row, const model::StoredType valueType)
    {
        if (!row.contains("action")) {
            return model::NoAction{};
        }
        auto action = loadAction(row);
        if (auto* call = std::get_if<model::CallFunction>(&action)) {
            for (std::size_t place = 0; place < call->arguments.size(); ++place) {
                if (call->arguments[place] == model::Value(std::string(VALUE_PARAMETER))) {
                    call->valueArguments.push_back({ .place = place, .text = VALUE_PARAMETER, .type = valueType });
                }
            }
        }
        return action;
    }

    /**
     * A setting of a package: its author's `id`, or one it gets from where its value is kept, its own
     * `applies` or the nearest above it, its `default`, how its number is shown, the options of a choice, and
     * its action, which Save runs.
     * A read-only setting has no default, as it has no Reset. Two settings with one id are an error of the
     * package, which the log has: an author gives one of them an id of its own. Its conditions and its
     * warnings are read once every setting of the package is (`loadConditions`).
     */
    model::Setting loadSetting(const Json& row, const model::SettingType type, const Inherited& above, PackageReading& reading)
    {
        const auto own = inherit(row, above);
        const auto valueType = loadValueType(row, type);
        auto storage = loadStorage(row, own.defaultFile, valueType, reading);
        auto id = getText(row, "id");
        if (id.empty()) {
            id = idOf(storage);
        }
        if (!id.empty() && !reading.settings.emplace(common::str_tolower(id), SettingInfo{ .id = id, .type = type, .valueType = valueType }).second) {
            logger::warn("Two settings have the id '{}': give one of them an id of its own", id);
        }
        auto valueOptions = loadValueOptions(row, type, valueType, reading);
        const bool readOnly = getBool(row, "readOnly");
        auto defaultValue = readOnly ? std::nullopt : loadDefault(row, type, valueType, valueOptions);
        return model::Setting{ .id = std::move(id),
            .sourceId = {},
            .type = type,
            .text = getLabel(row, reading.translations),
            .textRuns = getLabelRuns(row, reading.translations),
            .help = getShownText(row, "help", reading.translations),
            .valueOptions = std::move(valueOptions),
            .storage = std::move(storage),
            .valueType = valueType,
            .action = loadSettingAction(row, valueType),
            .visibleWhen = {},
            .enabledWhen = {},
            .applies = own.applies,
            .defaultValue = std::move(defaultValue),
            .display = loadDisplay(row, reading.translations),
            .readOnly = readOnly,
            .warnings = {} };
    }

    /**
     * A text row: its text in lines, and with the tags of an MCM config read where its `html` is true.
     */
    model::Text loadText(const Json& row, const PackageReading& reading)
    {
        const auto text = getShownText(row, "text", reading.translations);
        return model::Text{ .lines = getBool(row, "html") ? model::parseMarkup(text) : model::plainLines(text), .visibleWhen = {} };
    }

    /**
     * A spacer: as high as its `numLines`, from 1 to 20, and one line without it.
     */
    model::Spacer loadSpacer(const Json& row)
    {
        const auto lines = std::clamp(getNumber(row, "numLines", 1.0f), 1.0f, MAX_SPACER_LINES);
        return model::Spacer{ .lines = static_cast<int>(lines), .visibleWhen = {} };
    }

    /**
     * An image: its `texture`, a path under `Data/Textures`, and its `width` and `height` in the panel's pixels,
     * of which a side that is not more than 0 is none. One with no texture shows that an image is there, with
     * the reason in the log.
     */
    model::Image loadImage(const Json& row)
    {
        auto texture = getText(row, "texture");
        if (texture.empty()) {
            logger::warn("An image names no texture");
        }
        return model::Image{ .pixels = {},
            .texture = std::move(texture),
            .width = (std::max)(getNumber(row, "width", 0.0f), 0.0f),
            .height = (std::max)(getNumber(row, "height", 0.0f), 0.0f),
            .visibleWhen = {} };
    }

    /**
     * The items of the `content` of a page or a section, by their `type`, one for each row. A section is a
     * group of the items of its own `content`, and passes its `defaultFile` and its `applies` down to them.
     * It folds with `isCollapsible`, and starts folded with `collapsed`: one that does not fold could not be
     * opened, so its `collapsed` is left out, and the log says so.
     * Any other row the menu does not have is a placeholder with its type.
     */
    std::vector<model::Item> loadItems(const Json& container, const Inherited& above, PackageReading& reading)
    {
        std::vector<model::Item> items;
        const auto content = container.find("content");
        if (content == container.end() || !content->is_array()) {
            return items;
        }
        for (const auto& row : *content) {
            const auto type = getText(row, "type");
            if (type == "section") {
                const bool collapsible = getBool(row, "isCollapsible");
                if (getBool(row, "collapsed") && !collapsible) {
                    logger::warn("The section '{}' is collapsed and does not fold, so it is open", getText(row, "text"));
                }
                items.emplace_back(model::Group{ .text = getShownText(row, "text", reading.translations),
                    .help = getShownText(row, "help", reading.translations),
                    .items = loadItems(row, inherit(row, above), reading),
                    .visibleWhen = {},
                    .collapsible = collapsible,
                    .collapsed = collapsible && getBool(row, "collapsed") });
                continue;
            }
            if (type == "text") {
                items.emplace_back(loadText(row, reading));
                continue;
            }
            if (type == "spacer") {
                items.emplace_back(loadSpacer(row));
                continue;
            }
            if (type == "image") {
                items.emplace_back(loadImage(row));
                continue;
            }
            if (type == "button") {
                items.emplace_back(loadButton(row, reading));
                continue;
            }
            if (const auto setting = std::ranges::find(SETTING_TYPES, type, &SettingTypeName::first); setting != SETTING_TYPES.end()) {
                items.emplace_back(loadSetting(row, setting->second, above, reading));
                continue;
            }
            items.emplace_back(model::Placeholder{ .text = getLabel(row, reading.translations),
                .textRuns = getLabelRuns(row, reading.translations),
                .help = getShownText(row, "help", reading.translations),
                .type = type,
                .visibleWhen = {} });
        }
        return items;
    }

    /**
     * A page of a package, with the pages under it, which get its `defaultFile` and its `applies` as its
     * items do.
     */
    model::Page loadPage(const Json& page, const Inherited& above, PackageReading& reading)
    {
        const auto own = inherit(page, above);
        model::Page loaded{ .name = getShownText(page, "pageDisplayName", reading.translations),
            .help = getShownText(page, "help", reading.translations),
            .items = loadItems(page, own, reading),
            .pages = {} };
        if (const auto pages = page.find("pages"); pages != page.end() && pages->is_array()) {
            for (const auto& under : *pages) {
                loaded.pages.push_back(loadPage(under, own, reading));
            }
        }
        return loaded;
    }

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
     * "oneOf": [1, 2] }`. It names the setting by its id in any case, and has the id as the package gives it.
     * Or the reason it is not one: a setting the package does not have, a value of another type than the
     * setting's, an order of a value that is no number, and a `oneOf` or a `notOneOf` of a setting that is
     * neither a text nor a choice.
     */
    std::expected<model::Condition, std::string> loadComparison(const Json& condition, const PackageReading& reading)
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
        if (isList && setting.valueType != model::StoredType::String && !isChoice(setting.type)) {
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
     * A condition of a package, or the reason it is not one: a comparison of a setting's value, `all` and
     * `any` of a list of conditions, `not` of one, or whether a `plugin` or an `f4sePlugin` is loaded.
     * Whether a plugin is loaded does not change while the game runs, so it is answered here, as an `all` of
     * none, which holds, or an `any` of none, which does not.
     * A condition with a part that is not one is not one as a whole.
     */
    std::expected<model::Condition, std::string> loadCondition(const Json& condition, const PackageReading& reading)
    {
        if (!condition.is_object()) {
            return std::unexpected("a condition is not an object"s);
        }
        if (condition.size() == 2 && condition.contains("setting")) {
            return loadComparison(condition, reading);
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

    /**
     * The condition of a key of a row, as `visibleWhen`. None for a row with none, and for one that is not a
     * condition, with the reason in the log: the row is then shown and enabled as if it had none, so the
     * mistake is seen and nothing is hidden by it.
     */
    std::optional<model::Condition> loadRowCondition(const Json& row, const char* key, const PackageReading& reading)
    {
        const auto found = row.find(key);
        if (found == row.end()) {
            return std::nullopt;
        }
        auto condition = loadCondition(*found, reading);
        if (!condition) {
            logger::warn("The {} of '{}' is left out: {}", key, getText(row, "text"), condition.error());
            return std::nullopt;
        }
        return std::move(*condition);
    }

    /**
     * The `warnings` of a setting or a button: a text each, which the row tells while its `when` holds. One
     * with no text, and one whose `when` is not a condition, is left out, with the reason in the log.
     */
    std::vector<model::Warning> loadWarnings(const Json& row, const PackageReading& reading)
    {
        std::vector<model::Warning> warnings;
        const auto found = row.find("warnings");
        if (found == row.end()) {
            return warnings;
        }
        if (!found->is_array()) {
            logger::warn("The warnings of '{}' are left out: they are not a list", getText(row, "text"));
            return warnings;
        }
        for (const auto& warning : *found) {
            auto text = warning.is_object() ? getShownText(warning, "text", reading.translations) : std::string();
            if (text.empty()) {
                logger::warn("A warning of '{}' is left out: it has no text", getText(row, "text"));
                continue;
            }
            const auto when = warning.find("when");
            auto condition = when != warning.end() ? loadCondition(*when, reading) : std::unexpected("it has no when"s);
            if (!condition) {
                logger::warn("A warning of '{}' is left out: {}", getText(row, "text"), condition.error());
                continue;
            }
            warnings.push_back(model::Warning{ .when = std::move(*condition), .text = std::move(text) });
        }
        return warnings;
    }

    /**
     * A condition that needs both: an All of the two, or the one there is.
     */
    std::optional<model::Condition> join(const std::optional<model::Condition>& above, std::optional<model::Condition> own)
    {
        if (!above || !own) {
            return above ? above : own;
        }
        return model::Condition{ .kind = Kind::All, .setting = {}, .conditions = { *above, std::move(*own) } };
    }

    /**
     * What a row is shown and enabled by: its own conditions, and those of the sections it is in.
     */
    struct RowConditions
    {
        std::optional<model::Condition> visibleWhen;
        std::optional<model::Condition> enabledWhen;
    };

    /**
     * Gives the items of the `content` of a page or a section their conditions: a row's `visibleWhen` and
     * `enabledWhen`, each joined with those of the sections it is in, so a row needs every condition above
     * it, and the warnings of a setting and a button. A section's heading is shown by its `visibleWhen` too.
     * The items are the rows of the content, one
     * for each, in their order, as `loadItems` reads them.
     */
    void loadConditions(const Json& container, std::vector<model::Item>& items, const RowConditions& above, const PackageReading& reading)
    {
        const auto content = container.find("content");
        if (content == container.end() || !content->is_array()) {
            return;
        }
        for (std::size_t place = 0; place < content->size() && place < items.size(); ++place) {
            const auto& row = (*content)[place];
            auto& item = items[place];
            const RowConditions own{ .visibleWhen = join(above.visibleWhen, loadRowCondition(row, "visibleWhen", reading)),
                .enabledWhen = join(above.enabledWhen, loadRowCondition(row, "enabledWhen", reading)) };
            std::visit(
                [&own](auto& loaded) {
                    loaded.visibleWhen = own.visibleWhen;
                },
                item);
            if (auto* setting = std::get_if<model::Setting>(&item)) {
                setting->enabledWhen = own.enabledWhen;
                setting->warnings = loadWarnings(row, reading);
            } else if (auto* button = std::get_if<model::Button>(&item)) {
                button->enabledWhen = own.enabledWhen;
                button->warnings = loadWarnings(row, reading);
            } else if (auto* group = std::get_if<model::Group>(&item)) {
                loadConditions(row, group->items, own, reading);
            }
        }
    }

    /**
     * Gives the items of a page and of the pages under it their conditions, as `loadPage` read them.
     */
    void loadPageConditions(const Json& page, model::Page& loaded, const PackageReading& reading)
    {
        loadConditions(page, loaded.items, {}, reading);
        const auto pages = page.find("pages");
        if (pages == page.end() || !pages->is_array()) {
            return;
        }
        for (std::size_t place = 0; place < pages->size() && place < loaded.pages.size(); ++place) {
            loadPageConditions((*pages)[place], loaded.pages[place], reading);
        }
    }

    /**
     * Whether a game plugin is loaded in the running game: one that is in the Data folder and not enabled is not.
     */
    bool isGamePluginLoaded(const std::string_view plugin)
    {
        auto* const dataHandler = RE::TESDataHandler::GetSingleton();
        return dataHandler && dataHandler->LookupLoadedFile(plugin).first;
    }

    void ignoreMessage(F4SE::MessagingInterface::Message*)
    {}

    /**
     * Whether an F4SE plugin is loaded, by the name it registers with F4SE. F4SE VR has no call that asks it,
     * so the mod registers a listener of the plugin's messages, which F4SE refuses for a plugin that is not
     * loaded. The listener does nothing, and stays: F4SE keeps the first listener the mod registers for a
     * plugin, so the mod cannot listen to the messages of a plugin it asked of.
     */
    bool isF4sePluginLoaded(const std::string_view plugin)
    {
        const auto* messaging = F4SE::GetMessagingInterface();
        const std::string name(plugin);
        return messaging && messaging->RegisterListener(ignoreMessage, name);
    }

    /**
     * A path as UTF-8 text for the log. A path's own string() throws on a name the system's code page lacks.
     */
    std::string toLogText(const std::filesystem::path& path)
    {
        const auto text = path.u8string();
        return { text.begin(), text.end() };
    }

    /**
     * Names as a reason says them: `A`, `A and B`, `A, B and C`.
     */
    std::string joinNames(const std::vector<std::string>& names)
    {
        std::string joined;
        for (std::size_t i = 0; i < names.size(); ++i) {
            joined += (i == 0 ? "" : i + 1 == names.size() ? " and " : ", ") + names[i];
        }
        return joined;
    }

    /**
     * The plugins of a list of a package's requirements that are not loaded, in its order. None with no check.
     */
    std::vector<std::string> findMissing(const Json& package, const char* key, const package::PluginCheck& isLoaded)
    {
        std::vector<std::string> missing;
        const auto required = package.find(key);
        if (!isLoaded || required == package.end() || !required->is_array()) {
            return missing;
        }
        for (const auto& plugin : *required) {
            if (plugin.is_string() && !isLoaded(plugin.get_ref<const std::string&>())) {
                missing.push_back(plugin.get<std::string>());
            }
        }
        return missing;
    }

    /**
     * Why a package's mod is not available while a plugin it requires is not loaded: a game plugin of its
     * `pluginRequirements` or an F4SE plugin of its `f4sePluginRequirements`, as `its plugin MyMod.esp and
     * its F4SE plugin MyMod are not loaded`. Empty while every one is, and with no checks, as in a test of a
     * package alone.
     */
    std::string findMissingRequirements(const Json& package, const package::PluginChecks& plugins)
    {
        const auto game = findMissing(package, "pluginRequirements", plugins.isGamePluginLoaded);
        const auto f4se = findMissing(package, "f4sePluginRequirements", plugins.isF4sePluginLoaded);
        std::vector<std::string> parts;
        if (!game.empty()) {
            parts.push_back(std::format("its plugin{} {}", game.size() == 1 ? "" : "s", joinNames(game)));
        }
        if (!f4se.empty()) {
            parts.push_back(std::format("its F4SE plugin{} {}", f4se.size() == 1 ? "" : "s", joinNames(f4se)));
        }
        if (parts.empty()) {
            return {};
        }
        return std::format("{} {} not loaded", joinNames(parts), game.size() + f4se.size() == 1 ? "is" : "are");
    }

    /**
     * The `minSchemaVersion` of a package: a whole number from 1. None for a package that has none.
     */
    std::optional<std::int64_t> findSchemaVersion(const Json& package)
    {
        const auto version = package.find("minSchemaVersion");
        if (version == package.end() || !version->is_number_integer() || version->get<std::int64_t>() < 1) {
            return std::nullopt;
        }
        return version->get<std::int64_t>();
    }

    /**
     * A mod that is not available, which has its names and the reason alone.
     */
    model::Mod notAvailableMod(std::string name, std::string displayName, std::string reason)
    {
        return model::Mod{ .name = std::move(name),
            .displayName = std::move(displayName),
            .frontPage = {},
            .pages = {},
            .notAvailable = std::move(reason),
            .help = {},
            .source = model::Source::Package,
            .files = {} };
    }
}

namespace mods_config::package
{
    /**
     * Reads the text of a package's `config.json` into the model: the mod and its help, its files, its front
     * page and its pages, which nest, with their help, and their sections, which nest and fold, with their
     * help, settings, buttons, texts, spacers and images. Every
     * text the player sees is in the game's language. The package's root has to say its `minSchemaVersion`. A
     * setting has the file its value is kept in, and when its change takes effect, which the package's root has
     * to say. A choice has its options or a list of
     * the package's `sharedLists`. A row is shown and enabled by its conditions, which are read once every
     * setting is, as a condition can name a setting after it. A condition on a plugin is answered by the checks.
     * A mod whose package needs a newer schema than the menu reads, or whose required plugins are not all
     * loaded, is not available, and is its names and the reason alone: the rest of its package is not read.
     * The error says why a package cannot be read. Comments are skipped, as in an MCM config.
     */
    std::expected<model::Mod, std::string> loadPackage(const std::string_view text, const model::Translations& translations, const PluginChecks& plugins)
    {
        Json package;
        try {
            package = Json::parse(text, nullptr, true, true);
        } catch (const Json::parse_error& e) {
            return std::unexpected(std::format("Not valid JSON: {}", e.what()));
        }
        if (!package.is_object()) {
            return std::unexpected("Not a JSON object"s);
        }

        model::Mod mod;
        mod.name = getText(package, "modName");
        if (mod.name.empty()) {
            return std::unexpected("No modName"s);
        }
        const auto schemaVersion = findSchemaVersion(package);
        if (!schemaVersion) {
            return std::unexpected("No minSchemaVersion of a whole number from 1"s);
        }
        mod.source = model::Source::Package;
        mod.displayName = getShownText(package, "displayName", translations);
        if (mod.displayName.empty()) {
            mod.displayName = mod.name;
        }
        // a newer schema can say anything in another way, so nothing more of it is read
        auto reason = *schemaVersion > SCHEMA_VERSION ? std::format("its package needs a newer Mods Config VR, one that reads schema version {}", *schemaVersion)
                                                      : findMissingRequirements(package, plugins);
        if (!reason.empty()) {
            return notAvailableMod(std::move(mod.name), std::move(mod.displayName), std::move(reason));
        }
        const auto applies = findApplies(package);
        if (!applies) {
            return std::unexpected("No applies of live, save or restart"s);
        }
        mod.help = getShownText(package, "help", translations);

        auto files = loadFiles(package);
        const auto sharedLists = package.find("sharedLists");
        PackageReading reading{ .translations = translations,
            .files = files,
            .sharedLists = sharedLists != package.end() && sharedLists->is_object() ? &*sharedLists : nullptr,
            .plugins = plugins,
            .settings = {} };
        const Inherited root{ .defaultFile = getText(package, "defaultFile"), .applies = *applies };
        mod.frontPage.items = loadItems(package, root, reading);
        loadConditions(package, mod.frontPage.items, {}, reading);
        if (const auto pages = package.find("pages"); pages != package.end() && pages->is_array()) {
            for (const auto& page : *pages) {
                mod.pages.push_back(loadPage(page, root, reading));
            }
            for (std::size_t place = 0; place < pages->size(); ++place) {
                loadPageConditions((*pages)[place], mod.pages[place], reading);
            }
        }
        mod.files = std::move(files);
        return mod;
    }

    /**
     * Loads the `config.json` of every folder in the packages' folder, which is `Data\ModsConfigVR\Config` in
     * the game. A package that cannot be read is a mod that is not available, named by its folder, and so
     * are two packages with one `modName`: neither is picked. A mod that is not available has the reason in
     * the log. No such folder is no mods.
     */
    std::vector<model::Mod> loadPackages(const std::filesystem::path& configFolder, const model::Translations& translations, const PluginChecks& plugins)
    {
        std::vector<model::Mod> mods;
        std::error_code error;
        for (std::filesystem::directory_iterator folder(configFolder, error), end; !error && folder != end; folder.increment(error)) {
            const auto file = folder->path() / "config.json";
            std::error_code noFile;
            if (!std::filesystem::is_regular_file(file, noFile)) {
                continue;
            }
            std::ostringstream text;
            text << std::ifstream(file, std::ios::binary).rdbuf();

            auto mod = loadPackage(text.str(), translations, plugins);
            if (!mod) {
                logger::warn("Package '{}' cannot be read: {}", toLogText(file), mod.error());
                const auto folderName = toLogText(folder->path().filename());
                mod = notAvailableMod(folderName, folderName, "its package cannot be read");
            }
            mods.push_back(std::move(*mod));
        }

        std::map<std::string, int> names;
        for (const auto& mod : mods) {
            ++names[common::str_tolower(mod.name)];
        }
        for (auto& mod : mods) {
            if (mod.notAvailable.empty() && names[common::str_tolower(mod.name)] > 1) {
                mod = notAvailableMod(mod.name, mod.displayName, "its package has the modName of another package");
            }
            if (!mod.notAvailable.empty()) {
                logger::warn("Package mod '{}' is not available: {}", mod.name, mod.notAvailable);
            }
        }
        return mods;
    }

    /**
     * The packages of the running game, from `Data\ModsConfigVR\Config` in the game's folder, with their
     * conditions on a plugin answered by the plugins the game loaded.
     */
    std::vector<model::Mod> loadInstalledPackages(const model::Translations& translations)
    {
        auto mods = loadPackages(PACKAGE_CONFIG_FOLDER, translations, PluginChecks{ .isGamePluginLoaded = isGamePluginLoaded, .isF4sePluginLoaded = isF4sePluginLoaded });
        logger::info("Loaded {} packages from '{}'", mods.size(), PACKAGE_CONFIG_FOLDER);
        return mods;
    }
}
