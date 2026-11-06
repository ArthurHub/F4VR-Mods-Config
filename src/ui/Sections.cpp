#include "Sections.h"

#include <algorithm>
#include <cmath>
#include <format>
#include <string>
#include <variant>
#include <vector>

#include <imgui.h>

#include "Drawing.h"
#include "Theme.h"
#include "model/Conditions.h"
#include "model/TextStates.h"

namespace
{
    using namespace mods_config;

    /**
     * What a folded section hides: how many of its settings are invalid, and how many others wait for a Save.
     */
    struct HiddenValues
    {
        int invalid = 0;
        int unsaved = 0;
    };

    /**
     * Counts the settings of items, and of the sections in them, whose rows would be shown: those whose
     * condition holds. One is counted by the mark its row has: invalid, or else dirty.
     */
    void countHidden(const std::vector<model::Item>& items, model::Store& store, HiddenValues& hidden)
    {
        for (const auto& item : items) {
            if (const auto* group = std::get_if<model::Group>(&item)) {
                countHidden(group->items, store, hidden);
                continue;
            }
            const auto* setting = std::get_if<model::Setting>(&item);
            if (!setting || (setting->visibleWhen && !model::holds(*setting->visibleWhen, &store))) {
                continue;
            }
            const auto stored = store.value(*setting);
            if (stored.invalid) {
                ++hidden.invalid;
            } else if (stored.available && store.isDirty(*setting)) {
                ++hidden.unsaved;
            }
        }
    }

    /**
     * The help of the heading of a section that folds: its own, and then what it hides while it is folded,
     * as `Folded: 1 invalid value, 2 unsaved changes.`
     */
    std::string foldedHelp(const std::string& help, const HiddenValues& hidden)
    {
        std::string what;
        if (hidden.invalid > 0) {
            what = std::format("{} invalid value{}", hidden.invalid, hidden.invalid == 1 ? "" : "s");
        }
        if (hidden.unsaved > 0) {
            what += std::format("{}{} unsaved change{}", what.empty() ? "" : ", ", hidden.unsaved, hidden.unsaved == 1 ? "" : "s");
        }
        if (what.empty()) {
            return help;
        }
        return std::format("{}{}Folded: {}.", help, help.empty() ? "" : " ", what);
    }
}

namespace mods_config::ui
{
    /**
     * Whether a section that folds is folded: as the player left it on the page that is shown, or else as its
     * `collapsed` says.
     */
    bool Sections::isFolded(const model::Group& group) const
    {
        const auto found = _folded.find(&group);
        return found != _folded.end() ? found->second : group.collapsed;
    }

    /**
     * Folds a section or opens it, as a press on its heading does: an open one has its rows drawn.
     */
    void Sections::setFolded(const model::Group& group, const bool folded)
    {
        _folded[&group] = folded;
    }

    /**
     * Draws the heading of a section that folds in its row, which the page added at a place and in a width: an
     * arrow that points right while it is folded and down while it is open, its text, and a rule from it to the
     * right edge of the rows, as a heading has. Its text and its arrow are muted, under the ray too. A press on the
     * row folds or opens it.
     * Folded, it says at the right what it hides: how many of its settings are invalid, and how many wait for
     * a Save, and the help it tells while the ray is on it has that after the section's own.
     * Returns whether the section is folded, after a press in this frame.
     */
    bool Sections::drawHeading(const model::Group& group, model::Store* store, const ImVec2& pos, const float width, const SectionRow& row, std::string& help)
    {
        bool folded = isFolded(group);
        if (row.pressed) {
            folded = !folded;
            _folded[&group] = folded;
        }

        HiddenValues hidden;
        if (folded && store && row.shown) {
            countHidden(group.items, *store, hidden);
        }
        if (row.pointed) {
            help = foldedHelp(model::resolveClientText(group.help, store), hidden);
        }
        if (!row.shown) {
            return folded;
        }

        std::vector<MarkedCount> counts;
        if (hidden.invalid > 0) {
            counts.push_back({ .mark = CountMark::Invalid, .text = std::format("{} invalid", hidden.invalid), .color = theme::color::RED });
        }
        if (hidden.unsaved > 0) {
            counts.push_back({ .mark = CountMark::Pending, .text = std::format("{} unsaved", hidden.unsaved), .color = theme::color::AMBER });
        }
        drawFoldingHeading(model::resolveClientText(group.text, store), pos, width, folded, counts);
        return folded;
    }

    /**
     * Forgets how the sections of the page were folded, as another page is shown: each starts as its
     * `collapsed` says each time its page is shown.
     */
    void Sections::reset()
    {
        _folded.clear();
    }
}
