#include "Choices.h"

#include <algorithm>
#include <format>
#include <string>
#include <string_view>

#include <imgui.h>
#include <imgui_internal.h>

#include "Drawing.h"
#include "Haptics.h"
#include "Theme.h"
#include "model/FileChoices.h"
#include "model/Haptics.h"
#include "model/OptionSets.h"

namespace
{
    using namespace mods_config;

    /**
     * The box of a dropdown that ends at the right, which its list opens at: a haptic's is its frame, before its Play
     * button.
     */
    ImRect dropdownBox(const model::Setting& setting, const float right, const float top)
    {
        if (setting.type == model::SettingType::Haptic) {
            return ui::HapticBoxes(right, top).frame;
        }
        return controls::controlBox(right - ui::dropdownWidth(), top, ui::dropdownWidth());
    }

    /**
     * Whether an option of a choice of files is a file: the one that is no file stores an empty text, or the place 0 of
     * an MCM `dropdownFiles` whose storage keeps a place.
     */
    bool isFileOption(const model::Option& option)
    {
        if (const auto* text = std::get_if<std::string>(&option.value)) {
            return !text->empty();
        }
        const auto* place = std::get_if<int>(&option.value);
        return !place || *place != 0;
    }

    /**
     * The pattern of a haptic's option, and none for None and for a value that is no haptic.
     */
    std::optional<vrcf::HapticPattern> patternOf(const model::Value& value)
    {
        const auto* haptic = model::findHaptic(value);
        return haptic ? haptic->pattern : std::nullopt;
    }

    /**
     * Where a pattern's shape is in a box that ends with something of the given width at its right: in the middle of
     * its height, `space-3` before that.
     */
    ImVec2 shapeIn(const ImRect& box, const float after)
    {
        return { box.Max.x - after - look().space3 - ui::HAPTIC_SHAPE_WIDTH, box.GetCenter().y - ui::HAPTIC_SHAPE_HEIGHT * 0.5f };
    }

    /**
     * A haptic that ends at the right: its frame, as a dropdown's, with its text and the shape of its option's pattern
     * before the arrow, and its Play button after it. The text and the shape are in the color of its value's look, and
     * a value that is no option has its text alone. Play is disabled for None and for no option. Returns its left edge.
     */
    float drawHaptic(const ui::HapticBoxes& boxes, const std::string_view text, const model::Option* option, const ui::ValueLook look, const bool listOpen,
        const controls::ChoiceRow& row)
    {
        const auto& box = boxes.frame;
        const float arrowLeft = box.Max.x - ui::look().space4 - controls::ARROW_SIZE;
        const auto shape = shapeIn(box, ui::look().space4 + controls::ARROW_SIZE);
        controls::drawFrame(box, toneOf(look));
        if (listOpen) {
            controls::drawOutline(box.Min, box.Max, colors().accent);
        }
        controls::drawText(box.Min.x + ui::look().space4, shape.x - ui::look().space3, box.Min.y, ui::look().controlHeight, ui::valueColor(look), text);
        if (option && look != ui::ValueLook::Invalid) {
            ui::drawHapticShape(shape, patternOf(option->value), ImGui::GetColorU32(ui::valueColor(look)), ImGui::GetColorU32(colors().rule));
        }
        controls::drawControlArrow(ImVec2(arrowLeft + controls::ARROW_SIZE * 0.5f, box.GetCenter().y), controls::Arrow::Down);

        const bool plays = option && patternOf(option->value).has_value();
        const bool onPlay = boxes.onPlay(ImGui::GetIO().MousePos.x);
        ImGui::BeginDisabled(!plays);
        controls::drawButtonFrame(boxes.play, plays && row.pointed && onPlay, plays && row.held && onPlay);
        controls::drawIcon(controls::Icon::Vibrate, boxes.play.GetCenter(), colors().accent);
        ImGui::EndDisabled();
        return box.Min.x;
    }
}

namespace mods_config::ui
{
    /**
     * The place of the option a choice is at, among its options: the one whose value is the stored one, and
     * none where no option has it. A choice whose value is not available is at its first option.
     */
    std::optional<std::size_t> chosenOption(const std::vector<model::Option>& options, const model::StoredValue& stored)
    {
        if (!stored.available) {
            return options.empty() ? std::nullopt : std::optional<std::size_t>(0);
        }
        const auto option = std::ranges::find(options, stored.value, &model::Option::value);
        return option != options.end() ? std::optional(static_cast<std::size_t>(option - options.begin())) : std::nullopt;
    }

    /**
     * Starts a frame of the page, before its rows.
     */
    void Choices::startFrame()
    {
        _openSeen = false;
    }

    /**
     * The option the player picks in this frame for a stepper or a dropdown that ends at the right, by its
     * place among the options, and none where they pick none. The setting has an option at least.
     * A stepper is the framework's: its buttons go to the option before and after, a press elsewhere on its row to the
     * option after, as in MCM's menu, and the thumbstick steps it. With no option chosen, the one before is the last for a
     * choice that sets other settings.
     * A press on the row of a dropdown opens its list, which tells that the hand gets its pulse, and so does one on the
     * row of a haptic but on its Play button, which the row plays.
     */
    std::optional<std::size_t> Choices::edit(const model::Setting& setting, const std::vector<model::Option>& options, const float right, const float top,
        const controls::ChoiceRow& row, const std::optional<std::size_t> chosen, bool& pulse)
    {
        if (setting.type == model::SettingType::Haptic) {
            const bool onPlay = HapticBoxes(right, top).onPlay(ImGui::GetIO().MousePos.x);
            return editList(setting, options, right, top, { .pointed = row.pointed, .held = row.held, .pressed = row.pressed && !onPlay }, chosen, pulse);
        }
        if (setting.type == model::SettingType::Dropdown) {
            return editList(setting, options, right, top, row, chosen, pulse);
        }
        return controls::stepperEdit(right, top, row, chosen, options.size(), model::setsOthers(setting));
    }

