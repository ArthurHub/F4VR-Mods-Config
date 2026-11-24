#pragma once

#include <array>
#include <expected>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "ConfigDefinition.h"
#include "Store.h"
#include "render/Color.h"

// The value of a color setting, which is the TOML text of a CommonFramework color's inline table,
// `{ rgb = [r, g, b], alpha = a }`, as the framework reads it, with each key optional, or in an INI file its line,
// `r,g,b` or `r,g,b,a`.

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

    std::expected<ColorTable, std::string> parseColorTable(std::string_view text, TableForm form);
    std::string colorText(const ColorTable& table, TableForm form);
    std::string canonicalColor(const std::string& text, TableForm form);
    ColorTable colorOrDefault(const Setting& setting, std::string_view text);
    render::Color tableColor(const ColorTable& table, const render::Color& base);
    render::Color colorBase(const Setting& setting);
    std::optional<render::Color> colorOf(const Setting& setting, const Value& value);
    std::string colorNumbers(const render::Color& color, bool alpha);
    std::optional<std::string> colorOfParts(std::span<const Setting* const> parts, std::span<const Value> values);
    std::vector<std::optional<Value>> partsOfColor(std::span<const Setting* const> parts, std::string_view color, std::span<const Value> current = {});
    StoredValue colorPartsValue(std::span<const Setting* const> parts, const std::function<StoredValue(const Setting& part)>& read);
}
