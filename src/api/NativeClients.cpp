#include "NativeClients.h"

#include <algorithm>
#include <format>
#include <optional>

#include "common/CommonUtils.h"

using namespace mods_config;

namespace
{
    constexpr auto CUT_OFF_REASON = "Not available: its mod's client failed, and is cut off until the game is restarted.";

    using Thunk = void (*)(void* arg);

    /**
     * Runs a thunk inside a structured exception handler, which catches a fault, as an access violation, and a C++
     * exception, which is one of its kind, from the client's DLL as from this one. False when it did not return.
     * The function has no object to destroy, as a function with `__try` cannot have.
     */
    bool runGuarded(const Thunk thunk, void* arg)
    {
        __try {
            thunk(arg);
            return true;
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            return false;
        }
    }

    /**
     * Calls a function inside the guard of a client's call. False when it threw or faulted.
     */
    template <typename F>
    bool callGuarded(F& function)
    {
        return runGuarded(
            [](void* arg) {
                (*static_cast<F*>(arg))();
            },
            &function);
    }

    /**
     * The call of a choice's options that the host passes a client, with where its options are collected. The
     * client's call is its first member, so its `addOption` finds the rest from it.
     */
    struct HostOptionsCall
    {
        api::OptionsCall call;
        model::StoredType valueType;
        const model::Translations* translations;
        std::vector<model::Option>* options;
        int* leftOut;
    };

    static_assert(std::is_standard_layout_v<HostOptionsCall>);

    api::ValueType toApiType(const model::StoredType type)
    {
        switch (type) {
        case model::StoredType::Bool:
            return api::ValueType::Bool;
        case model::StoredType::Int:
            return api::ValueType::Int;
        case model::StoredType::Float:
            return api::ValueType::Float;
        case model::StoredType::String:
            break;
        }
        return api::ValueType::String;
    }

    /**
     * A client's value as the model's, in the setting's type. None for a value of another type, or a string with no text.
     */
    std::optional<model::Value> toValue(const api::SettingValue& value, const model::StoredType type)
    {
        if (value.type != toApiType(type)) {
            return std::nullopt;
        }
        switch (value.type) {
        case api::ValueType::Bool:
            return model::Value(value.boolValue);
        case api::ValueType::Int:
            return model::Value(static_cast<int>(value.intValue));
        case api::ValueType::Float:
            return model::Value(value.floatValue);
        case api::ValueType::String:
            break;
        }
        return value.text ? std::optional(model::Value(std::string(value.text))) : std::nullopt;
    }

    /**
     * What an option with no text of its own shows: its value.
     */
    std::string valueText(const model::Value& value)
    {
        return std::visit(
            []<typename T>(const T& v) -> std::string {
                if constexpr (std::is_same_v<T, std::string>) {
                    return v;
                } else if constexpr (std::is_same_v<T, bool>) {
                    return v ? "true" : "false";
                } else {
                    return std::format("{}", v);
                }
            },
            value);
    }

    /**
     * Adds an option a client gives, after those it gave before, with its text translated. One of another type than
     * its setting's is left out, and counted.
     */
    void MODS_CONFIG_VR_CALL addOption(api::OptionsCall* call, const api::SettingValue* value, const char* text)
    {
        const auto& host = *reinterpret_cast<HostOptionsCall*>(call);
        const auto converted = value ? toValue(*value, host.valueType) : std::nullopt;
        if (!converted) {
            ++*host.leftOut;
            return;
        }
        auto shown = text ? std::string(text) : valueText(*converted);
        if (host.translations) {
            shown = host.translations->translate(std::move(shown));
        }
        host.options->push_back({ .value = *converted, .text = std::move(shown) });
    }

    /**
     * The call of a function that the host passes a client, with where the message of its result goes. The client's
     * call is its first member, so its `setMessage` finds the rest from it.
     */
    struct HostFunctionCall
    {
        api::FunctionCall call;
        const model::Translations* translations;
        std::string* message;
    };

