# Mod work plan, phase 2: First release

The commits of milestones 5 to 11 of the [Mod work plan](mod-plan.md): packages and the native API, up to the first release. The plan has how the work is done, the MCM test set and the status of every milestone.

## 5. Packages with Save

A package's features match MCM's, but its lifecycle is new. The milestone defines the package and builds its core: settings that are read from a TOML file, edited and saved.

| #   | Repo | Commit                                                                                    | Status |
| --- | ---- | ----------------------------------------------------------------------------------------- | ------ |
| M103 | mod | docs: what a package has beyond MCM                                                      | done   |
| M47 | mod  | The package loader: files, pages, sections and settings, and Mods Config VR's own package | in review |
| M114 | mod | The package showcase, an optional file for developers                                    | in review |
| M48 | mod  | Choices, shared lists and conditions in a package                                         | in review |
| M89 | mod  | A TOML file is read, changed and written with its comments and order kept                 | todo   |
| M51 | mod  | The editing session: values from the file, pending values, dirty state                    | todo   |
| M52 | mod  | Save writes the dirty settings, and Discard                                               | todo   |
| M56 | mod  | Reset to the default                                                                      | todo   |

M103 decides what a package should support beyond MCM, and what it has in place of what MCM's authors work around: [A package and MCM](schema.md#a-package-and-mcm). The milestones build it:

- M47 reads the package's pages and sections, which nest, and the help of the mod and of its pages. A section in a section is drawn with a heading of its own. The tree shows a page's pages from M109.
- M48 has every operator of a condition, `enabledWhen`, the conditions on a loaded plugin, and shared lists.
- Milestone 6 has the rest: sections that fold, pages under pages, read-only settings and how a number is shown, a button's console command and its text, globals and properties with the action of a setting, and options from the game.

M114 adds a package of our own, `data/package-showcase/`, as the MCM showcase is for MCM: what a package has, each kind on a page, and what a package can get wrong, with a TOML file and an INI file of its values beside it. Its front page has a row of every kind, so a row the menu does not draw yet is a placeholder there until the commit that draws it. It is tried in the headset as each commit reads or draws something of a package, and grows with them. It is packaged by itself and published as an optional file, to show a mod's author what a package does in the mod.

