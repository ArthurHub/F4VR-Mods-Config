# Controls and Setting Types

This document defines the initial control vocabulary and interaction rules. The order the controls are built in is in the [Mod work plan](mod-plan.md).

## Separation of concerns

Every setting has four distinct concepts:

- **Type**: the value stored by the target mod. A package's `valueType`.
- **Control**: how Mods Config VR presents and edits that value. A package's `type`, as in an MCM config.
- **Storage**: where the value is read from and written to. A package's `storage`.
- **Application policy**: when a change takes effect in the running mod: as it is made, after Save, or after a restart. A package's `applies`.

Keeping these separate prevents the file format, widget, and runtime behavior from becoming unnecessarily coupled.

## Core stored types

The declarative model should support these scalar types:

- `bool`
- `int`
- `float`
- `string`
- `color`

`int` is a signed 32-bit integer and `float` is a 32-bit float.

A choice does not need a separate storage type. Its options may store typed boolean, integer, floating-point, or string values.

`vec2` and `vec3` are intentionally excluded for now. Complex CommonFramework values are represented by dedicated controls rather than generic vectors.

## Core interactive controls

### Switcher

Primary use: boolean settings.

- The entire row is a large target.
- The current state is shown in both shape/text and color.
- Trigger toggles the value.
- Reset restores the declared default.

### Slider

Primary use: bounded integer and floating-point settings.

Suggested row:

```text
[ - ]  [================o--------]  [ 1.25 ]  [ Reset ]
```

- Trigger-drag gives continuous preview when the setting supports it.
- Releasing the trigger ends the control interaction; it does not Save the page.
- Back during a drag restores the value from the beginning of that drag.
- Thumbstick left/right changes the slider the ray is on by the declared step, once the ray is on its row for a short time. A slider is not selected first.
- Holding the thumbstick may accelerate after an initial repeat delay.
- The value box only shows the value: it is not typed.
- The value has the decimals, the unit or the percentage of its `display`, and stays visible.
- Initial schema support is linear. Logarithmic or curved mappings can be considered later.
- Haptics should mark meaningful interaction boundaries, not fire on every tiny value change.

### Dropdown and stepper

Primary use: one value selected from a finite set.

They are two types, as in MCM:

- `dropdown`: a popup list
- `stepper`: previous and next

The host can choose a more compact presentation in Focused mode, where a stepper shows an option's short text when it has one.

Choice behavior:

- Each option has a typed stored value, a text, and an optional short text and help.
- The current option has a strong visual marker.
- Popup rows use large VR targets.
- Trigger selects; thumbstick scrolls; Back closes without changing the selection. So does a press beside the list, which does nothing else.
- A press anywhere on the row of a dropdown opens its list.
- The buttons of a stepper stop at the first and the last option. A press on the rest of its row goes to the option after, and from the last to the first.
- Thumbstick left/right steps the stepper the ray is on, one option for a push.
- The help line may show the pointed option's help before it is selected.

### Text input

Primary use: short strings, and numbers that are typed. A `textinput` whose `valueType` is int or float takes a number, as MCM's `textinputInt` and `textinputFloat` do, and is typed on the same keyboard. Its limits are the `min` and `max` of its `valueOptions`.

The keyboard is SteamVR's, which the game types its own names with, and it is shown while the field is edited. A physical keyboard types into the field too, and so does a keyboard overlay that types into the game's window. A runtime with no keyboard, as OpenComposite, has those only. The design is the framework's [VR keyboard](../../external/F4VR-CommonFramework/docs/tech/vr-keyboard.md).

- `Done` validates and commits the edit to the pending setting value.
- `Cancel`, the keyboard's close button, restores the value from before text editing began.
- Invalid temporary text is not sent as a preview; the last valid preview remains active.

Multiline text, password fields, text selection tools, and speech input are not part of the initial design.

### Color

