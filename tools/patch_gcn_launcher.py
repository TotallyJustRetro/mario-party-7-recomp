#!/usr/bin/env python3
"""Install Cruise Navigator startup and fatal-diagnostic hooks into gcn-recomp."""

from __future__ import annotations

import sys
from pathlib import Path

MAIN_MARKER = "// CRUISE_NAVIGATOR_LAUNCHER_HOOK"
CPU_MARKER = "// CRUISE_NAVIGATOR_CALL_TRACE_HOOK"


def replace_once(source: str, old: str, new: str, label: str) -> str:
    count = source.count(old)
    if count != 1:
        raise RuntimeError(f"Expected exactly one {label} anchor, found {count}.")
    return source.replace(old, new, 1)


def patch_main(path: Path) -> None:
    if not path.is_file():
        raise FileNotFoundError(f"gcn-recomp runtime entry point not found: {path}")

    source = path.read_text(encoding="utf-8")
    if MAIN_MARKER in source:
        print(f"Cruise Navigator startup hook already installed in {path}")
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
        "    " + MAIN_MARKER + "\n"
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


def patch_cpu(path: Path) -> None:
    if not path.is_file():
        raise FileNotFoundError(f"gcn-recomp CPU runtime not found: {path}")

    source = path.read_text(encoding="utf-8")
    if CPU_MARKER in source:
        print(f"Cruise Navigator call-trace hook already installed in {path}")
        return

    updated = replace_once(
        source,
        "#include <cmath>\n",
        "#include <cmath>\n\n"
        "// The fatal indirect-call path records guest thread stacks before aborting.\n"
        "void debug_dump_threads();\n",
        "CPU runtime include section",
    )
    updated = replace_once(
        updated,
        '    // GCN_RAM_DUMP=<path>: the guest RAM at this failure, for finding out what the code at\n'
        '    // address is and where it came from.\n'
        '    if (const char* path = getenv("GCN_RAM_DUMP")) {\n'
        '        if (FILE* f = fopen(path, "wb")) { fwrite(g_mem, 1, RAM_SIZE, f); fclose(f); }\n'
        '    }\n'
        '    fatal("call_indirect: no function at %08X (lr=%08X)", addr, c->lr);',
        '    // GCN_RAM_DUMP=<path>: preserve guest RAM for inspecting the unresolved target.\n'
        '    if (const char* path = getenv("GCN_RAM_DUMP")) {\n'
        '        if (FILE* f = fopen(path, "wb")) {\n'
        '            const size_t written = fwrite(g_mem, 1, RAM_SIZE, f);\n'
        '            fclose(f);\n'
        '            fprintf(stderr, "[diagnostics] RAM dump written: %s (%zu/%u bytes)\\n", path, written, (unsigned)RAM_SIZE);\n'
        '        } else {\n'
        '            fprintf(stderr, "[diagnostics] could not create RAM dump %s\\n", path);\n'
        '        }\n'
        '    } else {\n'
        '        fprintf(stderr, "[diagnostics] GCN_RAM_DUMP is not set; no RAM dump created\\n");\n'
        '    }\n'
        '    ' + CPU_MARKER + '\n'
        '    const uint32_t caller_pc = c->lr >= 4 ? c->lr - 4 : c->lr;\n'
        '    uint32_t caller_start = 0;\n'
        '    const char* caller_name = func_containing(caller_pc, &caller_start);\n'
        '    fprintf(stderr, "[diagnostics] indirect target=%08X return=%08X callsite=%08X caller=%s+%X\\n",\n'
        '            addr, c->lr, caller_pc, caller_name, caller_pc - caller_start);\n'
        '    debug_dump_threads();\n'
        '    fflush(stderr);\n'
        '    fatal("call_indirect: no function at %08X (lr=%08X)", addr, c->lr);',
        "indirect-call failure handler",
    )

    path.write_text(updated, encoding="utf-8", newline="\n")
    print(f"Installed Cruise Navigator call-trace hook in {path}")


def patch(runtime_main: Path) -> None:
    patch_main(runtime_main)
    patch_cpu(runtime_main.with_name("cpu.cpp"))


if __name__ == "__main__":
    try:
        if len(sys.argv) != 2:
            raise RuntimeError("Usage: patch_gcn_launcher.py path/to/gcn-recomp/runtime/main.cpp")
        patch(Path(sys.argv[1]))
    except Exception as exc:
        print(f"Cruise Navigator runtime patch failed: {exc}", file=sys.stderr)
        sys.exit(1)
