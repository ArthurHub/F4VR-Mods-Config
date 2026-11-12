#include "UnsavedPages.h"

#include <variant>

#include "Conditions.h"

namespace mods_config::model
{
    /**
     * How many settings of items, and of the sections in them, wait for a Save where their rows show it: dirty,
     * with their condition holding and their value available. A change that is invalid counts, as it is unsaved too.
     */
    std::size_t unsavedChanges(const std::vector<Item>& items, Store& store)
    {
        std::size_t count = 0;
        for (const auto& item : items) {
            if (const auto* group = std::get_if<Group>(&item)) {
                count += unsavedChanges(group->items, store);
                continue;
            }
            const auto* setting = std::get_if<Setting>(&item);
            if (!setting || !store.isDirty(*setting) || (setting->visibleWhen && !holds(*setting->visibleWhen, &store))) {
                continue;
            }
            if (store.value(*setting).available) {
                ++count;
            }
        }
        return count;
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
