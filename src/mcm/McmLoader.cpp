#include "McmLoader.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <format>
#include <fstream>
#include <map>
#include <optional>
#include <sstream>
#include <unordered_set>

#include <nlohmann/json.hpp>

#include "FallUITranslations.h"
#include "McmConfigImages.h"
#include "common/CommonUtils.h"
#include "model/TextMarkup.h"

using namespace mods_config;

namespace
{
    using Json = nlohmann::json;

    // MCM's plugin in Fallout 4 VR, and where its mods have their configs, from the game's folder
    constexpr auto MCM_DLL = "mcm_vr.dll";
    constexpr auto MCM_CONFIG_FOLDER = R"(Data\MCM\Config)";

    // the game's folder of its menus, which a FallUI mod names its language files from
    constexpr auto INTERFACE_FOLDER = R"(Data\Interface)";

    // the most lines a spacer is high, whatever its config says
    constexpr auto MAX_SPACER_LINES = 20.0;

    // the key of a row by which a FallUI mod names its language files
    constexpr auto FALLUI_LANGUAGE_FILE = "$fallUILanguageFile";

    /**
     * The text of a key. It is empty when the value has no such key, or the key holds something else than a text.
     */
    std::string getText(const Json& value, const char* key)
    {
        const auto found = value.find(key);
        return found != value.end() && found->is_string() ? found->get<std::string>() : std::string();
    }

    /**
     * Whether a key is set, as MCM's menu reads a flag in Flash: true, a number that is not zero, as FallUI
     * writes `1`, and a text that is not empty, as REPUTATION writes `"true"`.
     */
    bool getFlag(const Json& value, const char* key)
    {
        const auto found = value.find(key);
        if (found == value.end()) {
            return false;
        }
        if (found->is_string()) {
            // Flash has any text but an empty one as true, also `"false"`
            return !found->get_ref<const std::string&>().empty();
        }
        return found->is_boolean() ? found->get<bool>() : found->is_number() && found->get<double>() != 0;
    }

    /**
     * The number of lines a spacer is high: its `numLines`, and one line without it.
     */
    int getSpacerLines(const Json& row)
    {
        const auto found = row.find("numLines");
        return found != row.end() && found->is_number() ? static_cast<int>(std::clamp(found->get<double>(), 1.0, MAX_SPACER_LINES)) : 1;
    }

    /**
     * What translates the texts of a config: the game's translations, and before them FallUI's own for a
     * FallUI mod.
     */
    struct ConfigTexts
    {
        const model::Translations& game;
        std::optional<mcm::FallUITranslations> fallUI;
    };

    /**
     * A text that the player sees, in the game's language. A FallUI mod can name the text's token beside it.
     */
    std::string toShownText(std::string text, const std::string& fallUIToken, const ConfigTexts& texts)
    {
        if (texts.fallUI) {
            text = texts.fallUI->translate(text, fallUIToken);
        }
        return texts.game.translate(std::move(text));
    }

    /**
     * The text of a key that the player sees, in the game's language. A FallUI mod can name the text's token
     * in a key of its own beside it: the key's name after a `$`.
     */
    std::string getShownText(const Json& value, const char* key, const ConfigTexts& texts)
    {
        return toShownText(getText(value, key), getText(value, ("$"s + key).c_str()), texts);
    }

    /**
     * A text of a row as plain text on one line, as a label and the help line show it: in the game's language,
     * and then without its tags. A FallUI mod has its tokens inside the tags, so they are removed after the
     * translation, and a token of the game's that is left alone inside them is translated after.
     */
    std::string toPlainText(const std::string& shownText, const ConfigTexts& texts)
    {
        return texts.game.translate(model::removeMarkup(shownText));
    }

    /**
     * The text of a row that is not a text row, as MCM's menu shows it: it sets every such text as HTML,
     * whatever the row's `html` says, so its tags make no text of it.
     */
    std::string getLabel(const Json& row, const ConfigTexts& texts)
    {
        return toPlainText(getShownText(row, "text", texts), texts);
    }

    /**
     * The pieces of the text of a row that is not a text row, with the size, the color and the face its tags
     * give them, each in the game's language. Empty where its tags give none, as the text is then drawn plain.
     */
    std::vector<model::TextRun> getLabelRuns(const Json& row, const ConfigTexts& texts)
    {
        auto runs = model::markupRuns(getShownText(row, "text", texts));
        if (std::ranges::none_of(runs, [](const model::TextRun& run) {
                return run.size || run.color || run.bold || run.italic || run.underline;
            })) {
            return {};
        }
        for (auto& run : runs) {
            run.text = texts.game.translate(std::move(run.text));
        }
        return runs;
    }

