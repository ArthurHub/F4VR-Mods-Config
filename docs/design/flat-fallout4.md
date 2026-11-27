# Flat Fallout 4

Mods Config built for flat Fallout 4 as well: the same source makes a second DLL, `ModsConfigFO4.dll`, which runs in `Fallout4.exe`. The work is on the `flat-fo4` branch of this repository, of the framework submodule and of CommonLibF4VR.

## The shape

- **Two DLLs from one source.** A CMake option picks the game: VR, as today, or flat. CommonLibF4VR puts VR's struct layouts in at compile time (`PlayerCharacter` is 0x12B0 bytes in VR, 0xE10 flat), so one build can't read both games.
- **The flat DLL runs on every flat runtime.** CommonLibF4VR resolves 1.10.163 and 1.10.984 in one build, and builds for them as it is. 1.11.x, the current Steam build, is a third runtime the library doesn't have yet ([milestone 5](#5-111x)).
- **VR code stays out of the flat build.** A file that is all VR is built for VR only; a shared file guards its VR lines with `#ifdef ENABLE_FALLOUT_VR`. VR-only types (`VRPlayerNodes`, `VRHookSites`) don't exist in the flat build, so VR code that reaches one fails to compile there.
- **The menu is shared.** `ui/`, `model/`, `spec/`, `menu/` and `mcm/` don't know the game. The game side is behind `MenuPort`: `GameMenu` in VR, `FlatGameMenu` on flat.
- **One config spec for both.** A config spec's `myGames` root is the running game's `My Games` folder.

### The menu on the flat screen

