#include "Colors.h"

#include <format>

#include "common/TomlEditor.h"
#include "config/ValueParsers.h"

namespace
{
    // the color a key that neither the value nor its default has keeps: white, and opaque
    constexpr f4cf::render::Color WHITE{};
}

namespace mods_config::model
{
    /**
     * The color table of the TOML text of an inline table, as the framework reads it from its config, or the
     * framework's reason it is none, as `rgb has a number that is not a whole number in 0..255`, which starts with the
     * key it names as it is written. A text that is no TOML value is none too.
     */
    std::expected<ColorTable, std::string> parseColorTable(const std::string_view text)
    {
        std::string reason = "it is not a table, as { rgb = [255, 255, 255], alpha = 255 }";
        if (const auto document = common::parseToml(std::format("color = {}", text)); document && document->size() == 1) {
            const auto& node = *document->get("color");
            render::Color parsed;
            const auto read = config::parseColor(node, parsed);
            if (read) {
                // each key the table has, which the framework has read from a whole number in 0..255
                const auto bytes = parsed.bytes();
                ColorTable table;
                const auto& keys = *node.as_table();
                if (keys.contains("rgb")) {
                    table.rgb = std::array{ bytes[0], bytes[1], bytes[2] };
                }
                if (keys.contains("alpha")) {
                    table.alpha = bytes[3];
                }
                return table;
            }
            reason = read.error();
        }
        return std::unexpected(std::move(reason));
    }

    /**
     * A color table as the TOML text of its inline table, with the keys it has, rgb before alpha:
     * `{ rgb = [190, 255, 190], alpha = 200 }`. A table with no key is `{}`.
     */
    std::string colorText(const ColorTable& table)
    {
        std::string text;
        if (table.rgb) {
            text += std::format("rgb = [{}, {}, {}]", (*table.rgb)[0], (*table.rgb)[1], (*table.rgb)[2]);
        }
        if (table.alpha) {
            text += std::format("{}alpha = {}", text.empty() ? "" : ", ", *table.alpha);
        }
        return text.empty() ? "{}" : std::format("{{ {} }}", text);
    }

    /**
     * The text of a color as the menu writes it, so two texts of one color are the same text: a file can write a key's
     * place and its spaces in another way. A text that is no color is as it is.
     */
    std::string canonicalColor(const std::string& text)
    {
        const auto table = parseColorTable(text);
        return table ? colorText(*table) : text;
    }

    /**
     * The color a color table gives, as the framework reads it: the keys it has, over the channels of a base.
     */
    render::Color tableColor(const ColorTable& table, const render::Color& base)
    {
        auto bytes = base.bytes();
        if (table.rgb) {
            bytes = { (*table.rgb)[0], (*table.rgb)[1], (*table.rgb)[2], bytes[3] };
        }
        if (table.alpha) {
            bytes[3] = *table.alpha;
        }
        return render::Color::rgba(bytes[0], bytes[1], bytes[2], bytes[3]);
    }

    /**
     * The color a color setting's value is read over: its default, and white and opaque for a key the default does not
     * have, as for a setting with no default.
     */
    render::Color colorBase(const Setting& setting)
    {
        const auto* text = setting.defaultValue ? std::get_if<std::string>(&*setting.defaultValue) : nullptr;
        if (!text) {
            return WHITE;
        }
        const auto table = parseColorTable(*text);
        return table ? tableColor(*table, WHITE) : WHITE;
    }

    /**
     * The color of a value of a color setting, as the framework reads it over the setting's default. None for a value
     * that is no color.
     */
    std::optional<render::Color> colorOf(const Setting& setting, const Value& value)
    {
        const auto* text = std::get_if<std::string>(&value);
        if (!text) {
            return std::nullopt;
        }
        const auto table = parseColorTable(*text);
        return table ? std::optional(tableColor(*table, colorBase(setting))) : std::nullopt;
    }

    /**
     * A color as its numbers, as the file writes them: `190, 255, 190, alpha 200`, and with no alpha for a color that
     * is RGB only.
     */
    std::string colorNumbers(const render::Color& color, const bool alpha)
    {
        const auto bytes = color.bytes();
        const auto rgb = std::format("{}, {}, {}", bytes[0], bytes[1], bytes[2]);
        return alpha ? std::format("{}, alpha {}", rgb, bytes[3]) : rgb;
    }
}
