# Hotkey

An MCM hotkey: the key MCM has bound to it, shown in a box and not edited.

**Provides:** the row's text and help, the name of its key as MCM's menu names it (`Ctrl-Shift-F5`) or `None`, and the reason for a hotkey that is not available.

- Laid out as a setting's row: the text, and the key in a box that ends where a control ends, before the Reset slot. The Reset slot is kept empty, so the box lines up with the other controls: a hotkey has no default here.
- The box is `control-height` tall, `track` with a `frame-border` outline, and has the key's name in `t-body` `green` in its middle. It is as wide as the name with `space-3` on each side, and at least 96 px (`KEY_MIN_WIDTH` in `ModPage.cpp`, not a token), so a single key such as `F` is not a small square.
- A hotkey with no key says `None`.
- The whole row is always disabled (`BeginDisabled`, `opacity-disabled`): MCM binds the key, in its own menu, and runs the hotkey. A press does nothing, and the row has no pressed look.
- While the ray is on it, it has the hover look at full strength, `hover` with a `frame-border` outline, drawn before `BeginDisabled`, so its help is seen to be its. A [read-only](../ReadOnly/README.md) setting's row does the same.
- Its help has `(Read-only)` first: "Toggle the flashlight: (Read-only) Turns the flashlight on and off. Bind a key to it in MCM's menu." A hotkey with no help has `(Read-only)` alone.
- Not available, with no `id` or with one the mod's `keybinds.json` does not have, so that MCM binds no key to it: it looks the same, with `None`, and its help is the reason alone: "Not available: the hotkey strobe is not in the keybinds.json of the mod."
- The key is read from MCM's files as the mod is selected. MCM writes the player's file as its own menu closes, so a key bound there shows the next time the mod is selected.

Don't offer to bind a key here, or to bind a button of a VR controller: MCM runs a hotkey for a key of the keyboard, the mouse or a gamepad. The behavior is in [MCM support](../../../../mcm.md#rows).
