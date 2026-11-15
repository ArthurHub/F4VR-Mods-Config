#pragma once

#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "ConfigDefinition.h"

namespace mods_config::model
{
    class StoreReader;

    std::optional<std::vector<TextLine>> resolveClientText(std::span<const TextLine> lines, StoreReader* store);
    std::string resolveClientText(std::string_view text, StoreReader* store);
    std::vector<TextRun> resolveClientRuns(std::span<const TextRun> runs, StoreReader* store);
}
