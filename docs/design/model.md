# The settings model

The menu draws one model, whatever a mod's menu comes from. A loader fills it: the MCM loader first, the package loader later. This document says what the model holds, and shows it on three examples: an MCM mod, a package, and an activation sphere.

## Two parts

- **The definition** is what a mod's menu holds. It does not change once it is loaded.
- **The store** has the values of the mod that is selected. There is one store at a time, because only one mod has an editing session.

The UI reads the definition to know what to draw, and asks the store for every value. It never reads or writes a file, a global or a script property itself.

## The definition

```text
Mod      a name, its front page, its pages, and a category if it has one
Page     a name and its items. The front page is the mod's own, and has no name
Item     one of:
           Setting
           Group         a section: its text, its items, and sometimes a role
           Button        a row that runs an action
           Text, Image, Spacer
           Placeholder   a control the menu does not support, with the reason
```

A mod that is [not available](ui-and-vr-interaction.md#notices) has its name and the reason, and nothing else.

A setting, a button, and any other item that is a row has a `text` and can have a `help`. The text is what the row says. The help of the row the ray is on is shown in the help line, on the left of the footer. Both names are MCM's, and a package uses them too. In the test set 376 rows have a help: most switchers, sliders, steppers and buttons, and every hotkey.

A setting has:

| Field        | What it is                                                                                            |
| ------------ | ----------------------------------------------------------------------------------------------------- |
| id           | Its identity, unique in its mod: its author's, or one the loader makes                                |
| type         | How it is edited, by MCM's names: switcher, slider, stepper, dropdown, textinput                      |
| valueType    | What the value is: bool, int, float or string. Color comes with its control                           |
| text         | What the row says                                                                                     |
| help         | Its help text, shown in the help line while the ray is on the row                                     |
| valueOptions | The least, the most and the step of a number, the length limits of a text, or the options of a choice |
| default      | The value Reset gives. A package's setting always has one, an MCM row may not                         |
| visibleWhen  | A condition on the values of other settings. The row is shown while it holds                          |
| enabledWhen  | A condition of the same kind. The row can be edited while it holds                                    |
| storage      | Where the value is kept. Only the store reads it                                                      |
| applies      | When a change takes effect in the running mod: live, save or restart. A package's only                |
| action       | What runs after the value is written, for an MCM row that has one                                     |

Three of these are as they are because of MCM:

- **The loader makes an id where MCM has none.** In the MCM test set 33 of the 393 value rows have no id, and three ids are each on two rows. Such a row gets an id from its place in the config, its page and its position, which is the same on every run. MCM's event carries the id as the config has it, so the MCM store keeps that text with the row's storage. A package's setting has its author's id, or one made from its storage, the key and the section: see [Declarative schema](schema.md#settings).
- **The default is optional for MCM only.** MCM declares a default only for a ModSetting, and a row without one has no Reset. A package's setting always has a default: the schema requires it, and a setting without one is an error of the package.
- **The storage has several forms.** An MCM global, an MCM script property, an MCM ModSetting, or a key of a file. An MCM row can also have none: FallUI has three switchers, two of them named "Unfold settings", that only show and hide a group of rows and have nowhere to keep their value. The store holds the value of such a setting by itself: it starts off, lasts while the mod is selected, and is written nowhere. A package's setting always has a storage.

`applies` is a package's. It says when a change takes effect in the running mod: as it is made, after Save, or after the game is restarted. The mod always has one, and a page, a group and a setting can each have their own. A setting takes its own, or else its group's, its page's, its mod's. An MCM mod has none: an MCM config has no field for it.

## The store

What the UI asks of the store:

- The value of a setting, with its state.
- To set a value.
- Whether a change is saved at once or waits for Save.
- With Save: whether a setting is dirty, and to Save, Discard, and Reset a setting.
- To run an action.
- To read its values again.

There are two stores, and what happens around a change is theirs:

- **The MCM store** writes a value where its storage says, at once. It then runs the row's action and sends MCM's event. It has no Save.
- **The package store** reads the files when the mod is selected. A set changes the pending value, and that of a `live` setting is previewed to the mod's client. Save and Discard are as in [Architecture and lifecycle](architecture.md#editing-session).

Two things are kept apart: when a change is saved, and when the running mod acts on it. The first is the store's. The second is the mod's, and a package says it with `applies`:

| The setting        | Saved            | The mod acts on it                                     |
| ------------------ | ---------------- | ------------------------------------------------------ |
| MCM                | As it is changed | Not known to the menu                                  |
| Package, `live`    | On Save          | As it is changed, as a preview that Discard takes back |
| Package, `save`    | On Save          | After Save                                             |
| Package, `restart` | On Save          | After the game is restarted                            |

So a `live` setting is never saved by being previewed. And an MCM change that is saved is not always in effect: Unlimited Survival Mode's DLL reads MCM's settings file itself, and its page says in a text row that its settings need a restart. The menu shows such a text as MCM does, and claims no more than that the change is saved.

A value has a state, and a row whose value is not ready says why:

- **Ready.**
- **Not available**, with the reason. The value cannot be read or written: the form of a global is not found, a ModSetting is in neither of MCM's files, or a property is on no script of its form. The row is disabled.
- **Invalid.** The text in a file is not a value of the setting, or the player typed one that is not. The row shows the text as it is, and an invalid value that the player made blocks Save.

## Conditions

A condition compares the value of one setting, named by its id, or joins other conditions with all, any and not. It is evaluated on the store's values, which for a package are the pending ones, so a row shows and hides as the player edits.

- The MCM loader turns `groupControl` and `groupCondition` into `visibleWhen`: a group is on when the row that controls it is on. A group's number belongs to its page: the test set uses the same numbers again on other pages. Each of its 23 groups has one controlling row, a switcher, on the group's own page.
- A package has `visibleWhen` and `enabledWhen`, as in [Declarative schema](schema.md#conditions). A section's condition covers its rows there. The package loader gives it to each of those rows, joined with the row's own, so in the model a condition is always a row's. An MCM section's condition stays on its header row alone, as in MCM.

## A group with a role

A value that the framework writes as one text in an INI is a string setting with a control of its own: a transform, an input binding, a haptic.

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
│           default      1.0, from the mod's settings.ini
│           visibleWhen  main/2 is on
└─ Page "Advanced"                 a row under the mod in the tree
   ├─ Setting advanced/1           "Unfold settings"      bool, switcher
   │     storage      none: the store holds the value
   ├─ Setting iMode:Main           "Mode"                 int, dropdown: 0 "Low", 1 "High"
   │     help         Low is easier on the frame rate.
   │     storage      MCM ModSetting iMode of [Main]
   │     visibleWhen  advanced/1 is on
   └─ Button "Reset everything"    CallFunction ResetAll() on MyMod.esp|800
         help         Puts every value of the mod back.
```

- `main/2` and `advanced/1` are ids the loader made, from the page and the row's position. The two other settings have their author's.
- The section row became a group, with the rows that follow it.

What the MCM store answers while the mod is selected:

```text
Setting          Value   State
main/2           on      ready
fStrength:Main   1.3     ready
advanced/1       off     ready           held by the store, off when the mod is selected
iMode:Main               not available   the ModSetting is in neither of MCM's files
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
2. The MCM store writes 0 to the global, runs `OnToggle` with the value, and sends MCM's event.
3. The UI asks for the values again. `main/2` is off, so the Strength row is not visible.

Nothing is dirty. When the ray leaves the rows, the help line goes back to the status, which says that changes are saved as they are made.

### A package

The mod's package. It has the same shape: `content` is the front page, and `pages` has the other pages:

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

The definition the package loader makes of it:

```text
Mod "My Mod"
├─ Front page                           the package's content; the mod's row in the tree
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

The file holds `fPlayerHeight = 1.1` and has no `bEnableHolsters` key. What the package store answers after the player dragged the slider:

```text
Setting                 In the file   Pending   Dirty   State
bEnableHolsters:MyMod   no key        on        no      ready     the default, as the key is missing
playerHeight            1.1           1.25      yes     ready     previewed to the mod's client
```

And after the player typed 2.0, which is past the most:

```text
Setting                 In the file   Pending   Dirty   State
playerHeight            1.1           2.0       yes     invalid   not previewed, and Save is blocked
```

Save writes the dirty keys alone: `fPlayerHeight`, and no `bEnableHolsters`.

### An activation sphere

A section of Immersive Flashlight's INI, shortened:

```ini
[ImFl_BodyActivationSphere]
tZone = 3.91,7.11,0.67;0.00,0.00,0.00;20.00
sPrimaryBinding = offhand tap trigger suppress
sEntryHaptic = Tick
fIconSize = 2
```

The package names the section once, and the loader makes a group of it, with a setting for each key:

```text
Group "Body grab"                                         role: activation sphere
├─ Setting tZone:ImFl_BodyActivationSphere                string, transform control
├─ Setting sPrimaryBinding:ImFl_BodyActivationSphere      string, binding control
├─ Setting sEntryHaptic:ImFl_BodyActivationSphere         string, haptic control
└─ Setting fIconSize:ImFl_BodyActivationSphere            float, slider
```

- The role draws the four as one control, where the sphere can be tuned as a whole.
- The player changes the primary binding. That key alone is dirty, is previewed, and is written by Save.
- The framework's client previews it as it previews any key: an override for that section and key.
