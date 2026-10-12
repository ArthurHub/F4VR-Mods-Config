#include "GameTranslations.h"

namespace mods_config
{
    /**
     * The game's translation table, which its menus translate a `$` token with. It holds the game's own texts,
     * and F4SE adds the translation file of every enabled game plugin, for the language the game is set to.
     * The table is filled when the game starts, so it is copied once.
     */
    model::Translations loadGameTranslations()
    {
        model::Translations translations;
        const auto manager = RE::BSScaleformManager::GetSingleton();
        const auto state = manager && manager->loader ? manager->loader->GetStateAddRef(RE::Scaleform::GFx::State::StateType::kTranslator) : nullptr;
        if (!state) {
            logger::warn("The game's translation table is not found: '$' texts are shown as they are written");
            return translations;
        }

        for (const auto& entry : static_cast<RE::BSScaleformTranslator*>(state)->translator.translationMap) {
            auto token = F4SE::stl::utf16_to_utf8(entry.first);
            auto text = F4SE::stl::utf16_to_utf8(entry.second);
            if (token && text) {
                translations.add(std::move(*token), std::move(*text));
            }
        }
        state->Release();

        logger::info("Loaded {} texts of the game's translation table", translations.size());
        return translations;
    }
}
