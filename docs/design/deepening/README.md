# Deepen the hot spots

Eight changes that deepen the parts of the code that change most often. Each is implemented and reviewed on its own, and has its own file here. This README holds what they share: the problem, the test seams, the decisions that apply to all of them, and the order. It is outside the milestones of the [mod plan](../mod-plan.md).

Source: the [architecture review](../architecture-review.html), whose candidates 01–08 match the files here.

## The changes

In the suggested order. A change's row is marked done when its commit lands.

| Order | Change | Seam | Blocked by | Status |
| --- | --- | --- | --- | --- |
| 1 | [01. Setting value rules in one module per setting type](01-setting-value-rules.md) | A | | done |
| 2 | [02. A store session module that orders the store's lifecycle calls](02-store-session.md) | B | | done |
| 3 | [05. One walk for a page's pending marks](05-pending-marks.md) | A | | done |
| 4 | [06. A search session module behind the panel](06-search-session.md) | B | | done |
| 5 | [07. One row view for the panel and devbench, and Back owned by the mod page](07-row-view-and-back.md) | B | | todo |
| 6 | [08. A read part of the Store interface](08-store-read-interface.md) | A | 02 | todo |
| 7 | [03. A mod list module that owns the mods and their categories](03-mod-list.md) | B | | todo |
| 8 | [04. A game-free menu controller behind a port](04-menu-controller.md) | C | | todo |

## Problem Statement

The parts of Mods Config FO4 VR that change most often are the hardest to change safely. These are the panel, the menu, the two stores, the config spec loader and the devbench tool. Since v0.9 almost every feature has touched them.

Adding a setting type, or a store feature such as option sets, `alsoSets`, `typedOption` or categories, means editing a dozen to twenty files:

- cf.transform touched 17 source files.
- The Categories page touched 21.
- Choices that set other settings touched 13.

The same rule is written out in several places, so bugs sit where the copies drift apart. The menu's rules for requests and reloads link the game, so no unit test reaches them. Many panel tests can reach a rule only by pressing pixels.

## Solution

Deepen the hot spots so that each rule lives in one module and is tested through one interface. Nothing the player, a mod author or a native API client sees changes. The one exception is the suspected missing PageShown after a reload, which [02](02-store-session.md) checks and fixes if it is confirmed.

## Test seams

Every change is tested at one of three seams. No other seams are added.

- **A. The setting type modules in the model.** The existing color, transform, input binding and haptic modules, deepened, plus one entry point by setting type. Also the unsaved pages walk and the read part of the Store interface.
- **B. The panel's existing interface.** Driven with the existing test panel and a store double that records each call it gets, in order. The store session, the search session, the mod page's Back, the row view and the mod list are private modules behind the panel, tested through it.
- **C. A new game-free menu controller.** Behind one port, with a game adapter in the mod and a recording adapter in the tests. This is the only new seam.

## Decisions for every change

- The behaviour the player, a mod author and a native API client see does not change.
- The config spec schema, the native API header (and its identical copy in the framework) and the MCM handling do not change.
- The MCM store and the config spec store keep their own behaviour (write at once vs pending changes). They are not merged.
- New modules are plain: no registries or general machinery beyond what this mod's real cases need.
- New modules are in the mod's namespace, follow the repo's doc-comment rules, and get a row in the [components doc](../../agents/components.md). The glossary gains a term when a change names a new domain module.

## Testing for every change

- **What a good test is.** It drives a module through its interface and checks what a caller observes: return values, what a store was told and in what order, what the panel shows, what the game adapter was asked to do. It never checks private state or which helper ran.
- **Replace, don't layer.** When a rule moves into a deepened module, its tests move to that module's interface. Tests of the shallow pieces it absorbed are deleted.
- **Regression net.** The existing loader, store and panel tests must pass unchanged at each step, except tests that read a removed copy directly.
- **New panel tests** prefer the panel's queries over pressing pixels, and copy no layout constants.
- **e2e.** The pytest e2e tests through devbench are run by hand before a change is called done.

## Out of Scope

- Merging the MCM store and the config spec store.
- Splitting the config spec loader by schema area, beyond what [01](01-setting-value-rules.md) routes through its module.
- Reorganising the shared drawing primitives.
- Changing the settings model's data types, the config spec schema, the native API or MCM support.
- New player-facing features or look changes.
- Moving the existing panel tests off pixels wholesale.

## Further Notes

- The work spans many commits: ask the maintainer whether to use a branch before starting.
- When all eight are done, the rules they settled live in the code, the components doc and the glossary, and this folder can be condensed as the finished work plans were.
