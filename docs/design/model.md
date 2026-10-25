# The settings model

The menu draws one model, whatever a mod's menu comes from. A loader fills it: the MCM loader first, the config spec loader later. This document says what the model holds, and shows it on three examples: an MCM mod, a config spec, and an activation sphere.

## Two parts

- **The definition** is what a mod's menu holds. It does not change once it is loaded.
- **The store** has the values of the mod that is selected. There is one store at a time, because only one mod has an editing session.

The UI reads the definition to know what to draw, and asks the store for every value. It never reads or writes a file, a global or a script property itself.

## The definition

```text
Mod      a name, a help, its front page, its pages, a category if it has one, and a config spec's files
Page     a name, an id if it has one, a help, its items, and its pages. The front page is the mod's own, and has no name
Item     one of:
           Setting
           Group         a section: its text, its items, whether it folds, and sometimes a role
           Button        a row that runs an action
           Hotkey        a row that shows the key a hotkey of the mod is bound to
           Text, Image, Spacer
           Placeholder   a control the menu does not support, with the reason
```

A mod that is [not available](ui-and-vr-interaction.md#notices) has its name and the reason, and nothing else.

A setting, a button, and any other item that is a row has a `text` and can have a `help`. The text is what the row says. The help of the row the ray is on is shown in the help line, on the left of the footer. Both names are MCM's, and a config spec uses them too. In the test set 376 rows have a help: most switchers, sliders, steppers and buttons, and every hotkey.

A setting has:

| Field        | What it is                                                                                            |
| ------------ | ----------------------------------------------------------------------------------------------------- |
| id           | Its identity, unique in its mod: its author's, or one the loader makes                                |
| sourceId     | The id its source has for it, which MCM's events name an MCM row by. Empty for a row with none        |
| type         | How it is edited, by MCM's names: switcher, slider, stepper, dropdown, textinput                      |
| valueType    | What the value is: bool, int, float or string. A CommonFramework table is a string, its TOML text     |
| text         | What the row says                                                                                     |
| help         | Its help text, shown in the help line while the ray is on the row                                     |
| valueOptions | The least, the most and the step of a number, the length limits of a text, the options of a choice or where it reads them in the game, whether a color has an alpha, or whether a binding offers `suppress` |
| display      | How a number is shown: its decimals, a unit, or a percentage of its range. A config spec's only           |
| readOnly     | The value is shown and not edited. A config spec's only                                                   |
| warnings     | Texts with a condition each: while one holds, the row is marked and the help line shows it. A config spec's only |
| visibleWhen  | A condition on the values of other settings. The row is shown while it holds                          |
| enabledWhen  | A condition of the same kind. The row can be edited while it holds                                    |
| storage      | Where the value is kept. Only the store reads it                                                      |
| default      | The value Reset gives it. A config spec's only: an MCM row's is its store's to answer                     |
| applies      | When a change takes effect in the running mod: unknown, live, save or restart. Unknown for an MCM row     |
| action       | What runs after Save writes the value                                                                     |

Three things are as they are because of MCM:

- **The loader makes an id where MCM has none.** In the MCM test set 33 of the 393 value rows have no id, and three ids are each on two rows. Such a row gets an id from its place in the config: `main` for the front page, or `page` and the number of its page, and the place of the row in the page, as `main/2` and `page1/1`. It is the same on every run and in every language. MCM's event carries the id as the config has it, so a setting keeps that text too, as its `sourceId`. A config spec's setting has its author's id, or one made from its storage, as the key and the section: see [Declarative schema](schema.md#settings).
- **The default is the store's to answer.** MCM has a default only for a ModSetting, and not in its config: it is the value the mod's `settings.ini` has for it, a file the MCM store reads as the mod is selected. So the UI asks the store for the default, and a row without one has no Reset. A config spec's setting always has a default: the schema requires it, and a setting without one is an error of the config spec. The config spec loader reads it into the setting, and the config spec store answers it from there. A read-only setting is the one that needs none, as it has no Reset.
- **The storage has several forms.** A global, a script property, an MCM ModSetting, or a key of a file. An MCM row can also have none: FallUI has three switchers, two of them named "Unfold settings", that only show and hide a group of rows and have nowhere to keep their value: their source is a ModSetting, which is named by the row's id, and they have no id. The store holds the value of such a setting by itself: it starts off, lasts while the mod is selected, and is written nowhere. A config spec's setting always has a storage: a key of a file, a global or a script property.

`applies` says when a change takes effect in the running mod: as it is made, after Save, after the game is restarted, or not known. A config spec's mod, a page, a group and a setting can each have their own. A setting takes its own, or else the nearest group's it is in, the nearest page's, its mod's, and else it is unknown. An MCM setting is unknown: an MCM config has no field for it.

Pages and groups nest in a config spec: a page can have pages, and a group can hold groups. An MCM mod has one level of pages, and its groups do not nest.

## The store

What the UI asks of the store:

- The value of a setting, with its state.
- The key of a hotkey, which is only shown.
- To set a value.
- The default of a setting, where it has one. Reset sets it, as any other value is set.
- The options of a choice: the definition's, or the ones it reads in the game as the mod is selected, for a choice that takes them from a FormList or a property of a script. The MCM store reads them again as another page is shown, as MCM's menu does.
- Whether a setting is dirty, and to Save and Discard.
- To run an action.
- To read its values again.

There are two stores, and what happens around a change is theirs:

- **The MCM store** reads a clean value in every frame, and a set changes the pending value, with no preview. Save writes the dirty values where their storage says, then runs each row's action and sends MCM's event. See [MCM compatibility](architecture.md#mcm-compatibility).
- **The config spec store** reads the files, and the globals and properties, when the mod is selected. A set changes the pending value, and that of a `live` setting is previewed: to the mod's client for a key of a file, and into the game by the store itself for a global or a property. Save writes the values and then runs the actions of the settings it wrote. Save and Discard are as in [Architecture and lifecycle](architecture.md#editing-session).

Two things are kept apart: when a change is saved, and when the running mod acts on it. The first is the store's, and every change is saved on Save. The second is the mod's, and a config spec can say it with `applies`:

| `applies`  | The mod acts on it                                     |
| ---------- | ------------------------------------------------------ |
| `unknown`  | Not known to the menu: every MCM setting, and a config spec's that says nothing |
| `live`     | As it is changed, as a preview that Discard takes back |
| `save`     | After Save                                             |
| `restart`  | After the game is restarted                            |

So a `live` setting is never saved by being previewed. And an MCM change that is saved is not always in effect: Unlimited Survival Mode's DLL reads MCM's settings file itself, and its page says in a text row that its settings need a restart. The menu shows such a text as MCM does, and claims no more than that the change is saved.

A value has a state, and a row whose value is not ready says why:

- **Ready.**
- **Not available**, with the reason. The value cannot be read or written: the form of a global is not found, the text of a ModSetting is no number, or a property is on no script of its form. The row is disabled.
- **Invalid.** The text in a file is not a value of the setting, or the player typed one that is not. The row shows the text as it is, and an invalid value that the player made blocks Save.

## Conditions

A condition compares the value of one setting, named by its id, or joins other conditions with all, any and not. It is evaluated on the store's values, which for a config spec are the pending ones, so a row shows and hides as the player edits.

- The MCM loader turns `groupControl` and `groupCondition` into `visibleWhen`: a group is on when the row that controls it is on. A group's number belongs to its page: the test set uses the same numbers again on other pages. Each of its 23 groups has one controlling row, a switcher, on the group's own page.
- A `groupCondition` is a number, a list of numbers, or `AND`, `OR` or `ONLY` with a list, as MCM's menu reads it. A list is `OR`. `ONLY` holds with all of its groups while no other group of the page is on. A group that no row controls is never on.
- A condition that a value which is not available leaves open holds, so its row is shown: nothing is hidden for a reason the player cannot change. A value that is available still settles it: a row that needs two groups is hidden while one of them is off.
- The value of a setting that no row shows, as MCM's hidden switcher, is off while it is not available, as MCM's menu reads it. A script sets it to show a row while something of the game holds, so its rows stay hidden while it cannot be read.
- A config spec has `visibleWhen` and `enabledWhen`, as in [Declarative schema](schema.md#conditions). A section's conditions cover everything in it there, at every level. The config spec loader gives each row the conditions of the sections it is in, joined with the row's own, so in the model a condition is always a row's. An MCM section's condition stays on its header row alone, as in MCM.
- A config spec's condition can ask whether a plugin is loaded. That does not change while the game runs, so the config spec loader answers it as it reads the config spec, and the condition holds or does not.

## A group with a role

A value that the framework writes as an inline table in its TOML is a string setting of one key, whose value is the TOML text of the table, with a control of its own that reads and writes that text: a color, an input binding, a transform. The store writes it into the file as the inline table, not as a quoted string. A haptic is a string setting too, the name of a pattern, with a control of its own. See [Controls](controls.md#commonframework-specific-controls).

A control over several keys is a group with a role. Its settings are ordinary ones, one for each key, and the role says how the group is drawn. Two roles are known: an activation sphere, and a color in three keys.

A setting is stored in one key. So dirty state, Save and preview work for each key, with nothing new for them.

## Examples

The ids, the trees and the tables below show what the model holds. The sketch of the panel is only there to place the rows: the look is not designed here.

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
│ MODS CONFIG VR » My Mod                                           │
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
2. The MCM store holds off as its pending value. `main/2` is dirty.
3. The UI asks for the values again. `main/2` is off, so the Strength row is not visible.

When the ray leaves the rows, the help line goes back to the status, which counts one unsaved change. Save writes 0 to the global, runs `OnToggle` with the value, and sends MCM's event.

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

The config spec names the section once, and the loader makes a group of it, with a setting for each key:

```text
Group "Body grab"                                         role: activation sphere
├─ Setting tZone:ImFl_BodyActivationSphere                string (table), transform control
├─ Setting kPrimaryBinding:ImFl_BodyActivationSphere      string (table), binding control
├─ Setting sEntryHaptic:ImFl_BodyActivationSphere         string, haptic control
└─ Setting fIconSize:ImFl_BodyActivationSphere            float, slider
```

- The role draws the four as one control, where the sphere can be tuned as a whole.
- The player changes the primary binding. That key alone is dirty, is previewed, and is written by Save.
- The framework's client previews it as it previews any key: an override for that section and key.
