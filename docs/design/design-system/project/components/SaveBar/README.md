# SaveBar

The footer of the Full and Focused panels: the session's status on the left, Discard and Save on the right; for an MCM mod, a status line that says changes are saved as they are made.

**Provides:** the pending count, the invalid count, and whether the store saves or applies at once.

- Height `footer-height`, a `rule` line above, buttons 160 px wide in fixed places.
- The status shares the left of the footer with the help line (`DescriptionArea`): while the ray is on a setting or option, its help replaces the status. Discard and Save stay where they are.
- Clean: "No unsaved changes" in `green-muted`; Discard and Save disabled.
- Pending: the `amber` dot and "1 unsaved change" in `amber`; Save is primary.
- Blocked: the warning triangle and the problem in `red`; Save is secondary. Pressing it shows the summary and moves focus to the first invalid setting.
- Not saved: after a Save that failed, the warning triangle and "Not saved:" with the reason in `red`, wrapped as a help is: "Not saved: showcase.toml cannot be written." Nothing was written, so every change still waits and Save stays primary: it can be pressed again once the reason is gone. It stays until the next change, Save or Discard, or until the player leaves the mod.
- MCM: "Changes are saved as they are made" in `green-muted`, no buttons. It does not claim the mod acts on the change at once.
