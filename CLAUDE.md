# CLAUDE.md

Mods Config FO4 VR (`ModsConfigFO4VR`) is a C++23 F4SE plugin for Fallout 4 VR, built on the [F4VR Common Framework](external/F4VR-CommonFramework/README.md): a shared in-game configuration menu. A mod ships a declarative JSON config spec or already has an MCM menu, and the player edits the mod's real settings in VR. Every change waits for an explicit Save; a mod that registers through the native API gets live preview.

- Design: [docs/design/](docs/design/README.md). Milestones: [mod-plan.md](docs/design/mod-plan.md), with each phase's commits in `docs/design/mod-plan-phase-<n>.md`. Framework work: [framework-requirements.md](docs/design/framework-requirements.md), [framework-plan.md](docs/design/framework-plan.md).
- Components and their traps: [docs/agents/components.md](docs/agents/components.md). Read a component's row before changing it.
- Player and author docs: [docs/README.md](docs/README.md) with `faq.md` and `changelog.md` beside it, [docs/for-mod-authors/](docs/for-mod-authors/README.md) (the guide, the showcase walkthrough, MCM compatibility, the native API), and `docs/nexus.md`, the Nexus page in BBCode. A change a player or an author sees updates them.

## Build and test

```bash
cmake --preset default                                    # VS 2026; the vs2022 preset for VS 2022
cmake --build --preset release                            # or debug / tracy / package
ctest --test-dir build -C Release --output-on-failure     # the unit tests
build/tests/Release/ModsConfigFO4VR_linked_tests "<test name>"   # one test
```

- Prerequisites, presets, copying the DLL to the game, packaging and CI: [docs/agents/build.md](docs/agents/build.md).
- Catch2 unit tests in `tests/linked/<src area>/` cover the game-free code, built of the framework's test pieces; the pytest tests in [tests/e2e/](tests/e2e/README.md) drive the running game and are run by hand. Before adding or writing a test, read [docs/agents/testing.md](docs/agents/testing.md).
- After editing code, run `pre-commit run --files <changed files>`: it formats `src/` and `tests/` with clang-format and runs the repo's checks.

## Code style

- camelCase variables and functions, PascalCase types; all project code in `namespace mods_config`.
- Log with the framework's `logger::trace/debug/info/warn/error`.
- Widely used includes go in the precompiled header [src/PCH.h](src/PCH.h).

## Source layout

The entry point [src/ModsConfigFO4.cpp](src/ModsConfigFO4.cpp) is a `ModBase` subclass (global `g_modsConfig`) implementing the framework's lifecycle hooks: `onModLoaded`, `onGameLoaded`, `onGameSessionLoaded`, `onFrameUpdate`.

- `src/ModsConfigFO4.*`, `src/ConfigMenu.*`, `src/Config.*`, `src/AdjustInWorld.*`: the mod, the menu in the world and its vrui buttons, its own config, tuning a transform in the world.
- `src/ui/`: the Full panel and its parts, drawn with ImGui. Nothing in it knows the game, so a test draws it.
- `src/model/`: the settings model (`ConfigDefinition.h`, see [docs/design/model.md](docs/design/model.md)), the `Store` interface, and game-free helpers: conditions, choice options, choices whose options set other settings (`OptionSets.h`), files of a folder, the CommonFramework values, the search of the mods' names (`Search.h`), the mods' categories and the Categories page the menu fills (`Categories.h`), the unsaved changes of a page the tree marks (`UnsavedPages.h`).
- `src/spec/`: config specs: the loader, the store, TOML and INI files, live copies.
- `src/mcm/`: MCM mods: the loader, the store, ModSettings, keybinds, SWF images, FallUI translations.
- `src/api/`: the native API. `ModsConfigFO4Api.h` is the client header, which mods copy as is and the framework carries a copy of: keep both copies identical. `FRIKApi.h` is FRIK's header: never edit it.
- `src/devbench/`: the mod's devbench tool.

## Config, data and look

- [data/config/ModsConfigFO4VR.toml](data/config/ModsConfigFO4VR.toml) is the mod's shipped config. Before adding or changing a key, read [docs/agents/config-and-data.md](docs/agents/config-and-data.md).
- `data/mod/` is the mod's game `Data` tree, packaged beside the DLL; `data/showcase/` holds the config spec and MCM showcases. Add to the showcases what a new commit reads or draws ([config-and-data.md](docs/agents/config-and-data.md)).
- The panel's look is the design system in [docs/design/design-system/](docs/design/design-system/README.md); [src/Theme.h](src/Theme.h) is generated from its tokens by `tools/gen_theme.py` there: change the tokens and regenerate. The VR UI and its interaction: [docs/design/ui-and-vr-interaction.md](docs/design/ui-and-vr-interaction.md).

## Framework and dependencies

- `external/F4VR-CommonFramework` is a git submodule. Its [CLAUDE.md](external/F4VR-CommonFramework/CLAUDE.md) maps each subsystem to its README: read the matching README before using a subsystem.
- vcpkg's baseline is pinned to `5cd4931b18d49c77074703cf293a7837ac7506fa`, the framework's own, which CommonLibF4 builds with: keep it, unless CommonLibF4 is verified to build with the new one.

## Agent skills

### Issue tracker

Track issues and specs in GitHub Issues. Read `docs/agents/issue-tracker.md` before working with tickets.

### Triage labels

Use the five default triage roles. Read `docs/agents/triage-labels.md` before applying triage labels.

### Domain docs

Use a single-context layout. Read `docs/agents/domain.md` before exploring domain concepts or architectural decisions.
