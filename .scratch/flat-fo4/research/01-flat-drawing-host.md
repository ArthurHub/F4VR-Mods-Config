# Research: the flat drawing host

For [ticket 01](../issues/01-flat-drawing-host.md), F304 and F305 of [the plan](../../../docs/design/flat-fallout4.md).

## Short answer

- **F304, the host: two draw sites in the game's screen-space UI pass, no swapchain.** Both answer the menu's render-condition virtual, `IMenu::PassesRenderConditionText` (vfunc 7), which `UI::ScreenSpace_RenderMenus` asks each menu on the stack every pass. DearModdingUI's current source uses this design for 1.10.163, 1.10.984 and 1.11.240. That is HEAD `7e09986`: 0.2.3 plus the UI-pass drawing, which is not yet released.
  1. **A hidden menu of the framework's own.** It is registered with `RE::UI::GetSingleton()->RegisterMenu(name, create)` and opened with `RE::UIMessageQueue::GetSingleton()->AddMessage(name, RE::UI_MESSAGE_TYPE::kShow)` from an F4SE UI task.
     - Flags: `kAlwaysOpen`, `kDontHideCursorWhenTopmost`, `kAllowSaving`, `kCompanionAppAllowed`, `kAdvancesUnderPauseMenu`, `kRendersUnderPauseMenu`. Also `inputEventHandlingEnabled = false` and `depthPriority = kConsole`.
     - Its `PassesRenderConditionText` override draws when the reason is `kRenderScreenspace` and `CursorMenu` is not open, then returns false.
     - A save load closes it, so `OnRemovedFromMenuStack` queues `kShow` again unless `Main::quitGame` is set.
  2. **`CursorMenu`'s vtable slot 7, swapped.** The swap is `REL::Relocation<std::uintptr_t>{ RE::VTABLE::CursorMenu[0] }.write_vfunc(7, hook)`. The hook calls the original, draws on `kRenderScreenspace`, and returns the original's answer. While the cursor is shown, this puts the canvases under it.
- **The draw.**
  - Device and context come from `BSGraphics::RendererData` (+0x48, +0x50).
  - The target is whatever the pass has bound: `context->OMGetRenderTargets(1, &rtv, nullptr)`. Its size and format come from the texture's desc.
  - The draw sits inside `render::ScopedPipelineState`. The draw callbacks get a `SubmitFrame` with that target, its size, no camera and no scene depth. That is `SubmitHook.cpp`'s `drawInFrame` without its VR checks.
- **F305, a canvas on the screen.**
  - `ImGuiLayer` builds the frame and `ImGuiRenderer` rasterizes it into the dedicated canvas's texture as in VR, so step 1 is unchanged.
  - Step 2 changes. In VR it is a world quad through the per-eye stereo shader: instanced x2, camera in b0, scene depth. On flat it becomes one screen rectangle: a 2D vertex shader, the rectangle in target pixels turned to clip space from the frame's width and height, one draw, no depth.
  - On flat the supersample is 1 and the texture is the rectangle's size. The copy is then 1:1, so the text stays sharp.
- **Blocking: CommonLibF4VR has no 1.10.984 IDs for half of this.**
  - `UI::RegisterMenu`, `RendererData::GetSingleton`, and IMenu's virtuals that are backed by IDs are plain `REL::ID`s of 1.10.163. None of those IDs is in the 1.10.984 Address Library.
  - On flat, a missing ID does not fail. It resolves to the next ID in the database, which is an unrelated address.
  - A CommonLibF4VR commit that adds the NG IDs goes before F304. C1's upstream merge does not touch these files. The IDs are in [§5](#5-what-commonlibf4vr-lacks-on-flat), each one checked in `version-1-10-984-0.bin`.
- **The menu calls no IMenu ID of its own.** It overrides `ShouldHandleEvent`, `ProcessMessage` and `PassesRenderConditionText` without calling IMenu's bodies. Without a movie, it needs only the IDs of `UI`, `UIMessageQueue` and the Scaleform heap.

## Sources

