#include "McmGameTexts.h"

#include <algorithm>
#include <type_traits>
#include <variant>

#include "GameValues.h"
#include "model/TextMarkup.h"

using namespace mods_config;

namespace
{
    /**
     * The text the game has for a row, as MCM's menu reads it: the name or the description of a form, which keeps
     * the row's own where the form has none, and the text of a property, or one of its array by its place, which
     * is the row's even where it is empty. None where the game has no text for it.
     */
    std::optional<std::string> readGameText(const model::GameText& text)
    {
        using Kind = model::GameText::Kind;
        if (text.kind == Kind::FormName || text.kind == Kind::FormDescription) {
            auto shown = getFormText(text);
            return shown.empty() ? std::nullopt : std::optional(std::move(shown));
        }
        if (text.kind == Kind::Property) {
            const auto value = getProperty({ .plugin = text.plugin, .formId = text.formId, .script = text.script, .name = text.property, .type = model::StoredType::String });
            const auto* shown = value ? std::get_if<std::string>(&*value) : nullptr;
            return shown ? std::optional(*shown) : std::nullopt;
        }
        const auto texts = getOptionTexts({ .plugin = text.plugin, .formId = text.formId, .script = text.script, .property = text.property });
        return texts && text.index < texts->size() ? std::optional((*texts)[text.index]) : std::nullopt;
    }

    /**
     * The pieces of a row's text with the look its tags give them, as the loader has them: none where its tags give
     * none, as the text is then drawn plain.
     */
    std::vector<model::TextRun> labelRuns(const std::string& text)
    {
        auto runs = model::markupRuns(text);
        if (std::ranges::none_of(runs, [](const model::TextRun& run) {
                return run.size || run.color || run.bold || run.italic || run.underline;
            })) {
            return {};
        }
        return runs;
    }

    /**
     * Reads into the rows among the items the texts and the helps they take from the game, with those of the
     * groups. A row's text is set as MCM's menu sets it, as html, and a text row's takes tags with its `html`.
     */
    void readItems(std::vector<model::Item>& items)
    {
        for (auto& item : items) {
            std::visit(
                []<typename T>(T& row) {
                    if constexpr (std::is_same_v<T, model::Text>) {
                        if (const auto text = row.textFromGame ? readGameText(*row.textFromGame) : std::nullopt) {
                            row.lines = row.textFromGame->html ? model::parseMarkup(*text) : model::plainLines(*text);
                        }
                    } else if constexpr (std::is_same_v<T, model::Group>) {
                        if (const auto text = row.textFromGame ? readGameText(*row.textFromGame) : std::nullopt) {
                            row.text = model::removeMarkup(*text);
                        }
                        readItems(row.items);
                    } else if constexpr (requires { row.helpFromGame; }) {
                        if (const auto text = row.textFromGame ? readGameText(*row.textFromGame) : std::nullopt) {
                            row.text = model::removeMarkup(*text);
                            row.textRuns = labelRuns(*text);
                        }
                        if (const auto help = row.helpFromGame ? readGameText(*row.helpFromGame) : std::nullopt) {
                            row.help = model::removeMarkup(*help);
                        }
                    }
                },
                item);
        }
    }
}

namespace mods_config::mcm
{
    /**
     * Reads into an MCM mod's rows the texts and the helps they take from the game, as MCM's menu reads them as it
     * shows a page: as the mod is selected, and again as another page is shown, as a script can change a property.
     * A row the game has no text for keeps the one it shows.
     */
    void readGameTexts(model::Mod& mod)
    {
        readItems(mod.frontPage.items);
        for (auto& page : mod.pages) {
            readItems(page.items);
        }
    }
}
