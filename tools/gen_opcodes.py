#!/usr/bin/env python3
"""
gen_opcodes.py -- generate the PyRite opcode tables from opcodes.def.

The bytecode contract lives in exactly one place:  opcodes.def
This script projects it onto the implementations:

    src/OpCode.hpp                        (C++ compiler / VM)
    src/CastTypes.hpp                     (C++ cast type codes)
    ../PyRiteJS/src/runtime/opcodes.js    (JavaScript VM)
    ../PyRiteJS/src/runtime/castTypes.js  (JavaScript cast type codes)
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
CAST_CPP_PATH = os.path.join(PYRITE_ROOT, "src", "CastTypes.hpp")
INFO_CPP_PATH = os.path.join(PYRITE_ROOT, "src", "OpCodeInfo.hpp")
DEFAULT_JS_ROOT = os.path.join(os.path.dirname(PYRITE_ROOT), "PyRiteJS")
JS_OPCODES_PATH = os.path.join("src", "runtime", "opcodes.js")
JS_CAST_PATH = os.path.join("src", "runtime", "castTypes.js")
JS_DEF_PATH = os.path.join("src", "runtime", "opcodes.def")


class Op:
    __slots__ = ("number", "name", "has_operand", "comment")

    def __init__(self, number, name, has_operand, comment):
        self.number = number
        self.name = name
        self.has_operand = has_operand
        self.comment = comment


class Cast:
    __slots__ = ("token", "code", "comment")

    def __init__(self, token, code, comment):
        self.token = token
        self.code = code
        self.comment = comment


class Unsupported:
    __slots__ = ("name", "reason")

    def __init__(self, name, reason):
        self.name = name
        self.reason = reason


def parse_def(path):
    """Parse opcodes.def into (ops, sections, casts)."""
    ops = []
    sections = []          # list of (title, [Op])
    casts = []
    unsupported = []       # list of Unsupported
    current = None

    with open(path, "r", encoding="utf-8") as fh:
        for lineno, raw in enumerate(fh, 1):
            line = raw.strip()
            if not line:
                continue
            if line.startswith("@js-unsupported"):
                parts = line[len("@js-unsupported"):].split(None, 1)
                if len(parts) < 1:
                    raise SystemExit("%s:%d: @js-unsupported needs an opcode name"
                                     % (path, lineno))
                reason = parts[1].strip() if len(parts) > 1 else ""
                unsupported.append(Unsupported(parts[0], reason))
                continue
            if line.startswith("@cast"):
                parts = line[len("@cast"):].split()
                if len(parts) < 2:
                    raise SystemExit("%s:%d: @cast needs a token and a code"
                                     % (path, lineno))
                try:
                    cast_code = int(parts[1])
                except ValueError:
                    raise SystemExit("%s:%d: bad cast code %r" % (path, lineno, parts[1]))
                casts.append(Cast(parts[0], cast_code, " ".join(parts[2:])))
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

    validate(ops, casts, unsupported, path)
    return ops, sections, casts, unsupported


def validate(ops, casts, unsupported, path):
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

    cast_codes = [c.code for c in casts]
    if len(set(cast_codes)) != len(cast_codes):
        dupes = sorted({c for c in cast_codes if cast_codes.count(c) > 1})
        raise SystemExit("%s: duplicate cast type codes: %s" % (path, dupes))
    cast_tokens = [c.token for c in casts]
    if len(set(cast_tokens)) != len(cast_tokens):
        dupes = sorted({t for t in cast_tokens if cast_tokens.count(t) > 1})
        raise SystemExit("%s: duplicate cast tokens: %s" % (path, dupes))

    known = set(op.name for op in ops)
    for u in unsupported:
        if u.name not in known:
            raise SystemExit("%s: @js-unsupported names unknown opcode %s"
                             % (path, u.name))


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
# C++ cast type table
# --------------------------------------------------------------------------

CAST_CPP_TEMPLATE = """#pragma once
#include <cstdint>
#include "Tokenizer.hpp"

