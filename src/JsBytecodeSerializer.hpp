#pragma once
#include <string>
#include <vector>
#include "BytecodeChunk.hpp"

// ============================================================================
// JSON bytecode serializer -- the C++ -> JavaScript interface.
//
// Emits the "PRC1" JSON document that PyRiteJS/src/runtime/VM.js loads:
//
// {
//   "magic": "PRC1",
//   "version": 1,
//   "all_functions": [
//     {
//       "name": "main",
//       "code": [0, 4, 0, 0, 0, ...],       // bytecode bytes
//       "constants": [
//         {"t": 1, "v": "123"},            // Number (string, to keep precision)
//         {"t": 2, "v": "hello"},          // String
//         {"t": 0},                        // Null
//         {"t": 4, "v": [...]},            // List
//         {"t": 5, "v": {...}}             // Dict
//       ],
//       "lines": [1, 1, 1, ...],           // line per byte (may be omitted)
//       "arity": 0,
//       "num_params": 0,
//       "upvalue_count": 0,
//       "max_locals": 4,
//       "upvalue_descriptors": [{"is_local": true, "index": 0}],
//       "params": [{"name": "x", "has_default": false, "default": null}],
//       "local_names": ["x", "y"],
//       "is_method": false,
//       "is_exposed": false
//     }
//   ],
//   "entry_function": 0,
//   "exports": ["init"]
// }
//
// The constant type tags match PyRiteJS/src/runtime/PyValue.js (TYPE):
//   0 NULL   1 NUMBER   2 STRING   3 BOOLEAN   4 LIST   5 DICT   10 BINARY
//
// NOTE ON LIMITS (honest list, not a to-do list):
//   * Functions live in the function table and are referenced by index through
//     OP_CLOSURE; they are never constants, so there is no FUNCTION constant
//     tag to emit.
//   * A parameter default that is not a literal cannot be represented in the
//     constant format; such parameters are emitted with "default": null.
//   * Binary values (tag 10) are emitted, but the JS VM has no decoder for
//     them yet and falls back to Null.
// ============================================================================

// Options that describe how the program should be presented to the host.
struct JsonExportInfo {
    // Names the host is allowed to call (the script's `expose` declarations).
    std::vector<std::string> exports;
    // Subset of the above that are function definitions.
    std::vector<std::string> exposed_functions;
    // Line numbers are useful for debugging but leak source structure and
    // roughly double the file size. Release builds should turn this off.
    bool include_lines = true;
};

// Serialize a compiled chunk to the PRC1 JSON document.
std::string serializeToJson(BytecodeChunkPtr chunk, const JsonExportInfo& info = JsonExportInfo());

// The outcome of an export attempt.
struct JsonExportResult {
    bool ok;
    std::string error;   // empty when ok
};

// Write the PRC1 JSON document to disk.
//
// Refuses (ok == false) when the program contains an opcode the JavaScript
// VM cannot execute -- OP_EVAL_AST and the native try/catch opcodes. Those
// work in the C++ VM because it can fall back to the AST interpreter, but
// the browser VM has no such fallback, so shipping such a program would
// mean "runs locally, breaks when packaged". The error names the function
// and the feature so the author can fix it.
JsonExportResult writeJsonToFile(BytecodeChunkPtr chunk, const std::string& path,
                                 const JsonExportInfo& info = JsonExportInfo());
