#include "ModPage.h"

#include <algorithm>
#include <cmath>
#include <format>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include <imgui.h>
#include <imgui_internal.h>

#include "Choices.h"
#include "Drawing.h"
#include "PageRows.h"
#include "Sections.h"
#include "SettingRow.h"
#include "Sliders.h"
#include "Theme.h"
#include "model/Conditions.h"

namespace
{
    using namespace mods_config;

    /**
     * A text row at the cursor: its lines in the muted color, in from the edges of the rows and wrapped
     * between them.
     */
    void drawTextRow(const model::Text& text, const ui::Gutter& gutter)
    {
        const auto pos = ImGui::GetCursorScreenPos();
        const float width = ui::rowWidth();
        const float height = ui::drawTextLines(text.lines, ImVec2(pos.x + gutter.inset, pos.y), width - gutter.inset - theme::SPACE_2, theme::color::GREEN_MUTED);
        ImGui::Dummy(ImVec2(width, height));
    }

    /**
     * A row at the cursor that has no control, as its text and what it is. It is laid out as a setting's row:
     * the text after the gutter, and the type where the control ends, before the room of its Reset.
     * A row with a help is disabled, with the look of a row under the ray, and none of a press, which does nothing.
     * Returns whether the ray is on the row.
     */
    bool drawRow(const std::string_view text, const std::span<const model::TextRun> runs, const std::string_view type, const bool hasHelp, const ui::Gutter& gutter)
    {
        const auto pos = ImGui::GetCursorScreenPos();
        const float width = ui::rowWidth();
        const auto row = ui::addRow(false);
        if (!row.shown) {
            return false;
        }
        if (hasHelp) {
            ui::drawRowState();
        }

        ImGui::BeginDisabled(hasHelp);
        const float typeLeft = pos.x + width - theme::CONTROL_HEIGHT - theme::SPACE_2 - ui::textWidth(type);
        ui::drawText(typeLeft, pos.x + width, pos.y, theme::CONTROL_HEIGHT, theme::color::GREEN_MUTED, type);
        ui::drawLabel(pos, gutter, typeLeft - theme::SPACE_3, text, runs);
        ImGui::EndDisabled();
        return row.pointed;
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
        ui::TextInputs& textInputs;

        ui::Sections& sections;
        ui::PageDrawn& drawn;
    };

    // the button of a button's row, in the panel's pixels: as wide as the Open of the menu's front page
    constexpr float BUTTON_WIDTH = 160.0f;

    /**
     * The row of a button at the cursor: its text after the gutter, and its button where a control ends,
     * before the room of a Reset, with its own text or `Run`. The whole row is pressed, so the button is
     * only drawn, and a press runs the action in the store. A button with no action is disabled, and so is
     * one whose action the menu does not run, which the reason says, and one whose enabledWhen does not hold.
     * A button whose warning holds has the warning mark after its text.
     * Returns whether the ray is on the row, and tells the reason and the warning.
     */
    bool drawButton(const model::Button& button, const PageDrawing& page, const ui::Gutter& gutter, std::string& reason, std::string_view& warning)
    {
        const auto pos = ImGui::GetCursorScreenPos();
        const float width = ui::rowWidth();
        const bool enabled = !button.enabledWhen || model::holds(*button.enabledWhen, page.store);
        const auto row = ui::addRow(enabled);
        if (!enabled && row.shown) {
            ui::drawRowState();
        }

        const auto* unknown = std::get_if<model::UnknownAction>(&button.action);
        const bool runs = page.store && enabled && !unknown && !std::holds_alternative<model::NoAction>(button.action);
        if (runs && row.pressed && page.store->run(button.action)) {
            page.drawn.pressActed = true;
        }
        if (unknown) {
            reason = unknown->reason;
        }
        if (!row.shown) {
            return false;
        }
        warning = ui::warningOf(button.warnings, page.store);

        ImGui::BeginDisabled(!runs);
        const auto box = ui::controlBox(pos.x + width - theme::CONTROL_HEIGHT - theme::SPACE_2 - BUTTON_WIDTH, pos.y, BUTTON_WIDTH);
        ui::drawOutline(box.Min, box.Max, theme::color::FRAME_BORDER);
        ui::drawTextIn(box, button.buttonText.empty() ? std::string_view("Run") : std::string_view(button.buttonText), theme::color::GREEN, 0.5f, theme::SPACE_3);
        ui::drawLabel(pos, gutter, box.Min.x - theme::SPACE_3, button.text, button.textRuns, !warning.empty());
        ImGui::EndDisabled();
        return row.pointed;
    }

    // the least width of the box of a hotkey's key, in the panel's pixels: not a token of the design system
    constexpr float KEY_MIN_WIDTH = 96.0f;

