# PyRite — A Lightweight Scripting Language with BigNumber Arithmetic

PyRite is an interpreted scripting language that blends syntax from Python, C++, and BASIC. Its standout feature is **transparent arbitrary-precision arithmetic**: every number is a `BigNumber` by default, with no import, no `Decimal()`, no precision ceiling.

Version **v0.31.0** — hybrid AST/bytecode interpreter with `.prc`/`.prt` serialization.

---

## Syntax Design Philosophy

### Natural-Language Block Delimiters

PyRite uses English keywords as block boundaries instead of `{}` or indentation, making the language approachable for beginners and resilient to formatting changes.

```
if score >= 90 then
  say("excellent")
elif score >= 60 then
  say("pass")
else
  say("fail")
end
```

All blocks can be closed with a generic `end`, or with specific keywords (`endif`, `endfn`, `endwhile`, `endtry`, `endins`) for clarity.

### Explicit Type Keywords, Automatic Conversion

Variables are declared with a type keyword (`dec`, `str`, `bin`, `ln`, `dim`, `any`), but values are converted automatically on assignment:

```
dec x = "123"       // string → number (auto-converted)
str y = 456         // number → string (auto-converted)
any z = some_fn()   // any type accepted
```

This sits between static and dynamic typing: the declaration signals intent, but the runtime handles conversion.

### Transparent BigNumber Arithmetic

Every number is arbitrary-precision. No special syntax, no extra library:

```
fn fact(dec n) do
  if n == 0 then return 1 else return n * fact(n - 1) endif
end
say(fact(100))
// 93326215443944152681699238856266700490715968264381621468592963895217599993229915608941463976156518286253697920827223758251185210916864000000000000000000000000
```

### First-Class `swap()` and `using-as`

```
swap(a, b)                  // exchange two variables
using very_long_name as v   // create a short alias
```

### Lambda and Arrow Functions

```
fn double(dec x) -> x * 2          // named shorthand
dec f = fn(dec n) -> n * n         // anonymous lambda
apply(5, fn(dec n) -> n * 3)       // higher-order
```

---

## Feature Overview

| Category | Features |
|----------|----------|
| **Types** | `dec` (BigNumber), `str`, `bin` (hex binary), `ln` (list), `dim` (dict), `nul`, `any` |
| **Operators** | `+` `-` `*` `/` `^` `%`, comparison, `not` `and` `or`, `+=` `-=` etc. |
| **Control flow** | `if`/`elif`/`else`, `while`/`fin`, `loop`/`for`/`until`, `for-in`, `break`/`continue` |
| **Functions** | `fn` definition, `->` shorthand, lambdas, closures, default params, type hints |
| **OOP** | `struct` (data-only), `ins` (class with methods), `new()`, `this` |
| **Error handling** | `try`/`catch`/`fin`, `raise` |
| **Modules** | `require` (lazy-loading), single-file and directory modules, `expose`, `_on_load()` hook |
| **I/O** | `say()`, `ask()` |
| **Async** | `await`/`then` (polling-based) |

---

## Quick Start

### Build

```bash
make release    # optimized build  (-O2 -DNDEBUG)
make debug      # debug build      (-O0 -g -DDEBUG)
make clean      # remove artifacts
```

Requires a C++11 compiler (g++ or clang++).

### Run

```bash
./PyRite script.pr          # run a source file (AST interpreter)
./PyRite --vm script.pr     # run via bytecode VM
./PyRite script.prc         # load and run compiled bytecode
./PyRite script.prt         # load and run serialized AST
./PyRite                    # start REPL
```

### Export

```bash
./PyRite --emit-bytecode out.prc script.pr   # compile + save .prc + run
./PyRite --save-bytecode out.prc script.pr   # compile + save only
./PyRite --emit-ast out.prt script.pr        # save .prt + run
./PyRite --save-ast out.prt script.pr        # save .prt only
```

---

## REPL Commands

| Command | Description |
|---------|-------------|
| `run()` | Execute buffered code |
| `run(tick=1)` | Execute and show timing |
| `run(limit=1000)` | Set timeout (ms) |
| `halt()` | Exit |
| `about()` | Version info |
| `help()` | List built-in functions |
| `help("abs")` | Help for a specific function |
| `$ code` | Execute and add to buffer |
| `$# code` | Execute without adding to buffer |

---

## Implementation Architecture

```
Source (.pr)
    │
    ▼
┌────────────┐
│  Tokenizer │  Lexical analysis: char stream → token stream
└─────┬──────┘       (src/Tokenizer.hpp, src/Tokenizer.cpp)
      │
      ▼
┌────────────┐
│   Parser   │  Recursive-descent: tokens → AST (35 node types)
└─────┬──────┘       (src/Parser.hpp, src/Parser.cpp)
      │
      ▼
┌────────────┐     ┌──────────────────┐     ┌──────────────┐
│  Compiler  │────▶│ BytecodeChunk    │────▶│     VM       │
│ AST → BC   │     │ (CompiledFunction)│     │ Stack-based  │
└────────────┘     └──────────────────┘     │ bytecode VM  │
      │                                      └──────┬───────┘
      │                                             │
      ▼                                             ▼
┌────────────┐                             ┌────────────────┐
│Interpreter │  Hybrid bridge:              │  Serializer    │
│ AST-walk   │  VM falls back to            │ .prc / .prt    │
│            │  Interpreter via OP_EVAL_AST  │ binary I/O     │
└────────────┘                             └────────────────┘
      │
      ▼
┌────────────┐
│ Environment│  Lexical scoping, closures, OOP (Class/Instance)
└────────────┘       (src/Environment.hpp, src/Environment.cpp)
      │
      ▼
┌────────────┐
│   Value    │  Runtime type system: Null, Number (BigNumber),
│            │  String, Binary, Ln (list), Dim (dict),
│            │  Function, NativeFn, BoundMethod, Exception,
│            │  ModuleProxy, Closure
└────────────┘       (src/Value.hpp, src/Value.cpp, src/BigNumber.hpp)
```

