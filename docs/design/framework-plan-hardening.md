# Framework hardening plan

The next mod, a cheat terminal and item spawner, is a panel heavy with UI: a tree beside pages of buttons, sliders, number inputs, lists of thousands of items and a search. It is built from the framework's [`imgui::controls`](../../external/F4VR-CommonFramework/src/imgui/controls/README.md), which this mod's panel is drawn with. This plan is what the framework still lacks for a second panel to look and behave as this one with little code of its own: one theme, the targets a test points at, the generic parts of the panel that are still in this mod, the panel's layout, and a tier of controls with their label.

It continues the numbering of the [framework work plan](framework-plan.md): framework commits (F) from F33, each followed by this mod's port onto it (M) from M20, one at a time. A port changes no look and no behavior: the mod's panel tests, render tests and e2e tests pass unchanged after each. Framework version `0.7.0`, as F33 changes the controls' API.

## Decisions

- **A mod's design is the mod's own.** The framework's `Theme` has defaults, the framework's green, for a mod that sets nothing. This mod applies the theme of its design system, generated from its tokens, and depends on no default; a mod with another design applies its own theme the same way.
- **Every canvas gets the theme's ImGui style.** The framework sets it on its ImGui context in place of `ImGui::StyleColorsDark()`. No other mod draws ImGui yet.
- **The canvas font's default size is the theme's body size**, 21 px in the default theme, not 24.
- **Nothing waits for the next mod.** The controls with their label and the clipped list have no row in this mod, so each is tested in the framework and drawn by a render test of this mod (M27, M28).

## Commits

| #   | Repo      | Commit                                                                                                       | Status  |
| --- | --------- | ------------------------------------------------------------------------------------------------------------ | ------- |
| F33 | framework | imgui: one `Theme` for the controls, ImGui's style, the keyboard, the pointer and the panel, set live, also by devbench | planned |
| M20 | mod       | The design system's theme, generated from its tokens, applied as the framework's theme                       | planned |
| F34 | framework | imgui: the targets of a panel, which the controls record, and a devbench `targets` action                    | planned |
| M21 | mod       | Targets from the framework; `PanelTargets` and the wrappers that recorded for it are gone                    | planned |
| F35 | framework | imgui: the search field                                                                                      | planned |
| M22 | mod       | The tree's search field from the framework                                                                   | planned |
| F36 | framework | imgui: the drawn icons as one `Icon` set, and a popup beside its row                                         | planned |
| M23 | mod       | Icons and the editors' popup from the framework                                                              | planned |
| F37 | framework | imgui: the panes of a panel: a navigation pane with its header and rows, and a content pane                  | planned |
| M24 | mod       | The tree's pane and the content pane from the framework                                                      | planned |
| F38 | framework | imgui: the title bar: breadcrumbs that shorten to fit, and its buttons                                       | planned |
| M25 | mod       | The title bar from the framework                                                                             | planned |
| F39 | framework | imgui: the footer: a rule, its buttons at the right, and the room of a help line                             | planned |
| M26 | mod       | The footer from the framework                                                                                | planned |
| F40 | framework | imgui: controls with their label in a row, and the press pulse                                               | planned |
| M27 | mod       | A render test of the controls with their label                                                               | planned |
| F41 | framework | imgui: a clipped list of rows                                                                                | planned |
| M28 | mod       | A render test of a clipped list                                                                              | planned |
| M29 | mod       | The mod's own drawing reads the theme as it draws, not the constants of `Theme.h`                            | planned |

## Theme (F33, M20)

### Why

The house look is typed in five places, each with its own colors: `controls::ControlsStyle`, `vrui::F4VR_PANEL_STYLE`, `imgui::KeyboardStyle`, `imgui::PointerStyle`, and the ImGui style this mod sets with its generated `theme::applyStyle` ([`Theme.h`](../../src/Theme.h)). `GameMenu::applyPanelLook` puts them together by hand. The controls also need that ImGui style: `rowWidth()` reads `ScrollbarSize`, and the scroll bars, the gap between rows and the fade of a disabled row are ImGui's. A mod that calls the controls without this mod's `applyStyle` gets the framework's green controls among ImGui's default scroll bars and spacing.

### What the framework gets

`src/imgui/ImGuiTheme.h` and `.cpp`, namespace `f4cf::imgui`. `Theme` is a plain aggregate with a defaulted `operator==`. Its defaults are the framework's own look, its green, which a mod that sets no theme gets. A mod with a design sets every field from it, so a default that changes later does not change that mod.

A theme is the colors, the sizes, the font sizes and the spacing scale, which the gaps inside a control and between the rows are steps of. What a theme does not set is the shape of a drawn part: a mark, an icon, an arrow.

