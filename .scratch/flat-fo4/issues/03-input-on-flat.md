# Input on flat

Type: research
Status: resolved

## Question

How does the framework on flat give ImGui the mouse, wheel, keys and typed text, and take them from the game while a canvas uses them, with what CommonLibF4VR has? For F306 of the plan:

- the cursor's position from the game's menu cursor
- the game's input events, and how a plugin takes one from the game
- clearing held keys when the game clears its input (Alt+Tab)
- typed text, as the framework's window messages give it in VR
- the flags of a menu of the framework's own that shows the cursor and keeps the controls from the player, without pausing the game

## Answer

- **Cursor**: `MenuCursor::cursorPosX/Y` (client pixels) while `registeredCursors` is non-zero, scaled into the canvas's screen rectangle and then its place in the atlas, as `ImGuiPointer` feeds ImGui in VR. The game's cursor draws on top; ImGui draws none.
- **The framework's menu** (DearModdingUI's and PrismaUI's pattern): registered with `UI::RegisterMenu`, `kUsesCursor | kModal | kAssignCursorToRenderer`, under-pause flags, `kAllowSaving | kCompanionAppAllowed`, the `kCursor` input context, `depthPriority` `kConsole`; no `kPausesGame`, no `freezeTime`. It overrides the `IMenu` virtuals whose base calls the game, since their IDs are 1.10.163-only.
- **Buttons, wheel and Esc** from the game's input queue: slot 0 (`PerformInputProcessing`) of `MenuControls`, `PlayerControls` and `PlayerCamera` (+0x38), chained; while a canvas takes input every `ButtonEvent`, `CharacterEvent` and `ThumbstickEvent` is marked `kStop`, and mouse moves pass so the cursor moves. Esc's press and release are stopped too, so the pause menu doesn't open.
- **Keys and typed text** from the window, as in VR: the window subclass works on flat with the window found by `Main::hwnd` instead of `FindWindowA("Fallout4VR")`. `WM_KILLFOCUS` covers Alt+Tab.
- **Not shown anywhere**: that `kStop` and the `kCursor` context keep the player from moving while the game runs; DearModdingUI always pauses. The two unpaused mods (F4SEMenuFramework, PrismaUI) set `ControlMap::ignoreKeyboardMouse`. The first trial checks it.
- **CommonLibF4VR lacks 1.10.984 IDs** for `MenuControls`, `MenuCursor`, `BSInputDeviceManager`, `UI::RegisterMenu`, `BSScaleformManager` and `IMenu`'s base virtuals, and so does upstream. Its flat ID lookup has no exact-match check, so a missing ID silently resolves to the next one.
- **Trap**: `f4vr::isGameMenuTakingInput()` would count the framework's own menu, and the pause menu under the panel, and turn the canvases off.

The choices it brings up are [The panel's input on flat](14-the-panels-input-on-flat.md). Findings: [research/03-input-on-flat.md](../research/03-input-on-flat.md).
