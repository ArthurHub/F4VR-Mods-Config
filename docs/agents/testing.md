# Tests

[CLAUDE.md](../../CLAUDE.md) has the commands. What the unit and e2e tests don't cover is tested by hand in the game.

## Unit tests

- Catch2, in `ModsConfigFO4VR_linked_tests`, built of the framework's test pieces ([mod-tests.md](../../external/F4VR-CommonFramework/docs/mod-tests.md)). A normal build builds it; `-DMOD_BUILD_TESTS=OFF` skips it.
- Every test is linked: the executable compiles the sources listed in `tests/CMakeLists.txt` with the mod's PCH and links the framework and CommonLibF4. A new source under test goes in that list, with its `<Name>Tests.cpp`.
- A test file sits under its source's `src/` area (`tests/linked/model/ConditionsTests.cpp`), with a name unique across folders. CTest names start `ModsConfigFO4VR: linked: <area>: ...`. Shared helpers sit in `tests/linked/`.
- `REL::NO_GAME` is set (`F4VRCommon::test_no_game`): calling the game crashes with an access violation. The game-calling sources are not compiled; the doubles below replace them.

## Test doubles

| Double | Replaces | Fill / read with |
| --- | --- | --- |
| `TestGlobals.cpp` | `GameGlobals.cpp` | `TestGlobals` (the map persists across tests; a `TestGlobals` starts it empty) |
| `TestProperties.cpp` | `GameProperties.cpp` | `TestProperties`: script properties, option lists, form names |
| `TestIniSettings.cpp` | `GameIniSettings.cpp` | `TestIniSettings`, names in lower case |
| `TestEvents.cpp` | `mcm/McmEvents.cpp` | `TestEvents` |
| `TestActions.cpp` | `GameActions.cpp` | `TestActions`: function calls, console commands, Save messages |
| `TestFiles.h` | — | a per-test folder for MCM's files; `ReadOnly` makes an unwritable file |
| `TestSwf.h` | — | an in-memory SWF |

All live in `tests/linked/`. MCM itself is never called in a test.

## Panel tests

- `ui/FullPanelTests.cpp` drives the panel in ImGui frames with no game or renderer (`TestPanel.h` sets it up as the framework's canvas does) and checks selection and presses.
- `ui/FullPanelRenderTests.cpp` saves the panel as `.bmp` files in `build/tests/render/` for a visual check before trying a change in the headset. They check nothing and `ctest` skips them: run `ModsConfigFO4VR_linked_tests "[render]"`. Add a case for a new view or state. They draw every page of both showcases and open pages by position, so moving a page moves its render.
- `tools/gen_docs_renders.py` (needs Pillow) turns the walkthrough renders into `docs/assets/showcase/` PNGs: rerun both when the panel's look or a showcase page changes.

## End-to-end tests

[tests/e2e/](../../tests/e2e/README.md): pytest against the running game through devbench, run by hand. A new devbench action or showcase page gets its e2e test.
