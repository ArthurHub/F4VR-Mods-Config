# SectionHeader

A heading that groups rows, drawn as `ImGui::SeparatorText`, and the collapsible Advanced section.

**Provides:** the heading text; for Advanced, the open state.

- Heading: `t-body` `green` text, then a 2 px `rule` line to the right edge. `space-2` above it.
- Advanced: a `t-body` `green-muted` label with a drawn arrow, a `TreeNode`. Rows inside are indented `space-8`. Closed by default.
- Hover on Advanced: `hover` with a `frame-border` outline, text to `green`.

Don't style headings in capitals or a larger size: the line already marks them.
