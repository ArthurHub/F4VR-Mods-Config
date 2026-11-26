# The settings model

The menu draws one model, whatever a mod's menu comes from; the MCM loader and the config spec loader fill it. Code: [ConfigDefinition.h](../../src/model/ConfigDefinition.h), [Store.h](../../src/model/Store.h).

- **The definition** is a mod's menu. It doesn't change once loaded, but for the rows a [dynamic page](schema.md#dynamic-pages) is given.
- **The store** has the selected mod's values; one at a time, as only one mod has a session.

The UI reads the definition to know what to draw and asks the store for every value. It never touches a file, global or property itself.

## A setting's parts

A setting has four separate parts, so file format, control and runtime behavior stay uncoupled:

| Part | Meaning | Config spec field |
| --- | --- | --- |
| Type | The value the mod stores: `bool`, `int` (signed 32-bit), `float` (32-bit), `string` | `valueType` |
| Control | How the menu shows and edits it (MCM's names, and the CommonFramework types) | `type` |
| Storage | Where it is read and written | `storage` |
| Application policy | When the mod acts on a change | `applies` |

A choice has no type of its own: its options store typed values. Each control's look and behavior is its [design system](design-system/README.md) component.

### CommonFramework types

First-class controls for the framework's own value types, never flattened into tuples. The framework's TOML fixes the storage, and the key prefix says which:

| Control | Prefix | Stored as |
| --- | --- | --- |
| Haptic | `s` | A pattern name of the framework's library, or `"none"`: `sEntryHaptic = "Tick"` |
| Color | `c` | `{ rgb = [r, g, b], alpha = a }`, whole numbers 0..255; a missing key keeps the default's channels |
| InputBinding | `k` | `{ hand = "offhand", type = "tap", button = "grip" }`, optional `duration`, `suppress`, `modifier`; `{ hand, type = "axis", direction, axis, threshold }`; `{ type = "none" }` |
| Transform | `t` | `{ pos = [x, y, z], rot = [heading, roll, attitude], scale = s }`, degrees, each optional |
| ActivationSphere | | A whole section: its zones, bindings, haptics, and sphere and icon keys |
| List (planned) | `a` | An array of strings: `aHeadLightDenyList = ["0316D4\|Fallout4.esm"]` |
| HandPose (planned) | `h` | `{ thumb = [prox, mid, dist, splay], index, middle, ring, pinky, palm = [pitch, yaw] }`, each optional |

A table is one value in one key: dirty, previewed and saved as a whole. The framework names any wrong key or word, and an unreadable table is invalid with that reason. The editors are popups in the content pane: Done or a press beside keeps the value, Cancel and Back restore the value from when it opened.

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

A [not available](architecture.md#notices) mod has only its name and the reason. Every row item has a `text` and an optional `help`, MCM's names.

| Setting field | Meaning |
| --- | --- |
| id | Unique in its mod: the author's, or made by the loader |
| sourceId | The id its source has (MCM's events name a row by it); empty if none |
| type | How it's edited: MCM's switcher, slider, stepper, dropdown, textinput, and the CommonFramework binding, haptic, color and transform |
| valueType | bool, int, float or string. A CommonFramework table is a string of its TOML text |
| valueOptions | Number min/max/step, text length limits, choice options or where they're read, a dropdown's entry that types a new value, color alpha, transform axis ranges and parts, binding `suppress` and allowed hands/types/buttons |
| display | Number decimals, unit or percentage (config spec only) |
| readOnly | Shown, not edited (config spec only) |
| warnings | Texts with conditions; while one holds the row is marked and the help line shows it (config spec only) |
| visibleWhen, enabledWhen | Conditions for the row to show and to be editable |
| storage | Where the value is kept; only the store reads it |
| default | What Reset gives (config spec only; for MCM the store answers) |
| applies | `unknown`, `live`, `save`, `restart` |
| action | Runs after Save writes the value |
| omitDefault | Save removes the key of a value equal to the default, and a missing key is no warning |
| notSearched | Search doesn't find the row by its text (the menu's own Categories rows) |

Shaped by MCM:

- **The loader makes an id where MCM has none** (33 of the test set's 393 value rows, and three ids are on two rows each): the page and the row's position, `main/2` or `page1/1`, stable across runs and languages. MCM's own id is kept as `sourceId` for its events. A config spec setting has its author's id or one made from its storage ([schema](schema.md#settings)).
- **The default is the store's to answer.** MCM has defaults only for ModSettings, from the mod's `settings.ini`; a row without one has no Reset. A config spec setting always has a `default`, except a read-only one and a [choice that sets other settings](#a-choice-that-sets-other-settings).
- **Storage** is a global, script property, MCM ModSetting, file key or client state. An MCM row may have none (FallUI's "Unfold settings"): the store holds its value, off at selection, written nowhere. A color kept in sliders (MCM's R, G and B sliders, or a config spec's `cf.color` with `parts`) has theirs: its value is their `r,g,b` or `r,g,b,a`, and the sliders are hidden settings of their own.

`applies` resolves to the setting's own, else the nearest enclosing group's, page's, then mod's, else `unknown`. Config spec pages and groups nest; MCM has one level of pages and flat groups.

## The store

The UI asks the store for: a setting's value and state; a hotkey's key; to set a value; a default; a choice's options (the definition's, or read in the game on selection); dirty state, Save and Discard; a dirty setting's saved value and its single-row Discard; to run an action; to read values again.

- **MCM store:** rereads clean values every frame. A set writes the storage, runs the row's action and sends MCM's event; dirty while not at the baseline. See [MCM compatibility](architecture.md#mcm-compatibility).
- **Config spec store:** reads on selection. A set changes the pending value, and a `live` one is previewed. Save writes, then runs actions. See [Editing session](architecture.md#editing-session).

When a change is **saved** is the store's (config spec: on Save; MCM: as made). When the mod **acts** on it is the mod's, stated by `applies`:

| `applies` | The mod acts on it |
| --- | --- |
| `unknown` | Not known: every MCM setting, and config spec settings that say nothing |
| `live` | As it changes, as a preview Discard takes back |
| `save` | After Save |
| `restart` | After a game restart |

A preview never saves. An MCM change that is made isn't necessarily in effect (Unlimited Survival Mode's DLL reads MCM's file itself); the menu claims only that it is made.

Value states:

- **Ready.**
- **Not available**, with the reason: can't be read or written (global's form not found, ModSetting text not a number, property on no script of its form). The row is disabled.
- **Invalid:** the file's text or the player's input isn't a value of the setting. Shown as is; a player-made invalid value blocks Save.

An available value can also be **overridden**: another mod sets a value in place of it for the session. The store keeps each override, the one in effect first, with its owner and its value as the control shows it. The config spec store asks the mod's client as it reads values ([Overrides](architecture.md#overrides)); MCM has none. An overridden setting isn't previewed, and its row has the warning mark.

## Conditions

A condition compares one setting's value (by id), asks a client state or a plugin, or joins conditions with all / any / not, evaluated on the store's values (for a config spec, the pending ones, so rows react while editing).

- MCM's `groupControl` / `groupCondition` become `visibleWhen`: a group is on while its controlling row is on. Group numbers are per page. A `groupCondition` is a number, a list (= `OR`), or `AND` / `OR` / `ONLY` with a list, as MCM reads it; `ONLY` holds when all its groups are on and no other group of the page is. A group no row controls is never on.
- A condition left open by an unavailable value holds, so the row shows: nothing hides for a reason the player can't change. Available values still settle it.
- A value no row shows (MCM's hidden switcher) is off while not available, as MCM reads it.
- A config spec section's conditions cover everything inside; the loader joins them into each row's own, so in the model a condition is always a row's. An MCM section's condition stays on its header row, as in MCM.
- A plugin-loaded condition can't change at runtime, so the loader answers it while reading the config spec.

## A group with a role

A value the framework writes as a TOML inline table (color, input binding, transform) is a string setting holding the table's TOML text, with its own control; the store writes it back as the inline table. A haptic is a string naming a pattern. See [CommonFramework types](#commonframework-types).

A control spanning several keys is a group with a role: ordinary settings, one per key, with the role deciding how the group is drawn (activation sphere; a color in three keys). Each setting is one key, so dirty state, Save and preview need nothing new.

## A choice that sets other settings

A choice whose options set other settings ([schema](schema.md#options-that-set-other-settings)) is a setting whose storage is those settings. Each option has `sets`, the ids of the settings it sets with their values, and `alsoSets`, set the same way by a pick but never read back.

- Its value is the first option whose `sets` all match, or a value that is no option while none matches. A pick sets each of its `sets` and `alsoSets`, so dirty state, Save and preview stay theirs.
- It is dirty while one of them is, and its Discard is theirs. Its default and saved value are the options matching their defaults and saved values.
- The counts of changes skip it: what counts is what Save writes.

## Examples

### A config spec

```json
{
  "minSchemaVersion": 1,
  "modName": "mymod",
  "displayName": "My Mod",
  "files": { "main": { "format": "ini", "root": "myGames", "path": "Mods_Config/MyMod/MyMod.ini" } },
  "defaultFile": "main",
  "applies": "live",
  "content": [
    {
      "type": "switcher", "valueType": "bool", "text": "Holsters", "default": true, "applies": "restart",
      "storage": { "section": "MyMod", "key": "bEnableHolsters" }
    }
  ],
  "pages": [
    {
      "pageDisplayName": "Body",
      "content": [
        {
          "id": "playerHeight", "type": "slider", "valueType": "float", "text": "Player Height", "default": 1.0,
          "valueOptions": { "min": 0.5, "max": 1.5, "step": 0.01 },
          "storage": { "section": "MyMod", "key": "fPlayerHeight" },
          "enabledWhen": { "setting": "bEnableHolsters:MyMod", "equals": true }
        }
      ]
    }
  ]
}
```

The definition the loader makes of it:

```text
Mod "My Mod"
├─ Front page                           the mod's row in the tree
│  └─ Setting bEnableHolsters:MyMod     "Holsters"        bool, switcher
│        storage      bEnableHolsters of [MyMod] in MyMod.ini, default on, applies restart (its own)
└─ Page "Body"                          a row under the mod
   └─ Setting playerHeight              "Player Height"   float, slider from 0.5 to 1.5 by 0.01
         storage      fPlayerHeight of [MyMod] in MyMod.ini, default 1.0, applies live (the mod's)
         enabledWhen  bEnableHolsters:MyMod is on
```

`bEnableHolsters:MyMod` is made from the key and the section, as MCM names a ModSetting. With `fPlayerHeight = 1.1` in the file, no `bEnableHolsters` key, and the slider dragged:

```text
Setting                 In the file   Pending   Dirty   State
bEnableHolsters:MyMod   no key        on        no      ready     the default, as the key is missing
playerHeight            1.1           1.25      yes     ready     previewed to the mod's client
```

Save writes `fPlayerHeight` alone. With `fPlayerHeight = 2.0`, past the most, the setting is invalid, shown as it is, clean, and not previewed.

### An activation sphere

A section of Immersive Flashlight's TOML, shortened:

```toml
[ImFl_BodyActivationSphere]
tZone = { pos = [3.91, 7.11, 0.67], rot = [0.0, 0.0, 0.0], scale = 20.0 }
kPrimaryBinding = { hand = "offhand", type = "tap", button = "trigger", suppress = true }
sEntryHaptic = "Tick"
fIconSize = 2.0
```

The config spec names the section once, as a `cf.activationSphere` item, and the loader makes a group of it, with a setting for each key it shows; keys the config spec hides have none and keep the file's values.

```text
Group "Body grab"                                         role: activation sphere
├─ Setting tZone:ImFl_BodyActivationSphere                string (table), transform control
├─ Setting kPrimaryBinding:ImFl_BodyActivationSphere      string (table), binding control
├─ Setting sEntryHaptic:ImFl_BodyActivationSphere         string, haptic control
└─ Setting fIconSize:ImFl_BodyActivationSphere            float, slider
```

- The role draws them as one group ([ActivationSphere](design-system/project/components/ActivationSphere/README.md)). Its Icons row is a [choice that sets other settings](#a-choice-that-sets-other-settings), the Show keys of the icon and the binding icon.
- A change of the primary binding makes that key alone dirty, previewed, and written by Save. The framework's client previews it as any key: an override for that section and key.
