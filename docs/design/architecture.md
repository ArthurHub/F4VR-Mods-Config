# Architecture and lifecycle

## Purpose

Mods Config VR is a shared, VR-first configuration menu for Fallout 4 VR mods. It improves the current MCM experience while preserving compatibility with existing MCM packages.

The host renders through Dear ImGui using F4VR-CommonFramework. It owns navigation, interaction, persistence coordination, diagnostics, and the shared visual language. Each mod continues to own the meaning and runtime application of its settings.

## Integration sources

Three sources feed one internal model:

1. Mods Config VR declarative JSON packages.
2. A restricted native C++ API for dynamic behavior.
3. An adapter for existing Fallout 4 MCM packages.

The source is retained as metadata so the UI and lifecycle can preserve important differences, particularly MCM's immediate-change behavior.

```text
Declarative JSON ─────┐
Restricted C++ API ───┼──> Unified settings model ──> VR UI
MCM compatibility ────┘
```

## Ownership

- Mods Config VR owns menu discovery, rendering, input, validation orchestration, and safe persistence.
- A package definition describes the UI and binds controls to the mod's real settings files.
- A mod owns runtime interpretation and side effects.
- F4VR-CommonFramework is embedded separately in each client mod. Its config manager can provide a generic adapter that updates that mod's in-memory config and emits the mod's normal settings-change notifications.
- Per-setting callbacks are already scoped to a registered package/client and therefore do not carry a `modId`.

## Package and runtime identity

Every declarative package has a stable package `id`. The host uses it for discovery, diagnostics, and matching an optional native client registration. The package ID is not repeated in every scoped setting event.

A package remains usable without a native client. In that case it can read, validate, and save supported file-backed settings, but it cannot provide live runtime preview unless the mod independently watches or reloads its file.

## Persistence model

Mods Config VR writes directly to each mod's declared configuration files. There is no host-owned mirror of every mod's settings.

Supported storage formats are:

- INI
- JSON
- TOML

TOML comments and exact formatting are best-effort initially. This limitation is documented for package authors and logged in developer diagnostics; it is not shown as a routine player warning.

Target paths are explicit and have no inferred default. Packages may declare multiple files. Supported roots initially include the game `Data` directory and Fallout 4 VR's `My Games` directory. Absolute paths, parent traversal, and escapes through links or reparse points are rejected.

Writes use temporary files and replacement rather than editing originals in place.

## External changes

The host does not need a watcher or background conflict detector.

Immediately before Save, it reopens the latest version of every affected file and applies only settings that are dirty in the current editing session:

- An external change to an untouched setting is preserved.
- If both the external process and the player changed the same setting, the player's pending value wins.
- If the latest file cannot be parsed safely, Save fails instead of replacing it.

## Editing session

Only one mod has an active editing session.

1. Selecting a mod captures its saved values as the session baseline.
2. Player edits update a pending model.
3. Valid edits may be sent to an optional native preview adapter.
4. Focused and minimized modes retain the same session.
5. Save establishes a new baseline.
6. Discard restores the saved baseline and rolls back live previews.

Opening a different mod while the current mod is dirty prompts **Save / Discard / Cancel**. Closing the menu follows the same rule.

Reset-to-default is an ordinary pending edit. It is not persisted until Save.

## Save lifecycle

The player-facing operation is named **Save**, not Apply. A value may already be visible through live preview; Save means accept and persist the pending values.

Save proceeds as follows:

1. Validate all pending values.
2. Run optional native setting and batch validation.
3. Reopen the latest affected files.
4. Apply dirty settings only.
5. Serialize every affected file to a temporary file.
6. Parse the temporary output to verify it.
7. Replace originals, retaining temporary backups for rollback.
8. If a replacement fails, restore any files already replaced.
9. Call the scoped native `OnSettingsSaved` callback with one batch.
10. Broadcast one final native notification and one equivalent Papyrus notification.

The final notification means:

> Mod `FRIK` has had its configuration successfully saved. Reload it if you care.

It does not contain every changed setting. Native clients that need exact changes use the richer scoped batch API.

## Live preview API

Declarative-only integrations do not receive live preview. A registered native client receives a complete typed setting view, allowing a generic config adapter to update its in-memory representation without parsing `config.json`.

Conceptual types:

```cpp
enum class ChangePhase
{
    Preview,
    Discard
};

struct SettingChange
{
    const SettingView& setting;
    SettingValue previousValue;
    SettingValue value;
    ChangePhase phase;
};

struct SaveBatch
{
    std::span<const SettingChange> changes;
    std::span<const FileView> writtenFiles;
    std::uint64_t revision;
};
```

`SettingView` exposes the stable setting ID, value type, default, constraints, and resolved storage target. It does not require the client to parse the declarative package.

CommonFramework can register a generic adapter that:

1. Finds the appropriate registered in-memory config from the resolved target.
2. Type-checks and updates the entry.
3. Invokes the config manager's normal settings-updated notification.

Special gameplay side effects can use an optional binding API later.

Preview events occur only when a valid value changes. Slider previews are coalesced to at most the useful render/update rate; they are not generated merely because a control is focused.

## Final notifications

Successful Save emits one notification through each supported ecosystem:

- An F4SE plugin message for native listeners.
- An F4SE external Papyrus event for script mods.

Raw F4SE plugin messaging does not reach Papyrus by itself, so these are separate transports carrying the same semantic event.

The wire ABI must be versioned and C-compatible. It must not pass `std::string`, `std::variant`, or other C++ library objects across DLL boundaries. A small client header can hide discovery, registration timing, filtering, and decoding.

## Named actions

An action is not a setting. It has no storage binding, default, dirty state, or persisted value.

A declarative action invokes only a name registered by the same scoped native client. New-schema JSON cannot invoke arbitrary C++ functions, Papyrus functions, or console commands.

Action invocations receive typed JSON parameters and read-only access to current pending settings. Basic results are success or failure plus an optional localized message. Confirmation metadata is declarative so actions share one consistent VR dialog.

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

MCM pages remain a compatibility source rather than being rewritten into the new persistence lifecycle.

The adapter preserves:

- PropertyValue sources.
- GlobalValue sources.
- ModSettings and their established files.
- Existing `OnMCMSettingChange|ModName` external events.
- Menu open/close events.
- `MCM.RefreshMenu()` behavior.
- Supported MCM button actions and hotkeys.

MCM settings change immediately as existing mods expect. They do not acquire staged Save/Discard behavior. The UI shows a subtle **MCM** badge and an “Changes apply immediately” status instead of the normal Save bar.

## Raw custom UI

Ordinary integrations use declarative controls or the restricted API. Exposing the host's raw ImGui context across plugin boundaries is not planned for the initial architecture because it couples clients to context ownership, ImGui versions, input, styling, and crash behavior.

If hosted custom UI is added later, it should use a host-owned drawing interface with an explicit ABI. Full bespoke editors can instead use the external UI handoff lifecycle.
