# Architecture and lifecycle

## Purpose

Mods Config FO4 VR is a shared, VR-first configuration menu for Fallout 4 VR mods. It improves the current MCM experience while preserving compatibility with existing MCM menus.

The host is an F4SE plugin built on F4VR-CommonFramework. Most of its UI is one Dear ImGui panel, placed in the framework's vrui layout and operated with a controller ray. What the panel needed from the framework is done; see the [Framework work plan](framework-plan.md). The framework work that is left is made with the mod's milestones.

The host owns navigation, interaction, persistence coordination, diagnostics, and the shared visual language. Each mod continues to own the meaning and runtime application of its settings. A diagnostic, in these documents, is a line in the mod's log. To a player the menu shows only that a mod is not available, and why; see [Notices](ui-and-vr-interaction.md#notices). To a config spec's author, who turns on Developer tools in the menu's own settings, it shows each config spec's diagnostics too; see [Developer tools](ui-and-vr-interaction.md#developer-tools).

## Integration sources

Two sources fill one internal model:

1. Mods Config FO4 VR declarative JSON config specs.
2. A bridge to MCM for existing Fallout 4 MCM menus.

A mod's DLL can also register as a native client, through a restricted C++ API; see [Native API](#native-api). A client is not a third source. It adds to its mod's config spec: live preview, functions that buttons run, options and states that only its code knows, and entries of the quick launch section that open its own UI.

The UI does not show which source a mod's menu comes from. Both have the Save bar, and what differs is in the mod's store: an MCM change is made at once through MCM, as MCM's menu makes it, and Save keeps it, and a config spec's waits for Save, which writes it into its files and the game. See [The settings model](model.md).

```text
Declarative config spec ──┐
                      ├──> Unified settings model ──> VR UI
MCM config ───────────┘
```

## Ownership

- Mods Config FO4 VR owns menu discovery, rendering, input, validation, and safe persistence.
- A config spec definition describes the UI and binds controls to the mod's real settings: keys of its files, and globals and properties of its scripts.
- A mod owns runtime interpretation and side effects.
- F4VR-CommonFramework is linked statically into each client mod, so every mod has its own copy. Its config base keeps session overrides: values applied over the INI in memory and never written to disk. A generic adapter in the framework drives them for live preview; see [CommonFramework adapter](#commonframework-adapter).
- Per-setting callbacks are already scoped to a registered config spec/client and therefore do not carry the `modName`.

## Config spec and runtime identity

Every declarative config spec has a stable identity, its `modName`. The host uses it for discovery, diagnostics, and matching an optional native client registration. The `modName` is not repeated in every scoped setting event.

A native client registers under a name, and the host matches it to the config spec whose `modName` is that name. A client that matches no config spec gets no setting events. The quick launch entries it added are still on the menu's front page.

A config spec remains usable without a native client. In that case it can read, validate, and save its settings, and gets live preview only for a setting kept in a global or a script property, which the host writes into the game itself. Saved values reach the running mod only if the mod reads them again. CommonFramework mods do: the framework watches the INI and reloads it when it changes. A script mod reads them through the [Papyrus API](#papyrus-api), or keeps them in globals and properties, which it reads itself, and applies a change in the action of its setting.

The host cannot tell when a change takes effect in the running mod: a mod can read a global only when it needs it, and a mod with a client can still read one of its values only when the game starts. So the config spec can say it, with `applies`: `live`, `save` or `restart`, and where it does not, it is `unknown`. It is set for the mod, and a page, a section or a setting can have its own; see [When a change takes effect](schema.md#when-a-change-takes-effect). An MCM mod's settings are `unknown`.

## Persistence model

Mods Config FO4 VR writes directly to each mod's declared configuration files. There is no host-owned mirror of every mod's settings.

Supported storage formats are:

- INI
- TOML

A TOML file is changed with the framework's `TomlEditor`, as a CommonFramework mod saves its own: a value is replaced where it is written, and a key the file does not have is added after the last value of its table. Comments, blank lines, key order, and how every other value is written are preserved.

An INI file is read as CommonFramework mods and SimpleIni read one, and changed line by line: a value's text is replaced where it is written, and a key the file does not have is added after the last value of its section, or in a new section at the end of the file. Comments, blank lines, key order, and every other line are preserved.

- `;` or `#` starts a comment only at the beginning of a line, so a value may contain either character. A value is the text after its `=`, with no spaces around it.
- Section and key names are matched with no regard to case. A key before the first section is named by its key alone.
- A key that is in the file more than once has the value of its last, as SimpleIni reads it. A write sets each of them, so a reader that takes the first agrees. A section that is in the file more than once is one section.
- A boolean is read from `1`, `true`, `yes` or `on`, and `0`, `false`, `no` or `off`, in any case. It is written in the words and the case the file has for it, and as `1` or `0` for a new key, which every reader takes.
- A whole number is read in decimals, or in hex after `0x` as SimpleIni reads it, and is written in hex where the file has it so, with as many digits, as a form ID is.
- A number is written as short as it reads back as the same number, with a decimal point. A value the file has already keeps its text, as `1.250`.

Target paths are explicit and have no inferred default. Config specs may declare multiple files. Supported roots initially are the game `Data` directory and `Documents/My Games/Fallout4VR`. A path is checked as written: absolute paths and `..` segments are rejected, and the file extension must match the declared format. Links are not resolved, so a file that a mod manager deploys as a link is used as the game sees it. The game's own INI files under `My Games/Fallout4VR`, as `Fallout4Custom.ini`, are valid targets: the game reads them as it starts, so their settings are `applies` `restart`.

Files are written in place. The new content is serialized and verified in memory, then written over the existing file in one write. There are no temporary files, renames, or backup files. A file that is replaced instead of rewritten looks like a rename to a file watcher and can break the link a mod manager deployed it with; a file that is rewritten keeps working with both.

A declared file does not have to exist. The host creates it, with any missing folders in its path, on the first Save that changes one of its settings, and the new file holds only those settings. A config spec cannot forbid it: a mod reads a file that holds some of its settings with its defaults for the rest.

The host reads a mod's files as its session starts and writes them on Save, on the game thread. A config file is small, and each is read and written once.

A config spec can also keep a setting in a global or a property of a script, which the host reads and writes in the game, as MCM does. Its value is in the save game, not in a file; see [Global and script property](schema.md#global-and-script-property). And it can keep one in a state of the mod's native client, whose value is in the memory of the mod's DLL, which applies it; see [A client's state](#a-clients-state).

## External changes

The host does not need a watcher or background conflict detector.

Immediately before Save, it reads the latest content of every affected file and applies only the settings that are dirty in the current editing session:

- An external change to an untouched setting is preserved.
- If both the external process and the player changed the same setting, the player's pending value wins.
- If the latest file cannot be parsed, Save fails and the file is left as it is.

A mod that changes its own values while its menu is open, as FRIK's own UI saves its calibration while the menu is minimized, has the menu read them again with the same rule: a clean setting takes the new value, and a pending one stays the player's. See [The mod's own UI](#the-mods-own-ui).

## Editing session

Only one mod has an active editing session.

1. Selecting a mod reads its files, globals, properties and its client's states, and captures the values as the session baseline.
2. Player edits update a pending model.
3. A valid edit of a `live` setting is previewed. A key of a file is sent to the mod's native client, when the mod has one. A global or a property is written into the game by the host, and then sent to the client too, which can do what more it needs. A state of the client is sent to the client, which applies it.
4. Focused and minimized modes retain the same session.
5. Save establishes a new baseline.
6. Discard restores the baseline and rolls back live previews.

A setting is dirty when its pending value differs from its baseline value. Changing a value and then changing it back leaves it clean, so Save does not write it.

Opening a different mod, or going to the menu's front page, while the current mod is dirty prompts **Save / Discard / Cancel**. Closing the menu follows the same rule.

A pending value that returns to its baseline is sent to the preview adapter as `Discard`, whether the player discarded it or changed it back, and a global or a property that was previewed gets its baseline written back. A client therefore holds a preview only for a dirty setting. A session with a dirty setting ends through Save or Discard, so no preview outlives its session.

Reset-to-default is an ordinary pending edit. It is not persisted until Save.

An MCM mod has the same session, and each of its changes is made in the game as the player makes it, as a `live` one is previewed: see [MCM compatibility](#mcm-compatibility).

### Values at session start

For a global or a property the baseline is the game's value, and one that is not in the game is not available, with the reason, as an MCM row is. For a state of the client it is the client's answer, and a state it does not answer is not available, with the reason; an answer of another type, outside its limits or no option of its choice is invalid, as a file's value is. For a key of a file, the file is the source of the baseline, whatever state it is in:

- **Key present and valid.** The value is the baseline.
- **Key missing.** The setting shows its declared default and is clean. Save does not add the key unless the player changes the setting.
- **Value present but invalid** (wrong type, outside its limits, or not one of the choice options). The row shows the stored text with an invalid marker and is clean. Editing or resetting the setting replaces it.
- **File missing.** Every setting bound to the file shows its declared default and is clean. The first Save that changes one of them creates the file.
- **File that cannot be read or parsed**, as one its owner is in the middle of rewriting. Every setting bound to the file is not available, with the reason, until the mod's values are read again.

Only dirty settings can block Save. An invalid value that was already in the file does not, because the player did not enter it.

## Save lifecycle

The player-facing operation is named **Save**, not Apply. A value may already be visible through live preview; Save means accept and persist the pending values.

A config spec's Save proceeds as follows, and an MCM mod's is in [MCM compatibility](#mcm-compatibility):

1. Validate the dirty settings.
2. Read the latest content of every affected file into memory. A file that does not exist counts as empty.
3. Parse each file. A file that cannot be read or parsed fails the Save.
4. Apply the dirty settings.
5. Serialize each file to a memory buffer.
6. Parse each buffer and check that every dirty setting reads back with its intended value.
7. Check that every dirty global and property is still in the game, and that the client of every dirty state is there and not cut off. One that is not fails the Save.
8. Write each buffer over its file, creating a file that does not exist, with any missing folders in its path.
9. If a write fails, write the content read in step 2 back to the files already written, delete the files this Save created, and report the failure. The folders it created stay. The Save stops there.
10. Write the dirty globals and properties into the game.
11. Run the action of each setting that was written, in the order of the config spec, with its saved value.
12. Call the client's `onSaved` with one batch: every setting that was written, with its value before the Save, and the files written. A state of the client is written by this call, which the client applies it from. A Save that wrote nothing tells nothing.
13. Send one final native notification and one equivalent Papyrus notification, see [Final notifications](#final-notifications).
14. Tell the player that the game has to be restarted, when a `restart` setting was written.

Nothing is written before step 8, so a validation, parsing, or serialization problem, or a global or a property that is gone, leaves every file untouched. The game's values are written after the files, so a Save that fails changes none of them either. A Save that fails keeps every setting dirty, so the next Save tries them all again. The footer's status says why it failed, in the design system's SaveBar's Not saved state, until the next change, Save or Discard.

A crash during step 8 can leave the file being written incomplete. Each file is a few kilobytes written in one call, so the window is small and is accepted. Whether to keep a last-good copy is an open question.

The final notification means:

> Mod `FRIK` has had its configuration successfully saved. Reload it if you care.

It does not contain every changed setting. Native clients that need exact changes use the richer scoped batch API.

## Native API

A mod's DLL talks to the host through `ModsConfigFO4Api.h`, a client header the mod copies as it is, as a mod copies FRIK's `FRIKApi.h`. The framework carries a copy, and `ModBase` connects every CommonFramework mod's client with it; see [CommonFramework adapter](#commonframework-adapter).

### Binary interface

What passes between the DLLs is two interfaces of virtual functions. The host implements `ModsConfigFO4ApiV1`, and a client implements `IClient`, which it registers once. The host asks the client by name for what its config spec names: a function, a state, the options of a choice. So a client registers nothing by name, and a name the client does not answer is one it does not have.

`ModsConfigFO4VR.dll` exports one function, `ModsConfigFO4API_GetApiV1`, which returns the host's interface for the whole run of the game. The header's `ModsConfigFO4ApiV1::initialize` finds the DLL with `GetModuleHandle` and calls the export, as `FRIKApi::initialize` does. F4SE has loaded the DLL of every plugin by `kPostLoad`, so a client calls it from then on. While Mods Config FO4 VR is not installed it fails, and the mod works without it. The API is not asked for with an F4SE message: F4SEVR's listeners do not reach a plugin in every load order, which is why devbench's client has the same export as its fallback.

- An interface's methods take whole numbers of a fixed width, `float`, `bool`, enums on `std::uint32_t`, `const char*`, plain structs of those, and pointers to an interface. No `std::` type and no exception passes: each DLL has its own heap, and its own layout of them. An interface has single inheritance and no data, whose virtual table MSVC lays out as COM has it, and a method is only added at its end.
- Text is UTF-8, and ends with a zero. A text of the client that the player sees, as the message of a function or the text of a quick launch entry, can be a `$` token, which the host translates as it translates a config spec's text.
- What the host passes is valid during the call, and the client copies what it keeps. What the client passes the host copies before the call returns. What a method answers goes through an interface the host passes it, as `IFunctionCall::setMessage`, so it can only be given while the method runs.
- Every call, either way, is on the game's main thread. The host calls a client from its frame update, inside the guard of [A client that fails](#a-client-that-fails).

The header has a C++ layer over the interfaces, `ModsConfigFO4Client`, which is compiled into the client's DLL: an `IClient` that answers with the mod's lambdas, by name. A mod adds its functions, states, options and quick launch entries to it at any time, and connects it once. Nothing of it crosses to the host.

### Versions

- `V1` in the names changes only with a change that breaks a client. A V2 is another interface and another export, and the DLL keeps the export of V1 beside it, so a client built for V1 keeps working.
- Within V1, `getVersion` counts what was added: a method is only added at the end of an interface, and a struct only grows at its end. A client connects to any menu of V1, and uses what a later version added after it checks the menu's version (the header's client's `apiVersion()`), so a client of a newer header keeps working with an older menu. One that does not work at all with an older menu asks `connect` for the version it needs, and an older menu is not connected to.
- A client gives the version of its header as it registers, so the host calls only the methods of `IClient` that client has.
- The version is 1 at the first release, and V1 changes as the milestones need it until then, so a client is built again with the header of the menu it runs with. How the API and the schema change after it is [M82](mod-plan-phase-2.md#11-first-release)'s.

### The API

```cpp
namespace mods_config::api
{
    // the host's side, which its DLL exports
    class ModsConfigFO4ApiV1
    {
    public:
        virtual std::uint32_t getVersion() const = 0;
        virtual const char* getModVersion() const = 0; // Mods Config FO4 VR's own version

        // the client's entries of the quick launch section, one for each function
        virtual bool addQuickLaunch(IClient* client, const char* text, const char* help, const char* buttonText, const char* functionName) const = 0;

        // the client, once, under its mod's name: false when the name has a client already
        virtual bool registerClient(const char* name, std::uint32_t version, IClient* client) const = 0;

        // the menu, for any DLL, with or without a client
        virtual MenuState getMenuState() const = 0;
        virtual bool openMenu(const char* modName, const char* pageId) const = 0; // no mod: the front page; no page: the mod's front page
        virtual void minimizeMenu() const = 0;
        virtual void restoreMenu() const = 0; // only while it is minimized, and reads the shown mod's values again
        virtual void reloadValues(IClient* client) const = 0; // reads its mod's values again, while it is shown

        [[nodiscard]] static int initialize(std::uint32_t minVersion = MODS_CONFIG_FO4_API_VERSION);
        inline static const ModsConfigFO4ApiV1* inst = nullptr;
    };

    // the client's side, which the host asks by name
    class IClient
    {
    public:
        virtual bool hasFunction(const char* name) = 0;
        virtual bool runFunction(const char* name, IFunctionCall* call) = 0;
        virtual bool answerState(const char* name, SettingValue* value) = 0;             // false: not available
        virtual bool giveOptions(const SettingView* setting, IOptions* options) = 0; // false: it has none
        virtual void onEvent(const MenuEvent* event) = 0;
        virtual void onChange(const SettingChange* change) = 0; // a `live` setting's preview or discard
        virtual void onSaved(const SaveBatch* batch) = 0;       // what a Save wrote
    };
}
```

A second entry of a function, and a second client of a name, are refused, and the first stays.

### The client

A mod registers its client once, as it loads, and it lives for the whole run of the game: there is no unregister. The host matches its name to the config spec whose `modName` is that name. A second client under a name that has one is refused, so two mods cannot both have a config spec. The client is asked only of its own config spec, and a client that fails is cut off as a whole.

A client is told each change of a `live` setting of its config spec with `onChange`, also of one kept in the game, which the host writes first, as [Live preview API](#live-preview-api) has it, and the header's C++ client hands it to the mod's handlers (`subscribeToChanges`). It is told once what each Save wrote with `onSaved`, every setting of any `applies`, which ends the previews of the ones it wrote, and the C++ client hands that to the mod's handlers too (`subscribeToSaves`).

### The client's events

```cpp
enum class MenuEventType : std::uint32_t
{
    MenuOpened,
    MenuClosed,
    ModShown,
    ModLeft,
    PageShown,
    PageLeft
};

struct MenuEvent
{
    MenuEventType type;
    const char* pageId; // the page's `id` in the config spec, for PageShown and PageLeft
};
```

- Every client is told that the menu opened and closed, so a mod can put its own UI away while the menu is open. A minimized menu is still open.
- The other events are of the client's own config spec. Its mod is shown as it is selected, and left as another mod or the menu's front page is shown, and as the menu closes.
- A page of its config spec that has an `id` is shown and left as its mod is, and as another page is shown. A client shows in the world what the page sets while the player is on it: Immersive Flashlight draws its activation spheres while the page of its spheres is shown. A page with no `id` sends none.

### Functions

A `CallNativeFunction` button runs a function of its mod's client, by its name, and so does a quick launch entry.

```cpp
struct NamedValue
{
    const char* name;
    SettingValue value; // in its own JSON type
};

class IFunctionCall
{
public:
    virtual std::uint32_t paramCount() = 0;
    virtual const NamedValue* param(std::uint32_t index) = 0;
    virtual void setMessage(const char* text) = 0; // what the help line shows of the result
};
```

- `runFunction` returns at once, true when the function did what it does. Its message is in the help line after the info mark, or after the invalid mark when it returns false, as the result of an action is. Work that takes longer goes on after it returns, and a [state](#a-clients-state) of the client tells the rows that it is running.
- A button whose client does not have its function, as `hasFunction` says, is disabled, with the reason in the help line. The host asks it as the button is drawn.

### Quick launch

`addQuickLaunch` adds an entry of the client to the quick launch section of the menu's front page: its `text`, its `help`, which the help line shows, its `buttonText` in place of `Open`, and the function that a press runs. A client adds an entry for each UI it opens from there, as FRIK's main config and its Pip-Boy config: a function has one entry. A client that matches no config spec has its entries too. The function is asked for as the entry is pressed.

### A client's state

A condition of a config spec can ask a state of its client, by its name; see [Conditions](schema.md#conditions). The host asks it with `answerState`, which is false while the state is not available.

The host asks each state once a frame, only while the client's mod is shown, so the rows of one state cost one call. The method answers at once and changes nothing. A state the client does not answer, and every state of a client that is cut off, is not available.

A file of a config spec can take its path from a state too, with `pathFromClientState`; see [Files](schema.md#files). The host asks that one as the mod is selected and as its values are read again.

A setting can be kept in a state, a value in the memory of the mod's DLL; see [Client state](schema.md#client-state). The host asks it as the mod is selected and as its values are read again, as a file's value, and writes nothing: a change reaches the client as a change of any setting does, through `onChange` for a `live` one and `onSaved` for what a Save wrote, whose storage names the state. The header's C++ client takes a setter with a state, and calls it with the value of each of those changes of a setting kept in it, before the mod's own handlers:

```cpp
client.addState("pipboyType",
    [] { return std::optional<Value>(readPipboyType()); },
    [](const Value& value) { applyPipboyType(std::get<int>(value)); });
```

- A `live` one has its setter called with each valid pending value, with its baseline on Discard, and again with the value at Save, which it has already. A setter applies a value, so a value applied twice changes nothing.
- A setter applies, and does not persist. A mod that keeps the value after the game is restarted writes it in its handler of the Saves, which a preview does not reach.
- A setter cannot refuse a value: the host has checked it against its setting, and by the time a Save tells the client, it has written its files.
- A state with no setter is not changed, and its setting is `readOnly`. The host cannot see a setter, so a setting that is not marked so is edited, and its changes reach no setter.
- The mod tells the menu a change it made itself, as its own UI's, with `reloadValues`: a clean setting takes the new value, and a pending one stays the player's.

### The mod's own UI

A mod's own UI is opened by a function of its client, from a button of its config spec or from a quick launch entry. The function moves the menu out of the way itself, with the calls of the menu, which any DLL can make:

- `minimizeMenu` as it opens its UI. A minimized menu keeps its editing session, so a pending change stays pending. A function whose UI does not open does not minimize it, and its message says why.
- `restoreMenu` as its UI closes. It shows the menu where it was, and only while it is minimized: a player who has pressed `Resume` on the wrist or closed the menu is not shown it again. A UI that never calls it leaves the menu minimized, and `Resume` shows it.
- `restoreMenu` reads the values of the shown mod again, as its UI can have changed them: a clean setting takes the value of its file, its global or its property, and a pending one stays the player's, as Save keeps it. `reloadValues` does the same for a client whose mod changes its values at another time while it is shown, as with a hotkey.
- `openMenu` opens the menu at a mod and at a page of it by its `id`, at the mod's front page with no page, or at the menu's front page with no mod. A mod's wrist button or holotape opens its settings this way. While the menu is open on a mod with unsaved changes, the unsaved-changes dialog asks first. A mod or a page that is not there opens nothing, and the call returns false.

The header's C++ client adds a quick launch entry with its function, so an entry always has one, and has the calls of the menu. A quick action is a function that does its work and returns its message; one that opens a UI minimizes the menu, and the mod restores it from whatever ends the UI:

```cpp
auto& client = ModsConfigFO4Client::get();
client.addQuickLaunch("openPipboyConfig", { .text = "FRIK: Pip-Boy", .help = "Moves the Pip-Boy on your wrist." },
    [&client](const Params&) {
        if (!isSkeletonReady()) {
            return FunctionResult{ .done = false, .message = "The body is not ready yet." };
        }
        openPipboyConfigUI([&client] { client.restoreMenu(); });
        client.minimizeMenu();
        return FunctionResult{ .done = true };
    });
```

## Live preview API

A config spec with no native client gets live preview only for a setting kept in a global or a script property, which the host writes into the game. A registered native client receives a complete typed setting view, so it can update its in-memory representation without parsing `spec.json`.

Conceptual types, as the client header presents them. Across the DLLs each text is a `const char*` and each list a pointer and a count, as [Binary interface](#binary-interface) has it, and the header reads them as below.

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
    std::string_view name; // the file's key in the config spec's `files`
    FileFormat format;
    std::string_view path; // absolute, with the root resolved
};

struct StorageView
{
    const FileView* file;                   // none for a global or a script property
    std::string_view section;               // INI
    std::string_view key;                   // INI
    std::span<const std::string_view> path; // JSON and TOML
    std::string_view form;                  // a global, or the form of a script property
    std::string_view scriptName;            // a script property
    std::string_view propertyName;          // a script property
    std::string_view clientState;           // a state of the client
};

struct ValueOptionsView
{
    // each is empty when the config spec does not declare it
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
    std::string_view id; // unique in the config spec
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
    SettingValue baselineValue; // the value before the Save
    SettingValue value;         // the value written, to the file or the game
};

struct SaveBatch
{
    std::span<const SavedSetting> settings; // in the order of the config spec
    std::span<const FileView> writtenFiles;
    std::uint64_t revision; // counts this config spec's Saves that wrote something since the game started
};
```

A Save tells the client its batch once, after the action of each setting ran. It has every setting the Save wrote, whatever its `applies`, so a mod applies a `save` setting from it, as it is written. A `live` setting the client shows a preview of is told no Discard: the batch has it, and the file or the game has the value. A Save that wrote some settings and failed on a global that left the game has those it wrote.

`SettingView` exposes the stable setting ID, value type, default, value options, and resolved storage. It does not require the client to parse the declarative config spec. The header passes its id, its type and its storage in a file: the file's name, format and absolute path, and the names that lead to the value, its section and key in an INI file and its tables and key in a TOML file. A setting kept in the game has no file, and its form, with a property's script and name: the host writes its value into the game before it tells the client, which can do what more it needs. A setting kept in a state of the client has the state's name alone: the host writes nothing, and the client applies the value.

The host owns the views and the text in them. They are valid during the callback, and a client copies what it keeps.

Preview events occur only when a valid value of a `live` setting changes. A slider sets its value as the trigger is let go and at each step of the thumbstick, so a drag is one preview and nothing is coalesced. The ray on a control sends none.

Special gameplay side effects can use an optional binding API later.

### Options from a client

A choice with `listFromClientCallback` takes its options from its mod's client; see [Options from the mod's client](schema.md#options-from-the-mods-client). The host asks the client for them by the setting's id, as the mod is selected:

```cpp
class IOptions
{
public:
    // adds an option, in the setting's valueType, after those added before; a null text shows the value
    virtual void add(const SettingValue* value, const char* text) = 0;
};
```

The host asks `giveOptions` with the setting's view, which is false for a choice the client has no options of. It is called inside the guard of [A client that fails](#a-client-that-fails). The menu is a client of the API too, the one the framework connects for it as for any CommonFramework mod: it gives the options of its own config spec's Config spec setting, the config specs it found.

### A client that fails

The host calls a client inside a guard that catches a C++ exception and a fault, as an access violation. A client whose call fails is cut off until the game is restarted, and its mod is handled as a mod with no native client: its `live` settings are not previewed, its native buttons and its quick launch entries are disabled, with the reason in the help line, and its states are not available. The rest of its config spec, and every other mod, keep working. The log names the client and the call that failed.

### CommonFramework adapter

A CommonFramework mod does not need client code of its own. The framework registers a generic adapter that maps the API onto the config base's session overrides of the mod's TOML config, which are keyed by the value's path in the file, under an owner of their own, `ModsConfigFO4VR`:

1. A preview sets an override for the setting's path.
2. A discard clears it.
3. After a Save, the adapter clears the overrides of the settings the batch has. The file holds the values by then, so the mod reloads them from disk.

The adapter acts only on a setting of the mod's own config file, which it knows by the file's absolute path. The framework's client answers the state `configFile` with that path, the one the mod loaded (`ConfigBase::getConfigFilePath()`), and `modVersion` with the mod's version. A mod that loads another file at runtime, as FRIK loads `FRIK_FOLVR.toml` with Fallout London VR, has its config spec's file name `configFile` in `pathFromClientState`, so the menu reads, previews and saves the file the mod loaded, and the adapter's path matches it. It needs only the path of the value and the value. The mod's own loading code applies it as it applies any reload. A mod that has a setting of another file, or of the game, handles it with its own handlers of the client, and one kept in a state with the state's setter.

A preview therefore does what a reload of the config file does, and no more. A mod that does more work for a value to take effect subscribes to the config's change of that key, which a preview, a discard and a Save tell as a change of the file does. A value that the mod reads only once, when the game starts, is changed by neither, so the mod's config spec marks that setting `restart`.

The framework registers the adapter for every mod, under the mod's own name, whether or not the mod ships a config spec. The mod's config spec uses that name as its `modName`. This is the name the framework uses for the mod's log and config, which is not always its F4SE plugin name: FRIK's `modName` is `frik`, and its F4SE plugin name, `F4VRBody`, goes in `f4sePluginRequirements`.

A CommonFramework mod that has a config UI of its own adds it to the header's C++ client, which the framework connects: it is a function and a quick launch entry, with the menu minimized and restored around it; see [The mod's own UI](#the-mods-own-ui).

The framework side of this is listed in [CommonFramework work required](framework-requirements.md).

## Final notifications

Successful Save emits one notification through each supported ecosystem:

- An F4SE plugin message for native listeners. The host, F4SE plugin `ModsConfigFO4VR`, sends it to each F4SE plugin the config spec names in `f4sePluginRequirements`, after the client's batch. Its type is the client header's `MODS_CONFIG_FO4_SAVED_MESSAGE`, and its data is the config spec's `modName`, UTF-8 text that ends with a zero. It says that the mod's settings were saved, not what changed: a client's batch does.
- An F4SE external Papyrus event for script mods, named `OnModsConfigFO4Saved|<modName>`. A script registers for it with F4SE's `RegisterForExternalEvent`, and its callback receives the `modName`.

Raw F4SE plugin messaging does not reach Papyrus by itself, so these are separate transports carrying the same semantic event.

A Save that wrote nothing sends neither. One that wrote its files and failed on a global or a property that left the game sends both, as its files changed.

A native listener registers for the sender `ModsConfigFO4VR`, the client header's `MODS_CONFIG_FO4_F4SE_NAME`. F4SE accepts that registration only once the sender is loaded, so a client registers at `kPostLoad` or later.

Papyrus scripts cannot read INI, JSON, or TOML files on their own, so the Papyrus event tells a script that something changed but not what. The script reads the values through the [Papyrus API](#papyrus-api).

## Papyrus API

A script-only mod, a game plugin and scripts with no DLL, ships a config spec like any other mod, and its scripts read the saved values through functions that Mods Config FO4 VR provides. It needs neither MCM nor a DLL of its own.

The functions are for a setting kept in a file. A script-only mod can instead keep a setting in a global or a property of its script, as an MCM mod does, which it reads itself; see [Global and script property](schema.md#global-and-script-property).

```papyrus
Scriptname ModsConfigFO4 Native Hidden

bool Function GetModSettingBool(string asModName, string asSetting) native global
int Function GetModSettingInt(string asModName, string asSetting) native global
float Function GetModSettingFloat(string asModName, string asSetting) native global
string Function GetModSettingString(string asModName, string asSetting) native global
```

`asModName` is the config spec's `modName` and `asSetting` is the setting's `id`. The functions are named as MCM's `MCM.GetModSetting*` are, and a setting with no `id` of its own is named as MCM names a ModSetting, as `fPlayerHeight:MyMod`. So a script that moves from MCM to a config spec, and keeps its mod name, sections and keys, changes `MCM.` to `ModsConfigFO4.` in its getter calls and nothing else in them. Both are matched without regard to case, because Papyrus does not preserve the case of a string.

The functions are global, so they need a compiled script and no game plugin. Mods Config FO4 VR ships `Data/Scripts/ModsConfigFO4.pex`, with its source in `Data/Scripts/Source/User/` for authors to compile against, and the DLL registers the functions when it loads. MCM provides its `MCM.GetModSetting*` functions the same way.

The API covers Mods Config FO4 VR config specs. A mod with an MCM menu keeps using MCM's functions.

A mod that reads its settings this way requires Mods Config FO4 VR, as a mod with an MCM menu requires MCM.

### What a getter returns

A getter answers from the mod's file. The cases are the ones the menu handles at [session start](#values-at-session-start):

- **Key present and valid.** The value in the file.
- **Key or file missing.** The declared default.
- **Value invalid.** The declared default, with a diagnostic.
- **Unknown config spec or setting, or the wrong getter for the setting's `valueType`.** `false`, `0`, `0.0`, or an empty string, with a diagnostic. Types are not converted.

A choice is read with the getter for its `valueType`. Papyrus `int` and `float` are the same 32-bit types the schema uses. How a color is returned is an open question.

A getter never returns a pending value. A script mod therefore has no live preview: it sees a change after Save.

For a setting kept in a file, the file remains the only store. The host may keep parsed files in memory to answer quickly; how soon it notices a file that was changed outside the menu is an open question.

Getters have to work from the first script that runs, so config specs are loaded when the game's data is ready, before a save is loaded. They cannot wait for the menu to be opened.

### When a script reads its settings

After each successful Save, the host sends the config spec's Papyrus event, described under [Final notifications](#final-notifications). The files are written before the event is sent, so a getter called from the handler returns the saved value.

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

Settings live in files and are the same for every save game, while a script's variables are stored in the save. A script therefore reads its settings again after a save is loaded, not only when it starts and on the save event.

### The settings file of a script-only mod

A script-only mod has no code that creates its settings file. The host creates it on the first Save that changes one of its settings, and the file holds only the settings the player has changed. For every other setting a getter returns the declared default, so the config spec is where a script-only mod's defaults live.

## Buttons and actions

A button is not a setting. It has no storage, default, dirty state, or persisted value.

A button's action is written as in an MCM config, and a config spec can use every action the menu supports for MCM, such as calling a function of a script. A script-only mod therefore has buttons in its config spec, as it has in MCM. A button can also run a console command, which needs no script.

A setting can have an action too, which Save runs after it writes the setting's value; see [A setting's action](schema.md#a-settings-action).

A config spec has one action of its own, which runs a function that the config spec's native client registered, by its name, with typed parameters; see [Functions](#functions). In a config spec with no registered native client, such a button is shown disabled with an explanation. A button that opens the mod's own UI, as FRIK's calibration, Immersive Flashlight's or Virtual Holsters' editors in the world, is such a function too; see [The mod's own UI](#the-mods-own-ui).

A function's result is success or failure, with an optional localized message. Confirmation metadata is declarative so buttons share one consistent VR dialog.

An action shows no progress and cannot be cancelled: a function of a script is queued to Papyrus, which gives no result, and a function of the mod's DLL returns its result at once.

## MCM compatibility

Mods Config FO4 VR does not replace MCM. It bridges to it: MCM stays installed and keeps ownership of its settings, its Papyrus functions, and its hotkeys, and Mods Config FO4 VR shows the same menus in VR.

- Mods Config FO4 VR and the config specs written for it do not require MCM.
- MCM-backed mods appear only when MCM is installed. A mod with an MCM menu already requires MCM, because its scripts call MCM's Papyrus functions.
- A mod that also has a config spec is listed by its config spec alone, and its MCM menu is left out; see [Required metadata](schema.md#required-metadata).
- Without MCM, MCM menus are not shown, and a diagnostic says why.

Replacing MCM would mean reimplementing its Papyrus functions, its setting store, and its hotkey dispatch, and asking players to uninstall it. Bridging is less work and cannot change how existing mods behave.

MCM pages remain a compatibility source: their values are read and written as MCM does, and only when they are written is the menu's. The bridge:

- Reads each mod's MCM `config.json` and its declared default settings.
- Writes ModSettings through MCM's own `MCM.SetModSetting*` functions, so MCM's in-memory store and its settings file stay in agreement. It never writes MCM's settings files itself.
- Reads and writes GlobalValue and PropertyValue sources directly, as MCM does.
- Sends the existing `OnMCMSettingChange|ModName` external event as a change is made and as Discard sets it back, and the menu open and close events when the menu opens and closes and when an MCM mod is shown and left, so scripts react as they do to MCM's own menu.
- Runs the supported MCM button actions. A script reads the values the player sees, as each change is made already, so a press does not ask to save first, as a config spec's script button does; see [Buttons](schema.md#buttons).
- Shows hotkeys read-only. MCM keeps dispatching them.

How it handles each MCM feature is in [MCM support](mcm.md).

`MCM.RefreshMenu()` stays MCM's own function and refreshes MCM's own menu. The bridge cannot observe the call, so it reads MCM's settings file again as another page of the mod is shown.

An MCM change is made as the player makes it, as MCM's menu makes it, so the player sees it in the game, and a script gets what MCM's menu gives it, in MCM's order. It is still a change of the session, with the Save bar and the unsaved-changes dialog: Save keeps it, and Discard sets it back. So every mod has one lifecycle. MCM mods are not marked or grouped apart in the UI.

- A change writes the value, a global and a property into the game and a ModSetting through `MCM.SetModSetting*`, runs the row's action with the value and sends `OnMCMSettingChange`. A global or a property that is no longer in the game is not written, and its row is not available, with the reason.
- A setting's baseline is its value as its session starts, and after a Save the value it kept. A setting the player changed is dirty while its value is not its baseline. Only the player's changes are dirty: a row that a script sets, as a preset sets its rows, is not, and keeps what the script set. A clean value is still read in every frame, so a change a script makes shows. A setting with no storage is held by the store and is never dirty.
- Discard sets the dirty settings back to their baselines, in the reverse of the order the player last changed each, as a change is made: it writes each, runs its row's action with the value and sends its event. So a preset that is set back runs its script with the preset it had, which sets its rows back. A row's own Discard sets that setting back alone, the same way. Discard is what the player would do by hand, as MCM's menu has no undo: what a script did that the value set back does not undo stays, as the rows of a preset whose old value is one its script applies nothing for.
- Save writes nothing: the values it keeps become the baseline. Then, where the session wrote a ModSetting since the last Save, it tells the game's listeners that the pause menu opened and closed, so a DLL that reads MCM's file as the pause menu closes reads it. Those events go to every listener of the game's menus, so such a DLL gets the changes at Save, and not as each is made. MCM writes its file as Papyrus runs the queued call that sets the ModSetting, so the events wait, from frame to frame, until MCM's file has every ModSetting the session wrote, and are sent after a few seconds anyway, as for a value MCM had already and does not write again. They wait also after the menu is closed.
- A ModSetting's write is queued to Papyrus and gives no result, so a change of one does not fail once it is written.

When the mod acts on a change is the mod's, and an MCM config has no field for it: a script reads a global when it needs it, and a DLL can read MCM's settings file only when the game starts. So an MCM setting's `applies` is `unknown`. A mod that needs a restart says so in its own text, which the menu shows.

## Raw custom UI

Ordinary integrations use declarative controls or the restricted API. Exposing the host's raw ImGui context across plugin boundaries is not planned for the initial architecture because it couples clients to context ownership, ImGui versions, input, styling, and crash behavior.

If hosted custom UI is added later, it should use a host-owned drawing interface with an explicit ABI. A mod's own editor is opened instead as [The mod's own UI](#the-mods-own-ui) is.
