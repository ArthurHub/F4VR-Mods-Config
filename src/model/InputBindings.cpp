#include "InputBindings.h"

#include <algorithm>
#include <cctype>
#include <format>

#include "common/TomlEditor.h"
#include "vrcf/InputBindingParser.h"

namespace mods_config::model
{
    /**
     * The binding of the TOML text of an inline table, as the framework reads it from its config, or the
     * framework's reason it is none, as `'thumbstik' is not a button, one of trigger, grip, ...`, with its first
     * letter in upper case. A text that is no TOML value is none too.
     */
    std::expected<vrcf::InputBinding, std::string> parseBinding(const std::string_view text)
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
     * A binding as the TOML text of its inline table, as the framework writes it: its words, its keys in its order, and
     * a value left out where it is the default.
     */
    std::string bindingText(const vrcf::InputBinding& binding)
    {
        return vrcf::inputBindingTomlText(binding);
    }

    /**
     * The text of a binding as the framework writes it, so two texts of one binding are the same text: a file can
     * write a word, a key's place and a number in another way. A text that is no binding is as it is.
     */
    std::string canonicalBinding(const std::string& text)
    {
        const auto binding = parseBinding(text);
        return binding ? bindingText(*binding) : text;
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
