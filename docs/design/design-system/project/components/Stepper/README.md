# Stepper

Two kinds: a numeric stepper (minus, value, plus) where precision matters more than range, and a choice stepper (previous, label, next) for a short list.

**Provides:** numeric: value, min, max, step. Choice: options with labels and the current index.

- Buttons are 48 x 48 icon buttons with drawn minus, plus or 14 px triangles.
- Numeric: the value box is `value-width`, centered digits. Choice: the label box is 280 px, centered text.
- Pending: the value or label in `amber`.
- The Focused panel uses the choice stepper for every choice, so no popup opens on the hand.