| Short name | What it is |
| --- | --- |
| CL | `external/F4VR-CommonFramework/external/CommonLibF4VR/CommonLibF4`, branch `flat-fo4` at `ad7805e5`; remote `upstream` is alandtse/CommonLibF4 at `15f504ea` |
| FW | `external/F4VR-CommonFramework`, branch `flat-fo4` at `34a5300` |
| DMUI | `C:\Stuff\GitHub\Mine\Modding-Reference\Fallout4\github-repos\gold\DearModdingUI` at `7e09986` (0.2.3 plus UI-pass drawing) |
| DMUI-CL | DMUI's CommonLibF4 fork, `Depends/commonlibf4` at `981fb964` (Dear-Modding-FO4/commonlibf4) |
| DMUI-RE | `Modding-Reference\Fallout4\Analysis\gold\DearModdingUI_RE_REFERENCE.md` |
| BGS | `Modding-Reference\RE\github-repos\BethesdaGhidraScripts`: `addresslibrary/f4/version-1-10-163-0.bin`, `version-1-10-984-0.bin` and `version-1-11-221-0.bin`; vtable dumps `scripts/commonlibf4/refs/f4_{og,ng,ae}_vtables.csv`; the 1.11.221 PDB publics `scripts/commonlibf4/refs/f4_221_pdb_publics.txt` |
| EXE984 | `C:\Games\steamapps\steamapps\common\Fallout 4 - NG\Fallout4.exe`, file version 1.10.984.0, image base 0x140000000 |

- **Address lookups.** I read the Address Library bins with a scratch script, in the format `IDDB.cpp` reads: a `u64` count, then `{u64 id, u64 offset}` pairs sorted by ID.
- **EXE984 checks are static only.** Its `.text` is encrypted on disk (it has a SteamStub `.bind` section), so only `.rdata` and `.data` can be read: vtables, RTTI, and the function pointers in them. No code was disassembled.
- **No 1.10.163 exe on this machine.** 1.10.163 facts come from its Address Library and from BGS's OG vtable dump.
- All RVAs below are relative to image base 0x140000000.

Confidence: **high** means verified in the binary, the Address Library or a PDB. **Medium** means it is in a working mod's source (DMUI) but was not verified here. **Low** means inference.

## Evidence

### 1. The render-condition virtual

- **Declaration.** CL `include/RE/Bethesda/IMenu.h:296`: `virtual bool PassesRenderConditionText(MENU_RENDER_CONTEXT a_reason, const BSFixedString& a_customRendererName) const  // 07`. Its default body calls `REL::ID(937304)`. **High.**
- **Where slot 7 comes from.** In IMenu's primary vtable, `SWFToCodeFunctionHandler` gives 00 dtor, 01 `Call` and 02 `MapCodeObjectFunctions`. IMenu adds 03 `ProcessMessage`, 04 `AdvanceMovie`, 05 `PreDisplay`, 06 `PostDisplay`, then 07. Source: CL `SWFToCodeFunctionHandler.h:15-21`, `IMenu.h:269-296`. **High.**
  - Checked in EXE984. `VTABLE::IMenu[0]` (ID 1345832) resolves to 0x250EB58. Its complete object locator names `.?AVIMenu@@`, and its slot 7 is 0x19FE430. That is where the NG ID 2287379 resolves in the 1.10.984 bin. **High.**
  - On 1.10.163, BGS `f4_og_vtables.csv` gives `CursorMenu` slot 7 as 0x14210F310. That is where 937304 resolves in the 1.10.163 bin (0x210F310). **High.**
- **Name and parameters.** The 1.11.221 PDB publics list `IMenu::PassesRenderConditionText(MENU_RENDER_CONTEXT,BSFixedString&)` at 0x01B19420 (`f4_221_pdb_publics.txt:3944`). That is exactly where NG ID 2287379 resolves in `version-1-11-221-0.bin`, so the NG ID is this function. **High.**
- **As a free function for the swap.** The signature is `bool (*)(const RE::IMenu*, RE::MENU_RENDER_CONTEXT, const RE::BSFixedString&)`, with `this` in rcx, the reason in edx and the name in r8 (DMUI `src/Platform/rendering/GameCursorRender.cpp:18-21`). `MENU_RENDER_CONTEXT::kRenderScreenspace` is 2 (CL `IMenu.h:95-104`). **High.**
- **Who asks.** `UI::ScreenSpace_RenderMenus` asks each menu on the stack (comment at DMUI `src/Platform/menus/FrameMenu.cpp:35`; DMUI-RE §1). **Medium:** this is the working mod's design, and the 1.10.984 code can't be disassembled here.
  - The function itself: OG ID 230711 resolves to 0x2043A80 in the 1.10.163 bin. True-Through-Scope uses that ID (`Modding-Reference\F4VR\github-repos\gold\True-Through-Scope\src\HookManager.h:305`).
  - It is named `UI::ScreenSpace_RenderMenus_141A81BB0` for 1.11.191 in BGS `extras/IDAImportNames_1.11.191.0.py:3394`.
  - Neither CL nor DMUI-CL has an NG ID for it, and F304 does not need one.
