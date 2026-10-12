# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Mods Config VR (`ModsConfigVR`) is a **C++23 DLL plugin** for F4SE (Fallout 4 Script Extender) for Fallout 4 VR, built on the [F4VR Common Framework](external/F4VR-CommonFramework/README.md).

It is a shared in-game configuration menu for Fallout 4 VR mods. A mod ships a declarative JSON package or already has an MCM menu, and the player edits the mod's real settings in VR. A package is saved with an explicit Save, and a mod that registers through the native API gets live preview. The design is in [docs/design/](docs/design/README.md). So far the menu opens on its front page and lists the MCM mods that are installed. A mod that is selected shows its pages in the tree and their rows, each as its text and its type. The framework work the mod needed first is listed in [docs/design/framework-requirements.md](docs/design/framework-requirements.md), and its commits are tracked in [docs/design/framework-plan.md](docs/design/framework-plan.md). The mod's own milestones and commits are in [docs/design/mod-plan.md](docs/design/mod-plan.md).

## Build System

**Prerequisites:**

- `VCPKG_ROOT` environment variable pointing to a vcpkg installation
- Visual Studio 2022 or 2026 (MSVC toolchain, x64)
- CMake 4.2+
- Git submodules initialized: `git submodule update --init --recursive`

**Configure and build:**

```bash
cmake --preset default                                    # uses VS 2026; substitute vs2022 preset if needed
cmake --build --preset release                            # or: cmake --build build --config Release
cmake --build --preset debug                              # or: cmake --build build --config Debug
cmake --build --preset tracy                              # Release + the Tracy profiler client
```

The default preset inherits `cmake-dev + vcpkg + windows + vs2026`. The `release` / `debug` / `package` / `tracy` build presets build the `default` configure preset. The build output is `build/`. In-source builds are blocked.

**Auto-copy to game (optional):** `CMakeUserPresets.json` (git-ignored, start from `CMakeUserPresets.json.template`) defines a `custom` preset that sets `POST_BUILD_COPY_PLUGIN` and `COPY_PLUGIN_BASE_PATH` to copy the DLL and PDB to `<path>/F4SE/Plugins/` on each build. Multiple paths can be separated by `;`. `COPY_PLUGIN_CONFIGURATIONS` limits the copy to some build configurations: `all` (default) or a `;` list like `Release` / `Debug;RelWithDebInfo`.

**Packaging:** an explicit step, not part of a normal build — build the `package_mod` target (`cmake --build --preset package`, or `cmake --build build --config Release --target package_mod`), which builds the plugin if needed and runs `cmake/package.cmake` to produce a versioned `.7z` with the DLL and the `data/mod/` files in `build/package/`. A non-Release archive has the configuration in its file name.

**Unit tests:** code that does not need the running game (the settings model, the loaders) is unit tested with Catch2, in [tests/](tests/CMakeLists.txt). A normal build also builds `ModsConfigVR_tests`; run them with `ctest --test-dir build -C Release --output-on-failure`, or one with `build/tests/Release/ModsConfigVR_tests "<test name>"`.

- The executable compiles the sources it covers, listed in `tests/CMakeLists.txt`, and links the framework and CommonLibF4 as the mod does. `tests/Setup.cpp` sets CommonLibF4's `REL::NO_GAME`, so there is no game in the process: a test that calls the game fails with an access violation.
- A new source under test is added to that list, with its `<Name>Tests.cpp`.
- `tests/FullPanelTests.cpp` checks what the panel does: what is selected, and what a press of the pointer on a row of the tree or on a breadcrumb selects. It draws the panel in frames of ImGui with no game and no renderer, which `tests/TestPanel.h` sets up as the framework's canvas does.
- `tests/FullPanelRenderTests.cpp` draws the panel the same way and saves it as images, to look at a change of the UI before it is tried in the headset. They check nothing and are not run by `ctest`: run `build/tests/Release/ModsConfigVR_tests "[render]"`, and open the `.bmp` files in `build/tests/render/`. Add a case there for a new view or state of the panel.
- `-DMODS_CONFIG_BUILD_TESTS=OFF` leaves them out.

Everything else is tested by hand, in the game.

**CI:** `.github/workflows/build.yml` configures and builds Release on every push and pull request to `main`, and runs the unit tests. `.github/workflows/maintenance.yml` runs pre-commit there and commits any fixes back to the branch (pull requests from forks are only checked).

## Code Style

clang-format enforces style (`.clang-format`): LLVM-based, 180-column limit, 4-space indent, CRLF line endings, pointer-left (`T* p`), namespace indentation enabled, braces on new lines for classes/functions/namespaces.

