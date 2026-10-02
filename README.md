# Mods Config VR

> TODO: one-line tagline — what Mods Config VR does, for Fallout 4 VR.

TODO: a short paragraph on what the mod does and why a player wants it.

Built on the [F4VR Common Framework](https://github.com/ArthurHub/F4VR-CommonFramework).

## Features

- TODO
- TODO
- TODO

## Requirements

- [Fallout 4 VR](https://store.steampowered.com/app/611660/)
- [Fallout 4 VR Script Extender (F4SEVR)](https://f4se.silverlock.org/)
- TODO: any other required mods (e.g. [FRIK](https://github.com/rollingrock/Fallout-4-VR-Body))

## Installation

**With a mod manager (recommended):** install the release archive with Mod Organizer 2 or Vortex and
enable it. Launch the game through F4SEVR (`f4sevr_loader.exe`).

**Manual:** extract the archive into your `Fallout 4 VR` folder so the plugin lands at
`Data\F4SE\Plugins\ModsConfigVR.dll`, then launch through F4SEVR.

## Configuration

Settings live in an INI created on first run at:

```
%USERPROFILE%\Documents\My Games\Fallout4VR\Mods_Config\ModsConfigVR\ModsConfigVR.ini
```

Edits apply **live** while the game is running — no restart needed.

- Controller-binding syntax: [input-binding guide](https://github.com/ArthurHub/F4VR-CommonFramework/blob/main/docs/input-binding.md)
- Logging / debug keys: [debug-config guide](https://github.com/ArthurHub/F4VR-CommonFramework/blob/main/docs/debug-config.md)

TODO: document your mod's own settings here.

## Building from source

This mod is built with the [F4VR Common Framework](https://github.com/ArthurHub/F4VR-CommonFramework).
See the framework's [Creating a new mod](https://github.com/ArthurHub/F4VR-CommonFramework/blob/main/docs/creating-a-new-mod.md)
guide for the full toolchain (VS 2022/2026, CMake 4.2+, `VCPKG_ROOT`). Quick version:

```sh
git clone --recurse-submodules https://github.com/ArthurHub/F4VR-Mods-Config.git
cd F4VR-Mods-Config
cmake --preset default      # generates build/ModsConfigVR.sln
```

Open the solution and build. To package, build the `package_mod` target in Release; it writes a versioned `.7z` under `build/package/`.
To profile with [Tracy](https://github.com/wolfpld/tracy), build the `Tracy` configuration (`cmake --build --preset tracy`): a Release build
whose perf sites are also Tracy zones.

Code style is enforced by clang-format through [pre-commit](https://pre-commit.com/): run `pre-commit install` once after cloning so
the checks run on every commit. The same checks, and a Release build, run on GitHub for every push and pull request to `main`.

## Support

TODO: link your issue tracker / Nexus posts / Discord.

- [Nexus Mods](https://www.nexusmods.com/fallout4/mods/<ID>)
- [Report a bug](https://github.com/ArthurHub/F4VR-Mods-Config/issues)

## Credits

- Built on the [F4VR Common Framework](https://github.com/ArthurHub/F4VR-CommonFramework) by Arthur T.
- TODO: other authors / assets you built on.

## License

[GPL-3.0-or-later](LICENSE) © 2026 Arthur T
