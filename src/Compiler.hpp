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
//   AST node types that are too complex to bytecode-compile cleanly
//   (TryCatchNode, ClassDefNode, StructDefNode, RequireNode, UsingNode,
//    AwaitStatementNode, RaiseNode, SwapNode) are emitted as OP_EVAL_AST,
//   which delegates to the AST interpreter at runtime. This keeps the VM
//   implementation tractable while still delivering bytecode speed for the
//   hot paths (arithmetic, loops, function calls).
// ============================================================================

class Compiler {
public:
    Compiler();
    BytecodeChunkPtr compile(const std::vector<AstNodePtr>& statements);
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
        FuncState() : scope_depth(0), break_patches_ptr(nullptr), continue_patches_ptr(nullptr) {}
    };

    std::vector<FuncState> func_stack;
    FuncState* current;
    BytecodeChunkPtr chunk_root;
    bool had_error;
    std::string error_msg;
    int current_line;  // tracks the source line for bytecode line info

    // ---- Constant pool helpers ----
    uint32_t add_constant(ValuePtr value);
    uint32_t add_string_constant(const std::string& s);
    uint32_t add_ast_node(AstNodePtr node);
    uint32_t add_ast_node_raw(AstNode* node);  // non-owning wrapper

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
    void compile_lambda(LambdaNode* n);
    void compile_call(CallNode* n);
    void compile_subscript(SubscriptNode* n);
    void compile_get(GetNode* n);

    // ---- Function body compilation (shared by fn def and lambda) ----
    void compile_function_body(const std::string& name,
                               const std::vector<ParameterDefinition>& params,
                               const std::vector<AstNodePtr>& body,
                               bool is_lambda);

    // ---- Error ----
    void error(const std::string& msg);
};
