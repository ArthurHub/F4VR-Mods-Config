# MCM Compatibility

What your MCM menu gets in Mods Config FO4 VR as it is, what a config spec has beyond MCM, and how to move to one, or keep both.

Where to start: [For Mod Authors](README.md). Every field of a config spec: the [config spec schema](../design/schema.md). How the menu handles each MCM feature, in full: [MCM support](../design/mcm.md).

## Contents

- [Your MCM Menu, as It Is](#your-mcm-menu-as-it-is)
- [What a Config Spec Adds](#what-a-config-spec-adds)
- [Moving to a Config Spec](#moving-to-a-config-spec)
- [Keeping Both](#keeping-both)

## Your MCM Menu, as It Is

You don't have to do anything. While MCM VR is installed, the menu reads your `Data\MCM\Config\<mod>\config.json` as MCM's menu reads it, and shows it in VR, with the `MCM` tag on your mod's row.

![An MCM mod in the menu](../assets/mcm-fallui-inventory-image.jpg)

**What works as in MCM's menu:** your pages, every row MCM's menu draws, `help` in the help line, `html` text, `groupControl` and `groupCondition`, `sharedLists`, `listFromForm` and `listFromProperty`, the `$` tokens of your translation files, FallUI's language files, and your images, read from your `lib.swf`. A value is written as MCM writes it: a global and a property into the game, and a ModSetting through `MCM.SetModSetting*`. A row's `CallFunction` and `CallGlobalFunction` run, and `OnMCMSettingChange` is sent, as each change is made.

**What the player sees differently:**

- **Keep and Revert.** A change is made in the game as the player makes it, as in MCM's menu, and stays a change of the session until **Keep** accepts it. **Revert** sets the changes back, each as a change is made: its value is written, its action runs again and its event is sent, so a preset that is set back sets its rows back.
- **Text inputs are typed in the headset,** on SteamVR's keyboard, or a real one. MCM's menu in VR has no keyboard for them.
- **Reset** sets a ModSetting to its default from your `settings.ini`, which MCM's menu can't do.
- **The game keeps running** while the menu is open, so your script acts on a change while the player looks.
- **Hotkeys are shown, not edited.** MCM keeps binding and running them, and the player changes them in MCM's own menu.
- **The list is by name.** Mods are in the order of their display names, not of MCM's `sOrder`, and MCM's own `Hotkey manager` and `MCM Settings` aren't listed.
- **A mod that can't be shown** is greyed out with the reason, as one whose plugin isn't loaded, where MCM's menu shows a page that says why. So is a page whose own `pluginRequirements` aren't loaded, with its `messageIfMissingReqs` after the reason. `hideIfMissingReqs` leaves it out, as in MCM.
- **The menu's events:** `OnMCMMenuOpen|<mod>` and `OnMCMMenuClose|<mod>` are sent as your mod is picked and left, where MCM's menu sends none.
- **A DLL that reads MCM's settings file as the pause menu closes** gets the changes when the player presses Keep: the menu then tells the game the pause menu opened and closed.

**What isn't run:** a `positioner`, a `keyinput` and a `customClipLoader` are disabled rows that say the menu has no such control. `CallExternalFunction` calls Flash, which the menu has none of, and `RunConsoleCommand` and `SendEvent` are hotkey actions, which MCM's menu doesn't run in a row either. The code in your `lib.swf` isn't run. An image drawn as vectors, a JPEG, or an animated one is a row that says an image is there.

**What isn't supported:** `ownerModName`. Your config is a mod of its own in the list, and its pages aren't put under another mod.

The **MCM Showcase**, in the Showcase download, is an MCM menu with every kind of row in every look: install it to see how yours will look.

## What a Config Spec Adds

A config spec has what an MCM config has, by MCM's names, and more:

- **Your own files.** Settings are kept in your mod's INI and TOML files, and in globals and properties of your scripts. Nothing goes through MCM, and the menu doesn't need MCM installed.
- **Save and Discard.** A change waits for Save, so the player tries values and drops them.
- **Live preview,** for a mod built on the F4VR Common Framework with no code, or through the native API, and for a global or a property by the menu itself.
- **Defaults, read-only values and number display:** every setting has a `default` for Reset, a value can be shown and not edited, and a number has its decimals, a unit or a percentage.
- **Warnings** on a value that works and has a cost, or on a button whose action needs something of the player.
- **When a change takes effect:** `applies` tells the player whether a change works now, after Save, or after a restart.
- **Conditions** that compare any value with their operators, enable a row as well as show it, and ask whether a plugin is loaded.
- **Sections that nest and fold, and pages that nest.**
- **Buttons** that run a console command or a function of your DLL, with their own text, an icon, a style, and a confirmation.
- **The controls of the F4VR Common Framework:** controller bindings, haptics, colors, transforms and activation spheres.
- **A help for the mod and for each page,** and texts with a fallback, so English needs no translation file.
- **Images as textures,** so they need no `lib.swf`.

The [Showcase walkthrough](showcase.md) shows each.

## Moving to a Config Spec

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

## Keeping Both

A mod can ship both its MCM config and a config spec, with the same `modName`. Mods Config FO4 VR lists it by its config spec, and leaves its MCM menu out, so it isn't listed twice. MCM's own menu keeps showing the MCM config. The players of MCM keep what they have, and the players of this menu get the config spec's controls, Save and Discard, and live preview: the best of both.

Both menus should edit the same values. A global or a property of a script is the same value in both, so a mod that keeps its settings there has both menus on them, with nothing more to do. A ModSetting and a key of your own file are two values, so a setting kept in one menu as a ModSetting and in the other as a key of your file is two settings, which your mod has to reconcile.
