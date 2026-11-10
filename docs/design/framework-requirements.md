# CommonFramework work required

What F4VR-CommonFramework had to gain for this mod, written against framework 0.4.1 (`4dd511a`). All 13 items are done: the commits are in the [framework plan](framework-plan.md) (items 1–10) and the [mod plan](mod-plan.md) (11–13), and each subsystem's README in the framework documents the result. The full requirements text is at `git show 6a0be38:docs/design/framework-requirements.md`.

The mod's UI is one Dear ImGui panel in a vrui layout, operated with a controller ray, plus vrui buttons on the offhand wrist. That demanded: UI on the offhand facing the player, an interactive ImGui panel, one large sharp panel fixed in the world, and live preview for CommonFramework mods with no code of their own.

| # | Item | Needed for | Result |
| --- | --- | --- | --- |
| 1 | Offhand attachment | The wrist buttons | vrui element rotation; offhand and wrist presets (the wrist follows the forearm bone, falling back to the controller) |
| 2 | Pointer input for ImGui canvases | Any interaction | Wand ray and trigger as ImGui's mouse; one hand owns the pointer |
| 3 | Input capture and suppression | Interaction while the game runs | The pointing wand is hidden from the game |
| 4 | Pointer visuals | Any interaction | `PointerStyle` ray and mark |
| 5 | Scrolling | Long lists | Thumbstick as the mouse wheel |
| 6 | Dedicated panel | Panel size, dropdowns, dialogs | `setDedicated`: a texture of its own as ImGui's display |
| 7 | World-locked placement | The Full panel | `attachPresetToWorldAtHMD`, recenter, then curve and move |
| 8 | Config reload safety | Reliable apply on Save | Serialized loads; own writes recognized by content |
| 9 | Overrides at preview rates | Live preview | Batched overrides on the cached parse; subscribers once a frame on the game thread |
| 10 | Override ownership | Previews beside other overrides | Each override has an owner; last set wins |
| 11 | Images in canvases | MCM and config spec images | Textures from pixels, drawn as ImGui images |
| 12 | Text entry and on-screen keyboard | Text inputs, search | SteamVR's keyboard ([VR keyboard](../../external/F4VR-CommonFramework/docs/tech/vr-keyboard.md)) |
| 13 | Mods Config FO4 VR client in `ModBase` | Own UIs and live preview without mod code | See [CommonFramework adapter](architecture.md#commonframework-adapter) |

## Later work

Made only if a milestone needs it.

- **Fonts.** The canvas font is the embedded Roboto (Latin, Greek, Cyrillic, Vietnamese). Chinese, Japanese, Korean or Arabic text needs a fallback font merged into it.
- **Ray press for vrui buttons**, if vrui buttons end up in the panel, out of finger reach.

## Not framework work

- The opening gesture is an input binding (`primary longpress thumbstick 1.5 +offhand:thumbstick`, FRIK's own), taken from FRIK through `setConfigValueOverride`.
- Haptics are `VRHaptics`; FRIK fingertip pointing is vrui's own.
- Files, config specs, MCM, the Papyrus API and the host side of the native API live in the mod; the framework carries only the client.