### Module Details

| Module | Files | Responsibility |
|--------|-------|---------------|
| **BigNumber** | `BigNumber.hpp` | Arbitrary-precision decimal arithmetic (C++ implementation, no GMP dependency) |
| **OpCode** | `OpCode.hpp` | ~50 instruction opcodes for the bytecode VM |
| **Tokenizer** | `Tokenizer.hpp`, `Tokenizer.cpp` | Character-level scanning; produces `Token` stream (25+ token types) |
| **Parser** | `Parser.hpp`, `Parser.cpp` | Recursive-descent parser; produces 35 concrete `AstNode` subtypes |
| **Ast** | `Ast.hpp` | AST node definitions: `LiteralNode`, `BinaryOpNode`, `IfStatementNode`, `FnDefNode`, `ClassDefNode`, etc. |
| **Compiler** | `Compiler.hpp`, `Compiler.cpp` | Walks AST, emits bytecode into `CompiledFunction` with constant pool, upvalue descriptors, and AST side-table for OP_EVAL_AST |
| **BytecodeChunk** | `BytecodeChunk.hpp` | `CompiledFunction` (code bytes, constants, lines, params, upvalues, local names) + `Closure` (function + captured upvalues) |
| **VM** | `VM.hpp`, `VM.cpp` | Stack-based bytecode execution with call frames, upvalue management, try/catch handlers, and AST-interpreter bridge |
| **Interpreter** | `Interpreter.hpp`, `Interpreter.cpp` | AST-walking interpreter (Visitor pattern via `accept()`). Defines all native functions (abs, len, sin, sort, etc.) and module loading |
| **Environment** | `Environment.hpp`, `Environment.cpp` | Lexical scope chains, `Class`/`Instance` OOP model with method binding |
| **Value** | `Value.hpp`, `Value.cpp` | Polymorphic value hierarchy: 12 concrete subtypes with arithmetic, comparison, subscript, and slice operations |
| **Serializer** | `Serializer.hpp`, `Serializer.cpp` | Binary serialization of `BytecodeChunk` (.prc) and `vector<AstNodePtr>` (.prt) with magic-number validation and versioning |
| **msgs** | `msgs.hpp`, `msg_cn.hpp`, `msg_en.hpp` | Enum-based multi-language message system (~100 messages in Chinese and English) |

### Execution Modes

1. **AST Interpreter** (`--ast`, default): Parse → walk AST directly. Full language support, simpler debugging.
2. **Bytecode VM** (`--vm`): Parse → compile to bytecode → execute on VM. Faster for hot paths (arithmetic, loops, calls). Falls back to AST interpreter via `OP_EVAL_AST` for complex features (classes, try-catch, modules).
3. **Pre-compiled bytecode** (`.prc`): Load serialized `BytecodeChunk` → VM. No parsing needed.
4. **Pre-compiled AST** (`.prt`): Load serialized AST → Interpreter. No parsing needed.

### Key Design Decisions

- **Hybrid execution**: The compiler emits pure bytecode for simple operations (arithmetic, locals, control flow) and `OP_EVAL_AST` instructions for complex features. The VM handles both, bridging its locals into a temporary `Environment` for the AST interpreter.
- **Lazy module loading**: `require` registers a `ModuleProxy` that only loads and parses the target file on first access.
- **BigNumber transparency**: All numbers are `BigNumber` internally. The `NumberValue` class wraps it, and arithmetic operators on `Value` delegate to `BigNumber` methods. No runtime type dispatch overhead for numeric operations once compiled.

---

## File Reference

| File | Description |
|------|-------------|
| `src/main.cpp` | Entry point, CLI parsing, REPL |
| `src/Tokenizer.*` | Lexer |
| `src/Parser.*` | Parser |
| `src/Ast.hpp` | AST node types |
| `src/Compiler.*` | AST → bytecode compiler |
| `src/BytecodeChunk.hpp` | Bytecode data structures |
| `src/VM.*` | Virtual machine |
| `src/Interpreter.*` | AST interpreter |
| `src/Environment.*` | Scope / OOP runtime |
| `src/Value.*` | Value types |
| `src/BigNumber.hpp` | BigNumber arithmetic |
| `src/OpCode.hpp` | Instruction set |
| `src/Serializer.*` | .prc / .prt serialization |
| `src/msgs.hpp` | Message enum |
| `src/msg_cn.hpp` | Chinese translations |
| `src/msg_en.hpp` | English translations |
| `src/File.hpp` | File I/O object (WIP) |

---

## License

(c) 2024-2026 DarkstarXD. All Rights Reserved.
