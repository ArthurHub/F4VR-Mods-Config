#pragma once

#include "ConfigSpecReading.h"

// The actions of a config spec, which a button runs as it is pressed and a setting as Save writes its value, and its
// buttons.
namespace mods_config::spec
{
    model::Button loadButton(const Json& row, ConfigSpecReading& reading);
    model::Action loadSettingAction(const Json& row, const model::Storage& storage, model::StoredType valueType, ConfigSpecReading& reading);
}
