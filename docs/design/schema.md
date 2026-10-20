# Declarative schema

This document describes the current direction of the Mods Config VR declarative JSON format. It is a design specification, not a frozen versioned schema.

A package has what an MCM config has, by MCM's names wherever MCM has the same thing, and more. [A package and MCM](#a-package-and-mcm) lists the differences.

The examples use a made-up mod, My Mod, that keeps its settings where a CommonFramework mod does.

## Package location

```text
Data/ModsConfigVR/Config/<PackageFolder>/config.json
```

The folder name is organizational. The package's `modName` is its machine identity.

One `config.json` is supported per package: a package has no includes or fragments. A list of options that several choices use is written once, as a [shared list](#shared-lists).

## Package structure

```json
{
  "minSchemaVersion": 1,
  "modName": "mymod",
  "displayName": "My Mod",
  "help": "Body tracking and holsters for VR.",
  "category": "Gameplay",
  "packageVersion": 1,

  "pluginRequirements": ["MyMod.esp"],
  "f4sePluginRequirements": ["MyMod"],

  "files": {},
  "defaultFile": "main",
  "applies": "live",
  "sharedLists": {},
  "content": [],
  "pages": []
}
```

### Required metadata

- `minSchemaVersion`: the lowest version of the Mods Config VR package schema that can read the package, as `minMcmVersion` is in an MCM config. See [Schema version](#schema-version).
- `modName`: the stable, unique identity of the package, named as in an MCM config.
- `displayName`: the mod's name as the player sees it, a localized text.
- `files`: named settings-file declarations when the package has file-backed settings.
- `applies`: when a change to a setting takes effect in the running mod: `live`, `save` or `restart`. See [When a change takes effect](#when-a-change-takes-effect).
- `content`: the items of the mod's front page, which is shown when the mod is selected. It is not empty. It has no row of its own in the navigation tree: the mod's row is its row.

`pages` is optional: the mod's other pages, in order, each a row under the mod in the tree. A page can have pages of its own; see [Pages](#pages). This is the shape of an MCM config, whose root `content` is the mod's front page too.

`help` is optional: what the mod is, shown in the help line while the ray is on the mod's row in the tree.

`packageVersion` is useful for diagnostics and package evolution but does not represent the installed mod's marketing version.

Author, website, Nexus ID, and mod version may be optional metadata later.

Two packages with the same `modName` are both [not available](ui-and-vr-interaction.md#notices). The host does not pick one of them.

IDs are compared without regard to case, both a package's `modName` and the ids inside a package: two that differ only in case are duplicates. Papyrus scripts name them, and Papyrus does not preserve the case of a string.

A native client is matched to its package by the package's `modName`. A CommonFramework mod's client registers under the mod's own name, so its package uses that name as its `modName`. The F4SE plugin name, when it differs, goes only in `f4sePluginRequirements`.

### Schema version

The schema has one version number, and it only grows. A package ships with its mod and is not updated when Mods Config VR is, so a new version adds fields and control types and never changes what an existing one means. A host reads every package written for an older version.

- `minSchemaVersion` is the lowest version that has everything the package uses. It is required, a whole number from 1: a package without it is not loaded, and the error is reported.
- On a host below that version the package's mod is [not available](ui-and-vr-interaction.md#notices), and the reason names the version it needs.
- A field or a control type added after version 1 is documented with the version that added it.
- A field the host does not know is reported in diagnostics, and the rest of the package is read. A typo is noticed, and a package that uses a newer field than its `minSchemaVersion` says still works on an older host, without that field.

### Category

`category` is optional: the name of the group the mod is listed under when the mod list is long. See [Mod categories](ui-and-vr-interaction.md#mod-categories).

- It is a plain name in English, never a `$` token. Mods are grouped by the name itself, compared without regard to case, and two mods that use different tokens would be in two groups.
- Mods Config VR ships a list of names, and an author uses one of them where it fits: `Gameplay`, `Combat`, `Weapons and Armor`, `Settlements`, `Characters`, `Interface`, `Visuals`, `Audio`, `VR`, `Utilities`. The menu shows these in the game's language where it has the translation.
- Any other name is accepted.
- One category can be spelled in several ways, as case is not compared. A shipped name is shown as the list spells it. Any other name is shown as the player typed it, or else as the first mod in the list writes it.
- An MCM config can have the same field, at its root.

An MCM mod that is already published names no category. Mods Config VR ships a table that gives one to the mods it knows, in the file that also has the list of names:

```text
Data/ModsConfigVR/Categories.json
```

```json
{
  "categories": ["Gameplay", "Combat", "Interface", "Settlements"],
  "mods": {
    "SS2": "Settlements",
    "FallUI": "Interface"
  }
}
```

`mods` names a mod by its `modName`. The mod's own `category` comes before the table's, and the player's choice before both.

### Requirements

Requirements keep a menu from being opened while the mod that owns it is not loaded. With a missing requirement the package's mod is [not available](ui-and-vr-interaction.md#notices): its row in the tree is disabled, and the reason names what is missing.

- `pluginRequirements` lists game plugin files (`.esp`, `.esm`, `.esl`) by file name, as in an MCM config.
- `f4sePluginRequirements` lists F4SE plugins by the name each registers with F4SE, not by DLL file name. The two can differ: FRIK's DLL is `FRIK.dll` and its F4SE plugin name is `F4VRBody`. The host sends the native save notification to these names.

A script-only mod lists its game plugin and names no F4SE plugin.

A part of a package that needs another mod is shown or enabled by a condition on a loaded plugin; see [Conditions](#conditions).

## Files

Packages may bind settings to multiple real files:

```json
{
  "files": {
    "main": {
      "format": "ini",
      "root": "myGames",
      "path": "Mods_Config/MyMod/MyMod.ini"
    },
    "calibration": {
      "format": "json",
      "root": "myGames",
      "path": "Mods_Config/MyMod/calibration.json"
    },
    "advanced": {
      "format": "toml",
      "root": "data",
      "path": "F4SE/Plugins/MyMod/advanced.toml"
    }
  }
}
```

Rules:

- A target path is always explicit; there is no inferred settings file.
- `format` is `ini`, `json`, or `toml`, and the file's extension must match it.
- `root` initially supports `data`, the game's `Data` directory, and `myGames`, which is `Documents/My Games/Fallout4VR`. The Documents folder is located through Windows, as CommonFramework does, so a redirected Documents folder is followed.
- A path is checked as written: absolute paths and `..` segments are rejected. Links are not resolved.
- The file does not have to exist. The host creates it on the first Save that changes one of its settings.
- A package may contain only informational content and buttons, or keep its settings in globals and script properties alone, and therefore need no file declaration.

## Pages

A page is a row in the navigation tree, under the mod or under another page. It has a `pageDisplayName`, its own `content`, and can have a `help` and `pages` of its own, so the tree has as many levels as the package gives it:

```json
{
  "pages": [
    {
      "pageDisplayName": "Body",
      "help": "Calibration, and where the body's parts are.",
      "content": [],
      "pages": [
        { "pageDisplayName": "Hands", "content": [] }
      ]
    }
  ]
}
```

- A page's `content` is required and is not empty. A page with pages under it has rows of its own too.
- A page's `help` is shown in the help line while the ray is on its row in the tree.
- A page can have an `id`, unique among the package's pages. The mod's native client is told it as the page is shown and left, to show in the world what the page sets; see [Page events](architecture.md#page-events). A page with no `id` sends none.
- A page's pages are rows under it in the tree, shown while it or one of them is selected, as a mod's pages are shown while the mod is selected. The breadcrumbs name every level, as `MODS CONFIG VR » My Mod » Body » Hands`. See [Navigation behavior](ui-and-vr-interaction.md#navigation-behavior).
- What a page has goes down to the pages under it: its `defaultFile`, its `applies`, and its `visibleWhen`, which hides them with it.
- An MCM config has one level of pages.

## Layout and file defaults

Pages do not belong to files. A page may edit multiple files, and one file may provide settings to several pages.

Repetitive file references are avoided through inherited `defaultFile` values. A setting takes the first it has:

1. Explicit `storage.file`
2. The `defaultFile` of the nearest section it is in
3. The `defaultFile` of the nearest page it is on: its own page, then the pages above it
4. Package-level `defaultFile`

```json
{
  "defaultFile": "main",
  "pages": [
    {
      "pageDisplayName": "Body",
      "content": [
        { "type": "section", "text": "Calibration", "defaultFile": "calibration", "content": [] }
      ]
    }
  ]
}
```

## When a change takes effect

A mod does not show every change as it is made. Some mods read their file again after a Save, and some values are read once, when the game starts. Only the mod's author knows which, so the package says it, with `applies`:

| Value     | The change takes effect     | What the host does                                                                    |
| --------- | --------------------------- | ------------------------------------------------------------------------------------- |
| `live`    | As it is made               | Sends the pending value to the mod's native client, as a preview, or writes a global or a property into the game |
| `save`    | After Save                  | No preview                                                                            |
| `restart` | After the game is restarted | No preview. The row is marked, and a Save that wrote it says that a restart is needed |

`applies` is required at the package's root: a package without it is not loaded, and the error is reported. Below the root it is optional, and a setting takes the nearest one:

1. The setting's own `applies`
2. The `applies` of the nearest section it is in
3. The `applies` of the nearest page it is on: its own page, then the pages above it
4. Package-level `applies`

```json
{
  "applies": "live",
  "pages": [
    {
      "pageDisplayName": "Advanced",
      "content": [
        { "type": "section", "text": "Game hooks", "applies": "restart", "content": [] }
      ]
    }
  ]
}
```

A row of the mod's front page has no page above it, so it goes from its sections to the package.

Which value a mod uses:

- A CommonFramework mod is `live`. The framework's client previews a key by reloading the mod's config values, so a setting that the mod reads only once is marked `restart`.
- A mod that reads its file again after a Save is `save`. It does so through a file watcher, the save notification, or a script's save event.
- A mod that reads its file only when the game starts is `restart`.
- A setting kept in a global or a script property is `live` when the mod reads it as it plays, as the game's conditions read a global. The host previews it itself.

A `live` setting kept in a file, of a mod with no native client, is handled as `save`, as a preview has nowhere to go.

The host does not check what a package says. A setting that is `live` in a mod that ignores the change looks broken to the player.

## Ordering

JSON arrays define display order:

- `pages` defines page order.
- `content` defines section/control order.
- `options` defines choice order.

JSON object property order is never meaningful. The `files` map is unordered.

An explicit numeric `order` is not used for ordinary declarative content. Placement of future dynamic or merged contributions can be designed separately.

## Content items

Every entry in a `content` array has a `type`, which says what the row is. The names are MCM's wherever MCM has the same row:

- `switcher`, `slider`, `stepper`, `dropdown`, `textinput`: a setting, which is an editable value bound to a file, a global or a property of a script. The type is its control.
- `section`: a heading with its own rows; see [Sections](#sections).
- `button`: runs an action.
- `text`, `spacer`, `image`: rows that hold no value; see [Text, spacers and images](#text-spacers-and-images).

A section holds its rows in its own `content`, and a page can have pages, so both nest.

## Settings

Value type, UI control, storage, and application behavior remain separate concepts. `type` is the control, as in an MCM config, and `valueType` is the type of the stored value. When a change takes effect is the setting's `applies`, its own or the nearest one above it; see [When a change takes effect](#when-a-change-takes-effect).

```json
{
  "id": "playerHeight",
  "type": "slider",
  "valueType": "float",
  "text": "Player Height",
  "help": "Adjusts the estimated player height.",
  "default": 1.0,
  "valueOptions": {
    "min": 0.5,
    "max": 1.5,
    "step": 0.01
  },
  "display": {
    "decimals": 2,
    "unit": "m"
  },
  "storage": {
    "section": "MyMod",
    "key": "fPlayerHeight"
  }
}
```

`text` is what the row says, and `help` is its help text, shown in the help line while the row is pointed at. Both are the names MCM uses, and every item that is a row has them: a setting, a section, a button, and an option of a choice.

Settings and button labels can set `"html": true` to style their `text` with the tags described in [Text, spacers and images](#text-spacers-and-images). Labels stay on one line: line and paragraph breaks become spaces, paragraph alignment is ignored, and font sizes are capped at the row's normal text size. Search and the help line use the label without tags. Without `html`, or when it is not the boolean `true`, labels remain plain text. This flag styles the row's label; `help`, `buttonText`, section headings and choice options remain plain text.

Every writable setting has an inline required `default`. Defaults use the same type and constraint validation as player values. The default is the value Reset applies, so it has to match the default the mod itself uses.

A `textinput` whose `valueType` is `int` or `float` takes a number, as MCM's `textinputInt` and `textinputFloat` do. Its limits are the `min` and `max` of its `valueOptions`.

A setting's `id` is optional. A setting without one gets its id from its storage, as a name and where it is, joined by a colon:

- An INI key: the key and its section, as `fPlayerHeight:MyMod`, which is how MCM names a ModSetting.
- A JSON or TOML value: the last name of its path, and the names before it joined with dots, as `fScale:Panel` for `["Panel", "fScale"]`. A path of two is named as an INI key is, so a mod that moves its config from INI to TOML keeps its ids. A path of one is its name alone.
- A global: its form, as `MyMod.esp|1234`.
- A property of a script: its name and its form, as `fStrength:MyMod.esp|800`.

An author gives an id to a setting that something names, a condition or a script, when the name should stay the same after its key is renamed or moved.

Setting IDs are unique across the package, given or made. This makes conditions, validation issues, API events, search, and diagnostics unambiguous. Two settings whose ids come out the same are an error of the package, and giving one of them an id resolves it. Only a setting and a page have an id: nothing names a section or a button.

Scripts read a setting kept in a file by the package's `modName` and the setting's `id`, so renaming either breaks the scripts that use it; see [Papyrus API](architecture.md#papyrus-api). For a setting with no `id` of its own, that is renaming its section or its key.

### Read-only

A setting with `"readOnly": true` shows its value and is not edited: a value the mod writes for the player to read, as a measured height. Its row is disabled, the help line says first that it is read-only, and it has no Reset, so it needs no `default`. While its storage has no value, the row shows none.

### Display

`display` says how a number is shown, in its control and in the help line:

- `decimals`: how many decimals it has. Without it, as many as its `step` has.
- `unit`: a text after the number, as `m` or `%`. It is a localized text.
- `percent`: `true` shows the number as a percentage of its range, from 0% at its `min` to 100% at its `max`. An opacity from 0 to 255 that is 217 shows as 85%.

The value stored is the number itself.

### Warnings

A setting or a button can warn the player while a condition holds, and block nothing: a value that works and has a cost, or a button whose action needs a save to be loaded again.

```json
{
  "id": "playerHeight",
  "warnings": [
    {
      "when": { "setting": "playerHeight", "greaterThan": 1.4 },
      "text": "Above 1.4 m your view can be in the ceiling."
    }
  ]
}
```

- `when` is a condition on the pending values, as a `visibleWhen` is; see [Conditions](#conditions). `text` is a localized text.
- While one holds, the row has a warning mark, and the help line shows its text before the row's help. The first that holds is the one shown.
- A warning does not block Save. An invalid value is not a warning: it has its own mark, and blocks Save.

### A setting's action

A setting can have an `action`, written as a button's; see [Buttons](#buttons). It runs when Save writes the setting's value, after every file and value of the Save is written, so it finds the saved value. It is how a script mod applies a change, as the action of an MCM row does. A parameter that is exactly `"{value}"` is the value written, in the setting's `valueType`:

```json
{
  "type": "switcher",
  "valueType": "bool",
  "text": "Automation",
  "default": true,
  "storage": { "global": "MyMod.esp|1234" },
  "action": { "type": "CallFunction", "form": "MyMod.esp|800", "function": "OnAutomationChanged", "params": ["{value}"] }
}
```

A setting that Save does not write runs nothing: one the player did not change, or changed back. A preview and a Discard run nothing either.

## Storage

A setting's `storage` says where its value is kept: a key of one of the package's files, a global, or a property of a script. It is apart from the setting's `id`.

### INI

```json
{
  "storage": {
    "file": "main",
    "section": "MyMod",
    "key": "fPlayerHeight"
  }
}
```

A key before the first section has no `section`. A `path` names a key too, as `["MyMod", "fPlayerHeight"]`, and a path of more than two names no key of an INI file.

The INI conventions the host follows are described under [Persistence model](architecture.md#persistence-model).

### JSON and TOML

An array represents an unambiguous path without dotted-key escaping rules:

```json
{
  "storage": {
    "file": "calibration",
    "path": ["body", "playerHeight"]
  }
}
```

- A path has any depth, so a value inside a table is a setting of its own. A CommonFramework TOML is `[Section] key`, a path of two.
- A table that is a CommonFramework value, as a color or a binding, is one value in one key, edited whole by its control; see [CommonFramework-specific controls](controls.md#commonframework-specific-controls). The path names its key.
- Any other table is not a value: the package names each of its keys.
- An array is one value, and a path does not name an element of it. Its control is still to be designed.

The resolved storage is included in native `SettingView` data so generic adapters do not parse this package themselves.

### Global and script property

```json
{
  "storage": { "global": "MyMod.esp|1234" }
}
```

```json
{
  "storage": { "form": "MyMod.esp|800", "scriptName": "MyMod:ConfigScript", "propertyName": "fStrength" }
}
```

A form is named as in an MCM config, by its plugin and its id in that plugin. `scriptName` is optional: without it the property is on the form's first script.

- A mod with no DLL keeps its settings where its scripts and the game read them, as an MCM mod does. The game's own conditions read a global, so a plugin with no scripts is configured too.
- The value is in the save game, not in a file. A new game starts with the plugin's value, and each save has its own. The setting still has its `default`, for Reset.
- It has a package's lifecycle, as a key of a file has: a change is pending until Save, and Discard drops it. The host previews a `live` one itself, by writing the pending value into the game, and writes the value from before back on Discard. See [Editing session](architecture.md#editing-session).
- A script reads it itself, so the [Papyrus API](architecture.md#papyrus-api) is for a setting kept in a file.
- A global or a property that is not in the game is not available, and its row says why, as an MCM row does.

A package has no MCM ModSetting storage: it does not need MCM, and a key of its own file, read through the Papyrus API, is what a ModSetting is for an MCM mod.

## Sections

A section is a heading with its own rows, in its `content`. A section can be in a section:

```json
{
  "type": "section",
  "text": "Holsters",
  "content": [
    { "type": "switcher", "valueType": "bool", "text": "Enable holsters", "default": true, "storage": { "section": "Holsters", "key": "bEnabled" } },
    {
      "type": "section",
      "text": "Advanced",
      "isCollapsible": true,
      "collapsed": true,
      "content": []
    }
  ]
}
```

- `isCollapsible` makes the section fold: a press on its heading hides its rows, and another shows them again. `collapsed` makes it start folded. A section with `collapsed` and no `isCollapsible` could not be opened, so it is open, and the log says so.
- A section starts as `collapsed` says each time its page is shown. How the player left it is not remembered.
- A folded section that has an invalid value or an unsaved change inside it marks its heading, so the player sees that something is there. It counts the settings in it whose rows would show when it is opened, also in the sections in it.
- Detailed settings are a folded section right after the settings they belong to, as the Advanced section above. There is no flag of its own for them.
- What a section has goes down to its rows and to the sections in it: its `defaultFile`, its `applies`, and its conditions; see [Conditions](#conditions).
- A section has a `text` and can have a `help`, as any row.

An MCM section is a heading row whose rows are the ones after it, up to the next section, and it does not nest. The MCM loader reads it as MCM's menu does.

## Conditions

A row can be shown or enabled by the pending values of settings: it is shown while its `visibleWhen` holds, and enabled while its `enabledWhen` holds. A row that is not enabled is drawn disabled. A section's conditions cover everything in it, so a row needs every condition above it to hold. A page can have a `visibleWhen`, which hides its row in the tree, and the pages under it.

```json
{
  "enabledWhen": {
    "setting": "enableHolsters",
    "equals": true
  }
}
```

```json
{
  "visibleWhen": {
    "all": [
      {
        "setting": "enableHolsters",
        "equals": true
      },
      {
        "setting": "interactionMode",
        "oneOf": ["physical", "hybrid"]
      }
    ]
  }
}
```

Supported scalar values are bool, integer, float, and string. There is no implicit conversion between strings, numbers, and booleans.

Operators:

- All scalar types: `equals`, `notEquals`
- Numeric types: `lessThan`, `lessThanOrEqual`, `greaterThan`, `greaterThanOrEqual`
- Strings and choices: `oneOf`, `notOneOf`
- Composition: `all`, `any`, `not`

A condition can also ask whether a plugin is loaded, for a row that needs another mod:

```json
{
  "type": "text",
  "text": "Camera effects are set by the ENB patch.",
  "visibleWhen": { "plugin": "MyMod - ENB Patch.esp" }
}
```

- `plugin` names a game plugin by its file name, and holds while it is loaded, as `pluginRequirements` checks one.
- `f4sePlugin` names an F4SE plugin by the name it registers with F4SE, as `f4sePluginRequirements` does.
- Both are answered once, as the game starts, and do not change while it runs. They join with the others, so `{ "not": { "plugin": "..." } }` holds while a plugin is not loaded.

Conditions evaluate pending values so the UI responds immediately. Hiding or disabling a setting does not reset it or discard an existing pending edit.

A condition that names a missing setting or compares incompatible types is invalid as a whole, however it is composed. It creates a developer diagnostic, and the item is shown and enabled as if it had no condition, so the mistake is visible instead of hiding content.

Conditions on game state that changes while the game runs are deferred to the native API design.

## Choices

A choice stores a value, never the text the player sees. Its options are in `valueOptions`, as in an MCM config.

A list of texts is the short form, also MCM's. The stored value is the index of the chosen text:

```json
{
  "type": "dropdown",
  "valueType": "int",
  "text": "Flashlight location",
  "default": 1,
  "valueOptions": { "options": ["Head", "Offhand", "Primary hand"] },
  "storage": { "section": "MyMod", "key": "iFlashlightLocation" }
}
```

When the stored value is not the index, each option gives its own:

```json
{
  "id": "interactionMode",
  "type": "dropdown",
  "valueType": "string",
  "default": "physical",
  "valueOptions": {
    "options": [
      { "value": "physical", "text": "Physical", "help": "Use direct VR interaction." },
      { "value": "hybrid", "text": "Hybrid", "help": "Combine physical interaction with button input." }
    ]
  }
}
```

All option values in one choice share the setting's `valueType`. Conditions compare stored values, not translated labels.

An option of the second form can have a `shortText`, shown where the choice has little room, as the stepper of the Focused panel.

A choice is a `dropdown` or a `stepper`, as in MCM. The host may select a more compact presentation in focused mode.

### Shared lists

A list of options that several choices use is written once, as in an MCM config. `sharedLists` at the package's root holds each list by its name, in either form of `options`, and a choice names one with `sharedOptions` in place of its own `options`:

```json
{
  "sharedLists": { "hands": ["Left", "Right"] },
  "content": [
    {
      "type": "stepper",
      "valueType": "int",
      "text": "Flashlight hand",
      "default": 0,
      "valueOptions": { "sharedOptions": "hands" },
      "storage": { "section": "Flashlight", "key": "iHand" }
    }
  ]
}
```

### Options from the game

A choice can take its options from the game, as its page is shown, for a list its author does not know: the settlements the player built, or the weathers that other mods add. MCM's two fields do it:

- `listFromForm` names a FormList, as `"MyMod.esp|900"`. The options are the names of its forms, or the editor ID of a form with no name.
- `listFromProperty` names a property of a script whose value is an array of texts: `{ "form": "MyMod.esp|800", "scriptName": "MyMod:ConfigScript", "propertyName": "SettlementNames" }`. The options are its texts. `scriptName` is optional: without it, the property is of the form's first script.

The list is read as the mod is selected, with the values of the session. A choice of `valueType` `int` stores the place of the option, as MCM does, and one of `valueType` `string` stores its text: a choice of another type has no options. A list that changes its order moves a stored place to another option. While the list cannot be read, the choice has no options, is disabled, and the help line says why. A value of the file that is none of the options read is invalid. The `default` is any value of the setting's type, as the options are not known before the game is read.

Options supplied by native code are deferred to the native API.

## Buttons

A button has no storage, default, dirty state, or persisted value.

Its `action` is an object with a `type`, as in an MCM config:

- `CallFunction` calls a function of a script on a form, and `CallGlobalFunction` a global function of a script, as in an MCM config. So a script-only mod has buttons too.
- `RunConsoleCommand` runs a console command, as MCM runs one for a hotkey. It needs no script, so a mod with no plugin of its own has buttons too.
- `CallNativeFunction` runs a function that the mod's native client registered, by its name.

```json
{
  "type": "button",
  "text": "Reset everything",
  "help": "Puts every value of the mod back.",
  "buttonText": "Reset",
  "action": { "type": "CallFunction", "form": "MyMod.esp|800", "function": "ResetAll" }
}
```

```json
{
  "type": "button",
  "text": "Hide the HUD",
  "action": { "type": "RunConsoleCommand", "command": "tm" }
}
```

The `params` of a `CallFunction` and a `CallGlobalFunction` are a list of JSON values, each given to the script in its own type: a number written with no point is an int, one with a point or an exponent is a float, `true` and `false` are bools, and a text is a string. A text is never read for a type: the `{i}`, `{f}` and `{b}` of an MCM config are not a package's. `"{value}"` is the value of a setting's action; see [A setting's action](#a-settings-action).

`buttonText` is the text on the button, in place of `Run`. It is a localized text.

`CallNativeFunction` is the package's own action:

```json
{
  "type": "button",
  "text": "Calibrate Height",
  "help": "Measures the current player height and updates calibration.",
  "buttonText": "Calibrate",
  "action": {
    "type": "CallNativeFunction",
    "function": "calibrate",
    "params": { "mode": "height", "includeScale": true, "sampleCount": 30 }
  }
}
```

Its `params` are named, typed JSON values, where the `params` of MCM's actions are a list. The function can also read the current pending settings snapshot. In a package without a native client, such a button is shown disabled.

A button can use ordinary visibility and enablement conditions, and [warnings](#warnings).

Optional confirmation is declarative:

```json
{
  "confirmation": {
    "title": "Reset Calibration?",
    "message": "This will discard the current body calibration.",
    "confirm": "Reset",
    "dangerous": true
  }
}
```

## Text, spacers and images

These rows hold no value. `text` and `spacer` are as in an MCM config.

A `text` is a paragraph in the panel's muted color, wrapped to the width of the rows. With `html`, its text takes the tags that MCM configs use:

```json
{ "type": "text", "text": "<p align='center'><font size='24'>Calibration</font></p>Stand straight and look ahead.<br><font color='#FFFFFF'>Do not move</font> until it is done.", "html": true }
```

| Tag            | What it does                                                                                          |
| -------------- | ----------------------------------------------------------------------------------------------------- |
| `<br>`         | Starts a new line                                                                                     |
| `<p>`          | A paragraph on lines of its own. Its `align` is `left`, `center` or `right`                           |
| `<font size>`  | The size of its text, on the scale of MCM's menu: 20 is the size of the rows' text                    |
| `<font color>` | The color of its text, as `#RRGGBB`                                                                   |
| `<b>`          | Its text is bold                                                                                      |
| `<i>`          | Its text is italic. A text that is both bold and italic is bold: the panel has no face for both       |
| `<u>`          | Its text is underlined, with the spaces between its words                                             |

A size is a size of MCM's menu, where the ordinary text is about 20, so a text copied from an MCM config looks here as it does there: a 40 is twice the rows' text and a 10 is half of it. The panel draws a size at 1.05 times its number, as its rows' text is 21 px. A size is not limited: a small one is hard to read in the headset, and it is the author's to choose.

Any other tag is dropped and its text is kept. Entities, as `&amp;`, are decoded.

A `help` is plain text. The tags that an MCM config has in a help are removed; see [Help line](ui-and-vr-interaction.md#help-line).

A `spacer` is empty space, one line high or `numLines`:

```json
{ "type": "spacer", "numLines": 2 }
```

An MCM `image` names a picture inside the mod's SWF, which a package does not have. A package's `image` names a texture file by its path under `Data/Textures`, and can give the size it is drawn at, in the panel's pixels:

```json
{ "type": "image", "texture": "MyMod/config/logo.dds", "width": 512, "height": 128 }
```

The texture is loaded by the game, as a texture of a mesh is, from a loose file or a BA2 archive, in any DDS format the game reads. With no size it is drawn at its own, and with one side alone the other is of its aspect. One wider than the rows is as wide as they are. A texture the game does not load is a box of its size that names it, and the log has the reason.

A warning is a `text` row, as in MCM. The one warning the host draws itself is the mark of a setting that needs a restart; see [When a change takes effect](#when-a-change-takes-effect).

## Validation

The limits in `valueOptions` cover ordinary scalar validation:

```json
{
  "valueOptions": {
    "min": 0.5,
    "max": 1.5,
    "step": 0.01
  }
}
```

```json
{
  "valueOptions": {
    "allowEmpty": false,
    "minLength": 1,
    "maxLength": 64
  }
}
```

`allowEmpty` of false is a `minLength` of 1. A length is counted in characters, not bytes. A text input takes no character past its `maxLength` as it is typed.

Choice values must be members of their declared options.

These limits are the whole validation. The mod's code is not asked, and expressions across settings are not planned.

Package/schema errors disable only the affected content where safe and appear in diagnostics: in the log, and to the package's author in [Developer tools](ui-and-vr-interaction.md#developer-tools). A package that cannot be read at all is [not available](ui-and-vr-interaction.md#notices). Player input errors disable Save until corrected.

## Localization

A text the player sees has one of three forms. Plain text is always valid:

```json
{
  "text": "Player Height",
  "help": "Adjusts the estimated player height."
}
```

A text that starts with `$` is a token, as in an MCM config:

```json
{
  "text": "$MyMod_PlayerHeight"
}
```

A token with a fallback keeps the English text in the package:

```json
{
  "text": {
    "token": "$MyMod_PlayerHeight",
    "fallback": "Player Height"
  }
}
```

The translations are the game's translation files, the ones a mod with an MCM menu already has:

```text
Data/Interface/Translations/
├── MyMod_en.txt
└── MyMod_fr.txt
```

Each line of a file is a token, a tab, and its text. A file is UTF-16 LE with a byte order mark, which is the only form F4SE reads.

Rules:

- F4SE loads one file for every enabled game plugin, named `<plugin>_<language>.txt`, for the language the game is set to. It does not fall back to English. The host looks a token up among them, as it does for an MCM config.
- A mod with no game plugin has no file loaded by F4SE. The host reads `<modName>_<language>.txt` for its package itself.
- The fallback is shown when no file has the token: the game is set to a language the mod has no file for, or the file misses the token. So a package whose fallbacks are English needs no English file.
- A token with no fallback is shown as it is written when no file has it, as in MCM, with a diagnostic.
- Every text the player sees takes these forms: the name of a mod and of a page, a section, a setting, a help, an option and its short text, the text on a button, a confirmation, and a unit.

A language can be shown only if the UI font has its characters. The font covers Western European languages today; wider coverage is an open question.

This is what an MCM config does, with the fallback added. The texts of an MCM config therefore carry over to a package as they are.

## A package and MCM

A package has what an MCM config has, by MCM's names, and more:

- Its settings are kept in the mod's own INI, TOML and JSON files, and in globals and properties of its scripts, and are saved together with Save.
- Every setting has a `default`, for Reset, and can be read-only. A number has its `display`.
- A setting and a button can warn while a condition holds.
- `applies` says when a change takes effect.
- A condition compares any value with its operators, can enable a row as well as show it, and can ask whether a plugin is loaded.
- Sections nest and fold, and pages nest.
- A parameter has its type as a JSON value, and the action of a setting is run by Save.
- A button can run a console command or a function of the mod's DLL, has its own text, and can ask for a confirmation.
- The mod and each page have a help, and the mod's DLL is told which of its pages is shown.
- A text can have a fallback, and an image is a texture file.

What an MCM config has that a package does not, and what the package has in its place:

| MCM                                                    | A package                                                                                          |
| ------------------------------------------------------ | -------------------------------------------------------------------------------------------------- |
| `hiddenSwitcher`, set by a script to show rows         | A condition on a value, as a dropdown's, or on a loaded plugin. A row shown by game state that changes, as a mod's busy warning, waits for the native API |
| `groupControl` and `groupCondition`, groups by number  | `visibleWhen` and `enabledWhen`, which name settings by their ids                                  |
| `{i}`, `{f}` and `{b}` in the text of a parameter      | A JSON value with its type                                                                         |
| `{value}` in the text of a parameter                   | A parameter that is exactly `"{value}"`, in the setting's type                                     |
| A switcher with no storage that shows hidden rows      | A section that folds                                                                               |
| `textinputInt`, `textinputFloat`                       | A `textinput` whose `valueType` is `int` or `float`                                                |
| ModSetting storage                                     | A key of the package's file, which scripts read through the Papyrus API                            |
| `hotkey`, `keyinput`                                   | The [InputBinding](controls.md#commonframework-specific-controls) control, for a CommonFramework mod |
| `dropdownFiles`                                        | A choice of the files of a folder; see [Choice of files](controls.md#choice-of-files)              |
| `positioner`, `CallExternalFunction`                   | None: they are of MCM's own Flash menu                                                             |
| `ownerModName`, pages under another mod                | None: a package's pages are its mod's alone                                                        |
