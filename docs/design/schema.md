# Declarative schema

This document describes the current direction of the Mods Config VR declarative JSON format. It is a design specification, not a frozen versioned schema.

The examples use a made-up mod, My Mod, that keeps its settings where a CommonFramework mod does.

## Package location

```text
Data/ModsConfigVR/Config/<PackageFolder>/config.json
```

The folder name is organizational. The package's explicit `id` is its machine identity.

One `config.json` is supported per package for now. Includes and fragments are deliberately deferred.

## Package structure

```json
{
  "schemaVersion": 1,
  "id": "mymod",
  "name": "My Mod",
  "packageVersion": 1,

  "requirements": {
    "plugins": ["MyMod.esp"],
    "f4sePlugins": ["MyMod"]
  },

  "files": {},
  "defaultFile": "main",
  "pages": []
}
```

### Required metadata

- `schemaVersion`: version of the Mods Config VR package schema.
- `id`: stable, unique package identity.
- `name`: player-facing localized text.
- `files`: named settings-file declarations when the package has file-backed settings.
- `pages`: ordered page definitions.

`packageVersion` is useful for diagnostics and package evolution but does not represent the installed mod's marketing version.

Author, website, Nexus ID, and mod version may be optional metadata later.

Duplicate package IDs disable the conflicting definitions and create diagnostics. The host does not select one unpredictably.

IDs are compared without regard to case, both package IDs and the IDs inside a package: two that differ only in case are duplicates. Papyrus scripts name them, and Papyrus does not preserve the case of a string.

A native client is matched to its package by the package `id`. A CommonFramework mod's client registers under the mod's own name, so its package uses that name as its `id`. The F4SE plugin name, when it differs, goes only in `requirements.f4sePlugins`.

### Requirements

Requirements prevent orphaned menus from appearing when the owning mod is absent. A missing requirement hides the package from normal navigation and reports it in diagnostics.

- `plugins` lists game plugin files (`.esp`, `.esm`, `.esl`) by file name.
- `f4sePlugins` lists F4SE plugins by the name each registers with F4SE, not by DLL file name. The two can differ: FRIK's DLL is `FRIK.dll` and its F4SE plugin name is `F4VRBody`. The host sends the native save notification to these names.

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
- A package may contain only informational content and actions and therefore need no file declaration.

## Layout and file defaults

Pages do not belong to files. A page may edit multiple files, and one file may provide settings to several pages.

Repetitive file references are avoided through inherited `defaultFile` values:

1. Explicit `binding.file`
2. Nearest enclosing section's `defaultFile`
3. Page's `defaultFile`
4. Package-level `defaultFile`

```json
{
  "defaultFile": "main",
  "pages": [
    {
      "id": "body",
      "name": "Body",
      "content": [
        {
          "kind": "section",
          "id": "calibration",
          "name": "Calibration",
          "defaultFile": "calibration",
          "content": []
        }
      ]
    }
  ]
}
```

## Ordering

JSON arrays define display order:

- `pages` defines page order.
- `content` defines section/control order.
- `options` defines choice order.

JSON object property order is never meaningful. The `files` map is unordered.

An explicit numeric `order` is not used for ordinary declarative content. Placement of future dynamic or merged contributions can be designed separately.

## Content items

Every entry in a `content` array has a `kind`:

- `setting`: an editable value bound to a file.
- `section`: a container for other items.
- `action`: a named operation.
- `text`, `notice`, `separator`, `spacer`, `image`: informational and layout elements. Their fields are not designed yet.

Pages are a flat list. Only sections nest.

## Settings

Value type, UI control, storage binding, and application behavior remain separate concepts.

```json
{
  "kind": "setting",
  "id": "playerHeight",
  "type": "float",
  "control": "slider",
  "name": "Player Height",
  "description": "Adjusts the estimated player height.",
  "default": 1.0,
  "constraints": {
    "min": 0.5,
    "max": 1.5,
    "step": 0.01
  },
  "display": {
    "decimals": 2,
    "unit": "m"
  },
  "binding": {
    "section": "MyMod",
    "key": "fPlayerHeight"
  }
}
```

Every writable setting has an inline required `default`. Defaults use the same type and constraint validation as player values. The default is the value Reset applies, so it has to match the default the mod itself uses.

