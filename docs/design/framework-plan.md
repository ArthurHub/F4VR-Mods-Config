# Framework work plan

The framework commits (F) that delivered [CommonFramework work required](framework-requirements.md), with the mod commits (M) that used them as the test bed. All done; the [VR keyboard](#vr-keyboard) was added after them, and the [ImGui controls](#imgui-controls) are planned. Per-commit notes are in each framework subsystem's README and the framework's `0.5.0` changelog; the narrative this plan had is at `git show 6a0be38:docs/design/framework-plan.md`.

## Commits

### The test bed

| #   | Repo | Commit                                                                                   | Status |
| --- | ---- | ---------------------------------------------------------------------------------------- | ------ |
| M1  | mod  | Open the menu with the gesture taken from FRIK: one ImGui panel and the wrist buttons    | done   |

### Version 0.5.0

| #   | Repo      | Commit                                                                      | Status |
| --- | --------- | --------------------------------------------------------------------------- | ------ |
| F0  | framework | Version 0.5.0, the deprecated `PlayerNodes` and `getPlayerNodes()` removed  | done   |

### Hand-anchored UI (item 1)

| #   | Repo      | Commit                                                                      | Status    |
| --- | --------- | --------------------------------------------------------------------------- | --------- |
| F1  | framework | vrui: an element takes a rotation, and its whole tree turns with it         | done      |
| M2  | mod       | The panel tilted back, a wrist button for FRIK's config, the dev layout     | done      |
| F2  | framework | vrui: offhand controller presets, top and right, as on the primary wand     | done      |
| M3  | mod       | The wrist buttons on the offhand top preset                                 | done      |
| F3  | framework | vrui: wrist preset that follows the forearm bone, with controller fallback  | done      |

### Dev layout

Not an item of the requirements: the tool the placements above are tuned with.

| #   | Repo      | Commit                                                                      | Status    |
| --- | --------- | --------------------------------------------------------------------------- | --------- |
| F15 | framework | vrui: the dev layout is turned on by a debug flag and keeps its own file    | done      |
| M4  | mod       | The wrist buttons on the wrist preset, the dev layout by the framework flag | done      |

### Interactive ImGui panel (items 2 to 5)

| #   | Repo      | Commit                                                                                        | Status |
| --- | --------- | --------------------------------------------------------------------------------------------- | ------ |
| F4  | framework | imgui: opt-in pointer input from a wand ray and its trigger, one hand owning the pointer      | done      |
| F16 | framework | config: a color read from the INI                                                             | done      |
| F5  | framework | imgui: default pointer ray and hit mark                                                       | done      |
| M5  | mod       | The panel takes the pointer, with its look in the INI                                         | done      |
| F6  | framework | imgui: the wand that operates a panel is fully suppressed, with the press latch               | done      |
| F7  | framework | imgui: thumbstick scrolling                                                                   | done      |
| M6  | mod       | The menu's wand hidden from the game, the scroll speed in the INI                             | done      |

### Dedicated panel (item 6)

| #   | Repo      | Commit                                                                          | Status |
| --- | --------- | ------------------------------------------------------------------------------- | ------ |
| F8  | framework | imgui: the canvas window's zero padding and border are popped before content    | done      |
| F9  | framework | imgui: dedicated panel with a texture of its own, as ImGui's display            | done      |
| M7  | mod       | The panel is dedicated, with its combo at the bottom edge                       | done      |

### World-locked placement (item 7)

| #   | Repo      | Commit                                                                           | Status |
| --- | --------- | -------------------------------------------------------------------------------- | ------ |
| F17 | framework | vrui: a preset puts a root where the HMD is and leaves it in the world           | done   |
| F18 | framework | imgui: the mark keeps its size on the canvas, and the other hand's ray is drawn  | done   |
| M8  | mod       | The panel stays in the world, recentered by a gesture and when the player leaves | done   |

### Curved panel

Not an item of the requirements. The flat panel is as wide as its distance from the player, so its sides are further away than its middle and turned away from the player, and the text there reads smaller.

| #   | Repo      | Commit                                                              | Status    |
| --- | --------- | ------------------------------------------------------------------- | --------- |
| F19 | framework | imgui: a panel is curved toward the player, around a cylinder       | done      |
| M9  | mod       | The panel is curved around the player                               | done      |

### Moving the panel

Not an item of the requirements. The panel is spawned at one place in front of the player, and the player moves it to where it suits them.

| #   | Repo      | Commit                                                              | Status    |
| --- | --------- | ------------------------------------------------------------------- | --------- |
| F20 | framework | vrui: a root in the world is moved by the hand that points at it    | done      |
| M10 | mod       | The panel is moved with the grip, the button in the INI             | done      |

### Config (items 8 to 10)

These do not need the panel, so they can be moved earlier.

| #   | Repo      | Commit                                                                      | Status    |
| --- | --------- | --------------------------------------------------------------------------- | --------- |
| F10 | framework | config: loads are serialized                                                | done      |
| F11 | framework | config: the mod's own writes are recognized without the ignore flag         | done      |
| F12 | framework | config: several overrides in one reload, cached parsed INI, debug-level log | done      |
| F13 | framework | config: subscribers are notified after an override reload                   | done      |
| F14 | framework | config: each override has an owner                                          | done      |
| F21 | framework | config: the INI value and the set of several overrides are unit tested      | done      |

### VR keyboard

Not an item of the requirements. SteamVR's keyboard opens where the dashboard was last opened, its text reaches the field only by comparing whole texts, and OpenComposite and xrizer have none. The framework draws its own keyboard, typed with the rays, on every runtime, in place of SteamVR's. The design is in the framework's [`vr-keyboard.md`](../../external/F4VR-CommonFramework/docs/tech/vr-keyboard.md) once F24 is in. Framework version `0.6.0`.

| #   | Repo      | Commit                                                                                                          | Status |
| --- | --------- | --------------------------------------------------------------------------------------------------------------- | ------ |
| F22 | framework | imgui: a canvas of the framework's own ImGui context, sharing the font atlas, drawn beside a dedicated canvas   | done   |
| F23 | framework | imgui: the keyboard's logic: layouts, the key a point hits, Shift, Caps and pages, repeat, each key's events    | done   |
| F24 | framework | imgui: the VR keyboard replaces SteamVR's, typed with both rays, moved with the grip, closed by the mod's Back  | done   |
| F25 | framework | devbench: a `keyboard` action, its state and a key press                                                        | done   |
| M11 | mod       | The keyboard in the panel's theme, Back closes it and steps no page while it is open                            | done   |
| M12 | mod       | e2e: typing into a text setting and the search field                                                            | done   |

### ImGui controls

Not an item of the requirements. The next mod, a cheat terminal and item spawner, is a tree beside a page of buttons, number inputs, sliders and search: the panel's small controls, which any ImGui panel in VR needs, move to the framework so it is built from them. Only what is plainly generic moves now. Framework version `0.6.1`, the mod `1.1.1`.

- **Where:** `src/imgui/controls/` with its README, namespace `f4cf::imgui::controls`. The drawing primitives are free functions; the controls' state is one `Controls` instance, `controls::get()`, as the keyboard and the pointer are one each.
- **Shape:** ImGui's. A control takes plain values (a float with its range, step and format; option texts; a string) and keeps its state by `ImGuiID`. Nothing in it knows a setting, a store or a config spec, so a change to this mod's rows never touches the framework.
- **Look:** a `ControlsStyle` set with `controls::get().setStyle`, as `KeyboardStyle` and `PointerStyle`, whose defaults are the framework's green (`F4VR_PANEL_STYLE`). The mod fills it from its generated [`Theme.h`](../../src/Theme.h); the design system and its generator stay in the mod.
- **Input:** a control reads the pointing hand's trigger and thumbstick through the framework, whose layer reads the thumbstick and begins and ends the controls' frame: the mod passes no thumbstick value. A control reports its pulse, as a button reports its press, and the caller plays it with the rest of its press feedback, so one rule decides which a press gets.
- **Back:** `controls::get().back()` ends what a control holds open (a drag, a list, a text edit) and returns whether it did. The mod calls it first; the order of the rest of Back stays the mod's.
- **Pointed boxes:** a control returns its box, from which the mod's `PanelTargets` records it for devbench as now.
- **Tests:** the framework tests each control on a headless ImGui context, as the mod's panel tests do. A port changes no look or behavior: the mod's panel and render tests pass unchanged after each.

What moves: the generic part of [`Drawing.h`](../../src/ui/Drawing.h) (clipped and wrapped text, underlined ranges, the drawn icons and marks, frames and outlines, `popupTop`), the button, icon button and switcher, the dialog and its buttons, the slider (`Sliders`, `StepRepeat`, `valueBoxText`), the stepper and the dropdown with its list (`drawList`, `ListExtras`), and the text input on the framework's keyboard with the search field's Escape trap.

Then the rows and the tree's rows, which set the panel's look as the controls do and take plain values:

- **Rows** (`controls/Rows.h`): `rowWidth()`, the width of a row at the cursor whether or not the scroll bar shows, set in by a section; `addRow(pressable)`, which gives `Row{shown, pointed, started, held, pressed}` with the look of a row under the ray; `addFoldingRow(id, bool& folded)`, whose folded state the caller keeps, so the mod's `Sections` keeps it for each page and devbench and a search can open a section; `drawFoldingHeading`, the arrow, text and rule with `MarkedCount`s ending the rule (the error mark, the warning mark, or the dot in the warning color); and `sectionHeading`, the heading with a rule that ends where the rows do.
- **Tree rows** (`controls/Tree.h`): `treeRow(TreeRow, width)`, set in a step of 16 px for each level up to 3, in an arrow column of 16 px every row keeps, with its name, a tag of 10 px, the selected bar, the disabled look, the unsaved dot, underlined ranges, a count while searched, and `show` to scroll to the selected row; it gives pressed and pointed. `treeGroupRow(name, count, open, unsaved, width)` is a category's: muted, an arrow, a rule and the count. The tree's tag is its own, smaller than `drawTag` and in the color on the accent while selected.

What stays of them: `drawLabel` with the pending, applies and invalid marks, `sphereHeading`, what `Sections` counts in a folded section and says of it, and all of `ModTree`: the mods, pages, categories and search results it walks, the open categories, the scroll to the first rows a press opened, and the search field.

What waits for the next mod: the tree as a whole, panes and their scrolling, a clipped list for thousands of rows, breadcrumbs, the help line, the order of Back, devbench targets kept by the framework, and the editors of the framework's types (`cf.color`, `cf.transform`, `cf.inputBinding`, `cf.haptic`). The next mod builds them, copied from here where that helps; each moves once both mods have it and the shape they share shows.

What stays: everything about config specs, MCM and Save: the rows' rules (`RowView`, `ButtonView`), the pending, invalid and applies marks, the rich text of a config spec's markup, `FullPanel` and its sessions.

Each framework commit is followed by the mod's port onto it, one at a time.

| #   | Repo      | Commit                                                                                                       | Status  |
| --- | --------- | ------------------------------------------------------------------------------------------------------------ | ------- |
| F26 | framework | imgui: controls style and drawing primitives: text, underlined ranges, drawn icons and marks, frames         | done    |
| M13 | mod       | `Drawing` on the framework's primitives, `ControlsStyle` from `Theme.h`                                      | done    |
| F27 | framework | imgui: button, icon button, switcher, dialog and its buttons                                                 | done    |
| M14 | mod       | Buttons, switchers and dialogs from the framework                                                            | done    |
| F28 | framework | imgui: slider: trigger drag written on release, step arrows, thumbstick steps that repeat, Back cancels      | done    |
| M15 | mod       | Sliders from the framework, in rows and in the editors                                                       | done    |
| F29 | framework | imgui: stepper and dropdown, with its list beside its box: suffixes, a header line, an action after them     | done    |
| M16 | mod       | Steppers, dropdowns and the haptic list from the framework                                                   | done    |
| F30 | framework | imgui: text input on the framework's keyboard, kept through Escape                                           | done    |
| M17 | mod       | Text inputs and the search field from the framework                                                          | done    |
| F31 | framework | imgui: rows and section headings: a row as wide as the rows, a folding heading with counts, a heading's rule | done    |
| M18 | mod       | Page rows and section headings from the framework                                                            | done    |
| F32 | framework | imgui: tree rows: set in by level, an arrow column, selected, disabled, a tag, and a group row with a count  | todo    |
| M19 | mod       | The tree's rows from the framework                                                                           | todo    |

## Decisions that differ from the requirements

- The ray comes from the wand's UI node; the wand node itself points 59° above where the hand aims.
- Suppression hides the whole wand, per hand by its own ray, and a press outlives its canvas until the trigger is released (else the game sees a trigger already down).
- A dedicated panel is at most 4096 layout pixels a side: the largest D3D11 texture at the largest supersample.
- A world-locked root gets no transform: a preset puts it at the HMD and its own position and rotation place it from there.
- A set override re-parses the kept file text (SimpleIni can't copy a parsed file); no disk read.
- Config subscribers are called on the game thread at the start of the next frame, once for all loads since the last frame.

Items 11 to 13 (images, text entry with SteamVR's keyboard, the `ModBase` client) were made within the [mod's milestones](mod-plan.md).