    static_assert(std::is_standard_layout_v<HostFunctionCall>);

    /**
     * Keeps the message a client's function gives of its result, translated, in place of one it gave before.
     */
    void MODS_CONFIG_VR_CALL setMessage(api::FunctionCall* call, const char* text)
    {
        const auto& host = *reinterpret_cast<HostFunctionCall*>(call);
        *host.message = text ? std::string(text) : std::string();
        if (host.translations) {
            *host.message = host.translations->translate(std::move(*host.message));
        }
    }

    /**
     * A value as the API passes it, with its text, which points into the text given: it is valid while that is.
     */
    api::SettingValue toApiValue(const model::Value& value, const std::string& text)
    {
        api::SettingValue apiValue{ .type = api::ValueType::String, .text = text.c_str() };
        if (const auto* flag = std::get_if<bool>(&value)) {
            apiValue.type = api::ValueType::Bool;
            apiValue.boolValue = *flag;
        } else if (const auto* number = std::get_if<int>(&value)) {
            apiValue.type = api::ValueType::Int;
            apiValue.intValue = *number;
        } else if (const auto* real = std::get_if<float>(&value)) {
            apiValue.type = api::ValueType::Float;
            apiValue.floatValue = *real;
        }
        return apiValue;
    }
}

namespace mods_config::api
{
    /**
     * Registers a mod's client under its name, which is unique without regard to case. Null for an info with no name
     * or no version, and for a name that has a client already, which keeps the first.
     */
    Client* NativeClients::registerClient(const ClientInfo* info)
    {
        if (!info || info->version == 0 || !info->name || !*info->name) {
            logger::warn("Native API: a client with no name ({}) or no version ({}) is not registered",
                info && info->name ? info->name : "",
                info ? std::to_string(info->version) : "");
            return nullptr;
        }
        const std::string name = info->name;
        const auto lowerName = common::str_tolower(name);
        if (std::ranges::any_of(_clients, [&lowerName](const auto& client) {
                return common::str_tolower(client->name) == lowerName;
            })) {
            logger::warn("Native API: '{}' has a client already, so another is not registered", name);
            return nullptr;
        }
        auto& client = _clients.emplace_back(std::make_unique<Client>(Client{ .name = name, .version = info->version, .ctx = info->ctx }));
        _functions.addClient(name);
        logger::info("Native API: client '{}' registered, built with the API of version {}", name, info->version);
        return client.get();
    }

    /**
     * Registers a client's callback of the options of a choice of its config spec, by the setting's id. False for a
     * handle that is no client's, a client that is cut off, and a setting that has one already, which keeps the first.
     */
    bool NativeClients::registerOptions(Client* client, const char* settingId, const OptionsFn options, void* ctx)
    {
        if (!isClient(client)) {
            logger::warn("Native API: the options of '{}' are registered with a handle that is no client's", settingId ? settingId : "");
            return false;
        }
        if (!settingId || !*settingId || !options) {
            logger::warn("Native API: client '{}' registers options with no setting or no callback", client->name);
            return false;
        }
        if (client->cutOff) {
            return false;
        }
        const bool added = _options.add(client->name, settingId, [this, client, options, ctx](const model::Setting& setting) {
            return callOptions(*client, options, ctx, setting);
        });
        if (!added) {
            logger::warn("Native API: client '{}' registers the options of '{}' again, which keeps the first", client->name, settingId);
        }
        return added;
    }

