# Deepen the hot spots

Six changes that deepen the parts of the code that change most often. Each is implemented and reviewed on its own, and has its own file here. This README holds what they share: the problem, the test seams, the decisions that apply to all of them, and the order. It is outside the milestones of the [mod plan](../mod-plan.md).

Source: the [architecture review](architecture-review.html), whose candidates 01–06 match the files here.

## The changes

In the suggested order. A change's row is marked done when its commit lands.

| Order | Change | Seam | Blocked by | Status |
| --- | --- | --- | --- | --- |
| 1 | [01. Devbench reports and presses from the panel's views](01-devbench-views.md) | B | | done |
| 2 | [03. A game number as a stored type in the setting value rules](03-game-number-type.md) | A | | done |
| 3 | [02. The config spec loader reads every page before any condition](02-loader-walk.md) | C | | done |
| 4 | [04. Option sets work out the choice that sets other settings](04-option-sets.md) | A | | |
| 5 | [05. Binding wording and editing rules in the model](05-binding-rules.md) | A | | |
| 6 | [06. Where the panel is, and its Back order](06-panel-navigation.md) | B | | |

## Problem Statement

The panel, devbench, the config spec loader and the config spec store still change with almost every feature. Some of their rules are written out twice, and two copies already disagree:

- Devbench decides on its own whether a button saves first, whether a setting has a Reset and which breadcrumbs are links. For an MCM mod's script button it saves first, and the panel doesn't. The e2e tests check devbench's answer, not the player's.
- The loader walks the pages and rows of a config spec three times, and reads the front page's conditions before the pages: a front-page condition that names a page's setting is left out. One shared walk is not worth it yet, so 02 fixes only the order.
- The two stores each turn a game number into a stored type, and the MCM store compares floats with `==`.
- The config spec store works out a choice that sets other settings in seven methods; it can be tested only through the store, with files.
- The binding editor and the Help page word a binding differently, and the editor's rules for changing a binding are private to its drawing.
- Where the panel is lives in five fields, moved by eight methods.

## Solution

Deepen each so that its rule lives in one module and is tested through one interface. Nothing the player, a mod author or a native API client sees changes. The exceptions are the drifted copies, devbench follows the panel (01) and the binding wording picks one form (05), and the loader's order (02): a front-page condition can name a setting on a page.

## Test seams

Every change is tested at one of three seams. No other seams are added.

- **A. The model's modules:** the setting value rules, option sets and input bindings, through their free functions.
- **B. The panel's interface and its views:** the row view and the new button view and breadcrumbs, each a pure function tested at its own interface; the panel through its existing queries and the test panel.
- **C. The config spec loader's interface:** a config spec's text in, the mod and its problems out, as its tests drive it today.

## Decisions for every change

- The behaviour the player, a mod author and a native API client see does not change, except where a change says so.
- The config spec schema, the native API header (and its identical copy in the framework) and the MCM handling do not change.
- The MCM store and the config spec store keep their own behaviour (write at once vs pending changes). They are not merged.
- New modules are plain: no registries or general machinery beyond what this mod's real cases need.
- New modules are in the mod's namespace, follow the repo's doc-comment rules, and get a row in the [components doc](../../agents/components.md). The glossary gains a term when a change names a new domain module.

## Testing for every change

- **What a good test is.** It drives a module through its interface and checks what a caller observes. It never checks private state or which helper ran.
- **Replace, don't layer.** When a rule moves into a deepened module, its tests move to that module's interface. Tests that reached it only through pixels or files are deleted where the new tests cover them.
- **Regression net.** The existing loader, store and panel tests pass unchanged at each step, except tests that read a removed copy directly.
- **e2e.** The pytest e2e tests through devbench are run by hand before a change is called done.

## Out of Scope

- Merging the MCM store and the config spec store.
- Splitting the config spec loader by schema area.
- Reorganising the shared drawing primitives.
- Changing the settings model's data types, the config spec schema, the native API or MCM support.
- New player-facing features or look changes.
- Moving the existing panel tests off pixels wholesale.
