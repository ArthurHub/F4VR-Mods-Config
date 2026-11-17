#include "ActionSpec.h"

#include <algorithm>
#include <array>
#include <format>
#include <utility>
#include <variant>

#include "StorageSpec.h"

using namespace std::string_literals;

namespace mods_config::spec
{
    namespace
    {
        // the fields of a button row, as the schema has them (docs/design/schema.md)
        constexpr std::array<std::string_view, 13>
            BUTTON_FIELDS{ "type", "id", "text", "help", "html", "buttonText", "style", "icon", "action", "confirmation", "warnings", "visibleWhen", "enabledWhen" };
        // an action, of a button or a setting, and a button's confirmation:
        constexpr std::array<std::string_view, 8> ACTION_FIELDS{ "type", "function", "params", "form", "scriptName", "script", "command", "url" };
        constexpr std::array<std::string_view, 4> CONFIRMATION_FIELDS{ "title", "message", "confirm", "dangerous" };

        // the names a config spec gives a button's `style` and its `icon`
        constexpr std::array<std::pair<std::string_view, model::ButtonStyle>, 2> BUTTON_STYLES{ { { "important", model::ButtonStyle::Important },
            { "dangerous", model::ButtonStyle::Dangerous } } };
        constexpr std::array<std::pair<std::string_view, model::ButtonIcon>, 18> BUTTON_ICONS{ { { "play", model::ButtonIcon::Play },
            { "stop", model::ButtonIcon::Stop },
            { "bolt", model::ButtonIcon::Bolt },
            { "target", model::ButtonIcon::Target },
            { "check", model::ButtonIcon::Check },
            { "window", model::ButtonIcon::Window },
            { "external", model::ButtonIcon::External },
            { "link", model::ButtonIcon::Link },
            { "gear", model::ButtonIcon::Gear },
            { "import", model::ButtonIcon::Import },
            { "export", model::ButtonIcon::Export },
            { "copy", model::ButtonIcon::Copy },
            { "eye", model::ButtonIcon::Eye },
            { "hand", model::ButtonIcon::Hand },
            { "sound", model::ButtonIcon::Sound },
            { "pin", model::ButtonIcon::Pin },
            { "refresh", model::ButtonIcon::Refresh },
            { "trash", model::ButtonIcon::Trash } } };

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
         * A `CallNativeFunction`: the function of the mod's DLL it names, and its `params`, an object of named values,
         * each given to the function in its own JSON type, as a `CallFunction`'s. Or what is not written right.
         */
        std::expected<model::NativeFunction, std::string> loadNativeFunction(const Json& action)
        {
            model::NativeFunction native{ .function = getText(action, "function"), .arguments = {}, .valueArguments = {} };
            if (native.function.empty()) {
                return std::unexpected("its CallNativeFunction names no function"s);
            }
            const auto params = action.find("params");
            if (params == action.end()) {
                return native;
            }
            if (!params->is_object()) {
                return std::unexpected("the params of its CallNativeFunction are not an object of named values"s);
            }
            for (const auto& [name, param] : params->items()) {
                auto argument = loadArgument(param);
                if (!argument) {
                    return std::unexpected(std::format("the parameter '{}' of its CallNativeFunction is not a whole number of Papyrus, a number, a bool or a text", name));
                }
                native.arguments.push_back({ .name = name, .value = std::move(*argument) });
            }
            return native;
        }

        /**
         * An action of a button or a setting that is not written right, which the menu does not run: the help line
         * of a button has the mistake as its reason, and it is reported.
         */
        model::UnknownAction mistakenAction(std::string type, const std::string& mistake, ConfigSpecReading& reading)
        {
            reading.report(model::Severity::Error, std::format("Its action is not run: {}.", mistake));
            return model::UnknownAction{ .type = std::move(type), .reason = std::format("Not available: {}.", mistake) };
        }

        /**
         * What a button runs, by the `type` of its `action`: a `CallFunction` and a `CallGlobalFunction`, with
         * their `params`, a `RunConsoleCommand`, an `OpenURL`, and a `CallNativeFunction`, a function that the
         * mod's DLL registered with the native API, with its named `params`. A button with no action, any other action,
         * and one that is not written right are actions the menu does not run, with the reason, and are reported.
         */
        model::Action loadAction(const Json& row, ConfigSpecReading& reading)
        {
            const auto action = row.find("action");
            if (action == row.end() || !action->is_object()) {
                return mistakenAction({}, "the button has no action", reading);
            }
            const Into into(reading, "action");
            checkFields(*action, ACTION_FIELDS, reading);
            auto type = getText(*action, "type");
            if (type == "CallFunction" || type == "CallGlobalFunction") {
                auto call = loadCall(*action, type);
                return call ? model::Action(std::move(*call)) : mistakenAction(std::move(type), call.error(), reading);
            }
            if (type == "RunConsoleCommand") {
                auto command = getText(*action, "command");
                return command.empty() ? model::Action(mistakenAction(std::move(type), "its RunConsoleCommand has no command", reading))
                                       : model::ConsoleCommand{ std::move(command) };
            }
            if (type == "OpenURL") {
                return model::WebLink{ .url = getText(*action, "url") };
            }
            if (type.empty()) {
                return mistakenAction({}, "the action has no type", reading);
            }
            if (type == "CallNativeFunction") {
                auto native = loadNativeFunction(*action);
                return native ? model::Action(std::move(*native)) : mistakenAction(std::move(type), native.error(), reading);
            }
            reading.report(model::Severity::Error, std::format("Its action \"{}\" is not one the menu runs: it is not run.", type));
            auto reason = std::format("Not available: the menu does not run a {}.", type);
            return model::UnknownAction{ .type = std::move(type), .reason = std::move(reason) };
        }

