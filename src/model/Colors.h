#pragma once

#include <array>
#include <expected>
#include <optional>
#include <string>
#include <string_view>

#include "ConfigDefinition.h"
#include "render/Color.h"

// The value of a color setting, which is the TOML text of a CommonFramework color's inline table,
// `{ rgb = [r, g, b], alpha = a }`, as the framework reads it, with each key optional.

namespace mods_config::model
{
    /**
     * A color's table: the keys it has, each of which is optional, as the framework reads it.
     */
    struct ColorTable
    {
        std::optional<std::array<int, 3>> rgb;
        std::optional<int> alpha;

        bool operator==(const ColorTable&) const = default;
    };

    std::expected<ColorTable, std::string> parseColorTable(std::string_view text);
    std::string colorText(const ColorTable& table);
    std::string canonicalColor(const std::string& text);
    render::Color tableColor(const ColorTable& table, const render::Color& base);
    render::Color colorBase(const Setting& setting);
    std::optional<render::Color> colorOf(const Setting& setting, const Value& value);
    std::string colorNumbers(const render::Color& color, bool alpha);
}
