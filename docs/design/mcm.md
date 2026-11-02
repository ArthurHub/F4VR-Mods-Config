# MCM support

Every feature of an MCM config and of MCM VR, and how the menu handles each. It is checked against MCM's own documentation, the [MCM wiki](https://github.com/Neanka/MCM_0.1_AS3/wiki), and against what MCM VR does: its menu, `Interface\MCM_VR.swf`, and its DLL, `mcm_vr.dll`. The menu reads an MCM config as MCM's menu reads it, and the [Mod work plan](mod-plan.md) says where it differs, in the milestone named in the last column.

A feature is handled in one of four ways:

- **As in MCM**: the menu does what MCM's menu does.
- **Read-only**: it is shown, and changed in MCM alone.
- **Not run**: its row is disabled, and the help line says why.
- **Not read**: no mod of the author's four mod lists has it, so the menu does not read it, and the row says nothing of it.

## MCM VR

The menu shows MCM mods while `mcm_vr.dll` is loaded, and finds it by that name. It calls nothing of the DLL but MCM's Papyrus functions `MCM.SetModSetting*`, and reads the files MCM writes, so any build of MCM VR serves, and its version is not checked. Flat Fallout 4's MCM is not supported, as the game is not.

## The config

| Feature | How the menu handles it | |
| --- | --- | --- |
| `modName`, `displayName` | As in MCM. A mod is listed by its display name, in the order of the names and not of MCM's `sOrder` | M14 |
| `content`, `pages`, `pageDisplayName` | As in MCM: the mod's front page and its pages | M16 |
| `pluginRequirements` | As in MCM: a mod whose plugin is not loaded is a disabled row, with the reason in the help line and the notices | M41 |
| `minMcmVersion` | Not checked: MCM VR's version is 8, and no config of the author's lists asks more than 6 | M41 |
| A config that cannot be read | A disabled row named by its folder, with the reason | M41 |
| `sharedLists` | As in MCM | M37 |
| `ownerModName`, which puts the pages of a config under another mod | Not read: the config is a mod of its own | |
| A page's own `pluginRequirements`, `messageIfMissingReqs` and `hideIfMissingReqs` | Not read: the page is shown | |

## Rows

| Type | How the menu handles it | |
| --- | --- | --- |
| `section` | As in MCM: its text without tags, as a heading | M19 |
| `spacer` | As in MCM, with its `numLines`. A `height` in pixels is not read: the spacer is one line | M19 |
| `text` | As in MCM, with `<br>`, `<p align>`, `<font size color>`, `<b>`, `<i>`, `<u>` and entities when its `html` is on. A text that is both bold and italic is bold. Its `align` is not read: the one config that has it says the same in a `<p align>` | M19, M37, M106 |
| `image` | Its bitmap, read from the mod's `lib.swf`, at the size MCM's menu shows it. One drawn as vectors, a JPEG or one that is animated is a row that says an image is there. `width` and `height` are not used, as in MCM's menu | M22 |
| `switcher` | As in MCM | M25 |
| `hiddenSwitcher` | As in MCM: never shown, and controls its group | M38 |
| `slider` | As in MCM, with its `min`, `max` and `step` | M26 |
| `stepper`, `dropdown` | As in MCM, with `options` or `sharedOptions` | M27, M37 |
| `dropdownFiles` | As in MCM, with its `path` and `mask`, but its pick writes the name of the file, which MCM's menu loses | M39 |
| `button` | As in MCM: runs its action | M33 |
| `hotkey` | Read-only: its key, as MCM's files have it | M35 |
| `textinput`, `textinputInt`, `textinputFloat` | As in MCM, typed on SteamVR's keyboard or with the keys of the game's window. A text that is no number is not written for a `textinputInt` and a `textinputFloat` | M36, M45 |
| `positioner`, `keyinput`, `customClipLoader` | Not run: the help line says the menu has no such control | M36 |
| A type MCM's menu does not have, or no type | Not run, as a `positioner` is. MCM's menu shows an empty line. DearModdingUI's inventory has names that are no type of MCM's: `switch`, `hidden`, `menu`, `enum`, `color`, `input`, `header`, `empty` and `keymap`. They are such a type | M36, M38 |

What a row has beside its type:

| Feature | How the menu handles it | |
| --- | --- | --- |
| `text` | As in MCM, translated, with its tags as MCM's menu sets every row's text as html | M17, M104, M106 |
| `help` | As in MCM, in the help line | M21 |
| `id` | As in MCM: the name of a ModSetting and of a hotkey, and what `OnMCMSettingChange` gives | M29, M31 |
| `groupControl`, `groupCondition` | As in MCM: a number, a list, `AND`, `OR` and `ONLY`. A condition inside another is read too, which MCM's menu does not | M28 |
| A flag that is a text, as `"html": "true"` | As in MCM: on for any text but an empty one | M37 |
| `textFromFormName`, `textFromFormDescription`, `textFromStringProperty`, `textFromStringArrayProperty`, and the same for `help` | Not read: the text is shown as written | |
| `$$tintCT` in a text, which MCM's menu replaces with its tint color | Not read | |
| A row's own `modName`, for a setting of another mod | Not read: the setting is of the row's mod | |

## Values

