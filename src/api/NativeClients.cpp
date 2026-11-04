#include "NativeClients.h"

#include <algorithm>
#include <deque>
#include <format>
#include <utility>

#include "common/CommonUtils.h"

using namespace mods_config;

namespace
{
    static_assert(static_cast<int>(api::MenuEventType::PageLeft) == static_cast<int>(model::ClientEvent::PageLeft), "the API's events are the model's, in its order");
    static_assert(static_cast<int>(api::FileFormat::Toml) == static_cast<int>(model::FileFormat::Toml), "the API's file formats are the model's, in its order");

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
     * A client's value as the model's, in its own type. None for a string with no text.
     */
    std::optional<model::Value> toValue(const api::SettingValue& value)
    {
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
     * What an option with no text of its own shows, and a parameter as text: its value.
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

    api::FileView toFileView(const model::ClientFile& file)
    {
        return { .name = file.name.c_str(), .format = static_cast<api::FileFormat>(file.format), .path = file.path.c_str() };
    }

    /**
     * A setting's view as the API passes it, with where its value is kept: a file, or none for one of the game or of a
     * state of the client. It
     * points into the setting it is made of and into itself, so it is valid while the setting is, and is not copied.
     */
    class HostSettingView
    {
    public:
        explicit HostSettingView(const model::ClientSetting& setting)
        {
            const auto text = [](const std::string& value) {
                return value.empty() ? nullptr : value.c_str();
            };
            if (setting.file) {
                _file = toFileView(*setting.file);
            }
            _path.reserve(setting.path.size());
            for (const auto& name : setting.path) {
                _path.push_back(name.c_str());
            }
            _view = { .id = setting.id.c_str(),
                .valueType = toApiType(setting.valueType),
                .storage = { .file = setting.file ? &_file : nullptr,
                    .path = _path.data(),
                    .pathCount = static_cast<std::uint32_t>(_path.size()),
                    .form = text(setting.form),
                    .scriptName = text(setting.script),
                    .propertyName = text(setting.property),
                    .clientState = text(setting.clientState) } };
        }

        HostSettingView(const HostSettingView&) = delete;
        HostSettingView& operator=(const HostSettingView&) = delete;

        const api::SettingView* get() const
        {
            return &_view;
        }

    private:
        api::FileView _file{};
        std::vector<const char*> _path;
        api::SettingView _view{};
    };

    /**
     * The options of a choice that a client gives, which the host passes it: each is of the setting's type, with its
     * text translated, and one of another type is left out, and counted.
     */
    class HostOptions final : public api::IOptions
    {
    public:
        HostOptions(const model::StoredType valueType, const model::Translations* translations)
            : _valueType(valueType),
              _translations(translations)
        {}

        void add(const api::SettingValue* value, const char* text) override
        {
            const auto converted = value && value->type == toApiType(_valueType) ? toValue(*value) : std::nullopt;
            if (!converted) {
                ++leftOut;
                return;
            }
            auto shown = text ? std::string(text) : valueText(*converted);
            if (_translations) {
                shown = _translations->translate(std::move(shown));
            }
            options.push_back({ .value = *converted, .text = std::move(shown) });
        }

        std::vector<model::Option> options;
        int leftOut = 0;

    private:
        model::StoredType _valueType;
        const model::Translations* _translations;
    };

    /**
     * The call of a function that the host passes a client: its arguments, with their texts, and the message of its
     * result, translated.
     */
    class HostFunctionCall final : public api::IFunctionCall
    {
    public:
        HostFunctionCall(const std::vector<model::NamedArgument>& arguments, const model::Translations* translations)
            : _translations(translations)
        {
            _texts.reserve(arguments.size());
            _params.reserve(arguments.size());
            for (const auto& argument : arguments) {
                const auto& text = _texts.emplace_back(valueText(argument.value));
                _params.push_back({ .name = argument.name.c_str(), .value = toApiValue(argument.value, text) });
            }
        }

        std::uint32_t paramCount() override
        {
            return static_cast<std::uint32_t>(_params.size());
        }

        const api::NamedValue* param(const std::uint32_t index) override
        {
            return index < _params.size() ? &_params[index] : nullptr;
        }

        void setMessage(const char* text) override
        {
            message = text ? std::string(text) : std::string();
            if (_translations) {
                message = _translations->translate(std::move(message));
            }
        }

        std::string message;

    private:
        const model::Translations* _translations;
        std::vector<std::string> _texts;
        std::vector<api::NamedValue> _params;
    };
}

namespace mods_config::api
{
    /**
     * Registers a mod's client under its name, which is unique without regard to case, and adds it to the client calls
     * the config spec store reads. False for no name, no client, a client that is registered already, and a name that
     * has a client already, which keeps the first.
     */
    bool NativeClients::registerClient(const char* name, const std::uint32_t version, IClient* client)
    {
        if (!name || !*name || !client || version == 0) {
            logger::warn("Native API: a client with no name ({}), no object or no version ({}) is not registered", name ? name : "", version);
            return false;
        }
        if (find(client)) {
            logger::warn("Native API: the client of '{}' is registered already, so it is not registered as '{}'", find(client)->name, name);
            return false;
        }
        auto registered = std::make_unique<Registered>(Registered{ .name = name, .version = version, .client = client });
        auto* const kept = registered.get();
        const bool added = _calls.addClient(name,
            { .hasFunction =
                    [this, kept](const std::string_view function) {
                        return hasFunction(*kept, function);
                    },
                .runFunction =
                    [this, kept](const std::string_view function, const std::vector<model::NamedArgument>& arguments) {
                        return runFunction(*kept, function, arguments);
                    },
                .state =
                    [this, kept](const std::string_view state) {
                        return answerState(*kept, state);
                    },
                .options =
                    [this, kept](const model::Setting& setting) {
                        return giveOptions(*kept, setting);
                    },
                .onEvent =
                    [this, kept](const model::ClientEvent event, const std::string_view pageId) {
                        tellEvent(*kept, event, pageId);
                    },
                .onChange =
                    [this, kept](const model::SettingChange& change) {
                        tellChange(*kept, change);
                    },
                .onSaved =
                    [this, kept](const model::SaveBatch& batch) {
                        tellSave(*kept, batch);
                    } });
        if (!added) {
            logger::warn("Native API: '{}' has a client already, so another is not registered", name);
            return false;
        }
        _clients.push_back(std::move(registered));
        logger::info("Native API: client '{}' registered, built with the API of version {}", name, version);
        return true;
    }

