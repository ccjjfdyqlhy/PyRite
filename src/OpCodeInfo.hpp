#pragma once
#include "OpCode.hpp"

// ============================================================================
// PyRite opcode metadata.
// ============================================================================
// GENERATED FILE -- DO NOT EDIT BY HAND.
// Source: opcodes.def (sha256 d957ad6127a728b2fd13fd22422a4dc821fc06a6754b1374dce5092345df8a03)
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
constexpr bool OPCODE_HAS_OPERAND[OPCODE_COUNT] = {
    /*  0 */ true ,  // OP_CONSTANT
    /*  1 */ false,  // OP_NULL
    /*  2 */ false,  // OP_TRUE
    /*  3 */ false,  // OP_FALSE
    /*  4 */ false,  // OP_POP
    /*  5 */ false,  // OP_DUP
    /*  6 */ true ,  // OP_GET_LOCAL
    /*  7 */ true ,  // OP_SET_LOCAL
    /*  8 */ true ,  // OP_SET_LOCAL_KEEP
    /*  9 */ true ,  // OP_GET_GLOBAL
    /* 10 */ true ,  // OP_SET_GLOBAL
    /* 11 */ true ,  // OP_DEFINE_GLOBAL
    /* 12 */ true ,  // OP_GET_UPVALUE
    /* 13 */ true ,  // OP_SET_UPVALUE
    /* 14 */ false,  // OP_CLOSE_UPVALUE
    /* 15 */ false,  // OP_GET_INDEX
    /* 16 */ false,  // OP_SET_INDEX
    /* 17 */ true ,  // OP_GET_PROPERTY
    /* 18 */ true ,  // OP_SET_PROPERTY
    /* 19 */ true ,  // OP_BUILD_LIST
    /* 20 */ true ,  // OP_BUILD_DICT
    /* 21 */ false,  // OP_GET_LEN
    /* 22 */ false,  // OP_GET_SLICE
    /* 23 */ false,  // OP_SET_SLICE
    /* 24 */ false,  // OP_ADD
    /* 25 */ false,  // OP_SUB
    /* 26 */ false,  // OP_MUL
    /* 27 */ false,  // OP_DIV
    /* 28 */ false,  // OP_MOD
    /* 29 */ false,  // OP_POW
    /* 30 */ false,  // OP_NEGATE
    /* 31 */ false,  // OP_NOT
    /* 32 */ false,  // OP_EQUAL
    /* 33 */ false,  // OP_NOT_EQUAL
    /* 34 */ false,  // OP_LESS
    /* 35 */ false,  // OP_LESS_EQUAL
    /* 36 */ false,  // OP_GREATER
    /* 37 */ false,  // OP_GREATER_EQUAL
    /* 38 */ true ,  // OP_JUMP
    /* 39 */ true ,  // OP_JUMP_IF_FALSE
    /* 40 */ true ,  // OP_JUMP_IF_TRUE
    /* 41 */ true ,  // OP_JUMP_IF_FALSE_KEEP
    /* 42 */ true ,  // OP_JUMP_IF_TRUE_KEEP
    /* 43 */ true ,  // OP_LOOP
    /* 44 */ true ,  // OP_CALL
    /* 45 */ true ,  // OP_CLOSURE
    /* 46 */ false,  // OP_RETURN
    /* 47 */ false,  // OP_RETURN_NULL
    /* 48 */ false,  // OP_PRINT
    /* 49 */ false,  // OP_INPUT
    /* 50 */ true ,  // OP_CAST
    /* 51 */ false,  // OP_SWAP
    /* 52 */ true ,  // OP_EVAL_AST
    /* 53 */ false,  // OP_RAISE
    /* 54 */ true ,  // OP_TRY_BEGIN
    /* 55 */ false,  // OP_TRY_END
    /* 56 */ false,  // OP_CATCH
    /* 57 */ false,  // OP_HALT
};

