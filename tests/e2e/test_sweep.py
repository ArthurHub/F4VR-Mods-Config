"""The sweep: each mod and page of the MCM test set, of the showcases and of the config specs is shown, and each setting
the player can change is changed, saved, checked in its file, then set back, saved and checked again, one at a time.

Each mod is a test of its own, which reports every setting that failed and the ones it skipped, with -rA. A run that
asks for the sweep, by a -m that names it or full or by naming this file, sweeps every setting of every mod; any other
run sweeps one setting of each kind (see kind) of each mod of SUBSET.
"""

import html
import re

import pytest

from conftest import CONFIG_SPEC_SHOWCASE, asked_for, CONFIG_SPECS, MCM_SHOWCASE, REQUIRED_MCM, TEST_SET
from devbench import DevbenchError, wait_until
from menu import page_paths, plain, same
from stored import NotInFile, file_value, inline_table, stored_as, table

SWEPT = [*TEST_SET, MCM_SHOWCASE, CONFIG_SPEC_SHOWCASE, *CONFIG_SPECS]

# the mods a run that does not ask for the sweep sweeps: the showcases, the config specs, and the required MCM mods of
# the test set, which cover what the rest of it repeats
SUBSET = [MCM_SHOWCASE, CONFIG_SPEC_SHOWCASE, *CONFIG_SPECS, *REQUIRED_MCM]

# what the id of a row of the Categories page starts with: the rows are one setting made for each mod, so one is swept
CATEGORY_SETTING_ID = "category."

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
    """The values other than its own to try of a binding, a color or a transform kept in an INI file, or a color kept in
    sliders, which is its line: a binding on the other hand, a color with another red, and a transform with another
    first number."""
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
    elif kind in ("cf.inputBinding", "cf.color", "cf.transform") and (row["storage"].get("format") == "ini" or row["storage"]["kind"] == "colorParts"):
        # a color kept in sliders is its r,g,b,a line too
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


def kind(row):
    """
    What a setting is to a sweep that changes one of each kind of a mod: its type, whether it has options, where its
    value is kept (the storage's kind, and a file's format and root), and whether its file leaves its default out, as
    a set back of one removes its key.
    """
    storage = row["storage"]
    return (row["type"], "options" in row, storage["kind"], storage.get("format"), storage.get("root"), row["omitDefault"])


def asked(config):
    """Whether the run asks for the whole sweep: every setting of every mod."""
    return asked_for(config, "sweep", "test_sweep")


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


def check_file(bench, mod, storage, value, omitted=False):
    """
    Fail unless the setting's file has the value, or has no value when it is omitted, as a Save leaves an omitDefault
    setting's default out; a setting kept in no file is not checked.
    """
    expected = "no value" if omitted else repr(value)
    try:
        # MCM writes its file from Papyrus, on its own time, and its first write of a run is the slowest
        if omitted:
            wait_until(lambda: file_value(bench, mod, storage) is None, 10, what="the file without the value")
        else:
            wait_until(lambda: stored_as(file_value(bench, mod, storage), value), 10, what="the file's value")
    except NotInFile:
        return
    except TimeoutError:
        raise AssertionError(f"its file has {file_value(bench, mod, storage)!r}, not {expected}") from None


def part_values(menu, row):
    """
    The values of the sliders a color is kept in, by their ids, and none for another setting. A slider may be on
    another page, which the setting action finds it on.
    """
    if row["storage"]["kind"] != "colorParts":
        return {}
    return {part_id: menu.call("setting", id=part_id)["setting"]["value"] for part_id in row["storage"]["parts"]}


def check_saved(menu, bench, mod, saved, value, omitted=False, parts=()):
    """
    Fail unless a saved setting's file has its value, as check_file does. A color kept in sliders is checked in the
    files of the given sliders instead, the ones its change set, at each slider's own value: a slider it did not set may
    be in no file, as MCM writes a ModSetting only once it is changed.
    """
    if saved["storage"]["kind"] != "colorParts":
        check_file(bench, mod, saved["storage"], value, omitted)
        return
    for part_id in parts:
        part = menu.call("setting", id=part_id)["setting"]
        try:
            check_file(bench, mod, part["storage"], part["value"], omitted=part["omitDefault"] and same(part["value"], part["default"]))
        except AssertionError as error:
            raise AssertionError(f"its slider {part_id}: {error}") from None


