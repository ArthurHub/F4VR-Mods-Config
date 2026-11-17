#include "SettingSpec.h"

#include <algorithm>
#include <array>
#include <format>
#include <optional>
#include <tuple>
#include <variant>

#include "ActionSpec.h"
#include "FrameworkTypeSpec.h"
#include "StorageSpec.h"
#include "common/CommonUtils.h"
#include "model/FileChoices.h"
#include "model/OptionSets.h"
#include "model/SettingValues.h"

using namespace std::string_literals;

namespace mods_config::spec
{
    namespace
    {
        // the fields of a setting row and of its parts, as the schema has them (docs/design/schema.md)
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
         * A length of a key: a whole number above 0, or else 0, which is no limit.
         */
        int getLength(const Json& value, const char* key)
        {
            const auto found = value.find(key);
            return found != value.end() && found->is_number_integer() ? (std::max)(found->get<int>(), 0) : 0;
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
                    options.push_back(
                        { .value = static_cast<int>(place), .text = own ? readShownText(option, reading) : reading.translations.translate(option.get<std::string>()) });
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
        std::optional<model::FolderOptions> loadFolderOptions(const Json& valueOptions, const model::SettingType type, const model::StoredType valueType,
            ConfigSpecReading& reading)
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
                reading.report(model::Severity::Warning,
                    std::format("Its default {} is not in {}: it is invalid until it is.", model::fileNameOf(value), model::folderText(folder)));
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
    }

    /**
     * The setting type of a row's `type`, as `slider` or `cf.color`. None for a row that is no setting.
     */
    std::optional<model::SettingType> findSettingType(const std::string_view type)
    {
        const auto found = std::ranges::find(SETTING_TYPES, type, &SettingTypeName::first);
        return found != SETTING_TYPES.end() ? std::optional(found->second) : std::nullopt;
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
     * One with a storage too is an error, and so is any other type with them: their sets are ignored. Its fields are checked here.
     */
    model::Setting loadSetting(const Json& row, const model::SettingType type, const Inherited& above, ConfigSpecReading& reading)
    {
        checkFields(row, SETTING_FIELDS, reading);
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
}
