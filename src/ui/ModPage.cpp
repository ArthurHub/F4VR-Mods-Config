#include "ModPage.h"

#include <algorithm>
#include <cmath>
#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include <imgui.h>
#include <imgui_internal.h>

#include "Choices.h"
#include "Drawing.h"
#include "Sliders.h"
#include "Theme.h"
#include "model/Conditions.h"

namespace
{
    using namespace mods_config;

    /**
     * The width of a row: the same whether or not the page's scroll bar shows.
     */
    float rowWidth()
    {
        return ImGui::GetWindowWidth() - theme::SCROLLBAR_SIZE - theme::SPACE_2;
    }

    /**
     * A text row at the cursor: its lines in the muted color, in from the edges of the rows and wrapped
     * between them.
     */
    void drawTextRow(const model::Text& text)
    {
        const auto pos = ImGui::GetCursorScreenPos();
        const float width = rowWidth();
        const float height = ui::drawTextLines(text.lines, ImVec2(pos.x + theme::SPACE_2, pos.y), width - theme::SPACE_2 * 2.0f, theme::color::GREEN_MUTED);
        ImGui::Dummy(ImVec2(width, height));
    }

    /**
     * An image at the cursor, in the middle of the rows' width. It is as large as in MCM's menu beside the
     * rows' text, and smaller where the rows are not as wide. The row is as high as the image.
     */
    void drawImage(const model::Image& image, const ui::ImageDrawer& drawer)
    {
        const auto pos = ImGui::GetCursorScreenPos();
        const float width = rowWidth();
        const float scale = (std::min)(ui::mcmScale(), (width - theme::SPACE_2 * 2.0f) / image.width);
        const ImVec2 size(std::round(image.width * scale), std::round(image.height * scale));
        ImGui::Dummy(ImVec2(width, size.y));
        if (drawer && ImGui::IsItemVisible()) {
            const ImVec2 min(pos.x + std::floor((width - size.x) * 0.5f), pos.y);
            drawer(*image.pixels, min, ImVec2(min.x + size.x, min.y + size.y));
        }
    }

    /**
     * What a row that was added tells.
     */
    struct Row
    {
        // false for a row that is scrolled out of the page: nothing of it is drawn
        bool shown = false;

        // the ray is on it
        bool pointed = false;

        // the trigger was pulled on it in this frame, is held since a pull on it, and was let go on it
        bool started = false;
        bool held = false;
        bool pressed = false;
    };

    /**
     * Adds a row at the cursor, as wide as the rows and as high as a control. A row that is pressed has the
     * look of a row under the ray, and a stronger one while the trigger is held. Any other is only its room.
     */
    Row addRow(const bool pressable)
    {
        const ImVec2 size(rowWidth(), theme::CONTROL_HEIGHT);
        Row row;
        if (pressable) {
            row.pressed = ImGui::InvisibleButton("row", size);
            row.started = ImGui::IsItemActivated();
            row.held = ImGui::IsItemActive();
        } else {
            ImGui::Dummy(size);
        }
        row.shown = ImGui::IsItemVisible();
        if (row.shown) {
            row.pointed = ImGui::IsItemHovered();
            if (pressable) {
                ui::drawRowState();
            }
        }
        return row;
    }

    /**
     * A row at the cursor, as its text and what it is. It is laid out as a setting's row: the text after the
     * gutter, and the type where the control ends, before the room of its Reset.
     * Returns whether the ray is on the row.
     */
    bool drawRow(const std::string_view text, const std::string_view type, const bool pressable = true)
    {
        const auto pos = ImGui::GetCursorScreenPos();
        const float width = rowWidth();
        const auto row = addRow(pressable);
        if (!row.shown) {
            return false;
        }

        const float typeLeft = pos.x + width - theme::CONTROL_HEIGHT - theme::SPACE_2 - ui::textWidth(type);
        ui::drawText(typeLeft, pos.x + width, pos.y, theme::CONTROL_HEIGHT, theme::color::GREEN_MUTED, type);
        ui::drawText(pos.x + theme::SPACE_6 + theme::SPACE_3, typeLeft - theme::SPACE_3, pos.y, theme::CONTROL_HEIGHT, theme::color::GREEN, text);
        return row.pointed;
    }

