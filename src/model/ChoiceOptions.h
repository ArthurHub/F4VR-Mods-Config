#pragma once

#include <expected>
#include <functional>
#include <map>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "ClientCalls.h"
#include "ConfigDefinition.h"
#include "FileChoices.h"

namespace mods_config::model
{
    /**
     * The options of the choices of the mod a store has the session of: a choice's own, the ones it reads in
     * the game, which are read with `getOptionTexts`, the ones its mod's client gives, or a config spec's files of a
     * folder. Those are read as the mod is selected, and again as its store asks. An MCM `dropdownFiles` has its own,
     * which its loader listed.
     * It points into the mod, so it is cleared as the session ends.
     */
    class ChoiceOptions
    {
    public:
        void read(const Mod& mod, const ClientCalls* clients = nullptr, const FileFolders& folders = {});
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
        Read readFolder(const Setting& setting) const;

        // the choices of the mod that read their options in the game, from its client or from a folder, with what was
        // read of each
        std::unordered_map<const Setting*, Read> _read;

        // the mod's name, and the callbacks of the clients. None with no clients, as an MCM mod has
        std::string _modName;
        const ClientCalls* _clients = nullptr;

        // where the folders of a config spec's choices of files are from
        FileFolders _folders;
    };
}
