#include "TextStates.h"

#include <algorithm>
#include <format>
#include <string>
#include <string_view>
#include <type_traits>
#include <variant>

#include "Store.h"

namespace
{
    constexpr std::string_view CLIENT_STATE = "{clientState:";
    constexpr std::string_view GAME_INI_SETTING = "{gameIniSetting:";

    /**
     * Whether a text has a placeholder of either kind.
     */
    bool hasPlaceholder(const std::string_view text)
    {
        return text.contains(CLIENT_STATE) || text.contains(GAME_INI_SETTING);
    }

    /**
     * Inserts client states and settings of the game's INI in one run, once: returned strings are literal text, even if
     * they contain another placeholder. An unavailable value says so; an incomplete or empty placeholder stays as written.
     * A name ends at the first `}`, so a setting's name keeps its `:Section`.
     */
    std::string insertStates(const std::string_view text, mods_config::model::Store* store)
    {
        std::string shown;
        std::size_t from = 0;
        while (true) {
            const auto stateStart = text.find(CLIENT_STATE, from);
            const auto settingStart = text.find(GAME_INI_SETTING, from);
            const bool isSetting = settingStart < stateStart;
            const auto start = isSetting ? settingStart : stateStart;
            const auto prefix = isSetting ? GAME_INI_SETTING : CLIENT_STATE;
            const auto end = start == std::string_view::npos ? std::string_view::npos : text.find('}', start + prefix.size());
            if (end == std::string_view::npos) {
                shown += text.substr(from);
                return shown;
            }
            shown += text.substr(from, start - from);
            const auto name = text.substr(start + prefix.size(), end - start - prefix.size());
            if (name.empty()) {
                shown += text.substr(start, end + 1 - start);
            } else {
                const auto state = !store ? mods_config::model::StoredValue{} : isSetting ? store->gameIniSetting(name) : store->clientState(name);
                shown += state.available ? std::visit(
                                               []<typename T>(const T& value) -> std::string {
                                                   if constexpr (std::is_same_v<T, std::string>) {
                                                       return value;
                                                   } else {
                                                       return std::format("{}", value);
                                                   }
                                               },
                                               state.value)
                                         : std::format("Unknown ({})", name);
            }
            from = end + 1;
        }
    }
}

namespace mods_config::model
{
    /**
     * Resolves {clientState:name} and {gameIniSetting:name} in parsed text, keeping the look of each run and the
     * alignment of each line. Returns none for static text, which is drawn as loaded. Inserted strings are never parsed
     * as markup.
     */
    std::optional<std::vector<TextLine>> resolveClientText(const std::span<const TextLine> lines, Store* store)
    {
        if (std::ranges::none_of(lines, [](const TextLine& line) {
                return std::ranges::any_of(line.runs, [](const TextRun& run) {
                    return hasPlaceholder(run.text);
                });
            })) {
            return std::nullopt;
        }
        std::vector<TextLine> resolved;
        for (const auto& line : lines) {
            resolved.push_back({ .align = line.align, .runs = {} });
            for (const auto& run : line.runs) {
                const auto text = insertStates(run.text, store);
                std::size_t from = 0;
                while (true) {
                    const auto end = text.find('\n', from);
                    auto shown = run;
                    shown.text = text.substr(from, end == std::string::npos ? end : end - from);
                    if (shown.text.ends_with('\r')) {
                        shown.text.pop_back();
                    }
                    resolved.back().runs.push_back(std::move(shown));
                    if (end == std::string::npos) {
                        break;
                    }
                    resolved.push_back({ .align = line.align, .runs = {} });
                    from = end + 1;
                }
            }
        }
        return resolved;
    }

    /**
     * Resolves both placeholders in a text on one line, as a row's label and its help show it: a line break in an
     * inserted text is a space. A text with no placeholder is as it is written.
     */
    std::string resolveClientText(const std::string_view text, Store* store)
    {
        if (!hasPlaceholder(text)) {
            return std::string(text);
        }
        auto shown = insertStates(text, store);
        std::erase(shown, '\r');
        std::ranges::replace(shown, '\n', ' ');
        return shown;
    }

    /**
     * Resolves both placeholders in the runs of a label, each keeping its look, on one line as the label's text.
     */
    std::vector<TextRun> resolveClientRuns(const std::span<const TextRun> runs, Store* store)
    {
        std::vector<TextRun> resolved(runs.begin(), runs.end());
        for (auto& run : resolved) {
            run.text = resolveClientText(run.text, store);
        }
        return resolved;
    }
}
