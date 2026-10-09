# Declarative schema

This document describes the current direction of the Mods Config VR declarative JSON format. It is a design specification, not a frozen versioned schema.

The examples use a made-up mod, My Mod, that keeps its settings where a CommonFramework mod does.

## Package location

```text
Data/ModsConfigVR/Config/<PackageFolder>/config.json
```

The folder name is organizational. The package's `modName` is its machine identity.

One `config.json` is supported per package for now. Includes and fragments are deliberately deferred.

## Package structure

```json
{
  "minSchemaVersion": 1,
  "modName": "mymod",
  "displayName": "My Mod",
  "category": "Gameplay",
  "packageVersion": 1,

  "pluginRequirements": ["MyMod.esp"],
  "f4sePluginRequirements": ["MyMod"],

  "files": {},
  "defaultFile": "main",
  "applies": "live",
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
- `content`: the items of the mod's front page, which is shown when the mod is selected. It has no row of its own in the navigation tree: the mod's row is its row.

`pages` is optional: the mod's other pages, in order. Each has a `pageDisplayName` and its own `content`, and is a row under the mod in the tree. This is the shape of an MCM config, whose root `content` is the mod's front page too.

`packageVersion` is useful for diagnostics and package evolution but does not represent the installed mod's marketing version.

Author, website, Nexus ID, and mod version may be optional metadata later.

Two packages with the same `modName` are both [not available](ui-and-vr-interaction.md#notices). The host does not pick one of them.

IDs are compared without regard to case, both a package's `modName` and the ids inside a package: two that differ only in case are duplicates. Papyrus scripts name them, and Papyrus does not preserve the case of a string.

A native client is matched to its package by the package's `modName`. A CommonFramework mod's client registers under the mod's own name, so its package uses that name as its `modName`. The F4SE plugin name, when it differs, goes only in `f4sePluginRequirements`.

### Schema version

The schema has one version number, and it only grows. A package ships with its mod and is not updated when Mods Config VR is, so a new version adds fields and control types and never changes what an existing one means. A host reads every package written for an older version.

- `minSchemaVersion` is the lowest version that has everything the package uses.
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
- A package may contain only informational content and buttons and therefore need no file declaration.

## Layout and file defaults

Pages do not belong to files. A page may edit multiple files, and one file may provide settings to several pages.

Repetitive file references are avoided through inherited `defaultFile` values:

1. Explicit `storage.file`
2. The `defaultFile` of the section the row is under
3. Page's `defaultFile`
4. Package-level `defaultFile`

```json
{
  "defaultFile": "main",
  "pages": [
    {
      "pageDisplayName": "Body",
      "content": [
        { "type": "section", "text": "Calibration", "defaultFile": "calibration" }
      ]
    }
  ]
}
```

## When a change takes effect

A mod does not show every change as it is made. Some mods read their file again after a Save, and some values are read once, when the game starts. Only the mod's author knows which, so the package says it, with `applies`:

| Value     | The change takes effect     | What the host does                                                                    |
| --------- | --------------------------- | ------------------------------------------------------------------------------------- |
| `live`    | As it is made               | Sends the pending value to the mod's native client, as a preview                      |
| `save`    | After Save                  | No preview                                                                            |
| `restart` | After the game is restarted | No preview. The row is marked, and a Save that wrote it says that a restart is needed |

`applies` is required at the package's root: a package without it is not loaded, and the error is reported. Below the root it is optional, and a setting takes the nearest one:

1. The setting's own `applies`
2. The `applies` of the section the row is under
3. Page's `applies`
4. Package-level `applies`

```json
{
  "applies": "live",
  "pages": [
    {
      "pageDisplayName": "Advanced",
      "content": [
        { "type": "section", "text": "Game hooks", "applies": "restart" }
      ]
    }
  ]
}
```

A row of the mod's front page has no page above it, so it goes from its section to the package.

Which value a mod uses:

- A CommonFramework mod is `live`. The framework's client previews a key by reloading the mod's config values, so a setting that the mod reads only once is marked `restart`.
- A mod that reads its file again after a Save is `save`. It does so through a file watcher, the save notification, or a script's save event.
- A mod that reads its file only when the game starts is `restart`.

A `live` setting of a mod with no native client is handled as `save`, as a preview has nowhere to go.

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

- `switcher`, `slider`, `stepper`, `dropdown`, `textinput`: a setting, which is an editable value bound to a file. The type is its control.
- `section`: a header row. The rows after it belong to it, up to the next section.
- `button`: runs an action.
- `text`, `spacer`, `image`: rows that hold no value; see [Text, spacers and images](#text-spacers-and-images).

Pages are a flat list, and so is the `content` of a page: nothing nests.

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

Every writable setting has an inline required `default`. Defaults use the same type and constraint validation as player values. The default is the value Reset applies, so it has to match the default the mod itself uses.

A setting's `id` is optional. A setting without one gets its id from its storage: the key and the section of an INI, as `fPlayerHeight:MyMod`, which is how MCM names a ModSetting, or the path of a JSON or TOML value, joined with dots. An author gives an id to a setting that something names, a condition or a script, when the name should stay the same after its key is renamed or moved.

Setting IDs are unique across the package, given or made. This makes conditions, validation issues, API events, search, and diagnostics unambiguous. Two settings whose ids come out the same are an error of the package, and giving one of them an id resolves it. Only a setting has an id: nothing names a page, a section or a button.

Scripts read a setting by the package's `modName` and the setting's `id`, so renaming either breaks the scripts that use it; see [Papyrus API](architecture.md#papyrus-api). For a setting with no `id` of its own, that is renaming its section or its key.

## Storage

A setting's `storage` says where its value is kept. It is apart from the setting's `id`.

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

The resolved storage is included in native `SettingView` data so generic adapters do not parse this package themselves.

## Sections

A section is a header row, as in an MCM config. The rows after it belong to it, up to the next section:

```json
{
  "type": "section",
  "text": "Advanced",
  "advanced": true,
  "collapsed": true
}
```

`collapsed` and `advanced` apply to the rows of the section. So do a section's `defaultFile` and `applies`, for the rows that have none of their own. A section's conditions cover its rows too; see [Conditions](#conditions).

`advanced: true` gives the host semantic information for consistent styling, collapsing, searching, and future global preferences. It is not inferred from the English label.

## Conditions

A row can be shown or enabled by the pending values of settings. A section's condition covers its rows, so a row does not repeat it, and a row with a condition of its own needs both to hold. A page can have a `visibleWhen`, which hides its row in the tree.

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

Conditions evaluate pending values so the UI responds immediately. Hiding or disabling a setting does not reset it or discard an existing pending edit.

A condition that names a missing setting or compares incompatible types is invalid as a whole, however it is composed. It creates a developer diagnostic, and the item is shown and enabled as if it had no condition, so the mistake is visible instead of hiding content.

Dynamic runtime/game-state conditions are deferred to the native API design.

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

A choice is a `dropdown` or a `stepper`, as in MCM. The host may select a more compact presentation in focused mode.

Dynamic option lists are deferred to the native API.

## Buttons

A button has no storage, default, dirty state, or persisted value.

Its `action` is an object with a `type`, as in an MCM config, and a package can use every action the menu supports for MCM. So a script-only mod has buttons too:

```json
{
  "type": "button",
  "text": "Reset everything",
  "help": "Puts every value of the mod back.",
  "action": { "type": "CallFunction", "form": "MyMod.esp|800", "function": "ResetAll" }
}
```

One action is the package's own. `CallNativeFunction` runs a function that the mod's native client registered, by its name:

```json
{
  "type": "button",
  "text": "Calibrate Height",
  "help": "Measures the current player height and updates calibration.",
  "action": {
    "type": "CallNativeFunction",
    "function": "calibrate",
    "params": { "mode": "height", "includeScale": true, "sampleCount": 30 }
  }
}
```

Its `params` are named, typed JSON values, where the `params` of MCM's actions are a list. The function can also read the current pending settings snapshot. In a package without a native client, such a button is shown disabled.

A button can use ordinary visibility and enablement conditions.

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

A `text` is a paragraph in the panel's muted color, wrapped to the width of the rows. With `html`, its text takes the three tags that MCM configs use:

```json
{ "type": "text", "text": "<p align='center'><font size='24'>Calibration</font></p>Stand straight and look ahead.<br><font color='#FFFFFF'>Do not move</font> until it is done.", "html": true }
```

| Tag            | What it does                                                                                          |
| -------------- | ----------------------------------------------------------------------------------------------------- |
| `<br>`         | Starts a new line                                                                                     |
| `<p>`          | A paragraph on lines of its own. Its `align` is `left`, `center` or `right`                           |
| `<font size>`  | One of the panel's three text sizes: the title size from 24, the detail size up to 16, else the body  |
| `<font color>` | The color of its text, as `#RRGGBB`                                                                   |