```cpp
struct Theme
{
    // the colors of ControlsStyle, with their defaults: accent, muted, track, hover, pressed, glow, rule,
    // frameBorder, onAccent, warning, onWarning, error, onError, popup, scrim

    // the background of a panel, and of the keyboard
    render::Color glass = render::Color::rgba(15, 15, 15, 217);

    // the fill of a navigation pane, so it reads as a fixed region
    render::Color plate = render::Color::rgba(10, 250, 120, 15);

    // a button the player must not miss
    render::Color important = render::Color::rgba(95, 212, 255);

    // the pointer's ray, its mark and the mark's border
    render::Color ray = render::Color::rgba(190, 255, 190, 200);
    render::Color mark = render::Color::rgba(215, 255, 215, 180);
    render::Color markBorder = render::Color::rgba(215, 255, 215, 100);

    // the sizes of ControlsStyle: controlHeight 48, frameRounding 4, frameBorderSize 2, popupRounding 8,
    // detailFontSize 18

    float titleFontSize = 26.0f;
    float bodyFontSize = 21.0f;
    float scrollbarSize = 16.0f;
    float grabWidth = 20.0f;
    float grabRounding = 2.0f;
    float disabledOpacity = 0.4f;

    // the spacing scale: every gap and padding of the controls is a step of it
    float space1 = 4.0f;
    float space2 = 8.0f;
    float space3 = 12.0f;
    float space4 = 16.0f;
    float space6 = 24.0f;
    float space8 = 32.0f;
    float space12 = 48.0f;

    // a panel's edge, corner and padding, in vrui units
    float panelBorderUnits = 0.08f;
    float panelCornerUnits = 0.5f;
    float panelPaddingUnits = 0.3f;

    void applyTo(ImGuiStyle& style) const;
    vrui::UIPanelStyle panelStyle() const;
    KeyboardStyle keyboardStyle(KeyboardStyle base) const;
    PointerStyle pointerStyle(PointerStyle base) const;
};
```

- **`applyTo`** sets every `ImGuiStyle` field and `ImGuiCol_` of the two tables of the design system's [ImGui style](design-system/project/README.md#imgui-style) section: it is the body of today's generated `theme::applyStyle`, with the theme's fields in place of the constants.
- **`panelStyle`** is the look `GameMenu::applyPanelLook` types now: the content and the border in `accent`, the background `glass`, and the three panel sizes.
- **`keyboardStyle`** takes a keyboard style and gives it the theme's colors: `background` is `glass`, `border` and `accent` are `accent`, `accentText` is `onAccent`, and `selection` is `accent` at alpha 90. The key grays, `widthShare` and `tiltDegrees` stay as `base` has them.
- **`pointerStyle`** takes a pointer style and gives it `ray`, `mark` and `markBorder`. The rest stays as `base` has it.

**The spacing scale.** `applyTo` sets ImGui's paddings and spacings from it, as the design system's table has them (`WindowPadding` is `space4`, `ItemSpacing` is `space3` and `space2`, ...). A control reads its gaps from the theme as it draws, in place of its constants:

| Step | The constants that read it |
| --- | --- |
| `space1` | `Choices`: `BUTTON_SLOP`, `LIST_GAP`, `ACTION_GAP`, `HEADER_ABOVE`. `Sliders`: `ARROW_GAP` |
| `space2` | `Choices`: `STEPPER_GAP`, `DROPDOWN_ARROW_GAP`, `LIST_PADDING`, `HEADER_UNDER`, `HEADER_INSET`. `Dialogs`: `DIALOG_BUTTON_GAP`, the x of `DIALOG_ITEM_SPACING`. `Frames`: `TAG_PADDING`. `Rows`: `ROW_RIGHT_GAP`, `MARK_GAP`, `HEADING_PADDING`. `Sliders`: `VALUE_GAP`. `Text`: `MARK_SPACE`. `Tree`: `ROW_INSET`, `NAME_GAP`, `TAG_GAP` |
| `space3` | `Choices`: `STEPPER_TEXT_PADDING`, `OPTION_PADDING`, `HEADER_GAP`. `Rows`: `RULE_GAP`, `HEADING_TEXT_INSET`. `Sliders`: `SLIDER_VALUE_PADDING`. `Tree`: `ROW_RIGHT_INSET`, `GROUP_RULE_GAP` |
| `space4` | `Choices`: `DROPDOWN_TEXT_PADDING`, `OPTION_END_PADDING`. `Dialogs`: `DIALOG_BUTTON_PADDING`, the y of `DIALOG_ITEM_SPACING`. `TextInputs`: `TEXT_INPUT_PADDING`. `Tree`: `TREE_INDENT` |
| `space6` | `Dialogs`: `DIALOG_PADDING` |

- The four that are public (`HEADING_TEXT_INSET`, `SLIDER_VALUE_PADDING`, `TEXT_INPUT_PADDING`, `TREE_INDENT`) become functions of the same meaning: `headingTextInset()`, `sliderValuePadding()`, `textInputPadding()`, `treeIndent()`.
- A slider's grab is the theme's `grabWidth` and `grabRounding`, and as high as `controlHeight` less `space2`, in place of `GRAB_WIDTH`, `GRAB_ROUNDING` and `GRAB_HEIGHT`.
- What stays a constant is a size of a drawn part or of one control's design: the marks, arrows and icons, `FILL_HEIGHT`, `STEP_ARROW_WIDTH`, `BUTTON_RING`, `TAG_LINE_HEIGHT`, the tree's arrow column and tag, the least width of a rule, `SWITCHER_CELL_WIDTH`, `DIALOG_WIDTH`, `DIALOG_BUTTON_MIN_WIDTH`, `SELECTED_BAR_TIP_DEPTH`, `LIST_MAX_OPTIONS`, and the timings. A gap or a padding that is not listed is given its step by the same rule.
- The controls of the later commits (F35 to F41) read the scale from the theme from the start.

`Controls` holds the theme in place of `ControlsStyle`, which is removed:

| Now | After |
| --- | --- |
| `controls::ControlsStyle` | `imgui::Theme` |
| `controls::get().setStyle(style)` | `controls::get().setTheme(theme)` |
| `controls::get().style()` | `controls::get().theme()` |
| `controls::get().colors()`, `tone(t)` | the same |

`setTheme` does three things:

1. It keeps the theme and packs its colors, as `setStyle` does. `ControlsColors` has every color of the theme, the new ones too (`glass`, `plate`, `important`).
2. It sets the keyboard and the pointer at once: `keyboard().setStyle(theme.keyboardStyle(keyboard().style()))` and the same for the pointer. A mod that wants its own ray or keyboard look calls their `setStyle` after `setTheme`.
3. It marks ImGui's style as not applied. The layer applies it with `applyTo` as it makes its main context, in place of `StyleColorsDark`, and `Controls::beginFrame` applies it again after a `setTheme`. `Controls::applyImGuiStyle()` is public for a test's own context.

The canvas font is loaded at the theme's `bodyFontSize` unless `setFontSizePixels` was called, which then wins.

### Changing the theme live

`setTheme` is called at any time, and the next frame is drawn in the new theme. This mod does it already as its config is changed while the menu is open: `applyPanelLook` runs again for a new `iBackgroundOpacity`.

- The controls keep no color and no size from one frame to the next: each reads `colors()` and `theme()` as it draws. What they hold across frames is a value and an id (a drag, an open list, an edit), which a new theme does not touch.
- ImGui's style is applied again as the next frame begins, and the keyboard and the pointer are set in the call.
- `applyTo` sets `ImGuiStyle::FontSizeBase` to `bodyFontSize`, which ImGui reads in each frame, so the body size follows too. `setFontSizePixels` is then the theme's `bodyFontSize` of a mod that sets no theme.
- A panel's edge and background are its `UIPanelStyle`: the mod gives the panel `theme.panelStyle()` again after `setTheme`, as `applyPanelLook` does.
- A mod's own drawing follows where it reads the theme as it draws. This mod's reads its generated constants until M29, so until then only `glass` changes live in it.

### devbench: the `theme` action

A look is tuned in the headset without a build: an agent or a script changes a field, and the next frame shows it. The value that is right is then typed into the mod's design, as a token of `tokens.json` here.

- **The fields' table.** `ImGuiTheme.cpp` has one table of the theme's fields: each with its name as in the struct, whether it is a color or a number, and its member. `themeFields()` gives it. A `static_assert` that the fields' sizes add up to `sizeof(Theme)` stops a build whose new field is not in it.
- **The action** is built in, beside `keyboard` in `DevBench.cpp`, so every mod's tool has it:
  - With no argument it answers `fields`, each field by its name: a color as `[r, g, b, a]` from 0 to 255, a size as a number; and `overridden`, whether devbench set any.
  - `set` is an object of fields and their values, in the same shapes: `{ "set": { "accent": [255, 180, 60, 255], "space2": 6 } }`. A name that is no field, or a value of the wrong shape, is an error that names it, and nothing is set.
  - `reset` true goes back to the theme the mod set.
- **Over the mod's theme.** `Controls` keeps the theme the mod set beside the one devbench set over it (`overrideTheme`, internal), which is the one drawn. `reset` drops the override, and so does the mod's next `setTheme`, as on a change of its config.
- **What follows:** the controls, ImGui's style, the keyboard and the pointer, from the next frame. A panel's edge and background follow as the mod gives the panel its style again, and a mod's own drawing where it reads the theme.

A mod with no design of its own has the default theme with no call:

```cpp
_panel = std::make_shared<imgui::UIImGuiPanel>("Spawner_Panel", 24.0f, 15.0f);
_panel->setStyle(controls::get().theme().panelStyle());
```

and a mod with a design applies its theme before the first canvas draws:

```cpp
controls::get().setTheme(myDesignTheme());
_panel->setStyle(controls::get().theme().panelStyle());
```

What is checked while implementing: `keyboard()` and `pointer()` take a `setStyle` with no game, as `Controls`' tests run under `test_no_game`. If one does not, `setTheme` keeps the derived styles and the layer sets them before its next frame, unless the mod set one itself since.

### Tests and docs (F33)

- `tests/linked/imgui/controls/ControlsThemeTests.cpp`: the default theme's `accent` is `vrui::F4VR_PANEL_STYLE.color`; `applyTo` sets `ScrollbarSize`, `DisabledAlpha`, `ItemSpacing` and `ImGuiCol_Text` from the theme; `panelStyle()` has the theme's colors and sizes; `keyboardStyle` and `pointerStyle` change the colors and keep the rest of `base`; `setTheme` packs `colors()` again and marks the style, which `applyImGuiStyle` then sets.
- The controls' tests call `controls::get().applyImGuiStyle()` in their `TestContext`.

**That the controls use the theme** is tested with a theme no default equals. `tests/linked/imgui/controls/ControlsGallery.h` draws every control in every state it has (at rest, under the ray, held, disabled, in each tone, selected, a list open, a dialog open, a text edited), and `probeTheme()` is a theme whose every color has an RGB of its own and whose every size and step is unlike its default and unlike the others. `ControlsThemeUseTests.cpp`:

