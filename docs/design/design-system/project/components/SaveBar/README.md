# SaveBar

The footer of the Full panel: the session's status on the left, Discard and Save on the right. Every mod has it. A config spec's changes wait for Save, which writes them. An MCM mod's are made as the player makes them, as in MCM's menu, so its Save has nothing to write and its buttons say what they do: Keep accepts the changes, and Revert sets them back.

**Provides:** the pending count, the invalid count, and the last Save.

- Height `footer-height`, a `rule` line above, buttons 160 px wide in fixed places.
- The status shares the left of the footer with the help line (`DescriptionArea`): while the ray is on a setting or option, its help replaces the status. Discard and Save stay where they are.
- Clean: "No unsaved changes" in `green-muted`; Discard and Save disabled. An MCM mod's says "No changes made", and "Kept" after Keep where a config spec's says "Saved".
- Pending: the `amber` dot and "1 unsaved change" in `amber`; Save is primary. An MCM mod's change is in the game already, so its status says "1 change made", and Keep is primary.
- Blocked: the warning triangle and the count in `red`, "2 invalid values"; Save is disabled until no changed value is invalid. Each one has its mark on its row, and a folded section's heading counts those it hides.
- Not saved: after a Save that failed, the warning triangle and "Not saved:" with the reason in `red`, wrapped as a help is: "Not saved: showcase.toml cannot be written." Nothing was written, so every change still waits and Save stays primary: it can be pressed again once the reason is gone. It stays until the next change, Save or Discard, or until the player leaves the mod. An MCM mod's Keep writes nothing, so it is never Not saved.
