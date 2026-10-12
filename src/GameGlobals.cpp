#include "GameGlobals.h"

namespace mods_config
{
    std::optional<float> GameGlobals::get(const model::GlobalStorage& global)
    {
        const auto* form = find(global);
        return form ? std::optional(form->value) : std::nullopt;
    }

    /**
     * Sets the value of a global in the game's memory, as MCM's own menu does. The game keeps it in the save.
     */
    bool GameGlobals::set(const model::GlobalStorage& global, const float value)
    {
        auto* form = find(global);
        if (!form) {
            return false;
        }
        form->value = value;
        return true;
    }

    /**
     * The form of a global, and none for one that is not in the game: its plugin is not loaded, or the
     * plugin has no global of that id. A page asks for its values in every frame, and the forms of a plugin
     * are the same while the game runs, so each is looked up once. One that is not found is in the log once.
     */
    RE::TESGlobal* GameGlobals::find(const model::GlobalStorage& global)
    {
        const auto [found, isNew] = _found.try_emplace({ global.plugin, global.formId }, nullptr);
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