    /**
     * The row of a hotkey at the cursor: its text after the gutter, and the key it is bound to in a box as
     * wide as its name, where a control ends, before the room of a Reset. A hotkey with no key says `None`.
     * The key is only shown, so the whole row is always disabled, as a row that cannot be edited is. It has
     * the look of a row under the ray, for its help, but none of a press, which does nothing.
     * A hotkey that is not available looks the same, and the reason tells it from the others.
     * Returns whether the ray is on the row.
     */
    bool drawHotkey(const model::Hotkey& hotkey, const PageDrawing& page, const ui::Gutter& gutter, std::string& reason)
    {
        const auto pos = ImGui::GetCursorScreenPos();
        const float width = ui::rowWidth();
        const auto row = ui::addRow(false);

        auto stored = page.store ? page.store->hotkey(hotkey) : model::StoredValue{};
        reason = std::move(stored.reason);
        if (!row.shown) {
            return false;
        }
        ui::drawRowState();
        const auto* key = std::get_if<std::string>(&stored.value);
        const std::string_view name = key ? std::string_view(*key) : "None";
        const float boxWidth = (std::max)(KEY_MIN_WIDTH, std::ceil(ui::textWidth(name)) + theme::SPACE_3 * 2.0f);
        const auto box = ui::controlBox(pos.x + width - theme::CONTROL_HEIGHT - theme::SPACE_2 - boxWidth, pos.y, boxWidth);

        ImGui::BeginDisabled();
        ui::drawLabel(pos, gutter, box.Min.x - theme::SPACE_3, hotkey.text, hotkey.textRuns);
        ui::drawFrame(box);
        ui::drawTextIn(box, name, theme::color::GREEN, 0.5f);
        ImGui::EndDisabled();
        return row.pointed;
    }

    void drawItems(const std::vector<model::Item>& items, const PageDrawing& page, int depth = 0);

    /**
     * The heading of a section that folds, at the cursor: a row that a press folds or opens, which the
     * sections draw. It tells the help of the section and what it hides while the ray is on it.
     * Returns whether the section is folded, after a press in this frame.
     */
    bool drawFoldingHeading(const model::Group& group, const PageDrawing& page)
    {
        const auto pos = ImGui::GetCursorScreenPos();
        const float width = ui::rowWidth();
        const auto row = ui::addRow(true);
        std::string help;
        const bool folded =
            page.sections.drawHeading(group, page.store, pos, width, { .shown = row.shown, .pointed = row.pointed, .held = row.held, .pressed = row.pressed }, help);
        page.drawn.pressActed = page.drawn.pressActed || row.pressed;
        if (row.pointed) {
            page.drawn.pointed = ui::PointedRow{ .text = group.text, .help = std::move(help) };
        }
        return folded;
    }

    // how many levels a section in a section is set in at the most, so a name keeps its room
    constexpr int MAX_SET_IN = 3;

    /**
     * A section at the cursor: its heading, where it is shown, and the rows of its items under it. A section in a
     * section is set in with its rows, by the step a page is set in under its mod in the tree, up to the third
     * level, and while it is open and has rows its line runs down their left, so its rows are seen to be its and
     * not the outer section's. The depth is how many sections it is in.
     * A section that folds has a heading that a press folds or opens, and its rows are drawn while it is open.
     * One whose heading is not shown keeps its fold, and has no line. The heading of any other tells its help
     * while the ray is on it.
     */
    void drawGroup(const model::Group& group, const PageDrawing& page, const int depth, const bool headingShown)
    {
        const bool setIn = depth > 0 && depth <= MAX_SET_IN;
        if (setIn) {
            ImGui::Indent();
        }
        const float lineLeft = ImGui::GetCursorScreenPos().x + theme::SPACE_2;
        bool folded = false;
        if (group.collapsible) {
            folded = headingShown ? drawFoldingHeading(group, page) : page.sections.isFolded(group);
        } else if (headingShown) {
            ui::sectionHeading(group.text, ImGui::GetCursorScreenPos().x + ui::rowWidth());
            if (!group.help.empty() && ImGui::IsItemHovered()) {
                page.drawn.pointed = ui::PointedRow{ .text = group.text, .help = group.help };
            }
        }
        if (!folded) {
            const float rowsTop = ImGui::GetCursorScreenPos().y;
            const float lineTop = headingShown ? ImGui::GetItemRectMax().y : rowsTop;
            drawItems(group.items, page, depth + 1);
            const float rowsEnd = ImGui::GetCursorScreenPos().y;
            if (depth > 0 && headingShown && rowsEnd > rowsTop) {
                ui::drawSectionLine(lineLeft, lineTop, rowsEnd - ImGui::GetStyle().ItemSpacing.y);
            }
        }
        if (setIn) {
            ImGui::Unindent();
        }
    }

    /**
     * Draws an item as its row. A section is its heading, with the rows of its items under it while it is
     * not folded, and a spacer
     * is empty space as high as its lines of text. An image that could not be read is a row that says an
     * image is there.
     * A row with a text that the ray is on is told as the pointed one. A setting whose value is not
     * available has the reason as its help, where the store gives one, and so has a button whose action the
     * menu does not run, and a hotkey that is not available. Any other hotkey has that it is read-only
     * before its help. A control the menu does not have says so as its help. A setting and a button that
     * warn have the warning before their help.
     */
    struct ItemDrawer
    {
        const PageDrawing& page;

