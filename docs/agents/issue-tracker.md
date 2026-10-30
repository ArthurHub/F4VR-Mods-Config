# Issue tracker: GitHub

Issues and specs live in ArthurHub/F4VR-Mods-Config on GitHub.
Use the gh CLI from this clone.

## Conventions

- Before creating an issue, verify the target with
  `gh repo view --json nameWithOwner`.
- Create: `gh issue create --title "..." --body-file <file>`.
  Write multiline bodies to a temporary file with actual newlines.
- Read: `gh issue view <number> --json number,title,body,labels,comments`.
- List: `gh issue list --state open --json number,title,body,labels`.
  Add label and state filters as needed.
- Comment: `gh issue comment <number> --body-file <file>`.
- Label: `gh issue edit <number> --add-label "..."` or
  `--remove-label "..."`. Use docs/agents/triage-labels.md for triage roles.
- Close: post the explanation as a comment, then run
  `gh issue close <number>`.

Use GitHub's native sub-issues and issue dependencies when available.
Otherwise, put `Part of #<parent>` and `Blocked by: #<number>` lines
at the top of child issues and maintain a task list in the parent.
An issue is unblocked when every blocker is closed.

File issues into this repository. If gh resolves another repository,
confirm the intended target before publishing.

## Pull requests as a triage surface

**PRs as a request surface: no.**

## Skill operations

- "Publish to the issue tracker": create a GitHub issue.
- "Fetch the relevant ticket": read the GitHub issue.

## Wayfinding

The map is one issue labelled `wayfinder:map`.
Tickets are its child issues, labelled `wayfinder:<type>`:
research, prototype, grilling, or task.

The frontier is the first open child in map order with no open blockers
and no assignee. Claim it with `gh issue edit <number> --add-assignee @me`.
Resolve it by commenting with the answer, closing it, and adding a
context pointer to the map's Decisions-so-far.
