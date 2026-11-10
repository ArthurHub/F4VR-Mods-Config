# Mod work plan, phase 3: After the first release

The commits of milestones 11 to 16 of the [Mod work plan](mod-plan.md), which come after the first release. The plan has how the work is done, the MCM test set and the status of every milestone.

## 11. Ironing out

| #    | Repo     | Commit                                                                | Status |
| ---- | -------- | --------------------------------------------------------------------- | ------ |
| M169 | mod      | The open gesture closes the game's favorites                          | done   |
| M170 | mod      | A condition on a setting of the game's INI, with no client state      | done   |
| M171 | mod      | The Help page: the breadcrumbs, and more on Minimize                  | done   |
| M172 | mod      | docs: one hand points at a time, and the menu does not pause the game | done   |
| M173 | mod      | A color, a binding and a transform kept in an INI file                | done   |
| M145 | mod      | The client is told what the ray is on                                 | done   |
| F42  | framework | f4vr: an activation sphere lights while the menu points at it        | done   |

The milestone fixes and polishes what the alpha finds. Its commits are added as they come.

M169 closes the game's favorites that the gesture opening and closing the menu opens: the gesture holds the primary thumbstick in, whose press opens them. They are closed as soon as both buttons of the gesture are held, in either order, and when the gesture fires, which is all a gesture with no modifier has. ROCK keeps that press from the favorites, so it is tried without ROCK too.

M170 lets a condition name a setting of the game's INI, `gameIniSetting`, so a config spec conditions a row on one without a mod's DLL answering it as a client state, and a text shows one with `{gameIniSetting:name}`. Its type is the first letter of its name, and it is asked as the page is drawn, so a row follows the game's settings menu. Immersive Flashlight answers two of them as client states, which it moves to this after the commit, in its own repository. See [Conditions](schema.md#conditions).

M171 adds the breadcrumbs to the Help page, and says more of what Minimize does.

M172 tells the player, in the Help page or the docs, two things the alpha asked:

- One hand works the menu at a time: a hand pointing away selects nothing in the menu, and the active hand has the mark at the end of its ray. This helps prevent accidental presses while the game keeps running. With both rays on the panel, the other hand can take over with a trigger press, unless a drag is in progress.
- The menu does not pause the game, on purpose: live changes are seen in the game at once, with the menu still open. Settings marked Save or Restart still wait for that step.

M173 keeps a `cf.color`, a `cf.inputBinding` and a `cf.transform` in an INI file too, as a line of the text form CommonFramework's INI config had, so a mod with no TOML reads them. See [In an INI file](schema.md#in-an-ini-file).

M145 tells a client which item of its config spec the ray is on, a row or a group, as it moves on and off one, beside the page events, so the mod shows in the world what the item sets. A section, an activation sphere and a button can have an `id`, a sphere its section's with none, and a `PointedAt` event names the row and each group it is in by their ids, innermost first. It stays across the gaps between rows, while a row's editor is open and during Adjust in world. See [Pointed items](schema.md#pointed-items) and [The client's events](architecture.md#the-clients-events).

F42 lights an activation sphere of a CommonFramework mod by itself while the ray is on its rows, with nothing for the mod to add: the framework keeps the section it read the sphere from, and draws it as `showZone` does while the menu points at that section. A zone row lights it only while that zone is the one in use, in power armor or not. The config overload of a sphere's frame names its config; a mod that builds the frame itself names it with `config`, which Immersive Flashlight does for its three spheres, in its own repository, after the commit.

## 12. Search

| #    | Repo | Commit                                                | Status    |
| ---- | ---- | ----------------------------------------------------- | --------- |
| M91  | mod  | The search field, and the tree it filters             | in review |
| M174 | mod  | A page shows only its rows that match while searching | todo      |

The design is in [Search](ui-and-vr-interaction.md#search).

M91 makes the tree's header the search field, typed on SteamVR's keyboard, which filters the tree as each key is typed: the mods and pages with a row whose name matches, each with how many, and the match underlined. A word of the query starts a word of the name, in any order, with case and accents ignored, and can match the name of the mod, the page or the section a row is in, as long as one word matches the row's own. The most specific match wins: a section, a page or a mod whose name matches shows all that is under it only when nothing under it does. The selected mod's conditions are asked of its store, and another mod's hold. The MCM mods' rows read their texts from the game once, as a search starts. A press on a page shows it whole.

M174 shows a page reached while searching with only its rows that match, in their sections, which are open, and editable, and Back and Clear show it whole again, with the focus ring on the last row the ray was on. The content says how many settings match when no page with a match is selected, and a page whose rows that match are all hidden by a condition says so. The design docs and the design system's Search and FullPanelSearch then describe it, in place of the page of results.

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
