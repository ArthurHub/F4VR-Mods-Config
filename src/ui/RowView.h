#pragma once

#include <optional>
#include <string_view>

#include "Drawing.h"
#include "model/ConfigDefinition.h"
#include "model/Store.h"

namespace mods_config::ui
{
    /**
     * How the row of a setting looks, from the setting and its store. The setting row draws it and devbench reports
     * it, so what an e2e test checks of a row is what the player sees.
     */
    struct RowView
    {
        // its value in the store, with whether it is available or invalid, and why
        model::StoredValue stored;

        // its enabledWhen holds
        bool enabled = false;

        // the player can change it: it is available, enabled and not read-only
        bool editable = false;

        // its change waits for a Save. A value that is not available is never dirty
        bool dirty = false;

        // invalid in red before pending in amber
        ValueLook look = ValueLook::Saved;

        // the warning it tells, and none while its value is invalid, whose mark takes the warning's place. It views
        // the setting's own text, so the view lives no longer than the setting
        std::string_view warning;

        // when its change takes effect, only while it is dirty, and none where its store does not know
        std::optional<model::Applies> applies;

        // the mark after its name: invalid, warning, or none
        TextMark mark = TextMark::None;
    };

    RowView rowViewOf(const model::Setting& setting, model::StoreReader* store);
}
