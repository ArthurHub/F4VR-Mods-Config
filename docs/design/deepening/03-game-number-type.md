# 03. A game number as a stored type in the setting value rules

Part of [Deepen the hot spots](README.md), which holds the shared decisions, test seams and testing rules. Seam A.

## Problem Statement

Both stores turn a number into the type a setting's storage keeps, as Papyrus casts it: a bool is on at any number but 0, an int is the nearest whole number. Each has its own copy:

- [ConfigSpecStore.cpp](../../../src/spec/ConfigSpecStore.cpp) `asType`, which compares with `common::fNotEqual`;
- [McmStore.cpp](../../../src/mcm/McmStore.cpp) `typedValue`, which also reads a text as a number, and compares with `*number != 0.0f`.

The MCM store's `fromNumber` (a switcher is on at exactly 1, as MCM's menu reads a global) compares with `number == 1.0f`. Both `==` comparisons break the repo's float rule. The copies are reached only through store tests with real files.

The rest of the two stores' value helpers differ on purpose and stay: the value shown while there is none (the config spec store starts from the default, the MCM store from the control's least), and MCM's reading of a global.

## Solution

The setting value rules answer "this value of the game as a stored type" (`asStoredType`). Both stores ask it, and their copies go. `fromNumber`'s compare uses `common::fEqual`.

## User Stories

1. As the maintainer, I want one Papyrus-cast rule, so that a game value reads the same in a config spec mod and an MCM mod.
2. As the maintainer, I want the rule tested directly, without files.

## Implementation Decisions

- **Where.** `asStoredType(value, type)` in `model/SettingValues.h`, from a value and a `StoredType` to the value in that type, or none (a text as a number, a number as a text).
- **A text is never read as a number** by `asStoredType`: the config spec store reports a text for a number setting as no value of its type. The MCM store keeps its own step before the cast: a text that `textNumber` reads becomes a float. Its `toNumber` stays too, which its other reads use.
- **Float compares** use `common::fEqual` / `fNotEqual`. A bool is off at a number within `fNotEqual`'s epsilon of 0, where the MCM store's `!= 0.0f` had it on; no setting holds such a number.
- **A whole number** is `std::lround` of the number, half away from 0, unclamped, as both copies did.

## Testing Decisions

- **New tests** in the setting value rules tests: bool, int and float from each value kind, a bool near 0, rounding a half, and the none cases.
- **Regression net.** Both stores' tests pass unchanged.

## Out of Scope

- The value shown while there is none, and MCM's reading of a global's number.
- Devbench's reading of a value from JSON.
- `filledArgument`'s cast of a call argument, which cuts a whole number as MCM's menu does.
