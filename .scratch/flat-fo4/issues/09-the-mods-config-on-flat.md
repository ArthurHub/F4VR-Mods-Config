# The mod's config on flat

Type: grilling
Status: resolved

## Question

What config does the flat build ship, and what does the menu's own config spec show on flat?

- `data/config/ModsConfigFO4VR.toml` has VR sections (`[Panel]` placement in the world, `[Pointer]`) that mean nothing on flat, and flat has keys VR lacks (the open key, the panel's share of the screen's height)
- the menu's own config spec, `data/mod/ModsConfigFO4/Config/ModsConfigFO4/spec.json`, shows those settings
- one file and one config spec for both, or each build its own

## Answer

Decided with the user, 2026-11-27.

Each build ships its own: flat has `ModsConfigFO4.toml` and its own config spec with only flat settings (F10, the panel's share of the screen's height); the sections both have (`[Developer]`, `[Debug]`) are in both. Each package carries its own, and the file name already follows the build's name.
