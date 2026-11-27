# Research: frame tick on flat

Ticket: [02-frame-tick-on-flat.md](../issues/02-frame-tick-on-flat.md). For F303 of [the plan](../../../docs/design/flat-fallout4.md).

## Short answer

Hook the same place as VR: a `call rel32` inside `Main::OnIdle`, in the block that VR's tick sits in. VR's own callee there is VR-only code, so flat takes the next call in that block: `BGSDynamicPersistenceManager::DemoteQueuedActors(this)`. The framework's `write_call<5>` and its `void (*)(uint64_t rcx)` thunk carry over unchanged.

| | 1.10.163 | 1.10.984 | 1.11.240 (current Steam) |
|---|---|---|---|
| `Main::OnIdle`, the function that holds the call | ID **633524** | ID **2228917** | ID **2228917** (in the 1.11.191 and 1.11.221 databases; the 1.11.240 database not checked) |
| Offset of the call in `OnIdle` | **+0x210** | **+0x492** | **+0x492** |
| Callee, for a check before writing: `DemoteQueuedActors` | ID **727554** | ID **2193821** | ID **2193821** (191, 221) |
| Site RVA, bytes, callee RVA | 0xD39070 `E8 CB 32 45 FF` → 0x18C340 | 0xBAE2B2 `E8 89 5C 73 FF` → 0x2E3F40 | 0xC33E62 `E8 C9 47 70 FF` → 0x338630 |

In CommonLibF4VR terms: `REL::Relocation{ REL::RelocationID(633524, 2228917), REL::VariantOffset(0x210, 0x492, 0) }`, with a check that the site is still `E8` to `RelocationID(727554, 2193821)`, as `VRHookSites::CallSite::Verify` does in VR.

