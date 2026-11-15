#pragma once

#include <cstddef>
#include <vector>

#include "ConfigDefinition.h"
#include "Store.h"

namespace mods_config::model
{
    /**
     * What the rows of a folded section would mark: how many settings are invalid, and how many others wait for a Save.
     */
    struct FoldedMarks
    {
        std::size_t invalid = 0;
        std::size_t unsaved = 0;

        bool operator==(const FoldedMarks&) const = default;
    };

    std::size_t unsavedChanges(const std::vector<Item>& items, Store& store);
    std::size_t unsavedChanges(const Page& page, Store& store, bool withPages);
    std::size_t unsavedChanges(const Mod& mod, Store& store);
    FoldedMarks foldedMarks(const std::vector<Item>& items, Store& store);
}
