# The panel's input on flat

Type: grilling
Status: open

## Question

What does F306 do, given [Input on flat](03-input-on-flat.md)'s answer?

- keeping the player still while the game runs: `kStop` and the `kCursor` context first, tried in the game; if look or movement leaks, `ControlMap::ignoreKeyboardMouse` while a canvas takes input, or an input-enable layer
- ImGui's keys and typed text from the window, as in VR, with only the mouse buttons, wheel and Esc from the game's input; or keys from the game's input events, as the plan says now
- Esc: only Back (`controls::get().back()`, which lets a field or dropdown take it first), never also ImGui's Escape key
- which game menus turn the canvases off on flat (`f4vr::isGameMenuTakingInput()`): every menu but the framework's own, or only menus above it
- the keyboard's `ClearInputState` hook left out unless a key sticks; the window found by `Main::hwnd`
