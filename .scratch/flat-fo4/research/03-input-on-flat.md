# Research: input on flat (ticket 03, for F306)

Question: how the framework feeds Dear ImGui on flat Fallout 4 (1.10.163, 1.10.984): mouse position, buttons, wheel, keys and typed text. While a canvas uses them, the game must not get them; the game keeps running, and the player can't move or act.

Confidence tags: **[High]** read in source and consistent across sources; **[Medium]** read in one source, or a precedent's claim; **[Low]** inference; **[Try]** only the game can settle it.

## Short answer: what F306 should do

- **Cursor.** Each frame, read `MenuCursor::cursorPosX/Y` while `registeredCursors != 0`. Scale it from client pixels (`GetClientRect(Main::hwnd)`) to render-target pixels, then into the canvas's screen rectangle, then to the canvas's place in the atlas (`displayX/Y + x`, as `ImGuiPointer` already feeds ImGui). Warping the cursor isn't needed, so there's no `SetCursorPos`. ImGui doesn't draw a cursor, because the game's cursor draws on top.
- **A menu of the framework's own** (the DMUI and PrismaUI pattern). It's registered with `UI::RegisterMenu` and loads `Interface/CursorMenu.swf` as a hidden movie. Its flags are `kUsesCursor | kModal | kAssignCursorToRenderer | kAdvancesUnderPauseMenu | kRendersUnderPauseMenu`, plus `kAllowSaving | kCompanionAppAllowed` so the player can still save. It sets `customRendererName = "FlatScreenModel"`, `inputContext = kCursor` and `depthPriority = kConsole`. It has **no `kPausesGame`** and never writes `Main::freezeTime`. It's shown and hidden with `UIMessageQueue` messages from an F4SE UI task, while a canvas takes input. It overrides every `IMenu` virtual whose base calls the game (`ShouldHandleEvent` → `false`, `PassesRenderConditionText`, `ProcessMessage`, `AdvanceMovie`, `PreDisplay`), because those bases use 1.10.163-only IDs.
- **Buttons, wheel and Esc from the game's input queue.** Hook vtable slot 0 (`PerformInputProcessing`) of `MenuControls` (+0), `PlayerControls` (+0) and `PlayerCamera` (+0x38), chained, at game data ready.
  - Mouse `ButtonEvent`s: `idCode` 0–4 are ImGui buttons, 0x800 / 0x900 are wheel +1 / −1.
  - Keyboard `idCode` 0x1B is Back.
  - Events are queued and given to ImGui on the frame thread.
  - While a canvas takes input, every `ButtonEvent`, `CharacterEvent` and `ThumbstickEvent` is marked `kStop` in **every** hooked receiver. `PlayerControls` `data.checkHeldStates` / `setupHeldStatesForRelease` are set once as input is taken. Mouse moves pass through, because they move the game cursor.
- **Keys and typed text stay on the VR path.** The window subclass hands `WM_KEY*` and `WM_CHAR` to `ImGui_ImplWin32_WndProcHandler`. It works on flat once the window is found by `Main::hwnd` instead of `FindWindowA("Fallout4VR")`, and as long as the ImGui frame is built on the main thread. Keys come from the window only, not also from `ButtonEvent`s, so they can't arrive twice. This differs from the plan's wording ("keys from the game's input events"); see open point 2.
- **Alt+Tab.** The existing `WM_KILLFOCUS` release, extended on flat to the mouse buttons. The keyboard `ClearInputState` hook (slot 5) is needed only if a key gets stuck while the window keeps focus.
- **Esc.** In the receiver hook, mark the Esc press and its release `kStop` while a canvas takes input, and queue `controls::get().back()`. `MenuOpenHandler`, which opens the pause menu, then never sees it. `kDisablePauseMenu` on the menu is a possible backstop that nothing has tested yet.
- **To try in the game first:** whether mouse look and movement stay off while unpaused, with only `kStop` and the `kCursor` context. Nothing in the library shows it. The two unpaused precedents use `ControlMap::ignoreKeyboardMouse` instead (open point 1).
- **CommonLibF4VR needs 1.10.984 IDs** for `MenuControls`, `MenuCursor`, `BSInputDeviceManager`, `UI::RegisterMenu`, `BSScaleformManager` and `IMenu`'s base virtuals. Upstream lacks them too, so C1 doesn't bring them. The flat ID lookup also has no exact-match check: an ID missing from the 1.10.984 database resolves silently to the next entry.

## Sources

