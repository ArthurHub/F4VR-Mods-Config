#include "ConfigSpecLoader.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <cstdint>
#include <format>
#include <fstream>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <tuple>
#include <unordered_map>
#include <variant>
#include <vector>

#include <nlohmann/json.hpp>

#include "ActionSpec.h"
#include "ActivationSphereSpec.h"
#include "ConditionSpec.h"
#include "ConfigSpecReading.h"
#include "FrameworkTypeSpec.h"
#include "IniFile.h"
#include "TomlFile.h"
#include "common/CommonUtils.h"
#include "model/ActivationSpheres.h"
#include "model/Categories.h"
#include "model/Conditions.h"
#include "model/Haptics.h"
#include "model/InputBindings.h"
#include "model/OptionSets.h"
#include "model/SettingValues.h"
#include "model/TextMarkup.h"
#include "model/Transforms.h"

using namespace mods_config;

namespace
{
    using namespace spec;

    // the most lines a spacer is high, whatever its config spec says, as in an MCM config
    constexpr float MAX_SPACER_LINES = 20.0f;

    /**
     * A length of a key: a whole number above 0, or else 0, which is no limit.
     */
    int getLength(const Json& value, const char* key)
    {
        const auto found = value.find(key);
        return found != value.end() && found->is_number_integer() ? (std::max)(found->get<int>(), 0) : 0;
    }

    // the most characters of a row's name in a problem: a text row is a paragraph
    constexpr std::size_t MAX_ROW_NAME = 48;

    // The fields each object of a config spec can have, as the schema has them (docs/design/schema.md). Any other is
    // reported as one the menu does not know. A field the schema has and the menu does not read yet is known.
    // the config spec's root:
    constexpr std::array<std::string_view, 15> ROOT_FIELDS{ "minSchemaVersion",
        "modName",
        "displayName",
        "help",
        "category",
        "specVersion",
        "pluginRequirements",
        "f4sePluginRequirements",
        "files",
        "defaultFile",
        "applies",
        "sharedLists",
        "sharedPresetLists",
        "content",
        "pages" };
    // a file of its `files`:
    constexpr std::array<std::string_view, 4> FILE_FIELDS{ "format", "root", "path", "pathFromClientState" };
    // a page:
    constexpr std::array<std::string_view, 9> PAGE_FIELDS{ "pageDisplayName", "help", "id", "content", "pages", "defaultFile", "applies", "visibleWhen", "enabledWhen" };
    // a row, by its type:
    constexpr std::array<std::string_view, 11>
        SECTION_FIELDS{ "type", "id", "text", "help", "content", "isCollapsible", "collapsed", "defaultFile", "applies", "visibleWhen", "enabledWhen" };
    constexpr std::array<std::string_view, 16> SETTING_FIELDS{ "type",
        "id",
        "text",
        "help",
        "html",
        "valueType",
        "default",
        "valueOptions",
        "display",
        "storage",
        "readOnly",
        "warnings",
        "action",
        "applies",
        "visibleWhen",
        "enabledWhen" };
    constexpr std::array<std::string_view, 4> TEXT_ROW_FIELDS{ "type", "text", "html", "visibleWhen" };
    constexpr std::array<std::string_view, 3> SPACER_FIELDS{ "type", "numLines", "visibleWhen" };
    constexpr std::array<std::string_view, 5> IMAGE_FIELDS{ "type", "texture", "width", "height", "visibleWhen" };
    // a setting's parts:
    constexpr std::array<std::string_view, 22> VALUE_OPTIONS_FIELDS{ "min",
        "max",
        "step",
        "options",
        "sharedOptions",
        "listFromForm",
        "listFromProperty",
        "listFromFolder",
        "listFromClientCallback",
        "allowEmpty",
        "minLength",
        "maxLength",
        "suppress",
        "hands",
        "types",
        "buttons",
        "alpha",
        "presets",
        "sharedPresets",
        "position",
        "rotation",
        "scale" };
    constexpr std::array<std::string_view, 5> OPTION_FIELDS{ "value", "text", "help", "sets", "alsoSets" };
    constexpr std::array<std::string_view, 3> LIST_PROPERTY_FIELDS{ "form", "scriptName", "propertyName" };
    constexpr std::array<std::string_view, 4> LIST_FOLDER_FIELDS{ "root", "path", "mask", "noneText" };
    constexpr std::array<std::string_view, 3> DISPLAY_FIELDS{ "decimals", "unit", "percent" };

    /**
     * The name of a row as a problem names it: its text as the player sees it, on one line with no tags and
     * shortened, or its type for a row with no text, as a spacer.
     */
    std::string rowName(const Json& row, const model::Translations& translations)
    {
        auto text = common::trim(model::removeMarkup(getShownText(row, "text", translations)));
        if (text.empty()) {
            text = getText(row, "type");
            return text.empty() ? "A row"s : text;
        }
        if (text.size() > MAX_ROW_NAME) {
            text = common::trim(text.substr(0, MAX_ROW_NAME - 3)) + "...";
        }
        return text;
    }

    using FormatName = std::pair<std::string_view, model::FileFormat>;

    // what a file of a config spec is written in, by its `format`, which is also its extension
    constexpr std::array FILE_FORMATS{
        FormatName{ "ini", model::FileFormat::Ini },
        FormatName{ "toml", model::FileFormat::Toml },
    };

    /**
     * What is wrong with a path from a root of a config spec, as the end of a sentence of it: one that is absolute, or
     * goes up with `..`, could be any file of the system. Empty for one that is a path from its root.
     */
    std::string pathProblem(const std::string& path)
    {
        const std::filesystem::path checked(std::u8string(path.begin(), path.end()));
        if (path.empty() || checked.has_root_name() || checked.has_root_directory()) {
            return std::format("its path '{}' is not one from its root", path);
        }
        if (std::ranges::any_of(checked, [](const std::filesystem::path& part) {
                return part == "..";
            })) {
            return std::format("its path '{}' goes up out of its folder", path);
        }
        return {};
    }

    /**
     * A file of a config spec's `files`, or the reason it is not one: a `format` of `ini` or `toml`,
     * which its extension is, a `root` of `data` or `myGames`, and a `path` from that root. The path is
     * checked as it is written (`pathProblem`).
     */
    std::expected<model::File, std::string> loadFile(const Json& declared)
    {
        const auto formatText = getText(declared, "format");
        const auto format = std::ranges::find(FILE_FORMATS, formatText, &FormatName::first);
        if (format == FILE_FORMATS.end()) {
            return std::unexpected(std::format("its format '{}' is not ini or toml", formatText));
        }
        const auto rootText = getText(declared, "root");
        if (rootText != "data" && rootText != "myGames") {
            return std::unexpected(std::format("its root '{}' is not data or myGames", rootText));
        }
        auto path = getText(declared, "path");
        if (auto problem = pathProblem(path); !problem.empty()) {
            return std::unexpected(std::move(problem));
        }
        const std::filesystem::path checked(std::u8string(path.begin(), path.end()));
        const auto extension = checked.extension().u8string();
        if (common::str_tolower(std::string(extension.begin(), extension.end())) != "." + formatText) {
            return std::unexpected(std::format("its path '{}' does not end in .{}", path, formatText));
        }
        return model::File{ .format = format->second, .root = rootText == "data" ? model::FileRoot::Data : model::FileRoot::MyGames, .path = std::move(path) };
    }

    /**
     * The files of a config spec, by the names its `files` gives them. A file that is not written as one is
     * left out, and is reported, so its settings are not available.
     */
    std::map<std::string, model::File> loadFiles(const Json& configSpec, ConfigSpecReading& reading)
    {
        std::map<std::string, model::File> files;
        const auto found = configSpec.find("files");
        if (found == configSpec.end() || !found->is_object()) {
            return files;
        }
        const Into into(reading, "files");
        for (const auto& [name, declared] : found->items()) {
            const Into intoFile(reading, name);
            checkFields(declared, FILE_FIELDS, reading);
            auto file = declared.is_object() ? loadFile(declared) : std::unexpected("it is not an object"s);
            if (file) {
                // the state of the mod's client that gives its path, which its root and its path stand for while it is not
                // answered
                if (const auto state = declared.find("pathFromClientState"); state != declared.end()) {
                    if (state->is_string() && !state->get_ref<const std::string&>().empty()) {
                        file->pathFromClientState = state->get<std::string>();
                    } else {
                        const Into intoState(reading, "pathFromClientState");
                        reading.report(model::Severity::Warning, "Its pathFromClientState is not the name of a state: the file is read from its root and its path.");
                    }
                }
                files.emplace(name, std::move(*file));
            } else {
                reading.report(model::Severity::Error, std::format("The file \"{}\" is left out: {}. Its settings are not available.", name, file.error()));
            }
        }
        return files;
    }

