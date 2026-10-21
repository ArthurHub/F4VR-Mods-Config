#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "ConfigDefinition.h"

// The tags a text of a config can have, as MCM configs use them. See "Text, spacers and images" in
// docs/design/schema.md.

namespace mods_config::model
{
    std::vector<TextLine> parseMarkup(std::string_view text);
    std::vector<TextLine> plainLines(std::string_view text);
    std::vector<TextRun> markupRuns(std::string_view text);
    std::string removeMarkup(std::string_view text);
}
