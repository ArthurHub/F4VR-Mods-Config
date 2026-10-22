#include "ConfirmDialog.h"

#include <array>

#include <imgui.h>

#include "Drawing.h"
#include "Theme.h"

namespace
{
    // the popup's name: the title is the button's own
    constexpr auto POPUP_ID = "##confirmation";
}

namespace mods_config::ui
{
    /**
     * The confirmation a button of a config spec asks before it runs, as the design system's ButtonRow has it, in the
     * dialog of `beginDialog`: its title, its message, and Cancel and the confirming button at its right, which says
     * its `confirm`, the button's own text, or `Run`. A dangerous one has a red frame and a red confirming button.
     * It is shown while a button asks, and closed with none, as after Back. A pressed button closes it too.
     * Returns the button that was pressed.
     */
    ConfirmAnswer drawConfirmDialog(const model::Button* button)
    {
        const bool dangerous = button && button->confirmation && button->confirmation->dangerous;
        if (!beginDialog(POPUP_ID, button && button->confirmation, dangerous ? theme::color::RED : theme::color::GREEN)) {
            return ConfirmAnswer::None;
        }

        const auto& confirmation = *button->confirmation;
        if (!confirmation.title.empty()) {
            ImGui::PushFont(nullptr, theme::FONT_SIZE_TITLE);
            ImGui::TextUnformatted(confirmation.title.data(), confirmation.title.data() + confirmation.title.size());
            ImGui::PopFont();
        }
        if (!confirmation.message.empty()) {
            ImGui::PushTextWrapPos(0.0f);
            ImGui::TextUnformatted(confirmation.message.data(), confirmation.message.data() + confirmation.message.size());
            ImGui::PopTextWrapPos();
        }

        const std::string_view confirm = !confirmation.confirm.empty() ? std::string_view(confirmation.confirm)
                                         : !button->buttonText.empty() ? std::string_view(button->buttonText)
                                                                       : std::string_view("Run");
        const std::array<DialogButton, 2> buttons{ DialogButton{ .label = confirm, .look = dangerous ? ButtonLook::Danger : ButtonLook::Primary },
            DialogButton{ .label = "Cancel", .look = ButtonLook::Quiet } };
        const int pressed = dialogButtons(buttons);
        const auto answer = pressed == 0 ? ConfirmAnswer::Confirm : pressed == 1 ? ConfirmAnswer::Cancel : ConfirmAnswer::None;
        if (answer != ConfirmAnswer::None) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
        return answer;
    }
}