| Source | Version read | Trust |
|---|---|---|
| DearModdingUI (DMUI), `Modding-Reference/Fallout4/github-repos/gold/DearModdingUI` | `7e09986` (2026-11-18), CommonLibF4 fork `Depends/commonlibf4` `981fb96` | gold; one DLL for 1.10.163 / 1.10.984 / 1.11.240, draws in the UI pass and reads the engine input queue |
| `Analysis/gold/DearModdingUI_RE_REFERENCE.md`, `_MOD_OVERVIEW.md` | same | the library's analysis; checked against the source below |
| F4SEMenuFramework (F4SEMF) | `b031040` | gold; ImGui on the swapchain, WndProc input, `ignoreKeyboardMouse`, unpaused by default |
| PrismaUI flat (`framework-F4-Conversion`) | `d71e84f` | gold; a non-pausing cursor menu (`FocusMenu`) plus `ignoreKeyboardMouse` |
| CommonLibF4VR, `external/F4VR-CommonFramework/external/CommonLibF4VR` | `ad7805e5` (`flat-fo4`); upstream `alandtse/CommonLibF4` `15f504ea` | the library the framework builds on |
| F4VR-CommonFramework | `34a5300` (v0.7.0, `flat-fo4`) | the code F306 changes |
| `Fallout4.exe` 1.10.984 and 1.11.240 on disk (`C:\Games\steamapps\steamapps\common\Fallout 4 - NG`, `...\Fallout 4`) | file versions 1.10.984.0, 1.11.240.0 | a string scan only. VR's `.text` is encrypted on disk (`.bind`, SteamStub), so code references can't be read statically, and the flat exes weren't tried |

No web source was needed: the open questions are about the game's own behavior, which only the game can answer.

## 1. The cursor

**Fields.** CommonLibF4VR `RE/Bethesda/MenuCursor.h` defines `MenuCursor : BSInputEventSingleUser (00), BSTSingletonSDM (20)`, sizeof 0x58:

| Field | Offset | Meaning |
|---|---|---|
| `cursorPosX`, `cursorPosY` | 0x24, 0x28 | the position |
| `minCursorX` … `maxCursorY` | 0x2C–0x38 | bounds |
| `leftConstraintPct` … `bottomConstraintPct` | 0x3C–0x48 | constraints, as shares of the screen |
| `gamepadCursorSpeed` | 0x4C | |
| `registeredCursors` | 0x50 | |
| `forceOSCursorPos` | 0x54 | |

The same layout serves both flat runtimes, with no runtime branch **[High]**. DMUI relies on it unchanged on 1.10.163, 1.10.984 and 1.11.240 (`DearModdingUI_RE_REFERENCE.md`, "Layouts assumed identical") **[Medium]**.

