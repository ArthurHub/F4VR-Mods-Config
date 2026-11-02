#include "UnsavedDialog.h"

#include <array>
#include <format>

#include <imgui.h>
#include <imgui_internal.h>

#include "Drawing.h"
#include "Theme.h"

namespace
{
    using namespace mods_config;

    // the dialog's title, which is its popup's name too
    constexpr auto TITLE = "Unsaved changes";

    // its title for changes that are made already, as an MCM mod's are
    constexpr auto MADE_TITLE = "Keep changes?";

    /**
     * What the dialog says: the mod that has the changes, how many, and what the player was about to do. With
     * an invalid one Save is disabled, so it asks to Discard them. Changes that are made already are not invalid.
     */
    std::string questionText(const ui::UnsavedChanges& changes)
    {
        const bool one = changes.count == 1;
        if (changes.made) {
            return std::format("{} has {} change{} made. Keep {} before you {}?", changes.mod, changes.count, one ? "" : "s", one ? "it" : "them", changes.next);
        }
        const auto has = std::format("{} has {} unsaved change{}", changes.mod, changes.count, one ? "" : "s");
        if (changes.invalid == 0) {
            return std::format("{}. Save {} before you {}?", has, one ? "it" : "them", changes.next);
        }
        const auto invalid = one ? std::string(", which is invalid") : std::format(", and {} of them {} invalid", changes.invalid, changes.invalid == 1 ? "is" : "are");
        return std::format("{}{}. Discard {} before you {}?", has, invalid, one ? "it" : "them", changes.next);
    }
}

namespace mods_config::ui
{
    /**
     * The dialog that asks what to do with a mod's unsaved changes before the mod is left or the menu is
     * closed, as the design system's ConfirmDialog has it (`beginDialog`). Its title, the question, and Cancel,
     * Discard and Save at its right, with Save disabled while a change is invalid, as the footer's is. For changes
     * that are made already they are Revert and Keep, as the footer's are. It is shown while there are changes to ask about, and closed with none, as after Back. A pressed button
     * closes it too. Returns the button that was pressed.
     */
    DialogAnswer drawUnsavedDialog(const UnsavedChanges* changes)
    {
        if (!beginDialog(TITLE, changes != nullptr, theme::color::GREEN)) {
            return DialogAnswer::None;
        }

        ImGui::PushFont(nullptr, theme::FONT_SIZE_TITLE);
        ImGui::TextUnformatted(changes->made ? MADE_TITLE : TITLE);
        ImGui::PopFont();
        const auto question = questionText(*changes);
        ImGui::PushTextWrapPos(0.0f);
        ImGui::TextUnformatted(question.data(), question.data() + question.size());
        ImGui::PopTextWrapPos();

        const std::array<DialogButton, 3> buttons{ DialogButton{ .label = changes->made ? "Keep" : "Save", .look = ButtonLook::Primary, .disabled = changes->invalid > 0 },
            DialogButton{ .label = changes->made ? "Revert" : "Discard", .look = ButtonLook::Secondary },
            DialogButton{ .label = "Cancel", .look = ButtonLook::Quiet } };
        constexpr std::array ANSWERS{ DialogAnswer::Save, DialogAnswer::Discard, DialogAnswer::Cancel };
        const int pressed = dialogButtons(buttons);
        const auto answer = pressed >= 0 ? ANSWERS[pressed] : DialogAnswer::None;

        if (answer != DialogAnswer::None) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
        return answer;
    }
}
