#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_map>

#include "model/Translations.h"

namespace mods_config::mcm
{
    /**
     * The texts of a FallUI mod in the game's language. FallUI translates its pages in MCM's menu with code of
     * its own, from files that its config names. The menu does not run a mod's code, so this does what that
     * code does (`M8r.Service.MCMTranslator` in the mod's `lib.swf`).
     */
    class FallUITranslations
    {
    public:
        explicit FallUITranslations(const model::Translations& game);

        void loadFiles(std::string_view languageFile, const std::filesystem::path& interfaceFolder);
        void addFile(std::string_view text);

        [[nodiscard]] std::string translate(const std::string& text, const std::string& key = {}) const;

    private:
        [[nodiscard]] std::string_view languageCode() const;
        [[nodiscard]] const std::string* find(const std::string& token) const;
        [[nodiscard]] std::string translateTokens(std::string text) const;
        [[nodiscard]] std::string translateChunks(std::string_view text) const;

        // the game's texts, for a token that FallUI's files do not have. It has to outlive this
        const model::Translations& _game;

        // FallUI's texts by their tokens, each with its `$`: the English ones, and over them the game's language
        std::unordered_map<std::string, std::string> _texts;
    };
}
