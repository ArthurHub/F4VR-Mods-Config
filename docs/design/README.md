# Mods Config FO4 VR design

The product and architecture as they stand. What is left to build and the open questions: [Work plan](mod-plan.md).

## Documents

- [Architecture and lifecycle](architecture.md): integration, persistence, the editing session and Save, VR interaction (opening, placement, pointing, Back, the help line, navigation, search, categories), the native and Papyrus APIs, MCM compatibility.
- [The settings model](model.md): a setting's parts and the CommonFramework types, the definition the UI draws, and the store it asks for every value.
- [Declarative schema](schema.md): the config spec format, dynamic pages included.
- [MCM support](mcm.md): every MCM and MCM VR feature and how the menu handles it, and the MCM test set.
- [Design system](design-system/README.md): the look and behavior of every control, component and panel, with mocks (colors, type, sizes, the ImGui style); the source of the Design System artifact and of `src/Theme.h`.
- [Work plan](mod-plan.md): the remaining milestones and the open questions.
- [Flat Fallout 4](flat-fallout4.md): the plan for a second build of the same source, for flat Fallout 4.

The panel's generic controls are the framework's [`imgui::controls`](../../external/F4VR-CommonFramework/src/imgui/controls/README.md); the framework's subsystems are documented in its own READMEs.

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

- Fallout 4 VR first; flat Fallout 4 is planned as a second build of the same source ([Flat Fallout 4](flat-fallout4.md)).
- VR-native pointing, no fake desktop mouse.
- Mods own their settings and real config files; the menu keeps no second settings store.
- A declarative JSON config spec suffices for ordinary settings menus.
- A restricted C++ API adds live preview, functions buttons run, states conditions ask, and opening a mod's own UI.
- A script-only mod needs only a config spec: its scripts read saved values through the menu's Papyrus functions, or keep them in globals and properties as an MCM mod does.
- Existing MCM menus keep their storage and event behavior through a bridge to the installed MCM.
- The game keeps running while the menu is open: a `live` change is seen as made, a saved one as the mod acts on it.
