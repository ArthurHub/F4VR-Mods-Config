# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Mods Config FO4 VR (`ModsConfigFO4VR`) is a **C++23 DLL plugin** for F4SE (Fallout 4 Script Extender) for Fallout 4 VR, built on the [F4VR Common Framework](external/F4VR-CommonFramework/README.md).

It is a shared in-game configuration menu for Fallout 4 VR mods. A mod ships a declarative JSON config spec or already has an MCM menu, and the player edits the mod's real settings in VR. A change of every mod waits for an explicit Save, and a mod that registers through the native API gets live preview.

- Design: [docs/design/](docs/design/README.md). Milestones: [docs/design/mod-plan.md](docs/design/mod-plan.md), with the commits of each phase in `docs/design/mod-plan-phase-<n>.md`. Framework work: [framework-requirements.md](docs/design/framework-requirements.md), [framework-plan.md](docs/design/framework-plan.md).
- What the menu has so far, and each component's responsibility: [docs/agents/components.md](docs/agents/components.md).
- Docs for players and mod authors: [docs/README.md](docs/README.md) with `faq.md` and `changelog.md` beside it, and [docs/for-mod-authors/](docs/for-mod-authors/README.md): the guide, the showcase walkthrough, MCM compatibility and the native API. `docs/nexus.md` is the Nexus page's text, in BBCode. A change a player or an author sees updates them.

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

**Packaging:** an explicit step, not part of a normal build — build the `package_mod` target (`cmake --build --preset package`, or `cmake --build build --config Release --target package_mod`), which builds the plugin if needed and runs `cmake/package.cmake` to produce a versioned `.7z` with the DLL and the `data/mod/` files in `build/package/`. A non-Release archive has the configuration in its file name. It also packs the shared `data/showcase/` game `Data` tree, containing both the MCM and config spec showcases, into one optional Showcase `.7z` for developers.

**Unit tests:** code that does not need the running game (the settings model, the loaders) is unit tested with Catch2, in [tests/](tests/CMakeLists.txt). A normal build also builds `ModsConfigFO4VR_tests`; run them with `ctest --test-dir build -C Release --output-on-failure`, or one with `build/tests/Release/ModsConfigFO4VR_tests "<test name>"`.

- The executable compiles the sources it covers, listed in `tests/CMakeLists.txt`, and links the framework and CommonLibF4 as the mod does. `tests/Setup.cpp` sets CommonLibF4's `REL::NO_GAME`, so there is no game in the process: a test that calls the game fails with an access violation.
- A new source under test is added to that list, with its `<Name>Tests.cpp`.
- The game-calling sources (`GameActions.cpp`, `GameGlobals.cpp`, `GameProperties.cpp`, `mcm/McmEvents.cpp`) are not compiled: the tests link their own doubles in their place. Before writing a test that reads a global, a property, an event, an action or a ModSetting, or draws the panel, read [docs/agents/testing.md](docs/agents/testing.md).
- Render tests (`"[render]"`) save the panel as `.bmp` in `build/tests/render/`, check nothing, and are not run by `ctest`: add a case for a new view or state of the panel. `tools/gen_docs_renders.py` (needs Pillow) turns those of the showcase walkthrough into the PNGs of `docs/assets/showcase/`: run the render tests and it again when the panel's look or a showcase page changes.
- `-DMODS_CONFIG_BUILD_TESTS=OFF` leaves them out.

**End-to-end tests:** [tests/e2e/](tests/e2e/README.md) is pytest tests that drive the running game through devbench's REST port: from the main menu they start a game in QASmoke, back up the files a Save writes through devbench's `files` tool and put them back after, sweep every setting of the MCM test set and the config specs, and test the showcase walkthrough, the dialog, MCM's Keep and Revert, the live preview, Back, the breadcrumbs, Help and Minimize, what a row does beyond its value, and a loading screen ending the session. They are run by hand (`pytest tests/e2e`, after `pip install -r tests/e2e/requirements.txt`), as CI has no game; a run a crash ended is put back by the next one, or by `pytest tests/e2e --e2e-restore`. A new devbench action or a new page of a showcase gets its test there.

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

The entry point is [src/ModsConfigFO4.h](src/ModsConfigFO4.h) / [src/ModsConfigFO4.cpp](src/ModsConfigFO4.cpp), a `ModBase` subclass (global `g_modsConfig`) that implements the F4SE plugin hooks:

- `F4SEPlugin_Query` / `F4SEPlugin_Load` — standard F4SE registration, forwarded to the framework
- `onModLoaded()` — F4SE load phase; register Papyrus functions and install hooks
- `onGameLoaded()` — once, when the game finishes loading
- `onGameSessionLoaded()` — on a new game and on every save load
- `onFrameUpdate()` — every frame while the player is loaded

### Source layout

Each component's responsibility, in detail: [docs/agents/components.md](docs/agents/components.md). Read its row before changing a component.

