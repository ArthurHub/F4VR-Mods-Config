# Config Spec vs MCM

What a config spec has beyond an MCM config, and how to move your MCM menu to one.

Where to start: [For Mod Authors](README.md). Your MCM menu as it is, and keeping both: [MCM compatibility](mcm.md). Every field of a config spec: the [config spec schema](../design/schema.md).

- [What a Config Spec Has Beyond MCM](#what-a-config-spec-has-beyond-mcm)
- [Moving From MCM](#moving-from-mcm)

## What a Config Spec Has Beyond MCM

A config spec has what an MCM config has, by MCM's names, and gives your players, and you, more:

- **Your own files.** INI and TOML files, in `Data` or in `Documents\My Games\Fallout4VR`, and your scripts' globals and properties. The menu edits them in place, and keeps each file's comments and layout.
- **A richer page, with no code:**
  - Pages that nest, and sections that nest and fold.
  - Conditions that compare any value with their operators, show or enable a row, and ask whether a plugin is loaded.
  - Warnings on a value that works and has a cost, or on a button whose action needs something of the player.
  - Numbers with decimals, a unit or a percentage, and read-only values, shown and not edited.
  - Buttons that run a console command or a function of your DLL, with their own text, an icon, a style and a confirmation.
  - Presets: a choice whose options set several settings at once.
  - A help for the mod and for each page, and texts with a fallback, so English needs no translation file.
  - Images as textures, with no `lib.swf`.
- **Save and Discard.** A change waits for Save, so players try values and drop them. Each setting has a default for Reset, and says when it takes effect: live, on Save, or after a restart.
- **Live preview.** A change shows in the game as it is made, before Save: for a global or a property by the menu itself, for a Common Framework mod with no code, and for any DLL through the [native API](native-api.md).
- **The Common Framework's controls,** for a mod built on it: controller bindings, haptics, colors with a picker, positions tuned in the world, and activation spheres.
- **Tools for you, in the headset:** each config spec's errors and warnings, with where each is in `spec.json`, and a live copy that reloads as you save it, so you edit on the desktop and see the page change as the game runs. See [Testing It in the Game](README.md#testing-it-in-the-game).

The [Showcase walkthrough](showcase.md) shows each.

## Moving From MCM

A config spec is written as an MCM config is, so most of yours carries over: `modName`, `displayName`, `content`, `pages` and `pageDisplayName`, the row types, `text`, `help`, `id`, `options`, `sharedOptions`, `listFromForm`, `$` tokens, `CallFunction` and `CallGlobalFunction`. Put it at `Data\ModsConfigFO4\Config\<your mod>\spec.json`. What changes:

| In an MCM config | In a config spec |
| --- | --- |
| `valueOptions.sourceType` and its source | `storage`, where the value is kept: a key of your file, a global, or a property |
| `GlobalValue`, with `sourceForm` | `"storage": { "global": "MyMod.esp|800" }` |
| `PropertyValue...`, with `sourceForm`, `scriptName` and `propertyName` | `"storage": { "form": "MyMod.esp|800", "scriptName": "MyMod:ConfigScript", "propertyName": "fStrength" }` |
| `ModSetting...` | A key of your own INI or TOML file: `"storage": { "file": "main", "section": "Main", "key": "fStrength" }`. See the note under this table |
| A `default` in `settings.ini` | A `default` on the setting |
| `hiddenSwitcher`, set by a script to show rows | `visibleWhen`, a condition on a value or on a loaded plugin |
| `groupControl` and `groupCondition`, groups by number | `visibleWhen` and `enabledWhen`, which name settings by their `id` |
| `{i}`, `{f}` and `{b}` in a parameter's text | A JSON value: `42`, `1.5`, `true` |
| `{value}` in a parameter's text | A parameter that is exactly `"{value}"`, in the setting's type |
| A switcher with no storage that shows hidden rows | A section that folds: `"isCollapsible": true` |
| `textinputInt`, `textinputFloat` | A `textinput` whose `valueType` is `int` or `float` |
| `dropdownFiles` | A `dropdown` with a `listFromFolder` |
| `hotkey`, `keyinput` | A `cf.inputBinding`, for a CommonFramework mod |
| An image in `lib.swf` | An `image` of a texture under `Data\Textures` |
| `positioner`, `CallExternalFunction` | None: they are of MCM's own Flash menu |
| `ownerModName` | None: a config spec's pages are its mod's alone |

**A ModSetting has no place in a config spec yet.** Your scripts read a ModSetting with `MCM.GetModSetting*`, and a config spec keeps that value in your own file instead, which a script reads through the menu's Papyrus API. That API is planned, and not in this release. Until it is, a script mod moves the settings it keeps in globals and properties, and keeps its MCM menu for its ModSettings. A DLL that reads MCM's settings file reads its own INI or TOML file instead.