M47 ships the first package, which is this mod's own, for `ModsConfigVR.toml`. The mod reloads its TOML when it changes, so a Save shows at once with no other mod involved. A package's text takes the three forms of [Localization](schema.md#localization): plain, a `$` token, and a token with a fallback. The lookup of a token is [M17](mod-plan-phase-1.md#1-browse-mcm-mods)'s.

The milestone is TOML only. A CommonFramework mod keeps its config in a TOML file, as this mod does, and reloads it by itself, so it needs no native API.

M52 follows the Save steps of [Architecture and lifecycle](architecture.md#save-lifecycle), for the Save that goes well.

M56 is the Reset of a package's setting. The button of a row and the default in the help line are [M102](mod-plan-phase-1.md#3-mcm-complete)'s. The package store answers the `default` of the setting, and a Reset is a pending value as any other: Save writes it, and Discard takes it back.

## 6. Packages complete

The milestone builds on milestone 5 to complete packages: the rows that are left, what M103 added beyond MCM, buttons, globals and properties, INI files, the rest of the Save steps, and the checks of a package.

| #   | Repo | Commit                                                                                    | Status |
| --- | ---- | ----------------------------------------------------------------------------------------- | ------ |
| M59 | mod  | Text, spacers and images in a package                                                     | todo   |
| M108 | mod | A section folds, and marks what it hides                                                  | todo   |
| M109 | mod | Pages under pages: the tree's levels, the breadcrumbs and Back                            | todo   |
| M111 | mod | A read-only setting, and how a number is shown                                            | todo   |
| M112 | mod | A row warns while a condition holds                                                       | todo   |
| M49 | mod  | A package's button calls a Papyrus function or runs a console command, with its own text  | todo   |
| M107 | mod | A package's setting kept in a global or a script property, and its action run by Save     | todo   |
| M110 | mod | A choice's options from a FormList or a script's property, in MCM and in a package        | todo   |
| M50 | mod  | An INI file is read, changed and written with its comments and order kept                 | todo   |
| M53 | mod  | A Save that fails leaves every file as it was                                             | todo   |
| M54 | mod  | Save creates a file that is missing                                                       | todo   |
| M55 | mod  | Validation                                                                                | todo   |
| M57 | mod  | The unsaved-changes prompt                                                                | todo   |
| M58 | mod  | A row says when its change takes effect, and Save names a needed restart                  | todo   |
| M60 | mod  | A package whose requirements are missing is not available                                 | todo   |
| M61 | mod  | A package's errors are reported                                                           | todo   |

M108, M109, M111 and M112 draw what M103 added to the page and the tree:

- M108: a section that folds opens and closes with a press on its heading, starts as its `collapsed` says each time its page is shown, and marks its heading while it hides an invalid value or an unsaved change.
- M109: a page's pages are rows under it in the tree, a level further in, with a crumb for each level, and Back goes a level up.
- M111: a read-only setting, and the decimals, the unit and the percentage of a number's `display`.
- M112: a setting or a button whose warning holds has a warning mark, and the help line shows the warning before the row's help.

M49 lets a package use the actions the menu supports for MCM, so a script-only mod has buttons, and a console command, which needs no script. The action of a native client comes in milestone 7.

M107 lets a script mod keep its package's settings where its scripts and the game read them, as an MCM mod does. A global and a property are pending until Save, as a key of a file is, and the store previews a `live` one by writing it into the game. Save writes them after the files, then runs the action of each setting it wrote, with `"{value}"`. It is tried with a package written for one of the test set's MCM mods that keeps its values in globals.

M110 reads MCM's `listFromForm` and `listFromProperty`, for an MCM config and a package alike. The store answers the options of a choice, which it reads in the game as the page is shown: until then they are fixed in the definition. It comes after M107, which reads a property of a script for a package. No MCM config of the author's lists has either, so the unit tests cover it.

M50 is for the many F4SE mods that keep their config in an INI file, and for a CommonFramework mod on an older framework.

M53 and M54 are the rest of the Save steps. M53 is the checks before the write, and the rollback of a write that fails.

M58 shows a package's `applies`. The help line says when the change of the pointed row takes effect, a row that needs a restart is marked, and a Save that wrote one says that the game has to be restarted. The loader reads `applies` from M47 on, as a package's root requires it. Until milestone 8 there is no client, so a `live` setting is handled as `save`.

M61 reports a package's errors in the log. It also weighs a page in the menu with each package's errors, for its author, as DearModdingUI's Health page has them.

## 7. Actions and handoff

| #   | Repo      | Commit                                                                            | Status |
| --- | --------- | --------------------------------------------------------------------------------- | ------ |
| M62 | mod       | docs: the native API's binary interface                                           | todo   |
| M63 | mod       | The native API: a client registers by name, with the client header                | todo   |
| F28 | framework | `ModBase` registers the mod as a client                                           | todo   |
| F29 | framework | A mod adds its functions, its own config UI and its page events to its client     | todo   |
| M64 | mod       | A package's button runs its client's function, with the confirmation dialog       | todo   |
| M113 | mod      | A client is told when a page of its package is shown and left                     | todo   |
| M65 | mod       | Handoff: an action opens the mod's own UI, and the menu comes back when it closes | todo   |
| M66 | mod       | The quick launch section takes the mods that registered for it                    | todo   |
| M67 | mod       | FRIK's config is opened through the handoff                                       | todo   |

M62 settles what the API passes between DLLs, how its text is encoded, and how its versions are told apart. It also settles what a package writes for a button that opens the mod's own UI.

M63, F28 and F29 are the first part of item 13 of the requirements. The client header is vendored in the framework, as devbench's is. M63 calls a client inside the guard of [A client that fails](architecture.md#a-client-that-fails), so a client whose call fails is cut off and the menu keeps working.

M113 sends the [page events](architecture.md#page-events): a page of a package that has an `id` tells its client as it is shown and left. Immersive Flashlight draws its activation spheres in the world while their page is shown.

M67 needs a FRIK that is built on a framework with F29. That is a commit in FRIK's repository, made when this commit needs it. A build of FRIK is enough: it does not have to be released. The quick launch entry needs no package. FRIK's entry in the quick launch section is then there when FRIK registers for it.

## 8. Live preview

| #   | Repo      | Commit                                                   | Status |
| --- | --------- | -------------------------------------------------------- | ------ |
| M68 | mod       | A client gets the preview and discard events             | todo   |
| M69 | mod       | A client gets the saved batch                            | todo   |
| F30 | framework | `ModBase`'s client previews through the config overrides | todo   |
| M70 | mod       | The F4SE message after a Save                            | todo   |

F30 is the second part of item 13. The overrides it drives are done (F12 to F14).

M68 previews the settings whose `applies` is `live`, and no others.

The milestone comes before the controls of milestone 9 and the Focused view of milestone 10. Both are for tuning a value while the player sees its effect.

## 9. CommonFramework controls

| #   | Repo | Commit                                                                                 | Status |
| --- | ---- | -------------------------------------------------------------------------------------- | ------ |
| M74 | mod  | docs: the CommonFramework controls, and how each key of Immersive Flashlight is edited | todo   |
| M75 | mod  | Color                                                                                  | todo   |
| M76 | mod  | A choice of the files in a folder                                                      | todo   |
| M77 | mod  | Haptic                                                                                 | todo   |
| M78 | mod  | InputBinding                                                                           | todo   |
| M79 | mod  | Transform                                                                              | todo   |
| M80 | mod  | ActivationSphere, a whole section                                                      | todo   |
| M81 | mod  | The values of Immersive Flashlight that are left: lists, flags and the hand pose       | todo   |

The milestone is measured by Immersive Flashlight's TOML. Beside numbers, switches and choices it has:

- 13 transforms.
- 3 activation spheres, each a section with a zone, bindings, haptics, an icon and a sphere.
- 4 colors, each in one key.
- 4 gobo textures, each the path of a file.
- 4 hand poses.
- 5 lists of text, and a flags value.

M74 settles what the design leaves open: a control that is bound to a whole section, and whether a transform is moved in the world with the framework's live tuning.

The commits after M74 are split once it is written. Transform, InputBinding and ActivationSphere are each likely two: edited in the panel, then tuned in the world. Each of the values that are left is a commit of its own.

Immersive Flashlight's package and its client are commits in its own repository, on a framework with F29 and F30. They are made when the milestone needs them, and a build is enough.

## 10. Focused view

| #   | Repo      | Commit                                                                          | Status |
| --- | --------- | ------------------------------------------------------------------------------- | ------ |
| M71 | mod       | docs: the Focused view: what it shows and how it is switched to                 | todo   |
| M72 | mod       | The Focused panel on the offhand, with the selected page                        | todo   |
| M73 | mod       | Full, Focused and Minimized are switched between, and keep the mod and the page | todo   |
| F31 | framework | imgui: touch as a pointer, if a ray does not suit a panel on the own hand       | todo   |

A change on the offhand panel shows as it is made, where the mod acts on it at once: an MCM value is written at once, and a package's `live` value is previewed.

## 11. First release

| #   | Repo | Commit                                                                  | Status |
| --- | ---- | ----------------------------------------------------------------------- | ------ |
| M82 | mod  | The version rules of the package schema and of the native API           | todo   |
| M83 | mod  | An open session on a save load, a loading screen and the player's death | todo   |
| M84 | mod  | The menu without FRIK, and without MCM                                  | todo   |
| M85 | mod  | docs: the author's guide                                                | todo   |
| M86 | mod  | A sample package                                                        | todo   |
| M87 | mod  | Version 1.0.0                                                           | todo   |

The package format and the native API are public from this release, so M82 settles how both change later.

A mod's text is in the game's language where the mod has a translation file for it and the font has its letters. A mod with no game plugin shows its fallback text until milestone 12.

FRIK and Immersive Flashlight are probably released at the same time, with their packages and their clients.
