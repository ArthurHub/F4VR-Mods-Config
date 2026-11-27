# Binding an MCM hotkey on flat

Type: grilling
Status: resolved
Blocked by: 03, 13

## Question

How does the menu bind an MCM hotkey on flat, given [MCM `SetKeybind` on flat](05-mcm-setkeybind-on-flat.md)'s answer: MCM takes a key only through the pause menu's movie, while the pause menu is open?

- what F10 during play does, with the pause menu closed: queue the change until the pause menu next opens, or bind only from the pause menu ([Opening from the pause menu](13-opening-from-the-pause-menu.md) settles whether the pause menu is open under the panel)
- what the player sees when the key is taken from another hotkey, which MCM does without a word
- keeping the pressed key from reaching MCM and the game while it is being bound ([Input on flat](03-input-on-flat.md))
- binding before a save has loaded: refused, or queued

## Answer

Decided with the user, 2026-11-27.

- Hotkeys are bound only while the panel is open over the pause menu. Opened by F10, the rows are read-only, with a hint to open Mods Config from the pause menu. No queue.
- A key another hotkey holds: the row shows which hotkey holds it, and Save takes it from that one, as MCM does.
- Before a save has loaded this session the rows are read-only, with a hint, since MCM's key table is still empty.
- A row takes the next key pressed with its modifiers; Esc cancels and a button clears the key. While it waits, the key goes to neither MCM nor the game.