    /**
     * The list of a dropdown: a press on the row opens it, at the box of the dropdown, as the framework's list. A press on
     * an option picks it and closes the list, and a press beside the list closes it and does nothing else: ImGui gives the
     * page under an open list no press. The list of a choice of files has its files' names with muted extensions under
     * its folder and how many files it has, the list of a haptic has each pattern's shape after its option's text, and the
     * list of a dropdown with a typed entry ends with Type your own.
     * Returns the option that was picked, the place after the options for Type your own, and none.
     */
    std::optional<std::size_t> Choices::editList(const model::Setting& setting, const std::vector<model::Option>& options, const float right, const float top,
        const controls::ChoiceRow& row, const std::optional<std::size_t> chosen, bool& pulse)
    {
        const auto id = ImGui::GetID("list");
        if (row.pressed) {
            ImGui::OpenPopupEx(id);
            pulse = true;
        }
        if (!ImGui::IsPopupOpen(id, ImGuiPopupFlags_None)) {
            return std::nullopt;
        }
        _open = &setting;
        _openSeen = true;

        std::vector<std::string_view> texts;
        texts.reserve(options.size());
        for (const auto& option : options) {
            texts.emplace_back(option.text);
        }
        // a choice of files has its files' names, with a muted extension, under its folder and how many files it has
        std::vector<std::string_view> extensions;
        std::string folder;
        std::string count;
        controls::ListExtras extras;
        if (const auto& files = setting.valueOptions.folderOptions) {
            std::size_t fileCount = 0;
            for (std::size_t i = 0; i < options.size(); ++i) {
                const auto shown = isFileOption(options[i]) ? model::shownFileName(options[i].text, files->mask) : model::ShownFileName{ .name = options[i].text };
                texts[i] = shown.name;
                extensions.push_back(shown.extension);
                fileCount += isFileOption(options[i]) ? 1 : 0;
            }
            folder = model::folderText(*files);
            count = std::format("{} file{}", fileCount, fileCount == 1 ? "" : "s");
            extras = { .suffixes = extensions, .headerLeft = folder, .headerRight = count, .action = {} };
        }
        if (setting.valueOptions.typedOption) {
            extras.action = TYPED_OPTION_TEXT;
        }
        controls::OptionDecoration decorate;
        if (setting.type == model::SettingType::Haptic) {
            decorate = [&options](const std::size_t option, const ImRect& box, const bool isChosen) {
                // on the chosen option's solid fill, the shape is in the color on green, over its base at 40%
                const auto color = isChosen ? colors().onAccent : colors().accent;
                const auto base = isChosen ? (colors().onAccent & ~IM_COL32_A_MASK) | (colors().rule & IM_COL32_A_MASK) : colors().rule;
                drawHapticShape(shapeIn(box, look().space4 - look().space3), patternOf(options[option].value), ImGui::GetColorU32(color), ImGui::GetColorU32(base));
            };
        }
        const auto drawn = controls::drawList(id, dropdownBox(setting, right, top), texts, chosen, decorate, extras);
        _pointedOption = drawn.pointed;
        return drawn.picked;
    }

    /**
     * A stepper or a dropdown that ends at the right, with the text of the option it is at in the color of
     * its value's look, and none where it is at no option. Returns its left edge.
     * A stepper and a dropdown are the framework's: the box of a dropdown has its outline in the full color while its list
     * is open. A haptic is a dropdown with its pattern's shape before the arrow, and a Play button after its box. A value
     * that is at no option shows the text given for it, as a haptic shows a name the framework does not have. A choice of
     * files shows a file's name with its extension muted, or none where its mask names it (`shownFileName`).
     */
    float Choices::draw(const model::Setting& setting, const std::vector<model::Option>& options, const float right, const float top, const controls::ChoiceRow& row,
        const std::optional<std::size_t> chosen, const ValueLook look, const std::string_view unchosenText) const
    {
        const auto option = chosen ? std::string_view(options[*chosen].text) : unchosenText;

        if (setting.type == model::SettingType::Haptic) {
            return drawHaptic(HapticBoxes(right, top), option, chosen ? &options[*chosen] : nullptr, look, isListOpen(setting), row);
        }
        if (setting.type == model::SettingType::Dropdown) {
            const auto box = dropdownBox(setting, right, top);
            // a file has its name with a muted extension, and the option that is no file its text
            const auto& files = setting.valueOptions.folderOptions;
            const auto shown = files && (!chosen || isFileOption(options[*chosen])) ? model::shownFileName(option, files->mask) : model::ShownFileName{ .name = option };
            controls::drawDropdown(box, shown.name, shown.extension, toneOf(look), isListOpen(setting));
            return box.Min.x;
        }
        return controls::drawStepper(right, top, row, option, toneOf(look), chosen, options.size(), model::setsOthers(setting));
    }

    /**
     * Ends a frame of the page, after its rows: a dropdown whose list was not open in it has none.
     */
    void Choices::endFrame()
    {
        if (!_openSeen) {
            _open = nullptr;
            _pointedOption.reset();
        }
    }

    bool Choices::isListOpen(const model::Setting& setting) const
    {
        return _open == &setting;
    }

    /**
     * Forgets the dropdown whose list is open, as another page is shown: it is a row of the page before. The page closes
     * its list (`controls::Controls::closePopups`).
     */
    void Choices::reset()
    {
        _open = nullptr;
        _openSeen = false;
        _pointedOption.reset();
    }
}
