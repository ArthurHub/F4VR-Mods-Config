# NumberDisplay

How a number is written in its control and in the help line, from the setting's `display`: its decimals, its unit, or a percentage of its range.

**Provides:** the value, the `step`, and `display`: `decimals`, `unit` and `percent`.

- **Decimals:** `display.decimals`, or else as many as the `step` has: a step of 0.01 writes `1.25`, a step of 1 writes `8`. The value is rounded to them for showing only; the stored value is the number itself.
- **Unit:** after the number. A unit that starts with `°`, `%`, `′` or `″` follows it with no space (`40°`), and any other with a space (`1.78 m`, `8 h`). It is a localized text.
- **Percent:** `display.percent` writes the place of the value in its range, from 0% at `min` to 100% at `max`, rounded to whole percents unless `decimals` says more: an opacity of 217 from 0 to 255 is `85%`. The help line has the stored number too: "85% (217 of 255)".
- A negative number has a hyphen-minus, `-0.50`, which every font of the game has.
- The decimal mark is a point. Another language's mark comes with the localization.
- **In the value box** (`value-width`, 104 px): right-aligned tabular digits in `t-body`, as the slider's. A text that does not fit is drawn in `t-detail`. When that does not fit either, the number is drawn alone, and the unit is in the help line. A number is never shortened.
- A numeric stepper centers the same text in its box.
- **In the help line**, the value is written the same way, and so is the default in its brackets: "(Default: 100%)". Reset's help names the value it resets to the same way: "Reset to 100%".

The behavior is in [Display](../../../../schema.md#display).