After editing code run `pre-commit run --files <changed files>`: it formats `src/` and `tests/` with clang-format and runs the repo's other checks (`.pre-commit-config.yaml`). `data/` and `external/` are excluded.

After cloning, run `pre-commit install` once to run those checks on every commit; the hook lives in `.git/hooks/` and is not version-controlled.

Conventions:

- **Naming:** camelCase for variables/functions, PascalCase for types/classes
- **Namespace:** all project code lives in `namespace mods_config`
- **Logging:** spdlog throughout; use `logger::trace/debug/info/warn/error` from the framework
- **C++ standard:** C++23; use modern features (ranges, constexpr, smart pointers, structured bindings)
- **PCH:** [src/PCH.h](src/PCH.h) is the precompiled header — add widely-used includes there

## Architecture

### Plugin Lifecycle

The entry point is [src/ModsConfigVR.h](src/ModsConfigVR.h) / [src/ModsConfigVR.cpp](src/ModsConfigVR.cpp), a `ModBase` subclass (global `g_modsConfig`) that implements the F4SE plugin hooks:

- `F4SEPlugin_Query` / `F4SEPlugin_Load` — standard F4SE registration, forwarded to the framework
- `onModLoaded()` — F4SE load phase; register Papyrus functions and install hooks
- `onGameLoaded()` — once, when the game finishes loading
- `onGameSessionLoaded()` — on a new game and on every save load
- `onFrameUpdate()` — every frame while the player is loaded

### Main Components

| Component  | Files                                                 | Responsibility                                                                    |
| ---------- | ----------------------------------------------------- | --------------------------------------------------------------------------------- |
| **ModsConfigVR**  | [src/ModsConfigVR.h](src/ModsConfigVR.h) / [.cpp](src/ModsConfigVR.cpp)    | The mod class and the F4SE entry points: the opening gesture, taking it from FRIK |
| **Config** | [src/Config.h](src/Config.h) / [.cpp](src/Config.cpp) | INI load via `ConfigBase` (global `g_config`), live hot-reload via a file watcher |
| **ConfigMenu** | [src/ConfigMenu.h](src/ConfigMenu.h) / [.cpp](src/ConfigMenu.cpp) | The menu: the ImGui panel in the world, which the player can move, and the vrui buttons on the offhand. What the panel shows is the Full panel's |
| **FullPanel** | [src/ui/FullPanel.h](src/ui/FullPanel.h) / [.cpp](src/ui/FullPanel.cpp) | What the panel shows, drawn with ImGui as the design system has it: the title bar with its breadcrumbs, the tree of the mods, and the menu's front page or a page of the selected mod. It holds what is selected ([src/ui/Selection.h](src/ui/Selection.h)), and sets the design system's style on ImGui's (`theme::applyStyle`). Nothing in `src/ui/` knows of the game, so a test draws it |
| **ModTree** | [src/ui/ModTree.h](src/ui/ModTree.h) / [.cpp](src/ui/ModTree.cpp) | The tree of the mods on the left of the Full panel: its plate, its header with the number of the mods, and their rows. The selected mod is open, with a row for each of its pages, and the selected row is scrolled into view |
| **ModPage** | [src/ui/ModPage.h](src/ui/ModPage.h) / [.cpp](src/ui/ModPage.cpp) | A page of a mod in the content pane: its rows. A row is its text and its type until the commit that draws its control |
| **Drawing** | [src/ui/Drawing.h](src/ui/Drawing.h) / [.cpp](src/ui/Drawing.cpp) | What the parts of the panel draw with: a line of text that ends in an ellipsis, an outline, and the look of a row under the ray |
| **Model** | [src/model/Definition.h](src/model/Definition.h) | The definition of the settings model, which the UI draws: a mod, its pages and their items. See [docs/design/model.md](docs/design/model.md) |
| **MCM loader** | [src/mcm/McmLoader.h](src/mcm/McmLoader.h) / [.cpp](src/mcm/McmLoader.cpp) | Reads an MCM `config.json` into the model, and finds the MCM mods of the game: every config under `Data\MCM\Config`, while `mcm_vr.dll` is loaded. It grows with the rows: the commit that draws a control adds the parsing of that control and its tests |
| **DevBench** | [src/devbench/MCDevBench.h](src/devbench/MCDevBench.h) / [.cpp](src/devbench/MCDevBench.cpp) | The mod's devbench tool: its state and its actions (open, close, mods, select) |
| **FRIK API** | [src/api/FRIKApi.h](src/api/FRIKApi.h) | FRIK's client header, copied as is from FRIK (`src/api/FRIKApi.h`, API v4): never edit it |

