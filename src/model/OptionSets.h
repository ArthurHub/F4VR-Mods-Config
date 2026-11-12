#pragma once

#include "ConfigDefinition.h"

// A choice whose options set other settings: what a value its option sets is to the value a setting has, as it is
// matched and as it is set. A transform's and a color's set only the parts of the table they name.

namespace mods_config::model
{
    bool setsOthers(const Setting& setting);
    bool hasSetValue(const Setting& target, const Value& current, const Value& set);
    Value withSetValue(const Setting& target, const Value& current, const Value& set);
    Value blankChoiceValue(const Setting& choice);
}
