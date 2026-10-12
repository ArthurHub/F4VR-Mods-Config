#include "FallUITranslations.h"

#include <algorithm>
#include <array>
#include <fstream>
#include <optional>
#include <sstream>

using namespace mods_config;

namespace
{
    // what a language file's name has in the place of the language
    constexpr std::string_view LANGUAGE_CODE = "${LANGUAGE_CODE}";

    constexpr std::string_view ENGLISH = "en";

    // the most tokens that are replaced in one text, as in FallUI
    constexpr auto MAX_TOKENS_IN_TEXT = 100;

    using LanguageOfText = std::pair<std::string_view, std::string_view>;

    // the game's text of `$Attach` in each language FallUI knows, which it tells the game's language by
    constexpr std::array LANGUAGES{
        LanguageOfText{ "ANBRINGEN", "de" },
        LanguageOfText{ "ATTACH", "en" },
        LanguageOfText{ "APLICAR", "es" },
        LanguageOfText{ "CONECTAR", "esmx" },
        LanguageOfText{ "FIXER", "fr" },
        LanguageOfText{ "APPLICA", "it" },
        LanguageOfText{ "DOŁĄCZ", "pl" },
        LanguageOfText{ "ANEXAR", "ptbr" },
        LanguageOfText{ "ПРИКРЕПИТЬ", "ru" },
        LanguageOfText{ "装上", "cn" },
        LanguageOfText{ "裝上", "cn" },
    };

    bool isSpace(const char c)
    {
        return c == ' ' || (c >= '\t' && c <= '\r');
    }

    bool isDigit(const char c)
    {
        return c >= '0' && c <= '9';
    }

    bool isWordChar(const char c)
    {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || isDigit(c) || c == '_';
    }

    struct TokenInText
    {
        std::size_t start;
        std::size_t length;
    };

    /**
     * The first `${Key}` of a text, with its braces.
     */
    std::optional<TokenInText> findTokenInText(const std::string_view text)
    {
        const auto isKeyChar = [](const char c) {
            return isWordChar(c) || c == '+' || c == ' ' || c == '.' || c == ':';
        };

        for (auto start = text.find("${"); start != std::string_view::npos; start = text.find("${", start + 1)) {
            auto end = start + 2;
            while (end < text.size() && isKeyChar(text[end])) {
                ++end;
            }
            if (end > start + 2 && end < text.size() && text[end] == '}') {
                return TokenInText{ .start = start, .length = end + 1 - start };
            }
        }
        return std::nullopt;
    }

    /**
     * The words of a piece of text, which its token is made of: without the spaces, dashes and dots around
     * them, the brackets around them all, and a number after a colon at their end.
     */
    std::string_view wordsOf(std::string_view text)
    {
        const auto isEdge = [](const char c) {
            return isSpace(c) || c == '-' || c == '.';
        };

        for (auto size = text.size() + 1; size != text.size();) {
            size = text.size();
            while (!text.empty() && isEdge(text.front())) {
                text.remove_prefix(1);
            }
            while (!text.empty() && isEdge(text.back())) {
                text.remove_suffix(1);
            }
            if (!text.empty() && text.front() == '(' && text.find(')') == text.size() - 1) {
                text = text.substr(1, text.size() - 2);
            }

            auto end = text.size();
            while (end > 0 && isDigit(text[end - 1])) {
                --end;
            }
            if (end == text.size()) {
                continue;
            }
            while (end > 0 && isSpace(text[end - 1])) {
                --end;
            }
            if (end > 0 && text[end - 1] == ':') {
                text = text.substr(0, end - 1);
            }
        }
        return text;
    }

    /**
     * The token FallUI's files have a plain text by: its letters and digits, with one `_` for the spaces
     * between them. "Max. item name length" is `$Max_item_name_length`. Empty for a text with none.
     */
    std::string tokenOf(const std::string_view words)
    {
        if (words.starts_with('$')) {
            return std::string(words);
        }
        std::string token = "$";
        for (const auto c : words) {
            if (isSpace(c) || c == '_') {
                if (token.back() != '_') {
                    token += '_';
                }
            } else if (isWordChar(c)) {
                token += c;
            }
        }
        return token.size() > 1 ? token : std::string();
    }
}

namespace mods_config::mcm
{
    FallUITranslations::FallUITranslations(const model::Translations& game)
        : _game(game)
    {}

