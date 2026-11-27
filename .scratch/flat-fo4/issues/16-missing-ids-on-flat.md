# Missing IDs on flat

Type: grilling
Status: resolved

## Question

On flat, CommonLibF4VR resolves an ID the Address Library lacks to the next ID's address, without a word ([Flat drawing host](01-flat-drawing-host.md)); VR checks for an exact match. What does the flat build do about it?

- CommonLibF4VR fails on a missing ID on flat as on VR, so the plugin stops and names the ID the first time it is resolved; this differs from upstream
- or a build-time check of every ID the flat build resolves against the 1.10.163 and 1.10.984 Address Library files
- and whether the 1.10.984 IDs CommonLibF4VR gains go upstream to alandtse/CommonLibF4

## Answer

Decided with the user, 2026-11-27.

- The flat lookup checks for an exact match, as VR's does: the plugin stops and names the ID the first time it is resolved.
- CommonLibF4VR's flat IDs and the exact lookup stay in the fork while the flat build is made, and are offered upstream after the release.
