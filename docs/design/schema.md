# Declarative schema

This document describes the current direction of the Mods Config FO4 VR declarative JSON format. It is a design specification, not a frozen versioned schema.

A config spec has what an MCM config has, by MCM's names wherever MCM has the same thing, and more. [A config spec and MCM](#a-config-spec-and-mcm) lists the differences.

The examples use a made-up mod, My Mod, that keeps its settings where a CommonFramework mod does.

## Config spec location

```text
Data/ModsConfigFO4/Config/<ConfigSpecFolder>/spec.json
```

The folder name is organizational. The config spec's `modName` is its machine identity.

One `spec.json` is supported per config spec: a config spec has no includes or fragments. A list of options that several choices use is written once, as a [shared list](#shared-lists).

## Config spec structure

```json
{
  "minSchemaVersion": 1,
  "modName": "mymod",
  "displayName": "My Mod",
  "help": "Body tracking and holsters for VR.",
  "category": "Gameplay",
  "specVersion": 1,

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

- `minSchemaVersion`: the lowest version of the Mods Config FO4 VR config spec schema that can read the config spec, as `minMcmVersion` is in an MCM config. See [Schema version](#schema-version).
- `modName`: the stable, unique identity of the config spec, named as in an MCM config.
- `displayName`: the mod's name as the player sees it, a localized text.
- `files`: named settings-file declarations when the config spec has file-backed settings.
- `content`: the items of the mod's front page, which is shown when the mod is selected. It is not empty. It has no row of its own in the navigation tree: the mod's row is its row.

`pages` is optional: the mod's other pages, in order, each a row under the mod in the tree. A page can have pages of its own; see [Pages](#pages). This is the shape of an MCM config, whose root `content` is the mod's front page too.

`help` is optional: what the mod is, shown in the help line while the ray is on the mod's row in the tree.

`applies` is optional: when a change to a setting takes effect in the running mod, `unknown`, `live`, `save` or `restart`. See [When a change takes effect](#when-a-change-takes-effect).

`specVersion` is useful for diagnostics and config spec evolution but does not represent the installed mod's marketing version.

Author, website, Nexus ID, and mod version may be optional metadata later.

Two config specs with the same `modName` are both [not available](ui-and-vr-interaction.md#notices). The host does not pick one of them.

A mod that has a config spec and an MCM menu, matched by the config spec's `modName` and the MCM config's, is listed by its config spec alone: its MCM menu is left out, so a mod that ships a config spec beside its MCM menu is not listed twice.

IDs are compared without regard to case, both a config spec's `modName` and the ids inside a config spec: two that differ only in case are duplicates. Papyrus scripts name them, and Papyrus does not preserve the case of a string.

A native client is matched to its config spec by the config spec's `modName`. A CommonFramework mod's client registers under the mod's own name, so its config spec uses that name as its `modName`. The F4SE plugin name, when it differs, goes only in `f4sePluginRequirements`.

### Schema version

The schema has one version number, and it only grows. A config spec ships with its mod and is not updated when Mods Config FO4 VR is, so a new version adds fields and control types and never changes what an existing one means. A host reads every config spec written for an older version.

- `minSchemaVersion` is the lowest version that has everything the config spec uses. It is required, a whole number from 1: a config spec without it is not loaded, and the error is reported.
- On a host below that version the config spec's mod is [not available](ui-and-vr-interaction.md#notices), and the reason names the version it needs.
- A field or a control type added after version 1 is documented with the version that added it.
- A field the host does not know is reported in diagnostics, and the rest of the config spec is read. A typo is noticed, and a config spec that uses a newer field than its `minSchemaVersion` says still works on an older host, without that field.

### Category

`category` is optional: the name of the group the mod is listed under when the mod list is long. See [Mod categories](ui-and-vr-interaction.md#mod-categories).

- It is a plain name in English, never a `$` token. Mods are grouped by the name itself, compared without regard to case, and two mods that use different tokens would be in two groups.
- Mods Config FO4 VR ships a list of names, and an author uses one of them where it fits: `Gameplay`, `Combat`, `Weapons and Armor`, `Settlements`, `Characters`, `Interface`, `Visuals`, `Audio`, `VR`, `Utilities`. The menu shows these in the game's language where it has the translation.
- Any other name is accepted.
- One category can be spelled in several ways, as case is not compared. A shipped name is shown as the list spells it. Any other name is shown as the player typed it, or else as the first mod in the list writes it.
- An MCM config can have the same field, at its root.

An MCM mod that is already published names no category. Mods Config FO4 VR ships a table that gives one to the mods it knows, in the file that also has the list of names:

```text
Data/ModsConfigFO4/Categories.json
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

Requirements keep a menu from being opened while the mod that owns it is not loaded. With a missing requirement the config spec's mod is [not available](ui-and-vr-interaction.md#notices): its row in the tree is disabled, and the reason names what is missing.

- `pluginRequirements` lists game plugin files (`.esp`, `.esm`, `.esl`) by file name, as in an MCM config.
- `f4sePluginRequirements` lists F4SE plugins by the name each registers with F4SE, not by DLL file name. The two can differ: FRIK's DLL is `FRIK.dll` and its F4SE plugin name is `F4VRBody`. The host sends the native save notification to these names.

A script-only mod lists its game plugin and names no F4SE plugin.

A part of a config spec that needs another mod is shown or enabled by a condition on a loaded plugin; see [Conditions](#conditions).

## Files

Config specs may bind settings to multiple real files:

```json
{
  "files": {
    "main": {
      "format": "ini",
      "root": "myGames",
      "path": "Mods_Config/MyMod/MyMod.ini"
    },
    "calibration": {
      "format": "toml",
      "root": "myGames",
      "path": "Mods_Config/MyMod/calibration.toml"
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
- `format` is `ini` or `toml`, and the file's extension must match it.
- `root` initially supports `data`, the game's `Data` directory, and `myGames`, which is `Documents/My Games/Fallout4VR`. The Documents folder is located through Windows, as CommonFramework does, so a redirected Documents folder is followed. The game's own INI files under `myGames`, as `Fallout4Custom.ini`, are valid targets: the game reads them as it starts, so their settings are `applies` `restart`.
- A path is checked as written: absolute paths and `..` segments are rejected. Links are not resolved.
- The file does not have to exist. The host creates it on the first Save that changes one of its settings, and a config spec cannot forbid it.
- `pathFromClientState` is optional: the name of a [state](architecture.md#a-clients-state) of the mod's native client whose text is the file's absolute path, for a mod that loads its file from another path at runtime. The host asks it as the mod is selected and as its values are read again, and reads, previews and saves that file while the client answers it with a path under `Data` or `Documents/My Games/Fallout4VR`. With no client, a client that is cut off or does not answer it, or a path elsewhere, which the log names, it uses the declared `root` and `path`. The file keeps its `format`, and its extension must match it too. A CommonFramework mod's client answers `configFile` with the path of the config file it loaded; see [CommonFramework adapter](architecture.md#commonframework-adapter).

```json
"main": { "format": "toml", "root": "myGames", "path": "Mods_Config/FRIK/FRIK.toml", "pathFromClientState": "configFile" }
```

FRIK loads `FRIK_FOLVR.toml` in place of `FRIK.toml` with Fallout London VR, unless its `bIgnoreFalloutLondonVR` is on, so its file names that state, and the menu edits the file FRIK loaded, whatever chose it.
- A config spec may contain only informational content and buttons, or keep its settings in globals and script properties alone, and therefore need no file declaration.

## Pages

A page is a row in the navigation tree, under the mod or under another page. It has a `pageDisplayName`, its own `content`, and can have a `help` and `pages` of its own, so the tree has as many levels as the config spec gives it:

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
- A page can have an `id`, unique among the config spec's pages. The mod's native client is told it as the page is shown and left, to show in the world what the page sets; see [The client's events](architecture.md#the-clients-events). A page with no `id` sends none.
- A page's pages are rows under it in the tree, shown while it or one of them is selected, as a mod's pages are shown while the mod is selected. The breadcrumbs name every level, as `MODS CONFIG FO4 » My Mod » Body » Hands`. See [Navigation behavior](ui-and-vr-interaction.md#navigation-behavior).
- A page can have `visibleWhen` and `enabledWhen`, using the same [conditions](#conditions) as rows. `visibleWhen` hides its navigation row and its children. `enabledWhen` leaves the row visible but disabled, with an explanation in the help line, and prevents opening it or its children.
- What a page has goes down to the pages under it: its `defaultFile`, its `applies`, its `visibleWhen` and its `enabledWhen`. Page conditions also cover its content, combined with each row's own conditions.
- If the selected page becomes hidden or disabled, the menu returns to the nearest visible, enabled parent, or the mod's front page. Unsaved changes stay pending. Direct page requests follow the same rule, including requests from a native client.
- An MCM config has one level of pages.

## Layout and file defaults

Pages do not belong to files. A page may edit multiple files, and one file may provide settings to several pages.

Repetitive file references are avoided through inherited `defaultFile` values. A setting takes the first it has:

1. Explicit `storage.file`
2. The `defaultFile` of the nearest section it is in
3. The `defaultFile` of the nearest page it is on: its own page, then the pages above it
4. Config spec-level `defaultFile`

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

Save writes a change, and the mod acts on it in its own way. Some mods read their file again after a Save, some values are read once, when the game starts, and a mod that reads a value every frame can still keep a copy of it. Only the mod's author knows which, so the config spec can say it, with `applies`:

| Value     | The change takes effect     | What the host does                                                                    |
| --------- | --------------------------- | ------------------------------------------------------------------------------------- |
| `unknown` | Not known to the menu       | No preview. The default                                                               |
| `live`    | As it is made: the mod handles the change it is told of | Sends the pending value to the mod's native client, as a preview, or writes a global or a property into the game |
| `save`    | After Save                  | No preview                                                                            |
| `restart` | After the game is restarted | No preview. A Save that wrote it says that a restart is needed                        |

The help line shows the `applies` of the row the ray is on while its change waits for Save, but for `unknown`, which has no mark, as most settings have it. The Help page says what each value means. `unknown` and `save` differ only there: the host does the same for both.

`applies` is optional at every level, and a setting takes the nearest one:

1. The setting's own `applies`
2. The `applies` of the nearest section it is in
3. The `applies` of the nearest page it is on: its own page, then the pages above it
4. Config spec-level `applies`
5. `unknown`

`unknown` can be written too, for a page, a section or a setting under one that says more.

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

A row of the mod's front page has no page above it, so it goes from its sections to the config spec.

Which value a mod uses:

- A CommonFramework mod is `live`. The framework's client previews a key by reloading the mod's config values, so a setting that the mod reads only once is marked `restart`.
- A mod that reads its file again after a Save is `save`. It does so through a file watcher, the save notification, or a script's save event.
- A mod that reads its file only when the game starts is `restart`.
- A setting kept in a global or a script property is `live` when the mod reads it as it plays, as the game's conditions read a global. The host previews it itself.
- A setting the author cannot say of stays `unknown`.

A `live` setting kept in a file, of a mod with no native client, is not previewed, as a preview has nowhere to go. It is still shown as `live`: the mod can act on the change in its own way.

The host does not check what a config spec says. A setting that is `live` in a mod that ignores the change looks broken to the player.

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
- `cf.inputBinding`: a setting of a CommonFramework controller binding; see [CommonFramework values](#commonframework-values).
- `cf.haptic`: a setting of a CommonFramework haptic pattern; see [CommonFramework values](#commonframework-values).
- `cf.color`: a setting of a CommonFramework color; see [CommonFramework values](#commonframework-values).
- `cf.transform`: a setting of a CommonFramework transform; see [CommonFramework values](#commonframework-values).
- `cf.activationSphere`: a section of a CommonFramework mod's TOML that is an activation sphere, drawn as one group of its keys; see [CommonFramework values](#commonframework-values).
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

### CommonFramework values

A setting whose `type` starts with `cf.` edits a value in the form CommonFramework fixes for its TOML config; see [CommonFramework-specific controls](controls.md#commonframework-specific-controls). The prefix says the value is the framework's, so an author knows its words and its checks are the framework's too.

`cf.inputBinding` is a controller binding, the inline table of a `k` key. It is kept in a value of a TOML file, as the framework keeps it, and its `valueType` is `string`, the TOML text of the table. Its `default` is the table as a JSON object, which the framework reads as it reads its config. `"valueOptions": { "suppress": true }` offers Suppress from the game in its editor, for a binding whose consumer applies it, as an activation sphere's. `hands`, `types` and `buttons` in its `valueOptions` are lists of the framework's words of each, which the binding takes alone, as a menu's Back that is no hold and no press of the trigger: its editor offers those, and a binding of another is invalid, with what it takes. The button of a push is its axis's, and a binding that is off is always taken. A list left out takes every one, and a word that is none of its part's is left out, with a warning. A `default` the setting does not take is an error, and it then has no Reset.

```json
{
  "type": "cf.inputBinding",
  "text": "Grab",
  "default": { "hand": "offhand", "type": "tap", "button": "trigger" },
  "valueOptions": { "suppress": true },
  "storage": { "path": ["HeadActivation", "kPrimaryBinding"] }
}
```

```json
{
  "type": "cf.inputBinding",
  "text": "Back",
  "default": { "hand": "any", "type": "tap", "button": "grip" },
  "valueOptions": { "types": ["tap", "press", "release", "longpress", "double"], "buttons": ["grip", "a", "b", "thumbstick"] },
  "storage": { "path": ["ModsConfigFO4VR", "kBackBinding"] }
}
```

`cf.haptic` is a haptic pattern, the text of an `s` key: the name of a pattern of the framework's library, or `"none"`. Its `valueType` is `string`, and its options are the menu's, None and the framework's patterns, so it has no `valueOptions`. A text is read as the framework reads it, with no regard to case, spaces, underscores and hyphens, so `"double click"` is DoubleClick, and `"off"` and an empty text are none. A changed value is written as the library names it. Its `default` is the name of a pattern: one the framework does not have is an error, and it then has no Reset. A name the framework does not have in the file is invalid.

```json
{
  "type": "cf.haptic",
  "text": "Entry haptic",
  "default": "Tick",
  "storage": { "path": ["HeadActivation", "sEntryHaptic"] }
}
```

`cf.color` is a color, the inline table of a `c` key, `{ rgb = [r, g, b], alpha = a }`, each a whole number in 0..255 and each key optional. It is kept in a value of a TOML file, as the framework keeps it, and its `valueType` is `string`, the TOML text of the table. Its `default` is the table as a JSON object, which the framework reads as it reads its config. The color shown is the one the framework reads: the keys the value has, over the default's channels. A changed value is written with the keys it had and the key of the part that changed. `"valueOptions": { "alpha": false }` is a color whose opacity means nothing, as a light's: it shows and edits no alpha, and writes `rgb` alone. A table the framework cannot read is invalid, with its reason, and a `default` that is none is an error, and it then has no Reset.

```json
{
  "type": "cf.color",
  "text": "Ray color",
  "default": { "rgb": [190, 255, 190], "alpha": 200 },
  "storage": { "path": ["Pointer", "cRayColor"] }
}
```

`cf.transform` is where a mesh, a light or a zone sits on its node, the inline table of a `t` key, `{ pos = [x, y, z], rot = [heading, roll, attitude], scale = s }`, with the rotation in degrees and each key optional. It is kept in a value of a TOML file, as the framework keeps it, and its `valueType` is `string`, the TOML text of the table. Its `default` is the table as a JSON object, which the framework reads as it reads its config. A key the value leaves out is what the framework reads for it: no move, no rotation, a scale of 1. A changed value is written with the keys it had and the key of the part that changed. A table the framework cannot read is invalid, with its reason, and a `default` that is none is an error, and it then has no Reset.

Its `valueOptions` has `position`, `rotation` and `scale`, the three parts its editor has a slider for, each `true`, `false` or the range of its sliders:

- `true`, or the part left out, is on, with the part's own range: a position -50 to 50 by 1, a rotation -180° to 180° by 1°, a scale 0.1 to 3 by 0.1.
- `false` is off: the row, the editor and Adjust in world leave the part out, and its value is kept as the file has it. A transform has at least one part on: one with none is an error, and it is not available.
- An object is on, with its `min`, `max` and `step`, each optional and the part's own where it is left out. A position's and a rotation's can also have one of an axis, `x`, `y` and `z` or `heading`, `roll` and `attitude`, an object of its own `min`, `max` and `step` over the part's.

A range whose `min` is not below its `max`, or whose `step` is not above 0, is an error, and the part has its own range. A value outside its slider's range is not invalid, as the framework reads it: it is shown as it is, and a step or a drag brings it in. A value is not rounded to its step, and an angle is not wrapped.

```json
{
  "type": "cf.transform",
  "text": "Body grab zone",
  "default": { "pos": [0.0, 0.0, 12.0], "scale": 1.0 },
  "valueOptions": {
    "position": { "min": -10, "max": 10, "step": 0.1, "z": { "min": 0, "max": 40 } },
    "rotation": false,
    "scale": { "min": 0.5, "max": 2.0 }
  },
  "storage": { "path": ["BodyActivation", "tZone"] }
}
```

`cf.activationSphere` is a section of the TOML that the framework reads as an activation sphere (`loadWandActivationConfig`), drawn as one group: its `text` is the group's heading, its `help` the heading's, and `storage.path` the section. A storage that is no section of a TOML file is an error, and the group has no rows. A file that has no such section is a warning: each key shows its default, and a Save adds it. Each key of the section is a setting of its own, of the control of its kind, with the section's `applies`, `live` where none is above it, as the framework previews a key. `keys` says, for each key by its name, how it is shown:

- `true`, or the key left out, shows it, with its default name.
- `false` hides it: it has no row, and its value is kept as the file has it. `kPrimaryBinding` cannot be hidden: a `false` for it is an error, and it is shown. Hiding `sShowIcon` hides `sIcon`, `cIconColor` and `fIconSize` too, and hiding `sShowSphere` hides the other keys of the sphere.
- An object shows it, with the fields of a setting that it gives: `text`, `help`, `default`, `valueOptions`, `applies`, `visibleWhen`, `enabledWhen` and `warnings`. Its `type`, `valueType` and `storage` are the key's and are not given. Its `valueOptions` is added to the key's own: a binding offers `suppress`, and a slider has a range. A zone has a transform's own ranges, and its scale is the zone's diameter, so a config spec gives it a range that fits its zone. Its `visibleWhen` and `enabledWhen` are joined with the conditions the key has of its own: a haptic has no row while its binding is off, and the look of an icon or a sphere set to `never` is disabled.

The keys and their default names: `kPrimaryBinding` Activate, `sEntryHaptic` Entry haptic and `sShowIcon` Show icon; under Location `tZone` Zone and `tZonePA` Zone in power armor; under Activation `sPrimaryHaptic` Primary haptic, `kSecondaryBinding` Secondary activate and `sSecondaryHaptic` Secondary haptic; under Visuals `sIcon` Icon, `cIconColor` Icon color, `fIconSize` Icon size, `sShowSphere` Show sphere, `sSphereStyle` Sphere style and `fSphereScale` Sphere size. The framework's other keys, `sSphereOrientation`, `cSphereColor`, `fSphereGlow`, `sSphereNif`, `sSphereTexture` and `aSphereFalloff`, are hidden unless `keys` names them, and are then shown under Visuals. `sShowIcon`, `sShowSphere`, `sSphereOrientation` and `sSphereStyle` are dropdowns whose value in the file is read as the framework reads it, with no regard to case, spaces, underscores and hyphens and with its other words, so `off` is Never: one it does not read is invalid. A changed value is written as its option has it. A name in `keys` that is none of these is a warning, and is left out. A setting's id is its key and the section, as any key's, so a condition can name it.

`sIcon`, `sSphereNif` and `sSphereTexture` are [choices of files](#options-from-a-folder) of the framework's, which every CommonFramework mod ships in its own folders, named by the config spec's `modName`: `sIcon` the `activation-icon-*.dds` of `Textures\<modName>\f4cf`, `sSphereTexture` the `*-sphere.dds` of it, and `sSphereNif` the `*.nif` of `Meshes\<modName>\f4cf`. A pick is written by its path in the Data folder, as `Textures\FRIK\f4cf\activation-icon-ring.dds`. `sIcon`'s default is the framework's hand icon. `sSphereNif` and `sSphereTexture` have first the option of the style's own, an empty text, which is their default. A value in another form, as the framework's short `f4cf\activation-icon-ring.dds` or the `none` of the mesh's own texture, is invalid until a file is picked. A mod with icons of its own gives `sIcon` a `listFromFolder` of its folder and mask, and a `default` there. `aSphereFalloff` is a pair of numbers, which the menu has no control for: naming it is a warning, and its row says so.

A key the file does not have is the framework's default for it, or the `default` its object gives. `tZonePA` is the exception: the framework uses `tZone` without it, so it has a row only while the file has the key, or its object gives a `default`, for a mod with a power-armor zone in its code. That is settled as the config spec is read. `keys` naming it with neither is a warning. With no `default` it has no Reset.

```json
{
  "type": "cf.activationSphere",
  "text": "Body grab",
  "help": "Where your off hand reaches to take the light from your chest.",
  "storage": { "path": ["ImFl_BodyActivationSphere"] },
  "keys": {
    "kPrimaryBinding": { "text": "Take the light", "valueOptions": { "suppress": true } },
    "kSecondaryBinding": { "text": "Switch hands" },
    "tZone": { "valueOptions": { "rotation": false } },
    "tZonePA": false,
    "sShowSphere": false
  }
}
```

A setting's `id` is optional. A setting without one gets its id from its storage, as a name and where it is, joined by a colon:

- An INI key: the key and its section, as `fPlayerHeight:MyMod`, which is how MCM names a ModSetting.
- A JSON or TOML value: the last name of its path, and the names before it joined with dots, as `fScale:Panel` for `["Panel", "fScale"]`. A path of two is named as an INI key is, so a mod that moves its config from INI to TOML keeps its ids. A path of one is its name alone.
- A global: its form, as `MyMod.esp|1234`.
- A property of a script: its name and its form, as `fStrength:MyMod.esp|800`.
- A state of the mod's client: its name, as `pipboyType`.

An author gives an id to a setting that something names, a condition or a script, when the name should stay the same after its key is renamed or moved.

Setting IDs are unique across the config spec, given or made. This makes conditions, validation issues, API events, search, and diagnostics unambiguous. Two settings whose ids come out the same are an error of the config spec, and giving one of them an id resolves it. Only a setting and a page have an id: nothing names a section or a button.

Scripts read a setting kept in a file by the config spec's `modName` and the setting's `id`, so renaming either breaks the scripts that use it; see [Papyrus API](architecture.md#papyrus-api). For a setting with no `id` of its own, that is renaming its section or its key.

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

A setting's `storage` says where its value is kept: a key of one of the config spec's files, a global, a property of a script, or a state of the mod's native client. It is apart from the setting's `id`.

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
- Any other table is not a value: the config spec names each of its keys.
- An array is one value, and a path does not name an element of it. Its control is still to be designed.

The resolved storage is included in native `SettingView` data so generic adapters do not parse this config spec themselves.

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
- It has a config spec's lifecycle, as a key of a file has: a change is pending until Save, and Discard drops it. The host previews a `live` one itself, by writing the pending value into the game, and writes the value from before back on Discard. See [Editing session](architecture.md#editing-session).
- A script reads it itself, so the [Papyrus API](architecture.md#papyrus-api) is for a setting kept in a file.
- A global or a property that is not in the game is not available, and its row says why, as an MCM row does.

A config spec has no MCM ModSetting storage: it does not need MCM, and a key of its own file, read through the Papyrus API, is what a ModSetting is for an MCM mod.

### Client state

```json
{
  "type": "dropdown",
  "valueType": "int",
  "text": "Pip-Boy",
  "default": 0,
  "applies": "live",
  "valueOptions": { "options": ["On the wrist", "Projected"] },
  "storage": { "clientState": "pipboyType" }
}
```

A value that is in the memory of the mod's DLL, and in no file, is a state of the mod's native client, by its name, as a condition and a file's `pathFromClientState` ask it: a value the mod takes from the game's settings, as which Pip-Boy the player uses, or one it measures. The client answers it, and applies a value the menu gives it; see [A client's state](architecture.md#a-clients-state).

- It has a config spec's lifecycle, as a global has: a change is pending until Save, and Discard drops it. A `live` one is applied as it is changed, and its value from before is applied again on Discard.
- The menu keeps it nowhere. A mod that keeps it after the game is restarted writes it itself, as a Save tells it what it wrote.
- A state the client gives no setter is not changed, so its setting is `readOnly`: a value the mod shows the player, as a measured height, with no file.
- A condition on the setting names it by its `id`, and has its pending value. A `clientState` condition on the same name has the client's value, which is the preview of a `live` one.
- With no client, a client that is cut off, or a state it does not answer, the setting is not available, and its row says why.
- A setting's `action` runs before the client is told what a Save wrote, and the client applies the value itself, so an `action` on it is a warning of the config spec.

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

A row can be shown or enabled by the pending values of settings: it is shown while its `visibleWhen` holds, and enabled while its `enabledWhen` holds. A row that is not enabled is drawn disabled. A section's conditions cover everything in it, so a row needs every condition above it to hold. A page supports both conditions too, covering its row in the tree, its content and its child pages:

```json
{
  "pageDisplayName": "Calibration",
  "visibleWhen": { "setting": "showCalibration", "equals": true },
  "enabledWhen": { "clientState": "ready" },
  "content": [ { "type": "text", "text": "Adjust calibration here." } ]
}
```

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

A condition can ask a state of the mod's native client, for what only the mod's code knows and changes while the game runs, as a calibration that is running:

```json
{
  "type": "button",
  "text": "Calibrate height",
  "action": { "type": "CallNativeFunction", "function": "calibrate" },
  "enabledWhen": { "clientState": "calibrating", "equals": false },
  "warnings": [{ "text": "Stand still until it is done.", "when": { "clientState": "calibrating", "equals": true } }]
}
```

- `clientState` names a state the client registered, by its name; see [A client's state](architecture.md#a-clients-state). It is a value of a type, as a setting's is, and takes the same operators. With none, it holds while the state is on, as a switch's value does. A state that is a setting's storage is asked by the setting's `id`, for its pending value; see [Client state](#client-state).
- The host asks the client as the page is drawn, so a row follows the state as it changes.
- While the client has not registered the state, or the mod has no client, the state is not available, as a value that cannot be read: the row is shown, and the warning is not told.

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

A choice is a `dropdown` or a `stepper`, as in MCM.

### Shared lists

A list of options that several choices use is written once, as in an MCM config. `sharedLists` at the config spec's root holds each list by its name, in either form of `options`, and a choice names one with `sharedOptions` in place of its own `options`:

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

### Options from a folder

A `dropdown` of `valueType` `string` can take its options from the files of a folder, as MCM's `dropdownFiles` does, for a list the player can add to: a mod's gobo textures, or its presets.

```json
{
  "type": "dropdown",
  "valueType": "string",
  "text": "Gobo on the head",
  "default": "data\\Textures\\ImmersiveFlashlightVR\\Gobos\\6_Incandescent.DDS",
  "valueOptions": { "listFromFolder": { "path": "Textures\\ImmersiveFlashlightVR\\Gobos", "mask": "*.dds" } },
  "storage": { "path": ["ImmersiveFlashlightVR", "sFlashlightOnHeadGoboPath"] }
}
```

- `root` is `data`, the default, or `myGames`, as a file's is, and `path` is the folder from it, which cannot go up out of it.
- `mask` names the files with the `*` and `?` of Windows, one pattern: every file without one. The folder alone is listed, not the folders in it, and only its loose files: a file packed in a BA2 is not.
- `noneText` adds a first option that is no file, with this text, which stores an empty text.

The folder is listed as the mod is selected, and again as its values are read again, alphabetical with no regard to case. Each file is an option: its name is its text, and its value is the `default` with the file's name in place of the default's, so a pick is written in the form the mod reads, with the default's folder and separators. A choice with no `default` writes its folder's `path` before the name.

A value names a file of the folder with no regard to case, to `/` or `\`, and to a `data\` before a path of the Data folder, and a bare name is the file of that name. Such a value is its option, and is clean: it is written again only as the player picks a file. A value of another folder, or of a file the folder does not have, is invalid. While the folder has no file the mask names, or is not there, the choice has no options, is disabled, and the help line says where it looked, unless it has a `noneText`, whose option it has alone.

The `default` has to be in the folder, a path of it or a bare name: one of another folder is an error, and the setting has no Reset. With the game's folders, the loader warns of a folder with no file its mask names, and of a default whose file the folder does not have.

A `stepper`, or a choice of another `valueType`, with a `listFromFolder` is an error, and has no options.

### Options from the mod's client

A choice can take its options from the mod's native client, for a list only the mod's code knows:

```json
"valueOptions": { "listFromClientCallback": true }
```

The client registers a callback for the setting's id with the [native API](architecture.md#options-from-a-client), and the host calls it as the mod is selected. It returns the options in either form of `options`: texts, or values with their texts, the values in the setting's `valueType`. While no callback is registered for the setting, the choice has no options, is disabled, and the help line says why. A value of the file that is none of the options is invalid, and the `default` is any value of the setting's type, as for options from the game.

Mods Config FO4 VR's own config spec uses it for its Config spec setting, whose options are the config specs the menu found: the menu registers that callback through the native API, with its own client.

## Buttons

A button has no storage, default, dirty state, or persisted value.

Its `action` is an object with a `type`, as in an MCM config:

- `CallFunction` calls a function of a script on a form, and `CallGlobalFunction` a global function of a script, as in an MCM config. So a script-only mod has buttons too.
- `RunConsoleCommand` runs a console command, as MCM runs one for a hotkey. It needs no script, so a mod with no plugin of its own has buttons too.
- `CallNativeFunction` runs a function that the mod's native client registered, by its name.
- `OpenURL` opens a link through Windows' default handler, normally the desktop browser for web links. It needs no script or native client and is only available for buttons.

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

The `params` of a `CallFunction` and a `CallGlobalFunction` are a list of JSON values, each given to the script in its own type: a number written with no point is an int, one with a point or an exponent is a float, `true` and `false` are bools, and a text is a string. A text is never read for a type: the `{i}`, `{f}` and `{b}` of an MCM config are not a config spec's. `"{value}"` is the value of a setting's action; see [A setting's action](#a-settings-action).

`OpenURL` passes its literal `url` to Windows without validating it. A launch failure is shown in the button's help line. The menu stays open in VR.

```json
{
  "type": "button",
  "text": "Documentation",
  "buttonText": "Open",
  "action": { "type": "OpenURL", "url": "https://example.com/docs" }
}
```

`buttonText` is the text on the button, in place of `Run`. It is a localized text.

`style` makes a button that matters stand out, so the player does not miss it, and `icon` draws a mark before its text:

```json
{
  "type": "button",
  "text": "Calibrate height",
  "buttonText": "Calibrate",
  "style": "important",
  "icon": "target",
  "action": { "type": "CallNativeFunction", "function": "calibrate" }
}
```

- `style` is `important` or `dangerous`. With none the button looks as every other.
  - `important` draws the button's outline, text and icon in the accent color, which no state of the menu uses. Keep it for the one or two buttons of a page a player must find, as a calibration or the mod's own UI: a page whose buttons are all important has none that stands out.
  - `dangerous` draws them in red, for an action that cannot be taken back. A press always asks first: the button's `confirmation`, red, or with none, one made from the button, its `text` as the title with a question mark, its `help` as the message and its `buttonText` or `Run` as the confirming button. A `confirmation` that is `dangerous` makes its button `dangerous` too.
- `icon` names one of the menu's drawn icons: `play`, `stop`, `bolt`, `target`, `check`, `window`, `external`, `link`, `gear`, `import`, `export`, `copy`, `eye`, `hand`, `sound`, `pin`, `refresh` and `trash`. It is drawn in the color of the button's text, before it. An `OpenURL` button with no `icon` has `link`.
- A `style` or an `icon` that is none of these is a warning, and the button is drawn without it. An MCM button has neither.

`CallNativeFunction` is the config spec's own action:

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

Its `params` are named, typed JSON values, where the `params` of MCM's actions are a list. Each is given to the function in its own type, as a `CallFunction`'s: a number written with no point is an int, one with a point or an exponent is a float, `true` and `false` are bools, and a text is a string. In a config spec without a native client, or whose client did not register the function, such a button is shown disabled.

A button that opens the mod's own UI, as FRIK's calibration, is a `CallNativeFunction` too. Its function minimizes the menu as it opens its UI, and shows the menu again as the UI closes; see [The mod's own UI](architecture.md#the-mods-own-ui).

A button can use ordinary visibility and enablement conditions, and [warnings](#warnings).

A `CallFunction` and a `CallGlobalFunction` call a script, which reads the saved values: the globals, properties and files that Save writes. So while its mod has unsaved changes, a press on such a button asks to save them first, with Cancel, Run without saving and Save and run. Save and run saves the changes and then runs the button, and is disabled while a change is invalid, as Save is. Run without saving runs it with the changes pending. Save and run stands for the button's confirmation too, with the confirmation's title and message, and is red for a dangerous one, so a button never asks twice. The button's help line says beforehand that it asks to save first. An MCM button does not ask: an MCM change is made as it is made, so the script reads it already; see [MCM compatibility](architecture.md#mcm-compatibility). A console command, a link and a function of the mod's DLL, which has the `live` values as a preview, run with the changes pending and do not ask.

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

A `text` is a paragraph in the panel's muted color, wrapped to the width of the rows.

A text row can insert the value of a native client state with `{clientState:name}`:

```json
[
  { "type": "text", "text": "Version {clientState:version}" },
  { "type": "text", "text": "<b>Version {clientState:version}</b> — {clientState:status}", "html": true }
]
```

The state belongs to the selected mod's client, registered with `addState`, as for a `clientState` condition. Text and conditions share the same cache: a state is asked once per frame, without regard to case, and its displayed value updates on the next frame. Strings are inserted as written, numbers as text, and bools as `true` or `false`. An unavailable state, including one with no native client, shows `Unknown (name)`, using the state name from the placeholder, such as `Unknown (version)`.

Placeholders are resolved after translation and markup parsing, so a value inherits the placeholder's formatting and is shown as plain text. Tags and placeholders inside a returned value are not interpreted. Line breaks in a returned string start another line. Keep each placeholder inside one formatting run, such as `<b>{clientState:version}</b>`. Ordinary braces and incomplete or empty placeholders stay as written. This syntax applies to `text` rows.

For a version supplied by a native mod, register a string state using the same version constant as the DLL:

```cpp
modsConfig.addState("version", [] { return std::optional<Value>(std::string(VERSION)); });
```

Here `VERSION` is the mod's own version constant; see [The native API](../../external/F4VR-CommonFramework/src/modsconfig/README.md) for the client setup.

With `html`, the text takes the tags that MCM configs use, including around placeholders:

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

An MCM `image` names a picture inside the mod's SWF, which a config spec does not have. A config spec's `image` names a texture file by its path under `Data/Textures`, and can give the size it is drawn at, in the panel's pixels:

```json
{ "type": "image", "texture": "MyMod/config/logo.dds", "width": 512, "height": 128 }
```

The texture is loaded by the game, as a texture of a mesh is, from a loose file or a BA2 archive, in any DDS format the game reads. With no size it is drawn at its own, and with one side alone the other is of its aspect. One wider than the rows is as wide as they are. A texture the game does not load is a box of its size that names it, and the help line has the reason while the ray is on it, as the log has it.

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

Config spec/schema errors disable only the affected content where safe and appear in diagnostics: in the log, and to the config spec's author in [Developer tools](ui-and-vr-interaction.md#developer-tools). A config spec that cannot be read at all is [not available](ui-and-vr-interaction.md#notices). Player input errors disable Save until corrected.

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

A token with a fallback keeps the English text in the config spec:

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
- A mod with no game plugin has no file loaded by F4SE. The host reads `<modName>_<language>.txt` for its config spec itself.
- The fallback is shown when no file has the token: the game is set to a language the mod has no file for, or the file misses the token. So a config spec whose fallbacks are English needs no English file.
- A token with no fallback is shown as it is written when no file has it, as in MCM, with a diagnostic.
- Every text the player sees takes these forms: the name of a mod and of a page, a section, a setting, a help, an option and its short text, the text on a button, a confirmation, and a unit.

A language can be shown only if the UI font has its characters. The font covers Western European languages today; wider coverage is an open question.

This is what an MCM config does, with the fallback added. The texts of an MCM config therefore carry over to a config spec as they are.

## A config spec and MCM

A config spec has what an MCM config has, by MCM's names, and more:

- Its settings are kept in the mod's own INI and TOML files, and in globals and properties of its scripts.
- Every setting has a `default`, for Reset, and can be read-only. A number has its `display`.
- A setting and a button can warn while a condition holds.
- `applies` says when a change takes effect.
- A condition compares any value with its operators, can enable a row as well as show it, and can ask whether a plugin is loaded.
- Sections nest and fold, and pages nest.
- A parameter has its type as a JSON value.
- A button can run a console command or a function of the mod's DLL, has its own text, and can ask for a confirmation.
- The mod and each page have a help, and the mod's DLL is told which of its pages is shown.
- A text can have a fallback, and an image is a texture file.

What an MCM config has that a config spec does not, and what the config spec has in its place:

| MCM                                                    | A config spec                                                                                          |
| ------------------------------------------------------ | -------------------------------------------------------------------------------------------------- |
| `hiddenSwitcher`, set by a script to show rows         | A condition on a value, as a dropdown's, or on a loaded plugin. A row shown by game state that changes, as a mod's busy warning, is a condition on a state of the mod's client |
| `groupControl` and `groupCondition`, groups by number  | `visibleWhen` and `enabledWhen`, which name settings by their ids                                  |
| `{i}`, `{f}` and `{b}` in the text of a parameter      | A JSON value with its type                                                                         |
| `{value}` in the text of a parameter                   | A parameter that is exactly `"{value}"`, in the setting's type                                     |
| A switcher with no storage that shows hidden rows      | A section that folds                                                                               |
| `textinputInt`, `textinputFloat`                       | A `textinput` whose `valueType` is `int` or `float`                                                |
| ModSetting storage                                     | A key of the config spec's file, which scripts read through the Papyrus API                            |
| `hotkey`, `keyinput`                                   | The [InputBinding](controls.md#commonframework-specific-controls) control, for a CommonFramework mod |
| `dropdownFiles`                                        | A dropdown with a `listFromFolder`; see [Options from a folder](#options-from-a-folder)             |
| `positioner`, `CallExternalFunction`                   | None: they are of MCM's own Flash menu                                                             |
| `ownerModName`, pages under another mod                | None: a config spec's pages are its mod's alone                                                        |