TODO: add a row for each component as the mod grows.

### Config

[data/config/ModsConfigVR.ini](data/config/ModsConfigVR.ini) is the shipped default config. It is embedded in the DLL (resource `IDR_CONFIG_INI`) and written on first run to `%USERPROFILE%\Documents\My Games\Fallout4VR\Mods_Config\ModsConfigVR\ModsConfigVR.ini`.

- The main section is `[ModsConfigVR]`; it must match `NAME` in `CMakeLists.txt`, which the config reads back as both the INI file name and the default section.
- `[Panel]` holds the menu's panel in the world: the button that moves it and how smoothly it follows the hand, the gesture that puts it in front of the player again, and the distance at which that happens by itself.
- `[Pointer]` holds the menu's pointer: where the ray is on the hand, how the ray and the mark on the panel look, and how fast the thumbstick scrolls. It is read into an `imgui::PointerStyle` and given to the framework's pointer every frame.
- `[Debug]` holds the framework's keys (log level and pattern, flow flags, data dumps, debug draw).
- To add a value: a member in `Config.h`, a read in `Config::loadIniConfigInternal()`, and the key in the shipped INI.
- `ConfigBase` watches the file and reloads it on change, so edits apply while the game runs. The reload runs on the watcher's thread: apply anything that calls into the engine from `onFrameUpdate()`.
- Bump `iVersion` in `[Debug]` when the shipped INI changes, so existing users' files are migrated.

### Mod Data

The mod's VR UI is mostly one Dear ImGui panel in a `vrui` layout (`imgui::UIImGuiPanel`), with the framework's primitive-drawn `vrui` panel buttons where a control sits on the hand; see [docs/design/ui-and-vr-interaction.md](docs/design/ui-and-vr-interaction.md). Its look (colors, type, sizes, the ImGui style, and mocks of each panel) is the design system in [docs/design/design-system/](docs/design/design-system/README.md), which is also published as an artifact: its README says how to change and publish it. [src/Theme.h](src/Theme.h), the colors, sizes and `applyStyle` for ImGui, is generated from it by `docs/design/design-system/tools/gen_theme.py`: change the tokens, not the header. It does not use NIF button atlases: the template's `ui-common` / `ui-example` atlases and their source PNGs were removed on purpose.

`data/mod/` is the mod's game `Data` tree, packaged beside the DLL. `Meshes\ModsConfigVR\f4cf\` and `Textures\ModsConfigVR\f4cf\` hold the framework's own assets (activation sphere, binding icons, shared VR UI icons): ship them as they came and refresh them from `external/F4VR-CommonFramework/mod-template/` when updating the framework. The mod's own assets go beside them.

### Framework

The framework's own [CLAUDE.md](external/F4VR-CommonFramework/CLAUDE.md) maps its subsystems, and each has a README with examples under `external/F4VR-CommonFramework/src/`:

| Need                                                        | Read                               |
| ----------------------------------------------------------- | ---------------------------------- |
| Controller input, input suppression, haptics                | `src/vrcf/README.md`               |
| Rebindable controller bindings, activation spheres          | `docs/input-binding.md`            |
| In-world VR UI: panels and buttons from text and images     | `src/vrui/README.md`               |
| Lines, images and text drawn over the VR view               | `src/render/README.md`             |
| Dear ImGui content on world-space quads                     | `src/imgui/README.md`              |
| Game state: nodes, skeleton, weapon and menu state, HUD     | `src/f4vr/README.md`               |
| Papyrus native functions                                    | `src/f4sevr/README.md`             |
| Debug draw overlay, the `[Debug]` INI keys                  | `src/debug/README.md`, `docs/debug-config.md` |
| Timing a hot path (`F4CF_PERF_SCOPE`), Tracy                | `src/perf/README.md`               |
| The mod's devbench tool (drive and inspect the running game) | `src/devbench/README.md`           |

### Dependencies

Pulled via vcpkg (`vcpkg.json`) with a **pinned baseline** required for CommonLibF4 compatibility:

- `F4VR-CommonFramework` (git submodule at `external/`) — re-exports CommonLibF4 and the framework base classes
- `spdlog`, `nlohmann-json`, `simpleini`, `thomasmonkman-filewatch`, `args`, `rapidcsv`, `rsm-mmio`, `xbyak`, `cpptrace`, `imgui` (with `dx11-binding`, for the framework's Dear ImGui UI layer), `catch2` (for the unit tests)

The vcpkg baseline **must not be changed** without verifying CommonLibF4 still builds; it was specifically pinned to `b4a3d89125e45bc8f80fb94bef9761d4f4e14fb9`.
