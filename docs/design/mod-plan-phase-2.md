# Mod work plan, phase 2: First release

The commits of milestones 5 to 10: config specs and the native API, up to the first release. Done. The [Mod work plan](mod-plan.md) has how the work is done and each milestone's "done when"; the rules these commits settled are in the design docs, and the per-commit notes are at `git show 6a0be38:docs/design/mod-plan-phase-2.md`.

## 5. Config specs with Save

A config spec's features match MCM's, but its lifecycle is new. The milestone defines the config spec and builds its core: settings that are read from a TOML file, edited and saved.

| #   | Repo | Commit                                                                                    | Status |
| --- | ---- | ----------------------------------------------------------------------------------------- | ------ |
| M103 | mod | docs: what a config spec has beyond MCM                                                      | done   |
| M47 | mod  | The config spec loader: files, pages, sections and settings, and Mods Config FO4 VR's own config spec | done   |
| M114 | mod | The config spec showcase, an optional file for developers                                    | done   |
| M48 | mod  | Choices, shared lists and conditions in a config spec                                         | done   |
| M89 | mod  | A TOML file is read, changed and written with its comments and order kept                 | done   |
| M51 | mod  | The editing session: values from the file, pending values, dirty state                    | done   |
| M52 | mod  | Save writes the dirty settings, and Discard                                               | done   |
| M56 | mod  | Reset to the default                                                                      | done   |
| M115 | mod | The title bar's Help, Minimize and Close, and the Help page                               | done   |

## 6. Config specs complete

The milestone builds on milestone 5 to complete config specs: the rows that are left, what M103 added beyond MCM, buttons, globals and properties, INI files, the rest of the Save steps, and the checks of a config spec.

| #   | Repo | Commit                                                                                    | Status |
| --- | ---- | ----------------------------------------------------------------------------------------- | ------ |
| M59 | mod  | Text, spacers and images in a config spec                                                     | done   |
| M108 | mod | A section folds, and marks what it hides                                                  | done   |
| M109 | mod | Pages under pages: the tree's levels, the breadcrumbs and Back                            | done   |
| M111 | mod | A read-only setting, and how a number is shown                                            | done   |
| M112 | mod | A row warns while a condition holds                                                       | done   |
| M49 | mod  | A config spec's button calls a Papyrus function or runs a console command, with its own text  | done   |
| M107 | mod | A config spec's setting kept in a global or a script property, and its action run by Save     | done   |
| M110 | mod | A choice's options from a FormList or a script's property, in MCM and in a config spec        | done   |
| M50 | mod  | An INI file is read, changed and written with its comments and order kept                 | done   |
| M53 | mod  | A Save that fails leaves every file as it was                                             | done   |
| M54 | mod  | Save creates a file that is missing                                                       | done   |
| M55 | mod  | Validation                                                                                | done   |
| M116 | mod | The save summary                                                                          | rejected |
| M57 | mod  | The unsaved-changes prompt                                                                | done   |
| M58 | mod  | A row says when its change takes effect, and Save names a needed restart                  | done   |
| M60 | mod  | A config spec whose requirements are missing is not available                                 | done   |
| M61 | mod  | A config spec's errors are reported, in the log and to its author in the menu                 | done   |
| M117 | mod | A live copy of a config spec's spec.json, read again as it is saved                         | done   |

## 7. Actions and own UIs

| #   | Repo      | Commit                                                                                      | Status |
| --- | --------- | ------------------------------------------------------------------------------------------- | ------ |
| M62 | mod       | docs: the native API: its binary interface, its versions and its calls                      | done   |
| M63 | mod       | The native API: a client registers by name, with the client header                          | done   |
| F28 | framework | `ModBase` registers the mod as a client                                                     | done   |
| M64 | mod       | A config spec's button runs its client's function, with the confirmation dialog             | done   |
| M118 | mod      | A condition asks a state of the mod's client                                                | done   |
| M113 | mod      | A client is told of the menu's events: the menu, its mod and its pages shown and left       | done   |
| M65 | mod       | A client minimizes and restores the menu, and opens it at its mod and page                  | done   |
| M66 | mod       | The quick launch section takes the mods that registered for it                              | done   |
| F29 | framework | `ModBase` connects the header's C++ client, to which a mod adds what its client answers | done   |
| M119 | mod      | The native API is two interfaces: the host asks a client by name, with a C++ client in the header | done   |
| M67 | mod       | FRIK's config is opened from its quick launch entry, and the menu comes back when it closes | done   |

## 8. Live preview

