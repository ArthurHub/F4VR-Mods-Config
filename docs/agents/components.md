# Components

Each component's files and responsibility, plus the traps the code doesn't make obvious. The code is the source of truth for behavior; the rules it implements are in [docs/design/](../design/README.md). [CLAUDE.md](../../CLAUDE.md) maps the source folders.

## The mod and the menu

| Component | Files | Responsibility |
| --- | --- | --- |
| **ModsConfigFO4** | [src/ModsConfigFO4.h](../../src/ModsConfigFO4.h) / [.cpp](../../src/ModsConfigFO4.cpp) | The `ModBase` subclass and F4SE entry points; the opening gesture, taken over from FRIK. |
| **Config** | [src/Config.h](../../src/Config.h) / [.cpp](../../src/Config.cpp) | The mod's TOML via `ConfigBase` (`g_config`), hot-reloaded. See [config-and-data.md](config-and-data.md). |
| **ConfigMenu** | [src/ConfigMenu.h](../../src/ConfigMenu.h) / [.cpp](../../src/ConfigMenu.cpp) | The panel in the world and the wrist vrui buttons; owns the stores and feeds the Full panel everything game-side: Back, haptic pulses, thumbstick, images and textures, handedness, quick launch entries, Developer config. Native API requests (`requestOpen`, `requestMinimize`, `requestRestore`, `requestReadAgain`) and live-copy actions are queued and done in its frame update. Polls live copies every 0.5 s, also while closed. Fills the Categories page of its own config spec (`addCategoryRows`) and gives its rows' options through its own native client, with the choices pending on the page; a Save of `categories` re-reads `Categories.toml` and regroups. |
| **AdjustInWorld** | [src/AdjustInWorld.h](../../src/AdjustInWorld.h) / [.cpp](../../src/AdjustInWorld.cpp), [src/model/TransformTuning.h](../../src/model/TransformTuning.h) / [.cpp](../../src/model/TransformTuning.cpp) | Tuning a transform with the controllers while the panel is minimized. `TransformTuning` is the game-free maths (unit tested); `AdjustInWorld` reads the controllers, draws the wrist column and sets values in the store for the client to preview. Cancel, a game menu opening, or a client's open/restore request restores the start value. |
| **DevBench** | [src/devbench/MCDevBench.h](../../src/devbench/MCDevBench.h) / [.cpp](../../src/devbench/MCDevBench.cpp) | The mod's devbench tool: menu state and actions (open, select, page, setValue, clientSetValue, save, discard, dialog answers, …) and events (menu, Save/Discard, native client calls, MCM events). Add an action here and its e2e test for each new driveable feature. |

## UI (`src/ui/`, game-free)

Nothing in `src/ui/` touches the game, so the tests draw it. Looks follow the [design system](../design/design-system/README.md) component of the same name.