All of the above was **verified** by disassembling the three flat binaries, except where the table says otherwise. The 1.10.984 binary was decrypted from its SteamStub first; see [Method](#method-and-sources).

## What the tick must give

From `src/MainLoopHook.h`, `src/ModBase.cpp` (`onFrameUpdateSafe`), the mod's `onFrameUpdate` and `MenuController::onFrameUpdate` (all **verified**, source read):

- **Once per frame, on the game thread.** devbench actions run there (`devbench::internal::onFrameStart`), as do config subscribers after a reload (`notifySubscribersOfReload`) and the mod's update. The frame-end callbacks follow, and `imgui::internal::onFrameEnd` builds the ImGui frame in them, then clones it for the render side (`src/imgui/README.md`, "How it is drawn"). The Submit hook records this thread as the game thread.
- **Before the frame's render.** The ImGui frame is built in the tick and drawn later (in VR at the player-space pass; on flat in the UI pass, F304). A tick before the frame's UI pass shows a change in the same frame. In VR the tick at `OnIdle+0x27E` runs before `OnIdle`'s `Main::Swap` call at 0xD844BC (`VRHookSites::MainUpdateSwap`) (**verified**).
- **In menus and while the game is paused: required on flat.** The menu opens from a pause-menu row (M303), and the pause menu pauses the game. While it is open, `MenuController::onFrameUpdate` drives the input, the client requests, saving and the ImGui frame. The VR site already runs in menus and while paused, see below.
- **During loading screens: not needed.** The menu closes without asking when `LoadingMenu` opens (`ModsConfigFO4::onGameLoaded`). devbench's `state` answers from the last snapshot while the game is stalled (`src/devbench/README.md`). VR's tick doesn't run during a load either: the main thread is inside the load (**likely**, see below).
- **Main menu: not needed.** `onFrameUpdate` returns until `PlayerCharacter::loadedData` is set (`ModsConfigFO4::isPlayerLoaded`).

## The VR site, and the same place on flat

### VR (1.2.72)

- `VRHookSites::MainUpdateFrameTick` = 0xD8405E, `call 0xD3C820`. It sits in function 0xD83DE0, which is `Main::OnIdle`. Its 1.10.163 ID is 633524 in the VR address library CSV, which is keyed by 1.10.163 IDs (756304 resolves to `Main`'s singleton in both) (**verified**).
- The callee 0xD3C820 takes `rcx` = `BGSSaveLoadManager` singleton (ID 1247320). It atomically reads a flag at +0x7CC; when the flag is set and `queuedEntryToLoad` (+0x48) is set, it clears the flag, sets `kLoadGame` in `queuedTasks` (+0x40) and returns true (**verified**). Its code (`prefetchw [rcx+0x7CC]` …) appears nowhere in the 1.10.163, 1.10.984 or 1.11.240 binaries (**verified**, byte search), so it is VR-only and the exact VR site has no flat counterpart.
- The site's only gate in `OnIdle` is `Main::QGameDataLoaded` (ID 1004331, 0xD83EE4 `cmp byte [..], 0; je 0xD8408C`). Every path inside that block reaches 0xD84057, the `mov rcx` before the call, whatever `inMenuMode` (+0x1D0) or `freezeTime` (+0x2A) is (**verified**). This is why the VR tick runs in menus and while paused.
- After the tick call, VR's block runs: `BGSDynamicPersistenceManager::DemoteQueuedActors`, `BGSSaveLoadManager::UpdateQueuedTasks`, a call with no arguments, and a call on the player (**verified**). Names come from the 1.11.191 IDA names in BethesdaGhidraScripts (`extras/IDAImportNames_1.11.191.0.py`), matched by ID (**sourced**).

### Flat: the same block in every runtime

| | 1.10.163 | 1.10.984 | 1.11.240 |
|---|---|---|---|
| `Main::OnIdle` RVA (ID) | 0xD38E60 (633524) | 0xBADE20 (2228917) | 0xC339D0 (found by the 1.10.984 prologue, unique) |
| `QGameDataLoaded` gate | +0xED, ID 1004331 | +0x372, ID 2698032 | +0x372 |
| `DemoteQueuedActors` call (recommended) | +0x210 | +0x492 | +0x492 |
| `UpdateQueuedTasks` call (alternative) | +0x21C, callee ID 927422 | +0x49E, callee ID 2228083 | +0x49E |
| `Main::Swap` call (renders the frame) | +0x6EC (0xD3954C), ID 1075087 | later in `OnIdle` | later in `OnIdle` |

- On each runtime, every path inside the `QGameDataLoaded` block reaches the two calls; the menu state doesn't change that (**verified**, all branch targets in the block followed).
- Both callees take `this` in `rcx` only, and their callers ignore the return value. `UpdateQueuedTasks` opens with `cmp dword [rcx+0x40], 0` (`queuedTasks`) on all three runtimes, and `DemoteQueuedActors`' openings match between 1.10.984 and 1.11.240 (**verified**).
- `UpdateQueuedTasks` has one caller in the whole binary, `OnIdle`, on 1.10.163 and 1.10.984. `DemoteQueuedActors` has 4 callers on 1.10.163 and 3 on 1.10.984, one of them in `OnIdle` (**verified**).
- The order matches VR: the tick, then the save/load queue, then later `Main::Swap` (world and UI drawn) in the same `OnIdle` (**verified** on 1.10.163; on 1.10.984 `Swap` (2228913) is called from `OnIdle` at 0xBAEAFC, after the site).
- The gate byte: CommonLibF4 upstream, the DearModdingUI fork (`Main::QGameDataLoaded{ 1004331, 2698032 }`) and the 1.11.191 IDA names (`Main::bGameDataLoaded`) call it `QGameDataLoaded` (**sourced**). A flat 1.11.240 plugin's hang notes call the same byte the "position the player" flag (F4Multiplayer, `docs/HANG-20261017.md`). Whatever the byte means, the flat site sits behind the same gate as VR's, so it runs whenever VR's tick runs.
- Second source for the function: the DearModdingUI fork's `include/RE/IDs.h` has `Main::OnIdle{ 633524, 2228917 }` (**sourced**, matches the mapping above).

### Loading screens

- **Flat:** the UI pass runs during loading screens, on another OS thread. DearModdingUI's README says so: "The pass runs whenever the game renders its interface, including the main menu and loading screens; blocking movie playback has none", and "loading screens move the pass to another OS thread" (`include/DearModdingUI/README.md`, "Frame observation") (**sourced**).
- **VR:** Submit runs on the game thread during play and on the loading screen's own thread during loads (framework RE reference, 0.4.1) (**sourced**).
- **Likely:** during a load the main thread is inside `UpdateQueuedTasks`, so `OnIdle` doesn't loop and the tick doesn't run. That matches VR. To confirm in the flat game.

## Candidates compared

| Candidate | Game thread | In menus and paused | Loading screens | Addresses | Verdict |
|---|---|---|---|---|---|
| **A1. `OnIdle`'s `DemoteQueuedActors` call** | yes, main loop | yes, behind the same gate as VR | no (same as VR) | 2 IDs per runtime + 1 offset | **Recommended**: VR's place in the frame, and nothing heavy runs inside the hooked call |
| A2. `OnIdle`'s `UpdateQueuedTasks` call | yes | yes | no | same | Equivalent. Its callee has one caller in the whole game, but saves and loads run inside the hooked call, so inside the framework's thunk |
| B. Hidden always-open menu, `PassesRenderConditionText` (vfunc 7), as DearModdingUI draws | main thread in play, **another thread during loads** | needs `kRendersUnderPauseMenu` | yes, off-thread | none (own class) | No: the wrong thread during loads; the menu is closed by a save load despite `kAlwaysOpen` and must reopen itself (DearModdingUI's `OnRemovedFromMenuStack`); runs mid-render. It stays F304's draw site, not the tick |
| C. Hidden menu `AdvanceMovie` (vfunc 4) | in `OnIdle` on 1.10.163 (UI advance, ID 910928, at 0xD395F1); also called from 1453703, which 109842 passes on as a function pointer (likely a job) | needs `kAdvancesUnderPauseMenu` (`CanAdvanceMovie`) | not established | none | No: runs **after** `Main::Swap`, so a frame late for the draw; an extra gate before it (`364995(0)`); thread not guaranteed |
| D. F4SE `AddTaskPermanent` (F4Multiplayer does this on 1.11.240) | yes, F4SE's pump in `OnIdle` (1.10.163 0xD3970B, 1.10.984 0xBAECD6) | likely | no | none | No: missing in F4SE 0.6.23, so not on 1.10.163; missing from CommonLibF4VR's `TaskInterface`; runs after `Swap` |
| D'. F4SE `AddTask` re-queued every frame | yes | – | – | none | No: F4SE drains with `while (!s_tasks.empty())` under a re-entrant lock, so a task that queues itself spins forever (`Hooks_Threads.cpp`) |
| E. DXGI `Present` (F4SEMenuFramework, PrismaUI) | render path | yes | yes, off-thread | a device or swapchain hook | No: the plan rules out a swapchain hook, and it has the same thread problem as B |
| F. The `OnIdle` call in `Main::Run`, or the `Swap` call | yes | yes | – | IDs | No: a hook on the `Run` call ticks outside the `QGameDataLoaded` gate, so likely before game data loads, which VR's tick never does (`Run`'s ID also differs on 1.11.x: 4484191); `Swap` is a frame later for the build |

## How flat plugins in the reference library tick

| Plugin | Per-frame point | IDs | Source |
|---|---|---|---|
| DearModdingUI 0.2.3+ | Own hidden `DearModdingUI_Frame` (`kAlwaysOpen`) `PassesRenderConditionText` once per UI pass, plus `CursorMenu` vfunc 7 while the cursor shows; frame observers run there | none of its own for this; `CursorMenu` vtable 245579 on all three | `Analysis/gold/DearModdingUI_RE_REFERENCE.md` §1, README (**sourced**) |
| F4SEMenuFramework 3.3.1 | `IDXGISwapChain::Present` vtable slot 8. The device comes from a `D3D11CreateDeviceAndSwapChain` hook: OG `write_call<5>` at ID 224250+0x419, AE 4492363+0x410, NG an IAT plus cached-pointer scan | as listed | `Analysis/gold/F4SEMenuFramework_RE_REFERENCE.md` §1–2 (**sourced**) |
| PrismaUI_F4 | MinHook on `Present` (8) and `ResizeBuffers` (13) | none | `Analysis/gold/framework-F4-Conversion_RE_REFERENCE.md` (**sourced**) |
| FloatingDamage (OG only) | Own menu's `DrawNextFrame`, which drains a queue on the UI thread | signature scans | `Analysis/gold/FloatingDamage_RE_REFERENCE.md` (**sourced**) |
| F4Multiplayer (outside the library) | `F4SE::GetTaskInterface()->AddTaskPermanent(OnFrame)` | none | GitHub `Jolley71717/F4Multiplayer` `src/main.cpp:1250` (**sourced**) |

None of these hooks `Main::OnIdle`'s calls. The library has no flat plugin with a main-loop `write_call` tick.

## What CommonLibF4VR has, and what's missing

The `RelocationID(a, b)` IDs below are (1.10.163, 1.10.984); VR resolves the first value through its CSV.

- **Has:** `RE::Main` with `GetSingleton` (756304, 2698043), `WorldRootCamera` (384264, 2228956), and the members `quitGame` +0x24, `freezeTime` +0x2A, `inMenuMode` +0x1D0. Flat `OnIdle` writes +0x1D0 from `UI::menuMode` (+0x1E0) and reads +0x2A on all three runtimes (**verified**). It also has the `BGSSaveLoadManager` singleton (1247320, 2697802).
- **Fork gap that C1 fixes:** `QGameSystemsShouldUpdate` is `RelocationID(0, 2698031)` and `QGameDataLoaded` is `RelocationID(0, 2698032)`. On 1.10.163 and VR they resolve to address 0. Upstream `master` has 779552 and 1004331, which match the binaries (**verified**: adjacent bytes 0x5AA425C and 0x5AA425D on 1.10.163, both read in `OnIdle`).
- **Missing, in the fork and upstream:**

  | Name | 1.10.163 | 1.10.984 / 1.11.x |
  |---|---|---|
  | `Main::OnIdle` | 633524 | 2228917 |
  | `BGSDynamicPersistenceManager::DemoteQueuedActors` | 727554 | 2193821 |
  | `BGSSaveLoadManager::UpdateQueuedTasks` | 927422 | 2228083 |
  | `Main::Swap` | 1075087 | 2228913 |
  | `Main::DrawWorld_And_UI` | 408683 | 2228969 |

  Also missing: the 1.10.984 and 1.11.x IDs of the `BGSDynamicPersistenceManager` singleton, which is `REL::ID(109630)` only (2689026 and 4796312 per the DearModdingUI fork; the hook doesn't need it), and `TaskInterface::AddTaskPermanent`.
- **No flat equivalent of `VRHookSites`** (call site + expected callee) exists yet. VR resolves `RelocationID(633524, …)` to its own `OnIdle` (0xD83DE0), so one ID can name `OnIdle` in all three games.

## Open points that need a decision

1. **A1 or A2.** A1 is recommended for VR's place in the frame. A2's callee has a single caller, so its site is unambiguous by construction; the cost is that saves and loads run inside the framework's thunk. Either is one line in the table above.
2. **Fixed offset or a scan.** `VariantOffset(0x210, 0x492)` needs an offset per runtime. The alternative scans `OnIdle` for the one `E8` whose target is the callee's ID, which makes a new 1.11.x build a matter of IDs only and is the pre-write check in the same step. 1.11.191 and 1.11.221 offsets are unverified (no binaries here); 0x492 is likely, as 1.10.984 and 1.11.240 agree.
3. **Where the site lives.** By the framework's rule (no addresses of its own), the IDs belong in CommonLibF4VR, e.g. a flat counterpart of `VRHookSites` with the site, the callee and `Verify()`. That would be a CommonLibF4VR commit ahead of F303, which the plan doesn't list yet.
4. **1.11.240 IDs.** 2228917 and 2193821 are in the 1.11.191 and 1.11.221 databases. The 1.11.240 database (`version-1-11-240-0.bin`) isn't on this machine; check it in ticket 06 / C4.
5. **To confirm in the flat game:** the tick runs in the pause menu and doesn't run during a loading screen; `validate()` stays quiet with the usual mods installed; whether `tm` (toggle menus) or a minimized window stops `OnIdle` (on 1.10.984, `OnIdle`+0x18C jumps near the function's end while `GetAsyncKeyState` reports Tab and Alt down, **verified**, as in VR).

## Method and sources

- **Binaries:** `D:\Modding\Fallout4-1.10.163-steamless.exe` and `D:\Modding\Fallout4-1.11.240-steamless.exe` (steamless copies); `Fallout 4 - NG\Fallout4.exe` 1.10.984.0, whose `.text` is SteamStub 3.1 AES-encrypted on disk. It was decrypted into a copy in the session scratchpad (header XOR chain, AES-CBC with the header's key and IV, as Steamless does); the game file was only read. VR: `D:\Modding\Fallout4VR-steamless.exe`. Disassembled with capstone, with function bounds from `.pdata`.
- **Address libraries:** BethesdaGhidraScripts `addresslibrary/f4/`: `version-1-10-163-0.bin`, `version-1-10-984-0.bin`, `version-1-11-191-0.bin`, `version-1-11-221-0.bin`, `version-1-2-72-0.csv`.
- **Names:** BethesdaGhidraScripts `extras/IDAImportNames_1.11.191.0.py`; the DearModdingUI CommonLibF4 fork (`981fb96`) `include/RE/IDs.h`; CommonLibF4VR (`ad7805e5`) and upstream `alandtse/CommonLibF4` `master` (`15f504ea`).
- **F4SE:** `ianpatt/f4se` on GitHub. `f4se/Hooks_Threads.cpp` and `PluginAPI.h` at v0.6.23, v0.7.2 and v0.7.9. `f4se_version.h` on `master`: the 1.11 runtimes are 137, 159, 169, 191, 221 and 240, and v0.7.9's `CURRENT_RELEASE_RUNTIME` is 1.11.240.
- **Reference library:** `Fallout4/Analysis/gold/*` (DearModdingUI, F4SEMenuFramework, framework-F4-Conversion, FloatingDamage), the source clones under `Fallout4/github-repos/gold/`, and `F4VR/Analysis/gold/F4VR-CommonFramework_RE_REFERENCE.md` for VR's Submit threads.
