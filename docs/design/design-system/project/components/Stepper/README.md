# Stepper

Two kinds: a numeric stepper (minus, value, plus) where precision matters more than range, and a choice stepper (previous, label, next) for a short list.

**Provides:** numeric: value, min, max, step. Choice: options with labels and the current index.

- Buttons are 48 x 48 icon buttons with drawn minus, plus or 14 px triangles.
- A button under the ray is `hover` with a `green` outline, and `pressed` with a `glow` outline while the trigger is held. A choice stepper's buttons stop at the first and the last option, where the button is disabled.
- A press on the rest of a choice stepper's row goes to the next option, and from the last to the first.
- A push of the thumbstick to a side steps the choice stepper whose row the ray is on, once for a push.
- Numeric: the value box is `value-width`, centered digits. Choice: the label box is 280 px, centered text.
- Pending: the value or label in `amber`.
