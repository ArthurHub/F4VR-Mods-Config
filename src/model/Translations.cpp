#include "Translations.h"

namespace mods_config::model
{
    /**
     * Adds the text of a token. The first text of a token is kept.
     */
    void Translations::add(std::string token, std::string text)
    {
        _texts.try_emplace(std::move(token), std::move(text));
    }

    /**
     * The text to show for a text of a config. A text that starts with `$` is a token: it becomes the text of
     * the language, and stays as it is written when there is none, as in MCM.
     */
    std::string Translations::translate(std::string text) const
    {
        if (!text.starts_with('$')) {
            return text;
        }
        const auto found = _texts.find(text);
        return found != _texts.end() ? found->second : text;
    }
}
