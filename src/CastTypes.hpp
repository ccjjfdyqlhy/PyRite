#pragma once
#include <cstdint>
#include "Tokenizer.hpp"

// ============================================================================
// PyRite cast type codes -- the operand of the cast instruction.
// ============================================================================
// GENERATED FILE -- DO NOT EDIT BY HAND.
// Source: opcodes.def (sha256 d957ad6127a728b2fd13fd22422a4dc821fc06a6754b1374dce5092345df8a03)
// Regenerate: python3 tools/gen_opcodes.py
//
// These codes are STABLE. They are deliberately not TokenType values:
// TokenType is a tokenizer implementation detail whose numbering shifts
// whenever a keyword is added, which would silently change the meaning of
// every already-compiled .prc file.
//
// The compiler emits one of these codes as the operand of the cast instruction; the VM
// (C++ and JavaScript alike) switches on it.
// ============================================================================

enum class CastType : uint8_t {
    CAST_DEC  = 0,   // number
    CAST_STR  = 1,   // string
    CAST_BIN  = 2,   // binary
    CAST_LN   = 3,   // list
    CAST_DIM  = 4,   // dict
    CAST_ANY  = 5,   // any (no conversion)
};

// Total number of cast type codes.
constexpr int CAST_TYPE_COUNT = 6;

// Map a declaration/cast keyword to its stable cast code.
// Returns false when the token is not a type keyword.
inline bool cast_code_for(TokenType t, uint32_t& out) {
    switch (t) {
        case TokenType::DEC: out = 0; return true;
        case TokenType::STR: out = 1; return true;
        case TokenType::BIN: out = 2; return true;
        case TokenType::LN: out = 3; return true;
        case TokenType::DIM: out = 4; return true;
        case TokenType::ANY: out = 5; return true;
    }
    return false;
}
