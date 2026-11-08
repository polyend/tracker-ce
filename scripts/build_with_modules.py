#!/usr/bin/env python3
"""Interactive Tracker CE build with feature-module selection.

Discovers modules under feature-modules/ (directories containing module.mk),
lets you toggle which ones to include, pick a firmware target, then runs make.

Examples:
  python3 scripts/build_with_modules.py
  make select
  python3 scripts/build_with_modules.py --modules performance-fx-scenes --target plus
  python3 scripts/build_with_modules.py --list
  python3 scripts/build_with_modules.py --catalog site/modules.json
"""

from __future__ import annotations

import argparse
import json
import os
import re
import subprocess
import sys
import termios
import tty
from dataclasses import dataclass
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
FEATURE_MODULES_DIR = ROOT / "feature-modules"
SELECTION_FILE = ROOT / ".feature-modules-selection"
MK_ASSIGN = re.compile(r"^\s*([A-Z_][A-Z0-9_]*)\s*:?=\s*(.*?)\s*$")

TARGETS = ("all", "original", "plus", "mini")


@dataclass
class ModuleInfo:
    name: str
    path: Path
    title: str
    description: str
    order: int = 100


def parse_module_mk(path: Path) -> dict[str, str]:
    values: dict[str, str] = {}
    for line in path.read_text(encoding="utf-8").splitlines():
        stripped = line.split("#", 1)[0].strip()
        if not stripped:
            continue
        match = MK_ASSIGN.match(stripped)
        if match:
            values[match.group(1)] = match.group(2).strip()
    return values


def discover_modules(modules_dir: Path) -> list[ModuleInfo]:
    modules: list[ModuleInfo] = []
    if not modules_dir.is_dir():
        return modules

    for child in sorted(modules_dir.iterdir()):
        if not child.is_dir():
            continue
        module_mk = child / "module.mk"
        if not module_mk.is_file():
            continue
        values = parse_module_mk(module_mk)
        name = values.get("MODULE_NAME") or child.name
        title = values.get("MODULE_TITLE") or name.replace("-", " ").title()
        description = values.get("MODULE_DESCRIPTION") or ""
        try:
            order = int(values.get("MODULE_ORDER", "100"))
        except ValueError:
            order = 100
        modules.append(
            ModuleInfo(
                name=name,
                path=child.resolve(),
                title=title,
                description=description,
                order=order,
            )
        )
    modules.sort(key=lambda m: (m.order, m.name))
    return modules


def load_previous_selection(modules: list[ModuleInfo]) -> set[str]:
    if not SELECTION_FILE.is_file():
        return {m.name for m in modules}
    try:
        names = {
            line.strip()
            for line in SELECTION_FILE.read_text(encoding="utf-8").splitlines()
            if line.strip() and not line.strip().startswith("#")
        }
    except OSError:
        return {m.name for m in modules}
    known = {m.name for m in modules}
    return names & known


def save_selection(selected: set[str]) -> None:
    lines = ["# Last feature-module selection from scripts/build_with_modules.py", *sorted(selected)]
    SELECTION_FILE.write_text("\n".join(lines) + "\n", encoding="utf-8")


def read_key() -> str:
    fd = sys.stdin.fileno()
    old = termios.tcgetattr(fd)
    try:
        tty.setraw(fd)
        ch = sys.stdin.read(1)
        if ch == "\x1b":
            # Arrow keys: ESC [ A/B/C/D
            rest = sys.stdin.read(2)
            mapping = {"[A": "up", "[B": "down", "[C": "right", "[D": "left"}
            return mapping.get(rest, "esc")
        if ch in ("\r", "\n"):
            return "enter"
        if ch == " ":
            return "space"
        if ch == "\x03":
            return "ctrl-c"
        return ch
    finally:
        termios.tcsetattr(fd, termios.TCSADRAIN, old)


def clear_screen() -> None:
    sys.stdout.write("\033[2J\033[H")
    sys.stdout.flush()


def render(
    modules: list[ModuleInfo],
    selected: set[str],
    target_idx: int,
    cursor: int,
) -> None:
    clear_screen()
    print("Tracker CE — Feature Module Build")
    print("=" * 40)
    print()
    print("Modules  (space = toggle, a = all, n = none)")
    print("-" * 40)
    if not modules:
        print("  (no modules found in feature-modules/)")
    for i, module in enumerate(modules):
        mark = "x" if module.name in selected else " "
        pointer = ">" if i == cursor else " "
        line = f" {pointer} [{mark}] {module.title}"
        if module.description:
            line += f"  — {module.description}"
        print(line)
        print(f"      {module.name}/")
    print()
    print("Firmware target  (left/right or t = cycle)")
    print("-" * 40)
    parts = []
    for i, name in enumerate(TARGETS):
        if i == target_idx:
            parts.append(f"[{name}]")
        else:
            parts.append(f" {name} ")
    print("  " + "  ".join(parts))
    print()
    print("enter/b = build   q = quit")
    print()
    enabled = sorted(selected) if selected else ["(none)"]
    print(f"Selected: {', '.join(enabled)}")
    print(f"Will run: make {TARGETS[target_idx]} FEATURE_MODULE_DIRS=...")