    /**
     * The values of a config spec's TOML and INI files that are there, read from the folders of their roots, by
     * their names, which tell a setting whose path its file does not have. A file that cannot be read is left out,
     * as its settings are not available then. With no folders, as for a config spec read alone, none is read.
     */
    std::map<std::string, std::unique_ptr<spec::ModFile>> readFiles(const std::map<std::string, model::File>& files, const spec::FileFolders& folders)
    {
        std::map<std::string, std::unique_ptr<spec::ModFile>> read;
        if (folders.data.empty() && folders.myGames.empty()) {
            return read;
        }
        for (const auto& [name, file] : files) {
            const auto path = (file.root == model::FileRoot::Data ? folders.data : folders.myGames) / std::u8string(file.path.begin(), file.path.end());
            std::error_code error;
            if (!std::filesystem::is_regular_file(path, error)) {
                continue;
            }
            std::unique_ptr<spec::ModFile> values;
            if (file.format == model::FileFormat::Toml) {
                values = std::make_unique<spec::TomlFile>(path);
            } else {
                values = std::make_unique<spec::IniFile>(path);
            }
            if (values->read()) {
                read.emplace(name, std::move(values));
            }
        }
        return read;
    }

    using SettingTypeName = std::pair<std::string_view, model::SettingType>;

    // the rows that edit a value, by their `type`
    constexpr std::array SETTING_TYPES{
        SettingTypeName{ "switcher", model::SettingType::Switcher },
        SettingTypeName{ "slider", model::SettingType::Slider },
        SettingTypeName{ "stepper", model::SettingType::Stepper },
        SettingTypeName{ "dropdown", model::SettingType::Dropdown },
        SettingTypeName{ "textinput", model::SettingType::TextInput },
        SettingTypeName{ "cf.inputBinding", model::SettingType::InputBinding },
        SettingTypeName{ "cf.haptic", model::SettingType::Haptic },
        SettingTypeName{ "cf.color", model::SettingType::Color },
        SettingTypeName{ "cf.transform", model::SettingType::Transform },
    };

    /**
     * Whether a setting's author lists its options in its config spec: a stepper and a dropdown. A haptic is a choice
     * too, whose options are the framework's patterns.
     */
    bool listsOptions(const model::SettingType type)
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
     * and a text for a text input. One that is not a type is a warning. A binding, a color and a transform are the text
     * of their table and a haptic the name of its pattern, so their `valueType` is a string, and another is a warning too.
     */
    model::StoredType loadValueType(const Json& row, const model::SettingType type, ConfigSpecReading& reading)
    {
        const auto name = getText(row, "valueType");
        if (model::isFrameworkType(type)) {
            if (row.contains("valueType") && name != "string") {
                const Into into(reading, "valueType");
                reading.report(model::Severity::Warning,
                    std::format("Its valueType {} is not string, which a {} is: it is read as string.", row["valueType"].dump(), model::valueKindName(type)));
            }
            return model::StoredType::String;
        }
        const auto found = std::ranges::find(VALUE_TYPES, name, &ValueTypeName::first);
        if (found != VALUE_TYPES.end()) {
            return found->second;
        }
        auto valueType = model::StoredType::String;
        if (type == model::SettingType::Switcher) {
            valueType = model::StoredType::Bool;
        } else if (model::isChoice(type)) {
            valueType = model::StoredType::Int;
        } else if (type == model::SettingType::Slider) {
            valueType = model::StoredType::Float;
        }
        if (row.contains("valueType")) {
            const Into into(reading, "valueType");
            const auto read = std::ranges::find(VALUE_TYPES, valueType, &ValueTypeName::second)->first;
            reading.report(model::Severity::Warning, std::format("Its valueType {} is not bool, int, float or string: it is read as {}.", row["valueType"].dump(), read));
        }
        return valueType;
    }

    /**
     * The value of a key in a type: a JSON bool for a bool, a whole number for an int, any number for a
     * float, and a text for a string. None for a key that has no value of the type.
     */
    std::optional<model::Value> getValue(const Json& value, const char* key, const model::StoredType type)
    {
        const auto found = value.find(key);
        return found != value.end() ? model::jsonValue(*found, type) : std::nullopt;
    }

    /**
     * The options of a choice, in the type of its value. A text is an option of the short form, whose value
     * is its place in the list, for a choice of an int. An object is an option with its own `value`, its
     * `text` and its optional `help`. Any other option, and one whose value is not of the choice's type, is left out, and is reported:
     * of a shared list, which can be another choice's, as an option of that list.
     * An option's own fields and its texts are checked once, as its list is read: a shared list as the config spec's
     * root is read.
     */
    std::vector<model::Option> loadOptions(const Json& list, const std::string& sharedName, const model::StoredType valueType, ConfigSpecReading& reading)
    {
        std::vector<model::Option> options;
        const bool own = sharedName.empty();
        for (std::size_t place = 0; place < list.size(); ++place) {
            const auto& option = list[place];
            std::optional<Into> into;
            if (own) {
                into.emplace(reading, "options", place);
            }
            if (option.is_string() && valueType == model::StoredType::Int) {
                options.push_back({ .value = static_cast<int>(place), .text = own ? readShownText(option, reading) : reading.translations.translate(option.get<std::string>()) });
                continue;
            }
            auto value = option.is_object() ? getValue(option, "value", valueType) : std::nullopt;
            if (!value) {
                const auto which = own ? std::format("Option {}", place + 1) : std::format("Option {} of the shared list \"{}\"", place + 1, sharedName);
                reading.report(model::Severity::Error, std::format("{} is left out: it is not a text of a choice of an int, or an object with a value of its type.", which));
                continue;
            }
            if (own) {
                checkFields(option, OPTION_FIELDS, reading);
            }
            options.push_back({ .value = std::move(*value),
                .text = own ? readShownText(option, "text", reading) : getShownText(option, "text", reading.translations),
                .help = own ? readShownText(option, "help", reading) : getShownText(option, "help", reading.translations) });
        }
        return options;
    }

    /**
     * Where a choice reads its options in the game: a `listFromForm` names a FormList, as `MyMod.esp|900`, and a
     * `listFromProperty` a property of a script, by its `form`, its `propertyName` and its `scriptName`, which
     * is optional. A choice of an int stores the place of an option, and one of a string its text. None for a
     * choice with neither, and reported for a list that is not written so, and for a choice of another type.
     */
    std::optional<model::GameOptions> loadGameOptions(const Json& valueOptions, const model::StoredType valueType, ConfigSpecReading& reading)
    {
        model::GameOptions list{ .storesText = valueType == model::StoredType::String };
        const auto property = valueOptions.find("listFromProperty");
        if (valueOptions.contains("listFromForm")) {
            const auto formList = getText(valueOptions, "listFromForm");
            auto form = parseForm(formList);
            if (!form) {
                const Into into(reading, "listFromForm");
                reading.report(model::Severity::Error, std::format("Its listFromForm \"{}\" is not a plugin and an id in hex, as MyMod.esp|900: it has no options.", formList));
                return std::nullopt;
            }
            list.plugin = std::move(form->first);
            list.formId = form->second;
        } else if (property != valueOptions.end() && property->is_object()) {
            const Into into(reading, "listFromProperty");
            checkFields(*property, LIST_PROPERTY_FIELDS, reading);
            const auto formText = getText(*property, "form");
            auto form = parseForm(formText);
            list.property = getText(*property, "propertyName");
            if (!form || list.property.empty()) {
                reading.report(model::Severity::Error,
                    std::format("Its listFromProperty \"{}\" of the form \"{}\" is not a name, and a plugin and an id in hex: it has no options.", list.property, formText));
                return std::nullopt;
            }
            list.plugin = std::move(form->first);
            list.formId = form->second;
            list.script = getText(*property, "scriptName");
        } else {
            const Into into(reading, "listFromProperty");
            reading.report(model::Severity::Error, "Its listFromProperty is not an object: it has no options.");
            return std::nullopt;
        }
        if (valueType != model::StoredType::Int && valueType != model::StoredType::String) {
            reading.report(model::Severity::Error, "It lists its options in the game, so its valueType is int or string: it has no options.");
            return std::nullopt;
        }
        return list;
    }

