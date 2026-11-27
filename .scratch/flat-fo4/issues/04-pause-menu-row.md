# Pause-menu row

Type: research
Status: resolved

## Question

How does a plugin add a `Mods Config` row to flat Fallout 4's pause menu that opens its menu? For M303 of the plan:

- the movie and the list object the pause menu's rows are in
- how DearModdingUI splices its row in, and how a click reaches C++
- living beside MCM, which adds its own row to the same menu
- what CommonLibF4VR and the framework's Scaleform helpers give for it

## Answer

The pause menu's rows are in `Interface/MainMenu.swf`, which the title screen shares, at `root.Menu_mc.MainPanel_mc.List_mc.entryList`. As DearModdingUI does, the mod adds a row when `PauseMenu` opens: native AS3 listeners on its movie, `enterFrame` on `root` and `BSScrollingList::itemPress` on `root.Menu_mc`. The row is `{text, index}` with an `index` of its own, added above Settings only when it is missing and never moved (moving would fight DearModdingUI's row every frame). The vanilla menu ignores a press on an unknown `index`.

- A press calls `Menu_mc.BGSCodeObj.onContinuePress()`, as Esc does, and opens the panel as `PauseMenu` closes, so the game runs again.
- MCM's row (`index` 100) goes in once, at the top, a few frames after the menu opens, and leaves other rows alone: both show.
- CommonLibF4VR lacks `GFx::Value::GetElement` (DearModdingUI's fork has its IDs), and `HasMember` has only its 1.10.163 ID, so on 1.10.984 it calls a wrong address. By the framework's rule that game functions go in CommonLibF4VR, that is a C commit before M303. The framework's Scaleform helpers lack a native listener and a splice.
- The fake `PauseMenu` event that `McmEvents.cpp` sends is filtered.

The choices it brings up are [Opening from the pause menu](13-opening-from-the-pause-menu.md). Findings: [research/04-pause-menu-row.md](../research/04-pause-menu-row.md).
