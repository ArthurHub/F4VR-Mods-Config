# 05. Binding wording and editing rules in the model

Part of [Deepen the hot spots](README.md), which holds the shared decisions, test seams and testing rules. Seam A.

## Problem Statement

A binding is put into words in two places, and they differ:

- `handWords` in [BindingEditor.cpp](../../../src/ui/BindingEditor.cpp) says "any hand", "off hand"; the one in [MenuControls.cpp](../../../src/ui/MenuControls.cpp) (the Help page) says "either hand", "the off hand".
- `buttonWords` is written twice, as a table and as a switch.
- The editor's `buttonOf` is the same mapping as `model::boundButton` in [InputBindings.cpp](../../../src/model/InputBindings.cpp).

The editor's rules for changing a binding (`hasType`, `withButton`, `fits`, `fittingType`, `holdingsOf` / `holdingOf`, `axisOf`) are private to its 972-line file and reached only through three FullPanel tests that press pixels.

## Solution

The binding's wording and its editing rules move into the model's input bindings module, as the transform tuning's maths lives apart from Adjust in world. The editor and the Help page read the same words; the editor only draws and calls the rules.

## User Stories

1. As a player, I want the Help page and the binding editor to name a hand and a button the same way.
2. As the maintainer, I want "a button the way doesn't fit changes the way" tested directly.
3. As the maintainer, I want one mapping from a push's axis to its button.

## Implementation Decisions

- **Wording.** One form for each hand and button. The editor's labels and the Help page's sentences may need the words with and without "the"; which forms the player sees is decided with the maintainer before writing.
- **Rules.** The editing rules become free functions of `model/InputBindings.h`, taking the binding and its setting's value options. `buttonOf` goes; the editor calls `boundButton`.
- **The editor keeps** its drawing, its open state and Back.

## Testing Decisions

- **New tests** in the input bindings tests: each rule's cases, and the words for each hand and button.
- **Moved tests.** The panel keeps its tests of opening the editor and Back; tests that only checked a rule through pixels move to the model.
- **Help page.** The Help page's binding sentence tests (`bindingSentence`) pass, changed only where the chosen wording changes them.

## Out of Scope

- The binding editor's look and layout.
- The framework's binding grammar and names.
