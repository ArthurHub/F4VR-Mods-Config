# Config, data and showcases

## The config file

[data/config/ModsConfigFO4VR.toml](../../data/config/ModsConfigFO4VR.toml) is embedded in the DLL (`IDR_CONFIG`) and written on first run to `Documents\My Games\Fallout4VR\Mods_Config\ModsConfigFO4VR\ModsConfigFO4VR.toml`. The framework migrates an older `ModsConfigFO4VR.ini` into it.

To add a key:

1. A member in `Config.h` and a read in `Config::loadConfigInternal()` (`config[SECTION]["key"].value_or(default)`).
2. The key in the shipped TOML: floats with a decimal point (`1.0`), strings quoted.
3. The setting in the mod's own config spec, `data/mod/ModsConfigFO4/Config/ModsConfigFO4/spec.json`. A unit test checks that every spec setting is a shipped key with the shipped value as its default.
4. Bump `iVersion` in `[Debug]` so users' files migrate; move a renamed or retyped key in `updateConfigToLatestVersionCustom`.

The file hot-reloads on the watcher's thread: apply anything that calls the engine from `onFrameUpdate()`.

## Sections

- `[ModsConfigFO4VR]`: must match `NAME` in `CMakeLists.txt`. The open binding, Back (`kBackBinding`, an `any`-hand binding that fires only on the pointing hand: `VRControllers.check(binding, hand)`), `bHighlightHelp` (cleared by `Config::saveHelpOpened` on the first Help).
- `[Panel]`: the panel's scale, placement, opacity, move binding (`kMoveBinding`, a long grip press; Back is a tap), follow smoothing and recenter.
- `[Developer]`: Developer tools on/off, the front page filter (`attention` / `all` / `one`) and its fold, written by `Config::saveDeveloper`.
- `[Pointer]`: the ray, its mark, scroll speed, press pulse; read into an `imgui::PointerStyle` each frame.
- `[Debug]`: the framework's keys.

## The mod's Data tree

`data/mod/` is packaged beside the DLL. `Meshes\ModsConfigFO4VR\f4cf\` and `Textures\ModsConfigFO4VR\f4cf\` are the framework's assets: ship them as they are, refreshed from `external/F4VR-CommonFramework/mod-template/` when the framework updates. The mod's own assets go beside them. The template's NIF button atlases are left out on purpose: the UI is ImGui plus primitive-drawn vrui buttons.

## Showcases

Both live in the `data/showcase/` Data tree, ship as one optional developer archive, are loaded by a unit test and rendered page by page by the render tests. Add to them whatever a new commit reads or draws, and install them to try a change in the headset.

**Config spec showcase:** `data/showcase/ModsConfigFO4/Config/ModsConfigFO4_Showcase/`
- Pages: General (one of each setting kind plus the common patterns), Detailed Examples (one kind per page: controls, look, behavior), Mistakes last. Values are in a TOML and an INI beside `spec.json`. A Files-page setting lives in an unshipped `My Games` file; `invalid.toml` holds deliberately invalid values.
- A unit test checks that its TOML has each setting's default (activation-sphere keys excepted).
- Textures in `data/showcase/Textures/ModsConfigFO4_Showcase/` are uncompressed DDS written by `tools/gen_config_spec_showcase_textures.py`: edit the image there and rerun it. Activation sphere assets are copied from the framework's `mod-template`.
- Its renders are the pictures of the [showcase walkthrough](../for-mod-authors/showcase.md) (`tools/gen_docs_renders.py`). A new page gets a row in that script's table and a section in the walkthrough.

**MCM showcase:** `data/showcase/MCM/Config/ModsConfigFO4_McmShowcase/`
- Every MCM row kind in every look, one kind per page, and only what MCM itself supports (the config spec showcase covers what the schema adds).
- `keybinds.json` backs its hotkey rows. `lib.swf` holds the Images page's images, written by `tools/gen_mcm_showcase_lib.py`: edit the image there and rerun it.
