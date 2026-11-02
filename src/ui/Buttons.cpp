#include "Buttons.h"

#include <algorithm>
#include <cmath>
#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <variant>

#include <imgui.h>

#include "ButtonIcons.h"
#include "ConfirmDialog.h"
#include "Drawing.h"
#include "PageRows.h"
#include "Theme.h"
#include "model/Conditions.h"

namespace
{
    using namespace mods_config;

    // the button of a button's row, in the panel's pixels: as wide as the Open of the menu's front page
    constexpr float BUTTON_WIDTH = 160.0f;

    // what the help line says of a run that failed with no message of its own
    constexpr auto FAILED = "It failed.";

    // what the help of a button that calls a script says while its mod has unsaved changes
    constexpr auto SAVE_FIRST_HELP = "A press asks to save the unsaved changes first.";

    /**
     * Whether a button's action reads the saved values of its mod, so a press asks to save its unsaved changes
     * first: a script's function, which reads the globals, properties and ModSettings that Save writes. A console
     * command, a link and a function of the mod's DLL, which has the `live` values as a preview, do not.
     */
    bool readsSavedValues(const model::Action& action)
    {
        return std::holds_alternative<model::CallFunction>(action);
    }

    /**
     * Whether a press on a button asks to save its mod's unsaved changes first: its action reads the saved values, and
     * its store keeps the changes for a Save. An MCM mod's store writes a change as it is made, so its script has it.
     */
    bool asksToSaveFirst(const model::Action& action, const model::Store& store)
    {
        return readsSavedValues(action) && !store.writesChangesAtOnce() && store.dirtyCount() > 0;
    }

    /**
     * The color of a button's text and icon, and of the outline of one with a style.
     */
    ImU32 styleColor(const model::ButtonStyle style)
    {
        switch (style) {
        case model::ButtonStyle::Important:
            return theme::color::ACCENT;
        case model::ButtonStyle::Dangerous:
            return theme::color::RED;
        case model::ButtonStyle::None:
            break;
        }
        return theme::color::GREEN;
    }

    /**
     * What a button says, in the middle of its box: its text, `Run` without one, with its icon `space-2` before it. A
     * text too long for the room left beside the icon ends in an ellipsis.
     */
    void drawButtonContent(const model::Button& button, const ImRect& box, const ImU32 color)
    {
        const auto text = button.buttonText.empty() ? std::string_view("Run") : std::string_view(button.buttonText);
        if (button.icon == model::ButtonIcon::None) {
            ui::drawTextIn(box, text, color, 0.5f, theme::SPACE_3);
            return;
        }
        const float iconRoom = ui::BUTTON_ICON_SIZE + theme::SPACE_2;
        const float room = box.GetWidth() - theme::SPACE_3 * 2.0f;
        const float left = std::floor(box.Min.x + theme::SPACE_3 + (std::max)(room - iconRoom - ui::textWidth(text), 0.0f) * 0.5f);
        ui::drawButtonIcon(button.icon, ImVec2(left + ui::BUTTON_ICON_SIZE * 0.5f, box.GetCenter().y), color);
        ui::drawText(left + iconRoom, box.Max.x - theme::SPACE_3, box.Min.y, box.GetHeight(), color, text);
    }
}

