# MCM `SetKeybind` on flat: findings

Research for [05-mcm-setkeybind-on-flat](../issues/05-mcm-setkeybind-on-flat.md), for M305 of the [plan](../../../docs/design/flat-fallout4.md).

Confidence tags: **[high]** read in source or confirmed by two sources; **[medium]** read in source but depends on game or engine behavior not tested; **[low]** an inference. Paths under `f4mcm/` are the flat MCM source (`reg2k/f4mcm` at `e2b187a`, shallow-cloned to `C:\Users\tepar\AppData\Local\Temp\claude\f4mcm-research-05\f4mcm`). The VR source is `Modding-Reference\F4VR\github-repos\gold\Neanka-mods-repo\f4mcm\src`.

## Short answer: how M305 changes a hotkey

- **Make the calls MCM's own menu makes, on MCM's `root.mcm` code object in the pause menu's movie.** No key: `ClearKeybind(modName, id)`. A key: `RemapKeybind(modName, id, keycode, modifiers)`, and `SetKeybind(...)` with the same arguments when Remap returns false because the hotkey had no key. Pass `keycode` and `modifiers` as GFx **Int** values; the handlers reject anything else. Make the calls from F4SE's UI task (`AddUITask`) while `PauseMenu` is open. These calls change MCM's live keybind table at once, and MCM saves the table to `Keybinds.json` itself.
- **`root.mcm` exists only while a `MainMenu.swf` movie is loaded.** That means the pause menu during a game, or the main menu at the title screen. MCM takes a key change through no other channel: it has no Papyrus native for keys, no F4SE message listener of its own, and no export. It reads `Keybinds.json` only when a save loads.
- **So a change made with the pause menu open applies at once.** If Mods Config opens from its pause-menu row and the pause menu stays open beneath it, pushes are immediate. A change made while the pause menu is closed (F10 during play) is queued and pushed the next time `PauseMenu` opens. Until then, the old key still runs the hotkey.
- **Also write the key to `Data\MCM\Settings\Keybinds.json`, in MCM's format.** This is the fallback when a save loads before a queued push goes through. On its own the file write is neither live nor lasting: MCM ignores the file until a save loads, and its next save of its table overwrites the file.
- **Push only after a save has loaded this session.** In a new game, and at the title screen, MCM's in-memory table is empty. A push there would make MCM's next save of the table write a `Keybinds.json` holding only the pushed keys, which wipes every other mod's keys. This comes from reading the source and isn't tested in the game yet ([open point 3](#open-points-that-need-a-decision)).
- **Prior art:** F4SEMenuFramework, a released flat mod, does exactly this (`MCMLiveSync`). It also opens its menu over the pause menu so its pushes apply at once.

## Evidence

### 1. Flat MCM's source and versions

- The flat `mcm.dll` source is **`reg2k/f4mcm`**: the `master` branch, last commit `e2b187a` on 2024-06-04, "Update for game version 1.10.984", `PLUGIN_VERSION 9`, `"1.40"`. **[high]** (`gh api repos/reg2k/f4mcm`, `f4mcm/src/Config.h`)
- The VR source is the same code. `diff` shows changes only in `MCM.cpp` (log path, runtime check, `F4SEPlugin_Version`), `Globals.cpp` and `Utils.cpp` (signatures), `Config.h`, and **one line in `ScaleformMCM.cpp`: the movie name** (`Interface/MainMenu.swf` on flat, `Interface/world_MainMenu.swf` in VR) plus the injected swf (`MCM.swf` / `MCM_VR.swf`). `MCMKeybinds.*`, `MCMSerialization.*`, `MCMInput.*` and `PapyrusMCM.*` are byte-identical. **[high]**
- From 1.38 (`d3c677a`, 1.10.162) to 1.40 (`e2b187a`), only `MCM.cpp`, `Utils.cpp`, `Config.h` and the project file changed. The keybind and Scaleform code is the same. **[high]** (`gh api repos/reg2k/f4mcm/compare/d3c677a...e2b187a`)
- The official Nexus files (mod 21497), from the Nexus GraphQL API `modFiles`: **[high]**

  | MCM | Runtime | Source public |
  | --- | --- | --- |
  | 1.39 (2019-12-07) | 1.10.163 | No commit of its own; the 1.38 source rebuilt **[low]** |
  | 1.40 BETA 2 (2024-05-14) | 1.10.984 | `reg2k/f4mcm` master |
  | 1.41 / 1.41h1 / 1.42 / 1.43 (Nov–Dec 2025) | 1.11.137 / .159 / .169 / .191 | No |
  | 1.11.221 (2026-06-08) | 1.11.221 | No |
  | 1.11.240 (2026-09-01, main file) | 1.11.240 | No |

- Each MCM build loads on one runtime only: `F4SEPlugin_Query` refuses a `runtimeVersion` other than `SUPPORTED_RUNTIME_VERSION`. So each flat runtime has its own `mcm.dll`. **[high]** (`f4mcm/src/MCM.cpp`)
- The 1.11.x builds are closed source. `MANOLOV02/MCM-Fork`, branch `mcm-1.11.240`, rebuilds the 1.40 source for 1.11.240 by changing one signature. It says its exported surface is "the same set of 31 Scaleform functions and 11 Papyrus natives as upstream", and that the official release code is now 11. That is a third party's statement about its own fork. **[medium]** Why the 1.11.x builds must still provide `root.mcm`: the shipped `MCM.swf` finds MCM through `stage.getChildAt(0)["mcm"]` and calls `SetKeybind` / `RemapKeybind` / `ClearKeybind` / `GetKeybind` on it, so any build whose menu works keeps that contract. **[medium, inference from the swf source]**
- Other forks (`Kraken-collab/f4se_mcm`, `clayne`, `rethesda`, ...) are 1.40 or older and change nothing in the keybind code. The `hotkey-fix` branch (2021, never merged into master) only stops hotkeys from firing during text input (`allowTextInput`). **[high]**

### 2. Which movie gets `root.mcm`, and how long it lives

- `ScaleformMCM::RegisterScaleform` (`f4mcm/src/ScaleformMCM.cpp:927`) runs for every movie F4SE initializes. It acts only when `root.loaderInfo.url == "Interface/MainMenu.swf"` (`:941`). It then creates `root.mcm`, registers the 31 functions on it (`RegisterFuncs`, `:754`), and loads `MCM.swf` into `root.Menu_mc` through `root.mcm_loader`. **[high]**
- F4SE calls each plugin's Scaleform callback from `ScaleformInitHook_Install`, a hook inside the game's movie loading, once per loaded movie. It also gives each plugin an object at `root.f4se.plugins.<name>`, which MCM leaves empty. **[high]** (`ianpatt/f4se` `f4se/Hooks_Scaleform.cpp`)
- On flat, `MainMenu.swf` is the movie of `PauseMenu`, and also of the title screen's `MainMenu`. MCM's own code reads the `PauseMenu` menu's movie (`ProcessKeyEvent`, `RefreshMenu`: `GetMenu("PauseMenu")->movie`). DearModdingUI and F4SEMenuFramework both edit `root.Menu_mc.MainPanel_mc.List_mc` in the movies of `PauseMenu` and `MainMenu`, and F4SEMenuFramework's host-swf check is `"Interface/MainMenu.swf"` on OG, NG and AE. **[high]**
- **`root.mcm` doesn't outlive the pause menu.** The movie is created when the menu opens. MCM re-creates `root.mcm` and re-injects `MCM.swf` on each opening (MCM's AS3 `makeChanges` runs per load). MCM's own C++ checks `IsMenuOpen("PauseMenu")` before touching the movie, and F4SEMenuFramework's notes say "root.mcm only exists while the PauseMenu movie is loaded". **[medium-high]**: not checked in the game; [devbench check](#open-points-that-need-a-decision).
- During play, no `MainMenu.swf` movie is loaded, so there is no `root.mcm` to call. **[medium-high]**

### 3. MCM's keybind functions

All handlers take the `KeybindManager` lock themselves. **[high]** (`f4mcm/src/ScaleformMCM.cpp`, `MCMKeybinds.cpp`)

| Function (on `root.mcm`) | Effect | Returns false when |
| --- | --- | --- |
| `SetKeybind(modName:String, id:String, keycode:int, modifiers:int)` (`:525`) | `RegisterKeybind`: loads the hotkey's definition from `Data\MCM\Config\<modName minus .esp>\keybinds.json` (its action form must resolve), then `m_data[key] = definition`, dirty | The id isn't in that file, or its form is missing. **It doesn't remove the hotkey's old key**: SetKeybind alone leaves two keys running the hotkey |
| `RemapKeybind(modName, id, keycode, modifiers)` (`:681`) | Moves the hotkey's existing entry to the new key, dirty | The hotkey has no key, or the new key equals the old one |
| `ClearKeybind(modName, id)` or `(keycode, modifiers)` (`:645`) | Erases the entry, dirty | The hotkey had no key |
| `GetKeybind(modName, id)` or `(keycode, modifiers)` (`:469`) | An object `{keycode, modifiers, keybindType, keybindID, keybindName, modName, ...}`: `keybindType` 1 is another MCM hotkey, 2 a game control in the Gameplay context | Needs `args->movie` to create the object |
| `OnMCMClose()` (`:84`) | `CommitKeybinds()`: writes `Keybinds.json` if dirty | |

- Every argument is type-checked (`GetType() != kType_Int` returns). In CommonLibF4VR, `Scaleform::GFx::Value(std::int32_t)` is `kInt`, but `Value(std::uint32_t)` is `kUInt` and is rejected. **[high]**
- `SetKeybindEx` exists as a class but **isn't registered**: changelog 0.64 says "Removed MCM Scaleform functions: SetKeybindEx". **[high]**
- `m_data` is a `std::map<Keybind, ...>` keyed by key. So `SetKeybind` / `RemapKeybind` onto a key that another hotkey already has **silently takes it from that hotkey**. **[high]**
- MCM's menu (`Neanka/MCM_0.1_AS3` `mcm/OptionsList.as`, hotkey `VALUE_CHANGE`): for keycode 0 it calls `ClearKeybind`. Otherwise it calls `GetKeybind(mod, id)` and then `RemapKeybind` when the returned `keycode != 0`, else `SetKeybind`. Before that, `hotkey_conflict_window` asks "Replace" (`$MCM_KEYBIND_IN_USE_MOD`) or "Bind anyway" (`$MCM_KEYBIND_IN_USE_GAME`). **[high]**

### 4. When MCM reads and writes `Keybinds.json`

- **Read:** only in the F4SE serialization `LoadCallback` (`f4mcm/src/MCMSerialization.cpp:24`), after `RevertCallback` (`:18`) has cleared `m_data`. Startup doesn't read it (`F4SEPlugin_Load` reads only the ModSettings, `MCM.cpp:203`). **[high]**
- F4SE calls the revert callbacks from its VM revert hook, which runs on a new game and on a load. It calls the load callbacks from `LoadRegistrationHandles_Hook` only when a save loads and its co-save opens; if the co-save opens, every plugin's load callback runs, including plugins without data in it. **[high]** (`ianpatt/f4se` `f4se/Serialization.cpp` `HandleLoadGlobalData`, `f4se/Hooks_Papyrus.cpp`; the same code in f4sevr)
- So during a **new game** (and at the title screen) MCM's `m_data` is empty: no MCM hotkey runs until a save is loaded. Binding one key in that state makes the next `CommitKeybinds` write a file holding only that key. This applies to MCM's own menu as well. **[medium]**: derived from the two sources and not tested in the game; no user report found.
- **Write:** `CommitKeybinds()` writes `Data\MCM\Settings\Keybinds.json` (format `{"version":1,"keybinds":[{keycode, modifiers, modName, id}]}`) only while `m_keybindsDirty` is set. It runs from `OnMCMClose` (MCM's menu closing) and from the serialization `SaveCallback` (each game save). **[high]**
- **No reload trigger.** Nothing reads the file during a session other than a save load. A plugin can't run MCM's load callback, which F4SE keeps internally. **Writing the file alone** therefore takes effect only at the next save load. Before that, MCM's next commit, if it becomes dirty, overwrites the file with its unchanged table. **[high]**

### 5. Other ways in: none

- **Papyrus:** 11 natives (`IsInstalled`, `GetVersionCode`, `RefreshMenu`, `Get/SetModSetting{Int,Bool,Float,String}`). None of them touches keys. **[high]** (`f4mcm/src/PapyrusMCM.cpp`, `dist/Data/Scripts/Source/User/MCM.psc`)
- **F4SE messaging:** MCM registers one listener, on sender `"F4SE"`, for `kMessage_GameLoaded` (`MCM.cpp:192`). It never listens to other plugins, so a message to it is dropped. **[high]**
- **Exports:** `exports.def` has `F4SEPlugin_Query` and `F4SEPlugin_Load`, and 1.40 adds `F4SEPlugin_Version` as data. Nothing else. **[high]**
- **Input:** `MCMInput` runs a hotkey on a key-down only while `UI::numPauseGame == 0` (`MCMInput.cpp:87`), from a handler pushed onto `MenuControls`'s list at `kMessage_GameLoaded`. **[high]**
- 1.41 and later: the binaries aren't read, so another channel can't be ruled out, but the Nexus changelogs say only "updated for game version …". **[medium]**

### 6. Calling `SetKeybind` from a plugin, and under what conditions

- **In a movie that has `root.mcm`:** `UI::GetSingleton()->GetMenu("PauseMenu")->uiMovie->asMovieRoot`, then `GetVariable(&mcm, "root.mcm")`, then `mcm.Invoke("RemapKeybind", &ret, args, 4)`. This works under these conditions:
  - `PauseMenu` is open.
  - The call runs on the UI task. F4SEMenuFramework uses `F4SE::GetTaskInterface()->AddUITask` and re-checks `GetMenuOpen("PauseMenu")` inside the task.
  - Keycode and modifiers are `kInt`.
  - A save has loaded this session (section 4).
  - The mod's `keybinds.json` has the id and its form resolves.

  The title screen's `MainMenu` movie has `root.mcm` too, but `m_data` is empty there, so don't push into it. **[high]** for the mechanism; F4SEMenuFramework ships it.
- **In another loaded movie:** not possible. MCM gates on the URL only, and F4SE's callback list is internal. A function value from the pause menu's movie belongs to that movie and can't be invoked after it closes. **[high]**
- **Calling the handler's C++ directly** (rejected, listed for completeness): the handlers keep no state. F4SE's `RegisterFunction` caches one instance per type in `mcm.dll`'s own `g_functionHandlerCache`, which isn't exported. The VR `mcm_vr.dll` builds carry RTTI names (`.?AVSetKeybind@ScaleformMCM@@`, `.?AVRemapKeybind@...`, `.?AVClearKeybind@...`, `.?AVOnMCMClose@...`, read from both installed copies), so a plugin could find the vtable through RTTI and call slot 1 (`Invoke`; slot 0 is the destructor of `GRefCountImplCore`) with a hand-made `Args` (`result, movie, thisObj, unk18, args, numArgs`). That works in any game state, but it depends on MCM's internals: the flat 1.11.x builds aren't checked for RTTI, and a released mod would be patching another mod. **[medium]**
- **Opening the pause menu for a frame** to push: rejected. The player sees and hears it, the game pauses, and every `PauseMenu` listener fires. **[low]**, not tried.
- **A hidden menu of the mod that loads `MainMenu.swf`:** MCM would give it a `root.mcm` that lives all session. Rejected: it loads the whole pause menu movie a second time with no code object of its own behind it. **[low]**, speculative.

### 7. How the menu reads MCM hotkeys today

- `src/mcm/McmKeybinds.cpp` (`McmKeybinds::load`) reads the mod's `Config\<mod>\keybinds.json`, which says which hotkeys exist (only actions `CallFunction`, `CallGlobalFunction`, `RunConsoleCommand`, `SendEvent`). It then reads `Settings\Keybinds.json` for the player's keys, taking the lower key when a hotkey has two. It runs when a mod is selected and when another page is shown (`McmStore.cpp:368`), and the hotkey row is read-only (`McmStore::hotkey`, `docs/design/mcm.md`). **[high]**
- For M305: after a push, MCM's table has the new key but the file still has the old one, until MCM's next commit (a game save or MCM's menu closing). The row must show the key Mods Config pushed or wrote, not just re-read the file. Writing the file at the same time as the push (as F4SEMenuFramework does) keeps the two in step. **[high]**
- Key codes MCM uses: keyboard keys are Windows virtual-key codes (FO4's `ButtonEvent` id code, which MCM takes as is). Mouse buttons are `256 + id` (Mouse3–Mouse8 = 258–263; MCM refuses Mouse1, Mouse2 and the wheel). Gamepad buttons are 266 and up. The modifier bits, Shift 1, Ctrl 2 and Alt 4, come from `GetAsyncKeyState`. **[high]** (`MCMInput.cpp`, `src/mcm/McmKeybinds.cpp`)

### 8. Prior art on flat

- **F4SEMenuFramework** (`DCCStudios/F4SEMenuFramework`, in the library at `Fallout4\github-repos\gold\F4SEMenuFramework`, `b031040`). `src/MCM/MCMLiveSync.cpp` queues `{modName, id} -> (vk, modifiers)`. Its per-frame `OnFrame` waits for `GetMenuOpen("PauseMenu")` and then runs `AddUITask`. The flush calls `ClearKeybind` for vk 0, and otherwise `RemapKeybind(...) || SetKeybind(...)` with `int32` arguments. Each push gets a status of `Pending`, `Synced` or `Failed`, shown next to the hotkey button. It also pulls rebinds made in MCM's own menu with `GetKeybind`, before pushing. `MCMKeybindStore` writes `Keybinds.json` on every change as the load-time fallback. Its `OpenMenu` from the pause-menu row **keeps the pause menu open** "so MCMLiveSync can push hotkey rebinds into the running MCM immediately" (`src/PauseMenuButton.cpp:56`). **[high]** for what the code does; how it behaves in the game is not checked here.
- **DearModdingUI's MCM bridge** shows hotkeys read-only: "No registration, dispatch, conflict handling or writing; MCM still dispatches the keys." (`Fallout4\Analysis\gold\DearModdingUI_MOD_OVERVIEW.md`, MCM Bridge). **[high]**

### 9. Differences between runtimes and between VR and flat

- The three flat runtimes don't differ in the keybind path: the same functions, the same movie (`Interface/MainMenu.swf`) and the same `Keybinds.json` timing. Only `mcm.dll` differs: 1.39, 1.40 and a 1.11.x build. Mods Config calls nothing but Scaleform members by name, so it needs no address and no MCM version check. Checking `mcm.HasMember("RemapKeybind")` before calling covers an unexpected build. **[high]** for 1.10.163 and 1.10.984 (source); **[medium]** for 1.11.x (closed builds).
- Mods Config needs these CommonLibF4VR pieces, all of which it has: `UI::GetMenu`, `IMenu::uiMovie`, `GFx::Movie::GetVariable` / `Value::Invoke`, and `F4SE::TaskInterface::AddUITask` (`CommonLibF4/include/F4SE/Interfaces.h:383`). DearModdingUI reads `IMenu::uiMovie` at `+0x40` on OG, NG and AE from one DLL. **[high]**
- VR: the same calls on `world_MainMenu.swf`'s `root.mcm` would work in MCM VR too. The plan keeps VR hotkeys read-only. **[high]** for the source.

## Open points that need a decision

1. **The pause menu under the flat menu.** If Mods Config opens from its row and the pause menu stays open beneath it, as F4SEMenuFramework does, a hotkey change is pushed at once, but the game is paused in that path. That conflicts with the plan's "the game keeps running" for a menu opened from the row. If the row closes the pause menu, every hotkey change waits for the pause menu's next opening. This decision belongs with ticket 04.
2. **What the player sees while a push waits** (F10 during play). Options: a status on the row (pending / applied / failed, as F4SEMenuFramework does), a help line ("takes effect when the pause menu next opens"), or nothing.
3. **New game and title screen.** Check in the game that an MCM hotkey doesn't run in a new game until a save is loaded (section 4). If that holds, decide: in that state, write the file only, or disable the row with a reason. If it doesn't hold, the source reading is wrong somewhere, and the guard needs re-checking.
4. **Conflicts.** `RemapKeybind` / `SetKeybind` silently take a key from another MCM hotkey. MCM's menu asks first: "Replace" for another MCM hotkey, "Bind anyway" for a game control. Mods Config would need its own check, from `Keybinds.json` plus the game's control map in the Gameplay context, since `GetKeybind` is only reachable with the pause menu open.
5. **Taking the next key press.** While the row waits for a key, the press must not reach MCM's `MCMInput`. Otherwise binding a key that already belongs to a hotkey also runs that hotkey, because MCMInput runs hotkeys while `numPauseGame == 0` and Mods Config's menu doesn't pause. This goes with ticket 03's input design.
6. **The session model.** As with a ModSetting, a hotkey change is made at once and is part of the session: Revert pushes the old key back (or clears it), and Keep does nothing more. Mods Config's own synthetic `PauseMenu` open/close events (`sendPauseMenuEvents`) must not trigger the flush: the flush re-checks `UI::GetMenuOpen("PauseMenu")` inside the UI task.
7. **Checks in the game before M305 is written:**
   - With devbench in 1.10.984, confirm that `root.mcm` is gone after the pause menu closes and comes back on the next opening.
   - Confirm that a `RemapKeybind` push changes the key that runs the hotkey, with no reload.
   - Confirm that MCM's next game save writes the pushed key to `Keybinds.json`.
   - Later, confirm that the 1.11.x MCM build still has `root.mcm.RemapKeybind`.
