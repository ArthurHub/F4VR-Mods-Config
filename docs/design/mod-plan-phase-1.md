# Mod work plan, phase 1: MCM support

The commits of milestones 1 to 4 of the [Mod work plan](mod-plan.md), in which the menu supports everything MCM has. The plan has how the work is done, the MCM test set and the status of every milestone. The phase is done.

## 1. Browse MCM mods

| #   | Repo      | Commit                                                                                                | Status    |
| --- | --------- | ----------------------------------------------------------------------------------------------------- | --------- |
| M11 | mod       | docs: the menu's front page, with the mod tree and a quick launch section                             | done      |
| M12 | mod       | docs: the settings model, checked against three kinds of setting                                      | done      |
| M13 | mod       | The mod's unit tests, the settings model, and the MCM loader for a mod, its pages and its rows' texts | done      |
| M14 | mod       | The front page lists the MCM mods that are found                                                      | done      |
| M15 | mod       | FRIK in the quick launch section                                                                      | done      |
| M100 | mod      | The panel's look from the design system, and the panel drawn into an image by a test                  | done      |
| M16 | mod       | A mod's pages in the tree, with every row as its text and type                                        | done      |
| M17 | mod       | MCM's `$` text tokens are shown as the game's translated text                                         | done      |
| M18 | mod       | FallUI's pages are translated from FallUI's own files                                                 | done      |
| M19 | mod       | Sections, spacers and text rows, with their HTML                                                      | done      |
| M101 | mod      | The MCM showcase, an optional file for developers                                                     | done      |
| M20 | mod       | The switcher, slider, stepper and dropdown are drawn with their default values                        | done      |
| M21 | mod       | The help line                                                                                         | done      |
| F22 | framework | render: a texture from pixels in memory                                                               | done      |
| F23 | framework | imgui: an image in a canvas                                                                           | done      |
| M22 | mod       | MCM images are shown, read from the mod's `lib.swf`                                                   | done      |
| M23 | mod       | The panel's look: sizes for the ray, and the states of a row                                          | done      |
| F33 | framework | vrui: the move button takes hold after a hold delay                                                   | done      |
| M24 | mod       | The Back button, and the menu's haptics                                                               | done      |

