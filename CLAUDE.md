# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Mods Config VR (`ModsConfigVR`) is a **C++23 DLL plugin** for F4SE (Fallout 4 Script Extender) for Fallout 4 VR, built on the [F4VR Common Framework](external/F4VR-CommonFramework/README.md).

It is a shared in-game configuration menu for Fallout 4 VR mods. A mod ships a declarative JSON package or already has an MCM menu, and the player edits the mod's real settings in VR. A package is saved with an explicit Save, and a mod that registers through the native API gets live preview. The design is in [docs/design/](docs/design/README.md). So far the menu opens on its front page and lists the MCM mods that are installed. A mod that is selected shows its pages in the tree and their rows. A switcher and a slider that are kept in a global of the game are edited, and any other row is its control or its text and its type. The framework work the mod needed first is listed in [docs/design/framework-requirements.md](docs/design/framework-requirements.md), and its commits are tracked in [docs/design/framework-plan.md](docs/design/framework-plan.md). The mod's own milestones and commits are in [docs/design/mod-plan.md](docs/design/mod-plan.md).

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

**Packaging:** an explicit step, not part of a normal build — build the `package_mod` target (`cmake --build --preset package`, or `cmake --build build --config Release --target package_mod`), which builds the plugin if needed and runs `cmake/package.cmake` to produce a versioned `.7z` with the DLL and the `data/mod/` files in `build/package/`. A non-Release archive has the configuration in its file name. It also packs the MCM showcase, `data/mcm-showcase/`, into a `.7z` of its own, which is an optional file of the mod.

**Unit tests:** code that does not need the running game (the settings model, the loaders) is unit tested with Catch2, in [tests/](tests/CMakeLists.txt). A normal build also builds `ModsConfigVR_tests`; run them with `ctest --test-dir build -C Release --output-on-failure`, or one with `build/tests/Release/ModsConfigVR_tests "<test name>"`.

