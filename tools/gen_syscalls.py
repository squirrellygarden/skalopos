#!/usr/bin/env python3
"""
gen_syscalls.py — emit syscall dispatch table and libc wrappers from schemas/.

Reads:
  schemas/syscalls.toml
  schemas/status.toml

Writes:
  kernel/syscall/dispatch_generated.c
  kernel/syscall/numbers_generated.h
  userland/libc/src/sys/syscall_generated.c
  userland/libc/include/sys/syscall.h
  userland/libc/include/sys/status.h
  userland/libc/src/sys/status_strings.c

This is a stub. The full implementation is straightforward:
  - parse the TOML
  - validate (unique numbers, known types, etc.)
  - format each output via simple string templates
Each output file has a "DO NOT EDIT — generated from schemas/" header.

Invocation:
  python3 tools/gen_syscalls.py              # regenerate everything
  python3 tools/gen_syscalls.py --check      # exit non-zero if outputs are stale
"""

import sys
from pathlib import Path

try:
    import tomllib  # Python 3.11+
except ImportError:
    import tomli as tomllib  # pip install tomli on older Pythons

REPO = Path(__file__).resolve().parent.parent


def load_schemas():
    with open(REPO / "schemas" / "syscalls.toml", "rb") as f:
        syscalls = tomllib.load(f)["syscall"]
    with open(REPO / "schemas" / "status.toml", "rb") as f:
        statuses = tomllib.load(f)["status"]
    return syscalls, statuses


def validate(syscalls, statuses):
    seen_nums = set()
    seen_names = set()
    for s in syscalls:
        if s["number"] in seen_nums:
            raise SystemExit(f"duplicate syscall number {s['number']}")
        if s["name"] in seen_names:
            raise SystemExit(f"duplicate syscall name {s['name']}")
        seen_nums.add(s["number"])
        seen_names.add(s["name"])

    seen_vals = set()
    seen_status_names = set()
    for st in statuses:
        if st["value"] in seen_vals:
            raise SystemExit(f"duplicate status value 0x{st['value']:08x}")
        if st["name"] in seen_status_names:
            raise SystemExit(f"duplicate status name {st['name']}")
        seen_vals.add(st["value"])
        seen_status_names.add(st["name"])


# Type mapping: schema type → (C declaration for libc wrapper, kernel-side type)
TYPE_MAP = {
    "handle":              ("handle_t",            "handle_t"),
    "handle_ptr":          ("handle_t*",           "handle_t*"),
    "const_handle_ptr":    ("const handle_t*",     "const handle_t*"),
    "status":              ("status_t",            "status_t"),
    "void_ptr":            ("void*",               "void*"),
    "const_void_ptr":      ("const void*",         "const void*"),
    "void_ptr_ptr":        ("void**",              "void**"),
    "char_ptr":            ("char*",               "char*"),
    "const_char_ptr":      ("const char*",         "const char*"),
    "const_char_ptr_ptr":  ("const char* const*",  "const char* const*"),
    "size":                ("size_t",              "size_t"),
    "ssize":               ("ssize_t",             "ssize_t"),
    "size_ptr":            ("size_t*",             "size_t*"),
    "off":                 ("int64_t",             "int64_t"),
    "u32":                 ("uint32_t",            "uint32_t"),
    "u32_ptr":             ("uint32_t*",           "uint32_t*"),
    "u64":                 ("uint64_t",            "uint64_t"),
    "i32":                 ("int32_t",             "int32_t"),
    "i64":                 ("int64_t",             "int64_t"),
    "i64_ptr":             ("int64_t*",            "int64_t*"),
    "bool":                ("bool",                "bool"),
    "const_spawn_opts_ptr": ("const spawn_opts_t*", "const spawn_opts_t*"),
}


def gen_syscall_numbers_h(syscalls):
    out = ["// DO NOT EDIT — generated from schemas/syscalls.toml",
           "#ifndef SKL_SYSCALL_NUMBERS_H",
           "#define SKL_SYSCALL_NUMBERS_H",
           ""]
    for s in syscalls:
        out.append(f"#define SKL_SYS_{s['name'].upper():32s} {s['number']}")
    out.append("")
    out.append(f"#define SKL_SYS_COUNT_DECLARED {len(syscalls)}")
    out.append("")
    out.append("#endif")
    return "\n".join(out) + "\n"


def gen_status_h(statuses):
    out = ["// DO NOT EDIT — generated from schemas/status.toml",
           "#ifndef SKL_STATUS_H",
           "#define SKL_STATUS_H",
           "",
           "#include <stdint.h>",
           "",
           "typedef int32_t status_t;",
           ""]
    for st in statuses:
        out.append(f"#define {st['name']:40s} ((status_t)0x{st['value']:08x})")
    out.append("")
    out.append("const char* status_name(status_t s);")
    out.append("const char* status_describe(status_t s);")
    out.append("")
    out.append("#endif")
    return "\n".join(out) + "\n"


# Remaining generators (libc wrappers, kernel dispatch table, status string
# table) follow the same shape. The mechanical code is intentionally not
# written yet — fill in when implementing v1.


def main(argv):
    syscalls, statuses = load_schemas()
    validate(syscalls, statuses)

    # For now, just print what we'd generate.
    if "--print" in argv or len(argv) == 1:
        print(f"# {len(syscalls)} syscalls, {len(statuses)} status codes")
        print()
        print("=== userland/libc/include/sys/syscall.h ===")
        print(gen_syscall_numbers_h(syscalls))
        print("=== userland/libc/include/sys/status.h ===")
        print(gen_status_h(statuses))
        return 0

    if "--check" in argv:
        # TODO: compare expected vs current contents of the generated files;
        # exit 1 if stale. Useful for CI.
        print("not yet implemented", file=sys.stderr)
        return 2

    # TODO: write to disk
    print("write mode not yet implemented; use --print to inspect output",
          file=sys.stderr)
    return 2


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
