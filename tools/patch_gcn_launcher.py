#!/usr/bin/env python3
"""Idempotently connect Cruise Navigator to gcn-recomp's desktop runtime entry point."""

from __future__ import annotations

import sys
from pathlib import Path

MARKER = "// CRUISE_NAVIGATOR_LAUNCHER_HOOK"


def replace_once(source: str, old: str, new: str, label: str) -> str:
    count = source.count(old)
    if count != 1:
        raise RuntimeError(f"Expected exactly one {label} anchor, found {count}.")
    return source.replace(old, new, 1)


def patch(path: Path) -> None:
    if not path.is_file():
        raise FileNotFoundError(f"gcn-recomp runtime entry point not found: {path}")

    source = path.read_text(encoding="utf-8")
    if MARKER in source:
        print(f"Cruise Navigator hook already installed in {path}")
        return

    updated = source
    updated = replace_once(
        updated,
        "uint32_t boot_load(const char* iso_path);",
        'extern "C" bool launcher_run(const char* default_path, char* selected_path, int selected_path_size);\n'
        "uint32_t boot_load(const char* iso_path);",
        "launcher declaration",
    )
    updated = replace_once(
        updated,
        "    const char* iso = iso_default.c_str();",
        "    const char* iso = iso_default.c_str();\n"
        "    bool iso_from_command_line = false;",
        "default ISO selection",
    )
    updated = replace_once(
        updated,
        "        else if (argv[i][0] != '-') iso = argv[i];",
        "        else if (argv[i][0] != '-') { iso = argv[i]; iso_from_command_line = true; }",
        "command-line ISO selection",
    )
    updated = replace_once(
        updated,
        "    if (!plat_readable(iso)) {",
        "    " + MARKER + "\n"
        "    // Explicit paths and headless runs bypass the interactive launcher.\n"
        "    if (!headless && !iso_from_command_line) {\n"
        "        char selected_iso[4096] = {};\n"
        "        if (!launcher_run(iso, selected_iso, (int)sizeof selected_iso)) return 0;\n"
        "        iso_default.assign(selected_iso);\n"
        "        iso = iso_default.c_str();\n"
        "    }\n\n"
        "    if (!plat_readable(iso)) {",
        "launcher invocation",
    )

    path.write_text(updated, encoding="utf-8", newline="\n")
    print(f"Installed Cruise Navigator startup hook in {path}")


if __name__ == "__main__":
    try:
        if len(sys.argv) != 2:
            raise RuntimeError("Usage: patch_gcn_launcher.py path/to/gcn-recomp/runtime/main.cpp")
        patch(Path(sys.argv[1]))
    except Exception as exc:
        print(f"launcher runtime patch failed: {exc}", file=sys.stderr)
        sys.exit(1)
