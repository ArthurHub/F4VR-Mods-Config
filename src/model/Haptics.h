#pragma once

#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "ConfigDefinition.h"
#include "vrcf/VRControllersHaptic.h"

// The value of a haptic setting, which is the name of a pattern of the framework's haptic library, or `none`, as the
// framework reads and writes it in its TOML config.

namespace mods_config::model
{
    // the value of a haptic that plays no pattern
    constexpr std::string_view NO_HAPTIC = "none";

    /**
     * A haptic the menu offers: its value as the framework names it, its text, its help, and its pattern, which None
     * has none of.
     */
    struct HapticOption
    {
        std::string_view value;
        std::string_view text;
        std::string_view help;
        std::optional<vrcf::HapticPattern> pattern;
    };

    std::span<const HapticOption> hapticOptions();
    std::vector<Option> hapticChoiceOptions();
    const HapticOption* findHaptic(const Value& value);
    std::string canonicalHaptic(const std::string& text);
    float hapticLength(vrcf::HapticPattern pattern);
    std::string hapticHelp(const HapticOption& option);
}
