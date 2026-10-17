#include "FullPanel.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <format>
#include <string_view>

#include <imgui.h>

#include "Drawing.h"
#include "Theme.h"
#include "common/CommonUtils.h"

namespace
{
    using namespace mods_config;

    // the menu's name: the first breadcrumb of the title bar, which stands for the menu's front page
    constexpr auto TITLE = "MODS CONFIG VR";

    // what stands between two breadcrumbs: U+00BB, as its bytes in UTF-8
    constexpr auto CRUMB_SEPARATOR = "\xC2\xBB";

    // what the help line says of a mod whose changes are not kept for a Save, while the ray is on no row
    constexpr auto SAVED_AS_MADE = "Changes are saved as they are made";

    // The panel's sizes that are not tokens of the design system, in the panel's pixels.
    // A button with a word on it:
    constexpr float BUTTON_WIDTH = 160.0f;
    // and the least the mod's breadcrumb is shortened to, beside the long name of a page:
    constexpr float MOD_CRUMB_MIN_WIDTH = theme::CONTROL_HEIGHT * 2.0f;

    enum class CrumbLook : std::uint8_t
    {
        // a crumb before the last, which is pressed to go there
        Link,
        // the last crumb: where the player is
        Current,
        // the menu's crumb while the menu's front page is open: a solid fill, as a selected row has
        Selected,
    };

    float crumbWidth(const std::string_view text)
    {
        return ui::textWidth(text) + theme::SPACE_2 * 2.0f;
    }

    /**
     * A breadcrumb of the title bar, in the width it is given: a text that does not fit ends in an ellipsis.
     * Returns whether it was pressed, which only a link is.
     */
    bool crumb(const char* id, const std::string_view text, const ImVec2& min, const float width, const CrumbLook look)
    {
        const auto max = ImVec2(min.x + width, min.y + theme::CONTROL_HEIGHT);
        bool pressed = false;
        auto color = theme::color::GREEN;
        if (look == CrumbLook::Selected) {
            ImGui::GetWindowDrawList()->AddRectFilled(min, max, theme::color::GREEN, theme::RADIUS_FRAME);
            color = theme::color::ON_GREEN;
        } else if (look == CrumbLook::Link) {
            ImGui::SetCursorScreenPos(min);
            pressed = ImGui::InvisibleButton(id, ImVec2(width, theme::CONTROL_HEIGHT));
            ui::drawRowState();
            if (!ImGui::IsItemHovered() && !ImGui::IsItemActive()) {
                color = theme::color::GREEN_MUTED;
            }
        }
        ui::drawText(min.x + theme::SPACE_2, max.x - theme::SPACE_1, min.y, theme::CONTROL_HEIGHT, color, text);
        return pressed;
    }

    /**
     * A text of the menu's front page at the cursor, wrapped in the page's width, in the muted color. It is in
     * from the edges of the page, as the texts of its rows are.
     */
    void pageText(const std::string_view text)
    {
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + theme::SPACE_2);
        ImGui::PushStyleColor(ImGuiCol_Text, theme::color::GREEN_MUTED);
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - theme::SPACE_2);
        ImGui::TextUnformatted(text.data(), text.data() + text.size());
        ImGui::PopTextWrapPos();
        ImGui::PopStyleColor();
    }

    /**
     * A row of the quick launch section, laid out as a setting's row: the mod's name after the gutter, and an
     * Open button in the control column. The whole row is pressed, so the button is only drawn.
     * Tells whether the ray is on the row.
     */
    bool quickLaunchRow(const char* name, bool& pointed)
    {
        const auto pos = ImGui::GetCursorScreenPos();
        const float width = ImGui::GetContentRegionAvail().x;
        const bool pressed = ImGui::InvisibleButton(name, ImVec2(width, theme::CONTROL_HEIGHT));
        pointed = ImGui::IsItemHovered();
        ui::drawRowState();

        // the button ends where a setting's control does, before the room of its Reset
        const float buttonRight = pos.x + width - theme::CONTROL_HEIGHT - theme::SPACE_2;
        const float buttonLeft = buttonRight - BUTTON_WIDTH;
        ui::drawOutline(ImVec2(buttonLeft, pos.y), ImVec2(buttonRight, pos.y + theme::CONTROL_HEIGHT), theme::color::FRAME_BORDER);
        const float labelLeft = buttonLeft + (BUTTON_WIDTH - ui::textWidth("Open")) * 0.5f;
        ui::drawText(labelLeft, buttonRight, pos.y, theme::CONTROL_HEIGHT, theme::color::GREEN, "Open");

        ui::drawText(pos.x + theme::SPACE_6 + theme::SPACE_3, buttonLeft - theme::SPACE_3, pos.y, theme::CONTROL_HEIGHT, theme::color::GREEN, name);
        return pressed;
    }
}

