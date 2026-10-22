#include "ClientFunctions.h"

#include <format>

#include "common/CommonUtils.h"

namespace mods_config::model
{
    /**
     * Notes that a mod has a client, so a function it did not register is told from a DLL that is not there.
     */
    void ClientFunctions::addClient(const std::string_view modName)
    {
        _clients.insert(common::str_tolower(std::string(modName)));
    }

    /**
     * Notes that a mod's client failed: none of its functions runs again until the game is restarted.
     */
    void ClientFunctions::cutOff(const std::string_view modName)
    {
        _cutOff.insert(common::str_tolower(std::string(modName)));
    }

    /**
     * Registers a function of a mod. False for a name that has one already, which keeps the first.
     */
    bool ClientFunctions::add(const std::string_view modName, const std::string_view name, Function function)
    {
        return _functions.try_emplace({ common::str_tolower(std::string(modName)), common::str_tolower(std::string(name)) }, std::move(function)).second;
    }

    /**
     * Why a function of a mod does not run, which a button's help line says: its mod has no client, as its DLL is
     * not loaded, its client is cut off, or it did not register the function. Empty for one that runs.
     */
    std::string ClientFunctions::notRunnable(const std::string_view modName, const std::string_view displayName, const std::string_view name) const
    {
        const auto mod = common::str_tolower(std::string(modName));
        if (!_clients.contains(mod)) {
            return std::format("Needs {}'s DLL, which is not loaded.", displayName);
        }
        if (_cutOff.contains(mod)) {
            return "Not available: its mod's client failed, and is cut off until the game is restarted.";
        }
        if (!_functions.contains({ mod, common::str_tolower(std::string(name)) })) {
            return std::format("Not available: {}'s DLL has no function '{}'.", displayName, name);
        }
        return {};
    }

    /**
     * Runs a function of a mod with its arguments. One that does not run is not done, with no message.
     */
    FunctionResult ClientFunctions::call(const std::string_view modName, const std::string_view name, const std::vector<NamedArgument>& arguments) const
    {
        const auto mod = common::str_tolower(std::string(modName));
        const auto found = _functions.find({ mod, common::str_tolower(std::string(name)) });
        if (found == _functions.end() || _cutOff.contains(mod)) {
            return {};
        }
        return found->second(arguments);
    }
}