    /**
     * Adds an entry of a client to the quick launch section, with the function of the client a press runs: a client
     * can have an entry for each of its functions. Its texts are kept as the client gives them: the menu translates them
     * as it shows them. False for a client that is not registered, one with no text or no function, and a function that
     * has an entry already, which keeps the first.
     */
    bool NativeClients::addQuickLaunch(const IClient* client, const char* text, const char* help, const char* buttonText, const char* functionName)
    {
        const auto* registered = find(client);
        if (!registered) {
            logger::warn("Native API: the quick launch entry '{}' is added by a client that is not registered", text ? text : "");
            return false;
        }
        if (!text || !*text || !functionName || !*functionName) {
            logger::warn("Native API: client '{}' adds a quick launch entry with no text or no function", registered->name);
            return false;
        }
        const bool added =
            _calls.addQuickLaunch({ .modName = registered->name, .text = text, .help = help ? help : "", .buttonText = buttonText ? buttonText : "", .function = functionName });
        if (!added) {
            logger::warn("Native API: client '{}' adds a quick launch entry of its function '{}' again, which keeps the first", registered->name, functionName);
        }
        return added;
    }

    /**
     * The name a client is registered under, and none for one that is not registered.
     */
    const std::string* NativeClients::nameOf(const IClient* client) const
    {
        const auto* registered = find(client);
        return registered ? &registered->name : nullptr;
    }

