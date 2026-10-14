# Architecture and lifecycle

## Purpose

Mods Config VR is a shared, VR-first configuration menu for Fallout 4 VR mods. It improves the current MCM experience while preserving compatibility with existing MCM menus.

The host is an F4SE plugin built on F4VR-CommonFramework. Most of its UI is one Dear ImGui panel, placed in the framework's vrui layout and operated with a controller ray. What the panel needed from the framework is done; see the [Framework work plan](framework-plan.md). The framework work that is left is made with the mod's milestones.

The host owns navigation, interaction, persistence coordination, diagnostics, and the shared visual language. Each mod continues to own the meaning and runtime application of its settings. A diagnostic, in these documents, is a line in the mod's log. The menu itself shows only that a mod is not available, and why; see [Notices](ui-and-vr-interaction.md#notices).

## Integration sources

Two sources fill one internal model:

1. Mods Config VR declarative JSON packages.
2. A bridge to MCM for existing Fallout 4 MCM menus.

A mod's DLL can also register as a native client, through a restricted C++ API. A client is not a third source. It adds to its mod's package: live preview, functions that buttons run, and the mod's own config UI.

The UI does not show which source a mod's menu comes from. What differs is in the mod's store: an MCM change is saved as it is made, and a package's on Save. See [The settings model](model.md).

```text
Declarative package ──┐
                      ├──> Unified settings model ──> VR UI
MCM config ───────────┘
```

## Ownership