// ============================================================================
// PyRite cast type codes -- the operand of OP_CAST.
// ============================================================================
// GENERATED FILE -- DO NOT EDIT BY HAND.
// Source: opcodes.def (sha256 {sha})
// Regenerate: python3 tools/gen_opcodes.py
//
// These codes are STABLE. They are deliberately not TokenType values:
// TokenType is a tokenizer implementation detail whose numbering shifts
// whenever a keyword is added, which would silently change the meaning of
// every already-compiled .prc file.
//
// The compiler emits one of these codes as the operand of OP_CAST; the VM
// (C++ and JavaScript alike) switches on it.
// ============================================================================

enum class CastType : uint8_t {{
{enum_body}}};

// Total number of cast type codes.
constexpr int CAST_TYPE_COUNT = {count};

// Map a declaration/cast keyword to its stable cast code.
// Returns false when the token is not a type keyword.
inline bool cast_code_for(TokenType t, uint32_t& out) {{
    switch (t) {{
{cases}    }}
    return false;
}}
"""


def render_cast_cpp(casts, sha):
    width = max([len(c.token) for c in casts]) if casts else 6
    lines = []
    for c in casts:
        comment = c.comment or ""
        if comment:
            lines.append("    CAST_%-*s = %d,   // %s" % (width + 1, c.token, c.code, comment))
        else:
            lines.append("    CAST_%-*s = %d," % (width + 1, c.token, c.code))
    enum_body = "\n".join(lines) + "\n"

    cases = []
    for c in casts:
        cases.append("        case TokenType::%s: out = %d; return true;" % (c.token, c.code))
    cases.append("")

    return CAST_CPP_TEMPLATE.format(
        sha=sha, enum_body=enum_body, count=len(casts), cases="\n".join(cases))


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

// Opcodes this VM cannot execute. The C++ compiler can fall back to its AST
// interpreter for these; the browser VM has no such fallback, so bytecode
// containing them would run locally and break once packaged. The C++ JSON
// exporter refuses to emit such a program; this set lets the VM refuse to
// load one too.
const JS_UNSUPPORTED_OPS = new Set([
{unsupported_body}]);

module.exports = {{ OP, OP_NAMES, OP_COUNT, NO_OPERAND_OPS, JS_UNSUPPORTED_OPS }};
"""


def render_js(ops, sections, unsupported, sha):
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

    unsupported_names = set(u.name for u in unsupported)
    unsupported_lines = []
    for op in ops:
        if op.name in unsupported_names:
            unsupported_lines.append("  %2d,  // %s" % (op.number, op.name))
    unsupported_body = "\n".join(unsupported_lines) + "\n"

    return JS_TEMPLATE.format(
        sha=sha, op_body=op_body, count=len(ops),
        no_operand_body=no_operand_body, unsupported_body=unsupported_body)


# --------------------------------------------------------------------------
# JavaScript cast type table
# --------------------------------------------------------------------------

CAST_JS_TEMPLATE = """// ============================================================================
// PyRite Cast Type Codes -- the operand of OP_CAST.
// ============================================================================
// GENERATED FILE -- DO NOT EDIT BY HAND.
// Source: opcodes.def (sha256 {sha})
// Regenerate: python3 ../PyRite/tools/gen_opcodes.py --js-root .
//
// These codes are STABLE and MUST match src/CastTypes.hpp in the C++
// compiler. The C++ compiler emits one of them as the operand of OP_CAST.
// ============================================================================

const CAST = {{
{cast_body}}};

// Reverse map for debugging
const CAST_NAMES = {{}};
for (const [name, code] of Object.entries(CAST)) {{
  CAST_NAMES[code] = name;
}}

// Total number of cast type codes.
const CAST_COUNT = {count};

module.exports = {{ CAST, CAST_NAMES, CAST_COUNT }};
"""


def render_cast_js(casts, sha):
    width = max([len(c.token) for c in casts]) if casts else 6
    lines = []
    for c in casts:
        comment = c.comment or ""
        if comment:
            lines.append("  %-*s %d,  // %s" % (width + 2, c.token + ":", c.code, comment))
        else:
            lines.append("  %-*s %d," % (width + 2, c.token + ":", c.code))
    cast_body = "\n".join(lines) + "\n"
    return CAST_JS_TEMPLATE.format(
        sha=sha, cast_body=cast_body, count=len(casts))