// Whether the JavaScript VM can execute the instruction. The JS VM has no AST
// fallback and no native exception handling, so bytecode containing any of
// these cannot run in the browser.
constexpr bool OPCODE_JS_UNSUPPORTED[OPCODE_COUNT] = {
    /*  0 */ false,  // OP_CONSTANT
    /*  1 */ false,  // OP_NULL
    /*  2 */ false,  // OP_TRUE
    /*  3 */ false,  // OP_FALSE
    /*  4 */ false,  // OP_POP
    /*  5 */ false,  // OP_DUP
    /*  6 */ false,  // OP_GET_LOCAL
    /*  7 */ false,  // OP_SET_LOCAL
    /*  8 */ false,  // OP_SET_LOCAL_KEEP
    /*  9 */ false,  // OP_GET_GLOBAL
    /* 10 */ false,  // OP_SET_GLOBAL
    /* 11 */ false,  // OP_DEFINE_GLOBAL
    /* 12 */ false,  // OP_GET_UPVALUE
    /* 13 */ false,  // OP_SET_UPVALUE
    /* 14 */ false,  // OP_CLOSE_UPVALUE
    /* 15 */ false,  // OP_GET_INDEX
    /* 16 */ false,  // OP_SET_INDEX
    /* 17 */ false,  // OP_GET_PROPERTY
    /* 18 */ false,  // OP_SET_PROPERTY
    /* 19 */ false,  // OP_BUILD_LIST
    /* 20 */ false,  // OP_BUILD_DICT
    /* 21 */ false,  // OP_GET_LEN
    /* 22 */ false,  // OP_GET_SLICE
    /* 23 */ false,  // OP_SET_SLICE
    /* 24 */ false,  // OP_ADD
    /* 25 */ false,  // OP_SUB
    /* 26 */ false,  // OP_MUL
    /* 27 */ false,  // OP_DIV
    /* 28 */ false,  // OP_MOD
    /* 29 */ false,  // OP_POW
    /* 30 */ false,  // OP_NEGATE
    /* 31 */ false,  // OP_NOT
    /* 32 */ false,  // OP_EQUAL
    /* 33 */ false,  // OP_NOT_EQUAL
    /* 34 */ false,  // OP_LESS
    /* 35 */ false,  // OP_LESS_EQUAL
    /* 36 */ false,  // OP_GREATER
    /* 37 */ false,  // OP_GREATER_EQUAL
    /* 38 */ false,  // OP_JUMP
    /* 39 */ false,  // OP_JUMP_IF_FALSE
    /* 40 */ false,  // OP_JUMP_IF_TRUE
    /* 41 */ false,  // OP_JUMP_IF_FALSE_KEEP
    /* 42 */ false,  // OP_JUMP_IF_TRUE_KEEP
    /* 43 */ false,  // OP_LOOP
    /* 44 */ false,  // OP_CALL
    /* 45 */ false,  // OP_CLOSURE
    /* 46 */ false,  // OP_RETURN
    /* 47 */ false,  // OP_RETURN_NULL
    /* 48 */ false,  // OP_PRINT
    /* 49 */ false,  // OP_INPUT
    /* 50 */ false,  // OP_CAST
    /* 51 */ false,  // OP_SWAP
    /* 52 */ true ,  // OP_EVAL_AST
    /* 53 */ false,  // OP_RAISE
    /* 54 */ true ,  // OP_TRY_BEGIN
    /* 55 */ true ,  // OP_TRY_END
    /* 56 */ true ,  // OP_CATCH
    /* 57 */ false,  // OP_HALT
};

// Human-readable reason an opcode is unsupported, shown to the author when
// the JSON exporter refuses a program. Empty when the opcode is supported.
inline const char* opcode_js_unsupported_reason(OpCode op) {
    switch (op) {
        case OpCode::OP_EVAL_AST: return "classes, structs, require, using, await and other AST fallbacks";
        case OpCode::OP_TRY_BEGIN: return "try/catch";
        case OpCode::OP_TRY_END: return "try/catch";
        case OpCode::OP_CATCH: return "try/catch";
    }
    return "";
}
