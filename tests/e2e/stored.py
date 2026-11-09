"""A setting's value where its mod keeps it: in its file, read through devbench as the game sees it, and in the game,
read through the console."""

import re
import tomllib

from menu import same

NUMBER = re.compile(r">>\s*(-?\d+(?:\.\d+)?)")


class NotInFile(Exception):
    """A setting whose value is in no file a test reads: a global, a property, a state of the mod's DLL, or a file whose
    path the mod's DLL answers."""


def _ini_value(text, section, key):
    """The text of a key of an INI file, as MCM and the menu read it: the last one of its name, case ignored."""
    current, found = None, None
    for line in text.splitlines():
        line = line.strip()
        if line.startswith("[") and line.endswith("]"):
            current = line[1:-1].strip().lower()
        elif "=" in line and not line.startswith((";", "#")) and current == section.lower():
            name, value = line.split("=", 1)
            if name.strip().lower() == key.lower():
                found = value.strip()
    return found


def _toml_value(text, path):
    value = tomllib.loads(text)
    for name in path:
        if not isinstance(value, dict) or name not in value:
            return None
        value = value[name]
    return value


def file_value(bench, mod, storage):
    """The value of a setting in its file, as the file has it: a TOML value, or an INI key's text. None when the file
    or the key is not there."""
    if storage["kind"] == "modSetting":
        key, section = storage["name"].split(":", 1)
        text = bench.read_text("data", f"MCM/Settings/{mod}.ini")
        return text and _ini_value(text, section, key)
    if storage["kind"] != "file" or "root" not in storage:
        raise NotInFile(storage["kind"])
    text = bench.read_text(storage["root"], storage["filePath"])
    if text is None:
        return None
    if storage["format"] == "ini":
        return _ini_value(text, storage["path"][0], storage["path"][1])
    return _toml_value(text, storage["path"])


def table(value):
    """A binding's, a color's and a transform's value, the TOML text of a table, as the table."""
    return tomllib.loads(f"v = {value}")["v"]


def inline_table(value):
    """A table as the TOML text of an inline table, which is how a binding, a color and a transform are set."""
    if isinstance(value, bool):
        return "true" if value else "false"
    if isinstance(value, str):
        return '"' + value.replace("\\", "\\\\").replace('"', '\\"') + '"'
    if isinstance(value, list):
        return "[" + ", ".join(inline_table(item) for item in value) + "]"
    if isinstance(value, dict):
        return "{ " + ", ".join(f"{key} = {inline_table(item)}" for key, item in value.items()) + " }"
    return repr(value)


def stored_as(found, value):
    """Whether a value found in a file is a setting's value: an INI key's text read as the value's type, and a table's
    TOML text as its table."""
    if found is None:
        return False
    if isinstance(found, str) and isinstance(value, str) and found != value:
        # MCM writes a number text input's float with its own decimals
        try:
            return same(float(found), float(value))
        except ValueError:
            return False
    if isinstance(found, str) and not isinstance(value, str):
        if isinstance(value, bool):
            return found.lower() in ("1", "true") if value else found.lower() in ("0", "false")
        try:
            return same(float(found), value)
        except ValueError:
            return False
    if isinstance(found, dict) and isinstance(value, str):
        return same(found, table(value))
    if isinstance(found, (int, float)) and not isinstance(found, bool) and isinstance(value, str):
        # a number text input's value is its text
        try:
            return same(found, float(value))
        except ValueError:
            return False
    if isinstance(found, bool) or isinstance(value, bool):
        return found is value
    return same(found, value)


def console_number(bench, command):
    """The number a console command prints after its '>>', as GetGlobalValue and GetInCell print it."""
    for line in reversed(bench.console(command)):
        if match := NUMBER.search(line):
            return float(match.group(1))
    raise AssertionError(f"'{command}' printed no number")
