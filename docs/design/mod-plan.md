# Mod work plan

The milestones that build the mod, in the order they are made, in three phases. The commits of each milestone are in the file of its phase. It follows the [Framework work plan](framework-plan.md), which made the panel the menu is drawn on.

The menu supports everything MCM has before it gains anything new. It is built so that config specs and the native API are added later without redoing the UI: the UI draws one model, and MCM is the first source that fills it.

A config spec is the new config format of [Declarative schema](schema.md): a mod's `spec.json` under `Data\ModsConfigFO4\Config`.

## How the work is done

- As in the framework plan: one commit at a time, reviewed and tried in the game before it is committed.
- Mod commits continue from M10 and framework commits from F21, and devbench's, in its own repository, start from D1. A commit that is added later takes the next free number of all three phases, and a commit keeps its number when it moves to another milestone.
- Each milestone ends with its "done when", tried in the headset.
- Code that does not need the running game (the model, the loaders, conditions, the INI file) is unit tested. The tests link the framework and CommonLibF4 with `REL::NO_GAME`, as the framework's linked tests do, so the code under test can use both. The first code commit adds the mod's tests.
- A loader grows with the rows. The commit that draws a control also adds the parsing of that control and its tests.
- A commit that settles an open question updates the design documents with it.
- Framework commits are in the tables of the phases. What each needs is in [CommonFramework work required](framework-requirements.md).

Status is one of `todo`, `in review`, or `done`.

## The MCM test set

The test set is the author's Mad God Overhaul mod list: 9 MCM mods with 687 controls.

What it has:

- Controls: `switcher` 298, `section` 97, `slider` 53, `spacer` 46, `text` 42, `button` 32, `textinput` and `textinputFloat` 25, `hotkey` 23, `dropdown` 20, `stepper` 19, `image` 7.
- Value sources: `GlobalValue` 262, `ModSetting*` 151, `PropertyValueInt` 2.
- Actions: `CallFunction` 65, in Sim Settlements 2 and Workshop Framework. Their parameters are strings, with a type prefix or `{value}`.
- Conditions: `groupCondition` as a number 67 times, and as `AND` 4 times.
- `help` text on 376 rows, and `html` text on 20.
- `$` text in 3 mods. Auto Eat's 21 are in the game's translation files. The 132 of FallUI and FallUI Workbench are in FallUI's own files, and 31 of their texts have a token inside them, as `${Key}`.
- Images: 7, all bitmaps stored in the mod's `lib.swf`.
- Code in a `lib.swf`: FallUI's and FallUI Workbench's hold code that runs inside MCM's menu and translates their pages. The menu does not run a mod's code.
- Text input is in FallUI, FallUI Workbench and XDI. Hotkeys are in Sim Settlements 2 and Workshop Framework.
- A mod that does not act on a change at once: Unlimited Survival Mode's DLL reads MCM's settings file itself, and its page says in a text row that its settings need a restart. XDI's DLL reads MCM's files itself too.
- MCM in VR is `mcm_vr.dll`. It keeps ModSettings in memory and writes `Data\MCM\Settings\<mod>.ini` on every set.

What it does not have, from DearModdingUI's inventory of MCM:

- Controls: `dropdownFiles`, `hiddenSwitcher`, `positioner` and `keyinput`, which MCM's menu has. The inventory also has `color`, `hidden`, `menu` and `enum`, and other names of controls (`switch`, `input`, `header`, `empty`, `keymap`): MCM's menu has none of them, and shows an empty line for a type it does not know.
- Value sources: `PropertyValueBool`, `PropertyValueFloat`, `PropertyValueString`.
- Actions: `CallGlobalFunction`, `CallExternalFunction`, and parameters that are plain numbers and booleans. The inventory also has `SendEvent` and `RunConsoleCommand`: MCM runs them for a hotkey alone, and its menu does nothing for them in a row.
- Conditions: `OR`, and a condition inside a condition.
- An image that is drawn as vectors or animated.
- A mod whose `pluginRequirements` are not met, a config that does not parse, and a game language other than English.
- A script that changes a value while its page is shown.

Two mods of the author's other mod lists have some of these, and are installed beside the test set for the trial of [M37](mod-plan-phase-1.md#3-mcm-complete): Global Stash and JOURNEY. The rest is in none of the 65 MCM configs of the author's four mod lists, so the unit tests and the MCM showcase cover it, and there is no test mod with a game plugin of its own. The inventory is DearModdingUI's and not MCM's own documentation, so the table of [M43](mod-plan-phase-1.md#3-mcm-complete) is checked against MCM's.

## Milestones

The milestones are in three phases, each in a file of its own with the commits of its milestones:

- [Phase 1: MCM support](mod-plan-phase-1.md), milestones 1 to 4: the menu supports everything MCM has. It is done.
- [Phase 2: First release](mod-plan-phase-2.md), milestones 5 to 10: config specs and the native API, up to the first release.
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
| 13  | Mod categories           | The mod list is grouped by category, and a mod that the player moved is in its category after a restart       | todo   |
| 14  | Localization             | A mod with no game plugin is shown in another language                                                        | todo   |
| 15  | Advanced features        | A list and a hand pose of Immersive Flashlight VR are edited, and a transform is grabbed with the hand        | todo   |
| 16  | Papyrus API              | A script-only mod reads its saved settings and is told of a Save                                              | todo   |
