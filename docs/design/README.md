# Mods Config VR design

Design documents for **Mods Config VR**, repository `F4VR-Mods-Config` and technical identifier `ModsConfigVR`.

These documents capture the current product and architecture decisions. The order the mod is built in, and what the first release holds, is in the [Mod work plan](mod-plan.md). Items that still need design work are collected in [Open questions](open-questions.md).

## Documents

- [Architecture and lifecycle](architecture.md) — goals, integration model, persistence, Save semantics, APIs, and MCM compatibility.
- [The settings model](model.md) — what the UI draws and the store it asks for every value, checked against an MCM global, a value of a package, and an activation sphere.
- [Declarative schema](schema.md) — package discovery, files, pages, settings, storage, conditions, buttons, localization, and validation.
- [Controls](controls.md) — core controls, their value semantics, and specialized CommonFramework controls reserved for later design.
- [UI and VR interaction](ui-and-vr-interaction.md) — opening the menu; full, focused, and minimized modes; world placement; pointing; editing; and low-fidelity wireframes.
- [CommonFramework work required](framework-requirements.md) — what F4VR-CommonFramework has to gain before this mod can be built on it. This work comes first.
- [Framework work plan](framework-plan.md) — the commits that deliver that work, and their status.
- [Mod work plan](mod-plan.md) — the milestones that build the mod, from MCM support to the first release and after, with the commits of each.
- [Open questions](open-questions.md) — deliberately deferred design topics without assigning them to a release.

## Names

- Player-facing name: **Mods Config VR**
- Nexus title: **Mods Config VR**
- Repository: `F4VR-Mods-Config`
- Technical identifier: `ModsConfigVR`
- DLL: `ModsConfigVR.dll`
- F4SE plugin name: `ModsConfigVR`
- Papyrus script: `ModsConfigVR`
- C++ namespace: `mods_config`
- Data root: `Data/ModsConfigVR`

The F4SE plugin name, the Papyrus script name, and the data root are public: other mods address messages to the first, call functions on the second, and install packages under the third.

## Design principles

- Fallout 4 VR only. Flat Fallout 4 is not supported and not planned.
- VR-native pointing and interaction, without exposing a desktop-style fake mouse to the player.
- Mods own their settings and their real configuration files. Mods Config VR does not create a second settings store.
- A declarative JSON package should be sufficient for ordinary settings menus.
- A restricted C++ API adds live preview, functions that buttons run, and the handoff to a mod's own UI.
- A script-only mod needs only a package: its scripts read the saved values through Papyrus functions that Mods Config VR provides.
- Existing MCM menus retain their established storage and event behavior through a bridge to the installed MCM.
- The game keeps running while the menu is open, so a change that the mod acts on at once is seen as it is made.
- The architecture should leave room for advanced features without requiring them in the initial implementation.
