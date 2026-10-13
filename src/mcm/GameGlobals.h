#pragma once

#include <map>
#include <optional>
#include <string>
#include <utility>

#include "McmStore.h"

namespace mods_config::mcm
{
    /**
     * The globals of the running game, which the MCM store reads and writes a setting of a global through.
     */
    class GameGlobals : public Globals
    {
    public:
        std::optional<float> get(const model::GlobalStorage& global) override;
        bool set(const model::GlobalStorage& global, float value) override;

    private:
        RE::TESGlobal* find(const model::GlobalStorage& global);

        // the globals that were looked up, each by its plugin and its id: none for one that is not in the game
        std::map<std::pair<std::string, std::uint32_t>, RE::TESGlobal*> _found;
    };
}
