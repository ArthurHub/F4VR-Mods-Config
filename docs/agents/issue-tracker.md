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

Maps are local markdown in this repository, not GitHub issues.

- Map: `.scratch/<effort>/map.md`.
- Tickets: `.scratch/<effort>/issues/NN-<slug>.md`, numbered from `01`,
  with the question in the body, a `Type:` line (research, prototype,
  grilling or task) and a `Status:` line (`open`, `claimed`, `resolved`).
- Blocking: a `Blocked by: NN, NN` line near the top. A ticket is
  unblocked when every ticket it lists is `resolved`.
- Frontier: the first ticket by number that is `open` and unblocked.
- Claim: set `Status: claimed` and save before any work.
- Resolve: append the answer under `## Answer`, set `Status: resolved`,
  and add a context pointer (the ticket's title as a link, and a gist)
  to the map's Decisions so far.
- Research findings: `.scratch/<effort>/research/NN-<slug>.md`, linked
  from the ticket's answer.

A map is committed on its effort's branch and deleted when it is done.