- The panel's canvas is centered on the screen, two thirds of the screen's height (a key in the mod's config), and drawn at that size, so its text stays sharp.
- Mouse and keyboard: the game's cursor is ImGui's mouse, the wheel scrolls, Esc is Back, and the keyboard types into text fields. A gamepad is decided after the first release. There are no wrist buttons, haptics, FRIK quick launch or Adjust in world.
- F10 opens and closes it (a key in the mod's config), and a `Mods Config` row in the game's pause menu opens it.
- While it takes input, a menu of the framework's own shows the cursor and keeps the controls from the player. The game keeps running, as in VR, so a live change is seen.
- The framework draws in the game's UI pass, as DearModdingUI does: a hidden menu that is always open draws the canvases when the pass asks whether it renders, and a hook of the same function of the cursor menu draws them before the cursor while it is shown. No swapchain hook.
- MCM mods show through the same bridge, with MCM's `mcm.dll`. Their hotkeys, keyboard keys on flat, are bound in the menu.

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
- Runtimes in order: 1.10.984, then 1.10.163, then 1.11.x.
- A flat address has no `VRAddressCheck`: it is tried in the flat game.
- devbench tries the flat commits in the game. It is one DLL for flat and VR, not run on flat before; what breaks in it is fixed in its repository, on a `flat-fo4` branch made then.

### Test setup

Steam keeps 1.11.x. 1.10.984 is a copy of the game outside Steam's folder:

1. Copy `steamapps\common\Fallout 4` to its own folder, e.g. `C:\Games\Fallout 4 NG`.
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
4. Into the copy: F4SE 0.7.2 for 1.10.984 (Nexus mod 42147, old files, "Game Version 1.10.984 required"), Address Library for F4SE Plugins (Nexus mod 47327) with `version-1-10-984-0.bin`, and devbench. MCM (Nexus mod 21497) for milestone 3.
5. Start it with the copy's `f4se_loader.exe`, with Steam running.
6. The flat preset in `CMakeUserPresets.json` copies the DLL to the copy's `Data`.
7. The `devbench-fo4` MCP server is added to this repository (`--game fo4`, port 8930), as the devbench MCP setup has it.

The two share `Documents\My Games\Fallout4`: the INIs, the saves and the F4SE logs.

## Milestones

| # | Milestone | Done when |
| --- | --- | --- |
| 1 | Two builds | `ModsConfigFO4.dll` loads in 1.10.984 and logs, devbench answers there, and the VR build plays as before |
| 2 | The menu on flat | The config spec showcase is opened by F10 and from the pause menu, edited and saved in 1.10.984 |
| 3 | MCM and the client header | The MCM showcase is edited and saved in 1.10.984, a hotkey of it bound, and the mod template built for flat registers with the menu |
| 4 | 1.10.163 | Milestones 1–3's "done when" pass in 1.10.163 |
| 5 | 1.11.x | They pass in the current Steam build |
| 6 | Release | The flat package is published |

### 1. Two builds

| # | Repo | Commit |
| --- | --- | --- |
| C1 | CommonLibF4VR | Upstream's fixes merged: 1.10.984 IDs, the virtual functions' slots per runtime |
| C2 | CommonLibF4VR | CI builds the flat runtimes as well as VR |
| F300 | framework | `F4CF_RUNTIME`: `VR`, the default, or `FLAT`, which builds CommonLibF4VR for 1.10.163 and 1.10.984 and leaves the VR-only files out |
| F301 | framework | The shared files build for flat, their VR lines guarded |
| F302 | framework | Flat F4SE loads the plugin: its version data, the flat runtimes accepted |
| F303 | framework | The frame tick on flat |
| M300 | mod | The `flat` presets build `ModsConfigFO4.dll` in `build-flat/`, with the mod's VR-only files left out |
| M301 | mod | The `My Games` folder and MCM's DLL by the running game |

- **C1**: the fork is 13 commits behind upstream, and two of them move virtual functions' slots, so `VRAddressCheck` runs and VR is played after the merge.
- **F300**: VR-only are `vrui/`; `vrcf/` but the binding parser and the haptic patterns, which the menu edits as values; the nodes, wands, skeleton, thumbstick and activation spheres of `f4vr/`; the Submit and player-pass hooks and the scene depth of `render/`; the wand pointer and the VR keyboard of `imgui/`; the compositor timing of `perf/`. The framework's docs say which runtime a subsystem has.
- **F302**: F4SE for 1.10.984 loads only a plugin that exports `F4SEPlugin_Version`. The framework fills its data from `ModBase::Settings`, and a mod exports it beside `F4SEPlugin_Query` and `F4SEPlugin_Load`.
- **F303**: VR's tick is a call in the main loop at a VR offset (`VRHookSites::MainUpdateFrameTick`). Flat needs a call with 1.10.163 and 1.10.984 IDs, found in the flat game.
- **M300**: left out of the flat build: `GameMenu`, `AdjustInWorld`, FRIK and the haptic preview. `NAME` and `FRIENDLY_NAME` come from the runtime, and `CMakeUserPresets.json.template` gets a flat copy preset.
- **M301**: `My Games\Fallout4VR` is written in 5 places, and `mcm_vr.dll` in the MCM loader.

### 2. The menu on flat

| # | Repo | Commit |
| --- | --- | --- |
| F304 | framework | render: the flat host, which draws in the game's UI pass |
| F305 | framework | imgui: a canvas on the screen |
| F306 | framework | imgui: the mouse, wheel, keys and typed text on flat, and the framework's menu while a canvas takes input |
| M302 | mod | `FlatGameMenu`, opened and closed by F10, its size a share of the screen's height |
| M303 | mod | A `Mods Config` row in the pause menu opens it |

- **F304** takes the game's D3D11 device from its renderer data and draws into the render target the UI pass has bound.
- **F305**: the frame is drawn into the canvases' texture as in VR; the canvas's part of it is then drawn in a rectangle of the screen, which its placement gives on flat.
- **F306**: the cursor's position from the game's menu cursor; the buttons, wheel and keys from the game's input events, taken from the game while the canvas uses them; typed text from the window's messages, as in VR. Esc is `controls::get().back()`.
- F304–F306 are tried with the panel shown as the game loads, a local change that isn't committed.
- **M303** adds a row to the pause menu's Scaleform list, as DearModdingUI does, with the framework's Scaleform helpers; a helper it needs and they lack is added there.

### 3. MCM and the client header

| # | Repo | Commit |
| --- | --- | --- |
| M304 | mod | The client header picks the DLL and plugin name by the running game |
| F307 | framework | The client header's copy, kept identical |
| M305 | mod | MCM hotkeys bound on flat |

- Showing MCM mods needs no commit of its own: M301 finds `mcm.dll`, and MCM's files and Papyrus functions are the same on flat.
- The mod template built for flat is the client M304 is tried with.
- **M305**: a hotkey's row takes the next key pressed, with its modifiers. MCM changes a key through `SetKeybind`, a Scaleform function it adds to the pause menu's movie for its own menu, and keeps the keys in `Data\MCM\Settings\Keybinds.json`. The commit starts by reading flat MCM for how to reach `SetKeybind` while the pause menu isn't open.

### 4. 1.10.163

- A copy of the game at 1.10.163, set up as 1.10.984's, with F4SE 0.6.23.
- No commit planned: the IDs are CommonLibF4VR's, and the VM's older `std::function` on 1.10.163 is handled there, as for VR. What fails is fixed here.

### 5. 1.11.x

| # | Repo | Commit |
| --- | --- | --- |
| C3 | CommonLibF4VR | 1.11.x, a third flat runtime: the runtime, its IDs, its Address Library file, its F4SE version data |
| C4 | CommonLibF4VR | 1.11.x IDs of every address the framework and the mod use on flat |
| F308 | framework | The flat build includes 1.11.x and accepts it |

### 6. Release

| # | Repo | Commit |
| --- | --- | --- |
| M306 | mod | The flat package, and CI building both |
| M307 | mod | docs: players, mod authors, the flat Nexus page, the changelog and the design docs for flat |

- **M307**: the flat Nexus page is a page of its own, in BBCode beside `docs/nexus.md`.
