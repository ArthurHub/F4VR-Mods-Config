# TakesEffect

When a change of a setting takes effect in its mod, as its config spec says it with `applies`: `live`, `save` or `restart`. Most settings, and every MCM one, are `unknown`: the menu cannot tell, so it shows nothing. Save keeps every change either way: when the mod acts on it is the mod's.

**Provides:** each setting's `applies`, and whether its value is pending.

- **The marks**, each in a 16 px box in `amber`, drawn with `AddCircleFilled`, `PathArcTo`, `AddLine` and `AddRectFilled` as the warning mark is:
  - `live`, as it is changed: a 2.2 px dot with an arc on each side, a broadcast.
  - `save`, after Save: a disk, a 2 px outline with its label and its shutter filled.
  - `restart`, after the game is restarted: a power mark, an arc open at the top with a line down into it.
- **A changed row** has the mark of its `applies` in place of the pending dot, after its name ([SettingRow](../SettingRow/README.md)): it says that the change waits for Save and when it takes effect. A row of an `unknown` setting keeps the plain dot. A row that is not changed has no mark: one on every row would take room and say little.
- **The help line**, while the ray is on a changed row, has the tag of its `applies` first ([DescriptionArea](../DescriptionArea/README.md)): its mark and its word, `Live`, `Save` or `Restart`, in `t-detail`, all in `amber` with an `amber` outline, as its value is. It appears as the setting is changed, and goes as it is set back or discarded. An `unknown` one has none.
- A count keeps the plain dot: the footer's status, a folded section's heading, the unsaved-changes dialog.
- **The Help page** says what each mark means, under `Saving` ([FullPanelHelp](../FullPanelHelp/README.md)).
- A Save that wrote a `restart` change says so after it, and the menu's front page until the game is restarted ([RestartNotice](../RestartNotice/README.md)).

The behavior is in [When a change takes effect](../../../../schema.md#when-a-change-takes-effect).
