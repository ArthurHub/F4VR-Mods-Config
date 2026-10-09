Mods Config VR is one settings panel for every Fallout 4 VR mod, drawn with Dear ImGui on a curved CommonFramework panel and worked with a controller ray. It should read as a large projected Pip-Boy: dark glass, one phosphor green, a strong border, big targets, and a selection bar that points at what it opened. These rules are for the C++ that draws it. The previews recreate the ImGui output at 1 layout pixel to 1 CSS pixel.

## Principles

- **One hue.** Draw text, borders, fills and focus in `green` (#0afa78, the framework's `F4VR_PANEL_STYLE` color) at full value or as a tint. The only other hues are `amber` for pending and `red` for invalid, and both always come with a drawn mark.
- **Solid green means chosen.** Use a solid `green` fill only for the selected tree row, the first breadcrumb while the menu's front page is open, the current option, the On half of a toggle, and the primary button, with `on-green` text on it. Everything else is outline or tint.
- **Never color alone.** A pending setting has the amber dot, an invalid one has the warning triangle and a message, a setting that needs a restart has the Restart mark, and a toggle says Off or On in words.
- **Nothing moves.** The help line, the reset slot of every row, Discard and Save, and the control column keep their place whether they are in use or not. Only text changes; nothing pressable appears or disappears under the ray.
- **Big enough for a ray.** Every pressable thing is at least `control-height` (48 px, one vrui unit) tall.

## Scale

- Lay out in ImGui pixels. A `UIImGuiPanel` gets 48 px per vrui unit, so the Full panel at 24 x 15 units is `panel-width` x `panel-height`: 1152 x 720 px.
- At the defaults in `ConfigMenu.cpp` (scale 6, 120 game units in front of the HMD, curve radius 25), one layout pixel is about 0.06 degrees at the panel's middle, about 17 px a degree. A `t-body` line is about 1.4 degrees tall, and a 48 px target about 2.9 degrees.
- Keep `setFontSizePixels` at 24 and `setSupersample` at 1.5. The panel texture is then 1728 x 1080, about 25 texels a degree. Raise the factor only if text looks soft in the headset: memory grows with its square.
- Set `TouchExtraPadding` to `space-1` on each side. It widens every hit box into the 8 px gap between items without moving anything, which absorbs hand jitter.

## Color

Draw the Full and Focused panels on `glass`. The framework's own `F4VR_PANEL_STYLE` uses alpha 166 (0.65); the panel uses 217 (0.85), because at 0.65 green text drops to 4.3:1 when a bright sky is behind it. Keep `glass-widget`, the framework's exact value, for the vrui wrist buttons only.

Every text color below is checked with a white world behind the glass, the worst case:

| Text | On `glass` | On `hover` | On `pressed` | Use |
| --- | --- | --- | --- | --- |
| `green` | 9.0:1 | 6.8:1 | 5.6:1 | All primary text and every value |
| `green-muted` | 8.3:1 | 6.3:1 | 5.2:1 | Descriptions, units, counts, status |
| `amber` | 7.7:1 | 5.8:1 | 4.8:1 | A pending value and the unsaved count |
| `red` | 6.0:1 | 4.5:1 | — | An invalid value and its message; never on `pressed` |
| `on-green` on `green` | 13:1 | | | Text on any solid green fill |

- Tint with green, never with gray: `plate` (6%) for fixed regions, `track` (10%) for the empty part of a control, `hover` (12%) under the ray, `pressed` (20%) while the trigger is held.
- Outline controls in `frame-border` (3.7:1). Draw section rules in `rule`, which is decorative.
- Draw a pressed outline and a dragged grab in `glow`, close to the pointer's `mark`, so feedback seems to come from the ray.
- Keep the pointer as the INI sets it: `ray`, `mark`, `mark-border`. They are `[Pointer]` defaults, not panel colors.
- Do not add white text, gray UI, or a second accent. The one exception is a color that a mod gives the text of its own `text` row, which is drawn as given.

## Type

- Use one face: Roboto Medium, the font the framework embeds (`render::TextFont`). Bold and italic do not exist. Hierarchy comes from size, color and case.
- Use three sizes, made with `ImGui::SetWindowFontScale` around the call and reset to 1.0 after it: `t-title` (30 px, scale 1.25), `t-body` (24 px), `t-detail` (20 px, scale 0.8333). Nothing smaller than `t-detail`.
- ImGui sizes a font by its ascent minus descent (`stbtt_ScaleForPixelHeight`), and Roboto's is 2701 of 2048 units, so ImGui's 24 px is an 18.2 px em in CSS. The type tokens carry the CSS em so previews match the headset.
- ImGui's line advance is the font size. Wrapped text has no extra leading. Help gets two `t-body` lines, or three `t-detail` lines when it needs them, and an ellipsis past that.
- Only U+0020 to U+00FF render, because the framework loads ImGui's default glyph ranges. `°`, `×` and `±` are safe; arrows, bullets, check marks and `›` are not. Draw those (see Iconography).
- Write `MODS CONFIG VR` and the wrist button labels in capitals. Write everything else in sentence case, and setting names exactly as the mod gives them.

## States

Every interactive item has these states, each told apart by more than color:

| State | When | How it looks | ImGui |
| --- | --- | --- | --- |
| Idle | | Text in `green`, frames in `track` with a `frame-border` outline | |
| Hover | The ray is on it | `hover` fill and a 2 px `frame-border` outline round the whole row | `IsItemHovered()`; draw the outline with `AddRect` |
| Focused | The thumbstick adjusts it: the last control pressed | A 2 px `green` ring, no fill | Track the last-activated id; draw the ring yourself |
| Active | Trigger held, or a drag | `pressed` fill, 2 px `glow` outline, grab in `glow` | `IsItemActive()`; `ButtonActive`, `SliderGrabActive` |
| Selected | The open page, the current option | Solid `green`, `on-green` text; a tree row ends in a 20 px tip | Push `ImGuiCol_Text` = `on-green` before the item |
| Pending | Differs from the saved value | `amber` dot in the gutter; the value, fill and grab in `amber`; Reset shown | Store state; nothing in ImGui |
| Invalid | Cannot be saved | Warning triangle in the gutter; value in `red` with a `red` outline; a one-line `t-detail` message under the name | Store state |
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
| `SliderGrab`, `ScrollbarGrabHovered`, `NavHighlight` | `green` |
| `SliderGrabActive`, `ScrollbarGrabActive` | `glow` |
| `ScrollbarGrab` | `frame-border` |
| `Separator` | `rule` |
| `ModalWindowDimBg` | `scrim` |

## Layout

- **Full panel** (`FullPanel`): title bar (`titlebar-height`) holding the breadcrumbs, then the mod tree (`nav-width`) and the content pane side by side with a `space-4` gap, then the footer (`footer-height`). The tree keeps its place and its width, with or without a selected mod.
- **Tree** (`ModTree`): the mods in one alphabetical list under a `Mods` header with the count. From 30 mods (`iCategoriesMinMods`) the list is grouped: categories are the top level, each a muted row with a rule and its count, and the mods are indented under them (`FullPanelCategories`). Selecting a mod expands it to its pages and opens its front page (`FullPanel`); a page is a row under it (`FullPanelPage`). The menu's front page (`FullPanelHome`) has no row: the first breadcrumb is its entry. About nine rows fit, so the rows scroll under the header, with the content pane's scroll bar in a room the tree always keeps; a row selected from outside the tree is scrolled into view.
- **Breadcrumbs**: `MODS CONFIG VR » <mod> » <page>` in `t-title` in the title bar, so the content pane gives its height to settings. The last crumb is `green`, the others `green-muted` and pressable: `MODS CONFIG VR` goes to the menu's front page, the mod to its front page. On the menu's front page `MODS CONFIG VR` is the only crumb and is selected: solid `green`, `on-green` text. The mod's name is the one that shortens with an ellipsis.
- **Back** follows the same path: a page to its mod's front page, a mod's front page to the menu's front page, where the first breadcrumb is selected. On the menu's front page it closes the menu.
- **Content pane**: only the rows, which scroll with the thumbstick behind a `scrollbar-size` position bar. A page's own help is its first item, in `green-muted`, and scrolls with it.
- **Footer**: Discard and Save fixed on the right. On the left, the help line (`DescriptionArea`): an error, else "Name: help" for the setting or option under the ray, else the session status.
- **Setting row**: a `space-6` gutter for the pending dot or warning mark, the name, then the `control-width` control column: the control right-aligned, then a 48 px Reset slot that is always reserved.
- **Focused panel** (`FocusedPanel`): `focused-width` x `focused-height` on the offhand. Its footer is too narrow for help, so a two-line help area (`description-height`) sits at the top. Each setting stacks its name over its control, and a dropdown becomes a stepper.
- **Minimized**: the panel is hidden; only the wrist buttons remain (`WristButtons`).
- Open dropdowns, popups and the dialog inside the panel. The ray never has to leave it.

## Iconography

There is no icon font, and ImGui renders nothing past U+00FF, so every icon is drawn:

- Tree and dropdown arrows: `ImGui::RenderArrow` or a filled triangle, 14 px.
- The current option's check: `ImGui::RenderCheckMark`.
- Pending: `AddCircleFilled`, 10 px across, `amber`.
- Invalid: a filled `red` triangle 20 x 18 with `!` in `on-red`.
- Close, Minimize, Reset, minus and plus: 2.5 px strokes in a 24 px box centered in a 48 px button. Reset is a three-quarter arc with an arrowhead, drawn with `PathArcTo`.
- The selected row's tip: `AddTriangleFilled`, 20 px deep, the row's height.
- Help is the text `?`.

## Words

- Write plain, literal sentences. No exclamation marks, no jokes, no "please".
- The commit is **Save**, never Apply. Its partners are **Discard** and **Cancel**. Per row it is **Reset**.
- Name what happened and what to do: "Must be 0.50 to 1.50", "Enter a value from 0.50 to 1.50, or Reset it to 1.00."
- An MCM mod's footer says "Changes are saved as they are made". It does not say that the mod acts on them: the menu does not know that. Do not label MCM mods or group them apart.
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

**Components** (`api/components/<Comp>.md`, 14)

- **Actions**: `Button` — A 48 px pressable: primary (Save), secondary (Discard), quiet (Cancel), and 48 x 48 icon buttons · `ConfirmDialog` — The modal that asks what to do with unsaved changes before leaving a mod or closing the menu: Save, Discard or Cancel · `SaveBar` — The footer of the Full and Focused panels: the session's status on the left, Discard and Save on the right; for an MCM mod, the immediate-apply line instead
- **Settings**: `DescriptionArea` — The fixed area at the top of the content pane that explains what the ray is on · `Dropdown` — A choice from a longer list: a frame showing the current option that opens a popup list inside the panel · `SectionHeader` — A heading that groups rows, drawn as ImGui::SeparatorText, and the collapsible Advanced section · `SettingRow` — One setting: a gutter mark, the name, and its control with a Reset slot, all one ray target · `Slider` — A bounded int or float: a track to drag, a value box, and Reset · `Stepper` — Two kinds: a numeric stepper (minus, value, plus) where precision matters more than range, and a choice stepper (previous, label, next) for a short list · `Toggle` — A boolean as two cells, Off and On, so the value is said in words as well as shown
- **Panels**: `FocusedPanel` — A smaller panel on the offhand that shows the selected page only, so the player can change a setting while looking at its effect · `FullPanel` — The main browsing and editing view: title bar, mod tree, content pane and footer on one curved 1152 x 720 px panel in front of the player · `WristButtons` — The vrui buttons on the offhand wrist that stay while the panel is shown or minimized: Minimize or Resume, Close, and FRIK when FRIK is installed
- **Navigation**: `ModTree` — The navigation: every mod in one alphabetical list, with the selected mod expanded to its pages
