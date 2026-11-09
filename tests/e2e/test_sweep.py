"""The sweep: each mod and page of the MCM test set, of the showcases and of the config specs is shown, and each setting
the player can change is changed, saved, checked in its file, then set back, saved and checked again, one at a time.

Each mod is a test of its own, which reports every setting that failed and the ones it skipped, with -rA.
"""

import pytest

from conftest import CONFIG_SPEC_SHOWCASE, CONFIG_SPECS, MCM_SHOWCASE, TEST_SET
from devbench import DevbenchError, wait_until
from menu import page_paths, plain, same
from stored import NotInFile, file_value, inline_table, stored_as, table

pytestmark = pytest.mark.sweep

SWEPT = [*TEST_SET, MCM_SHOWCASE, CONFIG_SPEC_SHOWCASE, *CONFIG_SPECS]

# the patterns a haptic takes when its row lists none
HAPTICS = ["Tick", "DoubleClick", "Heartbeat3"]

# the settings whose failure is an open issue, by their mod and id: listed as known, and not failed
ISSUE_1 = "https://github.com/ArthurHub/F4VR-Mods-Config/issues/1"
KNOWN = {
    ("SS2", "bMoveInOutpostSoldiers:War"): ISSUE_1,
    ("SS2", "bMoveInTerritoryGuards:War"): ISSUE_1,
    (MCM_SHOWCASE, "bOn:Switchers"): ISSUE_1,
}


def shows(shown, value):
    """Whether a row shows a value: a number text input shows the number its text is."""
    if same(shown, value):
        return True
    if isinstance(shown, bool) or isinstance(value, bool):
        return False
    try:
        return same(float(shown), float(value))
    except (TypeError, ValueError):
        return False


def line_candidates(kind, value):
    """The values other than its own to try of a binding, a color or a transform kept in an INI file, which is its line:
    a binding on the other hand, a color with another red, and a transform with another first number."""
    if kind == "cf.inputBinding":
        hand, _, rest = value.partition(" ")
        if hand in ("primary", "offhand"):
            yield f"{'offhand' if hand == 'primary' else 'primary'} {rest}"
    elif kind == "cf.color":
        channels = [int(channel) for channel in value.split(",")]
        for delta in (1, -1):
            if 0 <= channels[0] + delta <= 255:
                yield ",".join(str(channel) for channel in [channels[0] + delta, *channels[1:]])
    else:
        first, _, rest = value.partition(",")
        for delta in (0.1, -0.1):
            yield f"{round(float(first) + delta, 4)},{rest}"


def candidates(row):
    """The values other than a setting's own to try, the likeliest valid first."""
    value, kind = row["value"], row["type"]
    if kind == "switcher":
        yield not value
    elif kind == "slider":
        bounds = row["range"]
        step = bounds["step"] or (bounds["max"] - bounds["min"]) / 10 or 1
        for moved in (value + step, value - step):
            if bounds["min"] - 1e-6 <= moved <= bounds["max"] + 1e-6:
                yield round(moved, 6)
    elif "options" in row:
        yield from [option["value"] for option in row["options"] if not same(option["value"], value)][:2]
    elif kind == "cf.haptic":
        yield from [name for name in HAPTICS if name != value]
    elif kind == "textinput":
        try:
            number = float(value)
        except (TypeError, ValueError):
            yield from [f"{value}_e2e", f"{value}e", *([value[:-1]] if len(value) > 1 else []), "e2e"]
        else:
            # a number text input's value is its text, or a number
            for moved in (number + 1, number - 1):
                moved = int(moved) if moved.is_integer() and "." not in str(value) else moved
                yield str(moved) if isinstance(value, str) else moved
    elif kind in ("cf.inputBinding", "cf.color", "cf.transform") and row["storage"].get("format") == "ini":
        yield from line_candidates(kind, value)
    elif kind == "cf.inputBinding":
        binding = table(value)
        if binding.get("hand") in ("primary", "offhand"):
            yield inline_table({**binding, "hand": "offhand" if binding["hand"] == "primary" else "primary"})
    elif kind == "cf.color":
        color = table(value)
        for delta in (1, -1):
            yield inline_table({**color, "rgb": [color["rgb"][0] + delta, *color["rgb"][1:]]})
    elif kind == "cf.transform":
        transform = table(value)
        part = next((name for name in ("pos", "rot") if name in transform), None)
        for delta in (0.1, -0.1):
            if part:
                yield inline_table({**transform, part: [round(transform[part][0] + delta, 4), *transform[part][1:]]})
            elif "scale" in transform:
                yield inline_table({**transform, "scale": round(transform["scale"] + delta, 4)})


