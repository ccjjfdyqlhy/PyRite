#pragma once
#include <cstdint>
#include <string>

// ============================================================================
// PyRite Bytecode Instruction Set
// ============================================================================
// Stack-based VM. Operands are big-endian 24-bit unsigned integers (0..2^24-1)
// unless noted otherwise. Each instruction is 1 byte opcode + 3 bytes operand.
//
// Convention:
//   - "pop"  : remove and return top of value stack
//   - "peek" : read top without removing
//   - "push" : place on top of value stack
// ============================================================================

enum class OpCode : uint8_t {
    // ---- Constants ----
    OP_CONSTANT,       // [idx] push constants[idx]
    OP_NULL,           // push NullValue
    OP_TRUE,           // push NumberValue(1)
    OP_FALSE,          // push NumberValue(0)

    // ---- Stack manipulation ----
    OP_POP,            // pop & discard
    OP_DUP,            // duplicate top

    // ---- Local variables (stack slot indexed) ----
    OP_GET_LOCAL,      // [idx] push locals[idx]
    OP_SET_LOCAL,      // [idx] locals[idx] = pop (keeps value on stack? no, pops)
    OP_SET_LOCAL_KEEP, // [idx] locals[idx] = peek (keeps value on stack)

    // ---- Global variables (name indexed in constant pool) ----
    OP_GET_GLOBAL,     // [name_idx] push globals[name]
    OP_SET_GLOBAL,     // [name_idx] globals[name] = pop
    OP_DEFINE_GLOBAL,  // [name_idx] globals[name] = pop

    // ---- Upvalues (closures) ----
    OP_GET_UPVALUE,    // [idx] push upvalues[idx]->value
    OP_SET_UPVALUE,    // [idx] upvalues[idx]->value = pop
    OP_CLOSE_UPVALUE,  // pop; close the upvalue pointing to this slot

    // ---- Collections ----
    OP_GET_INDEX,      // pop index, pop obj; push obj[index]
    OP_SET_INDEX,      // pop val, pop index, pop obj; obj[index] = val
    OP_GET_PROPERTY,   // [name_idx] pop obj; push obj.name
    OP_SET_PROPERTY,   // [name_idx] pop val, pop obj; obj.name = val
    OP_BUILD_LIST,     // [n] pop n values; push LnValue
    OP_BUILD_DICT,     // [n] pop n key-value pairs; push DimValue
    OP_GET_LEN,        // pop obj; push len(obj)
    OP_GET_SLICE,      // pop step, end, start, obj; push obj[start:end:step]
    OP_SET_SLICE,      // pop val, step, end, start, obj; obj[start:end:step] = val

    // ---- Arithmetic ----
    OP_ADD, OP_SUB, OP_MUL, OP_DIV, OP_MOD, OP_POW,
    OP_NEGATE,         // pop a; push -a
    OP_NOT,            // pop a; push (a truthy ? 0 : 1)

    // ---- Comparison ----
    OP_EQUAL,          // pop b, a; push (a == b ? 1 : 0)
    OP_NOT_EQUAL,
    OP_LESS,
    OP_LESS_EQUAL,
    OP_GREATER,
    OP_GREATER_EQUAL,

    // ---- Control flow (24-bit jump target = absolute IP) ----
    OP_JUMP,           // [target] ip = target
    OP_JUMP_IF_FALSE,  // [target] pop; if falsy ip = target
    OP_JUMP_IF_TRUE,   // [target] pop; if truthy ip = target
    OP_JUMP_IF_FALSE_KEEP, // [target] if falsy ip = target (does not pop)
    OP_JUMP_IF_TRUE_KEEP,  // [target] if truthy ip = target (does not pop)
    OP_LOOP,           // [target] ip = target (backward jump)

    // ---- Logical short-circuit (keep left value) ----
    // (handled via OP_JUMP_IF_FALSE_KEEP / OP_JUMP_IF_TRUE_KEEP)

    // ---- Functions ----
    OP_CALL,           // [n] pop n args; pop callee; push result
    OP_CLOSURE,        // [fn_idx] build Closure from CompiledFunction
    OP_RETURN,         // pop return value; restore frame; push value to caller
    OP_RETURN_NULL,    // push null then return (no pop)

    // ---- I/O ----
    OP_PRINT,          // pop; print + newline
    OP_INPUT,          // pop prompt; read line; push string

    // ---- Type conversion (as) ----
    OP_CAST,           // [type_token] pop; push converted value

    // ---- Compound assignment helpers ----
    // For `a += b` we emit: GET a, EVAL b, ADD, SET a

    // ---- Swap ----
    OP_SWAP,           // pop b, pop a; push b, push a (then caller sets back)

    // ---- AST fallback (hybrid execution) ----
    // For features not yet bytecode-compiled (try-catch, classes, modules,
    // await, raise, using, require), the compiler emits OP_EVAL_AST with an
    // index into a side-table of AstNodePtr stored on the chunk. The VM
    // delegates to the AST interpreter for that single node.
    OP_EVAL_AST,       // [ast_idx] evaluate ast_nodes[idx] via interpreter

