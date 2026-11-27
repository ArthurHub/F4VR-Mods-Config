# Map: Flat Fallout 4

## Destination

[flat-fallout4.md](../../docs/design/flat-fallout4.md) with nothing left to decide: every commit through the release is specified well enough to make without a further decision. Making the commits stays outside this map, one at a time, as the plan's "How the work is done" has it.

## Notes

- The plan is [docs/design/flat-fallout4.md](../../docs/design/flat-fallout4.md); the words are [GLOSSARY.md](../../GLOSSARY.md)'s (runtime, flat Fallout 4, flat build / VR build).
- A ticket holds its finding and the reasoning. The plan gets only the resulting plan, in the design docs' style: the need, the reason, the smallest change, no history.
- Skills: `fallout4-feature` for flat Fallout 4, `f4vr-feature` for VR, `re-lookup` for reverse engineering. An address of one runtime is not valid for another without a mapping.
- Research findings go in [research/](research/), one file per ticket. Nothing in this map is committed, branched or pushed without the user's go-ahead.
- Tickets: [issues/](issues/), one file each, with `Type:`, `Status:` (`open`, `claimed`, `resolved`) and `Blocked by:` lines.

## Decisions so far

- [Flat drawing host](issues/01-flat-drawing-host.md): a hidden menu's render condition and `CursorMenu`'s slot 7 draw in the UI pass, DearModdingUI's way; F305 swaps only the world quad for a screen rectangle; needs 1.10.984 IDs CommonLibF4VR lacks (C6)
- [Frame tick on flat](issues/02-frame-tick-on-flat.md): a call in `Main::OnIdle`'s game-data-loaded block, VR's place; once a frame on the main thread, in menus and paused, before the frame draws; the IDs go in CommonLibF4VR
- [Input on flat](issues/03-input-on-flat.md): cursor from `MenuCursor`, a non-pausing cursor menu of the framework's own, buttons, wheel and Esc from the input queue with `kStop`, keys and text from the window as in VR; whether that keeps the player still is untried
- [Pause-menu row](issues/04-pause-menu-row.md): a row spliced into `MainMenu.swf`'s list as `PauseMenu` opens, DearModdingUI's way, beside MCM's; needs a CommonLibF4VR commit (C5) for `GetElement` and `HasMember`'s 1.10.984 ID
- [A third flat runtime in CommonLibF4VR](issues/06-third-flat-runtime.md): `Runtime::AE` with IDs defaulting to 1.10.984's (88% carry over), about 250 lines for C3 and half a day for C4; three singletons are wrong on 1.10.984 today and join C6
- [MCM `SetKeybind` on flat](issues/05-mcm-setkeybind-on-flat.md): only through `root.mcm` in the pause menu's movie while it is open; queue and write `Keybinds.json` otherwise, and push only after a save has loaded

## Not yet specified

- Whatever 1.10.163 breaks: only visible once it is tried.
- The release docs and the flat Nexus page: they depend on everything above.

## Out of scope

- Gamepad support: decided after the first release.
- World-space drawing on flat (debug draw, vrui panels): flat gets only the screen canvas.
- The framework's other mods on flat, beyond what Mods Config and its test client need.