    /**
     * Calls a client's callback of the options of a choice, inside the guard. A client that throws or faults is cut
     * off, so its choices are not available, with the reason. An option of another type is left out, in the log.
     */
    std::expected<std::vector<model::Option>, std::string> NativeClients::callOptions(Client& client, const OptionsFn options, void* ctx, const model::Setting& setting) const
    {
        if (client.cutOff) {
            return std::unexpected(CUT_OFF_REASON);
        }
        std::vector<model::Option> collected;
        int leftOut = 0;
        const SettingView view{ .id = setting.id.c_str(), .valueType = toApiType(setting.valueType) };
        HostOptionsCall host{ .call = { .setting = &view, .addOption = &addOption },
            .valueType = setting.valueType,
            .translations = _translations,
            .options = &collected,
            .leftOut = &leftOut };
        auto call = [options, ctx, &host] {
            options(ctx, &host.call);
        };
        if (!callGuarded(call)) {
            cutOff(client, std::format("the options of '{}'", setting.id));
            return std::unexpected(CUT_OFF_REASON);
        }
        if (leftOut > 0) {
            logger::warn("Native API: client '{}' gave {} options of '{}' that are not of its type, which are left out", client.name, leftOut, setting.id);
        }
        return collected;
    }

    /**
     * Registers a client's function, which a `CallNativeFunction` of its config spec runs by its name. False for a
     * handle that is no client's, a client that is cut off, and a name that has one already, which keeps the first.
     */
    bool NativeClients::registerFunction(Client* client, const char* name, const NativeFunction function, void* ctx)
    {
        if (!isClient(client)) {
            logger::warn("Native API: the function '{}' is registered with a handle that is no client's", name ? name : "");
            return false;
        }
        if (!name || !*name || !function) {
            logger::warn("Native API: client '{}' registers a function with no name or no callback", client->name);
            return false;
        }
        if (client->cutOff) {
            return false;
        }
        const bool added = _functions.add(client->name, name, [this, client, function, ctx, name = std::string(name)](const std::vector<model::NamedArgument>& arguments) {
            return callFunction(*client, function, ctx, name, arguments);
        });
        if (!added) {
            logger::warn("Native API: client '{}' registers the function '{}' again, which keeps the first", client->name, name);
        }
        return added;
    }

    /**
     * Calls a client's function with its arguments, inside the guard, and gives what it did and its message. A client
     * that throws or faults is cut off, and the result says so.
     */
    model::FunctionResult NativeClients::callFunction(Client& client, const NativeFunction function, void* ctx, const std::string& name,
        const std::vector<model::NamedArgument>& arguments) const
    {
        if (client.cutOff) {
            return { .done = false, .message = CUT_OFF_REASON };
        }
        std::vector<std::string> texts;
        texts.reserve(arguments.size());
        std::vector<NamedValue> params;
        params.reserve(arguments.size());
        for (const auto& argument : arguments) {
            const auto& text = texts.emplace_back(valueText(argument.value));
            params.push_back({ .name = argument.name.c_str(), .value = toApiValue(argument.value, text) });
        }
        std::string message;
        bool done = false;
        HostFunctionCall host{ .call = { .params = params.data(), .paramCount = static_cast<std::uint32_t>(params.size()), .setMessage = &setMessage },
            .translations = _translations,
            .message = &message };
        auto call = [function, ctx, &host, &done] {
            done = function(ctx, &host.call);
        };
        if (!callGuarded(call)) {
            cutOff(client, std::format("the function '{}'", name));
            return { .done = false, .message = CUT_OFF_REASON };
        }
        logger::info("Native API: client '{}' ran the function '{}': {}{}", client.name, name, done ? "done" : "failed", message.empty() ? "" : ": " + message);
        return { .done = done, .message = std::move(message) };
    }

    /**
     * Whether a handle is a registered client's.
     */
    bool NativeClients::isClient(const Client* client) const
    {
        return std::ranges::any_of(_clients, [client](const auto& registered) {
            return registered.get() == client;
        });
    }

    /**
     * Cuts a client off after a call of it failed: nothing of it is called again until the game is restarted.
     */
    void NativeClients::cutOff(Client& client, const std::string_view what) const
    {
        client.cutOff = true;
        _functions.cutOff(client.name);
        logger::error("Native API: client '{}' failed in {}, and is cut off until the game is restarted", client.name, what);
    }
}