    // ---- Exception handling (native) ----
    OP_RAISE,          // pop value; throw PyRiteRaiseException(value)
    OP_TRY_BEGIN,      // [catch_target] push try-handler with catch jump target
    OP_TRY_END,        // pop try-handler (try block completed without exception)
    OP_CATCH,          // (no operand) catch handler entry; exception value already on stack

    // ---- Halt ----
    OP_HALT,           // stop the VM
};

// Convert opcode to string for disassembly / debugging
inline const char* op_name(OpCode op) {
    switch (op) {
        case OpCode::OP_CONSTANT:         return "CONSTANT";
        case OpCode::OP_NULL:             return "NULL";
        case OpCode::OP_TRUE:             return "TRUE";
        case OpCode::OP_FALSE:            return "FALSE";
        case OpCode::OP_POP:              return "POP";
        case OpCode::OP_DUP:              return "DUP";
        case OpCode::OP_GET_LOCAL:        return "GET_LOCAL";
        case OpCode::OP_SET_LOCAL:        return "SET_LOCAL";
        case OpCode::OP_SET_LOCAL_KEEP:   return "SET_LOCAL_KEEP";
        case OpCode::OP_GET_GLOBAL:       return "GET_GLOBAL";
        case OpCode::OP_SET_GLOBAL:       return "SET_GLOBAL";
        case OpCode::OP_DEFINE_GLOBAL:    return "DEFINE_GLOBAL";
        case OpCode::OP_GET_UPVALUE:      return "GET_UPVALUE";
        case OpCode::OP_SET_UPVALUE:      return "SET_UPVALUE";
        case OpCode::OP_CLOSE_UPVALUE:    return "CLOSE_UPVALUE";
        case OpCode::OP_GET_INDEX:        return "GET_INDEX";
        case OpCode::OP_SET_INDEX:        return "SET_INDEX";
        case OpCode::OP_GET_PROPERTY:     return "GET_PROPERTY";
        case OpCode::OP_SET_PROPERTY:     return "SET_PROPERTY";
        case OpCode::OP_BUILD_LIST:       return "BUILD_LIST";
        case OpCode::OP_BUILD_DICT:       return "BUILD_DICT";
        case OpCode::OP_GET_LEN:          return "GET_LEN";
        case OpCode::OP_GET_SLICE:        return "GET_SLICE";
        case OpCode::OP_SET_SLICE:        return "SET_SLICE";
        case OpCode::OP_ADD:              return "ADD";
        case OpCode::OP_SUB:              return "SUB";
        case OpCode::OP_MUL:              return "MUL";
        case OpCode::OP_DIV:              return "DIV";
        case OpCode::OP_MOD:              return "MOD";
        case OpCode::OP_POW:              return "POW";
        case OpCode::OP_NEGATE:           return "NEGATE";
        case OpCode::OP_NOT:              return "NOT";
        case OpCode::OP_EQUAL:            return "EQUAL";
        case OpCode::OP_NOT_EQUAL:        return "NOT_EQUAL";
        case OpCode::OP_LESS:             return "LESS";
        case OpCode::OP_LESS_EQUAL:       return "LESS_EQUAL";
        case OpCode::OP_GREATER:          return "GREATER";
        case OpCode::OP_GREATER_EQUAL:    return "GREATER_EQUAL";
        case OpCode::OP_JUMP:             return "JUMP";
        case OpCode::OP_JUMP_IF_FALSE:    return "JUMP_IF_FALSE";
        case OpCode::OP_JUMP_IF_TRUE:     return "JUMP_IF_TRUE";
        case OpCode::OP_JUMP_IF_FALSE_KEEP: return "JUMP_IF_FALSE_KEEP";
        case OpCode::OP_JUMP_IF_TRUE_KEEP:  return "JUMP_IF_TRUE_KEEP";
        case OpCode::OP_LOOP:             return "LOOP";
        case OpCode::OP_CALL:             return "CALL";
        case OpCode::OP_CLOSURE:          return "CLOSURE";
        case OpCode::OP_RETURN:           return "RETURN";
        case OpCode::OP_RETURN_NULL:      return "RETURN_NULL";
        case OpCode::OP_PRINT:            return "PRINT";
        case OpCode::OP_INPUT:            return "INPUT";
        case OpCode::OP_CAST:             return "CAST";
        case OpCode::OP_SWAP:             return "SWAP";
        case OpCode::OP_EVAL_AST:         return "EVAL_AST";
        case OpCode::OP_RAISE:            return "RAISE";
        case OpCode::OP_TRY_BEGIN:        return "TRY_BEGIN";
        case OpCode::OP_TRY_END:          return "TRY_END";
        case OpCode::OP_CATCH:            return "CATCH";
        case OpCode::OP_HALT:             return "HALT";
    }
    return "UNKNOWN";
}
