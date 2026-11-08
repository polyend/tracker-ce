#!/usr/bin/env python3
"""Parse and validate custom firmware build requests from GitHub issues.

Used by .github/workflows/custom-firmware.yml. Never passes issue text to a shell.

Examples:
  python3 scripts/custom_firmware_request.py parse --body-file body.txt --out request.json
  python3 scripts/custom_firmware_request.py tag-name --target mini --modules a b
  python3 scripts/custom_firmware_request.py asset-name --target mini --sha abcdef1
"""

from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))

from build_with_modules import FEATURE_MODULES_DIR, discover_modules  # noqa: E402

DEVICE_TARGETS = ("original", "plus", "mini")
MODULE_NAME_RE = re.compile(r"^[a-z0-9]+(?:-[a-z0-9]+)*$")
SECTION_RE = re.compile(
    r"^###\s+(?P<title>.+?)\s*\n(?P<body>.*?)(?=\n###\s+|\Z)",
    re.MULTILINE | re.DOTALL,
)

ASSET_PREFIX = {
    "original": "PolyendTracker",
    "plus": "TrackerPlus",
    "mini": "TrackerMini",
}


def _normalize_section_title(title: str) -> str:
    return re.sub(r"\s+", " ", title.strip().lower())


def parse_issue_body(body: str) -> dict[str, str]:
    sections: dict[str, str] = {}
    for match in SECTION_RE.finditer(body or ""):
        key = _normalize_section_title(match.group("title"))
        value = match.group("body").strip()
        if value.lower() in ("_no response_", "none", "n/a"):
            value = ""
        sections[key] = value
    return sections


def extract_target(sections: dict[str, str]) -> str | None:
    for key in ("target device", "target", "device"):
        if key in sections:
            raw = sections[key].strip().lower()
            # Dropdown may include a label like "Tracker Mini (mini)"
            for target in DEVICE_TARGETS:
                if raw == target or raw.endswith(f"({target})") or raw.endswith(f"`{target}`"):
                    return target
            tokens = re.findall(r"[a-z+]+", raw)
            for token in tokens:
                if token in DEVICE_TARGETS:
                    return token
            if "original" in raw:
                return "original"
            if "mini" in raw:
                return "mini"
            if "plus" in raw or "tracker+" in raw:
                return "plus"
    return None


def extract_modules(sections: dict[str, str]) -> list[str]:
    raw = ""
    for key in ("feature modules", "modules", "module list"):
        if key in sections:
            raw = sections[key]
            break
    if not raw:
        return []
    # Accept commas, whitespace, and markdown list bullets.
    cleaned = raw.replace(",", " ").replace("`", " ")
    tokens: list[str] = []
    seen: set[str] = set()
    for token in cleaned.split():
        name = token.strip().lstrip("-*").strip()
        if not name or name in seen:
            continue
        if not MODULE_NAME_RE.fullmatch(name):
            raise ValueError(f"invalid module name: {name!r}")
        seen.add(name)
        tokens.append(name)
    return tokens


def validate_request(target: str | None, modules: list[str], modules_dir: Path) -> dict:
    known = {m.name: m for m in discover_modules(modules_dir)}
    errors: list[str] = []

    if target not in DEVICE_TARGETS:
        errors.append(
            f"target must be one of: {', '.join(DEVICE_TARGETS)} (got {target!r})"
        )

    unknown = [name for name in modules if name not in known]
    if unknown:
        errors.append(
            "unknown module(s): "
            + ", ".join(unknown)
            + ". Available: "
            + (", ".join(sorted(known)) or "(none)")
        )

    # Preserve MODULE_ORDER for Performance page cycling; slug is alphabetical for stable tags.
    ordered = [m.name for m in known.values() if m.name in set(modules)]
    slug_parts = sorted(ordered)
    return {
        "ok": not errors,
        "errors": errors,
        "target": target if target in DEVICE_TARGETS else None,
        "modules": ordered,
        "modules_slug": "--".join(slug_parts) if slug_parts else "none",
    }


def make_tag_name(target: str, modules_slug: str, sha: str) -> str:
    # Git ref-safe: lowercase, no spaces. Keep under typical tag length limits.
    short_sha = sha[:7]
    slug = modules_slug.replace("/", "-")
    return f"custom-{target}-{slug}-{short_sha}"


def make_asset_name(target: str, sha: str) -> str:
    # Original Tracker bootloader requires filenames <= 31 characters.
    prefix = ASSET_PREFIX[target]
    short_sha = sha[:7]
    name = f"{prefix}_c_{short_sha}.ptf"
    if len(name) > 31:
        name = f"{prefix[:12]}_c_{short_sha}.ptf"
    return name


