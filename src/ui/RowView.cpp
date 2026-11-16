#include "RowView.h"

#include "Choices.h"
#include "PageRows.h"
#include "model/Conditions.h"
#include "model/SettingValues.h"

namespace
{
    using namespace mods_config;

    /**
     * The store's default of a setting when its control can show it: a choice's must be one of its options, and a
     * slider needs a range. Unsupported values have none either.
     */
    std::optional<model::Value> shownDefault(const model::Setting& setting, model::Store& store)
    {
        auto value = store.defaultValue(setting);
        if (!value) {
            return std::nullopt;
        }
        const bool shown = model::isChoice(setting.type)
                               ? ui::chosenOption(store.options(setting), { .value = *value, .available = true, .reason = {} }).has_value()
                               : model::isControlValue(setting.type, *value) && (setting.type != model::SettingType::Slider || setting.valueOptions.max > setting.valueOptions.min);
        return shown ? value : std::nullopt;
    }
}

namespace mods_config::ui
{
    /**
     * How the row of a setting looks now, read from its store. With no store nothing is available, so the row
     * shows no value, cannot be edited and has no Reset.
     */
    RowView rowViewOf(const model::Setting& setting, model::Store* store)
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
        view.resetValue = view.stored.available ? shownDefault(setting, *store) : std::nullopt;
        view.resets = view.editable && !view.dirty && view.resetValue && (view.stored.invalid || !model::sameValue(view.stored.value, *view.resetValue));
        return view;
    }
}
