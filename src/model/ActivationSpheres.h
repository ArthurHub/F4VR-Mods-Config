#pragma once

#include <string>
#include <vector>

#include "ConfigDefinition.h"

// The value of a dropdown of an activation sphere's key, which is a word the framework reads with no regard to case,
// spaces, underscores and hyphens, and of which it reads more than the dropdown has.

namespace mods_config::model
{
    std::string canonicalSphereWord(SphereWords words, const std::vector<Option>& options, const std::string& text);
}