def cmd_parse(args: argparse.Namespace) -> int:
    body = Path(args.body_file).read_text(encoding="utf-8") if args.body_file else sys.stdin.read()
    sections = parse_issue_body(body)
    try:
        modules = extract_modules(sections)
    except ValueError as exc:
        result = {
            "ok": False,
            "errors": [str(exc)],
            "target": None,
            "modules": [],
            "modules_slug": "none",
        }
    else:
        target = extract_target(sections)
        modules_dir = Path(args.modules_dir)
        if not modules_dir.is_absolute():
            modules_dir = ROOT / modules_dir
        result = validate_request(target, modules, modules_dir)

    text = json.dumps(result, indent=2) + "\n"
    if args.out:
        Path(args.out).write_text(text, encoding="utf-8")
    else:
        sys.stdout.write(text)
    return 0 if result["ok"] else 1


def cmd_tag_name(args: argparse.Namespace) -> int:
    modules = args.modules or []
    modules_dir = FEATURE_MODULES_DIR
    result = validate_request(args.target, modules, modules_dir)
    if not result["ok"]:
        print("; ".join(result["errors"]), file=sys.stderr)
        return 1
    print(make_tag_name(result["target"], result["modules_slug"], args.sha))
    return 0


def cmd_asset_name(args: argparse.Namespace) -> int:
    if args.target not in DEVICE_TARGETS:
        print(f"error: invalid target {args.target!r}", file=sys.stderr)
        return 1
    print(make_asset_name(args.target, args.sha))
    return 0


def cmd_plan(args: argparse.Namespace) -> int:
    request = json.loads(Path(args.request_file).read_text(encoding="utf-8"))
    if not request.get("ok"):
        print("; ".join(request.get("errors") or ["invalid request"]), file=sys.stderr)
        return 1
    target = request["target"]
    modules_slug = request["modules_slug"]
    modules = request.get("modules") or []
    sha = args.sha
    plan = {
        "target": target,
        "modules": modules,
        "modules_slug": modules_slug,
        "sha": sha,
        "short_sha": sha[:7],
        "tag": make_tag_name(target, modules_slug, sha),
        "asset": make_asset_name(target, sha),
    }
    text = json.dumps(plan, indent=2) + "\n"
    if args.out:
        Path(args.out).write_text(text, encoding="utf-8")
    else:
        sys.stdout.write(text)
    return 0


def cmd_from_args(args: argparse.Namespace) -> int:
    modules = args.modules or []
    modules_dir = Path(args.modules_dir)
    if not modules_dir.is_absolute():
        modules_dir = ROOT / modules_dir
    result = validate_request(args.target, modules, modules_dir)
    text = json.dumps(result, indent=2) + "\n"
    if args.out:
        Path(args.out).write_text(text, encoding="utf-8")
    else:
        sys.stdout.write(text)
    return 0 if result["ok"] else 1


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="command", required=True)

    parse_p = sub.add_parser("parse", help="Parse a GitHub issue body into a JSON request")
    parse_p.add_argument("--body-file", help="Issue body file (default: stdin)")
    parse_p.add_argument("--out", help="Write JSON to this path")
    parse_p.add_argument(
        "--modules-dir",
        default=str(FEATURE_MODULES_DIR),
        help="Directory to scan for modules",
    )
    parse_p.set_defaults(func=cmd_parse)

    from_args_p = sub.add_parser("from-args", help="Validate target/modules CLI inputs into JSON")
    from_args_p.add_argument("--target", required=True, choices=DEVICE_TARGETS)
    from_args_p.add_argument("--modules", nargs="*", default=[])
    from_args_p.add_argument("--out", help="Write JSON to this path")
    from_args_p.add_argument(
        "--modules-dir",
        default=str(FEATURE_MODULES_DIR),
        help="Directory to scan for modules",
    )
    from_args_p.set_defaults(func=cmd_from_args)

    plan_p = sub.add_parser("plan", help="Expand a validated request JSON into build/tag metadata")
    plan_p.add_argument("--request-file", required=True)
    plan_p.add_argument("--sha", required=True)
    plan_p.add_argument("--out", help="Write JSON to this path")
    plan_p.set_defaults(func=cmd_plan)

    tag_p = sub.add_parser("tag-name", help="Print the prerelease tag name")
    tag_p.add_argument("--target", required=True, choices=DEVICE_TARGETS)
    tag_p.add_argument("--modules", nargs="*", default=[])
    tag_p.add_argument("--sha", required=True)
    tag_p.set_defaults(func=cmd_tag_name)

    asset_p = sub.add_parser("asset-name", help="Print a <=31-char .ptf asset name")
    asset_p.add_argument("--target", required=True, choices=DEVICE_TARGETS)
    asset_p.add_argument("--sha", required=True)
    asset_p.set_defaults(func=cmd_asset_name)

    args = parser.parse_args()
    return args.func(args)


if __name__ == "__main__":
    raise SystemExit(main())
