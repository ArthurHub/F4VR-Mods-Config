#pragma once

#include <optional>

#include "Choices.h"
#include "HelpLine.h"
#include "PageRows.h"
#include "Sliders.h"
#include "TextInputs.h"
#include "model/Definition.h"
#include "model/Store.h"

namespace mods_config::ui
{
    // The value store and control editors used by a setting row, owned by the menu and page.
    struct SettingRowContext
    {
        model::Store* store = nullptr;
        Sliders& sliders;
        Choices& choices;
        TextInputs& textInputs;
    };

    // Hover help and whether an input acted while drawing and editing a setting row.
    struct SettingRowDrawn
    {
        std::optional<PointedRow> pointed;
        bool pressActed = false;
    };

    SettingRowDrawn drawSettingRow(const model::Setting& setting, const SettingRowContext& context, const Gutter& gutter);
}