Primary use: a CommonFramework color, `{ rgb = [r, g, b], alpha = a }`, as a light's or a pointer's. MCM's menu has no color control.

- The row shows the color as a swatch, with its alpha over a checkerboard, and as its numbers: "190, 255, 190, alpha 200". A press on the row opens the editor.
- The editor is a popup in the content pane: a picker for the hue, saturation and brightness, dragged with the trigger, and a slider for each of red, green, blue and alpha. The thumbstick steps the slider the ray is on by 1, which is how a color is set exactly.
- It shows the color it had when it opened beside the color as it is, and a press on the first goes back to it.
- Each change sets the pending value as it is made, and a `live` color is previewed as it is dragged. Done, or a press beside the editor, keeps it. Cancel and Back give back the color it had when it opened.
- The value shown is the one the framework reads: the keys the value has, over the default's channels. A changed value is written as whole numbers, with the keys it had and the key of the part that changed, so `{ alpha = 60 }` with a new hue is `{ rgb = [255, 190, 120], alpha = 60 }`.
- A value the framework cannot read is invalid, shown as the file has it, with the framework's reason in the help line.
- A color whose opacity means nothing, as a light's, is RGB only: `"valueOptions": { "alpha": false }`. Its swatch is opaque, its numbers have no alpha, its editor has no Alpha slider, and only `rgb` is written. An `alpha` the file has is kept as it is.
- In the Focused panel the editor has no picker: its sliders fill the panel.

The look is the design system's [Color](design-system/project/components/Color/README.md).

### Button

Primary use: an operation that does not directly edit a stored value.

- A button runs what an MCM button runs, such as a function of a script, a console command, or a function that the mod's native client registered, by its name and with typed parameters if needed.
- Its button says `Run`, or the button text its package gives it.
- Potentially destructive actions require explicit confirmation.
- Long-running progress and cancellation behavior remain open design work.

### Read-only

Primary use: a value the mod writes for the player to read, as a measured height.

- A setting that is read-only shows its value in its control, and its whole row is disabled, as an MCM hotkey's is: a press does nothing, and the row has no look of one.
- The help line says first that it is read-only: `(Read-only)`.
- It has no Reset.

## Layout and informational elements

These elements do not directly store values:

- Section heading, with the rows of its section
- Explanatory text
- Spacer
- Image or preview asset

A package's section can hold sections, and can fold. A press on the heading of a section that folds hides its rows and shows them again, and its heading has a mark of which it is. A folded section with an invalid value or an unsaved change inside it marks its heading too. See [Sections](schema.md#sections).

Detailed settings are a folded section right after the settings they belong to, so they do not overwhelm ordinary players.

## CommonFramework-specific controls

The following exact CommonFramework concepts are reserved as first-class controls:

- `Haptic`
- `Color`
- `InputBinding`
- `Transform`
- `ActivationSphere`

The framework's TOML config already fixes how each is stored, and the type prefix of a key says which it is:

| Control            | Key prefix | Stored as                                                                                                                                                                                                                                       |
| ------------------ | ---------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `Haptic`           | `s`        | A string: the name of a pattern of the framework's library, or `"none"`. `sEntryHaptic = "Tick"`                                                                                                                                               |
| `Color`            | `c`        | An inline table, `{ rgb = [r, g, b], alpha = a }`, each a whole number in 0..255. Either key can be left out and keeps the default's channels, so `{ alpha = 60 }` changes only the opacity                                                   |
| `InputBinding`     | `k`        | An inline table, `{ hand = "offhand", type = "tap", button = "grip" }`, with `duration`, `suppress` and `modifier = { button, hand }` optional. `{ hand, type = "axis", direction, axis, threshold }` for a thumbstick or an axis, and `{ type = "none" }` for none |
| `Transform`        | `t`        | An inline table, `{ pos = [x, y, z], rot = [heading, roll, attitude], scale = s }`, the rotation in degrees, each key optional                                                                                                                 |
| `ActivationSphere` |            | A whole section: the zone `tZone` and an optional power-armor zone `tZonePA`, `kPrimaryBinding` and `kSecondaryBinding`, `sEntryHaptic`, `sPrimaryHaptic` and `sSecondaryHaptic`, and the keys of its sphere and its icon                  |