- **No color but the theme's:** the gallery is drawn in the probe theme, and the RGB of every vertex of ImGui's draw data is one of the probe theme's colors. A color typed in a control, or read from ImGui's default style, fails it. The alpha is not compared: a disabled control and an edge's fringe change it.
- **The right color:** a case for each role names what is drawn in it: a frame's fill is `track`, its outline `frameBorder`, and `accent` while edited; a row under the ray is `hover` and a held one `pressed`; a primary button's fill is `accent` and its text `onAccent`; a value in the warning or error tone is that color; a list's and a dialog's background is `popup`, and the dim behind a dialog `scrim`; a heading's text is `muted` and its rule `rule`.
- **No size but the theme's:** each control's box in the probe theme is what the probe's sizes give: a row is `controlHeight` high; a list is padded `space2` with `space1` between its options; a dialog is padded `space6`; a tree's row is set in `space4` a level; a heading's text starts `space3` in; a tag's and a count's text is `detailFontSize` high. The boxes are the ones the controls return and their items' rects.
- **ImGui's style:** every field and color `applyTo` sets has its theme value, by a table of the two, and a field the table lacks is as `StyleColorsDark` leaves it.
- **Live:** the gallery is drawn in the default theme, `setTheme(probeTheme())` is called, and the next frame passes the cases above, with a list open and a slider held across the change.

Each control of a later commit adds itself to the gallery, so these cases cover it. `probeTheme()` is built from `themeFields()`, so a new field is probed too.

**The `theme` action** is a thin handler over two functions of `ImGuiTheme.cpp`, the theme as JSON and a JSON's fields set on a theme, which `ControlsThemeTests.cpp` tests with no devbench: it answers every field of the table in its shape; a `set` changes those fields and no other, and is drawn in the next frame; an unknown field and a wrong shape set nothing; `reset` and a `setTheme` of the mod each end the override. The devbench README gets the `theme` row.
- [`imgui/README.md`](../../external/F4VR-CommonFramework/src/imgui/README.md): a Theme section. The controls README: its Style section becomes the theme's. `CHANGELOG.md` `0.7.0`, with **Upgrading**: the renames of the table above, the ImGui style every canvas gets, and the font size.

### The mod's port (M20)

- `gen_theme.py` writes `theme::designTheme()` into `Theme.h`: an `imgui::Theme` with every field set from a token, by a table in the generator of which token is which field (`accent` from `green`, `warning` from `amber`, `important` from `accent`, `bodyFontSize` from `t-body`, `space2` from `space-2`, `panelBorderUnits` from `border-panel` over 48, ...). A field of `Theme` the table lacks stops the generator: it reads the field names from the framework's `ImGuiTheme.h`, which `themeFields()` has too.
- `gen_theme.py` no longer writes `applyStyle`: the framework's `Theme::applyTo` sets ImGui's style from the theme. It still checks the first table of the design system's [ImGui style](design-system/project/README.md#imgui-style) section against the tokens, and the section's intro says the framework sets the style from the mod's theme.
- `Theme.h` keeps every token as a constant: the mod's own code keeps `theme::color::GREEN` and `theme::SPACE_2`, which are what `designTheme()` gives the framework, so nothing else is edited but the uses of the four public gaps that became functions (`controls::HEADING_TEXT_INSET` in six places).
- `GameMenu::applyPanelLook`: `theme::designTheme()` with `glass` at the config's `iBackgroundOpacity`, given to `controls::get().setTheme`, then `pointer().setStyle(g_config.pointerStyle)`, the config's ray over the theme's, then `_panel->setStyle(theme.panelStyle())`. The lines that give the keyboard the panel's background go: the theme does it. `setFontSizePixels(theme::FONT_SIZE)` goes: the font is loaded at the theme's `bodyFontSize`.
- `ui::controlsStyle()` and `colorOf` in [`Drawing.cpp`](../../src/ui/Drawing.cpp) go.
- `FullPanel::draw` no longer applies the style (`_styleApplied` goes). [`tests/linked/TestPanel.h`](../../tests/linked/TestPanel.h) sets `theme::designTheme()` and calls `controls::get().applyImGuiStyle()`.

Accepted when the panel tests, the render tests and e2e pass unchanged, and the in-game panel, keyboard and ray look as before.

## Targets (F34, M21)

### Why

A test with the controllers asks where a button or a row is and points at it. This mod keeps those places in `ui::PanelTargets` and wraps the framework's controls only to record them: `panelButton`, `panelDialogButtons`, `panelList`. The next mod would write all of it again to be tested in the game.

### What the framework gets

`src/imgui/controls/Targets.h` and `.cpp`: `PanelTargets` as it is, with a kind and an area that are texts, so a mod names its own.

```cpp
struct Target
{
    std::string kind;
    std::string key;
    std::vector<std::string> path;
    ImRect rect;
};

struct ScrollTarget
{
    std::string area;
    float y = 0.0f;
    float max = 0.0f;
};

class Targets
{
public:
    static Targets& get();

    void setRecording(bool recording);
    bool isRecording() const;
    const std::vector<Target>& targets() const;
    const std::vector<ScrollTarget>& scrolls() const;

    void add(std::string_view kind, std::string_view key, const ImRect& rect, std::vector<std::string> path = {});
    void addScroll(std::string_view area);
    void addScroll(std::string_view area, float y, float max);

    // Internal: the layer, or a test's frame
    void beginFrame();
    void beginCanvas(const ImVec2& origin);
};

inline Targets& targets() { return Targets::get(); }
```

