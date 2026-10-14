# Open Questions and Deferred Design

This file records unresolved work. The order the mod is built in, and what the first release holds, is in the [Mod work plan](mod-plan.md).

Work that F4VR-CommonFramework needs is tracked in [CommonFramework work required](framework-requirements.md).

## CommonFramework-specific controls

Design the exact schema, storage, widgets, preview behavior, and validation for:

- `ActivationSphere`
- `InputBinding`
- `Haptic`
- `Transform`

These likely need VR manipulation experiences in addition to ordinary form controls. In particular, determine whether editing occurs inside the panel, in the world, or through a controlled handoff between both.

The framework already defines how each is stored in an INI, which is the starting point:

- A transform is one value: `x,y,z;heading,roll,attitude;scale`.
- An input binding is one line: `<hand> <type> <button> [duration] [suppress] [+modifier]`.
- A haptic is the name of a pattern, or `none`.
- A color is `r,g,b` or `r,g,b,a`, each from 0 to 255.
- An activation sphere is a whole INI section: a zone transform with an optional power-armor variant, two bindings, three haptics, and the keys for its icon and sphere.

A setting is stored in one key. An activation sphere spans a section, so it is a group of per-key settings with a role; see [The settings model](model.md#a-group-with-a-role). How a package names such a section is still to be designed.

## Schema evolution

- Package dependency and optional-capability syntax
- Reusable fragments without making package resolution difficult to understand
- Dynamic pages, options, visibility, and values supplied by native code
- A condition that a game plugin is loaded, as `{ "plugin": "Other.esp", "loaded": true }`, for a row that needs another mod: what an MCM config does with a `hiddenSwitcher` on a property its script sets. It is static, and the check of `pluginRequirements` answers it. A condition on a value of the game that changes, as MCM's `hiddenSwitcher` on a global, stays with the native API
- Whether and how declarative packages can extend an existing package owned by another component
- A button that runs a console command, as `{ "type": "RunConsoleCommand", "command": "..." }`: MCM runs one for a hotkey, but not for a row of its menu. It needs no script, so a mod with no plugin of its own has buttons too

## Storage edge cases

- Whether a package can forbid creating a missing file, for a mod that cannot load a file that holds only some of its settings
- Duplicate INI sections or keys and which occurrence is authoritative
- How values are spelled in each INI: `true`/`false` or `1`/`0` for a boolean, and the precision a float is written with
- JSON/TOML representation choices for colors and complex values
- Format-preserving TOML editing, including comments and layout
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

The form is settled: a package's text is plain, a `$` token as in MCM, or a token with a fallback, and the translations are the game's translation files. See [Localization](schema.md#localization).

## Text input

- How a VR on-screen keyboard works: its layout, where it appears relative to the panel, and how it is pointed at
- A numeric keypad for value entry
- Keyboard layout per locale
- How key presses reach a focused ImGui text field
- Whether a physical keyboard is supported

## MCM compatibility

The bridge model is settled. MCM stays installed and owns its settings, and Mods Config VR shows MCM menus only when MCM is present. MCM keeps user values in `Data/MCM/Settings/<ModName>.ini` and defaults in `Data/MCM/Config/<ModName>/settings.ini`; the bridge writes neither.

Still open:

- Inventory the complete Fallout 4 MCM schema and control behavior. DearModdingUI's MCM bridge for flat Fallout 4 has made this inventory and is the place to start
- Which MCM builds for Fallout 4 VR the bridge supports, and how it detects them
- Map MCM immediate-change, event, and Papyrus behavior precisely
- Whether MCM hotkeys, which are keyboard and gamepad codes, can be made useful in VR
- Whether any DLL reads MCM's settings file again when MCM's own menu closes. It would not see a change made here. No DLL of the test set shows it

Compatibility should preserve how existing MCM mods work rather than silently changing their application lifecycle.

## Native API details

- Final C ABI types, ownership rules, string encoding, and API-version negotiation
- Threading and callback restrictions
- What the host does when a client callback throws or crashes
- Preview coalescing timing
- Save-batch and error-reporting structures
- Native final-save notification payload
- How the host detects a loaded F4SE plugin by name. F4SEVR has no `GetPluginInfo`; registering a listener for a sender fails when that plugin is not loaded, which may serve
- The parameter types of a function that a button runs, and asynchronous completion
- Registration and lifecycle for bespoke external UIs
- Timeout, crash, or abandonment recovery during external UI handoff
- Whether a safe host-owned custom-panel extension is needed later

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
- Search and filtering
- Direct-touch interaction
- Color picker design
- Bold, italic and underline in a text row: `<b>`, `<i>` and `<u>` are dropped. The panel has one font face, so bold and italic each need another, and an underline is a drawn line
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
