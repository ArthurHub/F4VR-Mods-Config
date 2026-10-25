# RestartNotice

A config spec setting whose change takes effect only after the game is restarted (`applies: restart`): what the footer says while one is pending and after a Save that wrote one, and the notice on the menu's front page.

**Provides:** each setting's `applies`; after a Save, how many of the settings it wrote are `restart`; for the menu's front page, the mods whose saved changes wait for a restart in this run of the game.

- **The row** has no mark of its own: changed, its pending mark is the Restart mark, and the help line has its tag ([TakesEffect](../TakesEffect/README.md)).
- **While it is pending**, the footer's status counts it: "2 unsaved changes, 1 takes effect after a restart", in `amber` with the dot.
- **After a Save that wrote one**, the status is the `Restart` tag and "Saved. 1 change takes effect after the game is restarted." in `green`. It stays while the player is on the mod, until the next change, which brings back the pending status. A Save that wrote none says "Saved" in `green-muted`, and then "No unsaved changes" once the player moves on.
- **The menu's front page** has a line under Notices for each mod with such a change, from its Save until the game is restarted: the `Restart` tag and "Restart the game: Immersive Flashlight VR has 1 saved change that takes effect then." in `green`. The restart notices come before the mods that are not available. A setting changed back and saved again still counts: the menu does not know what the game has read.
- The `Restart` tag of the status and the notice is `t-detail` `green-muted`, outlined in `frame-border`: the change is saved, not pending.
- Nothing asks the player to restart now, and nothing blocks: the game is the player's.

The behavior is in [When a change takes effect](../../../../schema.md#when-a-change-takes-effect).
