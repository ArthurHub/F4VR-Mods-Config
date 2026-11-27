# Flat Fallout 4

Mods Config built for flat Fallout 4 as well: the same source makes a second DLL, `ModsConfigFO4.dll`, which runs in `Fallout4.exe`. The work is on the `flat-fo4` branch of this repository, of the framework submodule and of CommonLibF4VR.

## The shape

- **Two DLLs from one source.** A CMake option picks the game: VR, as today, or flat. CommonLibF4VR puts VR's struct layouts in at compile time (`PlayerCharacter` is 0x12B0 bytes in VR, 0xE10 flat), so one build can't read both games.
- **The flat DLL runs on every flat runtime.** CommonLibF4VR resolves 1.10.163 and 1.10.984 in one build, and builds for them as it is. 1.11.240, the current Steam build, is a third runtime the library doesn't have yet ([milestone 5](#5-111x)).
- **VR code stays out of the flat build.** A file that is all VR is built for VR only; a shared file guards its VR lines with `#ifdef ENABLE_FALLOUT_VR`. VR-only types (`VRPlayerNodes`, `VRHookSites`) don't exist in the flat build, so VR code that reaches one fails to compile there.
- **The menu is shared.** `ui/`, `model/`, `spec/`, `menu/` and `mcm/` don't know the game. The game side is behind `MenuPort`: `GameMenu` in VR, `FlatGameMenu` on flat.
- **Each build its own config.** Flat ships `ModsConfigFO4.toml` and its own config spec of the menu, with only the flat settings; the sections both have (`[Developer]`, `[Debug]`) are in both.
- **One config spec of a mod for both.** A config spec's `myGames` root is the running game's `My Games` folder.

### The menu on the flat screen