def interactive_select(modules: list[ModuleInfo], selected: set[str], target: str) -> tuple[set[str], str] | None:
    if not sys.stdin.isatty() or not sys.stdout.isatty():
        print("error: interactive mode needs a TTY; use --modules / --target", file=sys.stderr)
        return None

    target_idx = TARGETS.index(target) if target in TARGETS else 0
    cursor = 0

    while True:
        render(modules, selected, target_idx, cursor)
        key = read_key()

        if key in ("q", "Q", "esc", "ctrl-c"):
            clear_screen()
            print("Cancelled.")
            return None
        if key in ("enter", "b", "B"):
            clear_screen()
            return selected, TARGETS[target_idx]
        if key == "space" and modules:
            name = modules[cursor].name
            if name in selected:
                selected.remove(name)
            else:
                selected.add(name)
        elif key == "a":
            selected = {m.name for m in modules}
        elif key == "n":
            selected = set()
        elif key in ("up", "k") and modules:
            cursor = (cursor - 1) % len(modules)
        elif key in ("down", "j") and modules:
            cursor = (cursor + 1) % len(modules)
        elif key in ("left", "h", "t", "T"):
            target_idx = (target_idx - 1) % len(TARGETS) if key in ("left", "h") else (target_idx + 1) % len(TARGETS)
        elif key in ("right", "l"):
            target_idx = (target_idx + 1) % len(TARGETS)
        elif key.isdigit() and modules:
            idx = int(key) - 1
            if 0 <= idx < len(modules):
                cursor = idx
                name = modules[idx].name
                if name in selected:
                    selected.remove(name)
                else:
                    selected.add(name)


def build(target: str, module_dirs: list[Path]) -> int:
    env = os.environ.copy()
    dirs_value = " ".join(str(p) for p in module_dirs)
    cmd = ["make", "-C", str(ROOT), target, f"FEATURE_MODULE_DIRS={dirs_value}"]
    print("==================================================================")
    print(f" Target : make {target}")
    if module_dirs:
        print(" Modules:")
        for path in module_dirs:
            print(f"   - {path}")
    else:
        print(" Modules: (none)")
    print("==================================================================")
    print()
    return subprocess.call(cmd, env=env)


def list_modules(modules: list[ModuleInfo]) -> int:
    if not modules:
        print("No feature modules found.")
        return 0
    for module in modules:
        desc = f" — {module.description}" if module.description else ""
        print(f"{module.name}\t{module.title}{desc}")
        print(f"\t{module.path}")
    return 0


def catalog_payload(modules: list[ModuleInfo]) -> dict:
    return {
        "targets": [
            {"id": "original", "title": "Original Tracker"},
            {"id": "plus", "title": "Tracker+"},
            {"id": "mini", "title": "Tracker Mini"},
        ],
        "modules": [
            {
                "name": module.name,
                "title": module.title,
                "description": module.description,
                "order": module.order,
            }
            for module in modules
        ],
    }


def write_catalog(modules: list[ModuleInfo], path: Path) -> int:
    path.parent.mkdir(parents=True, exist_ok=True)
    payload = catalog_payload(modules)
    path.write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")
    print(f"Wrote {path} ({len(modules)} module(s))")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description="Build Tracker CE with selected feature modules.")
    parser.add_argument(
        "--modules",
        nargs="+",
        metavar="NAME",
        help="Non-interactive: module directory names under feature-modules/",
    )
    parser.add_argument(
        "--none",
        action="store_true",
        help="Non-interactive: build with no feature modules",
    )
    parser.add_argument(
        "--target",
        choices=TARGETS,
        default="all",
        help="Firmware make target (default: all)",
    )
    parser.add_argument("--list", action="store_true", help="List discoverable modules and exit")
    parser.add_argument(
        "--catalog",
        type=Path,
        metavar="PATH",
        help="Write feature-module catalog JSON for the web picker and exit",
    )
    parser.add_argument(
        "--modules-dir",
        type=Path,
        default=FEATURE_MODULES_DIR,
        help="Directory to scan for modules (default: feature-modules/)",
    )
    parser.add_argument(
        "--no-save",
        action="store_true",
        help="Do not write .feature-modules-selection",
    )
    args = parser.parse_args()

    if args.modules and args.none:
        print("error: use either --modules or --none, not both", file=sys.stderr)
        return 1

    modules_dir = args.modules_dir if args.modules_dir.is_absolute() else (ROOT / args.modules_dir)
    modules = discover_modules(modules_dir)

    if args.list:
        return list_modules(modules)

    if args.catalog is not None:
        catalog_path = args.catalog if args.catalog.is_absolute() else (ROOT / args.catalog)
        return write_catalog(modules, catalog_path)

    by_name = {m.name: m for m in modules}

    if args.none:
        selected_names: set[str] = set()
        target = args.target
    elif args.modules is not None:
        unknown = [name for name in args.modules if name not in by_name]
        if unknown:
            print(f"error: unknown module(s): {', '.join(unknown)}", file=sys.stderr)
            print("Available:", ", ".join(by_name) or "(none)", file=sys.stderr)
            return 1
        selected_names = set(args.modules)
        target = args.target
    else:
        selected_names = load_previous_selection(modules)
        result = interactive_select(modules, set(selected_names), args.target)
        if result is None:
            return 1
        selected_names, target = result

    if not args.no_save:
        save_selection(selected_names)

    # Preserve MODULE_ORDER for Performance page cycling.
    ordered = [m for m in modules if m.name in selected_names]
    module_dirs = [m.path for m in ordered]
    return build(target, module_dirs)


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except KeyboardInterrupt:
        print("\nCancelled.")
        raise SystemExit(130)
