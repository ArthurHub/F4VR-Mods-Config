# 01. Setting value rules in one module per setting type

Part of [Deepen the hot spots](README.md), which holds the shared decisions, test seams and testing rules. Seam A.

## Problem Statement

What a value of a setting type is gets decided in many places: whether two values are the same, a value's canonical text, why it is invalid, how it is read from JSON, and whether it is kept as a table.

- Comparing two values is copied word for word into the config spec loader, the config spec store, the MCM store and the setting row. Conditions and option sets have their own variants.
- Whether a setting type is kept as a table (binding, color, transform) is spelled out four times.
- The loader and the config spec store disagree on whether a haptic is a choice.
- The config spec store ends two functions in the same switch over binding, haptic, color and transform, and repeats the type list in its invalid reasons.
- The loader reads defaults and option set values per type twice.
- The setting row switches over the types again.
- The color editor, the transform editor and Adjust in world each write "parse the value, else fall back to the default".

Adding cf.transform touched 17 source files. Each new setting type pays this again, and a fix in one copy leaves the others behind.

## Solution

Each setting type's value rules live in its own module in the model. One entry point by setting type answers them for any setting. The loader, both stores, the UI and the editors ask it, and their own copies go. A new setting type is mostly one new module.

## User Stories

1. As the maintainer, I want each setting type's value rules in one module, so that adding a setting type means writing one module rather than editing seventeen files.
2. As the maintainer, I want one answer to "are these two values of this setting the same", so that the loader, the two stores and the UI cannot disagree about whether a value changed.
3. As the maintainer, I want one answer to "is this setting type kept as a table", so that bindings, colors and transforms are treated alike everywhere.
4. As the maintainer, I want one answer to "is this setting a choice", so that a haptic is a choice in the loader and the store alike.
5. As the maintainer, I want one answer to "what is the canonical text of this value", so that a binding, color, transform or haptic is compared and written the same way in every caller.
6. As the maintainer, I want one answer to "why is this value invalid for its setting", so that the player sees the same reason whether it was read from a file or typed.
7. As the maintainer, I want one way to read a setting's value from JSON, so that a config spec's default and an option's set value parse values the same way.
8. As the maintainer, I want one "parse the value, else fall back to the default" step for the editors, so that the color editor, the transform editor and Adjust in world start from the same value.
9. As an AI agent adding a setting type, I want one obvious module to extend, so that I don't have to find every switch over setting types across the loader, the stores and the UI.
10. As the maintainer, I want each setting type's rules tested directly, so that a bug in one type's equality or canonical text shows up in a small test, not in a large loader or panel test.
11. As a mod author, I want my config spec's errors and warnings reported exactly as today, so that the change doesn't alter what the report tells me.
12. As a player, I want every control to show and save the same values as today, so that the change is invisible to me.

## Implementation Decisions

- **Where the rules live.** The model's existing color, transform, input binding and haptic modules become the home of their type's value rules: equality, canonical text, why a value is invalid, reading a value from JSON, the default when parsing fails, and whether the value is kept as a table.
- **One entry point.** `model/SettingValues.h`, free functions keyed by setting type that answer those questions for any setting, including the plain types (switcher, slider, stepper, dropdown, text input). Callers no longer switch over setting types themselves for these questions. A function takes the pieces it needs (type, value type, table form, value options), so the loader asks it before a setting exists, with a `Setting` overload for the stores and the UI.
- **Kept plain.** One switch by setting type over the type modules, not a registry or plugin mechanism.
- **Callers that move to it:**
  - the loader's default reading, set-value reading and value-of-setting check. `readJsonValue` returns the kind of failure and the framework's reason, and each caller keeps its own sentence, so the report does not change;
  - both stores' value comparison and canonical forms, and the config spec store's invalid reasons;
  - the setting row's default and edit-control decisions;
  - the editors' parse-else-default, which returns a type's own table, so it lives in each type module (`colorOrDefault`, `transformOrDefault`, `bindingOrDefault`).

  Their private copies are deleted.
- **"Is a choice".** One definition: a stepper, a dropdown and a haptic. The loader asked a different question in one place, whether the author lists the options in the config spec, which stays the loader's own (`listsOptions`: a stepper and a dropdown).
- **Equality.** `sameValue` compares two values: numbers with `fEqual`, any other two with `==`. Callers compare canonical values. Conditions (an int equals a float, as an author writes `1` for `1.0`) and option sets (the parts of a table an option sets) keep their own rules.
- **Devbench.** Devbench keeps its own reading of a value: it takes a table as its TOML text or INI line, which the e2e tests send, and a choice by its option's text.
- **What stays.** Type-specific drawing (swatches, icons, axis sliders) stays in the UI. Type-specific editing helpers that aren't value rules (a binding's taken hands, a transform's axis) stay where they are. So do the type names of the config spec schema (`cf.color`), which the loader and devbench spell.

## Testing Decisions

- **Through the one entry point.** Each setting type gets table-driven tests: equality, canonical text, invalid reasons, reading from JSON, parse-else-default, and the table and choice answers.
- **Prior art:** the existing color, transform, input binding and option sets tests in the model. A haptic test is added; there is none today.
- **Regression net.** The config spec loader, config spec store, MCM store and panel tests pass unchanged. Their assertions on loader errors and warnings must not move.
- **Moved tests.** Tests that only exercised a deleted private copy move to the entry point's tests.

## Out of Scope

- Changing which values a setting type accepts, or its JSON or file forms.
- Splitting the loader beyond routing its value reading through this module.
- The pending-changes counting, which is [05](05-pending-marks.md).

## Further Notes

- First in the suggested order: it has the widest fan-out and needs no new seam.
- The glossary may gain "setting value rules" if the entry point is named after it.