    /**
     * Hands the menu a pending value a client sets, of a setting of its mod, in the model's type. False, with why in the
     * log, for a client that is not registered or is cut off, a call with no id or no value, and a value the menu refuses.
     * One from inside its `onChange` or `onSaved` is refused too: its handlers after the caller would be told the change
     * it sets before the one they are being told, and keep that older one.
     */
    bool NativeClients::setPendingValue(const IClient* client, const char* settingId, const SettingValue* value) const
    {
        const auto* registered = find(client);
        if (!registered) {
            logger::warn("Native API: a client that is not registered sets a pending value, which is refused");
            return false;
        }
        const std::string id = settingId ? settingId : "";
        if (registered->cutOff) {
            logger::warn("Native API: client '{}' sets the pending value of '{}', which is refused: it is cut off", registered->name, id);
            return false;
        }
        if (registered->beingTold) {
            logger::warn("Native API: client '{}' sets the pending value of '{}', which is refused: it is called from its onChange or onSaved", registered->name, id);
            return false;
        }
        const auto typed = value ? toValue(*value) : std::nullopt;
        if (id.empty() || !typed) {
            logger::warn("Native API: client '{}' sets a pending value with no setting id or no value, which is refused", registered->name);
            return false;
        }
        const auto set = _pendingValueAction ? _pendingValueAction(registered->name, id, *typed) : std::unexpected(std::string("the menu takes none"));
        if (!set) {
            logger::warn("Native API: client '{}' sets the pending value of '{}', which is refused: {}", registered->name, id, set.error());
            return false;
        }
        return true;
    }

    /**
     * Asks a client whether it has a function, inside the guard. False for one that is cut off.
     */
    bool NativeClients::hasFunction(Registered& registered, const std::string_view name) const
    {
        const std::string function(name);
        bool has = false;
        auto ask = [&registered, &function, &has] {
            has = registered.client->hasFunction(function.c_str());
        };
        return call(registered, std::format("hasFunction '{}'", function), ask) && has;
    }

    /**
     * Runs a client's function with its arguments, inside the guard, and gives what it did and its message. A client
     * that throws or faults is cut off, and the result says so.
     */
    model::FunctionResult NativeClients::runFunction(Registered& registered, const std::string_view name, const std::vector<model::NamedArgument>& arguments) const
    {
        const std::string function(name);
        HostFunctionCall host(arguments, _translations);
        bool done = false;
        auto run = [&registered, &function, &host, &done] {
            done = registered.client->runFunction(function.c_str(), &host);
        };
        if (!call(registered, std::format("the function '{}'", function), run)) {
            return { .done = false, .message = CUT_OFF_REASON };
        }
        logger::info("Native API: client '{}' ran the function '{}': {}{}", registered.name, function, done ? "done" : "failed", host.message.empty() ? "" : ": " + host.message);
        return { .done = done, .message = std::move(host.message) };
    }

    /**
     * Asks a client a state, inside the guard. None while it is not available: the client does not answer it, or is
     * cut off, as one that throws or faults is. Its text is copied inside the guard, as its pointer can be bad.
     */
    std::optional<model::Value> NativeClients::answerState(Registered& registered, const std::string_view name) const
    {
        const std::string state(name);
        std::optional<model::Value> answer;
        auto ask = [&registered, &state, &answer] {
            SettingValue value{};
            if (registered.client->answerState(state.c_str(), &value)) {
                answer = toValue(value);
            }
        };
        return call(registered, std::format("the state '{}'", state), ask) ? answer : std::nullopt;
    }

    /**
     * Asks a client the options of a choice, inside the guard, each of the setting's type, with its text translated:
     * one of another type is left out, in the log. None, with the reason, for a choice it gives no options of, and for
     * a client that is cut off.
     */
    std::expected<std::vector<model::Option>, std::string> NativeClients::giveOptions(Registered& registered, const model::Setting& setting) const
    {
        HostOptions options(setting.valueType, _translations);
        const SettingView view{ .id = setting.id.c_str(), .valueType = toApiType(setting.valueType) };
        bool given = false;
        auto ask = [&registered, &view, &options, &given] {
            given = registered.client->giveOptions(&view, &options);
        };
        if (!call(registered, std::format("the options of '{}'", setting.id), ask)) {
            return std::unexpected(CUT_OFF_REASON);
        }
        if (!given) {
            return std::unexpected("Not available: its mod's client gives no options for it."s);
        }
        if (options.leftOut > 0) {
            logger::warn("Native API: client '{}' gave {} options of '{}' that are not of its type, which are left out", registered.name, options.leftOut, setting.id);
        }
        return std::move(options.options);
    }