| Component | Files | Responsibility |
| --- | --- | --- |
| **FullPanel** | [src/ui/FullPanel.h](../../src/ui/FullPanel.h) / [.cpp](../../src/ui/FullPanel.cpp), [src/ui/Selection.h](../../src/ui/Selection.h) | The panel: title bar and breadcrumbs, tree, content (front page, report, mod page, Help), footer. Owns the selection and opens/closes each mod's store session; runs the search (`updateSearch`) and page filtering (`isFiltered`, `showWhole`); asks `UnsavedDialog` before leaving a dirty mod. MCM mods write at once (`writesChangesAtOnce`), so their Save/Discard read Keep/Revert. `replaceMod` swaps in a re-read config spec, dropping its unsaved changes. |
| **FrontPage** | [src/ui/FrontPage.h](../../src/ui/FrontPage.h) / [.cpp](../../src/ui/FrontPage.cpp) | Front page: introduction, quick launch rows, notices (restart pending, mods not available). Empty sections are left out. |
| **Developer** | [src/ui/Developer.h](../../src/ui/Developer.h) / [.cpp](../../src/ui/Developer.cpp) | Front page Developer section: filter (attention / all / one) and a row per config spec with its problem counts, opening its report. Never lists MCM mods. |
| **ConfigSpecReport** | [src/ui/ConfigSpecReport.h](../../src/ui/ConfigSpecReport.h) / [.cpp](../../src/ui/ConfigSpecReport.cpp) | A config spec's report: source file, live copy controls (Reload, Overwrite, Open, Show), errors and warnings; a press on a problem shows its row. |
| **MenuControls** | [src/ui/MenuControls.h](../../src/ui/MenuControls.h) / [.cpp](../../src/ui/MenuControls.cpp) | The Help page (`HelpPage`): the player's controls from the config's bindings and handedness, Save and the take-effect marks. `bindingSentence` says a binding in words. |
| **ModTree** | [src/ui/ModTree.h](../../src/ui/ModTree.h) / [.cpp](../../src/ui/ModTree.cpp) | The mod tree: mods, nested pages to the third level, `MCM` tag; flat search results (`drawFound`) with match counts and underlines; the category rows of a grouped list (`drawCategories`), whose open state is its own, for the session. |
| **SearchField** | [src/ui/SearchField.h](../../src/ui/SearchField.h) / [.cpp](../../src/ui/SearchField.cpp) | The tree's header search field on SteamVR's keyboard. Trap: on Escape ImGui restores the field's old text, so the field keeps its own copy of the query to keep what was typed. |
| **ModPage** | [src/ui/ModPage.h](../../src/ui/ModPage.h) / [.cpp](../../src/ui/ModPage.cpp) | A page's rows: sections, texts, images, every setting control (delegating to the components below), Reset / per-row Discard, marks, focus ring. Reports the pointed row and item (`pointedItem`) and what a press did. |
| **Sliders** | [src/ui/Sliders.h](../../src/ui/Sliders.h) / [.cpp](../../src/ui/Sliders.cpp) | Slider drawing and editing: trigger drag (written on release, Back cancels), step arrows and thumbstick (`StepRepeat`), value box fitting (`valueBoxText`). |
| **Numbers** | [src/ui/Numbers.h](../../src/ui/Numbers.h) / [.cpp](../../src/ui/Numbers.cpp) | Number formatting per a setting's `display` (NumberDisplay). |
| **Choices** | [src/ui/Choices.h](../../src/ui/Choices.h) / [.cpp](../../src/ui/Choices.cpp) | Steppers and dropdowns; `drawList` is the shared dropdown list popup (also used by `BindingEditor`). Haptic dropdowns draw via `Haptics`. A `typedOption` dropdown's list ends with Type a new name, picked as the place after the options; its row then edits the text with `TextInputs`. |
| **TextInputs** | [src/ui/TextInputs.h](../../src/ui/TextInputs.h) / [.cpp](../../src/ui/TextInputs.cpp) | Text input editing on SteamVR's keyboard: written on Done/Enter/press-outside if changed; close button, Escape, Back and leaving the page write nothing. Number filtering and `maxLength`. |
| **Bindings** | [src/ui/Bindings.h](../../src/ui/Bindings.h) / [.cpp](../../src/ui/Bindings.cpp), [src/model/InputBindings.h](../../src/model/InputBindings.h) / [.cpp](../../src/model/InputBindings.cpp) | `cf.inputBinding`: row of icons and sentence. `InputBindings` reads/writes the TOML table or INI line with the framework (`canonicalBinding`). |
| **BindingEditor** | [src/ui/BindingEditor.h](../../src/ui/BindingEditor.h) / [.cpp](../../src/ui/BindingEditor.cpp) | Binding editor popup: Hand, Button, How, Direction, Duration/Threshold, While holding, Suppress. Each change is set live; Cancel/Back restore the opening value. |
| **Colors** | [src/ui/Colors.h](../../src/ui/Colors.h) / [.cpp](../../src/ui/Colors.cpp), [src/ui/ColorEditor.h](../../src/ui/ColorEditor.h) / [.cpp](../../src/ui/ColorEditor.cpp), [src/model/Colors.h](../../src/model/Colors.h) / [.cpp](../../src/model/Colors.cpp) | `cf.color`: swatch row and editor (picker, channel sliders, presets). `model/Colors` parses/writes the table or INI line keeping the keys it had (`ColorTable`, `colorText`). `alpha: false` is RGB only. |
| **Transforms** | [src/ui/Transforms.h](../../src/ui/Transforms.h) / [.cpp](../../src/ui/Transforms.cpp), [src/ui/TransformEditor.h](../../src/ui/TransformEditor.h) / [.cpp](../../src/ui/TransformEditor.cpp), [src/model/Transforms.h](../../src/model/Transforms.h) / [.cpp](../../src/model/Transforms.cpp) | `cf.transform`: row of numbers and an editor with a slider per enabled axis, plus Adjust in world (disabled with `notPreviewed`'s reason). `model/Transforms` parses/writes keeping the keys it had (`axisValue`, `setAxis`). |
| **Haptics** | [src/ui/Haptics.h](../../src/ui/Haptics.h) / [.cpp](../../src/ui/Haptics.cpp), [src/model/Haptics.h](../../src/model/Haptics.h) / [.cpp](../../src/model/Haptics.cpp) | `cf.haptic`: the framework's patterns with their drawn shapes; Play and picking an option play it on the pointing hand (`hapticPlayed`). `canonicalHaptic` reads text as the framework does. |
| **Buttons** | [src/ui/Buttons.h](../../src/ui/Buttons.h) / [.cpp](../../src/ui/Buttons.cpp), [src/ui/ButtonIcons.h](../../src/ui/ButtonIcons.h) / [.cpp](../../src/ui/ButtonIcons.cpp) | Button rows: style, icon, confirmation, result message. A script call with unsaved changes asks first, since scripts read saved values (`SaveFirst`); console commands, links and DLL functions don't. |
| **Sections** | [src/ui/Sections.h](../../src/ui/Sections.h) / [.cpp](../../src/ui/Sections.cpp) | Folding sections: fold state, heading with invalid/pending counts, the nesting line. |
| **HelpLine** | [src/ui/HelpLine.h](../../src/ui/HelpLine.h) / [.cpp](../../src/ui/HelpLine.cpp) | The footer's help line: the pointed row's help with its tags, or the session status when no row is pointed. |
| **UnsavedDialog** | [src/ui/UnsavedDialog.h](../../src/ui/UnsavedDialog.h) / [.cpp](../../src/ui/UnsavedDialog.cpp) | The unsaved-changes dialog (Cancel / Discard / Save, or Revert / Keep for MCM). |
| **ConfirmDialog** | [src/ui/ConfirmDialog.h](../../src/ui/ConfirmDialog.h) / [.cpp](../../src/ui/ConfirmDialog.cpp) | A button's confirmation, including the Save-and-run variant. |
| **Drawing** | [src/ui/Drawing.h](../../src/ui/Drawing.h) / [.cpp](../../src/ui/Drawing.cpp) | Shared drawing primitives: wrapped/marked text, marks and tags, dialogs (`beginDialog`, `dialogButtons`), folding headings, value looks (`ValueLook`). |

## Model (`src/model/`, game-free)

| Component | Files | Responsibility |
| --- | --- | --- |
| **Model** | [src/model/ConfigDefinition.h](../../src/model/ConfigDefinition.h) | The settings model the UI draws: mods, pages, items, settings with storage, conditions, actions, warnings, problems. See [model.md](../design/model.md). |
| **Store** | [src/model/Store.h](../../src/model/Store.h) | The interface the UI asks for every value: read/set, validity, dirty, Save/Discard, defaults, options, client state, actions (`ActionResult`, `notRunnable`), `readAgain`, `showPage`/`showPointed`. A session opens on selecting a mod and closes on leaving it. |
| **Conditions** | [src/model/Conditions.h](../../src/model/Conditions.h) / [.cpp](../../src/model/Conditions.cpp) | Evaluating conditions. A condition left open by an unavailable value holds (row shown) but does not `surelyHolds` (warning not told). |
| **Search** | [src/model/Search.h](../../src/model/Search.h) / [.cpp](../../src/model/Search.cpp) | `searchMod`: word-prefix, accent- and case-folded matching (CJK: substring); the most specific match wins. `matchRanges` for underlines. A `notSearched` setting isn't matched by its text. |
| **Categories** | [src/model/Categories.h](../../src/model/Categories.h) / [.cpp](../../src/model/Categories.cpp) | The shipped table (`Categories.json`), the player's choices (`Categories.toml`), a mod's category (`categoryOf`: choice, own, table), the grouping of the tree, and the rows of the Categories page with their options. Names match case-insensitively (`categoryKey`). |
| **Option sets** | [src/model/OptionSets.h](../../src/model/OptionSets.h) / [.cpp](../../src/model/OptionSets.cpp) | A choice whose options set other settings (`SetsStorage`): matching an option's value with a setting's (`hasSetValue`, a color's and a transform's on the parts it names) and setting it (`withSetValue`). The config spec store derives the choice's value, dirty state, default and Discard from those settings; the change counts skip it. An option's `alsoSets` are set by a pick and count for dirty state and Discard, but never decide the value. |
| **Choice options** | [src/model/ChoiceOptions.h](../../src/model/ChoiceOptions.h) / [.cpp](../../src/model/ChoiceOptions.cpp) | Options of a mod's choices: own, from its client, or read in the game (`getOptionTexts`). `typedOptionValue` turns a typed text into a value. |
| **Text markup** | [src/model/TextMarkup.h](../../src/model/TextMarkup.h) / [.cpp](../../src/model/TextMarkup.cpp) | MCM-style tags (`<br>`, `<p align>`, `<font>`, `<b>`, `<i>`, `<u>`, entities). Font sizes are MCM menu sizes, scaled by `MARKUP_BODY_SIZE` in `Drawing.cpp`. |
| **Translations** | [src/model/Translations.h](../../src/model/Translations.h) / [.cpp](../../src/model/Translations.cpp), [src/GameTranslations.h](../../src/GameTranslations.h) / [.cpp](../../src/GameTranslations.cpp) | `$` token translation; `loadGameTranslations` copies the game's table once. |

## Config specs (`src/spec/`)

| Component | Files | Responsibility |
| --- | --- | --- |
| **Config spec loader** | [src/spec/ConfigSpecLoader.h](../../src/spec/ConfigSpecLoader.h) / [.cpp](../../src/spec/ConfigSpecLoader.cpp) | Reads `spec.json` files under `Data\ModsConfigFO4\Config` into the model per [schema.md](../design/schema.md), recording each mistake as a `Problem` with its row place and JSON path. The allowed fields per object are lists in the loader: update them with the schema. `cf.activationSphere` expands to a section of made rows (`sphereSection`, `MADE_ROW_PATH`). F4SE VR can't query an F4SE plugin, so `PluginChecks` registers a listener, which F4SE refuses for a plugin that isn't loaded. `withConfigSpecs` hides an MCM mod that a config spec covers. |
| **Config spec store** | [src/spec/ConfigSpecStore.h](../../src/spec/ConfigSpecStore.h) / [.cpp](../../src/spec/ConfigSpecStore.cpp) | A config spec mod's editing session: baselines from files, globals, properties and client state; pending/dirty/invalid; live preview; the all-or-nothing Save of [architecture.md](../design/architecture.md#save-lifecycle); `readAgain`. An `omitDefault` value equal to its default is a key removed on Save. Trap: a `typedOption` choice's options are read again as it is set or discarded, and after `readAgain`, as its client offers the pending values. |
| **TOML file** | [src/spec/TomlFile.h](../../src/spec/TomlFile.h) / [.cpp](../../src/spec/TomlFile.cpp) | TOML values by path; writes with the framework's `TomlEditor`, keeping comments and layout. Tables (binding, color, transform) are read as their text (`tableText`). A removed key goes with `TomlEditor::remove`, after the sets; INI has no remove. |
| **INI file** | [src/spec/IniFile.h](../../src/spec/IniFile.h) / [.cpp](../../src/spec/IniFile.cpp) | INI values as SimpleIni reads them (case-insensitive, last key wins), but a `;` or `#` after a value and a space is a trailing comment; line-by-line writes that keep other lines and the file's bool and hex style. |
| **Mod file** | [src/spec/ModFile.h](../../src/spec/ModFile.h) / [.cpp](../../src/spec/ModFile.cpp) | A file's Save steps: `prepare` (new text, checked to read back), `commit` (write, creating folders), `restore` (undo after a failed Save). |
| **Live copies** | [src/spec/LiveCopies.h](../../src/spec/LiveCopies.h) / [.cpp](../../src/spec/LiveCopies.cpp) | Authors' live copies of spec.json in `My Games\…\ModsConfigFO4VR\Dev\<folder>\`: create, `overwrite`, `isStale`, and detecting saves (ignoring writes under 200 ms old, which an editor may still be writing). |

## MCM (`src/mcm/`)

| Component | Files | Responsibility |
| --- | --- | --- |
| **MCM loader** | [src/mcm/McmLoader.h](../../src/mcm/McmLoader.h) / [.cpp](../../src/mcm/McmLoader.cpp) | Reads `Data\MCM\Config\*\config.json` into the model while `mcm_vr.dll` is loaded, per [mcm.md](../design/mcm.md). Text flags are truthy for any non-empty text, as in Flash. |
| **MCM store** | [src/mcm/McmStore.h](../../src/mcm/McmStore.h) / [.cpp](../../src/mcm/McmStore.cpp) | An MCM mod's session. Save applies changes in the order the player last made them, each with its value-row function and `OnMCMSettingChange`, as MCM applies each at once. After writing a ModSetting it sends pause-menu open/close (DLLs reread MCM files then), but only once MCM's file has the values (`fileHas`) or after `PAUSE_MENU_WAIT_FRAMES`, since MCM writes from Papyrus; hence `frameUpdate` runs while the menu is closed. |
| **MCM ModSettings** | [src/mcm/McmModSettings.h](../../src/mcm/McmModSettings.h) / [.cpp](../../src/mcm/McmModSettings.cpp) | ModSettings from `settings.ini` (default) and `Settings\<mod>.ini` (player). Set via a queued `MCM.SetModSetting*` call, which returns nothing, so the written text is kept here. |
| **MCM keybinds** | [src/mcm/McmKeybinds.h](../../src/mcm/McmKeybinds.h) / [.cpp](../../src/mcm/McmKeybinds.cpp) | Hotkeys and bound keys from MCM's keybinds JSON files, read-only; `keyName` as MCM names keys. |
| **MCM events** | [src/mcm/McmEvents.cpp](../../src/mcm/McmEvents.cpp) | `sendEvent` (MCM external events to Papyrus) and `sendPauseMenuEvents`. |
| **MCM game texts** | [src/mcm/McmGameTexts.h](../../src/mcm/McmGameTexts.h) / [.cpp](../../src/mcm/McmGameTexts.cpp) | `readGameTexts`: a row's `textFrom…` / `helpFrom…` read from forms and properties. |
| **FallUI translations** | [src/mcm/FallUITranslations.h](../../src/mcm/FallUITranslations.h) / [.cpp](../../src/mcm/FallUITranslations.cpp) | Re-implements the translation that FallUI's `lib.swf` code does, since the menu runs no mod code. Applied before the game's translations. |
| **SWF images** | [src/mcm/SwfImages.h](../../src/mcm/SwfImages.h) / [.cpp](../../src/mcm/SwfImages.cpp) | Lossless bitmaps out of a `lib.swf` by class name, without Flash. |
| **MCM config images** | [src/mcm/McmConfigImages.h](../../src/mcm/McmConfigImages.h) / [.cpp](../../src/mcm/McmConfigImages.cpp) | An MCM image row's picture at MCM's size; `loadMain` for M8r intro logos (2/3 size). |

## Game access

The unit tests don't compile these `.cpp` files; they link doubles instead ([testing.md](testing.md)).

| Component | Files | Responsibility |
| --- | --- | --- |
| **Game actions** | [src/GameActions.h](../../src/GameActions.h) / [.cpp](../../src/GameActions.cpp) | `callFunction` (as `mcm_vr.dll` does, queued to Papyrus), `runConsoleCommand`, `openWebLink`, `sendSavedMessage` (F4SE message of a Save). |
| **Game globals** | [src/GameValues.h](../../src/GameValues.h), [src/GameGlobals.cpp](../../src/GameGlobals.cpp) | `getGlobal` / `setGlobal`, form looked up once. |
| **Game properties** | [src/GameValues.h](../../src/GameValues.h), [src/GameProperties.cpp](../../src/GameProperties.cpp) | `getProperty` / `setProperty` as `mcm_vr.dll` does; `getOptionTexts` (FormList or string array); `getFormText`. |
| **Game INI settings** | [src/GameValues.h](../../src/GameValues.h), [src/GameIniSettings.cpp](../../src/GameIniSettings.cpp) | `getGameIniSetting`: prefs then INI, by name in any case. |

## Native API (`src/api/`)

| Component | Files | Responsibility |
| --- | --- | --- |
| **Native API** | [src/api/ModsConfigFO4Api.h](../../src/api/ModsConfigFO4Api.h), [src/api/NativeClients.h](../../src/api/NativeClients.h) / [.cpp](../../src/api/NativeClients.cpp), [src/api/NativeApi.cpp](../../src/api/NativeApi.cpp), [src/model/ClientCalls.h](../../src/model/ClientCalls.h) / [.cpp](../../src/model/ClientCalls.cpp) | The [native API](../design/architecture.md#native-api). The header holds the ABI (`ModsConfigFO4ApiV1`, `IClient`) and the C++ client `ModsConfigFO4Client`; keep it identical to the framework's copy, or the linker warns about the client's two sizes. `NativeClients` matches clients to config specs by `modName` (case-insensitive); `ClientCalls` calls them under an SEH guard and cuts a faulting client off until restart. `setPendingValue` is refused from inside `onChange`/`onSaved`, while the dialog asks, or when the mod isn't shown. |
| **FRIK API** | [src/api/FRIKApi.h](../../src/api/FRIKApi.h) | FRIK's client header (API v4), copied as is: never edit it. |

Add a row when you add a component.
