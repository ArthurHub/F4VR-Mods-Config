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
     * Whether a key is set to true, or to a number that is not zero: FallUI writes a flag as `1`.
     */
    bool getFlag(const Json& value, const char* key)
    {
        const auto found = value.find(key);
        if (found == value.end()) {
            return false;
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
     * The help of a row, as the help line shows it: in the game's language, and then without its tags, on one
     * line. A FallUI mod has its tokens inside the tags, so they are removed after the translation.
     */
    std::string getHelp(const Json& row, const ConfigTexts& texts)
    {
        return model::removeMarkup(getShownText(row, "help", texts));
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
     * What a row runs, by the `type` of its `action`. A `CallFunction` names the form of its script in
     * `form`, as `MyMod.esp|800`, and its function in `function`. It can name the script in `scriptName`,
     * and have the parameters of the function in `params`.
     * A row with no action runs nothing, and neither does a `CallFunction` that is not written so. Any other
     * action is one the menu does not run.
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
        if (type != "CallFunction") {
            return model::UnknownAction{ .type = std::move(type) };
        }
        const auto formText = getText(*action, "form");
        auto form = parseSourceForm(formText);
        auto function = getText(*action, "function");
        if (!form || function.empty()) {
            logger::warn("The function '{}' of the form '{}' of the row '{}' is not a name, and a plugin and an id in hex", function, formText, getText(row, "text"));
            return model::NoAction{};
        }
        model::CallFunction call{ .plugin = std::move(form->first),
            .formId = form->second,
            .script = getText(*action, "scriptName"),
            .function = std::move(function),
            .arguments = {},
            .valueArguments = {} };
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
        return call;
    }

    /**
     * The language file a FallUI mod names in a row of its front page or of a page. Empty for any other mod.
     */
    std::string findFallUILanguageFile(const Json& config)
    {
        const auto inRows = [](const Json& page) {
            if (const auto content = page.find("content"); content != page.end() && content->is_array()) {
                for (const auto& row : *content) {
                    if (auto file = getText(row, FALLUI_LANGUAGE_FILE); !file.empty()) {
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

    // the rows that edit a value, by the type MCM's own menu knows each by
    constexpr std::array SETTING_TYPES{
        SettingTypeName{ "switcher", model::SettingType::Switcher },
        SettingTypeName{ HIDDEN_SWITCHER, model::SettingType::Switcher },
        SettingTypeName{ "slider", model::SettingType::Slider },
        SettingTypeName{ "stepper", model::SettingType::Stepper },
        SettingTypeName{ "dropdown", model::SettingType::Dropdown },
        SettingTypeName{ "textinput", model::SettingType::TextInput },
        SettingTypeName{ "textinputInt", model::SettingType::TextInput },
        SettingTypeName{ "textinputFloat", model::SettingType::TextInput },
    };

    // the groups of a page that a row controls, by their numbers: the id of the setting each is on with
    using Groups = std::map<int, std::string>;

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
     * A hidden switcher is a setting too: it has a value and controls a group, and only is not shown.
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
                rows.groups.insert_or_assign(getGroup(*control), id);
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
                for (const auto& [group, id] : groups) {
                    const auto isGroup = [group](const Json& listed) {
                        return getGroup(listed) == group;
                    };
                    if (!only->is_array() || std::ranges::none_of(*only, isGroup)) {
                        others.conditions.push_back({ .kind = Kind::On, .setting = id, .conditions = {} });
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
        return control != groups.end() ? model::Condition{ .kind = Kind::On, .setting = control->second, .conditions = {} }
                                       : model::Condition{ .kind = Kind::Any, .setting = {}, .conditions = {} };
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

    /**
     * A row of a config, by its type. A type the menu does not support is a placeholder, and so is a row
     * with no type. A setting has the given id.
     */
    model::Item loadRow(const std::string& type, const Json& row, std::string id, const ConfigTexts& texts, mcm::McmConfigImages& images)
    {
        auto text = getShownText(row, "text", texts);

        if (const auto setting = std::ranges::find(SETTING_TYPES, type, &SettingTypeName::first); setting != SETTING_TYPES.end()) {
            auto storage = loadStorage(row);
            auto action = loadAction(row, &storage);
            return model::Setting{ .id = std::move(id),
                .sourceId = getText(row, "id"),
                .type = setting->second,
                .text = std::move(text),
                .help = getHelp(row, texts),
                .valueOptions = loadValueOptions(row, texts),
                .storage = std::move(storage),
                .action = std::move(action),
                .visibleWhen = {} };
        }
        if (type == "button") {
            return model::Button{ .text = std::move(text), .help = getHelp(row, texts), .action = loadAction(row), .visibleWhen = {} };
        }
        if (type == "hotkey") {
            return model::Hotkey{ .id = getText(row, "id"), .text = std::move(text), .help = getHelp(row, texts), .visibleWhen = {} };
        }
        if (type == "text") {
            // the tags are read after the translation: a FallUI mod has its tokens inside them
            return model::Text{ .lines = getFlag(row, "html") ? model::parseMarkup(text) : model::plainLines(text), .visibleWhen = {} };
        }
        if (type == "image") {
            return images.load(getText(row, "libName"), getText(row, "className"));
        }
        if (type == "spacer") {
            return model::Spacer{ .lines = getSpacerLines(row), .visibleWhen = {} };
        }
        return model::Placeholder{ .text = std::move(text), .help = getHelp(row, texts), .type = type, .visibleWhen = {} };
    }

    /**
     * The items of the `content` array of a page. A section becomes a group, with the rows that follow it up
     * to the next section. The rows before the first section are items of the page itself.
     * A row with a `groupCondition` is shown by the groups of its own page, and a section's is of its heading
     * alone, as in MCM's menu. The page's name for an id is in the ids the loader makes for its settings.
     */
    std::vector<model::Item> loadItems(const Json& page, const std::string& pageId, const ConfigTexts& texts, mcm::McmConfigImages& images,
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
                items.emplace_back(model::Group{ .text = getShownText(row, "text", texts), .items = {}, .visibleWhen = std::move(visibleWhen) });
                inGroup = true;
                continue;
            }
            auto item = loadRow(type, row, std::move(rows.ids[place]), texts, images);
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
     * The error says why a config cannot be read. Comments are skipped, as MCM's own reader skips them.
     */
    std::expected<model::Mod, std::string> loadConfig(const std::string_view text, const model::Translations& translations, const std::filesystem::path& interfaceFolder,
        const std::filesystem::path& configFolder)
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
        McmConfigImages images(configFolder);
        std::unordered_set<std::string> takenIds;
        mod.frontPage.items = loadItems(config, "main", texts, images, takenIds);

        if (const auto pages = config.find("pages"); pages != config.end() && pages->is_array()) {
            for (const auto& page : *pages) {
                const auto pageId = std::format("page{}", mod.pages.size() + 1);
                mod.pages.push_back({ .name = getShownText(page, "pageDisplayName", texts), .items = loadItems(page, pageId, texts, images, takenIds) });
            }
        }
        return mod;
    }

    /**
     * Loads the `config.json` of every folder in MCM's config folder, which is `Data\MCM\Config` in the game.
     * A config that cannot be read is left out, with the reason in the log. No such folder is no mods.
     */
    std::vector<model::Mod> loadConfigs(const std::filesystem::path& configFolder, const model::Translations& translations, const std::filesystem::path& interfaceFolder)
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

            if (auto mod = loadConfig(text.str(), translations, interfaceFolder, configFolder)) {
                mods.push_back(std::move(*mod));
            } else {
                logger::warn("MCM config '{}' is not loaded: {}", toLogText(file), mod.error());
            }
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
        auto mods = loadConfigs(MCM_CONFIG_FOLDER, translations, INTERFACE_FOLDER);
        logger::info("Loaded {} MCM mods from '{}'", mods.size(), MCM_CONFIG_FOLDER);
        return mods;
    }
}
