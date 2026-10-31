#pragma once
#include <vector>
#include <memory>
#include <map>
#include <string>
#include "Value.hpp"
#include "BytecodeChunk.hpp"
#include "Environment.hpp"
#include "OpCode.hpp"
#include <unordered_map>

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
    // Frames live in a preallocated array; their locals share one arena.
    // A call is a handful of pointer writes, a return rolls the arena
    // top back - no heap traffic anywhere on the call path.
    struct CallFrame {
        CompiledFunction* function;
        const uint8_t* ip;
        const uint8_t* code_start;
        ValuePtr* locals;            // into the arena; stable (pre-sized)
        uint32_t nlocals;
        size_t locals_base;          // arena index, for unwinding
        std::shared_ptr<UpvalueObj>* upvalues;   // into the closure; null = none
        uint32_t upvalue_count;
    };

    static constexpr int FRAMES_MAX = 1000;   // == Interpreter::MAX_CALL_DEPTH
    CallFrame frames[FRAMES_MAX];
    int frame_top = 0;
    std::vector<ValuePtr> arena;             // all frames' locals
    size_t arena_top = 0;

    std::vector<ValuePtr> stack;
    BytecodeChunkPtr chunk;
    std::shared_ptr<Environment> globals;
    Interpreter* interpreter;

    // Open upvalues, sorted by stack slot (descending).
    std::vector<std::shared_ptr<UpvalueObj>> open_upvalues;

    // ---- Stack helpers ----
    void push(ValuePtr v) { stack.push_back(v); }
    ValuePtr pop() { ValuePtr v = std::move(stack.back()); stack.pop_back(); return v; }
    ValuePtr peek(int distance = 0) { return stack[stack.size() - 1 - distance]; }

    // ---- Frame helpers ----
    CallFrame& frame() { return frames[frame_top - 1]; }
    CompiledFunction* current_function() { return frames[frame_top - 1].function; }

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
    void push_frame(Closure* closure, const ValuePtr* args, size_t nargs, int line);
    void push_frame_from_stack(Closure* closure, size_t nargs, int line);
    // Same, but the arguments come from an explicit array (register calls).
    void push_frame_from_slots(Closure* closure, const ValuePtr* args, size_t nargs, int line);

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
    // OP_INPLACE_* fast path; false means "not applicable, run the plain
    // [load rhs operand] sequence instead".
    bool exec_inplace(OpCode op, uint32_t packed, struct CallFrame& frame, int line);
    void resolve_side(struct CallFrame& frame, uint32_t enc, ValuePtr& out);
    // Cached global-cell access for fused instructions (idx = name
    // constant index). The cell pointer is remembered per call site.
    ValuePtr* global_write_cell(struct CallFrame& frame, uint32_t idx);
    const ValuePtr* global_read_cell(struct CallFrame& frame, uint32_t idx);
    ValuePtr unary_op(OpCode op, const ValuePtr& a, int line);
};
