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
     * The functions of the mods' DLLs, which a `CallNativeFunction` runs, and their states, which a `clientState`
     * condition asks, each by its mod's name and its own, which the native API registers. It knows the mods that have
     * a client, and those whose client is cut off, which run nothing more and have no state.
     */
    class ClientFunctions
    {
    public:
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

    private:
        // by the mod's name and the function's in lower case: both are compared without regard to case
        std::map<std::pair<std::string, std::string>, Function> _functions;
        std::map<std::pair<std::string, std::string>, State> _states;

        // the mods that have a client, and those whose client is cut off, in lower case
        std::set<std::string> _clients;
        std::set<std::string> _cutOff;
    };
}
