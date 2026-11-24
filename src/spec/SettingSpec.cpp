#include "SettingSpec.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <format>
#include <limits>
#include <optional>
#include <variant>

#include "ActionSpec.h"
#include "ChoiceOptionsSpec.h"
#include "FrameworkTypeSpec.h"
#include "StorageSpec.h"
#include "common/CommonUtils.h"
#include "model/OptionSets.h"
#include "model/SettingValues.h"

using namespace std::string_literals;

namespace mods_config::spec
{
    namespace
    {
        // the fields of a setting row and of its parts, as the schema has them (docs/design/schema.md)
        constexpr std::array<std::string_view, 19> SETTING_FIELDS{ "type",
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
            "omitDefault",
            "warnings",
            "action",
            "applies",
            "visibleWhen",
            "enabledWhen",
            "countsAs",
            "searchable" };
        // a setting's parts:
        constexpr std::array<std::string_view, 23> VALUE_OPTIONS_FIELDS{ "min",
            "max",
            "step",
            "options",
            "sharedOptions",
            "listFromForm",
            "listFromProperty",
            "listFromFolder",
            "listFromClientCallback",
            "allowTyped",
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
         * The `valueOptions` of a setting: the least, the most and the step of a number, the fewest and the most
         * characters of a text input of a text and of a typed value of a choice (`allowTyped`), the hands, the ways and the buttons a binding takes, and the options of a
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
            loadFrameworkValueOptions(type, hasOptions ? &*found : nullptr, valueOptions, reading);
            if (listsOptions(type)) {
                loadChoiceOptions(type, valueType, hasOptions ? &*found : nullptr, valueOptions, reading);
            }
            const bool isText = type == model::SettingType::TextInput && valueType == model::StoredType::String;
            if (hasOptions && (isText || valueOptions.typedOption)) {
                valueOptions.minLength = getLength(*found, "minLength");
                valueOptions.maxLength = getLength(*found, "maxLength");
                const auto allowEmpty = found->find("allowEmpty");
                if (allowEmpty != found->end() && allowEmpty->is_boolean() && !allowEmpty->get<bool>()) {
                    valueOptions.minLength = (std::max)(valueOptions.minLength, 1);
                }
            }
            return valueOptions;
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

        /**
         * Whether a search finds the row by its text: unless its `searchable` is false. One that is not true or false
         * is left out, which is reported.
         */
        bool loadSearchable(const Json& row, ConfigSpecReading& reading)
        {
            const auto found = row.find("searchable");
            if (found == row.end()) {
                return true;
            }
            if (!found->is_boolean()) {
                const Into into(reading, "searchable");
                reading.report(model::Severity::Warning, "Its searchable is not true or false: it is left out, and search finds the row.");
                return true;
            }
            return found->get<bool>();
        }

        // the most decimals a number's display can have, as MCM's menu writes a number at the most
        constexpr int MAX_DECIMALS = 6;

        /**
         * The places a switcher takes of its sections' limits while it is on: its `countsAs`, a whole number of 1 or
         * more, and 1 without it. Any other is an error and counts 1. Only a switcher in a section with a `maxOn` is
         * counted, so on any other row it is ignored, and reported.
         */
        int loadCountsAs(const Json& row, const model::SettingType type, const Inherited& own, ConfigSpecReading& reading)
        {
            const auto found = row.find("countsAs");
            if (found == row.end()) {
                return 1;
            }
            const Into into(reading, "countsAs");
            if (type != model::SettingType::Switcher || !own.underLimit) {
                reading.report(model::Severity::Warning,
                    type != model::SettingType::Switcher ? "It has a countsAs and is no switcher: it is ignored."
                                                         : "It has a countsAs and no section above it has a maxOn: it is ignored.");
                return 1;
            }
            if (found->is_number_integer() && found->get<std::int64_t>() >= 1) {
                return static_cast<int>((std::min)(found->get<std::int64_t>(), static_cast<std::int64_t>((std::numeric_limits<int>::max)())));
            }
            reading.report(model::Severity::Error, std::format("Its countsAs {} is not a whole number of 1 or more: it counts 1.", found->dump()));
            return 1;
        }

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

        /**
         * Whether a setting's options set other settings, as their `sets` say: only a stepper's and a dropdown's with no
         * storage do, and the sets of any other are ignored, which is reported. The default and the `applies` of one that
         * does are left out, and reported too.
         */
        bool readsOptionSets(const Json& row, const model::SettingType type, ConfigSpecReading& reading)
        {
            if (!hasOptionSets(row, reading)) {
                return false;
            }
            if (type != model::SettingType::Stepper && type != model::SettingType::Dropdown) {
                const Into into(reading, "valueOptions");
                reading.report(model::Severity::Error, "Only a stepper and a dropdown have options that set other settings: its options' sets are ignored.");
                return false;
            }
            if (row.contains("storage")) {
                const Into into(reading, "storage");
                reading.report(model::Severity::Error, "It has a storage, and options that set other settings: its options' sets are ignored.");
                return false;
            }
            if (row.contains("default")) {
                const Into into(reading, "default");
                reading.report(model::Severity::Warning, "Its default is left out: it is the option whose sets are the defaults of their settings.");
            }
            if (findApplies(row)) {
                const Into into(reading, "applies");
                reading.report(model::Severity::Warning, "Its applies is left out: each setting its options set takes effect as its own says.");
            }
            return true;
        }

        /**
         * How a setting of a table type keeps its value: as a TOML table, or as the line of an INI file. A table type kept
         * in anything but a file's value is not available, which is reported, and its storage is then unknown.
         */
        model::TableForm loadTableForm(const model::SettingType type, model::Storage& storage, ConfigSpecReading& reading)
        {
            if (!model::isTableType(type) || std::holds_alternative<model::UnknownStorage>(storage)) {
                return model::TableForm::Toml;
            }
            const auto* file = std::get_if<model::FileStorage>(&storage);
            if (!file) {
                const std::string kind(model::valueKindName(type));
                const Into into(reading, "storage");
                reading.report(model::Severity::Error, std::format("A {} is kept in a value of a TOML or an INI file, and it is not: it is not available.", kind));
                storage = model::UnknownStorage{ .type = kind };
                return model::TableForm::Toml;
            }
            return reading.files.at(file->file).format == model::FileFormat::Ini ? model::TableForm::IniLine : model::TableForm::Toml;
        }

        /**
         * Whether a Save keeps no value of the setting that is its default, and takes its key out of its file: the nearest
         * `omitDefault`, its own, its section's, its page's or the root's (`inherited`), or else its file's. It needs a
         * value in a file and a default to compare, so a setting kept elsewhere, a read-only one and one with no default
         * never has it: its own `true` is reported, one from above or from its file ignored.
         */
        bool loadOmitDefault(const Json& row, const Inherited& inherited, const model::Storage& storage, const bool readOnly, const bool hasDefault, ConfigSpecReading& reading)
        {
            const auto* file = std::get_if<model::FileStorage>(&storage);
            const bool canOmit = file && !readOnly && hasDefault;
            if (const auto own = row.find("omitDefault"); !canOmit && own != row.end() && own->is_boolean() && own->get<bool>()) {
                const auto* reason = std::holds_alternative<model::SetsStorage>(storage) ? "its options set other settings, which omit their own defaults or not"
                                     : !file                                             ? "it is not kept in a file"
                                     : readOnly                                          ? "it is read-only"
                                                                                         : "it has no default";
                if (!std::holds_alternative<model::UnknownStorage>(storage)) {
                    const Into into(reading, "omitDefault");
                    reading.report(model::Severity::Warning, std::format("Its omitDefault is ignored: {}.", reason));
                }
                return false;
            }
            return canOmit && inherited.omitDefault.value_or(reading.files.at(file->file).omitDefault);
        }

        /**
         * Reports a setting kept in a file that is there and has no value at its path: it shows its default, which a Save
         * writes once the setting is changed, or none where it is read-only. A setting that omits its default is not
         * reported, as its file leaves its value out while it is the default, and neither is a row the loader made, as a
         * sphere's file leaves out most keys.
         */
        void reportMissingValue(const model::Storage& storage, const bool readOnly, const bool omitDefault, const bool made, ConfigSpecReading& reading)
        {
            const auto* file = std::get_if<model::FileStorage>(&storage);
            if (!file || omitDefault || made) {
                return;
            }
            if (const auto values = reading.fileValues.find(file->file); values != reading.fileValues.end() && !values->second->contains(file->path)) {
                const Into into(reading, "storage");
                reading.report(model::Severity::Warning,
                    std::format("Its file \"{}\" has no value {}: {}.",
                        file->file,
                        values->second->pathName(file->path),
                        readOnly ? "it shows none" : "it shows its default, which a Save writes once it is changed"));
            }
        }

        /**
         * The id of a setting, its own or the one its storage gives it, and what a condition knows of the setting, kept
         * by the id. A second setting with an id is an error, which is reported: a condition names the first, so the
         * second has no `SettingInfo`, and so is one with the id of a section or a button. An empty id, which no condition
         * names, has none either.
         */
        std::pair<std::string, SettingInfo*> registerSetting(const Json& row, const model::Storage& storage, const model::SettingType type, const model::StoredType valueType,
            ConfigSpecReading& reading)
        {
            auto id = getText(row, "id");
            if (id.empty()) {
                id = idOf(storage);
            }
            if (id.empty()) {
                return { std::move(id), nullptr };
            }
            // the ids of sections and buttons are read after every setting of a config spec, so only as the content a mod's
            // client gives a page is read are there any here
            if (reading.itemIds.contains(common::str_tolower(id))) {
                reading.report(model::Severity::Error, std::format("A section or a button has the id \"{}\": no condition names this setting. Give it an id of its own.", id));
                return { std::move(id), nullptr };
            }
            const auto [found, added] = reading.settings.emplace(common::str_tolower(id), SettingInfo{ .id = id, .type = type, .valueType = valueType });
            if (!added) {
                reading.report(model::Severity::Error,
                    std::format("A setting before it has the id \"{}\": a condition names that one. Give one of them an id of its own.", found->second.id));
                return { std::move(id), nullptr };
            }
            return { std::move(id), &found->second };
        }

        /**
         * Which of the framework's words a dropdown the loader made for a sphere has, by its `madeWords`.
         */
        model::SphereWords sphereWordsOf(const Json& row)
        {
            const auto words = getText(row, MADE_WORDS);
            return words == "visibility"     ? model::SphereWords::Visibility
                   : words == "orientation"  ? model::SphereWords::Orientation
                   : words == "style"        ? model::SphereWords::Style
                   : words == "bindingIcons" ? model::SphereWords::BindingIcons
                                             : model::SphereWords::None;
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
     * `applies` or the nearest above it, its `default`, how its number is shown, the options of a choice, its action,
     * which Save runs, and the places a switcher takes of its sections' limits.
     * A read-only setting has no default, as it has no Reset. One kept in a file can omit its default, by the nearest
     * `omitDefault` or its file's (`loadOmitDefault`), and then its file missing its value is no warning.
     * Two settings with one id are an error of the config spec, which is reported: a condition names the first, and an
     * author gives one of them an id of its own. Its conditions and its warnings are read once every setting of the
     * config spec is (`loadConditions`).
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
        const bool sets = readsOptionSets(row, type, reading);
        auto storage = sets ? model::Storage(model::SetsStorage{}) : loadStorage(row, own.defaultFile, valueType, reading);
        const auto tableForm = loadTableForm(type, storage, reading);
        const bool readOnly = getBool(row, "readOnly");
        const bool made = row.contains(MADE_ROW_PATH);
        auto [id, info] = registerSetting(row, storage, type, valueType, reading);
        auto valueOptions = loadValueOptions(row, type, valueType, reading);
        if (sets && valueOptions.typedOption) {
            const Into into(reading, "valueOptions");
            const Into intoTyped(reading, "allowTyped");
            reading.report(model::Severity::Warning, "A choice whose options set other settings takes no allowTyped, as a typed text sets none: it is ignored.");
            valueOptions.typedOption = false;
            valueOptions.minLength = 0;
            valueOptions.maxLength = 0;
        }
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
            valueOptions.sphereWords = sphereWordsOf(row);
        }
        auto defaultValue = sets || readOnly || (made && !row.contains("default")) ? std::nullopt : loadDefault(row, type, valueType, tableForm, valueOptions, reading);
        const bool omitDefault = loadOmitDefault(row, own, storage, readOnly, defaultValue.has_value(), reading);
        reportMissingValue(storage, readOnly, omitDefault, made, reading);
        auto text = getLabel(row, reading);
        auto help = readShownText(row, "help", reading);
        auto display = loadDisplay(row, reading);
        auto action = loadSettingAction(row, storage, valueType, reading);
        const auto countsAs = loadCountsAs(row, type, own, reading);
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
            .warnings = {},
            .omitDefault = omitDefault,
            .notSearched = !loadSearchable(row, reading),
            .countsAs = countsAs };
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
}
