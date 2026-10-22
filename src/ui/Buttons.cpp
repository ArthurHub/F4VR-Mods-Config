#include "Buttons.h"

#include <string_view>
#include <variant>

#include <imgui.h>

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
}

namespace mods_config::ui
{
    /**
     * The row of a button at the cursor: its text, and its button where a control ends, before the room of a Reset,
     * with its own text or `Run`. The whole row is pressed, so the button is only drawn, and a press runs the action
     * in the store, or shows its confirmation first. A button with no action is disabled, and so is one whose action
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
        if (runs && row.pressed) {
            if (button.confirmation) {
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
        drawOutline(box.Min, box.Max, theme::color::FRAME_BORDER);
        drawTextIn(box, button.buttonText.empty() ? std::string_view("Run") : std::string_view(button.buttonText), theme::color::GREEN, 0.5f, theme::SPACE_3);
        drawLabel(pos, box.Min.x - theme::SPACE_3, button.text, button.textRuns, { .mark = mark });
        ImGui::EndDisabled();
        if (!row.pointed) {
            return std::nullopt;
        }

        PointedRow pointed{ .text = button.text, .help = reason.empty() ? button.help : std::move(reason), .warning = warning };
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
     * closes it with nothing run. Returns whether a press on it ran the action or closed it.
     */
    bool Buttons::drawDialog(model::Store* store)
    {
        if (!store) {
            _confirming = nullptr;
        }
        const auto answer = drawConfirmDialog(_confirming);
        if (answer == ConfirmAnswer::None) {
            return false;
        }
        const auto* button = _confirming;
        _confirming = nullptr;
        if (answer == ConfirmAnswer::Confirm) {
            run(*button, *store);
        }
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
