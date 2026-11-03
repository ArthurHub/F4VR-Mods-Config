#include "ActivationSpheres.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <string_view>
#include <variant>

#include "common/CommonUtils.h"
#include "f4vr/WandActivationSphere.h"

namespace
{
    using namespace f4cf;

    // the options of a dropdown by the framework's values, in the order of its enums
    constexpr std::array<std::string_view, 4> VISIBILITY_WORDS{ "never", "always", "wheninside", "whenavailable" };
    constexpr std::array<std::string_view, 3> ORIENTATION_WORDS{ "hmd", "body", "world" };
}

namespace mods_config::model
{
    /**
     * A word of a sphere's dropdown as its option has it, so the framework's other words for it are that option: `off` is
     * `never`, and `Cyan_Subtle` is `cyan-subtle`. The framework gives its fallback for a word it does not read, so a word
     * that reads as both fallbacks is one it has. A word it does not read is as it is, which is no option, and the
     * framework logs it.
     */
    std::string canonicalSphereWord(const SphereWords words, const std::vector<Option>& options, const std::string& text)
    {
        if (words == SphereWords::Visibility) {
            const auto read = f4vr::parseActivationSphereVisibility(text, f4vr::ActivationSphereVisibility::Never);
            const bool known = read == f4vr::parseActivationSphereVisibility(text, f4vr::ActivationSphereVisibility::Always);
            return known ? std::string(VISIBILITY_WORDS[static_cast<std::size_t>(read)]) : text;
        }
        if (words == SphereWords::Orientation) {
            const auto read = f4vr::parseActivationSphereOrientation(text, f4vr::ActivationSphereOrientation::Hmd);
            const bool known = read == f4vr::parseActivationSphereOrientation(text, f4vr::ActivationSphereOrientation::World);
            return known ? std::string(ORIENTATION_WORDS[static_cast<std::size_t>(read)]) : text;
        }
        const auto token = common::normalizeConfigToken(text);
        const auto found = std::ranges::find_if(options, [&token](const Option& option) {
            const auto* value = std::get_if<std::string>(&option.value);
            return value && common::normalizeConfigToken(*value) == token;
        });
        return found != options.end() ? std::get<std::string>(found->value) : text;
    }
}