| Feature | How the menu handles it | |
| --- | --- | --- |
| `GlobalValue` | As in MCM: read in every frame, and written as it is changed | M25, M138 |
| `ModSettingBool`, `Int`, `Float`, `String` | As in MCM: read from MCM's two files as the mod is selected and as another page is shown, and written through `MCM.SetModSetting*` as it is changed | M29, M30, M42, M138 |
| `PropertyValueBool`, `Int`, `Float`, `String` | As in MCM, with `scriptName`: read in every frame, and written as it is changed | M32, M138 |
| No `sourceType` | As in MCM: the value is held while the mod is selected, and is never an unsaved change | M25 |
| A value that is not available | The row is disabled, and the help line says why | M25 |
| The default of a ModSetting | Its Reset, which MCM's menu does not have | M102 |
| `listFromForm`, `listFromProperty` | As in MCM: the stepper or the dropdown has the options it reads in the game, in place of its `options`, as its mod is selected and as another page is shown. A `sharedOptions` comes first, then a `listFromForm`. A list that is not in the game leaves it with no options, and the help line says why | M110 |
| `allowModifierKeys` of a hotkey | Not read: it is of an edit the menu does not make | |
| `precision` of a text input | Not read: a number has 6 decimals at most, MCM's default | M45 |

## Actions

| Feature | How the menu handles it | |
| --- | --- | --- |
| `CallFunction` | As in MCM, on a button and on a value row, the row's after its value is written, and again as Revert sets it back. A button runs with the changes made, and does not ask to save first | M33, M34, M138 |
| `CallGlobalFunction` | As in MCM | M40 |
| `CallExternalFunction` | Not run: it calls a function of Flash, which the menu has none of | M40 |
| `RunConsoleCommand`, `SendEvent`, and any other type | Not run: they are actions of a hotkey, which MCM runs, and MCM's menu runs none of them in a row | M40 |
| `params` | As in MCM: `{i}`, `{f}` and `{b}`, JSON numbers and bools, and `{value}`. `{s}` is in MCM's documentation and not in its menu: the text is given as written, in both. A `{b}` text of a button is off for `0` and `false`, which MCM's menu has on | M33, M34 |

## Text and files

| Feature | How the menu handles it | |
| --- | --- | --- |
| `$` tokens | As in MCM: the game's translations, which F4SE loads for each enabled game plugin | M17 |
| FallUI's language files | As FallUI's own code does in MCM's menu | M18, M37 |
| An M8r intro image | The logo its code loads | M37 |
| `Data\MCM\Config\<mod>\settings.ini` | Read as the defaults of the ModSettings. Never written | M29 |
| `Data\MCM\Settings\<mod>.ini` | Read over the defaults. MCM writes it | M29, M42 |
| `keybinds.json`, `Data\MCM\Settings\Keybinds.json` | Read for the keys of the hotkeys. MCM writes the second | M35 |

## MCM's behavior

| Feature | How the menu handles it | |
| --- | --- | --- |
| A change is saved as it is made | As in MCM, and it is a change of the session until Keep accepts it or Revert sets it back, which runs its row's action and sends its event again. Its `applies` is `unknown`: see [MCM compatibility](architecture.md#mcm-compatibility) | M25, M138 |
| `OnMCMOpen`, `OnMCMClose` | As in MCM | M31 |
| `OnMCMMenuOpen`, `OnMCMMenuOpen\|<mod>`, `OnMCMMenuClose\|<mod>` | Sent as a mod is selected and left, also as the menu closes, where MCM's menu sends none | M31 |
| `OnMCMSettingChange`, `OnMCMSettingChange\|<mod>` | As in MCM, after the action, as a change is made, and as Revert sets it back | M31, M34, M138 |
| MCM's Papyrus functions: `GetModSetting*`, `SetModSetting*`, `RefreshMenu`, `IsInstalled`, `GetVersionCode` | MCM's own, which keep working. `RefreshMenu` refreshes MCM's menu alone, and this menu reads every value again by itself | M42 |
| Hotkeys | MCM binds and runs them, for a key of the keyboard, the mouse or a gamepad. The menu shows them read-only. A button of a VR controller is no key MCM runs a hotkey for, and the menu does not add one | M35 |
| MCM's `Hotkey manager` and `MCM Settings` | Not listed: a mod's hotkeys are on its own pages | M11 |
| A DLL that reads MCM's file as the pause menu closes, which MCM's menu is in | After a Save of a session that wrote a ModSetting, the menu tells the game's listeners that the pause menu opened and closed, so the DLL reads its file. It gets the changes at Save, and not as each is made | M105, M138 |
| The game is paused while MCM's menu is open | The game keeps running, so a script acts on a change while the menu is open | |

## What MCM's authors work around

MCM has no field for some things a mod needs, and its authors get them from what it has. A config spec has each in a field of its own; see [A config spec and MCM](schema.md#a-config-spec-and-mcm):

- A row that is shown while something holds: a `hiddenSwitcher` that a script sets. The author's lists have three uses of it. JOURNEY's script sets two from its mode dropdown, to show rows for one mode and for two of them: a config spec's condition on the dropdown's value. Global Stash and NAC X show a row while another mod is installed: a condition on a loaded plugin. Global Stash also shows a warning while it is busy, which follows the game as it plays: that waits for the native API.
- The groups of conditions are numbers, shared by a whole page, and a group is on while a switcher is: a config spec's condition names the setting, and compares its value.
- The type of a parameter is written in its text, as `{i}42`: a config spec's parameter is a JSON value with its type.
- An action gets the value of its row from `{value}` in the text of a parameter: a config spec's setting has an action that Save runs as it writes the value, and its parameter `"{value}"` is that value, in the type of the setting.
- A switcher with no storage shows rows that are hidden, as FallUI's "Unfold settings": a config spec's section folds.
