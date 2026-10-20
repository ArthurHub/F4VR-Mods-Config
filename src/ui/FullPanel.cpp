#include "FullPanel.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <format>
#include <string_view>
#include <tuple>

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

    // the crumb that stands for the pages between the mod and the page, when their crumbs do not fit: U+2026,
    // as its bytes in UTF-8
    constexpr auto PAGES_ELIDED = "\xE2\x80\xA6";

    // the breadcrumb of the Help page, after the menu's
    constexpr auto HELP_CRUMB = "Help";

    // what the help line says of the title bar's buttons
    constexpr auto HELP_HELP = "What each control of the menu does, with the buttons your config has for it.";
    constexpr auto MINIMIZE_HELP = "Hides the menu, as you left it. Resume on your offhand wrist shows it again.";
    constexpr auto CLOSE_HELP = "Closes the menu.";

    // what the help line says of a mod whose changes are not kept for a Save, while the ray is on no row
    constexpr auto SAVED_AS_MADE = "Changes are saved as they are made";

    // and of a mod whose changes wait for a Save, while none does
    constexpr auto NO_UNSAVED = "No unsaved changes";

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
        // the ellipsis that stands for the pages between the mod and the page, which is not pressed
        Elided,
    };

    float crumbWidth(const std::string_view text)
    {
        return ui::textWidth(text) + theme::SPACE_2 * 2.0f;
    }

    /**
     * A breadcrumb of the title bar, in the width it is given: a text that does not fit ends in an ellipsis.
     * Returns whether it was pressed, which only a link is. The crumb of the pages that do not fit is muted, as a
     * link is while the ray is not on it.
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
        } else if (look == CrumbLook::Elided) {
            color = theme::color::GREEN_MUTED;
        }
        ui::drawText(min.x + theme::SPACE_2, max.x - theme::SPACE_1, min.y, theme::CONTROL_HEIGHT, color, text);
        return pressed;
    }

    /**
     * A 48 px button of the title bar in its box, which its caller draws the icon of: the outline of a button,
     * and its look under the ray and while it is pressed. Tells whether the ray is on it. Returns whether it
     * was pressed.
     */
    bool iconButton(const char* id, const ImRect& box, bool& pointed)
    {
        ImGui::SetCursorScreenPos(box.Min);
        const bool pressed = ImGui::InvisibleButton(id, box.GetSize());
        pointed = ImGui::IsItemHovered();
        ui::drawButtonFrame(box, pointed, ImGui::IsItemActive());
        return pressed;
    }

    /**
     * The Minimize icon around a point: a line at the bottom of a box of 24, as the design system draws it.
     */
    void drawMinimizeIcon(const ImVec2& center)
    {
        ImGui::GetWindowDrawList()->AddLine(ImVec2(center.x - 6.0f, center.y + 5.0f),
            ImVec2(center.x + 6.0f, center.y + 5.0f),
            ImGui::GetColorU32(theme::color::GREEN),
            ui::ICON_THICKNESS);
    }

    /**
     * The Close icon around a point: a cross in a box of 24, as the design system draws it.
     */
    void drawCloseIcon(const ImVec2& center)
    {
        constexpr float HALF = 6.0f;
        const auto color = ImGui::GetColorU32(theme::color::GREEN);
        auto* drawList = ImGui::GetWindowDrawList();
        drawList->AddLine(ImVec2(center.x - HALF, center.y - HALF), ImVec2(center.x + HALF, center.y + HALF), color, ui::ICON_THICKNESS);
        drawList->AddLine(ImVec2(center.x + HALF, center.y - HALF), ImVec2(center.x - HALF, center.y + HALF), color, ui::ICON_THICKNESS);
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

    /**
     * A button of the footer, in its box. Save is the primary button: a solid green, with its text in the color
     * on green, a lighter green under the ray, and an outline while it is pressed. Discard is an outline.
     * Inside `BeginDisabled` it is faded and is not pressed. Returns whether it was pressed.
     */
    bool footerButton(const char* label, const ImRect& box, const bool primary)
    {
        ImGui::SetCursorScreenPos(box.Min);
        const bool pressed = ImGui::InvisibleButton(label, box.GetSize());
        const bool pointed = ImGui::IsItemHovered();
        const bool held = ImGui::IsItemActive();
        if (primary) {
            ImGui::GetWindowDrawList()->AddRectFilled(box.Min, box.Max, ImGui::GetColorU32(pointed && !held ? theme::color::GLOW : theme::color::GREEN), theme::RADIUS_FRAME);
            if (held) {
                ui::drawOutline(box.Min, box.Max, theme::color::GLOW);
            }
        } else {
            ui::drawButtonFrame(box, pointed, held);
        }
        ui::drawTextIn(box, label, primary ? theme::color::ON_GREEN : theme::color::GREEN, 0.5f);
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

    void FullPanel::setControls(std::vector<ControlHelp> controls)
    {
        _controls = std::move(controls);
    }

    /**
     * What the title bar's Minimize and Close do. With none, as in a test, a press on them does nothing.
     */
    void FullPanel::setMenuActions(MenuActions actions)
    {
        _menuActions = std::move(actions);
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
     * The store the panel asks for the values of a selected mod of a source, and sets them in. It is not the
     * panel's own, and has to outlive it. With none, as in a test of the tree, no value is available.
     */
    void FullPanel::setStore(const model::Source source, model::Store* store)
    {
        (source == model::Source::Mcm ? _mcmStore : _packageStore) = store;
    }

    /**
     * The store of the values of a mod: the one of the source of its menu.
     */
    model::Store* FullPanel::storeOf(const model::Mod& mod) const
    {
        return mod.source == model::Source::Mcm ? _mcmStore : _packageStore;
    }

    /**
     * The store of the selected mod's values, which its editing session is in. None on the menu's front page.
     */
    model::Store* FullPanel::selectedStore() const
    {
        return _selection.mod ? storeOf(_mods[*_selection.mod]) : nullptr;
    }

    /**
     * Gives the help line the status of the selected mod's editing session: that its changes are saved as
     * they are made, how many changes are invalid, which block Save, why the last Save failed, until the next
     * change, or how many wait for a Save. Nothing with no mod, and with no store.
     */
    void FullPanel::updateStatus()
    {
        auto* const store = _selection.mod ? storeOf(_mods[*_selection.mod]) : nullptr;
        if (!store) {
            _helpLine.setStatus({});
        } else if (store->savesAtOnce()) {
            _helpLine.setStatus(SAVED_AS_MADE);
        } else if (const auto invalid = store->invalidCount(); invalid > 0) {
            _helpLine.setStatus(std::format("{} invalid value{}", invalid, invalid == 1 ? "" : "s"), StatusLook::Invalid);
        } else if (const auto failure = store->saveFailure(); !failure.empty()) {
            _helpLine.setStatus(std::format("Not saved: {}.", failure), StatusLook::Invalid);
        } else if (const auto dirty = store->dirtyCount(); dirty == 0) {
            _helpLine.setStatus(NO_UNSAVED);
        } else {
            _helpLine.setStatus(std::format("{} unsaved change{}", dirty, dirty == 1 ? "" : "s"), StatusLook::Pending);
        }
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
     * what is not there shows the nearest that is: for a page, the deepest page on the way to it that is there,
     * or the mod's front page, and the menu's front page for a mod, and for a mod that is not available.
     * Another mod than before starts its editing session in its store, which ends the session it had. A mod
     * that is left for no mod, or for a mod of another store, ends its session in its store. Another page of
     * the same mod is told to its store, which reads the values it keeps in files again.
     * It leaves the Help page: the selection under it is shown again, as Back shows it, with its session kept.
     * Returns whether it shows another page than before.
     */
    bool FullPanel::select(const Selection& selection)
    {
        Selection shown;
        if (selection.mod && *selection.mod < _mods.size() && _mods[*selection.mod].notAvailable.empty()) {
            shown.mod = selection.mod;
            const auto* pages = &_mods[*selection.mod].pages;
            for (const auto place : selection.page) {
                if (place >= pages->size()) {
                    break;
                }
                shown.page.push_back(place);
                pages = &(*pages)[place].pages;
            }
        }
        if (shown == _selection) {
            return showHelp(false);
        }
        _helpShown = false;
        auto* const storeBefore = _selection.mod ? storeOf(_mods[*_selection.mod]) : nullptr;
        auto* const store = shown.mod ? storeOf(_mods[*shown.mod]) : nullptr;
        if (shown.mod != _selection.mod) {
            if (storeBefore && storeBefore != store) {
                storeBefore->close();
            }
            if (store) {
                store->open(_mods[*shown.mod]);
            }
        } else if (store) {
            store->pageShown();
        }
        _selection = shown;
        pageShown();
        return true;
    }

    /**
     * The page of the selection: the selected mod's front page, or the page its places name. None on the
     * menu's front page.
     */
    const model::Page* FullPanel::selectedPage() const
    {
        if (!_selection.mod) {
            return nullptr;
        }
        const auto& mod = _mods[*_selection.mod];
        const auto* page = &mod.frontPage;
        const auto* pages = &mod.pages;
        for (const auto place : _selection.page) {
            page = &(*pages)[place];
            pages = &page->pages;
        }
        return page;
    }

    /**
     * Another page is shown in the content pane, from its top: a row of the page before keeps no drag, list,
     * edit or help, and the tree shows the selection.
     */
    void FullPanel::pageShown()
    {
        _pageChanged = true;
        _modPage.reset();
        _helpLine.clear();
        updateStatus();
        _modTree.showSelection();
    }

    /**
     * Shows the Help page in place of the selection's page, or the selection's page again in its place. The
     * selection is kept, and so is its editing session. Returns whether it shows another page than before.
     */
    bool FullPanel::showHelp(const bool shown)
    {
        if (shown == _helpShown) {
            return false;
        }
        _helpShown = shown;
        pageShown();
        return true;
    }

    /**
     * Goes one step back: a slider that is dragged has the value it had before the drag, the list of a
     * dropdown that is open is closed, and a text input that is edited keeps its value. With none of them, from
     * the Help page to the page it was shown over, from a page of a mod to the page it is under or to the mod's
     * front page, and from there to the menu's front page.
     * Returns false on the menu's front page, which has nothing before it: Back closes the menu there.
     */
    bool FullPanel::back()
    {
        if (_modPage.cancelDrag() || _modPage.closeList() || _modPage.cancelEdit() || showHelp(false)) {
            return true;
        }
        if (!_selection.mod) {
            return false;
        }
        if (_selection.page.empty()) {
            select({});
        } else {
            auto above = _selection;
            above.page.pop_back();
            select(above);
        }
        return true;
    }

    /**
     * The panel as the menu opens: on the menu's front page, with the tree at its top. A menu that was closed
     * does not keep its place. The editing session of a mod that was shown ends in its store.
     */
    void FullPanel::reset()
    {
        if (auto* const store = _selection.mod ? storeOf(_mods[*_selection.mod]) : nullptr) {
            store->close();
        }
        _selection = {};
        _helpShown = false;
        _pageChanged = true;
        _pressActed = false;
        _thumbstickSideValue = 0.0f;
        _modPage.reset();
        _helpLine.clear();
        updateStatus();
        _modTree.showTop();
    }

    /**
     * Draws the panel into the current ImGui window: the title bar, under it the tree of the mods with the
     * menu's front page, the selected mod's page or the Help page beside it, and the footer at the bottom.
     * The help line has the row of the page the ray is on, or the row of the tree with a help, or the button of
     * the title bar, until the ray is on another or leaves the page:
     * in the gap between two rows it keeps the row it has, so it does not flicker as the ray sweeps the rows.
     * With no row it has the status of the selected mod's editing session: that its changes are saved as they
     * are made, or how many wait for a Save.
     * A press selects, or does what its button of the title bar does, once all of the panel is drawn, so a
     * frame is drawn with one selection.
     * It also tells whether a press did something in this frame, for the pulse of the hand that points.
     */
    void FullPanel::draw()
    {
        if (!_styleApplied) {
            theme::applyStyle(ImGui::GetStyle());
            _styleApplied = true;
        }

        _pressActed = false;
        auto titleBar = drawTitleBar();
        auto pressed = titleBar.pressed;
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
            if (_helpShown) {
                drawHelpPage();
            } else if (_selection.mod) {
                auto drawn = _modPage.draw(*selectedPage(), storeOf(_mods[*_selection.mod]), _thumbstickSideValue);
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
        if (!pointed) {
            pointed = std::move(titleBar.pointed);
        }
        if (pointed) {
            _helpLine.show(*pointed);
        } else if (!onPage) {
            _helpLine.clear();
        }
        updateStatus();
        drawFooter(ImVec2(bodyPos.x, bodyPos.y + bodyHeight + theme::SPACE_3));

        // a press on the row or the breadcrumb of what is shown already changes nothing, and neither does Help
        // on the Help page
        if (pressed ? select(*pressed) : pressButton(titleBar.pressedButton)) {
            _pressActed = true;
        }
    }

    /**
     * Does what a button of the title bar does: Help shows the Help page, and Minimize and Close are the
     * menu's. Returns whether it did something.
     */
    bool FullPanel::pressButton(const TitleButton button)
    {
        switch (button) {
        case TitleButton::Help:
            return showHelp(true);
        case TitleButton::Minimize:
            if (_menuActions.minimize) {
                _menuActions.minimize();
                return true;
            }
            return false;
        case TitleButton::Close:
            if (_menuActions.close) {
                _menuActions.close();
                return true;
            }
            return false;
        default:
            return false;
        }
    }

    /**
     * The title bar, with a line under it. Its breadcrumbs say where the player is: the menu, the selected
     * mod, and each page on the way to the selected page, or the menu and the Help page. The last is where the
     * player is, and each one before it is pressed to go there. The menu's crumb stands for the menu's front
     * page, and is drawn selected while that page is open. Help, Minimize and Close are at the right of the
     * bar, and the crumbs end before them. When the crumbs do not fit, the pages between the mod and the last
     * become one ellipsis, which is not pressed, and then the mod's name is shortened.
     * Returns what a pressed crumb selects, the button that was pressed, and the button the ray is on.
     */
    FullPanel::TitleBar FullPanel::drawTitleBar() const
    {
        const auto pos = ImGui::GetCursorScreenPos();
        const float width = ImGui::GetContentRegionAvail().x;
        TitleBar bar;

        // the buttons from the right of the bar, before the crumbs, which are in the title's size
        struct Button
        {
            TitleButton button;
            const char* name;
            const char* help;
        };

        constexpr std::array BUTTONS{ Button{ TitleButton::Close, "Close", CLOSE_HELP },
            Button{ TitleButton::Minimize, "Minimize", MINIMIZE_HELP },
            Button{ TitleButton::Help, "Help", HELP_HELP } };
        const float buttonTop = std::floor(pos.y + (theme::TITLEBAR_HEIGHT - theme::CONTROL_HEIGHT) * 0.5f);
        float buttonLeft = std::floor(pos.x + width - theme::SPACE_2);
        for (const auto& button : BUTTONS) {
            buttonLeft -= theme::CONTROL_HEIGHT;
            const auto box = controlBox(buttonLeft, buttonTop, theme::CONTROL_HEIGHT);
            bool pointed = false;
            if (iconButton(button.name, box, pointed)) {
                bar.pressedButton = button.button;
            }
            if (pointed) {
                bar.pointed = PointedRow{ .text = button.name, .help = button.help };
            }
            if (button.button == TitleButton::Help) {
                drawTextIn(box, "?", theme::color::GREEN, 0.5f);
            } else if (button.button == TitleButton::Minimize) {
                drawMinimizeIcon(box.GetCenter());
            } else {
                drawCloseIcon(box.GetCenter());
            }
            buttonLeft -= theme::SPACE_2;
        }

        ImGui::PushFont(nullptr, theme::FONT_SIZE_TITLE);
        // on whole pixels, so a crumb is exactly as wide as the text it was measured for
        ImVec2 at(std::floor(pos.x + theme::SPACE_2), buttonTop);
        const float right = buttonLeft;
        const float gap = theme::SPACE_1;
        const float separatorWidth = textWidth(CRUMB_SEPARATOR);
        const auto drawSeparator = [&at, right, separatorWidth, gap] {
            drawText(at.x, right, at.y, theme::CONTROL_HEIGHT, theme::color::GREEN_MUTED, CRUMB_SEPARATOR);
            at.x += separatorWidth + gap;
        };
        const auto* mod = _selection.mod && !_helpShown ? &_mods[*_selection.mod] : nullptr;

        if (crumb("menu", TITLE, at, crumbWidth(TITLE), mod || _helpShown ? CrumbLook::Link : CrumbLook::Selected)) {
            bar.pressed = Selection{};
        }
        at.x += crumbWidth(TITLE) + gap;

        if (_helpShown) {
            drawSeparator();
            crumb("help", HELP_CRUMB, at, (std::min)(crumbWidth(HELP_CRUMB), right - at.x), CrumbLook::Current);
        } else if (mod) {
            // the pages on the way to the selected page, by their names: MCM's own list sets a page in under
            // another by spaces before its name, which are the tree's
            std::vector<std::string> pageNames;
            const auto* pages = &mod->pages;
            for (const auto place : _selection.page) {
                pageNames.push_back(common::trim((*pages)[place].name));
                pages = &(*pages)[place].pages;
            }

            // the room of a crumb after another: the gap after the one before, the separator, and a gap
            const float before = gap + separatorWidth + gap;
            float pagesWidth = 0.0f;
            for (const auto& name : pageNames) {
                pagesWidth += before + crumbWidth(name);
            }
            const float modRoom = right - at.x - separatorWidth - gap;
            const bool elided = pageNames.size() > 1 && crumbWidth(mod->displayName) + pagesWidth > modRoom;
            if (elided) {
                pagesWidth = before + crumbWidth(PAGES_ELIDED) + before + crumbWidth(pageNames.back());
            }
            const float modWidth = (std::min)(crumbWidth(mod->displayName), (std::max)(modRoom - pagesWidth, MOD_CRUMB_MIN_WIDTH));

            drawSeparator();
            if (crumb("mod", mod->displayName, at, modWidth, pageNames.empty() ? CrumbLook::Current : CrumbLook::Link)) {
                bar.pressed = Selection{ .mod = _selection.mod, .page = {} };
            }
            at.x += modWidth + gap;

            if (elided) {
                drawSeparator();
                crumb("elided", PAGES_ELIDED, at, crumbWidth(PAGES_ELIDED), CrumbLook::Elided);
                at.x += crumbWidth(PAGES_ELIDED) + gap;
            }
            for (std::size_t level = elided ? pageNames.size() - 1 : 0; level < pageNames.size(); ++level) {
                const bool last = level + 1 == pageNames.size();
                const float pageWidth = last ? (std::min)(crumbWidth(pageNames[level]), right - at.x - separatorWidth - gap) : crumbWidth(pageNames[level]);
                drawSeparator();
                ImGui::PushID(static_cast<int>(level));
                if (crumb("page", pageNames[level], at, pageWidth, last ? CrumbLook::Current : CrumbLook::Link)) {
                    bar.pressed = Selection{ .mod = _selection.mod, .page = { _selection.page.begin(), _selection.page.begin() + static_cast<std::ptrdiff_t>(level + 1) } };
                }
                ImGui::PopID();
                at.x += pageWidth + gap;
            }
        }
        ImGui::PopFont();

        const float lineTop = pos.y + theme::TITLEBAR_HEIGHT - theme::BORDER_FRAME;
        ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(pos.x, lineTop), ImVec2(pos.x + width, lineTop + theme::BORDER_FRAME), ImGui::GetColorU32(theme::color::RULE));
        // the crumbs moved the cursor: the bar takes its room from where it began
        ImGui::SetCursorScreenPos(pos);
        ImGui::Dummy(ImVec2(width, theme::TITLEBAR_HEIGHT));
        return bar;
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
     * The Help page, which a press on Help shows over the selection's page: a text of what it is, and each
     * control of the menu as `Name: text`, its name in green and its text in the muted color, as the help line
     * has a row's help. Each is wrapped in the page's width, in from its edges as the front page's texts are.
     */
    void FullPanel::drawHelpPage() const
    {
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + theme::SPACE_1);
        pageText("How the menu is used, with the buttons your config has.");
        ImGui::SeparatorText("Controls");

        const float width = ImGui::GetContentRegionAvail().x - theme::SPACE_2 * 2.0f;
        for (const auto& control : _controls) {
            const auto name = control.name + ": ";
            const std::array pieces{ TextPiece{ .text = name, .color = theme::color::GREEN }, TextPiece{ .text = control.text, .color = theme::color::GREEN_MUTED } };
            const int lines = countTextLines(pieces, width);
            const auto pos = ImGui::GetCursorScreenPos();
            drawTextPieces(pieces, ImVec2(pos.x + theme::SPACE_2, pos.y), width, lines);
            ImGui::Dummy(ImVec2(width, static_cast<float>(lines) * ImGui::GetFontSize()));
        }
    }

    /**
     * The footer, from its top left: a line over it, the help line on its left, and Discard and Save on its
     * right, which keep their places. They are enabled while the selected mod has changes that wait for a
     * Save, also after a Save that failed, and are disabled on the menu's front page, which has no session.
     * Save is disabled while a change is invalid, which the status says. A press on Save writes the changes,
     * and one on Discard takes them back. An MCM mod saves its changes as they are made, so it has no Discard
     * and Save, and its help line has all of the footer's width.
     */
    void FullPanel::drawFooter(const ImVec2& pos)
    {
        const float width = ImGui::GetContentRegionAvail().x;
        ImGui::GetWindowDrawList()->AddRectFilled(pos, ImVec2(pos.x + width, pos.y + theme::BORDER_FRAME), ImGui::GetColorU32(theme::color::RULE));

        float helpRight = pos.x + width - theme::SPACE_2;
        auto* const store = _selection.mod ? storeOf(_mods[*_selection.mod]) : nullptr;
        if (!store || !store->savesAtOnce()) {
            const float top = pos.y + (theme::FOOTER_HEIGHT - theme::CONTROL_HEIGHT) * 0.5f;
            const auto save = controlBox(helpRight - BUTTON_WIDTH, top, BUTTON_WIDTH);
            const auto discard = controlBox(save.Min.x - theme::SPACE_3 - BUTTON_WIDTH, top, BUTTON_WIDTH);
            ImGui::BeginDisabled(!store || store->dirtyCount() == 0);
            if (footerButton("Discard", discard, false)) {
                store->discard();
                _pressActed = true;
            }
            ImGui::BeginDisabled(store && store->invalidCount() > 0);
            if (footerButton("Save", save, true)) {
                // the status has the reason of a Save that failed, which the store keeps
                std::ignore = store->save();
                _pressActed = true;
            }
            ImGui::EndDisabled();
            ImGui::EndDisabled();
            updateStatus();
            helpRight = discard.Min.x - theme::SPACE_3;
        }

        _helpLine.draw(ImVec2(pos.x + theme::SPACE_4, pos.y), helpRight - pos.x - theme::SPACE_4);
        ImGui::SetCursorScreenPos(pos);
        ImGui::Dummy(ImVec2(width, theme::FOOTER_HEIGHT));
    }
}
