# CommonFramework work required

Mods Config VR is built on F4VR-CommonFramework, and its design assumes capabilities the framework does not have yet. This document lists that work so it can be done in the framework first.

It describes the framework at version 0.4.1 (unreleased), commit `4dd511a`. File paths are relative to the framework repository.

## What the mod needs from the framework

Most of the configuration UI is a single Dear ImGui panel: an `imgui::UIImGuiPanel` in a vrui layout that holds the mod list, the selected mod's settings, and the footer with the help line and the Save bar. The player operates it with a controller ray. A few controls that sit on the hand, such as the Minimized Resume and Close buttons, are ordinary vrui buttons pressed by touch.

That makes four demands on the framework:

- A panel or a row of buttons has to sit on the offhand and face the player.
- An ImGui panel has to be interactive. Today it is display-only.
- One panel has to be large and sharp enough for the whole UI, and stay fixed in the world.
- A CommonFramework mod should get live preview of its settings without writing code for it.

## Order of work

| #   | Item                               | Needed for                          |
| --- | ---------------------------------- | ----------------------------------- |
| 1   | Offhand attachment                 | The Focused and Minimized views     |
| 2   | Pointer input for ImGui canvases   | Any interaction                     |
| 3   | Input capture and suppression      | Any interaction while the game runs |
| 4   | Pointer visuals                    | Any interaction                     |
| 5   | Scrolling                          | Lists longer than the panel         |
| 6   | Dedicated panel                    | Panel size, dropdowns, and dialogs  |
| 7   | World-locked placement             | The Full panel                      |
| 8   | Config reload safety               | Reliable apply on Save              |
| 9   | Overrides at preview rates         | Live preview                        |
| 10  | Override ownership                 | Previews beside other overrides     |
| 11  | Images in canvases                 | MCM images and a config spec's images   |
| 12  | Text entry and on-screen keyboard  | Typing a text, a value, or a search |
| 13  | Mods Config VR client in `ModBase` | Handoff and live preview without mod code |

