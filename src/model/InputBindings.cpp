#include "InputBindings.h"

#include <algorithm>
#include <cctype>
#include <format>

#include "ParseCache.h"
#include "common/CommonUtils.h"
#include "common/TomlEditor.h"
#include "vrcf/InputBindingParser.h"

namespace mods_config::model
{
    /**
     * The binding of the TOML text of an inline table, parsed each time: `parseBinding` keeps what it gives.
     */
    static std::expected<vrcf::InputBinding, std::string> parseBindingText(const std::string_view text)
    {
        std::string reason = R"(it is not a table, as { hand = "offhand", type = "tap", button = "grip" })";
        if (const auto document = common::parseToml(std::format("binding = {}", text)); document && document->size() == 1) {
            auto binding = vrcf::parseInputBindingTable(*document->get("binding"));
            if (binding) {
                return std::move(*binding);
            }
            reason = std::move(binding.error());
        }
        if (!reason.empty()) {
            reason.front() = static_cast<char>(std::toupper(static_cast<unsigned char>(reason.front())));
        }
        return std::unexpected(std::move(reason));
    }

    /**
     * The binding of an INI file's line of a binding, parsed each time: `parseBinding` keeps what it gives.
     */
    static std::expected<vrcf::InputBinding, std::string> parseBindingLine(const std::string_view text)
    {
        auto binding = vrcf::parseInputBinding(text);
        if (!binding && !binding.error().empty()) {
            binding.error().front() = static_cast<char>(std::toupper(static_cast<unsigned char>(binding.error().front())));
        }
        return binding;
    }

    /**
     * The binding of a binding's text, read as the framework reads it, or the framework's reason it is none, as
     * `'thumbstik' is not a button, one of trigger, grip, ...`, with its first letter in upper case. The TOML text of an
     * inline table is read as the table of the framework's config, and a text that is no TOML value is none too. An INI
     * line is read as the framework's line of a binding, with the other names of its words, and an empty one is off.
     * Parsed once per text (`cachedParse`).
     */
    std::expected<vrcf::InputBinding, std::string> parseBinding(const std::string_view text, const TableForm form)
    {
        return form == TableForm::IniLine ? cachedParse<parseBindingLine>(text) : cachedParse<parseBindingText>(text);
    }

    /**
     * A binding as the text of its form. The TOML text of its inline table is as the framework writes it: its words, its
     * keys in its order, and a value left out where it is the default. An INI line has the grammar's own words alone:
     * `<hand> <type> <button> [duration]` or `<hand> axis <axis> <direction> [threshold]`, then `suppress` and
     * `+button` or `+hand:button` where it has them, and `none` for an off binding. A line has no `accelerate`: the
     * loader refuses an INI setting's default that turns it off, and the menu never changes it.
     */
    std::string bindingText(const vrcf::InputBinding& binding, const TableForm form)
    {
        if (form == TableForm::Toml) {
            return vrcf::inputBindingTomlText(binding);
        }
        if (!binding.isEnabled()) {
            return "none";
        }
        auto line = std::format("{} {}", vrcf::handName(binding.hand), vrcf::activationTypeName(binding.type));
        if (binding.type == vrcf::ActivationType::AxisDirection) {
            line += std::format(" {} {}", vrcf::axisName(binding.axis), vrcf::directionName(binding.direction));
            if (!common::fEqual(binding.threshold, vrcf::InputBinding{}.threshold)) {
                line += std::format(" {}", binding.threshold);
            }
        } else {
            line += " " + vrcf::buttonName(binding.button);
            if (binding.duration > 0.0f) {
                line += std::format(" {}", binding.duration);
            }
        }
        if (binding.suppress) {
            line += " suppress";
        }
        if (binding.modifier) {
            line += binding.modifier->hand ? std::format(" +{}:{}", vrcf::handName(*binding.modifier->hand), vrcf::buttonName(binding.modifier->button))
                                           : std::format(" +{}", vrcf::buttonName(binding.modifier->button));
        }
        return line;
    }

    /**
     * The text of a binding as the menu writes it, so two texts of one binding are the same text: a file can write a
     * word, a key's place and a number in another way. A text that is no binding is as it is.
     */
    std::string canonicalBinding(const std::string& text, const TableForm form)
    {
        const auto binding = parseBinding(text, form);
        return binding ? bindingText(*binding, form) : text;
    }

    /**
     * Whether a binding setting takes a hand: one of its `hands`, or any where it names none.
     */
    bool takesHand(const ValueOptions& options, const vrcf::Hand hand)
    {
        return options.hands.empty() || std::ranges::contains(options.hands, hand);
    }

    /**
     * Whether a binding setting takes a way a button is worked: one of its `types`, or any where it names none.
     */
    bool takesType(const ValueOptions& options, const vrcf::ActivationType type)
    {
        return options.types.empty() || std::ranges::contains(options.types, type);
    }

    /**
     * Whether a binding setting takes a button, a push of whose axis too: one of its `buttons`, or any where it names
     * none.
     */
    bool takesButton(const ValueOptions& options, const vr::EVRButtonId button)
    {
        return options.buttons.empty() || std::ranges::contains(options.buttons, button);
    }

    /**
     * The button a binding is worked with: its own, or the button of the axis a push moves.
     */
    vr::EVRButtonId boundButton(const vrcf::InputBinding& binding)
    {
        if (binding.type != vrcf::ActivationType::AxisDirection) {
            return binding.button;
        }
        switch (binding.axis) {
        case vrcf::Axis::Trigger:
            return vr::k_EButton_SteamVR_Trigger;
        case vrcf::Axis::Grip:
            return vr::k_EButton_Grip;
        default:
            return vr::k_EButton_SteamVR_Touchpad;
        }
    }

    /**
     * Why a binding is not one its setting takes, in the framework's words, as `'trigger' is not a button this
     * binding takes, one of grip, menu`: its hand, then its way and then its button. Empty for one it takes, and for a
     * binding that is off, which a setting always takes.
     */
    std::string untakenPart(const vrcf::InputBinding& binding, const ValueOptions& options)
    {
        const auto reason = [](const std::string& word, const std::string_view part, const auto& taken, const auto& nameOf) {
            std::string words;
            for (const auto& value : taken) {
                words += (words.empty() ? "" : ", ") + nameOf(value);
            }
            return std::format("'{}' is not {} this binding takes, one of {}", word, part, words);
        };
        if (!binding.isEnabled()) {
            return {};
        }
        if (!takesHand(options, binding.hand)) {
            return reason(vrcf::handName(binding.hand), "a hand", options.hands, vrcf::handName);
        }
        if (!takesType(options, binding.type)) {
            return reason(vrcf::activationTypeName(binding.type), "a way", options.types, vrcf::activationTypeName);
        }
        if (const auto button = boundButton(binding); !takesButton(options, button)) {
            return reason(vrcf::buttonName(button), "a button", options.buttons, vrcf::buttonName);
        }
        return {};
    }
}