    /**
     * Reads the language files of a mod: the English one, and over it the one of the game's language.
     * The name is the `$fallUILanguageFile` of the mod's config, from the game's `Data\Interface` folder,
     * with `${LANGUAGE_CODE}` in the place of the language.
     */
    void FallUITranslations::loadFiles(const std::string_view languageFile, const std::filesystem::path& interfaceFolder)
    {
        const auto load = [&](const std::string_view language) {
            std::string name(languageFile);
            if (const auto code = name.find(LANGUAGE_CODE); code != std::string::npos) {
                name.replace(code, LANGUAGE_CODE.size(), language);
            }
            std::ifstream file(interfaceFolder / std::filesystem::path(std::u8string(name.begin(), name.end())), std::ios::binary);
            if (!file) {
                return false;
            }
            std::ostringstream text;
            text << file.rdbuf();
            addFile(text.str());
            return true;
        };

        if (!load(ENGLISH)) {
            logger::warn("FallUI's English language file '{}' is not found", languageFile);
        }
        if (const auto language = languageCode(); language != ENGLISH) {
            load(language);
        }
        logger::info("Loaded {} texts of FallUI's language file '{}'", _texts.size(), languageFile);
    }

    /**
     * Adds the texts of a language file: a line is a token, a tab and its text. A text that is added later
     * replaces the one its token has.
     */
    void FallUITranslations::addFile(std::string_view text)
    {
        while (!text.empty()) {
            const auto line = text.substr(0, text.find_first_of("\r\n"));
            text.remove_prefix(std::min(line.size() + 1, text.size()));

            const auto tab = line.find('\t');
            if (tab == std::string_view::npos || tab + 1 == line.size() || line.find('\t', tab + 1) != std::string_view::npos) {
                continue;
            }
            _texts.insert_or_assign(std::string(line.substr(0, tab)), std::string(line.substr(tab + 1)));
        }
    }

    /**
     * The text of a config in the game's language, as it is written where FallUI's files do not have it.
     * A text with a key of its own is looked up by it: FallUI names a help's token in `$help`.
     * A `${Key}` inside a text is replaced, and any other text is looked up by the token of its words.
     */
    std::string FallUITranslations::translate(const std::string& text, const std::string& key) const
    {
        if (std::ranges::all_of(text, isSpace)) {
            return text;
        }
        if (!key.empty()) {
            const auto found = find(key);
            return found ? *found : text;
        }
        auto translated = text.contains("${") ? translateTokens(text) : translateChunks(text);
        return translated.empty() ? text : translated;
    }

    /**
     * The game's language by FallUI's code for it, which is in the name of a language file.
     */
    std::string_view FallUITranslations::languageCode() const
    {
        const auto language = std::ranges::find(LANGUAGES, _game.translate("$Attach"), &LanguageOfText::first);
        return language != LANGUAGES.end() ? language->second : ENGLISH;
    }

    const std::string* FallUITranslations::find(const std::string& token) const
    {
        const auto found = _texts.find(token);
        return found != _texts.end() ? &found->second : nullptr;
    }

    /**
     * A text with every `${Key}` in it replaced by the text of `$Key`, FallUI's or else the game's.
     * What is left is then looked up as one token.
     */
    std::string FallUITranslations::translateTokens(std::string text) const
    {
        for (auto count = 0; count < MAX_TOKENS_IN_TEXT; ++count) {
            const auto inText = findTokenInText(text);
            if (!inText) {
                break;
            }
            const auto token = "$" + text.substr(inText->start + 2, inText->length - 3);
            const auto found = find(token);
            text.replace(inText->start, inText->length, found ? *found : _game.translate(token));
        }
        const auto found = find(text);
        return found ? *found : _game.translate(std::move(text));
    }

    /**
     * A text with each piece between its tags and its line ends translated by the token of its words.
     * What is around the words stays: the tags, and the spaces a row is set in by.
     */
    std::string FallUITranslations::translateChunks(const std::string_view text) const
    {
        std::string result;
        for (std::size_t start = 0; start < text.size();) {
            auto end = start + 1;
            while (end < text.size() && text[end] != '<' && text[end - 1] != '>' && text[end - 1] != '\n') {
                ++end;
            }
            const auto chunk = text.substr(start, end - start);
            start = end;

            const auto words = chunk.front() == '<' ? std::string_view() : wordsOf(chunk);
            const auto token = tokenOf(words);
            if (token.empty() || !find(token)) {
                result += chunk;
                continue;
            }
            const auto translated = translateTokens(token);
            const auto at = chunk.find(words);
            result += chunk.substr(0, at);
            result += translated;
            result += chunk.substr(at + words.size());
        }
        return result;
    }
}
