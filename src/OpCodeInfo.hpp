#pragma once
#include "OpCode.hpp"

// ============================================================================
// PyRite opcode metadata.
// ============================================================================
// GENERATED FILE -- DO NOT EDIT BY HAND.
// Source: opcodes.def (sha256 12d591f75dd71877da0de718394052c1c571503ac24aef0d152b794e1f497dfc)
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
    /* 58 */ true ,  // OP_INPLACE_ADD
    /* 59 */ true ,  // OP_INPLACE_SUB
    /* 60 */ true ,  // OP_INPLACE_MUL
    /* 61 */ true ,  // OP_CMP_JUMP
    /* 62 */ true ,  // OP_INPLACE_AM
    /* 63 */ true ,  // OP_DEF_STRUCT
    /* 64 */ true ,  // OP_CALL_MEMBER
    /* 65 */ false,  // OP_RESERVED_2
    /* 66 */ false,  // OP_RESERVED_3
    /* 67 */ false,  // OP_RESERVED_4
    /* 68 */ false,  // OP_RESERVED_5
    /* 69 */ false,  // OP_RESERVED_6
    /* 70 */ false,  // OP_RESERVED_7
    /* 71 */ false,  // OP_RESERVED_8
};

// How many 3-byte big-endian operands the instruction carries (0, 1, or
// more for the fused superinstructions).
constexpr int OPCODE_OPERAND_COUNT[OPCODE_COUNT] = {
    /*  0 */ 1,     // OP_CONSTANT
    /*  1 */ 0,     // OP_NULL
    /*  2 */ 0,     // OP_TRUE
    /*  3 */ 0,     // OP_FALSE
    /*  4 */ 0,     // OP_POP
    /*  5 */ 0,     // OP_DUP
    /*  6 */ 1,     // OP_GET_LOCAL
    /*  7 */ 1,     // OP_SET_LOCAL
    /*  8 */ 1,     // OP_SET_LOCAL_KEEP
    /*  9 */ 1,     // OP_GET_GLOBAL
    /* 10 */ 1,     // OP_SET_GLOBAL
    /* 11 */ 1,     // OP_DEFINE_GLOBAL
    /* 12 */ 1,     // OP_GET_UPVALUE
    /* 13 */ 1,     // OP_SET_UPVALUE
    /* 14 */ 0,     // OP_CLOSE_UPVALUE
    /* 15 */ 0,     // OP_GET_INDEX
    /* 16 */ 0,     // OP_SET_INDEX
    /* 17 */ 1,     // OP_GET_PROPERTY
    /* 18 */ 1,     // OP_SET_PROPERTY
    /* 19 */ 1,     // OP_BUILD_LIST
    /* 20 */ 1,     // OP_BUILD_DICT
    /* 21 */ 0,     // OP_GET_LEN
    /* 22 */ 0,     // OP_GET_SLICE
    /* 23 */ 0,     // OP_SET_SLICE
    /* 24 */ 0,     // OP_ADD
    /* 25 */ 0,     // OP_SUB
    /* 26 */ 0,     // OP_MUL
    /* 27 */ 0,     // OP_DIV
    /* 28 */ 0,     // OP_MOD
    /* 29 */ 0,     // OP_POW
    /* 30 */ 0,     // OP_NEGATE
    /* 31 */ 0,     // OP_NOT
    /* 32 */ 0,     // OP_EQUAL
    /* 33 */ 0,     // OP_NOT_EQUAL
    /* 34 */ 0,     // OP_LESS
    /* 35 */ 0,     // OP_LESS_EQUAL
    /* 36 */ 0,     // OP_GREATER
    /* 37 */ 0,     // OP_GREATER_EQUAL
    /* 38 */ 1,     // OP_JUMP
    /* 39 */ 1,     // OP_JUMP_IF_FALSE
    /* 40 */ 1,     // OP_JUMP_IF_TRUE
    /* 41 */ 1,     // OP_JUMP_IF_FALSE_KEEP
    /* 42 */ 1,     // OP_JUMP_IF_TRUE_KEEP
    /* 43 */ 1,     // OP_LOOP
    /* 44 */ 1,     // OP_CALL
    /* 45 */ 1,     // OP_CLOSURE
    /* 46 */ 0,     // OP_RETURN
    /* 47 */ 0,     // OP_RETURN_NULL
    /* 48 */ 0,     // OP_PRINT
    /* 49 */ 0,     // OP_INPUT
    /* 50 */ 1,     // OP_CAST
    /* 51 */ 0,     // OP_SWAP
    /* 52 */ 1,     // OP_EVAL_AST
    /* 53 */ 0,     // OP_RAISE
    /* 54 */ 1,     // OP_TRY_BEGIN
    /* 55 */ 0,     // OP_TRY_END
    /* 56 */ 0,     // OP_CATCH
    /* 57 */ 0,     // OP_HALT
    /* 58 */ 1,     // OP_INPLACE_ADD
    /* 59 */ 1,     // OP_INPLACE_SUB
    /* 60 */ 1,     // OP_INPLACE_MUL
    /* 61 */ 4,     // OP_CMP_JUMP
    /* 62 */ 2,     // OP_INPLACE_AM
    /* 63 */ 1,     // OP_DEF_STRUCT
    /* 64 */ 2,     // OP_CALL_MEMBER
    /* 65 */ 0,     // OP_RESERVED_2
    /* 66 */ 0,     // OP_RESERVED_3
    /* 67 */ 0,     // OP_RESERVED_4
    /* 68 */ 0,     // OP_RESERVED_5
    /* 69 */ 0,     // OP_RESERVED_6
    /* 70 */ 0,     // OP_RESERVED_7
    /* 71 */ 0,     // OP_RESERVED_8
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
    /* 58 */ true ,  // OP_INPLACE_ADD
    /* 59 */ true ,  // OP_INPLACE_SUB
    /* 60 */ true ,  // OP_INPLACE_MUL
    /* 61 */ true ,  // OP_CMP_JUMP
    /* 62 */ true ,  // OP_INPLACE_AM
    /* 63 */ false,  // OP_DEF_STRUCT
    /* 64 */ true ,  // OP_CALL_MEMBER
    /* 65 */ true ,  // OP_RESERVED_2
    /* 66 */ true ,  // OP_RESERVED_3
    /* 67 */ true ,  // OP_RESERVED_4
    /* 68 */ true ,  // OP_RESERVED_5
    /* 69 */ true ,  // OP_RESERVED_6
    /* 70 */ true ,  // OP_RESERVED_7
    /* 71 */ true ,  // OP_RESERVED_8
};

