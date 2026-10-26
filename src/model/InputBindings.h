#pragma once

#include <expected>
#include <string>
#include <string_view>

#include "vrcf/VRControllersManager.h"

// The value of a binding setting, which is the TOML text of a CommonFramework binding's inline table, as the
// framework reads and writes it.

namespace mods_config::model
{
    std::expected<vrcf::InputBinding, std::string> parseBinding(std::string_view text);
    std::string bindingText(const vrcf::InputBinding& binding);
    std::string canonicalBinding(const std::string& text);
}
