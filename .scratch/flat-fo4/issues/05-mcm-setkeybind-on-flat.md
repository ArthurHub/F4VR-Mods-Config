# MCM `SetKeybind` on flat

Type: research
Status: resolved

## Question

How can the menu change an MCM hotkey on flat while the game runs? For M305 of the plan:

- flat MCM's source: which movie gets `root.mcm` and its `SetKeybind`, and whether it outlives the pause menu
- whether MCM takes a key change any other way: Papyrus, an F4SE message, an export
- when MCM reads `Data\MCM\Settings\Keybinds.json`, and whether writing it is enough
- how the menu reads MCM hotkeys today

## Answer

MCM takes a key change only through `root.mcm` in the pause menu's movie (`Interface/MainMenu.swf`): `ClearKeybind`, else `RemapKeybind`, falling back to `SetKeybind`, with Int arguments, on the UI task while `PauseMenu` is open. MCM then saves `Keybinds.json` itself. It has no Papyrus native, message or export for keys, and reads `Keybinds.json` only as a save loads. So:

- with the pause menu open under the panel, a change applies at once; otherwise it is queued until the pause menu next opens, and also written to `Keybinds.json` in case a save loads first
- a push waits until a save has loaded, since MCM's table is empty before that and its next save would drop every other key (from the source, not tried in the game)
- F4SEMenuFramework's `MCMLiveSync` does the same, opening its menu over the pause menu
- each flat runtime has its own `mcm.dll`; the keybind code is the same from 1.38 to 1.40 and in VR, and the 1.11.x builds are closed source

The choices it brings up are [Binding an MCM hotkey on flat](12-binding-an-mcm-hotkey-on-flat.md). Findings: [research/05-mcm-setkeybind-on-flat.md](../research/05-mcm-setkeybind-on-flat.md).
