# Build, packaging and CI

[CLAUDE.md](../../CLAUDE.md) has the everyday commands.

## Prerequisites

- `VCPKG_ROOT` set; Visual Studio 2022 or 2026 (MSVC x64); CMake 4.2+.
- `git submodule update --init --recursive`.
- `pre-commit install` once after cloning (the hook isn't version-controlled).

## Presets

`default` configures for VS 2026 (`vs2022` for VS 2022). Build presets `release`, `debug`, `package`, `tracy` (Release + Tracy client) use it. Output goes to `build/`; in-source builds are blocked.

## Copying the DLL to the game

Copy `CMakeUserPresets.json.template` to `CMakeUserPresets.json` (git-ignored). Its `custom` preset sets `POST_BUILD_COPY_PLUGIN` and `COPY_PLUGIN_BASE_PATH` (`;`-separated) to copy the DLL and PDB to `<path>/F4SE/Plugins/` on each build; `COPY_PLUGIN_CONFIGURATIONS` limits it (`all`, or e.g. `Release;Debug`). A running game locks the DLL.

## Packaging

`cmake --build --preset package` (target `package_mod`) runs `cmake/package.cmake`: a versioned `.7z` of the DLL and `data/mod/` in `build/package/` (non-Release builds carry the configuration in the name), plus an optional Showcase `.7z` of `data/showcase/`.

## CI

`.github/workflows/build.yml` builds Release and runs the unit tests on pushes and PRs to `main`. `.github/workflows/maintenance.yml` runs pre-commit and commits fixes back (fork PRs are only checked).