    /**
     * The folder a choice's `listFromFolder` lists the files of: its `root`, `data` where it has none, its `path` from
     * that root, its `mask`, and the `noneText` of the option that is no file. Only a dropdown of a string lists one.
     * None for a list that is not written so, which is reported. With the folders of the game, a folder with no file
     * its mask lists is a warning, as a typo of its path would be.
     */
    std::optional<model::FolderOptions> loadFolderOptions(const Json& valueOptions, const model::SettingType type, const model::StoredType valueType, ConfigSpecReading& reading)
    {
        const Into into(reading, "listFromFolder");
        const auto& list = valueOptions["listFromFolder"];
        if (!list.is_object()) {
            reading.report(model::Severity::Error, "Its listFromFolder is not an object: it has no options.");
            return std::nullopt;
        }
        checkFields(list, LIST_FOLDER_FIELDS, reading);
        if (type != model::SettingType::Dropdown || valueType != model::StoredType::String) {
            reading.report(model::Severity::Error, "A listFromFolder is of a dropdown whose valueType is string: it has no options.");
            return std::nullopt;
        }
        const auto rootText = list.contains("root") ? getText(list, "root") : std::string("data");
        if (rootText != "data" && rootText != "myGames") {
            reading.report(model::Severity::Error, std::format("Its root '{}' is not data or myGames: it has no options.", rootText));
            return std::nullopt;
        }
        model::FolderOptions folder{ .root = rootText == "data" ? model::FileRoot::Data : model::FileRoot::MyGames,
            .path = getText(list, "path"),
            .mask = getText(list, "mask"),
            .noneText = readShownText(list, "noneText", reading) };
        if (const auto problem = pathProblem(folder.path); !problem.empty()) {
            reading.report(model::Severity::Error, std::format("Its folder is left out: {}. It has no options.", problem));
            return std::nullopt;
        }
        if (const auto path = model::folderOf(folder, reading.folders); !path.empty() && model::listFiles(path, folder.mask).empty()) {
            reading.report(model::Severity::Warning,
                std::format("{} is not there, or has no {}: the setting is not available until it has one.",
                    model::folderText(folder),
                    folder.mask.empty() ? std::string("files") : folder.mask + " files"));
        }
        return folder;
    }

