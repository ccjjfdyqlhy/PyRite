#!/usr/bin/env python3
"""
gen_opcodes.py -- generate the PyRite opcode tables from opcodes.def.

The bytecode contract lives in exactly one place:  opcodes.def
This script projects it onto the two implementations:

    src/OpCode.hpp                        (C++ compiler / VM)
    ../PyRiteJS/src/runtime/opcodes.js    (JavaScript VM)
    ../PyRiteJS/src/runtime/opcodes.def   (pinned snapshot for the JS repo)

Usage
-----
    python3 tools/gen_opcodes.py            # write all generated files
    python3 tools/gen_opcodes.py --check    # CI: exit 1 if anything is stale
    python3 tools/gen_opcodes.py --js-root PATH   # point at another PyRiteJS

The generated files carry the sha256 of the .def they were produced from, so a
hand edit is detectable even when the sibling repository is not available.
"""

import argparse
import hashlib
import os
import re
import sys

PYRITE_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DEF_PATH = os.path.join(PYRITE_ROOT, "opcodes.def")
CPP_PATH = os.path.join(PYRITE_ROOT, "src", "OpCode.hpp")
DEFAULT_JS_ROOT = os.path.join(os.path.dirname(PYRITE_ROOT), "PyRiteJS")
JS_OPCODES_PATH = os.path.join("src", "runtime", "opcodes.js")
JS_DEF_PATH = os.path.join("src", "runtime", "opcodes.def")


class Op:
    __slots__ = ("number", "name", "has_operand", "comment")

    def __init__(self, number, name, has_operand, comment):
        self.number = number
        self.name = name
        self.has_operand = has_operand
        self.comment = comment


def parse_def(path):
    """Parse opcodes.def into (list[Op], list[(section_title, [Op])])."""
    ops = []
    sections = []          # list of (title, [Op])
    current = None

    with open(path, "r", encoding="utf-8") as fh:
        for lineno, raw in enumerate(fh, 1):
            line = raw.strip()
            if not line:
                continue
            if line.startswith("@section"):
                title = line[len("@section"):].strip()
                current = (title, [])
                sections.append(current)
                continue
            if line.startswith("#"):
                continue
            code, _, comment = line.partition("#")
            parts = code.split()
            if len(parts) != 3:
                raise SystemExit(
                    "%s:%d: expected '<num> <NAME> <operand|no-operand>', got %r"
                    % (path, lineno, code.strip()))
            try:
                number = int(parts[0])
            except ValueError:
                raise SystemExit("%s:%d: bad opcode number %r" % (path, lineno, parts[0]))
            name = parts[1]
            flag = parts[2]
            if flag not in ("operand", "no-operand"):
                raise SystemExit("%s:%d: bad operand flag %r" % (path, lineno, flag))
            if not re.fullmatch(r"OP_[A-Z0-9_]+", name):
                raise SystemExit("%s:%d: bad opcode name %r" % (path, lineno, name))
            op = Op(number, name, flag == "operand", comment.strip())
            ops.append(op)
            if current is None:
                current = ("Ungrouped", [])
                sections.append(current)
            current[1].append(op)

    validate(ops, path)
    return ops, sections


def validate(ops, path):
    numbers = [op.number for op in ops]
    names = [op.name for op in ops]
    if len(set(numbers)) != len(numbers):
        dupes = sorted({n for n in numbers if numbers.count(n) > 1})
        raise SystemExit("%s: duplicate opcode numbers: %s" % (path, dupes))
    if len(set(names)) != len(names):
        dupes = sorted({n for n in names if names.count(n) > 1})
        raise SystemExit("%s: duplicate opcode names: %s" % (path, dupes))
    expected = list(range(len(ops)))
    if sorted(numbers) != expected:
        raise SystemExit(
            "%s: opcode numbers must be contiguous 0..%d (missing %s)"
            % (path, len(ops) - 1,
               sorted(set(expected) - set(numbers))))


def def_sha256(path):
    with open(path, "rb") as fh:
        return hashlib.sha256(fh.read()).hexdigest()


# --------------------------------------------------------------------------
# C++ header
# --------------------------------------------------------------------------

CPP_TEMPLATE = """#pragma once
#include <cstdint>
#include <string>

// ============================================================================
// PyRite Bytecode Instruction Set
// ============================================================================
// GENERATED FILE -- DO NOT EDIT BY HAND.
// Source: opcodes.def (sha256 {sha})
// Regenerate: python3 tools/gen_opcodes.py
//
// Stack-based VM. Operands are big-endian 24-bit unsigned integers (0..2^24-1)
// unless noted otherwise. Each instruction is 1 byte opcode + 3 bytes operand.
//
// Convention:
//   - "pop"  : remove and return top of value stack
//   - "peek" : read top without removing
//   - "push" : place on top of value stack
//
// The numbering below is part of the on-disk bytecode format. Changing a
// number invalidates every previously compiled .prc file.
// ============================================================================

enum class OpCode : uint8_t {{
{enum_body}}};

// Total number of opcodes (the permutation range used by the obfuscator).
constexpr int OPCODE_COUNT = {count};

// Convert opcode to string for disassembly / debugging
inline const char* op_name(OpCode op) {{
    switch (op) {{
{cases}    }}
    return "UNKNOWN";
}}
"""


