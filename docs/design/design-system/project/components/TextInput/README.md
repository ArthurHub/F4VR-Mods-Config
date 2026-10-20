# TextInput

A `textinput` setting: a field that shows its text, typed on SteamVR's keyboard. A `textinput` whose `valueType` is `int` or `float` is a typed number, as MCM's `textinputInt` and `textinputFloat`.

**Provides:** the value, its default, its state, and its limits: `allowEmpty`, `minLength` and `maxLength` for a text, `min` and `max` for a number.

## The row

- The field is 392 x 48, as wide as a dropdown's: `track` with a `frame-border` outline. The text in `t-body`, `space-4` in, left-aligned and ending in an ellipsis where it is long; a number right-aligned in tabular digits, written as its `display` says. A drawn pencil, 18 px in `green-muted`, at the right says it is typed. Then the Reset slot.
- An empty text says `Empty` in `green-muted`.
- A press anywhere on the row starts typing.
- Pending: the text in `amber`, the pending dot after the name. Invalid: a `red` outline, the text in `red`, the invalid mark after the name, and the reason in the help line ("Invalid: the text has 40 characters, and can have at most 32.", "Invalid: 250 is not from 1 to 100.").

## Typing

- The field takes a `green` outline and a caret at the end of the text, and the row is active. SteamVR's keyboard is shown, where SteamVR puts it, and its keys reach the field as the game window's do, so the field changes as each key is typed. A physical keyboard types into it too.
- With a `maxLength`, the pencil's place has the count, `12 / 32`, in `t-detail` `green-muted`, and a key past it is not taken.
- The help line says how it ends: "Typing: Done keeps it, Cancel puts back “Sanctuary Hills”." and then the limits.
- The keyboard's Done keeps the text as the pending value, as Enter and a press beside the field do. Cancel, the keyboard's close button, Escape or Back puts back the text from before typing.
- A text that is not valid is kept and shown invalid, so the player sees why; it is not sent as a preview, and the last valid one stays previewed.
- A number takes the keys of a number only. Its text is read when typing ends: one that is not a number is invalid ("Invalid: "4.5" is not a whole number.").
- A runtime with no keyboard, as OpenComposite, types with a physical keyboard only, and the help line says so while typing.

## Focused panel

The same field, the width of the panel, and the same keyboard.

The behavior is in [Text input](../../../../controls.md#text-input).
