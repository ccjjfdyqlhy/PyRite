#pragma once
#include <vector>
#include <memory>
#include <map>
#include <string>
#include "Value.hpp"
#include "BytecodeChunk.hpp"
#include "Environment.hpp"
#include "OpCode.hpp"

class Interpreter;

// ============================================================================
// VM: a stack-based bytecode virtual machine for PyRite.
//
// The VM executes a BytecodeChunk produced by the Compiler. It maintains:
//   - A value stack for operands
//   - A call frame stack (each frame = closure + ip + locals array)
//   - A globals environment (shared with the AST interpreter)
//   - An open-upvalues list (for closures)
//
// Hybrid execution: OP_EVAL_AST delegates single AST nodes to the AST
// interpreter, bridging the VM's locals into a temporary Environment.
// This allows the VM to handle the full language while keeping the
// performance-critical paths (arithmetic, loops, calls) in bytecode.
// ============================================================================
class VM {
public:
    VM();

    // Set the interpreter to use for OP_EVAL_AST and native function calls.
    void set_interpreter(Interpreter* interp) { interpreter = interp; }

    // Set the globals environment (shared with the interpreter).
    void set_globals(std::shared_ptr<Environment> g) { globals = g; }

    // Execute a compiled chunk. Returns true on success.
    bool run(BytecodeChunkPtr chunk);

    // Disassemble a chunk (for debugging with --dump-bytecode).
    void disassemble(BytecodeChunkPtr chunk);

    bool had_error;
    std::string error_message;

private:
    // ---- Call frame ----
    struct CallFrame {
        CompiledFunctionPtr function;
        const uint8_t* ip;
        const uint8_t* code_start;
        std::vector<ValuePtr> locals;
        // Upvalues captured by the closure running in this frame.
        std::vector<std::shared_ptr<UpvalueObj>> upvalues;
    };

    std::vector<CallFrame> frames;
    std::vector<ValuePtr> stack;
    BytecodeChunkPtr chunk;
    std::shared_ptr<Environment> globals;
    Interpreter* interpreter;

    // Open upvalues, sorted by stack slot (descending).
    std::vector<std::shared_ptr<UpvalueObj>> open_upvalues;

    // ---- Stack helpers ----
    void push(ValuePtr v) { stack.push_back(v); }
    ValuePtr pop() { ValuePtr v = stack.back(); stack.pop_back(); return v; }
    ValuePtr peek(int distance = 0) { return stack[stack.size() - 1 - distance]; }

    // ---- Frame helpers ----
    CallFrame& frame() { return frames.back(); }
    CompiledFunctionPtr current_function() { return frames.back().function; }

    // ---- Upvalue management ----
    std::shared_ptr<UpvalueObj> capture_upvalue(int slot);
    void close_upvalues(int slot);

    // ---- OP_EVAL_AST bridging ----
    ValuePtr eval_ast(const AstNodePtr& node);

    // ---- Call dispatch ----
    // Calls `callee` with `args`. If the callee is a VM closure, pushes a new
    // frame and returns true (caller should return to the dispatch loop). If
    // the callee is native/AST, executes it synchronously and pushes the result.
    bool call_value(ValuePtr callee, const std::vector<ValuePtr>& args, int line);

    // Set up a new call frame for a VM closure.
    void push_frame(ClosurePtr closure, const std::vector<ValuePtr>& args, int line);

    // ---- Error ----
    void runtime_error(int line, const std::string& msg);  // throws RuntimeError
    void set_error(int line, const std::string& msg);      // sets flag only (no throw)
    int current_line();

    // ---- Exception handling ----
    struct TryHandler {
        size_t frame_depth;      // number of frames when try was entered
        size_t stack_depth;      // value stack depth when try was entered
        const uint8_t* catch_ip; // IP to jump to on exception (in handler's frame)
    };
    std::vector<TryHandler> try_handlers;
    bool handle_exception(ValuePtr exc_value);  // returns true if handled

    // ---- Truthiness ----
    static bool is_truthy(const ValuePtr& v);

    // ---- Arithmetic helpers ----
    ValuePtr binary_op(OpCode op, const ValuePtr& a, const ValuePtr& b, int line);
    ValuePtr unary_op(OpCode op, const ValuePtr& a, int line);
};
