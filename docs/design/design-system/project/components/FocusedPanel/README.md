# FocusedPanel

A smaller panel on the offhand that shows the selected page only, so the player can change a setting while looking at its effect.

**Provides:** the same editing session as Full: page, store, pending count.

- Canvas `focused-width` x `focused-height` (12 x 9 units). At scale 1.6, about 45 cm from the eyes, its text is the same angular size as the Full panel's. Tune both in the headset.
- Same chrome as Full: `glass`, `border-panel`, `radius-panel`.
- Header: the mod name in `t-body`, then **Full** (returns to the Full panel) and Minimize.
- A two-line help area at the top (`description-height`, on `plate`) in the "Name: help" form. The footer is too narrow to hold help here.
- Each setting stacks: the name (with its marks after it) on one line, the control on the next at full width with the Reset slot. A dropdown is shown as a choice `Stepper`, so no popup opens on the hand.
- Footer: the pending count and **Discard** / **Save** at 128 px.

Don't drop descriptions, validation or Save here: Focused is the same session, only smaller.
