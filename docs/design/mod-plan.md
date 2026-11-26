# Work plan

What is left to build, in order, and the design questions still open. The design docs describe the mod as it is; finished work is in git history and the [changelog](../changelog.md).

## How the work is done

- One commit at a time, reviewed and tried in the game before it is committed. A milestone ends with its "done when", tried in the headset.
- Numbering: mod commits `M`, framework `F`, devbench (its own repository) `D`. A new commit takes the next free number: M180, F98, D2.
- Game-free code is unit tested ([testing](../agents/testing.md)); the commit that draws a control adds its parsing and tests.
- A commit that settles an open question updates the design docs.

## Milestones

| #   | Milestone         | Done when                                                                                              |
| --- | ----------------- | ------------------------------------------------------------------------------------------------------ |
| 14  | Localization      | A mod with no game plugin is shown in another language                                                 |
| 15  | Advanced features | A list and a hand pose of Immersive Flashlight VR are edited, and a transform is grabbed with the hand |
| 16  | Papyrus API       | A script-only mod reads its saved settings and is told of a Save                                       |

### 14. Localization

| #   | Repo      | Commit                                                                       |
| --- | --------- | ---------------------------------------------------------------------------- |
| M90 | mod       | A translation file is read for a config spec whose mod has no game plugin    |
| F98 | framework | imgui: glyphs beyond Western European languages, if a translation needs them |

- **M90** reads `Interface\Translations\<modName>_<language>.txt` itself: F4SE loads translations only for an enabled game plugin, so a DLL-only mod has none.
- **F98**: the canvas font is the embedded Roboto (Latin, Greek, Cyrillic, Vietnamese). Chinese, Japanese, Korean or Arabic text needs a fallback font merged into it.

### 15. Advanced features

| #    | Repo | Commit                                                        |
| ---- | ---- | ------------------------------------------------------------- |
| M143 | mod  | List                                                          |
| M133 | mod  | HandPose                                                      |
| M144 | mod  | Transform: grabbed with the hand                              |
| M147 | mod  | Menu color                                                    |
| M99  | mod  | The Categories page imports the categories of MCM Categorizer |

- **M143** edits a [List](design-system/project/components/List/README.md); Immersive Flashlight has 5. The design is written, not reviewed yet.
- **M133** edits a [Hand pose](design-system/project/components/HandPose/README.md); Immersive Flashlight has 4. Design written, not reviewed; open: whether the menu's FRIK tag is over a mod's own, and the joint ranges.
- **M144** lets the player grab a transform's object and move it by hand. Design open: [grabbing a transform](#commonframework-controls).
- **M147** lets the player change the menu's color: one house color in the mod's config, from which the tokens' green is made (rules, hover, selection, text), with Pip-Boy presets and a custom color in the mod's own config spec. The drawing reads the theme that is set, so the commit builds a theme from the color and sets it. Amber (pending) and red (invalid) must stay readable, so an amber or red menu would lose them. Open: [menu color](#ui).
- **M99** adds an `Import from MCM Categorizer` button at the top of the [Categories page](architecture.md#mod-categories), shown when [MCM Categorizer](https://www.nexusmods.com/fallout4/mods/66311)'s `Data/MCM/Settings/MCMCategorizer.ini` exists. It gives each mod in one of its categories that name as a pending choice (Save keeps, Discard takes back); the file is only read. It keeps its categories in one value in its own encoding; F4SEMenuFramework reads the same value and is the reference. First check that MCM Categorizer runs with MCM VR: if not, no player has the file and the commit is dropped.

### 16. Papyrus API

Design: [Papyrus API](architecture.md#papyrus-api).

| #   | Repo | Commit                                                                      |
| --- | ---- | --------------------------------------------------------------------------- |
| M92 | mod  | The Papyrus getters, with config specs loaded when the game's data is ready |
| M93 | mod  | The Papyrus event after a Save                                              |
| M94 | mod  | docs: the script author's guide                                             |

## Design system upkeep

- Tokens: the usage text of `space-6` and `red` in `tokens.json` (from which `src/Theme.h` is generated) still names the row's gutter, which is gone.

## Open questions

Not assigned to a milestone.

### CommonFramework controls

- **Grabbing a transform** (M144): the player grabs the object the transform places and moves it with the hand. The menu needs where the object is in the world, and to turn a place of the hand back into the transform's value, through the node it sits on. A proposal: a callback that, without side effects, gives the world positions of the configured position and of three axis offsets from it in one frame, from which an invertible affine mapping converts back to config coordinates, also for an attachment that moves. Open: the API, what the framework can answer by itself, and a mapping that cannot be inverted.
- **Buttons:** cancel and progress for a running action.

### Localization

- Placeholders, plurals and runtime values in a text.
- Whether an asset path can vary by language.
- A tool that finds missing and unused tokens.

### Native API

- Whether a host-owned custom panel is needed.

### Papyrus API

- How a color is returned to a script.
- Whether the getters run without waiting (`NoWait`), which needs them thread-safe. MCM's do not.
- How soon a getter sees a file that was changed outside the menu.
- What happens to a script that calls a missing `ModsConfigFO4` script, so a script mod can treat the menu as optional; checked in the game.
- Whether scripts are told to read their settings again after a save is loaded.
- Whether script mods get live preview: a preview event, and getters that return pending values.
- Whether scripts can change settings.

### Save

- Whether to keep a last-good copy of a file, against a crash during its write.

### UI

- Whether world geometry hides the Full panel, and where it goes when a wall is closer than its distance.
- Accessibility beyond the panel's scale.
- Menu color (M147): which presets it has while amber and red mean a pending and an invalid change, and whether a "Match the Pip-Boy color" choice reads the game's Pip-Boy color and follows it.
- A ray press for vrui buttons, if vrui buttons end up in the panel, out of finger reach.

### Profiles, backups and portability

- Back up and restore all mod configurations, or one mod's.
- Files that cannot be restored safely, and how that is explained.
- Global profiles or per-mod profiles.
- Paths that include the player's My Games folder, across machines.
- Restoring over newer external changes.
- An author's presets as profiles shipped beside the config spec.
