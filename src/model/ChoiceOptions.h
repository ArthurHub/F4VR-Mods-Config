#pragma once

#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "Definition.h"

namespace mods_config::model
{
    /**
     * The options of the choices of the mod a store has the session of: a choice's own, or the ones it reads in
     * the game, which are read with `getOptionTexts` as the mod is selected, and again as its store asks.
     * It points into the mod, so it is cleared as the session ends.
     */
    class ChoiceOptions
    {
    public:
        void read(const Mod& mod);
        void readAgain();
        void clear();
        const std::vector<Option>& of(const Setting& setting) const;
        std::string_view notAvailable(const Setting& setting) const;

    private:
        /**
         * What was read of the list of a choice in the game.
         */
        struct Read
        {
            std::vector<Option> options;

            // why the list cannot be read, and empty for one that is read
            std::string notAvailable;
        };

        static Read readList(const GameOptions& list);

        // the choices of the mod that read their options in the game, with what was read of each
        std::unordered_map<const Setting*, Read> _read;
    };
}
