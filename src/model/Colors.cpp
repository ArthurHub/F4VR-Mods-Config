#include "Colors.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <format>
#include <ranges>
#include <vector>

#include "ParseCache.h"
#include "common/CommonUtils.h"
#include "common/TomlEditor.h"
#include "config/ValueParsers.h"

namespace
{
    // the color a key that neither the value nor its default has keeps: white, and opaque
    constexpr f4cf::render::Color WHITE{};

    /**
     * The channel from 0 to 255 a value of a color's slider is: its place from the slider's min to its max, the nearest
     * whole number. None for a value that is no number, and for a slider with no range.
     */
    std::optional<int> channelOf(const mods_config::model::Setting& part, const mods_config::model::Value& value)
    {
        const auto& range = part.valueOptions;
        const float span = range.max - range.min;
        const auto* whole = std::get_if<int>(&value);
        const auto* number = std::get_if<float>(&value);
        if ((!whole && !number) || span <= 0.0f || f4cf::common::fEqual(span, 0.0f)) {
            return std::nullopt;
        }
        const float at = whole ? static_cast<float>(*whole) : *number;
        return std::clamp(static_cast<int>(std::lround((at - range.min) / span * 255.0f)), 0, 255);
    }
}

namespace mods_config::model
{
    /**
     * The color table of the TOML text of an inline table, parsed each time: `parseColorTable` keeps what it gives.
     */
    static std::expected<ColorTable, std::string> parseColorTableText(const std::string_view text)
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
     * The color table of an INI file's line of a color, parsed each time: `parseColorTable` keeps what it gives.
     */
    static std::expected<ColorTable, std::string> parseColorLine(const std::string_view text)
    {
        const auto notColor = [] {
            return std::unexpected(std::string("it is not r,g,b or r,g,b,a, each a whole number in 0..255, as 255,180,107"));
        };
        std::vector<int> channels;
        for (const auto part : std::views::split(text, ',')) {
            const auto number = common::trim(std::string(std::string_view(part)));
            int channel = 0;
            const auto [end, error] = std::from_chars(number.data(), number.data() + number.size(), channel);
            if (number.empty() || error != std::errc() || end != number.data() + number.size() || channel < 0 || channel > 255) {
                return notColor();
            }
            channels.push_back(channel);
        }
        if (channels.size() != 3 && channels.size() != 4) {
            return notColor();
        }
        return ColorTable{ .rgb = std::array{ channels[0], channels[1], channels[2] }, .alpha = channels.size() == 4 ? std::optional(channels[3]) : std::nullopt };
    }

    /**
     * The color table of a color's text, or the reason it is none. The TOML text of an inline table is read as the
     * framework reads it from its config, with the framework's reason, as `rgb has a number that is not a whole number
     * in 0..255`, which starts with the key it names as it is written. An INI line is `r,g,b` or `r,g,b,a`, with spaces
     * around a number allowed, and has rgb, and alpha where it has four. Parsed once per text (`cachedParse`).
     */
    std::expected<ColorTable, std::string> parseColorTable(const std::string_view text, const TableForm form)
    {
        return form == TableForm::IniLine ? cachedParse<parseColorLine>(text) : cachedParse<parseColorTableText>(text);
    }