    // a cell of a switcher, which has two, in the panel's pixels: not a token of the design system
    constexpr float TOGGLE_CELL_WIDTH = 88.0f;

    /**
     * A switcher that ends at the right: an Off and an On cell, with the one of its value marked. On is a
     * solid fill, and Off a ring, so a solid fill always says on. Returns its left edge.
     */
    float drawSwitcher(const float right, const float top, const bool on)
    {
        const auto box = ui::controlBox(right - TOGGLE_CELL_WIDTH * 2.0f, top, TOGGLE_CELL_WIDTH * 2.0f);
        const auto offCell = ui::controlBox(box.Min.x, top, TOGGLE_CELL_WIDTH);
        const auto onCell = ui::controlBox(offCell.Max.x, top, TOGGLE_CELL_WIDTH);
        ui::drawFrame(box);
        if (on) {
            ImGui::GetWindowDrawList()->AddRectFilled(onCell.Min, onCell.Max, ImGui::GetColorU32(theme::color::GREEN), theme::RADIUS_FRAME, ImDrawFlags_RoundCornersRight);
        } else {
            ImGui::GetWindowDrawList()->AddRectFilled(offCell.Min, offCell.Max, ImGui::GetColorU32(theme::color::PRESSED), theme::RADIUS_FRAME, ImDrawFlags_RoundCornersLeft);
            ui::drawOutline(offCell.Min, offCell.Max, theme::color::GREEN);
        }
        ui::drawTextIn(offCell, "Off", on ? theme::color::GREEN_MUTED : theme::color::GREEN, 0.5f);
        ui::drawTextIn(onCell, "On", on ? theme::color::ON_GREEN : theme::color::GREEN_MUTED, 0.5f);
        return box.Min.x;
    }

    /**
     * What the rows of a page are drawn with, and what their drawing tells.
     */
    struct PageDrawing
    {
        const ui::ImageDrawer& imageDrawer;
        model::Store* store;
        ui::Sliders& sliders;
        ui::Choices& choices;
        ui::PageDrawn& drawn;
    };

    /**
     * The row of a setting at the cursor: its text after the gutter, and its control where the control column
     * ends, before the room of its Reset. The text has the room up to its own control.
     * The control shows the value the store has for the setting. A press anywhere on the row of a switcher
     * turns it on or off, and a slider is dragged by its track, which is written as the trigger is let go,
     * and stepped by the thumbstick. A stepper
     * goes to the option before and after, and a dropdown opens its list. A control whose value is not
     * available is disabled, and so is a slider with no range and a stepper and a dropdown with no options.
     * The row itself has the look of a row under the ray, as it has a help.
     * A text input is its text and its type until the commit that draws it.
     * Returns whether the ray is on the row or on its open list, and tells why a value is not available.
     */
    bool drawSetting(const model::Setting& setting, const PageDrawing& page, std::string& reason)
    {
        if (setting.type == model::SettingType::TextInput) {
            return drawRow(setting.text, "textinput");
        }
        const auto pos = ImGui::GetCursorScreenPos();
        const float width = rowWidth();
        const auto row = addRow(true);

        auto stored = page.store ? page.store->value(setting) : model::StoredValue{};
        const auto& options = setting.valueOptions;
        const auto* on = std::get_if<bool>(&stored.value);
        const auto* number = std::get_if<float>(&stored.value);
        const bool isSwitcher = stored.available && setting.type == model::SettingType::Switcher && on;
        const bool isSlider = stored.available && setting.type == model::SettingType::Slider && number && options.max > options.min;
        const bool isChoice = stored.available && (setting.type == model::SettingType::Stepper || setting.type == model::SettingType::Dropdown) && !options.options.empty();
        const float right = pos.x + width - theme::CONTROL_HEIGHT - theme::SPACE_2;

        // before the row is known to be shown: a slider that is dragged out of the page is still held
        float value = number ? *number : options.min;
        if (isSlider) {
            bool write = false;
            bool pulse = false;
            const ui::SliderRow sliderRow{ .pointed = row.pointed, .started = row.started, .held = row.held };
            const float edited = page.sliders.edit(setting, right, pos.y, sliderRow, value, write, pulse);
            if (!write || page.store->set(setting, edited)) {
                value = edited;
            }
            page.drawn.pressActed = page.drawn.pressActed || pulse;
        }
        if (isSwitcher && row.pressed && page.store->set(setting, !*on)) {
            page.drawn.pressActed = true;
        }
        // before the row is known to be shown too: the list of a dropdown is not a part of its row
        auto chosen = ui::chosenOption(setting, stored);
        const ui::ChoiceRow choiceRow{ .pointed = isChoice && row.pointed, .held = isChoice && row.held, .pressed = isChoice && row.pressed };
        if (isChoice) {
            bool pulse = false;
            const auto picked = page.choices.edit(setting, right, pos.y, choiceRow, chosen, pulse);
            if (picked && picked != chosen && page.store->set(setting, options.options[*picked].value)) {
                chosen = picked;
                pulse = true;
            }
            page.drawn.pressActed = page.drawn.pressActed || pulse;
        }
        // the ray is on the list then, and the help line keeps the row of its dropdown
        const bool listOpen = page.choices.isListOpen(setting);
        reason = std::move(stored.reason);
        if (!row.shown) {
            return listOpen;
        }

        float left = right;
        ImGui::BeginDisabled(!isSwitcher && !isSlider && !isChoice);
        switch (setting.type) {
        case model::SettingType::Switcher:
            left = drawSwitcher(right, pos.y, on && *on);
            break;
        case model::SettingType::Slider:
            left = page.sliders.draw(setting, right, pos.y, value);
            break;
        case model::SettingType::Stepper:
        case model::SettingType::Dropdown:
            left = page.choices.draw(setting, right, pos.y, choiceRow, chosen);
            break;
        case model::SettingType::TextInput:
            break;
        }
        ui::drawText(pos.x + theme::SPACE_6 + theme::SPACE_3, left - theme::SPACE_3, pos.y, theme::CONTROL_HEIGHT, theme::color::GREEN, setting.text);
        ImGui::EndDisabled();
        return row.pointed || listOpen;
    }