def sweep_setting(menu, bench, mod, row):
    """Change a setting, save it and check it in its file, then set it back the same way. Returns a reason it was not
    changed, or None."""
    setting_id, original = row["id"], row["value"]
    parts_before = part_values(menu, row)
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
    # the sliders of a color the change set, which its set back sets again
    parts = [part_id for part_id, value in part_values(menu, row).items() if not same(value, parts_before[part_id])]
    assert parts or not parts_before, "its change set none of its sliders"
    check_saved(menu, bench, mod, saved, changed["value"], parts=parts)

    menu.set(setting_id, original)
    menu.save()
    check_saved(menu, bench, mod, saved, original, omitted=row["omitDefault"] and same(original, row["default"]), parts=parts)
    return None


def pytest_generate_tests(metafunc):
    """
    Each mod a test of its own: every mod when the run asks for the whole sweep, and SUBSET otherwise. The parameter
    is swept, as mod is devbench's fixture of the mod's tool.
    """
    if "swept" not in metafunc.fixturenames:
        return
    metafunc.parametrize("swept", SWEPT if asked(metafunc.config) else [mod for mod in SWEPT if mod in SUBSET])


def _linked(text):
    """A line of text as HTML, with its web addresses as links, as a known failure names its issue."""
    return re.sub(r"https?://[^\s)<]+", lambda match: f"<a href='{match.group(0)}'>{match.group(0)}</a>", html.escape(text))


def report_sweep(report, mod, changed, alike, skipped, problems, known):
    """
    Add a mod's sweep to the report: its counts in the Sweep table, and the settings it failed, knew or skipped. alike
    counts the settings left out as one of their kind was swept.
    """
    report.section(
        "Sweep by mod",
        intro="Each setting changed, saved, checked in its file and set back; known failures are open issues. Alike: "
        "settings left out as one of their kind was swept, which -m sweep or -m full sweeps too.",
        columns=["Mod", "Changed", "Alike", "Skipped", "Failed", "Known"],
        rows=[[mod, changed, alike, len(skipped), len(problems), len(known)]],
    )
    lists = ""
    for kind, lines in (("failed", problems), ("known", known), ("skipped", skipped)):
        if lines:
            items = "".join(f"<li>{_linked(line)}</li>" for line in lines)
            lists += f"<details><summary>{len(lines)} {kind}</summary><ul class='list'>{items}</ul></details>"
    if lists:
        report.section("Sweep settings", html=f"<h3>{html.escape(mod)}</h3>{lists}")


def test_sweep(menu, game, report, test_set, swept, request):
    mod = swept
    if mod in TEST_SET and mod not in test_set:
        pytest.skip(f"{mod} is not installed or not available")
    listed = next(entry for entry in menu.mods() if entry["name"] == mod)
    whole = asked(request.config)
    problems, skipped, known, changed, alike = [], [], [], 0, 0
    # the kinds of setting swept, when the run sweeps one of each
    covered = set()
    category_swept = None
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
            if setting_id.startswith(CATEGORY_SETTING_ID):
                if category_swept:
                    skipped.append(f"{name}: a Categories row as {category_swept}, which is swept")
                    continue
                category_swept = setting_id
            if reason := why_skipped(row):
                skipped.append(f"{name}: {reason}")
                continue
            if not whole and kind(row) in covered:
                alike += 1
                continue
            try:
                reason = sweep_setting(menu, game, mod, row)
            except (AssertionError, DevbenchError, TimeoutError) as error:
                failure = f"{name}: {str(error).splitlines()[0]}"
                # a known failure leaves its kind to the next setting of it
                if issue := KNOWN.get((mod, setting_id)):
                    known.append(f"{failure} (known: {issue})")
                else:
                    problems.append(failure)
                    covered.add(kind(row))
                menu.discard()
                continue
            if reason:
                skipped.append(f"{name}: {reason}")
            else:
                changed += 1
                covered.add(kind(row))

    report_sweep(report, mod, changed, alike, skipped, problems, known)
    print(f"\n{mod}: {changed} settings changed and set back, {alike} alike left out, {len(skipped)} skipped, {len(problems)} failed, {len(known)} known")
    for line in known:
        print(f"  known {line}")
    for line in skipped:
        print(f"  skipped {line}")
    assert not problems, f"{len(problems)} settings of {mod} failed:\n" + "\n".join(problems)