namespace mods_config::ui
{
    /**
     * The mods the menu lists, from every source they come from. They are kept in order of their display names,
     * and none of them is selected.
     */
    void FullPanel::setMods(std::vector<model::Mod> mods)
    {
        std::ranges::sort(mods, common::CaseInsensitiveComparator(), &model::Mod::displayName);
        _mods = std::move(mods);
        reset();
    }

    void FullPanel::setQuickLaunch(std::vector<QuickLaunchEntry> entries)
    {
        _quickLaunch = std::move(entries);
    }

    /**
     * What draws the images of a mod's page: the menu's, with the game's renderer. With none, as in a test,
     * an image keeps its room and is not drawn.
     */
    void FullPanel::setImageDrawer(ImageDrawer drawer)
    {
        _modPage.setImageDrawer(std::move(drawer));
    }

    /**
     * The store the panel asks for the values of the selected mod, and sets them in. It is not the panel's
     * own, and has to outlive it. With none, as in a test of the tree, no value is available.
     */
    void FullPanel::setStore(model::Store* store)
    {
        _store = store;
    }

    /**
     * How far the thumbstick of the hand that points is pushed to a side, from -1 at the left to 1 at the
     * right, for the frames that are drawn next: it steps the slider or the stepper the ray is on. The panel does not know
     * the game, so the menu tells it.
     */
    void FullPanel::setThumbstickSideValue(const float value)
    {
        _thumbstickSideValue = value;
    }

    /**
     * The width of the tree of the mods, in the panel's pixels: the content of a page has the rest of the
     * panel's width. The design system's is `theme::NAV_WIDTH`, which the panel has until it is told another.
     */
    void FullPanel::setTreeWidth(const float width)
    {
        _modTree.setWidth(width);
    }

    /**
     * Shows a mod's front page or one of its pages, or the menu's front page with no mod. A selection of
     * what is not there shows the nearest that is: the mod's front page for a page, the menu's for a mod,
     * and for a mod that is not available.
     * Another mod than before starts its editing session in the store, and no mod ends the one there is.
     * Another page of the same mod is told to the store, which reads the values it keeps in files again.
     * Returns whether it shows another page than before.
     */
    bool FullPanel::select(const Selection& selection)
    {
        Selection shown;
        if (selection.mod && *selection.mod < _mods.size() && _mods[*selection.mod].notAvailable.empty()) {
            shown.mod = selection.mod;
            if (selection.page && *selection.page < _mods[*selection.mod].pages.size()) {
                shown.page = selection.page;
            }
        }
        if (shown == _selection) {
            return false;
        }
        if (_store && shown.mod != _selection.mod) {
            if (shown.mod) {
                _store->open(_mods[*shown.mod]);
            } else {
                _store->close();
            }
        } else if (_store && shown.mod) {
            _store->pageShown();
        }
        _selection = shown;
        _pageChanged = true;
        _modPage.reset();
        _helpLine.clear();
        _helpLine.setStatus(_store && shown.mod ? SAVED_AS_MADE : "");
        _modTree.showSelection();
        return true;
    }

    /**
     * Goes one step back: a slider that is dragged has the value it had before the drag, the list of a
     * dropdown that is open is closed, and a text input that is edited keeps its value. With none of them, from a page of a mod to the mod's front page, and from
     * there to the menu's front page.
     * Returns false on the menu's front page, which has nothing before it: Back closes the menu there.
     */
    bool FullPanel::back()
    {
        if (_modPage.cancelDrag() || _modPage.closeList() || _modPage.cancelEdit()) {
            return true;
        }
        if (!_selection.mod) {
            return false;
        }
        select(_selection.page ? Selection{ .mod = _selection.mod, .page = std::nullopt } : Selection{});
        return true;
    }

