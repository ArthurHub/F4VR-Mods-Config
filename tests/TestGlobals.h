#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <utility>

#include "mcm/McmStore.h"

// The globals of a test, in place of the game's.
namespace test_globals
{
    using namespace mods_config;

    /**
     * Globals that are kept in a map, each by its plugin and its id. One that is not in the map is not in
     * the game.
     */
    struct TestGlobals : mcm::Globals
    {
        std::map<std::pair<std::string, std::uint32_t>, float> values;

        std::optional<float> get(const model::GlobalStorage& global) override
        {
            const auto found = values.find({ global.plugin, global.formId });
            return found != values.end() ? std::optional(found->second) : std::nullopt;
        }

        bool set(const model::GlobalStorage& global, const float value) override
        {
            const auto found = values.find({ global.plugin, global.formId });
            if (found == values.end()) {
                return false;
            }
            found->second = value;
            return true;
        }
    };
}
