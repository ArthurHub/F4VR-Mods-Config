# Mod work plan, phase 2: First release

The commits of milestones 5 to 11 of the [Mod work plan](mod-plan.md): config specs and the native API, up to the first release. The plan has how the work is done, the MCM test set and the status of every milestone.

## 5. Config specs with Save

A config spec's features match MCM's, but its lifecycle is new. The milestone defines the config spec and builds its core: settings that are read from a TOML file, edited and saved.

| #   | Repo | Commit                                                                                    | Status |
| --- | ---- | ----------------------------------------------------------------------------------------- | ------ |
| M103 | mod | docs: what a config spec has beyond MCM                                                      | done   |
| M47 | mod  | The config spec loader: files, pages, sections and settings, and Mods Config VR's own config spec | done   |
| M114 | mod | The config spec showcase, an optional file for developers                                    | done   |
| M48 | mod  | Choices, shared lists and conditions in a config spec                                         | done   |
| M89 | mod  | A TOML file is read, changed and written with its comments and order kept                 | done   |
| M51 | mod  | The editing session: values from the file, pending values, dirty state                    | done   |
| M52 | mod  | Save writes the dirty settings, and Discard                                               | done   |
| M56 | mod  | Reset to the default                                                                      | done   |
| M115 | mod | The title bar's Help, Minimize and Close, and the Help page                               | done   |

M103 decides what a config spec should support beyond MCM, and what it has in place of what MCM's authors work around: [A config spec and MCM](schema.md#a-config-spec-and-mcm). The milestones build it:

- M47 reads the config spec's pages and sections, which nest, and the help of the mod and of its pages. A section in a section is drawn with a heading of its own. The tree shows a page's pages from M109.
- M48 has every operator of a condition, `enabledWhen`, the conditions on a loaded plugin, and shared lists.
- Milestone 6 has the rest: sections that fold, pages under pages, read-only settings and how a number is shown, a button's console command and its text, globals and properties with the action of a setting, and options from the game.

M114 adds a config spec of our own, `data/showcase/ModsConfigVR/Config/ModsConfigVR_Showcase/`, as the MCM showcase is for MCM: what a config spec has, each kind on a page, and what a config spec can get wrong, with a TOML file and an INI file of its values beside it. Its front page has a row of every kind, so a row the menu does not draw yet is a placeholder there until the commit that draws it. It is tried in the headset as each commit reads or draws something of a config spec, and grows with them. It is packaged with the other showcase in one optional archive, to show a mod's author what a config spec does in the mod.

