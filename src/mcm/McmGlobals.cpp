#include "McmStore.h"

#include <map>
#include <utility>

using namespace mods_config;

namespace
{
    /**
     * The form of a global, and none for one that is not in the game: its plugin is not loaded, or the
     * plugin has no global of that id. A page asks for its values in every frame, and the forms of a plugin
     * are the same while the game runs, so each is looked up once. One that is not found is in the log once.
     */
    RE::TESGlobal* findGlobal(const model::GlobalStorage& global)
    {
        // the globals that were looked up, each by its plugin and its id: none for one that is not in the game
        static std::map<std::pair<std::string, std::uint32_t>, RE::TESGlobal*> looked;

        const auto [found, isNew] = looked.try_emplace({ global.plugin, global.formId }, nullptr);
        if (isNew) {
            const auto dataHandler = RE::TESDataHandler::GetSingleton();
            found->second = dataHandler ? dataHandler->LookupForm<RE::TESGlobal>(global.formId, global.plugin) : nullptr;
            if (!found->second) {
                logger::warn("The global {:X} of '{}' is not in the game: its setting is not available", global.formId, global.plugin);
            }
        }
        return found->second;
    }
}

namespace mods_config::mcm
{
    /**
     * The value a global has in the game now, and none for one that is not in the game.
     */
    std::optional<float> getGlobal(const model::GlobalStorage& global)
    {
        const auto* form = findGlobal(global);
        return form ? std::optional(form->value) : std::nullopt;
    }

    /**
     * Sets the value of a global in the game's memory, as MCM's own menu does. The game keeps it in the save.
     * Returns false for one that is not in the game.
     */
    bool setGlobal(const model::GlobalStorage& global, const float value)
    {
        auto* form = findGlobal(global);
        if (!form) {
            return false;
        }
        form->value = value;
        return true;
    }
}
