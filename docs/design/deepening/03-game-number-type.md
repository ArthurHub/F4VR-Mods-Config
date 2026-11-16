# 03. A game number as a stored type in the setting value rules

Part of [Deepen the hot spots](README.md), which holds the shared decisions, test seams and testing rules. Seam A.

## Problem Statement

Both stores turn a number into the type a setting's storage keeps, as Papyrus casts it: a bool is on at any number but 0, an int is the nearest whole number. Each has its own copy:

- [ConfigSpecStore.cpp](../../../src/spec/ConfigSpecStore.cpp) `asType`, which compares with `common::fNotEqual`;
- [McmStore.cpp](../../../src/mcm/McmStore.cpp) `typedValue`, which also reads a text as a number, and compares with `*number != 0.0f`.

The MCM store's `fromNumber` (a switcher is on at exactly 1, as MCM's menu reads a global) compares with `number == 1.0f`. Both `==` comparisons break the repo's float rule. The copies are reached only through store tests with real files.

The rest of the two stores' value helpers differ on purpose and stay: the value shown while there is none (the config spec store starts from the default, the MCM store from the control's least), and MCM's reading of a global.

## Solution

The setting value rules answer "this number (or value) as a stored type". Both stores ask it, and their copies go. `fromNumber`'s compare uses `common::fEqual`.

## User Stories

1. As the maintainer, I want one Papyrus-cast rule, so that a game value reads the same in a config spec mod and an MCM mod.
2. As the maintainer, I want the rule tested directly, without files.

## Implementation Decisions

- **Where.** A function in `model/SettingValues.h`, from a value and a `StoredType` to the value in that type, or none (a text as a number, a number as a text). Whether a text that is a number is read as one, as the MCM store does, is a parameter or a step the MCM store keeps: settled when written.
- **Float compares** use `common::fEqual` / `fNotEqual`.

## Testing Decisions

- **New tests** in the setting value rules tests: bool, int and float from each value kind, and the none cases.
- **Regression net.** Both stores' tests pass unchanged.

## Out of Scope

- The value shown while there is none, and MCM's reading of a global's number.
- Devbench's reading of a value from JSON.
