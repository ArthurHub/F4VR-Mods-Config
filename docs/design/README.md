# Mods Config FO4 VR design

Design documents for **Mods Config FO4 VR**, repository `F4VR-Mods-Config` and technical identifier `ModsConfigFO4VR`.

These documents capture the current product and architecture decisions. The order the mod is built in, and what the first release holds, is in the [Mod work plan](mod-plan.md). Items that still need design work are collected in [Open questions](open-questions.md).

## Documents

- [Architecture and lifecycle](architecture.md) — goals, integration model, persistence, Save semantics, APIs, and MCM compatibility.
- [The settings model](model.md) — what the UI draws and the store it asks for every value, checked against an MCM global, a value of a config spec, and an activation sphere.
- [Declarative schema](schema.md) — config spec discovery, files, pages, settings, storage, conditions, buttons, localization, validation, and what a config spec has beyond MCM.
- [MCM support](mcm.md) — every feature of an MCM config and of MCM VR, and how the menu handles each.
- [Controls](controls.md) — core controls, their value semantics, and specialized CommonFramework controls reserved for later design.
- [UI and VR interaction](ui-and-vr-interaction.md) — opening the menu; the full and minimized states; world placement; pointing; editing; and low-fidelity wireframes.
- [Design system](design-system/README.md) — the look: colors, type, sizes, the Dear ImGui style, and mocks of every component and panel. It is the source of the Design System artifact.
- [CommonFramework work required](framework-requirements.md) — what F4VR-CommonFramework has to gain before this mod can be built on it. This work comes first.
- [Framework work plan](framework-plan.md) — the commits that deliver that work, and their status.
- [Mod work plan](mod-plan.md) — the milestones that build the mod, from MCM support to the first release and after, in three phases. The commits of each milestone are in the file of its phase:
  - [Phase 1: MCM support](mod-plan-phase-1.md) — milestones 1 to 4, done.
  - [Phase 2: First release](mod-plan-phase-2.md) — milestones 5 to 10.
  - [Phase 3: After the first release](mod-plan-phase-3.md) — milestones 11 to 16.
- [Open questions](open-questions.md) — deliberately deferred design topics without assigning them to a release.

## In review

Designs that are written and published to the design system but that Arthur has not reviewed yet. A design leaves this list when it is reviewed, changed or dropped.

- [List](controls.md#list) — a list of texts, an `a` key: the row shows the entries and their count, and an editor has a line for each entry, with Add, Remove, and Up and Down where the order means something. Design system: [List](design-system/project/components/List/README.md).
- [Hand pose](controls.md#hand-pose) — an `h` key: a bar for each finger in the row, and an editor of each finger's joints and the palm, shown live on the player's hand through FRIK. Open: whether the menu's FRIK tag is over a mod's own, and the ranges of the joints. Design system: [HandPose](design-system/project/components/HandPose/README.md).
- [Search](ui-and-vr-interaction.md#search) — the tree's header is the field, a query filters the tree, and the settings that match are a page of their own, with a step of Back for it. Design system: [Search](design-system/project/components/Search/README.md) and [FullPanelSearch](design-system/project/components/FullPanelSearch/README.md).

## Not in the design system yet

Done in the code, and still to be drawn in the design system. A note leaves this list when the design system has it.

- Tokens: the usage of `space-6` and of `red` in `tokens.json`, which `src/Theme.h` is generated from, still names the row's gutter, which is gone.

## Names

- Player-facing name: **Mods Config FO4 VR**
- Nexus title: **Mods Config FO4 VR**
- Repository: `F4VR-Mods-Config`
- Short name, in the menu's title bar, its own config spec and the showcases: **Mods Config FO4**
- Technical identifier, CMake's `NAME`: `ModsConfigFO4VR`
- DLL: `ModsConfigFO4VR.dll`
- F4SE plugin name: `ModsConfigFO4VR`
- Config file: `My Games\Fallout4VR\Mods_Config\ModsConfigFO4VR\ModsConfigFO4VR.toml`
- Code names (the mod's class, the client header and its types): `ModsConfigFO4`
- Papyrus script: `ModsConfigFO4`
- C++ namespace: `mods_config`
- Data root: `Data/ModsConfigFO4`

The F4SE plugin name, the Papyrus script name, and the data root are public: other mods address messages to the first, call functions on the second, and install config specs under the third.

## Design principles

- Fallout 4 VR only. Flat Fallout 4 is not supported and not planned.
- VR-native pointing and interaction, without exposing a desktop-style fake mouse to the player.
- Mods own their settings and their real configuration files. Mods Config FO4 VR does not create a second settings store.
- A declarative JSON config spec should be sufficient for ordinary settings menus.
- A restricted C++ API adds live preview, functions that buttons run, states that conditions ask, and opening a mod's own UI.
- A script-only mod needs only a config spec: its scripts read the saved values through Papyrus functions that Mods Config FO4 VR provides, or keep them in globals and properties, as an MCM mod does.
- Existing MCM menus retain their established storage and event behavior through a bridge to the installed MCM.
- The game keeps running while the menu is open, so a `live` change is seen as it is made, and a saved one as the mod acts on it.
- The architecture should leave room for advanced features without requiring them in the initial implementation.
