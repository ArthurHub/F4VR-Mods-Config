# Frame tick on flat

Type: research
Status: resolved

## Question

What once-a-frame point on the game thread does the framework hook on flat, as `VRHookSites::MainUpdateFrameTick`, a call in the main loop at a VR offset, is in VR? For F303 of the plan:

- what `onFrameUpdate` relies on: every frame, in menus, during loading screens
- candidates with IDs for all three flat runtimes: a call in the main loop, or the hidden menu's per-frame call
- how flat mods in the reference library get their per-frame tick

## Answer

The same place as VR's tick: a call inside `Main::OnIdle`, in the block gated by `QGameDataLoaded`. Read in the disassembly of all three flat executables, it runs once a frame on the main thread, in menus and while paused, before `Main::Swap` draws the frame, and likely not during loading screens, as in VR. VR's own callee there is VR-only code, so flat takes a neighbouring call:

- **A1**: the call to `BGSDynamicPersistenceManager::DemoteQueuedActors`: `OnIdle` + 0x210 on 1.10.163 (ID 633524), + 0x492 on 1.10.984 and 1.11.240 (ID 2228917); callee 727554 / 2193821, checked before writing
- **A2**: the call to `UpdateQueuedTasks` (+0x21C / +0x49E), whose callee has one caller; saves and loads would run inside the framework's thunk
- the existing `write_call<5>` and its `void(uint64_t rcx)` thunk carry over unchanged
- not taken: the hidden menu's render condition runs on another thread during loads; `AdvanceMovie` and F4SE's task pump run after `Swap`; `AddTaskPermanent` doesn't exist in F4SE 0.6.23 for 1.10.163
- CommonLibF4VR has none of these IDs; by the framework's rule they go there, as a flat counterpart of `VRHookSites`, before F303. Its `QGameDataLoaded` lacks the 1.10.163 ID, which C1 brings from upstream.

The choices it brings up are [The flat frame tick's site](18-the-flat-frame-ticks-site.md). Findings: [research/02-frame-tick-on-flat.md](../research/02-frame-tick-on-flat.md).