Setting IDs are unique across the package. This makes conditions, validation issues, API events, search, and diagnostics unambiguous. Pages, sections, and actions share the same namespace, so no two items in a package have the same `id`.

Scripts read a setting by the package `id` and the setting `id`, so renaming either breaks the scripts that use it; see [Papyrus API](architecture.md#papyrus-api).

## Bindings

### INI

```json
{
  "binding": {
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
  "binding": {
    "file": "calibration",
    "path": ["body", "playerHeight"]
  }
}
```

The resolved binding is included in native `SettingView` data so generic adapters do not parse this package themselves.

## Sections

Sections are structural containers:

```json
{
  "kind": "section",
  "id": "advanced",
  "name": "Advanced",
  "advanced": true,
  "collapsed": true,
  "content": []
}
```

`advanced: true` gives the host semantic information for consistent styling, collapsing, searching, and future global preferences. It is not inferred from the English label.

## Conditions

Settings and sections may be enabled or visible based on other pending values.

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

Choices use typed stored values rather than localized display labels:

```json
{
  "kind": "setting",
  "id": "interactionMode",
  "type": "string",
  "control": "choice",
  "default": "physical",
  "presentation": "dropdown",
  "options": [
    {
      "value": "physical",
      "name": "Physical",
      "description": "Use direct VR interaction."
    },
    {
      "value": "hybrid",
      "name": "Hybrid",
      "description": "Combine physical interaction with button input."
    }
  ]
}
```

All option values in one choice share the setting's declared scalar type. Conditions compare stored values, not translated labels.

Supported presentation hints are `auto`, `dropdown`, `stepper`, and `radio`. The host may select a more compact presentation in focused mode.

Dynamic option lists are deferred to the native API.

## Actions

Actions have no binding, default, dirty state, or persisted value.

```json
{
  "kind": "action",
  "id": "calibrateHeight",
  "name": "Calibrate Height",
  "description": "Measures the current player height and updates calibration.",
  "action": "calibrate",
  "parameters": {
    "mode": "height",
    "includeScale": true,
    "sampleCount": 30
  }
}
```

Parameters are typed JSON values. The registered handler can also read the current pending settings snapshot. There is no expression or string-substitution language initially.

An action runs a name that the package's native client registered. In a package without a native client, the action is shown disabled.

Actions can use ordinary visibility and enablement conditions.

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

## Validation

Built-in constraints cover ordinary scalar validation:

```json
{
  "constraints": {
    "min": 0.5,
    "max": 1.5,
    "step": 0.01
  }
}
```

```json
{
  "constraints": {
    "allowEmpty": false,
    "minLength": 1,
    "maxLength": 64
  }
}
```

Choice values must be members of their declared options.

Optional native validators receive both the proposed setting and a read-only pending snapshot. An optional batch validator runs before Save for cross-setting invariants. Complex declarative validation expressions are not currently planned.

Package/schema errors disable only the affected content where safe and appear in diagnostics. Player input errors disable Save until corrected.

## Localization

Inline text is always valid:

```json
{
  "name": "Player Height",
  "description": "Adjusts the estimated player height."
}
```

Any player-facing field may instead use a localized-text object:

```json
{
  "name": {
    "key": "settings.playerHeight.name",
    "fallback": "Player Height"
  },
  "description": {
    "key": "settings.playerHeight.description",
    "fallback": "Adjusts the estimated player height."
  }
}
```

Translation files sit beside `config.json` and are named `config.<language>.json`:

```text
Data/ModsConfigVR/Config/MyMod/
├── config.json
├── config.fr.json
└── config.de.json
```

```json
{
  "settings.playerHeight.name": "Taille du joueur",
  "settings.playerHeight.description": "Ajuste la taille estimée du joueur."
}
```

Rules:

- Translation keys are package-scoped.
- Files are UTF-8 JSON.
- `fallback` prevents raw tokens from appearing to players.
- English translation files are optional when fallbacks are English.
- Missing keys fall back and create developer diagnostics.
- Regional lookup may fall back from `pt-BR` to `pt` and then to the inline fallback.
- Pages, sections, settings, descriptions, options, actions, confirmations, units, and validation messages use the same localized-text representation.

A language can be shown only if the UI font has its characters. The font covers Western European languages today; wider coverage is an open question.

MCM packages retain their existing `$Token` and MCM translation-file conventions through the MCM bridge.