// Human-readable reason an opcode is unsupported, shown to the author when
// the JSON exporter refuses a program. Empty when the opcode is supported.
inline const char* opcode_js_unsupported_reason(OpCode op) {
    switch (op) {
        case OpCode::OP_EVAL_AST: return "ins classes, using, await and structs with expression defaults";
        case OpCode::OP_TRY_BEGIN: return "try/catch";
        case OpCode::OP_TRY_END: return "try/catch";
        case OpCode::OP_CATCH: return "try/catch";
        case OpCode::OP_INPLACE_ADD: return "in-place fused assignment";
        case OpCode::OP_INPLACE_SUB: return "in-place fused assignment";
        case OpCode::OP_INPLACE_MUL: return "in-place fused assignment";
        case OpCode::OP_CMP_JUMP: return "fused compare-and-jump";
        case OpCode::OP_INPLACE_AM: return "fused accumulate-multiply";
        case OpCode::OP_CALL_MEMBER: return "fused member call";
        case OpCode::OP_RESERVED_2: return "reserved opcode";
        case OpCode::OP_RESERVED_3: return "reserved opcode";
        case OpCode::OP_RESERVED_4: return "reserved opcode";
        case OpCode::OP_RESERVED_5: return "reserved opcode";
        case OpCode::OP_RESERVED_6: return "reserved opcode";
        case OpCode::OP_RESERVED_7: return "reserved opcode";
        case OpCode::OP_RESERVED_8: return "reserved opcode";
    }
    return "";
}
