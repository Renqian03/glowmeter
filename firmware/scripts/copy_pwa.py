"""PlatformIO pre-build hook: copy host/pwa/ into firmware/data/.

Why: the firmware serves the web app from LittleFS, and PlatformIO builds the
LittleFS image from firmware/data/. We keep the single source of truth in
host/pwa/ (so the PWA can also be opened from a laptop while developing) and
copy it here instead of using a symlink, which git and Windows handle badly.
firmware/data/ is git-ignored.
"""
import shutil
from pathlib import Path

Import("env")  # noqa: F821  (provided by PlatformIO/SCons)

project_dir = Path(env["PROJECT_DIR"])  # noqa: F821
src = project_dir.parent / "host" / "pwa"
dst = project_dir / "data"

if src.is_dir():
    if dst.exists():
        shutil.rmtree(dst)
    shutil.copytree(src, dst)
    n = sum(1 for p in dst.rglob("*") if p.is_file())
    print(f"copy_pwa: copied {n} files from {src} -> {dst}")
else:
    print(f"copy_pwa: WARNING {src} not found, firmware/data/ left as is")
