#pragma once

#include <cstddef>
#include <vector>

#include "ConfigDefinition.h"
#include "Store.h"

namespace mods_config::model
{
    std::size_t unsavedChanges(const std::vector<Item>& items, Store& store);
    std::size_t unsavedChanges(const Page& page, Store& store, bool withPages);
    std::size_t unsavedChanges(const Mod& mod, Store& store);
}
