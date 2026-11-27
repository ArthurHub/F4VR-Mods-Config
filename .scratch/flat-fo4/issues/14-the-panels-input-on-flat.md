# The panel's input on flat

Type: grilling
Status: resolved

## Question

What does F306 do, given [Input on flat](03-input-on-flat.md)'s answer?

- keeping the player still while the game runs: `kStop` and the `kCursor` context first, tried in the game; if look or movement leaks, `ControlMap::ignoreKeyboardMouse` while a canvas takes input, or an input-enable layer
- ImGui's keys and typed text from the window, as in VR, with only the mouse buttons, wheel and Esc from the game's input; or keys from the game's input events, as the plan says now
- Esc: only Back (`controls::get().back()`, which lets a field or dropdown take it first), never also ImGui's Escape key
- which game menus turn the canvases off on flat (`f4vr::isGameMenuTakingInput()`): every menu but the framework's own, or only menus above it
- the keyboard's `ClearInputState` hook left out unless a key sticks; the window found by `Main::hwnd`

## Answer

Decided with the user, 2026-11-27.

- The player is kept still by `kStop` and the `kCursor` context first; if the first try shows look or movement leaking while the game runs, `ControlMap::ignoreKeyboardMouse` is set while a canvas takes input, as F4SEMenuFramework and PrismaUI do.
- Keys and typed text come from the window, as in VR, found by `Main::hwnd`; only the mouse buttons, wheel and Esc come from the game's input.
- Esc is only Back (`controls::get().back()`); ImGui never gets its Escape key.
- Only a menu above the framework's own on the menu stack switches the canvases off; the pause menu under the panel and the framework's own menu don't.
- The keyboard's `ClearInputState` hook is left out unless a key sticks.
