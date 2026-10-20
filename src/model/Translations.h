#pragma once

#include <string>
#include <unordered_map>

namespace mods_config::model
{
    /**
     * The texts of the game's language, by their tokens. A loader gives it every text the player sees, and a
     * text that is a token becomes the text of the language.
     */
    class Translations
    {
    public:
        void add(std::string token, std::string text);

        [[nodiscard]] std::size_t size() const
        {
            return _texts.size();
        }

        [[nodiscard]] std::string translate(std::string text, std::string fallback = {}) const;

        // whether the language has a text for a token
        [[nodiscard]] bool has(const std::string& token) const
        {
            return _texts.contains(token);
        }

    private:
        // a token is kept as it is written in a config, with its `$`
        std::unordered_map<std::string, std::string> _texts;
    };
}
