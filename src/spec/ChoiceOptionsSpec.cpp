#include "ChoiceOptionsSpec.h"

#include <array>
#include <format>
#include <optional>
#include <tuple>

#include "StorageSpec.h"
#include "model/FileChoices.h"
#include "model/SettingValues.h"

namespace mods_config::spec
{
    namespace
    {
        // the fields of an option of a choice, and of where a choice lists its options, as the schema has them
        // (docs/design/schema.md)
        constexpr std::array<std::string_view, 5> OPTION_FIELDS{ "value", "text", "help", "sets", "alsoSets" };
        constexpr std::array<std::string_view, 3> LIST_PROPERTY_FIELDS{ "form", "scriptName", "propertyName" };
        constexpr std::array<std::string_view, 4> LIST_FOLDER_FIELDS{ "root", "path", "mask", "noneText" };

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
     * The options of a stepper's or a dropdown's `valueOptions`, which is none for a setting with none: its own
     * `options`, the shared list its `sharedOptions` names, where it reads them in the game (`listFromForm`,
     * `listFromProperty`), the folder whose files they are (`listFromFolder`), or that its mod's client gives them, with a
     * `listFromClientCallback` that is true. A choice with none of them has no options, and is reported, so it is
     * disabled.
     */
    void loadChoiceOptions(const model::SettingType type, const model::StoredType valueType, const Json* options, model::ValueOptions& valueOptions, ConfigSpecReading& reading)
    {
        const Json* list = nullptr;
        std::string sharedName;
        if (options) {
            const auto own = options->find("options");
            if (own != options->end()) {
                list = &*own;
            } else {
                sharedName = getText(*options, "sharedOptions");
                list = findSharedList(reading.sharedLists, sharedName);
            }
        }
        if (list && list->is_array()) {
            valueOptions.options = loadOptions(*list, sharedName, valueType, reading);
            return;
        }
        if (options && (options->contains("listFromForm") || options->contains("listFromProperty"))) {
            valueOptions.gameOptions = loadGameOptions(*options, valueType, reading);
        } else if (options && options->contains("listFromFolder")) {
            valueOptions.folderOptions = loadFolderOptions(*options, type, valueType, reading);
        } else if (options && getBool(*options, "listFromClientCallback")) {
            valueOptions.clientOptions = true;
        } else {
            reading.report(model::Severity::Error, "It has no list of options, or names no shared list of its config spec: it has no options.");
        }
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