# --------------------------------------------------------------------------
# C++ opcode metadata (operand table + JS support)
# --------------------------------------------------------------------------

INFO_CPP_TEMPLATE = """#pragma once
#include "OpCode.hpp"

// ============================================================================
// PyRite opcode metadata.
// ============================================================================
// GENERATED FILE -- DO NOT EDIT BY HAND.
// Source: opcodes.def (sha256 {sha})
// Regenerate: python3 tools/gen_opcodes.py
//
// Two tables live here:
//
//   OPCODE_HAS_OPERAND      does the instruction carry a 3-byte operand?
//   OPCODE_JS_UNSUPPORTED   can the JavaScript VM execute the instruction?
//
// The operand table is what lets the C++ side walk a bytecode stream without
// hard-coding instruction lengths. It used to be a hand-written list in the
// disassembler, which is exactly the kind of thing that drifts.
// ============================================================================

// Whether the instruction carries a 3-byte big-endian operand.
constexpr bool OPCODE_HAS_OPERAND[OPCODE_COUNT] = {{
{operand_table}}};

// Whether the JavaScript VM can execute the instruction. The JS VM has no AST
// fallback and no native exception handling, so bytecode containing any of
// these cannot run in the browser.
constexpr bool OPCODE_JS_UNSUPPORTED[OPCODE_COUNT] = {{
{unsupported_table}}};

// Human-readable reason an opcode is unsupported, shown to the author when
// the JSON exporter refuses a program. Empty when the opcode is supported.
inline const char* opcode_js_unsupported_reason(OpCode op) {{
    switch (op) {{
{cases}    }}
    return "";
}}
"""


def render_opcode_info(ops, unsupported, sha):
    by_name = dict((op.name, op) for op in ops)

    operand_lines = []
    for op in ops:
        operand_lines.append("    /* %2d */ %-5s,  // %s"
                             % (op.number, "true" if op.has_operand else "false", op.name))
    operand_table = chr(10).join(operand_lines) + chr(10)

    unsupported_names = set(u.name for u in unsupported)
    unsupported_lines = []
    for op in ops:
        unsupported_lines.append("    /* %2d */ %-5s,  // %s"
                                 % (op.number, "true" if op.name in unsupported_names else "false", op.name))
    unsupported_table = chr(10).join(unsupported_lines) + chr(10)

    cases = []
    for u in unsupported:
        if u.name not in by_name:
            continue
        reason = u.reason.replace(chr(34), chr(92) + chr(34))
        cases.append("        case OpCode::%s: return %s%s%s;" % (u.name, chr(34), reason, chr(34)))
    cases.append("")

    return INFO_CPP_TEMPLATE.format(
        sha=sha,
        operand_table=operand_table,
        unsupported_table=unsupported_table,
        cases=chr(10).join(cases))


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

    ops, sections, casts, unsupported = parse_def(args.def_path)
    sha = def_sha256(args.def_path)

    targets = [
        (CPP_PATH, render_cpp(ops, sections, sha)),
        (CAST_CPP_PATH, render_cast_cpp(casts, sha)),
        (INFO_CPP_PATH, render_opcode_info(ops, unsupported, sha)),
    ]

    js_root = os.path.abspath(args.js_root)
    js_opcodes = os.path.join(js_root, JS_OPCODES_PATH)
    js_cast = os.path.join(js_root, JS_CAST_PATH)
    js_def = os.path.join(js_root, JS_DEF_PATH)

    if os.path.isdir(js_root):
        targets.append((js_opcodes, render_js(ops, sections, unsupported, sha)))
        targets.append((js_cast, render_cast_js(casts, sha)))
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
        print("\n%d opcodes, %d cast types and %d js-unsupported opcodes "
              "generated from opcodes.def (sha256 %s)"
              % (len(ops), len(casts), len(unsupported), sha[:12]))
    return 0


if __name__ == "__main__":
    sys.exit(main())
