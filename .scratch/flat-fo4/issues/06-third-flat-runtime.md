# A third flat runtime in CommonLibF4VR

Type: research
Status: resolved

## Question

What does adding 1.11.x as a third flat runtime to CommonLibF4VR take? For C3 and C4 of the plan:

- `REL::Module`'s runtimes, the ID types (`ID`, `RelocationID`, `VariantID`) and the Address Library loading
- whether 1.11.x's Address Library is a set of IDs of its own, and its file
- F4SE's version data for 1.11.x, and which 1.11.x builds and F4SE versions exist
- how other CommonLibF4 forks and flat plugins carry three flat runtimes, and whether upstream plans it
- how much work it is

## Answer

1.11.x keeps 1.10.984's Address Library IDs for most code (88% of 1.10.984's code IDs carry over; globals and RTTI get new ones), in the same file name and format. So the smallest design is a fourth `Runtime`, `AE`, whose ID defaults to the 1.10.984 ID, with an override only where 1.11.x gave a new one.

- **C3**: medium, about 250 lines in about 12 files: `Runtime::AE` (CommonLibF4VR detects 1.11.x as 1.10.984 today), the `ENABLE_FALLOUT_AE` option and the runtime conditions, an AE slot defaulting to the 1.10.984 value in `RelocationID`, `VariantID` and `VariantOffset`, `Relocate`, the newer VM ABI branches, the six `RUNTIME_1_11_*` constants, F4SE's `1 << 2` address-library and layout flags, CMake, and the two bad published bins (1.11.159, 1.11.240) refused as the unsorted 1.10.980 one is.
- **C4**: small, once the flat build's addresses are final: 30 of 474 hand-written IDs lost their 1.10.984 ID in 1.11.x, mostly singletons; the Dear-Modding fork has 29 of them, each found in the 1.11.191 and 1.11.221 files. 7 of the framework's 11 flat singletons need one.
- The builds are 1.11.137, 159, 169, 191, 221 and 240 (F4SE 0.7.4 to 0.7.9); 1.11.240 is the current Steam build. Upstream has no 1.11.x work.
- **Found outside the ticket**: `RendererData`, `BSScaleformManager` and `TaskQueueInterface`'s singletons have only their 1.10.163 ID, so they read a wrong address on 1.10.984 today (1.10.984 IDs 2704429, 2689600, 2698331). They join C6.

The choices it brings up are [1.11.x in CommonLibF4VR](17-1-11-x-in-commonlibf4vr.md). Findings: [research/06-third-flat-runtime.md](../research/06-third-flat-runtime.md).
