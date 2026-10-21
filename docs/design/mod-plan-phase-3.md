# Mod work plan, phase 3: After the first release

The commits of milestones 12 to 15 of the [Mod work plan](mod-plan.md), which come after the first release. The plan has how the work is done, the MCM test set and the status of every milestone.

## 12. Localization and search

| #   | Repo      | Commit                                                                       | Status |
| --- | --------- | ---------------------------------------------------------------------------- | ------ |
| M90 | mod       | A translation file is read for a config spec whose mod has no game plugin        | todo   |
| F32 | framework | imgui: glyphs beyond Western European languages, if a translation needs them | todo   |
| M91 | mod       | Search in the mod list and in a mod's settings                               | todo   |

M90 reads `Interface\Translations\<modName>_<language>.txt` itself. F4SE loads a translation file only for an enabled game plugin, so a mod that is a DLL alone has none loaded.

## 13. Papyrus API

| #   | Repo | Commit                                                                  | Status |
| --- | ---- | ----------------------------------------------------------------------- | ------ |
| M92 | mod  | The Papyrus getters, with config specs loaded when the game's data is ready | todo   |
| M93 | mod  | The Papyrus event after a Save                                          | todo   |
| M94 | mod  | docs: the script author's guide                                         | todo   |

## 14. Mod categories

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

## 15. JSON files

| #   | Repo | Commit                                   | Status |
| --- | ---- | ---------------------------------------- | ------ |
| M88 | mod  | A JSON file is read, changed and written | todo   |
