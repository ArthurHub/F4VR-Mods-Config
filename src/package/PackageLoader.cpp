#include "PackageLoader.h"

#include <algorithm>
#include <array>
#include <format>
#include <fstream>
#include <map>
#include <optional>
#include <sstream>
#include <unordered_map>

#include <nlohmann/json.hpp>

#include "common/CommonUtils.h"

using namespace mods_config;

namespace
{
    using Json = nlohmann::json;

    // where the packages are, from the game's folder: a folder each, with its `config.json`
    constexpr auto PACKAGE_CONFIG_FOLDER = R"(Data\ModsConfigVR\Config)";

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
     * Where a setting keeps its value: a value of the file its `storage` names, or else of the nearest
     * `defaultFile` above it. The value is named by its `path`, or by its `section` and its `key`, as an INI
     * key is. A storage that names no file of the package or no value, and a global or a property, are ones
     * the menu does not read, with the reason in the log.
     */
    model::Storage loadStorage(const Json& row, const std::string& defaultFile, const PackageReading& reading)
    {
        const auto storage = row.find("storage");
        if (storage == row.end() || !storage->is_object()) {
            logger::warn("The setting '{}' has no storage", getText(row, "text"));
            return model::UnknownStorage{};
        }
        if (storage->contains("global") || storage->contains("form")) {
            logger::warn("The setting '{}' is kept in a global or a property, which a package's menu does not read", getText(row, "text"));
            return model::UnknownStorage{ .type = storage->contains("global") ? "global" : "property" };
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
        if (!file || path.empty()) {
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
     * The `valueOptions` of a setting: the least, the most and the step of a number, and the options of a
     * choice, its own `options` or the shared list its `sharedOptions` names. A choice with neither has no
     * options, with the reason in the log, so it is disabled.
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
        } else {
            logger::warn("The choice '{}' has no list of options, or names no shared list of its package", getText(row, "text"));
        }
        return valueOptions;
    }

    /**
     * The `default` of a setting, in its value's type. None for a setting with no default of its type, which
     * is an error of the package that the log has.
     */
    std::optional<model::Value> loadDefault(const Json& row, const model::StoredType type)
    {
        auto value = getValue(row, "default", type);
        if (!value) {
            logger::warn("The setting '{}' has no default of the type of its value", getText(row, "text"));
        }
        return value;
    }

    /**
     * A setting of a package: its author's `id`, or one it gets from where its value is kept, its own
     * `applies` or the nearest above it, its `default`, and the options of a choice. Two settings with one id
     * are an error of the package, which the log has: an author gives one of them an id of its own. Its
     * conditions are read once every setting of the package is (`loadConditions`).
     */
    model::Setting loadSetting(const Json& row, const model::SettingType type, const Inherited& above, PackageReading& reading)
    {
        const auto own = inherit(row, above);
        auto storage = loadStorage(row, own.defaultFile, reading);
        auto id = getText(row, "id");
        if (id.empty()) {
            id = idOf(storage);
        }
        const auto valueType = loadValueType(row, type);
        if (!id.empty() && !reading.settings.emplace(common::str_tolower(id), SettingInfo{ .id = id, .type = type, .valueType = valueType }).second) {
            logger::warn("Two settings have the id '{}': give one of them an id of its own", id);
        }
        return model::Setting{ .id = std::move(id),
            .sourceId = {},
            .type = type,
            .text = getShownText(row, "text", reading.translations),
            .textRuns = {},
            .help = getShownText(row, "help", reading.translations),
            .valueOptions = loadValueOptions(row, type, valueType, reading),
            .storage = std::move(storage),
            .valueType = valueType,
            .action = {},
            .visibleWhen = {},
            .enabledWhen = {},
            .applies = own.applies,
            .defaultValue = loadDefault(row, valueType) };
    }

    /**
     * The items of the `content` of a page or a section, by their `type`, one for each row. A section is a
     * group of the items of its own `content`, and passes its `defaultFile` and its `applies` down to them.
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
                items.emplace_back(
                    model::Group{ .text = getShownText(row, "text", reading.translations), .items = loadItems(row, inherit(row, above), reading), .visibleWhen = {} });
                continue;
            }
            if (const auto setting = std::ranges::find(SETTING_TYPES, type, &SettingTypeName::first); setting != SETTING_TYPES.end()) {
                items.emplace_back(loadSetting(row, setting->second, above, reading));
                continue;
            }
            items.emplace_back(model::Placeholder{ .text = getShownText(row, "text", reading.translations),
                .textRuns = {},
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
     * it. A section's heading is shown by its `visibleWhen` too. The items are the rows of the content, one
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
     * page and its pages, which nest, with their help, and their sections, which nest, and settings. Every
     * text the player sees is in the game's language. A setting has the file its value is kept in, and when
     * its change takes effect, which the package's root has to say. A choice has its options or a list of
     * the package's `sharedLists`. A row is shown and enabled by its conditions, which are read once every
     * setting is, as a condition can name a setting after it. A condition on a plugin is answered by the checks.
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
        const auto applies = findApplies(package);
        if (!applies) {
            return std::unexpected("No applies of live, save or restart"s);
        }
        mod.source = model::Source::Package;
        mod.displayName = getShownText(package, "displayName", translations);
        if (mod.displayName.empty()) {
            mod.displayName = mod.name;
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
