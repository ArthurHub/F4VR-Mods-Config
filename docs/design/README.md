# Mods Config VR design

Design documents for **Mods Config VR**, repository `F4VR-Mods-Config` and technical identifier `ModsConfigVR`.

These documents capture the current product and architecture decisions. The order the mod is built in, and what the first release holds, is in the [Mod work plan](mod-plan.md). Items that still need design work are collected in [Open questions](open-questions.md).

## Documents

- [Architecture and lifecycle](architecture.md) — goals, integration model, persistence, Save semantics, APIs, and MCM compatibility.
- [The settings model](model.md) — what the UI draws and the store it asks for every value, checked against an MCM global, a value of a package, and an activation sphere.
- [Declarative schema](schema.md) — package discovery, files, pages, settings, storage, conditions, buttons, localization, validation, and what a package has beyond MCM.
- [MCM support](mcm.md) — every feature of an MCM config and of MCM VR, and how the menu handles each.
- [Controls](controls.md) — core controls, their value semantics, and specialized CommonFramework controls reserved for later design.
- [UI and VR interaction](ui-and-vr-interaction.md) — opening the menu; full, focused, and minimized modes; world placement; pointing; editing; and low-fidelity wireframes.
- [Design system](design-system/README.md) — the look: colors, type, sizes, the Dear ImGui style, and mocks of every component and panel. It is the source of the Design System artifact.
- [CommonFramework work required](framework-requirements.md) — what F4VR-CommonFramework has to gain before this mod can be built on it. This work comes first.
- [Framework work plan](framework-plan.md) — the commits that deliver that work, and their status.
- [Mod work plan](mod-plan.md) — the milestones that build the mod, from MCM support to the first release and after, in three phases. The commits of each milestone are in the file of its phase:
  - [Phase 1: MCM support](mod-plan-phase-1.md) — milestones 1 to 4, done.
  - [Phase 2: First release](mod-plan-phase-2.md) — milestones 5 to 11.
  - [Phase 3: After the first release](mod-plan-phase-3.md) — milestones 12 to 15.
- [Open questions](open-questions.md) — deliberately deferred design topics without assigning them to a release.

## In review

Designs that are written and published to the design system but that Arthur has not reviewed yet. A design leaves this list when it is reviewed, changed or dropped.

- [List](controls.md#list) — a list of texts, an `a` key: the row shows the entries and their count, and an editor has a line for each entry, with Add, Remove, and Up and Down where the order means something. Design system: [List](design-system/project/components/List/README.md).
- [Hand pose](controls.md#hand-pose) — an `h` key: a bar for each finger in the row, and an editor of each finger's joints and the palm, shown live on the player's hand through FRIK. Open: whether the menu's FRIK tag is over a mod's own, and the ranges of the joints. Design system: [HandPose](design-system/project/components/HandPose/README.md).
- [Search](ui-and-vr-interaction.md#search) — the tree's header is the field, a query filters the tree, and the settings that match are a page of their own, with a step of Back for it. Design system: [Search](design-system/project/components/Search/README.md) and [FullPanelSearch](design-system/project/components/FullPanelSearch/README.md).
- [Warnings](schema.md#warnings) — a hollow `amber` triangle after the row's name while a warning holds, apart from the invalid mark in the gutter; its text first in the help line. Design system: [RowWarning](design-system/project/components/RowWarning/README.md).
- [Read-only](controls.md#read-only) — the row disabled with its value in its control, no Reset, and no value drawn while its storage has none. Design system: [ReadOnly](design-system/project/components/ReadOnly/README.md).
- [Number display](schema.md#display) — decimals from `display` or the step, a unit with or without a space, a percentage of the range, and a value too wide for its box drawn smaller, then without its unit. Design system: [NumberDisplay](design-system/project/components/NumberDisplay/README.md).
- [Buttons](controls.md#button) — the button in the control column, `Running` while its action runs, its result under the name, and the confirmation, red when `dangerous`. Open: cancelling an action that runs. Design system: [ButtonRow](design-system/project/components/ButtonRow/README.md).
- [The save summary](controls.md#help-and-validation-feedback) — a popup above Save that names every invalid setting, where it is and why it does not show, each with Reset, while the page focuses the first that shows. Design system: [SaveSummary](design-system/project/components/SaveSummary/README.md).
- [Restart after Save](schema.md#when-a-change-takes-effect) — the help says when a change takes effect, the status after the Save names the restart, and the menu's front page keeps a notice until the game is restarted. Design system: [RestartNotice](design-system/project/components/RestartNotice/README.md).
- [Text input](controls.md#text-input) — a field with the text and a pencil; while typed on SteamVR's keyboard, a caret and the count of characters; a typed number right-aligned. Design system: [TextInput](design-system/project/components/TextInput/README.md).
- [Text, spacers and images](schema.md#text-spacers-and-images) — a muted paragraph with MCM's tags, a spacer of one row or more, an image at its size, and a box naming a texture that is missing. Design system: [TextRows](design-system/project/components/TextRows/README.md).
- [Choice of files](controls.md#choice-of-files) — a dropdown of a folder's files, with a texture's thumbnail, written in the form the value had. Design system: [FileChoice](design-system/project/components/FileChoice/README.md).
- [Transform](controls.md#transform) — position and rotation on two lines in the row; an editor of steppers with a step size, and In the world. Open: tuning another mod's key in the world is framework work. Design system: [Transform](design-system/project/components/Transform/README.md).
- [ActivationSphere](controls.md#activationsphere) — a section drawn as one group with the sphere's mark: zones, actions, and its look folded; the sphere lit in the world while the ray is on it. Open: the client needs a group event to light it. Design system: [ActivationSphere](design-system/project/components/ActivationSphere/README.md).
- Not designed yet, beside these: Immersive Flashlight's flags value, `sFlashlightFlagsBitmask`, which needs the package to name each flag.

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
- A script-only mod needs only a package: its scripts read the saved values through Papyrus functions that Mods Config VR provides, or keep them in globals and properties, as an MCM mod does.
- Existing MCM menus retain their established storage and event behavior through a bridge to the installed MCM.
- The game keeps running while the menu is open, so a change that the mod acts on at once is seen as it is made.
- The architecture should leave room for advanced features without requiring them in the initial implementation.
