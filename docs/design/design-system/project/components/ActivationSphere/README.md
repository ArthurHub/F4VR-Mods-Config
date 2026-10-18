# ActivationSphere

A CommonFramework activation sphere: a whole section of the mod's TOML, a zone the hand reaches into and what a press there does, drawn as one group of rows under a heading with the sphere's mark. Each key is its own setting, with the control of its kind; the group's role is what draws them together.

**Provides:** the group's name and help, and its settings by key: `tZone` and `tZonePA`, `kPrimaryBinding` and `kSecondaryBinding`, `sEntryHaptic`, `sPrimaryHaptic` and `sSecondaryHaptic`, `sShowIcon`, `sIcon`, `cIconColor`, `fIconSize`, `sShowSphere`, `sSphereStyle` and `fSphereScale`; the keys the section has.

## The group

- A section ([SectionHeader](../SectionHeader/README.md)) whose heading has the sphere's mark before its name: a 20 px drawn sphere, a `green` ring over a `green` glow (`AddCircleFilled` with a radial fade, then `AddCircle`).
- While the ray is on its heading or one of its rows, the mod's client draws that sphere lit in the world, and the heading says "Lit in the world" in `t-detail` `green-muted` at its right, after the rule, and the sphere's mark has a halo. The player sees which sphere the rows are of.
- Its rows, in this order, each the control of its key:
  - **Zone** (`tZone`), a [Transform](../Transform/README.md). Its In the world tunes the zone where it is, on the body, the head or the hand.
  - **Zone in power armor** (`tZonePA`), a Transform, only for a sphere whose package says it has one. While the file has no `tZonePA`, the row says `As Zone` in `green-muted`, as the framework uses the zone; its editor starts from the zone's values, and a change writes the key.
  - **Actions**, a section in the section: the primary and the secondary binding ([InputBinding](../InputBinding/README.md)), each named for what it does by the package ("Off hand", "Primary hand"), and the entry, primary and secondary haptics ([Haptic](../Haptic/README.md)). A binding set to none hides the haptic of its action.
  - **Icon and sphere**, a section that folds, folded when the page opens: when the icon shows (a choice of `Never`, `Always`, `When inside`, `When available`), the icon ([FileChoice](../FileChoice/README.md)), its color ([Color](../Color/README.md)) and size; when the sphere shows, its style and its size. The rows of an icon or a sphere set to `Never` are disabled.
- A key the section does not have, and the package does not name, has no row: the Head sphere has no icon, so its Icon and sphere section has only the sphere.
- Each key is dirty, previewed and saved alone, as any setting: a change to the primary binding writes `kPrimaryBinding` and nothing else.

## Live

- The sphere's keys are `live`: a change shows on the sphere in the world as it is made, through the framework's preview of a key.
- The page's spheres are drawn while the page is shown, by the page events (M113). Lighting the one the ray is on needs the client to know which group the ray is in: a group event, beside the page events. Open: this is framework work, and without it the spheres are drawn as the page events draw them, none lit.

## Focused panel

The same rows, in the Focused panel's single column. The Focused panel is the place to tune a zone while reaching into it.

The behavior is in [The settings model](../../../../model.md#an-activation-sphere) and [Controls](../../../../controls.md#activationsphere).
