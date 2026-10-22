#include "ClientCalls.h"

#include <algorithm>
#include <format>

#include "common/CommonUtils.h"

namespace
{
    // why a choice of a mod's client has no options: it has none of them, or no client
    constexpr auto NO_OPTIONS = "Not available: its mod's client gives no options for it.";

    // why nothing of a client that is cut off is there
    constexpr auto CUT_OFF = "Not available: its mod's client failed, and is cut off until the game is restarted.";
}

namespace mods_config::model
{
    /**
     * Adds a mod's client, by its mod's name. False for a mod that has one already, which keeps the first.
     */
    bool ClientCalls::addClient(const std::string_view modName, Client client)
    {
        return _clients.try_emplace(common::str_tolower(std::string(modName)), std::move(client)).second;
    }

    /**
     * Notes that a mod's client failed: it is asked nothing more until the game is restarted.
     */
    void ClientCalls::cutOff(const std::string_view modName)
    {
        _cutOff.insert(common::str_tolower(std::string(modName)));
    }

    /**
     * The client of a mod that is asked: none for a mod with no client, and for one whose client is cut off.
     */
    const ClientCalls::Client* ClientCalls::find(const std::string_view modName) const
    {
        const auto mod = common::str_tolower(std::string(modName));
        const auto found = _clients.find(mod);
        return found != _clients.end() && !_cutOff.contains(mod) ? &found->second : nullptr;
    }

    /**
     * Why a function of a mod does not run, which a button's help line says: its mod has no client, as its DLL is
     * not loaded, its client is cut off, or has no function of the name. Empty for one that runs.
     */
    std::string ClientCalls::notRunnable(const std::string_view modName, const std::string_view displayName, const std::string_view name) const
    {
        const auto mod = common::str_tolower(std::string(modName));
        if (!_clients.contains(mod)) {
            return std::format("Needs {}'s DLL, which is not loaded.", displayName);
        }
        const auto* client = find(modName);
        if (!client) {
            return CUT_OFF;
        }
        if (!client->hasFunction || !client->hasFunction(name)) {
            // asking it can have cut it off
            return find(modName) ? std::format("Not available: {}'s DLL has no function '{}'.", displayName, name) : std::string(CUT_OFF);
        }
        return {};
    }

    /**
     * Runs a function of a mod with its arguments. One that does not run is not done, with no message.
     */
    FunctionResult ClientCalls::call(const std::string_view modName, const std::string_view name, const std::vector<NamedArgument>& arguments) const
    {
        const auto* client = find(modName);
        return client && client->runFunction ? client->runFunction(name, arguments) : FunctionResult{};
    }

    /**
     * The value of a state of a mod now. None while it is not available: its mod has no client, its client is cut off,
     * or does not answer it.
     */
    std::optional<Value> ClientCalls::state(const std::string_view modName, const std::string_view name) const
    {
        const auto* client = find(modName);
        return client && client->state ? client->state(name) : std::nullopt;
    }

    /**
     * The options of a choice of a mod, which its client gives, or why there are none.
     */
    std::expected<std::vector<Option>, std::string> ClientCalls::options(const std::string_view modName, const Setting& setting) const
    {
        if (!_clients.contains(common::str_tolower(std::string(modName)))) {
            return std::unexpected(NO_OPTIONS);
        }
        const auto* client = find(modName);
        if (!client) {
            return std::unexpected(CUT_OFF);
        }
        return client->options ? client->options(setting) : std::unexpected(std::string(NO_OPTIONS));
    }

    /**
     * Tells a mod's client an event. Nothing for a mod with no client, or whose client is cut off.
     */
    void ClientCalls::send(const std::string_view modName, const ClientEvent event, const std::string_view pageId) const
    {
        if (const auto* client = find(modName); client && client->onEvent) {
            client->onEvent(event, pageId);
        }
    }

    /**
     * Tells every client an event of the menu, as that it opened.
     */
    void ClientCalls::sendToAll(const ClientEvent event) const
    {
        for (const auto& [mod, client] : _clients) {
            if (!_cutOff.contains(mod) && client.onEvent) {
                client.onEvent(event, {});
            }
        }
    }

    /**
     * Adds an entry of a mod to the quick launch section, by the function a press runs: a mod can have an entry for
     * each of its functions. False for a function of the mod that has one already, which keeps the first.
     */
    bool ClientCalls::addQuickLaunch(ClientQuickLaunch entry)
    {
        const auto mod = common::str_tolower(entry.modName);
        const auto function = common::str_tolower(entry.function);
        if (std::ranges::any_of(_quickLaunch, [&mod, &function](const ClientQuickLaunch& added) {
                return common::str_tolower(added.modName) == mod && common::str_tolower(added.function) == function;
            })) {
            return false;
        }
        _quickLaunch.push_back(std::move(entry));
        return true;
    }
}