    /**
     * A color table as the text of its form. The TOML text of its inline table has the keys it has, rgb before alpha:
     * `{ rgb = [190, 255, 190], alpha = 200 }`, and `{}` for a table with no key. An INI line is `r,g,b`, white for a
     * table with no rgb, with `,a` where the table has alpha: `190,255,190,200`.
     */
    std::string colorText(const ColorTable& table, const TableForm form)
    {
        if (form == TableForm::IniLine) {
            const auto rgb = table.rgb.value_or(std::array{ 255, 255, 255 });
            const auto line = std::format("{},{},{}", rgb[0], rgb[1], rgb[2]);
            return table.alpha ? std::format("{},{}", line, *table.alpha) : line;
        }
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
    std::string canonicalColor(const std::string& text, const TableForm form)
    {
        const auto table = parseColorTable(text, form);
        return table ? colorText(*table, form) : text;
    }

    /**
     * The table of a color's text, or of its setting's default for a text that is no color, and an empty table where
     * neither is one: the color an editor starts from.
     */
    ColorTable colorOrDefault(const Setting& setting, const std::string_view text)
    {
        auto table = parseColorTable(text, setting.tableForm);
        const auto* fallback = setting.defaultValue ? std::get_if<std::string>(&*setting.defaultValue) : nullptr;
        if (!table && fallback) {
            table = parseColorTable(*fallback, setting.tableForm);
        }
        return table.value_or(ColorTable{});
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
        const auto table = parseColorTable(*text, setting.tableForm);
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
        const auto table = parseColorTable(*text, setting.tableForm);
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

    /**
     * The `r,g,b` or `r,g,b,a` of a color kept in parts (`ColorPartsStorage`) from their values: each part is a slider,
     * and a value from its min to its max is a channel from 0 to 255, the nearest whole number. None for a value that is
     * no number, a part with no range, or other than three or four parts.
     */
    std::optional<std::string> colorOfParts(const std::span<const Setting* const> parts, const std::span<const Value> values)
    {
        if (parts.size() < 3 || parts.size() > 4 || values.size() != parts.size()) {
            return std::nullopt;
        }
        ColorTable table{ .rgb = std::array<int, 3>{}, .alpha = std::nullopt };
        for (std::size_t i = 0; i < parts.size(); ++i) {
            const auto channel = channelOf(*parts[i], values[i]);
            if (!channel) {
                return std::nullopt;
            }
            if (i < 3) {
                (*table.rgb)[i] = *channel;
            } else {
                table.alpha = *channel;
            }
        }
        return colorText(table, TableForm::IniLine);
    }

    /**
     * The value of a color kept in sliders from what `read` gives each of its parts, in their order. It is not available
     * while a part is not, or is no setting of its mod (null), with the part's reason, and is invalid while a part is,
     * with that part's reason. With no value it is black.
     */
    StoredValue colorPartsValue(const std::span<const Setting* const> parts, const std::function<StoredValue(const Setting& part)>& read)
    {
        StoredValue color{ .value = std::string("0,0,0"), .available = false, .invalid = false, .reason = {} };
        std::vector<Value> values;
        for (const auto* part : parts) {
            if (!part) {
                color.reason = "Not available: a part of the color is no setting of its mod.";
                return color;
            }
            auto stored = read(*part);
            if (!stored.available) {
                color.reason = std::move(stored.reason);
                return color;
            }
            if (stored.invalid && !color.invalid) {
                color.invalid = true;
                color.reason = std::move(stored.reason);
            }
            values.push_back(std::move(stored.value));
        }
        auto text = colorOfParts(parts, values);
        if (!text) {
            return { .value = std::move(color.value), .available = false, .invalid = false, .reason = {} };
        }
        color.value = std::move(*text);
        color.available = true;
        return color;
    }

    /**
     * The values a color's `r,g,b` or `r,g,b,a` gives its parts, in their order: each channel from 0 to 255 as a value
     * from its slider's min to its max, as a slider's control has it, a float, of a whole number for a slider of one.
     * None for a part whose `current` value is that channel already, which keeps its value rather than its channel's
     * rounding, and for the alpha part while the color has no alpha. Empty for a text that is no color, and for a part
     * with no range.
     */
    std::vector<std::optional<Value>> partsOfColor(const std::span<const Setting* const> parts, const std::string_view color, const std::span<const Value> current)
    {
        const auto table = parseColorTable(color, TableForm::IniLine);
        if (!table || !table->rgb || parts.size() < 3 || parts.size() > 4) {
            return {};
        }
        std::vector<std::optional<Value>> values;
        for (std::size_t i = 0; i < parts.size(); ++i) {
            const auto channel = i < 3 ? std::optional((*table->rgb)[i]) : table->alpha;
            const auto& range = parts[i]->valueOptions;
            if (range.max <= range.min || common::fEqual(range.max, range.min)) {
                return {};
            }
            if (!channel || (i < current.size() && channelOf(*parts[i], current[i]) == channel)) {
                values.emplace_back();
                continue;
            }
            const float value = range.min + static_cast<float>(*channel) / 255.0f * (range.max - range.min);
            values.emplace_back(parts[i]->valueType == StoredType::Int ? std::round(value) : value);
        }
        return values;
    }
}