def why_skipped(row):
    """Why the sweep does not change a setting, or None when it does."""
    if not row["shown"]:
        return "hidden by its condition"
    if row["readOnly"]:
        return "read-only"
    if not row["available"]:
        return f"not available: {row['reason']}"
    if not row["enabled"]:
        return "disabled"
    if row["invalid"]:
        return f"invalid: {row['reason']}"
    if row["storage"]["kind"] in ("none", "unknown"):
        return "kept nowhere: the menu holds its value, and a Save has nothing to write"
    if "options" in row and not any(same(option["value"], row["value"]) for option in row["options"]):
        return f"its value {row['value']!r} is none of its options, which the menu cannot set back"
    return None


def check_file(bench, mod, storage, value):
    """Fail unless the setting's file has the value; a setting kept in no file is not checked."""
    try:
        # MCM writes its file from Papyrus, on its own time, and its first write of a run is the slowest
        wait_until(lambda: stored_as(file_value(bench, mod, storage), value), 10, what="the file's value")
    except NotInFile:
        return
    except TimeoutError:
        raise AssertionError(f"its file has {file_value(bench, mod, storage)!r}, not {value!r}") from None


def sweep_setting(menu, bench, mod, row):
    """Change a setting, save it and check it in its file, then set it back the same way. Returns a reason it was not
    changed, or None."""
    setting_id, original = row["id"], row["value"]
    for candidate in candidates(row):
        answer = menu.call("setValue", id=setting_id, value=candidate)
        changed = answer["setting"]
        if changed["storage"] != row["storage"]:
            menu.discard()
            return "its id is another setting's too, which setValue sets"
        if answer["set"] and not changed["invalid"]:
            break
        menu.discard(setting_id)
    else:
        return "no other value it takes"

    assert shows(changed["value"], candidate), f"set to {candidate!r}, it shows {changed['value']!r}"
    if not changed["dirty"] and changed["storage"]["kind"] == "modSetting":
        menu.discard(setting_id)
        return "MCM does not have its ModSetting, so a change of it is not kept"
    assert changed["dirty"], "a change is not dirty"
    menu.save()
    saved = menu.setting(id=setting_id)
    assert not saved["dirty"], "it is dirty after a Save"
    check_file(bench, mod, saved["storage"], changed["value"])

    menu.set(setting_id, original)
    menu.save()
    check_file(bench, mod, saved["storage"], original)
    return None


@pytest.mark.parametrize("mod", SWEPT)
def test_sweep(menu, game, report, mod):
    listed = next(entry for entry in menu.mods() if entry["name"] == mod)
    problems, skipped, known, changed = [], [], [], 0
    for path in [[], *page_paths(listed["pages"])]:
        where = " / ".join([mod, *path])
        try:
            menu.select(mod, path)
        except (AssertionError, DevbenchError) as error:
            problems.append(f"{where}: not shown: {error}")
            continue
        page_rows = menu.rows()
        skipped += [f"{where}: hotkey {row['text']!r}: a hotkey is not set through devbench" for row in page_rows if row["kind"] == "hotkey"]
        for setting_id in [row["id"] for row in page_rows if row["kind"] == "setting"]:
            row = menu.setting(id=setting_id)
            name = f"{where}: {plain(row['text'])!r} ({setting_id})"
            if reason := why_skipped(row):
                skipped.append(f"{name}: {reason}")
                continue
            try:
                reason = sweep_setting(menu, game, mod, row)
            except (AssertionError, DevbenchError, TimeoutError) as error:
                failure = f"{name}: {str(error).splitlines()[0]}"
                if issue := KNOWN.get((mod, setting_id)):
                    known.append(f"{failure} (known: {issue})")
                else:
                    problems.append(failure)
                menu.discard()
                continue
            if reason:
                skipped.append(f"{name}: {reason}")
            else:
                changed += 1

    report.sweeps[mod] = {"changed": changed, "skipped": skipped, "failed": problems, "known": known}
    print(f"\n{mod}: {changed} settings changed and set back, {len(skipped)} skipped, {len(problems)} failed, {len(known)} known")
    for line in known:
        print(f"  known {line}")
    for line in skipped:
        print(f"  skipped {line}")
    assert not problems, f"{len(problems)} settings of {mod} failed:\n" + "\n".join(problems)
