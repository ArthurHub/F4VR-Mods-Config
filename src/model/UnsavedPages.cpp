#include "UnsavedPages.h"

#include <variant>

#include "Conditions.h"
#include "OptionSets.h"

namespace
{
    using namespace mods_config::model;

    /**
     * The setting of an item that a count asks of. None for a section, a row of no setting, and a choice that sets
     * other settings, which saves nothing of its own: they count on their rows.
     */
    const Setting* countedSetting(const Item& item)
    {
        const auto* setting = std::get_if<Setting>(&item);
        return setting && !setsOthers(*setting) ? setting : nullptr;
    }

    /**
     * Whether a setting's row is shown, as its condition holds. A count asks it after what is cheaper to ask of the
     * store, as a condition can read the game.
     */
    bool isShown(const Setting& setting, Store& store)
    {
        return !setting.visibleWhen || holds(*setting.visibleWhen, &store);
    }
}

namespace mods_config::model
{
    /**
     * How many settings of items, and of the sections in them, wait for a Save where their rows show it: dirty,
     * with their condition holding and their value available. A change that is invalid counts, as it is unsaved too.
     * The condition and the value of a setting that is not dirty are not read, as the tree counts every frame.
     */
    std::size_t unsavedChanges(const std::vector<Item>& items, Store& store)
    {
        std::size_t count = 0;
        for (const auto& item : items) {
            if (const auto* group = std::get_if<Group>(&item)) {
                count += unsavedChanges(group->items, store);
                continue;
            }
            const auto* setting = countedSetting(item);
            if (setting && store.isDirty(*setting) && isShown(*setting, store) && store.value(*setting).available) {
                ++count;
            }
        }
        return count;
    }

    /**
     * What the rows of the settings of a folded section's items, and of the sections in them, would mark, where their
     * condition holds: each is counted by its row's mark, invalid, or else unsaved while dirty and available.
     */
    FoldedMarks foldedMarks(const std::vector<Item>& items, Store& store)
    {
        FoldedMarks marks;
        for (const auto& item : items) {
            if (const auto* group = std::get_if<Group>(&item)) {
                const auto inner = foldedMarks(group->items, store);
                marks.invalid += inner.invalid;
                marks.unsaved += inner.unsaved;
                continue;
            }
            const auto* setting = countedSetting(item);
            if (!setting || !isShown(*setting, store)) {
                continue;
            }
            const auto stored = store.value(*setting);
            if (stored.invalid) {
                ++marks.invalid;
            } else if (stored.available && store.isDirty(*setting)) {
                ++marks.unsaved;
            }
        }
        return marks;
    }

    /**
     * How many changes of a page's rows wait for a Save, as its row in the tree tells them. With `withPages`, also
     * those of the pages under it that are in the tree, as a closed page's row tells what it hides: a page whose
     * `visibleWhen` does not hold counts nothing, and a disabled one counts its rows. The page itself is counted
     * whether or not it is in the tree: its caller asks for one that is.
     */
    std::size_t unsavedChanges(const Page& page, Store& store, const bool withPages)
    {
        std::size_t count = unsavedChanges(page.items, store);
        if (!withPages) {
            return count;
        }
        for (const auto& under : page.pages) {
            if (!under.visibleWhen || holds(*under.visibleWhen, &store)) {
                count += unsavedChanges(under, store, true);
            }
        }
        return count;
    }

    /**
     * How many changes of a mod's pages wait for a Save, as the row of its closed category tells them: its front
     * page's and those of every page of it that is in the tree.
     */
    std::size_t unsavedChanges(const Mod& mod, Store& store)
    {
        std::size_t count = unsavedChanges(mod.frontPage, store, false);
        for (const auto& page : mod.pages) {
            if (!page.visibleWhen || holds(*page.visibleWhen, &store)) {
                count += unsavedChanges(page, store, true);
            }
        }
        return count;
    }
}
