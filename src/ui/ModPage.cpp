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

#include "Buttons.h"
#include "Choices.h"
#include "Drawing.h"
#include "PageRows.h"
#include "Sections.h"
#include "SettingRow.h"
#include "Sliders.h"
#include "Theme.h"
#include "model/Conditions.h"
#include "model/TextStates.h"

namespace
{
    using namespace mods_config;

    /**
     * A text row at the cursor: its lines in the muted color, under the rows' names, and wrapped to the end of
     * the rows.
     */
    void drawTextRow(const model::Text& text, model::Store* store)
    {
        const auto pos = ImGui::GetCursorScreenPos();
        const float width = ui::rowWidth();
        const auto resolved = model::resolveClientText(text.lines, store);
        const float height =
            ui::drawTextLines(resolved ? *resolved : text.lines, ImVec2(pos.x + theme::SPACE_3, pos.y), width - theme::SPACE_3 - theme::SPACE_2, theme::color::GREEN_MUTED);
        ImGui::Dummy(ImVec2(width, height));
    }

    /**
     * A row at the cursor that has no control, as its text and what it is. It is laid out as a setting's row:
     * the text, and the type where the control ends, before the room of its Reset.
     * A row with a help is disabled, with the look of a row under the ray, and none of a press, which does nothing.
     * Returns whether the ray is on the row.
     */
    bool drawRow(const std::string_view text, const std::span<const model::TextRun> runs, const std::string_view type, const bool hasHelp)
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
        ui::drawLabel(pos, typeLeft - theme::SPACE_3, text, runs);
        ImGui::EndDisabled();
        return row.pointed;
    }

    /**
     * What the rows of a page are drawn with, and what their drawing tells.
     */
    struct PageDrawing
    {
        const ui::ImageDrawer& imageDrawer;

        // the player's handedness, which says what a button of a hand is printed as
        bool leftHanded = false;

        // the binding settings of the mod, which a binding that is the same press as one of them names
        std::span<const model::Setting* const> bindings;
        model::Store* store;
        ui::Sliders& sliders;
        ui::Choices& choices;
        ui::TextInputs& textInputs;

        ui::Sections& sections;
        ui::Buttons& buttons;
        ui::PageDrawn& drawn;

        // the item that has the focus ring, and none, and whether the page is scrolled to it in this frame
        const model::Item* focused;
        bool& scrollToFocused;
    };

    // the least width of the box of a hotkey's key, in the panel's pixels: not a token of the design system
    constexpr float KEY_MIN_WIDTH = 96.0f;

    /**
     * The row of a hotkey at the cursor: its text, and the key it is bound to in a box as
     * wide as its name, where a control ends, before the room of a Reset. A hotkey with no key says `None`.
     * The key is only shown, so the whole row is always disabled, as a row that cannot be edited is. It has
     * the look of a row under the ray, for its help, but none of a press, which does nothing.
     * A hotkey that is not available looks the same, and the reason tells it from the others.
     * Returns whether the ray is on the row.
     */
    bool drawHotkey(const model::Hotkey& hotkey, const PageDrawing& page, std::string& reason)
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
        ui::drawLabel(pos, box.Min.x - theme::SPACE_3, hotkey.text, hotkey.textRuns);
        ui::drawFrame(box);
        ui::drawTextIn(box, name, theme::color::GREEN, 0.5f);
        ImGui::EndDisabled();
        return row.pointed;
    }

    void drawItems(const std::vector<model::Item>& items, const PageDrawing& page, int depth = 0);

    /**
     * Adds the binding settings of the items to the list, with those of the sections among them.
     */
    void addBindings(const std::vector<model::Item>& items, std::vector<const model::Setting*>& bindings)
    {
        for (const auto& item : items) {
            if (const auto* setting = std::get_if<model::Setting>(&item); setting && setting->type == model::SettingType::InputBinding) {
                bindings.push_back(setting);
            } else if (const auto* group = std::get_if<model::Group>(&item)) {
                addBindings(group->items, bindings);
            }
        }
    }

    /**
     * Adds the binding settings of pages to the list, with those of the pages under them.
     */
    void addBindings(const std::vector<model::Page>& pages, std::vector<const model::Setting*>& bindings)
    {
        for (const auto& page : pages) {
            addBindings(page.items, bindings);
            addBindings(page.pages, bindings);
        }
    }

    /**
     * The focus ring around an item's rows, as the design system has it: a 2 px green outline. The page is
     * scrolled to bring it into view, once. An item that is not shown has no room, and no ring.
     */
    void drawFocus(const ImRect& rows, bool& scrollTo)
    {
        if (rows.Max.y <= rows.Min.y) {
            return;
        }
        ui::drawOutline(rows.Min, rows.Max, theme::color::GREEN);
        if (scrollTo) {
            ImGui::ScrollToRectEx(ImGui::GetCurrentWindow(), rows, ImGuiScrollFlags_KeepVisibleCenterY);
            scrollTo = false;
        }
    }

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
        const bool folded = page.sections.drawHeading(group, page.store, pos, width, { .shown = row.shown, .pointed = row.pointed, .pressed = row.pressed }, help);
        page.drawn.pressActed = page.drawn.pressActed || row.pressed;
        if (row.pointed) {
            page.drawn.pointed = ui::PointedRow{ .text = group.text, .help = std::move(help) };
        }
        return folded;
    }

    // how many levels a section in a section is set in at the most, so a name keeps its room
    constexpr int MAX_SET_IN = 3;

    /**
     * A section at the cursor: its heading, where it is shown, and the rows of its items under it, which are set
     * in a level. A section in a section is set in a level with what is in it, up to the third, so its heading
     * starts where the outer section's rows do. While it is open and has rows its line runs from under its
     * heading, `space-1` in, down to its last row, and stops short of the rows, so its rows are seen to be its and
     * not the outer section's. The depth is how many sections it is in.
     * A section that folds has a heading that a press folds or opens, and its rows are drawn while it is open.
     * One whose heading is not shown keeps its fold, and has no line. The heading of any other tells its help
     * while the ray is on it.
     */
    void drawGroup(const model::Group& group, const PageDrawing& page, const int depth, const bool headingShown)
    {
        const bool setIn = depth > 0 && depth <= MAX_SET_IN;
        if (setIn) {
            ImGui::Indent(theme::SPACE_3);
        }
        const float headingLeft = ImGui::GetCursorScreenPos().x;
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
                // the rows' frames are a level in from the heading
                const float rowsLeft = headingLeft + theme::SPACE_3;
                ui::drawSectionLine(headingLeft + theme::SPACE_1, lineTop, rowsEnd - ImGui::GetStyle().ItemSpacing.y, rowsLeft - theme::SPACE_1);
            }
        }
        if (setIn) {
            ImGui::Unindent(theme::SPACE_3);
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
            auto drawn = ui::drawSettingRow(setting,
                { .store = page.store,
                    .sliders = page.sliders,
                    .choices = page.choices,
                    .textInputs = page.textInputs,
                    .imageDrawer = page.imageDrawer,
                    .leftHanded = page.leftHanded,
                    .bindings = page.bindings });
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
            if (auto pointed = page.buttons.draw(button, page.store, page.drawn.pressActed)) {
                page.drawn.pointed = std::move(*pointed);
            }
        }

        void operator()(const model::Hotkey& hotkey) const
        {
            std::string reason;
            if (drawHotkey(hotkey, page, reason)) {
                page.drawn.pointed = ui::PointedRow{ .text = hotkey.text, .help = ui::readOnlyHelp(std::move(reason), hotkey.help) };
            }
        }

        void operator()(const model::Text& text) const
        {
            drawTextRow(text, page.store);
        }

        void operator()(const model::Image& image) const
        {
            if (image.pixels) {
                ui::drawImage(image, page.imageDrawer, ui::rowWidth());
            } else if (!image.texture.empty()) {
                std::string reason;
                if (ui::drawTexture(image, page.imageDrawer, ui::rowWidth(), reason)) {
                    page.drawn.pointed = ui::PointedRow{ .text = image.texture, .help = std::move(reason), .invalid = true };
                }
            } else {
                drawRow({}, {}, "image", false);
            }
        }

        void operator()(const model::Spacer& spacer) const
        {
            ImGui::Dummy(ImVec2(0.0f, ImGui::GetFontSize() * static_cast<float>(spacer.lines)));
        }

        void operator()(const model::Placeholder& placeholder) const
        {
            const bool typed = !placeholder.type.empty();
            if (drawRow(placeholder.text, placeholder.textRuns, typed ? std::string_view(placeholder.type) : "unknown", true)) {
                auto help = typed ? std::format("Not available: the menu has no {} control.", placeholder.type) : std::string("Not available: the row has no type of control.");
                page.drawn.pointed = ui::PointedRow{ .text = placeholder.text, .help = std::move(help) };
            }
        }
    };

    /**
     * Whether an item is shown: one with a condition is, while it holds on the values of the store. With no
     * store no value is available, so a condition on a value holds, and one that is settled without a value,
     * as a config spec's condition on a plugin, is asked as any other.
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
     * Draws the items that are shown, each as its row, set in a level from the heading they are under. A row
     * under no heading is at the content's left, as a heading's row is, so its text lines up with a heading's.
     * An item that is not shown has no row and no room. A section that is not shown is
     * without its heading alone: its items are shown by their own conditions.
     * The depth is how many sections the items are in.
     */
    void drawItems(const std::vector<model::Item>& items, const PageDrawing& page, const int depth)
    {
        for (const auto& item : items) {
            // a row is an item of ImGui by the name of its kind: its place in the page tells it from the others
            ImGui::PushID(&item);
            // a section sets in its heading and its rows itself
            const bool setIn = depth > 0 && !std::holds_alternative<model::Group>(item);
            if (setIn) {
                ImGui::Indent(theme::SPACE_3);
            }
            const auto top = ImGui::GetCursorScreenPos();
            if (isShown(item, page.store)) {
                std::visit(ItemDrawer{ .page = page, .depth = depth }, item);
            } else if (const auto* group = std::get_if<model::Group>(&item)) {
                drawGroup(*group, page, depth, false);
            }
            if (&item == page.focused) {
                drawFocus(ImRect(top, ImVec2(top.x + ui::rowWidth(), ImGui::GetCursorScreenPos().y - ImGui::GetStyle().ItemSpacing.y)), page.scrollToFocused);
            }
            if (setIn) {
                ImGui::Unindent(theme::SPACE_3);
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

    void ModPage::setLeftHanded(const bool leftHanded)
    {
        _leftHanded = leftHanded;
    }

    /**
     * Draws the rows of a page of a mod into the current ImGui window, which scrolls them, with the values the store
     * has for its settings. With no store no value is available. A binding that is the same press as another of the
     * mod's, on any of its pages, names it. The thumbstick's value is how far the one
     * of the hand that points is pushed to a side, from -1 at the left to 1 at the right: it steps the
     * slider or the stepper whose row the ray is on.
     * Returns the row the ray is on, for the help line: none in the gap between two rows, and none on a row
     * that has no text of its own, as a text row, an image and a spacer, or on the heading of a section with
     * no help that does not fold. While the list of a dropdown is open it is the row of that dropdown. And
     * whether a press did something, or the thumbstick stepped a slider or a stepper.
     */
    PageDrawn ModPage::draw(const model::Mod& mod, const model::Page& page, model::Store* store, const float thumbstickSideValue)
    {
        PageDrawn drawn;
        _sliders.startFrame(thumbstickSideValue);
        _choices.startFrame(thumbstickSideValue);
        std::vector<const model::Setting*> bindings;
        addBindings(mod.frontPage.items, bindings);
        addBindings(mod.pages, bindings);
        drawItems(page.items,
            PageDrawing{ .imageDrawer = _imageDrawer,
                .leftHanded = _leftHanded,
                .bindings = bindings,
                .store = store,
                .sliders = _sliders,
                .choices = _choices,
                .textInputs = _textInputs,
                .sections = _sections,
                .buttons = _buttons,
                .drawn = drawn,
                .focused = _focused,
                .scrollToFocused = _scrollToFocused });
        _sliders.endFrame();
        _choices.endFrame();
        if (_buttons.drawDialog(store)) {
            drawn.pressActed = true;
        }
        // the ring is the last row that was pressed, and a press on the page is on another
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && ImGui::IsWindowHovered()) {
            _focused = nullptr;
        }
        return drawn;
    }

    /**
     * Gives an item of the page that is shown the focus ring, and scrolls the page to it as it is drawn next:
     * the sections it is in, from the outermost, are opened first. The ring stays until a press on the page,
     * or until another page is shown.
     */
    void ModPage::focus(const model::Item& item, const std::vector<const model::Group*>& sections)
    {
        for (const auto* section : sections) {
            _sections.open(*section);
        }
        _focused = &item;
        _scrollToFocused = true;
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
     * Closes the confirmation a button shows with nothing run, for Back.
     * Returns false while none is shown.
     */
    bool ModPage::cancelConfirmation()
    {
        return _buttons.cancelConfirmation();
    }

    /**
     * Forgets what is edited in the page, as another page is shown, how its sections were folded, as each
     * starts as its `collapsed` says each time its page is shown, the results of its buttons, and the row that has
     * the focus ring.
     */
    void ModPage::reset()
    {
        _sliders.reset();
        _choices.reset();
        _textInputs.reset();
        _sections.reset();
        _buttons.reset();
        _focused = nullptr;
        _scrollToFocused = false;
    }
}
