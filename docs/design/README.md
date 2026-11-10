# Mods Config FO4 VR design

The current product and architecture decisions. Build order and release contents: [Mod work plan](mod-plan.md). Topics still needing design: [Open questions](open-questions.md).

## Documents

- [Architecture and lifecycle](architecture.md): goals, integration, persistence, Save, the APIs, MCM compatibility.
- [The settings model](model.md): the definition the UI draws and the store it asks for every value.
- [Declarative schema](schema.md): the config spec format.
- [MCM support](mcm.md): every MCM and MCM VR feature and how the menu handles it.
- [Controls](controls.md): the controls, their value semantics, the CommonFramework controls.
- [UI and VR interaction](ui-and-vr-interaction.md): opening, the Full and Minimized states, placement, pointing, navigation, search, categories.
- [Design system](design-system/README.md): the look (colors, type, sizes, the ImGui style) and mocks of every component and panel; the source of the Design System artifact.
- [CommonFramework work required](framework-requirements.md) and [Framework work plan](framework-plan.md): what the framework gained for this mod, done.
- [Mod work plan](mod-plan.md): the milestones, with each phase's commits in [phase 1](mod-plan-phase-1.md) (MCM support, done), [phase 2](mod-plan-phase-2.md) (first release, done) and [phase 3](mod-plan-phase-3.md) (after the first release).
- [Open questions](open-questions.md): deferred design topics, not assigned to a release.

## In review

Designs written and published to the design system that Arthur has not reviewed yet. One leaves the list when it is reviewed, changed or dropped.

- [List](controls.md#list): an `a` key; the row shows the entries and count, the editor a line per entry with Add, Remove, and Up/Down when ordered. Design system: [List](design-system/project/components/List/README.md).
- [Hand pose](controls.md#hand-pose): an `h` key; a bar per finger in the row, an editor of each finger's joints and the palm, shown live on the hand through FRIK. Open: whether the menu's FRIK tag is over a mod's own, and the joint ranges. Design system: [HandPose](design-system/project/components/HandPose/README.md).
- [Search](ui-and-vr-interaction.md#search): the tree's header is the field; a query filters the tree and the shown page, which Back shows whole. Design system: [Search](design-system/project/components/Search/README.md), [FullPanelSearch](design-system/project/components/FullPanelSearch/README.md).

## Not in the design system yet

Done in the code, still to be drawn in the design system. A note leaves when the design system has it.

- Tokens: the usage of `space-6` and `red` in `tokens.json` (from which `src/Theme.h` is generated) still names the row's gutter, which is gone.

## Names

| What | Name |
| --- | --- |
| Player-facing and Nexus title | **Mods Config FO4 VR** |
| Short name: title bar, its own config spec, the showcases | **Mods Config FO4** |
| Repository | `F4VR-Mods-Config` |
| Technical identifier (CMake `NAME`), DLL, F4SE plugin name | `ModsConfigFO4VR`, `ModsConfigFO4VR.dll` |
| Config file | `My Games\Fallout4VR\Mods_Config\ModsConfigFO4VR\ModsConfigFO4VR.toml` |
| Code names (the mod's class, the client header and its types), Papyrus script | `ModsConfigFO4` |
| C++ namespace | `mods_config` |
| Data root | `Data/ModsConfigFO4` |

Public: other mods address messages to the F4SE plugin name, call functions on the Papyrus script, and install config specs under the data root.

## Design principles

- Fallout 4 VR only; flat Fallout 4 is not supported or planned.
- VR-native pointing, no fake desktop mouse.
- Mods own their settings and real config files; the menu keeps no second settings store.
- A declarative JSON config spec suffices for ordinary settings menus.
- A restricted C++ API adds live preview, functions buttons run, states conditions ask, and opening a mod's own UI.
- A script-only mod needs only a config spec: its scripts read saved values through the menu's Papyrus functions, or keep them in globals and properties as an MCM mod does.
- Existing MCM menus keep their storage and event behavior through a bridge to the installed MCM.
- The game keeps running while the menu is open: a `live` change is seen as made, a saved one as the mod acts on it.
- Leave room for advanced features without requiring them up front.