- The panel's canvas is centered on the screen, two thirds of the screen's height (a key in the mod's config), and drawn at that size, so its text stays sharp.
- Mouse and keyboard: the game's cursor is ImGui's mouse, the wheel scrolls, Esc is Back, and keys and typed text come from the game's window, as in VR. A gamepad is decided after the first release. There are no wrist buttons, haptics, FRIK quick launch or Adjust in world.
- F10 opens and closes it during play (a key in the mod's config), and the game keeps running, as in VR, so a live change is seen.
- A `MODS CONFIG FO4` row above Settings in the pause menu opens it over the pause menu, which stays open and keeps the game paused; closing the panel returns to it. The title screen has no row.
- While the panel takes input, a menu of the framework's own shows the cursor and keeps the controls from the player.
- The framework draws in the game's UI pass, as DearModdingUI does: a hidden menu that is always open draws the canvases when the pass asks whether it renders, and a hook of the same function of the cursor menu draws them before the cursor while it is shown. No swapchain hook.
- MCM mods show through the same bridge, with MCM's `mcm.dll`. Their hotkeys, keyboard keys on flat, are bound in the menu while it is open over the pause menu, after a save has loaded: MCM takes a key only then.

### Names

| What | VR | Flat |
| --- | --- | --- |
| Player-facing title, Nexus page | **Mods Config FO4 VR** | **Mods Config FO4**, a page of its own |
| CMake `NAME`, DLL, F4SE plugin name | `ModsConfigFO4VR` | `ModsConfigFO4` |
| Config file | `My Games\Fallout4VR\Mods_Config\ModsConfigFO4VR\ModsConfigFO4VR.toml` | `My Games\Fallout4\Mods_Config\ModsConfigFO4\ModsConfigFO4.toml` |
| Build folder | `build/` | `build-flat/` |

The data root `Data/ModsConfigFO4`, the Papyrus script `ModsConfigFO4`, the namespace and the client header are the same in both. The client header finds the menu by its DLL name and messages it by its plugin name, so it picks them by the running game.

## How the work is done

- As in the [work plan](mod-plan.md): one commit at a time, reviewed and tried in the game before it is committed.
- Numbers: mod `M300` on, framework `F300` on, devbench `D300` on, CommonLibF4VR `C1` on, apart from the numbers `dev` takes.
- The VR build stays as it is: every commit builds and plays in VR, and the unit tests build and pass in both builds.
- Runtimes in order: 1.10.984, then 1.10.163, then 1.11.240.
- A flat address has no `VRAddressCheck`: it is tried in the flat game. On flat, an ID the Address Library lacks stops the plugin with the ID named, as on VR (C8).
- CommonLibF4VR's commits stay in the fork while the flat build is made, and are offered upstream after the release.
- devbench tries the flat commits in the game. It is one DLL for flat and VR, not run on flat before; what breaks in it is fixed in its repository, on a `flat-fo4` branch made then.

### Test setup

Steam keeps 1.11.240. 1.10.984 is a copy of the game outside Steam's folder:

1. Copy `steamapps\common\Fallout 4` to its own folder: `C:\Games\steamapps\steamapps\common\Fallout 4 - NG`.
2. Open Steam's console (`steam://open/console`) and download the depots of the 1.10.984 build, one at a time. The manifests are the community guide's ([Steam guide](https://steamcommunity.com/sharedfiles/filedetails/?id=3604110005)):
   ```
   download_depot 377160 377162 5698952341602575696
   download_depot 377160 377161 7332110922360867314
   download_depot 377160 377163 8681102885670959037
   download_depot 377160 393885 5000262035721758737
   download_depot 377160 435871 7785009542965564688
   download_depot 377160 435880 366079256218893805
   ```
   Each lands in `steamapps\content\app_377160\depot_<depot>`.
3. Copy every depot folder's content over the copy. Its `Fallout4.exe` is then file version 1.10.984.0.
4. Into the copy: F4SE 0.7.2 for 1.10.984 (Nexus mod 42147, old files, "Game Version 1.10.984 required"), its `f4se_loader.exe` and `f4se_1_10_984.dll`.
5. A Mod Organizer 2 instance for the copy, `C:\Stuff\Fallout4\MO2`, with Address Library for F4SE Plugins (Nexus mod 47327, `version-1-10-984-0.bin`) and devbench as mods, and MCM (Nexus mod 21497) for milestone 3.
6. Start it from MO2 through `f4se_loader.exe`, with Steam running.
7. The flat preset in `CMakeUserPresets.json` copies the DLL to MO2's `mods\ModsConfigFO4 - DEV`, as the VR preset copies to its own MO2.
8. The `devbench-fo4` MCP server is added to this repository (`--game fo4`, port 8930), as the devbench MCP setup has it.

Steam's install and the copy share `Documents\My Games\Fallout4` (the INIs, the saves, the F4SE logs), unless the MO2 profile keeps its own INIs and saves.

## Milestones

| # | Milestone | Done when |
| --- | --- | --- |
| 1 | Two builds | `ModsConfigFO4.dll` loads in 1.10.984 and logs, devbench answers there, and the VR build plays as before |
| 2 | The menu on flat | The config spec showcase is opened by F10 and from the pause menu, edited and saved in 1.10.984 |
| 3 | MCM and the client header | The MCM showcase is edited and saved in 1.10.984, a hotkey of it bound, and the test client registers with the menu |
| 4 | 1.10.163 | Milestones 1–3's "done when" pass in 1.10.163 |
| 5 | 1.11.x | They pass in 1.11.240, the current Steam build |
| 6 | Release | The flat package is published |

### 1. Two builds

| # | Repo | Commit |
| --- | --- | --- |
| C1 | CommonLibF4VR | Upstream's fixes merged: 1.10.984 IDs, the virtual functions' slots per runtime |
| C2 | CommonLibF4VR | CI builds the flat runtimes as well as VR |
| C8 | CommonLibF4VR | On flat, an ID the Address Library lacks fails, as on VR |
| F300 | framework | `F4CF_RUNTIME`: `VR`, the default, or `FLAT`, which builds CommonLibF4VR for 1.10.163 and 1.10.984 and leaves the VR-only files out |
| F301 | framework | The shared files build for flat, their VR lines guarded |
| F302 | framework | Flat F4SE loads the plugin: its version data, the flat runtimes accepted |
| C7 | CommonLibF4VR | The flat frame tick's call site and its callee, beside `VRHookSites` |
| F303 | framework | The frame tick on flat |
| M300 | mod | The `flat` presets build `ModsConfigFO4.dll` in `build-flat/`, with the mod's VR-only files left out |
| M301 | mod | The `My Games` folder and MCM's DLL by the running game |

- **C1**: the fork is 13 commits behind upstream, and two of them move virtual functions' slots, so `VRAddressCheck` runs and VR is played after the merge.
- **C8**: CommonLibF4VR's flat lookup takes the next ID when an ID is missing, so a missing ID is a wrong address with no word. It checks for an exact match, as VR's does, and stops the plugin with the ID named the first time it is resolved.
- **F300**: VR-only are `vrui/` and `debug/`; `vrcf/` but the binding parser and the haptic patterns, which the menu edits as values, and the debug adjuster the controllers drive; the skeleton, thumbstick, turning, weapon tracking, wands and activation spheres of `f4vr/`, but an activation sphere's config values; the Submit and player-pass hooks, the primitives and the scene depth of `render/`; the wand pointer and the VR keyboard of `imgui/`. The framework's docs say which runtime a subsystem has.
- **F301**: a shared file keeps its VR lines under `#ifdef ENABLE_FALLOUT_VR`: `ModBase`'s VR frame work and main-loop hook, the VR nodes, the haptic player, the compositor timing, the pointer and keyboard in the ImGui layer, devbench's `input` and `keyboard`. The draw-callback API is `render/DrawHost.h`: VR's host is the Submit hook, and flat has none until F304, so nothing draws there.
- **F302**: F4SE for 1.10.984 loads only a plugin that exports `F4SEPlugin_Version`, and calls no `F4SEPlugin_Query`. F4SE reads it from the DLL mapped as data, before any of its code runs, so it is constant-initialized: the framework's `pluginVersionData` builds it from the mod's name and version, and the mod exports it beside `F4SEPlugin_Query` and `F4SEPlugin_Load`. `onF4SEPluginLoad` sets up the logging the query would have. The query, which 1.10.163's F4SE still calls, accepts 1.10.163 and 1.10.984 on flat.
- **C7, F303**: VR's tick is a call in `Main::OnIdle` at a VR offset (`VRHookSites::MainUpdateFrameTick`), whose callee is VR-only code. Flat redirects the call to `DemoteQueuedActors` in the same block, VR's place in the frame: it runs once a frame on the main thread, in menus and while paused, before the frame is drawn. The site is a fixed offset per runtime (`OnIdle` + 0x210 on 1.10.163, + 0x492 on 1.10.984 and 1.11.240), its callee checked before writing, as `VRHookSites` checks VR's. The `write_call` and its thunk are VR's. F303 puts the main-loop hook and the per-frame work behind one side per runtime, VR's updating the controllers, the VR UI and debug draw, so `ModBase` keeps none of F301's guards.
- **M300**: left out of the flat build: `GameMenu`, `AdjustInWorld`, FRIK and the haptic preview. `NAME` and `FRIENDLY_NAME` come from the runtime, flat gets its own `data/config` TOML and config spec of the menu, and `CMakeUserPresets.json.template` gets a flat copy preset.
- **M301**: `My Games\Fallout4VR` is written in 5 places, and `mcm_vr.dll` in the MCM loader.

### 2. The menu on flat

| # | Repo | Commit |
| --- | --- | --- |
| C6 | CommonLibF4VR | The 1.10.984 IDs of the menus, the renderer, the menu cursor and the input devices |
| F304 | framework | render: the flat host of `DrawHost.h`, which draws in the game's UI pass |
| F305 | framework | imgui: a canvas on the screen |
| F306 | framework | imgui: the mouse, wheel, keys and typed text on flat, and the framework's menu while a canvas takes input |
| M302 | mod | `FlatGameMenu`, opened and closed by F10, its size a share of the screen's height |
| C5 | CommonLibF4VR | Scaleform: `GFx::Value`'s `GetElement` and `SetElement`, and `HasMember`'s 1.10.984 ID |
| M303 | mod | A `MODS CONFIG FO4` row in the pause menu opens it |

- **C6**: CommonLibF4VR has only 1.10.163 IDs for `UI::RegisterMenu`, `RendererData`, `IMenu`'s virtuals, the Scaleform movie loader, `MenuControls`, `MenuCursor`, `BSInputDeviceManager`, `BSScaleformManager` and `TaskQueueInterface`; upstream too. Their 1.10.984 IDs are in the 1.10.984 Address Library, matched by name to the 1.11.221 PDB.
- **F304** draws in two places of the UI pass, as DearModdingUI does: the render condition of a hidden menu of the framework's own, always open, which loads the game's `CursorMenu.swf` and hides it, and `CursorMenu`'s, swapped, so the canvases are under the cursor while it shows. Each draws into the render target the pass has bound, with the device from the renderer's data. Another mod's panel in the same pass stacks by load order.
- **F305**: the ImGui frame is built on the frame tick and drawn into the canvas's texture, as in VR; the UI pass draws the latest. Only the world quad becomes a rectangle of the screen, which the canvas's placement gives on flat, drawn 1:1.
- **F306**:
  - the cursor's position from the game's menu cursor, scaled into the canvas
  - the mouse buttons, wheel and Esc from the game's input queue, stopped there while a canvas takes input; Esc is only `controls::get().back()`, never ImGui's Escape key
  - keys and typed text from the game's window, as in VR, found by `Main::hwnd`
  - a menu of the framework's own shows the cursor and keeps the controls from the player, without pausing; if the player can still look or move while the game runs, `ControlMap::ignoreKeyboardMouse` is set while a canvas takes input
  - only a menu above the framework's own switches the canvases off, so the pause menu under the panel doesn't
  - the ImGui layer and `Controls` read an input source: VR's is the wand pointer, the thumbstick and the VR keyboard, flat's the cursor, mouse and keys; VR's side registers devbench's `input` and `keyboard`. F301's guards there go
- F304–F306 are tried with the panel shown as the game loads, a local change that isn't committed.
- **After F306**: the framework's sources move to `core/`, `vr/` and `flat/`, and CMake leaves out the other runtime's folder in place of F300's list of files. The draw host (F301, F304), the frame (F303) and the input source (F306) have both runtimes' sides by then; the few files still mixed (`PlayerNodes.h`, the haptics, `F4VRUtils`, `RenderUtils`, `FrameSampler`) split into a core part and a VR part, and `f4vr/` and `vrcf/` get names that fit what is left in them. Every mod's includes change, so it is a framework release of its own, with **Upgrading** notes.
- **C5**: the pause menu's row needs to read the list's elements, which CommonLibF4VR has no function for, and `HasMember` has only its 1.10.163 ID, so on 1.10.984 it calls a wrong address. DearModdingUI's CommonLibF4 has both functions' IDs.
- **M303** adds a `MODS CONFIG FO4` row above Settings to the pause menu's list as it opens, as DearModdingUI does: native listeners on its movie, and the row put in when it is missing and never moved. A press opens the panel over the pause menu, which stays open; closing the panel returns to it. The framework's Scaleform helpers get a native listener and a splice for it.

### 3. MCM and the client header

| # | Repo | Commit |
| --- | --- | --- |
| M304 | mod | The client header picks the DLL and plugin name by the running game |
| F307 | framework | The client header's copy, kept identical |
| M308 | mod | test: a client of the client header, built for flat |
| M305 | mod | MCM hotkeys bound on flat |

- Showing MCM mods needs no commit of its own: M301 finds `mcm.dll`, and MCM's files and Papyrus functions are the same on flat.
- **M308**: a minimal F4SE plugin built only for testing, which includes the client header and registers one page. The mod template stays VR-only.
- **M305**: a hotkey's row takes the next key pressed with its modifiers; Esc cancels and a button clears the key, and while it waits the key goes to neither MCM nor the game. A key another hotkey holds shows that hotkey on the row, and Save takes the key from it, as MCM does. On Save the menu makes MCM's own calls on `root.mcm` in the pause menu's movie: `ClearKeybind`, else `RemapKeybind`, falling back to `SetKeybind`. MCM takes a key only while the pause menu is open, and holds no keys until a save has loaded, so the rows are read-only, with a hint, while the panel was opened by F10 or before a save has loaded.

### 4. 1.10.163

- A copy of the game at 1.10.163, set up as 1.10.984's, with F4SE 0.6.23.
- No commit planned: the IDs are CommonLibF4VR's, and the VM's older `std::function` on 1.10.163 is handled there, as for VR. What fails is fixed here.

### 5. 1.11.x

| # | Repo | Commit |
| --- | --- | --- |
| C3 | CommonLibF4VR | 1.11.240, a third flat runtime: the runtime, its IDs, its Address Library file, its F4SE version data |
| C4 | CommonLibF4VR | 1.11.240 IDs of every address the framework and the mod use on flat |
| F308 | framework | The flat build includes 1.11.240 and accepts it |

- 1.11.x keeps 1.10.984's Address Library IDs for most code, in the same file format, so a 1.11.x ID is 1.10.984's unless 1.11.x gave the address a new one: mostly singletons, 30 in the library.
- **C3**: `Runtime::AE`, which a build has only with 1.10.984 (`ENABLE_FALLOUT_AE` needs `ENABLE_FALLOUT_NG`). An ID's 1.11.x value is its 1.10.984 one unless a fourth argument overrides it: `RelocationID(f4, ng, vr, ae)`, `VariantID(f4, ng, vrOffset, ae)`. `IsNG()` is true on 1.10.984 and 1.11.x, and `IsAE()` marks the few 1.11.x-only places. `RUNTIME_LATEST` stays 1.10.984 beside a new `RUNTIME_LATEST_AE`. The two bad published Address Library files of 1.11.x are refused, as the unsorted 1.10.980 one is.
- **C4**: each ID read from 1.11.240's Address Library file and checked, and the RTTI tables regenerated with `RTTIDump`. Nothing is copied from the forks that have them, which are GPL; CommonLibF4VR is MIT.
- 1.11.240 only: F4SE also loads the DLL on the other 1.11.x builds, with their IDs unchecked.

### 6. Release

| # | Repo | Commit |
| --- | --- | --- |
| M306 | mod | The flat package, and CI building both |
| M307 | mod | docs: players, mod authors, the flat Nexus page, the changelog and the design docs for flat |

- One version and one changelog for both builds: every release ships both packages, each to its own Nexus page.
- **M307**: the flat Nexus page is a page of its own, in BBCode beside `docs/nexus.md`.