    /**
     * Tells a client an event of the menu, inside the guard.
     */
    void NativeClients::tellEvent(Registered& registered, const model::ClientEvent event, const std::string_view pageId) const
    {
        const std::string page(pageId);
        const MenuEvent menuEvent{ .type = static_cast<MenuEventType>(event), .pageId = page.empty() ? nullptr : page.c_str() };
        auto tell = [&registered, &menuEvent] {
            registered.client->onEvent(&menuEvent);
        };
        call(registered, "an event of the menu", tell);
    }

    /**
     * Tells a client a change of a `live` setting, inside the guard: the setting's view with where its value is kept,
     * in a file or in the game, and the value it was told last and the new one.
     */
    void NativeClients::tellChange(Registered& registered, const model::SettingChange& change) const
    {
        const HostSettingView setting(change.setting);
        const auto previousText = valueText(change.previous);
        const auto newText = valueText(change.value);
        const SettingChange apiChange{ .setting = setting.get(),
            .previousValue = toApiValue(change.previous, previousText),
            .value = toApiValue(change.value, newText),
            .phase = change.phase == model::ChangePhase::Discard ? ChangePhase::Discard : ChangePhase::Preview };
        auto tell = [&registered, &apiChange] {
            registered.client->onChange(&apiChange);
        };
        const bool told = std::exchange(registered.beingTold, true);
        call(registered, std::format("a change of '{}'", change.setting.id), tell);
        registered.beingTold = told;
    }

    /**
     * Tells a client what a Save wrote, inside the guard: each setting's view with where its value is kept, its value
     * before the Save and the value written, the files written and the Save's revision.
     */
    void NativeClients::tellSave(Registered& registered, const model::SaveBatch& batch) const
    {
        std::deque<HostSettingView> views;
        std::vector<std::string> texts;
        texts.reserve(batch.settings.size() * 2);
        std::vector<SavedSetting> settings;
        settings.reserve(batch.settings.size());
        for (const auto& saved : batch.settings) {
            const auto& view = views.emplace_back(saved.setting);
            const auto& beforeText = texts.emplace_back(valueText(saved.before));
            const auto& writtenText = texts.emplace_back(valueText(saved.value));
            settings.push_back({ .setting = view.get(), .baselineValue = toApiValue(saved.before, beforeText), .value = toApiValue(saved.value, writtenText) });
        }
        std::vector<FileView> files;
        files.reserve(batch.files.size());
        for (const auto& file : batch.files) {
            files.push_back(toFileView(file));
        }
        const SaveBatch apiBatch{ .settings = settings.data(),
            .settingCount = static_cast<std::uint32_t>(settings.size()),
            .writtenFiles = files.data(),
            .fileCount = static_cast<std::uint32_t>(files.size()),
            .revision = batch.revision };
        auto tell = [&registered, &apiBatch] {
            registered.client->onSaved(&apiBatch);
        };
        const bool told = std::exchange(registered.beingTold, true);
        call(registered, "a Save", tell);
        registered.beingTold = told;
    }

    /**
     * Calls a client inside the guard. A client that throws or faults is cut off: nothing of it is called again until
     * the game is restarted. Returns false for one that failed, or is cut off.
     */
    template <typename F>
    bool NativeClients::call(Registered& registered, const std::string_view what, F& function) const
    {
        if (registered.cutOff) {
            return false;
        }
        if (runGuarded(
                [](void* arg) {
                    (*static_cast<F*>(arg))();
                },
                &function)) {
            return true;
        }
        registered.cutOff = true;
        _calls.cutOff(registered.name);
        logger::error("Native API: client '{}' failed in {}, and is cut off until the game is restarted", registered.name, what);
        return false;
    }

    /**
     * The registered client of an object, and none.
     */
    const NativeClients::Registered* NativeClients::find(const IClient* client) const
    {
        const auto found = std::ranges::find_if(_clients, [client](const auto& registered) {
            return registered->client == client;
        });
        return found != _clients.end() ? found->get() : nullptr;
    }
}
