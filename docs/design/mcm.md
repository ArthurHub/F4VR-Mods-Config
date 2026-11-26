# MCM support

Every feature of an MCM config and of MCM VR, and how the menu handles it, checked against the [MCM wiki](https://github.com/Neanka/MCM_0.1_AS3/wiki) and MCM VR's `Interface\MCM_VR.swf` and `mcm_vr.dll`.

- **As in MCM**: the menu does what MCM's menu does.
- **Read-only**: shown; changed in MCM alone.
- **Not run**: the row is disabled; the help line says why.
- **Not read**: no mod of the author's four mod lists has it, so it is ignored silently.

## MCM VR

MCM mods show while `mcm_vr.dll` is loaded (found by name). The menu calls only MCM's Papyrus `MCM.SetModSetting*` and reads MCM's files, so any MCM VR build works and its version isn't checked. Flat Fallout 4's MCM is not supported.

## The config

| Feature | How the menu handles it |
| --- | --- |
| `modName`, `displayName` | As in MCM. A mod is listed by its display name, in the order of the names and not of MCM's `sOrder` |
| `content`, `pages`, `pageDisplayName` | As in MCM: the mod's front page and its pages |
| `pluginRequirements` | As in MCM: a mod whose plugin is not loaded is a disabled row, with the reason in the help line and the notices |
| `minMcmVersion` | Not checked: MCM VR's version is 8, and no config of the author's lists asks more than 6 |
| A config that cannot be read | A disabled row named by its folder, with the reason |
| `sharedLists` | As in MCM |
| `ownerModName`, which puts the pages of a config under another mod | Not supported, and never will be: the config is a mod of its own, and the menu does not merge two configs into one mod |
| A page's own `pluginRequirements`, `messageIfMissingReqs` and `hideIfMissingReqs` | While a plugin of its own is not loaded, a page with `hideIfMissingReqs` is left out, as in MCM. Any other is a disabled row, with the reason and its `messageIfMissingReqs` in the help line, as a mod is: MCM's menu opens it on a page that says them. A page's own `minMcmVersion` is not checked, as the config's is not |

## Rows

| Type | How the menu handles it |
| --- | --- |
| `section` | As in MCM: its text without tags, as a heading |
| `spacer` | As in MCM, with its `numLines`. A `height` in pixels is not read: the spacer is one line |
| `text` | As in MCM, with `<br>`, `<p align>`, `<font size color>`, `<b>`, `<i>`, `<u>` and entities when its `html` is on. A text that is both bold and italic is bold. Its `align` is not read: the one config that has it says the same in a `<p align>` |
| `image` | Its bitmap, read from the mod's `lib.swf`, at the size MCM's menu shows it. One drawn as vectors, a JPEG or one that is animated is a row that says an image is there. `width` and `height` are not used, as in MCM's menu |
| `switcher` | As in MCM |
| `hiddenSwitcher` | As in MCM: never shown, and controls its group |
| `slider` | As in MCM, with its `min`, `max` and `step` |
| Three sliders of a color, R, G and B | One color row, which the color editor edits: three sliders in a row, each from 0 to 255, whose ids are the same but for one letter, `R`, `G` and `B` in this order, as FallUI's `iCustomColorR:Interface`. A fourth with `A` is its alpha. The row is named by its section, or `Color` out of one. The sliders are hidden settings, as a `hiddenSwitcher` is: a change of the color sets each slider that changes, with its action and its event, and counts as one change. MCM's menu shows the sliders |
| `stepper`, `dropdown` | As in MCM, with `options` or `sharedOptions` |
| `dropdownFiles` | As in MCM, with its `path` and `mask`, but its pick writes the name of the file, which MCM's menu loses. It is drawn as a config spec's choice of files: a file's name with its extension muted where the mask does not name it, and its folder and how many files it has over the list |
| `button` | As in MCM: runs its action |
| `hotkey` | Read-only: its key, as MCM's files have it |
| `textinput`, `textinputInt`, `textinputFloat` | As in MCM, typed on the framework's keyboard or with the keys of the game's window. A text that is no number is not written for a `textinputInt` and a `textinputFloat` |
| `positioner`, `keyinput`, `customClipLoader` | Not run: the help line says the menu has no such control |
| A type MCM's menu does not have, or no type | Not run, as a `positioner` is. MCM's menu shows an empty line. DearModdingUI's inventory has names that are no type of MCM's: `switch`, `hidden`, `menu`, `enum`, `color`, `input`, `header`, `empty` and `keymap`. They are such a type |

What a row has beside its type:

| Feature | How the menu handles it |
| --- | --- |
| `text` | As in MCM, translated, with its tags as MCM's menu sets every row's text as html |
| `help` | As in MCM, in the help line |
| `id` | As in MCM: the name of a ModSetting and of a hotkey, and what `OnMCMSettingChange` gives |
| `groupControl`, `groupCondition` | As in MCM: a number, a list, `AND`, `OR` and `ONLY`. A condition inside another is read too, which MCM's menu does not |
| A flag that is a text, as `"html": "true"` | As in MCM: on for any text but an empty one |
| `textFromFormName`, `textFromFormDescription`, `textFromStringProperty`, `textFromStringArrayProperty`, and the same for `help` | As in MCM: read in the game as the mod is selected and as another page is shown. A form with no name or description keeps the row's own text, and so does a property that is not in the game |
| `$$tintCT` in a text, which MCM's menu replaces with its tint color | Not read |
| A row's own `modName`, for a setting of another mod | Not read: the setting is of the row's mod |

## Values

| Feature | How the menu handles it |
| --- | --- |
| `GlobalValue` | As in MCM: read in every frame, and written as it is changed |
| `ModSettingBool`, `Int`, `Float`, `String` | As in MCM: read from MCM's two files as the mod is selected and as another page is shown, and written through `MCM.SetModSetting*` as it is changed |
| `PropertyValueBool`, `Int`, `Float`, `String` | As in MCM, with `scriptName`: read in every frame, and written as it is changed |
| No `sourceType` | As in MCM: the value is held while the mod is selected, and is never an unsaved change |
| A value that is not available | The row is disabled, and the help line says why |
| The default of a ModSetting | Its Reset, which MCM's menu does not have |
| `listFromForm`, `listFromProperty` | As in MCM: the stepper or the dropdown has the options it reads in the game, in place of its `options`, as its mod is selected and as another page is shown. A `sharedOptions` comes first, then a `listFromForm`. A list that is not in the game leaves it with no options, and the help line says why |
| `allowModifierKeys` of a hotkey | Not read: it is of an edit the menu does not make |
| `precision` of a text input | Not read: a number has 6 decimals at most, MCM's default |

## Actions

| Feature | How the menu handles it |
| --- | --- |
| `CallFunction` | As in MCM, on a button and on a value row, the row's after its value is written, and again as Revert sets it back. A button runs with the changes made, and does not ask to save first |
| `CallGlobalFunction` | As in MCM |
| `CallExternalFunction` | Not run: it calls a function of Flash, which the menu has none of |
| `RunConsoleCommand`, `SendEvent`, and any other type | Not run: they are actions of a hotkey, which MCM runs, and MCM's menu runs none of them in a row |
| `params` | As in MCM: `{i}`, `{f}` and `{b}`, JSON numbers and bools, and `{value}`. `{s}` is in MCM's documentation and not in its menu: the text is given as written, in both. A `{b}` text of a button is off for `0` and `false`, which MCM's menu has on |

## Text and files

| Feature | How the menu handles it |
| --- | --- |
| `$` tokens | As in MCM: the game's translations, which F4SE loads for each enabled game plugin |
| FallUI's language files | As FallUI's own code does in MCM's menu |
| An M8r intro image | The logo its code loads |
| `Data\MCM\Config\<mod>\settings.ini` | Read as the defaults of the ModSettings. Never written |
| `Data\MCM\Settings\<mod>.ini` | Read over the defaults. MCM writes it |
| `keybinds.json`, `Data\MCM\Settings\Keybinds.json` | Read for the keys of the hotkeys. MCM writes the second |

## MCM's behavior

| Feature | How the menu handles it |
| --- | --- |
| A change is saved as it is made | As in MCM, and it is a change of the session until Keep accepts it or Revert sets it back, which runs its row's action and sends its event again. Its `applies` is `unknown`: see [MCM compatibility](architecture.md#mcm-compatibility) |
| `OnMCMOpen`, `OnMCMClose` | As in MCM, and Keep sends `OnMCMClose` then `OnMCMOpen`, so a script that applies its properties as MCM's menu closes, as GCM does, applies the kept values |
| `OnMCMMenuOpen`, `OnMCMMenuOpen\|<mod>`, `OnMCMMenuClose\|<mod>` | Sent as a mod is selected and left, also as the menu closes, where MCM's menu sends none |
| `OnMCMSettingChange`, `OnMCMSettingChange\|<mod>` | As in MCM, after the action, as a change is made, and as Revert sets it back |
| MCM's Papyrus functions: `GetModSetting*`, `SetModSetting*`, `RefreshMenu`, `IsInstalled`, `GetVersionCode` | MCM's own, which keep working. `RefreshMenu` refreshes MCM's menu alone, and this menu reads every value again by itself |
| Hotkeys | MCM binds and runs them, for a key of the keyboard, the mouse or a gamepad. The menu shows them read-only. A button of a VR controller is no key MCM runs a hotkey for, and the menu does not add one |
| MCM's `Hotkey manager` and `MCM Settings` | Not listed: a mod's hotkeys are on its own pages |
| A DLL that reads MCM's file as the pause menu closes, which MCM's menu is in | After a Save of a session that wrote a ModSetting, the menu tells the game's listeners that the pause menu opened and closed, so the DLL reads its file. It gets the changes at Save, and not as each is made |
| The game is paused while MCM's menu is open | The game keeps running, so a script acts on a change while the menu is open |

## What MCM's authors work around

Workarounds for fields MCM lacks, and the config spec field that replaces each ([A config spec and MCM](schema.md#a-config-spec-and-mcm)):

| MCM workaround | Config spec |
| --- | --- |
| A script-set `hiddenSwitcher` shows rows while something holds: JOURNEY's mode dropdown sets two; Global Stash and NAC X show a row while another mod is installed | A condition on the dropdown's value; a condition on a loaded plugin. Global Stash's busy warning, which follows the game, waits for the native API |
| Condition groups are page-wide numbers, on while a switcher is | A condition names the setting and compares its value |
| A parameter's type is in its text, `{i}42` | A typed JSON value |
| `{value}` in a parameter's text gives the row's value | A setting's action, run by Save, with `"{value}"` in the setting's type |
| A switcher with no storage unhides rows (FallUI's "Unfold settings") | A section that folds |

## The MCM test set

The author's Mad God Overhaul mod list, which the e2e tests run against: 9 MCM mods, 687 controls.

- Controls: `switcher` 298, `section` 97, `slider` 53, `spacer` 46, `text` 42, `button` 32, `textinput` / `textinputFloat` 25, `hotkey` 23, `dropdown` 20, `stepper` 19, `image` 7.
- Value sources: `GlobalValue` 262, `ModSetting*` 151, `PropertyValueInt` 2. Actions: `CallFunction` 65 (Sim Settlements 2, Workshop Framework). Conditions: `groupCondition` as a number 67 times, as `AND` 4 times. `help` on 376 rows, `html` on 20.
- `$` text in 3 mods: Auto Eat's in the game's translation files; FallUI's and FallUI Workbench's in FallUI's own files, some with a nested `${Key}`. 7 images, all bitmaps in the mod's `lib.swf`.
- Text input in FallUI, FallUI Workbench and XDI; hotkeys in Sim Settlements 2 and Workshop Framework. Unlimited Survival Mode's and XDI's DLLs read MCM's file themselves.

What the test set lacks (`dropdownFiles`, `hiddenSwitcher`, `positioner`, `keyinput`, the other `PropertyValue*` types, `CallGlobalFunction`, `OR` and nested conditions, vector images, unmet `pluginRequirements`, an unparsable config, a non-English game) is covered by Global Stash and JOURNEY, from the author's other lists, where they have it, and otherwise by unit tests and the MCM showcase.
