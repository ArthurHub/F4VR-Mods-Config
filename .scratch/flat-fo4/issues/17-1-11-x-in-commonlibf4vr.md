# 1.11.x in CommonLibF4VR

Type: grilling
Status: resolved
Blocked by: 16

## Question

How does CommonLibF4VR take 1.11.x, given [A third flat runtime in CommonLibF4VR](06-third-flat-runtime.md)'s answer?

- which 1.11.x builds: 1.11.240 only, the current Steam build, or all six; a build is supported once its IDs are checked against its Address Library file
- an AE ID defaults to the 1.10.984 ID, with an override where 1.11.x changed it; or every ID written out
- `IsNG()` on 1.11.x: true on both, with `IsAE()` for the few 1.11.x-only places; or strict, with each of its about 10 call sites reviewed
- the override's form: a fourth argument, `RelocationID(f4, ng, vr, ae)` and `VariantID(f4, ng, vrOffset, ae)`
- where the AE IDs come from: read from the Address Library files one by one, and the RTTI tables regenerated with `RTTIDump`, since the Dear-Modding fork and libxse are GPL-3.0 with a modding exception and CommonLibF4VR is MIT
- `RUNTIME_LATEST`: the framework's plugin query rejects a flat runtime below it, so moving it to 1.11.240 would reject 1.10.163; keep it and add `RUNTIME_LATEST_AE`, decided with F302 and F308
- a build with 1.11.x but without 1.10.984 not supported, which keeps C3 out of the 17 headers with VR-only conditions

## Answer

Decided with the user, 2026-11-27.

- 1.11.240 only, the current Steam build; F4SE loads the DLL on the other 1.11.x builds too, unchecked.
- An ID's 1.11.x value is its 1.10.984 one unless a fourth argument overrides it: `RelocationID(f4, ng, vr, ae)`, `VariantID(f4, ng, vrOffset, ae)`.
- `IsNG()` is true on 1.10.984 and 1.11.x, with `IsAE()` for the few 1.11.x-only places.
- The 1.11.x IDs are read from 1.11.240's Address Library file one by one, and the RTTI tables regenerated with `RTTIDump`; nothing is copied from the GPL forks.
- `RUNTIME_LATEST` stays 1.10.984, and `RUNTIME_LATEST_AE` is 1.11.240; the framework's query accepts the flat runtimes the build has.
- A build with 1.11.x needs 1.10.984 too (`ENABLE_FALLOUT_AE` requires `ENABLE_FALLOUT_NG`), which keeps C3 out of the 17 headers with VR-only conditions.
