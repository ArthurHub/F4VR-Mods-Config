# 08. A read part of the Store interface for the model's read-only helpers

Part of [Deepen the hot spots](README.md), which holds the shared decisions, test seams and testing rules. Seam A. Blocked by [02](02-store-session.md).

## Problem Statement

The model's read-only helpers depend on the whole Store interface, which has 33 virtual methods. These helpers are conditions, search, unsaved pages and text states, and they use about five of them: value, value by id, whether a setting is dirty, client state and game INI setting.

As a result, the unsaved pages tests' store double overrides twenty-one methods to stub what it never uses. Every new store method is another stub in every double.

## Solution

The Store interface splits into a read part and the editing session that extends it. The read-only helpers take the read part. Both stores implement the whole interface as today.

## User Stories

1. As the maintainer, I want the model's read-only helpers (conditions, search, unsaved pages, text states) to depend only on the read part of the Store interface, so that their test doubles implement about five methods instead of twenty-one.
2. As the maintainer, I want a new editing method on the Store not to touch the read-only helpers' doubles, so that adding store features costs less in tests.
3. As an AI agent reading a helper, I want its parameter to say it only reads values, so that I know it cannot change the session.
4. As the maintainer, I want both stores to keep implementing everything they implement today, so that the change is invisible at runtime.

## Implementation Decisions

- **The split.** The Store interface becomes a read part (value, value by id, whether a setting is dirty, client state, game INI setting) and the editing session that extends it. The final membership of the read part follows what the helpers actually call.
- **Who takes the read part:** conditions, search, unsaved pages and text states. Others that only read can follow.
- **Unchanged.** Both stores implement the whole interface as today. The panel keeps the whole interface.
- **Order.** This lands after [02](02-store-session.md), which moves the lifecycle notifications out of the panel's call sites.

## Testing Decisions

- **Seam.** The model helpers' existing tests, with their store doubles shrunk to the read part.
- **Prior art:** the existing conditions, search, unsaved pages and text states tests.
- **Regression net.** All existing tests pass. The only change is smaller doubles.

## Out of Scope

- Changing any store method's behaviour.
- Removing methods from the Store interface.

## Further Notes

- Sixth in the suggested order.
