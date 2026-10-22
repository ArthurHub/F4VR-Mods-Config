#pragma once

#include <functional>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
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
     * What the mods' DLLs registered with the native API, which the menu calls: the functions a `CallNativeFunction`
     * runs and the states a `clientState` condition asks, each by its mod's name and its own, and the events of each
     * mod. It knows the mods that have a client, and those whose client is cut off, which are called no more.
     */
    class ClientCalls
    {
    public:
        // tells a client an event, with the `id` of the page of a PageShown and a PageLeft
        using Events = std::function<void(ClientEvent event, std::string_view pageId)>;

        using Function = std::function<FunctionResult(const std::vector<NamedArgument>& arguments)>;

        // a state's value, and none while it is not available
        using State = std::function<std::optional<Value>()>;

        void addClient(std::string_view modName);
        void cutOff(std::string_view modName);
        bool add(std::string_view modName, std::string_view name, Function function);
        std::string notRunnable(std::string_view modName, std::string_view displayName, std::string_view name) const;
        FunctionResult call(std::string_view modName, std::string_view name, const std::vector<NamedArgument>& arguments) const;
        bool addState(std::string_view modName, std::string_view name, State state);
        std::optional<Value> state(std::string_view modName, std::string_view name) const;
        void addEvents(std::string_view modName, Events events);
        void send(std::string_view modName, ClientEvent event, std::string_view pageId = {}) const;
        void sendToAll(ClientEvent event) const;

    private:
        // by the mod's name and the function's in lower case: both are compared without regard to case
        std::map<std::pair<std::string, std::string>, Function> _functions;
        std::map<std::pair<std::string, std::string>, State> _states;

        // by the mod's name in lower case
        std::map<std::string, Events, std::less<>> _events;

        // the mods that have a client, and those whose client is cut off, in lower case
        std::set<std::string> _clients;
        std::set<std::string> _cutOff;
    };
}
