"""The backup of the files a Save writes, taken before a run changes anything and put back after it.

It is read and written through devbench's files tool, so it sees the game's Data folder as the game does: under MO2
a file the game edits is in its mod's folder, and one it creates is in overwrite. The manifest is on disk, written
in full before the run starts, so a run that a crash ended is put back by the next run, or by --e2e-restore.
"""

import base64
import json
import os
from datetime import datetime
from pathlib import Path

# every file the tests can make a Save write: MCM's ModSettings and hotkeys, the config specs' files in Data, and the
# mods' files in My Games
GLOBS = [
    ("data", "MCM/Settings/*"),
    ("data", "ModsConfigFO4/Config/*/*"),
    ("mygames", "Mods_Config/**"),
    ("mygames", "FRIK_Config/*"),
]

BACKUP_DIR = Path(__file__).resolve().parent / ".backup"
MANIFEST = BACKUP_DIR / "manifest.json"


def pending():
    """Whether a backup is waiting to be put back: a run did not end, or is running."""
    return MANIFEST.is_file()


def _key(root, path):
    # Windows ignores case, and so does the game
    return root, path.replace("\\", "/").lower()


def take(bench):
    """Read every file of the globs and write them to the manifest, which is in place only once it is whole."""
    files = []
    for root, glob in GLOBS:
        for listed in bench.list_files(root, glob):
            data = bench.read_bytes(root, listed["path"])
            if data is not None:
                files.append({"root": root, "path": listed["path"], "content": base64.b64encode(data).decode("ascii")})
    BACKUP_DIR.mkdir(exist_ok=True)
    partial = MANIFEST.with_suffix(".partial")
    partial.write_text(json.dumps({"taken": datetime.now().isoformat(timespec="seconds"), "globs": GLOBS, "files": files}), encoding="utf-8")
    os.replace(partial, MANIFEST)
    return len(files)


def restore(bench):
    """Put the manifest's files back: write each one that differs, delete each file of its globs that it has not,
    and remove the manifest once all of it is done. Returns the paths it wrote and deleted."""
    manifest = json.loads(MANIFEST.read_text(encoding="utf-8"))
    kept = {_key(entry["root"], entry["path"]): entry for entry in manifest["files"]}
    written, deleted = [], []
    for entry in manifest["files"]:
        data = base64.b64decode(entry["content"])
        if bench.read_bytes(entry["root"], entry["path"]) != data:
            bench.write_bytes(entry["root"], entry["path"], data)
            written.append(f"{entry['root']}/{entry['path']}")
    for root, glob in manifest["globs"]:
        for listed in bench.list_files(root, glob):
            if _key(root, listed["path"]) not in kept:
                bench.delete_file(root, listed["path"])
                deleted.append(f"{root}/{listed['path']}")
    MANIFEST.unlink()
    return written, deleted