    /**
     * The panel as the menu opens: on the menu's front page, with the tree at its top. A menu that was closed
     * does not keep its place. The editing session of a mod that was shown ends in the store.
     */
    void FullPanel::reset()
    {
        if (_store && _selection.mod) {
            _store->close();
        }
        _selection = {};
        _pageChanged = true;
        _pressActed = false;
        _thumbstickSideValue = 0.0f;
        _modPage.reset();
        _helpLine.clear();
        _helpLine.setStatus({});
        _modTree.showTop();
    }

    /**
     * Draws the panel into the current ImGui window: the title bar, under it the tree of the mods with the
     * menu's front page or the selected mod's page beside it, and the footer at the bottom.
     * The help line has the row of the page the ray is on, or the row of the tree of a mod that is not
     * available, until the ray is on another or leaves the page:
     * in the gap between two rows it keeps the row it has, so it does not flicker as the ray sweeps the rows.
     * With no row it says that the changes of the selected mod are saved as they are made.
     * A press selects once all of the panel is drawn, so a frame is drawn with one selection.
     * It also tells whether a press did something in this frame, for the pulse of the hand that points.
     */
    void FullPanel::draw()
    {
        if (!_styleApplied) {
            theme::applyStyle(ImGui::GetStyle());
            _styleApplied = true;
        }

        _pressActed = false;
        auto pressed = drawTitleBar();
        // the body stands a little further from the title bar than a row from a row
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + theme::SPACE_3 - ImGui::GetStyle().ItemSpacing.y);

        // the body ends as far above the footer as it starts under the title bar
        const auto bodyPos = ImGui::GetCursorScreenPos();
        const float bodyHeight = ImGui::GetContentRegionAvail().y - theme::SPACE_3 - theme::FOOTER_HEIGHT;

        auto tree = _modTree.draw(_mods, _selection, bodyHeight);
        if (tree.pressed) {
            pressed = tree.pressed;
        }

        ImGui::SameLine(0.0f, theme::SPACE_4);
        if (_pageChanged) {
            // the pages share the pane, which would keep the place the page before was scrolled to
            ImGui::SetNextWindowScroll(ImVec2(0.0f, 0.0f));
            _pageChanged = false;
        }
        std::optional<PointedRow> pointed;
        bool onPage = false;
        if (ImGui::BeginChild("Content", ImVec2(0.0f, bodyHeight))) {
            onPage = ImGui::IsWindowHovered();
            if (_selection.mod) {
                const auto& mod = _mods[*_selection.mod];
                auto drawn = _modPage.draw(_selection.page ? mod.pages[*_selection.page] : mod.frontPage, _store, _thumbstickSideValue);
                pointed = std::move(drawn.pointed);
                _pressActed = drawn.pressActed;
            } else {
                pointed = drawFrontPage();
            }
        }
        ImGui::EndChild();

        if (!pointed) {
            pointed = std::move(tree.pointed);
        }
        if (pointed) {
            _helpLine.show(*pointed);
        } else if (!onPage) {
            _helpLine.clear();
        }
        drawFooter(ImVec2(bodyPos.x, bodyPos.y + bodyHeight + theme::SPACE_3));

