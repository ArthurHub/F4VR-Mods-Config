#include "ConfirmDialog.h"

#include <array>
#include <format>
#include <string>
#include <string_view>

#include <imgui.h>

#include "Drawing.h"
#include "Theme.h"

namespace
{
    using namespace mods_config;

    // the popup's name: the title is the button's own
    constexpr auto POPUP_ID = "##confirmation";

    // the title of a confirmation that asks only to save the unsaved changes first
    constexpr std::string_view UNSAVED_TITLE = "Unsaved changes";

    /**
     * What a confirmation says of the unsaved changes it can save before the button runs: how many, that the
     * button's script reads the saved values, and how many are invalid, which are fixed or discarded to be saved.
     */
    std::string saveFirstText(const ui::SaveFirst& saveFirst)
    {
        const bool one = saveFirst.count == 1;
        const auto has = std::format("You have {} unsaved change{}", saveFirst.count, one ? "" : "s");
        if (saveFirst.invalid == 0) {
            return std::format("{}, and the button's script reads the saved values. Save {} before it runs?", has, one ? "it" : "them");
        }
        const auto invalid = one ? std::string(", which is invalid") : std::format(", and {} of them {} invalid", saveFirst.invalid, saveFirst.invalid == 1 ? "is" : "are");
        return std::format("{}{}. The button's script reads the saved values: fix or discard {} to save {} before it runs.",
            has,
            invalid,
            one ? "it" : "them",
            one ? "it" : "them");
    }

    /**
     * A text of the dialog, wrapped in its width.
     */
    void wrappedText(const std::string_view text)
    {
        ImGui::PushTextWrapPos(0.0f);
        ImGui::TextUnformatted(text.data(), text.data() + text.size());
        ImGui::PopTextWrapPos();
    }
}

namespace mods_config::ui
{
    /**
     * The confirmation a button asks before it runs, as the design system's ButtonRow has it, in the dialog of
     * `beginDialog`: its title, its message, and Cancel and the confirming button at its right, which says its
     * `confirm`, the button's own text, or `Run`. A dangerous one has a red frame and a red confirming button.
     * With unsaved changes to save first it asks for a button with no confirmation too, and says so: its confirming
     * button is `Save and run`, which is disabled while one of them is invalid, and `Run without saving` is beside it.
     * It is shown while a button asks, and closed with none, as after Back. A pressed button closes it too.
     * Returns the button that was pressed.
     */
    ConfirmAnswer drawConfirmDialog(const model::Button* button, const SaveFirst* saveFirst)
    {
        const auto* confirmation = button && button->confirmation ? &*button->confirmation : nullptr;
        const bool dangerous = confirmation && confirmation->dangerous;
        if (!beginDialog(POPUP_ID, button && (confirmation || saveFirst), dangerous ? theme::color::RED : theme::color::GREEN)) {
            return ConfirmAnswer::None;
        }

        const std::string_view title = confirmation ? std::string_view(confirmation->title) : UNSAVED_TITLE;
        if (!title.empty()) {
            ImGui::PushFont(nullptr, theme::FONT_SIZE_TITLE);
            ImGui::TextUnformatted(title.data(), title.data() + title.size());
            ImGui::PopFont();
        }
        if (confirmation && !confirmation->message.empty()) {
            wrappedText(confirmation->message);
        }
        if (saveFirst) {
            wrappedText(saveFirstText(*saveFirst));
        }

        const std::string_view confirm = saveFirst                        ? std::string_view("Save and run")
                                         : !confirmation->confirm.empty() ? std::string_view(confirmation->confirm)
                                         : !button->buttonText.empty()    ? std::string_view(button->buttonText)
                                                                          : std::string_view("Run");
        const DialogButton confirming{ .label = confirm, .look = dangerous ? ButtonLook::Danger : ButtonLook::Primary, .disabled = saveFirst && saveFirst->invalid > 0 };
        const DialogButton cancel{ .label = "Cancel", .look = ButtonLook::Quiet };
        ConfirmAnswer answer = ConfirmAnswer::None;
        if (saveFirst) {
            const std::array buttons{ confirming, DialogButton{ .label = "Run without saving", .look = ButtonLook::Secondary }, cancel };
            constexpr std::array ANSWERS{ ConfirmAnswer::Confirm, ConfirmAnswer::RunWithoutSaving, ConfirmAnswer::Cancel };
            const int pressed = dialogButtons(buttons);
            answer = pressed >= 0 ? ANSWERS[pressed] : ConfirmAnswer::None;
        } else {
            const std::array buttons{ confirming, cancel };
            const int pressed = dialogButtons(buttons);
            answer = pressed == 0 ? ConfirmAnswer::Confirm : pressed == 1 ? ConfirmAnswer::Cancel : ConfirmAnswer::None;
        }
        if (answer != ConfirmAnswer::None) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
        return answer;
    }
}
