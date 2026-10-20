#include "UnsavedDialog.h"

#include <algorithm>
#include <cmath>
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

    // The dialog's sizes, which are not tokens of the design system, in the panel's pixels.
    // Its width:
    constexpr float DIALOG_WIDTH = 576.0f;
    // and the least a button is, which is wider for a longer word:
    constexpr float BUTTON_MIN_WIDTH = 112.0f;

    /**
     * The width of a button: its word with the padding at its sides, and no less than the least.
     */
    float buttonWidth(const std::string_view label)
    {
        return (std::max)(BUTTON_MIN_WIDTH, std::ceil(ui::textWidth(label) + theme::SPACE_4 * 2.0f));
    }

    /**
     * What the dialog says: the mod that has the changes, how many, and what the player was about to do. With
     * an invalid one Save is disabled, so it asks to Discard them.
     */
    std::string questionText(const ui::UnsavedChanges& changes)
    {
        const bool one = changes.count == 1;
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
     * closed, as the design system's ConfirmDialog has it: a modal popup of ImGui in the middle of the panel,
     * which dims the panel behind it and takes every press. Its title, the question, and Cancel, Discard and
     * Save at its right, with Save disabled while a change is invalid, as the footer's is.
     * It is shown while there are changes to ask about, and closed with none, as after Back: ImGui opens and
     * closes a popup inside a frame. A pressed button closes it too. Returns the button that was pressed.
     */
    DialogAnswer drawUnsavedDialog(const UnsavedChanges* changes)
    {
        const auto id = ImGui::GetID(TITLE);
        if (changes && !ImGui::IsPopupOpen(id, ImGuiPopupFlags_None)) {
            ImGui::OpenPopupEx(id);
        }
        if (!ImGui::IsPopupOpen(id, ImGuiPopupFlags_None)) {
            return DialogAnswer::None;
        }

        ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(DIALOG_WIDTH, 0.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(theme::SPACE_6, theme::SPACE_6));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(theme::SPACE_2, theme::SPACE_4));
        ImGui::PushStyleColor(ImGuiCol_Border, theme::color::GREEN);
        const bool shown = ImGui::BeginPopupModal(TITLE,
            nullptr,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize);
        ImGui::PopStyleColor();
        ImGui::PopStyleVar(2);
        if (!shown) {
            return DialogAnswer::None;
        }
        if (!changes) {
            ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
            return DialogAnswer::None;
        }

        ImGui::PushFont(nullptr, theme::FONT_SIZE_TITLE);
        ImGui::TextUnformatted(TITLE);
        ImGui::PopFont();
        const auto question = questionText(*changes);
        ImGui::PushTextWrapPos(0.0f);
        ImGui::TextUnformatted(question.data(), question.data() + question.size());
        ImGui::PopTextWrapPos();

        // the buttons from the right, a little further from the question than it is from the title
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + theme::SPACE_2);
        const auto pos = ImGui::GetCursorScreenPos();
        float right = pos.x + ImGui::GetContentRegionAvail().x;
        const auto boxOf = [&right, &pos](const std::string_view label) {
            const float width = buttonWidth(label);
            const auto box = controlBox(right - width, pos.y, width);
            right -= width + theme::SPACE_2;
            return box;
        };
        const auto save = boxOf("Save");
        const auto discard = boxOf("Discard");
        const auto cancel = boxOf("Cancel");

        auto answer = DialogAnswer::None;
        ImGui::BeginDisabled(changes->invalid > 0);
        if (button("Save", save, ButtonLook::Primary)) {
            answer = DialogAnswer::Save;
        }
        ImGui::EndDisabled();
        if (button("Discard", discard, ButtonLook::Secondary)) {
            answer = DialogAnswer::Discard;
        }
        if (button("Cancel", cancel, ButtonLook::Quiet)) {
            answer = DialogAnswer::Cancel;
        }
        ImGui::SetCursorScreenPos(pos);
        ImGui::Dummy(ImVec2(ImGui::GetContentRegionAvail().x, theme::CONTROL_HEIGHT));

        if (answer != DialogAnswer::None) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
        return answer;
    }
}
