# Mod work plan

The milestones that build the mod, in order, in three phases; each phase's file has its commits. It follows the [Framework work plan](framework-plan.md), which made the panel.

The menu supports everything MCM has before it gains anything new, and is built so config specs and the native API come without redoing the UI: the UI draws one model, and MCM is its first source. A config spec is a mod's `spec.json` under `Data\ModsConfigFO4\Config` ([Declarative schema](schema.md)).

## How the work is done

- One commit at a time, reviewed and tried in the game before it is committed. Each milestone ends with its "done when", tried in the headset.
- Numbering: mod commits `M`, framework `F`, devbench (its own repository) `D`. A new commit takes the next free number across all phases; a commit keeps its number when it moves to another milestone.
- Game-free code (model, loaders, conditions, INI file) is unit tested, linking the framework and CommonLibF4 with `REL::NO_GAME`.
- The commit that draws a control also adds its parsing and tests.
- A commit that settles an open question updates the design docs.
- Status is `todo`, `in review` or `done`.

## The MCM test set

The author's Mad God Overhaul mod list: 9 MCM mods, 687 controls.

What it has:

- Controls: `switcher` 298, `section` 97, `slider` 53, `spacer` 46, `text` 42, `button` 32, `textinput` / `textinputFloat` 25, `hotkey` 23, `dropdown` 20, `stepper` 19, `image` 7.
- Value sources: `GlobalValue` 262, `ModSetting*` 151, `PropertyValueInt` 2.
- Actions: `CallFunction` 65 (Sim Settlements 2, Workshop Framework), string parameters with a type prefix or `{value}`.
- Conditions: `groupCondition` as a number 67 times, as `AND` 4 times.
- `help` on 376 rows, `html` on 20.
- `$` text in 3 mods: Auto Eat's 21 in the game's translation files; FallUI's and FallUI Workbench's 132 in FallUI's own files, 31 with a nested `${Key}`.
- 7 images, all bitmaps in the mod's `lib.swf`. FallUI's and FallUI Workbench's `lib.swf` also hold code that translates their pages inside MCM's menu; the menu runs no mod code.
- Text input in FallUI, FallUI Workbench and XDI; hotkeys in Sim Settlements 2 and Workshop Framework.
- Mods that read MCM's file themselves: Unlimited Survival Mode's DLL (its page says settings need a restart) and XDI's.
- MCM VR (`mcm_vr.dll`) keeps ModSettings in memory and writes `Data\MCM\Settings\<mod>.ini` on every set.

What it lacks, from DearModdingUI's inventory of MCM:

- Controls `dropdownFiles`, `hiddenSwitcher`, `positioner`, `keyinput` (MCM has them); the inventory's `color`, `hidden`, `menu`, `enum`, `switch`, `input`, `header`, `empty`, `keymap` aren't MCM types (MCM shows an empty line).
- `PropertyValueBool`, `PropertyValueFloat`, `PropertyValueString`.
- `CallGlobalFunction`, `CallExternalFunction`, plain number and bool parameters; `SendEvent` and `RunConsoleCommand` run only for hotkeys.
- `OR` conditions and nested conditions.
- Vector or animated images.
- Unmet `pluginRequirements`, an unparsable config, a non-English game.
- A script changing a value while its page is shown.

Global Stash and JOURNEY, from the author's other lists, cover some of these and are installed beside the test set for [M37](mod-plan-phase-1.md#3-mcm-complete). The rest is in none of the 65 MCM configs of the author's four lists, so unit tests and the MCM showcase cover it; there is no test mod with its own game plugin. The inventory isn't MCM's documentation, so [M43](mod-plan-phase-1.md#3-mcm-complete)'s table is checked against MCM's.

## Milestones

- [Phase 1: MCM support](mod-plan-phase-1.md), milestones 1 to 4.
- [Phase 2: First release](mod-plan-phase-2.md), milestones 5 to 10: config specs and the native API.
- [Phase 3: After the first release](mod-plan-phase-3.md), milestones 11 to 16.

| #   | Milestone                | Done when                                                                                                     | Status |
| --- | ------------------------ | ------------------------------------------------------------------------------------------------------------- | ------ |
| 1   | Browse MCM mods          | Every MCM mod of the test set is browsed in the Full panel: its pages, rows and images, with translated names | done   |
| 2   | Edit MCM values          | A global, a ModSetting and a script property are changed from the panel, and the mod reacts as it does to MCM | done   |
| 3   | MCM complete             | Every MCM feature is supported, shown read-only, or shown as unsupported with the reason                      | done   |
| 4   | VR keyboard              | An MCM text row is typed in the headset                                                                       | done   |
| 5   | Config specs with Save       | This mod's own TOML is edited from its config spec and saved, and the mod reloads its file                        | done   |
| 6   | Config specs complete        | FRIK's TOML, an INI file and a script mod's globals are edited from config specs and saved, and a Save that fails changes no file | done   |
| 7   | Actions and own UIs      | FRIK's config is opened from the front page, and the menu comes back when it closes                           | done   |
| 8   | Live preview             | A CommonFramework mod with a config spec previews a slider live, with no code of its own                          | done   |
| 9   | CommonFramework controls | Every setting of Immersive Flashlight VR but its lists and hand poses is edited from its config spec              | done   |
| 10  | First release            | The mod is ready for alpha testing: its Nexus page, README and docs are written, and it is tested and perf tested in the headset | done   |
| 11  | Ironing out              | What the alpha finds is fixed                                                                                 | done   |
| 12  | Search                   | A setting is found by typing its name                                                                         | done   |
| 13  | Mod categories           | The mod list is grouped by category, and a mod that the player moved is in its category after a restart       | done   |
| 14  | Localization             | A mod with no game plugin is shown in another language                                                        | todo   |
| 15  | Advanced features        | A list and a hand pose of Immersive Flashlight VR are edited, and a transform is grabbed with the hand        | todo   |
| 16  | Papyrus API              | A script-only mod reads its saved settings and is told of a Save                                              | todo   |
