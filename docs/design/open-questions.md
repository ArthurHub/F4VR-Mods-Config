# Open Questions

The design questions that are still open. The order the mod is built in is in the [Mod work plan](mod-plan.md), and the work F4VR-CommonFramework needs in [CommonFramework work required](framework-requirements.md).

## CommonFramework-specific controls

- Grabbing a transform: the player grabs the object the transform places and moves it with the hand to where it should be. The menu needs where the object is in the world, and to turn a place of the hand back into the transform's value, through the node it sits on. A proposal is a callback that, without side effects, gives the world positions of the configured position and of three axis offsets from it in one frame, from which an invertible affine mapping converts back to config coordinates, also for an attachment that moves. The API, what the framework can answer by itself, and a mapping that cannot be inverted are open.
- How the client is told which item of its config spec the ray is on, a row or a group, so it shows in the world what the item sets, as an activation sphere lit while the ray is on its rows. Open: how the event names a row and a group, whether a row inside a group is told as both, and how often it is sent as the ray crosses rows.

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
- Accessibility beyond the panel's scale.
- The menu color of [M147](mod-plan-phase-3.md#15-advanced-features): which presets it has while amber and red mean a pending and an invalid change, and whether a "Match the Pip-Boy color" choice reads the game's own Pip-Boy color and follows it as it changes.

## Profiles, backups and portability

- Back up and restore all mod configurations, or one mod's.
- Files that cannot be restored safely, and how that is explained.
- Global profiles or per-mod profiles.
- Paths that include the player's My Games folder, across machines.
- Restoring over newer external changes.
- An author's presets as profiles shipped beside the config spec.
