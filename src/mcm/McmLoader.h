#pragma once

#include <expected>
#include <string>
#include <string_view>

#include "model/Definition.h"

namespace mods_config::mcm
{
    std::expected<model::Mod, std::string> loadConfig(std::string_view text);
}
