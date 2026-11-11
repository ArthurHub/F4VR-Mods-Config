Mods Config FO4 VR is one settings panel for every Fallout 4 VR mod, drawn with Dear ImGui on a curved CommonFramework panel and worked with a controller ray. It should read as a large projected Pip-Boy: dark glass, one phosphor green, a strong border, big targets, and a selection bar that points at what it opened. These rules are for the C++ that draws it. The previews recreate the ImGui output at 1 layout pixel to 1 CSS pixel.

## Principles

- **One hue.** Draw text, borders, fills and focus in `green` (#0afa78, the framework's `F4VR_PANEL_STYLE` color) at full value or as a tint. The only other hues are `amber` for pending and `red` for invalid, which always come with a drawn mark, and `accent` for a button its mod marks important, which no state uses.
- **Solid green means chosen.** Use a solid `green` fill only for the selected tree row, the first breadcrumb while the menu's front page is open, the current option, the On half of a toggle, and the primary button, with `on-green` text on it. Everything else is outline or tint.
- **Never color alone.** A pending setting has the amber dot, or the amber mark of when it takes effect, an invalid one has the warning triangle and its reason in the help line, and a toggle says Off or On in words.
- **Nothing moves.** The help line, the reset slot of every row, Discard and Save, and the control column keep their place whether they are in use or not. Only text changes; nothing pressable appears or disappears under the ray.
- **Big enough for a ray.** Every pressable thing is at least `control-height` (48 px, one vrui unit) tall.

## Scale

- Lay out in ImGui pixels. A `UIImGuiPanel` gets 48 px per vrui unit, so the Full panel at 24 x 15 units is `panel-width` x `panel-height`: 1152 x 720 px.
- At the defaults in `ConfigMenu.cpp` (scale 6, 120 game units in front of the HMD, curve radius 25), one layout pixel is about 0.06 degrees at the panel's middle, about 17 px a degree. A `t-body` line is about 1.2 degrees tall, and a 48 px target about 2.9 degrees.
- Set `setFontSizePixels` to the `t-body` size, 21 (`theme::FONT_SIZE`), and keep `setSupersample` at 1.5. The panel texture is then 1728 x 1080, about 25 texels a degree. Raise the factor only if text looks soft in the headset: memory grows with its square.
- Set `TouchExtraPadding` to `space-1` on each side. It widens every hit box into the 8 px gap between items without moving anything, which absorbs hand jitter.

## Color

Draw the Full panel on `glass`. The framework's own `F4VR_PANEL_STYLE` uses alpha 166 (0.65); the panel uses 217 (0.85), because at 0.65 green text drops to 4.3:1 when a bright sky is behind it. Keep `glass-widget`, the framework's exact value, for the vrui wrist buttons only.

Every text color below is checked with a white world behind the glass, the worst case:

| Text | On `glass` | On `hover` | On `pressed` | Use |
| --- | --- | --- | --- | --- |
| `green` | 9.0:1 | 6.8:1 | 5.6:1 | All primary text and every value |
| `green-muted` | 8.3:1 | 6.3:1 | 5.2:1 | Descriptions, units, counts, status |
| `amber` | 7.7:1 | 5.8:1 | 4.8:1 | A pending value and the unsaved count |
| `red` | 6.0:1 | 4.5:1 | — | An invalid value and its reason, and a dangerous button; never on `pressed` |
| `accent` | 7.4:1 | 5.6:1 | 4.6:1 | An important button: its outline, label and icon |
| `on-green` on `green` | 13:1 | | | Text on any solid green fill |

- Tint with green, never with gray: `plate` (6%) for fixed regions, `track` (10%) for the empty part of a control, `hover` (12%) under the ray, `pressed` (20%) while the trigger is held.
- Outline controls in `frame-border` (3.7:1). Draw section rules in `rule`, which is decorative.
- Draw a pressed outline and a dragged grab in `glow`, close to the pointer's `mark`, so feedback seems to come from the ray.
- Keep the pointer as the config sets it: `ray`, `mark`, `mark-border`. They are `[Pointer]` defaults, not panel colors.
- Do not add white text, gray UI, or another accent. The exceptions are a color that a mod gives the text of its own `text` row, which is drawn as given, and the swatches, tracks and picker of a color setting (`Color`), which show the color they edit.

## Type

- Draw the panel's own text in one face: Roboto Medium, the font the framework embeds (`render::TextFont`). Hierarchy comes from size, color and case. A mod's text is bold or italic where its tags say, in the canvas font's bold and italic faces (`imgui::canvasFont`), and underlined where they say.
- Use three sizes, pushed with `ImGui::PushFont(nullptr, size)` around the call and popped after it: `t-title` (26 px), `t-body` (21 px), `t-detail` (18 px). ImGui rasterizes each size it draws, so each is as sharp as the body, and rounds a size to a whole pixel, so each is a whole number. Nothing smaller than `t-detail`.
- ImGui sizes a font by its ascent minus descent (`stbtt_ScaleForPixelHeight`), and Roboto's is 2400 of 2048 units, so ImGui's 21 px is a 17.92 px em in CSS. The type tokens carry the CSS em so previews match the headset.
- ImGui's line advance is the font size. Wrapped text has no extra leading. Help gets two `t-body` lines, or three `t-detail` lines when it needs them, and an ellipsis past that.
- A character renders when the font has it: ImGui loads each glyph as it is first drawn. Roboto has Latin with its extensions, Greek, Cyrillic and Vietnamese, and `°`, `×`, `±`, `•`, `›` and `…`. It has no arrows, check marks or triangles, and no Chinese, Japanese, Korean or Arabic: a character it does not have is drawn as `?`. Draw the icons (see Iconography).
- Write `MODS CONFIG FO4` and the wrist button labels in capitals. Write everything else in sentence case, and setting names exactly as the mod gives them.

## States

Every interactive item has these states, each told apart by more than color:

| State | When | How it looks | ImGui |
| --- | --- | --- | --- |
| Idle | | Text in `green`, frames in `track` with a `frame-border` outline | |
| Hover | The ray is on it | `hover` fill and a 2 px `frame-border` outline round the whole row | `IsItemHovered()`; draw the outline with `AddRect` |
| Focused | The thumbstick adjusts it: the last control pressed | A 2 px `green` ring, no fill | Track the last-activated id; draw the ring yourself |
| Active | Trigger held, or a drag | `pressed` fill, 2 px `glow` outline, grab in `glow` | `IsItemActive()`; `ButtonActive`, `SliderGrabActive` |
| Selected | The open page, the current option | Solid `green`, `on-green` text; a tree row ends in a 10 px tip inside it | Push `ImGuiCol_Text` = `on-green` before the item |
| Pending | Differs from the saved value | `amber` dot after the name; the value, fill and grab in `amber`; Discard in the Reset slot | Store state; nothing in ImGui |
| Invalid | Cannot be saved | The invalid mark after the name; value in `red` with a `red` outline; the reason in the help line | Store state |
| Disabled | Condition off, plugin missing | The whole row at `opacity-disabled` and no hover | `BeginDisabled()` with `DisabledAlpha` 0.4 |

ImGui draws a label in `ImGuiCol_Text` whatever the item's state, so a solid fill can only be used for a state that is known before the item is drawn. That is why a press is a stronger tint and not an inverted bar.

## ImGui style

Set these once, before the first canvas draws. In the mod, `mods_config::theme::applyStyle` does it: `src/Theme.h` is generated from these two tables and `tokens.json` by `tools/gen_theme.py`, with every token as a constant (`theme::color::GREEN`, `theme::CONTROL_HEIGHT`). Keep the tables in this form, because the generator reads them, and a value in the first table that differs from its token stops it.

| `ImGuiStyle` | Value | Token |
| --- | --- | --- |
| `WindowPadding` | 16, 16 | `space-4` (popups, the dialog, child panes) |
| `FramePadding` | 16, 12 | `space-4`, `space-3` → 48 px frames |
| `ItemSpacing` | 12, 8 | `space-3`, `space-2` |
| `ItemInnerSpacing` | 8, 8 | `space-2` |
| `TouchExtraPadding` | 4, 4 | `space-1` |
| `IndentSpacing` | 32 | `space-8` |
| `ScrollbarSize` | 16 | `scrollbar-size` |
| `GrabMinSize` | 20 | `grab-width` |
| `FrameBorderSize`, `PopupBorderSize`, `WindowBorderSize` | 2 | `border-frame` |
| `ChildBorderSize` | 0 | |
| `FrameRounding`, `ScrollbarRounding` | 4 | `radius-frame` |
| `WindowRounding`, `ChildRounding`, `PopupRounding` | 8 | `radius-popup` |
| `GrabRounding` | 2 | `radius-grab` |
| `SeparatorTextBorderSize` | 2 | `border-frame` |
| `SeparatorTextAlign`, `SeparatorTextPadding` | (0, 0.5), (8, 8) | |
| `SelectableTextAlign` | (0, 0.5) | |
| `DisabledAlpha` | 0.4 | `opacity-disabled` |

| `ImGuiCol_` | Token |
| --- | --- |
| `Text` | `green` |
| `TextDisabled` | `green-muted` (secondary text, through `ImGui::TextDisabled`) |
| `WindowBg`, `ChildBg`, `BorderShadow`, `Button` | transparent: the canvas draws `glass` |
| `PopupBg` | `popup` |
| `Border` | `frame-border` |
| `FrameBg`, `ScrollbarBg` | `track` |
| `FrameBgHovered`, `ButtonHovered`, `HeaderHovered` | `hover` |
| `FrameBgActive`, `ButtonActive`, `HeaderActive`, `TextSelectedBg` | `pressed` |
| `Header` | `green` (selected; push `Text` = `on-green`) |
| `CheckMark` | `on-green` |
| `SliderGrab`, `ScrollbarGrabHovered`, `NavCursor` | `green` |
| `SliderGrabActive`, `ScrollbarGrabActive` | `glow` |
| `ScrollbarGrab` | `frame-border` |
| `Separator` | `rule` |
| `ModalWindowDimBg` | `scrim` |

## Layout

- **Full panel** (`FullPanel`): title bar (`titlebar-height`) holding the breadcrumbs, then the mod tree (`nav-width`) and the content pane side by side with a `space-4` gap, then the footer (`footer-height`). The tree keeps its place and its width, with or without a selected mod.
- **Tree** (`ModTree`): the mods in one alphabetical list under a `Mods` header with the count. From 20 mods (`iCategoriesMinMods`) the list is grouped: categories are the top level, each a muted row with a rule and its count, and the mods are indented under them (`FullPanelCategories`). Selecting a mod expands it to its pages and opens its front page (`FullPanel`); a page is a row under it (`FullPanelPage`), and a config spec's page that has pages of its own is expanded to them while it or one of them is selected (`FullPanelSubpage`). Each level is set in by `IndentSpacing` (`space-8`), up to the third, and every row keeps the arrow's column, so the names of a level line up: a mod always has its arrow, and a page has one only when it has pages (`ModTree`). The menu's front page (`FullPanelHome`) has no row: the first breadcrumb is its entry. About nine rows fit, so the rows scroll under the header, with the content pane's scroll bar in a room the tree always keeps; a row selected from outside the tree is scrolled into view.
- **Breadcrumbs** (`Breadcrumbs`): `MODS CONFIG FO4 » <mod> » <page>`, with a crumb for each page above the page, in `t-title` in the title bar, so the content pane gives its height to settings. The last crumb is `green`, the others `green-muted` and pressable: `MODS CONFIG FO4` goes to the menu's front page, the mod to its front page, a page to it. On the menu's front page `MODS CONFIG FO4` is the only crumb and is selected: solid `green`, `on-green` text. When the crumbs do not fit, the pages between the mod and the page become one `…`, which is not pressed, and then the mod's name shortens with an ellipsis. The first crumb and the last are never shortened.
- **Back** follows the same path: a page to the page it is under, or to its mod's front page, a mod's front page to the menu's front page, where the first breadcrumb is selected. On the menu's front page it closes the menu.
- **Search** (`Search`, `FullPanelSearch`): the tree's header is the field, "Search 11 mods". A query filters the tree to the mods and pages that match, with how many, with the match underlined in each name, and the page that is shown to its rows that match, each its own row.
- **Content pane**: only the rows, which scroll with the thumbstick behind a `scrollbar-size` position bar. A page's own help is its first item, in `green-muted`, and scrolls with it.
- **Sections** (`SectionHeader`): a heading is `SeparatorText`. A section in a section is set in a level, `space-3`, with its rows, up to the third level, and its rows end where the others do, so the controls stay in one column. While it is open, a `rule` line runs from under its heading, `space-1` in, down to the bottom of its last row and turns toward the rows, stopping `space-1` short of their frames. A heading's text is `green-muted`. A section that folds has a heading row that is pressed: its arrow, its text in `green-muted`, under the ray too, and a heading's rule. Folded, it says what it hides at the end of the rule: the warning mark and `n invalid` in `red`, then the pending dot and `n unsaved` in `amber`, and the help line says it after the section's help.
- **Developer tools** (`Developer`, `FullPanelDeveloper`, `FullPanelConfigSpecReport`), for a config spec's author, behind a switch in the menu's own settings: a Developer section at the end of the menu's front page, with a filter and a row for each config spec's errors and warnings, and a config spec's report with its live copy and its problems, each a row that goes to the row it names. A player never sees them.
- **Footer**: Discard and Save fixed on the right, named Revert and Keep for an MCM mod. On the left, the help line (`DescriptionArea`): an error, else "Name: help" for the setting or option under the ray, else the session status.
- **Indent**: a heading's text and a folding heading's arrow start `space-3` into its row, as a row's name does in its frame, so a heading lines up with the rows beside it; the page's texts start there too. A row under a heading is a level in, `space-3`, so its frame starts under the heading's text and it reads as its heading's. A row under no heading is at the content's left, where a heading's row starts. A section in a section is a level further, up to the third. The content's levels are `ImGui::Indent(theme::SPACE_3)`: `IndentSpacing` stays the tree's `space-8`.
- **Setting row**: the name, `space-3` inside the row's frame, with its marks after it (the pending mark, then the warning or the invalid mark), then the `control-width` control column: the control right-aligned, then a 48 px Reset slot that is always reserved. There is no gutter: most rows have no mark, and the name has their room.
- **Minimized**: the panel is hidden; only the wrist buttons remain (`WristButtons`).
- Open dropdowns, popups and the dialog inside the panel. The ray never has to leave it.

## Iconography

There is no icon font, and Roboto has no arrows or check marks, so every icon is drawn:

- Tree and dropdown arrows: `ImGui::RenderArrow` or a filled triangle, 14 px.
- The current option's check: `ImGui::RenderCheckMark`.
- Pending: `AddCircleFilled`, 10 px across, `amber`. A changed setting whose mod says when it takes effect has its mark in its place, 16 px, `amber`: a broadcast for `live`, a disk for `save`, a power mark for `restart` (see `TakesEffect`).
- Invalid: a filled `red` triangle 20 x 18 with `!` in `on-red`.
- A warning: a hollow triangle 20 x 18, a 2 px `amber` outline with `!` in `amber`, after the row's name (see `RowWarning`).
- Running: a three-quarter arc that turns, 20 px, in a button (see `ButtonRow`).
- A button's icon, which its config spec names: one of 18 drawn shapes, 20 px, 2 px strokes or a fill, in the color of the button's label, `space-2` before it (see `ButtonRow`).
- A result to read: an info mark, a 2 px `green` circle 17 px across with an `i` in `green`, 20 px, after the name of a button's row until its result is read (see `ButtonRow`).
- A text field's pencil, 18 px, in `green-muted` (see `TextInput`).
- An activation sphere's mark: a 20 px ring over a glow, with a halo while its sphere is lit (see `ActivationSphere`).
- Close, Minimize, Reset, Discard, minus and plus: 2.5 px strokes in a 24 px box centered in a 48 px button. Reset is a three-quarter arc with an arrowhead, drawn with `PathArcTo`. Discard is an undo: an arrow to the left whose tail turns back down in a half circle, in `amber`, as the pending change it takes back.
- Play, which plays a haptic: a 5 px dot with two arcs on each side, in the same 24 px box.
- A haptic pattern's shape: a quad for each keyframe over a 2 px base, 96 x 24 (see `Haptic`).
- Up and Down, which move a list's entry: a chevron in the same 24 px box (see `List`).
- A hand pose: a bar for each finger in a 104 x 32 frame, as tall as the finger is straight (see `HandPose`).
- Search: a 6 px-radius circle with a handle, 20 px; a match is a 2 px line under its letters (see `Search`).
- The selected row's tip: inside the row, 10 px deep, the row's height, drawn with the bar as one shape.
- Help is the text `?`: until the player first opens the Help page, `? Help` in the `important` style, so a new player finds it.

## Words

- Write plain, literal sentences. No exclamation marks, no jokes, no "please".
- The commit is **Save**, never Apply. Its partners are **Discard** and **Cancel**. Per row it is **Reset**, and **Discard** while the row's change is pending.
- Name what happened and what to do: "Must be 0.50 to 1.50", "Enter a value from 0.50 to 1.50, or Reset it to 1.00."
- Every mod has the Save bar, an MCM mod's too: a config spec's change waits for Save, and an MCM change is made as it is made, as in MCM's menu, so its bar and its dialog say Keep and Revert, in place of Save and Discard, which would claim a write that does not happen. Say when a change takes effect only where the mod's config spec says it: the menu does not know it otherwise. An MCM mod has the `MCM` tag in the tree, as its change is made at once: do not group MCM mods apart or mark them anywhere else.
- An entry that opens the keyboard says so in words: "Type a new name". Do not end a label with an ellipsis: `…` is outside the font's range, and three dots read as a typo in this voice.
- Help reads "Name: help": the setting's name in `green`, a colon, the help in `green-muted`. Do not repeat the name on a line of its own or write "help".
- Counts use the number and the noun: "1 unsaved change", "3 pages".

## Motion and haptics

- Change state on the frame it happens. No fades, slides or easing, which smear on a curved panel in a headset.
- Haptics as the design specifies: a light pulse when a control becomes engaged, a distinct one for Save, a distinct error pulse for rejected input, and none for hover or small slider steps.


---

## Consuming this system (generated — do not edit)

Every path named below is under `project/` in this design system: read `project/api/tokens.md`, not `api/tokens.md`.

14 components are documented without a runnable `components/bundle.js`: read each component’s card, and its README where it has one (`components/<Comp>/README.md`), and build to those guidelines. Tokens: the values are on `api/tokens.md`; a Slides deck or Design canvas also takes `tokens.json` by file path.

Fonts: `tokens.css` loads each font from its file, by path.

- **Design canvas:** copy `tokens.css` and `fonts/Roboto-Medium.ttf` to the same paths under the canvas’s `project/ds/<folder>/` (`<folder>` is the folder you install this system under; `fonts/Roboto-Medium.ttf` lands on `project/ds/<folder>/fonts/Roboto-Medium.ttf`), in the same call that installs this system. Link that copy of `tokens.css` in the artboard’s `<head>` as `ds/<folder>/tokens.css`, or as Design’s instructions say for an artboard in a folder. If the install’s reply leaves a font out, bring that font in as for any other page.
- **Slides deck:** for each family you use in the deck (`faces` holds at most the number of typefaces Slides’ `deck-files.md` gives), install its file with the system and name the path it lands on as that family’s `src` in `project/deck.json` `faces`, under the key in the Slides column. A deck takes a font as a file only under the rule in Slides’ `fonts.md` (no space or leading underscore in its path, for two): any other font, or one kept by id, is uploaded as `fonts.md` says.
- **Any other page:** one file per family is enough (below: the upright face nearest regular weight; bold and italic synthesize). Fetch it as the fetch column says (Artifact tool `read`, that id or path as `path`), upload it as an asset (`publish`, `file_path`, `asset:true`) and declare it with an `@font-face { font-family: "<family>"; src: url(<uploaded>) }` rule.

| family | file | fetch | CSS | Slides `faces` key |
| --- | --- | --- | --- | --- |
| Roboto | `fonts/Roboto-Medium.ttf` (weight 500) | `read` the file with `project/` in front | `var(--font-sans)` | `roboto` |

**Read, per thing:** a component’s props, parts and examples: `api/components/<Comp>.md`; token values: `api/tokens.md`. After this README, fetch the cards and fonts you need in ONE message as parallel calls — none depends on another.

**Two rules.** Before you use a thing — a component, a token group, an icon, an asset — read its card from the index below; a value you did not read from a card is a guess. `tokens.json`, `manifest.json` and `design-system.json` are sources for tools: hand them over. `components/<Comp>/README.md` is the long-form second read a card links to; `SKILL.md` and `artifact-type/` beside them are authoring guidance, not needed to consume the system.

## Index (generated — do not edit)

**Tokens**

- `api/tokens.md` — Every token: fill, palette, type, spacing, radius, size, border, opacity. (11.5k)

**Components** (`api/components/<Comp>.md`, 13)

- **Actions**: `Button` — A 48 px pressable: primary (Save), secondary (Discard), quiet (Cancel), and 48 x 48 icon buttons · `ConfirmDialog` — The modal that asks what to do with unsaved changes before leaving a mod or closing the menu: Save, Discard or Cancel, or Keep, Revert or Cancel for an MCM mod · `SaveBar` — The footer of the Full panel: the session's status on the left, Discard and Save on the right, or Revert and Keep for an MCM mod, for every mod
- **Settings**: `DescriptionArea` — The fixed area at the top of the content pane that explains what the ray is on · `Dropdown` — A choice from a longer list: a frame showing the current option that opens a popup list inside the panel · `SectionHeader` — A heading that groups rows, drawn as ImGui::SeparatorText, and the collapsible Advanced section · `SettingRow` — One setting: a gutter mark, the name, and its control with a Reset slot, all one ray target · `Slider` — A bounded int or float: a track to drag, a value box, and Reset · `Stepper` — Two kinds: a numeric stepper (minus, value, plus) where precision matters more than range, and a choice stepper (previous, label, next) for a short list · `Toggle` — A boolean as two cells, Off and On, so the value is said in words as well as shown
- **Panels**: `FullPanel` — The main browsing and editing view: title bar, mod tree, content pane and footer on one curved 1152 x 720 px panel in front of the player · `WristButtons` — The vrui buttons on the offhand wrist while the panel is minimized: Restore and Exit, under the menu's title
- **Navigation**: `ModTree` — The navigation: every mod in one alphabetical list, with the selected mod expanded to its pages
