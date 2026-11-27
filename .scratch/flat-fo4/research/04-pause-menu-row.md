# Pause-menu row: findings

Ticket: [04-pause-menu-row.md](../issues/04-pause-menu-row.md). For M303 of [flat-fallout4.md](../../../docs/design/flat-fallout4.md).

Confidence tags: **sure** (read in source or in the decompiled game swf), **likely** (a shipped mod does it on these runtimes, or a strong inference from source), **unverified** (needs a try in the game).

## Short answer: what M303 does

- **When.** On `MenuOpenCloseEvent` for `PauseMenu` opening, through the mod's existing `GameMenusHandler` callback. Attach only when `UI::GetMenu("PauseMenu")` has a `uiMovie`, which skips the mod's own fake `PauseMenu` event (`src/mcm/McmEvents.cpp`). Attach two native AS3 listeners, each a `Scaleform::GFx::FunctionHandler` wrapped by `Movie::CreateFunction`:
  - `enterFrame` on `root`;
  - `BSScrollingList::itemPress` on `root.Menu_mc`.

  Skip the title screen (`MainMenu`), which loads the same swf. The F4SE Scaleform interface is not needed.
- **Where the row goes.** The list is `root.Menu_mc.MainPanel_mc.List_mc`, an AS3 array `entryList` of `{ text, index, disabled? }`. A row's action follows its `index` field, not its position. On each `enterFrame`, if the list isn't empty and holds no entry with our `index`, the handler:
  - creates an object `{ text: "MODS CONFIG", index: <ours> }`;
  - `splice`s it in directly above the Settings row (`index == 8`);
  - calls `List_mc.InvalidateData()`.

  It adds the row only when it is missing and never moves a row that is already there. Moving our row would make it fight DearModdingUI's fixed-slot row on every frame. The check repairs the list after vanilla rebuilds it (`InitList` clears it) and after MCM's late insert.
