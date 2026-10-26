#include "InputBindings.h"

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
}