        /**
         * What a button asks before it runs: its `confirmation`, with its `title`, its `message`, the text of the button
         * that runs it, `confirm`, and whether it is `dangerous`. None for a button with none, and for one that is no
         * object, which is reported.
         */
        std::optional<model::Confirmation> loadConfirmation(const Json& row, ConfigSpecReading& reading)
        {
            const auto found = row.find("confirmation");
            if (found == row.end()) {
                return std::nullopt;
            }
            const Into into(reading, "confirmation");
            if (!found->is_object()) {
                reading.report(model::Severity::Error, "Its confirmation is not an object: the button runs with none.");
                return std::nullopt;
            }
            checkFields(*found, CONFIRMATION_FIELDS, reading);
            return model::Confirmation{ .title = readShownText(*found, "title", reading),
                .message = readShownText(*found, "message", reading),
                .confirm = readShownText(*found, "confirm", reading),
                .dangerous = getBool(*found, "dangerous") };
        }

        /**
         * What a button's key names, of the names it can have. None for a button with no such key, and for a name that is
         * none of them, which is reported: the button is drawn without it.
         */
        template <typename Named, std::size_t COUNT>
        Named readButtonName(const Json& row, const char* key, const std::array<std::pair<std::string_view, Named>, COUNT>& names, ConfigSpecReading& reading)
        {
            const auto found = row.find(key);
            if (found == row.end()) {
                return Named::None;
            }
            const auto name = found->is_string() ? found->get<std::string>() : std::string();
            const auto named = std::ranges::find_if(names, [&name](const auto& entry) {
                return entry.first == name;
            });
            if (named != names.end()) {
                return named->second;
            }
            const Into into(reading, key);
            reading.report(model::Severity::Warning, std::format("Its {} {} is none the menu has: the button is drawn without it.", key, found->dump()));
            return Named::None;
        }

        /**
         * Makes a button dangerous where its `style` or its confirmation says so: its confirmation is red, and a button
         * with none asks with what it says itself, so a press always asks first.
         */
        void applyDangerous(model::Button& button)
        {
            if (button.confirmation && button.confirmation->dangerous) {
                button.style = model::ButtonStyle::Dangerous;
            }
            if (button.style != model::ButtonStyle::Dangerous) {
                return;
            }
            if (!button.confirmation) {
                button.confirmation =
                    model::Confirmation{ .title = button.text.empty() ? std::string() : button.text + "?", .message = button.help, .confirm = {}, .dangerous = true };
            }
            button.confirmation->dangerous = true;
        }

        // the parameter of a setting's action that is its value
        constexpr auto VALUE_PARAMETER = "{value}";
    }

    /**
     * A button: its text, its action, the text on its button, `buttonText`, which is `Run` without one, its `style` and
     * its `icon`, an `OpenURL` having `link` without one, and what it asks before it runs. Its conditions and its
     * warnings are read once every setting of the config spec is (`loadConditions`). Its fields are checked here.
     */
    model::Button loadButton(const Json& row, ConfigSpecReading& reading)
    {
        checkFields(row, BUTTON_FIELDS, reading);
        auto button = model::Button{ .text = getLabel(row, reading),
            .textRuns = getLabelRuns(row, reading.translations),
            .help = readShownText(row, "help", reading),
            .buttonText = readShownText(row, "buttonText", reading),
            .style = readButtonName(row, "style", BUTTON_STYLES, reading),
            .icon = readButtonName(row, "icon", BUTTON_ICONS, reading),
            .action = loadAction(row, reading),
            .visibleWhen = {},
            .enabledWhen = {},
            .warnings = {},
            .confirmation = loadConfirmation(row, reading) };
        if (button.icon == model::ButtonIcon::None && std::holds_alternative<model::WebLink>(button.action)) {
            button.icon = model::ButtonIcon::Link;
        }
        applyDangerous(button);
        return button;
    }

    /**
     * What a setting runs as Save writes its value: an action as a button's, of which a parameter that is
     * exactly `"{value}"` is the value written, in the setting's `valueType`. A setting with no `action` runs
     * nothing, and so does one whose action the menu does not run, which is reported. The action of a setting kept in
     * a state of its mod's client is a warning: it runs before the client is told the Save, which applies the value.
     */
    model::Action loadSettingAction(const Json& row, const model::Storage& storage, const model::StoredType valueType, ConfigSpecReading& reading)
    {
        if (!row.contains("action")) {
            return model::NoAction{};
        }
        if (std::holds_alternative<model::ClientStateStorage>(storage)) {
            const Into into(reading, "action");
            reading.report(model::Severity::Warning,
                "It is kept in a state of its mod's client, which applies the value as a Save tells it: its action runs before that, so the client does not have the value yet.");
        }
        auto action = loadAction(row, reading);
        if (std::holds_alternative<model::WebLink>(action)) {
            return mistakenAction("OpenURL", "OpenURL is only available for buttons", reading);
        }
        if (auto* call = std::get_if<model::CallFunction>(&action)) {
            for (std::size_t place = 0; place < call->arguments.size(); ++place) {
                if (call->arguments[place] == model::Value(std::string(VALUE_PARAMETER))) {
                    call->valueArguments.push_back({ .place = place, .text = VALUE_PARAMETER, .type = valueType });
                }
            }
        } else if (auto* native = std::get_if<model::NativeFunction>(&action)) {
            for (std::size_t place = 0; place < native->arguments.size(); ++place) {
                if (native->arguments[place].value == model::Value(std::string(VALUE_PARAMETER))) {
                    native->valueArguments.push_back(place);
                }
            }
        }
        return action;
    }
}
