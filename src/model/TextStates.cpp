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
    constexpr std::string_view PREFIX = "{clientState:";

    /**
     * Inserts client states in one run, once: returned strings are literal text, even if they contain another
     * placeholder. An unavailable state says so; an incomplete or empty placeholder stays as written.
     */
    std::string insertStates(const std::string_view text, mods_config::model::Store* store)
    {
        std::string shown;
        std::size_t from = 0;
        while (true) {
            const auto start = text.find(PREFIX, from);
            const auto end = start == std::string_view::npos ? std::string_view::npos : text.find('}', start + PREFIX.size());
            if (end == std::string_view::npos) {
                shown += text.substr(from);
                return shown;
            }
            shown += text.substr(from, start - from);
            const auto name = text.substr(start + PREFIX.size(), end - start - PREFIX.size());
            if (name.empty()) {
                shown += text.substr(start, end + 1 - start);
            } else {
                const auto state = store ? store->clientState(name) : mods_config::model::StoredValue{};
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
     * Resolves {clientState:name} in parsed text, keeping the look of each run and the alignment of each line.
     * Returns none for static text, which is drawn as loaded. State strings are never parsed as markup.
     */
    std::optional<std::vector<TextLine>> resolveClientText(const std::span<const TextLine> lines, Store* store)
    {
        if (std::ranges::none_of(lines, [](const TextLine& line) {
                return std::ranges::any_of(line.runs, [](const TextRun& run) {
                    return run.text.contains(PREFIX);
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
}