- Mods Config VR owns menu discovery, rendering, input, validation, and safe persistence.
- A package definition describes the UI and binds controls to the mod's real settings files.
- A mod owns runtime interpretation and side effects.
- F4VR-CommonFramework is linked statically into each client mod, so every mod has its own copy. Its config base keeps session overrides: values applied over the INI in memory and never written to disk. A generic adapter in the framework drives them for live preview; see [CommonFramework adapter](#commonframework-adapter).
- Per-setting callbacks are already scoped to a registered package/client and therefore do not carry the `modName`.

## Package and runtime identity

Every declarative package has a stable identity, its `modName`. The host uses it for discovery, diagnostics, and matching an optional native client registration. The `modName` is not repeated in every scoped setting event.

A native client registers under a name, and the host matches it to the package whose `modName` is that name. A client that matches no package gets no setting events. If the mod registered a config UI of its own, it still has its entry in the quick launch section.

A package remains usable without a native client. In that case it can read, validate, and save supported file-backed settings, but it gets no live preview. Saved values reach the running mod only if the mod reads them again. CommonFramework mods do: the framework watches the INI and reloads it when it changes. A script mod reads them through the [Papyrus API](#papyrus-api).

The host cannot tell when a change takes effect in the running mod: a mod with a client can still read one of its values only when the game starts. So the package says it, with `applies`: `live`, `save` or `restart`. It is set for the mod, and a page, a section or a setting can have its own; see [When a change takes effect](schema.md#when-a-change-takes-effect).

## Persistence model

Mods Config VR writes directly to each mod's declared configuration files. There is no host-owned mirror of every mod's settings.

Supported storage formats are:

- INI
- JSON
- TOML

TOML comments and exact formatting are best-effort initially. This limitation is documented for package authors and logged in developer diagnostics; it is not shown as a routine player warning.

INI files follow the conventions CommonFramework mods use: `;` or `#` starts a comment only at the beginning of a line, so a value may contain either character. Comments, key order, and untouched lines are preserved. How each value is spelled in a given file, for example `true`/`false` or `1`/`0` for a boolean, is not settled; see [Open questions](open-questions.md).

Target paths are explicit and have no inferred default. Packages may declare multiple files. Supported roots initially are the game `Data` directory and `Documents/My Games/Fallout4VR`. A path is checked as written: absolute paths and `..` segments are rejected, and the file extension must match the declared format. Links are not resolved, so a file that a mod manager deploys as a link is used as the game sees it.

Files are written in place. The new content is serialized and verified in memory, then written over the existing file in one write. There are no temporary files, renames, or backup files. A file that is replaced instead of rewritten looks like a rename to a file watcher and can break the link a mod manager deployed it with; a file that is rewritten keeps working with both.

A declared file does not have to exist. The host creates it, with any missing folders in its path, on the first Save that changes one of its settings, and the new file holds only those settings.

## External changes

The host does not need a watcher or background conflict detector.

Immediately before Save, it reads the latest content of every affected file and applies only the settings that are dirty in the current editing session:

- An external change to an untouched setting is preserved.
- If both the external process and the player changed the same setting, the player's pending value wins.
- If the latest file cannot be parsed, Save fails and the file is left as it is.

## Editing session

Only one mod has an active editing session.

1. Selecting a mod reads its files and captures the values as the session baseline.
2. Player edits update a pending model.
3. A valid edit of a `live` setting is sent to the mod's native client as a preview, when the mod has one.
4. Focused and minimized modes retain the same session.
5. Save establishes a new baseline.
6. Discard restores the baseline and rolls back live previews.

A setting is dirty when its pending value differs from its baseline value. Changing a value and then changing it back leaves it clean, so Save does not write it.

Opening a different mod, or going to the menu's front page, while the current mod is dirty prompts **Save / Discard / Cancel**. Closing the menu follows the same rule.

A pending value that returns to its baseline is sent to the preview adapter as `Discard`, whether the player discarded it or changed it back. A client therefore holds a preview only for a dirty setting. A session with a dirty setting ends through Save or Discard, so no preview outlives its session.

Reset-to-default is an ordinary pending edit. It is not persisted until Save.

### Values at session start

The file is the source of the baseline, whatever state it is in:

- **Key present and valid.** The value is the baseline.
- **Key missing.** The setting shows its declared default and is clean. Save does not add the key unless the player changes the setting.
- **Value present but invalid** (wrong type, outside its limits, or not one of the choice options). The row shows the stored text with an invalid marker and is clean. Editing or resetting the setting replaces it.
- **File missing.** Every setting bound to the file shows its declared default and is clean. The first Save that changes one of them creates the file.

Only dirty settings can block Save. An invalid value that was already in the file does not, because the player did not enter it.

## Save lifecycle

The player-facing operation is named **Save**, not Apply. A value may already be visible through live preview; Save means accept and persist the pending values.

Save proceeds as follows:

1. Validate the dirty settings.
2. Read the latest content of every affected file into memory. A file that does not exist counts as empty.
3. Parse each file. A file that cannot be read or parsed fails the Save.
4. Apply the dirty settings.
5. Serialize each file to a memory buffer.
6. Parse each buffer and check that every dirty setting reads back with its intended value.
7. Write each buffer over its file, creating a file that does not exist.
8. If a write fails, write the content read in step 2 back to the files already written, delete the files this Save created, and report the failure.
9. Call the scoped native `OnSettingsSaved` callback with one batch.
10. Send one final native notification and one equivalent Papyrus notification.
11. Tell the player that the game has to be restarted, when a `restart` setting was written.

Nothing is written before step 7, so a validation, parsing, or serialization problem leaves every file untouched.

A crash during step 7 can leave the file being written incomplete. Each file is a few kilobytes written in one call, so the window is small and is accepted. Whether to keep a last-good copy is an open question.

The final notification means:

> Mod `FRIK` has had its configuration successfully saved. Reload it if you care.

It does not contain every changed setting. Native clients that need exact changes use the richer scoped batch API.

## Live preview API

Declarative-only integrations do not receive live preview. A registered native client receives a complete typed setting view, so it can update its in-memory representation without parsing `config.json`.

Conceptual types, as the client header presents them. The wire ABI underneath is C-compatible; see [Final notifications](#final-notifications).

A setting and its value:

```cpp
enum class ValueType
{
    Bool,
    Int,   // signed 32-bit
    Float, // 32-bit
    String,
    Color // typed access is not designed yet
};

struct SettingValue
{
    ValueType valueType;
    std::string_view text; // the value as text, for a client that only forwards it

    // typed access; each is valid only for its own type
    bool asBool() const;
    std::int32_t asInt() const;
    float asFloat() const;
    std::string_view asString() const;
};

enum class FileFormat
{
    Ini,
    Json,
    Toml
};

struct FileView
{
    std::string_view name; // the file's key in the package's `files`
    FileFormat format;
    std::string_view path; // absolute, with the root resolved
};

struct StorageView
{
    const FileView& file;
    std::string_view section;               // INI
    std::string_view key;                   // INI
    std::span<const std::string_view> path; // JSON and TOML
};

struct ValueOptionsView
{
    // each is empty when the package does not declare it
    std::optional<double> min; // numeric types
    std::optional<double> max;
    std::optional<double> step;
    std::optional<bool> allowEmpty; // string
    std::optional<std::uint32_t> minLength;
    std::optional<std::uint32_t> maxLength;
    std::span<const SettingValue> options; // the stored values of a choice
};

struct SettingView
{
    std::string_view id; // unique in the package
    ValueType valueType;
    SettingValue defaultValue;
    ValueOptionsView valueOptions;
    StorageView storage; // where the value is kept, resolved
};
```

The events a client receives:

```cpp
enum class ChangePhase
{
    Preview, // a valid pending value changed
    Discard  // the pending value was rolled back to the session baseline
};

struct SettingChange
{
    const SettingView& setting;
    SettingValue previousValue; // the value this client last received for the setting, or its baseline
    SettingValue value;
    ChangePhase phase;
};

struct SavedSetting
{
    const SettingView& setting;
    SettingValue baselineValue; // the value before this editing session
    SettingValue value;         // the value written to the file
};

struct SaveBatch
{
    std::span<const SavedSetting> settings;
    std::span<const FileView> writtenFiles;
    std::uint64_t revision; // counts this package's successful Saves since the game started
};
```

`SettingView` exposes the stable setting ID, value type, default, value options, and resolved storage. It does not require the client to parse the declarative package.

The host owns the views and the text in them. They are valid during the callback, and a client copies what it keeps.

Preview events occur only when a valid value of a `live` setting changes. Slider previews are coalesced to at most the useful render/update rate; they are not generated merely because a control is focused.

Special gameplay side effects can use an optional binding API later.

### CommonFramework adapter

A CommonFramework mod does not need client code of its own. The framework registers a generic adapter that maps the API onto the config base's session overrides, which are keyed by INI section and key:

1. A preview sets an override for the setting's resolved section and key.
2. A discard clears it.
3. After a Save, the adapter clears the overrides it set. The file holds the values by then, so the mod reloads them from disk.

The adapter needs only the resolved storage and the value as text. The mod's own loading code parses and type-checks it and applies it as it applies any reload.

A preview therefore does what a reload of the INI does, and no more. A value that the mod reads only once, when the game starts, is changed by neither, so the mod's package marks that setting `restart`.

The framework registers the adapter for every mod, under the mod's own name, whether or not the mod ships a package. The mod's package uses that name as its `modName`. This is the name the framework uses for the mod's log and config, which is not always its F4SE plugin name: FRIK's `modName` is `frik`, and its F4SE plugin name, `F4VRBody`, goes in `f4sePluginRequirements`.

The framework side of this is listed in [CommonFramework work required](framework-requirements.md).

## Final notifications

Successful Save emits one notification through each supported ecosystem:

- An F4SE plugin message for native listeners. The host, F4SE plugin `ModsConfigVR`, sends it to each F4SE plugin the package names in `f4sePluginRequirements`. The payload carries the package's `modName`.
- An F4SE external Papyrus event for script mods, named `OnModsConfigVRSaved|<modName>`. A script registers for it with F4SE's `RegisterForExternalEvent`, and its callback receives the `modName`.

Raw F4SE plugin messaging does not reach Papyrus by itself, so these are separate transports carrying the same semantic event.

A native listener registers for the sender `ModsConfigVR`. F4SE accepts that registration only once the sender is loaded, so a client registers at `kPostLoad` or later.

The wire ABI must be versioned and C-compatible. It must not pass `std::string`, `std::variant`, or other C++ library objects across DLL boundaries. A small client header can hide discovery, registration timing, filtering, and decoding. The devbench client that CommonFramework vendors (`DevBenchAPI.h`) is the model: an interface fetched once at `kPostPostLoad`, a DLL export as the fallback where F4SEVR's listener registration falls short, and functions appended behind a version check.

Papyrus scripts cannot read INI, JSON, or TOML files on their own, so the Papyrus event tells a script that something changed but not what. The script reads the values through the [Papyrus API](#papyrus-api).

## Papyrus API

A script-only mod, a game plugin and scripts with no DLL, ships a package like any other mod, and its scripts read the saved values through functions that Mods Config VR provides. It needs neither MCM nor a DLL of its own.

```papyrus
Scriptname ModsConfigVR Native Hidden

bool Function GetModSettingBool(string asModName, string asSetting) native global
int Function GetModSettingInt(string asModName, string asSetting) native global
float Function GetModSettingFloat(string asModName, string asSetting) native global
string Function GetModSettingString(string asModName, string asSetting) native global
```

`asModName` is the package's `modName` and `asSetting` is the setting's `id`. The functions are named as MCM's `MCM.GetModSetting*` are, and a setting with no `id` of its own is named as MCM names a ModSetting, as `fPlayerHeight:MyMod`. So a script that moves from MCM to a package, and keeps its mod name, sections and keys, changes `MCM.` to `ModsConfigVR.` in its getter calls and nothing else in them. Both are matched without regard to case, because Papyrus does not preserve the case of a string.

The functions are global, so they need a compiled script and no game plugin. Mods Config VR ships `Data/Scripts/ModsConfigVR.pex`, with its source in `Data/Scripts/Source/User/` for authors to compile against, and the DLL registers the functions when it loads. MCM provides its `MCM.GetModSetting*` functions the same way.

The API covers Mods Config VR packages. A mod with an MCM menu keeps using MCM's functions.

A mod that reads its settings this way requires Mods Config VR, as a mod with an MCM menu requires MCM.

### What a getter returns

A getter answers from the mod's file. The cases are the ones the menu handles at [session start](#values-at-session-start):

- **Key present and valid.** The value in the file.
- **Key or file missing.** The declared default.
- **Value invalid.** The declared default, with a diagnostic.
- **Unknown package or setting, or the wrong getter for the setting's `valueType`.** `false`, `0`, `0.0`, or an empty string, with a diagnostic. Types are not converted.

A choice is read with the getter for its `valueType`. Papyrus `int` and `float` are the same 32-bit types the schema uses. How a color is returned is an open question.

A getter never returns a pending value. A script mod therefore has no live preview: it sees a change after Save.

The file remains the only store. The host may keep parsed files in memory to answer quickly; how soon it notices a file that was changed outside the menu is an open question.

Getters have to work from the first script that runs, so packages are loaded when the game's data is ready, before a save is loaded. They cannot wait for the menu to be opened.

### When a script reads its settings

After each successful Save, the host sends the package's Papyrus event, described under [Final notifications](#final-notifications). The files are written before the event is sent, so a getter called from the handler returns the saved value.

```papyrus
Event OnQuestInit()
    RegisterForExternalEvent("OnModsConfigVRSaved|mymod", "OnConfigSaved")
    LoadConfig()
EndEvent

Function OnConfigSaved(string asModName)
    LoadConfig()
EndFunction

Function LoadConfig()
    fPlayerHeight = ModsConfigVR.GetModSettingFloat("mymod", "playerHeight")
EndFunction
```

Settings live in files and are the same for every save game, while a script's variables are stored in the save. A script therefore reads its settings again after a save is loaded, not only when it starts and on the save event.

### The settings file of a script-only mod

A script-only mod has no code that creates its settings file. The host creates it on the first Save that changes one of its settings, and the file holds only the settings the player has changed. For every other setting a getter returns the declared default, so the package is where a script-only mod's defaults live.

## Buttons and actions

A button is not a setting. It has no storage, default, dirty state, or persisted value.

A button's action is written as in an MCM config, and a package can use every action the menu supports for MCM, such as calling a function of a script. A script-only mod therefore has buttons in its package, as it has in MCM.

A package has one action of its own, which runs a function that the package's native client registered, by its name. The function receives typed JSON parameters and read-only access to current pending settings. In a package with no registered native client, such a button is shown disabled with an explanation.

Basic results are success or failure plus an optional localized message. Confirmation metadata is declarative so buttons share one consistent VR dialog.

Long-running progress, cancellation, and external UI ownership are deferred.

## Bespoke UI handoff

Mods such as FRIK, Immersive Flashlight VR, and Virtual Holsters may expose a launch action for a specialized live editor.

The intended lifecycle is:

1. Resolve any pending changes through Save or Discard.
2. Minimize Mods Config VR.
3. Release input ownership.
4. Ask the client to open its editor.
5. Wait for explicit `Opened`, `Closed`, or `Failed` status.
6. Restore the previous Mods Config VR context.

Timeout/recovery behavior and protection against competing UI owners require a later API design pass.

## MCM compatibility

Mods Config VR does not replace MCM. It bridges to it: MCM stays installed and keeps ownership of its settings, its Papyrus functions, and its hotkeys, and Mods Config VR shows the same menus in VR.

- Mods Config VR and the packages written for it do not require MCM.
- MCM-backed mods appear only when MCM is installed. A mod with an MCM menu already requires MCM, because its scripts call MCM's Papyrus functions.
- Without MCM, MCM menus are not shown, and a diagnostic says why.

Replacing MCM would mean reimplementing its Papyrus functions, its setting store, and its hotkey dispatch, and asking players to uninstall it. Bridging is less work and cannot change how existing mods behave.

MCM pages remain a compatibility source rather than being rewritten into the new persistence lifecycle. The bridge:

- Reads each mod's MCM `config.json` and its declared default settings.
- Writes ModSettings through MCM's own `MCM.SetModSetting*` functions, so MCM's in-memory store and its settings file stay in agreement. It never writes MCM's settings files itself.
- Reads and writes GlobalValue and PropertyValue sources directly, as MCM does.
- Sends the existing `OnMCMSettingChange|ModName` external event after a change, and the menu open and close events when the menu opens and closes and when an MCM mod is shown and left, so scripts react as they do to MCM's own menu.
- Runs the supported MCM button actions.
- Shows hotkeys read-only. MCM keeps dispatching them.

`MCM.RefreshMenu()` stays MCM's own function and refreshes MCM's own menu. The bridge cannot observe the call, so it reads MCM's settings file again as another page of the mod is shown.

MCM settings are saved as they are changed, as existing mods expect. They do not acquire staged Save/Discard behavior. MCM mods are not marked or grouped apart in the UI. It shows a “Changes are saved as they are made” status instead of the normal Save bar.

Saved is all the status says. When the mod acts on a change is the mod's, and an MCM config has no field for it: a script reads a global when it needs it, and a DLL can read MCM's settings file only when the game starts. A mod that needs a restart says so in its own text, which the menu shows.

## Raw custom UI

Ordinary integrations use declarative controls or the restricted API. Exposing the host's raw ImGui context across plugin boundaries is not planned for the initial architecture because it couples clients to context ownership, ImGui versions, input, styling, and crash behavior.

If hosted custom UI is added later, it should use a host-owned drawing interface with an explicit ABI. Full bespoke editors can instead use the external UI handoff lifecycle.