    /**
     * The `valueOptions` of a setting: the least, the most and the step of a number, the fewest and the most
     * characters of a text input of a text, the hands, the ways and the buttons a binding takes, and the options of a
     * choice: its own `options`, the shared list
     * its `sharedOptions` names, where it reads them in the game, the folder whose files they are, or that its mod's
     * client gives them, with a `listFromClientCallback` that is true. An `allowEmpty` of false is a least of one character. A length that
     * is no whole number above 0 is none. A haptic's options are the menu's: None and the framework's patterns. A color
     * has an alpha unless its `alpha` is false, and a transform has the ranges of its parts that are on.
     * A choice with none of them has no options, and is reported, so it is disabled.
     */
    model::ValueOptions loadValueOptions(const Json& row, const model::SettingType type, const model::StoredType valueType, ConfigSpecReading& reading)
    {
        model::ValueOptions valueOptions;
        const auto found = row.find("valueOptions");
        const bool hasOptions = found != row.end() && found->is_object();
        std::optional<Into> into;
        if (hasOptions) {
            into.emplace(reading, "valueOptions");
            checkFields(*found, VALUE_OPTIONS_FIELDS, reading);
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
        loadFrameworkValueOptions(type, hasOptions ? &*found : nullptr, valueOptions, reading);
        if (!listsOptions(type)) {
            return valueOptions;
        }

        const Json* list = nullptr;
        std::string sharedName;
        if (hasOptions) {
            const auto own = found->find("options");
            if (own != found->end()) {
                list = &*own;
            } else {
                sharedName = getText(*found, "sharedOptions");
                list = findSharedList(reading.sharedLists, sharedName);
            }
        }
        if (list && list->is_array()) {
            valueOptions.options = loadOptions(*list, sharedName, valueType, reading);
            return valueOptions;
        }
        if (hasOptions && (found->contains("listFromForm") || found->contains("listFromProperty"))) {
            valueOptions.gameOptions = loadGameOptions(*found, valueType, reading);
        } else if (hasOptions && found->contains("listFromFolder")) {
            valueOptions.folderOptions = loadFolderOptions(*found, type, valueType, reading);
        } else if (hasOptions && getBool(*found, "listFromClientCallback")) {
            valueOptions.clientOptions = true;
        } else {
            reading.report(model::Severity::Error, "It has no list of options, or names no shared list of its config spec: it has no options.");
        }
        return valueOptions;
    }

    /**
     * Whether the `default` of a choice of files can be one: a path in its folder, or a bare name, which every pick is
     * written in the form of. One that is not is an error, which is reported. With the folders of the game, a default
     * whose file is not in the folder is a warning, as the player's files can differ from its author's.
     */
    bool isFolderDefault(const std::string& value, const model::FolderOptions& folder, ConfigSpecReading& reading)
    {
        const Into into(reading, "default");
        if (!model::isInFolder(value, folder)) {
            reading.report(model::Severity::Error, std::format("Its default \"{}\" is not in its folder {}: it has no Reset.", value, model::folderText(folder)));
            return false;
        }
        const auto path = model::folderOf(folder, reading.folders);
        if (value.empty() || path.empty()) {
            return true;
        }
        std::vector<model::Option> files;
        for (auto& file : model::listFiles(path, folder.mask)) {
            files.push_back({ .value = std::move(file), .text = {} });
        }
        if (!files.empty() && !model::findFileOption(files, folder, value)) {
            reading.report(model::Severity::Warning, std::format("Its default {} is not in {}: it is invalid until it is.", model::fileNameOf(value), model::folderText(folder)));
        }
        return true;
    }

    /**
     * What a report says of a setting with no `default` of its type, with an example of one for a framework type.
     */
    std::string_view missingDefault(const model::SettingType type)
    {
        switch (type) {
        case model::SettingType::InputBinding:
            return R"(It has no default binding, an object as { "hand": "offhand", "type": "tap", "button": "trigger" }: it has no Reset.)";
        case model::SettingType::Haptic:
            return R"(It has no default haptic, a text as "Tick" or "none": it has no Reset.)";
        case model::SettingType::Color:
            return R"(It has no default color, an object as { "rgb": [255, 255, 255], "alpha": 255 }: it has no Reset.)";
        case model::SettingType::Transform:
            return R"(It has no default transform, an object as { "pos": [0, 0, 0], "rot": [0, 0, 0], "scale": 1 }: it has no Reset.)";
        default:
            return "It has no default of the type of its value: it has no Reset.";
        }
    }

    /**
     * The `default` of a setting, read as `model::readJsonValue` reads a value: in its value's type, and a binding's, a
     * color's and a transform's in its table's form. None for a setting with no default of its type, or with one that
     * is not one of its values, with the framework's reason for a table. Both are errors of the config spec that are
     * reported, and the setting then has no Reset.
     */
    std::optional<model::Value> loadDefault(const Json& row, const model::SettingType type, const model::StoredType valueType, const model::TableForm tableForm,
        const model::ValueOptions& options, ConfigSpecReading& reading)
    {
        using ReadKind = model::ValueReadError::Kind;
        const auto found = row.find("default");
        const auto written = found != row.end() ? *found : Json();
        const auto read = model::readJsonValue(type, valueType, tableForm, options, written);
        if (read) {
            if (options.folderOptions && !isFolderDefault(std::get<std::string>(*read), *options.folderOptions, reading)) {
                return std::nullopt;
            }
            return *read;
        }
        const auto& error = read.error();
        if (error.kind == ReadKind::NotOfType) {
            reading.report(model::Severity::Error, std::string(missingDefault(type)));
            return std::nullopt;
        }
        if (model::isChoice(type) && options.options.empty() && !options.gameOptions && !options.clientOptions && !options.folderOptions) {
            // a choice with no options is reported already, and no default is one of its values
            return std::nullopt;
        }
        const Into into(reading, "default");
        if (error.kind == ReadKind::Untaken) {
            reading.report(model::Severity::Error, std::format("Its default is not a binding it takes: {}. It has no Reset.", error.reason));
        } else if (error.kind == ReadKind::IniAccelerate) {
            reading.report(model::Severity::Error, "Its default turns accelerate off, which the line of an INI file cannot have. It has no Reset.");
        } else if (model::isTableType(type)) {
            reading.report(model::Severity::Error, std::format("Its default is not a {}: {}. It has no Reset.", model::valueKindName(type), error.reason));
        } else if (type == model::SettingType::Haptic) {
            reading.report(model::Severity::Error, std::format("Its default {} is not a haptic pattern of the framework: it has no Reset.", written.dump()));
        } else {
            reading.report(model::Severity::Error,
                "Its default is not one of its values: outside its limits, of a length it does not allow, or none of its options. It has no Reset.");
        }
        return std::nullopt;
    }

    // the most decimals a number's display can have, as MCM's menu writes a number at the most
    constexpr int MAX_DECIMALS = 6;

    /**
     * The `display` of a setting: its decimals, its unit, which is a text the player sees, and whether it is a
     * percentage. Decimals that are no whole number from 0 to 6 are none, and are reported.
     */
    model::NumberDisplay loadDisplay(const Json& row, ConfigSpecReading& reading)
    {
        const auto found = row.find("display");
        if (found == row.end() || !found->is_object()) {
            return {};
        }
        const Into into(reading, "display");
        checkFields(*found, DISPLAY_FIELDS, reading);
        model::NumberDisplay display{ .decimals = {}, .unit = readShownText(*found, "unit", reading), .percent = getBool(*found, "percent") };
        if (const auto decimals = found->find("decimals"); decimals != found->end()) {
            if (decimals->is_number_integer() && decimals->get<int>() >= 0 && decimals->get<int>() <= MAX_DECIMALS) {
                display.decimals = decimals->get<int>();
            } else {
                const Into intoDecimals(reading, "decimals");
                reading.report(model::Severity::Error, std::format("Its decimals are not a whole number from 0 to {}: it has as many as its step.", MAX_DECIMALS));
            }
        }
        return display;
    }

    /**
     * The list of options a choice's `valueOptions` writes: its own `options`, or the shared list it names. None for a
     * choice that takes them from elsewhere, or names no list.
     */
    const Json* writtenOptions(const Json& row, const ConfigSpecReading& reading)
    {
        const auto found = row.find("valueOptions");
        if (found == row.end() || !found->is_object()) {
            return nullptr;
        }
        const auto own = found->find("options");
        const auto* list = own != found->end() ? &*own : findSharedList(reading.sharedLists, getText(*found, "sharedOptions"));
        return list && list->is_array() ? list : nullptr;
    }

    /**
     * Whether an option a choice writes has `sets` or `alsoSets`, which makes it a choice kept in the settings they set.
     */
    bool hasOptionSets(const Json& row, const ConfigSpecReading& reading)
    {
        const auto* list = writtenOptions(row, reading);
        return list && std::ranges::any_of(*list, [](const Json& option) {
            return option.is_object() && (option.contains("sets") || option.contains("alsoSets"));
        });
    }

    /**
     * A setting of a config spec, or a row made for a key of an activation sphere, which has no warning of a key its file
     * does not have, as a sphere leaves out most: its author's `id`, or one it gets from where its value is kept, its own
     * `applies` or the nearest above it, its `default`, how its number is shown, the options of a choice, and
     * its action, which Save runs.
     * A read-only setting has no default, as it has no Reset. Two settings with one id are an error of the
     * config spec, which is reported: a condition names the first, and an author gives one of them an id of its
     * own. Its conditions and its warnings are read once every setting of the config spec is (`loadConditions`).
     * A binding, a color and a transform are kept in a value of a file: in a TOML file as the table the framework keeps,
     * and in an INI file as its line, which is the setting's `tableForm`. In any other storage they are not available,
     * which is reported, and so is a transform with no part on.
     * A stepper or a dropdown whose options have `sets` is kept in the settings they set (`SetsStorage`): it has no
     * storage, default or `applies` of its own, and its options' sets are read once every setting is (`loadOptionSets`).
     * One with a storage too is an error, and so is any other type with them: their sets are ignored.
     */
    model::Setting loadSetting(const Json& row, const model::SettingType type, const Inherited& above, ConfigSpecReading& reading)
    {
        const auto own = inherit(row, above, reading);
        const auto valueType = loadValueType(row, type, reading);
        bool sets = hasOptionSets(row, reading);
        if (sets && type != model::SettingType::Stepper && type != model::SettingType::Dropdown) {
            const Into into(reading, "valueOptions");
            reading.report(model::Severity::Error, "Only a stepper and a dropdown have options that set other settings: its options' sets are ignored.");
            sets = false;
        } else if (sets && row.contains("storage")) {
            const Into into(reading, "storage");
            reading.report(model::Severity::Error, "It has a storage, and options that set other settings: its options' sets are ignored.");
            sets = false;
        }
        if (sets && row.contains("default")) {
            const Into into(reading, "default");
            reading.report(model::Severity::Warning, "Its default is left out: it is the option whose sets are the defaults of their settings.");
        }
        if (sets && findApplies(row)) {
            const Into into(reading, "applies");
            reading.report(model::Severity::Warning, "Its applies is left out: each setting its options set takes effect as its own says.");
        }
        auto storage = sets ? model::Storage(model::SetsStorage{}) : loadStorage(row, own.defaultFile, valueType, reading);
        auto tableForm = model::TableForm::Toml;
        if (model::isTableType(type) && !std::holds_alternative<model::UnknownStorage>(storage)) {
            const auto* file = std::get_if<model::FileStorage>(&storage);
            if (!file) {
                const std::string kind(model::valueKindName(type));
                const Into into(reading, "storage");
                reading.report(model::Severity::Error, std::format("A {} is kept in a value of a TOML or an INI file, and it is not: it is not available.", kind));
                storage = model::UnknownStorage{ .type = kind };
            } else if (reading.files.at(file->file).format == model::FileFormat::Ini) {
                tableForm = model::TableForm::IniLine;
            }
        }
        const bool readOnly = getBool(row, "readOnly");
        const bool made = row.contains(MADE_ROW_PATH);
        if (const auto* file = std::get_if<model::FileStorage>(&storage); file && !made) {
            if (const auto values = reading.fileValues.find(file->file); values != reading.fileValues.end() && !values->second->contains(file->path)) {
                const Into into(reading, "storage");
                reading.report(model::Severity::Warning,
                    std::format("Its file \"{}\" has no value {}: {}.",
                        file->file,
                        values->second->pathName(file->path),
                        readOnly ? "it shows none" : "it shows its default, and a Save adds it"));
            }
        }
        auto id = getText(row, "id");
        if (id.empty()) {
            id = idOf(storage);
        }
        SettingInfo* info = nullptr;
        if (!id.empty()) {
            const auto [found, added] = reading.settings.emplace(common::str_tolower(id), SettingInfo{ .id = id, .type = type, .valueType = valueType });
            if (added) {
                info = &found->second;
            } else {
                reading.report(model::Severity::Error,
                    std::format("A setting before it has the id \"{}\": a condition names that one. Give one of them an id of its own.", found->second.id));
            }
        }
        auto valueOptions = loadValueOptions(row, type, valueType, reading);
        if (info) {
            info->tableForm = tableForm;
            info->valueOptions = valueOptions;
            info->setsOthers = sets;
        }
        if (type == model::SettingType::Transform && !std::ranges::any_of(valueOptions.transformAxes, &model::TransformAxis::on)) {
            const Into into(reading, "valueOptions");
            reading.report(model::Severity::Error, "Its position, its rotation and its scale are all off: it is not available.");
            storage = model::UnknownStorage{ .type = "transform parts" };
        }
        if (made) {
            const auto words = getText(row, MADE_WORDS);
            valueOptions.sphereWords = words == "visibility"     ? model::SphereWords::Visibility
                                       : words == "orientation"  ? model::SphereWords::Orientation
                                       : words == "style"        ? model::SphereWords::Style
                                       : words == "bindingIcons" ? model::SphereWords::BindingIcons
                                                                 : model::SphereWords::None;
        }
        auto defaultValue = sets || readOnly || (made && !row.contains("default")) ? std::nullopt : loadDefault(row, type, valueType, tableForm, valueOptions, reading);
        auto text = getLabel(row, reading);
        auto help = readShownText(row, "help", reading);
        auto display = loadDisplay(row, reading);
        auto action = loadSettingAction(row, storage, valueType, reading);
        return model::Setting{ .id = std::move(id),
            .sourceId = {},
            .type = type,
            .text = std::move(text),
            .textRuns = getLabelRuns(row, reading.translations),
            .help = std::move(help),
            .valueOptions = std::move(valueOptions),
            .storage = std::move(storage),
            .valueType = valueType,
            .tableForm = tableForm,
            .action = std::move(action),
            .visibleWhen = {},
            .enabledWhen = {},
            .applies = sets ? model::Applies::Unknown : own.applies,
            .defaultValue = std::move(defaultValue),
            .display = std::move(display),
            .readOnly = readOnly,
            .warnings = {} };
    }

    /**
     * A text row: its text in lines, and with the tags of an MCM config read where its `html` is true.
     */
    model::Text loadText(const Json& row, ConfigSpecReading& reading)
    {
        const auto text = readShownText(row, "text", reading);
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
     * of which a side that is not more than 0 is none. One with no texture shows that an image is there, and
     * is reported.
     */
    model::Image loadImage(const Json& row, ConfigSpecReading& reading)
    {
        auto texture = getText(row, "texture");
        if (texture.empty()) {
            reading.report(model::Severity::Error, "It names no texture: a box is shown in its place.");
        }
        return model::Image{ .pixels = {},
            .texture = std::move(texture),
            .width = (std::max)(getNumber(row, "width", 0.0f), 0.0f),
            .height = (std::max)(getNumber(row, "height", 0.0f), 0.0f),
            .visibleWhen = {} };
    }

    /**
     * A row of a `content` as the rows are read: the section a sphere was made into, and any other as it is.
     */
    const Json& readRow(const Json& row, const ConfigSpecReading& reading)
    {
        const auto sphere = reading.spheres.find(&row);
        return sphere != reading.spheres.end() ? sphere->second : row;
    }

    /**
     * The reading at a row of a `content`, by its place: a row the loader made says where it is itself.
     */
    Into intoRow(ConfigSpecReading& reading, const Json& row, const std::size_t place)
    {
        if (const auto made = row.find(MADE_ROW_PATH); made != row.end()) {
            return Into(reading, made->get_ref<const std::string&>());
        }
        return Into(reading, "content", place);
    }

    std::vector<model::Item> loadItems(const Json& container, const Inherited& above, ConfigSpecReading& reading);

    /**
     * A section: a group of the items of its own `content`, to which it passes its `defaultFile` and its `applies`.
     * It folds with `isCollapsible`, and starts folded with `collapsed`: one that does not fold could not be
     * opened, so its `collapsed` is left out, and it is reported.
     */
    model::Group loadSection(const Json& row, const Inherited& above, ConfigSpecReading& reading)
    {
        checkFields(row, SECTION_FIELDS, reading);
        const bool collapsible = getBool(row, "isCollapsible");
        if (getBool(row, "collapsed") && !collapsible) {
            reading.report(model::Severity::Warning, "It is collapsed and does not fold: it is open.");
        }
        auto text = readShownText(row, "text", reading);
        auto help = readShownText(row, "help", reading);
        return model::Group{ .text = std::move(text),
            .help = std::move(help),
            .items = loadItems(row, inherit(row, above, reading), reading),
            .visibleWhen = {},
            .collapsible = collapsible,
            .collapsed = collapsible && getBool(row, "collapsed") };
    }

    /**
     * The items of the `content` of a page or a section, by their `type`, one for each row. An activation sphere is a
     * section the loader makes of the keys of its own. Any other row the menu does not have is a placeholder with its
     * type, and a type the schema does not have is reported. A row's fields that its type does not have are reported.
     */
    std::vector<model::Item> loadItems(const Json& container, const Inherited& above, ConfigSpecReading& reading)
    {
        std::vector<model::Item> items;
        const auto content = container.find("content");
        if (content == container.end() || !content->is_array()) {
            return items;
        }
        for (std::size_t place = 0; place < content->size(); ++place) {
            const auto& row = (*content)[place];
            const auto into = intoRow(reading, row, place);
            reading.at.row = rowName(row, reading.translations);
            reading.at.itemPlaces.push_back(place);
            const auto type = getText(row, "type");
            if (type == "section") {
                items.emplace_back(loadSection(row, above, reading));
                continue;
            }
            if (type == "cf.activationSphere") {
                const auto& section = reading.spheres.emplace(&row, sphereSection(row, above, reading)).first->second;
                auto group = loadSection(section, above, reading);
                group.role = model::GroupRole::ActivationSphere;
                items.emplace_back(std::move(group));
                continue;
            }
            if (type == "text") {
                checkFields(row, TEXT_ROW_FIELDS, reading);
                items.emplace_back(loadText(row, reading));
                continue;
            }
            if (type == "spacer") {
                checkFields(row, SPACER_FIELDS, reading);
                items.emplace_back(loadSpacer(row));
                continue;
            }
            if (type == "image") {
                checkFields(row, IMAGE_FIELDS, reading);
                items.emplace_back(loadImage(row, reading));
                continue;
            }
            if (type == "button") {
                items.emplace_back(loadButton(row, reading));
                continue;
            }
            if (const auto setting = std::ranges::find(SETTING_TYPES, type, &SettingTypeName::first); setting != SETTING_TYPES.end()) {
                checkFields(row, SETTING_FIELDS, reading);
                items.emplace_back(loadSetting(row, setting->second, above, reading));
                continue;
            }
            reading.report(model::Severity::Warning,
                type.empty() ? "It has no type: it is shown as a row the menu does not have."s
                             : std::format("The menu has no row of the type \"{}\": it is shown as one it does not have.", type));
            auto text = getLabel(row, reading);
            auto help = readShownText(row, "help", reading);
            items.emplace_back(
                model::Placeholder{ .text = std::move(text), .textRuns = getLabelRuns(row, reading.translations), .help = std::move(help), .type = type, .visibleWhen = {} });
        }
        return items;
    }

    /**
     * A page of a config spec, with the pages under it, which get its `defaultFile` and its `applies` as its
     * items do. A problem in it is on it: the caller reads it `Into` its place, with its place among the pages,
     * which takes the page back off.
     */
    model::Page loadPage(const Json& page, const Inherited& above, ConfigSpecReading& reading)
    {
        reading.at.pages.push_back(common::trim(getShownText(page, "pageDisplayName", reading.translations)));
        checkFields(page, PAGE_FIELDS, reading);
        model::Page loaded{ .name = readShownText(page, "pageDisplayName", reading),
            .help = readShownText(page, "help", reading),
            .items = {},
            .pages = {},
            .id = getText(page, "id") };
        const auto own = inherit(page, above, reading);
        loaded.items = loadItems(page, own, reading);
        if (const auto pages = page.find("pages"); pages != page.end() && pages->is_array()) {
            for (std::size_t place = 0; place < pages->size(); ++place) {
                const Into into(reading, "pages", place);
                reading.at.pagePlaces.push_back(place);
                loaded.pages.push_back(loadPage((*pages)[place], own, reading));
            }
        }
        return loaded;
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
     * A value an option sets on a setting, read as the setting's default is (`model::readJsonValue`): a binding, a color
     * and a transform are their table as a JSON object, a haptic the name of its pattern, and any other a value of the
     * setting's type that is one of its values. A binding is the whole binding, in the setting's form; a color and a
     * transform are the TOML text of the parts they name, which the option merges into the setting's table. None for a
     * value that is not one, which is reported.
     */
    std::optional<model::Value> loadSetValue(const Json& value, const SettingInfo& target, ConfigSpecReading& reading)
    {
        using ReadKind = model::ValueReadError::Kind;
        const auto leftOut = [&reading](const std::string& why) {
            reading.report(model::Severity::Error, std::format("Its value {}: it is left out.", why));
            return std::nullopt;
        };
        const auto form = target.type == model::SettingType::InputBinding ? target.tableForm : model::TableForm::Toml;
        const auto read = model::readJsonValue(target.type, target.valueType, form, target.valueOptions, value);
        if (read) {
            return *read;
        }
        const auto& error = read.error();
        if (error.kind == ReadKind::Untaken) {
            return leftOut(std::format("is not a binding the setting takes: {}", error.reason));
        }
        if (error.kind == ReadKind::IniAccelerate) {
            return leftOut("turns accelerate off, which the line of an INI file cannot have"s);
        }
        if (target.type == model::SettingType::Haptic) {
            return leftOut(std::format("{} is not a haptic pattern of the framework", value.dump()));
        }
        if (model::isTableType(target.type)) {
            return leftOut(error.kind == ReadKind::NotOfType ? std::format("{} is not a table, an object as the setting's default is", value.dump())
                                                             : std::format("is not a {}: {}", model::valueKindName(target.type), error.reason));
        }
        return leftOut(error.kind == ReadKind::NotOfType
                           ? std::format("{} is not of the setting's type", value.dump())
                           : std::format("{} is not one of the setting's values: outside its limits, of a length it does not allow, or none of its options", value.dump()));
    }

    /**
     * The values an option's `sets` or `alsoSets` (its `field`) set: an object of values by the ids of their settings, as
     * a condition names a setting. An id of no setting, and of a choice that sets other settings, and a value that is
     * not one of its setting's (`loadSetValue`), is reported and left out, and the option keeps the rest.
     */
    std::vector<model::OptionSet> loadSetValues(const Json& written, const char* field, ConfigSpecReading& reading)
    {
        std::vector<model::OptionSet> loaded;
        const auto sets = written.find(field);
        if (sets == written.end() || !sets->is_object()) {
            return loaded;
        }
        const Into intoSets(reading, field);
        for (const auto& [id, set] : sets->items()) {
            const Into intoSet(reading, id);
            const auto target = reading.settings.find(common::str_tolower(id));
            if (target == reading.settings.end()) {
                reading.report(model::Severity::Error, std::format("\"{}\" names no setting: it is left out.", id));
                continue;
            }
            if (target->second.setsOthers) {
                reading.report(model::Severity::Error, std::format("\"{}\" is a choice that sets other settings, which no option sets: it is left out.", id));
                continue;
            }
            if (auto value = loadSetValue(set, target->second, reading)) {
                loaded.push_back({ .setting = target->second.id, .value = std::move(*value) });
            }
        }
        return loaded;
    }

    /**
     * The values the options of a choice that sets other settings set, once every setting of its config spec is read, so
     * an option can name one after it: each option's `sets`, which its value is read from, and `alsoSets`, set too as
     * it is picked (`loadSetValues`). An option with no `sets` is never the choice's value, and is reported.
     */
    void loadOptionSets(const Json& row, model::Setting& setting, ConfigSpecReading& reading)
    {
        const auto* list = writtenOptions(row, reading);
        if (!list) {
            return;
        }
        const Into into(reading, "valueOptions");
        for (std::size_t place = 0; place < list->size(); ++place) {
            const auto& written = (*list)[place];
            const auto value = written.is_object() ? getValue(written, "value", setting.valueType) : std::nullopt;
            const auto option = value ? std::ranges::find_if(setting.valueOptions.options,
                                            [&value](const model::Option& loaded) {
                                                return model::sameValue(loaded.value, *value);
                                            })
                                      : setting.valueOptions.options.end();
            if (option == setting.valueOptions.options.end()) {
                continue;
            }
            const Into intoOption(reading, "options", place);
            const auto sets = written.find("sets");
            if (sets == written.end() || !sets->is_object() || sets->empty()) {
                reading.report(model::Severity::Warning,
                    written.contains("alsoSets") ? "It has no sets: the row never shows it, and picking it sets only its alsoSets."
                                                 : "It sets no setting: picking it changes nothing, and the row never shows it.");
            }
            option->sets = loadSetValues(written, "sets", reading);
            option->alsoSets = loadSetValues(written, "alsoSets", reading);
        }
    }

    /**
     * Gives the items of the `content` of a page or a section their conditions: a row's `visibleWhen` and
     * `enabledWhen`, each joined with those of the sections it is in, so a row needs every condition above
     * it, and the warnings of a setting and a button. A section's heading is shown by its `visibleWhen` too.
     * The items are the rows of the content, one
     * for each, in their order, as `loadItems` reads them.
     */
    void loadConditions(const Json& container, std::vector<model::Item>& items, const RowConditions& above, ConfigSpecReading& reading)
    {
        const auto content = container.find("content");
        if (content == container.end() || !content->is_array()) {
            return;
        }
        for (std::size_t place = 0; place < content->size() && place < items.size(); ++place) {
            const auto& row = readRow((*content)[place], reading);
            auto& item = items[place];
            const auto into = intoRow(reading, row, place);
            reading.at.row = rowName(row, reading.translations);
            reading.at.itemPlaces.push_back(place);
            const RowConditions own{ .visibleWhen = joinConditions(above.visibleWhen, loadRowCondition(row, "visibleWhen", reading)),
                .enabledWhen = joinConditions(above.enabledWhen, loadRowCondition(row, "enabledWhen", reading)) };
            std::visit(
                [&own](auto& loaded) {
                    loaded.visibleWhen = own.visibleWhen;
                },
                item);
            if (auto* setting = std::get_if<model::Setting>(&item)) {
                setting->enabledWhen = own.enabledWhen;
                setting->warnings = loadWarnings(row, reading);
                if (model::setsOthers(*setting)) {
                    loadOptionSets(row, *setting, reading);
                }
            } else if (auto* button = std::get_if<model::Button>(&item)) {
                button->enabledWhen = own.enabledWhen;
                button->warnings = loadWarnings(row, reading);
            } else if (auto* group = std::get_if<model::Group>(&item)) {
                loadConditions(row, group->items, own, reading);
            }
        }
    }

    /**
     * Gives the sections, the activation spheres and the buttons of the `content` of a page or a section their `id`,
     * which their mod's client is told while the ray is on them: once every setting is read, as an id is unique among
     * the settings' too. One a setting has, or a section or a button before it, is an error, and it is left out: the
     * setting keeps its own, as conditions name it.
     */
    void loadItemIds(const Json& container, std::vector<model::Item>& items, ConfigSpecReading& reading)
    {
        const auto content = container.find("content");
        if (content == container.end() || !content->is_array()) {
            return;
        }
        for (std::size_t place = 0; place < content->size() && place < items.size(); ++place) {
            const auto& row = readRow((*content)[place], reading);
            auto& item = items[place];
            auto* group = std::get_if<model::Group>(&item);
            auto* button = std::get_if<model::Button>(&item);
            if (!group && !button) {
                continue;
            }
            const auto into = intoRow(reading, row, place);
            reading.at.row = rowName(row, reading.translations);
            reading.at.itemPlaces.push_back(place);
            if (auto id = getText(row, "id"); !id.empty()) {
                const auto lower = common::str_tolower(id);
                if (const auto setting = reading.settings.find(lower); setting != reading.settings.end()) {
                    reading.report(model::Severity::Error,
                        std::format("The setting \"{}\" has its id: the setting keeps it, and this tells its mod's client nothing. Give it an id of its own.", setting->second.id));
                } else if (!reading.itemIds.insert(lower).second) {
                    reading.report(model::Severity::Error,
                        std::format("A section or a button before it has the id \"{}\": this tells its mod's client nothing. Give it an id of its own.", id));
                } else {
                    (group ? group->id : button->id) = std::move(id);
                }
            }
            if (group) {
                loadItemIds(row, group->items, reading);
            }
        }
    }

    /**
     * Gives the sections, the activation spheres and the buttons of a page and of its child pages their `id`.
     */
    void loadPageItemIds(const Json& page, model::Page& loaded, ConfigSpecReading& reading)
    {
        reading.at.pages.push_back(common::trim(loaded.name));
        loadItemIds(page, loaded.items, reading);
        const auto pages = page.find("pages");
        if (pages == page.end() || !pages->is_array()) {
            return;
        }
        for (std::size_t place = 0; place < pages->size() && place < loaded.pages.size(); ++place) {
            const Into into(reading, "pages", place);
            reading.at.pagePlaces.push_back(place);
            loadPageItemIds((*pages)[place], loaded.pages[place], reading);
        }
    }

    /**
     * Gives a page, its items and its child pages their conditions, including those of its parent pages.
     */
    void loadPageConditions(const Json& page, model::Page& loaded, ConfigSpecReading& reading, const RowConditions& above = {})
    {
        reading.at.pages.push_back(common::trim(loaded.name));
        loaded.visibleWhen = joinConditions(above.visibleWhen, loadRowCondition(page, "visibleWhen", reading));
        loaded.enabledWhen = joinConditions(above.enabledWhen, loadRowCondition(page, "enabledWhen", reading));
        const RowConditions own{ .visibleWhen = loaded.visibleWhen, .enabledWhen = loaded.enabledWhen };
        loadConditions(page, loaded.items, own, reading);
        const auto pages = page.find("pages");
        if (pages == page.end() || !pages->is_array()) {
            return;
        }
        for (std::size_t place = 0; place < pages->size() && place < loaded.pages.size(); ++place) {
            const Into into(reading, "pages", place);
            reading.at.pagePlaces.push_back(place);
            loadPageConditions((*pages)[place], loaded.pages[place], reading, own);
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
     * The plugins a list of a config spec's requirements names, in its order: none where it has no list, and an entry
     * that is no text is left out.
     */
    std::vector<std::string> requiredPlugins(const Json& configSpec, const char* key)
    {
        std::vector<std::string> plugins;
        const auto required = configSpec.find(key);
        if (required == configSpec.end() || !required->is_array()) {
            return plugins;
        }
        for (const auto& plugin : *required) {
            if (plugin.is_string()) {
                plugins.push_back(plugin.get<std::string>());
            }
        }
        return plugins;
    }

    /**
     * The plugins of a list of a config spec's requirements that are not loaded, in its order. None with no check.
     */
    std::vector<std::string> findMissing(const Json& configSpec, const char* key, const spec::PluginCheck& isLoaded)
    {
        std::vector<std::string> missing;
        if (!isLoaded) {
            return missing;
        }
        for (auto& plugin : requiredPlugins(configSpec, key)) {
            if (!isLoaded(plugin)) {
                missing.push_back(std::move(plugin));
            }
        }
        return missing;
    }

    /**
     * Why a config spec's mod is not available while a plugin it requires is not loaded: a game plugin of its
     * `pluginRequirements` or an F4SE plugin of its `f4sePluginRequirements`, as `its plugin MyMod.esp and
     * its F4SE plugin MyMod are not loaded`. Empty while every one is, and with no checks, as in a test of a
     * config spec alone.
     */
    std::string findMissingRequirements(const Json& configSpec, const spec::PluginChecks& plugins)
    {
        const auto game = findMissing(configSpec, "pluginRequirements", plugins.isGamePluginLoaded);
        const auto f4se = findMissing(configSpec, "f4sePluginRequirements", plugins.isF4sePluginLoaded);
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
     * The `minSchemaVersion` of a config spec: a whole number from 1. None for a config spec that has none.
     */
    std::optional<std::int64_t> findSchemaVersion(const Json& configSpec)
    {
        const auto version = configSpec.find("minSchemaVersion");
        if (version == configSpec.end() || !version->is_number_integer() || version->get<std::int64_t>() < 1) {
            return std::nullopt;
        }
        return version->get<std::int64_t>();
    }

    /**
     * Checks the options of the config spec's shared lists once, as several choices can read one: the fields of an
     * option, and its texts. An option that is not one of a choice is reported as the choice reads the list.
     */
    void checkSharedLists(ConfigSpecReading& reading)
    {
        if (!reading.sharedLists) {
            return;
        }
        const Into into(reading, "sharedLists");
        for (const auto& [name, list] : reading.sharedLists->items()) {
            if (!list.is_array()) {
                continue;
            }
            const Into intoList(reading, name);
            for (std::size_t place = 0; place < list.size(); ++place) {
                const Into intoOption(reading, "", place);
                if (list[place].is_object()) {
                    checkFields(list[place], OPTION_FIELDS, reading);
                    std::ignore = readShownText(list[place], "text", reading);
                    std::ignore = readShownText(list[place], "help", reading);
                } else {
                    std::ignore = readShownText(list[place], reading);
                }
            }
        }
    }

    /**
     * Whether a problem is before another in spec.json, by their paths: a place in a list is compared as a
     * number, so `content[2]` is before `content[10]`, and a path is before the paths in it. The keys of an
     * object have no order in the file the reader keeps, so they are in the order of their names.
     */
    bool isBefore(const model::Problem& problem, const model::Problem& other)
    {
        // the parts of a path: the names, and the places, which are numbers
        const auto parts = [](const std::string& path) {
            std::vector<std::variant<std::size_t, std::string>> parts;
            std::size_t at = 0;
            while (at < path.size()) {
                if (path[at] == '.') {
                    ++at;
                } else if (path[at] == '[') {
                    const auto end = path.find(']', at);
                    std::size_t place = 0;
                    std::from_chars(path.data() + at + 1, path.data() + end, place);
                    parts.emplace_back(place);
                    at = end + 1;
                } else {
                    const auto end = path.find_first_of(".[", at);
                    parts.emplace_back(path.substr(at, end - at));
                    at = end == std::string::npos ? path.size() : end;
                }
            }
            return parts;
        };
        return parts(problem.path) < parts(other.path);
    }

    /**
     * A count of a thing as the log says it: `1 error`, `2 errors`.
     */
    std::string countOf(const std::size_t count, const std::string_view thing)
    {
        return std::format("{} {}{}", count, thing, count == 1 ? "" : "s");
    }

    /**
     * Logs each problem of a config spec, with the config spec's name first, where it is after it, and how many
     * errors and warnings the config spec has.
     */
    void logProblems(const model::Mod& mod)
    {
        std::size_t errors = 0;
        for (const auto& problem : mod.problems) {
            const bool error = problem.severity == model::Severity::Error;
            errors += error ? 1 : 0;
            auto where = problem.where;
            if (!problem.path.empty()) {
                where += (where.empty() ? "" : ", ") + problem.path;
            }
            logger::warn("Config spec '{}': {} of {}: {}{}", mod.displayName, error ? "error" : "warning", problem.row, problem.message, where.empty() ? "" : " (" + where + ")");
        }
        logger::info("Config spec '{}' has {} and {}", mod.displayName, countOf(errors, "error"), countOf(mod.problems.size() - errors, "warning"));
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
            .source = model::Source::ConfigSpec,
            .files = {} };
    }
}

namespace mods_config::spec
{
    /**
     * Reads the text of a config spec's `spec.json` into the model: the mod and its help, its files, its front
     * page and its pages, which nest, with their help, and their sections, which nest and fold, with their
     * help, settings, buttons, texts, spacers and images. Every
     * text the player sees is in the game's language. The config spec's root has to say its `minSchemaVersion`. A
     * setting has the file its value is kept in, and when its change takes effect, which the config spec's root has
     * to say. A choice has its options or a list of
     * the config spec's `sharedLists`. A row is shown and enabled by its conditions, which are read once every
     * setting is, as a condition can name a setting after it. A condition on a plugin is answered by the checks.
     * A mod whose config spec needs a newer schema than the menu reads, or whose required plugins are not all
     * loaded, is not available, and is its names and the reason alone: the rest of its config spec is not read.
     * The error says why a config spec cannot be read. Comments are skipped, as in an MCM config.
     * With the folders of its files, the files that are there are read, and a setting whose path its file does not
     * have is a warning.
     */
    std::expected<model::Mod, std::string> loadConfigSpec(const std::string_view text, const model::Translations& translations, const PluginChecks& plugins,
        const FileFolders& folders)
    {
        Json configSpec;
        try {
            configSpec = Json::parse(text, nullptr, true, true);
        } catch (const Json::parse_error& e) {
            return std::unexpected(std::format("Not valid JSON: {}", e.what()));
        }
        if (!configSpec.is_object()) {
            return std::unexpected("Not a JSON object"s);
        }

        model::Mod mod;
        mod.name = getText(configSpec, "modName");
        if (mod.name.empty()) {
            return std::unexpected("No modName"s);
        }
        const auto schemaVersion = findSchemaVersion(configSpec);
        if (!schemaVersion) {
            return std::unexpected("No minSchemaVersion of a whole number from 1"s);
        }
        const auto sharedLists = configSpec.find("sharedLists");
        const auto sharedPresetLists = configSpec.find("sharedPresetLists");
        ConfigSpecReading reading{ .translations = translations,
            .files = {},
            .modName = mod.name,
            .sharedLists = sharedLists != configSpec.end() && sharedLists->is_object() ? &*sharedLists : nullptr,
            .sharedPresetLists = sharedPresetLists != configSpec.end() && sharedPresetLists->is_object() ? &*sharedPresetLists : nullptr,
            .plugins = plugins,
            .folders = folders,
            .settings = {},
            .problems = {},
            .at = {} };
        mod.source = model::Source::ConfigSpec;
        mod.displayName = readShownText(configSpec, "displayName", reading);
        if (mod.displayName.empty()) {
            mod.displayName = mod.name;
        }
        // a newer schema can say anything in another way, so nothing more of it is read
        auto reason = *schemaVersion > SCHEMA_VERSION ? std::format("its spec.json needs a newer Mods Config FO4 VR, one that reads schema version {}", *schemaVersion)
                                                      : findMissingRequirements(configSpec, plugins);
        mod.category = model::categoryName(getText(configSpec, "category"));
        if (!reason.empty()) {
            auto notAvailable = notAvailableMod(std::move(mod.name), std::move(mod.displayName), std::move(reason));
            notAvailable.category = std::move(mod.category);
            return notAvailable;
        }
        checkFields(configSpec, ROOT_FIELDS, reading);
        if (configSpec.contains("category") && mod.category.empty()) {
            const Into into(reading, "category");
            reading.report(model::Severity::Warning, "The category is not read: it is a plain name, as \"Gameplay\", and not a `$` token.");
        }
        mod.help = readShownText(configSpec, "help", reading);
        mod.f4sePlugins = requiredPlugins(configSpec, "f4sePluginRequirements");
        reading.files = loadFiles(configSpec, reading);
        reading.fileValues = readFiles(reading.files, folders);
        checkSharedLists(reading);
        checkSharedPresetLists(reading);

        // a setting that nothing says it of takes effect at a time the menu does not know
        const auto root = inherit(configSpec, Inherited{}, reading);
        mod.frontPage.items = loadItems(configSpec, root, reading);
        const auto pages = configSpec.find("pages");
        const bool hasPages = pages != configSpec.end() && pages->is_array();
        if (hasPages) {
            for (std::size_t place = 0; place < pages->size(); ++place) {
                const Into into(reading, "pages", place);
                reading.at.pagePlaces.push_back(place);
                mod.pages.push_back(loadPage((*pages)[place], root, reading));
            }
        }

        // the conditions and the ids, once every setting is read: a row can name one on any page
        loadConditions(configSpec, mod.frontPage.items, {}, reading);
        if (hasPages) {
            for (std::size_t place = 0; place < pages->size(); ++place) {
                const Into into(reading, "pages", place);
                reading.at.pagePlaces.push_back(place);
                loadPageConditions((*pages)[place], mod.pages[place], reading);
            }
        }
        loadItemIds(configSpec, mod.frontPage.items, reading);
        if (hasPages) {
            for (std::size_t place = 0; place < pages->size() && place < mod.pages.size(); ++place) {
                const Into into(reading, "pages", place);
                reading.at.pagePlaces.push_back(place);
                loadPageItemIds((*pages)[place], mod.pages[place], reading);
            }
        }
        mod.files = std::move(reading.files);
        // the conditions are read after every row, and their problems are put in their places
        std::ranges::stable_sort(reading.problems, isBefore);
        mod.problems = std::move(reading.problems);
        return mod;
    }

    /**
     * Loads the `spec.json` of every folder in the config specs' folder, which is `Data\ModsConfigFO4\Config` in
     * the game. A config spec that cannot be read is a mod that is not available, named by its folder, and so
     * are two config specs with one `modName`: neither is picked. Both are a problem of the config spec. A mod that is
     * not available has the reason in the log, and so has each problem of a config spec, with the config spec's name
     * Reads a config spec's `spec.json` from its file, as `loadConfigSpec` reads its text. The error says why it
     * cannot be read, also for a file that cannot be opened.
     */
    std::expected<model::Mod, std::string> loadConfigSpecFile(const std::filesystem::path& file, const model::Translations& translations, const PluginChecks& plugins,
        const FileFolders& folders)
    {
        std::ifstream stream(file, std::ios::binary);
        if (!stream) {
            return std::unexpected("The file cannot be opened"s);
        }
        std::ostringstream text;
        text << stream.rdbuf();
        return loadConfigSpec(text.str(), translations, plugins, folders);
    }

    /**
     * The mod of a config spec whose `spec.json` cannot be read: not available, named by its folder, with the
     * reason as its error.
     */
    model::Mod notLoadedConfigSpec(const std::string& folder, const std::string& reason)
    {
        auto mod = notAvailableMod(folder, folder, "its spec.json cannot be read");
        mod.problems.push_back({ .severity = model::Severity::Error, .row = SPEC_FILE, .message = std::format("It cannot be read: {}.", reason), .where = {}, .path = {} });
        mod.folder = folder;
        return mod;
    }

    /**
     * Logs why a config spec's mod is not available, each of its problems, with the config spec's name first, and
     * how many errors and warnings it has.
     */
    void logConfigSpec(const model::Mod& mod)
    {
        if (!mod.notAvailable.empty()) {
            logger::warn("Config spec '{}' is not available: {}", mod.name, mod.notAvailable);
        }
        logProblems(mod);
    }

    /**
     * The folders of the running game a config spec's files are read from: the game's Data folder, and
     * `Documents\My Games\Fallout4VR`.
     */
    FileFolders gameFileFolders()
    {
        return { .data = "Data", .myGames = common::getRelativePathInDocuments(R"(\My Games\Fallout4VR)") };
    }

    /**
     * The checks of the running game: a game plugin it loaded, and an F4SE plugin F4SE loaded.
     */
    PluginChecks gamePluginChecks()
    {
        return { .isGamePluginLoaded = isGamePluginLoaded, .isF4sePluginLoaded = isF4sePluginLoaded };
    }

    /**
     * Loads the `spec.json` of every folder in the config specs' folder, which is `Data\ModsConfigFO4\Config` in
     * the game. A config spec that cannot be read is a mod that is not available, named by its folder, and so
     * are two config specs with one `modName`: neither is picked. Both are a problem of the config spec. A mod that is
     * not available has the reason in the log, and so has each problem of a config spec, with the config spec's name
     * first, and how many errors and warnings each config spec has. No such folder is no mods.
     */
    std::vector<model::Mod> loadConfigSpecs(const std::filesystem::path& configFolder, const model::Translations& translations, const PluginChecks& plugins,
        const FileFolders& folders)
    {
        std::vector<model::Mod> mods;
        std::error_code error;
        for (std::filesystem::directory_iterator folder(configFolder, error), end; !error && folder != end; folder.increment(error)) {
            const auto file = folder->path() / SPEC_FILE;
            std::error_code noFile;
            if (!std::filesystem::is_regular_file(file, noFile)) {
                continue;
            }
            auto mod = loadConfigSpecFile(file, translations, plugins, folders);
            const auto folderName = toLogText(folder->path().filename());
            if (!mod) {
                mod = notLoadedConfigSpec(folderName, mod.error());
            }
            mod->folder = folderName;
            mods.push_back(std::move(*mod));
        }

        std::map<std::string, int> names;
        for (const auto& mod : mods) {
            ++names[common::str_tolower(mod.name)];
        }
        for (auto& mod : mods) {
            if (mod.notAvailable.empty() && names[common::str_tolower(mod.name)] > 1) {
                auto folder = std::move(mod.folder);
                mod = notAvailableMod(mod.name, mod.displayName, "another config spec has its modName");
                mod.folder = std::move(folder);
                mod.problems.push_back({ .severity = model::Severity::Error,
                    .row = SPEC_FILE,
                    .message = std::format("Another config spec has the modName \"{}\": neither is read.", mod.name),
                    .where = {},
                    .path = "modName" });
            }
            logConfigSpec(mod);
        }
        return mods;
    }

    /**
     * The config specs of the running game, from `Data\ModsConfigFO4\Config` in the game's folder, with their
     * conditions on a plugin answered by the plugins the game loaded.
     */
    std::vector<model::Mod> loadInstalledConfigSpecs(const model::Translations& translations)
    {
        auto mods = loadConfigSpecs(CONFIG_SPEC_FOLDER, translations, gamePluginChecks(), gameFileFolders());
        logger::info("Loaded {} config specs from '{}'", mods.size(), CONFIG_SPEC_FOLDER);
        return mods;
    }

    /**
     * Every mod the menu lists: the MCM mods and then the config specs. A mod that has both is shown by its config spec
     * alone, matched by its `modName` without regard to case, also one that is not available: its MCM menu is left out.
     */
    std::vector<model::Mod> withConfigSpecs(std::vector<model::Mod> mcmMods, std::vector<model::Mod> configSpecs)
    {
        std::set<std::string> names;
        for (const auto& mod : configSpecs) {
            names.insert(common::str_tolower(mod.name));
        }
        std::erase_if(mcmMods, [&names](const model::Mod& mod) {
            if (!names.contains(common::str_tolower(mod.name))) {
                return false;
            }
            logger::info("The MCM menu of '{}' is left out: its config spec is listed", mod.name);
            return true;
        });
        std::ranges::move(configSpecs, std::back_inserter(mcmMods));
        return mcmMods;
    }
}
