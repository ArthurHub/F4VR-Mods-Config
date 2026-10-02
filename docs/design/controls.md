# Controls and Setting Types

This document defines the initial control vocabulary and interaction rules. It does not decide which controls must ship in v1.

## Separation of concerns

Every setting has four distinct concepts:

- **Type**: the value stored by the target mod.
- **Control**: how Mods Config VR presents and edits that value.
- **Binding**: where the value is read from and written to.
- **Application policy**: whether the value supports preview, requires Save, or requires a reload/restart.

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

### Toggle

Primary use: boolean settings.

- The entire row is a large target.
- The current state is shown in both shape/text and color.
- Trigger toggles the value.
- Reset restores the declared default.

### Numeric slider

Primary use: bounded integer and floating-point settings.

Suggested row:

```text
[ - ]  [================o--------]  [ 1.25 ]  [ Reset ]
```

- Trigger-drag gives continuous preview when the setting supports it.
- Releasing the trigger ends the control interaction; it does not Save the page.
- Back during a drag restores the value from the beginning of that drag.
- Thumbstick left/right changes by the declared step.
- Holding the thumbstick may accelerate after an initial repeat delay.
- The value field opens the numeric keyboard for precise entry.
- Units and an appropriate number of decimal places remain visible.
- Initial schema support is linear. Logarithmic or curved mappings can be considered later.
- Haptics should mark meaningful interaction boundaries, not fire on every tiny value change.

Out-of-range typed input is a validation error. It should not be silently clamped because silent correction hides configuration mistakes.

### Numeric input or stepper

Primary use: numeric values where precision matters more than scanning a range.

- Shows decrement, formatted value, and increment targets.
- May share the same minimum, maximum, step, units, and validation model as a slider.
- The value target opens the numeric keyboard.

### Choice

Primary use: one value selected from a finite set.

Possible presentations:

- Dropdown or popup list
- Previous/next stepper
- Radio-style list for a small number of choices

The schema may request a presentation, but the host can choose a more suitable compact presentation in Focused mode.

Choice behavior:

- Each option has a typed stored value, label, and optional description.
- The current option has a strong visual marker.
- Popup rows use large VR targets.
- Trigger selects; thumbstick scrolls; Back closes without changing the selection.
- The description area may show the pointed option's description before it is selected.

### Text input

Primary use: short strings.

Dear ImGui supplies text-editing widgets and accepts character/key events, but it does not supply a VR on-screen keyboard. How that keyboard works is not designed yet; see [Open questions](open-questions.md#text-input). What is settled:

- `Done` validates and commits the edit to the pending setting value.
- `Cancel` restores the value from before text editing began.
- Invalid temporary text is not sent as a preview; the last valid preview remains active.
- Physical keyboard input, when available, takes the same path as the on-screen keyboard.

One constraint is known. A focused ImGui `InputText` keeps its own copy of the text and ignores outside changes to its buffer, so key presses from an on-screen keyboard have to reach it as ImGui character and key events.

Multiline text, password fields, text selection tools, and speech input are not part of the initial design.

### Color

Primary use: RGB or RGBA colors.

- Show a large swatch and readable numeric representation.
- Support precise numeric entry as well as an appropriate VR-friendly picker.
- Exact picker geometry and storage encodings remain to be designed.

### Action

Primary use: a named operation that does not directly edit a stored value.

- Actions are declared by name, with typed parameters if needed.
- Potentially destructive actions require explicit confirmation.
- Arbitrary function pointers, console commands, and script strings are not accepted from declarative packages.
- Long-running progress and cancellation behavior remain open design work.

## Layout and informational elements

These elements do not directly store values:

- Section heading
- Collapsible section
- Advanced section
- Explanatory text
- Notice, warning, or error
- Separator
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

## Descriptions and validation feedback

The full and focused layouts reserve a stable description area for the item currently pointed at or focused. The same area presents validation errors so content does not jump around.

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

The MCM bridge should preserve the existing MCM control vocabulary and behavior as faithfully as practical instead of translating every control into a more powerful native concept. MCM-originated controls are visually identified, especially where their changes apply immediately.
