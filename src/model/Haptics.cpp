#include "Haptics.h"

#include <algorithm>
#include <array>
#include <format>

#include "common/CommonUtils.h"

namespace
{
    using namespace mods_config::model;
    using f4cf::vrcf::HapticPattern;

    // None and the patterns of the framework's library, in the library's order, as the design system's Haptic has them
    constexpr std::array HAPTICS{
        HapticOption{ NO_HAPTIC, "None", "No pulse.", std::nullopt },
        HapticOption{ "Tick", "Tick", "The lightest single tap. For a change of selection.", HapticPattern::Tick },
        HapticOption{ "Click", "Click", "A crisp single tap. For a press.", HapticPattern::Click },
        HapticOption{ "DoubleClick", "Double click", "Two quick taps. For turning something on, or a change of mode.", HapticPattern::DoubleClick },
        HapticOption{ "TripleClick", "Triple click", "Three quick taps. For a rare action.", HapticPattern::TripleClick },
        HapticOption{ "Success", "Success", "A light tap, then a strong one. For something that worked.", HapticPattern::Success },
        HapticOption{ "Warning", "Warning", "A strong tap, then a weak one. For something that needs attention.", HapticPattern::Warning },
        HapticOption{ "Error", "Error", "Two short buzzes, then a long strong one. For something that failed.", HapticPattern::Error },
        HapticOption{ "Notification", "Notification", "A long buzz, then two light taps. For something that happened elsewhere.", HapticPattern::Notification },
        HapticOption{ "Start", "Start", "A rise into a hold. For something that began.", HapticPattern::Start },
        HapticOption{ "Stop", "Stop", "A hold, then a fall. For something that ended.", HapticPattern::Stop },
        HapticOption{ "RampUp", "Ramp up", "A smooth rise from nothing to full. For charging or filling.", HapticPattern::RampUp },
        HapticOption{ "RampDown", "Ramp down", "A smooth fall from full to nothing. For draining or turning off.", HapticPattern::RampDown },
        HapticOption{ "Heartbeat1", "Heartbeat 1", "One heartbeat.", HapticPattern::Heartbeat1 },
        HapticOption{ "Heartbeat2", "Heartbeat 2", "Two heartbeats.", HapticPattern::Heartbeat2 },
        HapticOption{ "Heartbeat3", "Heartbeat 3", "Three heartbeats.", HapticPattern::Heartbeat3 },
        HapticOption{ "Buzz", "Buzz", "A short, even rumble.", HapticPattern::Buzz },
        HapticOption{ "MidBuzz", "Mid buzz", "An even rumble.", HapticPattern::MidBuzz },
        HapticOption{ "LongBuzz", "Long buzz", "A long, stronger rumble.", HapticPattern::LongBuzz },
    };

    /**
     * The haptic a text names, as the framework reads it: with no regard to case, spaces, underscores and hyphens, and
     * `off` and an empty text are None. None for a text that names no haptic.
     */
    const HapticOption* hapticOfText(const std::string_view text)
    {
        const auto key = f4cf::common::normalizeConfigToken(text);
        if (key.empty() || key == "off") {
            return HAPTICS.data();
        }
        const auto found = std::ranges::find_if(HAPTICS, [&key](const HapticOption& option) {
            return f4cf::common::normalizeConfigToken(option.value) == key;
        });
        return found != HAPTICS.end() ? &*found : nullptr;
    }
}

namespace mods_config::model
{
    /**
     * None and the 18 patterns of the framework's library, in the library's order.
     */
    std::span<const HapticOption> hapticOptions()
    {
        return HAPTICS;
    }

    /**
     * The haptics as the options of a choice, each its value as the framework names it and its text, which a haptic
     * setting has as its own options.
     */
    std::vector<Option> hapticChoiceOptions()
    {
        std::vector<Option> options;
        options.reserve(HAPTICS.size());
        for (const auto& option : HAPTICS) {
            options.push_back({ .value = std::string(option.value), .text = std::string(option.text) });
        }
        return options;
    }

    /**
     * The haptic a value of a haptic setting is, by its name as the framework writes it. None for a value that is no
     * haptic, as a name the library does not have.
     */
    const HapticOption* findHaptic(const Value& value)
    {
        const auto* text = std::get_if<std::string>(&value);
        if (!text) {
            return nullptr;
        }
        const auto found = std::ranges::find(HAPTICS, std::string_view(*text), &HapticOption::value);
        return found != HAPTICS.end() ? &*found : nullptr;
    }

    /**
     * The text of a haptic as the framework names it, so two texts of one haptic are the same text: `double click` is
     * `DoubleClick`, and `off` and an empty text are `none`. A text that names no haptic is as it is.
     */
    std::string canonicalHaptic(const std::string& text)
    {
        const auto* option = hapticOfText(text);
        return option ? std::string(option->value) : text;
    }

    /**
     * How long a pattern plays, in seconds: the length of its keyframes.
     */
    float hapticLength(const vrcf::HapticPattern pattern)
    {
        float length = 0.0f;
        for (const auto& segment : vrcf::VRControllersHaptic::getPattern(pattern)) {
            length += segment.duration;
        }
        return length;
    }

    /**
     * The help of a haptic as the help line shows it: how it feels, what it is for, and how long it plays, as
     * `Two quick taps. For turning something on, or a change of mode. 0.11 s.`
     */
    std::string hapticHelp(const HapticOption& option)
    {
        return option.pattern ? std::format("{} {:.2f} s.", option.help, hapticLength(*option.pattern)) : std::string(option.help);
    }
}