- The executable compiles the sources it covers, listed in `tests/CMakeLists.txt`, and links the framework and CommonLibF4 as the mod does. `tests/Setup.cpp` sets CommonLibF4's `REL::NO_GAME`, so there is no game in the process: a test that calls the game fails with an access violation.
- A new source under test is added to that list, with its `<Name>Tests.cpp`.
- `tests/TestSwf.h` builds an SWF file in memory, for a test of an image.
- `tests/TestGlobals.h` has globals in a map in place of the game's, for a test of the MCM store and of a row that is pressed.
- `tests/FullPanelTests.cpp` checks what the panel does: what is selected, and what a press of the pointer on a row of the tree or on a breadcrumb selects. It draws the panel in frames of ImGui with no game and no renderer, which `tests/TestPanel.h` sets up as the framework's canvas does.
- `tests/FullPanelRenderTests.cpp` draws the panel the same way and saves it as images, to look at a change of the UI before it is tried in the headset. They check nothing and are not run by `ctest`: run `build/tests/Release/ModsConfigVR_tests "[render]"`, and open the `.bmp` files in `build/tests/render/`. Add a case there for a new view or state of the panel. One case draws every page of the MCM showcase. An image of a page is drawn from its pixels, as the game draws it from its texture.
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
| **ConfigMenu** | [src/ConfigMenu.h](src/ConfigMenu.h) / [.cpp](src/ConfigMenu.cpp) | The menu: the ImGui panel in the world, which the player can move, and the vrui buttons on the offhand. What the panel shows is the Full panel's, and the values it shows are the MCM store's, with the game's globals. It sees the Back button, which goes a step back in the panel and closes the menu from its front page, and gives the hand that points a pulse as its press does something in the panel, and as its thumbstick steps a slider. It tells the panel how far the thumbstick of that hand is pushed to a side, which steps a slider. It draws the images of a mod's page for the panel, with a texture made from the pixels of each when it is first drawn (`imgui::addImage`) |
| **FullPanel** | [src/ui/FullPanel.h](src/ui/FullPanel.h) / [.cpp](src/ui/FullPanel.cpp) | What the panel shows, drawn with ImGui as the design system has it: the title bar with its breadcrumbs, the tree of the mods, the menu's front page or a page of the selected mod, and the footer with the help line. It holds what is selected ([src/ui/Selection.h](src/ui/Selection.h)), starts a mod's editing session in the store the menu gives it as the mod is selected, and sets the design system's style on ImGui's (`theme::applyStyle`). Nothing in `src/ui/` knows of the game, so a test draws it |
| **ModTree** | [src/ui/ModTree.h](src/ui/ModTree.h) / [.cpp](src/ui/ModTree.cpp) | The tree of the mods on the left of the Full panel: its plate, its header with the number of the mods, and their rows. The selected mod is open, with a row for each of its pages, and the selected row is scrolled into view |
| **ModPage** | [src/ui/ModPage.h](src/ui/ModPage.h) / [.cpp](src/ui/ModPage.cpp) | A page of a mod in the content pane: its rows. A section is its heading, a spacer its empty space, a text row its wrapped lines, and an image its picture in the middle of the rows, drawn by the `ImageDrawer` the menu gives the panel. A switcher, a slider, a stepper and a dropdown are drawn as their controls, with the value the store has for each. A press on the row of a switcher turns it on or off in the store. A slider is drawn and edited by `Sliders`. A stepper and a dropdown are disabled until the commit that makes them work, and so is a control whose value is not available. Any other row is its text and its type until the commit that draws its control. A row with a text of its own has the look of a row under the ray, and of one that is pressed. It tells the row the ray is on, for the help line, with the reason as its help where its value is not available, and whether a press changed a value or took hold of a slider |
| **Sliders** | [src/ui/Sliders.h](src/ui/Sliders.h) / [.cpp](src/ui/Sliders.cpp) | The sliders of the page that is shown: how one is drawn, and how the player edits one. The trigger drags a slider by its track, to a value on its step, and Back during the drag gives it the value it had. The thumbstick steps the slider whose row the ray is on, after a short time there, and again while it is held |
| **HelpLine** | [src/ui/HelpLine.h](src/ui/HelpLine.h) / [.cpp](src/ui/HelpLine.cpp) | The help line, on the left of the footer: the text and the help of the row the ray is on, as `Name: help`. Two lines of body text, or three in the detail size for a help that needs them, and a longer one ends in an ellipsis. It keeps its row until the ray is on another or leaves the page. With no row it is the status of the mod's editing session: that changes are saved as they are made |
| **Drawing** | [src/ui/Drawing.h](src/ui/Drawing.h) / [.cpp](src/ui/Drawing.cpp) | What the parts of the panel draw with: a line of text that ends in an ellipsis, lines of text that are wrapped in a width with their sizes, colors and align, a text in pieces of a color each that is wrapped in a number of lines, a section's heading, an outline, the look of a row under the ray, and the frame of a control with a text in it |
| **Model** | [src/model/Definition.h](src/model/Definition.h) | The definition of the settings model, which the UI draws: a mod, its pages and their items, and for a setting its range or its options, and where its value is kept. See [docs/design/model.md](docs/design/model.md) |
| **Store** | [src/model/Store.h](src/model/Store.h) | The store of the settings model, which the UI asks for every value and sets it in: the values of the mod that is selected. A value that cannot be read is not available, with the reason |
| **MCM store** | [src/mcm/McmStore.h](src/mcm/McmStore.h) / [.cpp](src/mcm/McmStore.cpp) | The store of an MCM mod: a value is read from its storage in every frame, and a change is written there at once. A global is read and written through `Globals`, so a test has it without the game. A setting with no storage is held here while its mod is selected. It grows with the storages: the commit that reads one adds it here, with its tests |
| **Game globals** | [src/GameGlobals.h](src/GameGlobals.h) / [.cpp](src/GameGlobals.cpp) | The `Globals` of the running game: a global's form by its plugin and its id, looked up once |
| **Text markup** | [src/model/TextMarkup.h](src/model/TextMarkup.h) / [.cpp](src/model/TextMarkup.cpp) | The tags a text of a config can have, as MCM configs use them: `<br>`, `<p align>`, `<font size color>` and entities. A font size is kept as written: it is a size of MCM's menu, which the drawing scales to the panel's (`MARKUP_BODY_SIZE` in `Drawing.cpp`). `parseMarkup` reads a text row into the model's lines and runs, `plainLines` a text row with no `html`, and `removeMarkup` gives a help as plain text on one line, which the MCM loader reads a row's `help` with |
| **Translations** | [src/model/Translations.h](src/model/Translations.h) / [.cpp](src/model/Translations.cpp), [src/GameTranslations.h](src/GameTranslations.h) / [.cpp](src/GameTranslations.cpp) | The texts of the game's language by their `$` tokens, which a loader translates every text the player sees with. `loadGameTranslations` copies the game's translation table once: F4SE fills it with the translation file of every enabled game plugin |
| **FallUI translations** | [src/mcm/FallUITranslations.h](src/mcm/FallUITranslations.h) / [.cpp](src/mcm/FallUITranslations.cpp) | The texts of a FallUI mod in the game's language, from the language files its config names. FallUI translates its pages with code of its own in its `lib.swf`, which the menu does not run: this does what that code does. The MCM loader translates a FallUI mod's texts with it, before the game's translations |
| **MCM loader** | [src/mcm/McmLoader.h](src/mcm/McmLoader.h) / [.cpp](src/mcm/McmLoader.cpp) | Reads an MCM `config.json` into the model, with its `$` texts translated, and finds the MCM mods of the game: every config under `Data\MCM\Config`, while `mcm_vr.dll` is loaded. It reads where a row keeps its value: a `GlobalValue`, or nowhere. It grows with the rows: the commit that draws a control adds the parsing of that control and its tests |
| **SWF images** | [src/mcm/SwfImages.h](src/mcm/SwfImages.h) / [.cpp](src/mcm/SwfImages.cpp) | The images of an MCM mod, read out of its `lib.swf` with no Flash: an image row names a class of that file, and `SwfFile` gives the pixels of the bitmap the class shows and the size it is shown at. It reads a bitmap that is stored without loss, by itself, in a sprite, or beside a class of code. `McmConfigImages` reads an image row with it |
| **MCM config images** | [src/mcm/McmConfigImages.h](src/mcm/McmConfigImages.h) / [.cpp](src/mcm/McmConfigImages.cpp) | The images of an MCM config, which the MCM loader reads an image row with: the class the row names, of the `lib.swf` in the config folder of the mod it names, at the size MCM's menu shows it. A file is read once for all its images |
| **DevBench** | [src/devbench/MCDevBench.h](src/devbench/MCDevBench.h) / [.cpp](src/devbench/MCDevBench.cpp) | The mod's devbench tool: its state and its actions (open, close, back, mods, select) |
| **FRIK API** | [src/api/FRIKApi.h](src/api/FRIKApi.h) | FRIK's client header, copied as is from FRIK (`src/api/FRIKApi.h`, API v4): never edit it |

