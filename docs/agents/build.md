# Building, packaging and CI

The build's setup and its optional steps. [CLAUDE.md](../../CLAUDE.md) has the everyday build and test commands.

## Prerequisites

- `VCPKG_ROOT` pointing to a vcpkg installation
- Visual Studio 2022 or 2026 (MSVC toolchain, x64)
- CMake 4.2+
- Submodules initialized: `git submodule update --init --recursive`
- `pre-commit install`, once after cloning, to run the repo's checks on every commit (the hook lives in `.git/hooks/`, not version-controlled)

## Presets

The `default` configure preset inherits `cmake-dev + vcpkg + windows + vs2026`; `vs2022` is the same for VS 2022. The `release` / `debug` / `package` / `tracy` build presets build the `default` configure preset (`tracy` is Release plus the Tracy profiler client). The build output is `build/`; in-source builds are blocked.

## Copying the DLL to the game

`CMakeUserPresets.json` (git-ignored, started from `CMakeUserPresets.json.template`) defines a `custom` preset that sets `POST_BUILD_COPY_PLUGIN` and `COPY_PLUGIN_BASE_PATH` to copy the DLL and PDB to `<path>/F4SE/Plugins/` on each build. Separate several paths with `;`. `COPY_PLUGIN_CONFIGURATIONS` limits the copy to some configurations: `all` (default) or a `;` list like `Release` / `Debug;RelWithDebInfo`.

## Packaging

An explicit step, not part of a normal build: `cmake --build --preset package` (or `cmake --build build --config Release --target package_mod`) builds the plugin if needed and runs `cmake/package.cmake`, which writes a versioned `.7z` with the DLL and the `data/mod/` files to `build/package/`. A non-Release archive has the configuration in its file name. It also packs the shared `data/showcase/` game `Data` tree, with both showcases, into one optional Showcase `.7z` for developers.

## CI

`.github/workflows/build.yml` configures and builds Release on every push and pull request to `main`, and runs the unit tests. `.github/workflows/maintenance.yml` runs pre-commit there and commits any fixes back to the branch (pull requests from forks are only checked).
