#pragma once

#include <functional>
#include <map>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "Definition.h"

namespace mods_config::model
{
    /**
     * The callbacks that give the options of the choices that take them from their mod's client
     * (`listFromClientCallback`), each for one setting, by its mod's name and its id. The native API registers a
     * client's: until it is there the menu registers the one of its own package.
     */
    class ClientOptions
    {
    public:
        using Callback = std::function<std::vector<Option>(const Setting& setting)>;

        void add(std::string_view modName, std::string_view settingId, Callback callback);
        const Callback* find(std::string_view modName, std::string_view settingId) const;

    private:
        // by the mod's name and the setting's id in lower case: both are compared without regard to case
        std::map<std::pair<std::string, std::string>, Callback> _callbacks;
    };

    /**
     * The options of the choices of the mod a store has the session of: a choice's own, the ones it reads in
     * the game, which are read with `getOptionTexts`, or the ones its mod's client gives. Those are read as the
     * mod is selected, and again as its store asks.
     * It points into the mod, so it is cleared as the session ends.
     */
    class ChoiceOptions
    {
    public:
        void read(const Mod& mod, const ClientOptions* clients = nullptr);
        void readAgain();
        void clear();
        const std::vector<Option>& of(const Setting& setting) const;
        std::string_view notAvailable(const Setting& setting) const;

    private:
        /**
         * What was read of the list of a choice in the game, or from its mod's client.
         */
        struct Read
        {
            std::vector<Option> options;

            // why the list cannot be read, and empty for one that is read
            std::string notAvailable;
        };

        static Read readList(const GameOptions& list);
        Read readClient(const Setting& setting) const;

        // the choices of the mod that read their options in the game or from its client, with what was read of each
        std::unordered_map<const Setting*, Read> _read;

        // the mod's name, and the callbacks of the clients. None with no clients, as an MCM mod has
        std::string _modName;
        const ClientOptions* _clients = nullptr;
    };
}
