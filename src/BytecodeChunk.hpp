#pragma once
#include <vector>
#include <string>
#include <memory>
#include <cstdint>
#include <map>
#include "Value.hpp"
#include "Ast.hpp"
#include "Tokenizer.hpp"
#include "OpCode.hpp"

// ============================================================================
// Upvalue descriptor: tells the VM how to capture a closure variable.
//   is_local == true  -> capture from the enclosing frame's locals[index]
//   is_local == false -> capture from the enclosing closure's upvalues[index]
// ============================================================================
struct UpvalueDesc {
    bool is_local;
    int index;
};

// ============================================================================
// CompiledFunction: a function compiled to bytecode.
// ============================================================================
struct CompiledFunction {
    std::string name;
    std::vector<uint8_t> code;          // bytecode
    std::vector<ValuePtr> constants;    // constant pool (literals + global names as StringValue)
    std::vector<int> lines;             // line number per byte (parallel to code)
    std::vector<AstNodePtr> ast_nodes;  // side-table for OP_EVAL_AST
    int arity;                          // number of required params
    int num_params;                     // total params (incl. defaults)
    int upvalue_count;
    int max_locals;                     // peak local count (for VM frame sizing)
    std::vector<UpvalueDesc> upvalue_descriptors;  // how to capture each upvalue
    // Parameter metadata (for default values & type checks at call time)
    struct ParamInfo {
        TokenType type_keyword;
        std::string name;
        ValuePtr default_value;      // cached literal default (or null)
        AstNodePtr default_expr;     // expression default (or null)
        bool has_default;
    };
    std::vector<ParamInfo> params;
    std::vector<std::string> local_names;  // local_names[slot] = name (for OP_EVAL_AST bridging)
    bool is_method;       // true if `this` should be bound
    bool is_lambda;

    CompiledFunction() : arity(0), num_params(0), upvalue_count(0), max_locals(0), is_method(false), is_lambda(false) {}
};
using CompiledFunctionPtr = std::shared_ptr<CompiledFunction>;

// ============================================================================
// Upvalue: a captured variable from an enclosing function's stack frame.
//   - If is_local == true, the upvalue points to a slot in the *enclosing*
//     call frame's locals (by index).
//   - If is_local == false, the upvalue points to another upvalue (by index)
//     of the enclosing closure.
// (UpvalueDesc is defined above, near the top of this file.)
// ============================================================================

// ============================================================================
// UpvalueObj: runtime representation of an upvalue. Points either to a stack
// slot (when the enclosing frame is still alive) or, after the frame returns,
// holds the value itself (closed).
// ============================================================================
struct UpvalueObj {
    ValuePtr* location;   // points into a CallFrame's locals vector
    ValuePtr closed;      // value stored here once the owning frame returns
    std::shared_ptr<UpvalueObj> next;  // linked list of open upvalues

    UpvalueObj(ValuePtr* loc) : location(loc) {}
};
using UpvalueObjPtr = std::shared_ptr<UpvalueObj>;

// ============================================================================
// Closure: a CompiledFunction paired with its captured upvalues.
// ============================================================================
struct Closure : public Value {
    CompiledFunctionPtr function;
    std::vector<UpvalueObjPtr> upvalues;

    Closure(CompiledFunctionPtr fn) : function(fn) {
        upvalues.resize(fn->upvalue_count);
    }
    std::string toString() const override {
        return "<closure " + function->name + ">";
    }
    std::string repr() const override { return toString(); }
    bool isTruthy() const override { return true; }
    ValuePtr clone() const override {
        // Closures are not cloned; share the same closure object.
        return std::make_shared<Closure>(*this);
    }
};
using ClosurePtr = std::shared_ptr<Closure>;

// ============================================================================
// BytecodeChunk: top-level container for a compiled program. Holds the
// "script" CompiledFunction plus a registry of all functions compiled.
// ============================================================================
struct BytecodeChunk {
    CompiledFunctionPtr script;        // entry-point function
    std::vector<CompiledFunctionPtr> all_functions;  // for debugging/dump

    BytecodeChunk() : script(std::make_shared<CompiledFunction>()) {
        script->name = "<script>";
        all_functions.push_back(script);
    }
};
using BytecodeChunkPtr = std::shared_ptr<BytecodeChunk>;

// ============================================================================
// Helpers for reading/writing 24-bit operands in bytecode.
// ============================================================================
inline void emit_byte(std::vector<uint8_t>& code, uint8_t b) { code.push_back(b); }
inline void emit_op(std::vector<uint8_t>& code, OpCode op) { code.push_back(static_cast<uint8_t>(op)); }

inline void emit_operand24(std::vector<uint8_t>& code, uint32_t val) {
    code.push_back(static_cast<uint8_t>((val >> 16) & 0xFF));
    code.push_back(static_cast<uint8_t>((val >> 8) & 0xFF));
    code.push_back(static_cast<uint8_t>(val & 0xFF));
}

inline uint32_t read_operand24(const uint8_t* p) {
    return (static_cast<uint32_t>(p[0]) << 16)
         | (static_cast<uint32_t>(p[1]) << 8)
         |  static_cast<uint32_t>(p[2]);
}

// Patch a 24-bit operand in-place at a given code offset.
inline void patch_operand24(std::vector<uint8_t>& code, size_t offset, uint32_t val) {
    code[offset]     = static_cast<uint8_t>((val >> 16) & 0xFF);
    code[offset + 1] = static_cast<uint8_t>((val >> 8) & 0xFF);
    code[offset + 2] = static_cast<uint8_t>(val & 0xFF);
}
