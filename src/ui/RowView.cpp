#include "RowView.h"

#include "PageRows.h"
#include "model/Conditions.h"

namespace mods_config::ui
{
    /**
     * How the row of a setting looks now, read from its store. With no store nothing is available, so the row
     * shows no value and cannot be edited.
     */
    RowView rowViewOf(const model::Setting& setting, model::StoreReader* store)
    {
        RowView view;
        view.stored = store ? store->value(setting) : model::StoredValue{};
        view.enabled = !setting.enabledWhen || model::holds(*setting.enabledWhen, store);
        view.editable = view.stored.available && view.enabled && !setting.readOnly;
        view.dirty = view.stored.available && store->isDirty(setting);
        view.look = view.stored.invalid ? ValueLook::Invalid : view.dirty ? ValueLook::Pending : ValueLook::Saved;
        view.warning = view.stored.invalid ? std::string_view() : warningOf(setting.warnings, store);
        view.applies = view.dirty ? store->applies(setting) : std::nullopt;
        view.mark = view.stored.invalid ? TextMark::Invalid : !view.warning.empty() ? TextMark::Warning : TextMark::None;
        return view;
    }
}
