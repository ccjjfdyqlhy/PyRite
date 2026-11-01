#pragma once
#include <string>
#include <vector>
#include <memory>
#include <map>
#include "Ast.hpp"
#include "Tokenizer.hpp"
#include "Value.hpp"
#include "BytecodeChunk.hpp"

// ============================================================================
// Compiler: walks the AST and emits bytecode into a BytecodeChunk.
//
// Design:
//   - Each function body (including the top-level script) is compiled into
//     its own CompiledFunction with its own constant pool and locals vector.
//   - Locals are stack slots. The compiler tracks a `locals` table mapping
//     variable names to slot indices, plus a `scope_depth` for block scoping.
//   - Variables that cannot be resolved as locals (because they belong to an
//     enclosing function) are resolved as upvalues via resolve_upvalue().
//   - Variables not resolved as locals or upvalues become globals.
//   - Closures are created at runtime via OP_CLOSURE, which captures the
//     upvalues described by the CompiledFunction's upvalue descriptors.
//
// Hybrid execution:
//   Nearly everything compiles to bytecode, including try/catch
//   (OP_TRY_BEGIN/END/CATCH), raise (OP_RAISE), for-in, struct
//   declarations (OP_DEF_STRUCT) and inlined require modules. The few
//   remaining constructs (ins classes, using, await, and structs whose
//   field defaults are expressions) are emitted as OP_EVAL_AST, which
//   delegates to the AST interpreter at runtime. This keeps the VM
//   implementation tractable while still delivering bytecode speed for
//   the hot paths (arithmetic, loops, function calls).
// ============================================================================

class Compiler {
public:
    // Fused superinstructions (OP_INPLACE_*, OP_CMP_JUMP, OP_INPLACE_AM)
    // have no JavaScript implementation yet; the JSON exporter recompiles
    // with this off so those programs still export.
    bool allow_fusion = true;

    Compiler();
    // The source path matters: a require is resolved relative to the file
    // that asks for it, so the compiler has to know where that file is.
    BytecodeChunkPtr compile(const std::vector<AstNodePtr>& statements,
                             const std::string& source_path = "");
    bool has_error() const { return had_error; }
    const std::string& get_error() const { return error_msg; }

private:
    struct LocalVar {
        std::string name;
        int depth;
        bool is_captured;
    };
    // UpvalueDesc is defined in BytecodeChunk.hpp (shared with CompiledFunction).
    struct FuncState {
        CompiledFunctionPtr func;
        std::vector<LocalVar> locals;
        std::vector<UpvalueDesc> upvalues;
        int scope_depth;
        // Loop bookkeeping: loop_starts tracks the IP to jump back to for the
        // innermost loop; break_patches_ptr / continue_patches_ptr point to a
        // vector of jump offsets that must be patched when the loop ends.
        std::vector<size_t> loop_starts;
        std::vector<size_t>* break_patches_ptr;
        std::vector<size_t>* continue_patches_ptr;
        // Try handlers alive at this point of the function, so break /
        // continue / return know how many OP_TRY_END to emit on their way
        // out (a jump would otherwise skip them and leave stale handlers).
        int try_depth;
        // Register-scratch slots: temporaries for fused arithmetic and
        // call arguments. They live past the declared locals; the count
        // is folded into max_locals when the function ends.
        int scratch_used;
        // Try depth recorded when each enclosing loop started; a break or
        // continue only pops the handlers nested inside its own loop.
        std::vector<int> loop_entry_try_depths;
        FuncState() : scope_depth(0), break_patches_ptr(nullptr), continue_patches_ptr(nullptr), try_depth(0), scratch_used(0) {}
    };

    std::vector<FuncState> func_stack;
    FuncState* current;
    BytecodeChunkPtr chunk_root;
    bool had_error;

    std::string error_msg;
    int current_line;  // tracks the source line for bytecode line info

    // > 0 while compiling the literal tree of a `return [...]` / `return {...}`.
    // String literals inside such a wholesale-returned display literal are
    // shown to the player, so they go through add_display_constant and the
    // packaging step can watermark them. Keys and directive names stay OUT
    // of returned literals (the data is positional) or they get marked too.
    int display_literal_depth_ = 0;

    // ---- Constant pool helpers ----
    uint32_t add_constant(ValuePtr value);
    uint32_t add_display_constant(ValuePtr value);
    uint32_t add_string_constant(const std::string& s);
    uint32_t add_ast_node(AstNodePtr node);
    uint32_t add_ast_node_raw(AstNode* node);  // non-owning wrapper

    // ---- Tables ----
    // A table of literals becomes ONE constant: the rows stay in the
    // constant pool as references, so the runtime can open a row when it
    // is read and not before.  That is what keeps the plaintext of a
    // whole table out of the runtime memory.
    //
    // Returns the index of the table constant, or -1 when the list holds
    // something that is not a literal (then it is built the old way).
    int32_t table_constant(ListLiteralNode* n);