M11 is [The menu's front page](ui-and-vr-interaction.md#the-menus-front-page), which is what the menu shows with no mod selected:

- It is what the menu opens on. It has no row in the tree: the first breadcrumb, `MODS CONFIG FO4`, goes to it, and is shown selected while it is open.
- It is like MCM's, and the section lists what differs.
- The mods are a tree, as in [UI and VR interaction](ui-and-vr-interaction.md#navigation-behavior): a selected mod is expanded to show its pages, and its front page has no row.
- It has a quick launch section, which opens a mod's own config UI.
- FRIK is its first entry. The menu took the opening gesture from FRIK, so FRIK's config is opened from here.
- It has the notices. A mod that the menu found and cannot show is in the tree as a disabled row, and the page says why.
- Back on it closes the menu.

M12 is [The settings model](model.md): what the UI draws, and the store it asks for every value. It is checked there against an MCM global, a value of a config spec, and an activation sphere, so the model of the first code commit also fits the milestones after MCM.

M13 adds that model, with the MCM store still empty:

- The MCM loader fills the definition with a mod, its pages, and the text and the type of each row. What a control holds beyond that is read by the commit that draws it.
- Both are unit tested.

M14 removes the placeholder controls of the test bed. MCM mods are listed only when `mcm_vr.dll` is loaded, and a config that does not parse is reported in the log.

M15 puts FRIK in the quick launch section, when FRIK is installed. The entry opens FRIK's config, and the FRIK wrist button that opened it is removed. Other mods come to the section in milestone 7, and FRIK's entry is then there when FRIK registers for it.

M100 gives the panel the look of the [design system](design-system/README.md), which every commit after it draws in:

- The design system's sizes and colors are set on ImGui's style, with the generated `theme::applyStyle`.
- The title bar, the list of the mods and the menu's front page are laid out as their mocks.
- A row under the ray has its tint and its outline.
- The panel's drawing is apart from the game, in `src/ui/`, and a test draws it into an image. A change of the panel is looked at there before it is tried in the headset.

M23 is then the look of what comes after: the sizes as they are tuned in the headset, and the other states of a row.

M16 expands the selected mod in the tree and shows its front page. The breadcrumbs in the title bar name the mod and the page. The tree keeps room for its scroll bar, and a row selected from a breadcrumb is scrolled into view. Every row is its text and what it is. A button, a text input and a hotkey stay such rows until the commits that make them work.

M17 looks a text that starts with `$` up in the game's translation table. F4SE fills it with the translation file of every enabled game plugin, for the language the game is set to.

M18 does what FallUI's own code does in MCM's menu, for FallUI and FallUI Workbench:

- Their config names a translation file of their own, in the key `$fallUILanguageFile` of a row. The English file is read, and over it the file of the game's language. FallUI tells the language by the game's text of `$Attach`.
- A text that starts with `$` is looked up in it, and so is a `${Key}` inside a text.
- A plain text is looked up by a token made of its words: "Max. item name length" is `$Max_item_name_length`. FallUI's config is written in English this way. A text with tags is looked up piece by piece, between the tags.
- A token that FallUI's files do not have is looked up in the game's translations, as in M17.
- A row's help is looked up the same way. FallUI also names the help's entry in a key of its own, `$help`, beside the English text. The loader reads a help in M21, and translates it then.

M19 draws the tags of a text row and removes those of a help, with one parser, as in [Text, spacers and images](schema.md#text-spacers-and-images). It runs after the translation of M17 and M18, because FallUI's `${Key}` tokens are inside the tags. The loader reads a help in M21, which then removes its tags.

M101 adds an MCM config of our own, `data/showcase/MCM/Config/ModsConfigFO4_McmShowcase/`, with every kind of row an MCM config has, in every look: the sizes, colors and aligns of a text, the options of each control, and what a config can get wrong. It has only what works in MCM too, so the two menus are compared on it: what the config spec schema adds to MCM would be a showcase of its own. It is tried in the headset as each commit draws a row, and grows with them. It is packaged with the other showcase in one optional archive, to show a mod's author how an MCM menu looks in the mod. It has no game plugin, so what needs one stays with the test mod of M37.

M20 draws the four controls, and disabled, as they change nothing yet:

- The menu has no values before M25, so a control shows what it is with none: a switcher is off, a slider is at its least, and a stepper and a dropdown show their first option.
- The loader reads a slider's `min`, `max` and `step`, and the options of a choice. An MCM option is a text whose value is its place in the list: in the model it is an option with that number as its typed value, as a config spec's option has its own.
- A row's text has the room up to its own control, so a switcher's name is longer than a slider's before it ends in an ellipsis.

M21 shows the `help` text of the row the ray is on in the help line, on the left of the footer, as in [UI and VR interaction](ui-and-vr-interaction.md#help-line):

- The loader reads the help of a setting, of a button, and of a control the menu does not support yet. It is translated as the row's text is, and its tags are then removed.
- A row with no help shows its text alone, so a text that ends in an ellipsis in its row is read in full.
- The footer has the help line alone, at the footer's whole width. An MCM mod has no Save and Discard, and its status line comes with M25.
- FRIK's entry of the quick launch section has a help too.

F22 and F23 are item 11 of the requirements.

F22 adds `render::Texture::fromPixels`:

- The pixels are RGBA, four bytes each in rows from the top, and are drawn as given.
- The texture is created on the GPU when it is first drawn, so it is made on any thread, and its size is known from the start.
- It is shared with no one, and is drawn as a loaded texture is. It lives as long as its holders, and the frames that draw it.
- The creation of the GPU texture is unit tested, on a software device.

F23 adds `imgui::image` and `imgui::addImage`, which the content of a canvas calls in place of ImGui's own:

- `image` is an item at the cursor, and `addImage` a rectangle of a draw list. Both take a `render::Texture`, loaded from a file or made from pixels, and a tint.
- The published frame holds the views of its images, so a texture is dropped at any time, also while its panel is shown.
- A texture that is not ready, or whose file is missing, draws nothing and keeps its room.
- A texture whose view decodes sRGB is drawn with a shader that encodes it back, so it has the colors of its file.
- They call the game's renderer, so the mod's panel tests do not draw an image with them.

M22 reads an image out of the mod's `lib.swf`:

- An MCM image is a class of that file, the one in the config folder of the mod its row names. In the test set every one is a bitmap stored in it without loss. Six are in a class of their own beside a class of code, named `<class>_bitmap`, and one fills a shape that a sprite places.
- The menu reads the bitmap and does not run Flash. An image that is a JPEG, that is drawn as vectors or that is animated is a row that says an image is there, with the reason in the log.
- An image is as large beside the rows' text as in MCM's menu, and smaller where MCM shows it smaller or the rows are not as wide. It is in the middle of the rows, and its row is as high as it is. The `width` and `height` of its row are not used: in MCM they do not size the image.
- The pixels are read when the configs are loaded. Their textures are made when they are first drawn, and are dropped when the menu closes.
- Reading it needs zlib, from vcpkg at the pinned baseline.
- The MCM showcase has a `lib.swf` of its own, which `tools/gen_mcm_showcase_lib.py` writes.

M23 is the look of M100 as it was tried in the headset:

- The sizes of the design system stay as they are. The player sets the panel's size and place in `[Panel]` of the INI: `fScale`, `fDistance`, `fHeight` and `fTilt`, and how much its background hides the world in `iBackgroundOpacity`, which ships at the design system's `glass`. A change shows at once while the menu is open. The scale makes the text and the rows larger together, so the layout is the same at any size, and the panel's curve follows its distance.
- A row of a page has the look of a row under the ray, and the stronger one while the trigger is held on it, as a row of the tree has. A row with no text of its own has neither: a section, a text row, an image and a spacer.
- A setting's control is still drawn disabled until M25. The other states come with the commits that give them a meaning: disabled with conditions (M28) and a mod that is not available (M41), and pending and invalid with config specs (milestone 5).

M24 adds Back and the haptics of [UI and VR interaction](ui-and-vr-interaction.md):

- Back is a short press of a button of the hand that points at the panel, either hand: the grip as shipped, and `sBackButton` in the INI. It is not a binding with a hand of its own, as the panel's move button is not: the wand that points is hidden from the game, so the game does not get the press, and a press while the player looks away from the panel stays the game's and does not close the menu.
- The grip also moves the panel. While Back and the move are on one button, the panel is taken hold of only when the button is held for longer than a short press, 0.3 seconds. That is the framework's `UIManager::setWorldMoveHoldDelay`, which F33 adds: 0 by default, so the move button of another mod takes hold at once as before.
- In this milestone it goes from a page of a mod to the mod's front page, from there to the menu's front page, and from there it closes the menu. A later commit adds its own step to it: a dropdown is closed, and a drag or a text edit is cancelled.
- The hand that points gets a light pulse when its press does something: another page is shown, or a quick launch entry is opened. Back gives it too, as it always goes a step back or closes the menu. A press that changes nothing gives none: one on the row or the breadcrumb of what is shown already, on a scroll bar, or on a row of a page until its control works. The commit that makes a control work gives its change the pulse. The pattern is `sPressHaptic` in the INI, `Tick` as shipped, and `none` turns it off. The pulses of a Save and of a value that is rejected come with those commits.
- The devbench tool has a `back` action.

## 2. Edit MCM values

| #   | Repo      | Commit                                                                              | Status    |
| --- | --------- | ----------------------------------------------------------------------------------- | --------- |
| M25 | mod       | Globals are read and written: the switcher works                                    | done      |
| M26 | mod       | The slider                                                                          | done      |
| M27 | mod       | The stepper and the dropdown                                                        | done      |
| M28 | mod       | Conditions show and hide rows                                                       | done      |
| M29 | mod       | ModSettings are read from MCM's files                                               | done      |
| M30 | mod       | ModSettings are written through MCM                                                 | done      |
| M31 | mod       | MCM's events are sent: a setting changed, the menu opened and closed                | done      |
| M32 | mod       | Script properties are read and written                                              | done      |
| F24 | framework | f4sevr: script properties, if the calls it has do not work in the game              | done      |

Globals come first: they are most of the test set, and need neither MCM nor Papyrus.

M25 adds the store of [The settings model](model.md#the-store), and the MCM store, which saves a change as it is made:

- The loader reads where a row keeps its value. A `GlobalValue` names its global in `sourceForm`, as `MyMod.esp|1234`. A row with no `sourceType` is kept nowhere, and the store holds its value while the mod is selected. Any other source is not read until its commit, and its row stays disabled.
- A page asks the store for its values in every frame, so a global that a script or the console changes shows at once.
- A switcher is on when its global is 1 and off at any other number, and is written as 1 and 0, as in MCM's menu. A press anywhere on its row turns it, and gives the hand its pulse.
- A slider, a stepper and a dropdown show the value of their global, and stay disabled until M26 and M27.
- A global that is not in the game is not available: its plugin is not loaded, or has no global of that id. Its row is disabled, and the help line says why while the ray is on it.
- The help line says "Changes are saved as they are made" while a mod is shown and the ray is on no row. There is no Save bar.
- The store reads and writes a global with two functions, `mcm::getGlobal` and `mcm::setGlobal`. The unit tests link their own in their place, which keep the globals in a map, so the store is tested without the game.

M26 makes the slider work, as in [Controls](controls.md#slider):

- The trigger drags it. A pull on the track puts the grab under the ray, and the grab follows the ray until the trigger is let go, also beside the track. A pull on the grab itself does not move it. The value is on a step of the slider, counted from its least. It is written as the trigger is let go, and not while it is dragged: MCM writes its file for every ModSetting that is set.
- While it is dragged, the grab and the outline of the track are in `glow`.
- Back during a drag ends it with nothing written, so the slider has the value it had before the drag, and the rest of that press changes nothing. It is Back's first step, before the pages.
- The thumbstick steps the slider whose row the ray is on, once the ray is there for 0.3 seconds, so a ray that crosses a row steps nothing. A push to a side is one step. A push that is held steps again after 0.4 seconds, and faster after 1.5. A push that began off that row steps nothing until the thumbstick is let go. A push that is more up or down scrolls the page, and steps nothing. A slider is not selected, and has no focused state.
- The hand gets its pulse as it takes hold of a slider, and none for the steps of the drag. It gets one for each step of the thumbstick that changes the value.
- A slider whose `max` is not over its `min` is disabled.
- The box of the value only shows it: it is not typed (M46).
- The MCM showcase has two sliders with no `sourceType`, which work now. Its others are ModSettings, which M29 reads.

M27 makes the stepper and the dropdown work, as in [Controls](controls.md#dropdown-and-stepper):

- A stepper has a button to the option before and one to the option after. They stop at the first and the last option, where the button is disabled. A press anywhere else on the row goes to the option after, and from the last to the first, as in MCM's menu. A button has the look of a control under the ray, and the stronger one while the trigger is held on it.
- A press anywhere on the row of a dropdown opens its list, in a popup of ImGui inside the panel. The list is as wide as the box of the dropdown, and under it, or over it where the page has no room under it. It is never over the title bar and the footer. It shows five options at most, and one with more scrolls with the thumbstick, from the chosen option as it opens.
- A press on an option picks it and closes the list. A press beside the list closes it and does nothing else. Back closes it too, with no option picked: it is Back's step after a drag, before the pages.
- The chosen option is a solid fill with a check. While the list is open the box of the dropdown has its outline in `green`, and the help line keeps the row of the dropdown: an MCM option has no help of its own.
- The value is written as it is picked: for a global, the place of the option in the list.
- The hand gets its pulse as a press picks another option, and as it opens a list. A press that picks the option the choice is at gives none.
- A stepper and a dropdown with no options are disabled. One whose value is no option shows none: its list has no chosen option, and a stepper goes to its first option from there.
- The thumbstick steps the stepper whose row the ray is on: a push to a side is one step, to the option before or after, and stops at the first and the last as the buttons do. A push that is held steps no more, and there is no wait for the ray as a slider has. The hand gets its pulse for a step that changes the option.
- The MCM showcase has a stepper and three dropdowns with no `sourceType`, which work now. Its others are ModSettings, which M29 reads.

M28 translates `groupControl` and `groupCondition` to the model's conditions, as in [The settings model](model.md#conditions). Until this commit every row is shown.

- A setting has its id: its row's, or one the loader makes from its page and its place. A condition names the setting that controls its group by it, and the store gives the value of an id.
- A group is on while the value of the row that controls it is 1, as in MCM's menu: a switcher that is on. A group that two rows control is on with the later one.
- A `groupCondition` is a number, a list, `AND`, `OR` or `ONLY`. MCM's menu reads no condition inside another. The loader reads one, as DearModdingUI does, since that is what its author meant.
- A row that is not shown has no room in the page. A section's condition hides its heading alone, as in MCM's menu.
- A row whose condition needs a value that is not available is shown. So the rows of a group that a ModSetting controls are all shown until M29 reads it.
- A ModSetting with no id is kept nowhere, as it names no setting: FallUI's "Unfold settings" switchers work now, and so does the group of the MCM showcase.

M29 reads a ModSetting from MCM's two files, the mod's defaults and the player's values, which MCM writes on every set. It is plain file code.

- The defaults are `Data\MCM\Config\<mod>\settings.ini`, and the player's values `Data\MCM\Settings\<mod>.ini`, which is read over them.
- A ModSetting is named by the `id` of its row as the config has it, its key and its section: `fStrength:Main` is `fStrength` of `[Main]`, in the letter case of the file, as MCM finds it.
- The files are read as the mod is selected, and not in every frame as a global is. A value a script sets while the mod is shown is read by M42.
- The text of a key is read as the `sourceType` of its row says, as MCM reads it: a bool is off at `0` and on at any other text, and a number is read up to what is no part of one.
- A ModSetting whose text is no number is not available. The row is disabled, and the help line says why. One that is in neither file is what MCM reads for it (M37).
- A row shows the value of its ModSetting, and a press on it changes nothing until M30.
- A group that a ModSetting controls is now shown while that ModSetting is on.

M30 writes a ModSetting through `MCM.SetModSetting*`, so MCM's memory and its file agree. It is the menu's first call into Papyrus.

- The function is the one of the row's `sourceType`, as in MCM's menu: `SetModSettingBool`, `Int`, `Float` or `String`. A slider of a whole number is written as the nearest one, and a choice as the place of its option.
- The call is queued to Papyrus and gives no result, so the store keeps the text MCM writes to its file for the value. The row shows the change at once, and the player's file is read again as another page of the mod is shown (M42).
- A ModSetting that is in neither file is not written: MCM has only those of its files. The store holds it while its mod is selected, as MCM's menu does (M37).
- A unit test has no game, so MCM is not called there (`REL::NO_GAME`): it checks the value and the text the store keeps.
- The switchers, sliders, steppers and dropdowns of the MCM showcase that are ModSettings work now.

M31 sends the events MCM's menu sends, so a script acts on this menu as on MCM's:

- A script registers for an event with F4SE's `RegisterForExternalEvent`. F4SE gives a plugin the scripts of an event, and each is called with the function it registered, as F4SE calls it for MCM's menu. The call is the framework's `f4sevr`, which M30 calls MCM with.
- `OnMCMOpen` as the menu opens, and `OnMCMClose` as it closes. A menu that is minimized is still open.
- `OnMCMMenuOpen` and `OnMCMMenuOpen|<mod>` as a mod is selected, and `OnMCMMenuClose|<mod>` as it is left: for another mod, for the menu's front page, and as the menu closes, before `OnMCMClose`.
- MCM's menu differs in two things. It sends the close of a mod only as another mod is selected, and none as the menu closes. It sends the open again for every press on a row of its list, also on a page of the mod that is shown, where this menu sends none.
- `OnMCMSettingChange` and `OnMCMSettingChange|<mod>` after a value is written, with the name of the mod and the id of the row. A row with no `id` in its config sends none, as in MCM's menu. A setting keeps the id of its row for it, beside its own, which the loader can make. A slider that is dragged sends them once, as the trigger is let go.
- A ModSetting is set with a call that is queued to Papyrus, and the event's call is queued after it.
- The store sends them with one function, `mcm::sendEvent`. The unit tests link their own in its place, which keeps what is sent, so a test checks the events and their order without the game.

M32 reads and writes a property of a script, in all four types, as `mcm_vr.dll` does for MCM's menu:

- A `PropertyValueBool`, `Int`, `Float` or `String` names the form of its script in `sourceForm`, as a global does, and its property in `propertyName`. It can name the script in `scriptName`. With none it is the form's first script.
- A page asks for the value in every frame, as it does for a global, so a property that a script changes shows at once. The form and the place of the property in its script are looked up once.
- The value is read in the type the script keeps it in. A bool is on at any number but 0, as in MCM's menu, and a switcher of a number is on at 1.
- It is written in the type of the row's `sourceType`, as in MCM's menu: a slider of a whole number as the nearest one, and a choice as the place of its option. The game keeps it in the save.
- A property is not available while its form is not in the game, its form has no such script attached, or the script has no property of that name. Its row is disabled, and the help line says that it is not in the game. The log has which of the three it is, once.
- The store reads and writes it with two functions, `mcm::getProperty` and `mcm::setProperty`, in `McmProperties.cpp`. The unit tests link their own in their place, which keep the properties in a map.
- The calls are the framework's `f4sevr`, which has what `mcm_vr.dll` calls: `GetObjectIdentifier`, `GetVMPropertyInfo`, `GetPropertyValueByIndex` and `SetPropertyValue`.
- The test set has two, whole numbers of Sim Settlements 2. Game Configuration Menu is installed beside it for the trial: it has 171, numbers, whole numbers and bools, all on one form with no `scriptName`. The MCM showcase has one that is not in the game: a property needs a script of a plugin, which the showcase does not have.

F24 has no commit: the calls `f4sevr` has for script properties work in the game, as the trial of M32 found. The events need none: they are sent with what `f4sevr` has.

## 3. MCM complete

| #   | Repo | Commit                                                               | Status    |
| --- | ---- | -------------------------------------------------------------------- | --------- |
| M33 | mod  | Buttons run `CallFunction`, with typed parameters                    | done      |
| M34 | mod  | A value row runs its action after the write, with `{value}`          | done      |
| M35 | mod  | Hotkeys are shown read-only                                          | done      |
| M36 | mod  | Text rows are shown read-only, and unknown controls as placeholders  | done      |
| M102 | mod | A ModSetting is reset to the default of its mod                      | done      |
| M38 | mod  | A hidden switcher controls its group, and is not shown               | done      |
| M39 | mod  | A `dropdownFiles` lists the files of a folder                        | done      |
| M40 | mod  | The actions beside `CallFunction`                                    | done      |
| M37 | mod  | A trial with Global Stash and JOURNEY, and what it finds             | done      |
| M104 | mod | The text of a row has the size and the color of its font tags        | done      |
| M41 | mod  | A mod that is not available: its disabled row, and the notices       | done      |
| M42 | mod  | A page that is shown reads its values again                          | done      |
| M43 | mod  | docs: the table of MCM features and how the menu handles each        | done      |
| F34 | framework | imgui: bold and italic faces of the canvas font                 | done      |
| M106 | mod | `<b>`, `<i>` and `<u>` in the text of a row, and the panel's font size | done    |
| M105 | mod | A DLL that reads MCM's file as the pause menu closes reads it as the menu closes | done |

M33 makes a button run its `CallFunction`, as `mcm_vr.dll` does for MCM's menu:

- A button is its text and a `Run` button in the control column. A press anywhere on its row runs its action, and the hand gets its pulse as the call is queued.
- A `CallFunction` names the form of its script in `form`, as a global does, and its function in `function`. It can name the script in `scriptName`. With none it is the form's first script.
- The call is queued to Papyrus and gives no result. A script that has no such function, or one with other parameters, has its error in Papyrus's log alone.
- A parameter has the type MCM's menu gives it. A text that begins with `{i}` is a whole number, and with `{f}` a number: a text that is no number is 0, and a whole number is cut at its point. Any other text is a text. A JSON bool is a bool. A JSON number is a number, also one that is written whole: Papyrus does not call a function that takes a number with a whole one, and Game Configuration Menu's weather buttons have `"params": [0]` for a function that takes a number.
- One parameter differs from MCM's menu. A text that begins with `{b}` is off for nothing, `0` and `false`, and on for anything else. MCM's menu has it on for any text, also `{b}0` and `{b}false`, which is not what its author meant.
- A button with no action is disabled, and so is one whose `CallFunction` names no form or no function, with the reason in the log. A button with an action the menu does not run is disabled, and the help line says why (M40).
- A function whose form is not in the game, or whose form has no such script attached, is not called: the press does nothing, and the log has the reason.
- The store runs it with one function, `mcm::callFunction`, in `McmActions.cpp`. The unit tests link their own in its place, which keeps what is called.
- MCM's menu sends no event for a button, and neither does this one.
- The test set has 67 buttons with a `CallFunction`, in Sim Settlements 2 and Workshop Framework, and one with a `CallGlobalFunction`. The MCM showcase has one of each, and its `CallFunction` is of a form that is not in the game.

M34 makes a value row run its `CallFunction` as its value is set, as MCM's menu does:

- The order is MCM's: the value is written, then the function is called, then `OnMCMSettingChange` is sent. A value that is not written runs nothing. A slider that is dragged runs it once, as the trigger is let go.
- A parameter with `{value}` in it has the value the row is set to in its place: 1 and 0 for a switcher, the place of its option for a stepper and a dropdown, and the number of a slider. Only the first `{value}` of a text is the value, as in MCM's menu.
- `{value}` alone is given in the type of the row's `sourceType`: a number for a global, and what a ModSetting and a property are kept as.
- A text that begins with `{i}`, `{f}` or `{b}` is given in that type, after the value is put in it, as a button's is in M33. A bool is on at any number but 0. Any other text is a text with the value in it.
- A parameter with no `{value}` is as a button's. Sim Settlements 2 has `{i}{0}` to `{i}{10}`, which is no number and so 0, in MCM's menu too.
- One thing differs from MCM's menu. `{value}` alone, in a row with no `sourceType`, is the value as a text. MCM's menu gives the script nothing there, for any row but a text input.
- A function whose form is not in the game is not called, with the reason in the log. The value is set, and its event is sent.
- A row with an action the menu does not run sets its value, and runs nothing (M40).
- The model has it as the action of the setting. The store fills the parameters and calls `mcm::callFunction`, which M33 made.
- The test set has 33 value rows with a `CallFunction`, in Sim Settlements 2 and Workshop Framework: switchers, dropdowns, a stepper and sliders, all of a global or a property. 22 have `{value}`. The MCM showcase has one, of a form that is not in the game.

M35 shows the key of a hotkey, read from MCM's two keybind files. It is only shown: MCM binds a key in its own menu, and runs the hotkey as its key is pressed.

- A hotkey is its text and its key in a box, in the control column. Its whole row is always disabled, as the key is not changed here. A press on its row does nothing, and the row has no look of one.
- The help line says that it is read-only, before the help of the row: `(Read-only)`.
- The mod's file, `Data\MCM\Config\<mod>\keybinds.json`, has the hotkeys the mod has, each with its `id` and the action MCM runs for it. The player's file, `Data\MCM\Settings\Keybinds.json`, has the keys of every mod: a `keycode` and its `modifiers`, for a `modName` and an `id`.
- A row is its hotkey by the `id` of the row and the `modName` of its config, in any letter case, as in MCM.
- A key is named as MCM's menu names it: the name of the key, after the keys that are held with it, as `Ctrl-Alt-Shift-F5`. A key of the keyboard is its virtual-key code of Windows, and a button of the mouse is from 256. A code MCM's menu has no name for is its number, as `Key 270`.
- A hotkey with no key says `None`, as in MCM's menu.
- A hotkey is not available while the mod's file does not have it: MCM binds no key to it, and drops the key the player's file has for it. So is one of a mod with no such file, or with a file that names another mod. Its row looks as any hotkey's, and the help line says that it is not in the mod's `keybinds.json`.
- MCM also drops a hotkey whose action is of a form that is not in the game. That is not looked up, so such a hotkey shows the key of the player's file.
- The files are read as the mod is selected, as its ModSettings are. MCM writes the player's file as its own menu closes, so a key that is bound there shows the next time the mod is selected.
- The model has it as an item of its own, a hotkey with its id. The store answers its key, as it answers the value of a setting, with `McmKeybinds`.
- The test set has 23 hotkeys, 22 in Sim Settlements 2 and one in Workshop Framework, and each is in the `keybinds.json` of its mod. It has no player's file: no key is bound. The MCM showcase has a `keybinds.json` with three hotkeys, which show a notification, and a row of one that is not in it.

M36 shows the text of a text input, which is only shown until milestone 4, and a control the menu does not have as a row that says so:

- A text input is its text and the text of its value at the left of a box, in the control column. A longer one ends in an ellipsis. Its whole row is always disabled, as a hotkey's is: a press on it does nothing, and the row has no look of one.
- The help line says that it is read-only, before the help of the row: `(Read-only)`.
- The value is the one the store already has for it: the text of a ModSetting as MCM's files have it, also for a `textinputInt` and a `textinputFloat`, and a property of a script, with a number as its text. One with no `sourceType` is empty.
- A text input whose value is not available has an empty box, and the help line says why.
- A control the menu does not have is its text and the type its config names, in the control column. Its row is disabled too, and the help line says that the menu has no such control: `Not available: the menu has no color control.` A row with no type is `unknown`, and the help line says that it has no type.
- An image that could not be read stays a row that says `image`, with no help.
- The test set has 25 text inputs, in FallUI, FallUI Workbench and XDI, all ModSettings. The MCM showcase has five, and a `dropdownFiles`, a `color`, a type that no menu knows and a row with no type.

M102 gives a row whose setting has a default its Reset, which MCM's menu does not have. An MCM mod has a default for a ModSetting alone:

- The default of a ModSetting is the value the mod's `settings.ini` has for it, whatever the player's file has. A global and a property of a script have none, and neither has a ModSetting that is in the player's file alone.
- The UI asks the store for the default of a setting. The definition does not have it: it is in a file the store reads as the mod is selected.
- Reset is an icon button in the room every row keeps for it, after the control. A switcher, a slider, a stepper and a dropdown with a default have it, and it is disabled while the value is the default, so nothing comes and goes under the ray. A row with no default keeps the room empty, and so does a stepper and a dropdown whose default is no option of theirs.
- A press on it sets the default as any other value is set: it is written through MCM at once, the action of the row is run, and MCM's event is sent. The hand gets its pulse. There is no Save, so nothing takes a Reset back.
- A press on it does nothing else in the row: a switcher is not turned, a stepper is not stepped, and the list of a dropdown is not opened.
- The help line names the default after the help of the row, in brackets, as `(Default: 1.30)`: on and off for a switcher, the text of the option for a stepper and a dropdown, and the number of a slider as its box has it. While the ray is on a Reset that is not disabled, it says what Reset gives, with no period after it: `Reset to 1.30`
- The test set has 151 ModSettings. The ModSettings of the MCM showcase all have a default, so each of its rows has a Reset, which is disabled until the row is changed.

M38 reads a `hiddenSwitcher`, as MCM's menu (`mcm/MCM_Menu.as`) has it:

- It is a switcher that is never shown, whatever its `groupCondition`. A script sets its value, and it controls a group with its `groupControl`: a mod shows a row with it while something of the game holds, as Global Stash shows a warning while it is busy.
- It is a setting of the model with a condition that never holds, so the store has its value and the page draws nothing for it. The definition and the UI do not change.
- Its value is read in every frame, as any other, so its group shows and hides while the page is shown. MCM's menu reads it as the page is opened.
- One with no `sourceType` is off, so its group is never on, as in MCM's menu.
- The other controls of DearModdingUI's inventory are not MCM's: `hidden`, `menu`, `enum`, `color`, `switch`, `input`, `header`, `empty` and `keymap` are no type of MCM's menu, so no MCM config has them, and each stays a placeholder with its type. So do `positioner` and `keyinput`, which MCM's menu has and the inventory does not: no mod of the author's lists has one.
- The author's lists have 7 `hiddenSwitcher`: 4 in Global Stash, 2 in JOURNEY and 1 in NAC X. The MCM showcase has two, one on and one off, with a text row of the group of each.

M39 reads a `dropdownFiles`, as MCM's menu (`mcm/MCM_Menu.as`) has it:

- It is a dropdown whose options are `None` and the files of a folder: the one its `path` names from the game's folder, as `Data/Interface/Presets`, with the names that match its `mask`, as `*.xml`, or all of them with none. MCM's menu lists the folder with F4SE's `GetDirectoryListing`, which takes the mask of the system. This menu lists it the same way, so MO2's files are in it.
- A folder in the folder is no option: MCM's menu has it, and `.` and `..` with it, which no player picks. The files are in the order of their names.
- The folder is listed as the configs are loaded, as the game starts. A file that is added while the game runs is in the list the next time it starts: MCM's menu lists it as the page is opened. A folder that is not there has `None` alone.
- A storage that keeps a text keeps the name of the file, as `Preset.xml`, and an empty text for `None`: a `ModSettingString`, a `PropertyValueString`, and a row with no `sourceType`. `{value}` in its action is the name. A name that is no file of the list shows no option. Any other storage keeps the place of the option, as a dropdown does.
- One thing differs from MCM's menu. Its pick writes no name: the name of the file is lost on the way to the write, so MCM writes nothing useful and reads `None` the next time. Its own example (`KNOW ISSUES.txt`) keeps the name in a `ModSettingString` and loads it with `"Loading {value}!"`, which is what its author meant.
- No mod of the author's lists has one. The MCM showcase has three on its "Steppers and dropdowns" page, of its own folder, and one with no storage.

M40 runs the actions MCM's menu (`mcm/OptionsList.as`) runs beside `CallFunction`, or says why it does not:

- A `CallGlobalFunction` calls a global function of the script its `script` names, with its `params`, on a button and on a value row, as a `CallFunction` is called: the same parameters, `{value}` and order. It is queued to Papyrus as `mcm_vr.dll` queues it, with no lookup, so a script that is not in the game has its error in Papyrus's log alone.
- One with no `script` or no `function` runs nothing, with the reason in the log, as a `CallFunction` with no form.
- The model has it as a `CallFunction` with no form, and `mcm::callFunction` calls it.
- A `CallExternalFunction` is not run: it calls a Flash function an F4SE plugin gives MCM's menu, and this menu has no Flash. Its button is disabled, and the help line says so. No mod of the author's lists has one.
- MCM's menu runs no other action in a row: `RunConsoleCommand` and `SendEvent` are actions of a hotkey, which MCM runs. A row with one, or with an action of no type, is as one the menu does not run, and the help line of its button says that the menu does not run it.
- A value row with an action the menu does not run is set as any other, and runs nothing. The log has each such action as its config is loaded.
- The test set has one `CallGlobalFunction`, on a button. JOURNEY has 5, on value rows, with `{b}{value}` and `{i}{value}`. The MCM showcase has a button and a switcher of `Debug.Notification`, which need no script of a mod, and a button with a `CallExternalFunction`.

M37 tries in the game what M38 and M40 make and the test set does not have, with two mods that are installed beside it. Its commit is what the trial finds:

- Global Stash has a `CallFunction` with a `scriptName`, 15 times, one with a parameter that is a JSON bool, and 4 `hiddenSwitcher`.
- JOURNEY has a `CallGlobalFunction`, 5 times, with `{b}` and `{i}` parameters, and 2 `hiddenSwitcher`. Its MCM config is a mod of its own, beside the mod.
- Game Configuration Menu, which M32 installed, has the parameters that are JSON numbers.
- The MCM showcase has a `CallGlobalFunction` of `Debug.Notification` on a switcher, which M40 adds to it, with the value in its text.
- No mod of the author's lists has the rest, so it is not tried with a real script: a `PropertyValueString`, a property with a `scriptName`, an `OR` condition and one inside another, and the `dropdownFiles` of M39. The unit tests cover it, and the MCM showcase has a row of what needs no script.

JOURNEY's rows run its script, with their typed parameters. The trial, and a load of the 67 MCM configs of the author's four lists with the menu's own loader, find what the test set does not have. Each is read as MCM's menu reads it, or as the mod's own Flash code shows it:

- A FallUI language file can name the `libName` of its row: JOURNEY's is `../MCM/Config/{libName}/Translation/MCM_{libName}_${LANGUAGE_CODE}.txt`. M8r's intros put the row's `libName` in its place, and so does the loader.
- An image row of `M8r.View.McmIntros.M8rIntro` or `M8rIntroLight` shows no picture of its own: its code loads the file of the row's `m8rIntro`, or `logo.swf`, from the mod's config folder. The image is the class of that file itself, at 2/3 of its bitmap's size, as M8r's `EmbeddedImage` shows it. Global Stash, JOURNEY and NAC X have one. MCM Settings Manager's loads a menu of its own, written in Flash, and stays a row that says an image is there.
- A flag can be a text, as REPUTATION's `"html": "true"`. Flash has any text but an empty one as on, also `"false"`.
- MCM's menu sets the text of every row as html, whatever its `html` says, so a section, a setting, a button and a hotkey read its tags, on one line, as a help is. A newer Sim Settlements 2 has 18 switchers with `<font>` and `<u>` in their text, and Safe Travels has a game's token inside the tags of its sections, which is translated once they are removed. The spaces a text starts with stay, as FallUI sets a row in by them.
- A stepper and a dropdown can take their options from a list of the config: `sharedOptions` names a list of its `sharedLists`, in place of the row's own `options`. NAC X has 5. MCM's menu can also take them from a form or a property (`listFromForm`, `listFromProperty`), which no mod of the lists has, and the loader does not read.
- A ModSetting that is in neither of MCM's files is read as MCM reads it: off, -1 for a number, and an empty text. A row of it is set as any other, and the store holds the value while the mod is selected, as MCM's menu holds it: MCM sets no such ModSetting, and has none the next time. Its action runs, and its event is sent. The newer Sim Settlements 2 has no `settings.ini`, and its switchers that show the groups of its Gameplay page are such ModSettings.
- A hidden switcher whose value is not available is off, as MCM's menu reads it, so the rows of its group are hidden. A condition on any other setting whose value is not available still holds. Global Stash shows a button while a property of its script is on, which the script it names does not have: MCM's menu hides the button.
- Three are left as they are. FallUI Inventory's 5 images are shown at 0.66 of their size in MCM's menu, and here at their size: the scale is an argument of the code of each class, which the menu does not read, and FallUI Workbench's and NPCs Travel's are shown at their size. Global Stash's `${$SKKGSIsBusy}` is no token FallUI's code reads either. REPUTATION's `align` of a text row is the same as the `<p align>` of its text.
- The MCM showcase has a stepper and a dropdown of a shared list, a switcher with tags in its text, and a text row whose `html` is `"true"`.

M104 draws the text of a row as MCM's menu does: a setting, a button, a hotkey and a placeholder have the size and the color of its `<font>` tags, on one line. A size is no larger than the rows' text, so the text stays in its row, and a `<br>` and a paragraph are a space. Sim Settlements 2's switchers are smaller, and JOURNEY has one in gray. A section is its text without the tags, in the look of a heading. The help line and the log have the text without its tags. The MCM showcase has a switcher in a small gray, and one with a red and a large word that ends in an ellipsis.

M41 shows a mod that is [not available](ui-and-vr-interaction.md#notices), as MCM's menu (`mcm/MCM_Menu.as`) finds one:

- A config's `pluginRequirements` names the game plugins its mod needs. While one is not loaded the mod is not available, and the reason names each that is not: `its plugins A.esp and B.esp are not loaded`. A plugin is loaded as `mcm_vr.dll` checks it for MCM's menu: one that is in the Data folder and not enabled is not.
- A config that cannot be read is a mod that is not available, named by its folder, as in MCM's menu: `its config cannot be read`. The log has what is wrong with it.
- A mod that is not available is its names and the reason alone: its pages are not read. The log has the reason too.
- Its row is in the tree, in its place by name, and is disabled: a press does nothing, and the row has the look of a row under the ray. While the ray is on it, the help line says why: `Sim Settlements 2: Not available: its plugin SS2.esm is not loaded.`
- The front page has a `Notices` section after the quick launch section, with a line for each such mod and the same reason: `Sim Settlements 2 is not available: its plugin SS2.esm is not loaded.` It is left out while there is none.
- `minMcmVersion` is not checked: it asks for a version of MCM's own menu, and a row this menu does not show says so on its own.
- 43 of the 69 MCM configs of the author's lists name a required plugin. A trial needs no test mod: a plugin of an MCM mod is disabled in MO2, and its config kept.

M42 reads the player's file of a ModSetting again as another page of its mod is shown, so a value a script set since shows there:

- A global and a property of a script are read in every frame already (M25, M32). A ModSetting is the one storage that is read from a file, as its mod is selected.
- MCM writes `Data\MCM\Settings\<mod>.ini` on every set, also one of a script, so the file has what MCM has in its memory. It is read again over the mod's defaults as another page of the mod is shown, also its front page. The mod's own `settings.ini` does not change while the game runs, and is not read again.
- It is not read while a page is shown: no player expects a page to change under the ray. A script that sets a value as the player changes another shows it on the next page.
- Hotkeys are not read again: MCM writes their file as its own menu closes, which is not open while this one is.
- No mod of the test set sets a ModSetting while its page is shown. The trial sets one of the MCM showcase from the console, and shows another page and back: `cgf "MCM.SetModSettingBool" "ModsConfigFO4_McmShowcase" "bOn:Switchers" false`.

M43 is [MCM support](mcm.md): a table of every feature of an MCM config and of MCM VR, and how the menu handles each, checked against the MCM wiki and MCM VR's menu:

- It answers the MCM items of [Open questions](open-questions.md). Any `mcm_vr.dll` serves, as the menu calls nothing of it but `MCM.SetModSetting*`. MCM's hotkeys stay MCM's, for a key of the keyboard, the mouse or a gamepad.
- It also lists the names of DearModdingUI's inventory that are no type of MCM's menu, as `menu` and `switch`, and that each is a placeholder: M38 has why.
- MCM's menu reads more than the wiki has, and no mod of the author's lists has it: the pages of a config under another mod (`ownerModName`), the requirements of a page, a text from a form or a property, options from a form or a property, and a spacer's `height` in pixels. The table has each as not read.
- Three DLLs of the author's lists read MCM's file as the pause menu closes, which this menu does not open: M105.

F34 gives the canvas font a bold and an italic face, for a run of text in the content (`imgui::canvasFont`):

- They are Roboto Black and Roboto Medium Italic, embedded beside the Roboto Medium. The bold face is Black, as Roboto Bold is too close to Medium to tell apart. A mod's own are `<ModName>-Bold.ttf` and `<ModName>-Italic.ttf`, beside its `<ModName>.ttf`.
- All three are Roboto 2.137 as Google Fonts ships it. ImGui sizes a font by its line, which is shorter in this build, so ImGui's text is 12.5% larger at the same size. The text of vrui is sized by its capitals, and keeps its size.

M106 draws `<b>`, `<i>` and `<u>` as MCM's menu does, in a text row and in the text of a setting, a button, a hotkey and a placeholder:

- `<b>` is the bold face, `<i>` the italic face, and `<u>` a line under the words, with the spaces between them. A text that is both bold and italic is bold: the panel has no face for both.
- A tag holds from where it opens to where it closes, inside a `<font>` and around one.
- A section is its text without its tags, in the look of a heading. The help line and the log have the text without its tags.
- The panel's font is set to 21 px, as the framework's default of 24 px is 12.5% larger with F34: 21.3 px is the size the rows' text has now. The title and the detail text are sized from it, as they are. The design system's type tokens take ImGui's px as 2048/2400 of an em.
- Eight mods of the author's lists have `<b>`, in 35 texts. The newer Sim Settlements 2 has `<u>` on 18 switchers, and Game Configuration Menu on one text. No mod of the lists has `<i>`. The MCM showcase has a text row with all three, and a switcher with `<u>`.
- The design system's rule of one face becomes: the panel's own text is in one face, and a mod's text is bold or italic where its tags say.

M105 makes a DLL that reads MCM's settings file as the pause menu closes see a change made here, as it sees one of MCM's menu:

- MCM's menu is in the pause menu, so such a DLL reads its file as the player leaves MCM. This menu does not open the pause menu, so without M105 the DLL reads the change the next time the player closes the pause menu.
- As the menu closes, after it wrote a ModSetting of any mod while it was open, it sends the game's `MenuOpenCloseEvent` of `PauseMenu`, opening and then closing, to the listeners of that event. The pause menu itself is not opened. Both are sent, so a mod that counts the menus that are open stays right.
- A global and a property of a script are kept in the game, which a DLL reads at any time, so a change of one sends nothing.
- Three DLLs of the author's lists do it: MCM Controls says in its log that it checks MCM's file as the pause menu closes, and VR HUD Config and VR Shadow Boost watch the pause menu and read their MCM file too. None is in the test set: the trial installs MCM Controls beside it, changes a setting of it here, and closes the menu.
- The trial also checks that FRIK and the other mods of the test set do nothing the player sees for the two events.

## 4. VR keyboard

| #   | Repo      | Commit                                                      | Status    |
| --- | --------- | ----------------------------------------------------------- | --------- |
| F25 | framework | docs: the VR keyboard's design                              | done      |
| M44 | mod       | docs: the keyboard's design, as item 12 of the requirements | done      |
| F26 | framework | imgui: the keys of the game's window reach a text field     | done      |
| F27 | framework | imgui: SteamVR's keyboard while a text field is edited      | done      |
| M45 | mod       | MCM text rows are edited                                    | done      |
| M46 | mod       | A slider's value is typed                                   | rejected  |

F25 is the design, the framework's [VR keyboard](../../external/F4VR-CommonFramework/docs/tech/vr-keyboard.md). The keyboard is SteamVR's, which the game types its own names with, so the framework draws none of its own:

- F26 hands the keys of the game's window to ImGui. A physical keyboard types into a field, and so does a keyboard overlay that types into the window. On a runtime with no keyboard, as OpenComposite, that is the only way to type.
- F27 shows SteamVR's keyboard by itself while ImGui edits a text field, so the content calls `ImGui::InputText` as on a screen.

M44 brings the design to it: item 12 of the [requirements](framework-requirements.md#12-text-entry-and-an-on-screen-keyboard), the text input of [Controls](controls.md#text-input), and the questions of [Open questions](open-questions.md) it answers. It moves the submodule to F25.

M45 types a number on the same keyboard: there is no number pad.

M45 edits a text input, as MCM's field (`mcm/Option_TextInput.as`) is edited:

- A press anywhere on its row starts the edit: the field in its box is ImGui's, with the caret at the end of the text. The framework shows SteamVR's keyboard for it, and a physical keyboard types into it too. The hand gets its pulse.
- While it is edited, its box has its outline in `green`, as an open dropdown has. The row is not pressed meanwhile, so a press on the field is the field's.
- Done writes the text, as the field is left, when it differs from the one it started with. The keyboard's close button gives the field back the text it had, and writes nothing. Back, on a runtime with no keyboard, where the keyboard holds no controllers, does the same: it is Back's step after a drag and a list, before the pages.
- A press beside the field, with the keys of the window, leaves it with its text and writes it, as Done does, and as a field on a screen does.
- A `textinputInt` takes digits and a minus, and a `textinputFloat` digits, a point and a minus, as MCM's field. A text that is no number is not written, and the row keeps its value. A number is shown as MCM's menu shows it: a whole number, and a number with 6 decimals at most, so `3.1400` in a file is `3.14`. The model has what a text input takes as its `valueType`.
- It is written as any other value: through MCM at once for a ModSetting, then the action of the row with the text as `{value}`, then MCM's event.
- It has its Reset, as the rows of M102: a ModSetting with a default in the mod's `settings.ini`. The help line names a text default in quotes, so an empty one shows: `(Default: "Sanctuary Hills")` and `Reset to ""`. A number has none: `(Default: 3.14)`.
- The edit ends with nothing written as another page is shown.
- A number text input of a global is read and written as a number. No mod has one.
- The four mod lists have 83 text inputs, all ModSettings: 73 texts, as FallUI's keys and regular expressions, 9 numbers, as XDI's place of the subtitles, and one whole number. The MCM showcase has a text, an empty one, a long one, a whole number and a number.

M46 is rejected: a slider's value is not typed. The drag and the thumbstick set it on its steps, and MCM's menu has no such entry either.