- **Recording** is off until asked, so a game with no test pays one check a target.
- **A frame:** `Controls::beginFrame` clears the targets and scrolls. The layer calls `beginCanvas` with the canvas's top left in ImGui's display before it draws the canvas's content. A rect is kept as the part its window shows, from that origin: the canvas's pixels, which is the space `Pointer::state()` gives the pointer in. It must give this mod's targets the places `PanelTargets::startFrame` gives them now (the canvas's window stands half the panel's border inside the canvas).
- **What records itself**, by a key a test knows without the mod:

| Control | Kind | Key |
| --- | --- | --- |
| `button`, `dialogButtons` | `button` | its label |
| `drawList` | `option` | each option's text, and the action entry's; its scroll as area `list` |
| `searchField` (F35) | `search` | `Field`, `Clear` |
| `titleBar` (F38) | `crumb`, `titleButton` | the crumb's text, the button's name |
| the panes (F37) | | their scroll, as the area the mod names |

- **What the mod records**, with `controls::targets().add`: a row whose key is the mod's own, as a tree's row by its node and path, a setting's row by its id, a section by its text.
- **devbench:** a built-in `targets` action beside `keyboard` in `DevBench.cpp`, with the answer of this mod's `targets` action: `recording`, `targets` (`kind`, `key`, `x`, `y`, `width`, `height`, and `path` where it has one) and `scrolls` (`area`, `y`, `max`). The first call starts the recording and answers none.

With two panels at once a target does not say which canvas it is in. A `canvas` name is added when a mod has two panels with targets.

### Tests and docs (F34)

- `ControlsTargetsTests.cpp`, from this mod's `PanelTargetsTests.cpp`: nothing is kept while not recording; a rect is clipped to its window and kept from the canvas's origin; one scrolled out is not kept; a frame clears; a `button` and a `drawList` record themselves.
- The controls README: a Targets section. The devbench README: the `targets` row. `CHANGELOG.md`.

### The mod's port (M21)

- `src/ui/PanelTargets.h/.cpp` go. The kinds become texts in `src/ui/Targets.h` (`ui::target::TREE = "tree"`, ...), with the names `targetKindName` gives now, so e2e reads the same answer.
- Every `panelTargets().add(TargetKind::X, ...)` becomes `controls::targets().add(ui::target::X, ...)`, and `addScroll(ScrollArea::X)` the area's text.
- `panelButton`, `panelDialogButtons` and `panelList` go: their callers call `controls::button`, `controls::dialogButtons` and `controls::drawList`.
- The mod's `targets` devbench action and `runTargets` go: the built-in one answers. [`tests/e2e/controllers.py`](../../tests/e2e/controllers.py) is unchanged.
- `PanelTargetsTests.cpp` keeps what is the mod's: which rows record which kind and key.

## Search field (F35, M22)

[`SearchField.cpp`](../../src/ui/SearchField.cpp) knows nothing of a mod but its two help texts and its placeholder.

`src/imgui/controls/SearchFields.h`:

```cpp
constexpr float SEARCH_FIELD_HEIGHT = 40.0f;

struct SearchField
{
    // shown while the query is empty and not edited, as "Search 11 mods"
    std::string_view placeholder;

    // how many match, at the right of the query: none for none
    std::optional<std::size_t> count;
};

struct SearchDrawn
{
    // the query as it is typed, and whether Clear emptied it
    std::optional<std::string> write;
    bool cleared = false;

    // the ray is on the field, or on Clear
    bool pointed = false;
    bool clearPointed = false;

    // a press started the edit, or cleared
    bool pulse = false;
};

SearchDrawn searchField(const char* id, const ImVec2& pos, float width, std::string_view query, const SearchField& field = {});
```

- It draws what the mod's field draws: the frame, the magnifier, the placeholder in the detail size or the query, the count, and Clear with its cross while there is a query.
- A press edits it on the live, bare text input, so `controls::get().isTextEdited(ImGui::GetID(id))` says it is edited, Back ends the edit, and a physical keyboard's Escape keeps the query, as now.
- The caller keeps the query: it writes `write`, and empties it on `cleared`.

**The mod (M22):** `ui::SearchField` keeps the query, `setQuery`, `clear`, `isEdited` and the two help texts, and its `draw` is one `controls::searchField` call. `drawMagnifier` and `drawCross` go.

## Icons and the row popup (F36, M23)

**Icons.** The drawn icons are in five files of the mod: the 18 of a config spec's button ([`ButtonIcons.cpp`](../../src/ui/ButtonIcons.cpp)), Minimize and Close ([`TitleBar.cpp`](../../src/ui/TitleBar.cpp)), Play (`Haptics`), and the magnifier and the cross, which F35 moved. `src/imgui/controls/Icons.h` has them as one set:

```cpp
enum class Icon : std::uint8_t
{
    None, Search, Cross, Close, Minimize,
    Play, Stop, Bolt, Target, Check, Window, External, Link, Gear, Import, Export, Copy, Eye, Hand, Sound, Pin, Refresh, Trash,
};

// the box an icon is drawn in at scale 1
constexpr float ICON_SIZE = 24.0f;

void drawIcon(Icon icon, const ImVec2& center, ImU32 color, float size = ICON_SIZE);
```

Each icon is drawn as the mod draws it now, strokes scaled with its size. `Marks.h` keeps the marks, the arrows, Reset and Undo. The mod maps `model::ButtonIcon` to `controls::Icon` in `ButtonIcons.cpp`, which keeps only that, and the title bar, the haptic's Play and the search field draw with `drawIcon`.

