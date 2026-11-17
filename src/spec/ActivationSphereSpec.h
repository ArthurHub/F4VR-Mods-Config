#pragma once

#include "ConfigSpecReading.h"

// A config spec's `cf.activationSphere` row, which the loader reads as the section of made rows it becomes.
namespace mods_config::spec
{
    Json sphereSection(const Json& row, const Inherited& above, ConfigSpecReading& reading);
}