        // a press on the row or the breadcrumb of what is shown already changes nothing
        if (pressed && select(*pressed)) {
            _pressActed = true;
        }
    }

    /**
     * The title bar, with a line under it. Its breadcrumbs say where the player is: the menu, the selected
     * mod, and the page. The last is where the player is, and each one before it is pressed to go there.
     * The menu's crumb stands for the menu's front page, and is drawn selected while that page is open.
     * Returns what a pressed crumb selects.
     */
    std::optional<Selection> FullPanel::drawTitleBar() const
    {
        const auto pos = ImGui::GetCursorScreenPos();
        const float width = ImGui::GetContentRegionAvail().x;
        std::optional<Selection> pressed;

        ImGui::PushFont(nullptr, theme::FONT_SIZE_TITLE);
        // on whole pixels, so a crumb is exactly as wide as the text it was measured for
        ImVec2 at(std::floor(pos.x + theme::SPACE_2), std::floor(pos.y + (theme::TITLEBAR_HEIGHT - theme::CONTROL_HEIGHT) * 0.5f));
        const float right = pos.x + width - theme::SPACE_2;
        const float gap = theme::SPACE_1;
        const auto* mod = _selection.mod ? &_mods[*_selection.mod] : nullptr;

        if (crumb("menu", TITLE, at, crumbWidth(TITLE), mod ? CrumbLook::Link : CrumbLook::Selected)) {
            pressed = Selection{};
        }
        at.x += crumbWidth(TITLE) + gap;

        if (mod) {
            const float separatorWidth = textWidth(CRUMB_SEPARATOR);
            const auto drawSeparator = [&at, right, separatorWidth, gap] {
                drawText(at.x, right, at.y, theme::CONTROL_HEIGHT, theme::color::GREEN_MUTED, CRUMB_SEPARATOR);
                at.x += separatorWidth + gap;
            };
            // MCM's own list sets a page in under another by spaces before its name: they are the tree's
            const auto pageName = _selection.page ? common::trim(mod->pages[*_selection.page].name) : std::string();

            // when the crumbs do not fit the bar, the mod's name is the one that is shortened
            float modRoom = right - at.x - separatorWidth - gap;
            if (_selection.page) {
                modRoom -= gap + separatorWidth + gap + crumbWidth(pageName);
            }
            const float modWidth = (std::min)(crumbWidth(mod->displayName), (std::max)(modRoom, MOD_CRUMB_MIN_WIDTH));

            drawSeparator();
            if (crumb("mod", mod->displayName, at, modWidth, _selection.page ? CrumbLook::Link : CrumbLook::Current)) {
                pressed = Selection{ .mod = _selection.mod, .page = std::nullopt };
            }
            at.x += modWidth + gap;

            if (_selection.page) {
                drawSeparator();
                crumb("page", pageName, at, (std::min)(crumbWidth(pageName), right - at.x), CrumbLook::Current);
            }
        }
        ImGui::PopFont();

        const float lineTop = pos.y + theme::TITLEBAR_HEIGHT - theme::BORDER_FRAME;
        ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(pos.x, lineTop), ImVec2(pos.x + width, lineTop + theme::BORDER_FRAME), ImGui::GetColorU32(theme::color::RULE));
        // the crumbs moved the cursor: the bar takes its room from where it began
        ImGui::SetCursorScreenPos(pos);
        ImGui::Dummy(ImVec2(width, theme::TITLEBAR_HEIGHT));
        return pressed;
    }

    /**
     * The menu's front page, which is what the menu shows while no mod is selected: what the menu is, with its
     * version, the quick launch section, and the notices: a line for each mod that is not available, with the
     * reason its row in the tree has. A section is left out while it has nothing in it.
     * Returns the quick launch entry the ray is on, for the help line.
     */
    std::optional<PointedRow> FullPanel::drawFrontPage()
    {
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + theme::SPACE_1);
        pageText(std::format("Settings for the mods you have installed. Pick a mod on the left to see its settings.\nVersion {}", Version::NAME));

        std::optional<PointedRow> pointed;
        if (!_quickLaunch.empty()) {
            ImGui::SeparatorText("Quick launch");
        }
        for (const auto& entry : _quickLaunch) {
            bool onRow = false;
            if (quickLaunchRow(entry.name.c_str(), onRow)) {
                entry.open();
                _pressActed = true;
            }
            if (onRow) {
                pointed = PointedRow{ .text = entry.name, .help = entry.help };
            }
        }

        bool hasNotices = false;
        for (const auto& mod : _mods) {
            if (mod.notAvailable.empty()) {
                continue;
            }
            if (!hasNotices) {
                ImGui::SeparatorText("Notices");
                hasNotices = true;
            }
            pageText(std::format("{} is not available: {}.", mod.displayName, mod.notAvailable));
        }
        return pointed;
    }

    /**
     * The footer, from its top left: a line over it, and the help line. The help line has all of its width,
     * in from its edges as the mock has it: an MCM mod has no Discard and Save beside it, and its status is
     * the help line's.
     */
    void FullPanel::drawFooter(const ImVec2& pos) const
    {
        const float width = ImGui::GetContentRegionAvail().x;
        ImGui::GetWindowDrawList()->AddRectFilled(pos, ImVec2(pos.x + width, pos.y + theme::BORDER_FRAME), ImGui::GetColorU32(theme::color::RULE));
        _helpLine.draw(ImVec2(pos.x + theme::SPACE_4, pos.y), width - theme::SPACE_4 - theme::SPACE_2);
        ImGui::SetCursorScreenPos(pos);
        ImGui::Dummy(ImVec2(width, theme::FOOTER_HEIGHT));
    }
}