    // the button of a button's row, in the panel's pixels: as wide as the Open of the menu's front page
    constexpr float BUTTON_WIDTH = 160.0f;

    /**
     * The row of a button at the cursor: its text after the gutter, and a Run button where a control ends,
     * before the room of a Reset. The whole row is pressed, so the button is only drawn, and a press runs
     * the action in the store. A button with no action is disabled, and so is one whose action the menu
     * does not run, which the reason says.
     * Returns whether the ray is on the row.
     */
    bool drawButton(const model::Button& button, const PageDrawing& page, std::string& reason)
    {
        const auto pos = ImGui::GetCursorScreenPos();
        const float width = rowWidth();
        const auto row = addRow(true);

        const auto* unknown = std::get_if<model::UnknownAction>(&button.action);
        const bool runs = page.store && !unknown && !std::holds_alternative<model::NoAction>(button.action);
        if (runs && row.pressed && page.store->run(button.action)) {
            page.drawn.pressActed = true;
        }
        if (unknown) {
            reason = std::format("Not available: the menu does not run a {}.", unknown->type.empty() ? "button of this kind" : unknown->type);
        }
        if (!row.shown) {
            return false;
        }

        ImGui::BeginDisabled(!runs);
        const auto box = ui::controlBox(pos.x + width - theme::CONTROL_HEIGHT - theme::SPACE_2 - BUTTON_WIDTH, pos.y, BUTTON_WIDTH);
        ui::drawOutline(box.Min, box.Max, theme::color::FRAME_BORDER);
        ui::drawTextIn(box, "Run", theme::color::GREEN, 0.5f);
        ui::drawText(pos.x + theme::SPACE_6 + theme::SPACE_3, box.Min.x - theme::SPACE_3, pos.y, theme::CONTROL_HEIGHT, theme::color::GREEN, button.text);
        ImGui::EndDisabled();
        return row.pointed;
    }

    void drawItems(const std::vector<model::Item>& items, const PageDrawing& page);

    /**
     * Draws an item as its row. A section is its heading, with the rows of its items under it, and a spacer
     * is empty space as high as its lines of text. An image that could not be read is a row that says an
     * image is there.
     * A row with a text that the ray is on is told as the pointed one. A setting whose value is not
     * available has the reason as its help, where the store gives one, and so has a button whose action the
     * menu does not run.
     */
    struct ItemDrawer
    {
        const PageDrawing& page;

