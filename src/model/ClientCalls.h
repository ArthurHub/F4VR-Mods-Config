#pragma once

#include <expected>
#include <functional>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "ConfigDefinition.h"

namespace mods_config::model
{
    /**
     * What a function of a mod's DLL did: whether it did what it does, and the message of its result, which can
     * be none.
     */
    struct FunctionResult
    {
        bool done = false;
        std::string message;
    };

    /**
     * What happens in the menu, which a mod's client is told: the menu opened or closed, which every client is told,
     * and its mod or a page of its config spec shown or left.
     */
    enum class ClientEvent : std::uint8_t
    {
        MenuOpened,
        MenuClosed,
        ModShown,
        ModLeft,
        PageShown,
        PageLeft,
    };

    /**
     * An entry of a mod's client in the quick launch section: its texts as the client gives them, and the function of
     * the client a press runs, which tells it from the mod's other entries.
     */
    struct ClientQuickLaunch
    {
        std::string modName;
        std::string text;
        std::string help;
        std::string buttonText;
        std::string function;
    };

    /**
     * The mods' DLLs that registered a client with the native API, which the menu asks by name: the functions a
     * `CallNativeFunction` runs, the states a `clientState` condition asks, and the options of a choice with
     * `listFromClientCallback`, and tells the menu's events. It knows those whose client is cut off, which are asked
     * nothing more, and the clients' quick launch entries.
     */
    class ClientCalls
    {
    public:
        /**
         * What the menu asks a mod's client, each by name. One that is empty is not asked.
         */
        struct Client
        {
            std::function<bool(std::string_view name)> hasFunction;
            std::function<FunctionResult(std::string_view name, const std::vector<NamedArgument>& arguments)> runFunction;

            // a state's value, and none while it is not available
            std::function<std::optional<Value>(std::string_view name)> state;

            // the options of a choice, or why it has none
            std::function<std::expected<std::vector<Option>, std::string>(const Setting& setting)> options;

            // an event, with the `id` of the page of a PageShown and a PageLeft
            std::function<void(ClientEvent event, std::string_view pageId)> onEvent;
        };

        bool addClient(std::string_view modName, Client client);
        void cutOff(std::string_view modName);
        std::string notRunnable(std::string_view modName, std::string_view displayName, std::string_view name) const;
        FunctionResult call(std::string_view modName, std::string_view name, const std::vector<NamedArgument>& arguments) const;
        std::optional<Value> state(std::string_view modName, std::string_view name) const;
        std::expected<std::vector<Option>, std::string> options(std::string_view modName, const Setting& setting) const;
        void send(std::string_view modName, ClientEvent event, std::string_view pageId = {}) const;
        void sendToAll(ClientEvent event) const;
        bool addQuickLaunch(ClientQuickLaunch entry);

        // the quick launch entries of the clients, in the order they were added
        const std::vector<ClientQuickLaunch>& quickLaunch() const
        {
            return _quickLaunch;
        }

    private:
        const Client* find(std::string_view modName) const;

        // by the mod's name in lower case: it is compared without regard to case
        std::map<std::string, Client, std::less<>> _clients;

        // the mods whose client is cut off, in lower case
        std::set<std::string, std::less<>> _cutOff;

        std::vector<ClientQuickLaunch> _quickLaunch;
    };
}
