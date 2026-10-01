# Open Questions and Deferred Design

This file records unresolved work without implying that an item is in or out of v1. Release scope will be decided separately.

## CommonFramework-specific controls

Design the exact schema, storage bindings, widgets, preview behavior, and validation for:

- `ActivationSphere`
- `InputBinding`
- `Haptic`
- `Transform`

These likely need VR manipulation experiences in addition to ordinary form controls. In particular, determine whether editing occurs inside the panel, in the world, or through a controlled handoff between both.

## Schema evolution

- Exact schema versioning and migration rules
- Compatibility promises for unknown fields and newer minor versions
- Package dependency and optional-capability syntax
- Reusable fragments without making package resolution difficult to understand
- Dynamic pages, options, visibility, and values supplied by native code
- Whether and how declarative packages can extend an existing package owned by another component

## Storage edge cases

- Missing target files and whether creation must be explicitly permitted
- Duplicate INI sections or keys and which occurrence is authoritative
- JSON/TOML representation choices for colors and complex values
- Format-preserving TOML editing, including comments and layout
- What to do when an external edit changes a value currently being previewed but not yet saved
- File-change detection and optional refresh while the menu is open
- Backup retention and recovery after an interrupted multi-file Save

The settled baseline remains: on Save, re-read each affected file, write only dirty keys, preserve external changes to untouched keys, and let the player's pending value win for a key they changed.

## Localization

- Exact locale lookup and regional fallback details
- Formatting placeholders, pluralization, and runtime values
- Localizing messages supplied by native actions
- Whether asset paths can vary by locale
- Validation tooling for missing or unused keys

Translation file naming is settled as `config.<language>.json`, for example `config.fr.json`.

## MCM compatibility

- Inventory the complete Fallout 4 MCM schema and control behavior
- Confirm default-value availability and reset semantics
- Confirm which files and paths MCM currently permits
- Map MCM immediate-change, event, and Papyrus behavior precisely
- Decide how unsupported or custom MCM constructs appear
- Decide whether MCM entries are grouped separately, badged individually, or both

Compatibility should preserve how existing MCM mods work rather than silently changing their application lifecycle.

## Native API details

- Final C ABI types, ownership rules, string encoding, and API-version negotiation
- Threading and callback restrictions
- Preview coalescing timing
- Save-batch and error-reporting structures
- Native and Papyrus final-save notification payloads
- Named-action parameter types and asynchronous completion
- Registration and lifecycle for bespoke external UIs
- Timeout, crash, or abandonment recovery during external UI handoff
- Whether a safe host-owned custom-panel extension is needed later

Raw ImGui ownership by client mods is deliberately deferred because it weakens input, layout, lifecycle, and compatibility guarantees.

## UI work

- Headset-scale mockups and legibility testing
- Compare world-locked placement with Fallout 4 VR's projected Pip-Boy
- Comfortable initial distance, height, curvature, transparency, and border weight
- Exact contracted-navigation width and no-selection layout
- Final description placement after in-headset testing
- Search and filtering
- Direct-touch interaction
- Color picker design
- On-screen keyboard layout and locale handling
- Image/preview assets
- Progress, cancellation, and error handling for long-running actions
- Accessibility options beyond the three main UI sizes

Free panel repositioning is not currently planned. A simple recovery/recenter operation is distinct from a repositioning system.

## Profiles, backups, and portability

- Back up and restore all mod configurations
- Back up and restore one selected mod
- Detect and explain files that cannot be safely restored
- Global profiles versus per-mod profiles
- Portability across machines when paths include the user's My Games directory
- Conflict policy when restoring over newer external changes

Profiles are considered advanced; a global model is preferred initially if they are pursued.

## Product and release decisions

- Decide v1 scope only after the architecture, schema, controls, UI, and compatibility investigation are mature enough to estimate
- Final Nexus display name between `Mods Config VR` and `Mods Config VR - Fallout 4 VR`
- Exact terminology for declarative/native-capable packages; avoid the ambiguous label `Native`
- Packaging, installation, diagnostics, author documentation, and sample mods
- Whether flat Fallout 4 support is ever desirable; it is not an active goal
