# Mod work plan, phase 3: After the first release

The commits of milestones 11 to 16 of the [Mod work plan](mod-plan.md), which come after the first release. The plan has how the work is done, the MCM test set and the status of every milestone.

## 11. Ironing out

| #    | Repo     | Commit                                                                | Status |
| ---- | -------- | --------------------------------------------------------------------- | ------ |
| D2   | devbench | Drive the ImGui pointer's ray, the keyboard, and a panel's controls   | todo   |
| M169 | mod      | The open gesture closes the game's favorites                          | in review |
| M170 | mod      | A condition on a game setting, with no client state                   | todo   |
| M171 | mod      | The Help page: the breadcrumbs, and more on Minimize                  | todo   |
| M172 | mod      | docs: one hand points at a time, and the menu does not pause the game | todo   |
| M173 | mod      | A color, a binding and a transform kept in an INI file                | todo   |
| M145 | mod      | The client is told what the ray is on                                 | todo   |

The milestone fixes and polishes what the alpha finds. Its commits are added as they come.

D2 is a commit in devbench's own repository. With it, `test_perf.py` measures the ray on the panel, SteamVR's keyboard and an open editor, which [M155](mod-plan-phase-2.md#10-first-release) left for it.

M169 closes the game's favorites that the gesture opening and closing the menu opens: the gesture holds the primary thumbstick in, whose press opens them. They are closed as soon as both buttons of the gesture are held, in either order, and when the gesture fires, which is all a gesture with no modifier has. ROCK keeps that press from the favorites, so it is tried without ROCK too.

M170 lets a condition name a setting of the game itself, so a config spec conditions a row on one without a mod's DLL answering it as a client state.

M171 adds the breadcrumbs to the Help page, and says more of what Minimize does.

M172 tells the player, in the Help page or the docs, two things the alpha asked:

- One hand points at a time: the trigger of the other hand selects nothing while a ray is on the panel. The hand that points has the mark at the end of its ray. It is on purpose, as the game is not paused, so a press is not made by accident.
- The menu does not pause the game, on purpose: a change is seen in the game at once, with the menu still open.

M173 keeps a `cf.color`, a `cf.inputBinding` and a `cf.transform` in an INI file too, which take a TOML file alone now.

M145 tells a client which item of its config spec the ray is on, a row or a group, as it moves on and off one, beside the page events, so the mod shows in the world what the item sets: Immersive Flashlight lights the activation sphere whose rows the ray is on. Its design is open; see [Open questions](open-questions.md#commonframework-specific-controls).

## 12. Search

| #   | Repo | Commit                                         | Status |
| --- | ---- | ---------------------------------------------- | ------ |
| M91 | mod  | Search in the mod list and in a mod's settings | todo   |

## 13. Mod categories

| #   | Repo | Commit                                                                                          | Status |
| --- | ---- | ----------------------------------------------------------------------------------------------- | ------ |
| M95 | mod  | The `category` of a config spec and of an MCM config, and the shipped names and table of known mods | todo   |
| M96 | mod  | The tree is grouped by category from a number of mods, which is in the config                   | todo   |
| M97 | mod  | The Categories page: the player's choice for each mod, kept in `Categories.toml`                | todo   |
| M98 | mod  | A new category name is typed                                                                    | todo   |
| M99 | mod  | The Categories page imports the categories of MCM Categorizer                                   | todo   |

The design is in [Mod categories](ui-and-vr-interaction.md#mod-categories) and [Category](schema.md#category). It comes after the first release: the list has to be long for it to matter, and the player's choice needs a config spec's Save and the keyboard.

A config spec can name its `category` from the first release. The field is in the schema, and the menu does not use it before this milestone.

M95 ships `Categories.json`. Its table starts with the MCM mods of the test set, and it is checked in the game that MCM's own menu still shows a config that has a `category`.

M96 adds `iCategoriesMinMods` to the config, 30 as shipped, and a row for it in the mod's own config spec. The test set has 9 mods, so it is tried with a lower number.

M97 adds the page to the mod's own config spec of [M47](mod-plan-phase-2.md#5-config-specs-with-save). The menu fills it with a row for every mod, and each row is a setting of that config spec, so Save, Discard and Reset are the ones of milestone 5. One thing is new to them: a choice that is the same as the category the mod has without it is not written, and Save removes its key.

M98 adds the entry that types a new name to the dropdown, with the keyboard of milestone 4.

M99 adds the import button, which is shown when MCM Categorizer's settings file is found. The file keeps its categories in one value, in an encoding of its own. F4SEMenuFramework reads the same value, and is the reference for it. Before the commit it is checked that MCM Categorizer runs with MCM in VR: if it does not, no player has the file and the commit is dropped.

## 14. Localization

| #   | Repo      | Commit                                                                       | Status |
| --- | --------- | ---------------------------------------------------------------------------- | ------ |
| M90 | mod       | A translation file is read for a config spec whose mod has no game plugin    | todo   |
| F32 | framework | imgui: glyphs beyond Western European languages, if a translation needs them | todo   |

M90 reads `Interface\Translations\<modName>_<language>.txt` itself. F4SE loads a translation file only for an enabled game plugin, so a mod that is a DLL alone has none loaded.

## 15. Advanced features

| #    | Repo | Commit                            | Status |
| ---- | ---- | --------------------------------- | ------ |
| M143 | mod  | List                              | todo   |
| M133 | mod  | HandPose                          | todo   |
| M144 | mod  | Transform: grabbed with the hand  | todo   |
| M147 | mod  | Menu color                        | todo   |

M143 edits a list of texts, as [List](controls.md#list) has it: Immersive Flashlight has 5.

M133 edits a hand pose, as [Hand pose](controls.md#hand-pose) has it: Immersive Flashlight has 4.

M144 has the player grab the object a transform places and move it with the hand to where it should be. Its design is open; see [Open questions](open-questions.md#commonframework-specific-controls).

M147 lets the player change the menu's color: one house color in the mod's config, which the green of the design system's tokens is made from, the rules, the hover, the selection and the text, with presets of the Pip-Boy's colors and a custom color in the mod's own config spec. The theme is read at runtime in place of the generated constants. A house color cannot hide what other colors say: amber is a pending change and red an invalid one, so an amber or red menu would lose them. Whether the menu can match the Pip-Boy's color by itself is open; see [Open questions](open-questions.md#ui).

## 16. Papyrus API

| #   | Repo | Commit                                                                  | Status |
| --- | ---- | ----------------------------------------------------------------------- | ------ |
| M92 | mod  | The Papyrus getters, with config specs loaded when the game's data is ready | todo   |
| M93 | mod  | The Papyrus event after a Save                                          | todo   |
| M94 | mod  | docs: the script author's guide                                         | todo   |
