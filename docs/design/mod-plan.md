# Mod work plan

The milestones that build the mod, and the commits of each, in the order they are made. It follows the [Framework work plan](framework-plan.md), which made the panel the menu is drawn on.

The menu supports everything MCM has before it gains anything new. It is built so that packages and the native API are added later without redoing the UI: the UI draws one model, and MCM is the first source that fills it.

A package is the new config format of [Declarative schema](schema.md): a mod's `config.json` under `Data\ModsConfigVR\Config`.

## How the work is done

- As in the framework plan: one commit at a time, reviewed and tried in the game before it is committed.
- Mod commits continue from M10 and framework commits from F21. A commit that is added later takes the next free number.
- Each milestone ends with its "done when", tried in the headset.
- Code that does not need the running game (the model, the loaders, conditions, the INI file) is unit tested. The tests link the framework and CommonLibF4 with `REL::NO_GAME`, as the framework's linked tests do, so the code under test can use both. The first code commit adds the mod's tests.
- A loader grows with the rows. The commit that draws a control also adds the parsing of that control and its tests.
- A commit that settles an open question updates the design documents with it.
- Framework commits are in the tables below. What each needs is in [CommonFramework work required](framework-requirements.md).

Status is one of `todo`, `in review`, or `done`.

## The MCM test set

The test set is the author's Mad God Overhaul mod list: 9 MCM mods with 687 controls.

What it has:

- Controls: `switcher` 298, `section` 97, `slider` 53, `spacer` 46, `text` 42, `button` 32, `textinput` and `textinputFloat` 25, `hotkey` 23, `dropdown` 20, `stepper` 19, `image` 7.
- Value sources: `GlobalValue` 262, `ModSetting*` 151, `PropertyValueInt` 2.
- Actions: `CallFunction` 65, in Sim Settlements 2 and Workshop Framework. Their parameters are strings, with a type prefix or `{value}`.
- Conditions: `groupCondition` as a number 67 times, and as `AND` 4 times.
- `help` text on 376 rows, and `html` text on 20.
- `$` text in 3 mods. Auto Eat's 21 are in the game's translation files. The 132 of FallUI and FallUI Workbench are in FallUI's own files, and 31 of their texts have a token inside them, as `${Key}`.
- Images: 7, all bitmaps stored in the mod's `lib.swf`.
- Code in a `lib.swf`: FallUI's and FallUI Workbench's hold code that runs inside MCM's menu and translates their pages. The menu does not run a mod's code.
- Text input is in FallUI, FallUI Workbench and XDI. Hotkeys are in Sim Settlements 2 and Workshop Framework.
- A mod that does not act on a change at once: Unlimited Survival Mode's DLL reads MCM's settings file itself, and its page says in a text row that its settings need a restart. XDI's DLL reads MCM's files itself too.
- MCM in VR is `mcm_vr.dll`. It keeps ModSettings in memory and writes `Data\MCM\Settings\<mod>.ini` on every set.

What it does not have, from DearModdingUI's inventory of MCM:

- Controls: `dropdownFiles`, `color`, `hidden` and `hiddenSwitcher`, `menu` and `enum`, and the other names of controls it has (`switch`, `input`, `header`, `empty`, `keymap`).
- Value sources: `PropertyValueBool`, `PropertyValueFloat`, `PropertyValueString`.
- Actions: `CallGlobalFunction`, `CallExternalFunction`, `SendEvent`, `RunConsoleCommand`, and parameters that are plain numbers and booleans.
- Conditions: `OR`, and a condition inside a condition.
- An image that is drawn as vectors or animated.
- A mod whose `pluginRequirements` are not met, a config that does not parse, and a game language other than English.
- A script that changes a value while its page is shown.

A test MCM mod of our own covers these (M37). The inventory is DearModdingUI's and not MCM's own documentation, so the table of M43 is checked against MCM's.

## Milestones

