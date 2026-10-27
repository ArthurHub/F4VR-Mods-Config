# Open Questions

The design questions that are still open. The order the mod is built in is in the [Mod work plan](mod-plan.md), and the work F4VR-CommonFramework needs in [CommonFramework work required](framework-requirements.md).

## CommonFramework-specific controls

- How a transform's In the world tunes a key of another mod: the framework's live tuning tunes only its own mod's keys.
- How the client is told which group of an activation sphere the ray is on, so the sphere is lit in the world.
- How a config spec names a section that is an activation sphere.

## Localization

- Glyphs beyond Western European languages: wider coverage or fallback fonts.
- Placeholders, plurals and runtime values in a text.
- Whether an asset path can vary by language.
- A tool that finds missing and unused tokens.

## Native API

- Whether a host-owned custom panel is needed.
- Pages supplied by native code.

## Papyrus API

- How a color is returned to a script.
- Whether the getters run without waiting (`NoWait`), which needs them thread-safe. MCM's do not.
- How soon a getter sees a file that was changed outside the menu.
- Whether a script mod can treat Mods Config FO4 VR as optional: what happens to a script that calls a missing `ModsConfigFO4` script, checked in the game.
- Whether scripts are told to read their settings again after a save is loaded.
- Whether script mods get live preview: a preview event, and getters that return pending values.
- Whether scripts can change settings.

## UI

- Whether world geometry hides the Full panel, and where the panel goes when a wall is closer than its distance.
- An open editing session on a save load, a loading screen or the player's death: [M83](mod-plan-phase-2.md#10-first-release).
- Accessibility beyond the panel's scale.

## Profiles, backups and portability

- Back up and restore all mod configurations, or one mod's.
- Files that cannot be restored safely, and how that is explained.
- Global profiles or per-mod profiles.
- Paths that include the player's My Games folder, across machines.
- Restoring over newer external changes.
- An author's presets as profiles shipped beside the config spec.