**The row popup.** `ui::beginRowEditor` ([`Drawing.cpp`](../../src/ui/Drawing.cpp)) is generic: a popup of a width and a height, centered on its row as far as the window lets it, under or over the row as `popupTop` places it, padded the theme's `space4` and outlined in the accent. It moves to `Dialogs.h` as `bool beginRowPopup(ImGuiID id, const ImRect& row, float width, float height)`, ended by `ImGui::EndPopup`. The binding, color and transform editors of the mod call it.

Tests: each icon draws inside its box at two sizes (the draw list's vertices are within it); the popup is placed under a row with room, over one without, and never past its window's sides.

## Panes (F37, M24)

The panel's body is two panes: the tree's, a plate with a header that stays and rows that scroll, and the content's, which scrolls from its top as its page changes. `ModTree::draw` and `FullPanel::drawContent` build them from `BeginChild` with the style pushed by hand. `src/imgui/controls/Panes.h`:

```cpp
// the width of a row of a navigation pane of a width: without its padding and the room of the tip and the scroll bar
float navRowWidth(float paneWidth);

bool beginNavPane(const char* id, float width, float height);
bool beginNavRows(const char* id, float paneWidth, bool showTop = false);
void endNavRows(std::string_view scrollArea = "tree");
void endNavPane();

bool beginContentPane(const char* id, float height, std::optional<float> scrollTo = {});
void endContentPane(std::string_view scrollArea = "page");
```

- **`beginNavPane`** is a child of the theme's `plate`, padded `space2`. The caller draws its header at the cursor, as the search field, and gives it its room with `ImGui::Dummy`.
- **`beginNavRows`** is the child under the header that scrolls, with the tree's spacing between rows (`space1`), wider than its rows by the room of the selected bar's tip and the scroll bar. `showTop` scrolls it to its top.
- **`beginContentPane`** is the child beside it, which fills the width left. `scrollTo` sets its scroll in this frame: 0 as the page is another, or a place kept for a page.
- **Each `end` is called whether or not its `begin` returned true**, as ImGui's `EndChild`. It records the pane's scroll as a target's area.
- The gap between the panes is the caller's `ImGui::SameLine(0.0f, controls::get().theme().space4)`.

**The mod (M24):** `ModTree::draw` and `FullPanel::drawContent` call them. `RIGHT_ROOM` and the row width's arithmetic in `ModTree.cpp` go to `navRowWidth`. `theme::NAV_WIDTH` stays the mod's.

Tests: a nav pane's rows are `navRowWidth` wide with and without a scroll bar; `showTop` and `scrollTo` set the scroll; the scrolls are recorded under their areas.

## Title bar (F38, M25)

`drawTitleBar` ([`TitleBar.cpp`](../../src/ui/TitleBar.cpp)) is breadcrumbs that shorten to fit, and buttons at the right. What is the mod's is which crumbs there are and what a press on one does. `src/imgui/controls/TitleBars.h`:

```cpp
// ui::CrumbLook, which moves here: a link is pressed, the current one is the last, a selected one is a solid fill
enum class CrumbLook : std::uint8_t
{
    Link,
    Current,
    Selected,
};

struct Crumb
{
    std::string_view text;
    CrumbLook look;

    // false for a crumb that keeps its width where the second one would shorten
    bool shortens = true;
};

struct TitleButton
{
    // its name, which is its ID and its target's key
    const char* name;
    Icon icon = Icon::None;

    // drawn in place of an icon, as "?", and the wider text of one that stands out
    std::string_view text;
    std::string_view highlightText;
};

struct TitleBar
{
    std::span<const Crumb> crumbs;

    // from the right edge
    std::span<const TitleButton> buttons;

    // said after the last crumb, as " (filtered)"
    std::string_view suffix;
    float height = 64.0f;
};

struct TitleBarDrawn
{
    std::optional<std::size_t> pressedCrumb;
    std::optional<std::size_t> pressedButton;
    std::optional<std::size_t> pointedButton;
};

TitleBarDrawn titleBar(const TitleBar& bar);
```

- The crumbs are in the theme's `titleFontSize`, with `»` between them, and shorten by the rules of today: with more than one crumb after the second, the ones between the second and the last become one `…`, which is not pressed; then the second shortens, down to a least width, unless it does not `shorten`; the last ends in an ellipsis. The first is never shortened.
- A button is an `iconButton` as high as a control. One with a `highlightText` is drawn wider with that text, as the mod's Help while it stands out.
- A rule in the theme's `rule` ends the bar. Crumbs and buttons record themselves as targets (`crumb`, `titleButton`).

**The mod (M25):** [`Breadcrumbs`](../../src/ui/Breadcrumbs.h) keeps `breadcrumbsOf` and each crumb's target; `drawTitleBar` builds the `TitleBar`, calls `controls::titleBar`, and makes the help line's row of the pointed button with its help text. `theme::TITLEBAR_HEIGHT` is the `height` it gives.

Tests, new in the framework, as the mod's `BreadcrumbsTests.cpp` is about which crumbs there are and stays: the crumbs fit; the middle ones become `…`; the second shortens to its least width; a second that does not shorten keeps its width; a press gives the crumb's place.

## Footer (F39, M26)

`src/imgui/controls/Footers.h`:

```cpp
struct FooterButton
{
    const char* label;
    ButtonLook look = ButtonLook::Secondary;
    bool disabled = false;
};

struct FooterDrawn
{
    // the place of the pressed button, -1 for none
    int pressed = -1;

    // the room at the left of the buttons, for a help line
    ImRect left;
};

FooterDrawn footer(const ImVec2& pos, std::span<const FooterButton> buttons, float height = 72.0f, float buttonWidth = 160.0f);
```

It draws the rule over it and the buttons at the right in their order, the last at the right edge, `space3` apart, in the middle of its height, and takes its room. `left` starts `space4` in and ends `space3` before the first button.

**The mod (M26):** `FullPanel::drawFooter` gives Discard and Save (Revert and Keep for an MCM mod) with their disabled states, acts on `pressed`, and draws its `HelpLine` in `left`. `HelpLine` stays the mod's: what it says of a row is about settings.

## Controls with their label, and the press pulse (F40, M27)

Today a slider is a row the caller adds, an id, `sliderEdit`, a write, then `drawSlider` at a `right` and a `top`. That is what this mod's rows need, as they draw a pending or invalid look between the edit and the draw. A plain page wants one call. `src/imgui/controls/Labelled.h`, built from the controls that are there:

```cpp
// the room of a control at the right of its row
constexpr float CONTROL_COLUMN_WIDTH = 392.0f;

struct Labelled
{
    Mark mark = Mark::None;
    Tone tone = Tone::Normal;
    bool disabled = false;
    std::span<const TextRange> found;
};

struct SliderText
{
    int decimals = 0;
    std::string_view unit;
};

bool labelledSwitcher(std::string_view label, bool& on, const Labelled& row = {});
bool labelledSlider(std::string_view label, float& value, const SliderRange& range, const SliderText& text = {}, const Labelled& row = {});
bool labelledStepper(std::string_view label, std::size_t& chosen, std::span<const std::string_view> options, const Labelled& row = {});
bool labelledDropdown(std::string_view label, std::size_t& chosen, std::span<const std::string_view> options, const Labelled& row = {});
bool labelledText(std::string_view label, std::string& text, const TextOptions& options = {}, const Labelled& row = {});
bool labelledButton(std::string_view label, std::string_view buttonText, const Labelled& row = {}, Icon icon = Icon::None);
void labelledValue(std::string_view label, std::string_view value, const Labelled& row = {});
float drawRowLabel(const ImVec2& pos, float right, std::string_view text, Mark mark = Mark::None, std::span<const TextRange> found = {});
```

- **A row** is `addRow(true)`: the label `headingTextInset()` in, cut with an ellipsis before its mark, and the control at the row's right in `CONTROL_COLUMN_WIDTH`, which a slider with its value's box, a stepper, a dropdown and a text input each fill.
- **It returns whether the value changed** in this frame, and has written it: a slider as its drag is let go or a step moves it, a text as its edit ends.
- **The ID** is the label's, in ImGui's ID stack, so two rows of one label need `ImGui::PushID`. The row is the last item: `ImGui::IsItemHovered()` says the ray is on it, as for a help line.
- **`labelledText`** is called for a row scrolled out too, as `textEdit` needs.
- **`drawRowLabel`** is the label alone, for a mod's own row. It returns the width it drew.
- **Targets:** each records its row as `row` by its label.

**The press pulse.** A labelled control returns only its change, so it cannot hand its pulse to the caller. `Controls` gets `setPressPulse(std::optional<vrcf::HapticPattern>)`, whose default is the lightest pattern, `Tick`, and `pulse()`, which asks for it. The layer plays it on the pointing hand after the frame, once however often it was asked, so `Controls` calls no game and a test reads `pulsed()`. The labelled controls call `pulse()` where the controls under them report theirs, and a mod calls it for its own presses. The controls that report `pulse` to their caller still do, and play nothing themselves: this mod's rule of which press gets a pulse stays its own.

Tests: each labelled control on a headless context: a press changes the value and returns true once; a disabled one changes nothing; the slider writes on release; `pulsed()` is true in the frame of the press.

**The mod (M27):** this mod's rows stay on the controls under them, so nothing is ported. `tests/linked/ui/ControlsRenderTests.cpp`, tagged `[render]` as the panel's render tests and saved as they are, draws a page of every labelled control in this mod's theme: each at rest, under the ray, disabled, with a mark, and in the warning and error tones, with a dropdown's list open. It is the look's check before a panel has them in the headset.

## A clipped list of rows (F41, M28)

A page adds every row, also one scrolled out, which draws nothing. That holds for a mod's settings and not for a list of every item of the game. `Rows.h` gets:

```cpp
// calls drawRow for the rows that are shown, and `keep` too where it is one: each call adds one row
void clippedRows(std::size_t count, const std::function<void(std::size_t index)>& drawRow, std::optional<std::size_t> keep = {});
```

It is ImGui's `ImGuiListClipper` over rows of the control's height with the spacing between them, so the scroll bar is that of all the rows. `keep` is a row drawn while scrolled out, as the one whose text is edited. Each call of `drawRow` adds exactly one row, as `addRow` or a labelled control does.

Tests: of 10,000 rows only the shown ones are drawn; the content is as high as all of them; `keep` is drawn while scrolled out.

**The mod (M28):** `ControlsRenderTests.cpp` gets a content pane of 10,000 labelled rows scrolled to its middle.

## The mod reads the theme (M29)

### Why

The mod's own drawing reads the constants of `Theme.h`: about 550 reads in 33 files of `src/`. They are the values `designTheme()` gives the framework, so the panel is right, and it is drawn in them whatever theme is set. With the theme read as it draws, the `theme` action tunes the whole panel in the headset, a part that moves to the framework later moves as it is, and a theme the player chooses needs no more of the drawing.

It is the last commit: M22 to M26 take the search field, the icons, the panes, the title bar and the footer out of the mod, and their reads with them.

### What changes

[`Drawing.h`](../../src/ui/Drawing.h) gets the two the drawing reads:

```cpp
// the theme that is set, and its colors packed for ImGui
inline const f4cf::imgui::Theme& look() { return controls::get().theme(); }
inline const controls::ControlsColors& colors() { return controls::get().colors(); }
```

Each read of a constant that is a field of the theme becomes a read of that field:

| `Theme.h` | Read as |
| --- | --- |
| `color::GREEN`, `GREEN_MUTED`, `ON_GREEN` | `colors().accent`, `muted`, `onAccent` |
| `color::AMBER`, `ON_AMBER`, `RED`, `ON_RED` | `colors().warning`, `onWarning`, `error`, `onError` |
| `color::ACCENT` | `colors().important` |
| `color::TRACK`, `HOVER`, `PRESSED`, `GLOW`, `RULE`, `FRAME_BORDER`, `POPUP`, `SCRIM`, `PLATE`, `GLASS` | `colors()`'s of the same name |
| `color::DISABLED` | `colors().accent` at `look().disabledOpacity` |
| `SPACE_1` ... `SPACE_12` | `look().space1` ... `space12` |
| `CONTROL_HEIGHT`, `BORDER_FRAME`, `RADIUS_FRAME`, `RADIUS_POPUP`, `RADIUS_GRAB`, `GRAB_WIDTH`, `SCROLLBAR_SIZE`, `OPACITY_DISABLED` | `look().controlHeight`, `frameBorderSize`, `frameRounding`, `popupRounding`, `grabRounding`, `grabWidth`, `scrollbarSize`, `disabledOpacity` |
| `FONT_SIZE_TITLE`, `FONT_SIZE_BODY`, `FONT_SIZE_DETAIL` | `look().titleFontSize`, `bodyFontSize`, `detailFontSize` |
| `ROW_PITCH` | `look().controlHeight + look().space2` |

- **What stays a constant** is the mod's own layout, which no theme sets: `PANEL_WIDTH`, `PANEL_HEIGHT`, `NAV_WIDTH`, `TITLEBAR_HEIGHT`, `FOOTER_HEIGHT`, `CONTROL_WIDTH`, `VALUE_WIDTH`, and the vrui wrist buttons' `GLASS_WIDGET`, `RADIUS_WIDGET` and `BORDER_WIDGET`.
- **The `constexpr` sizes made of a theme's field** become functions that give the same number: `DROPDOWN_WIDTH`, `HAPTIC_FRAME_WIDTH`, `FIELD_WIDTH` (`TextInputs`), `PRESETS_HEIGHT` (`ColorEditor`), `EDITOR_HEIGHT_NO_DIRECTION` and `PART_LINE_WIDTH` (`BindingEditor`), `MOD_CRUMB_MIN_WIDTH`, where M22 to M26 left them in the mod. `AdjustInWorld`'s `PENDING_COLOR` is `look().warning`, which is a `render::Color` already.
- **`Theme.h`** keeps every token as a constant: `designTheme()` is built of them, and the tests read them as the values they expect.
- **The check that it stays so:** `gen_theme.py`, which has the table of which token is a field, also fails when a file of `src/` other than `Theme.h` names the constant of one. Its pre-commit hook runs on a change of `src/` too.
- The mod's design docs keep the design system's names (`green`, `amber`): the code's are the theme's roles.

### Accepted when

- The images of the render tests (`ModsConfigFO4VR_linked_tests "[render]"`, in `build/tests/render/`) are the same files, byte for byte, as before the commit.
- The panel tests pass unchanged, and e2e.
- In the headset, the `theme` action with another `accent` and `space2` changes the whole panel: the tree, the rows, the editors, the title bar and the footer.

## What stays in the mod

- Everything about a setting, a store, a config spec, MCM and Save: `SettingRow`, `RowView`, `ButtonView`, the pending, applies and invalid marks, `drawLabel` with them, `Sections`, the rich text of a config spec's markup.
- The editors of the framework's types (binding, color, transform, haptic).
- `ModTree`: the nodes it walks, the open categories, the scroll to what a press opened. The framework has the tree's rows and its pane.
- `HelpLine`, the order of Back, `FullPanel` and its sessions.
- The design system, its tokens and its generator: the mod's design, which it applies as its theme.

## Not planned

- **A theme for each canvas.** `Controls` exists once in a mod's DLL, so all its canvases have one theme, and the controls' frame is the main context's: a canvas of the framework's own context, as the keyboard's, is not drawn with them.
- **A denser look.** The controls' gaps do not follow `controlHeight`. A list of shorter rows is designed when a page needs one.
- **A theme from one color.** A mod's theme sets every field from its design. A helper that derives the colors that follow an accent is written when a mod wants one.
- **A theme the player chooses.** After M29 the drawing would follow one. A setting for it, and the themes to choose from, are designed when the mod wants them.
- **New controls** (tabs, tables, toasts): each with its use case in the next mod.