M47 ships the first config spec, which is this mod's own, for `ModsConfigVR.toml`. The mod reloads its TOML when it changes, so a Save shows at once with no other mod involved. A config spec's text takes the three forms of [Localization](schema.md#localization): plain, a `$` token, and a token with a fallback. The lookup of a token is [M17](mod-plan-phase-1.md#1-browse-mcm-mods)'s.

The milestone is TOML only. A CommonFramework mod keeps its config in a TOML file, as this mod does, and reloads it by itself, so it needs no native API.

M51 is the config spec store: the [editing session](architecture.md#editing-session) and the [values at session start](architecture.md#values-at-session-start) of a TOML file. A pending value has the design system's pending look, and the footer counts the unsaved changes. An invalid value of the file has its mark, its color and its reason in the help line. Until M55 a value the player sets outside its limits is not set.

M52 follows the Save steps of [Architecture and lifecycle](architecture.md#save-lifecycle), for the Save that goes well. Save and Discard are in the footer, as in the design system's `SaveBar`: enabled while a change waits, disabled on the menu's front page, and not there for an MCM mod. Each file is written in one write, and its settings are then clean with the values written as their baseline. Until M53 a file that cannot be written keeps its settings dirty, and the other files are still written.

M56 is the Reset of a config spec's setting. The button of a row and the default in the help line are [M102](mod-plan-phase-1.md#3-mcm-complete)'s. The config spec store answers the `default` of the setting, and a Reset is a pending value as any other: Save writes it, and Discard takes it back. A `default` that is not one of its setting's values, outside its limits or none of its options, is none, as one of another type is: the loader logs it, and the setting has no Reset.

M115 puts Help, Minimize and Close on the right of the title bar, as the design system has them. Minimize and Close do what the wrist's buttons do. Help shows [the Help page](ui-and-vr-interaction.md#the-help-page) over where the player is: each control of the menu, with the buttons and the bindings of the player's config in the sentence of the InputBinding control, which M78 shows in its row.

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

M108, M109, M111 and M112 draw what M103 added to the page and the tree:

- M108: a section that folds opens and closes with a press on its heading, starts as its `collapsed` says each time its page is shown, and marks its heading while it hides an invalid value or an unsaved change. A section in a section that is open has the line down its rows that shows what it covers.
- M109: a page's pages are rows under it in the tree, a level further in, with a crumb for each level, and Back goes a level up.
- M111: a read-only setting, and the decimals, the unit and the percentage of a number's `display`.
- M112: a setting or a button whose warning holds has a warning mark, and the help line shows the warning before the row's help.

M59 draws a config spec's `text`, `spacer` and `image` as an MCM config's are drawn. An image is a texture file, which the game loads as it loads a texture of a mesh, from a loose file or a BA2 archive. It is drawn at the size its row gives, or at the texture's own, and a texture the game does not load is a box that names it. The config spec showcase has a page of them, with textures of its own.

M49 lets a config spec use the actions the menu supports for MCM, so a script-only mod has buttons, and a console command, which needs no script. The action of a native client comes in milestone 7.

M107 lets a script mod keep its config spec's settings where its scripts and the game read them, as an MCM mod does. A global and a property are pending until Save, as a key of a file is, and the store previews a `live` one by writing it into the game. Save writes them after the files, then runs the action of each setting it wrote, with `"{value}"`. It is tried with a config spec written for one of the test set's MCM mods that keeps its values in globals. The config spec showcase gets a slider of the time of day, kept in the game's global `GameHour` (`Fallout4.esm|38`) with `applies` `live`, so the time changes as the slider moves.

M110 reads MCM's `listFromForm` and `listFromProperty`, for an MCM config and a config spec alike. The store answers the options of a choice, which it reads in the game as the mod is selected: the MCM store again as another page is shown, as MCM's menu does, and the config spec store with the rest of its baseline. A choice whose list is not in the game is not available, with the reason. It comes after M107, which reads a property of a script for a config spec. No MCM config of the author's lists has either, so the unit tests cover it, and both showcases have choices of the game's FormLists of its settlements and of its companions.

M50 is for the many F4SE mods that keep their config in an INI file, and for a CommonFramework mod on an older framework. It settles how an INI file is read and how its values are spelled, in [Persistence model](architecture.md#persistence-model): as SimpleIni reads it, with a key's last value and a whole number in hex, and a boolean written in the words the file has. The config spec showcase's INI section gets a number and a text with a `;` in it.

M53 and M54 are the rest of the Save steps, and are made in one commit. M53 is the checks before the write, and the rollback of a write that fails: every file gets its new text, checked to read back, and every global and property is checked to be in the game, before anything is written, and a write that fails puts back the files written before it. A Save that fails keeps every setting dirty. M54 creates a file that is not there, with the folders on the way to it, as the config spec showcase's `player.toml` in My Games is, and a Save that fails after it removes the file. The footer's status says why a Save failed, in red after the invalid mark, until the next change, and Save stays enabled. A write that fails is tried in the headset with a file of the config spec showcase made read-only.

M55 checks the values the player sets. A value of the type its control shows is set, also one that is no value of its setting: a text of a number that is none, a number outside its limits, or a text whose length its `minLength` and `maxLength` do not allow. It is pending and invalid, as a value of the file is shown: the invalid mark, the value in red, and the reason in the help line while the ray is on its row, with no message under its name. A `live` setting is not previewed with it, so the last valid preview stays. While a changed value is invalid the footer's status says how many there are, in red after the invalid mark, and Save is disabled: an invalid value that was in the file and was not changed does not block it. A text with a `maxLength` takes no character past it as it is typed, and shows the count, `12 / 16`. The config spec showcase's text inputs page has a section of limits.

M116 is rejected: Save has no summary of the invalid values. Save is disabled while a changed value is invalid, the status says how many there are, and each has its mark on its row and in the heading of a folded section.

M57 asks before a mod with unsaved changes is left: for another mod, for the menu's front page, and as the menu is closed by the title bar, the wrist or the gesture, which shows a minimized panel again. The dialog is the design system's ConfirmDialog. Save and Discard then do what the player was about to do, and Cancel and Back stay on the mod. A Save that fails stays too, with its reason in the status, and Save is disabled while a change is invalid.

M58 shows a config spec's `applies`, as the design system's RestartNotice has it. The help line says when the change of the pointed row takes effect, a row that needs a restart has the Restart mark, and the status counts the pending changes that need one. A Save that wrote one says that the game has to be restarted until the next change, a Save that wrote none says Saved, and the menu's front page has a notice for each mod with such a change until the game is restarted. The loader reads `applies` from M47 on, as a config spec's root requires it. Until milestone 8 there is no client, so a `live` setting of a file is handled as `save`: a `live` global or property is previewed by the store itself.

M60 reads a config spec's `pluginRequirements` and `f4sePluginRequirements`. While a game plugin or an F4SE plugin of them is not loaded the config spec's mod is not available, as an MCM mod whose `pluginRequirements` are not met is, and the reason names each that is not: `its plugin SS2.esm and its F4SE plugin MyMod are not loaded`. The rest of the config spec is not read. A plugin is checked as a condition on a `plugin` or an `f4sePlugin` is. A config spec's `minSchemaVersion` is required, as its `applies` is, and a config spec whose `minSchemaVersion` is above the schema version the menu reads is not available too, before its plugins are checked, and the reason names the version it needs. The config spec showcase requires `Fallout4.esm` and the mod's own F4SE plugin, which are always loaded.

M61 reports a config spec's problems to its author. Each is a line of the log with the config spec's name first, `Config spec 'Immersive Flashlight VR': …`, and the log has a count of each config spec's errors and warnings once the config specs are read. An error leaves something out: the whole config spec, when it cannot be read, or a part of it. A warning is read, but is likely not what the author meant. It adds what the schema promises and the loader does not report yet: a field the menu does not know, which needs the known fields of each kind of object, and a `$` token with no translation and no fallback. The others the schema names are checked to be reported: a condition that names no setting or compares another type, and two settings with one id.

With `Developer tools` on, a switch in Mods Config VR's own settings that is off as shipped, the menu's front page has a Developer section after its notices, which folds. Its filter, a stepper written at once, lists the config specs that need attention, all of them, or the one the settings name, and a config spec's row has its errors and warnings. The settings name that config spec with a dropdown whose options are the config specs the menu found: it reads them with `listFromClientCallback`, a choice's options from its mod's client, which the menu answers for its own config spec until the native API is there. A press on the row opens the config spec's report: its problems, errors first, under headings that fold, each a row whose help line says where it is, and which goes to the row it names. See the design system's [Developer](design-system/project/components/Developer/README.md), [FullPanelDeveloper](design-system/project/components/FullPanelDeveloper/README.md) and [FullPanelConfigSpecReport](design-system/project/components/FullPanelConfigSpecReport/README.md), and [Developer tools](ui-and-vr-interaction.md#developer-tools). The config spec showcase's Mistakes page has one of each problem.

M117 lets a config spec's author edit its `spec.json` while the game runs. On the config spec's report, `Read the live copy` copies its `spec.json` to `My Games\Fallout4VR\Mods_Config\ModsConfigVR\Dev\<folder>\spec.json` and reads the copy in its place. The copy is watched and read again each time it is saved, as the mod's own TOML is, and Reload reads it at once. A read drops the mod's unsaved changes, and the page that is open stays while it is still there. A copy that cannot be read is an error of the report, and the mod keeps what it read last. Only `spec.json` is copied. `Open` and `Show` open the copy and its folder on the desktop. It is for config specs alone: MCM reads its own copy of an MCM config, so a live copy of one would show what MCM does not.

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

M62 settles the [Native API](architecture.md#native-api): what it passes between DLLs, how its text is encoded, how its versions are told apart, and its calls. The API is two interfaces: the host's, which `ModsConfigVR.dll` exports, and the client's, which the host asks by name (M119). A button that opens the mod's own UI is a plain `CallNativeFunction`, whose function minimizes the menu and restores it, so the schema has nothing of its own for it.

M63, F28 and F29 are the first part of item 13 of the requirements. F29 comes after M66: it gives a CommonFramework mod the functions, the states, the events, the calls of the menu and the quick launch entry that M64, M118, M113, M65 and M66 add to the API. The client header is vendored in the framework, as devbench's is. M63 has `registerClient` and `registerOptions`, and calls a client inside the guard of [A client that fails](architecture.md#a-client-that-fails), so a client whose call fails is cut off and the menu keeps working. The menu is the first client: the options of its own Config spec setting go through the API, so M63 is tried in the headset with the menu alone.

M64 runs a `CallNativeFunction` of a button, and of a setting's action as Save writes its value, with its named `params` in their own types, and shows a function's message as the result of its run: after the info mark in the help line, and in red after the invalid mark when it failed. A button whose mod's DLL is not loaded, whose client is cut off, or whose DLL has no such function is disabled, with the reason. A button's `confirmation` asks first in the design system's dialog, red when it is `dangerous`, for any action. Until F29 no mod in the game registers a function, so a unit test covers the run, and the config spec showcase's Buttons page has a confirmation and a dangerous one.

M118 reads a condition's `clientState`, which the host asks the client once a frame while its mod is shown. The config spec showcase has no client, so a unit test covers it, with a state of the tests' own, and the showcase's Conditions page has a button whose states are not available, which is enabled and does not warn.

M113 sends the [client's events](architecture.md#the-clients-events): the menu opened and closed, to every client, and its mod and a page of its config spec that has an `id` shown and left. Immersive Flashlight draws its activation spheres in the world while their page is shown.

M65 is the menu's calls: `getMenuState`, `minimizeMenu`, `restoreMenu`, which reads the shown mod's values again, `reloadValues`, and `openMenu` at a mod and a page. A call is done in the menu's frame update, as one comes from a function of the client that runs while the panel is drawn. An MCM mod's values are read as they are asked, so the MCM store reads nothing again. Until F29 no mod in the game makes the calls, so a unit test covers the values read again.

M119 makes the native API two interfaces of virtual functions, the host's and the client's, `IClient`, in place of a struct of function pointers each with a context. A client registers once, and the host asks it by name for a function, a state and the options of a choice, so the API has no call to register each. The header has a C++ client over them, `ModsConfigVRClient`, compiled into each client's DLL, which answers with the mod's lambdas, adds a quick launch entry with the function it runs, and minimizes and restores the menu: a client that is no CommonFramework mod has it too. The menu's own options go through it.

M66 is `addQuickLaunch`: a client adds an entry for each UI it opens from the menu's front page, with its text, its help, the text of its button and the function a press runs, which tells the entry from the client's others. The menu translates the texts as it opens, as a client registers before the game's translations are read, and lists the entries as the mods are. A press tells the function's message in the help line, in red when it failed, and an entry whose function does not run, as one of a client that is cut off, is disabled with the reason. FRIK's own entry stays until M67. It comes before F29, which gives a CommonFramework mod its entries and the menu's minimize and restore.

M67 has FRIK add its own entry, in a commit of FRIK's repository on a framework with F29, and the menu keeps its own for a FRIK of before it, which adds none, while FRIK is installed: the menu takes FRIK's gesture from that FRIK too. A build of FRIK is enough: it does not have to be released. FRIK's entry opens its main config in FRIK's next frame update, as the press runs while the menu draws its panel, and minimizes the menu, which FRIK restores once none of its configs is open: the Pip-Boy and weapon configs open from the main config, which closes as they do. The quick launch entry needs no config spec.

## 8. Live preview

| #   | Repo      | Commit                                                   | Status |
| --- | --------- | -------------------------------------------------------- | ------ |
| M120 | mod      | docs: Save for every mod, and `applies` that is not known | in review |
| M121 | mod      | An MCM mod's changes wait for a Save                     | in review |
| M122 | mod      | A script button of a mod with unsaved changes asks to save first | in review |
| M123 | mod      | `applies` is `unknown` where nothing says it, and the help line says it | in review |
| M124 | mod      | design system: the marks of `applies`, and the Help page's legend | in review |
| M125 | mod      | The marks of `applies` on a pending row and in the help line, and the Help page's legend | in review |
| M68 | mod       | A client gets the preview and discard events             | in review |
| M69 | mod       | A client gets the saved batch                            | in review |
| F30 | framework | `ModBase`'s client previews through the config overrides | in review |
| F31 | framework | config: a subscriber is told the change of its keys         | in review |
| M70 | mod       | The F4SE message after a Save                            | todo   |

M120 to M125 give every mod one lifecycle before a client previews: a change waits for Save, and the menu says when a change takes effect only where a config spec says it. See [MCM compatibility](architecture.md#mcm-compatibility) and [When a change takes effect](schema.md#when-a-change-takes-effect).

- M121 keeps an MCM mod's changes pending, with the Save bar and the unsaved-changes dialog. Save writes them all, then runs each row's action and sends MCM's event in the order of the config, then the pause menu's events where it wrote a ModSetting. A setting with no storage is never dirty.
- M122 has a button that calls a script, of a mod with unsaved changes, ask to save them first, with Cancel, Run without saving and Save and run, which stands for the button's own confirmation. Its help says so. It is for an MCM mod and a config spec alike. A console command, a link and a function of the mod's DLL do not ask.
- M123 reads `unknown`, and an `applies` that is not at the config spec's root, which is `unknown`. An MCM setting is `unknown`. The help line says the pointed row's `applies` in an amber tag while its change is pending, but for `unknown`, which has none, and a row has no Restart mark. A `live` setting of a file with no client still says `live`.
- M124 designs the marks: a broadcast for `live`, a disk for `save` and a power mark for `restart`, the pending mark as that one in amber, and a plain pending mark for an `unknown` one and for a count. It also brings the design system up to M121 and M122: every mod has the Save bar, and a script's button can ask to save first. M125 draws them, and the Help page says what each means and what Save does.

F30 is the second part of item 13. The overrides it drives are done (F12 to F14). `ModBase` subscribes a `ConfigPreviews` to the mod's client, which sets an override under the owner `ModsConfigVR` for a preview of a setting of the mod's own config file, by the value's path in it, and clears it for a discard, and a Save clears the overrides of the settings it wrote. A setting of another file or of the game is left to the mod's own handlers.

F31 is for a mod that does more work for a value to take effect, as one that rebuilds what a value makes: a subscriber of the config names its keys, a value or a whole section or table, and is called on the game thread at the start of the next frame only for a load that changed one of them, with the ones that did. Each load is compared with the one before, whatever made it, so a preview, a discard and a Save tell it as a change of the file does. The first load changes nothing.

M68 previews the settings whose `applies` is `live`, and no others: a valid pending value is told to its mod's client, that of a global or a property after the store writes it into the game, with the value it was told before, and a Discard back to the baseline as it is set back, discarded or its mod is left. A Save tells nothing: the saved batch is M69's. `IClient` grows `onChange`, and the framework's copy of the header is changed with it.

M69 tells a client once what a Save of its config spec wrote, after the action of each setting ran: every setting it wrote, of any `applies`, with its value before the Save and the value written, the files it wrote, and a revision, which counts the Saves of the config spec that wrote something in this run of the game. A preview the Save wrote ends with it, with no Discard. `IClient` grows `onSaved`, and the C++ client `subscribeToSaves`, in both copies of the header.

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

Immersive Flashlight's config spec and its client are commits in its own repository, on a framework with F29 and F30. They are made when the milestone needs them, and a build is enough.

## 10. Focused view

| #   | Repo      | Commit                                                                          | Status |
| --- | --------- | ------------------------------------------------------------------------------- | ------ |
| M71 | mod       | docs: the Focused view: what it shows and how it is switched to                 | todo   |
| M72 | mod       | The Focused panel on the offhand, with the selected page                        | todo   |
| M73 | mod       | Full, Focused and Minimized are switched between, and keep the mod and the page | todo   |
| F31 | framework | imgui: touch as a pointer, if a ray does not suit a panel on the own hand       | todo   |

A change on the offhand panel shows as it is made where its `applies` is `live`, as it is previewed. Any other shows once it is saved, and the mod acts on it.

## 11. First release

| #   | Repo | Commit                                                                  | Status |
| --- | ---- | ----------------------------------------------------------------------- | ------ |
| M82 | mod  | The version rules of the config spec schema and of the native API           | todo   |
| M83 | mod  | An open session on a save load, a loading screen and the player's death | todo   |
| M84 | mod  | The menu without FRIK, and without MCM                                  | todo   |
| M85 | mod  | docs: the author's guide                                                | todo   |
| M86 | mod  | A sample config spec                                                        | todo   |
| M87 | mod  | Version 1.0.0                                                           | todo   |

The config spec format and the native API are public from this release, so M82 settles how both change later.

A mod's text is in the game's language where the mod has a translation file for it and the font has its letters. A mod with no game plugin shows its fallback text until milestone 12.

FRIK and Immersive Flashlight are probably released at the same time, with their config specs and their clients.
