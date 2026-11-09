#pragma once

#include <string>
#include <string_view>
#include <unordered_map>

namespace mods_config::model
{
    /**
     * The result of Parse for a text, kept by the text, as a row parses its value's TOML text in every frame it is
     * drawn. Parse is a pure function of the text. One cache per thread, so the loader's thread takes no lock, emptied
     * when it holds PARSE_CACHE_LIMIT texts, as a value that is dragged is a new text in each frame.
     */
    template <auto Parse>
    auto cachedParse(const std::string_view text) -> decltype(Parse(text))
    {
        constexpr std::size_t PARSE_CACHE_LIMIT = 256;

        struct Hash
        {
            using is_transparent = void;

            std::size_t operator()(const std::string_view key) const
            {
                return std::hash<std::string_view>{}(key);
            }
        };

        thread_local std::unordered_map<std::string, decltype(Parse(text)), Hash, std::equal_to<>> cache;
        if (const auto found = cache.find(text); found != cache.end()) {
            return found->second;
        }
        if (cache.size() >= PARSE_CACHE_LIMIT) {
            cache.clear();
        }
        return cache.emplace(std::string(text), Parse(text)).first->second;
    }
}
