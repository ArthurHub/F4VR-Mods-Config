# Flat drawing host

Type: research
Status: resolved

## Question

How does the framework draw its ImGui canvases on the flat screen inside the game's UI pass, with no swapchain hook, using what CommonLibF4VR has for 1.10.163 and 1.10.984? For F304 and F305 of the plan:

- the menus' render-condition virtual that the UI pass asks each frame, and the cursor menu's vtable slot for it
- the flags of a hidden menu that is always open, and how a plugin registers a menu of its own
- the game's D3D11 device and context, and the render target the UI pass has bound
- what CommonLibF4VR lacks for this on flat (missing 1.10.984 IDs, layouts)
- which parts of the framework's ImGui renderer carry over, and which are VR's per-eye drawing

## Answer

- **Two draw sites in the game's screen-space UI pass**, DearModdingUI's design (its unreleased HEAD, on all three flat runtimes). Both answer `IMenu::PassesRenderConditionText` (vfunc 7), which `UI::ScreenSpace_RenderMenus` asks each menu every pass:
  - a hidden menu of the framework's own, registered with `UI::RegisterMenu` and shown with `UIMessageQueue` `kShow` from a UI task; flags `kAlwaysOpen`, `kDontHideCursorWhenTopmost`, `kAllowSaving`, `kCompanionAppAllowed`, `kAdvancesUnderPauseMenu`, `kRendersUnderPauseMenu`, input off, `depthPriority` `kConsole`. It draws while `CursorMenu` is closed, and is shown again after a save load closes it.
  - `CursorMenu`'s slot 7, swapped with `write_vfunc(7, …)` on `VTABLE::CursorMenu[0]`, which draws under the cursor while it is shown.
- **The draw**: the device and context from `BSGraphics::RendererData`, into the render target the pass has bound (`OMGetRenderTargets`), inside `ScopedPipelineState`: `SubmitHook.cpp`'s frame without its VR checks.
- **F305**: the frame is built and drawn into the canvas's texture as in VR; only the per-eye world quad becomes one screen rectangle (a 2D vertex shader, one draw, no depth). Supersample 1 on flat keeps the text 1:1.
- **CommonLibF4VR lacks the 1.10.984 IDs** of `UI::RegisterMenu`, `RendererData::GetSingleton`, `IMenu`'s ID-backed virtuals and the Scaleform movie loader; upstream too. Each NG ID was found in `version-1-10-984-0.bin` and matched to the 1.11.221 PDB's names. On flat a missing ID resolves silently to the next ID's address.

The choices it brings up are [The flat drawing host's choices](15-the-flat-drawing-hosts-choices.md) and [Missing IDs on flat](16-missing-ids-on-flat.md). Findings, with the IDs: [research/01-flat-drawing-host.md](../research/01-flat-drawing-host.md).
