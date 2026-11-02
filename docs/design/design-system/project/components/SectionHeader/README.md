# SectionHeader

A heading that groups rows, drawn as `ImGui::SeparatorText`, a section in a section, and a section that folds, as the Advanced section.

**Provides:** the heading text and its help; how many sections it is in (its depth); whether it folds and is open; and, for a folded section, how many of the settings inside it are invalid and how many have unsaved changes.

## A heading

- `t-body` `green-muted` text, `space-3` into its row as a row's name is in its frame, so it lines up with the rows beside it, then a 2 px `rule` line to the right edge of the rows. `space-2` above it. The rows under it are set in a level, `space-3`, so their frames start under its text and they are seen to be its; see [SettingRow](../SettingRow/README.md).
- It is not pressed and has no hover look. While the ray is on it, the help line shows the section's help, when it has one.

## A section in a section

- A section in a section is set in a level, `space-3`, with what is in it: its heading starts where the outer section's rows do, and its rows and the sections in it are a level further. The rows still end where the other rows do, so the controls stay in one column and only the name narrows. `ImGui::Indent(theme::SPACE_3)`, with the width of a row taken from the cursor: not `IndentSpacing`, which is the tree's `space-8`.
- While it is open and has rows, a 2 px `rule` line runs from under its heading, `space-1` in, down to the bottom of its last row, and turns there toward the rows in a foot that stops `space-1` short of their frames, with a `radius-frame` corner. It shows what the section covers, so a row after it is seen to be the outer section's. One `AddLine` down and one along, or a path with `PathArcTo` at the corner.
- Its rows are a level in from the line's start, so their hover frames clear it by `space-2`, and their marks are after their names, away from it.
- A folded section has no line: it has no rows showing. A section in it has its own line, inside the outer one.
- A level past the third is set in as the third, so a name keeps its room.
- A section of an MCM config does not nest: its rows are the ones after it, up to the next section.

## A section that folds

- Its heading is a row, a `TreeNode` as high as a row (`control-height`): a drawn arrow `space-3` in, where a heading's text starts, then a `t-body` `green-muted` label, and a 2 px `rule` line as a heading has, `space-3` after the label at the height of `SeparatorText`'s. The rule is how a heading is known, so the rows above are seen to end there. Hover: `hover` with a `frame-border` outline over the whole row, rule too; the arrow and the label stay `green-muted`, as a heading that does not fold is. The arrow is what tells it from a heading that does not fold, as in the tree's category row.
- The rule runs to the right edge of the rows, as a heading's does. Folded with something to say, it stops `space-2` before what it hides. One `AddLine` from the end of the label.
- The arrow points right while it is folded and down while it is open. A press folds or opens it, as many times as the player likes. It starts as `collapsed` says each time its page is shown, closed for Advanced.
- Its rows are set in as any section's are, by the sections it is in. Advanced is in the section it belongs to, so its heading and its rows are a level in.
- Folded, it says what it hides at the right of its heading, after the rule, in `t-detail`: the warning mark and `1 invalid` in `red` first, then the pending dot and `2 unsaved` in `amber`, each only when there is one. They are the rows' own marks, so the state is not told by color alone. Open, it says nothing: its rows show their own.
- While the ray is on it, the help line has the section's help and then what it hides: "Advanced: Fine-tuning of the left hip holster. Folded: 1 invalid value, 2 unsaved changes."
- Detailed settings are a folded section, Advanced, right after the settings they belong to.

Don't style headings in capitals or a larger size: the line already marks them.

The behavior is in [Sections](../../../../schema.md#sections).