- **The click.** When an `itemPress` comes in, the handler reads `args[0].target.selectedEntry`. If its `index` is ours (and it isn't `disabled`):
  1. close the pause menu with `root.Menu_mc.BGSCodeObj.onContinuePress()`, the call vanilla makes when Esc is pressed there;
  2. set a pending flag;
  3. open Mods Config (M302's open) when the `PauseMenu` closing event arrives.

  Once the pause menu closes, the game runs again, as the plan wants for live preview.
- **Missing from CommonLibF4VR and the framework.**
  - `GFx::Value::GetElement` is missing from CommonLibF4VR. It is needed to read a row by position. DearModdingUI's fork has the IDs OG `827659`, NG `2285881`. Adding it is a CommonLibF4VR commit (a `C` number), or M303 uses a workaround (open point 6).
  - The framework's `ScaleformUtils` needs three helpers: a native function attached as an AS3 event listener, finding a list entry by its `index`, and splicing an entry in at a slot.
  - Nothing else: no swf, no hook, no address of the mod's own.
- **MCM is not a problem.** MCM's row is `index: 100`. It inserts it once, a few frames after the pause menu opens, at `iPosition:Main` (default 1, the top), and it ignores presses on other rows. Both rows show, MCM's at the top and ours above Settings.
- **Traps.**
  - Don't call `Value::HasMember` on NG: CommonLibF4VR gives it only the OG ID, and on flat a missing ID resolves silently to a wrong address.
  - Don't call `VisitMembers` on OG until C1 merges upstream's OG ID.

## Evidence

### 1. The movie and the list

- **The pause menu's movie is `Interface/MainMenu.swf`, shared with the title screen.** Sure. Five independent signs:
  - `Fallout4 - Interface.ba2` of 1.10.984 holds `Interface\MainMenu.swf` and no `PauseMenu.swf`. Listed with a BA2 reader, 911 files.
  - MainMenu.swf's document class `MainMenu` has `set pauseMode(Boolean)`, set by the game, and builds pause rows when `PauseMode` is true (decompiled with JPEXS 26.5.0).
  - Flat MCM's `RegisterScaleform` injects into the movie whose `root.loaderInfo.url` is `"Interface/MainMenu.swf"`, then checks `Menu_mc["PauseMode"]` (reg2k/f4mcm `src/ScaleformMCM.cpp:941`, `MCM.swf` `mcm.MCM_Main.enterFrameHandler`).
  - DearModdingUI's fork has `PauseMenu` and `MainMenu` both deriving from `StartMenuBase` (`Depends/commonlibf4/include/RE/P/PauseMenu.h`, `RE/M/MainMenu.h`).
  - DearModdingUI uses the same paths for both menus (`StartMenuEntry.cpp:38`).
- **Paths.** Sure (decompiled 1.10.984 swf). `root` is the stage's first child. `root.Menu_mc` is the `MainMenu` instance. Its rows are `Menu_mc.MainPanel_mc.List_mc`, a `MainMenuList extends BSScrollingList`, with `entryList:Array`, `selectedEntry`, `InvalidateData()`, `ClearList()` and `numListItems = 9` (`MainMenu_fla/MainListPanel_49.as:54`).
- **Rows.** Sure. `MainMenu.InitList` calls `List_mc.ClearList()` and then pushes plain objects `{ "text":"$SAVE", "disabled":..., "index":this.SAVE_INDEX }`. The `index` values:

  | index | Row | Pause | Title |
  |---|---|---|---|
  | 0 | Continue | | yes |
  | 1 | New | | yes |
  | 2 | Quicksave | if enabled | |
  | 3 | Save | yes | |
  | 4 | Load | yes | yes |
  | 5 | Add-ons (DLC) | | yes |
  | 6 | Bethesda.net | if shown | if shown |
  | 7 | Help | yes | |
  | 8 | Settings | yes | yes |
  | 9 | Credits | | yes |
  | 10 | Quit | if shown | if shown |
  | 11 | Platform help | if shown | if shown |
  | 12 | Creation Club | | if shown |
  | 13 | PS5 transfer | | PS5 |
  | 14 | Installed content | yes | |

  On PC the pause order is [Quicksave], Save, Load, Installed content, [Bethesda.net], Help, Settings, Quit.
- **A press dispatches on `selectedEntry.index`, and vanilla does nothing for an unknown one.** Sure.
  - `MainMenu` listens to `BSScrollingList::itemPress` on itself (`MainMenu.as:305`), which the list's press bubbles to.
  - `onListItemPress` runs only when `!bSavingSettings && BGSCodeObj.IsMainMenuReady()`. It then calls `onMainListItemPress`.
  - `onMainListItemPress` plays the cancel sound if the entry is `disabled`, `Spinner_mc` is visible or settings changed. Otherwise it resets its sub-panels and runs `switch(selectedEntry.index)` with no `default`.
- **Vanilla greys rows by position, not by entry.** Sure. `UpdateModsIndex`, `UpdateCClubIndex` and `UpdateInstalledContentIndex` (`MainMenu.as:2650-2678`) store a position and set that list clip's `alpha`. A row spliced above Bethesda.net or Installed content therefore leaves the grey on the wrong row. MCM's default top position already does this. A row above Settings shifts only Settings and Quit, and it leaves the selection alone (the default selection is Save).
- **The list is rebuilt while open.** Sure. `InitList` runs again from AS3 after deleting the last save (`MainMenu.as:1022`, `:1182`), and the game calls it as well. Each rebuild drops every spliced row, MCM's included. MCM doesn't put its row back; DearModdingUI checks every frame.
- **When a plugin can add a row.** Likely. The entries don't exist when the movie loads: the game calls `InitList` after creating the menu. That is why MCM waits for its loaded swf's first `ENTER_FRAME`, and DearModdingUI waits while `entryList` is empty (`StartMenuEntry.cpp:400-402`). The pause menu is created on each open, so its movie loads on each open:
  - F4SE calls the plugin Scaleform callbacks from inside `BSScaleformManager::LoadMovie` (ianpatt/f4se `f4se/Hooks_Scaleform.cpp:446-499`, hook at `LoadMovie+0x226`);
  - MCM injects on each load;
  - DearModdingUI attaches on each opening event.
- **1.11.240** (milestone 5). Sure. Its MainMenu.swf has the same structure. The only change is a Creation Club row in pause mode when Bethesda.net and Creation Club are both shown.
- **1.10.163.** Likely, not decompiled: no 1.10.163 copy exists on this machine. MCM's 2019 build for 1.10.162 splices into the same path, and DearModdingUI runs on OG with the same paths and Settings index.

### 2. How DearModdingUI adds its row and gets the press

Source: `Modding-Reference\Fallout4\github-repos\gold\DearModdingUI\src\Platform\menus\StartMenuEntry.cpp` (HEAD `7e09986`, 2026-11-18), registered from `src\Main.cpp:122,137`. The analysis is `Fallout4\Analysis\gold\DearModdingUI_RE_REFERENCE.md`, "Native Menus" and "Start-menu entries". Sure for what the code does; likely that it works on OG, NG and AE, which DearModdingUI ships for.

- **Registration.** At `kPostPostLoad`, and again at `kGameDataReady`, DearModdingUI calls `UI::RegisterSink<MenuOpenCloseEvent>`, plus an immediate attach if `MainMenu` is already open. On each `opening` event for `PauseMenu` or `MainMenu`, it takes `ui->GetMenu(name)->uiMovie` and attaches its listeners. It calls the movie directly from the event sink, on the game thread.
- **Listeners.** `Movie::CreateFunction(&value, &handler)` wraps a `FunctionHandler` subclass. AS3 `addEventListener` is then invoked with it:
  ```cpp
  menu.Invoke("addEventListener", nullptr, { "BSScrollingList::itemPress", press }); // on root.Menu_mc
  root.Invoke("addEventListener", nullptr, { "enterFrame", frame });               // on root
  ```
  The handlers are static objects. Scaleform's refcount starts at 1 (`CommonLibF4VR ... SF_RefCount.h:21`, `refCount{ 1 }`), so the AS3 function's release never deletes them ("The initial reference keeps these alive", `StartMenuEntry.cpp:463`).
- **Splice.** It creates an object with `movie.CreateObject(&entry)`, `SetMember("text", ...)` and `SetMember("index", 0x444D)`, then calls `entries.Invoke("splice", nullptr, { slot, 0, entry })` and `List_mc.Invoke("InvalidateData")`.
  - It finds rows by reading each element's `index` (`GetArraySize` plus `GetElement`).
  - The slot comes from `iPauseMenuEntryPosition`, which is fixed.
  - After moving rows it re-sends `UpdateModsIndex(6)`, `UpdateCClubIndex(12)` and `UpdateInstalledContentIndex(14)` with their new positions.
  - It also sets `disabled` and `waitingForLoad` until its host is ready.
- **Every frame.** The `enterFrame` handler re-checks each frame but works only when its row or Settings moved ("MCM splices its row in a few frames after the pause menu opens", `StartMenuEntry.cpp:184`).
- **The click.** `PressHandler::Call` reads `args[0].target.selectedEntry`. If its `index` is `0x444D`, it calls `ToggleHost` directly.
- **DearModdingUI keeps the pause menu open underneath.** Its own `DearModdingUI_Menu` is `kModal | kUsesCursor | kPausesGame`, and Escape closes only DearModdingUI (CHANGELOG line 42). Its defaults are `sPauseMenuEntry = "off"` and `bSettingsMenuEntry = true`, so out of the box its row is in the Settings submenu, not the pause list (`data/F4SE/Plugins/DearModdingUI.toml:16-20`). `"replace"` mode removes MCM's row.

### 3. MCM in the same menu

Sources: flat MCM `reg2k/f4mcm`, last commit `e2b187a8` (2024-06-04, "Update for game version 1.10.984"). That covers `src/ScaleformMCM.cpp`, `src/MCM.cpp` and `dist/Data/Interface/MCM.swf`, decompiled with JPEXS. The VR copy is `Modding-Reference\F4VR\github-repos\gold\Neanka-mods-repo\f4mcm\src\ScaleformMCM.cpp:927`. Sure.

- **C++ side.** MCM registers with `g_scaleform->Register("f4mcm", RegisterScaleform)` at load (`MCM.cpp:190`). On every load of `Interface/MainMenu.swf`, title and pause alike, `RegisterScaleform`:
  - sets `root.mcm`, a native object with MCM's Scaleform functions (`SetKeybind` among them, see [05](../issues/05-mcm-setkeybind-on-flat.md));
  - creates a `flash.display.Loader` at `root.mcm_loader`, which loads `MCM.swf`;
  - adds the loader as a child of `root.Menu_mc`.

  The VR build does the same with `Interface/world_MainMenu.swf` and `MCM_VR.swf`.
- **AS3 side** (`mcm.MCM_Main`). On its first `ENTER_FRAME`, and only if `Menu_mc["PauseMode"]`, `makeChanges()` does this once:
  ```as3
  this.MainMenu["MainPanel_mc"].List_mc.entryList.splice(pos - 1, 0, { "text":"$MOD_CONFIG", "index":100 });
  this.MainMenu["MainPanel_mc"].List_mc.InvalidateData();
  this.MainMenu.addEventListener("BSScrollingList::itemPress", this.itemPressedHandler);
  ```
  - `pos` is `iPosition:Main` from `Data\MCM\Config\MCM\settings.ini`, default 1, the top.
  - `$MOD_CONFIG` reads "MOD CONFIG" (`MCM_en.txt`).
  - `itemPressedHandler` opens MCM's panel inside the pause menu for `index` 100. Its `default` branch only restores a few saved panel positions, so a press on our row is harmless.
- **Living together.** Likely.
  - The two rows use different `index` values, and each handler ignores the other's.
  - MCM inserts once, after us, at its slot. A row above Settings stays above Settings, since MCM's insert shifts both or neither.
  - When MCM's panel is open it hides `MainPanel_mc`, our row with it.
  - Our insert-only-when-missing rule never moves MCM's row, and neither does an anchor-free MCM.
  - DearModdingUI places at an absolute slot and re-places only its own row. Insert-only-when-missing settles with it. A "keep ours directly above Settings" rule would ping-pong with it when DearModdingUI's slot falls between our row and Settings: each moves its row every frame. Worked through by hand from `SyncEntry`'s logic.
- **Order with defaults and MCM installed:** MOD CONFIG, SAVE, LOAD, INSTALLED, HELP, MODS CONFIG, SETTINGS, QUIT (plus QUICKSAVE or BETHESDA.NET when the game shows them).
- **The list scrolls.** Ten rows exceed the list's 9 clips, so `BSScrollingList` scrolls. MCM plus DearModdingUI already do this; only cosmetic.

### 4. What CommonLibF4VR and the framework give on flat

CommonLibF4VR is at `external/F4VR-CommonFramework/external/CommonLibF4VR`, branch `flat-fo4`, `ad7805e5`. A two-ID `REL::RelocationID(og, ng)` resolves per runtime. A one-ID `REL::ID(n)` is looked up as is. On flat, `IDDB::id2offset` doesn't check for an exact match (`src/REL/IDDB.cpp:142-148`), so a missing ID gives the next ID's address without an error. Sure.

| Need | CommonLibF4VR | Flat OG / NG |
|---|---|---|
| `UI::GetSingleton`, `GetMenu`, `GetMenuOpen`, `RegisterSink<MenuOpenCloseEvent>` | `RE/Bethesda/UI.h` | yes: `RelocationID(548587, 2689028)`, menu-map lock `(578487, 2707105)`, sink inline |
| `MenuOpenCloseEvent { menuName, opening }` | `RE/Bethesda/Events.h:361` | layout only |
| `IMenu::uiMovie` at 0x40 | `RE/Bethesda/IMenu.h:394` | same layout as DearModdingUI's fork |
| `PauseMenu` / `MainMenu` classes | none; use the names `"PauseMenu"`, `"MainMenu"` | n/a |
| `Movie::GetVariable`, `CreateObject`, `CreateFunction`, `Invoke` | `GFx_Player.cpp`, through `ASMovieRootBase` virtuals | yes, no ID |
| `FunctionHandler` with `Params { retVal, movie, self, argsWithThisRef, args, argCount, userData }` | `GFx_Player.h:1015` | yes |
| `Value::GetMember`, `SetMember`, `Invoke`, `GetArraySize`, `PushBack`, `RemoveElements`, AddRef/Release | `GFx_Player.h:544-605` | yes, both IDs |
| `Value::GetElement` / `SetElement` | **missing** | DearModdingUI's fork: `GetElement{ 827659, 2285881 }`, `SetElement{ 433707, 2286575 }` (`Depends/commonlibf4/include/RE/IDs.h:3947-3948`) |
| `Value::HasMember` | `REL::ID(788691)` | **OG only**; NG `2286078` per DearModdingUI's fork; upstream after C1 still OG only |
| `Value::VisitMembers` | `RelocationID(0, 2286786)` | **NG only**; upstream after C1 adds OG `1276961` |
| `UIMessageQueue::AddMessage` | `RE/Bethesda/UIMessageQueue.h` | yes: `RelocationID(1182019, 2284929)` |
| F4SE Scaleform interface | `F4SE::GetScaleformInterface()->Register(name, bool(Movie*, Value*))`, queried by `F4SE::Init` | yes; not needed |

The GFx rows were checked against `git show upstream/master` of alandtse/CommonLibF4 (13 commits ahead). After C1, `VisitMembers` gets its OG ID, `HasMember` stays OG-only, and `GetElement` is still missing. Sure.

**Framework.** `src/f4vr/ScaleformUtils.*` has `getScaleformBool/Int`, `isElementVisible`, list move and press (`doOperationOnScaleformList`), `invokeScaleformProcessUserEvent/DispatchEvent` and `findAndWorkOnScaleformElement`. They use only `GetVariable`, `GetMember`, `Invoke` and `CreateObject`, so they are flat-safe. None registers a native listener, reads an array element or splices a row. `f4vr/GameMenusHandler.h` is a `MenuOpenCloseEvent` sink with an `(menu, opened)` callback, which the mod already sets in `ModsConfigFO4::onGameLoaded`. The plan doesn't list it as VR-only. Sure.

**Workarounds for `GetElement`.** Unverified.
- `entries.GetMember("3", &v)`. Scaleform's AS3 `Array` property lookup probably treats a numeric name as an index, but nothing here proves it.
- Keep our spliced entry `Value` and call `entries.Invoke("indexOf", ...)` to find it. This holds an AS3 reference across frames, so it must be released before the movie dies. It also still can't find Settings by `index`.
- Insert at `entryList.length - 1`, before Quit. This assumes Quit is shown and is last.

### 5. Closing the pause menu, and the game's state

- **Esc closes the pause menu through `onContinuePress`.** Sure. Esc or Start in the pause menu's main state runs `onCancelPress`. In `MAIN_STATE` with `PauseMode` that does `BGSCodeObj.PlayCancelSound(); BGSCodeObj.onContinuePress();` (`MainMenu.as:1059-1071`), and the pause menu has no other cancel action.

  M303 can call the same function from its press handler, through `Menu_mc.BGSCodeObj` and `Invoke("onContinuePress")`, inside the same AS3 dispatch that vanilla's own presses call it from. That it closes the menu when called this way is likely; try it in the game.
- **Alternative close.** `UIMessageQueue::AddMessage("PauseMenu", kHide)`. IMenu's default `ProcessMessage` answers `kHide` with `kHandled` (`IMenu.h:268-280`). Whether `PauseMenu`'s override honours it is unverified. It is queued and runs on the next UI message pass.
- **The open waits for the close.** Likely. The `MenuOpenCloseEvent` for `PauseMenu` closing comes on the game thread between frames; the mod's comment at `ModsConfigFO4.cpp:89-91` and the VR flow already rely on that. Opening there keeps the pause menu's own close (cursor refresh, input context, its `kPausesGame` count) from running after our menu has taken the cursor.
- **Game state after the close.** Likely, from Fallout 4's behaviour and `UI`'s counters. The game unpauses: `UI::menuMode`, `freezeFramePause` and the frozen, blurred background are released, and `PauseMenu` restores the VATS and rumble state it holds (`PauseMenu.h` fields `vatsWasEnabled`, `rumbleLock`). The world runs while Mods Config is open, as the plan wants ("The game keeps running, as in VR"), and a live change is seen. Enemies keep acting too.
- **Keeping the pause menu open, as DearModdingUI does.** The game would stay paused behind a frozen frame, so a live preview isn't seen. The canvas, two thirds of the screen's height, would cover the pause list. The pause menu would also get keyboard and mouse input unless the framework's menu stops it, as DearModdingUI does with `kStop`.

### 6. Smaller facts

- **Label.** Vanilla tokens resolve to short upper-case words. The game's `Interface\Translate_en.txt` (1.10.984) has `$SAVE` SAVE, `$LOAD` LOAD, `$INSTALLED CONTENT` INSTALLED, `$HELP` HELP, `$SETTINGS` SETTINGS and `$QUIT` QUIT. MCM's row reads "MOD CONFIG". Sure.

  F4SE loads `Interface\Translations\<plugin>_<lang>.txt` only for enabled `.esp`/`.esm`/`.esl` files in `plugins.txt` (ianpatt/f4se `f4se/Translation.cpp:82-112`). Mods Config ships no game plugin, so a `$` token in our row would show as written unless the mod adds it to the translator. Plain text such as `"MODS CONFIG"` shows as is. Likely.
- **The fake event.** `McmEvents.cpp:68-78` sends `PauseMenu` opening and closing without the menu. Sure. Our sink must check that `GetMenu("PauseMenu")` returns a menu with a movie before attaching. Whether the menu map's pointer is null after a close is likely but unverified; `GetMenuOpen` (`OnStack`) is the stricter check.
- **The `enterFrame` handler stays cheap.** Once the row is in, each frame costs one `GetVariable`, one array length and a few element reads, and only while the pause menu is open.
- **An index for our row.** It must avoid 0–14, 100 (MCM) and `0x444D` (DearModdingUI, 17485).

## Open points that need a decision

1. **Close the pause menu, or open over it?** Closing it matches the plan (the game runs, live preview is seen) but unpauses the game. Opening over it is DearModdingUI's way: the game stays paused, the canvas covers the pause list, and the framework's menu must block the pause menu's input. Recommended: close.
2. **What does closing Mods Config return to?** Gameplay, or the pause menu again with `UIMessageQueue::AddMessage("PauseMenu", kShow)`, since the row was a step inside it. F10 opens it from gameplay either way.
3. **The label.** "MODS CONFIG" is upper case like its neighbours, but it is one letter from MCM's "MOD CONFIG", which sits in the same list. Something else, such as the player-facing title "MODS CONFIG FO4"? It is a literal, unless the mod adds translations.
4. **Position.** Above Settings, which needs no setting and shifts none of the rows vanilla greys? Or a configurable slot like MCM's `iPosition` and DearModdingUI's `iPauseMenuEntryPosition`? A configurable slot needs DearModdingUI's re-sending of `UpdateModsIndex`/`UpdateInstalledContentIndex` when it lands above them. Or the Settings submenu (DearModdingUI's default), which needs new `SettingsCategoryListEntry` clips and a taller border?
5. **A title-screen row.** `MainMenu` uses the same swf, but the plan names only the pause menu. The VR mod also closes Mods Config when `MainMenu` opens (`ModsConfigFO4.cpp:93-97`). Recommended: leave it out.
6. **`GetElement`.** Add `GetElement` (and `SetElement`) to CommonLibF4VR's `GFx::Value` with DearModdingUI's IDs. That is a new `C` commit before M303, and it needs a VR value or a check that VR never calls it; with the two-ID form, VR takes the OG ID. Or have M303 use a workaround from section 4, all of them unverified or fragile. Recommended: the CommonLibF4VR commit. Fixing `HasMember`'s NG ID in the same commit would remove a trap.
7. **Our row's `index` value.** Any free number, for example `0x4D43` ("MC").

## Sources

- Plan and ticket: `docs/design/flat-fallout4.md`, `.scratch/flat-fo4/issues/04-pause-menu-row.md`.
- DearModdingUI: `C:\Stuff\GitHub\Mine\Modding-Reference\Fallout4\github-repos\gold\DearModdingUI` (`7e09986`). `src/Platform/menus/StartMenuEntry.cpp`, `src/Main.cpp`, `CHANGELOG.md`, `data/F4SE/Plugins/DearModdingUI.toml`, and its CommonLibF4 fork `Depends/commonlibf4` (`981fb964`) `include/RE/IDs.h`, `include/RE/P/PauseMenu.h`, `include/RE/M/MainMenu.h`. The analysis: `Fallout4\Analysis\gold\DearModdingUI_RE_REFERENCE.md` and `FloatingDamage_MOD_OVERVIEW.md` / `_RE_REFERENCE.md` (the `root.f4se.plugins.<name>` route).
- Flat MCM: GitHub `reg2k/f4mcm` (`e2b187a8`), `src/ScaleformMCM.cpp`, `src/MCM.cpp`, `dist/Data/Interface/MCM.swf` (decompiled: `mcm/MCM_Main.as`), `dist/Data/MCM/Config/MCM/settings.ini`, `dist/Data/Interface/Translations/MCM_en.txt`. VR MCM: `Modding-Reference\F4VR\github-repos\gold\Neanka-mods-repo\f4mcm\src\ScaleformMCM.cpp`.
- The game: `Interface\MainMenu.swf` from `Fallout4 - Interface.ba2` of the 1.10.984 copy (`C:\Games\steamapps\steamapps\common\Fallout 4 - NG`) and of Steam's 1.11.240, decompiled with JPEXS ffdec 26.5.0 (`MainMenu.as`, `Shared/AS3/BSScrollingList.as`, `MainMenu_fla/MainListPanel_49.as`); `Interface\Translate_en.txt` from the same 1.10.984 archive.
- F4SE: ianpatt/f4se clone at `Modding-Reference\F4VR\github-repos\gold\f4se` (`6f6a7ca`). `f4se/Hooks_Scaleform.cpp` (per-movie plugin callbacks, `root.f4se`), `f4se/Translation.cpp`.
- CommonLibF4VR (`flat-fo4`, `ad7805e5`) and upstream alandtse/CommonLibF4 `master`. `include/RE/Bethesda/UI.h`, `IMenu.h`, `Events.h`, `UIMessageQueue.h`, `include/RE/Scaleform/GFx/GFx_Player.h`, `src/RE/Scaleform/GFx/GFx_Player.cpp`, `include/RE/Scaleform/Kernel/SF_RefCount.h`, `include/REL/ID.h`, `src/REL/IDDB.cpp`, `include/F4SE/Interfaces.h`, `src/F4SE/API.cpp`.
- Framework (`flat-fo4`, `34a5300`): `src/f4vr/ScaleformUtils.h/.cpp`, `src/f4vr/GameMenusHandler.h`. Mod: `src/ModsConfigFO4.cpp`, `src/mcm/McmEvents.cpp`.
