# ActivationSphere

A CommonFramework activation sphere: a whole section of the mod's TOML, a zone the hand reaches into and what a press there does, drawn as one group of rows under a heading with the sphere's mark. Each key is its own setting, with the control of its kind; the group's role is what draws them together. The config spec declares it as one `cf.activationSphere` item; see [Schema](../../../../schema.md#commonframework-values).

**Provides:** the group's name and help, and its settings by key, each with the text and the help the config spec gives it or its own, and whether it is shown; the keys the section has.

## The group

- A section ([SectionHeader](../SectionHeader/README.md)) whose heading has the sphere's mark before its name: a 20 px drawn sphere, a `green` ring over a `green` glow (`AddCircleFilled` with a radial fade, then `AddCircle`).
- While the ray is on its heading or one of its rows, the mod's client draws that sphere lit in the world, and the mark has a halo, so the player sees which sphere the rows are of. The heading has no text for it: the player sees the sphere itself.
- First its main rows, then three sections that fold ([SectionHeader](../SectionHeader/README.md)), set in a level, each folded each time the page is shown. Folded, a section says how many of its rows are pending or invalid, as any section that folds.

| Where | Key | Its row, as named by default | Control |
| --- | --- | --- | --- |
| Main | `kPrimaryBinding` | Activate | [InputBinding](../InputBinding/README.md) |
| Main | `sEntryHaptic` | Entry haptic | [Haptic](../Haptic/README.md) |
| Main | `sShowIcon` | Show icon | Dropdown: `Never`, `Always`, `When inside`, `When available` |
| Position and size | `tZone` | Zone | [Transform](../Transform/README.md) |
| Position and size | `tZonePA` | Zone in power armor | Transform |
| Activation | `sPrimaryHaptic` | Primary haptic | Haptic |
| Activation | `kSecondaryBinding` | Secondary activate | InputBinding |
| Activation | `sSecondaryHaptic` | Secondary haptic | Haptic |
| Visuals | `sIcon` | Icon | [FileChoice](../FileChoice/README.md) of the framework's icons in the mod's folder |
| Visuals | `cIconColor` | Icon color | [Color](../Color/README.md) |
| Visuals | `fIconSize` | Icon size | [Slider](../Slider/README.md) |
| Visuals | `sShowBindingIcon` | Show binding icon | Dropdown, as Show icon |
| Visuals | `sBindingIcons` | Binding icons | Dropdown: `Of the hand inside`, `All`, `Primary`, `Secondary` |
| Visuals | `tBindingIcon` | Binding icon position | Transform, with no rotation |
| Visuals | `cBindingIconColor` | Binding icon color | Color |
| Visuals | `sShowSphere` | Show sphere | Dropdown, as Show icon |
| Visuals | `sSphereStyle` | Sphere style | Dropdown of the framework's styles |

- Each row is named by its config spec's `text`, or as the table names it: a mod names its bindings for what they do ("Take the light", "Switch hands"). Each has its help in the help line, the config spec's or its own.
- Each row is shown unless the config spec hides it. `kPrimaryBinding` is always shown. A key that is hidden has no row and keeps the value the file has.
- Hiding Show icon hides the icon's rows, hiding Show binding icon the binding icon's, and hiding Show sphere the sphere's. A section with no row shown is left out, its heading too.
- The framework's other sphere keys, `fSphereScale` (Sphere size, a slider), `sSphereOrientation`, `cSphereColor`, `fSphereGlow`, `sSphereNif`, `sSphereTexture` and `aSphereFalloff`, have a row under Visuals, after the sphere's, only where the config spec names them. Sphere mesh and Sphere texture are FileChoices too, whose first option is the style's own.
- A binding set to None hides its haptic: with Secondary activate off, Secondary haptic has no row. The rows of an icon, a binding icon or a sphere set to `Never` are disabled.
- Zone in power armor has a row only while the file has `tZonePA`, or the config spec gives it a default: the framework uses the zone without it. With no default its Reset slot is empty.
- A key the file does not have shows the value the framework uses for it, its default, or the config spec's `default` for it.
- Each key is dirty, previewed and saved alone, as any setting: a change to the primary binding writes `kPrimaryBinding` and nothing else.

## Live

- The sphere's keys are `live`: a change shows on the sphere in the world as it is made, through the framework's preview of a key.
- The page's spheres are drawn while the page is shown, by the page events (M113). Lighting the one the ray is on needs the client to know which group the ray is in: a group event, beside the page events. Open: this is framework work, and without it the spheres are drawn as the page events draw them, none lit, and the mark has no halo.

The behavior is in [The settings model](../../../../model.md#an-activation-sphere) and [Controls](../../../../controls.md#activationsphere).