| #   | Repo      | Commit                                                   | Status |
| --- | --------- | -------------------------------------------------------- | ------ |
| M120 | mod      | docs: Save for every mod, and `applies` that is not known | done   |
| M121 | mod      | An MCM mod's changes wait for a Save                     | done   |
| M122 | mod      | A script button of a mod with unsaved changes asks to save first | done   |
| M123 | mod      | `applies` is `unknown` where nothing says it, and the help line says it | done   |
| M124 | mod      | design system: the marks of `applies`, and the Help page's legend | done   |
| M125 | mod      | The marks of `applies` on a pending row and in the help line, and the Help page's legend | done   |
| M68 | mod       | A client gets the preview and discard events             | done   |
| M69 | mod       | A client gets the saved batch                            | done   |
| F30 | framework | `ModBase`'s client previews through the config overrides | done   |
| F31 | framework | config: a subscriber is told the change of its keys         | done   |
| M70 | mod       | The F4SE message after a Save                            | done   |
| M126 | mod      | docs: a file's path from its client                      | done   |
| F35 | framework | `ModBase`'s client answers the path of its config file   | done   |
| M127 | mod      | A file takes its path from a state of its client         | done   |
| M128 | mod      | design system: a pending row's Discard                  | done   |
| M129 | mod      | A pending row's Discard                                  | done   |
| M130 | mod      | A mod with a config spec and an MCM menu is listed by its config spec | done   |
| M134 | mod      | docs: a setting kept in a state of its client            | done   |
| M135 | mod      | A setting kept in a state of its client                  | done   |

## 9. CommonFramework controls

| #   | Repo | Commit                                                                                 | Status |
| --- | ---- | -------------------------------------------------------------------------------------- | ------ |
| M74 | mod  | docs: the CommonFramework controls, and how each key of Immersive Flashlight is edited | done   |
| M75 | mod  | Color                                                                                  | done   |
| M77 | mod  | Haptic                                                                                 | done   |
| M78 | mod  | InputBinding                                                                           | done   |
| M79 | mod  | Transform: its row, and its editor of sliders                                          | done   |
| M142 | mod | Transform: Adjust in world, tuned from the wrist                                       | done   |
| M80 | mod  | ActivationSphere, a whole section                                                      | done   |
| M76 | mod  | A choice of the files in a folder                                                      | done   |
| F36 | framework | vrcf: a binding of any hand                                                       | done   |
| F37 | framework | vrui: the world move is a binding                                                 | done   |
| M131 | mod | Back and the move are bindings of any hand                                             | done   |
| M132 | mod | A binding takes the hands, the types and the buttons its config spec names             | done   |

## 10. First release

| #   | Repo | Commit                                                                  | Status |
| --- | ---- | ----------------------------------------------------------------------- | ------ |
| M136 | mod | docs: an MCM change is made as it is made, and Discard sets it back     | done   |
| M137 | mod | design system: the Help page and the buttons of an MCM mod              | done   |
| M138 | mod | An MCM change is made as it is made, and Discard sets it back           | done |
| M139 | mod | An MCM mod has the MCM tag in the tree                                  | done |
| M140 | mod | An MCM mod's Save bar says Keep and Revert                              | done |
| M141 | mod | A config spec's button has a style and an icon                          | done |
| M82 | mod  | docs: the API and the config spec schema only grow from version 1.0     | done   |
| M142 | mod | A mod's own UI hands its edits back to the menu as pending values       | done   |
| M83 | mod  | docs: a loading screen ends the editing session                         | done   |
| M84 | mod  | The menu without FRIK, and without MCM                                  | done   |
| M146 | mod | A color's editor has basics and its config spec's presets              | done |
| M148 | mod | docs: the Nexus page                                                   | done   |
| M149 | mod | docs: the mod's README                                                 | done   |
| M150 | mod | docs: the README of the docs, and the FAQ                              | done   |
| M151 | mod | docs: the plan of the docs for players and for mod authors            | done   |
| M157 | mod | docs: the menu's own settings, and the changelog                       | done   |
| M85 | mod  | docs: for mod authors                                                   | done   |
| M153 | mod | docs: the showcase walkthrough, with renders of the panel              | done   |
| M152 | mod | docs: MCM compatibility, what a config spec adds, and moving from MCM  | done   |
| M156 | mod | docs: the native API, for DLL authors                                  | done   |
| M158 | mod | An MCM page's own plugin requirements                                  | done   |
| M159 | mod | An MCM row's text from the game                                        | done   |
| M160 | mod | A state of the mod's client in a row's text and help                   | done   |
| M161 | mod | devbench: a page's rows as the panel draws them, and the status        | done   |
| M162 | mod | devbench: the errors and warnings of each config spec                  | done   |
| M163 | mod | devbench: the menu's events, and what clients and MCM scripts are told | done   |
| F38 | framework | devbench: a snapshot of the ImGui canvases as a PNG                  | done   |
| F39 | framework | devbench: the input suppressed now is read, and is no event          | done   |
| M164 | mod | devbench: a picture of the panel                                       | done   |
| D1 | devbench | files: list, read, write and delete files as the game sees them      | done   |
| M165 | mod | The end-to-end tests, run through devbench                             | done   |
| M154 | mod | The release tested in the headset                                      | done   |
| F40 | framework | perf: the ImGui canvases' and the vrui panels' frame end             | done   |
| M155 | mod | The release perf tested                                                | done   |
| M166 | mod | A color, binding and transform parse their value once per text        | done   |
| F41 | framework | config: a reload of an unchanged file parses nothing                 | done   |
| M167 | mod | A dragged live setting is told to its mod at most every 50 ms          | done   |
| M168 | mod | The menu's frame cost checked against its budgets                      | done   |
