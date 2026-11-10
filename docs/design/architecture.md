# Architecture and lifecycle

## Purpose

A shared, VR-first configuration menu for Fallout 4 VR mods, compatible with existing MCM menus. The host is an F4SE plugin on F4VR-CommonFramework; its UI is one Dear ImGui panel in a vrui layout, operated with a controller ray.

The host owns navigation, interaction, persistence, diagnostics and the visual language. Each mod owns the meaning and runtime application of its settings. A diagnostic is a line in the mod's log: players see only that a mod is not available and why ([Notices](ui-and-vr-interaction.md#notices)); a config spec's author sees every diagnostic with [Developer tools](ui-and-vr-interaction.md#developer-tools) on.

## Integration sources

```text
Declarative config spec ──┐
                          ├──> Unified settings model ──> VR UI
MCM config ───────────────┘
```

A mod's DLL can also register a [native client](#native-api). It is not a third source: it adds to its mod's config spec live preview, functions for buttons, options and states only its code knows, and quick launch entries.

Both sources have the Save bar; the difference is in the store ([model](model.md)). A config spec's change waits for Save (writes files and game) or Discard. An MCM change is made at once through MCM, so its buttons are Keep (accept) and Revert (set back). The tree marks an MCM mod with the `MCM` tag and shows no other difference.

## Ownership

- The host owns menu discovery, rendering, input, validation and safe persistence.
- A config spec describes the UI and binds controls to the mod's real settings: file keys, globals, script properties.
- A mod owns runtime interpretation and side effects.
- CommonFramework is linked statically into each mod. Its config keeps session overrides (in memory, never written), which the [adapter](#commonframework-adapter) drives for live preview.
- Client callbacks are scoped to the client's config spec, so they carry no `modName`.

## Config spec and runtime identity

A config spec's identity is its `modName`, used for discovery, diagnostics and matching a native client registered under that name. A client that matches no config spec gets no setting events, but its quick launch entries still show.

Without a client a config spec can read, validate and save, and previews only settings kept in globals or script properties (the host writes those into the game). Saved values reach the mod only if it reads them again: CommonFramework mods reload their watched config; script mods use the [Papyrus API](#papyrus-api) or keep values in globals/properties and apply a change in the setting's action.

The host cannot tell when a change takes effect, so the config spec says it with `applies`: `live`, `save`, `restart`, or `unknown` where nothing says ([When a change takes effect](schema.md#when-a-change-takes-effect)). MCM settings are `unknown`.

## Persistence model

The host writes each mod's declared files directly; it keeps no mirror of settings. Formats: INI and TOML.

- **TOML** is edited with the framework's `TomlEditor`: values replaced in place, a new key added after its table's last value; comments, blank lines, order and other values' formatting kept.
- **INI** is read as SimpleIni / CommonFramework read it and edited line by line: a value's text replaced in place, a new key added after its section's last value or in a new section at the end; every other line kept.
  - `;` or `#` starts a comment only at line start, so values may contain them. A value is the text after `=`, trimmed.
  - Section and key names are case-insensitive. A key before any section is named by the key alone.
  - A duplicate key reads as its last occurrence; a write sets every occurrence. Duplicate sections are one section.
  - Bools read `1/true/yes/on` and `0/false/no/off` in any case; written in the file's existing word and case, or `1`/`0` for a new key.
  - Integers read decimal or `0x` hex; written in hex with the same digit count where the file had hex (form IDs).
  - Floats are written as the shortest text that reads back the same, with a decimal point; an unchanged value keeps its text (`1.250`).

**Paths** are explicit, with no default. Roots: the game `Data` folder and `Documents/My Games/Fallout4VR`. Absolute paths and `..` are rejected, and the extension must match the format. Links are not resolved, so a mod-manager link is used as the game sees it. The game's own INIs (`Fallout4Custom.ini`) are valid targets with `applies` `restart`.

**Writes are in place:** the new content is serialized and verified in memory, then written over the file in one write. No temporary files, renames or backups: a replaced file looks like a rename to file watchers and can break mod-manager links.

**A missing file** is created, with its folders, on the first Save that changes one of its settings, holding only those settings (a mod reads its defaults for the rest). A config spec cannot forbid this.

Files are read at session start and written on Save, on the game thread.

A setting can also live in a global or script property (in the save game, [schema](schema.md#global-and-script-property)), or in a [client state](#a-clients-state) (in the DLL's memory).

## External changes

No watcher. At Save the host rereads every affected file and applies only the session's dirty settings:

- An external change to an untouched setting is kept.
- When both changed a setting, the player's pending value wins.
- A file that no longer parses fails the Save and is left as is.

Reading values again (`readAgain`, e.g. after a mod's [own UI](#the-mods-own-ui) saved) follows the same rule: clean settings take the new value, pending ones stay the player's.

## Editing session

One mod at a time has a session.

1. Selecting a mod reads its files, globals, properties and client states as the baseline.
2. Player edits, and edits a mod's own UI hands back, update the pending values.
3. A valid edit of a `live` setting is previewed: a file key is sent to the client; a global/property is written by the host and then sent to the client; a client state is sent to the client to apply. Previews to a client are throttled to one per 50 ms per setting (the last edit is sent once the time passes); Discards are sent at once.
4. Minimizing keeps the session.
5. Save sets a new baseline. Discard restores it and rolls back previews.

A setting is dirty while its pending value differs from its baseline; changing it back makes it clean. A pending value that returns to its baseline is sent to the client as `Discard` and a previewed global/property gets its baseline back, so a client holds previews only for dirty settings, and no preview outlives its session. Reset is an ordinary pending edit.

Leaving a dirty mod (another mod, the front page, closing the menu) asks **Save / Discard / Cancel**, or **Keep / Revert / Cancel** for MCM.

A loading screen or the main menu (save load, new game, door, death) closes the menu without asking: a config spec's pending changes are discarded with their previews; MCM changes stay (as Keep). A game saved during a `live` global/property preview keeps the preview in the save; Discard restores the running game, not the save.

### Values at session start

Globals and properties: the game's value, or not available (with the reason) if absent. Client states: the client's answer, or not available if unanswered; an answer of the wrong type, out of limits or not an option is invalid. File keys:

| File state | Result |
| --- | --- |
| Key present and valid | The baseline |
| Key missing | Declared default, clean; Save adds the key only if the player changes it |
| Value invalid (type, limits, not an option) | Stored text with the invalid marker, clean; editing or Reset replaces it |
| File missing | Defaults, clean; the first Save that changes one creates the file |
| File unreadable or unparsable | Its settings are not available until values are read again |

Only dirty settings can block Save; a pre-existing invalid value cannot.

## Save lifecycle

The operation is **Save**, not Apply: it accepts and persists the pending values, which live preview may already show. A config spec's Save (MCM's is in [MCM compatibility](#mcm-compatibility)):

1. Validate the dirty settings.
2. Read every affected file into memory (missing = empty).
3. Parse each; a failure fails the Save.
4. Apply the dirty settings.
5. Serialize each file to a buffer.
6. Parse each buffer back and check every dirty setting reads its intended value.
7. Check every dirty global/property still exists and every dirty state's client is present and not cut off.
8. Write each buffer over its file, creating missing files and folders.
9. On a write failure: write back the step-2 content of files already written, delete files this Save created (created folders stay), report, stop.
10. Write dirty globals and properties into the game.
11. Run each written setting's action, in config spec order, with its saved value.
12. Call the client's `onSaved` once with every written setting (with its prior value) and the written files. Client states are applied from this call. A Save that wrote nothing tells nothing.
13. Send the [final notifications](#final-notifications).
14. Tell the player to restart if a `restart` setting was written.

Nothing is written before step 8 and the game after the files, so a failed Save changes nothing and keeps every setting dirty. The footer shows why (SaveBar's Not saved) until the next change, Save or Discard. A crash in step 8 can truncate the file being written; each is a few KB in one call, so the risk is accepted (whether to keep a last-good copy is open).

## Native API

A mod's DLL talks to the host through [`ModsConfigFO4Api.h`](../../src/api/ModsConfigFO4Api.h), which the mod copies as is (as with FRIK's `FRIKApi.h`). The framework carries a copy, through which `ModBase` connects every CommonFramework mod ([adapter](#commonframework-adapter)). The header documents every type and method; this section has the design rules. The author guide is [native-api.md](../for-mod-authors/native-api.md).

### Binary interface

- Two interfaces of virtual functions: the host implements `ModsConfigFO4ApiV1`, a client implements `IClient` and registers it once. The host asks the client by name for what its config spec names (functions, states, choice options), so a client registers no names, and an unanswered name doesn't exist.
- The host DLL exports `ModsConfigFO4API_GetApiV1`, valid for the whole run. `ModsConfigFO4ApiV1::initialize` finds it with `GetModuleHandle` (as `FRIKApi::initialize` does), from `kPostLoad` on, and fails harmlessly without the mod. No F4SE message handshake: F4SEVR listeners don't reach a plugin in every load order.
- Method arguments: fixed-width integers, `float`, `bool`, `std::uint32_t` enums, `const char*`, plain structs of those, interface pointers. No `std::` types or exceptions cross (separate heaps and layouts). Interfaces have single inheritance and no data (COM vtable layout).
- Text is zero-terminated UTF-8. Player-visible client text (function messages, quick launch text) may be a `$` token, translated by the host.
- The caller's data is valid during the call; the callee copies what it keeps. Answers go through an interface the host passes (`IFunctionCall::setMessage`), so only during the call.
- Every call, either way, is on the game's main thread. The host calls clients from its frame update inside the [failure guard](#a-client-that-fails).
- The header's C++ layer, `ModsConfigFO4Client`, compiles into the client's DLL: an `IClient` answering by name from the mod's lambdas, added to at any time and connected once.

### Versions

- The API only grows: methods and struct fields are added only at the end, nothing changes or is removed, and `getVersion` counts additions. A client uses later additions after checking `apiVersion()`; one that needs a newer menu asks `connect` for that version.
- A client registers with its header's version, so the host calls only the `IClient` methods it has.
- Before mod 1.0, V1 changes as milestones need and stays version 1, so clients rebuild against the menu's header. From 1.0 it only grows.

### The client

- Registered once at load under its mod's name, for the whole run (no unregister). A second client of a name, or a second entry of a function, is refused and the first stays, so two mods can't share a config spec.
- It is asked only about its own config spec, and a failing client is cut off as a whole.
- `onChange` tells each change of a `live` setting, including game-kept ones the host wrote first ([Live preview API](#live-preview-api)); the C++ client forwards to `subscribeToChanges`. `onSaved` tells once what each Save wrote, of any `applies`, which ends those settings' previews (`subscribeToSaves`).

### The client's events

`MenuEvent` types: `MenuOpened`, `MenuClosed`, `ModShown`, `ModLeft`, `PageShown`, `PageLeft`, `PointedAt`.

- Every client gets menu open/close (minimized is still open), so a mod can put its own UI away.
- The rest concern the client's own config spec. Its mod is shown on selection and left on another mod, the front page, or close.
- A page with an `id` is shown and left as its mod is and as other pages are shown; a page without one sends nothing. A client shows in the world what the page sets (Immersive Flashlight draws its spheres while their page is shown).
- `PointedAt` names the pointed row's id (a setting's, or a section's or button's `id`), then each enclosing group with an id, innermost first, regardless of the page it's drawn on; a client checks whether a group's id is among them. Items without ids are left out; an activation sphere uses its section's id ([Pointed items](schema.md#pointed-items)).
- `PointedAt` is sent on change, not per frame. It persists across row gaps, an open list or editor, and Adjust in world; it becomes none off the rows, on the front page, Help or a report, and when the menu leaves the mod, closes or minimizes for another UI.
- It was added within version 1: older clients ignore an unknown event type, and newer clients never see `pointedIds` from an older menu. The C++ client keeps the ids (`pointedIds`, `isPointedAt`) for `subscribeToPointing` handlers.

### Functions

A `CallNativeFunction` button or a quick launch entry runs a client function by name, with named parameters (`IFunctionCall`).

- `runFunction` returns at once: true if it did its work. Its message shows in the help line after the info mark, or the invalid mark on false. Longer work continues after it returns, with a [state](#a-clients-state) telling rows it is running.
- A button whose function the client lacks (`hasFunction`, asked as it's drawn) is disabled with the reason.

### Quick launch

`addQuickLaunch` adds a front page entry: `text`, `help`, `buttonText` (instead of `Open`) and the function a press runs, one entry per function (FRIK has its main config and its Pip-Boy config). A client matching no config spec keeps its entries. The function is looked up on press.

### A client's state

A condition can ask a client state by name ([Conditions](schema.md#conditions)) through `answerState` (false: not available). The host asks each state once a frame, only while the mod is shown; the method answers at once and changes nothing. Unanswered states and all states of a cut-off client are not available.

A file can take its path from a state (`pathFromClientState`, [Files](schema.md#files)), asked on selection and on reading values again.

A setting can be kept in a state ([Client state](schema.md#client-state)): read on selection and reread like a file value; the host writes nothing, and changes reach the client through `onChange` (`live`) and `onSaved`, whose storage names the state. The C++ client's `addState(name, getter, setter)` calls the setter with each such change before the mod's handlers:

- A `live` state's setter gets every valid pending value, the baseline on Discard, and the value again at Save; applying a value twice must change nothing.
- A setter applies and does not persist; a mod persists in its Save handler, which previews don't reach.
- A setter cannot refuse: the host already validated, and has written its files by the time Save tells the client.
- A state without a setter is unchanged, so mark its setting `readOnly` (the host can't see setters).
- A mod tells the menu about a change it saved itself with `reloadValues`; its own UI hands edits back with `setPendingValue` instead.

### The mod's own UI

A mod's own UI opens from a client function (button or quick launch), which moves the menu aside itself with the menu calls any DLL can make:

- `minimizeMenu` as the UI opens; the session and its pending changes stay. A function whose UI doesn't open doesn't minimize, and says why in its message.
- `restoreMenu` as it closes: shows the menu where it was, only while minimized (a player who pressed Restore or closed the menu isn't shown it again). It rereads the shown mod's values (clean settings take the new value, pending stay). `reloadValues` does the same at other times, e.g. after a hotkey.
- `openMenu(mod, pageId)` opens at a mod's page, the mod's front page with no page, or the menu's front page with no mod (wrist buttons, holotapes). With unsaved changes on the shown mod, the unsaved-changes dialog asks first. A missing mod or page returns false.
- A UI that edits config spec settings hands each edit back with `setPendingValue` and doesn't save. The menu owns the whole edit (pending markers, Save, Discard, Reset, one Save batch); two savers would overwrite each other. FRIK's calibration, which saves itself, uses `restoreMenu`'s reread instead.

`setPendingValue(settingId, value)` sets one value, in its valueType (color, transform, binding: their TOML table text or INI line), as a player edit:

- Accepted only while the menu (shown or minimized) shows the client's mod and the unsaved-changes dialog isn't asking. The baseline value makes the setting clean.
- A `live` value is told back to the client via `onChange` (Preview, or Discard at the baseline), so the adapter keeps it as an override and Discard takes it back. The C++ client marks it `handedOver`: the client already applied it, so handlers only record it and the adapter doesn't reload.
- Refused (false, nothing changed, reason in the log): a call from inside `onChange`/`onSaved` (later handlers would get the older value; a hand-back loop), an unknown id, a read-only or unavailable setting, a wrong type, or a value outside the setting's values (not kept as pending-invalid, unlike player text).

## Live preview API

Without a client, live preview covers only globals and script properties. A client receives a typed `SettingView` (id, valueType, default, value options, resolved storage), so it never parses `spec.json`. Types are in the header ([binary rules](#binary-interface): texts are `const char*`, lists pointer + count).

- Storage in a file gives the file's name, format and absolute path, plus the INI section and key or the TOML path. A game-kept setting has no file but its form (and a property's script and name), written by the host before the client is told. A client-state setting has only the state's name; the host writes nothing.
- `onChange` (`SettingChange`): `Preview` when a valid pending value of a `live` setting changes, `Discard` back to the baseline, with the value the client last received. A slider sets its value on trigger release and per thumbstick step, so a drag is one preview; pointing sends nothing.
- `onSaved` (`SaveBatch`): once per Save, after the actions, every written setting of any `applies` in config spec order, with its prior value, the written files and a `revision` counting this config spec's writing Saves this run. A `live` setting in the batch gets no Discard. A Save that failed on a vanished global still reports what it wrote.
- The host owns the views; they are valid during the callback.
- A gameplay side-effect binding API may come later.

### Options from a client

A choice with `listFromClientCallback` ([schema](schema.md#options-from-the-mods-client)) gets options from `giveOptions(settingView, IOptions)` on selection, inside the failure guard; false means none. `IOptions::add` takes a value in the setting's valueType and an optional text. The menu itself is a client (connected by the framework), giving the options of its own spec's config spec setting.

### A client that fails

Every call to a client is guarded against C++ exceptions and SEH faults. A failing client is cut off until restart and treated as absent: no `live` previews, native buttons and quick launch entries disabled with the reason, states not available. The rest of its config spec and all other mods keep working. The log names the client and the call.

### CommonFramework adapter

A CommonFramework mod needs no client code. The framework registers, for every mod under its own name, an adapter that maps the API onto the config's session overrides of the mod's TOML, keyed by path, under owner `ModsConfigFO4VR`:

1. Preview sets the override; Discard clears it.
2. After Save it clears the overrides of the batch's settings; the file has the values, so the mod reloads them.

- It acts only on settings of the mod's own config file, matched by absolute path. Its client answers `configFile` (`ConfigBase::getConfigFilePath()`) and `modVersion`. A mod that loads another file at runtime (FRIK's `FRIK_FOLVR.toml` under Fallout London VR) sets its file's `pathFromClientState` to `configFile`, so the menu edits the loaded file and the paths match.
- A preview does exactly what a config reload does. A mod needing more subscribes to the config's change of that key (previews, discards and Saves tell it). A value read only at startup is `restart`.
- The client name is the framework's mod name (for log and config), which may differ from the F4SE plugin name: FRIK's `modName` is `frik`, while `F4VRBody` goes in `f4sePluginRequirements`.
- Other files, game values and client states need the mod's own handlers or state setters. A mod's own UI is added to the C++ client as a function and a quick launch entry ([own UI](#the-mods-own-ui)).
- A `handedOver` change from the mod's own UI is only recorded as an override (or cleared on Discard) with no reload, so unhanded edits in the UI survive and values can be handed over every frame.

## Final notifications

A Save that wrote something sends, after the client's batch:

- **F4SE message** from sender `ModsConfigFO4VR` (`MODS_CONFIG_FO4_F4SE_NAME`), type `MODS_CONFIG_FO4_SAVED_MESSAGE`, data the zero-terminated `modName`, to each plugin in the config spec's `f4sePluginRequirements`. It says that the mod saved, not what (the batch does). F4SE accepts a listener only once the sender is loaded: register at `kPostLoad` or later.
- **Papyrus external event** `OnModsConfigFO4Saved|<modName>` (F4SE `RegisterForExternalEvent`), passing `modName`. Scripts then read values with the [Papyrus API](#papyrus-api).

Two transports because F4SE messages don't reach Papyrus. A Save that failed on a vanished global/property after writing files still sends both.

## Papyrus API

Not built yet ([milestone 16](mod-plan.md)). A script-only mod (plugin and scripts, no DLL) ships a config spec and reads saved file values through global natives, needing neither MCM nor a DLL. Globals and script properties remain an alternative it reads itself.

```papyrus
Scriptname ModsConfigFO4 Native Hidden

bool Function GetModSettingBool(string asModName, string asSetting) native global
int Function GetModSettingInt(string asModName, string asSetting) native global
float Function GetModSettingFloat(string asModName, string asSetting) native global
string Function GetModSettingString(string asModName, string asSetting) native global
```

- `asModName` is the `modName`, `asSetting` the setting `id`; a setting without its own id is named as MCM names a ModSetting (`fPlayerHeight:MyMod`). Moving from MCM means changing `MCM.` to `ModsConfigFO4.` in getter calls. Both names match case-insensitively (Papyrus doesn't preserve string case).
- The mod ships `Data/Scripts/ModsConfigFO4.pex` and its source in `Data/Scripts/Source/User/`; the DLL registers the natives on load. Only config specs are covered; MCM mods keep MCM's getters. A mod using them requires Mods Config FO4 VR.

### What a getter returns

From the mod's file, never a pending value (so no live preview for scripts):

- Key present and valid: the value. Key or file missing: the default. Value invalid: the default, with a diagnostic.
- Unknown config spec or setting, or the wrong getter for the `valueType`: `false`/`0`/`0.0`/`""` with a diagnostic; no type conversion.

A choice uses its `valueType`'s getter. Open: a color's return type, and how soon an externally changed file is noticed (the host may cache parsed files). Config specs load when the game's data is ready, before a save loads, so getters work from the first script.

### When a script reads its settings

The files are written before the Save event, so a getter in its handler returns the saved value. Settings are the same for every save game while script variables live in the save, so a script also rereads after a save loads.

```papyrus
Event OnQuestInit()
    RegisterForExternalEvent("OnModsConfigFO4Saved|mymod", "OnConfigSaved")
    LoadConfig()
EndEvent

Function OnConfigSaved(string asModName)
    LoadConfig()
EndFunction

Function LoadConfig()
    fPlayerHeight = ModsConfigFO4.GetModSettingFloat("mymod", "playerHeight")
EndFunction
```

### The settings file of a script-only mod

The host creates it on the first Save that changes a setting, holding only changed settings; getters return the config spec's defaults for the rest.

## Buttons and actions

- A button is not a setting: no storage, default, dirty state or value.
- A config spec can use every MCM action the menu supports (script functions, so script-only mods have buttons) plus console commands.
- A setting can have an action that Save runs after writing it ([schema](schema.md#a-settings-action)).
- `CallNativeFunction` runs a client [function](#functions) by name with typed parameters; without a registered client the button is disabled with the reason. Opening a mod's [own UI](#the-mods-own-ui) (FRIK calibration, Immersive Flashlight's or Virtual Holsters' world editors) is such a function.
- A result is success or failure with an optional localized message. Confirmations are declarative so every button shares one dialog.
- No progress or cancel: Papyrus calls are queued with no result, and DLL functions return at once.

## MCM compatibility

The host bridges to MCM rather than replacing it: MCM stays installed and owns its settings, Papyrus functions and hotkey dispatch. Replacing it would mean reimplementing all of that and asking players to uninstall it, and could change existing mods' behavior.

- The host and config specs don't require MCM. MCM mods appear only with MCM installed; without it the log says why.
- A mod with both a config spec and an MCM menu is listed by its config spec alone ([Required metadata](schema.md#required-metadata)).
- The bridge reads each `config.json` and its default settings; writes ModSettings only through `MCM.SetModSetting*` (never MCM's files), so MCM's memory and file agree; reads and writes GlobalValue and PropertyValue sources directly as MCM does.
- It sends `OnMCMSettingChange|ModName` on each change and on Revert, plus MCM's menu open/close events when the menu opens/closes and when an MCM mod is shown/left.
- It runs the supported button actions. Changes are already made, so a script button doesn't ask to save first ([Buttons](schema.md#buttons)).
- Hotkeys are read-only; MCM dispatches them.
- `MCM.RefreshMenu()` can't be observed, so the bridge rereads MCM's settings file whenever another page of the mod is shown.

Per-feature handling is in [MCM support](mcm.md).

MCM changes keep MCM's timing, so the player sees them in the game and scripts get them in MCM's order, but they are still session changes with the Save bar and the dialog. The `MCM` tag tells the player changes are made at once; the status counts them as made, not saved.

- **Change:** writes the value (global/property into the game, ModSetting through `MCM.SetModSetting*`), runs the row's action with the value, sends `OnMCMSettingChange`. A vanished global/property isn't written and its row becomes not available.
- **Dirty:** the baseline is the value at session start and after each Save; only the player's changes are dirty. Rows a script sets (a preset's) are not, and keep the script's value. Clean values are reread every frame so script changes show. Storage-less settings are held by the store and never dirty.
- **Discard:** sets dirty settings back in reverse order of last change, each as a change (write, action, event), so a reverted preset reruns its script with the old preset. A row's Discard does one. Like doing it by hand, it can't undo what a script did beyond the value.
- **Save** writes nothing; kept values become the baseline. If the session wrote a ModSetting since the last Save, it then sends pause-menu open/close to the game's menu listeners, so DLLs that reread MCM's file on pause-menu close get the changes at Save. MCM writes its file when Papyrus runs the queued set, so these events wait frame by frame until the file has every written ModSetting, or a few seconds at most (an unchanged value isn't rewritten), also after the menu closes.
- A ModSetting write is queued with no result, so it can't fail once made.
- `applies` is `unknown` (MCM has no field for it); a mod that needs a restart says so in its own text.

## Raw custom UI

Not planned: sharing the host's ImGui context across plugins would couple clients to its ownership, ImGui version, input, styling and crashes. Hosted custom UI, if ever added, would be a host-owned drawing interface with an explicit ABI. A mod's own editor opens as [The mod's own UI](#the-mods-own-ui).
