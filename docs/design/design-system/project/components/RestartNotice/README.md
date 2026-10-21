# RestartNotice

A config spec setting whose change takes effect only after the game is restarted (`applies: restart`): its mark, its help, what the footer says after a Save that wrote one, and the notice on the menu's front page.

**Provides:** each setting's `applies`; after a Save, how many of the settings it wrote are `restart`; for the menu's front page, the mods whose saved changes wait for a restart in this run of the game.

- **The row** has the `Restart` mark after its name ([SettingRow](../SettingRow/README.md)): `t-detail` `green-muted`, outlined in `frame-border`. It is there whether the value is changed or not, so the player knows before changing it.
- **Its help** ends with when it takes effect: "Takes effect after the game is restarted.", before the default. A `save` setting's help says "Takes effect after Save." A `live` one says nothing.
- **While it is pending**, the footer's status counts it: "2 unsaved changes, 1 takes effect after a restart", in `amber` with the dot.
- **After a Save that wrote one**, the status is the `Restart` mark and "Saved. 1 change takes effect after the game is restarted." in `green`. It stays while the player is on the mod, until the next change, which brings back the pending status. A Save that wrote none says "Saved" in `green-muted`, and then "No unsaved changes" once the player moves on.
- **The menu's front page** has a line under Notices for each mod with such a change, from its Save until the game is restarted: the `Restart` mark and "Restart the game: Immersive Flashlight VR has 1 saved change that takes effect then." in `green`. The restart notices come before the mods that are not available. A setting changed back and saved again still counts: the menu does not know what the game has read.
- Nothing asks the player to restart now, and nothing blocks: the game is the player's.

The behavior is in [When a change takes effect](../../../../schema.md#when-a-change-takes-effect).