- `src/ModsConfigFO4.*`, `src/ConfigMenu.*`, `src/Config.*`, `src/AdjustInWorld.*`: the mod, the menu in the world and its vrui buttons, its own config, tuning a transform in the world.
- `src/ui/`: the Full panel and its parts, drawn with ImGui. Nothing in it knows the game, so a test draws it.
- `src/model/`: the settings model (`ConfigDefinition.h`, see [docs/design/model.md](docs/design/model.md)), the `Store` interface, and game-free helpers: conditions, choice options, files of a folder, the CommonFramework values.
- `src/spec/`: config specs: the loader, the store, TOML and INI files, live copies.
- `src/mcm/`: MCM mods: the loader, the store, ModSettings, keybinds, SWF images, FallUI translations.
- `src/api/`: the native API. `ModsConfigFO4Api.h` is the client header, which mods copy as is and the framework carries a copy of: keep both copies identical. `FRIKApi.h` is FRIK's header: never edit it.
- `src/devbench/`: the mod's devbench tool.

### Config

[data/config/ModsConfigFO4VR.toml](data/config/ModsConfigFO4VR.toml) is the shipped default config, in TOML. It is embedded in the DLL (resource `IDR_CONFIG`) and written on first run to `%USERPROFILE%\Documents\My Games\Fallout4VR\Mods_Config\ModsConfigFO4VR\ModsConfigFO4VR.toml`. A player's `ModsConfigFO4VR.ini` from an older version is moved into it by the framework, with its values, on the first load that finds no TOML.

Sections: `[ModsConfigFO4VR]` (the opening and Back bindings, `bHighlightHelp`), `[Panel]`, `[Developer]`, `[Pointer]`, `[Debug]`. What each key does: [docs/agents/config-and-data.md](docs/agents/config-and-data.md).

- To add a value: a member in `Config.h`, a read in `Config::loadConfigInternal()` (`config[SECTION]["key"].value_or(default)`, which gives the default for a key that is missing or of another type), and the key in the shipped file. Write a float with a decimal point (`1.0`) and a string in quotes.
- `ConfigBase` watches the file and reloads it on change, so edits apply while the game runs. The reload runs on the watcher's thread: apply anything that calls into the engine from `onFrameUpdate()`.
- Bump `iVersion` in `[Debug]` when the shipped file changes, so existing users' files are migrated. A value whose type or name changes is moved in `updateConfigToLatestVersionCustom`.

### Mod Data

The mod's VR UI is mostly one Dear ImGui panel in a `vrui` layout (`imgui::UIImGuiPanel`), with the framework's primitive-drawn `vrui` panel buttons where a control sits on the hand; see [docs/design/ui-and-vr-interaction.md](docs/design/ui-and-vr-interaction.md). Its look (colors, type, sizes, the ImGui style, and mocks of each panel) is the design system in [docs/design/design-system/](docs/design/design-system/README.md), which is also published as an artifact: its README says how to change and publish it. [src/Theme.h](src/Theme.h), the colors, sizes and `applyStyle` for ImGui, is generated from it by `docs/design/design-system/tools/gen_theme.py`: change the tokens, not the header. It does not use NIF button atlases: the template's `ui-common` / `ui-example` atlases and their source PNGs were removed on purpose.

`data/mod/` is the mod's game `Data` tree, packaged beside the DLL. `Meshes\ModsConfigFO4VR\f4cf\` and `Textures\ModsConfigFO4VR\f4cf\` hold the framework's own assets (activation sphere, binding icons, shared VR UI icons): ship them as they came and refresh them from `external/F4VR-CommonFramework/mod-template/` when updating the framework. The mod's own assets go beside them.

`data/mod/ModsConfigFO4/Config/ModsConfigFO4/spec.json` is the mod's own config spec, which edits its TOML. A unit test checks that each of its settings is a key of the shipped TOML, with the shipped value as its default: change the config spec with the TOML.

`data/showcase/` holds the config spec showcase and the MCM showcase: add to them what a new commit reads or draws. Their layout, files and tests: [docs/agents/config-and-data.md](docs/agents/config-and-data.md).

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
| Debug draw overlay, the `[Debug]` config keys               | `src/debug/README.md`, `docs/debug-config.md` |
| Timing a hot path (`F4CF_PERF_SCOPE`), Tracy                | `src/perf/README.md`               |
| The mod's devbench tool (drive and inspect the running game) | `src/devbench/README.md`           |

### Dependencies

Pulled via vcpkg (`vcpkg.json`) with a **pinned baseline** required for CommonLibF4 compatibility:

- `F4VR-CommonFramework` (git submodule at `external/`) — re-exports CommonLibF4 and the framework base classes
- `spdlog`, `nlohmann-json`, `simpleini` (MCM's ModSettings files are INI), `tomlplusplus` (the framework's config), `thomasmonkman-filewatch`, `args`, `rapidcsv`, `rsm-mmio`, `xbyak`, `cpptrace`, `imgui` (with `dx11-binding` and `win32-binding`, for the framework's Dear ImGui UI layer, at 1.92.9 from the pinned baseline), `catch2` (for the unit tests), `zlib` (an MCM mod's `lib.swf` is compressed with it)

The vcpkg baseline **must not be changed** without verifying CommonLibF4 still builds; it is pinned to `5cd4931b18d49c77074703cf293a7837ac7506fa`, the framework's own baseline.

## Agent skills

### Issue tracker

Track issues and specs in GitHub Issues. Read `docs/agents/issue-tracker.md` before working with tickets.

### Triage labels

Use the five default triage roles. Read `docs/agents/triage-labels.md` before applying triage labels.

### Domain docs

Use a single-context layout. Read `docs/agents/domain.md` before exploring domain concepts or architectural decisions.