- **Order.** "The frame menu is asked before the cursor, so one site draws" (DMUI `src/Platform/rendering/PlatformImGui.cpp:96-98`). `kConsole` (19) is the depth just below `kCursor` (20) (CL `IMenu.h:115-138`). **Medium.**

### 2. CursorMenu's vtable, and swapping one slot

- **The IDs.** CL `include/RE/VTABLE_IDs.h:6418`: `std::array<REL::ID, 2> CursorMenu{ REL::ID(245579), REL::ID(324946) }`. These are plain IDs: the same ID is looked up in whichever runtime's database is loaded. CL `include/RE/RTTI_IDs.h:7891`: `REL::RelocationID CursorMenu{ 1551630, 2779135 }`. **High.**
- **The class.** CL `IMenu.h:1410-1421`: `CursorMenu : GameMenuBase`, sizeof 0xE8, `MENU_NAME` "CursorMenu".

| | ID | 1.10.163 | 1.10.984 |
| --- | --- | --- | --- |
| `VTABLE::CursorMenu[0]`, the IMenu primary vtable | 245579 | 0x2D86928 | 0x23A9D40. EXE984: its locator names `.?AVCursorMenu@@` at offset 0 |
| `VTABLE::CursorMenu[1]`, BSInputEventUser at +0x10 | 324946 | 0x2D869D8 | 0x23A9DE8. EXE984: same class, offset 16; slot 1 is 0x19FE810, IMenu::ShouldHandleEvent (NG ID 2287392) |
| `RTTI::CursorMenu` | 1551630 (OG) / 2779135 (NG) | 0x3805E90 | 0x2DFEFB8. EXE984: the type descriptor's name is `.?AVCursorMenu@@` |
| What slot 7 holds | | 0x210F310, IMenu's body (BGS OG dump) | 0x19FE430, IMenu's body (EXE984; BGS NG dump agrees) |

- **CursorMenu inherits slot 7.** It does not override it on either runtime, so the original to chain to is IMenu's default. The swap changes `CursorMenu`'s own vtable only; IMenu's vtable and other menus are untouched. **High.**
- **The swap.** CL `include/REL/Relocation.h:430-444`: `write_vfunc(idx, fn)` reads the slot, writes it with `safe_write` (`VirtualProtect`), and returns the old pointer. **High.**
  - DMUI does the same with `VirtualProtect` and `InterlockedExchangePointer` (DMUI `src/Support/Detours.cpp:44-61`).
  - DMUI refuses a slot that is null or already its hook (`GameCursorRender.cpp:47-67`). It installs at `F4SEPlugin_Load`, since the vtable is static `.rdata` and needs no UI singleton (DMUI-RE, Hooks table).
- **Chaining with other plugins.** DMUI swaps the same slot. Each hook calls the one it replaced, so the chain holds in either load order; the last installed draws first. **Medium.**
- **1.11.221.** ID 245579 is there too, at 0x25A4A70. BGS `f4_ae_vtables.csv` has `CursorMenu` slot 7 at IMenu's body as well. This is for milestone 5 only.

### 3. A hidden menu that is always open

**Flags and fields.** Bits are from CL `IMenu.h:140-170`. DMUI-CL uses the same names and bits (`include/RE/U/UI_MENU_FLAGS.h`). The reasons are DMUI's comments (`FrameMenu.cpp:42-57`).

