#pragma once

#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "ConfigDefinition.h"

namespace mods_config::model
{
    class Store;

    std::optional<std::vector<TextLine>> resolveClientText(std::span<const TextLine> lines, Store* store);
    std::string resolveClientText(std::string_view text, Store* store);
    std::vector<TextRun> resolveClientRuns(std::span<const TextRun> runs, Store* store);
}
