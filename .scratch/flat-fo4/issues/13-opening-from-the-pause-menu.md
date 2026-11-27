# Opening from the pause menu

Type: grilling
Status: resolved

## Question

How does the `Mods Config` row in the pause menu behave, given [Pause-menu row](04-pause-menu-row.md)'s answer?

- the press closes the pause menu and opens the panel over the running game, or opens the panel over the pause menu, which stays open and keeps the game paused (DearModdingUI's way; an MCM hotkey change then applies at once, [Binding an MCM hotkey on flat](12-binding-an-mcm-hotkey-on-flat.md))
- what closing the panel returns to: the game, or the pause menu again
- the label: MCM's row in the same list reads "MOD CONFIG"
- the place: above Settings, a slot in the mod's config, or in the Settings submenu
- a row on the title screen too, which has the same movie, or the pause menu only

## Answer

Decided with the user, 2026-11-27.

- The press opens the panel over the pause menu, which stays open and keeps the game paused; closing the panel returns to it. An MCM hotkey change then applies at once.
- The row reads `MODS CONFIG FO4`, plainly not MCM's `MOD CONFIG`.
- It sits above Settings: a fixed place, with no setting, that shifts none of the rows the game greys out.
- The pause menu only: no row on the title screen, as VR has no menu there.
