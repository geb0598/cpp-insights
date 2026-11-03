"""
cpp-insights release builder
Usage: python make_release.py
"""

import os
import shutil
import subprocess
import sys
import zipfile
from pathlib import Path

# ── Paths ─────────────────────────────────────────────────────────────────────
SCRIPT_DIR   = Path(__file__).parent.resolve()     # <repo>/scripts
PROJECT_DIR  = SCRIPT_DIR.parent                   # <repo> — CMakePresets.json lives here
RELEASES_DIR = PROJECT_DIR.parent / "releases"     # kept outside the repo (build output)

# ── Build output dirs (Visual Studio multi-config generator) ──────────────────
DEBUG_OUT   = PROJECT_DIR / "build" / "debug"   / "Debug"
RELEASE_OUT = PROJECT_DIR / "build" / "release" / "Release"
VIEWER_DEBUG_OUT   = PROJECT_DIR / "build" / "debug"   / "viewer" / "Debug"
VIEWER_RELEASE_OUT = PROJECT_DIR / "build" / "release" / "viewer" / "Release"

# ── Source assets ─────────────────────────────────────────────────────────────
INCLUDE_SRC = PROJECT_DIR / "include" / "insights"
FONT_SRC    = PROJECT_DIR / "viewer" / "data" / "JetBrainsMonoNerdFont-Regular.ttf"
IMGUI_INI   = PROJECT_DIR / "imgui.ini"


def run(cmd: list[str], cwd: Path) -> None:
    """Run a subprocess, streaming output. Raises on failure."""
    print(f"\n▶ {' '.join(str(c) for c in cmd)}")
    result = subprocess.run(cmd, cwd=cwd)
    if result.returncode != 0:
        print(f"\n✗ Command failed with exit code {result.returncode}")
        sys.exit(result.returncode)


def ask_version() -> str:
    default = "v0.0.0"
    raw = input(f"\nRelease version [{default}]: ").strip()
    return raw if raw else default


def clear_cmake_cache(build_dir: Path) -> None:
    """Delete all CMakeCache.txt files under build_dir (including _deps subdirs)."""
    if not build_dir.exists():
        return
    for cache in build_dir.rglob("CMakeCache.txt"):
        print(f"  Removing stale cache: {cache}")
        cache.unlink()


def build_debug(project_dir: Path) -> None:
    print("\n── Debug build ──────────────────────────────────────────────────────")
    clear_cmake_cache(project_dir / "build" / "debug")
    run(["cmake", "--preset", "debug"], cwd=project_dir)
    run(["cmake", "--build", "build/debug", "--config", "Debug"], cwd=project_dir)


def run_tests(project_dir: Path) -> None:
    print("\n── Tests ────────────────────────────────────────────────────────────")
    build_debug_dir = project_dir / "build" / "debug"
    run(["ctest", "--output-on-failure", "-C", "Debug"], cwd=build_debug_dir)


def build_release(project_dir: Path) -> None:
    print("\n── Release build ────────────────────────────────────────────────────")
    clear_cmake_cache(project_dir / "build" / "release")
    run(["cmake", "--preset", "release"], cwd=project_dir)
    run(["cmake", "--build", "build/release", "--config", "Release"], cwd=project_dir)


