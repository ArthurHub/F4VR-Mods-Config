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
- The value field opens the keyboard for precise entry.
- Units and an appropriate number of decimal places remain visible.
- Initial schema support is linear. Logarithmic or curved mappings can be considered later.
- Haptics should mark meaningful interaction boundaries, not fire on every tiny value change.

Out-of-range typed input is a validation error. It should not be silently clamped because silent correction hides configuration mistakes.

### Dropdown and stepper

Primary use: one value selected from a finite set.

They are two types, as in MCM:

- `dropdown`: a popup list
- `stepper`: previous and next

A radio-style list for a small number of choices can be a third type later. The host can choose a more compact presentation in Focused mode.

Choice behavior:

- Each option has a typed stored value, a text, and an optional help.
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

Primary use: RGB or RGBA colors.

- Show a large swatch and readable numeric representation.
- Support precise numeric entry as well as an appropriate VR-friendly picker.
- Exact picker geometry and storage encodings remain to be designed.

### Button

Primary use: an operation that does not directly edit a stored value.

- A button runs what an MCM button runs, such as a function of a script, or a function that the mod's native client registered, by its name and with typed parameters if needed.
- Potentially destructive actions require explicit confirmation.
- Long-running progress and cancellation behavior remain open design work.

## Layout and informational elements

These elements do not directly store values:

- Section heading
- Collapsible section
- Advanced section
- Explanatory text
- Spacer
- Image or preview asset

Advanced content should be collapsible and visually secondary so detailed configuration does not overwhelm ordinary players.

## CommonFramework-specific controls

The following exact CommonFramework concepts are reserved as first-class controls:

- `ActivationSphere`
- `InputBinding`
- `Haptic`
- `Transform`

Their schemas, storage mappings, live-preview semantics, and VR manipulation experiences will be designed separately; [Open questions](open-questions.md#commonframework-specific-controls) lists what the framework already defines for each.

These should not be flattened into `vec2` or `vec3`, because their meaning, validation, and editing affordances are richer than a tuple of numbers.

## Help and validation feedback

The full and focused layouts reserve a stable place for the help of the item the ray is on: the help line on the left of the Full panel's footer, and a two-line area at the top of the Focused panel. See [Help line](ui-and-vr-interaction.md#help-line). The same place presents validation errors so content does not jump around.

Additional feedback:

- Invalid rows receive a clear marker and concise inline message.
- Save is blocked while a dirty setting is invalid. An invalid value that was already in the file does not block it.
- Attempting Save shows a summary and moves focus to the first invalid setting that is visible.
- The summary names every invalid setting, including one that a condition currently hides or disables. Such a setting can be reset from the summary.
- Validation must not rely on color alone.

## Defaults and reset

Every editable declarative setting must declare its default beside the setting definition.

- Each setting can be reset independently.
- A page or section may expose Reset All when unambiguous.
- Reset changes the pending value and participates in preview like any other edit.
- Reset is not persisted until Save.
- Discard restores the values from the start of the editing session, including any values changed through Reset.

## MCM controls

The MCM bridge should preserve the existing MCM control vocabulary and behavior as faithfully as practical instead of translating every control into a more powerful native concept. MCM's controls are not marked as MCM's. A page whose changes are saved as they are made says so.