    // ---- Emission helpers ----
    void emit_op(OpCode op);
    void emit_op_operand(OpCode op, uint32_t operand);
    void emit_byte(uint8_t b);
    size_t emit_jump(OpCode op);          // emits op + placeholder operand, returns offset of operand
    void patch_jump(size_t offset);       // patches the operand at `offset` with current code size
    void emit_loop(size_t loop_start);    // emits OP_LOOP back to loop_start

    // ---- Scope management ----
    void begin_scope();
    void end_scope();
    void declare_local(const std::string& name);
    int resolve_local(const std::string& name);
    int add_upvalue(FuncState* func, bool is_local, int index);
    int resolve_upvalue(FuncState* func, const std::string& name);

    // ---- Variable load/store ----
    void load_variable(const std::string& name, int line);
    void store_variable(const std::string& name, int line);

    // ---- Modules ----
    //
    // A require is resolved and inlined at compile time: the module
    // becomes a function of the program and its exports become a
    // dictionary the alias points at.  The shipped program stays one
    // file, so the browser needs no file system and the module's code
    // stays inside the encrypted pool.
    std::string source_path_;      // the file being compiled
    std::string source_dir_;       // its directory
    std::vector<std::string> module_stack_;   // the modules being compiled
    int module_counter_ = 0;

    void compile_require(RequireNode* n);
    void compile_module(const std::string& resolved, bool is_dir_module,
                        const std::vector<AstNodePtr>& statements,
                        const std::string& alias);

    // ---- Top-level dispatch ----
    void compile_block(const std::vector<AstNodePtr>& statements);
    void compile_statement(const AstNodePtr& node);
    void compile_expression(const AstNodePtr& node);

    // ---- Statement compilers ----
    void compile_var_declaration(VarDeclarationNode* n);
    void compile_if(IfStatementNode* n);
    void compile_while(WhileStatementNode* n);
    void compile_loop_for(LoopForNode* n);
    void compile_for_in(ForInNode* n);
    void compile_loop_until(LoopUntilNode* n);
    void compile_break(BreakNode* n);
    void compile_continue(ContinueNode* n);
    void compile_say(SayNode* n);
    void compile_inp(InpNode* n);
    void compile_fn_def(FnDefNode* n);
    void compile_struct_def(const AstNodePtr& node, StructDefNode* n);
    void compile_class_def(const AstNodePtr& node, ClassDefNode* n);
    void compile_return(ReturnNode* n);
    void compile_raise(RaiseNode* n);
    void compile_try_catch(TryCatchNode* n);
    void compile_expression_statement(ExpressionStatementNode* n);

    // ---- Expression compilers ----
    void compile_literal(LiteralNode* n);
    void compile_list_literal(ListLiteralNode* n);
    void compile_dim_literal(DimLiteralNode* n);
    void compile_variable(VariableNode* n);
    void compile_unary(UnaryOpNode* n);
    void compile_binary(BinaryOpNode* n);
    void compile_logical(LogicalOpNode* n);
    void compile_typecast(TypeConversionNode* n);
    void compile_assignment(AssignmentNode* n);
    // Fuses `name = name <op> rhs` into one OP_INPLACE_* when name is a
    // local and rhs is a variable, a small literal, or a local*literal.
    bool try_compile_inplace(const std::string& name, AstNode* value, int line);
    // Same fusion, but the result stays off the stack: statement-level
    // assignments whose value nobody reads.
    bool try_compile_inplace_np(const std::string& name, AstNode* value);
    bool try_compile_inplace_common(const std::string& name, AstNode* value, int line, bool no_push);
    // ---- register-arithmetic fusion ----
    // One side of an _R instruction: bit23 global (12-bit name index),
    // kind bits 13-12 (0 local, 1 inline int, 2 global), payload 11-0.
    bool encode_r_side(AstNode* node, uint32_t& out);
    // Emits `slot = a <op> b` for encodable sides; false = not fusable.
    bool compile_r_bin(uint32_t dst_slot, bool dst_global, TokenType op,
                       AstNode* a, AstNode* b);
    int push_scratch();
    void pop_scratch();
    // Fuses `while lhs <op> rhs` into OP_CMP_JUMP; emits the jump with a
    // placeholder target and returns the patch location, or -1 when the
    // condition does not fit the pattern.
    int try_compile_cmp_jump(AstNode* condition);
    // One side of a fused instruction: bit23 global (name constant index
    // in bits 22-12), else local slot; bits 11-9 kind, bits 8-0 payload.
    bool encode_value_side(AstNode* node, bool allow_inline, uint32_t& out);
    // s = s + i * k as one instruction.
    bool try_compile_inplace_am(const std::string& name, AstNode* value);
    void compile_lambda(LambdaNode* n);
    void compile_call(CallNode* n);
    void compile_subscript(SubscriptNode* n);
    void compile_get(GetNode* n);

    // ---- Function body compilation (shared by fn def and lambda) ----
    void compile_function_body(const std::string& name,
                               const std::vector<ParameterDefinition>& params,
                               const std::vector<AstNodePtr>& body,
                               bool is_lambda,
                               bool is_method = false);

    // ---- Error ----
    void error(const std::string& msg);
};
