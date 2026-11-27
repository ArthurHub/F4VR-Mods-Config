# A third flat runtime in CommonLibF4VR: findings

Ticket: [06-third-flat-runtime.md](../issues/06-third-flat-runtime.md). Researched 2026-11-27.

Confidence tags: **[H]** checked in source code or binary data, **[M]** follows from several consistent sources, **[L]** one indirect source.

## Short answer

1.11.x ("anniversary edition" in F4SE's source; "AE" in the other libraries) keeps 1.10.984's Address Library IDs for most code, so the smallest design is **a fourth `Runtime` value, `AE`, whose ID defaults to the NG ID, with an AE override only where 1.11.x gave the address a new ID.**

- **C3, the runtime machinery: medium, about 1–1.5 days.** Around 250 lines in about 12 files:
  - `Runtime::AE`, with `load_version` mapping 1.11.x to it. Today it maps 1.11.x to NG.
  - The `ENABLE_FALLOUT_AE` option, and the `FALLOUT_REL`/`GetRuntime` conditions taught a fourth runtime.
  - An AE slot in `RelocationID`, `VariantID` and `VariantOffset` that defaults to the NG value, plus a 4-argument form for the override.
  - AE handled in `Relocate`.
  - The NG-only VM ABI branches made to include AE.
  - The flat ID lookup made exact, and the known bad 1.11.x bins refused.
  - The six 1.11.x `RUNTIME_` constants, F4SE's `1 << 2` address-library and layout flags, and CMake presets.
  - The bin's file name and format need no change.
- **C4, the IDs per address: small, about half a day once the flat address list is final.**
  - A hand-written ID needs an AE value only when 1.11.x dropped its NG ID.
  - In the whole library that is **30 of 474** hand-written `RelocationID`/`VariantID` sites: 27 globals (singletons, form maps, event sources) and 3 functions (`MemoryManager::GetSingleton`, `BSStringT::Set`, GFx `ObjectInterface::GetMember`).
  - 29 of those 30 have an AE ID in the Dear-Modding fork, and every one of those 29 is present in both the 1.11.191 and the 1.11.221 bins.
  - The rest carry over unchanged: 244 of the 273 hand-written `RelocationID`s that have an NG ID (~89%), and 88% of all NG code IDs.
  - Of the 11 flat singletons the framework uses today, 7 need an AE ID. Separately, 3 have only an OG ID and need an NG ID first, because they are wrong on 1.10.984 today (see [Open points](#open-points-that-need-a-decision)).

Nothing upstream to merge: alandtse's `master` has no 1.11.x work, and its unmerged `clean-upstream-structure` branch uses a different layout and gets the AE fallback wrong.

## Evidence

### 1. The 1.11.x builds and their F4SE

| Runtime | F4SE | F4SE tag date | Notes |
|---|---|---|---|
| 1.11.137 | 0.7.4 | 2025-11-11 | "'anniversary edition' update". F4SE 0.7.4 still accepts the 1.10.980 address-library flag |
| 1.11.159 | 0.7.5 | 2025-11-18 | whatsnew: "address library requires a new version for the 1.11+ series of games, so plugins will need to be recompiled" |
| 1.11.169 | 0.7.6 | 2025-11-25 | |
| 1.11.191 | 0.7.7 | 2025-12-16 | |
| 1.11.221 | 0.7.8 | 2026-05-26 | |
| 1.11.240 | 0.7.9 | 2026-08-16 | **current**: `CURRENT_RELEASE_RUNTIME = RUNTIME_VERSION_1_11_240`, "creation club" |

- [H] The builds and the F4SE pairing come from `ianpatt/f4se`: `f4se_common/f4se_version.h` (`RUNTIME_VERSION_1_11_137` … `_240`), `f4se_whatsnew.txt`, and the `v0.7.x` tags (dates from `gh api`). There is no 1.11.x build beyond these six.
- [H] f4se.silverlock.org lists "Fallout 4 runtime 1.11.240 - build: 0.7.9" as the latest, beside 0.7.2 (1.10.984) and 0.6.23 (1.10.163).
- [H] The Steam copy on this machine is `Fallout4.exe` 1.11.240.0. `Fallout 4 - NG` beside it is 1.10.984.0.
- [H] Each F4SE build runs only on its own runtime. libxse/commonlibf4's `RUNTIME_LATEST` is 1.11.240 (commit c191ee77, 2026-08-22).
- [M] F4SE commit `4cb4566b` (2026-08-15) is titled "first pass address update for 1.11.420". Read it as a typo for 240: the tag that follows it is 0.7.9 for 1.11.240.

### 2. The 1.11.x Address Library

**File and format** [H]
- The file is `Data\F4SE\Plugins\version-1-11-<build>-0.bin`. F4SE's `PluginManager::CheckAddressLibrary` builds the name as `version-%d-%d-%d-%d.bin` with 0 as the last part.
- The format is the same plain table as 1.10.984: a `u64` count, then sorted `{u64 id, u64 offset}` pairs.
- Checked on `version-1-11-191-0.bin` and `version-1-11-221-0.bin` in `Modding-Reference\RE\github-repos\BethesdaGhidraScripts\addresslibrary\f4`: sorted, no trailing bytes.
- CommonLibF4VR's `IDDB` already reads this format and builds the path from the exe version. It loads the file unchanged.

**IDs: a continuation of 1.10.984's set, not a new one** [H]. Compared directly with a script over the bins:

| | 1.10.984 | 1.11.191 | 1.11.221 |
|---|---|---|---|
| IDs | 594,329 | 651,571 | 651,581 |
| highest ID | 3,640,324 | 8,717,431 | 10,260,203 |

- 383,582 IDs are in both 1.10.984 and 1.11.191. 1.11.191 dropped 210,747 of 1.10.984's IDs and added 267,989 new ones, all above 1.10.984's highest (4,465,011 to 8,717,431).
- By the section of their 1.10.984 offset (sections read from the local 1.10.984 exe):

  | Section | NG IDs dropped by 1.11.x |
  |---|---|
  | `.text` (code) | 12% (21,497 of 173,457) |
  | `.rdata` | 45% |
  | `.data` | 45% |

  So a function mostly keeps its ID, and globals, RTTI descriptors and the like get a new one. This matches what the forks carry: `UI::RegisterMenu` is 2284766 on NG and AE, while the `UI` singleton is 2689028 on NG and 4796314 on AE.
- Spot checks: 2707105 (`UI::GetMenuMapRWLock`), 2284766, 2704429 (`GetRendererData`) and 245579 (`VTABLE::CursorMenu`) are in all three bins. 2689028 and 2689091 (the `UI` and `UIMessageQueue` singletons) are in 1.10.984 only, and 4796314 and 4796377 are in 1.11.x only.
- [H] **The 1.11.x builds don't share one identical set either.**
  - 1.11.191 → 1.11.221: 55,978 IDs dropped and 55,988 new (all ≥ 10,071,426). Only 20 of the dropped ones are NG-era IDs.
  - In practice the IDs people use are stable. All 10,920 AE IDs in the Dear-Modding fork are in both bins, and 13,709 of libxse's 13,736 are.
  - The few libxse misses are templated-class RTTI entries minted in 1.11.240 (IDs around 11,947,000, e.g. `CleanupQueue<…>` RTTI). So each supported 1.11.x build's bin needs checking.
- [H] F4SE added `kAddressIndependence_AddressLibrary_1_11_137 = 1 << 2` because the data IDs changed: a plugin built against NG's IDs reads the wrong globals on 1.11.x (whatsnew 0.7.5, quoted above).
- [L] The Nexus page (mod 47327) refuses automated fetches (403), so its author and changelog aren't confirmed here.
  - Search snippets show a 1.11.137 file uploaded 13 Nov 2025, and other mods requiring `version-1-11-221-0.bin`.
  - BethesdaGhidraScripts' analysis says OG, NG and AE "share the meh321 ID namespace".

**Bad bins** [H]
- libxse/commonlib-shared refuses two published 1.11.x bins by SHA-512 (commit 642bb8e0, 2026-08-22): one for 1.11.159 and one for 1.11.240. This is like the unsorted 1.10.980 bin that CommonLibF4VR already refuses (`src/REL/IDDB.cpp`).
- The Dear-Modding fork carries the same list.

### 3. F4SE's version data for 1.11.x

[H] From `f4se/PluginAPI.h` and `f4se/PluginManager.cpp` (commit 5d53dd1e, 2025-11-17, PR #32 by shad0wshayd3):

```cpp
kAddressIndependence_AddressLibrary_1_10_980 = 1 << 1,
kAddressIndependence_AddressLibrary_1_11_137 = 1 << 2,   // new
kStructureIndependence_1_10_980Layout        = 1 << 1,
kStructureIndependence_1_11_137Layout        = 1 << 2,   // new
```

- From F4SE 0.7.5 on, a plugin loads on any 1.11.x runtime without listing it only if `addressIndependence` has `Signatures | 1<<2` **and** `structureIndependence` has `NoStructs | 1<<2`. Otherwise `compatibleVersions[16]` must list the exact runtime.
- With the `1<<2` address flag set, F4SE also requires the `version-…bin` file to exist.
- F4SE 0.7.4 (1.11.137) and 0.7.2 (1.10.984) check `1 << 1` instead. 0.6.23 (1.10.163) uses `F4SEPlugin_Query`.
- **So the flat plugin sets both bits in both fields**, and its `F4SEPlugin_Version` then loads on 1.10.984 and every 1.11.x build.
- CommonLibF4VR's `PluginVersionData` (`include/F4SE/Interfaces.h`) only has setters for `1 << 1`: `UsesAddressLibrary` and `IsLayoutDependent`.
- The Dear-Modding fork adds `UsesAddressLibraryNG/AE` and `IsLayoutDependentNG/AE`, and its plugin template sets both (`res/commonlibf4-plugin.cpp.in`).
- libxse moved `UsesAddressLibrary` to `1 << 2` only, which drops NG.

### 4. CommonLibF4VR today, and what a third flat runtime changes

| Where | Now | Change for 1.11.x |
|---|---|---|
| `include/REL/Module.h`, `src/REL/Module.cpp` | `Runtime { NG=1, F4=2, VR=4 }`. `load_version`: minor 2 → VR, else build > 163 → NG. **1.11.x is detected as NG today** [H] | `AE = 1 << 3`, minor 11 → AE, `IsAE()`. Decide what `IsNG()` means for AE (open point 3) |
| `include/REL/Common.h` | `FALLOUT_REL`, `FALLOUT_ADDR` and `FALLOUT_REL_CONSTEXPR` are chosen by testing pairs of the three macros | Count four. An NG+AE build is multi-runtime and has two ID families |
| `Module::GetRuntime()` | constant for single-runtime builds, `_runtime` otherwise | the NG-only branch also needs `!AE` |
| `include/REL/ID.h` | `RelocationID(f4, ng)` (VR takes f4), `RelocationID(f4, ng, vr)`, `VariantID(f4, ng, vrOffset)`. `id()`/`offset()` switch on runtime | `_aeID` set from `ng` by the existing constructors. A 4-argument form `(f4, ng, vr, ae)` for the override. An AE case in the switch |
| `include/REL/Offset.h` | `VariantOffset(f4, ng, vr)` (VR use only) | AE takes the NG offset |
| `include/REL/Relocation.h` | `Relocate(f4AndVR, ng)`, `Relocate(f4, ng, vr)`. `RelocateVirtual`/`RelocateMember` select on `IsVR()` only | AE returns the NG value. `RelocateVirtual`/`RelocateMember` need nothing |
| `src/REL/IDDB.cpp` | flat lookup is `lower_bound` with **no exact-match check** (only VR checks `it->id != a_id`) | fail on a missing ID. Refuse the 1.11.159 and 1.11.240 bad-bin SHA-512s |
| `include/RE/Bethesda/BSScriptUtil.h`, `BSScript/IVirtualMachine.h`, `FunctionArgs.h` | the newer VM `std::function` ABI is chosen by `ENABLE_FALLOUT_NG` / `IsNG()` (about 10 sites) | AE uses the newer ABI. Include AE, or let `IsNG()` be true on AE |
| `include/F4SE/Version.h` | latest is `RUNTIME_1_10_984`, `RUNTIME_LATEST = 1_10_984` | `RUNTIME_1_11_137/159/169/191/221/240`. Bumping `RUNTIME_LATEST` is open point 7 |
| `include/F4SE/Interfaces.h` | `1 << 1` setters only | `UsesAddressLibraryAE`, `IsLayoutDependentAE` (`1 << 2`) |
| `CommonLibF4/CMakeLists.txt`, `CMakePresets.json`, `xmake.lua` | `ENABLE_FALLOUT_F4/NG/VR`; presets `runtimes-flat` = F4+NG | `ENABLE_FALLOUT_AE`; `runtimes-flat` adds AE. CI is C2's |

Notes on the table:
- [H] **The inexact lookup is a trap that 1.11.x makes worse.**
  - A missing ID silently resolves to the next-higher ID's offset. On 1.11.x, 35% of 1.10.984's IDs are missing.
  - libxse fixed the same bug in commonlib-shared (a600abbf, 2026-10-27): "lower_bound returned the next-higher id's offset when the id was missing, so the call silently went to an unrelated address."
  - The Dear-Modding fork fails with "Failed to find offset for Address Library ID".
  - The plan's new C5 is an instance of it on 1.10.984 already (`HasMember` with only its 1.10.163 ID).
- [H] 17 RE headers test the runtime macros, mostly in the VR-only form `defined(ENABLE_FALLOUT_VR) && !defined(ENABLE_FALLOUT_NG) && !defined(ENABLE_FALLOUT_F4)`. These stay right if AE is allowed only beside NG: an `#error` for AE without NG. That keeps all of them out of C3 except the VM ABI ones listed in the table.

**Existing call sites** [H] (a regex count over `include/` and `src/`; NG IDs checked against the bins):

| Kind | Sites | NG ID kept in 1.11.x | NG ID dropped in 1.11.x | NG = 0 or not an NG ID |
|---|---|---|---|---|
| hand-written `RelocationID` | 333 | 244 | **29** | 60 |
| hand-written `VariantID` | 141 | 13 | **1** | 127 |
| hand-written single `REL::ID` | 459 | 13 | 0 | 446 are not in the 1.10.984 bin at all (OG/VR IDs) |
| generated `RTTI_IDs.h` | 12,906 | 0 | 11,776 | 1,130 |
| generated `NiRTTI_IDs.h` | 347 | 319 | 28 | 0 |
| generated `VTABLE_IDs.h` | 12,631 | 10,896 | 743 | 992 |

- With "AE defaults to NG", only the 30 sites listed below need a fourth value. With "AE explicit everywhere", all 474 hand-written `RelocationID`/`VariantID` sites and the 25,884 generated entries would. Of the generated ones, 12,547 have a dropped NG ID, and the Dear-Modding fork gives an AE ID for 10,777 of those.
- The 30 sites are hand-written `RelocationID`s, unless marked otherwise. AE values are taken from the Dear-Modding fork by NG ID, and each one is present in both the 1.11.191 and 1.11.221 bins:
  - Singletons and globals:
    - `ActorEquipManager` 2690994 → 4798287
    - `BGSDefaultObjectManager` array 2690473 → 4797762
    - `BGSInventoryInterface` 2689299 → 4796586
    - `BGSStoryEventManager` 2693504 → 4800796
    - `BGSSynchronizedAnimationManager` 2690996 → 4798289
    - `BSTGlobalEvent` 2688814 → 4796078
    - `Calendar` 2689092 → 4796378
    - `ConsoleLog` 2690148 → 4797437
    - `ControlMap` 2692014 → 4799307
    - two `Events.h` event sources: 2696196 → 4803487 and 2696280 → 4803571
    - `FormFactory` factories 2689177 → 4796464
    - `GameVM` 2689134 → 4796420
    - `DynamicNavmesh` 2690147 → 4797436
    - `PlayerCharacter` 2690919 → 4798212
    - `PlayerControls` 2692013 → 4799306
    - `ProcessLists` 2688869 → 4796160
    - `GameSettingCollection` 2690301 → 4797590
    - `PlayerCamera` 2688801 → 4796065
    - `TESDataHandler` 2688883 → 4796135
    - the `allForms`, `allFormsByEditorID` maps and their locks: 2689178/9, 2689189/90 → 4796465/6, 4796476/7
    - `UI` 2689028 → 4796314
    - `UIMessageQueue` 2689091 → 4796377
  - Functions:
    - `MemoryManager::GetSingleton` 2193197 → 4471522. Every `RE::malloc` path uses it.
    - `BSStringT::Set` 2189084 → 4471693
    - GFx `ObjectInterface::GetMember` 2285936 → 4494126. M303's pause-menu row uses it.
  - No AE value found: the `VariantID` for the `TESInitScriptEvent` source, 2691330.

### 5. How others carry three flat runtimes

**Dear-Modding-FO4/commonlibf4** (used by DearModdingUI, vendored by F4SEMenuFramework) [H]

Read at `981fb96`, with its `lib/commonlib-shared` from libxse.
- `COMMONLIB_RUNTIMECOUNT=3`. `REX::FModule::GetRuntimeIndex()` returns `kOG/kNG/kAE` from exact version lists:
  - OG: 1.10.163
  - NG: 1.10.980, 1.10.984
  - AE: 1.11.137, 159, 169, 191, 221, 240
  - Anything else fails with "Unsupported Fallout 4 runtime".
- `REL::ID{…}` and `REL::VariantID{…}` take up to N values and **repeat the last one**: a two-value `{OG, NG}` gives AE the NG ID. That is the same default as recommended here.
- `include/RE/IDs.h` holds 1,789 entries: 16 with one value, 1,632 with two, and 140 with three (115 of them with a distinct AE value, mostly `Singleton` and `GetEventSource`).
- The RTTI and NiRTTI files carry three values throughout. Vtables are mostly one ID for all runtimes.
- Layout drift is handled where it is known: `UI::GetRuntimeData()` adds `BSTimer::GetRuntimeSize(runtime)`. AE's `BSTimer` is 0x50, against 0x40 on OG and NG.
- Lookup is exact. Bad bins are refused. The F4SE flags are set for both NG and AE.
- License: GPL-3.0 with a modding exception. CommonLibF4VR is MIT.

**F4SEMenuFramework** [H] (Modding-Reference analysis)
- One DLL for OG, NG and 1.11.x on the Dear-Modding fork. It avoids Address Library IDs almost entirely: 3 `REL::ID` sites, with everything else through typed singletons, vtable slots and import-table patching by name.
- Its one 1.11.x ID, 4492363, is noted as "verified present in every 1.11.x bin". The NG slot is a never-queried 0, because "REL aborts the process on a missing-ID lookup".

**libxse/commonlibf4** (shad0wshayd3, qudix; the base of the Dear-Modding fork) [H]
- Single-runtime: one `REL::ID` per entry, kept on the latest build.
- It moved to 1.11.137 in Nov 2025. The bulk of that was regenerated RTTI, NiRTTI and vtable files (765b0771: +12,281/−11,776 lines in `IDs_RTTI.h`), plus a few dozen singleton IDs in `IDs.h` (5df69170, 00183877, f454fb6a).
- Each later 1.11.x build only bumped `Version.h` (c0070c2c, 7a6c9c0b, 92b65d25, c84949a8, c191ee77), with no ID changes.

**alandtse/CommonLibF4** (the fork's upstream; local `upstream` is current at 15f504ea, 2026-11-08) [H]
- `master` has no 1.11.x anything.
- No issue or PR mentions 1.11, AE or anniversary. The open ones are #59 (Scaleform refcount), #58 (Papyrus), #2 (Update IDs) and #1 (Port f4sevr).
- The branch `clean-upstream-structure` (last commit 2026-01-22) adds "AE" as a fourth runtime on a libxse-style layout (`include/REL/F4/Runtime.h`, `RuntimeIndex4`). Its `AE_IMPLEMENTATION_SUMMARY.md` notes:
  - it was written while waiting for an AE Address Library ("Monitor for AE Address Library release on Nexus Mods")
  - its two-value IDs give AE the **pre-NG** ID, which the bins show is wrong: AE follows NG
- It shares no history with `master` and isn't merged. It isn't a base for C3, only a sign that upstream has thought about it and has nothing usable.

### 6. AE layout changes

[H] AE's `BSTimer` grew from 0x40 to 0x50 (libxse `include/RE/B/BSTimer.h`: `unk20[0x10]`, `static_assert(sizeof(BSTimer) == 0x50)`). Every struct after an embedded `BSTimer` shifts by 0x10 on 1.11.x:
- `UI` grows from 0x250 to 0x260: `menuSystemVisible`, `closingAllMenus` and `freezeFrameScreenshotReady` move.
- `GameScript::Profiler` (inside `GameVM` at 0x4F0) also shifts. `GameVM::impl` at 0xB0 is before it and doesn't move.

The framework and the mod read none of the moved fields today (grep). F4SE's separate `kStructureIndependence_1_11_137Layout` flag says layouts did change. No full list of AE layout changes exists beyond what libxse and the Dear-Modding fork carry. [M]

### 7. The framework's and the mod's addresses on flat

[H] The framework and the mod name no Address Library ID directly: the only one is `VRHookSites::MainUpdateFrameTick`, which is VR-only. Every flat address comes through CommonLibF4VR's accessors. The singletons they use today:

| Accessor | CommonLibF4VR ID (OG, NG) | NG ID in 1.11.x? | AE ID (Dear-Modding fork) |
|---|---|---|---|
| `UI` | 548587, 2689028 | no | 4796314 |
| `TESDataHandler` | 711558, 2688883 | no | 4796135 |
| `GameVM` | 996227, 2689134 | no | 4796420 |
| `UIMessageQueue` | 82123, 2689091 | no | 4796377 |
| `PlayerCamera` | 1171980, 2688801 | no | 4796065 |
| `PlayerCharacter` | 303410, 2690919 | no | 4798212 |
| `INISettingCollection` | 791183, 2704108 | yes | = NG |
| `INIPrefSettingCollection` | 767844, 2703234 | yes | = NG |
| `BSGraphics::RendererData` | **`REL::ID(1235449)`, OG only** | | NG 2704429 (kept in 1.11.x) |
| `BSScaleformManager` | **`REL::ID(106578)`, OG only** | | NG 2689600, AE 4796889 |
| `TaskQueueInterface` | **`REL::ID(7491)`, OG only** | | NG 2698331 (kept in 1.11.x) |
| `VRWorldData` | VR offset | | VR-only |

The last three are wrong on 1.10.984 as well (see open point 8).

## Estimate

**C3: runtime machinery, about 1–1.5 days**, all in CommonLibF4VR (§4).
- `Module` (runtime enum, detection, `IsAE`), `Common.h` and the `GetRuntime` conditions: about 60 lines.
- `ID.h`/`Offset.h`/`Relocation.h` AE slots defaulting to NG, and a 4-argument override: about 80 lines.
- `IDDB` exact lookup and the 1.11.159/1.11.240 SHA-512s: about 20 lines.
- VM ABI branches: about 10 sites.
- `Version.h` and the `Interfaces.h` flags: about 15 lines.
- CMake, presets and xmake: about 20 lines.
- Then building flat (F4+NG+AE) and VR, and loading in 1.11.240 with F4SE 0.7.9 and `version-1-11-240-0.bin`. The `runtimes-flat` preset changes, which C2's CI follows.

**C4: IDs per address, about half a day** for the few dozen accessors milestones 1–3 end up using.
- Per address: if the NG ID is in each supported 1.11.x bin, nothing changes (88% of NG code IDs are kept). If not, take the AE ID from the Dear-Modding fork (by NG ID) or libxse (by name), check it against each supported bin, and try it in the game.
- A small script does the bin checks. The ones written for this research may be cleared with the session's temp folder:
  - `C:\Users\tepar\AppData\Local\Temp\claude\1e2c4480219c46a0\7b402446-3863-4898-af8d-b7a861770e17\scratchpad\probe_ids.py` checks given IDs in each bin.
  - `C:\Users\tepar\AppData\Local\Temp\claude\1e2c4480219c46a0\7b402446-3863-4898-af8d-b7a861770e17\scratchpad\map_ae_ids.py` and `C:\Users\tepar\AppData\Local\Temp\claude\1e2c4480219c46a0\7b402446-3863-4898-af8d-b7a861770e17\scratchpad\list_dropped.py` map dropped NG IDs to AE.
  - `C:\Users\tepar\AppData\Local\Temp\claude\1e2c4480219c46a0\7b402446-3863-4898-af8d-b7a861770e17\scratchpad\compare_bins.py` compares the bins' ID sets.
- An address with no AE ID anywhere costs about 0.5–2 h in Ghidra on the 1.11.240 exe.
- Library-wide alternative:
  - Fill AE values for all 30 hand-written sites and the 10,777 generated RTTI, NiRTTI and vtable entries that map, by script from the Dear-Modding fork: about half a day.
  - Or regenerate the RTTI files with the repo's `RTTIDump` plugin in 1.11.x: about half a day plus merging.
  - The flat build doesn't need the RTTI IDs for what it does today: `TESForm::As<>` switches on form type, not RTTI.

## Open points that need a decision

1. **Which 1.11.x builds.**
   - (a) 1.11.240 only, the current Steam build: one bin to check, and F4SE's flags let the DLL load on the others anyway.
   - (b) All six, as the Dear-Modding fork does: every AE ID has to be checked in six bins. Only 1.11.191 and 1.11.221 are on this machine; 137, 159, 169 and 240 come from Nexus.
   - IDs churn a little between 1.11.x builds (§2), so "supported" has to mean "checked against that bin".
2. **An AE ID's default: the NG ID (recommended), or explicit everywhere.** The bins support the NG default. Explicit means touching every one of the 474 hand-written sites and 25,884 generated entries.
3. **What `IsNG()` means on 1.11.x.**
   - It is used for the newer VM ABI (true on AE too) and in `RelocationID`/`VariantID` switches.
   - (a) `IsNG()` true on NG and AE, with `IsAE()` for the few AE-only spots: fewest edits, but `GetRuntime() == NG` comparisons need care.
   - (b) `IsNG()` strict, and each call site reviewed (about 10).
4. **The override's form.**
   - `RelocationID(f4, ng, vr, ae)` and `VariantID(f4, ng, vrOffset, ae)` keep the existing 2- and 3-argument meanings.
   - The Dear-Modding `{OG, NG, AE}` brace form can't be used, because the fork's 3-argument form is `(f4, ng, vr)`.
5. **Where the AE IDs come from.**
   - The Dear-Modding fork and libxse are GPL-3.0 with a modding exception. CommonLibF4VR and its upstream are MIT.
   - The 30 hand-written values are facts read from the bins and easy to recheck one by one. Copying the generated RTTI tables wholesale is the part with a license question.
   - Regenerating them with `RTTIDump` in 1.11.x avoids that question.
6. **Exact flat lookup changes behaviour for NG too.** Any of the 446 OG/VR-only single `REL::ID`s that the flat build reaches on 1.10.984 stops silently using a wrong address and fails with a message instead. That is wanted, but it is a change in C3 that milestone 1's testing will see.
7. **`RUNTIME_LATEST`.** libxse moved it to 1.11.240. Here `ModBase::onF4SEPluginQuery` rejects a flat runtime below it (`skse->RuntimeVersion() < (IsF4() ? RUNTIME_LATEST : RUNTIME_LATEST_VR)`), so bumping it would reject 1.10.163. Either keep it, or add `RUNTIME_LATEST_AE` and let F302/F308 decide the check.
8. **Found outside this ticket: three singletons the framework uses are wrong on 1.10.984 today.**
   - `BSGraphics::RendererData::GetSingleton` (`REL::ID(1235449)`), `BSScaleformManager::GetSingleton` (`REL::ID(106578)`) and `TaskQueueInterface::GetSingleton` (`REL::ID(7491)`) have only their OG IDs.
   - None of the three is in the 1.10.984 bin, and upstream `master` has the same lines.
   - With the inexact lookup, F304's renderer data and the Scaleform/task-queue paths read a wrong address on 1.10.984.
   - This is the same kind of fix as C5 (`HasMember`): NG IDs 2704429, 2689600 and 2698331. It belongs to milestone 1–2, not to C3/C4.
9. **Builds with AE but without NG (AE-only, VR+AE): not supported?** Allowing them means rewriting the VR-only conditions in 17 RE headers. Requiring NG with AE keeps C3 out of them.

## Sources

- CommonLibF4VR (`external/F4VR-CommonFramework/external/CommonLibF4VR`, branch `flat-fo4`, ad7805e5): `include/REL/{Common,Module,ID,IDDB,Offset,Relocation}.h`, `src/REL/{Module,IDDB}.cpp`, `include/F4SE/{Version,Interfaces}.h`, `CMakePresets.json`, `RTTIDump/`, `VRAddressCheck/`. `upstream/*` branches via `git log` and `git show`.
- alandtse/CommonLibF4 issues and PRs via `gh issue list` / `gh pr list` (all states).
- ianpatt/f4se via `gh api`: `f4se_common/f4se_version.h`, `f4se/PluginAPI.h`, `f4se/PluginManager.cpp` (at `master`, `v0.7.2`, `v0.7.4`, `v0.7.5`), `f4se_whatsnew.txt`, commit 5d53dd1e, tags `v0.7.2`–`v0.7.9`. f4se.silverlock.org.
- libxse/commonlibf4 (shallow clone at 5bf0ee91, 2026-11-17, with `lib/commonlib-shared` at 29fbdb0e), and commit history via `gh api`. libxse/commonlib-shared commits 642bb8e0 and a600abbf.
- Dear-Modding-FO4/commonlibf4 at 981fb96 (`Modding-Reference\Fallout4\github-repos\gold\DearModdingUI\Depends\commonlibf4`): `lib/commonlib-shared/include/REL/{ID,VariantID,IDDB}.h`, `src/REL/IDDB.cpp`, `src/REX/FModule.cpp`, `include/RE/IDs*.h`, `include/RE/U/UI.h`, `res/commonlibf4-plugin.cpp.in`.
- Modding-Reference: `Fallout4\Analysis\gold\DearModdingUI_RE_REFERENCE.md` (Runtime Notes), `F4SEMenuFramework_RE_REFERENCE.md`, `RE\Analysis\BethesdaGhidraScripts_ANALYSIS.md`, and the bins in `RE\github-repos\BethesdaGhidraScripts\addresslibrary\f4\` (1.10.163, 1.10.980, 1.10.984, 1.11.191, 1.11.221).
- The local game: `C:\Games\steamapps\steamapps\common\Fallout 4\Fallout4.exe` (1.11.240.0) and `...\Fallout 4 - NG\Fallout4.exe` (1.10.984.0; its PE section table was used to classify IDs).
- Nexus (mods 47327 and 42147) returned 403 to automated fetches. Search results and a Steam discussion (steamcommunity.com/app/377160/discussions/0/685238726928893036) were used only for [L] facts.
