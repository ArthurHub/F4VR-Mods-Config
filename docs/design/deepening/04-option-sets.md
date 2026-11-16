# 04. Option sets work out the choice that sets other settings

Part of [Deepen the hot spots](README.md), which holds the shared decisions, test seams and testing rules. Seam A.

## Problem Statement

A choice that sets other settings has no value of its own: it is the first option whose `sets` its targets all have. The config spec store works this out in seven methods, each starting with `if (model::setsOthers(setting))`: `value`, `set`, `defaultValue`, `isDirty`, `discard`, `savedValue`, and the reading of values. Its private `setTargets`, `setOption` and `pickSets` hold the rules; [OptionSets.h](../../../src/model/OptionSets.h) holds only the matching of one value.

The rules read only the targets' current, default and saved values, yet they can be tested only through the store with files. Choices that set other settings, and then `alsoSets`, each changed the store again.

## Solution

The option sets module answers, from a small reader of the targets' values:

- the choice's value, default and saved value (which option holds);
- whether it is dirty (any target is);
- the targets a Discard takes back (its `sets` and `alsoSets`);
- what a pick sets on each target.

Each store branch becomes one call. The store keeps setting the targets, their preview and their Save.

## User Stories

1. As the maintainer, I want the rules of a choice that sets other settings in one module, so that a new kind of set (as `alsoSets` was) changes that module.
2. As the maintainer, I want those rules tested with a small fake of the targets' values, not files.

## Implementation Decisions

- **The reader.** What the module asks of the targets: a setting by its id, and its current, default and saved value. The store passes itself, or a small adapter over its entries; settled when written.
- **What stays in the store.** Setting a target (pending value, preview, client), Discard of each target, and the change counts that skip the choice.

## Testing Decisions

- **New tests** in the option sets tests through the reader fake: which option holds for current, default and saved values, dirty, the Discard targets, and a pick's set values with `alsoSets`.
- **Moved tests.** Store tests that only check the derivation move there; the store keeps the tests of a pick's preview and Save.

## Out of Scope

- The config spec schema of `sets` and `alsoSets`.
- MCM, which has no choices that set other settings.
