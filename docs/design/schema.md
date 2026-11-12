# Declarative schema

The config spec JSON format. It changes as needed until mod 1.0, then only grows ([Schema version](#schema-version)). A config spec has everything an MCM config has, under MCM's names where MCM has the same thing, plus more ([A config spec and MCM](#a-config-spec-and-mcm)). Examples use a made-up CommonFramework-style mod, My Mod.

## Config spec location

```text
Data/ModsConfigFO4/Config/<ConfigSpecFolder>/spec.json
```

The folder name is organizational; `modName` is the identity. One `spec.json` per config spec, with no includes; reused option lists are [shared lists](#shared-lists).

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
  "sharedPresetLists": {},
  "content": [],
  "pages": []
}
```

### Required metadata

| Field | |
| --- | --- |
| `minSchemaVersion` | Required. Lowest schema version that reads it, like MCM's `minMcmVersion` ([Schema version](#schema-version)) |
| `modName` | Required. Stable unique identity, named as in MCM |
| `displayName` | Required. The mod's name for the player, a localized text |
| `files` | Required when settings are file-backed ([Files](#files)) |
| `content` | Required, non-empty. The mod's front page; the mod's tree row is its row |
| `pages` | Optional. The other pages, in order, each a tree row under the mod; they nest ([Pages](#pages)) |
| `help` | Optional. Shown while the ray is on the mod's tree row |
| `applies` | Optional ([When a change takes effect](#when-a-change-takes-effect)) |
| `sharedLists`, `sharedPresetLists` | Optional ([Shared lists](#shared-lists), [CommonFramework values](#commonframework-values)) |
| `specVersion` | Optional. For diagnostics, not the mod's release version |

Author, website, Nexus ID and mod version may come later.

- Two config specs with one `modName` are both [not available](ui-and-vr-interaction.md#notices); the host doesn't pick.
- A mod with both a config spec and an MCM config of the same `modName` is listed by its config spec alone.
- All ids, `modName` and those inside a config spec, compare case-insensitively (Papyrus doesn't keep string case); ids differing only in case are duplicates.
- A native client matches by `modName`. A CommonFramework mod's client registers under the mod's framework name, so that is its `modName`; a differing F4SE plugin name goes only in `f4sePluginRequirements`.

### Schema version

One number that only grows. A config spec ships with its mod and isn't updated with the menu, so new versions only add fields and control types, never change meanings, and every older config spec still reads.

- `minSchemaVersion` is required, a whole number from 1. Without it the config spec isn't loaded (reported).
- On an older host the mod is [not available](ui-and-vr-interaction.md#notices), naming the version needed.
- Before mod 1.0 the schema stays at 1 and changes freely; the rules hold from 1.0. Later additions are documented with their version.
- An unknown field is a diagnostic and the rest is read, so typos are caught and a newer optional field degrades on an older host.

### Category

`category` (optional) groups the mod in a long list ([Mod categories](ui-and-vr-interaction.md#mod-categories)).

- A plain English name, never a `$` token, which is a warning and isn't read: grouping is by the name, case-insensitive.
- Shipped names, in English: `Gameplay`, `Survival`, `Combat`, `Weapons and Armor`, `Settlements`, `Characters`, `Interface`, `Visuals`, `Audio`, `Performance`, `VR`, `Utilities`. Any other name is accepted.
- A shipped name shows as the list spells it; any other as the player typed it, else as the first mod writes it.
- An MCM config may have the same root field.

Published MCM mods get categories from a shipped table, `Data/ModsConfigFO4/Categories.json`, which also holds the name list. It has the MCM mods of the author's four mod lists and those of the Nexus mods with 100,000 downloads or more, by `modName` in any case. A Nexus mod is listed by its config's folder, which is the `modName` of most mods:

```json
{
  "categories": ["Gameplay", "Combat", "Interface", "Settlements"],
  "mods": { "SS2": "Settlements", "FallUI": "Interface" }
}
```

Precedence: the player's choice, then the mod's own `category`, then the table (by `modName`).

### Requirements

With a missing requirement the mod is [not available](ui-and-vr-interaction.md#notices): its tree row is disabled and names what's missing.

- `pluginRequirements`: game plugins (`.esp`/`.esm`/`.esl`) by file name, as in MCM.
- `f4sePluginRequirements`: F4SE plugins by the name they register, not the DLL name (FRIK's `FRIK.dll` registers as `F4VRBody`). The native save notification goes to these.

A script-only mod lists its plugin and no F4SE plugin. A part that needs another mod uses a plugin [condition](#conditions).

## Files

```json
{
  "files": {
    "main": { "format": "ini", "root": "myGames", "path": "Mods_Config/MyMod/MyMod.ini" },
    "calibration": { "format": "toml", "root": "myGames", "path": "Mods_Config/MyMod/calibration.toml" },
    "advanced": { "format": "toml", "root": "data", "path": "F4SE/Plugins/MyMod/advanced.toml" }
  }
}
```

- Paths are always explicit.
- `format` is `ini` or `toml`; the extension must match.
- `root` is `data` (the game's `Data`) or `myGames` (`Documents/My Games/Fallout4VR`, located through Windows like CommonFramework, so a redirected Documents folder is followed). The game's own INIs (`Fallout4Custom.ini`) are valid, with `applies` `restart`.
- Paths are checked as written: absolute paths and `..` are rejected; links aren't resolved.
- A missing file is created on the first Save that changes one of its settings; a config spec can't forbid it.
- `pathFromClientState` (optional) names a [client state](architecture.md#a-clients-state) whose text is the file's absolute path, for a mod that picks its file at runtime. Asked on selection and on reading values again. Used while it answers a path under `Data` or `My Games\Fallout4VR` with the right extension; otherwise (no client, cut off, no answer, a path elsewhere, logged) `root` + `path` is used. A CommonFramework client answers `configFile` with the file it loaded ([adapter](architecture.md#commonframework-adapter)). FRIK loads `FRIK_FOLVR.toml` under Fallout London VR unless `bIgnoreFalloutLondonVR` is on, so:

  ```json
  "main": { "format": "toml", "root": "myGames", "path": "Mods_Config/FRIK/FRIK.toml", "pathFromClientState": "configFile" }
  ```

- A config spec with only text and buttons, or only globals and properties, needs no files.

## Pages

A page is a tree row under the mod or another page, so the tree has as many levels as the config spec gives.

```json
{
  "pages": [
    {
      "pageDisplayName": "Body",
      "help": "Calibration, and where the body's parts are.",
      "content": [],
      "pages": [{ "pageDisplayName": "Hands", "content": [] }]
    }
  ]
}
```

- `content` is required and non-empty, also on a page with sub-pages.
- `help` shows while the ray is on its tree row.
- `id` (optional, unique among pages) is told to the client as the page is shown and left ([The client's events](architecture.md#the-clients-events)).
- Sub-pages show in the tree while the page or one of them is selected; breadcrumbs name every level (`MODS CONFIG FO4 » My Mod » Body » Hands`, [Navigation behavior](ui-and-vr-interaction.md#navigation-behavior)).
- `visibleWhen` hides the row and its children; `enabledWhen` disables it (with an explanation in the help line) and blocks opening it or its children. Page conditions also join every row's own.
- `defaultFile`, `applies`, `visibleWhen` and `enabledWhen` are inherited by sub-pages.
- A selected page that becomes hidden or disabled goes to its nearest visible, enabled ancestor, or the front page; pending changes stay. Direct page requests (including a client's) follow the same rule.
- MCM has one level of pages.

## Layout and file defaults

Pages don't belong to files: a page may edit several files and a file may span pages. A setting's file is the first of:

1. `storage.file`
2. The nearest enclosing section's `defaultFile`
3. The nearest page's `defaultFile`, then its ancestors'
4. The config spec's `defaultFile`

```json
{
  "defaultFile": "main",
  "pages": [
    {
      "pageDisplayName": "Body",
      "content": [{ "type": "section", "text": "Calibration", "defaultFile": "calibration", "content": [] }]
    }
  ]
}
```

## When a change takes effect

Only the author knows when the mod acts on a saved change, so the config spec says it with `applies`:

| Value | Takes effect | The host |
| --- | --- | --- |
| `unknown` | Not known (default) | No preview |
| `live` | As made: the mod handles the change it's told | Previews to the client, or writes a global/property into the game |
| `save` | After Save | No preview |
| `restart` | After a game restart | No preview; a Save that wrote it says a restart is needed |

While a change is pending, the help line shows its `applies` mark (none for `unknown`); the Help page explains each. `unknown` and `save` differ only in that display.

A setting takes the first of: its own, the nearest section's, the nearest page's then its ancestors', the config spec's, else `unknown` (front page rows skip the page step). `unknown` can be written to override a level above.

```json
{
  "applies": "live",
  "pages": [
    {
      "pageDisplayName": "Advanced",
      "content": [{ "type": "section", "text": "Game hooks", "applies": "restart", "content": [] }]
    }
  ]
}
```

Choosing:

- CommonFramework mod: `live` (the client previews by reloading config values); mark settings read only once `restart`.
- Rereads its file after Save (watcher, save notification, script save event): `save`.
- Reads its file only at startup: `restart`.
- A global or property the mod reads as it plays (as game conditions read a global): `live`; the host previews it.
- Unsure: leave `unknown`.

A `live` file setting of a mod without a client isn't previewed (nowhere to send it) but still shows `live`. The host doesn't verify `applies`: a `live` setting the mod ignores looks broken.

## Ordering

Arrays order the display: `pages`, `content`, `options`. Object key order never matters (`files` is unordered). There is no numeric `order`; placing future merged contributions is a separate design.

## Content items

Every `content` entry has a `type` (MCM's names where MCM has the row):

- `switcher`, `slider`, `stepper`, `dropdown`, `textinput`: a setting bound to a file, global, script property or client state; the type is its control.
- `cf.inputBinding`, `cf.haptic`, `cf.color`, `cf.transform`: CommonFramework settings; `cf.activationSphere`: a TOML section drawn as one group ([CommonFramework values](#commonframework-values)).
- `section`: a heading with its own rows ([Sections](#sections)).
- `button`: runs an action ([Buttons](#buttons)).
- `text`, `spacer`, `image`: no value ([Text, spacers and images](#text-spacers-and-images)).

Sections and pages both nest.

## Settings

`type` is the control and `valueType` the stored type (bool, int, float, string); storage and `applies` are separate concepts.

```json
{
  "id": "playerHeight",
  "type": "slider",
  "valueType": "float",
  "text": "Player Height",
  "help": "Adjusts the estimated player height.",
  "default": 1.0,
  "valueOptions": { "min": 0.5, "max": 1.5, "step": 0.01 },
  "display": { "decimals": 2, "unit": "m" },
  "storage": { "section": "MyMod", "key": "fPlayerHeight" }
}
```

- Every row (setting, section, button, choice option) has `text` (what it says) and optional `help` (help line), MCM's names.
- `"html": true` styles a setting's or button's `text` with the [text tags](#text-spacers-and-images), kept to one line: breaks become spaces, alignment is ignored, font sizes cap at the row size. Search and the help line use the untagged label. Only the boolean `true` enables it. `help`, `buttonText`, section headings and options stay plain.
- Every writable setting has a required inline `default`, validated like a player value. It is what Reset sets, so it must match the mod's own default.
- A `textinput` with `valueType` `int`/`float` takes a number (MCM's `textinputInt`/`textinputFloat`), limited by `min`/`max`.

### CommonFramework values

A `cf.` type edits a value in CommonFramework's TOML form ([CommonFramework-specific controls](controls.md#commonframework-specific-controls)); its words and checks are the framework's. `cf.inputBinding`, `cf.color` and `cf.transform` share these rules:

- Kept in a TOML value as the framework keeps it, or an INI line ([In an INI file](#in-an-ini-file)). `valueType` is `string`: the table's TOML text or the line.
- `default` is the table as a JSON object, read as the framework reads its config.
- A value the framework can't read is invalid with its reason. An invalid `default` is an error, and the setting has no Reset.
- A changed table is written with the keys it had plus the key of the changed part.

**`cf.inputBinding`**: a `k` key's binding table.

- `"valueOptions": { "suppress": true }` offers Suppress from the game, for a binding whose consumer applies it (an activation sphere's).
- `hands`, `types`, `buttons` restrict it to those framework words (Back: no hold, no trigger). The editor offers only those, and others are invalid with what it takes. A push's button is its axis's; an off binding is always taken. An omitted list takes all; an unknown word is dropped with a warning. A `default` it doesn't take is an error.

```json
{
  "type": "cf.inputBinding",
  "text": "Back",
  "default": { "hand": "any", "type": "tap", "button": "grip" },
  "valueOptions": { "types": ["tap", "press", "release", "longpress", "double"], "buttons": ["grip", "a", "b", "thumbstick"] },
  "storage": { "path": ["ModsConfigFO4VR", "kBackBinding"] }
}
```

**`cf.haptic`**: an `s` key naming a framework pattern, or `"none"`. `valueType` `string`; the options are None plus the framework's patterns, so no `valueOptions`. Read as the framework reads it, ignoring case, spaces, underscores and hyphens (`"double click"` is DoubleClick; `"off"` and empty are none); written as the library names it. A `default` naming no pattern is an error (no Reset); such a name in the file is invalid.

```json
{ "type": "cf.haptic", "text": "Entry haptic", "default": "Tick", "storage": { "path": ["HeadActivation", "sEntryHaptic"] } }
```

**`cf.color`**: a `c` key, `{ rgb = [r, g, b], alpha = a }`, whole numbers 0..255, each key optional. The shown color is the framework's reading: the value's keys over the default's channels. `"alpha": false` (e.g. a light) shows and edits no alpha and writes `rgb` alone.

```json
{ "type": "cf.color", "text": "Ray color", "default": { "rgb": [190, 255, 190], "alpha": 200 }, "storage": { "path": ["Pointer", "cRayColor"] } }
```

- The editor always has five basics (white, black, red, green, blue), keeping the color's alpha.
- `presets` in `valueOptions` adds a line of colors that suit it, each written like `default` with an optional `text` for the help line. A preset's `alpha` applies only to a color with alpha; without one the color keeps its alpha. At most 12; extras, non-colors and presets without `rgb` are dropped and reported.
- `sharedPresetLists` at the root holds named preset lists; a color uses one with `"sharedPresets": "lights"`. Own `presets` win over it. An unknown name is a warning (basics only).

```json
"valueOptions": {
  "alpha": false,
  "presets": [
    { "rgb": [255, 180, 107], "text": "Warm white" },
    { "rgb": [255, 228, 206], "text": "Cool white" }
  ]
}
```

**`cf.transform`**: a `t` key placing a mesh, light or zone on its node, `{ pos = [x, y, z], rot = [heading, roll, attitude], scale = s }`, rotation in degrees, each key optional. An omitted key reads as the framework's (no move, no rotation, scale 1). `valueOptions` has `position`, `rotation`, `scale`, each:

- `true` or omitted: on, with the part's range (position -50..50 by 1, rotation -180°..180° by 1°, scale 0.1..3 by 0.1).
- `false`: off. The row, editor and Adjust in world leave it out, and the file's value is kept. At least one part must be on, else an error and not available.
- An object: on, with optional `min`, `max`, `step` over the part's. Position and rotation may also give one axis (`x`/`y`/`z`, `heading`/`roll`/`attitude`) its own object.

A range with `min` ≥ `max` or `step` ≤ 0 is an error and uses the part's range. A value outside its slider range is valid (the framework reads it): shown as is until a step or drag brings it in. Values aren't rounded to the step; angles aren't wrapped.

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

**`cf.activationSphere`**: a TOML section the framework reads as an activation sphere (`loadWandActivationConfig`), drawn as one group. `text` and `help` are the heading's, `storage.path` is the section, and `id` is what the client is told while the ray is on its rows (its section without one; [Pointed items](#pointed-items)). Storage that isn't a TOML section is an error (no rows). A missing section is a warning: keys show defaults and a Save adds them. Each key is an ordinary setting of its kind with the section's `applies` (`live` if none above).

`keys` sets, per key name, how it shows:

- `true` or omitted: shown with its default name.
- `false`: no row; the file's value is kept. `kPrimaryBinding` can't be hidden (an error; it shows). Hiding `sShowIcon` also hides `sIcon`, `cIconColor`, `fIconSize`; hiding `sShowBindingIcon` hides `sBindingIcons`, `tBindingIcon`, `cBindingIconColor`; hiding `sShowSphere` hides the other sphere keys.
- An object: shown, with setting fields `text`, `help`, `default`, `valueOptions`, `applies`, `visibleWhen`, `enabledWhen`, `warnings` (`type`, `valueType`, `storage` are the key's). `valueOptions` adds to the key's own (a binding's `suppress`, a slider's range). A zone has a transform's ranges and its scale is the zone's diameter, so give it a fitting range. Conditions join the key's built-in ones: a haptic has no row while its binding is off, and icon or sphere looks are disabled when set to `never`.

| Group | Keys (default name) |
| --- | --- |
| (top) | `kPrimaryBinding` Activate, `sEntryHaptic` Entry haptic, `icons` Icons |
| Position and size | `tZone` Zone, `tZonePA` Zone in power armor |
| Activation | `sPrimaryHaptic` Primary haptic, `kSecondaryBinding` Secondary activate, `sSecondaryHaptic` Secondary haptic |
| Visuals | `sShowIcon` Show icon, `sIcon` Icon, `cIconColor` Icon color, `fIconSize` Icon size, `sShowBindingIcon` Show binding icon, `sBindingIcons` Binding icons, `tBindingIcon` Binding icon position and size, `cBindingIconColor` Binding icon color, `sShowSphere` Show sphere, `sSphereStyle` Sphere style |
| Visuals, only when named in `keys` | `fSphereScale` Sphere size, `sSphereOrientation`, `cSphereColor`, `fSphereGlow`, `sSphereNif`, `sSphereTexture`, `aSphereFalloff` |

- The binding icon is the icons of the gesture's bindings, in a row facing the player. `tBindingIcon` has no rotation; its scale is each icon's size.
- **Icons** (`icons`, no key of the file) is a [choice that sets other settings](#options-that-set-other-settings), the player's pick of which icons show: None, Icon, Binding (the binding icon in the icon's place: `tBindingIcon` `{ pos = [0, 0, 0], scale = 1.25 }`) or Icon and binding (both `wheninside`, the binding icon at the icon's lower right: `{ pos = [0.65, -0.65, 0], scale = 1 }`). Each Show key turns off with `never` and on with its default's word, or `wheninside` for a default of `never`; a file whose Show keys say another word shows it blank. The Show keys are its `sets` and the binding icon's place its `alsoSets`, so moving or sizing the binding icon keeps the option. Those places are for an icon of size 1 and are multiplied by `fIconSize`'s default. The option of the Show keys' defaults sets `tBindingIcon`'s default instead, so a mod that gives its own defaults in `keys` has them as an option, and as Icons' default. `"icons": false` hides it, as does hiding `sShowIcon` or `sShowBindingIcon`, and Show icon is then a top row. An object gives it `text`, `help`, `visibleWhen`, `enabledWhen`, and `valueOptions` whose `options` replace the four, naming the keys by their ids (`sShowIcon:BodySphere`), as a mod with a bigger icon moves the binding icon further right. With `tBindingIcon` hidden no option sets it.
- `sShowIcon`, `sShowSphere`, `sShowBindingIcon`, `sBindingIcons`, `sSphereOrientation`, `sSphereStyle` are dropdowns read as the framework reads them (ignoring case, spaces, underscores, hyphens, with its synonyms: `off` is Never, `both` is All); unreadable values are invalid; changes are written as the option spells it.
- An unknown name in `keys` is a warning and dropped. Key ids are key + section, so conditions can name them; Icons is `icons` + section.
- `sIcon`, `sSphereNif`, `sSphereTexture` are [folder choices](#options-from-a-folder) of the framework files every CommonFramework mod ships under its `modName`: `sIcon` from `Textures\<modName>\f4cf\activation-icon-*.dds`, `sSphereTexture` from `*-sphere.dds` there, `sSphereNif` from `Meshes\<modName>\f4cf\*.nif`. Picks are written as Data paths (`Textures\FRIK\f4cf\activation-icon-ring.dds`). `sIcon` defaults to the framework's hand icon. `sSphereNif` and `sSphereTexture` start with an empty "style's own" option, their default. Other forms (the framework's short `f4cf\…`, the mesh's `none`) are invalid until a file is picked. A mod with its own icons gives `sIcon` a `listFromFolder` and a `default` there.
- `aSphereFalloff` (a number pair) has no control: naming it is a warning and its row says so.
- A missing key uses the framework's default or the object's `default`. Exception: `tZonePA` (the framework falls back to `tZone`) has a row only if the file has it or its object gives a `default`, settled at load; naming it with neither is a warning; without a `default` it has no Reset.

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

**Setting ids.** `id` is optional; without it the id comes from the storage, `name:where`:

| Storage | Made id |
| --- | --- |
| INI key | `key:section` (`fPlayerHeight:MyMod`, MCM's ModSetting naming) |
| JSON/TOML path | last name, then the earlier names joined by dots (`fScale:Panel`); a two-name path matches the INI form, so moving INI→TOML keeps ids; a one-name path is the name |
| Global | its form (`MyMod.esp|1234`) |
| Script property | `property:form` (`fStrength:MyMod.esp|800`) |
| Client state | its name (`pipboyType`) |

- Give an id to a setting that a condition or script names, so renaming or moving its key doesn't break it.
- Ids, given or made, are unique in the config spec; a clash is an error, fixed by giving one an id. Sections, spheres and buttons may have ids too, told only to the client ([Pointed items](#pointed-items)); conditions and scripts name settings only.
- Scripts read a file setting by `modName` + `id` ([Papyrus API](architecture.md#papyrus-api)); renaming either (or, without an own id, its section or key) breaks them.

#### In an INI file

A binding, color or transform can live in an INI file, for a mod without TOML, as a line in CommonFramework's old INI text form, which the mod parses itself:

- **Color:** `r,g,b` or `r,g,b,a`, 0..255 (`255,180,107,200`). No `a` takes the default's alpha. Written with `a` if it had one or alpha changed; `alpha: false` writes `r,g,b`.
- **Transform:** all seven numbers `x,y,z;heading,roll,attitude;scale`, degrees (`0.0,0.0,12.0;0.0,-5.0,0.0;1.0`). Fewer is invalid. Always written with seven: an off part keeps the file's numbers; a new key uses the default's (omitted default keys as the framework reads them).
- **Binding:** `<hand> <type> <button> [duration]`, or `<hand> axis <axis> <direction> [threshold]` for a stick or trigger push, then optional `suppress` and modifier `+button` / `+hand:button` (`primary longpress thumbstick 1.5 +offhand:grip`); off is `none`. Written words:
  - hand: `primary`, `offhand`, `right`, `left`, `any`
  - type: `press`, `touch`, `hold`, `release`, `tap`, `longpress`, `double`, `axis`
  - button: `trigger`, `grip`, `a`, `menu` (B), `thumbstick`, `system`
  - axis: `thumbstick`, `trigger`, `grip`
  - direction: `up`, `down`, `left`, `right`, `any`, `updown`, `rightleft`

  The framework's other names are read too (`off` for `offhand`); an empty line is off. A line has no `accelerate`, so a `default` that turns it off is an error (no Reset).

Spaces around numbers are read; none are written. Anything else, or an empty color or transform, is invalid with the expected form. The `default` stays the JSON table, so moving between INI and TOML changes only `storage`. The client is told the line and hands the line back ([Native API](../for-mod-authors/native-api.md)).

### Read-only

`"readOnly": true` shows a value the mod writes for the player (a measured height): disabled, the help line says read-only first, no Reset, so no `default`. With no stored value it shows none.

### Display

`display` formats a number in its control and the help line; the stored value is unchanged.

- `decimals`: default as many as `step` has.
- `unit`: a localized text after the number (`m`, `%`).
- `percent: true`: a percentage of the range, 0% at `min` to 100% at `max` (217 of 0..255 shows 85%).

### Warnings

A setting or button can warn while a condition holds, without blocking anything (a value with a cost; a button needing a save reload).

```json
{
  "id": "playerHeight",
  "warnings": [
    { "when": { "setting": "playerHeight", "greaterThan": 1.4 }, "text": "Above 1.4 m your view can be in the ceiling." }
  ]
}
```

- `when` is a [condition](#conditions) on pending values; `text` is localized.
- While one holds the row has the warning mark and the help line shows the first holding warning before the help.
- Warnings never block Save; invalid values have their own mark and do.

### A setting's action

`action`, written as a button's ([Buttons](#buttons)), runs when Save writes the setting, after every file and value is written, so it sees the saved value. It is how a script mod applies a change, like an MCM row's action. A parameter exactly `"{value}"` is the written value in the setting's `valueType`.

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

Unwritten settings (unchanged, or changed back), previews and Discards run nothing.

## Storage

`storage` is where the value is kept, separate from `id`: a file key, a global, a script property, or a client state.

### INI

```json
{ "storage": { "file": "main", "section": "MyMod", "key": "fPlayerHeight" } }
```

A key before the first section has no `section`. A `path` (`["MyMod", "fPlayerHeight"]`) works too; a path longer than two names no INI key. INI conventions: [Persistence model](architecture.md#persistence-model).

### JSON and TOML

```json
{ "storage": { "file": "calibration", "path": ["body", "playerHeight"] } }
```

- An array path of any depth, with no dotted-key escaping. CommonFramework TOML is `[Section] key`, a two-name path.
- A CommonFramework table (color, binding, …) is one value in one key, edited whole by its control. Any other table isn't a value: name each key.
- An array is one value and paths don't index into it; its control isn't designed yet.
- The resolved storage is in the client's `SettingView`, so adapters don't parse the config spec.

### Global and script property

```json
{ "storage": { "global": "MyMod.esp|1234" } }
```

```json
{ "storage": { "form": "MyMod.esp|800", "scriptName": "MyMod:ConfigScript", "propertyName": "fStrength" } }
```

Forms are named as in MCM, plugin and id. Without `scriptName` the property is on the form's first script.

- For mods without a DLL: values live where scripts and the game read them; game conditions read globals, so even a script-less plugin is configurable.
- The value is in the save game: a new game starts with the plugin's value. `default` is still required for Reset.
- Config spec lifecycle: pending until Save, dropped by Discard; a `live` one is previewed by the host writing it into the game and restored on Discard ([Editing session](architecture.md#editing-session)).
- Scripts read it themselves; the [Papyrus API](architecture.md#papyrus-api) is for file settings.
- A missing global or property is not available, with the reason.

There is no ModSetting storage: config specs don't need MCM, and a file key read through the Papyrus API plays that role.

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

A value held only in the DLL's memory (taken from the game's settings, or measured), named as a condition or `pathFromClientState` names a state. The client answers it and applies what the menu gives it ([A client's state](architecture.md#a-clients-state)).

- Config spec lifecycle, like a global: a `live` one is applied as changed and its old value reapplied on Discard.
- The menu stores it nowhere; a mod persists it itself from the Save it's told.
- A state with no setter can't change, so mark it `readOnly`.
- A condition on the setting's `id` sees the pending value; a `clientState` condition on the state's name sees the client's value (the preview, for `live`).
- No client, a cut-off client, or no answer: not available, with the reason.
- An `action` on it is a warning: actions run before the client is told the Save, and the client applies the value itself.

## Sections

A heading with rows in its own `content`; sections nest.

```json
{
  "type": "section",
  "text": "Holsters",
  "content": [
    { "type": "switcher", "valueType": "bool", "text": "Enable holsters", "default": true, "storage": { "section": "Holsters", "key": "bEnabled" } },
    { "type": "section", "text": "Advanced", "isCollapsible": true, "collapsed": true, "content": [] }
  ]
}
```

- `isCollapsible` makes it fold on a heading press; `collapsed` starts it folded. `collapsed` without `isCollapsible` is ignored (it couldn't open) and logged.
- It starts as `collapsed` says each time its page is shown; the player's fold isn't remembered.
- Folded, its heading counts invalid and unsaved settings inside whose rows would show, including nested sections.
- Detailed settings are a folded section right after what they belong to; there's no separate flag.
- `defaultFile`, `applies` and conditions are inherited by its rows and nested sections.
- `text`, optional `help`, and optional `id` (told to the client, [Pointed items](#pointed-items)).

An MCM section is a heading row owning the rows up to the next section, unnested; the MCM loader reads it as MCM does.

## Pointed items

The client is told what the ray is on, to show it in the world ([The client's events](architecture.md#the-clients-events)): the row's id, then each enclosing group's, innermost first.

- A setting: its id, given or made.
- A section or button: its `id`; without one it isn't told (its rows still are).
- An activation sphere: its `id`, or its section path joined with dots (`ImFl_BodyActivationSphere`). Its keys are settings, told as `tZone:ImFl_BodyActivationSphere` then `ImFl_BodyActivationSphere`. The framework lights spheres by section, so a CommonFramework sphere needs no `id`.

```json
{ "type": "section", "id": "holsterZones", "text": "Holster zones", "content": [] }
```

These ids share one case-insensitive namespace with setting ids. A section, sphere or button id that a setting, or an earlier section, sphere or button, already has is an error: dropped (that item isn't told; the setting keeps its id). Conditions, scripts and the API name settings only. Page ids are separate.

## Conditions

`visibleWhen` shows and `enabledWhen` enables a row (disabled rows are drawn disabled), evaluated on pending values so the UI reacts at once. A section's conditions cover everything inside, so a row needs every condition above it. Pages take both too, covering their tree row, content and sub-pages.

```json
{
  "pageDisplayName": "Calibration",
  "visibleWhen": { "setting": "showCalibration", "equals": true },
  "enabledWhen": { "clientState": "ready" },
  "content": [{ "type": "text", "text": "Adjust calibration here." }]
}
```

```json
{
  "visibleWhen": {
    "all": [
      { "setting": "enableHolsters", "equals": true },
      { "setting": "interactionMode", "oneOf": ["physical", "hybrid"] }
    ]
  }
}
```

Values are bool, int, float, string, with no implicit conversion. Operators:

- All types: `equals`, `notEquals`
- Numbers: `lessThan`, `lessThanOrEqual`, `greaterThan`, `greaterThanOrEqual`
- Strings and choices: `oneOf`, `notOneOf`
- Composition: `all`, `any`, `not`

Hiding or disabling a setting doesn't reset it or drop its pending edit. A condition naming a missing setting or comparing incompatible types is invalid as a whole: a developer diagnostic, and the item shows and enables as if unconditioned, so the mistake is visible.

**Plugins**, for rows that need another mod:

```json
{ "type": "text", "text": "Camera effects are set by the ENB patch.", "visibleWhen": { "plugin": "MyMod - ENB Patch.esp" } }
```

- `plugin`: a game plugin file name, holds while loaded. `f4sePlugin`: an F4SE plugin's registered name.
- Answered once at game start; they compose (`{ "not": { "plugin": "..." } }`).

**Client states**, for what only the mod's code knows and changes at runtime:

```json
{
  "type": "button",
  "text": "Calibrate height",
  "action": { "type": "CallNativeFunction", "function": "calibrate" },
  "enabledWhen": { "clientState": "calibrating", "equals": false },
  "warnings": [{ "text": "Stand still until it is done.", "when": { "clientState": "calibrating", "equals": true } }]
}
```

- `clientState` names a registered state ([A client's state](architecture.md#a-clients-state)), typed like a setting, with the same operators; with no operator it holds while on. A state that is a setting's storage is named by the setting's `id` instead, for its pending value ([Client state](#client-state)).
- Asked as the page draws, so rows follow it.
- Unregistered, or no client: not available, so the row shows and the warning isn't told.

**Game INI settings**, which the game's settings menu changes at runtime, with no DLL:

```json
{
  "type": "text",
  "text": "The game's shadows are below High, which may break the beam's shadows.",
  "visibleWhen": { "gameIniSetting": "iShadowMapResolution:Display", "lessThan": 2048 }
}
```

- `name:Section`, any case, looked up in `Fallout4Prefs.ini` then `Fallout4.ini`/`Fallout4Custom.ini`, with the value the game runs with now, not the file's.
- Typed by its first letter: `b` bool, `i`/`u` int, `f` float, `s` string, with that type's operators plus `oneOf`/`notOneOf` for string and int; no operator holds while on. Another letter (a color's `r`) or a mismatched operator or value is not a condition.
- Asked as the page draws, so rows follow the settings menu.
- A setting the game lacks is not available (row shows, no warning), logged once.
- GMSTs aren't supported: plugins set them, not players.

## Choices

A choice (`dropdown` or `stepper`) stores a value, never the displayed text; options are in `valueOptions` as in MCM. The short form, a list of texts, stores the index:

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

Otherwise each option gives its value:

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

Option values share the setting's `valueType`; conditions compare values, not labels. An option's optional `help` shows while the ray is on it in an open list, else the row's help.

### Shared lists

`sharedLists` at the root holds named lists (either form); a choice uses one with `sharedOptions` instead of `options`, as in MCM.

```json
{
  "sharedLists": { "hands": ["Left", "Right"] },
  "content": [
    { "type": "stepper", "valueType": "int", "text": "Flashlight hand", "default": 0, "valueOptions": { "sharedOptions": "hands" }, "storage": { "section": "Flashlight", "key": "iHand" } }
  ]
}
```

### Options from the game

For lists the author can't know (the player's settlements, weathers other mods add), with MCM's fields:

- `listFromForm`: a FormList (`"MyMod.esp|900"`); options are its forms' names, or editor IDs for unnamed forms.
- `listFromProperty`: a script's string-array property, `{ "form": "MyMod.esp|800", "scriptName": "MyMod:ConfigScript", "propertyName": "SettlementNames" }` (`scriptName` optional: the form's first script).

Read on selection. `int` stores the option's index (as MCM; a reordered list moves it), `string` stores its text; other types get no options. An unreadable list leaves the choice with no options, disabled, with the reason. A file value not among the options is invalid. `default` is any value of the type, since options aren't known in advance.

### Options from a folder

A `string` `dropdown` can list a folder's files, like MCM's `dropdownFiles`, for lists players extend (gobo textures, presets).

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

- `root`: `data` (default) or `myGames`; `path` can't leave it.
- `mask`: one Windows pattern (`*`, `?`); none lists every file. Only the folder's own loose files, not subfolders or BA2 contents.
- `noneText`: a first option for no file, storing an empty text.

- Listed on selection and on reading values again, alphabetically, case-insensitive. An option's text is the file name; its value is the `default` with the file name swapped in, so picks keep the default's folder and separators. With no `default`, the folder's `path` prefixes the name.
- A value matches a file ignoring case, `/` vs `\`, and a leading `data\`; a bare name matches too. A match is its option and clean (rewritten only on a new pick). Another folder or a missing file is invalid.
- An empty or missing folder leaves no options, disabled, and the help line says where it looked; with `noneText` only that option.
- The `default` must be in the folder (path or bare name); otherwise an error and no Reset. With the game's folders, the loader warns of an empty folder and of a default file that's missing.
- `listFromFolder` on a `stepper` or a non-string choice is an error (no options).

### Options from the mod's client

```json
"valueOptions": { "listFromClientCallback": true }
```

The client answers through the [native API](architecture.md#options-from-a-client) on selection, in either option form with values in the setting's `valueType`. Without an answer the choice has no options, disabled, with the reason. A file value not among them is invalid; `default` is any value of the type. The menu's own config spec uses it for its Config spec setting (the config specs it found).

### Options that set other settings

A `stepper` or `dropdown` whose written options (its own or a shared list) have `sets` or `alsoSets` is a choice of presets: picking an option sets other settings, and the choice keeps no value of its own.

```json
{
  "type": "stepper",
  "valueType": "string",
  "id": "quality",
  "text": "Quality",
  "valueOptions": {
    "options": [
      { "value": "low", "text": "Low", "sets": { "shadows": false }, "alsoSets": { "drawDistance": 2000 } },
      { "value": "high", "text": "High", "sets": { "shadows": true, "glowColor": { "alpha": 255 } }, "alsoSets": { "drawDistance": 8000 } }
    ]
  }
}
```

- `sets` and `alsoSets` name settings by id, as a [condition](#conditions) does, of any storage, on any page, hidden or not. Each value is checked as that setting's `default` is.
- A color's or a transform's value is a table that sets only the parts it names (`{ "alpha": 255 }` keeps the color). A binding is a whole binding.
- **Its value** is the first option whose `sets` all match their settings' values, numbers compared as a condition's `equals` does, and a table on the parts it names. `alsoSets` are not compared: they are what a player tunes after the pick, as a place or a size, and a tuned one keeps the option. While none matches, the row shows no option, and its help line says "None of its options: their settings are set otherwise." From there a stepper's left arrow picks the last option and its right arrow the first.
- **A pick** sets each setting of its `sets` and `alsoSets` as a change of its own: pending, previewed and saved as any. A setting that is read-only or not available is skipped. A mod's DLL that hands back a value for the choice's `id` picks as the player does.
- It is pending while one of the settings of its `sets` or `alsoSets` is, and its Discard takes them all back. Its default, which Reset picks, is the option matching their defaults; with none it has no Reset. It has no `applies` (each setting takes effect as its own says) and never counts as a change: its settings count on their own rows.
- No `storage`, `default` or `applies`: a `storage` with `sets` is an error and the `sets` are ignored; a `default` or `applies` is a warning and left out. `sets` on any other type is an error.
- An option with no `sets` is a warning: it is never the value, and a pick sets only its `alsoSets`. An id of no setting, of another choice with `sets`, or a value that isn't one of its setting's, is an error, and that entry is left out.
- It can have an `id`, so conditions can name it; its value is the matching option's, or none.

## Buttons

No storage, default, dirty state or value. An optional `id` is told to the client ([Pointed items](#pointed-items)). `action.type`:

- `CallFunction` (a script function on a form) and `CallGlobalFunction`, as in MCM, so script-only mods have buttons.
- `RunConsoleCommand`, as MCM runs one for a hotkey; needs no script.
- `CallNativeFunction`: a function of the mod's client, by name.
- `OpenURL`: opens the literal `url` through Windows' default handler (the browser for web links), unvalidated; buttons only. A launch failure shows in the help line; the menu stays open in VR.

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
{ "type": "button", "text": "Hide the HUD", "action": { "type": "RunConsoleCommand", "command": "tm" } }
```

```json
{ "type": "button", "text": "Documentation", "buttonText": "Open", "action": { "type": "OpenURL", "url": "https://example.com/docs" } }
```

**Parameters.** `CallFunction`/`CallGlobalFunction` `params` is a list of JSON values, each passed in its JSON type: a number without a point is an int, with a point or exponent a float, `true`/`false` bools, text a string. Text is never parsed for a type (no MCM `{i}`, `{f}`, `{b}`). `"{value}"` is a setting action's value ([A setting's action](#a-settings-action)). `CallNativeFunction` `params` is an object of named values, typed the same way:

```json
{
  "type": "button",
  "text": "Calibrate Height",
  "help": "Measures the current player height and updates calibration.",
  "buttonText": "Calibrate",
  "action": { "type": "CallNativeFunction", "function": "calibrate", "params": { "mode": "height", "includeScale": true, "sampleCount": 30 } }
}
```

Without a client, or a client lacking the function, a native button is disabled. A button opening the mod's own UI (FRIK's calibration) is a native function that minimizes and restores the menu ([The mod's own UI](architecture.md#the-mods-own-ui)).

**Look.** `buttonText` (localized) replaces `Run`. `style` and `icon`:

```json
{ "type": "button", "text": "Calibrate height", "buttonText": "Calibrate", "style": "important", "icon": "target", "action": { "type": "CallNativeFunction", "function": "calibrate" } }
```

- `style` `important`: outline, text and icon in the accent color, which no menu state uses. Keep it for the one or two buttons a player must find; if everything is important nothing stands out.
- `style` `dangerous`: red, for irreversible actions. Always confirms: its `confirmation` in red, or one made from the button (`text` + `?` as title, `help` as message, `buttonText` or `Run` as the confirm button). A `dangerous` confirmation makes its button dangerous.
- `icon`, drawn in the text color before it: `play`, `stop`, `bolt`, `target`, `check`, `window`, `external`, `link`, `gear`, `import`, `export`, `copy`, `eye`, `hand`, `sound`, `pin`, `refresh`, `trash`. `OpenURL` defaults to `link`.
- An unknown `style` or `icon` is a warning and ignored. MCM buttons have neither.

Buttons take `visibleWhen`, `enabledWhen` and [warnings](#warnings).

**Save first.** Script calls (`CallFunction`/`CallGlobalFunction`) read saved values, so while the mod has unsaved changes a press asks: Cancel, Run without saving (runs with changes pending), Save and run (saves then runs; disabled while a change is invalid). Save and run also stands in for the button's confirmation, with its title and message, red if dangerous, so a button never asks twice. The help line says beforehand that it will ask. MCM buttons don't ask (MCM changes are already made, [MCM compatibility](architecture.md#mcm-compatibility)); console commands, links and native functions (which see `live` previews) run with changes pending.

**Confirmation:**

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

Rows without a value. `text` and `spacer` are as in MCM.

**`text`** is a paragraph in the muted color, wrapped to the rows' width.

**Placeholders.** `{clientState:name}` inserts a state of the selected mod's client (registered with `addState`):

```json
[
  { "type": "text", "text": "Version {clientState:version}" },
  { "type": "text", "text": "<b>Version {clientState:version}</b> — {clientState:status}", "html": true }
]
```

- Shares the conditions' cache: asked once per frame, case-insensitive, updating next frame. Strings as written, numbers as text, bools `true`/`false`. Unavailable (including no client) shows `Unknown (name)`.
- Resolved after translation and markup, so the value takes the placeholder's formatting as plain text; tags and placeholders inside it aren't interpreted. Line breaks start a new line. Keep a placeholder inside one formatting run (`<b>{clientState:version}</b>`). Plain braces and incomplete or empty placeholders stay as written.
- Settings', buttons' and sections' `text` and `help` take placeholders too, on one line (a line break becomes a space).
- `{gameIniSetting:name}` inserts a game INI setting by its [condition](#conditions) name, keeping `:Section` (`{gameIniSetting:iShadowMapResolution:Display}`), with no DLL; missing shows `Unknown (name)`.
- A native mod's version: register a string state with the DLL's own constant, `modsConfig.addState("version", [] { return std::optional<Value>(std::string(VERSION)); });` (client setup: the framework's [modsconfig README](../../external/F4VR-CommonFramework/src/modsconfig/README.md)).

**Tags.** With `"html": true`, MCM's tags, also around placeholders:

```json
{ "type": "text", "text": "<p align='center'><font size='24'>Calibration</font></p>Stand straight and look ahead.<br><font color='#FFFFFF'>Do not move</font> until it is done.", "html": true }
```

| Tag | Effect |
| --- | --- |
| `<br>` | New line |
| `<p>` | Paragraph on its own lines; `align` is `left`, `center` or `right` |
| `<font size>` | MCM menu scale: 20 is the rows' text size |
| `<font color>` | `#RRGGBB` |
| `<b>`, `<i>` | Bold, italic; both is bold (no bold-italic face) |
| `<u>` | Underlined, including spaces between words |

Sizes follow MCM so copied text looks the same: 40 is twice the rows' text, 10 half. The panel draws a size at 1.05× its number (rows are 21 px). Sizes aren't limited; small ones are the author's call. Other tags are dropped, keeping their text; entities (`&amp;`) are decoded. `help` is plain text; MCM help tags are stripped ([Help line](ui-and-vr-interaction.md#help-line)).

**`spacer`**: empty space, one line or `numLines`.

```json
{ "type": "spacer", "numLines": 2 }
```

**`image`**: MCM names a picture in the mod's SWF; a config spec names a texture under `Data/Textures`, with optional size in panel pixels:

```json
{ "type": "image", "texture": "MyMod/config/logo.dds", "width": 512, "height": 128 }
```

Loaded by the game like a mesh texture (loose or BA2, any DDS format the game reads). No size: its own; one side: the other keeps the aspect; wider than the rows: shrunk to fit. An unloadable texture is a box of its size naming it, with the reason in the help line and log.

A warning to the player is a `text` row, as in MCM; the only one the host draws is the restart mark ([When a change takes effect](#when-a-change-takes-effect)).

## Validation

`valueOptions` limits are the whole validation; the mod isn't asked and cross-setting expressions aren't planned.

```json
{ "valueOptions": { "min": 0.5, "max": 1.5, "step": 0.01 } }
```

```json
{ "valueOptions": { "allowEmpty": false, "minLength": 1, "maxLength": 64 } }
```

- `allowEmpty: false` equals `minLength: 1`. Lengths count characters, not bytes; typing stops at `maxLength`.
- Choice values must be among their options.
- Config spec errors disable only the affected content where safe and go to the log and [Developer tools](ui-and-vr-interaction.md#developer-tools); an unreadable config spec is [not available](ui-and-vr-interaction.md#notices). Invalid player input disables Save until fixed.

## Localization

Every player-visible text (mod and page names, sections, settings, helps, options and their short texts, button texts, confirmations, units) takes one of three forms:

```json
{ "text": "Player Height" }
```

```json
{ "text": "$MyMod_PlayerHeight" }
```

```json
{ "text": { "token": "$MyMod_PlayerHeight", "fallback": "Player Height" } }
```

Translations are the game's translation files, as MCM mods already have: `Data/Interface/Translations/MyMod_en.txt`, `MyMod_fr.txt`. Each line is a token, a tab, the text; files are UTF-16 LE with a BOM (the only form F4SE reads).

- F4SE loads `<plugin>_<language>.txt` for each enabled plugin in the game's language, with no English fallback; tokens are looked up among them as for MCM.
- A mod without a game plugin: the host reads `<modName>_<language>.txt` itself.
- The `fallback` shows when no file has the token, so English fallbacks need no English file.
- A token without a fallback that no file has shows as written, with a diagnostic, as in MCM.

A language shows only if the UI font has its glyphs: Western European today; wider coverage is open. MCM texts carry over to a config spec as they are.

## A config spec and MCM

Beyond MCM, a config spec has:

- Settings in the mod's own INI and TOML files, plus globals and properties.
- A `default` on every setting, read-only settings, and number `display`.
- Warnings on settings and buttons.
- `applies`.
- Conditions that compare any value, enable as well as show, and check loaded plugins and game INI settings.
- Nesting, folding sections and nesting pages.
- Typed JSON parameters.
- Buttons that run console commands or DLL functions, with their own text and confirmations.
- Help on the mod and pages, and pages told to the mod's DLL.
- Text fallbacks, and images as texture files.

What MCM has and what replaces it:

| MCM | A config spec |
| --- | --- |
| `hiddenSwitcher`, set by a script to show rows | A condition on a value or loaded plugin; for changing game state (a busy warning), a condition on a client state |
| `groupControl` / `groupCondition` by number | `visibleWhen` / `enabledWhen` naming setting ids |
| `{i}`, `{f}`, `{b}` in parameter text | Typed JSON values |
| `{value}` in parameter text | A parameter exactly `"{value}"`, in the setting's type |
| A storage-less switcher that shows hidden rows | A folding section |
| `textinputInt`, `textinputFloat` | `textinput` with `valueType` `int` / `float` |
| ModSetting storage | A file key, read by scripts through the Papyrus API |
| `hotkey`, `keyinput` | [InputBinding](controls.md#commonframework-specific-controls), for CommonFramework mods |
| `dropdownFiles` | A dropdown with [`listFromFolder`](#options-from-a-folder) |
| `textFrom…` / `helpFrom…` | `{clientState:name}` in text or help ([Text, spacers and images](#text-spacers-and-images)); form names are written by the author and translated with `$` tokens |
| `positioner`, `CallExternalFunction` | None: MCM's Flash menu only |
| `ownerModName`, pages under another mod | None: a config spec's pages are its own |
