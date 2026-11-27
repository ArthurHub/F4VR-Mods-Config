# The flat drawing host's choices

Type: grilling
Status: resolved
Blocked by: none

## Question

What does F304 do where [Flat drawing host](01-flat-drawing-host.md) left a choice?

- the hidden menu without a movie, tried first in 1.10.984, with DearModdingUI's hidden `CursorMenu.swf` movie as the fallback; or DearModdingUI's shape outright
- where the ImGui frame is built: on the frame tick, as in VR, which runs while the pause menu is up ([Frame tick on flat](02-frame-tick-on-flat.md)), to be confirmed in the game; or in the UI pass, as DearModdingUI does
- another mod's panel drawn in the same pass (DearModdingUI's): which one is on top follows load and stack order, and that is accepted

## Answer

Decided with the user, 2026-11-27.

- The hidden menu takes DearModdingUI's shape: it loads the game's `CursorMenu.swf` and hides it, proven on all three flat runtimes.
- The ImGui frame is built on the frame tick, as in VR, and the UI pass draws the latest one: one path for both builds.
- Another mod's panel in the same pass (DearModdingUI's) stacks by load order, and that is accepted.
