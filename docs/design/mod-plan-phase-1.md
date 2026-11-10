# Mod work plan, phase 1: MCM support

The commits of milestones 1 to 4, in which the menu supports everything MCM has. Done. The [Mod work plan](mod-plan.md) has how the work is done and each milestone's "done when"; the rules these commits settled are in the design docs, and the per-commit notes are at `git show 6a0be38:docs/design/mod-plan-phase-1.md`.

## 1. Browse MCM mods

| #   | Repo      | Commit                                                                                                | Status    |
| --- | --------- | ----------------------------------------------------------------------------------------------------- | --------- |
| M11 | mod       | docs: the menu's front page, with the mod tree and a quick launch section                             | done      |
| M12 | mod       | docs: the settings model, checked against three kinds of setting                                      | done      |
| M13 | mod       | The mod's unit tests, the settings model, and the MCM loader for a mod, its pages and its rows' texts | done      |
| M14 | mod       | The front page lists the MCM mods that are found                                                      | done      |
| M15 | mod       | FRIK in the quick launch section                                                                      | done      |
| M100 | mod      | The panel's look from the design system, and the panel drawn into an image by a test                  | done      |
| M16 | mod       | A mod's pages in the tree, with every row as its text and type                                        | done      |
| M17 | mod       | MCM's `$` text tokens are shown as the game's translated text                                         | done      |
| M18 | mod       | FallUI's pages are translated from FallUI's own files                                                 | done      |
| M19 | mod       | Sections, spacers and text rows, with their HTML                                                      | done      |
| M101 | mod      | The MCM showcase, an optional file for developers                                                     | done      |
| M20 | mod       | The switcher, slider, stepper and dropdown are drawn with their default values                        | done      |
| M21 | mod       | The help line                                                                                         | done      |
| F22 | framework | render: a texture from pixels in memory                                                               | done      |
| F23 | framework | imgui: an image in a canvas                                                                           | done      |
| M22 | mod       | MCM images are shown, read from the mod's `lib.swf`                                                   | done      |
| M23 | mod       | The panel's look: sizes for the ray, and the states of a row                                          | done      |
| F33 | framework | vrui: the move button takes hold after a hold delay                                                   | done      |
| M24 | mod       | The Back button, and the menu's haptics                                                               | done      |

## 2. Edit MCM values

| #   | Repo      | Commit                                                                              | Status    |
| --- | --------- | ----------------------------------------------------------------------------------- | --------- |
| M25 | mod       | Globals are read and written: the switcher works                                    | done      |
| M26 | mod       | The slider                                                                          | done      |
| M27 | mod       | The stepper and the dropdown                                                        | done      |
| M28 | mod       | Conditions show and hide rows                                                       | done      |
| M29 | mod       | ModSettings are read from MCM's files                                               | done      |
| M30 | mod       | ModSettings are written through MCM                                                 | done      |
| M31 | mod       | MCM's events are sent: a setting changed, the menu opened and closed                | done      |
| M32 | mod       | Script properties are read and written                                              | done      |
| F24 | framework | f4sevr: script properties, if the calls it has do not work in the game              | done      |

## 3. MCM complete

| #   | Repo | Commit                                                               | Status    |
| --- | ---- | -------------------------------------------------------------------- | --------- |
| M33 | mod  | Buttons run `CallFunction`, with typed parameters                    | done      |
| M34 | mod  | A value row runs its action after the write, with `{value}`          | done      |
| M35 | mod  | Hotkeys are shown read-only                                          | done      |
| M36 | mod  | Text rows are shown read-only, and unknown controls as placeholders  | done      |
| M102 | mod | A ModSetting is reset to the default of its mod                      | done      |
| M38 | mod  | A hidden switcher controls its group, and is not shown               | done      |
| M39 | mod  | A `dropdownFiles` lists the files of a folder                        | done      |
| M40 | mod  | The actions beside `CallFunction`                                    | done      |
| M37 | mod  | A trial with Global Stash and JOURNEY, and what it finds             | done      |
| M104 | mod | The text of a row has the size and the color of its font tags        | done      |
| M41 | mod  | A mod that is not available: its disabled row, and the notices       | done      |
| M42 | mod  | A page that is shown reads its values again                          | done      |
| M43 | mod  | docs: the table of MCM features and how the menu handles each        | done      |
| F34 | framework | imgui: bold and italic faces of the canvas font                 | done      |
| M106 | mod | `<b>`, `<i>` and `<u>` in the text of a row, and the panel's font size | done    |
| M105 | mod | A DLL that reads MCM's file as the pause menu closes reads it as the menu closes | done |

## 4. VR keyboard

| #   | Repo      | Commit                                                      | Status    |
| --- | --------- | ----------------------------------------------------------- | --------- |
| F25 | framework | docs: the VR keyboard's design                              | done      |
| M44 | mod       | docs: the keyboard's design, as item 12 of the requirements | done      |
| F26 | framework | imgui: the keys of the game's window reach a text field     | done      |
| F27 | framework | imgui: SteamVR's keyboard while a text field is edited      | done      |
| M45 | mod       | MCM text rows are edited                                    | done      |
| M46 | mod       | A slider's value is typed                                   | rejected  |
