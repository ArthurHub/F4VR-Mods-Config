# The flat frame tick's site

Type: grilling
Status: resolved

## Question

Which call does F303 redirect, and how is it found, given [Frame tick on flat](02-frame-tick-on-flat.md)'s answer?

- A1, the call to `DemoteQueuedActors`, VR's place in the frame; or A2, the call to `UpdateQueuedTasks`, whose callee has one caller
- a fixed offset in `Main::OnIdle` per runtime (`VariantOffset(0x210, 0x492)`); or a scan of `OnIdle` for the one call to the callee's ID, so a new 1.11.x build needs only IDs, and the scan is also the check before writing
- the site, its callee and its check in CommonLibF4VR as a flat counterpart of `VRHookSites`, a CommonLibF4VR commit before F303

## Answer

Decided with the user, 2026-11-27.

- The call to `DemoteQueuedActors`, VR's place in the frame: `OnIdle` + 0x210 on 1.10.163, + 0x492 on 1.10.984 and 1.11.240.
- A fixed offset per runtime, its callee checked before writing, as `VRHookSites` does.
- The site, its callee and its check go in CommonLibF4VR as a flat counterpart of `VRHookSites`, a commit before F303.
