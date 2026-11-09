#pragma once

#include <array>
#include <cstddef>
#include <expected>
#include <optional>
#include <string>
#include <string_view>

#include "ConfigDefinition.h"

// The value of a transform setting, which is the TOML text of a CommonFramework transform's inline table,
// `{ pos = [x, y, z], rot = [heading, roll, attitude], scale = s }`, as the framework reads it, with each key optional
// and the rotation in degrees, or in an INI file its line of seven numbers, `x,y,z;heading,roll,attitude;scale`.

namespace mods_config::model
{
    /**
     * A transform's table: the keys it has, each of which is optional, as the framework reads it.
     */
    struct TransformTable
    {
        std::optional<std::array<float, 3>> pos;
        std::optional<std::array<float, 3>> rot;
        std::optional<float> scale;

        bool operator==(const TransformTable&) const = default;
    };

    std::expected<TransformTable, std::string> parseTransformTable(std::string_view text, TableForm form);
    std::string transformText(const TransformTable& table, TableForm form);
    std::string canonicalTransform(const std::string& text, TableForm form);
    float axisValue(const TransformTable& table, std::size_t axis);
    void setAxis(TransformTable& table, std::size_t axis, float value);
}