| #   | Milestone                | Done when                                                                                                     | Status |
| --- | ------------------------ | ------------------------------------------------------------------------------------------------------------- | ------ |
| 1   | Browse MCM mods          | Every MCM mod of the test set is browsed in the Full panel: its pages, rows and images, with translated names | todo   |
| 2   | Edit MCM values          | A global, a ModSetting and a script property are changed from the panel, and the mod reacts as it does to MCM | todo   |
| 3   | MCM complete             | Every MCM feature is supported, shown read-only, or shown as unsupported with the reason                      | todo   |
| 4   | VR keyboard              | An MCM text row and a slider's value are typed in the headset                                                 | todo   |
| 5   | Packages with Save       | This mod's own INI and FRIK's are edited from packages and saved, and each mod reloads its file               | todo   |
| 6   | Actions and handoff      | FRIK's config is opened from the front page, and the menu comes back when it closes                           | todo   |
| 7   | Live preview             | A CommonFramework mod with a package previews a slider live, with no code of its own                          | todo   |
| 8   | Focused view             | A setting is changed on the offhand panel while the player looks at its effect                                | todo   |
| 9   | CommonFramework controls | Every setting of Immersive Flashlight VR is edited from its package                                           | todo   |
| 10  | First release            | The mod is published, with the author's guide and a sample package                                            | todo   |
| 11  | JSON and TOML files      | A setting in a JSON file and one in a TOML file are edited and saved                                          | todo   |
| 12  | Localization and search  | A mod with no game plugin is shown in another language, and a setting is found by typing its name             | todo   |
| 13  | Papyrus API              | A script-only mod reads its saved settings and is told of a Save                                              | todo   |

## Commits

### 1. Browse MCM mods

| #   | Repo      | Commit                                                                                                | Status |
| --- | --------- | ----------------------------------------------------------------------------------------------------- | ------ |
| M11 | mod       | docs: the menu's front page, with the mod tree and a quick launch section                             | todo   |
| M12 | mod       | docs: the settings model, checked against three kinds of setting                                      | todo   |
| M13 | mod       | The mod's unit tests, the settings model, and the MCM loader for a mod, its pages and its rows' texts | todo   |
| M14 | mod       | The front page lists the MCM mods that are found                                                      | todo   |
| M15 | mod       | FRIK in the quick launch section                                                                      | todo   |
| M16 | mod       | A mod's pages in the tree, with every row as its text and type                                        | todo   |
| M17 | mod       | MCM's `$` text tokens are shown as the game's translated text                                         | todo   |
| M18 | mod       | FallUI's pages are translated from FallUI's own files                                                 | todo   |
| M19 | mod       | Sections, spacers and text rows, with their HTML                                                      | todo   |
| M20 | mod       | The switcher, slider, stepper and dropdown are drawn with their default values                        | todo   |
| M21 | mod       | The help line                                                                                         | todo   |
| F22 | framework | render: a texture from pixels in memory                                                               | todo   |
| F23 | framework | imgui: an image in a canvas                                                                           | todo   |
| M22 | mod       | MCM images are shown, read from the mod's `lib.swf`                                                   | todo   |
| M23 | mod       | The panel's look: sizes for the ray, and the states of a row                                          | todo   |
| M24 | mod       | The Back button, and the menu's haptics                                                               | todo   |

M11 designs the menu's front page, which is what it shows with no mod selected:

- It is what the menu opens on. It has no row in the tree: the first breadcrumb, `MODS CONFIG VR`, goes to it, and is shown selected while it is open.
- It is like MCM's, with differences that are settled in this commit.
- The mods are a tree, as in [UI and VR interaction](ui-and-vr-interaction.md#navigation-behavior): a selected mod is expanded to show its pages, and its front page has no row.
- It has a quick launch section, which opens a mod's own config UI.
- FRIK is its first entry. The menu took the opening gesture from FRIK, so FRIK's config is opened from here.

M12 is [The settings model](model.md): what the UI draws, and the store it asks for every value. It is checked there against an MCM global, a value of a package, and an activation sphere, so the model of the first code commit also fits the milestones after MCM.

M13 adds that model, with the MCM store still empty:

- The MCM loader fills the definition with a mod, its pages, and the text and the type of each row. What a control holds beyond that is read by the commit that draws it.
- Both are unit tested.

M14 removes the placeholder controls of the test bed. MCM mods are listed only when `mcm_vr.dll` is loaded, and a config that does not parse is reported in the log.

M15 puts FRIK in the quick launch section, when FRIK is installed. Its config is opened the way the FRIK wrist button opens it today. Other mods come to the section in milestone 6, and FRIK's entry is then there when FRIK registers for it.

M16 expands the selected mod in the tree and shows its front page. The breadcrumbs in the title bar name the mod and the page. Every row is its text and what it is. A button, a text input and a hotkey stay such rows until the commits that make them work.

M17 looks a text that starts with `$` up in the game's translation table. F4SE fills it with the translation file of every enabled game plugin, for the language the game is set to.

M18 does what FallUI's own code does in MCM's menu, for FallUI and FallUI Workbench:

- Their config names a translation file of their own. A text that starts with `$` is looked up in it, and so is a `${Key}` inside a text.
- A row's help is looked up the same way. FallUI also names the help's entry in a key of its own, `$help`, beside the English text.

M20 draws the four controls with their default values, and disabled. A choice's index becomes typed options.

M21 shows the `help` text of the row the ray is on in the help line, on the left of the footer, as in [UI and VR interaction](ui-and-vr-interaction.md#help-line).

F22 and F23 are item 11 of the requirements.

M22 reads an image out of the mod's `lib.swf`:

- An MCM image is a class of that file. In the test set every one is a bitmap stored in it, six by themselves and one inside a sprite.
- The menu reads the bitmap and does not run Flash. An image that is drawn as vectors or animated is shown as a placeholder.
- Reading it needs zlib, from vcpkg at the pinned baseline.

M24 adds Back and the haptics of [UI and VR interaction](ui-and-vr-interaction.md):

- Back is a binding in the INI, which settles the button it is on. In this milestone it goes from a page of a mod to the mod's front page, and from there to the menu's front page. Whether it then closes the menu is settled in M11. A later commit adds its own step to it: a dropdown is closed, and a drag or a text edit is cancelled.
- The hand that points gets a light pulse when it presses a row or a mod. The pulses of a Save and of a value that is rejected come with those commits.

### 2. Edit MCM values

| #   | Repo      | Commit                                                                              | Status |
| --- | --------- | ----------------------------------------------------------------------------------- | ------ |
| M25 | mod       | Globals are read and written: the switcher works                                    | todo   |
| M26 | mod       | The slider                                                                          | todo   |
| M27 | mod       | The stepper and the dropdown                                                        | todo   |
| M28 | mod       | Conditions show and hide rows                                                       | todo   |
| M29 | mod       | ModSettings are read from MCM's files                                               | todo   |
| M30 | mod       | ModSettings are written through MCM                                                 | todo   |
| M31 | mod       | MCM's events are sent: a setting changed, the menu opened and closed                | todo   |
| M32 | mod       | Script properties are read and written                                              | todo   |
| F24 | framework | f4sevr: script properties and external events, if CommonLibF4VR's do not work in VR | todo   |

Globals come first: they are most of the test set, and need neither MCM nor Papyrus.

M25 saves a change as it is made. A status line says so, and there is no Save bar.

M28 translates `groupControl` and `groupCondition` to the model's conditions: a number, `AND`, `OR`, and one inside another. Until this commit every row is shown.

M29 reads a ModSetting from MCM's two files, the mod's defaults and the player's values, which MCM writes on every set. It is plain file code.

M30 writes a ModSetting through `MCM.SetModSetting*`, so MCM's memory and its file agree. It is the menu's first call into Papyrus.

M32 reads and writes all four property types.

F24 is made only if CommonLibF4VR's own calls for both do not work in VR, which M31 and M32 find out.

### 3. MCM complete

| #   | Repo | Commit                                                               | Status |
| --- | ---- | -------------------------------------------------------------------- | ------ |
| M33 | mod  | Buttons run `CallFunction`, with typed parameters                    | todo   |
| M34 | mod  | A value row runs its action after the write, with `{value}`          | todo   |
| M35 | mod  | Hotkeys are shown read-only                                          | todo   |
| M36 | mod  | Text rows are shown read-only, and unknown controls as placeholders  | todo   |
| M37 | mod  | A test MCM mod for what the test set does not have                   | todo   |
| M38 | mod  | The hidden controls, `menu`, `enum`, and the other names of controls | todo   |
| M39 | mod  | `dropdownFiles`                                                      | todo   |
| M40 | mod  | The actions beside `CallFunction`                                    | todo   |
| M41 | mod  | A mod whose required plugins are missing is not listed               | todo   |
| M42 | mod  | A page that is shown reads its values again                          | todo   |
| M43 | mod  | docs: the table of MCM features and how the menu handles each        | todo   |

M35 reads MCM's two keybind files, the mod's and the player's, and names each key.

M36 shows a text row read-only until milestone 4.

M37 adds a test mod: a game plugin with a quest, its script and globals, as DearModdingUI's test fixture is. It also proves what earlier commits made and the test set does not use: the other property types (M32) and `OR` conditions (M28).

M38 shows `color` as a placeholder until the Color control of milestone 9.

M40 supports each action, or marks it unsupported with the reason.

M43 answers the MCM items of [Open questions](open-questions.md).

### 4. VR keyboard

| #   | Repo      | Commit                                                      | Status |
| --- | --------- | ----------------------------------------------------------- | ------ |
| M44 | mod       | docs: the keyboard's design, as item 12 of the requirements | todo   |
| F25 | framework | imgui: characters and keys reach a text field               | todo   |
| F26 | framework | imgui: an on-screen keyboard while a text field is edited   | todo   |
| F27 | framework | imgui: a number pad for a field that takes a number         | todo   |
| M45 | mod       | MCM text rows are edited                                    | todo   |
| M46 | mod       | A slider's value is typed                                   | todo   |

M44 decides where the keyboard is. The menu's panel is dedicated, so it is ImGui's only display. The keyboard is either drawn inside that panel by ImGui, or beside it from vrui buttons, which then need to be pressed by the ray.

F26 makes the keyboard the framework's. It is shown by itself while ImGui edits a text field, so the content calls `ImGui::InputText` as on a screen.

### 5. Packages with Save

The largest milestone. A package's features match MCM's, but its lifecycle is new.

| #   | Repo | Commit                                                                                    | Status |
| --- | ---- | ----------------------------------------------------------------------------------------- | ------ |
| M47 | mod  | The package loader: files, pages, sections and settings, and Mods Config VR's own package | todo   |
| M48 | mod  | Choices and conditions in a package                                                       | todo   |
| M49 | mod  | A package's button calls a Papyrus function, as an MCM button does                        | todo   |
| M50 | mod  | An INI file is read, changed and written with its comments and order kept                 | todo   |
| M51 | mod  | The editing session: values from the file, pending values, dirty state                    | todo   |
| M52 | mod  | Save writes the dirty settings, and Discard                                               | todo   |
| M53 | mod  | A Save that fails leaves every file as it was                                             | todo   |
| M54 | mod  | Save creates a file that is missing                                                       | todo   |
| M55 | mod  | Validation                                                                                | todo   |
| M56 | mod  | Reset to the default                                                                      | todo   |
| M57 | mod  | The unsaved-changes prompt                                                                | todo   |
| M58 | mod  | A row says when its change takes effect, and Save names a needed restart                  | todo   |
| M59 | mod  | Text, spacers and images in a package                                                     | todo   |
| M60 | mod  | Requirements hide a package                                                               | todo   |
| M61 | mod  | A package's errors are reported                                                           | todo   |

M47 ships the first package, which is this mod's own, for `ModsConfigVR.ini`. The mod reloads its INI when it changes, so a Save shows at once with no other mod involved. A package's text takes the three forms of [Localization](schema.md#localization): plain, a `$` token, and a token with a fallback. The lookup of a token is M17's.

M49 lets a package use the actions the menu supports for MCM, so a script-only mod has buttons. The action of a native client comes in milestone 6.

The milestone is INI only. A CommonFramework mod reloads its INI by itself, so it needs no native API.

M52 to M54 follow the Save steps of [Architecture and lifecycle](architecture.md#save-lifecycle). M52 is the Save that goes well. M53 is the checks before the write, and the rollback of a write that fails.

M58 shows a package's `applies`. The help line says when the change of the pointed row takes effect, a row that needs a restart is marked, and a Save that wrote one says that the game has to be restarted. The loader reads `applies` from M47 on, as a package's root requires it. Until milestone 7 there is no client, so a `live` setting is handled as `save`.

### 6. Actions and handoff

| #   | Repo      | Commit                                                                            | Status |
| --- | --------- | --------------------------------------------------------------------------------- | ------ |
| M62 | mod       | docs: the native API's binary interface                                           | todo   |
| M63 | mod       | The native API: a client registers by name, with the client header                | todo   |
| F28 | framework | `ModBase` registers the mod as a client                                           | todo   |
| F29 | framework | A mod adds its functions and its own config UI to its client                      | todo   |
| M64 | mod       | A package's button runs its client's function, with the confirmation dialog       | todo   |
| M65 | mod       | Handoff: an action opens the mod's own UI, and the menu comes back when it closes | todo   |
| M66 | mod       | The quick launch section takes the mods that registered for it                    | todo   |
| M67 | mod       | FRIK's config is opened through the handoff, and its wrist button is removed      | todo   |

M62 settles what the API passes between DLLs, how its text is encoded, and how its versions are told apart. It also settles what a package writes for a button that opens the mod's own UI.

M63, F28 and F29 are the first part of item 13 of the requirements. The client header is vendored in the framework, as devbench's is.

M67 needs a FRIK that is built on a framework with F29. That is a commit in FRIK's repository, made when this commit needs it. A build of FRIK is enough: it does not have to be released. The quick launch entry needs no package. FRIK's entry in the quick launch section is then there when FRIK registers for it.

### 7. Live preview

| #   | Repo      | Commit                                                   | Status |
| --- | --------- | -------------------------------------------------------- | ------ |
| M68 | mod       | A client gets the preview and discard events             | todo   |
| M69 | mod       | A client gets the saved batch                            | todo   |
| F30 | framework | `ModBase`'s client previews through the config overrides | todo   |
| M70 | mod       | The F4SE message after a Save                            | todo   |

F30 is the second part of item 13. The overrides it drives are done (F12 to F14).

M68 previews the settings whose `applies` is `live`, and no others.

The milestone comes before the Focused view and the controls of milestone 9. Both are for tuning a value while the player sees its effect.

### 8. Focused view

| #   | Repo      | Commit                                                                          | Status |
| --- | --------- | ------------------------------------------------------------------------------- | ------ |
| M71 | mod       | docs: the Focused view: what it shows and how it is switched to                 | todo   |
| M72 | mod       | The Focused panel on the offhand, with the selected page                        | todo   |
| M73 | mod       | Full, Focused and Minimized are switched between, and keep the mod and the page | todo   |
| F31 | framework | imgui: touch as a pointer, if a ray does not suit a panel on the own hand       | todo   |

A change on the offhand panel shows as it is made, where the mod acts on it at once: an MCM value is written at once, and a package's `live` value is previewed.

### 9. CommonFramework controls

| #   | Repo | Commit                                                                                 | Status |
| --- | ---- | -------------------------------------------------------------------------------------- | ------ |
| M74 | mod  | docs: the CommonFramework controls, and how each key of Immersive Flashlight is edited | todo   |
| M75 | mod  | Color, in one key and in three                                                         | todo   |
| M76 | mod  | A choice of the files in a folder                                                      | todo   |
| M77 | mod  | Haptic                                                                                 | todo   |
| M78 | mod  | InputBinding                                                                           | todo   |
| M79 | mod  | Transform                                                                              | todo   |
| M80 | mod  | ActivationSphere, a whole INI section                                                  | todo   |
| M81 | mod  | The values of Immersive Flashlight that are left: lists, flags and the hand pose       | todo   |

The milestone is measured by Immersive Flashlight's INI. Beside numbers, switches and choices it has:

- 13 transforms.
- 3 activation spheres, each a section with a zone, bindings, haptics, an icon and a sphere.
- 4 colors that are three keys each, and a color in one key.
- 4 gobo textures, each the path of a file.
- 4 hand poses.
- 5 lists of text, and a flags value.

M74 settles what the design leaves open: a control that is bound to a whole section, a color in three keys, and whether a transform is moved in the world with the framework's live tuning.

The commits after M74 are split once it is written. Transform, InputBinding and ActivationSphere are each likely two: edited in the panel, then tuned in the world. Color is likely two, and each of the values that are left a commit of its own.

MCM's `color` rows use the Color control of M75.

Immersive Flashlight's package and its client are commits in its own repository, on a framework with F29 and F30. They are made when the milestone needs them, and a build is enough.

### 10. First release

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

### 11. JSON and TOML files

| #   | Repo | Commit                                   | Status |
| --- | ---- | ---------------------------------------- | ------ |
| M88 | mod  | A JSON file is read, changed and written | todo   |
| M89 | mod  | A TOML file is read, changed and written | todo   |

### 12. Localization and search

| #   | Repo      | Commit                                                                       | Status |
| --- | --------- | ---------------------------------------------------------------------------- | ------ |
| M90 | mod       | A translation file is read for a package whose mod has no game plugin        | todo   |
| F32 | framework | imgui: glyphs beyond Western European languages, if a translation needs them | todo   |
| M91 | mod       | Search in the mod list and in a mod's settings                               | todo   |

M90 reads `Interface\Translations\<modName>_<language>.txt` itself. F4SE loads a translation file only for an enabled game plugin, so a mod that is a DLL alone has none loaded.

### 13. Papyrus API

| #   | Repo | Commit                                                                  | Status |
| --- | ---- | ----------------------------------------------------------------------- | ------ |
| M92 | mod  | The Papyrus getters, with packages loaded when the game's data is ready | todo   |
| M93 | mod  | The Papyrus event after a Save                                          | todo   |
| M94 | mod  | docs: the script author's guide                                         | todo   |
