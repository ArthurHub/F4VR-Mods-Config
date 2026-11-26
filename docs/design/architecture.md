# Architecture and lifecycle

A shared, VR-first configuration menu for Fallout 4 VR mods, compatible with existing MCM menus: an F4SE plugin on F4VR-CommonFramework, whose UI is one Dear ImGui panel in a vrui layout, operated with a controller ray.

The host owns discovery, navigation, input, validation, safe persistence, diagnostics and the look. Each mod owns the meaning and runtime application of its settings. A diagnostic is a line in the log: players see only that a mod is not available and why ([Notices](#notices)); a config spec's author sees every diagnostic with [Developer tools](#developer-tools) on.

## Integration sources

```text
Declarative config spec ──┐
                          ├──> Settings model ──> VR UI
MCM config ───────────────┘
```

A mod's DLL can also register a [native client](#native-api). It is not a third source: it adds to its config spec live preview, functions for buttons, options and states only its code knows, [page content](#page-content), and quick launch entries.

Both sources have the Save bar; they differ in the store ([model](model.md)). A config spec's change waits for Save or Discard. An MCM change is made at once through MCM, so its buttons are Keep and Revert, and the tree marks the mod with the `MCM` tag.

A config spec is identified by its `modName`: for discovery, diagnostics, and matching the native client registered under that name. Client callbacks are scoped to the client's config spec, so they carry no `modName`. Without a client a config spec still reads, validates and saves, and previews only settings kept in globals or script properties. Saved values reach the mod only if it reads them again: CommonFramework mods reload their watched config; script mods use the [Papyrus API](#papyrus-api) or keep values in globals and properties.

The host cannot tell when a change takes effect, so the config spec says it with `applies`: `live`, `save`, `restart`, or `unknown` where nothing says ([When a change takes effect](schema.md#when-a-change-takes-effect)). MCM settings are `unknown`.

## Persistence model

The host writes each mod's declared files directly and keeps no mirror of settings. Formats: INI and TOML. A setting can also live in a global or script property (in the save game, [schema](schema.md#global-and-script-property)) or in a [client state](#a-clients-state) (in the DLL's memory).

- **TOML** is edited with the framework's `TomlEditor`: values replaced in place, a new key added after its table's last value; comments, blank lines, order and other values' formatting kept. A table written as a `[header]` section or as dotted keys is set key by key in that form.
- **INI** is read as SimpleIni and CommonFramework read it and edited line by line; every line not written is kept.
  - `;` or `#` starts a comment at line start, or after a value with a space or tab before it (`8.5  # target`). A value is the text after `=` up to that comment, trimmed, so `#FF0000` and `a;b` stay whole. A write keeps the trailing comment.
  - Section and key names are case-insensitive; a key before any section is named by the key alone. A duplicate key reads as its last occurrence and a write sets every occurrence; duplicate sections are one.
  - Bools read `1/true/yes/on` and `0/false/no/off` in any case, and are written in the file's word and case, or `1`/`0` for a new key. Integers read decimal or `0x` hex, and keep hex with its digit count (form IDs). Floats are written as the shortest text that reads back the same; an unchanged value keeps its text.
- **Paths** are explicit, under the game `Data` folder or `Documents/My Games/Fallout4VR`. Absolute paths and `..` are rejected, and the extension must match the format. Links are not resolved, so a mod-manager link is used as the game sees it. The game's own INIs (`Fallout4Custom.ini`) are valid with `applies` `restart`.
- **Writes are in place:** the new content is serialized and verified in memory, then written over the file in one write. No temporary files, renames or backups: a replaced file looks like a rename to file watchers and can break mod-manager links.
- **A missing file** is created, with its folders, by the first Save that changes one of its settings, holding only those (the mod reads its defaults for the rest).
- **A removed key**, for a setting that [omits its default](schema.md#omitting-defaults), takes its lines out, and the `[header]` of a section it leaves empty. A Save that leaves a file with no values deletes it; a failed Save writes it back.

Files are read at session start and written on Save, on the game thread. There is no watcher: at Save the host rereads every affected file and applies only the session's dirty settings, so an external change to an untouched setting is kept, the player's pending value wins over an external change of the same setting, and a file that no longer parses fails the Save. Reading values again (`readAgain`, as after a mod's [own UI](#the-mods-own-ui) saved) follows the same rule.

## Editing session

One mod at a time has a session.

1. Selecting a mod reads its files, globals, properties and client states as the baseline.
2. Player edits, and edits a mod's own UI hands back, update the pending values.
3. A valid edit of a `live` setting is previewed: a file key is sent to the client; a global or property is written by the host, then sent; a client state is sent to the client to apply. Previews are throttled to one per 50 ms per setting (the last edit is sent once the time passes); Discards are sent at once.
4. Minimizing keeps the session. Save sets a new baseline; Discard restores it and rolls back previews.

A setting is dirty while its pending value differs from its baseline. A pending value back at its baseline is sent as a Discard, so a client holds previews only for dirty settings and no preview outlives its session.

Reset sets a setting's default as an ordinary pending edit, previewed like any edit and saved only by Save; a read-only setting has none. While a change is pending, the row's Discard takes back that change alone, in Reset's place.

Leaving a dirty mod (another mod, the front page, closing) asks **Save / Discard / Cancel**, or **Keep / Revert / Cancel** for MCM. A loading screen or the main menu closes the menu without asking: a config spec's pending changes are discarded with their previews, MCM changes stay. A game saved during a `live` global or property preview keeps the preview in the save.

### Values at session start

Globals and properties: the game's value, or not available with the reason. Client states: the client's answer, or not available; an answer of the wrong type, out of limits or not an option is invalid. File keys:

| File state | Result |
| --- | --- |
| Key present and valid | The baseline |
| Key missing | Declared default, clean; Save adds the key only if the player changes it |
| Value invalid (type, limits, not an option) | Stored text with the invalid marker, clean; editing or Reset replaces it |
| File missing | Defaults, clean; the first Save that changes one creates the file |
| File unreadable or unparsable | Its settings are not available until values are read again |

Only dirty settings can block Save; an invalid value already in the file cannot.

## Save lifecycle

**Save** accepts and persists the pending values, which live preview may already show. A config spec's Save (MCM's is in [MCM compatibility](#mcm-compatibility)):

1. Validate the dirty settings.
2. Read every affected file into memory (missing = empty) and parse it; a failure fails the Save.
3. Apply the dirty settings and serialize each file to a buffer.
4. Parse each buffer back and check every dirty setting reads its intended value.
5. Check every dirty global or property still exists and every dirty state's client is present and not cut off.
6. Write each buffer over its file, creating missing files and folders. On a write failure, write back the content read in step 2, delete files this Save created, report, and stop.
7. Write dirty globals and properties into the game.
8. Run each written setting's action, in config spec order, with its saved value.
9. Call the client's `onSaved` once with every written setting, its prior value, and the written files; client states are applied from it. A Save that wrote nothing tells nothing.
10. Send the [final notifications](#final-notifications), and tell the player to restart if a `restart` setting was written.

Nothing is written before step 6, and the game after the files, so a failed Save changes nothing and keeps every setting dirty; the footer says why until the next change, Save or Discard. A crash during step 6 can truncate the file being written; each is a few KB in one call, so the risk is accepted.

## VR interaction

The panel is a large projected Pip-Boy interface, not a desktop window in VR: translucent, strong framing, high contrast, large targets sized for controller jitter, stable placement. How each part looks and behaves is its [design system](design-system/README.md) component; this section has what spans them.

### Opening and the two states

- The gesture is both thumbsticks pressed and held, FRIK's own (`primary longpress thumbstick 1.5 +offhand:thumbstick`), a rebindable binding in `ModsConfigFO4VR.toml`. With FRIK installed, the menu takes it over: FRIK's `sOpenConfigButton` is turned off for the session through FRIK's config override API, and FRIK's configuration opens from the front page's quick launch.
- The game isn't paused: a `live` change is seen as made, a saved one as the mod acts on it.
- **Full** is the browsing and editing state: the tree of mods on the left, the selected page on the right, breadcrumbs with Help, Minimize and Close in the title bar, and the help line with Discard and Save in the footer ([FullPanel](design-system/project/components/FullPanel/README.md)).
- **Minimized** hides the panel while the player checks the effect of changes; the session and previews stay. The offhand wrist then has `Restore` and `Exit`, vrui buttons pressed by touch: with FRIK by the index fingertip, following the forearm; without, by the controller ([WristButtons](design-system/project/components/WristButtons/README.md)). Restore, or the gesture, shows the panel as it was and rereads the shown mod's values; Exit closes, asking first with unsaved changes.
- The menu opens on its [front page](#the-front-page): a closed menu doesn't keep its place, a minimized one keeps its mod and page.

### Placement

- The panel spawns in front of the player at a comfortable distance and height, level, and is world-locked: a panel moving with the head would move under the ray. It is curved so its sides face the player.
- **Move:** point and hold the grip longer than Back's tap; the panel follows that hand, level, until release. It is the framework's world move binding, on any hand, rebindable or off.
- Opening and Restore place it fresh. **Recenter** (long press of the offhand thumbstick, rebindable) respawns it in front, and so does walking away, without closing the menu.

### Pointing and input

- A controller ray operates the panel, ending in a mark. Either controller points; one owns interaction at a time, and the other's ray is fainter, without a mark. No desktop cursor.
- Trigger activates and drags. Thumbstick up/down scrolls the list under the ray; left/right steps the slider under it (after a short dwell) or the stepper. Popups, lists and dialogs open inside the panel's edges.
- While a ray points at the panel, that hand's trigger and thumbstick drive the UI and are hidden from the game. A press that starts on the panel stays with the UI until released; a trigger already held as the ray arrives is ignored until released. With no ray on the panel the controls are the game's, so the player points away to move, turn or fire, and points back.
- A text is typed on the framework's [VR keyboard](../../external/F4VR-CommonFramework/docs/tech/vr-keyboard.md), shown under the panel and typed with the rays.

### Back

Back is a tap of the grip of the hand pointing at the panel, either hand; a longer hold moves the panel. With no ray on the panel the press is the game's. While the keyboard is open, Back is its Done and does nothing else. Otherwise it resolves the most local interaction first:

1. Cancel an active drag.
2. Close a popup, dropdown or dialog.
3. From the Help page, return to the page under it.
4. While searching, show a filtered page whole, with the focus ring on the last row the ray was on.
5. From a mod's page, go to its parent page or the mod's front page.
6. From a mod's front page, go to the menu's front page, asking Save / Discard / Cancel first with unsaved changes.
7. From the menu's front page, close the menu.

### Haptics

A light pulse goes to the pointing hand in the frame its press acts: a page shown, the menu minimized or closed, a quick launch entry opened, a value changed, a dropdown opened, Back stepping back. A press that changes nothing gives none, and a drag none per frame (a thumbstick step gets one). The pattern is `sPressHaptic` in `[Pointer]`, `Tick` as shipped, or off.

### Help line

The left of the footer, with Discard and Save fixed to its right; only its text changes, which avoids layout movement and tooltips at uncomfortable depths ([DescriptionArea](design-system/project/components/DescriptionArea/README.md)). It shows, in priority:

1. An active validation error.
2. The help of the row, option, tree row or button the ray is on, as `Name: help`. A holding [warning](schema.md#warnings) comes first, and who [overrides](#overrides) the setting before that.
3. With the ray on no row, the session's status: the unsaved changes or the last Save.

- The help holds while the ray crosses gaps between rows, so a sweep doesn't flicker. Two lines of room; a third is drawn smaller, more ends in an ellipsis.
- While a change waits for Save, it adds the amber mark of its `applies` ([TakesEffect](design-system/project/components/TakesEffect/README.md)); `unknown` has none.
- An invalid row has the invalid mark after its name, its value in red, and the reason here; a warning has a hollow amber mark and doesn't block Save. Save is disabled while a dirty setting is invalid, the status counts them, and a folded section counts those it hides. Validation never relies on color alone.

### Navigation

- Mods are alphabetical by display name, in one list whatever their source; pages and settings follow their JSON order. Only the selected mod is expanded, its pages under it; a mod's front page is its root `content`, the mod's own row. A config spec page with subpages is expanded while it or a subpage is selected ([ModTree](design-system/project/components/ModTree/README.md)).
- Breadcrumbs name every level, `MODS CONFIG FO4 » <mod> » <page>`, and each but the last goes there when pressed ([Breadcrumbs](design-system/project/components/Breadcrumbs/README.md)).
- A list longer than its area scrolls with the thumbstick while the ray is on it. A row selected from outside the tree (crumb, Back, panel shown again) is scrolled into view.
- The Help page explains the menu's own ways with the buttons the player's config has; it shows in place of the page, which stays selected with its session ([FullPanelHelp](design-system/project/components/FullPanelHelp/README.md)).

### The front page

What the Full panel shows with no mod selected: no tree row, the `MODS CONFIG FO4` crumb is its entry. A text with the version, then quick launch, notices and Developer; nothing is edited there ([FullPanelHome](design-system/project/components/FullPanelHome/README.md)).

- **Quick launch:** an entry per UI a mod's client registered ([Functions and quick launch](#functions-and-quick-launch)), the whole row pressed. For a FRIK without the native API, the menu adds an entry while FRIK is installed. A mod's own UI is never drawn inside the panel ([The mod's own UI](#the-mods-own-ui)).
- MCM's `MCM Settings` and `Hotkey manager` aren't listed: hotkeys are on each mod's pages.

### Notices

A mod the menu found but can't show is **not available** when a required plugin isn't loaded, its MCM config or spec.json can't be read (then it is named by its folder), or its spec.json needs a newer Mods Config FO4 VR or repeats another's `modName`. Its tree row is in its place, disabled, the help line gives the reason, and the front page lists it under Notices. A mod with a saved change that takes effect after a restart has a notice, listed first, until the restart ([RestartNotice](design-system/project/components/RestartNotice/README.md)).

### Developer tools

For config spec authors: a switch in the menu's own settings, off as shipped. On, the front page has a Developer section with a row per config spec (MCM mods are never listed), filtered to those needing attention, all, or one. A row opens its report: its errors and warnings, each press going to its row, and `Read the live copy`, which copies `spec.json` to `My Games\Fallout4VR\Mods_Config\ModsConfigFO4VR\Dev\<folder>\spec.json` and reads that instead, again on each save of it. A read drops the mod's unsaved changes and keeps the open page if it still exists ([Developer](design-system/project/components/Developer/README.md), [FullPanelConfigSpecReport](design-system/project/components/FullPanelConfigSpecReport/README.md)).

### Search

The tree's header is a search field that finds a setting by the names the player sees, across all mods: mod, page, section and row text, not help or text rows. Each query word must start a word of the name, in any order, ignoring case and accents. With a query the tree is flat, with only the mods and pages that match and their counts; the shown page has only its matching rows, each the real row, edited and saved as on the whole page. A setting with [`"searchable": false`](schema.md#settings) is found only with its section, page or mod. Search never asks a client, so a [dynamic page](schema.md#dynamic-pages)'s rows are found once it has been shown. The rules in full: [Search](design-system/project/components/Search/README.md).

### Mod categories

- At `iCategoriesMinMods` mods or more (20 as shipped, 0 always) the tree's top level is categories; below it the list is flat. A mod's category is the first it has of: the player's choice, its own [`category`](schema.md#category), the shipped table of known mods; otherwise `No category`, the last row. Categories don't nest, aren't in the crumbs, and which are open is kept until the game closes ([ModTree](design-system/project/components/ModTree/README.md)).
- The player's choices are made on the `Categories` page of the menu's own entry, a [dynamic page](schema.md#dynamic-pages) of a dropdown per mod, ordinary settings that Save keeps ([FullPanelCategories](design-system/project/components/FullPanelCategories/README.md)). A typed name is trimmed and at most 24 characters; one already in the list, in any case, is that one.
- They are kept in `Categories.toml` beside `ModsConfigFO4VR.toml`, one key per `modName`, empty text for `No category`. Only a choice that differs from the mod's own category is kept, so an unmoved mod follows its own category and the table as they change.

## Native API

A mod's DLL talks to the host through [`ModsConfigFO4Api.h`](../../src/api/ModsConfigFO4Api.h), which it copies as is; the framework carries a copy, through which `ModBase` connects every CommonFramework mod ([adapter](#commonframework-adapter)). The header documents every type and method, and [native-api.md](../for-mod-authors/native-api.md) is the author guide; this section has the design rules.

### Binary interface

- Two interfaces of virtual functions: the host implements `ModsConfigFO4ApiV1`; a client implements `IClient` and registers it once. The host asks the client by name for what its config spec names (functions, states, options, page content), so a client registers no names, and an unanswered name doesn't exist.
- The host exports `ModsConfigFO4API_GetApiV1`, valid for the whole run. `initialize` finds it with `GetModuleHandle` from `kPostLoad` on, as `FRIKApi::initialize` does, and fails harmlessly without the mod. No F4SE message handshake: F4SEVR listeners don't reach a plugin in every load order.
- Only fixed-width integers, `float`, `bool`, `std::uint32_t` enums, zero-terminated UTF-8 `const char*`, plain structs of those, and interface pointers cross; no `std::` types or exceptions. Interfaces have single inheritance and no data (COM vtable layout).
- The caller's data is valid during the call; the callee copies what it keeps. Answers go through an interface the host passes, so only during the call.
- Every call, either way, is on the game's main thread; the host calls clients from its frame update inside the [failure guard](#a-client-that-fails).
- Player-visible client text may be a `$` token, translated by the host.
- The header's C++ layer, `ModsConfigFO4Client`, compiles into the client's DLL: an `IClient` answering by name from the mod's lambdas.

### Versions

The API only grows: methods and fields are added at the end, nothing changes or is removed, and `getVersion` counts additions. A client registers with its header's version, so the host calls only the `IClient` methods it has; one that needs a newer menu asks `connect` for that version. Version 2 adds `closeMenu`, `giveOverrides` and `giveContent`.

### The client

- Registered once at load under its mod's name, for the whole run. A second client of a name, or a second entry of a function, is refused and the first stays.
- It is asked only about its own config spec. A client that matches no config spec gets no setting events, but its quick launch entries still show.
- `onChange` tells each change of a `live` setting ([Live preview API](#live-preview-api)); `onSaved` tells once what each Save wrote, of any `applies`, which ends those settings' previews. The C++ client forwards them to `subscribeToChanges` and `subscribeToSaves`.

### The client's events

`MenuEvent` types: `MenuOpened`, `MenuClosed`, `ModShown`, `ModLeft`, `PageShown`, `PageLeft`, `PointedAt`. A client ignores a type it doesn't know.

- Every client gets menu open and close (minimized is still open), so a mod can put its own UI away. The rest concern its own config spec: its mod is shown on selection and left on another mod, the front page, or close.
- A page with an `id` is shown and left as its mod is and as other pages are shown; a page without one sends nothing. A client shows in the world what the page sets (Immersive Flashlight draws its spheres while their page is shown).
- `PointedAt` names the pointed row's id (a setting's, or a section's or button's `id`), then each enclosing group with an id, innermost first; items without ids are left out, and an activation sphere uses its section's id ([Pointed items](schema.md#pointed-items)). It is sent on change, holds across row gaps, an open list or editor and Adjust in world, and becomes none off the rows, on the front page, Help or a report, and when the menu leaves the mod. The C++ client keeps the ids (`pointedIds`, `isPointedAt`) for `subscribeToPointing` handlers.

### Page content

A page with `contentFromClientCallback` ([Dynamic pages](schema.md#dynamic-pages)) gets its rows from `giveContent(pageId, IContent)`, a version 2 call: the client gives the page's `content` array as JSON text with `IContent::set`, and the host reads it after the call, against the loaded config spec. The C++ client's `addPageContent(pageId, handler)` answers by page id. The menu's own Categories page is one.

- The menu asks as the page is shown (selected, pressed again, Back from a sub-page, a search result), and while it is shown after a Save or Discard that succeeds, on `reloadValues`, and on Restore. A client whose rows changed while the page is shown calls `reloadValues`.
- It never asks while a row of the page has a pending change or is in use (an editor, list or keyboard open over it, a drag, a confirmation, Adjust in world): new rows would drop it. An ask held back is dropped; the next Save, Discard or show asks.
- Each ask reads the mod's values again. The page keeps its scroll; its folded sections and buttons' results start again. Once given, the rows are in the definition like any others.
- **Not available:** a client that can't be asked (DLL not loaded, cut off, or version 1) disables the page and its sub-pages in the tree with the reason, known without a call. A client that gives nothing (false, no `set`, or not a JSON array) leaves the page open, saying why in place of its rows.

### Functions and quick launch

- A `CallNativeFunction` button or a quick launch entry runs a client function by name, with named parameters (`IFunctionCall`). `runFunction` returns at once, true if it did its work; its message shows in the help line after the info mark, or the invalid mark on false. Longer work continues after it returns, with a [state](#a-clients-state) telling rows it is running.
- A button whose function the client lacks (`hasFunction`, asked as it's drawn) is disabled with the reason.
- `addQuickLaunch` adds a front page entry: `text`, `help`, `buttonText` and the function a press runs, one entry per function (FRIK has its main config and its Pip-Boy config). The function is looked up on press.

### A client's state

- A condition can ask a client state by name ([Conditions](schema.md#conditions)) through `answerState`. The host asks each state once a frame while the mod is shown; the method answers at once and changes nothing. Unanswered states and all states of a cut-off client are not available.
- A file can take its path from a state (`pathFromClientState`, [Files](schema.md#files)), asked on selection and on reading values again.
- A setting can be kept in a state ([Client state](schema.md#client-state)): read on selection and reread like a file value. The host writes nothing; changes reach the client through `onChange` (`live`) and `onSaved`. The C++ client's `addState(name, getter, setter)` calls the setter with each change before the mod's handlers:
  - A `live` state's setter gets every valid pending value, the baseline on Discard, and the value again at Save; applying a value twice must change nothing.
  - A setter applies and does not persist (the mod persists in its Save handler) and cannot refuse (the host validated, and has written its files by then).
  - A state without a setter is unchanged, so its setting is marked `readOnly`.

### The mod's own UI

A mod's own UI opens from a client function, which moves the menu aside with the menu calls any DLL can make:

- `minimizeMenu` as the UI opens; the session and its pending changes stay. A function whose UI doesn't open doesn't minimize, and says why in its message.
- `restoreMenu` as it closes: shows the menu where it was, only while minimized, and rereads the shown mod's values (clean settings take the new value, pending stay). `reloadValues` rereads at other times, as after a hotkey or a change the mod saved itself.
- `openMenu(mod, pageId)` opens at a mod's page, the mod's front page with no page, or the menu's front page with no mod (wrist buttons, holotapes). With unsaved changes on the shown mod, the unsaved-changes dialog asks first. A missing mod or page returns false.
- `closeMenu(discardChanges)` closes the menu as the player's Exit does: with unsaved changes the dialog asks, unless `discardChanges` drops them.
- A UI that edits config spec settings hands each edit back with `setPendingValue(settingId, value)` and doesn't save: the menu owns the edit (pending marks, Save, Discard, Reset, one Save batch), as two savers would overwrite each other. FRIK's calibration, which saves itself, uses `restoreMenu`'s reread instead.

`setPendingValue` sets one value in its valueType (color, transform, binding: their TOML table text or INI line) as a player edit. It is accepted only while the menu, shown or minimized, shows the client's mod and the unsaved-changes dialog isn't asking. A `live` value is told back through `onChange`, marked `handedOver`, so handlers only record it and the adapter doesn't reload. It is refused (false, nothing changed, reason in the log) from inside `onChange` or `onSaved`, for an unknown id, a read-only or unavailable setting, a wrong type, or a value outside the setting's values.

## Live preview API

Without a client, live preview covers only globals and script properties. A client receives a typed `SettingView` (id, valueType, default, value options, resolved storage), so it never parses `spec.json`. The host owns the views; they are valid during the callback.

- File storage gives the file's name, format and absolute path, and the INI section and key or the TOML path. A game-kept setting gives its form (and a property's script and name), written by the host before the client is told. A client-state setting gives the state's name.
- `onChange` (`SettingChange`): `Preview` when a valid pending value of a `live` setting changes, `Discard` back to the baseline. A slider sets its value on trigger release and per thumbstick step, so a drag is one preview.
- `onSaved` (`SaveBatch`): once per Save, after the actions, every written setting in config spec order with its prior value, the written files, and a `revision` counting this config spec's writing Saves this run. A `live` setting in the batch gets no Discard.

### Options from a client

A choice with `listFromClientCallback` ([schema](schema.md#options-from-the-mods-client)) gets its options from `giveOptions(settingView, IOptions)` on selection; false means none. `IOptions::add` takes a value in the setting's valueType and an optional text. The menu is a client of itself, giving the options of its own config spec's settings.

### Overrides

Another mod can set a value in place of a setting's own for the game session, as ROCK sets FRIK's Pip-Boy binding through FRIK's API. A change of the setting is then saved but does nothing until the override ends, so the menu shows it.

- As the store reads values, the host asks a version 2 client `giveOverrides(settingView, IOverrides)` for each setting. `IOverrides::add(owner, value)` gives each override, the one in effect first: its owner's name and its value in the setting's valueType (a String of its text when it is not one).
- A client gives every override it knows; the host drops those whose owner is its own name, `MODS_CONFIG_FO4_F4SE_NAME`, which are its previews. The menu's own override of FRIK's open gesture is under the owner `Mods Config FO4 VR`, so FRIK's row shows it.
- An overridden setting is never previewed, as its Save would take the preview back; `notPreviewed` gives the reason, and Adjust in world is off. Save, Reset and Discard work as for any setting. A choice that sets other settings is overridden while one of them is.
- The C++ client answers from each source added with `addOverrides`; the [adapter](#commonframework-adapter) adds one for the mod's config file. MCM mods and mods with their own config code have none: an author who knows of a conflict states it with a [warning](schema.md#warnings) on a plugin condition.

### A client that fails

Every call to a client is guarded against C++ exceptions and SEH faults. A failing client is cut off until restart and treated as absent: no previews, native buttons and quick launch entries disabled with the reason, states not available. The rest of its config spec and all other mods keep working. The log names the client and the call.

### CommonFramework adapter

A CommonFramework mod needs no client code. The framework registers for every mod, under the framework's mod name, a client that maps the API onto the config's session overrides of the mod's TOML, under owner `ModsConfigFO4VR`:

1. Preview sets the override; Discard clears it.
2. After Save it clears the overrides of the batch's settings; the file has the values, so the mod reloads them.
3. It answers [overrides](#overrides) with every owner's session override of a setting of the file (`ConfigBase::getConfigOverrides`).

- It acts only on settings of the mod's own config file, matched by absolute path; its client answers `configFile` and `modVersion`. A mod that loads another file at runtime (FRIK's `FRIK_FOLVR.toml` under Fallout London VR) sets its file's `pathFromClientState` to `configFile`.
- A preview does exactly what a config reload does; a mod needing more subscribes to the config's change of that key. A value read only at startup is `restart`.
- The client name is the framework's mod name, which may differ from the F4SE plugin name: FRIK's `modName` is `frik`, while `F4VRBody` goes in `f4sePluginRequirements`.
- Other files, game values and client states need the mod's own handlers or state setters; its own UI is a function and a quick launch entry.
- A `handedOver` change is only recorded as an override, with no reload, so values can be handed over every frame.

## Final notifications

A Save that wrote something sends, after the client's batch, also when it failed on a vanished global after writing files:

- **F4SE message** from `ModsConfigFO4VR` (`MODS_CONFIG_FO4_F4SE_NAME`), type `MODS_CONFIG_FO4_SAVED_MESSAGE`, data the zero-terminated `modName`, to each plugin in the config spec's `f4sePluginRequirements`. It says that the mod saved, not what. F4SE accepts a listener only once the sender is loaded: register at `kPostLoad` or later.
- **Papyrus external event** `OnModsConfigFO4Saved|<modName>`, passing `modName` (planned with the [Papyrus API](#papyrus-api)). F4SE messages don't reach Papyrus.

## Papyrus API

Planned ([milestone 16](mod-plan.md#16-papyrus-api)). A script-only mod (plugin and scripts, no DLL) ships a config spec and reads saved file values through global natives, needing neither MCM nor a DLL.

```papyrus
Scriptname ModsConfigFO4 Native Hidden

bool Function GetModSettingBool(string asModName, string asSetting) native global
int Function GetModSettingInt(string asModName, string asSetting) native global
float Function GetModSettingFloat(string asModName, string asSetting) native global
string Function GetModSettingString(string asModName, string asSetting) native global
```

- `asModName` is the `modName` and `asSetting` the setting `id`, both case-insensitive; a setting without its own id is named as MCM names a ModSetting (`fPlayerHeight:MyMod`), so moving from MCM changes `MCM.` to `ModsConfigFO4.`.
- The mod ships `Data/Scripts/ModsConfigFO4.pex` and its source; the DLL registers the natives on load. Only config specs are covered; MCM mods keep MCM's getters.
- A getter returns the file's value, never a pending one: the default for a missing key or file, the default with a diagnostic for an invalid value, and `false`/`0`/`0.0`/`""` with a diagnostic for an unknown config spec or setting or the wrong getter. A choice uses its `valueType`'s getter.
- Config specs load when the game's data is ready, so getters work from the first script. The files are written before the Save event, so a getter in its handler returns the saved value. Settings are the same for every save game while script variables live in the save, so a script reads them again after a save loads:

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

## Buttons and actions

- A button is not a setting: no storage, default, dirty state or value. A setting can have an action that Save runs after writing it ([schema](schema.md#a-settings-action)).
- A config spec can use every MCM action the menu supports (script functions, so script-only mods have buttons) plus console commands, and `CallNativeFunction` for a client [function](#functions-and-quick-launch); without a client that button is disabled with the reason.
- A result is success or failure with an optional message. Confirmations are declarative, so every button shares one dialog. No progress or cancel: Papyrus calls are queued with no result, and DLL functions return at once.

## MCM compatibility

The host bridges to MCM rather than replacing it: MCM stays installed and owns its settings, Papyrus functions and hotkey dispatch. Replacing it would mean reimplementing all of that, asking players to uninstall it, and could change existing mods' behavior. Per-feature handling is in [MCM support](mcm.md).

- The host and config specs don't require MCM; MCM mods appear only with MCM installed. A mod with both a config spec and an MCM menu is listed by its config spec ([Required metadata](schema.md#required-metadata)).
- The bridge reads each `config.json` and its default settings, writes ModSettings only through `MCM.SetModSetting*` (never MCM's files) so MCM's memory and file agree, and reads and writes globals and script properties directly, as MCM does.
- It sends `OnMCMSettingChange|ModName` on each change and Revert, and MCM's menu open and close events as the menu opens and closes and as an MCM mod is shown and left. Keep sends `OnMCMClose` then `OnMCMOpen`, for a script that applies its values as MCM's menu closes.
- `MCM.RefreshMenu()` can't be observed, so the bridge rereads MCM's settings file whenever another page of the mod is shown. Hotkeys are read-only; MCM dispatches them.

MCM changes keep MCM's timing, so scripts get them in MCM's order, but they are session changes with the Save bar and the dialog; the status counts them as made, not saved.

- **Change:** writes the value (a ModSetting through `MCM.SetModSetting*`, queued with no result, so it can't fail once made), runs the row's action with the value, sends `OnMCMSettingChange`. A vanished global or property isn't written and its row becomes not available.
- **Dirty:** the baseline is the value at session start and after each Save; only the player's changes are dirty. Rows a script sets keep the script's value and are not dirty. Clean values are reread every frame so script changes show. Storage-less settings are held by the store and never dirty.
- **Discard** (Revert) sets dirty settings back in reverse order of last change, each as a change, so a reverted preset reruns its script with the old preset. It can't undo what a script did beyond the value. A script button doesn't ask to save first, as changes are made.
- **Save** (Keep) writes nothing; kept values become the baseline. If the session wrote a ModSetting, it then sends pause-menu open and close to the game's menu listeners, so DLLs that reread MCM's file on pause-menu close get the changes. It waits frame by frame until MCM's file has every written ModSetting, or a few seconds at most, also after the menu closes.

## Raw custom UI

Not planned: sharing the host's ImGui context across plugins would couple clients to its ownership, ImGui version, input, styling and crashes. A mod's own editor opens as [The mod's own UI](#the-mods-own-ui).