A table is one value in one key. In the settings model its value is a string, the TOML text of the table (`{ rgb = [255, 51, 51], alpha = 40 }`), which its control reads and writes, and which the store writes into the file as the inline table, not as a quoted string. So its setting is dirty, previewed and saved as a whole, as any other. The framework checks every key and word of a table and names the one that is wrong, and a table it cannot read is an invalid value with that reason.

The Haptic control is designed below, and Color above. The schemas, storage mappings, live-preview semantics, and VR manipulation experiences of the others will be designed separately; [Open questions](open-questions.md#commonframework-specific-controls) lists what is left.

These should not be flattened into `vec2` or `vec3`, because their meaning, validation, and editing affordances are richer than a tuple of numbers.

### Haptic

Primary use: the pattern a mod plays on a controller when something happens, as an activation sphere's `sEntryHaptic`, or the menu's own `sPressHaptic`.

- The value is one of None and the 18 patterns of the framework's library (`vrcf::HapticPattern`), in the library's order. The menu knows the list, with a text and a help for each; a package names only the setting's key and its default.
- It is a dropdown in the Full panel, and a choice stepper in the Focused panel, as any choice. Each option shows its pattern's shape, drawn from the framework's keyframes (`VRControllersHaptic::getPattern`), so patterns can be told apart before one is felt.
- A pattern is felt and not seen, so the control plays it on the hand that points: once when an option is picked, at each step of the stepper, and when the Play button beside the control is pressed. A press that plays a pattern gets no press pulse of the menu: the pattern is its feedback.
- Pointing at an option plays nothing, as hover has no haptic. The help line shows the option's help: how it feels, its length, and what it is for.
- Reset sets the default and does not play it. Play plays it.
- A text in the file is matched as the framework reads it: case-insensitive, with spaces, underscores and hyphens ignored, so `double click` is Double click, and `off` and an empty text are None. A changed value is written as the library names it, `DoubleClick`, or `none`.
- A name the library does not have is invalid, shown as the file has it. The framework plays nothing for it, and the help says so.
- A `live` haptic is previewed as any setting: the mod plays the pending pattern when its event happens.

The look is the design system's [Haptic](design-system/project/components/Haptic/README.md).

## Help and validation feedback

The full and focused layouts reserve a stable place for the help of the item the ray is on: the help line on the left of the Full panel's footer, and a two-line area at the top of the Focused panel. See [Help line](ui-and-vr-interaction.md#help-line). The same place presents validation errors so content does not jump around.

Additional feedback:

- Invalid rows receive a clear marker and concise inline message.
- A row whose warning holds has a warning mark, and the help line shows the warning before its help. A warning does not block Save. See [Warnings](schema.md#warnings).
- Save is blocked while a dirty setting is invalid. An invalid value that was already in the file does not block it.
- Attempting Save shows a summary and moves focus to the first invalid setting that is visible.
- The summary names every invalid setting, including one that a condition currently hides or disables. Such a setting can be reset from the summary.
- Validation must not rely on color alone.

## Defaults and reset

Every editable declarative setting must declare its default beside the setting definition. A read-only setting has no Reset, and needs none.

- Each setting can be reset independently.
- A page or section may expose Reset All when unambiguous.
- Reset changes the pending value and participates in preview like any other edit.
- Reset is not persisted until Save.
- Discard restores the values from the start of the editing session, including any values changed through Reset.

## MCM controls

The MCM bridge should preserve the existing MCM control vocabulary and behavior as faithfully as practical instead of translating every control into a more powerful native concept. MCM's controls are not marked as MCM's. A page whose changes are saved as they are made says so.