        // how many sections the item is in
        int depth = 0;

        void operator()(const model::Setting& setting) const
        {
            auto drawn = ui::drawSettingRow(setting, { .store = page.store, .sliders = page.sliders, .choices = page.choices, .textInputs = page.textInputs }, ui::gutterAt(depth));
            if (drawn.pointed) {
                page.drawn.pointed = std::move(drawn.pointed);
            }
            page.drawn.pressActed = page.drawn.pressActed || drawn.pressActed;
        }

        void operator()(const model::Group& group) const
        {
            drawGroup(group, page, depth, true);
        }

        void operator()(const model::Button& button) const
        {
            std::string reason;
            std::string_view warning;
            if (drawButton(button, page, ui::gutterAt(depth), reason, warning)) {
                page.drawn.pointed = ui::PointedRow{ .text = button.text, .help = reason.empty() ? button.help : std::move(reason), .warning = warning };
            }
        }

        void operator()(const model::Hotkey& hotkey) const
        {
            std::string reason;
            if (drawHotkey(hotkey, page, ui::gutterAt(depth), reason)) {
                page.drawn.pointed = ui::PointedRow{ .text = hotkey.text, .help = ui::readOnlyHelp(std::move(reason), hotkey.help) };
            }
        }

        void operator()(const model::Text& text) const
        {
            drawTextRow(text, ui::gutterAt(depth));
        }

        void operator()(const model::Image& image) const
        {
            if (image.pixels) {
                ui::drawImage(image, page.imageDrawer, ui::rowWidth(), ui::gutterAt(depth).inset);
            } else if (!image.texture.empty()) {
                ui::drawTexture(image, page.imageDrawer, ui::rowWidth(), ui::gutterAt(depth).inset);
            } else {
                drawRow({}, {}, "image", false, ui::gutterAt(depth));
            }
        }

        void operator()(const model::Spacer& spacer) const
        {
            ImGui::Dummy(ImVec2(0.0f, ImGui::GetFontSize() * static_cast<float>(spacer.lines)));
        }

        void operator()(const model::Placeholder& placeholder) const
        {
            const bool typed = !placeholder.type.empty();
            if (drawRow(placeholder.text, placeholder.textRuns, typed ? std::string_view(placeholder.type) : "unknown", true, ui::gutterAt(depth))) {
                auto help = typed ? std::format("Not available: the menu has no {} control.", placeholder.type) : std::string("Not available: the row has no type of control.");
                page.drawn.pointed = ui::PointedRow{ .text = placeholder.text, .help = std::move(help) };
            }
        }
    };

    /**
     * Whether an item is shown: one with a condition is, while it holds on the values of the store. With no
     * store no value is available, so a condition on a value holds, and one that is settled without a value,
     * as a package's condition on a plugin, is asked as any other.
     */
    bool isShown(const model::Item& item, model::Store* store)
    {
        const auto& visibleWhen = std::visit(
            [](const auto& row) -> const std::optional<model::Condition>& {
                return row.visibleWhen;
            },
            item);
        return !visibleWhen || model::holds(*visibleWhen, store);
    }

    /**
     * Draws the items that are shown, each as its row. An item that is not shown has no row and no room.
     * A section that is not shown is without its heading alone: its items are shown by their own conditions.
     * The depth is how many sections the items are in.
     */
    void drawItems(const std::vector<model::Item>& items, const PageDrawing& page, const int depth)
    {
        for (const auto& item : items) {
            // a row is an item of ImGui by the name of its kind: its place in the page tells it from the others
            ImGui::PushID(&item);
            if (isShown(item, page.store)) {
                std::visit(ItemDrawer{ .page = page, .depth = depth }, item);
            } else if (const auto* group = std::get_if<model::Group>(&item)) {
                drawGroup(*group, page, depth, false);
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
     * that has no text of its own, as a text row, an image and a spacer, or on the heading of a section with
     * no help that does not fold. While the list of a dropdown is open it is the row of that dropdown. And
     * whether a press did something, or the thumbstick stepped a slider or a stepper.
     */
    PageDrawn ModPage::draw(const model::Page& page, model::Store* store, const float thumbstickSideValue)
    {
        PageDrawn drawn;
        _sliders.startFrame(thumbstickSideValue);
        _choices.startFrame(thumbstickSideValue);
        drawItems(page.items,
            PageDrawing{ .imageDrawer = _imageDrawer, .store = store, .sliders = _sliders, .choices = _choices, .textInputs = _textInputs, .sections = _sections, .drawn = drawn });
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
     * Ends the edit of a text input with nothing written, for Back.
     * Returns false while none is edited.
     */
    bool ModPage::cancelEdit()
    {
        return _textInputs.cancelEdit();
    }

    /**
     * Forgets what is edited in the page, as another page is shown, and how its sections were folded: each
     * starts as its `collapsed` says each time its page is shown.
     */
    void ModPage::reset()
    {
        _sliders.reset();
        _choices.reset();
        _textInputs.reset();
        _sections.reset();
    }
}