**What the position means.** DMUI treats `cursorPosX/Y` as **client-area pixels** and maps them onto the UI render target with `x * targetWidth / clientWidth` (`include/Platform/rendering/ImGuiPlatformTargets.h` `MapNativeCursorToTarget`; `src/Platform/rendering/PlatformImGuiBackend.cpp` `NativeCursorPositionLocked`, with the client size from `GetClientRect`) **[Medium: DMUI's reading, not checked independently]**. The F306 trial confirms it by logging `cursorPosX` at the right edge beside `GetClientRect`.

**`registeredCursors == 0`** means nothing gives a position. DMUI then doesn't read the cursor and falls back to the OS cursor, `GetCursorPos` + `ScreenToClient` (`PlatformImGuiBackend.cpp` `ApplyCoordinateSpaceLocked`, `SamplePointerLocked`) **[High: read]**. For F306, a canvas takes the cursor only while it's registered.

**`MenuCursor` is itself an input receiver.** `BSInputEventSingleUser` derives from `BSInputEventReceiver` (`BSInputEventSingleUser.h:9`), so it moves the cursor from `MouseMoveEvent`s. Stopping mouse moves would freeze the cursor, which is why DMUI never stops them (`GameInput.cpp` `IsSuppressible`: "Mouse moves drive the game cursor") **[High]**.

**`SetCursorPos`.** It isn't in CommonLibF4VR. DMUI calls it only to warp the cursor for gamepad navigation: `REL::VariantID{ 865628, 2287488 }`, `void(MenuCursor*, int32 x, int32 y)`, "also dispatches the position to registered Scaleform cursors" (`PlatformImGuiBackend.cpp:400`) **[Medium]**. F306 doesn't need it. `CenterCursor` is in CommonLibF4VR (`REL::ID(1107298)`, 1.10.163 only; 1.10.984 is 2287473 in the DMUI fork's `include/RE/IDs.h:2527`).

**When the cursor is shown.** The game opens its `CursorMenu` while a menu with `kUsesCursor` is on the stack. `UI::RefreshCursor` hides it when the topmost menu doesn't use the cursor, unless that menu has `kDontHideCursorWhenTopmost`. This is DMUI's comment on its always-open frame menu, "Else UI::RefreshCursor hides the cursor under us" (`src/Platform/menus/FrameMenu.cpp`) **[Medium: a comment, not read in the binary]**. `kAssignCursorToRenderer` with `customRendererName = "FlatScreenModel"` "explicitly return[s] the shared game cursor from custom menu rendering to screen space" (`HostMenu.cpp`). The Pip-Boy, for example, assigns the cursor to its own renderer **[Medium]**.

**Drawing.** DMUI sets `ImGuiConfigFlags_NoMouseCursorChange`, keeps `io.MouseDrawCursor` false while the game cursor is used, and hides the OS cursor on `WM_SETCURSOR`/`HTCLIENT` (`src/Platform/input/CursorLoader.cpp`) **[High: read]**. The game cursor draws after the canvases because the `CursorMenu` vfunc-7 draw site runs before the cursor (ticket 01).

## 2. A menu of the framework's own: cursor, controls, no pause

What the precedents do:

| | DMUI `DearModdingUI_Menu` (`src/Platform/menus/HostMenu.cpp`) | PrismaUI `FocusMenu` (`src/Menus/FocusMenu/FocusMenu.cpp`) | F4SEMF |
|---|---|---|---|
| Flags | `kUsesCursor`, `kAssignCursorToRenderer`, `kModal`, `kAdvancesUnderPauseMenu`, `kRendersUnderPauseMenu`, plus `kPausesGame` unless FallSouls mode | `kUsesCursor`, `kModal` ("REQUIRED: without it the menu beneath steals mouse input via the game's device layer"), `kAllowSaving`, `kAdvancesUnderPauseMenu`, `kRendersUnderPauseMenu` | no menu of its own |
| `inputContext` | `kCursor` | `kCursor` | — |
| `depthPriority` | `kConsole` | 13 | — |
| Movie | `Interface/CursorMenu.swf` via `LoadMovieEx`, kept invisible, `AdvanceMovie` empty, `ProcessMessage` handles its own name (`HiddenMenu.cpp`) | the same | — |
| Pause | `Main::freezeTime = visible` always (`src/DearModdingUI/host/HostContext.cpp:43`) and `kPausesGame` | only if asked: `UI::freezeFramePause++` (`src/PrismaUI/ViewManager.cpp:247`) | only if `FreezeTimeOnMenu` (shipped `false`): `UI::menuMode += 1` (`src/GameLock.cpp:18`) |
| Controls off | per-event `kStop` (§3) | `ControlMap::SetIgnoreKeyboardMouse(true)` (`ViewManager.cpp:242`) | `ControlMap::ignoreKeyboardMouse = true` (`GameLock.cpp:73`) |

**[High: read]** for every cell.

**Flag meanings.** These come from CommonLibF4VR `IMenu.h:140` `UI_MENU_FLAGS`, the `UI` counters in `UI.h`, and the precedents above.
- `kPausesGame` (bit 0) raises `UI::menuMode` (0x1E0). F4SEMF raises it by hand to pause, so F306 must leave it unset **[High]**.
- `Main::freezeTime` (0x2A) freezes the world. The library already flags that DMUI's "FallSouls mode" still writes it, so DMUI never really runs unpaused (`DearModdingUI_MOD_OVERVIEW.md`, Caveats) **[High]**. F306 must not write it.
- `kAllowSaving` and `kCompanionAppAllowed`: "Missing kAllowSaving or kCompanionAppAllowed blocks saving" (DMUI `FrameMenu.cpp`); compare the `UI::savingDisabled` (0x1F4) and `disablesCompanion` counters **[Medium]**.
- `kDisablePauseMenu` (bit 7) and `UI::pauseMenuDisableCt` (0x1E8): the names suggest a menu with the flag keeps the pause menu from opening. Not one precedent uses it **[Low → Try]**.
- `kUsesMenuContext` (bit 3): no precedent uses it, and its effect is unknown.

**Input context.** `IMenu::inputContext` (0x68) feeds `ControlMap::contextPriorityStack` (`ControlMap.h`, 0x128 on flat). The game is believed to push a menu's context while the menu is on the stack, so keys map to that context's user events. Both precedents set `kCursor` **[Low: inferred from the fields; the push wasn't read in the binary]**.

**How it's opened and closed.** Register with `UI::RegisterMenu(name, create)` and check that `menuMap` kept the registration (DMUI `HiddenMenu::Register`). Then, on an F4SE UI task (`F4SE::GetTaskInterface()->AddUITask`, which CommonLibF4VR has, `F4SE/Interfaces.h:383`), call `UIMessageQueue::AddMessage(name, kShow / kHide)` **[High]**.

**`IMenu` base virtuals call the game.** The game calls a menu's virtuals while routing input and on every UI pass, so a C++ menu that keeps CommonLibF4VR's base versions calls 1.10.163-only IDs on 1.10.984:

| Base virtual | CommonLibF4VR ID (1.10.163) | 1.10.984 (DMUI fork `IDs.h:2329`) |
|---|---|---|
| `ShouldHandleEvent` (`BSInputEventUser` slot 1) | `REL::ID(1241790)` | 2287392 |
| `PassesRenderConditionText` (slot 7) | `REL::ID(937304)` | 2287379 |

F306's menu overrides both: `ShouldHandleEvent` returns `false` because the menu takes nothing through the menu stack, and `PassesRenderConditionText` returns `false` with no draw. The alternative is adding the 1.10.984 IDs **[High: read]**.

**The framework's own test trips on this menu.** `f4vr::isGameMenuTakingInput()` (`src/f4vr/F4VRUtils.cpp:85`) is true when any menu on the stack has `kPausesGame | kUsesCursor | kUsesMenuContext | kModal`. `ImGuiLayer.cpp:692` then takes every canvas off the pointer, so the framework's own menu would turn its canvases off. Opened from the pause menu (M303), `PauseMenu` is on the stack as well **[High: read]**. See open point 4.

## 3. The game's input events, and how a plugin takes one

**Receivers** (CommonLibF4VR):

| Class | Receiver offset | 1.10.163 singleton | 1.10.984 singleton |
|---|---|---|---|
| `UI` | +0 (`UI.h:39`) | `RelocationID(548587, 2689028)` | present |
| `MenuControls` | +0 (`MenuControls.h:19`) | `REL::ID(520890)` | **missing**: 2689089 (DMUI fork `IDs.h:2519`) |
| `PlayerControls` | +0 (`PlayerControls.h:173`) | `RelocationID(544871, 2692013)` | present |
| `PlayerCamera` | +0x38, after `TESCamera` (0x38) (`TESCamera.h:183`) | `RelocationID(1171980, 2688801)` | present |
| `MenuCursor` | +0, through `BSInputEventSingleUser` | `REL::ID(695696)` | **missing**: 2696546 (`IDs.h:2526`) |

`BSInputEventReceiver` has one virtual, `PerformInputProcessing(const InputEvent* queueHead)` in **slot 0**, and `currInputTimeCount` at 0x08; sizeof 0x10 (`BSInputEventReceiver.h`) **[High]**.

**How DMUI hooks them** (`src/Platform/input/GameInput.cpp`):
1. At `kGameDataReady`, take each singleton and check that `static_cast<BSInputEventReceiver*>(obj) - obj` is 0, 0 and 0x38. Fail rather than patch a wrong layout.
2. Read the receiver's vtable, store slot 0's pointer as the original, and swap the slot (`VirtualProtect`, `InterlockedExchangePointer`, `FlushInstructionCache`; `src/Support/Detours.cpp`). It's never unhooked.
3. The hook walks the queue, may set `handled`, then **always calls the original with the real head**.

F4SEMF hooks slot 0 of `PlayerCamera`+0x38 and `PlayerControls` the same way, chain-friendly (`include/Hooks.h:70-93`). Both are released mods that work on 1.10.163 and 1.10.984 **[High]**.

**Which receiver sees what.** F4SEMF reports, "verified in game", that no single receiver carries every device in every state: `PlayerCamera` has the keyboard always but loses the wheel while aiming, and `PlayerControls` has mouse and gamepad but no keyboard (`include/Hooks.h:75-84`) **[Medium]**. DMUI therefore hooks all three and handles each event once, by `InputEvent::timeCode` (0x20), in whichever receiver sees it first ("Receivers poll overlapping queues") **[High: read]**.

**`kStop`.** `InputEvent::handled` (0x24) is `HANDLED_RESULT { kUnhandled, kContinue, kStop }`. "Every engine receiver and menu skips kStop events" (DMUI `GameInput.cpp:171`) **[Medium: DMUI's comment]**.
- DMUI marks an event only at first sight. That stops later receivers only if they share the same event objects and run after the one that marked it, and neither is shown.
- DMUI's evidence for unpaused suppression is weak: its shell always freezes time, so `PlayerControls` is idle anyway.
- F306 should mark in every hooked receiver. Only reading for ImGui is de-duplicated by `timeCode` **[Low: a safer reading of the same mechanism]**.

**Event types** (`InputEvent.h`; no runtime branches; the same in VR apart from `BSInputEventUser`'s three extra VR virtuals) **[High]**:

| Type | sizeof | Fields |
|---|---|---|
| `InputEvent` | 0x28 | `device` 08, `deviceID` 0C, `eventType` 10, `next` 18, `timeCode` 20, `handled` 24 |
| `IDEvent` | 0x38 | `strUserEvent` 28, `idCode` 30, `disabled` 34 |
| `ButtonEvent` (`kButton`) | 0x40 | `value` 38, `heldDownSecs` 3C |
| `MouseMoveEvent` (`kMouseMove`) | 0x40 | `mouseInputX/Y` 38/3C, deltas |
| `CursorMoveEvent` (`kCursorMove`) | 0x40 | `cursorPosX/Y` 38/3C |
| `CharacterEvent` (`kChar`) | 0x30 | `charCode` 28 |
| `ThumbstickEvent` (`kThumbstick`) | 0x48 | |
| `DeviceConnectEvent` (`kDeviceConnect`) | 0x30 | |

`InputEvent::As<T>()` checks `eventType`. `INPUT_DEVICE`: `kKeyboard` 0, `kMouse` 1, `kGamepad` 2 (`InputDevice.h`).

**Codes and edges:**
- **Keyboard** `idCode` is a **Windows virtual-key code**. `BS_BUTTON_CODE::kEscape = 0x1B`, `kLShift = 0xA0`, and so on (`InputEvent.h`); DMUI maps it with `VirtualKeyToImGuiKey` (`src/Platform/input/EngineKeys.cpp`) **[High]**.
- **Mouse** `idCode` 0 is left, 1 right, 2 middle, 3–7 extra. 0x800 is wheel up and 0x900 wheel down, one press per notch with no release (DMUI `ImGuiPlatformTargets.h` `MouseKeyCode`, `PlatformImGuiInput.cpp` "A wheel notch has no release") **[High]**.
- CommonLibF4VR's `BS_BUTTON_CODE::kWheelUp = 0x10800` doesn't match a mouse event's `idCode`, so F306 compares `idCode` directly **[Medium]**.
- **Edges:** just pressed is `value != 0 && heldDownSecs == 0`; held is `value != 0 && heldDownSecs > 0`; released is `value == 0 && heldDownSecs > 0` (`ButtonEvent::QJustPressed/QReleased`). DMUI feeds ImGui edges only, since "Held keys repeat inside ImGui" **[High]**.
- The queue is sorted by code, so a modifier can come after the key it changes. DMUI folds modifiers first (`EventObserver::FoldModifiers`) **[Medium]**. This doesn't matter if keys come from the window (open point 2).

**Threads.** During play, input is processed on the main thread. DMUI acts on a toggle at once when it's "on the UI thread", which is the window's thread. But "Loading polls from job threads; the host is not thread-safe" (DMUI `PlatformImGuiInput.cpp` `RequestToggle`). DMUI feeds ImGui under its own lock (`FeedKey`, `ContextLock`) **[Medium]**. The framework touches ImGui only on the game thread, so F306's hooks queue what they read (a small locked buffer) and the frame drains it before `NewFrame`.

**Releasing held game actions.** "Mirror the engine's menu-mode release, which non-pausing menus skip": when input is taken, DMUI sets `PlayerControls::data.setupHeldStatesForRelease` and `data.checkHeldStates` (`data` at 0x44, fields at +0x49 and +0x48) (`GameInput.cpp` `SuppressGameInput`) **[High: read; the effect is DMUI's claim]**. Without it, a held W would keep walking.

**Keeping the player from moving while unpaused** has three known mechanisms. None is proven for this exact case:

1. **`kStop`, the `kCursor` context and the held-state release** (DMUI style). Mouse moves aren't stopped, so whether the camera still turns while the menu is open is **[Try]**. No precedent runs this way unpaused.
2. **`ControlMap::ignoreKeyboardMouse`** (0x141 flat, 0x139 VR). The guide says "This tells the game engine to stop processing keyboard and mouse input, preventing the player from walking, looking around, or firing" (F4SEMF `PLUGIN_DEVELOPMENT_GUIDE.md:1436`). Two released mods use it unpaused, F4SEMF and PrismaUI **[Medium]**. `ControlMap::GetSingleton` has its 1.10.984 ID already (`RelocationID(325206, 2692014)`). Unknown **[Try]**: whether the receivers still get keyboard and mouse `ButtonEvent`s under it, and whether `MenuCursor` still moves. Both precedents read input from window messages and use the OS cursor instead, and PrismaUI draws its own cursor.
3. **An input-enable layer** (`BSInputEnableManager::AllocateNewLayer` / `EnableUserEvent` with `kMovement | kLooking | kFighting | kActivate | ...`), the engine's own way to disable player controls during scenes. It's 1.10.163-only in CommonLibF4VR (`781703`, `537494`, `1432984`); the DMUI fork has the 1.10.984 IDs (2689007, 2268244, 2268263; `IDs.h:1199`). No precedent uses it for a menu **[Low]**.

## 4. Clearing held keys when the game clears input

- `BSInputDevice` (CommonLibF4VR `BSInputDeviceManager.h`) has these virtuals: 0 `Initialize`, 1 `Poll`, 2 `Shutdown`, 3 `IsConnected`, 4 destructor, **5 `ClearInputState`**. `BSInputDeviceManager::devices[5]` is at 0x08 (`[kKeyboard]` = 0), and `pollingEnabled` at 0x35 **[High]**.
- DMUI swaps slot 5 on the live keyboard device's vtable. Its hook calls the original, then lets go of everything DMUI holds, because "ClearInputState sends no release edges for held keys". It logs `pollingEnabled` beside it (`GameInput.cpp` `HKClearInputState`, `InstallKeyboardDevice`). The library notes say this happens on Alt+Tab and focus loss **[High: read]**.
- `BSInputDeviceManager::GetSingleton` is `REL::ID(1284221)` only; the 1.10.984 ID is 2694300 (`IDs.h:1189`).
- For F306, keys and text come from the window, so the existing `WM_KILLFOCUS` → `Keyboard::releaseKeys` covers Alt+Tab (`ImGuiKeyboard.cpp:291`). On flat it should also let go of the mouse buttons the receiver hook pressed, because the VR version deliberately leaves the wand's button alone (`docs/tech/vr-keyboard.md:124-126`). The slot-5 hook is needed only if the game clears input while the window keeps focus **[Low]**.

## 5. Typed text

**What the framework does now.** `Keyboard::install` (`src/imgui/ImGuiKeyboard.cpp:244-279`):
1. Finds the window with `FindWindowA("Fallout4VR")` (`GAME_WINDOW_CLASS`, line 36).
2. Refuses unless the window's thread is the thread that builds the ImGui frame.
3. Calls `ImGui_ImplWin32_Init(window)` and subclasses the window with `SetWindowLongPtrW(GWLP_WNDPROC)`.

The window procedure (line 281) hands `WM_KEYDOWN`, `WM_KEYUP`, `WM_SYSKEYDOWN`, `WM_SYSKEYUP` and `WM_CHAR` to `ImGui_ImplWin32_WndProcHandler`, then passes every message on. Mouse messages aren't handed on, and `ImGui_ImplWin32_NewFrame` isn't called (`docs/tech/vr-keyboard.md:108-129`) **[High]**.

**On flat, it doesn't work unchanged:**
- **The class is VR's.** The flat `Fallout4.exe` 1.10.984 holds the null-terminated string `Fallout4` once (RVA 0x2362930) and no `Fallout4VR`. VR holds both side by side. So the flat class is most likely `Fallout4` **[Low: string scan only]**. Runtime-independent alternatives:
  - `Main::GetSingleton()->hwnd` (0x30; `RelocationID(756304, 2698043)`, both IDs present), which DMUI uses (`ThreadMessageTap` install, `Main::hwnd`). **[High]**
  - The renderer window's `hwnd`, which PrismaUI uses.
- **The thread.** On flat the window's messages arrive on the main thread, the thread F4SE sends its plugin messages on. DMUI installs its tap at `kPostPostLoad` on that thread, then checks that it is the window's thread and logs an error if not (`PlatformImGuiText.cpp` `VerifyThreadTap`) **[Medium]**. The framework's check holds as long as F303/F305 build the ImGui frame on the main thread. During play the UI pass also runs there, as VR's Submit does (`src/render/README.md:205`) **[Medium]**.
- **Messages reach the window on flat.** F4SEMF reads `WM_KEYDOWN`, `WM_CHAR` and mouse messages from its subclass (`src/Hooks.cpp:592-770`), PrismaUI reads them from a `SetWindowSubclass`, and DMUI's tap sees `WM_KEYDOWN` and `WM_CHAR` **[High]**.

**DMUI's `WH_GETMESSAGE` way** (`src/Platform/input/ThreadMessageTap.cpp`). It has two thread hooks on the window's thread, with the DLL as the hook module (`GetModuleHandleExW(FROM_ADDRESS)`):
- `SetWindowsHookExW(WH_GETMESSAGE)` acts on `PM_REMOVE` only, since "a peek would be seen again". `WM_KEYDOWN` decides whether DMUI claims the next character. `WM_CHAR` / `WM_SYSCHAR` is queued for the frame and, while ImGui wants text, rewritten to `WM_NULL` so the game never sees it. It also decodes DBCS lead and trail bytes for an ANSI window in the input language's code page.
- `WH_CALLWNDPROCRET` handles `WM_SETCURSOR` (hide the OS cursor) and `WM_INPUTLANGCHANGE`.

DMUI chose it to avoid a subclass, which it had before (`GWLP_WNDPROC`, removed since 0.1.3). It privatizes the Win32 backend's names (`IMGUI_CONTEXT` property, window class) so it can't collide with other ImGui plugins (`DearModdingUI_RE_REFERENCE.md` §3–4) **[High: read]**. Nothing found says a subclass fails on flat: F4SEMF still uses one. For F306, keep the subclass, and borrow `WM_CHAR` → `WM_NULL` only if a character typed into a field reaches the game. The receiver hook's `kStop` on `CharacterEvent` should already cover that **[Low]**.

**`CharacterEvent::charCode`** (0x28) also carries typed characters through the game's queue. Neither precedent feeds ImGui from it.

## 6. Esc as Back, with the pause menu kept closed

- `MenuControls` (`MenuControls.h`) holds `handlers` (0x18) and `menuOpenHandler` (0x50). `MenuOpenHandler` opens the pause menu, Pip-Boy and console from the queue. A slot-0 hook runs before the original, so marking the Esc `ButtonEvent` (`device == kKeyboard`, `idCode == 0x1B`) `kStop` there keeps the handler from seeing it **[Medium: DMUI's mechanism and its "kStop is skipped" comment]**.
- DMUI captures the Esc press while its shell is visible, owns it until the release, and stops both (`PlatformImGuiInput.cpp` `ObserveKeyboard`, `ImGuiPlatformTargets.h` `DecideEscapeEdge`). From the pause-menu row, "Escape then closes only DMUI, not the game menu underneath" (`DearModdingUI_MOD_OVERVIEW.md:300`) **[High: read; the behavior is DMUI's claim]**.
- F4SEMF closes its window on `VK_ESCAPE` in the subclass (`src/Hooks.cpp:660`) and relies on `ignoreKeyboardMouse` and on not passing messages on to keep the game out.
- Backstop **[Try]**: `kDisablePauseMenu` on F306's menu, read through `UI::pauseMenuDisableCt`.

## 7. Runtimes: 1.10.163 vs 1.10.984, and what CommonLibF4VR lacks

**Layouts.** CommonLibF4VR has one flat layout for all the types above. Every runtime branch is VR vs flat only:
- `ControlMap` is 0x148 flat and 0x140 VR.
- `BSInputEventUser` has three more virtuals in VR.
- `INPUT_CONTEXT_ID::kCreationClub` exists on flat only.

DMUI runs one binary on 1.10.163, 1.10.984 and 1.11.240 with the same offsets and slots for these types. Its only runtime guard is the `PlayerCamera` receiver-offset check. **No 1.10.163 / 1.10.984 difference is known** **[Medium]**. On 1.11.x, the `UI` fields after `uiTimer` (0x208) move, because `BSTimer` is 0x50 there, not 0x40 (`DearModdingUI_RE_REFERENCE.md`, Runtime Notes). F306 reads none of them.

**Slots.** `PerformInputProcessing` 0, `ClearInputState` 5, `IMenu::PassesRenderConditionText` 7: the same on both flat runtimes (DMUI's single binary) **[Medium]**. Upstream's slot fixes (#53, #56, #57) are in `IVirtualMachine` and `NiObject`, not here.

**IDs.** These are Address Library IDs, the same database for every fork. The 1.10.984 values below come from the DMUI fork's `include/RE/IDs.h` and agree with every 1.10.984 ID CommonLibF4VR already has (`UI` 2689028, `UIMessageQueue` 2689091 / 2284929, `PlayerControls` 2692013, `PlayerCamera` 2688801, `ControlMap` 2692014, `Main` 2698043) **[High]**.

| Needed by F306 | CommonLibF4VR now | Upstream | 1.10.984 ID to add |
|---|---|---|---|
| `MenuControls::GetSingleton` | `REL::ID(520890)` | same | 2689089 |
| `MenuCursor::GetSingleton` | `REL::ID(695696)` | same | 2696546 |
| `BSInputDeviceManager::GetSingleton` (only for the slot-5 hook) | `REL::ID(1284221)` | same | 2694300 |
| `UI::RegisterMenu` | `REL::ID(1519575)` | same | 2284766 |
| `UI::RefreshCursor` (if called) | `VariantID(1436639, 0, VR)` | `ID(1436639)` | 2284772 |
| `BSScaleformManager::GetSingleton` (hidden movie) | `REL::ID(106578)` | same | 2689600 |
| `LoadMovieEx`'s `FileUncacheOnMenuOpen` setting (`src/RE/Bethesda/BSScaleformManager.cpp`) | `REL::ID(454316)` | same | 2667999 |
| `IMenu::ShouldHandleEvent`, `PassesRenderConditionText` (if not overridden) | `ID(1241790)`, `ID(937304)` | same | 2287392, 2287379 |
| `BSInputEnableManager` (only for mechanism 3) | `ID(781703)`, `ID(537494)`, `ID(1432984)` | same | 2689007, 2268244, 2268263 |
| `MenuCursor::SetCursorPos` (not needed) | absent | absent | 865628 / 2287488 |
| `PlayerControls`, `PlayerCamera`, `UI`, `UIMessageQueue`, `Main`, `ControlMap` singletons | present | present | — |

**The silent-lookup trap.** CommonLibF4VR's `IDDB::id2offset` (`src/REL/IDDB.cpp`, the same in upstream) uses `lower_bound` and checks `it->id == a_id` **only on VR**. On flat, an ID missing from the 1.10.984 database returns the offset of the next ID. That's a wrong address with no error, where DMUI's library fails loudly (`commonlib-shared/src/REL/IDDB.cpp:441`).
- The 1.10.984 database does carry some 1.10.163 IDs: DMUI resolves the `CursorMenu` vtable with `REL::ID(245579)` on every runtime under that strict check **[High]**.
- Whether it carries the 1.10.163 singleton IDs above is unknown, and every fork that targets 1.10.984 uses the separate 1.10.984 IDs instead.

## Open points that need a decision

1. **What keeps the player from moving while unpaused.** Nothing in the library shows `kStop` plus the `kCursor` context doing it with the game running (§3). Let the first F306 trial (panel shown as the game loads) check walking, mouse look, firing, the Pip-Boy key and the console key. If look or movement leaks, there are two ways to go. One adds `ControlMap::ignoreKeyboardMouse` while a canvas takes input, after checking that the receivers and `MenuCursor` still get the mouse under it. The other uses an input-enable layer (needs 1.10.984 IDs). Which is the fallback?
2. **Where ImGui's keys come from.** The plan's F306 says "the buttons, wheel and keys from the game's input events".
   - This research suggests keys and text from the window, as in VR, and only buttons, wheel and Esc from the game: one key path, no modifier folding, and the VR code unchanged apart from the window lookup.
   - The other way is to feed ImGui keys from `ButtonEvent`s (DMUI's way) and keep the window for `WM_CHAR` only.
   - Which does the plan say?
3. **Esc for ImGui and for Back.** If Esc both goes to ImGui as `ImGuiKey_Escape` (through the subclass) and is Back, one press may revert a field and also go back a page. Decide whether flat drops Esc from what ImGui gets and makes it only `controls::get().back()`, which already lets a field or dropdown handle Back first.
4. **`f4vr::isGameMenuTakingInput()` on flat.** It would count the framework's own menu, and `PauseMenu` when opened from the pause row (M303), and turn the canvases off. Options:
   - Skip the framework's own menu by name, and on flat count only menus above it on the stack.
   - A flat-only test.
5. **CommonLibF4VR's 1.10.984 IDs and the lookup check.** The IDs in §7 have to be added, in C1 or in a CommonLibF4VR commit before F306; upstream lacks them. Making the flat lookup exact (fail on a missing ID) would catch the rest. Both belong to CommonLibF4VR, and may already come up from ticket 01.
6. **The `ClearInputState` hook.** It's needed only if a key or button sticks while the window keeps focus. Leave it out of F306 unless the trial shows a stuck key?
7. **The window lookup.** `Main::hwnd` (both IDs present, what DMUI does), or the flat class name `Fallout4` beside `Fallout4VR` (unverified)?