def render_cpp(ops, sections, sha):
    lines = []
    for title, group in sections:
        lines.append("    // ---- %s ----" % title)
        for op in group:
            comment = op.comment or ""
            if comment:
                lines.append("    %-22s // %s" % (op.name + ",", comment))
            else:
                lines.append("    %s," % (op.name + ","))
        lines.append("")
    enum_body = "\n".join(lines).rstrip() + "\n"

    width = max(len(op.name) for op in ops)
    cases = []
    for op in ops:
        label = "case OpCode::%s:" % op.name
        cases.append("        %-*s return \"%s\";" % (width + 7, label, op.name[3:]))
    cases.append("")

    return CPP_TEMPLATE.format(
        sha=sha, enum_body=enum_body, count=len(ops), cases="\n".join(cases))


# --------------------------------------------------------------------------
# JavaScript table
# --------------------------------------------------------------------------

JS_TEMPLATE = """// ============================================================================
// PyRite Bytecode Opcode Definitions
// ============================================================================
// GENERATED FILE -- DO NOT EDIT BY HAND.
// Source: opcodes.def (sha256 {sha})
// Regenerate: python3 ../PyRite/tools/gen_opcodes.py --js-root .
//
// Stack-based VM. Operands are big-endian 24-bit unsigned integers (0..2^24-1).
// Each instruction is 1 byte opcode + 3 bytes operand.
//
// Convention:
//   - "pop"  : remove and return top of value stack
//   - "peek" : read top without removing
//   - "push" : place on top of value stack
//
// The numbering below is part of the on-disk bytecode format and MUST stay
// identical to src/OpCode.hpp in the C++ compiler.  Both are generated from
// opcodes.def; the consistency test in tests/test_opcode_consistency.js
// enforces it.
// ============================================================================

const OP = {{
{op_body}}};

// Reverse map for debugging
const OP_NAMES = {{}};
for (const [name, code] of Object.entries(OP)) {{
  OP_NAMES[code] = name;
}}

// Total number of opcodes (used for shuffle range)
const OP_COUNT = {count};

// Opcodes that have NO operand (single byte)
const NO_OPERAND_OPS = new Set([
{no_operand_body}]);

module.exports = {{ OP, OP_NAMES, OP_COUNT, NO_OPERAND_OPS }};
"""


def render_js(ops, sections, sha):
    lines = []
    for title, group in sections:
        lines.append("  // ---- %s ----" % title)
        for op in group:
            comment = op.comment or ""
            if comment:
                lines.append("  %-24s %2d,  // %s" % (op.name + ":", op.number, comment))
            else:
                lines.append("  %-24s %2d," % (op.name + ":", op.number))
        lines.append("")
    op_body = "\n".join(lines).rstrip() + "\n"

    no_operand = [op for op in ops if not op.has_operand]
    no_operand_body = "\n".join(
        "  OP.%s," % op.name for op in no_operand) + "\n"

    return JS_TEMPLATE.format(
        sha=sha, op_body=op_body, count=len(ops), no_operand_body=no_operand_body)


# --------------------------------------------------------------------------
# Driver
# --------------------------------------------------------------------------

def read_text(path):
    if not os.path.exists(path):
        return None
    with open(path, "r", encoding="utf-8") as fh:
        return fh.read()


def write_text(path, content):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w", encoding="utf-8") as fh:
        fh.write(content)


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--check", action="store_true",
                    help="fail if any generated file is out of date")
    ap.add_argument("--js-root", default=DEFAULT_JS_ROOT,
                    help="path to the PyRiteJS checkout")
    ap.add_argument("--def", dest="def_path", default=DEF_PATH,
                    help="path to opcodes.def")
    args = ap.parse_args(argv)

    ops, sections = parse_def(args.def_path)
    sha = def_sha256(args.def_path)

    targets = [
        (CPP_PATH, render_cpp(ops, sections, sha)),
    ]

    js_root = os.path.abspath(args.js_root)
    js_opcodes = os.path.join(js_root, JS_OPCODES_PATH)
    js_def = os.path.join(js_root, JS_DEF_PATH)

    if os.path.isdir(js_root):
        targets.append((js_opcodes, render_js(ops, sections, sha)))
        with open(args.def_path, "r", encoding="utf-8") as fh:
            targets.append((js_def, fh.read()))
    else:
        print("note: PyRiteJS checkout not found at %s -- "
              "skipping JavaScript outputs" % js_root)

    stale = []
    for path, expected in targets:
        actual = read_text(path)
        if actual == expected:
            print("  ok      %s" % os.path.relpath(path, PYRITE_ROOT))
            continue
        if args.check:
            stale.append(path)
            print("  STALE   %s" % os.path.relpath(path, PYRITE_ROOT))
        else:
            write_text(path, expected)
            print("  written %s" % os.path.relpath(path, PYRITE_ROOT))

    if stale:
        print("\n%d generated file(s) out of date. "
              "Run: python3 tools/gen_opcodes.py" % len(stale))
        return 1

    if not args.check:
        print("\n%d opcodes generated from opcodes.def (sha256 %s)"
              % (len(ops), sha[:12]))
    return 0


if __name__ == "__main__":
    sys.exit(main())
