# The settings model

The menu draws one model, whatever a mod's menu comes from; the MCM loader and the config spec loader fill it. Code: [ConfigDefinition.h](../../src/model/ConfigDefinition.h), [Store.h](../../src/model/Store.h).

- **The definition** is a mod's menu. It doesn't change once loaded.
- **The store** has the selected mod's values. One at a time, as only one mod has a session.

The UI reads the definition to know what to draw and asks the store for every value. It never touches a file, global or property itself.

## The definition

```text
Mod      name, help, front page, pages, category (optional), a config spec's files
Page     name, id (optional), help, items, pages. The front page is the mod's own and has no name
Item     one of:
           Setting
           Group         a section: text, items, whether it folds, optional role, optional id
           Button        a row that runs an action, optional id
           Hotkey        shows the key a mod's hotkey is bound to
           Text, Image, Spacer
           Placeholder   a control the menu doesn't support, with the reason
```

A [not available](ui-and-vr-interaction.md#notices) mod has only its name and the reason.

Every row item has a `text` (what the row says) and an optional `help` (shown in the help line while the ray is on it), both MCM's names.

| Setting field | Meaning |
| --- | --- |
| id | Unique in its mod: the author's, or made by the loader |
| sourceId | The id its source has (MCM's events name a row by it); empty if none |
| type | How it's edited, by MCM's names: switcher, slider, stepper, dropdown, textinput |
| valueType | bool, int, float or string. A CommonFramework table is a string of its TOML text |
| text, help | As above |
| valueOptions | Number min/max/step, text length limits, choice options or where they're read in the game, a dropdown's entry that types a new value, color alpha, transform axis ranges and parts, binding `suppress` and allowed hands/ways/buttons |
| display | Number decimals, unit or percentage (config spec only) |
| readOnly | Shown, not edited (config spec only) |
| warnings | Texts with conditions; while one holds the row is marked and the help line shows it (config spec only) |
| visibleWhen | Condition for the row to show |
| enabledWhen | Condition for the row to be editable |
| storage | Where the value is kept; only the store reads it |
| default | What Reset gives (config spec only; for MCM the store answers) |
| applies | `unknown`, `live`, `save`, `restart` |
| action | Runs after Save writes the value |
| omitDefault | Save removes the key of a value equal to the default (the menu's own Categories rows only) |
| notSearched | Search doesn't find the row by its text (the menu's own Categories rows only) |

Shaped by MCM:

- **The loader makes an id where MCM has none.** In the test set, 33 of 393 value rows have no id and three ids are on two rows each. A made id is the page and the row's position, `main/2` or `page1/1`: stable across runs and languages. MCM's own id text is kept as `sourceId` for its events. A config spec setting has its author's id or one made from its storage ([schema](schema.md#settings)).
- **The default is the store's to answer.** MCM has defaults only for ModSettings, from the mod's `settings.ini`, which the MCM store reads on selection; a row without one has no Reset. A config spec setting always has a `default` (its absence is an error), except a read-only one and a [choice that sets other settings](#a-choice-that-sets-other-settings).
- **Storage has several forms:** global, script property, MCM ModSetting, file key. An MCM row may have none (FallUI's "Unfold settings" switchers: a ModSetting source without an id); the store holds its value, starting off, for as long as the mod is selected, written nowhere. A config spec setting always has storage; a choice that sets other settings has theirs.

`applies` resolves to the setting's own, else the nearest enclosing group's, page's, then mod's, else `unknown`. MCM settings are `unknown` (no such field).

Config spec pages and groups nest; MCM has one level of pages and flat groups.

## The store

The UI asks the store for: a setting's value and state; a hotkey's key (display only); to set a value; a default (Reset sets it as any value); a choice's options (the definition's, or read in the game on selection; MCM rereads them on each page, as MCM does); dirty state, Save and Discard; a dirty setting's saved value and its single-row Discard; to run an action; to read values again.

- **MCM store:** rereads clean values every frame. A set writes the storage, runs the row's action and sends MCM's event; dirty while not at the baseline. Discard sets values back in reverse order; Save keeps them as the baseline (Revert / Keep). See [MCM compatibility](architecture.md#mcm-compatibility).
- **Config spec store:** reads files, globals and properties on selection. A set changes the pending value; a `live` one is previewed (to the client for file keys, into the game by the store for globals/properties). Save writes, then runs actions. See [Editing session](architecture.md#editing-session).

When a change is **saved** is the store's (config spec: on Save; MCM: as made). When the mod **acts** on it is the mod's, stated by `applies`:

| `applies` | The mod acts on it |
| --- | --- |
| `unknown` | Not known: every MCM setting, and config spec settings that say nothing |
| `live` | As it changes, as a preview Discard takes back |
| `save` | After Save |
| `restart` | After a game restart |

A preview never saves. An MCM change that is made isn't necessarily in effect (Unlimited Survival Mode's DLL reads MCM's file itself and says in a text row that it needs a restart); the menu claims only that it is made.

Value states:

- **Ready.**
- **Not available**, with the reason: can't be read or written (global's form not found, ModSetting text not a number, property on no script of its form). The row is disabled.
- **Invalid:** the file's text or the player's input isn't a value of the setting. Shown as is; a player-made invalid value blocks Save.

## Conditions

A condition compares one setting's value (by id) or joins conditions with all / any / not, evaluated on the store's values (for a config spec, the pending ones, so rows react while editing).

- MCM's `groupControl` / `groupCondition` become `visibleWhen`: a group is on while its controlling row is on. Group numbers are per page (the test set reuses them). Each of the test set's 23 groups has one controlling switcher on its own page.
- A `groupCondition` is a number, a list (= `OR`), or `AND` / `OR` / `ONLY` with a list, as MCM reads it. `ONLY` holds when all its groups are on and no other group of the page is. A group no row controls is never on.
- A condition left open by an unavailable value holds, so the row shows: nothing hides for a reason the player can't change. Available values still settle it (a row needing two groups hides while one is off).
- A value no row shows (MCM's hidden switcher) is off while not available, as MCM reads it, so rows a script reveals stay hidden while it can't be read.
- Config specs have `visibleWhen` and `enabledWhen` ([schema](schema.md#conditions)). A section's conditions cover everything inside at every level; the loader joins them into each row's own, so in the model a condition is always a row's. An MCM section's condition stays on its header row, as in MCM.
- A plugin-loaded condition can't change at runtime, so the loader answers it while reading the config spec.

## A group with a role

A value the framework writes as a TOML inline table (color, input binding, transform) is a string setting holding the table's TOML text, with its own control; the store writes it back as the inline table, not a quoted string. A haptic is a string setting naming a pattern. See [Controls](controls.md#commonframework-specific-controls).

A control spanning several keys is a group with a role: ordinary settings, one per key, with the role deciding how the group is drawn (activation sphere; a color in three keys). Each setting is one key, so dirty state, Save and preview need nothing new.

## A choice that sets other settings

A choice whose options set other settings ([schema](schema.md#options-that-set-other-settings)) is a setting whose storage is those settings. Each option has `sets`: the ids of the settings it sets, each with its value, and `alsoSets`, set the same way by a pick but never read back. A color's or a transform's value is the TOML text of the parts it names.

- The store answers its value from theirs: the first option whose `sets` they all have, or a value of its type that is no option while none matches. A pick sets each of its `sets` and `alsoSets`, so dirty state, Save and preview stay theirs.
- It is dirty while one of them is, and its Discard is theirs. Its default and its saved value are the options matching their defaults and their saved values.
- The counts of changes (footer, folded section, tree) skip it: what counts is what Save writes.

## Examples

The sketch of the panel only places the rows; the look is the design system's.

### An MCM mod

The mod's `config.json`. Its root `content` is the mod's front page, and `pages` has its other pages:

```json
{
  "modName": "MyMod",
  "displayName": "My Mod",
  "content": [
    { "type": "section", "text": "General" },
    {
      "type": "switcher", "text": "Enable the feature", "help": "Turns the feature on.",
      "groupControl": 2,
      "valueOptions": { "sourceType": "GlobalValue", "sourceForm": "MyMod.esp|1234" },
      "action": { "type": "CallFunction", "form": "MyMod.esp|800", "function": "OnToggle", "params": ["{value}"] }
    },
    {
      "type": "slider", "text": "Strength", "help": "How strong the feature is.", "id": "fStrength:Main",
      "groupCondition": 2,
      "valueOptions": { "sourceType": "ModSettingFloat", "min": 0, "max": 2, "step": 0.1 }
    }
  ],
  "pages": [
    {
      "pageDisplayName": "Advanced",
      "content": [
        { "type": "switcher", "text": "Unfold settings", "groupControl": 3, "valueOptions": { "sourceType": "ModSettingBool" } },
        {
          "type": "dropdown", "text": "Mode", "help": "Low is easier on the frame rate.", "id": "iMode:Main",
          "groupCondition": 3,
          "valueOptions": { "sourceType": "ModSettingInt", "options": ["Low", "High"] }
        },
        {
          "type": "button", "text": "Reset everything", "help": "Puts every value of the mod back.",
          "action": { "type": "CallFunction", "form": "MyMod.esp|800", "function": "ResetAll" }
        }
      ]
    }
  ]
}
```

The definition the MCM loader makes of it:

```text
Mod "My Mod"
├─ Front page                      the config's root content; the mod's row in the tree
│  └─ Group "General"
│     ├─ Setting main/2            "Enable the feature"   bool, switcher
│     │     help         Turns the feature on.
│     │     storage      MCM global MyMod.esp|1234
│     │     action       CallFunction OnToggle({value}) on MyMod.esp|800
│     └─ Setting fStrength:Main    "Strength"             float, slider from 0 to 2 by 0.1
│           help         How strong the feature is.
│           storage      MCM ModSetting fStrength of [Main]
│           visibleWhen  main/2 is on
└─ Page "Advanced"                 a row under the mod in the tree
   ├─ Setting page1/1              "Unfold settings"      bool, switcher
   │     storage      none: the store holds the value
   ├─ Setting iMode:Main           "Mode"                 int, dropdown: 0 "Low", 1 "High"
   │     help         Low is easier on the frame rate.
   │     storage      MCM ModSetting iMode of [Main]
   │     visibleWhen  page1/1 is on
   └─ Button "Reset everything"    CallFunction ResetAll() on MyMod.esp|800
         help         Puts every value of the mod back.
```

- `main/2` and `page1/1` are ids the loader made, from the page and the row's place in it. The two other settings have their author's.
- The section row became a group, with the rows that follow it.

What the MCM store answers while the mod is selected:

```text
Setting          Value   Default   State
main/2           on                ready
fStrength:Main   1.3     1.0       ready           the default is the one of the mod's settings.ini
page1/1          off               ready           held by the store, off when the mod is selected
iMode:Main       none              ready           in neither of MCM's files: MCM reads -1, and the store holds what it is set to
```

The front page, drawn from the two, with the ray on "Enable the feature". Its help is in the help line, at the bottom:

```text
┌───────────────────────────────────────────────────────────────────┐
│ MODS CONFIG FO4 » My Mod                                          │
├───────────────────┬───────────────────────────────────────────────┤
│ ▾ My Mod          │ General                                       │
│     Advanced      │ Enable the feature                   [ On ]   │
│ ▸ Another Mod     │ Strength          [-] [=====o----] [1.3]      │
│                   │                                               │
├───────────────────┴───────────────────────────────────────────────┤
│ Enable the feature: Turns the feature on.                         │
└───────────────────────────────────────────────────────────────────┘
```

A press on "Enable the feature":

1. The UI sets `main/2` to off.
2. The MCM store writes 0 to the global, runs `OnToggle` with the value, and sends MCM's event. `main/2` is dirty.
3. The UI asks for the values again. `main/2` is off, so the Strength row is not visible.

When the ray leaves the rows, the help line goes back to the status, which counts one change made. Keep accepts it. Revert writes 1 to the global, runs `OnToggle` with it, and sends MCM's event again.

### A config spec

The mod's config spec. It has the same shape: `content` is the front page, and `pages` has the other pages:

```json
{
  "minSchemaVersion": 1,
  "modName": "mymod",
  "displayName": "My Mod",
  "files": {
    "main": { "format": "ini", "root": "myGames", "path": "Mods_Config/MyMod/MyMod.ini" }
  },
  "defaultFile": "main",
  "applies": "live",
  "content": [
    {
      "type": "switcher", "valueType": "bool",
      "text": "Holsters", "help": "Draw and holster weapons at the body.",
      "default": true, "applies": "restart",
      "storage": { "section": "MyMod", "key": "bEnableHolsters" }
    }
  ],
  "pages": [
    {
      "pageDisplayName": "Body",
      "content": [
        {
          "id": "playerHeight", "type": "slider", "valueType": "float",
          "text": "Player Height", "help": "Adjusts the estimated player height.",
          "default": 1.0,
          "valueOptions": { "min": 0.5, "max": 1.5, "step": 0.01 },
          "storage": { "section": "MyMod", "key": "fPlayerHeight" },
          "enabledWhen": { "setting": "bEnableHolsters:MyMod", "equals": true }
        }
      ]
    }
  ]
}
```

The definition the config spec loader makes of it:

```text
Mod "My Mod"
├─ Front page                           the config spec's content; the mod's row in the tree
│  └─ Setting bEnableHolsters:MyMod     "Holsters"        bool, switcher
│        help         Draw and holster weapons at the body.
│        storage      bEnableHolsters of [MyMod] in MyMod.ini
│        default      on
│        applies      restart, its own
└─ Page "Body"                          a row under the mod in the tree
   └─ Setting playerHeight              "Player Height"   float, slider from 0.5 to 1.5 by 0.01
         help         Adjusts the estimated player height.
         storage      fPlayerHeight of [MyMod] in MyMod.ini
         default      1.0
         enabledWhen  bEnableHolsters:MyMod is on
         applies      live, the mod's
```

- `bEnableHolsters:MyMod` is an id the loader made, from the key and the section, as MCM names a ModSetting. `playerHeight` is its author's.
- `playerHeight` has no `applies`, and neither has its page, so it takes the mod's: `live`. The Holsters switcher has its own, `restart`. It is not previewed, and a Save that writes it says that the game has to be restarted.

The file holds `fPlayerHeight = 1.1` and has no `bEnableHolsters` key. What the config spec store answers after the player dragged the slider:

```text
Setting                 In the file   Pending   Dirty   State
bEnableHolsters:MyMod   no key        on        no      ready     the default, as the key is missing
playerHeight            1.1           1.25      yes     ready     previewed to the mod's client
```

And with `fPlayerHeight = 2.0` in the file, which is past the most:

```text
Setting                 In the file   Pending   Dirty   State
playerHeight            2.0           2.0       no      invalid   shown as it is, and not previewed
```

Save writes the dirty keys alone: `fPlayerHeight`, and no `bEnableHolsters`.

### An activation sphere

A section of Immersive Flashlight's TOML, shortened:

```toml
[ImFl_BodyActivationSphere]
tZone = { pos = [3.91, 7.11, 0.67], rot = [0.0, 0.0, 0.0], scale = 20.0 }
kPrimaryBinding = { hand = "offhand", type = "tap", button = "trigger", suppress = true }
sEntryHaptic = "Tick"
fIconSize = 2.0
```

The config spec names the section once, as a `cf.activationSphere` item, and the loader makes a group of it, with a setting for each key it shows: the keys the config spec hides, as a sphere with no icon, have none, and keep the file's values.

```text
Group "Body grab"                                         role: activation sphere
├─ Setting tZone:ImFl_BodyActivationSphere                string (table), transform control
├─ Setting kPrimaryBinding:ImFl_BodyActivationSphere      string (table), binding control
├─ Setting sEntryHaptic:ImFl_BodyActivationSphere         string, haptic control
└─ Setting fIconSize:ImFl_BodyActivationSphere            float, slider
```

- The role draws them as one group: the primary binding, the entry haptic and Icons, then Position and size, Activation and Visuals, which fold. Icons is a [choice that sets other settings](#a-choice-that-sets-other-settings), the Show keys of the icon and the binding icon, which the loader makes for every sphere. Each setting has the text the config spec gives it, or its default name, as Activate for the primary binding.
- The player changes the primary binding. That key alone is dirty, is previewed, and is written by Save.
- The framework's client previews it as it previews any key: an override for that section and key.