def copy_sdk(version: str, release_root: Path) -> Path:
    """Assemble the SDK package and return its path."""
    sdk_dir = release_root / f"cpp-insights-{version}-sdk"

    # include/insights/
    dst_include = sdk_dir / "include" / "insights"
    print(f"\n  Copying headers → {dst_include.relative_to(release_root)}")
    shutil.copytree(INCLUDE_SRC, dst_include, dirs_exist_ok=True)

    # lib/Debug/
    dst_lib_debug = sdk_dir / "lib" / "Debug"
    dst_lib_debug.mkdir(parents=True, exist_ok=True)
    debug_files = [
        "cpp-insights-core.lib",
        "cpp-insights-core.pdb",
        "cpp-insights-gpu.lib",
        "cpp-insights-gpu.pdb",
        "insights-test.pdb",
    ]
    print(f"  Copying debug libs → {dst_lib_debug.relative_to(release_root)}")
    for fname in debug_files:
        src = DEBUG_OUT / fname
        if src.exists():
            shutil.copy2(src, dst_lib_debug / fname)
        else:
            print(f"    ⚠  {fname} not found, skipping")

    # lib/Release/
    dst_lib_release = sdk_dir / "lib" / "Release"
    dst_lib_release.mkdir(parents=True, exist_ok=True)
    release_files = [
        "cpp-insights-core.lib",
        "cpp-insights-gpu.lib",
    ]
    print(f"  Copying release libs → {dst_lib_release.relative_to(release_root)}")
    for fname in release_files:
        src = RELEASE_OUT / fname
        if src.exists():
            shutil.copy2(src, dst_lib_release / fname)
        else:
            print(f"    ⚠  {fname} not found, skipping")

    return sdk_dir


def copy_viewer(version: str, release_root: Path) -> Path:
    """Assemble the viewer package and return its path."""
    viewer_dir = release_root / f"cpp-insights-{version}-viewer"
    viewer_dir.mkdir(parents=True, exist_ok=True)

    viewer_files = {
        VIEWER_RELEASE_OUT / "cpp-insights-viewer.exe": "cpp-insights-viewer.exe",
        VIEWER_RELEASE_OUT / "imgui.lib":               "imgui.lib",
        VIEWER_RELEASE_OUT / "implot.lib":              "implot.lib",
        IMGUI_INI:                                      "imgui.ini",
        FONT_SRC:                                       "JetBrainsMonoNerdFont-Regular.ttf",
    }

    print(f"\n  Copying viewer files → {viewer_dir.relative_to(release_root)}")
    for src, dst_name in viewer_files.items():
        if src.exists():
            shutil.copy2(src, viewer_dir / dst_name)
        else:
            print(f"    ⚠  {src.name} not found, skipping")

    return viewer_dir


def zip_dir(source_dir: Path) -> Path:
    """Zip a directory next to itself and return the zip path."""
    zip_path = source_dir.parent / (source_dir.name + ".zip")
    with zipfile.ZipFile(zip_path, "w", zipfile.ZIP_DEFLATED) as zf:
        for file in source_dir.rglob("*"):
            if file.is_file():
                zf.write(file, file.relative_to(source_dir.parent))
    return zip_path


def main() -> None:
    version = ask_version()
    release_root = RELEASES_DIR / version

    if release_root.exists():
        overwrite = input(f"\n'{release_root}' already exists. Overwrite? [y/N]: ").strip().lower()
        if overwrite != "y":
            print("Aborted.")
            sys.exit(0)
        shutil.rmtree(release_root)

    release_root.mkdir(parents=True)

    # ── Build ──────────────────────────────────────────────────────────────────
    build_debug(PROJECT_DIR)
    run_tests(PROJECT_DIR)
    build_release(PROJECT_DIR)

    # ── Package ───────────────────────────────────────────────────────────────
    print("\n── Packaging ────────────────────────────────────────────────────────")

    sdk_dir    = copy_sdk(version, release_root)
    viewer_dir = copy_viewer(version, release_root)

    print("\n── Zipping ──────────────────────────────────────────────────────────")
    sdk_zip    = zip_dir(sdk_dir)
    viewer_zip = zip_dir(viewer_dir)

    # ── Summary ───────────────────────────────────────────────────────────────
    base = RELEASES_DIR.parent
    print(f"""
── Done ─────────────────────────────────────────────────────────────
  {sdk_dir.relative_to(base)}
  {sdk_zip.relative_to(base)}
  {viewer_dir.relative_to(base)}
  {viewer_zip.relative_to(base)}
─────────────────────────────────────────────────────────────────────
""")


if __name__ == "__main__":
    main()
