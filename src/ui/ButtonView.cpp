#include "ButtonView.h"

#include <variant>

#include "model/Conditions.h"

namespace
{
    using namespace mods_config;

    /**
     * Whether a button's action reads the saved values of its mod, so a press asks to save its unsaved changes
     * first: a script's function, which reads the globals, properties and ModSettings that Save writes. A console
     * command, a link and a function of the mod's DLL, which has the `live` values as a preview, do not.
     */
    bool readsSavedValues(const model::Action& action)
    {
        return std::holds_alternative<model::CallFunction>(action);
    }
}

namespace mods_config::ui
{
    /**
     * How a press on a button goes now. With no store nothing runs. One whose action reads the saved values asks to
     * save first while its store keeps unsaved changes for a Save; an MCM mod's store writes a change as it is made,
     * so its script has it.
     */
    ButtonView buttonViewOf(const model::Button& button, model::Store* store)
    {
        ButtonView view;
        view.enabled = !button.enabledWhen || model::holds(*button.enabledWhen, store);
        const auto* unknown = std::get_if<model::UnknownAction>(&button.action);
        view.reason = unknown ? unknown->reason : store ? store->notRunnable(button.action) : std::string();
        view.runs = store && view.enabled && !unknown && !std::holds_alternative<model::NoAction>(button.action) && view.reason.empty();
        view.asksToSaveFirst = view.runs && readsSavedValues(button.action) && !store->writesChangesAtOnce() && store->dirtyCount() > 0;
        return view;
    }
}
