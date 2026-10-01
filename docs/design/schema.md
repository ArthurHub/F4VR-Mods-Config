# Declarative schema

This document describes the current direction of the Mods Config VR declarative JSON format. It is a design specification, not a frozen versioned schema.

## Package location

```text
Data/F4VRModsConfig/Config/<PackageFolder>/config.json
```

The folder name is organizational. The package's explicit `id` is its machine identity.

One `config.json` is supported per package for now. Includes and fragments are deliberately deferred.

## Package structure

```json
{
  "schemaVersion": 1,
  "id": "frik",
  "name": "FRIK",
  "packageVersion": 1,

  "requirements": {
    "plugins": ["FRIK.esp"],
    "f4sePlugins": ["FRIK.dll"]
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

### Requirements

Requirements prevent orphaned menus from appearing when the owning mod is absent. A missing requirement hides the package from normal navigation and reports it in diagnostics.

## Files

Packages may bind settings to multiple real files:

```json
{
  "files": {
    "main": {
      "format": "ini",
      "root": "data",
      "path": "F4SE/Plugins/FRIK.ini"
    },
    "calibration": {
      "format": "json",
      "root": "myGames",
      "path": "F4VR/FRIK/calibration.json"
    },
    "advanced": {
      "format": "toml",
      "root": "data",
      "path": "F4SE/Plugins/FRIK/advanced.toml"
    }
  }
}
```

Rules:

- A target path is always explicit; there is no inferred settings file.
- `format` is `ini`, `json`, or `toml`.
- `root` initially supports `data` and `myGames`.
- Paths must remain below their selected root.
- Absolute paths, `..`, and resolved escapes are rejected.
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

## Settings

Value type, UI control, storage binding, and application behavior remain separate concepts.

```json
{
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
    "section": "Body",
    "key": "fPlayerHeight"
  }
}
```

Every writable setting has an inline required `default`. Defaults use the same type and constraint validation as player values.

Setting IDs are unique across the package. This makes conditions, validation issues, API events, search, and diagnostics unambiguous.

## Bindings

### INI

```json
{
  "binding": {
    "file": "main",
    "section": "Body",
    "key": "fPlayerHeight"
  }
}
```

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

A missing setting reference or incompatible comparison creates a developer diagnostic and evaluates as false.

Dynamic runtime/game-state conditions are deferred to the native API design.

## Choices

Choices use typed stored values rather than localized display labels:

```json
{
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

Translation files sit beside `config.json` and use the requested `config.<language>.json` naming:

```text
Data/F4VRModsConfig/Config/FRIK/
├── config.json
├── config.fr.json
├── config.de.json
└── config.ja.json
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

MCM packages retain their existing `$Token` and MCM translation-file conventions through the MCM adapter.