| CL name | Bit | Why |
| --- | --- | --- |
| `UI_MENU_FLAGS::kAlwaysOpen` | 1 << 1 | Stays on the stack. A save load still removes it (see below) |
| `UI_MENU_FLAGS::kDontHideCursorWhenTopmost` | 1 << 14 | "else UI::RefreshCursor hides the cursor under us" |
| `UI_MENU_FLAGS::kAllowSaving` | 1 << 11 | "Missing kAllowSaving or kCompanionAppAllowed blocks saving" |
| `UI_MENU_FLAGS::kCompanionAppAllowed` | 1 << 23 | Same as `kAllowSaving` |
| `UI_MENU_FLAGS::kAdvancesUnderPauseMenu` | 1 << 20 | Asked under the pause menu |
| `UI_MENU_FLAGS::kRendersUnderPauseMenu` | 1 << 21 | Asked under the pause menu (IMenu's `OnMenuStackChanged` reads it, `IMenu.h:305-312`) |
| `BSInputEventUser::inputEventHandlingEnabled = false` | IMenu +0x18 | Input comes from the engine's queue hooks, never from the menu stack. Member at `BSInputEventUser.h:36`; BSInputEventUser is IMenu's base at +0x10 |
| `IMenu::depthPriority = UI_DEPTH_PRIORITY::kConsole` | 19, IMenu +0x64 | Below the cursor (`kCursor`, 20), so the cursor draws after it |

**Registering.**
- The call: `RE::UI::RegisterMenu(const char* a_menu, Create_t* a_create, StaticUpdate_t* a_staticUpdate = nullptr)` (CL `include/RE/Bethesda/UI.h:116-121`). `Create_t` is `IMenu*(const UIMessage&)` (`UI.h:28`), and the engine calls it when the menu is shown.
- DMUI registers at `kPostPostLoad`. It checks `menuMap` under `UI::GetMenuMapRWLock()` both before the call, that no other plugin owns the name, and after it, that UI kept the registration (DMUI `src/Platform/menus/HiddenMenu.cpp:14-81`). **Medium.**
- The name must be unique per mod. Two framework mods on flat would otherwise register one name.

**Opening.**
- The call: `RE::UIMessageQueue::GetSingleton()->AddMessage(RE::BSFixedString(name), RE::UI_MESSAGE_TYPE::kShow)` (CL `include/RE/Bethesda/UIMessageQueue.h:15-26`).
- DMUI sends it from `F4SE::GetTaskInterface()->AddUITask(...)` (CL `include/F4SE/Interfaces.h:383-384`; DMUI `FrameMenu.cpp:23-33` and `84-96`). **Medium.**

**Reopening.**
- A save load closes the menu despite `kAlwaysOpen`. DMUI overrides `OnRemovedFromMenuStack` to queue `kShow` again unless `RE::Main::GetSingleton()->quitGame` is set (`FrameMenu.cpp:70-80`). **Medium.**
- In CL, `OnRemovedFromMenuStack` is inline and ID-free (`IMenu.h:322`). `Main::GetSingleton` is `RelocationID(756304, 2698043)` and `quitGame` is at +0x24 (`Main.h:33, 63`).

**What the subclass inherits.**
- **Allocation is safe on NG.** IMenu allocates with `SWFToCodeFunctionHandler`'s `operator new`, which uses the Scaleform global heap: `RelocationID(939898, 2707353)` (CL `Scaleform/Kernel/SF_Memory.h:14`). The NG ID is there.
- **Virtuals that are inline and ID-free:** dtor, `Call`, `MapCodeObjectFunctions`, `AdvanceMovie`, `PreDisplay`, `PostDisplay`, `OnMenuStackChanged`, `OnAdded/RemovedFromMenuStack`, `CanAdvanceMovie`, `SetViewportRect`, and the `OnButtonEvent` override.
- **Virtuals whose CL bodies call IDs that are broken on NG:**
  - `ShouldHandleEvent` calls `REL::ID(1241790)`.
  - `PassesRenderConditionText` calls `REL::ID(937304)`.
  - `ProcessMessage` reaches `ProcessScaleformEvent`, a `VariantID(150211, 0, …)` whose NG value is 0, and `RefreshPlatform`, `REL::ID(1071829)`.
- **The fix in the subclass.** It overrides all three without calling IMenu's bodies:
  - `ShouldHandleEvent` returns false.
  - `ProcessMessage` returns `kHandled` for its own name and `kPassOn` otherwise, as DMUI's `HiddenMenu.cpp:39-44` does.
  - `PassesRenderConditionText` returns false for every reason, drawing first when the reason is `kRenderScreenspace`. DMUI calls IMenu's body for the other reasons.

**The movie.** DMUI loads `Interface/CursorMenu.swf` into each of its menus and hides it every `PreDisplay` (`HiddenMenu.cpp:12, 26-50`). It refuses to create the menu without a movie (`include/Platform/menus/HiddenMenu.h:33-42`). Its source gives no reason; it is inherited from the Addictol carrier menu (commit `7babce3`). See open point 3.

**For F306, not this ticket.** DMUI's modal menu has `kUsesCursor`, `kAssignCursorToRenderer`, `kModal`, `kAdvancesUnderPauseMenu`, `kRendersUnderPauseMenu`, plus `kPausesGame` unless FallSouls mode. It also sets `customRendererName = "FlatScreenModel"` and `inputContext = kCursor` (DMUI-RE, Native Menus).

### 4. Device, context, window, and the bound target

- **Device and context.** CL `include/RE/Bethesda/BSGraphics.h:277-281`: `RendererData::GetSingleton()` is `REL::Relocation<RendererData**>{ REL::ID(1235449) }`. **High** for the layout; the ID is broken on 1.10.984.
  - Fields: `initialized` +0x33, `device` +0x48, `context` +0x50, `renderWindow[32]` +0x58. In `RendererWindow`, `hwnd` is at +0x00 and `swapChain` at +0x18.
  - In the flat branch, sizeof is 0x25C0 (`BSGraphics.h:283-324`). VR's bigger target arrays sit behind `ENABLE_FALLOUT_VR && !FALLOUT_CROSS_VR`, so a flat build gets the flat layout.
  - DMUI-CL has the same accessor as `VariantID{ 1235449, 2704429 }` (`include/RE/IDs.h:1158`), with the same `RendererData**` type and offsets. DMUI relies on it for OG, NG and AE (DMUI `src/Platform/rendering/PlatformImGuiBackend.cpp:179-198`; DMUI-RE, Runtime Notes).
  - DMUI reads the device once: "The engine creates its device once and never replaces it."
  - FW already reads it this way (`src/render/RenderUtils.cpp:7-17`). Only the ID changes.
- **The window.**
  - DMUI uses `GetCurrentRendererWindow`, `VariantID{ 91810, 2704431 }` (DMUI-CL `IDs.h:1159`). CL has no such accessor.
  - CL has `RE::Main::hwnd` (+0x30) and `threadID` (+0x40), behind a singleton with an NG ID (`Main.h:33, 71, 73`).
  - FW's VR keyboard finds the window with `FindWindowA("Fallout4VR")` (`src/imgui/ImGuiKeyboard.cpp:36, 251`). The flat window class was not verified here. **Medium.**
- **The bound target.** In the pass:
  - Read it: `context->OMGetRenderTargets(1, rtv.GetAddressOf(), nullptr)`, then `rtv->GetResource`, `As<ID3D11Texture2D>` and `GetDesc` for the width and height. With no target bound, skip and warn once (DMUI `PlatformImGuiBackend.cpp:311-329`, `534-540`).
  - Draw: DMUI binds `OMSetRenderTargets(1, &rtv, nullptr)` and draws. It restores the render targets, the depth view, the OM UAVs and the HS, DS and CS shaders afterwards (`PlatformImGuiBackend.cpp:410-500`; `include/Platform/rendering/D3D11State.h:29-70`). **Medium.**
  - FW's `drawInFrame` already does this read and logs the target's size and its texture and view formats once (`src/render/SubmitHook.cpp:370-450`).

### 5. What CommonLibF4VR lacks on flat

**How a runtime picks an ID.**
- CL decides the runtime from the file version: a third field above 163 is NG (`src/REL/Module.cpp:67`). 1.10.984, and 1.11.x too, therefore look IDs up in the NG database.
- `RelocationID` and `VariantID` pick the NG ID there (`include/REL/ID.h:81-99, 147-165`).
- A plain `REL::ID` looks its one ID up in whichever database is loaded.

**The trap.**
- `IDDB::id2offset` fails only past the end of the table, or on VR when the ID is not an exact match (CL `src/REL/IDDB.cpp:141-158`; upstream is the same).
- On flat, an ID missing from the database therefore returns the offset of the next higher ID, silently. Examples on 1.10.984: `UI::RegisterMenu`'s 1519575 lands on ID 1519577 at 0x2439E28, and `RendererData`'s 1235449 lands on ID 1235455 at 0x272C780.
- On 1.10.163, an NG ID would fail loudly (past the end), but `RelocationID` never asks for one there.

**The table.**
- The NG IDs come from DMUI-CL `include/RE/IDs.h` at the lines given.
- Every NG ID below is present in `version-1-10-984-0.bin`.
- Where the 1.11.221 PDB names the function, the NG ID resolves in the 1.11.221 bin to that exact PDB address. That holds for `PassesRenderConditionText`, `ShouldHandleEvent`, `RefreshPlatform`, `OnSetSafeRect`, `AddMessage` and `LoadMovie`.

| CL accessor (file) | CL now | 1.10.984 | NG ID | Needed for |
| --- | --- | --- | --- | --- |
| `UI::GetSingleton` (UI.h:77) | `RelocationID(548587, 2689028)` | ok, 0x2E66400 | | menu checks, `GetMenuOpen` |
| `UI::GetMenuMapRWLock` (UI.h:71) | `RelocationID(578487, 2707105)` | ok | | `GetMenu`, `GetMenuOpen` |
| `UIMessageQueue::GetSingleton`, `AddMessage` (UIMessageQueue.h:17, 24) | `RelocationID` both | ok | | opening the menu |
| `VTABLE::CursorMenu[0]` (VTABLE_IDs.h:6418) | `REL::ID(245579)` | ok, 0x23A9D40 | | the slot swap |
| Scaleform global heap (SF_Memory.h:14) | `RelocationID(939898, 2707353)` | ok | | `new` of the menu |
| `Main::GetSingleton` (Main.h:33) | `RelocationID(756304, 2698043)` | ok | | `quitGame`, `hwnd` |
| **`UI::RegisterMenu`** (UI.h:119) | `REL::ID(1519575)` | **missing** | 2284766 (IDs.h:3812) | registering the menu |
| **`BSGraphics::RendererData::GetSingleton`** (BSGraphics.h:279) | `REL::ID(1235449)` | **missing** | 2704429 (IDs.h:1158) | device, context |
| **`IMenu::PassesRenderConditionText`** (IMenu.h:299) | `REL::ID(937304)` | **missing** | 2287379 (IDs.h:2330) | any call of IMenu's body |
| **`IMenu::ShouldHandleEvent`** (IMenu.h:247) | `REL::ID(1241790)` | **missing** | 2287392 (IDs.h:2329) | as above |
| **`IMenu::ProcessScaleformEvent`** (IMenu.h:360) | `VariantID(150211, 0, 0x21998A0)` | **0 on NG** | 2287395 (IDs.h:2332) | IMenu's `ProcessMessage` |
| **`IMenu::RefreshPlatform`** (IMenu.h:367) | `REL::ID(1071829)` | **missing** | 2287374 (IDs.h:2333) | IMenu's `ProcessMessage`, `LoadMovieEx` |
| **`IMenu::OnSetSafeRect`** (IMenu.h:351) | `REL::ID(964859)` | **missing** | 2287375 (IDs.h:2331) | `LoadMovieEx` |
| **`BSScaleformManager::GetSingleton`** (BSScaleformManager.h:88) | `REL::ID(106578)` | **missing** | 2689600 (IDs.h:1360) | a hidden movie |
| **`BSScaleformManager::InitMovieViewport`** (BSScaleformManager.h:130) | `REL::ID(206895)` | **missing** | 2287430 (IDs.h:1363) | a hidden movie |
| **`LoadMovieEx`'s `FileUncacheOnMenuOpen`** (src/RE/Bethesda/BSScaleformManager.cpp:18) | `REL::ID(454316)` | **missing** | 2667999 (IDs.h:1364) | a hidden movie |
| **`BSScaleformManager::LoadMovie`** (BSScaleformManager.h:101) | `REL::ID(1526234)` | **missing** | 2287422 (IDs.h:1361) | a hidden movie, if used instead of `LoadMovieEx` |
| **`MenuCursor::GetSingleton`** (MenuCursor.h:18) | `REL::ID(695696)` | **missing** | 2696546 (IDs.h:2526) | F306, the cursor position |
| **`UI::RefreshCursor`** (UI.h:112) | `VariantID(1436639, 0, 0x20CD070)` | **0 on NG** | 2284772 (IDs.h:3811) | F306, if used |

- **The fix keeps VR as it is.** `RelocationID(og, ng)` gives VR the OG ID, so `REL::ID(x)` becomes `RelocationID(x, ng)`. `VariantID(x, 0, vrOffset)` becomes `VariantID(x, ng, vrOffset)`.
- **Layouts.** IMenu (0x70), UI (`menuStack` +0x190, `menuMap` +0x1A8), `RendererData`, `Main` and `MenuCursor` have no VR branch in CL. Their flat layouts are the ones DMUI relies on for OG, NG and AE (DMUI-RE, Runtime Notes). `BSInputEventUser`'s three VR-only virtuals are behind `ENABLE_FALLOUT_VR && !FALLOUT_CROSS_VR` (`BSInputEventUser.h:25-31`), so they are absent from the flat build. **High** for CL's source; **medium** for the layouts, which DMUI uses but which were not verified here.
- **No VR-only layout in this path.** The only VR branches touched are `RendererData`'s target arrays, after `context`, which F304 does not read.
- **1.11.x.** `UI::GetSingleton`'s NG ID 2689028 is missing from the 1.11.221 bin; DMUI-CL gives AE 4796314. That is C3/C4 work in milestone 5. The other NG IDs above are present in 1.11.221.

### 6. The framework's ImGui pipeline: what carries over

| Part (FW) | On flat |
| --- | --- |
| `imgui/ImGuiLayer.cpp` frame building: canvases, `ensureContext`, the dedicated-canvas display, `ClonedDrawData`, `renderer::publish` | **Carries over.** The context and DX11 backend are made on the device from `render::getDevice()` (`ImGuiLayer.cpp:138-185`) |
| `ImGuiLayer.cpp` placement into world quads: `viewerPosition` from `f4vr::getVRPlayerNodes` (`:234`), `buildQuad` from a `NiTransform` (`:246`), `appendDrawnQuads` curve (`:280`) | **VR.** Flat gives a screen rectangle in its place (F305) |
| `ImGuiLayer.cpp` pointer and keyboard updates, haptics `vrcf::VRHaptics.trigger` (`:730`), includes of `f4vr/PlayerNodes.h` and `vrcf/VRControllersHaptic.h` (`:11-15`) | **VR**, guarded or split (F301). The pointer becomes the game's cursor in F306 |
| `imgui/ImGuiRenderer.cpp` step 1: each frame into its texture with `ImGui_ImplDX11_RenderDrawData`, `writeTextures`, `copyForCapture`/`readCapture` (devbench snapshot) (`:567-605`) | **Carries over** unchanged |
| `ImGuiRenderer.cpp` step 2: `K_QUAD_VERTEX_SHADER`, the stereo split by `SV_InstanceID` with `matProjView[2]` in b0 (`:44-80`); `drawQuads` with scene-depth test states and `DrawInstanced(…, 2, …)` (`:429-480`); rebinding `submitFrame.sceneDepth` (`:606-640`) | **VR.** Replaced by one non-instanced 2D draw. The quad pixel shader (atlas sample), the premultiplied blend (ONE / INV_SRC_ALPHA, `:362-374`) and the linear sampler carry over |
| `imgui/ImGuiTextures.*`, `ImGuiFonts.*`, `ImGuiImage.*`, `ImGuiSnapshot.*`, `ImGuiTheme.*`, `controls/` | **Carry over.** Textures from `render::Texture` were not checked for NG IDs |
| `imgui/ImGuiPointer*`, `ImGuiKeyboard*` | **VR**, except the window-procedure hook for physical keys, which F306 reuses with the flat window |
| `render/SubmitHook.cpp`: callback table, `registerDrawCallback`, `setDrawCallbackActive`, `runCallbacks` (`:289`), GPU timer, capture markers | **Carries over**, run from the flat host |
| `SubmitHook.cpp`: OpenVR Submit hook and its named-mutex install (`:795`), player-pass hook, `getStereoCameraConstants`, scene-depth capture, eye-texture size check (`:395-418`) | **VR** |
| `render/RenderUtils.*`: `getDevice`/`getContext`, `ScopedPipelineState` | **Carry over.** `getStereoCameraConstants` is VR |

**For F305:**
- **The rectangle.** It is in target pixels, set by the canvas's placement on flat, and turned to clip space render side from `SubmitFrame::width`/`height`. Draw it with no depth, one draw, and a viewport over the target.
- **Supersample 1 on flat.** The default supersample is 1.5 (`imgui/ImGuiSettings.cpp:7`). At 1.5 the dedicated texture is 1.5x the canvas (`ImGuiLayer.cpp:84-91`), and the linear sampler, which has no mips, would shrink it. The canvas's pixel size should equal the rectangle's, which is whole pixels and aligned to whole pixels.

## Gaps and open points that need a decision

1. **A CommonLibF4VR commit for the NG IDs** in §5's table, before F304. The plan's C1 is the upstream merge, and upstream has none of these IDs. It needs a number in the plan (C1a, or before F304).
2. **Missing IDs resolve silently on flat.** `IDDB::id2offset` checks an exact match only on VR (§5). Every plain `REL::ID` the flat build calls on 1.10.984 is a wrong address unless that ID is in the NG database. This reaches beyond F304 (F302, F303, M30x).
   - Option A: CommonLibF4VR fails on a missing ID on flat too, so the plugin stops with the ID named the first time that ID is resolved. This diverges from upstream.
   - Option B: a build-time check of every ID the flat build resolves against the 1.10.163 and 1.10.984 bins.
   - Either way, it is outside this ticket.
3. **A hidden movie or none.**
   - DMUI's proven shape loads `Interface/CursorMenu.swf` and hides it. That costs the four Scaleform IDs above, plus `RefreshPlatform` and `OnSetSafeRect`.
   - A menu with no movie needs none of them. Whether the UI accepts a menu with no movie through show, the pass and close is unverified.
   - Try without a movie first in 1.10.984 and fall back to DMUI's shape, or take DMUI's shape outright.
4. **The thread and the tick, coupled with F303.** In VR, the game thread builds the ImGui frame at frame end and the pass draws the latest frame published.
   - On flat, M303 opens the panel from the pause menu. If F303's tick does not run while the pause menu is up, the panel freezes. The UI pass still runs, since the menu has `kRendersUnderPauseMenu`.
   - DMUI builds its ImGui frame in the UI pass.
   - Decide with F303: confirm the tick runs under the pause menu, or build the frame in the pass.
5. **To log on the first pass in 1.10.984**, as VR's "draws into WxH format F" line does:
   - the pass's thread, against the tick's thread and `Main::threadID`;
   - the target's size and texture and view formats, in particular whether the view is sRGB;
   - whether `ScreenSpace_RenderMenus` runs more than once a frame or into a target of another size.
   The host then declines a target of another size, as VR declines one that isn't the eye texture's size. It also draws once a frame.
6. **The screen size reaches the game thread one frame late.** The panel's size (two thirds of the screen's height, M302) is decided on the game thread. It comes from the target size seen in the last pass, or from the renderer.
   - DMUI logs when the client size differs from the UI target and maps input to the target. F306 needs the same mapping.
7. **The window.** It comes from `Main::hwnd` in CL, or from `FindWindowA` with the flat class name, which is unverified ("Fallout4" is a guess). That is for F306.
8. **OM UAVs.** `ScopedPipelineState` saves render targets and the depth view but not OM UAVs; DMUI saves them. The risk is low, and it can be checked in a capture.
9. **Other UI-pass drawers.** DMUI's frame menu sits at `kConsole` too, and both chain `CursorMenu` slot 7. Where two panels overlap, which one is on top depends on load order and stack order. That is acceptable unless the user decides otherwise.
10. **Limits of this check.**
    - The 1.10.984 code can't be read statically (SteamStub encrypts `.text`), so `ScreenSpace_RenderMenus`'s calls rest on DMUI's working source.
    - No 1.10.163 exe is local, so its vtable facts rest on its Address Library and BGS's OG dump.
    - The 1.11.240 Address Library is not local. The 1.11.221 one stands in for 1.11.x.
