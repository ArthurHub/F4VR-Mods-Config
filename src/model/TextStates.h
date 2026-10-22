#pragma once

#include <optional>
#include <span>
#include <vector>

#include "ConfigDefinition.h"

namespace mods_config::model
{
    class Store;

    std::optional<std::vector<TextLine>> resolveClientText(std::span<const TextLine> lines, Store* store);
}
