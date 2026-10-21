# Open Questions and Deferred Design

This file records unresolved work. The order the mod is built in, and what the first release holds, is in the [Mod work plan](mod-plan.md).

Work that F4VR-CommonFramework needs is tracked in [CommonFramework work required](framework-requirements.md).

## CommonFramework-specific controls

Every control is designed: see [Haptic](controls.md#haptic), [Color](controls.md#color), [InputBinding](controls.md#inputbinding), [List](controls.md#list), [Hand pose](controls.md#hand-pose), [Transform](controls.md#transform) and [ActivationSphere](controls.md#activationsphere) in Controls. What is left of them:

- A transform is edited in the panel, and its In the world hands it to the framework's live tuning. The framework tunes only its own mod's keys today: tuning another mod's key, and handing the value back, is framework work.
- An activation sphere is lit in the world while the ray is on its rows. That needs the client to be told which group the ray is in, an event beside the page events.
- How a config spec names a section that is an activation sphere.

The framework already defines how each is stored in its TOML config, which is the starting point; see [Controls](controls.md#commonframework-specific-controls) for the full forms:

- A haptic is a string, the name of a pattern, or `"none"`.
- A color is an inline table, `{ rgb = [r, g, b], alpha = a }`, each from 0 to 255, either key optional.
- An input binding is an inline table, `{ hand = "offhand", type = "tap", button = "grip" }`, with `duration`, `suppress` and `modifier` optional, an axis form, and `{ type = "none" }`.
- A transform is an inline table, `{ pos = [x, y, z], rot = [heading, roll, attitude], scale = s }`, each key optional.
- An activation sphere is a whole section: a zone transform with an optional power-armor variant, two bindings, three haptics, and the keys for its icon and sphere.

A setting is stored in one key, and a table is one value of it: a string setting whose value is the TOML text of the table. An activation sphere spans a section, so it is a group of per-key settings with a role; see [The settings model](model.md#a-group-with-a-role). How a config spec names such a section is still to be designed.

Whether InputBinding warns of two bindings that collide, of one mod or of two: FRIK, Immersive Flashlight and this mod all use the thumbsticks.

## Storage edge cases

- Whether a config spec can forbid creating a missing file, for a mod that cannot load a file that holds only some of its settings
- JSON representation choices for colors and complex values. In a TOML file they are the framework's tables
- Whether the game's own INI files under `My Games/Fallout4VR` are valid targets
- What to do when an external edit changes a value currently being previewed but not yet saved
- File-change detection and optional refresh while the menu is open
- Reading a file while its owner is rewriting it; a truncated INI still parses
- Whether to keep a last-good copy for recovery after a crash during a write
- Whether reading files at session start and writing them on Save runs off the game thread

The settled baseline remains: on Save, re-read each affected file, write only dirty keys, preserve external changes to untouched keys, and let the player's pending value win for a key they changed. Files are verified in memory and written in place, with no temporary files or renames. A missing file is created by the first Save that changes one of its settings.

## Localization

- Font coverage: the UI font shows Western European languages only, so other languages need wider glyph coverage or fallback fonts
- Formatting placeholders, pluralization, and runtime values
- Localizing messages supplied by native functions
- Whether asset paths can vary by locale
- Tooling that finds missing or unused tokens

The form is settled: a config spec's text is plain, a `$` token as in MCM, or a token with a fallback, and the translations are the game's translation files. See [Localization](schema.md#localization).

## Native API details

- Final C ABI types, ownership rules, string encoding, and API-version negotiation
- Threading and callback restrictions
- Preview coalescing timing
- Save-batch and error-reporting structures
- Native final-save notification payload
- How the host detects a loaded F4SE plugin by name. F4SEVR has no `GetPluginInfo`; registering a listener for a sender fails when that plugin is not loaded, which may serve
- The parameter types of a function that a button runs, and asynchronous completion
- Registration and lifecycle for bespoke external UIs
- Timeout, crash, or abandonment recovery during external UI handoff
- Whether a safe host-owned custom-panel extension is needed later
- Pages, options, visibility, and values supplied by native code, as a row that is shown while something of the game holds, which an MCM config does with a `hiddenSwitcher` that its script sets

Raw ImGui ownership by client mods is deliberately deferred because it weakens input, layout, lifecycle, and compatibility guarantees.

## Papyrus API

The getters and the save event are settled; see [Papyrus API](architecture.md#papyrus-api). Still open:

- How a color is returned to a script, which waits on the color storage encoding
- Whether the getters are flagged to run without waiting (`NoWait`), which requires them to be thread-safe. MCM's are not flagged
- How soon a getter sees a file that was changed outside the menu
- Whether a script mod can treat Mods Config VR as optional. F4SE's `F4SE.GetPluginVersion` tells a script whether the plugin is loaded; what happens to a script that calls a missing `ModsConfigVR` script has to be checked in game
- Whether the host also tells scripts to read their settings again after a save is loaded
- Whether script mods get live preview: a preview event, and getters that return pending values
- Whether scripts can change settings

## UI work

- Headset-scale mockups and legibility testing
- Comfortable initial distance, height, curvature, transparency, and border weight
- Whether world geometry hides the Full panel or it always draws on top, and where the panel goes when a wall is closer than its distance
- The width of the navigation tree
- Whether the help line, at the bottom edge of the Full panel, is comfortable to read in the headset
- What happens to an open editing session on a save load, a loading screen, or the player's death
- Direct-touch interaction
- Image/preview assets
- Progress, cancellation, and error handling for long-running actions
- Accessibility options beyond the three main UI sizes

## Profiles, backups, and portability

- Back up and restore all mod configurations
- Back up and restore one selected mod
- Detect and explain files that cannot be safely restored
- Global profiles versus per-mod profiles
- Portability across machines when paths include the user's My Games directory
- Conflict policy when restoring over newer external changes

Profiles are considered advanced; a global model is preferred initially if they are pursued.

An author's presets are profiles too: a file the author ships beside the config spec, as `Presets/<name>.json`, in the same form as a player's profile. A profile maps setting ids to values, so it covers a config spec whose settings are in several files, globals and properties. Applying one sets pending values, which Save writes, and a setting it does not name stays as it is.