        void operator()(const model::Setting& setting) const
        {
            std::string reason;
            if (drawSetting(setting, page, reason)) {
                page.drawn.pointed = ui::PointedRow{ .text = setting.text, .help = reason.empty() ? setting.help : std::move(reason) };
            }
        }

        void operator()(const model::Group& group) const
        {
            ui::sectionHeading(group.text);
            drawItems(group.items, page);
        }

        void operator()(const model::Button& button) const
        {
            std::string reason;
            if (drawButton(button, page, reason)) {
                page.drawn.pointed = ui::PointedRow{ .text = button.text, .help = reason.empty() ? button.help : std::move(reason) };
            }
        }

        void operator()(const model::Text& text) const
        {
            drawTextRow(text);
        }

        void operator()(const model::Image& image) const
        {
            if (image.pixels) {
                drawImage(image, page.imageDrawer);
            } else {
                drawRow({}, "image", false);
            }
        }

        void operator()(const model::Spacer& spacer) const
        {
            ImGui::Dummy(ImVec2(0.0f, ImGui::GetFontSize() * static_cast<float>(spacer.lines)));
        }

        void operator()(const model::Placeholder& placeholder) const
        {
            if (drawRow(placeholder.text, placeholder.type.empty() ? "unknown" : std::string_view(placeholder.type))) {
                page.drawn.pointed = ui::PointedRow{ .text = placeholder.text, .help = placeholder.help };
            }
        }
    };

    /**
     * Whether an item is shown: one with a condition is, while it holds on the values of the store. With no
     * store every item is shown.
     */
    bool isShown(const model::Item& item, model::Store* store)
    {
        const auto& visibleWhen = std::visit(
            [](const auto& row) -> const std::optional<model::Condition>& {
                return row.visibleWhen;
            },
            item);
        return !visibleWhen || !store || model::holds(*visibleWhen, *store);
    }

    /**
     * Draws the items that are shown, each as its row. An item that is not shown has no row and no room.
     * A section that is not shown is without its heading alone: its items are shown by their own conditions.
     */
    void drawItems(const std::vector<model::Item>& items, const PageDrawing& page)
    {
        for (const auto& item : items) {
            // a row is an item of ImGui by the name of its kind: its place in the page tells it from the others
            ImGui::PushID(&item);
            if (isShown(item, page.store)) {
                std::visit(ItemDrawer{ page }, item);
            } else if (const auto* group = std::get_if<model::Group>(&item)) {
                drawItems(group->items, page);
            }
            ImGui::PopID();
        }
    }
}

namespace mods_config::ui
{
    void ModPage::setImageDrawer(ImageDrawer drawer)
    {
        _imageDrawer = std::move(drawer);
    }

    /**
     * Draws the rows of a page into the current ImGui window, which scrolls them, with the values the store
     * has for its settings. With no store no value is available. The thumbstick's value is how far the one
     * of the hand that points is pushed to a side, from -1 at the left to 1 at the right: it steps the
     * slider or the stepper whose row the ray is on.
     * Returns the row the ray is on, for the help line: none in the gap between two rows, and none on a row
     * that has no text of its own, as a section, a text row, an image and a spacer. While the list of a
     * dropdown is open it is the row of that dropdown. And whether a press did something, or the thumbstick
     * stepped a slider or a stepper.
     */
    PageDrawn ModPage::draw(const model::Page& page, model::Store* store, const float thumbstickSideValue)
    {
        PageDrawn drawn;
        _sliders.startFrame(thumbstickSideValue);
        _choices.startFrame(thumbstickSideValue);
        drawItems(page.items, PageDrawing{ .imageDrawer = _imageDrawer, .store = store, .sliders = _sliders, .choices = _choices, .drawn = drawn });
        _sliders.endFrame();
        _choices.endFrame();
        return drawn;
    }

    /**
     * Ends the drag of a slider with nothing written, for Back.
     * Returns false while no slider is dragged.
     */
    bool ModPage::cancelDrag()
    {
        return _sliders.cancelDrag();
    }

    /**
     * Closes the list of a dropdown that is open, with no option picked, for Back.
     * Returns false while none is open.
     */
    bool ModPage::closeList()
    {
        return _choices.closeList();
    }

    /**
     * Forgets what is edited in the page, as another page is shown.
     */
    void ModPage::reset()
    {
        _sliders.reset();
        _choices.reset();
    }
}
