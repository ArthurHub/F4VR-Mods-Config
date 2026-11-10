# Mod work plan, phase 3: After the first release

The commits of milestones 11 to 16 of the [Mod work plan](mod-plan.md), which has how the work is done and each milestone's "done when". Milestones 11 and 12 are done: the rules they settled are in the design docs, and their per-commit notes are at `git show f75c5f4:docs/design/mod-plan-phase-3.md`.

## 11. Ironing out

Fixes and polish from the alpha; commits are added as they come.

| #    | Repo      | Commit                                                                | Status |
| ---- | --------- | --------------------------------------------------------------------- | ------ |
| M169 | mod       | The open gesture closes the game's favorites                          | done   |
| M170 | mod       | A condition on a setting of the game's INI, with no client state      | done   |
| M171 | mod       | The Help page: the breadcrumbs, and more on Minimize                  | done   |
| M172 | mod       | docs: one hand points at a time, and the menu does not pause the game | done   |
| M173 | mod       | A color, a binding and a transform kept in an INI file                | done   |
| M145 | mod       | The client is told what the ray is on                                 | done   |
| F42  | framework | f4vr: an activation sphere lights while the menu points at it         | done   |

## 12. Search

Design: [Search](ui-and-vr-interaction.md#search).

| #    | Repo | Commit                                                | Status |
| ---- | ---- | ----------------------------------------------------- | ------ |
| M91  | mod  | The search field, and the tree it filters             | done   |
| M174 | mod  | A page shows only its rows that match while searching | done   |

## 13. Mod categories

Design: [Mod categories](ui-and-vr-interaction.md#mod-categories), [Category](schema.md#category). After the first release because the list must be long to matter, and the player's choice needs a config spec's Save and the keyboard. A config spec's `category` is in the schema already; the menu ignores it until this milestone.

| #   | Repo | Commit                                                                                          | Status |
| --- | ---- | ----------------------------------------------------------------------------------------------- | ------ |
| M95 | mod  | The `category` of a config spec and of an MCM config, and the shipped names and table of known mods | todo   |
| M96 | mod  | The tree is grouped by category from a number of mods, which is in the config                   | todo   |
| M97 | mod  | The Categories page: the player's choice for each mod, kept in `Categories.toml`                | todo   |
| M98 | mod  | A new category name is typed                                                                    | todo   |
| M99 | mod  | The Categories page imports the categories of MCM Categorizer                                   | todo   |

- **M95** ships `Categories.json`, its table starting with the test set's MCM mods. Check in the game that MCM's own menu still shows a config with a `category`.
- **M96** adds `iCategoriesMinMods` (30 as shipped) with a row in the mod's own config spec. The test set has 9 mods, so try it with a lower number.
- **M97** adds the page to the mod's own config spec ([M47](mod-plan-phase-2.md#5-config-specs-with-save)), filled with a setting per mod, so Save, Discard and Reset are milestone 5's. New: a choice equal to the mod's own category isn't written, and Save removes its key.
- **M98** adds the dropdown entry that types a new name on the keyboard.
- **M99** adds the import button, shown when MCM Categorizer's settings file exists. The file keeps its categories in one value in its own encoding; F4SEMenuFramework reads the same value and is the reference. First check that MCM Categorizer runs with MCM VR: if not, no player has the file and the commit is dropped.

## 14. Localization

| #   | Repo      | Commit                                                                       | Status |
| --- | --------- | ---------------------------------------------------------------------------- | ------ |
| M90 | mod       | A translation file is read for a config spec whose mod has no game plugin    | todo   |
| F32 | framework | imgui: glyphs beyond Western European languages, if a translation needs them | todo   |

- **M90** reads `Interface\Translations\<modName>_<language>.txt` itself: F4SE loads translations only for an enabled game plugin, so a DLL-only mod has none.

## 15. Advanced features

| #    | Repo | Commit                            | Status |
| ---- | ---- | --------------------------------- | ------ |
| M143 | mod  | List                              | todo   |
| M133 | mod  | HandPose                          | todo   |
| M144 | mod  | Transform: grabbed with the hand  | todo   |
| M147 | mod  | Menu color                        | todo   |

- **M143** edits a [List](controls.md#list); Immersive Flashlight has 5.
- **M133** edits a [Hand pose](controls.md#hand-pose); Immersive Flashlight has 4.
- **M144** lets the player grab a transform's object and move it by hand. Design open: [Open questions](open-questions.md#commonframework-specific-controls).
- **M147** lets the player change the menu's color: one house color in the mod's config, from which the tokens' green is made (rules, hover, selection, text), with Pip-Boy presets and a custom color in the mod's own config spec. The theme is read at runtime instead of the generated constants. Amber (pending) and red (invalid) must stay readable, so an amber or red menu would lose them. Matching the Pip-Boy's color automatically is open: [Open questions](open-questions.md#ui).

## 16. Papyrus API

| #   | Repo | Commit                                                                  | Status |
| --- | ---- | ----------------------------------------------------------------------- | ------ |
| M92 | mod  | The Papyrus getters, with config specs loaded when the game's data is ready | todo   |
| M93 | mod  | The Papyrus event after a Save                                          | todo   |
| M94 | mod  | docs: the script author's guide                                         | todo   |