Item 1 puts a panel on the offhand. Items 2 to 5 make a panel interactive, so it can be tested in the headset. Items 6 and 7 complete what the Full panel needs. Items 8 to 10 do not depend on the UI and can be done at any time. Items 11 and 12 are needed by what the menu shows, not by the panel itself. Item 11 depends on nothing else, so it can be done at any time. Item 13 needs the Mods Config VR native API to exist first. [Later work](#later-work) lists what can wait.

The commits of items 1 to 10 are in the [Framework work plan](framework-plan.md), and those of items 11 to 13 in the [Mod work plan](mod-plan.md), with the mod's milestones that need them.

## Hand-anchored UI

### 1. Offhand attachment

**Need.** The Focused panel and the Minimized buttons sit on the offhand. Focused faces the player when the hand is raised. The Minimized buttons lie along the inner wrist, and the arm must never cover them.

FRIK is not required, so both are attached to the offhand controller. When an arm is drawn at the controller, as with FRIK, the Minimized buttons follow the forearm. The forearm's direction comes from the elbow, so a fixed offset from the controller can put the buttons inside the arm.

**Today.** `UIManager::attachElement(element, attachNode)` takes any node, and `f4vr::getOffhandWandNode()` (`src/f4vr/PlayerNodes.h`) returns the offhand controller's node. The presets cover only the primary wand and the HMD (`src/vrui/UIManager.h`).

A vrui element has a position and a scale but no rotation (`UIElement::calculateTransform` in `src/vrui/UIElement.cpp`), so a panel faces the way its node faces. The primary-wand presets use `primaryUIAttachNode`, a node the game provides for UI. Which way the offhand's nodes face (`secondaryWandNode` and `secondaryUIOffsetNode` in `RE::VRPlayerNodes`) is not known.

The forearm and the hand are bones of the player's skeleton (`LArm_ForeArm3` and `LArm_Hand`, or the `RArm` pair in left-handed mode, named in `src/f4vr/F4VRSkelly.h`), found by name under the player's root node. The skeleton is replaced on a save load and when the player enters or leaves power armor. Without FRIK, nothing is expected to move that arm to the controller; this was not checked in game.

**Work.**

- A vrui root element can be given a rotation relative to its attach node, and its whole tree turns with it.
- A preset attaches a root to the offhand controller with an offset and a rotation. The placement is mirrored in left-handed mode, as in `attachPresetToPrimaryWandLeft`.
- A second preset is for the wrist. It attaches a root to the offhand controller in the same way. While the offhand's hand bone is close to the controller, it sets the root's offset and rotation each frame so that the root lies on the forearm bone. When the hand bone is missing or away from the controller, the controller placement is used. The bones are looked up by name, and again after the skeleton is replaced.

**Done when.** A `UIImGuiPanel` attached to the offhand faces the player when the hand is raised, in right-handed and left-handed mode. With FRIK, a row of vrui buttons stays on the inner wrist and outside the arm while the elbow moves, also in power armor. Without FRIK, the same row sits beside the controller.

## Interactive ImGui panel

### 2. Pointer input for ImGui canvases

**Need.** A canvas can be pointed at, clicked, and dragged, with ImGui doing the hover, press, and drag logic of its own widgets.

**Today.** Canvases are display-only. `imgui::internal::onFrameEnd()` in `src/imgui/ImGuiLayer.cpp` runs the ImGui frame without feeding any input, and each canvas window is created with `ImGuiWindowFlags_NoNav`. `src/imgui/README.md` and the `UIImGuiPanel` header both say so.

**Work.**

- Interactivity is opt-in per canvas and per `UIImGuiPanel`. A canvas that does not ask for it costs nothing more than today.
- The framework produces a pointer sample each frame: a world-space ray from a wand node (`primaryWandNode` or `secondaryWandNode` in `RE::VRPlayerNodes`), with an offset the mod can tune, and whether the trigger is down. The source of the sample is kept separate from its use, so a fingertip can become a second source later.
- In `onFrameEnd()`, after placements are resolved and the atlas is packed, and before `ImGui::NewFrame()`: intersect the ray with the quad of each interactive canvas, take the nearest hit on a front face, convert it to that canvas's pixels, add the canvas's offset in the shared atlas if it has one, and pass the result to `io.AddMousePosEvent`. With no hit, pass `(-FLT_MAX, -FLT_MAX)` so nothing stays hovered.
- The trigger becomes `io.AddMouseButtonEvent(0, down)`.
- ImGui has one pointer, so one hand owns it at a time: the hand that pressed last, or the primary hand when both rays hit and neither has pressed. A mod can restrict pointing to one hand.
- The mod can ask what the pointer is doing: which canvas is hovered, where, and whether ImGui wants the pointer.

**Done when.** A button, a checkbox, a slider, and a list in a `UIImGuiPanel` can be operated with a ray, both when the panel is attached to the other hand and when it stands one to two meters away.

### 3. Input capture and suppression

**Need.** The game is not paused while the menu is open. Operating the panel must not fire the weapon or move the player, and leaving the panel must give the controls back to the game.

**Today.** `VRControllersSuppress` (`src/vrcf/VRControllersSuppressor.h`) hides buttons and axes from the game per owner key while the mod still reads the real input. Nothing connects it to canvases.

**Work.**

- Suppress on hover, not on press. While the pointer hand's ray is on an interactive canvas, that hand's trigger is suppressed, and so is its thumbstick when the canvas scrolls. The decision is then made before a press can happen. `io.WantCaptureMouse` is known only after ImGui has run, which is too late to hide the first frame of a click from the game.
- Latch on the press edge. A press that starts on a canvas belongs to the UI until it is released, even if the ray leaves the canvas: suppression holds, the pointer keeps following the ray's intersection with the canvas plane, and ImGui gets the release. Without this the game sees a trigger that is already down, and ImGui keeps a widget stuck active.
- A trigger that is already down when the ray reaches a canvas is ignored until it is released: no click on entry, and suppression does not start in the middle of a press.
- Suppression is released when the canvas is hidden or destroyed.

The reasoning behind the press latch is in the modding reference library, `knowledge-base/imgui_ui_overlay_v3_open_questions.md` section 3.

**Done when.** Pulling the trigger on a widget never fires the drawn weapon, including when the ray slides off the panel during a drag. Pulling it away from the panel fires as normal. Closing the panel in the middle of a press leaves nothing suppressed.

### 4. Pointer visuals

**Need.** The player sees where they are pointing: a ray from the controller and a mark where it meets the panel.

**Today.** Nothing draws either. `render::PrimitiveDraw` can draw lines and images in a layer with a declared draw order, and ImGui can draw a mark at the pointer position in its foreground draw list. The game has a laser pointer of its own (`primaryWandLaserPointer` in `RE::VRPlayerNodes`); whether it can be reused is not known.

**Work.** A default ray and hit mark that draw above the panels (`DRAW_ORDER_PANELS`) and are not hidden by the world. Color, width, and the ray's length when it hits nothing are configurable, and a mod can turn the default off and draw its own.

**Done when.** The ray and mark are visible and stable in both eyes, and the mark sits on the widget the trigger would press.

### 5. Scrolling

**Need.** The mod list and a settings page can be longer than the panel.

**Today.** No wheel input is fed, so nothing scrolls. The pieces that would scroll exist: the content child of a fixed-size canvas scrolls when its content is taller than the canvas, and content can create scrolling child windows of its own.

**Work.** While the pointer is on a canvas, the pointer hand's thumbstick Y axis is fed as `io.AddMouseWheelEvent`, scaled by frame time, and that axis is suppressed as in item 3.

**Done when.** A list longer than its panel scrolls with the thumbstick while the ray is on it, and the player does not move or turn.

### 6. Dedicated panel

**Need.** Mods Config VR shows one ImGui panel at a time, and the Full panel is a large one. The panel has to be large enough for the whole Full layout and sharp at reading distance. Its dropdowns, popups, and confirmation dialogs open inside it and are ImGui's own: content calls `ImGui::BeginCombo`, `ImGui::BeginPopup`, and `ImGui::BeginPopupModal` as in any ImGui program.

**Today.** All of a mod's canvases share one atlas of 1024 by 1024 layout pixels (`MAX_CANVAS_PIXEL_SIZE` in `src/imgui/ImGuiCanvas.h`), and the atlas is also ImGui's display (`io.DisplaySize` in `src/imgui/ImGuiLayer.cpp`). That suits several small panels. For one large panel it causes two problems:

- **Size.** A canvas cannot be larger than the atlas. A `UIImGuiPanel` gets 48 pixels per vrui unit, so a panel over about 21 units is scaled down to fit and looks softer. At the default 24-pixel font, 1024 pixels is roughly 80 characters across: about the width of the Full wireframe, with nothing to spare for VR-sized controls.
- **Popups.** ImGui fits a popup to its display, and a canvas shows only its own part of the atlas. A combo near the bottom of a canvas opens downward, into a part of the atlas the canvas does not show, and a dialog is centered on the atlas instead of the canvas.

The layer also pushes zero window padding and border size for the canvas window and leaves them pushed while the content runs, so a popup gets no padding.

**Work.** A `UIImGuiPanel` can be marked as dedicated, and it passes the mark to its canvas.

- A dedicated panel has a texture of its own, sized to the panel, instead of a part of the shared atlas. The atlas does not limit its size, so it keeps 48 pixels per vrui unit at any size.
- While a dedicated panel is visible, ImGui's display is that panel. ImGui then keeps every popup inside the panel by itself.
- While a dedicated panel is visible, it is the only ImGui panel the mod draws, because ImGui has one display. The mod's other ImGui panels are drawn again when it is hidden. vrui buttons and vrui text panels are not ImGui and are not affected. Other mods are not affected either: each has its own copy of the framework.
- For every canvas, the zero window padding and border size are popped before the content runs.

The GPU time of a texture of the panel's size is checked with the existing perf sites, which already time each overlay layer.

**Done when.** A dedicated panel larger than 1024 pixels renders at full resolution, and its GPU time is known. A combo box near its bottom edge opens fully inside the panel, and a row is picked with the ray. A confirmation dialog opens centered on the panel.

### 7. World-locked placement

**Need.** The Full panel is spawned in front of the player and then stays fixed in the world. A panel attached to the HMD moves with the head, so it also moves under the ray. It can be respawned to recenter it.

**Today.** A vrui root is attached to a game node: `UIManager::attachElement(element, attachNode)`, with presets for the primary wand and the HMD (`src/vrui/UIManager.h`). `UIImGuiPanel` resolves its placement from that node and does not draw without one. A bare `imgui::Canvas` accepts any transform from its placement callback, but it is then outside vrui layout.

**Work.** A vrui root can be placed at a fixed world transform instead of on a node, and the transform can be changed later. A helper computes the usual spawn transform from the HMD: a given distance along the horizontal head direction, upright, facing the player.

**Done when.** A `UIImGuiPanel` root stays in place while the player walks and turns, including snap and smooth turning, and can be respawned in front of the player.

## Live preview for CommonFramework mods

Mods Config VR writes a mod's INI on Save, and the framework's file watcher reloads it. Before Save, live preview needs values that are applied in memory only.

The framework already has the mechanism. `ConfigBase::setConfigOverride`, `clearConfigOverride`, and `clearAllConfigOverrides` (`src/ConfigBase.h`) keep session overrides keyed by INI section and key, apply them over the file on every reload, and never write them to disk. Each mod's devbench tool drives them today (`set` and `clear` in `src/devbench/DevBench.cpp`), and FRIK exposes the same thing through its API.

### 8. Config reload safety

**Need.** A Save from Mods Config VR always reaches the running mod, and config loads never overlap.

**Today.** These come from reading `src/ConfigBase.cpp`. None was reproduced in game.

- The file watcher reloads on its own thread, and an override change reloads on the caller's thread. Nothing stops the two from running `loadIniConfigInternal` at the same time. A Save followed by clearing the overrides triggers one of each within a fraction of a second.
- When the mod saves its own INI it sets `_ignoreNextIniFileChange`, and the next watcher event that passes the duplicate filter clears it. If that event is filtered out instead, because the mod saved again within 200 ms of the previous event, the flag stays set and the next external change is ignored. That next change would be the player's Save.
- A second external write within 200 ms of the first is dropped as a duplicate.

**Work.** Config loads are serialized, or the watcher's reload is moved to the game thread. The mod's own writes are recognized by something that cannot outlive them, such as the write time or the content of its last save, instead of a flag.

**Done when.** An external write is reloaded after any sequence of the mod's own saves, and two loads never run at once.

### 9. Overrides at preview rates

**Need.** Dragging a slider previews a value many times a second.

**Today.** Every `setConfigOverride` call reads and parses the INI from disk, runs the mod's whole `loadIniConfigInternal`, writes an info-level log line, and emits a devbench event. Subscribers registered with `subscribeForIniChangedEvent` are called only after a file-watch reload, so a mod that reacts to config changes through them does not see an override.

**Work.**

- Several overrides can be set or cleared with one reload.
- Preview-rate changes are logged at debug level.
- The parsed file is reused until the file changes, so a preview does not read the disk.
- Config-changed subscribers are notified after an override-driven reload too.

**Done when.** A slider drag previews at the rate the host sends it, with no disk read and no info log line per change, and a subscribed mod reacts to each one.

### 10. Override ownership

**Need.** Several callers override a mod's config: the devbench tool, another mod through the mod's API, and the Mods Config VR client. A caller that clears its override must not remove another caller's.

**Today.** `ConfigBase` keeps one override per INI section and key, with no owner (`_overrides` in `src/ConfigBase.h`). A set replaces whatever is there, and a clear removes it, whoever set it. The devbench tool's `clear` with `all` calls `clearAllConfigOverrides`, which removes every caller's overrides. FRIK's API takes a `caller` name on set and clear and uses it only in a log line.

Mods Config VR has a case of its own. It takes the opening gesture from FRIK by overriding a FRIK config key through FRIK's API. If the player previews that key in the menu, the preview replaces the override, and Discard or Save clears it.

**Work.**

- Each override has an owner name. Set, clear, and clear-all take the owner, and a clear removes only that owner's overrides.
- When two owners override the same key, the one set last applies. When it is cleared, the other applies again.
- The devbench tool uses an owner name of its own. A mod's API passes its caller's name as the owner, so FRIK's API keeps its signature.

**Done when.** With two owners overriding one key, clearing either leaves the other's value applied. One owner's clear-all leaves the other owner's overrides in place.

## Images and text

### 11. Images in canvases

**Need.** A canvas shows pictures: MCM's images, which the menu reads out of a mod's `lib.swf` as pixels, the images of a config spec, and later the controller icons of a binding.

**Today.** `render::Texture::load` loads a texture file through the engine by its path (`src/render/Texture.h`). There is no texture from pixels. `ImGui::Image` needs a texture that is valid on the render thread and stays alive until the cloned frame has been drawn. `render::Texture` gives that guarantee to `PrimitiveDraw` frames, and the ImGui path has no equivalent.

**Work.**

- A `render::Texture` is created from pixels in memory.
- The content of a canvas draws a `render::Texture` as an ImGui image, and the texture stays alive until the frame that uses it has been drawn.

**Done when.** A picture made from pixels in memory and one loaded from a file are both shown in a dedicated panel, and a texture can be dropped while its panel is shown.

### 12. Text entry and an on-screen keyboard

**Need.** A text field in a canvas is typed into: an MCM text row, a search. VR has no keyboard, so one is shown.

**Today.** No characters or keys are fed to ImGui (`src/imgui/ImGuiLayer.cpp`), so an `ImGui::InputText` shows its text and cannot be edited. A focused `InputText` keeps its own copy of the text and ignores outside changes to its buffer, so typing has to reach it as ImGui's character and key events.

**Work.** The design is the framework's [VR keyboard](../../external/F4VR-CommonFramework/docs/tech/vr-keyboard.md).

- The keys of the game's window reach ImGui as its character and key events. A physical keyboard types into a field, and so does a keyboard overlay that types into the window.
- SteamVR's keyboard is shown while ImGui edits a text field, as the game shows it for its own names, and its text reaches the field as the same events. The content calls `ImGui::InputText` as on a screen, and the mod has nothing to call.
- A runtime with no keyboard, as OpenComposite, has the keys of the window only. The framework draws no keyboard of its own, and a number is typed on the same keyboard.

**Done when.** A text field in a dedicated panel is typed into with SteamVR's keyboard and accepted, a field that is cancelled keeps the text it had, and a physical keyboard types into it too.

## The client of Mods Config VR

### 13. Mods Config VR client in `ModBase`

**Need.** A CommonFramework mod gets live preview by shipping a config spec. It writes no client code. A mod that has a config UI of its own offers it to the menu, and a mod can add functions that the buttons of its config spec run.

**Today.** `ModBase` registers a devbench tool for every mod after `onGameLoaded` (`src/ModBase.cpp`), using the vendored devbench client (`src/devbench/DevBenchAPI.h`). Nothing knows about Mods Config VR.

**Work.** In two parts. The menu needs the first for the handoff, and the second for live preview.

The client and its functions:

- When Mods Config VR is installed, `ModBase` registers the mod as a native client through a vendored client header, the way it registers the devbench tool. It registers every mod, under `Settings.name`, and does not look for a config spec. The host matches the client to the config spec whose `modName` is that name. A `Settings` flag opts a mod out, like `devbenchTool`.
- A mod adds functions to its client, each under a name, which the buttons of its config spec run.
- A mod that has a config UI of its own gives the client how to open it, and tells the client when it has closed. The host opens it through the handoff.
- A mod can be told when a page of its config spec is shown and left, to show in the world what the page sets; see [Page events](architecture.md#page-events).

The preview. The client maps the host's calls onto the overrides, under an owner name of its own that no API caller uses (item 10):

- A preview sets the override for the setting's INI section and key.
- A discard clears it.
- After a Save, it clears the overrides it set. The file holds the values by then, so the reload that follows reads them from disk.

It acts only on settings bound to the mod's own INI.

This item depends on the native API of Mods Config VR, described in [Architecture and lifecycle](architecture.md#live-preview-api), so each part comes after the host has its side of it.

**Done when.** A framework mod's own config UI is opened from the menu, and the menu comes back when it closes. A framework mod with a config spec and no client code previews a slider live, rolls back on Discard, and keeps the value after Save.

## Later work

Each of these is made only if a milestone of the [Mod work plan](mod-plan.md) finds that it needs it.

- **Fonts.** `loadCanvasFont` (`src/imgui/ImGuiFonts.cpp`) loads the embedded Roboto, and ImGui rasterizes each glyph at each size as it is first drawn, so Latin, Greek, Cyrillic and Vietnamese text renders at any size. A config spec or MCM config translated into Chinese, Japanese, Korean or Arabic needs a fallback font with those glyphs, merged into the canvas font.
- **Touch as a second pointer source.** A fingertip crossing the canvas plane, taken from the same fingertip that vrui buttons use. For the hand-anchored Focused view, if a ray does not suit a panel on the own hand.
- **Ray press for vrui buttons.** Needed if vrui buttons end up in the Full panel's layout, out of reach of a finger.

## Not framework work

These are handled in Mods Config VR or exist already:

- **The opening gesture.** Holding both thumbsticks is expressible as an input binding today: `primary longpress thumbstick 1.5 +offhand:thumbstick`, the binding FRIK uses for its own configuration. Taking the gesture over from FRIK goes through FRIK's API (`setConfigValueOverride`).
- **FRIK integration.** vrui finds the fingertip and points the hand through FRIK's API by itself.
- **Hand-anchored vrui buttons**, such as Minimized's Resume and Close. Only their placement on the offhand is framework work (item 1).
- **Haptics.** `VRHaptics` and its named patterns.
- **Files, config specs, and MCM.** Reading and writing other mods' files, the config spec schema, and the MCM bridge live in the mod.
- **The Papyrus API.** The framework's F4SE VR port (`src/f4sevr/`) already registers native functions that take and return strings, and calls a function on a script. F4SEVR lists the scripts registered for an external event.
- **The host side of the native API.** The framework only carries the client (item 13).
