# Mods Config VR design

Design documents for **Mods Config VR**, repository `F4VR-Mods-Config` and technical identifier `ModsConfigVR`.

These documents capture the current product and architecture decisions. They intentionally do not define a v1 scope yet. Items that still need design work are collected in [Open questions](open-questions.md).

## Documents

- [Architecture and lifecycle](architecture.md) — goals, integration model, persistence, Save semantics, APIs, and MCM compatibility.
- [Declarative schema](schema.md) — package discovery, files, pages, settings, bindings, conditions, actions, localization, and validation.
- [Controls](controls.md) — core controls, their value semantics, and specialized CommonFramework controls reserved for later design.
- [UI and VR interaction](ui-and-vr-interaction.md) — opening the menu; full, focused, and minimized modes; world placement; pointing; editing; and low-fidelity wireframes.
- [CommonFramework work required](framework-requirements.md) — what F4VR-CommonFramework has to gain before this mod can be built on it. This work comes first.
- [Open questions](open-questions.md) — deliberately deferred design topics without assigning them to a release.

## Names

- Player-facing name: **Mods Config VR**
- Nexus title: **Mods Config VR** or **Mods Config VR - Fallout 4 VR**
- Repository: `F4VR-Mods-Config`
- Technical identifier: `ModsConfigVR`
- DLL: `ModsConfigVR.dll`
- F4SE plugin name: `ModsConfigVR`
- Papyrus script: `ModsConfigVR`
- C++ namespace: `mods_config`
- Data root: `Data/ModsConfigVR`

The F4SE plugin name, the Papyrus script name, and the data root are public: other mods address messages to the first, call functions on the second, and install packages under the third.

## Design principles

- VR first; flat Fallout 4 support is not currently planned.
- VR-native pointing and interaction, without exposing a desktop-style fake mouse to the player.
- Mods own their settings and their real configuration files. Mods Config VR does not create a second settings store.
- A declarative JSON package should be sufficient for ordinary settings menus.
- A restricted C++ API adds live preview, dynamic behavior, validation, actions, and bespoke UI handoff.
- A script-only mod needs only a package: its scripts read the saved values through Papyrus functions that Mods Config VR provides.
- Existing MCM packages retain their established storage and event behavior through a bridge to the installed MCM.
- The game keeps running while the menu is open, so the effect of a change is seen as it is made.
- The architecture should leave room for advanced features without requiring them in the initial implementation.
