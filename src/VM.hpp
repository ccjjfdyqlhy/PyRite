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
        Slot* locals;                // into the arena; stable (pre-sized)
        uint32_t nlocals;
        size_t locals_base;          // arena index, for unwinding
        std::shared_ptr<UpvalueObj>* upvalues;   // into the closure; null = none
        uint32_t upvalue_count;
        // Where this frame's result goes in the CALLER's frame: a local
        // slot, or NO_DEST to leave it on the operand stack.
        uint32_t ret_dest;
    };

    static constexpr int FRAMES_MAX = 1000;   // == Interpreter::MAX_CALL_DEPTH
    // The marker for "no register destination": the call result goes on
    // the operand stack as usual.
    static constexpr uint32_t NO_DEST = 0xFFFFFFFFu;
    CallFrame frames[FRAMES_MAX];
    int frame_top = 0;
    std::vector<Slot> arena;                 // all frames' locals
    size_t arena_top = 0;

    std::vector<Slot> stack;
    BytecodeChunkPtr chunk;
    std::shared_ptr<Environment> globals;
    Interpreter* interpreter;

    // Open upvalues, sorted by stack slot (descending).
    std::vector<std::shared_ptr<UpvalueObj>> open_upvalues;

    // Interned small integers: loop counters and small results come back
    // here instead of a fresh allocation. Numbers are immutable, so the
    // cached objects can never go stale.
    static ValuePtr intern_small(long long v);
    static Value* intern_small_raw(long long v);   // borrow, no refcount
    // Range-checked front end: [-4096, 4095] plus a wider positive band
    // (list lengths and similar monotone results live here); anything
    // else allocates.
    static ValuePtr intern_num(long long v);

    // binary op on two stack values, reusing a uniquely-owned left side
    // instead of allocating a fresh result object.
    ValuePtr binary_op_ip(OpCode op, ValuePtr&& a, ValuePtr&& b, int line);

    // ---- Stack helpers ----
    // The stack holds slots: a boxed value or a raw machine integer.  The
    // integer form is what keeps arithmetic off the heap; the boxed form is
    // what every non-numeric operation sees.
    void push(const ValuePtr& v) { stack.push_back(Slot(v)); }
    void push_int(int64_t v) { stack.push_back(Slot::of_int(v)); }
    void push_slot(Slot s) { stack.push_back(std::move(s)); }
    // Pops the raw slot: an integer stays an integer.
    Slot pop_slot() { Slot s = std::move(stack.back()); stack.pop_back(); return s; }
    // Pops the boxed form: an integer becomes a NumberValue here.  A boxed
    // value is MOVED out, not copied - the reference count stays where it
    // was, exactly as the plain ValuePtr stack did.
    ValuePtr pop() {
        Slot s = pop_slot();
        if (s.kind == Slot::REF) return std::move(s.ref);
        return s.to_value();
    }
    // A peek past the bottom of the stack yields an empty slot instead of
    // reading out of bounds: a malformed sequence must not take the
    // process down.
    Slot peek_slot(int distance = 0) {
        if (distance < 0 || (size_t)distance >= stack.size()) return Slot();
        return stack[stack.size() - 1 - distance];
    }
    ValuePtr peek(int distance = 0) { return peek_slot(distance).to_value(); }

    // ---- Frame helpers ----
    CallFrame& frame() { return frames[frame_top - 1]; }
    CompiledFunction* current_function() { return frames[frame_top - 1].function; }

    // ---- Upvalue management ----
    std::shared_ptr<UpvalueObj> capture_upvalue(int slot);
    void close_upvalues(int slot);

    // ---- OP_EVAL_AST bridging ----
    ValuePtr eval_ast(const AstNodePtr& node);

    // A borrowed view of an operand side: either a raw machine integer or
    // a boxed value.  Both outlive the instruction that borrows them, so
    // the comparison paths need no reference traffic at all.
    struct Side {
        const Value* val = nullptr;
        int64_t num = 0;
        bool is_int = false;
    };

    // ---- Call dispatch ----
    // Calls `callee` with `args`. If the callee is a VM closure, pushes a new
    // frame and returns true (caller should return to the dispatch loop). If
    // the callee is native/AST, executes it synchronously and pushes the result.
    bool call_value(ValuePtr callee, const std::vector<ValuePtr>& args, int line);

    // Set up a new call frame for a VM closure.
    void push_frame(Closure* closure, const Slot* args, size_t nargs, int line);
    void push_frame_from_stack(Closure* closure, size_t nargs, int line);
    // Same, but the arguments come from an explicit array (register calls).
    void push_frame_from_slots(Closure* closure, const Slot* args, size_t nargs, int line);

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
    // Truthiness of a slot: a raw integer is true when it is not zero, and
    // needs no boxing to find out.
    static bool slot_truthy(const Slot& s) {
        if (s.is_int()) return s.int_value() != 0;
        return is_truthy(s.to_value());
    }

    // ---- Arithmetic helpers ----
    ValuePtr binary_op(OpCode op, const ValuePtr& a, const ValuePtr& b, int line);
    // OP_INPLACE_* fast path; false means "not applicable, run the plain
    // [load rhs operand] sequence instead".
    bool exec_inplace(OpCode op, uint32_t packed, struct CallFrame& frame, int line);
    // Cold opcode handlers, kept out of the dispatch loop (see VM.cpp).
    bool exec_def_class();
    bool exec_call_ctor();
    void resolve_side(struct CallFrame& frame, uint32_t enc, ValuePtr& out);
    // Borrow-only read (no refcount traffic) for read-only instructions.
    // The side is returned by value: the compiler keeps it in registers.
    Side resolve_side_raw(struct CallFrame& frame, uint32_t enc);
    // Cached global-cell access for fused instructions (idx = name
    // constant index). The cell pointer is remembered per call site.
    ValuePtr* global_write_cell(struct CallFrame& frame, uint32_t idx);
    const ValuePtr* global_read_cell(struct CallFrame& frame, uint32_t idx);
    ValuePtr unary_op(OpCode op, const ValuePtr& a, int line);
};