namespace mods_config::ui
{
    /**
     * The row of a button at the cursor: its text, and its button where a control ends, before the room of a Reset,
     * with its own text or `Run` after its icon, in the color of its style, with a ring of it while it is pressed. The
     * whole row is pressed, so the button is only drawn, and a press runs the action
     * in the store, or shows its confirmation first: also one with none that calls a script while the store keeps
     * unsaved changes for a Save, as the script reads the saved values, which its help says. A button with no action is disabled, and so is one whose action
     * the menu does not run or its store cannot run, which the reason says, and one whose enabledWhen does not hold.
     * After its text it has the invalid mark of a run that failed, the info mark of a result that is not read, or
     * the warning mark of a warning that holds. Returns the row for the help line while the ray is on it.
     */
    std::optional<PointedRow> Buttons::draw(const model::Button& button, model::Store* store, bool& pressActed)
    {
        const auto pos = ImGui::GetCursorScreenPos();
        const float width = rowWidth();
        const bool enabled = !button.enabledWhen || model::holds(*button.enabledWhen, store);
        const auto row = addRow(enabled);
        if (!enabled && row.shown) {
            drawRowState();
        }

        const auto* unknown = std::get_if<model::UnknownAction>(&button.action);
        auto reason = unknown ? unknown->reason : store ? store->notRunnable(button.action) : std::string();
        const bool runs = store && enabled && !unknown && !std::holds_alternative<model::NoAction>(button.action) && reason.empty();
        const bool saveFirst = runs && asksToSaveFirst(button.action, *store);
        if (runs && row.pressed) {
            if (button.confirmation || saveFirst) {
                _confirming = &button;
                pressActed = true;
            } else if (run(button, *store)) {
                pressActed = true;
            }
        }

        const auto found = _results.find(&button);
        auto* result = found != _results.end() ? &found->second : nullptr;
        if (result && row.pointed) {
            result->seen = true;
        } else if (result && result->seen) {
            result->unread = false;
        }
        if (!row.shown) {
            return std::nullopt;
        }
        const auto warning = warningOf(button.warnings, store);
        const auto mark = result && result->failed ? TextMark::Invalid : result && result->unread ? TextMark::Info : !warning.empty() ? TextMark::Warning : TextMark::None;

        ImGui::BeginDisabled(!runs);
        const auto box = controlBox(pos.x + width - theme::CONTROL_HEIGHT - theme::SPACE_2 - BUTTON_WIDTH, pos.y, BUTTON_WIDTH);
        const bool styled = button.style != model::ButtonStyle::None;
        const auto color = styleColor(button.style);
        drawOutline(box.Min, box.Max, styled ? color : theme::color::FRAME_BORDER);
        if (styled && row.held) {
            drawButtonRing(box, color);
        }
        drawButtonContent(button, box, color);
        drawLabel(pos, box.Min.x - theme::SPACE_3, button.text, button.textRuns, { .mark = mark });
        ImGui::EndDisabled();
        if (!row.pointed) {
            return std::nullopt;
        }

        PointedRow pointed{ .text = button.text, .help = reason.empty() ? button.help : std::move(reason), .warning = warning };
        if (saveFirst) {
            pointed.help += pointed.help.empty() ? SAVE_FIRST_HELP : std::string(" ") + SAVE_FIRST_HELP;
        }
        if (result && result->failed) {
            pointed.help = result->message;
            pointed.invalid = true;
        } else if (result) {
            pointed.result = result->message;
        }
        return pointed;
    }

    /**
     * The confirmation of the button that shows it: its confirming button runs the button's action, and Cancel
     * closes it with nothing run. For a script, while the store has unsaved changes, the confirming button saves
     * them first, and a Save that fails runs nothing, which the button's result says, and `Run without saving`
     * runs it with them pending. A button with no confirmation of its own whose changes are gone, as Discard took
     * them back, asks nothing more and is not run.
     * Returns whether a press on it ran the action or closed it.
     */
    bool Buttons::drawDialog(model::Store* store)
    {
        std::optional<SaveFirst> saveFirst;
        if (store && _confirming && asksToSaveFirst(_confirming->action, *store)) {
            saveFirst = SaveFirst{ .count = store->dirtyCount(), .invalid = store->invalidCount() };
        }
        if (!store || (_confirming && !_confirming->confirmation && !saveFirst)) {
            _confirming = nullptr;
        }
        const auto answer = drawConfirmDialog(_confirming, saveFirst ? &*saveFirst : nullptr);
        if (answer == ConfirmAnswer::None) {
            return false;
        }
        const auto* button = _confirming;
        _confirming = nullptr;
        if (answer == ConfirmAnswer::Cancel) {
            return true;
        }
        if (answer == ConfirmAnswer::Confirm && saveFirst) {
            if (const auto saved = store->save(); !saved) {
                _results.insert_or_assign(button, Result{ .failed = true, .message = std::format("Not run, as the changes are not saved: {}.", saved.error()) });
                return true;
            }
        }
        run(*button, *store);
        return true;
    }

    /**
     * Closes the confirmation that is shown with nothing run, as Back does. Returns whether one was shown.
     */
    bool Buttons::cancelConfirmation()
    {
        const bool shown = _confirming != nullptr;
        _confirming = nullptr;
        return shown;
    }

    /**
     * Forgets the results and the confirmation, as another page is shown.
     */
    void Buttons::reset()
    {
        _results.clear();
        _confirming = nullptr;
    }

    /**
     * Runs a button's action in its store, and keeps its result: a run that failed, or one with a message. One with
     * neither drops the result before it. Returns whether the action ran.
     */
    bool Buttons::run(const model::Button& button, model::Store& store)
    {
        auto result = store.run(button.action);
        _results.erase(&button);
        if (result.ran && (result.failed || !result.message.empty())) {
            _results.emplace(&button, Result{ .failed = result.failed, .message = result.message.empty() ? std::string(FAILED) : std::move(result.message) });
        }
        return result.ran;
    }
}