TODO: add a row for each component as the mod grows.

### Config

[data/config/ModsConfigVR.ini](data/config/ModsConfigVR.ini) is the shipped default config. It is embedded in the DLL (resource `IDR_CONFIG_INI`) and written on first run to `%USERPROFILE%\Documents\My Games\Fallout4VR\Mods_Config\ModsConfigVR\ModsConfigVR.ini`.

- The main section is `[ModsConfigVR]`; it must match `NAME` in `CMakeLists.txt`, which the config reads back as both the INI file name and the default section. It holds the binding that opens the menu, and the Back button: a short press of a button of the hand that points at the panel, and not a binding. As shipped it is the grip, which also moves the panel: the panel then moves only when the grip is held longer (`UIManager::setWorldMoveHoldDelay`).
- `[Panel]` holds the menu's panel in the world: its scale, which is the size of its text and rows, its distance, height and tilt, the opacity of its background, the button that moves it and how smoothly it follows the hand, the gesture that puts it in front of the player again, and the distance at which that happens by itself.
- `[Pointer]` holds the menu's pointer: where the ray is on the hand, how the ray and the mark on the panel look, and how fast the thumbstick scrolls. It is read into an `imgui::PointerStyle` and given to the framework's pointer every frame. It also holds the pulse the hand that points gets as its press does something in the panel.
- `[Debug]` holds the framework's keys (log level and pattern, flow flags, data dumps, debug draw).
- To add a value: a member in `Config.h`, a read in `Config::loadIniConfigInternal()`, and the key in the shipped INI.
- `ConfigBase` watches the file and reloads it on change, so edits apply while the game runs. The reload runs on the watcher's thread: apply anything that calls into the engine from `onFrameUpdate()`.
- Bump `iVersion` in `[Debug]` when the shipped INI changes, so existing users' files are migrated.

### Mod Data

The mod's VR UI is mostly one Dear ImGui panel in a `vrui` layout (`imgui::UIImGuiPanel`), with the framework's primitive-drawn `vrui` panel buttons where a control sits on the hand; see [docs/design/ui-and-vr-interaction.md](docs/design/ui-and-vr-interaction.md). Its look (colors, type, sizes, the ImGui style, and mocks of each panel) is the design system in [docs/design/design-system/](docs/design/design-system/README.md), which is also published as an artifact: its README says how to change and publish it. [src/Theme.h](src/Theme.h), the colors, sizes and `applyStyle` for ImGui, is generated from it by `docs/design/design-system/tools/gen_theme.py`: change the tokens, not the header. It does not use NIF button atlases: the template's `ui-common` / `ui-example` atlases and their source PNGs were removed on purpose.

`data/mod/` is the mod's game `Data` tree, packaged beside the DLL. `Meshes\ModsConfigVR\f4cf\` and `Textures\ModsConfigVR\f4cf\` hold the framework's own assets (activation sphere, binding icons, shared VR UI icons): ship them as they came and refresh them from `external/F4VR-CommonFramework/mod-template/` when updating the framework. The mod's own assets go beside them.

`data/mcm-showcase/` is the MCM showcase, a game `Data` tree of its own: an MCM config (`MCM\Config\ModsConfigVR_McmShowcase`) with every kind of row an MCM config has, in every look, one kind a page. It has only what works in MCM too: what the package schema adds to MCM would be a showcase of its own. It shows how an MCM menu looks in the mod, and is published as an optional file for developers. Install it in the game to try a change of a row in the headset, and add to it what a new commit draws: a control, a state, an edge case. A unit test loads it, and a render test draws each of its pages (`mcm-showcase-*.bmp`). Its `lib.swf` holds the images of its "Images" page, and is written by `tools/gen_mcm_showcase_lib.py`: change an image there and run it again.

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
- `spdlog`, `nlohmann-json`, `simpleini`, `thomasmonkman-filewatch`, `args`, `rapidcsv`, `rsm-mmio`, `xbyak`, `cpptrace`, `imgui` (with `dx11-binding`, for the framework's Dear ImGui UI layer), `catch2` (for the unit tests), `zlib` (an MCM mod's `lib.swf` is compressed with it)

The vcpkg baseline **must not be changed** without verifying CommonLibF4 still builds; it was specifically pinned to `b4a3d89125e45bc8f80fb94bef9761d4f4e14fb9`.