Any other tag is dropped and its text is kept. That includes `<b>`, `<i>` and `<u>`; see [Open questions](open-questions.md#ui-work). Entities, as `&amp;`, are decoded.

A `help` is plain text. The tags that an MCM config has in a help are removed; see [Help line](ui-and-vr-interaction.md#help-line).

A `spacer` is empty space, one line high or `numLines`:

```json
{ "type": "spacer", "numLines": 2 }
```

An MCM `image` names a picture inside the mod's SWF, which a package does not have. A package's `image` names a texture file by its path under `Data/Textures`, and can give the size it is drawn at:

```json
{ "type": "image", "texture": "MyMod/config/logo.dds", "width": 512, "height": 128 }
```

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

Choice values must be members of their declared options.

These limits are the whole validation. The mod's code is not asked, and expressions across settings are not planned.

Package/schema errors disable only the affected content where safe and appear in diagnostics. A package that cannot be read at all is [not available](ui-and-vr-interaction.md#notices). Player input errors disable Save until corrected.

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
- Every text the player sees takes these forms: the name of a mod and of a page, a section, a setting, a help, an option, a confirmation, and a unit.

A language can be shown only if the UI font has its characters. The font covers Western European languages today; wider coverage is an open question.

This is what an MCM config does, with the fallback added. The texts of an MCM config therefore carry over to a package as they are.