    /**
     * The help of a row, as the help line shows it, on one line.
     */
    std::string getHelp(const Json& row, const ConfigTexts& texts)
    {
        return toPlainText(getShownText(row, "help", texts), texts);
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
     * The `valueOptions` of a row: the range of a slider, and the options of a stepper and a dropdown.
     * An MCM option is a text, and the value that is stored for it is its place in the list. So an entry that
     * is no text stays in the list, with no text, and the options after it keep their values.
     */
    model::ValueOptions loadValueOptions(const Json& row, const ConfigTexts& texts)
    {
        model::ValueOptions valueOptions;
        const auto found = row.find("valueOptions");
        if (found == row.end() || !found->is_object()) {
            return valueOptions;
        }
        valueOptions.min = getNumber(*found, "min", valueOptions.min);
        valueOptions.max = getNumber(*found, "max", valueOptions.max);
        valueOptions.step = getNumber(*found, "step", valueOptions.step);

        if (const auto options = found->find("options"); options != found->end() && options->is_array()) {
            for (const auto& option : *options) {
                const auto index = static_cast<int>(valueOptions.options.size());
                valueOptions.options.push_back({ .value = index, .text = toShownText(option.is_string() ? option.get<std::string>() : std::string(), {}, texts) });
            }
        }
        return valueOptions;
    }

    using StoredTypeName = std::pair<std::string_view, model::StoredType>;

    // what a ModSetting and a property of a script are kept as, by the end of the `sourceType` of their rows
    constexpr std::array STORED_TYPES{
        StoredTypeName{ "Bool", model::StoredType::Bool },
        StoredTypeName{ "Int", model::StoredType::Int },
        StoredTypeName{ "Float", model::StoredType::Float },
        StoredTypeName{ "String", model::StoredType::String },
    };

    constexpr std::string_view MOD_SETTING_SOURCE = "ModSetting";
    constexpr std::string_view PROPERTY_SOURCE = "PropertyValue";

    /**
     * The type a `sourceType` ends in after the name of its source, as `ModSettingFloat` does in `Float`.
     * None for one of another source, and for one that ends in no type of MCM.
     */
    std::optional<model::StoredType> findStoredType(const std::string_view sourceType, const std::string_view source)
    {
        if (!sourceType.starts_with(source)) {
            return std::nullopt;
        }
        const auto type = std::ranges::find(STORED_TYPES, sourceType.substr(source.size()), &StoredTypeName::first);
        return type != STORED_TYPES.end() ? std::optional(type->second) : std::nullopt;
    }

    /**
     * A form as a `sourceForm` names it: the plugin's file and the form's id in hex, with or without `0x`,
     * as `MyMod.esp|1234`. None for a text that is not written so.
     */
    std::optional<std::pair<std::string, std::uint32_t>> parseSourceForm(const std::string& sourceForm)
    {
        const auto separator = sourceForm.find('|');
        auto plugin = common::trim(sourceForm.substr(0, separator));
        auto id = separator == std::string::npos ? std::string() : common::trim(sourceForm.substr(separator + 1));
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
     * Where a stepper or a dropdown reads its options in the game, as MCM's menu reads them in place of its
     * `options`: a `listFromForm` names a FormList, as `MyMod.esp|900`, or else a `listFromProperty` names a
     * property of a script, by its `form`, its `propertyName` and its `scriptName`. A row with a
     * `sharedOptions` has the options of that list. An option's value is its place in the list, as MCM stores it.
     * A list that is not written so leaves the row with no options, as MCM's menu does, with the reason in the log.
     */
    void loadGameOptions(const Json& row, model::ValueOptions& valueOptions)
    {
        const auto found = row.find("valueOptions");
        if (found == row.end() || !found->is_object() || !getText(*found, "sharedOptions").empty()) {
            return;
        }
        if (const auto formList = getText(*found, "listFromForm"); !formList.empty()) {
            valueOptions.options.clear();
            if (auto form = parseSourceForm(formList)) {
                valueOptions.gameOptions = model::GameOptions{ .plugin = std::move(form->first), .formId = form->second };
            } else {
                logger::warn("The FormList '{}' of the row '{}' is not a plugin and an id in hex", formList, getText(row, "text"));
            }
            return;
        }
        const auto property = found->find("listFromProperty");
        if (property == found->end() || !property->is_object()) {
            return;
        }
        valueOptions.options.clear();
        const auto formText = getText(*property, "form");
        auto form = parseSourceForm(formText);
        auto name = getText(*property, "propertyName");
        if (form && !name.empty()) {
            valueOptions.gameOptions =
                model::GameOptions{ .plugin = std::move(form->first), .formId = form->second, .script = getText(*property, "scriptName"), .property = std::move(name) };
        } else {
            logger::warn("The property '{}' of the form '{}' that the row '{}' lists is not a name, and a plugin and an id in hex", name, formText, getText(row, "text"));
        }
    }

    /**
     * Where a row keeps its value, by the `sourceType` of its `valueOptions`. A row with none is kept
     * nowhere. A `GlobalValue` names its global in `sourceForm`, as `MyMod.esp|1234`. A ModSetting is named
     * by the `id` of its row as the config has it, also where another row has that id too, so one with no id
     * is kept nowhere. A `PropertyValue` names the form of its script in `sourceForm`, its property in
     * `propertyName`, and can name the script in `scriptName`.
     * Any other source, and a global or a property that is not written so, is one the menu does not read.
     */
    model::Storage loadStorage(const Json& row)
    {
        const auto valueOptions = row.find("valueOptions");
        if (valueOptions == row.end() || !valueOptions->is_object()) {
            return model::NoStorage{};
        }
        auto sourceType = getText(*valueOptions, "sourceType");
        if (sourceType.empty() || (sourceType.starts_with(MOD_SETTING_SOURCE) && getText(row, "id").empty())) {
            return model::NoStorage{};
        }
        const auto sourceForm = getText(*valueOptions, "sourceForm");
        if (sourceType == "GlobalValue") {
            if (auto form = parseSourceForm(sourceForm)) {
                return model::GlobalStorage{ .plugin = std::move(form->first), .formId = form->second };
            }
            logger::warn("The global '{}' of the row '{}' is not a plugin and an id in hex", sourceForm, getText(row, "text"));
        }
        if (const auto type = findStoredType(sourceType, MOD_SETTING_SOURCE)) {
            return model::ModSettingStorage{ .name = getText(row, "id"), .type = *type };
        }
        if (const auto type = findStoredType(sourceType, PROPERTY_SOURCE)) {
            auto form = parseSourceForm(sourceForm);
            auto name = getText(*valueOptions, "propertyName");
            if (form && !name.empty()) {
                return model::PropertyStorage{ .plugin = std::move(form->first),
                    .formId = form->second,
                    .script = getText(*valueOptions, "scriptName"),
                    .name = std::move(name),
                    .type = *type };
            }
            logger::warn("The property '{}' of the form '{}' of the row '{}' is not a name, and a plugin and an id in hex", name, sourceForm, getText(row, "text"));
        }
        return model::UnknownStorage{ .type = std::move(sourceType) };
    }

    /**
     * A text as the number MCM's menu reads it as, and 0 for a text that is none.
     */
    double toNumber(const std::string_view text)
    {
        double number = 0.0;
        const auto* end = text.data() + text.size();
        const auto [read, error] = std::from_chars(text.data(), end, number);
        return error == std::errc() && read == end ? number : 0.0;
    }

    // the least and the most a whole number of Papyrus is
    constexpr double MIN_WHOLE = -2147483648.0;
    constexpr double MAX_WHOLE = 2147483647.0;

    /**
     * A parameter of a button's action, in the type the script gets it in, as MCM's menu types it. A text
     * that begins with `{i}` is a whole number, with `{f}` a number, and with `{b}` a bool: off for nothing,
     * `0` and `false`, and on for anything else. Any other text is a text. A JSON bool is a bool, and a JSON
     * number is a number, also one that is written whole: Papyrus does not call a function that takes a
     * number with a whole one. Anything else is an empty text, so the parameters after it keep their places.
     */
    model::Value loadArgument(const Json& param)
    {
        if (param.is_boolean()) {
            return param.get<bool>();
        }
        if (param.is_number()) {
            return param.get<float>();
        }
        if (!param.is_string()) {
            return std::string();
        }
        auto text = param.get<std::string>();
        const std::string_view typed = std::string_view(text).substr((std::min)(text.size(), std::size_t{ 3 }));
        if (text.starts_with("{i}")) {
            return static_cast<int>(std::clamp(toNumber(typed), MIN_WHOLE, MAX_WHOLE));
        }
        if (text.starts_with("{f}")) {
            return static_cast<float>(toNumber(typed));
        }
        if (text.starts_with("{b}")) {
            return !typed.empty() && typed != "0" && typed != "false";
        }
        return text;
    }

    constexpr std::string_view VALUE_TOKEN = "{value}";

    /**
     * What the value of a row is given to a script as, for a parameter that is `{value}` alone, as MCM's menu
     * types it by the row's `sourceType`: a global is a number, a ModSetting and a property are what they
     * are kept as, and a row with none is a text.
     */
    model::StoredType storedTypeOf(const model::Storage& storage)
    {
        if (std::holds_alternative<model::GlobalStorage>(storage)) {
            return model::StoredType::Float;
        }
        if (const auto* modSetting = std::get_if<model::ModSettingStorage>(&storage)) {
            return modSetting->type;
        }
        if (const auto* property = std::get_if<model::PropertyStorage>(&storage)) {
            return property->type;
        }
        return model::StoredType::String;
    }

    using TypePrefix = std::pair<std::string_view, model::StoredType>;

    // what a parameter is given to a script as, by the start of its text
    constexpr std::array TYPE_PREFIXES{
        TypePrefix{ "{i}", model::StoredType::Int },
        TypePrefix{ "{f}", model::StoredType::Float },
        TypePrefix{ "{b}", model::StoredType::Bool },
    };

    /**
     * A parameter of a value row's action that has `{value}` in it, which the store fills as the value is
     * set. `{value}` alone is the value in the type of the row's storage. A text that begins with `{i}`,
     * `{f}` or `{b}` is the rest of it in that type, and any other text is a text.
     */
    model::ValueArgument loadValueArgument(const std::string_view text, const std::size_t place, const model::Storage& storage)
    {
        if (text == VALUE_TOKEN) {
            return { .place = place, .text = std::string(text), .type = storedTypeOf(storage) };
        }
        for (const auto& [prefix, type] : TYPE_PREFIXES) {
            if (text.starts_with(prefix)) {
                return { .place = place, .text = std::string(text.substr(prefix.size())), .type = type };
            }
        }
        return { .place = place, .text = std::string(text), .type = model::StoredType::String };
    }

    /**
     * What the help line says of an action the menu does not run, by its `type`. MCM's menu runs none of these
     * either but a `CallExternalFunction`, which calls a function an F4SE plugin gives a Flash menu.
     */
    std::string unknownActionReason(const std::string& type)
    {
        if (type == "CallExternalFunction") {
            return "Not available: a CallExternalFunction calls a Flash function of an F4SE plugin, and the menu has no Flash.";
        }
        return type.empty() ? "Not available: the action has no type."s : std::format("Not available: the menu does not run a {}.", type);
    }

    /**
     * The function a `CallFunction` or a `CallGlobalFunction` calls, with no parameters, or none for one
     * that is not written so, with the reason in the log. A `CallFunction` names the form of its script in
     * `form`, as `MyMod.esp|800`, and can name the script in `scriptName`. A `CallGlobalFunction` names its
     * script in `script`, and has no form.
     */
    std::optional<model::CallFunction> loadCall(const Json& row, const Json& action, const bool global)
    {
        auto function = getText(action, "function");
        if (global) {
            auto script = getText(action, "script");
            if (script.empty() || function.empty()) {
                logger::warn("The global function '{}' of the script '{}' of the row '{}' has no name, or no script", function, script, getText(row, "text"));
                return std::nullopt;
            }
            return model::CallFunction{ .plugin = {}, .formId = 0, .script = std::move(script), .function = std::move(function), .arguments = {}, .valueArguments = {} };
        }
        const auto formText = getText(action, "form");
        auto form = parseSourceForm(formText);
        if (!form || function.empty()) {
            logger::warn("The function '{}' of the form '{}' of the row '{}' is not a name, and a plugin and an id in hex", function, formText, getText(row, "text"));
            return std::nullopt;
        }
        return model::CallFunction{ .plugin = std::move(form->first),
            .formId = form->second,
            .script = getText(action, "scriptName"),
            .function = std::move(function),
            .arguments = {},
            .valueArguments = {} };
    }

    /**
     * What a row runs, by the `type` of its `action`, as MCM's menu runs it: a `CallFunction` and a
     * `CallGlobalFunction`, with the parameters of the function in `params`.
     * A row with no action runs nothing, and neither does a call that is not written so. Any other action is
     * one the menu does not run, with the reason, which the log has for a row as it is loaded.
     * A value row gives its storage: a parameter of it with `{value}` is filled as the value is set. A
     * button gives none, and its `{value}` is a text as it is written, as in MCM's menu.
     */
    model::Action loadAction(const Json& row, const model::Storage* valueStorage = nullptr)
    {
        const auto action = row.find("action");
        if (action == row.end() || !action->is_object()) {
            return model::NoAction{};
        }
        auto type = getText(*action, "type");
        if (type != "CallFunction" && type != "CallGlobalFunction") {
            logger::info("The row '{}' has an action '{}', which the menu does not run", getText(row, "text"), type);
            auto reason = unknownActionReason(type);
            return model::UnknownAction{ .type = std::move(type), .reason = std::move(reason) };
        }
        auto loaded = loadCall(row, *action, type == "CallGlobalFunction");
        if (!loaded) {
            return model::NoAction{};
        }
        auto& call = *loaded;
        if (const auto params = action->find("params"); params != action->end() && params->is_array()) {
            for (const auto& param : *params) {
                if (valueStorage && param.is_string() && param.get_ref<const std::string&>().contains(VALUE_TOKEN)) {
                    call.valueArguments.push_back(loadValueArgument(param.get_ref<const std::string&>(), call.arguments.size(), *valueStorage));
                    call.arguments.emplace_back(std::string());
                } else {
                    call.arguments.push_back(loadArgument(param));
                }
            }
        }
        return std::move(call);
    }

    /**
     * The names of the files of a folder that match a mask, in the order of their names. The mask is the
     * system's, as F4SE lists a folder for MCM's menu: `*` and `?`, in the folder alone. A folder in it is no
     * file. None for a folder that is not there.
     */
    std::vector<std::string> listFiles(const std::filesystem::path& folder, const std::string& mask)
    {
        std::vector<std::string> files;
        WIN32_FIND_DATAW found{};
        const auto pattern = folder / std::filesystem::path(std::u8string(mask.begin(), mask.end()));
        const auto search = FindFirstFileW(pattern.c_str(), &found);
        if (search == INVALID_HANDLE_VALUE) {
            return files;
        }
        do {
            if ((found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0) {
                const auto name = std::filesystem::path(found.cFileName).u8string();
                files.emplace_back(name.begin(), name.end());
            }
        } while (FindNextFileW(search, &found));
        FindClose(search);
        std::ranges::sort(files, {}, common::str_tolower);
        return files;
    }

    // the option of a `dropdownFiles` that is no file, which MCM's menu has first
    constexpr auto NO_FILE = "None";

    /**
     * The options of a `dropdownFiles`, as MCM's menu lists them: `None`, and the files of the folder its
     * `path` names from the game's folder that match its `mask`, or all of them with none. They are listed
     * as the config is loaded. A storage that keeps a text keeps the name of the file, and an empty text for
     * `None`. Any other keeps the place of the option, as a dropdown does.
     */
    std::vector<model::Option> loadFileOptions(const Json& row, const model::Storage& storage, const std::filesystem::path& gameFolder)
    {
        const auto valueOptions = row.find("valueOptions");
        const auto path = valueOptions != row.end() ? getText(*valueOptions, "path") : std::string();
        const auto mask = valueOptions != row.end() ? getText(*valueOptions, "mask") : std::string();
        const bool keepsName = storedTypeOf(storage) == model::StoredType::String;

        std::vector<model::Option> options{ { .value = keepsName ? model::Value(std::string()) : model::Value(0), .text = NO_FILE } };
        if (path.empty()) {
            return options;
        }
        const auto folder = gameFolder / std::filesystem::path(std::u8string(path.begin(), path.end()));
        for (const auto& file : listFiles(folder, mask.empty() ? "*" : mask)) {
            const auto place = static_cast<int>(options.size());
            options.push_back({ .value = keepsName ? model::Value(file) : model::Value(place), .text = file });
        }
        return options;
    }

    // what the name of a FallUI language file can have, for the `libName` of its row
    constexpr std::string_view LIB_NAME_TOKEN = "{libName}";

    /**
     * Gives each row with a `sharedOptions` the options of the list of that name in the config's `sharedLists`,
     * as MCM's menu does as it reads a page: in place of the row's own, and none for a name of no list.
     */
    void useSharedLists(Json& config)
    {
        const auto found = config.find("sharedLists");
        const Json* lists = found != config.end() && found->is_object() ? &*found : nullptr;
        const auto inRows = [lists](Json& page) {
            const auto content = page.find("content");
            if (content == page.end() || !content->is_array()) {
                return;
            }
            for (auto& row : *content) {
                const auto valueOptions = row.find("valueOptions");
                if (valueOptions == row.end() || !valueOptions->is_object()) {
                    continue;
                }
                const auto name = getText(*valueOptions, "sharedOptions");
                if (name.empty()) {
                    continue;
                }
                const auto list = lists ? lists->find(name) : Json::const_iterator();
                (*valueOptions)["options"] = lists && list != lists->end() ? *list : Json::array();
            }
        };

        inRows(config);
        if (const auto pages = config.find("pages"); pages != config.end() && pages->is_array()) {
            for (auto& page : *pages) {
                inRows(page);
            }
        }
    }

    /**
     * The language file a FallUI mod names in a row of its front page or of a page, with the `libName` of the
     * row for its `{libName}`. Empty for any other mod.
     */
    std::string findFallUILanguageFile(const Json& config)
    {
        const auto inRows = [](const Json& page) {
            if (const auto content = page.find("content"); content != page.end() && content->is_array()) {
                for (const auto& row : *content) {
                    if (auto file = getText(row, FALLUI_LANGUAGE_FILE); !file.empty()) {
                        // M8r's intros put the `libName` of their row in its place, as JOURNEY's file has it
                        const auto libName = getText(row, "libName");
                        for (auto at = file.find(LIB_NAME_TOKEN); at != std::string::npos; at = file.find(LIB_NAME_TOKEN, at + libName.size())) {
                            file.replace(at, LIB_NAME_TOKEN.size(), libName);
                        }
                        return file;
                    }
                }
            }
            return std::string();
        };

        auto file = inRows(config);
        if (const auto pages = config.find("pages"); file.empty() && pages != config.end() && pages->is_array()) {
            for (const auto& page : *pages) {
                if (file = inRows(page); !file.empty()) {
                    break;
                }
            }
        }
        return file;
    }

    using SettingTypeName = std::pair<std::string_view, model::SettingType>;

    // a switcher MCM's menu never shows: a script sets its value, and it controls a group as a switcher does
    constexpr std::string_view HIDDEN_SWITCHER = "hiddenSwitcher";

    // a dropdown whose options are the files of a folder
    constexpr std::string_view DROPDOWN_FILES = "dropdownFiles";

    // the rows that edit a value, by the type MCM's own menu knows each by
    constexpr std::array SETTING_TYPES{
        SettingTypeName{ "switcher", model::SettingType::Switcher },
        SettingTypeName{ HIDDEN_SWITCHER, model::SettingType::Switcher },
        SettingTypeName{ "slider", model::SettingType::Slider },
        SettingTypeName{ "stepper", model::SettingType::Stepper },
        SettingTypeName{ "dropdown", model::SettingType::Dropdown },
        SettingTypeName{ DROPDOWN_FILES, model::SettingType::Dropdown },
        SettingTypeName{ "textinput", model::SettingType::TextInput },
        SettingTypeName{ "textinputInt", model::SettingType::TextInput },
        SettingTypeName{ "textinputFloat", model::SettingType::TextInput },
    };

    /**
     * What a text input of a type takes, as MCM's menu has it: a whole number for a `textinputInt`, a number
     * for a `textinputFloat`, and a text for a `textinput`, and for every other row.
     */
    model::StoredType textInputType(const std::string_view type)
    {
        if (type == "textinputInt") {
            return model::StoredType::Int;
        }
        return type == "textinputFloat" ? model::StoredType::Float : model::StoredType::String;
    }

    // the groups of a page that a row controls, by their numbers: the id of the setting each is on with
    // the condition a group is on by, which is that the setting that controls it is on
    using Groups = std::map<int, model::Condition>;

    /**
     * A page as its rows have to be known before they are read: the ids of its settings, and its groups.
     */
    struct PageRows
    {
        // by the place of the row in the page's content. Empty for a row that is no setting
        std::vector<std::string> ids;
        Groups groups;
    };

    /**
     * The number of a group, as a row names it. It is 0 for what is no number: MCM's menu has a group 0
     * that is always on, which no row controls.
     */
    int getGroup(const Json& value)
    {
        return value.is_number() ? static_cast<int>(value.get<double>()) : 0;
    }

    /**
     * The ids of the settings of a page, and the groups its rows control with `groupControl`.
     * A setting has the `id` of its row. One with none, or with an id another setting of the mod has, gets
     * one from its place: the page's name for an id and the place of its row, from 1, as `main/2`.
     * A group that two rows control is on with the later one, as it is when MCM's menu opens the page.
     * A hidden switcher is a setting too: it has a value and controls a group, and only is not shown. Its
     * group is off while its value is not available, as MCM's menu reads it.
     */
    PageRows readPageRows(const Json& content, const std::string& pageId, std::unordered_set<std::string>& takenIds)
    {
        PageRows rows;
        for (std::size_t place = 0; place < content.size(); ++place) {
            const auto& row = content[place];
            auto& id = rows.ids.emplace_back();
            if (std::ranges::find(SETTING_TYPES, getText(row, "type"), &SettingTypeName::first) == SETTING_TYPES.end()) {
                continue;
            }
            id = getText(row, "id");
            if (id.empty() || !takenIds.insert(id).second) {
                id = std::format("{}/{}", pageId, place + 1);
                takenIds.insert(id);
            }
            if (const auto control = row.find("groupControl"); control != row.end() && getGroup(*control) != 0) {
                rows.groups.insert_or_assign(getGroup(*control),
                    model::Condition{ .kind = model::Condition::Kind::On, .setting = id, .conditions = {}, .offWhenNotAvailable = getText(row, "type") == HIDDEN_SWITCHER });
            }
        }
        return rows;
    }

    model::Condition toCondition(const Json& value, const Groups& groups);

    /**
     * The conditions of a list, joined: each is the number of a group, or a condition of its own.
     */
    model::Condition joinConditions(const model::Condition::Kind kind, const Json& list, const Groups& groups)
    {
        model::Condition joined{ .kind = kind, .setting = {}, .conditions = {} };
        if (list.is_array()) {
            for (const auto& value : list) {
                joined.conditions.push_back(toCondition(value, groups));
            }
        }
        return joined;
    }

    /**
     * A `groupCondition`, or a part of one, as a condition on the settings that control the page's groups.
     * A number is a group, which is on while the setting that controls it is on. A group that no row of the
     * page controls is never on. A list is on with one of its groups, as an `OR` is, and an `AND` with all of
     * them. An `ONLY` is on with all of its groups while no other group of the page is on.
     * Anything else always holds, as MCM's menu shows its row.
     */
    model::Condition toCondition(const Json& value, const Groups& groups)
    {
        using Kind = model::Condition::Kind;
        if (value.is_array()) {
            return joinConditions(Kind::Any, value, groups);
        }
        if (value.is_object()) {
            if (const auto all = value.find("AND"); all != value.end()) {
                return joinConditions(Kind::All, *all, groups);
            }
            if (const auto only = value.find("ONLY"); only != value.end()) {
                auto joined = joinConditions(Kind::All, *only, groups);
                model::Condition others{ .kind = Kind::Any, .setting = {}, .conditions = {} };
                for (const auto& [group, on] : groups) {
                    const auto isGroup = [group](const Json& listed) {
                        return getGroup(listed) == group;
                    };
                    if (!only->is_array() || std::ranges::none_of(*only, isGroup)) {
                        others.conditions.push_back(on);
                    }
                }
                joined.conditions.push_back({ .kind = Kind::Not, .setting = {}, .conditions = { std::move(others) } });
                return joined;
            }
            if (const auto any = value.find("OR"); any != value.end()) {
                return joinConditions(Kind::Any, *any, groups);
            }
            return { .kind = Kind::All, .setting = {}, .conditions = {} };
        }
        const int group = getGroup(value);
        if (group == 0) {
            return { .kind = Kind::All, .setting = {}, .conditions = {} };
        }
        const auto control = groups.find(group);
        return control != groups.end() ? control->second : model::Condition{ .kind = Kind::Any, .setting = {}, .conditions = {} };
    }

    /**
     * What a row is shown by: its `groupCondition`. None for a row that is always shown.
     * A hidden switcher is never shown, whatever its `groupCondition`, as in MCM's menu.
     */
    std::optional<model::Condition> loadVisibleWhen(const Json& row, const Groups& groups)
    {
        if (getText(row, "type") == HIDDEN_SWITCHER) {
            return model::Condition{ .kind = model::Condition::Kind::Any, .setting = {}, .conditions = {} };
        }
        const auto found = row.find("groupCondition");
        if (found == row.end()) {
            return std::nullopt;
        }
        auto condition = toCondition(*found, groups);
        if (condition.kind == model::Condition::Kind::All && condition.conditions.empty()) {
            return std::nullopt;
        }
        return condition;
    }

    // the classes of M8r's code that show the picture of another file of the mod's config folder: they load it
    constexpr std::string_view M8R_INTRO = "M8r.View.McmIntros.M8rIntro";
    constexpr std::string_view M8R_INTRO_LIGHT = "M8r.View.McmIntros.M8rIntroLight";

    // the file an M8r intro loads, where its row names none
    constexpr auto M8R_INTRO_FILE = "logo.swf";

    /**
     * The image of a row: the class it names of the mod's `lib.swf`. An M8r intro shows no picture of its own:
     * its code loads the file of its row's `m8rIntro`, or `logo.swf`, from the mod's config folder, and the
     * image is the class of that file itself. `M8rIntro` always loads `logo.swf`.
     */
    model::Image loadImage(const Json& row, mcm::McmConfigImages& images)
    {
        const auto libName = getText(row, "libName");
        const auto className = getText(row, "className");
        if (className == M8R_INTRO) {
            return images.loadMain(libName, M8R_INTRO_FILE);
        }
        if (className == M8R_INTRO_LIGHT) {
            const auto intro = row.find("m8rIntro");
            return images.loadMain(libName, intro != row.end() && intro->is_object() ? getText(*intro, "file") : M8R_INTRO_FILE);
        }
        return images.load(libName, className);
    }

    /**
     * A row of a config, by its type. A type the menu does not support is a placeholder, and so is a row
     * with no type. A setting has the given id. A `dropdownFiles` lists its files from the game's folder.
     */
    model::Item loadRow(const std::string& type, const Json& row, std::string id, const ConfigTexts& texts, mcm::McmConfigImages& images, const std::filesystem::path& gameFolder)
    {
        if (type == "text") {
            // the tags are read after the translation: a FallUI mod has its tokens inside them
            const auto text = getShownText(row, "text", texts);
            return model::Text{ .lines = getFlag(row, "html") ? model::parseMarkup(text) : model::plainLines(text), .visibleWhen = {} };
        }
        if (type == "image") {
            return loadImage(row, images);
        }
        if (type == "spacer") {
            return model::Spacer{ .lines = getSpacerLines(row), .visibleWhen = {} };
        }

        auto text = getLabel(row, texts);
        auto textRuns = getLabelRuns(row, texts);
        if (const auto setting = std::ranges::find(SETTING_TYPES, type, &SettingTypeName::first); setting != SETTING_TYPES.end()) {
            auto storage = loadStorage(row);
            auto action = loadAction(row, &storage);
            auto valueOptions = loadValueOptions(row, texts);
            if (type == DROPDOWN_FILES) {
                valueOptions.options = loadFileOptions(row, storage, gameFolder);
            } else if (setting->second == model::SettingType::Stepper || setting->second == model::SettingType::Dropdown) {
                loadGameOptions(row, valueOptions);
            }
            return model::Setting{ .id = std::move(id),
                .sourceId = getText(row, "id"),
                .type = setting->second,
                .text = std::move(text),
                .textRuns = std::move(textRuns),
                .help = getHelp(row, texts),
                .valueOptions = std::move(valueOptions),
                .storage = std::move(storage),
                .valueType = textInputType(type),
                .action = std::move(action),
                .visibleWhen = {} };
        }
        if (type == "button") {
            return model::Button{ .text = std::move(text),
                .textRuns = std::move(textRuns),
                .help = getHelp(row, texts),
                .buttonText = {},
                .style = {},
                .icon = {},
                .action = loadAction(row),
                .visibleWhen = {},
                .enabledWhen = {},
                .warnings = {} };
        }
        if (type == "hotkey") {
            return model::Hotkey{ .id = getText(row, "id"), .text = std::move(text), .textRuns = std::move(textRuns), .help = getHelp(row, texts), .visibleWhen = {} };
        }
        return model::Placeholder{ .text = std::move(text), .textRuns = std::move(textRuns), .help = getHelp(row, texts), .type = type, .visibleWhen = {} };
    }

    /**
     * The items of the `content` array of a page. A section becomes a group, with the rows that follow it up
     * to the next section. The rows before the first section are items of the page itself.
     * A row with a `groupCondition` is shown by the groups of its own page, and a section's is of its heading
     * alone, as in MCM's menu. The page's name for an id is in the ids the loader makes for its settings.
     */
    std::vector<model::Item> loadItems(const Json& page, const std::string& pageId, const ConfigTexts& texts, mcm::McmConfigImages& images, const std::filesystem::path& gameFolder,
        std::unordered_set<std::string>& takenIds)
    {
        std::vector<model::Item> items;
        const auto content = page.find("content");
        if (content == page.end() || !content->is_array()) {
            return items;
        }
        auto rows = readPageRows(*content, pageId, takenIds);

        bool inGroup = false;
        for (std::size_t place = 0; place < content->size(); ++place) {
            const auto& row = (*content)[place];
            const auto type = getText(row, "type");
            auto visibleWhen = loadVisibleWhen(row, rows.groups);
            if (type == "section") {
                items.emplace_back(model::Group{ .text = getLabel(row, texts), .items = {}, .visibleWhen = std::move(visibleWhen) });
                inGroup = true;
                continue;
            }
            auto item = loadRow(type, row, std::move(rows.ids[place]), texts, images, gameFolder);
            std::visit(
                [&visibleWhen](auto& loaded) {
                    loaded.visibleWhen = std::move(visibleWhen);
                },
                item);
            auto& target = inGroup ? std::get<model::Group>(items.back()).items : items;
            target.push_back(std::move(item));
        }
        return items;
    }

    /**
     * Why a mod is not available while a plugin of its `pluginRequirements` is not loaded, as
     * `its plugins A.esp and B.esp are not loaded`. Empty while every one is, and with no check, as in a test
     * of a config alone. MCM's menu checks each the same way, and names the missing ones.
     */
    std::string findMissingPlugins(const Json& config, const mcm::PluginCheck& isPluginLoaded)
    {
        const auto required = config.find("pluginRequirements");
        if (!isPluginLoaded || required == config.end() || !required->is_array()) {
            return {};
        }
        std::vector<std::string> missing;
        for (const auto& plugin : *required) {
            if (plugin.is_string() && !isPluginLoaded(plugin.get_ref<const std::string&>())) {
                missing.push_back(plugin.get<std::string>());
            }
        }
        if (missing.empty()) {
            return {};
        }
        if (missing.size() == 1) {
            return std::format("its plugin {} is not loaded", missing[0]);
        }
        std::string names = missing[0];
        for (std::size_t i = 1; i < missing.size(); ++i) {
            names += (i + 1 == missing.size() ? " and " : ", ") + missing[i];
        }
        return std::format("its plugins {} are not loaded", names);
    }

    /**
     * Whether a game plugin is loaded in the running game, as MCM's menu asks it of `mcm_vr.dll`: one that is
     * in the Data folder and not enabled is not.
     */
    bool isGamePluginLoaded(const std::string_view plugin)
    {
        auto* const dataHandler = RE::TESDataHandler::GetSingleton();
        return dataHandler && dataHandler->LookupLoadedFile(plugin).first;
    }

    /**
     * A path as UTF-8 text for the log. A path's own string() throws on a name the system's code page lacks.
     */
    std::string toLogText(const std::filesystem::path& path)
    {
        const auto text = path.u8string();
        return { text.begin(), text.end() };
    }
}

namespace mods_config::mcm
{
    /**
     * Reads the text of an MCM `config.json` into the model: the mod, its front page and its pages, and the
     * text, the help and the type of every row, the lines of a text row, the height of a spacer, the range of a
     * slider, the options of a stepper and a dropdown, the global a setting is kept in, the function a button
     * calls, the id of a hotkey, and the pixels of an image.
     * A setting has an id, and a row is shown by the groups its `groupCondition` names.
     * A text that is a `$` token is looked up in the translations.
     * A FallUI mod is translated from its own language files, which it names from the interface folder.
     * An image is read from the `lib.swf` of the mod its row names, in that mod's folder of the config folder.
     * The files of a `dropdownFiles` are listed from the game's folder, which has the config folder in
     * `Data\MCM\Config`: the game's own folder is the one it runs in.
     * A mod whose required plugins are not all loaded is not available, and is its names and the reason alone.
     * The error says why a config cannot be read. Comments are skipped, as MCM's own reader skips them.
     */
    std::expected<model::Mod, std::string> loadConfig(const std::string_view text, const model::Translations& translations, const std::filesystem::path& interfaceFolder,
        const std::filesystem::path& configFolder, const PluginCheck& isPluginLoaded)
    {
        Json config;
        try {
            config = Json::parse(text, nullptr, true, true);
        } catch (const Json::parse_error& e) {
            return std::unexpected(std::format("Not valid JSON: {}", e.what()));
        }
        if (!config.is_object()) {
            return std::unexpected("Not a JSON object"s);
        }
        useSharedLists(config);

        model::Mod mod;
        mod.name = getText(config, "modName");
        if (mod.name.empty()) {
            return std::unexpected("No modName"s);
        }

        ConfigTexts texts{ .game = translations, .fallUI = std::nullopt };
        if (const auto languageFile = findFallUILanguageFile(config); !languageFile.empty()) {
            texts.fallUI.emplace(translations).loadFiles(languageFile, interfaceFolder);
        }

        mod.displayName = getShownText(config, "displayName", texts);
        if (mod.displayName.empty()) {
            mod.displayName = mod.name;
        }
        mod.notAvailable = findMissingPlugins(config, isPluginLoaded);
        if (!mod.notAvailable.empty()) {
            return mod;
        }
        McmConfigImages images(configFolder);
        const auto gameFolder = configFolder.parent_path().parent_path().parent_path();
        std::unordered_set<std::string> takenIds;
        mod.frontPage.items = loadItems(config, "main", texts, images, gameFolder, takenIds);

        if (const auto pages = config.find("pages"); pages != config.end() && pages->is_array()) {
            for (const auto& page : *pages) {
                const auto pageId = std::format("page{}", mod.pages.size() + 1);
                mod.pages.push_back({ .name = getShownText(page, "pageDisplayName", texts), .items = loadItems(page, pageId, texts, images, gameFolder, takenIds) });
            }
        }
        return mod;
    }

    /**
     * Loads the `config.json` of every folder in MCM's config folder, which is `Data\MCM\Config` in the game.
     * A config that cannot be read is a mod that is not available, named by its folder as in MCM's menu. A mod
     * that is not available has the reason in the log. No such folder is no mods.
     */
    std::vector<model::Mod> loadConfigs(const std::filesystem::path& configFolder, const model::Translations& translations, const std::filesystem::path& interfaceFolder,
        const PluginCheck& isPluginLoaded)
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

            auto mod = loadConfig(text.str(), translations, interfaceFolder, configFolder, isPluginLoaded);
            if (!mod) {
                logger::warn("MCM config '{}' cannot be read: {}", toLogText(file), mod.error());
                const auto folderName = toLogText(folder->path().filename());
                mod = model::Mod{ .name = folderName, .displayName = folderName, .frontPage = {}, .pages = {}, .notAvailable = "its config cannot be read" };
            }
            if (!mod->notAvailable.empty()) {
                logger::warn("MCM mod '{}' is not available: {}", mod->name, mod->notAvailable);
            }
            mods.push_back(std::move(*mod));
        }
        return mods;
    }

    /**
     * The MCM mods of the running game. There are none while MCM itself is not loaded: a mod with an MCM menu
     * needs MCM for its settings, so its menu would change nothing.
     */
    std::vector<model::Mod> loadInstalledMods(const model::Translations& translations)
    {
        if (!common::isDLLModLoaded(MCM_DLL)) {
            logger::info("MCM is not loaded ({}): no MCM mods are listed", MCM_DLL);
            return {};
        }
        auto mods = loadConfigs(MCM_CONFIG_FOLDER, translations, INTERFACE_FOLDER, isGamePluginLoaded);
        logger::info("Loaded {} MCM mods from '{}'", mods.size(), MCM_CONFIG_FOLDER);
        return mods;
    }
}
