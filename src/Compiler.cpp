#include "Compiler.hpp"
#include <sstream>

// ============================================================================
// Constructor
// ============================================================================
Compiler::Compiler() : had_error(false), current(nullptr), current_line(0) {
    func_stack.reserve(16);
}

// ============================================================================
// Top-level entry: compile a program into a BytecodeChunk
// ============================================================================
BytecodeChunkPtr Compiler::compile(const std::vector<AstNodePtr>& statements) {
    chunk_root = std::make_shared<BytecodeChunk>();
    func_stack.emplace_back();
    current = &func_stack.back();
    current->func = chunk_root->script;
    current->func->name = "<script>";
    current->scope_depth = 0;
    current->locals.push_back({"", 0, false});  // slot 0 reserved

    compile_block(statements);

    // local_names is populated incrementally in declare_local().

    emit_op(OpCode::OP_RETURN_NULL);
    emit_op(OpCode::OP_HALT);

    return chunk_root;
}

// ============================================================================
// Error handling
// ============================================================================
void Compiler::error(const std::string& msg) {
    if (had_error) return;
    had_error = true;
    error_msg = msg;
}

// ============================================================================
// Constant pool / AST side-table helpers
// ============================================================================
uint32_t Compiler::add_constant(ValuePtr value) {
    auto& consts = current->func->constants;
    for (size_t i = 0; i < consts.size(); ++i) {
        if (consts[i]->isEqualTo(*value)) return static_cast<uint32_t>(i);
    }
    consts.push_back(value);
    return static_cast<uint32_t>(consts.size() - 1);
}

uint32_t Compiler::add_string_constant(const std::string& s) {
    return add_constant(std::make_shared<StringValue>(s));
}

uint32_t Compiler::add_ast_node(AstNodePtr node) {
    auto& nodes = current->func->ast_nodes;
    nodes.push_back(node);
    return static_cast<uint32_t>(nodes.size() - 1);
}

// Non-owning overload: wraps a raw AST pointer (the AST tree is alive for the
// entire compilation, so a no-op deleter is safe).
uint32_t Compiler::add_ast_node_raw(AstNode* node) {
    AstNodePtr ptr(node, [](AstNode*){});
    return add_ast_node(ptr);
}

// ============================================================================
// Emission helpers
// ============================================================================
void Compiler::emit_op(OpCode op) {
    current->func->code.push_back(static_cast<uint8_t>(op));
    current->func->lines.push_back(current_line);
}

void Compiler::emit_byte(uint8_t b) {
    current->func->code.push_back(b);
    current->func->lines.push_back(current_line);
}

void Compiler::emit_op_operand(OpCode op, uint32_t operand) {
    emit_op(op);
    // Push each operand byte through emit_byte so the lines vector stays
    // in sync with the code vector.
    emit_byte(static_cast<uint8_t>((operand >> 16) & 0xFF));
    emit_byte(static_cast<uint8_t>((operand >> 8) & 0xFF));
    emit_byte(static_cast<uint8_t>(operand & 0xFF));
}

size_t Compiler::emit_jump(OpCode op) {
    emit_op(op);
    size_t patch_loc = current->func->code.size();
    // Placeholder operand bytes — also tracked in lines for sync.
    emit_byte(0);
    emit_byte(0);
    emit_byte(0);
    return patch_loc;
}

void Compiler::patch_jump(size_t patch_loc) {
    uint32_t target = static_cast<uint32_t>(current->func->code.size());
    patch_operand24(current->func->code, patch_loc, target);
}

void Compiler::emit_loop(size_t loop_start) {
    emit_op_operand(OpCode::OP_LOOP, static_cast<uint32_t>(loop_start));
}

// ============================================================================
// Scope management
// ============================================================================
void Compiler::begin_scope() { current->scope_depth++; }

void Compiler::end_scope() {
    current->scope_depth--;
    // Pop locals that go out of scope.
    // NOTE: In this VM, locals live in frame.locals (a separate vector),
    // NOT on the value stack. So we do NOT emit OP_POP for them.
    // Captured locals need OP_CLOSE_UPVALUE (with the slot index) so the
    // VM migrates their value into the upvalue's `closed` field.
    while (!current->locals.empty() &&
           current->locals.back().depth > current->scope_depth) {
        if (current->locals.back().is_captured) {
            emit_op_operand(OpCode::OP_CLOSE_UPVALUE,
                static_cast<uint32_t>(current->locals.size() - 1));
        }
        current->locals.pop_back();
    }
}

void Compiler::declare_local(const std::string& name) {
    // Check for duplicate in current scope.
    for (auto it = current->locals.rbegin(); it != current->locals.rend(); ++it) {
        if (it->depth < current->scope_depth) break;
        if (it->name == name) return;  // shadowing allowed; just skip
    }
    current->locals.push_back({name, current->scope_depth, false});
    int slot = static_cast<int>(current->locals.size()) - 1;
    // Track peak local count for VM frame sizing.
    int count = static_cast<int>(current->locals.size());
    if (count > current->func->max_locals) {
        current->func->max_locals = count;
    }
    // Save the name for OP_EVAL_AST bridging (indexed by slot).
    if (slot >= static_cast<int>(current->func->local_names.size())) {
        current->func->local_names.resize(slot + 1, "");
    }
    current->func->local_names[slot] = name;
}

int Compiler::resolve_local(const std::string& name) {
    for (int i = static_cast<int>(current->locals.size()) - 1; i >= 0; --i) {
        if (current->locals[i].name == name) return i;
    }
    return -1;
}

int Compiler::add_upvalue(FuncState* func, bool is_local, int index) {
    for (int i = 0; i < static_cast<int>(func->upvalues.size()); ++i) {
        if (func->upvalues[i].is_local == is_local && func->upvalues[i].index == index) {
            return i;
        }
    }
    func->upvalues.push_back({is_local, index});
    return static_cast<int>(func->upvalues.size()) - 1;
}

int Compiler::resolve_upvalue(FuncState* func, const std::string& name) {
    if (func_stack.empty()) return -1;
    // Find the enclosing function state.
    // func_stack.back() is `func` (or a descendant). We need the one before
    // the chain leading to `func`.
    // Simpler: search from the bottom up for the function that encloses `func`.
    size_t idx = 0;
    for (; idx < func_stack.size(); ++idx) {
        if (&func_stack[idx] == func) break;
    }
    if (idx == 0) return -1;
    FuncState* enclosing = &func_stack[idx - 1];

    int local = -1;
    for (int i = static_cast<int>(enclosing->locals.size()) - 1; i >= 0; --i) {
        if (enclosing->locals[i].name == name) { local = i; break; }
    }
    if (local != -1) {
        enclosing->locals[local].is_captured = true;
        return add_upvalue(func, true, local);
    }
    int upv = resolve_upvalue(enclosing, name);
    if (upv != -1) {
        return add_upvalue(func, false, upv);
    }
    return -1;
}

void Compiler::load_variable(const std::string& name, int line) {
    int slot = resolve_local(name);
    if (slot != -1) {
        emit_op_operand(OpCode::OP_GET_LOCAL, static_cast<uint32_t>(slot));
        return;
    }
    int upv = resolve_upvalue(current, name);
    if (upv != -1) {
        emit_op_operand(OpCode::OP_GET_UPVALUE, static_cast<uint32_t>(upv));
        return;
    }
    uint32_t name_idx = add_string_constant(name);
    emit_op_operand(OpCode::OP_GET_GLOBAL, name_idx);
}

void Compiler::store_variable(const std::string& name, int line) {
    int slot = resolve_local(name);
    if (slot != -1) {
        emit_op_operand(OpCode::OP_SET_LOCAL_KEEP, static_cast<uint32_t>(slot));
        return;
    }
    int upv = resolve_upvalue(current, name);
    if (upv != -1) {
        emit_op_operand(OpCode::OP_SET_UPVALUE, static_cast<uint32_t>(upv));
        return;
    }
    uint32_t name_idx = add_string_constant(name);
    emit_op_operand(OpCode::OP_SET_GLOBAL, name_idx);
}

// ============================================================================
// Statement / expression dispatch
// ============================================================================
void Compiler::compile_block(const std::vector<AstNodePtr>& statements) {
    begin_scope();
    for (const auto& stmt : statements) {
        compile_statement(stmt);
    }
    end_scope();
}

void Compiler::compile_statement(const AstNodePtr& node) {
    if (!node) { emit_op(OpCode::OP_NULL); return; }
    current_line = node->line;  // track source line for error reporting
    if (auto n = dynamic_cast<VarDeclarationNode*>(node.get())) { compile_var_declaration(n); return; }
    if (auto n = dynamic_cast<IfStatementNode*>(node.get())) { compile_if(n); return; }
    if (auto n = dynamic_cast<WhileStatementNode*>(node.get())) { compile_while(n); return; }
    if (auto n = dynamic_cast<LoopForNode*>(node.get())) { compile_loop_for(n); return; }
    if (auto n = dynamic_cast<ForInNode*>(node.get())) { compile_for_in(n); return; }
    if (auto n = dynamic_cast<LoopUntilNode*>(node.get())) { compile_loop_until(n); return; }
    if (auto n = dynamic_cast<BreakNode*>(node.get())) { compile_break(n); return; }
    if (auto n = dynamic_cast<ContinueNode*>(node.get())) { compile_continue(n); return; }
    if (auto n = dynamic_cast<SayNode*>(node.get())) { compile_say(n); return; }
    if (auto n = dynamic_cast<InpNode*>(node.get())) { compile_inp(n); return; }
    if (auto n = dynamic_cast<FnDefNode*>(node.get())) { compile_fn_def(n); return; }
    if (auto n = dynamic_cast<ReturnNode*>(node.get())) { compile_return(n); return; }
    if (auto n = dynamic_cast<RaiseNode*>(node.get())) { compile_raise(n); return; }
    if (auto n = dynamic_cast<TryCatchNode*>(node.get())) { compile_try_catch(n); return; }
    if (auto n = dynamic_cast<ExpressionStatementNode*>(node.get())) { compile_expression_statement(n); return; }
    // Complex statements -> OP_EVAL_AST
    emit_op_operand(OpCode::OP_EVAL_AST, add_ast_node(node));
    emit_op(OpCode::OP_POP);
}

void Compiler::compile_expression(const AstNodePtr& node) {
    if (!node) { emit_op(OpCode::OP_NULL); return; }
    current_line = node->line;  // track source line for error reporting
    if (auto n = dynamic_cast<LiteralNode*>(node.get())) { compile_literal(n); return; }
    if (auto n = dynamic_cast<ListLiteralNode*>(node.get())) { compile_list_literal(n); return; }
    if (auto n = dynamic_cast<DimLiteralNode*>(node.get())) { compile_dim_literal(n); return; }
    if (auto n = dynamic_cast<VariableNode*>(node.get())) { compile_variable(n); return; }
    if (auto n = dynamic_cast<UnaryOpNode*>(node.get())) { compile_unary(n); return; }
    if (auto n = dynamic_cast<BinaryOpNode*>(node.get())) { compile_binary(n); return; }
    if (auto n = dynamic_cast<LogicalOpNode*>(node.get())) { compile_logical(n); return; }
    if (auto n = dynamic_cast<TypeConversionNode*>(node.get())) { compile_typecast(n); return; }
    if (auto n = dynamic_cast<AssignmentNode*>(node.get())) { compile_assignment(n); return; }
    if (auto n = dynamic_cast<LambdaNode*>(node.get())) { compile_lambda(n); return; }
    if (auto n = dynamic_cast<CallNode*>(node.get())) { compile_call(n); return; }
    if (auto n = dynamic_cast<SubscriptNode*>(node.get())) { compile_subscript(n); return; }
    if (auto n = dynamic_cast<GetNode*>(node.get())) { compile_get(n); return; }
    if (auto n = dynamic_cast<InpNode*>(node.get())) { compile_inp(n); return; }
    // Complex expressions -> OP_EVAL_AST
    emit_op_operand(OpCode::OP_EVAL_AST, add_ast_node(node));
}

// ============================================================================
// Statement compilers
// ============================================================================
void Compiler::compile_var_declaration(VarDeclarationNode* n) {
    if (n->initializer) {
        compile_expression(n->initializer);
    } else {
        emit_op(OpCode::OP_NULL);
    }
    // Apply type conversion based on the declaration keyword (dec/str/bin/ln/dim/any),
    // mirroring the AST interpreter's VarDeclarationNode::accept logic.
    emit_op_operand(OpCode::OP_CAST,
        static_cast<uint32_t>(n->keyword.type));
    // At script scope, define as global; otherwise as local.
    if (func_stack.size() == 1) {
        uint32_t name_idx = add_string_constant(n->name);
        emit_op_operand(OpCode::OP_DEFINE_GLOBAL, name_idx);
    } else {
        declare_local(n->name);
        int slot = resolve_local(n->name);
        emit_op_operand(OpCode::OP_SET_LOCAL, static_cast<uint32_t>(slot));
    }
}

void Compiler::compile_if(IfStatementNode* n) {
    compile_expression(n->condition);
    size_t else_jump = emit_jump(OpCode::OP_JUMP_IF_FALSE);
    emit_op(OpCode::OP_POP);  // pop condition
    compile_block(n->then_branch);
    size_t end_jump = emit_jump(OpCode::OP_JUMP);
    patch_jump(else_jump);
    emit_op(OpCode::OP_POP);  // pop condition
    if (!n->else_branch.empty()) {
        compile_block(n->else_branch);
    }
    patch_jump(end_jump);
}

void Compiler::compile_while(WhileStatementNode* n) {
    size_t loop_start = current->func->code.size();
    compile_expression(n->condition);
    size_t exit_jump = emit_jump(OpCode::OP_JUMP_IF_FALSE);
    emit_op(OpCode::OP_POP);

    std::vector<size_t> breaks, continues;
    current->loop_starts.push_back(loop_start);
    current->break_patches_ptr = &breaks;
    current->continue_patches_ptr = &continues;

    compile_block(n->do_branch);

    current->loop_starts.pop_back();
    current->break_patches_ptr = nullptr;
    current->continue_patches_ptr = nullptr;

    // Continue jumps to the condition check.
    for (size_t p : continues) patch_jump(p);
    emit_loop(loop_start);
    patch_jump(exit_jump);
    emit_op(OpCode::OP_POP);  // pop condition

    if (!n->finally_branch.empty()) {
        compile_block(n->finally_branch);
    }
    for (size_t p : breaks) patch_jump(p);
}

void Compiler::compile_loop_for(LoopForNode* n) {
    // loop(i) for N times { body }
    // Semantics (matching AST interpreter): a hidden counter goes from 0 to N-1.
    // Each iteration: i = counter (reset from counter, body can't persistently
    // modify i). Condition checked BEFORE body (counter < count).
    begin_scope();

    // Hidden counter, initialized to 0
    declare_local("__counter__");
    int counter_slot = resolve_local("__counter__");
    emit_op(OpCode::OP_FALSE);
    emit_op_operand(OpCode::OP_SET_LOCAL, static_cast<uint32_t>(counter_slot));

    // Evaluate count and store in a hidden local
    compile_expression(n->count_expr);
    declare_local("__count__");
    int count_slot = resolve_local("__count__");
    emit_op_operand(OpCode::OP_SET_LOCAL, static_cast<uint32_t>(count_slot));

    // Loop variable (reset each iteration from counter)
    declare_local(n->index_var_name);
    int i_slot = resolve_local(n->index_var_name);

    size_t loop_start = current->func->code.size();

    // Condition: if counter >= count, exit (checked BEFORE body)
    emit_op_operand(OpCode::OP_GET_LOCAL, static_cast<uint32_t>(counter_slot));
    emit_op_operand(OpCode::OP_GET_LOCAL, static_cast<uint32_t>(count_slot));
    emit_op(OpCode::OP_GREATER_EQUAL);
    size_t exit_jump = emit_jump(OpCode::OP_JUMP_IF_TRUE);
    emit_op(OpCode::OP_POP);

    // Set i = counter (reset loop variable from counter)
    emit_op_operand(OpCode::OP_GET_LOCAL, static_cast<uint32_t>(counter_slot));
    emit_op_operand(OpCode::OP_SET_LOCAL, static_cast<uint32_t>(i_slot));

    std::vector<size_t> breaks, continues;
    current->loop_starts.push_back(loop_start);
    current->break_patches_ptr = &breaks;
    current->continue_patches_ptr = &continues;

    compile_block(n->body);

    current->loop_starts.pop_back();
    current->break_patches_ptr = nullptr;
    current->continue_patches_ptr = nullptr;

    // Continue: increment counter, then loop
    for (size_t p : continues) patch_jump(p);
    emit_op_operand(OpCode::OP_GET_LOCAL, static_cast<uint32_t>(counter_slot));
    emit_op(OpCode::OP_TRUE);  // push 1
    emit_op(OpCode::OP_ADD);
    emit_op_operand(OpCode::OP_SET_LOCAL, static_cast<uint32_t>(counter_slot));
    emit_loop(loop_start);

    patch_jump(exit_jump);
    emit_op(OpCode::OP_POP);

    for (size_t p : breaks) patch_jump(p);
    end_scope();
}

void Compiler::compile_for_in(ForInNode* n) {
    // for x in iterable { body }
    begin_scope();
    // Evaluate iterable, store in hidden local
    compile_expression(n->iterable);
    declare_local("__iter__");
    int iter_slot = resolve_local("__iter__");
    emit_op_operand(OpCode::OP_SET_LOCAL, static_cast<uint32_t>(iter_slot));

    // Hidden index variable
    declare_local("__idx__");
    int idx_slot = resolve_local("__idx__");
    emit_op(OpCode::OP_FALSE);
    emit_op_operand(OpCode::OP_SET_LOCAL, static_cast<uint32_t>(idx_slot));

    // Loop variable
    declare_local(n->var_name);
    int var_slot = resolve_local(n->var_name);

    size_t loop_start = current->func->code.size();

    // Check: if idx >= len(iter), exit
    emit_op_operand(OpCode::OP_GET_LOCAL, static_cast<uint32_t>(idx_slot));
    emit_op_operand(OpCode::OP_GET_LOCAL, static_cast<uint32_t>(iter_slot));
    emit_op(OpCode::OP_GET_LEN);
    emit_op(OpCode::OP_GREATER_EQUAL);
    size_t exit_jump = emit_jump(OpCode::OP_JUMP_IF_TRUE);
    emit_op(OpCode::OP_POP);

    // var = iter[idx]
    emit_op_operand(OpCode::OP_GET_LOCAL, static_cast<uint32_t>(iter_slot));
    emit_op_operand(OpCode::OP_GET_LOCAL, static_cast<uint32_t>(idx_slot));
    emit_op(OpCode::OP_GET_INDEX);
    emit_op_operand(OpCode::OP_SET_LOCAL, static_cast<uint32_t>(var_slot));

    std::vector<size_t> breaks, continues;
    current->loop_starts.push_back(loop_start);
    current->break_patches_ptr = &breaks;
    current->continue_patches_ptr = &continues;

    compile_block(n->body);

    current->loop_starts.pop_back();
    current->break_patches_ptr = nullptr;
    current->continue_patches_ptr = nullptr;

    // Continue: idx++, loop
    for (size_t p : continues) patch_jump(p);
    emit_op_operand(OpCode::OP_GET_LOCAL, static_cast<uint32_t>(idx_slot));
    emit_op(OpCode::OP_TRUE);
    emit_op(OpCode::OP_ADD);
    emit_op_operand(OpCode::OP_SET_LOCAL, static_cast<uint32_t>(idx_slot));
    emit_loop(loop_start);

    patch_jump(exit_jump);
    emit_op(OpCode::OP_POP);

    for (size_t p : breaks) patch_jump(p);
    end_scope();
}

void Compiler::compile_loop_until(LoopUntilNode* n) {
    // loop(j) ... until cond
    // Semantics (matching AST interpreter): a hidden counter starts at 0.
    // Each iteration: j = counter; counter++. Then execute body, then check
    // condition. The body may modify j, but j is reset at the next iteration.
    begin_scope();

    // Hidden counter variable, initialized to 0
    declare_local("__counter__");
    int counter_slot = resolve_local("__counter__");
    emit_op(OpCode::OP_FALSE);
    emit_op_operand(OpCode::OP_SET_LOCAL, static_cast<uint32_t>(counter_slot));

    // Loop variable (reset each iteration from the counter)
    declare_local(n->index_var_name);
    int i_slot = resolve_local(n->index_var_name);

    size_t loop_start = current->func->code.size();

    std::vector<size_t> breaks, continues;
    current->loop_starts.push_back(loop_start);
    current->break_patches_ptr = &breaks;
    current->continue_patches_ptr = &continues;

    // --- Start of each iteration: j = counter; counter++ ---
    emit_op_operand(OpCode::OP_GET_LOCAL, static_cast<uint32_t>(counter_slot));
    emit_op_operand(OpCode::OP_SET_LOCAL, static_cast<uint32_t>(i_slot));
    emit_op_operand(OpCode::OP_GET_LOCAL, static_cast<uint32_t>(counter_slot));
    emit_op(OpCode::OP_TRUE);  // push 1
    emit_op(OpCode::OP_ADD);
    emit_op_operand(OpCode::OP_SET_LOCAL, static_cast<uint32_t>(counter_slot));

    compile_block(n->body);

    current->loop_starts.pop_back();
    current->break_patches_ptr = nullptr;
    current->continue_patches_ptr = nullptr;

    // Continue: jump back to loop_start (resets j from counter)
    for (size_t p : continues) patch_jump(p);

    // Check condition: if truthy, exit
    compile_expression(n->condition);
    size_t exit_jump = emit_jump(OpCode::OP_JUMP_IF_TRUE);
    emit_op(OpCode::OP_POP);
    emit_loop(loop_start);
    patch_jump(exit_jump);
    emit_op(OpCode::OP_POP);

    for (size_t p : breaks) patch_jump(p);
    end_scope();
}

void Compiler::compile_break(BreakNode* n) {
    if (!current->break_patches_ptr) {
        error("'break' used outside of a loop");
        return;
    }
    current->break_patches_ptr->push_back(emit_jump(OpCode::OP_JUMP));
}

void Compiler::compile_continue(ContinueNode* n) {
    if (!current->continue_patches_ptr) {
        error("'continue' used outside of a loop");
        return;
    }
    current->continue_patches_ptr->push_back(emit_jump(OpCode::OP_JUMP));
}

void Compiler::compile_say(SayNode* n) {
    compile_expression(n->expression);
    emit_op(OpCode::OP_PRINT);
}

void Compiler::compile_inp(InpNode* n) {
    if (n->expression) {
        compile_expression(n->expression);
    } else {
        emit_op(OpCode::OP_NULL);
    }
    emit_op(OpCode::OP_INPUT);
}

void Compiler::compile_fn_def(FnDefNode* n) {
    // Declare the local BEFORE compiling the body so that recursive calls
    // inside the body can resolve the name as an upvalue.
    bool is_global = (func_stack.size() == 1);
    if (!is_global) {
        declare_local(n->name);
    }
    compile_function_body(n->name, n->params, n->body, false);
    if (is_global) {
        uint32_t name_idx = add_string_constant(n->name);
        emit_op_operand(OpCode::OP_DEFINE_GLOBAL, name_idx);
    } else {
        int slot = resolve_local(n->name);
        emit_op_operand(OpCode::OP_SET_LOCAL, static_cast<uint32_t>(slot));
    }
}

void Compiler::compile_return(ReturnNode* n) {
    if (n->value) {
        compile_expression(n->value);
    } else {
        emit_op(OpCode::OP_NULL);
    }
    emit_op(OpCode::OP_RETURN);
}

void Compiler::compile_raise(RaiseNode* n) {
    compile_expression(n->expression);
    emit_op(OpCode::OP_RAISE);
}

void Compiler::compile_try_catch(TryCatchNode* n) {
    // try ... catch e ... endtry
    //
    // Layout:
    //   TRY_BEGIN <catch_target>   // push try-handler
    //   <try body>
    //   TRY_END                    // pop handler (success)
    //   JUMP <after_catch>
    // <catch_target>:
    //   CATCH                      // exception value already on stack
    //   <store exception in var>
    //   <catch body>
    // <after_catch>:
    //   <finally body if any>
    begin_scope();

    // Emit TRY_BEGIN with placeholder catch target.
    emit_op(OpCode::OP_TRY_BEGIN);
    size_t catch_target_loc = current->func->code.size();
    emit_byte(0); emit_byte(0); emit_byte(0);  // placeholder

    // Compile try body.
    for (const auto& stmt : n->try_branch) {
        compile_statement(stmt);
    }

    // Try completed without exception: pop handler and jump past catch.
    emit_op(OpCode::OP_TRY_END);
    size_t skip_catch_jump = emit_jump(OpCode::OP_JUMP);

    // Catch handler: patch TRY_BEGIN target to here.
    patch_jump(catch_target_loc);
    emit_op(OpCode::OP_CATCH);

    // Store the exception value in the catch variable.
    if (!n->exception_var.empty()) {
        if (func_stack.size() == 1) {
            // Global scope: define as global.
            uint32_t name_idx = add_string_constant(n->exception_var);
            emit_op_operand(OpCode::OP_DEFINE_GLOBAL, name_idx);
        } else {
            // Local scope: declare as local.
            declare_local(n->exception_var);
            int slot = resolve_local(n->exception_var);
            emit_op_operand(OpCode::OP_SET_LOCAL, static_cast<uint32_t>(slot));
        }
    } else {
        // No catch variable: pop the exception value.
        emit_op(OpCode::OP_POP);
    }

    // Compile catch body.
    for (const auto& stmt : n->catch_branch) {
        compile_statement(stmt);
    }

    // Patch the skip-catch jump to here.
    patch_jump(skip_catch_jump);

    // Compile finally body (if any).
    for (const auto& stmt : n->finally_branch) {
        compile_statement(stmt);
    }

    end_scope();
}

void Compiler::compile_expression_statement(ExpressionStatementNode* n) {
    compile_expression(n->expression);
    emit_op(OpCode::OP_POP);
}

// ============================================================================
// Expression compilers
// ============================================================================
void Compiler::compile_literal(LiteralNode* n) {
    if (!n->value) { emit_op(OpCode::OP_NULL); return; }
    if (auto num = std::dynamic_pointer_cast<NumberValue>(n->value)) {
        if (num->value == BigNumber(0)) { emit_op(OpCode::OP_FALSE); return; }
        if (num->value == BigNumber(1)) { emit_op(OpCode::OP_TRUE); return; }
    }
    if (std::dynamic_pointer_cast<NullValue>(n->value)) { emit_op(OpCode::OP_NULL); return; }
    uint32_t idx = add_constant(n->value);
    emit_op_operand(OpCode::OP_CONSTANT, idx);
}

void Compiler::compile_list_literal(ListLiteralNode* n) {
    for (const auto& el : n->elements) {
        compile_expression(el);
    }
    emit_op_operand(OpCode::OP_BUILD_LIST, static_cast<uint32_t>(n->elements.size()));
}

void Compiler::compile_dim_literal(DimLiteralNode* n) {
    for (const auto& entry : n->entries) {
        compile_expression(entry.first);   // key
        compile_expression(entry.second);  // value
    }
    emit_op_operand(OpCode::OP_BUILD_DICT, static_cast<uint32_t>(n->entries.size()));
}

void Compiler::compile_variable(VariableNode* n) {
    load_variable(n->name, n->line);
}

void Compiler::compile_unary(UnaryOpNode* n) {
    compile_expression(n->right);
    switch (n->op.type) {
        case TokenType::MINUS: emit_op(OpCode::OP_NEGATE); break;
        case TokenType::NOT:  emit_op(OpCode::OP_NOT); break;
        default:
            emit_op_operand(OpCode::OP_EVAL_AST, add_ast_node_raw(n));
            return;
    }
}

void Compiler::compile_binary(BinaryOpNode* n) {
    compile_expression(n->left);
    compile_expression(n->right);
    switch (n->op.type) {
        case TokenType::PLUS:          emit_op(OpCode::OP_ADD); break;
        case TokenType::MINUS:         emit_op(OpCode::OP_SUB); break;
        case TokenType::STAR:          emit_op(OpCode::OP_MUL); break;
        case TokenType::SLASH:         emit_op(OpCode::OP_DIV); break;
        case TokenType::MODULO:       emit_op(OpCode::OP_MOD); break;
        case TokenType::CARET:         emit_op(OpCode::OP_POW); break;
        case TokenType::EQUAL_EQUAL:   emit_op(OpCode::OP_EQUAL); break;
        case TokenType::BANG_EQUAL:    emit_op(OpCode::OP_NOT_EQUAL); break;
        case TokenType::LESS:          emit_op(OpCode::OP_LESS); break;
        case TokenType::LESS_EQUAL:    emit_op(OpCode::OP_LESS_EQUAL); break;
        case TokenType::GREATER:       emit_op(OpCode::OP_GREATER); break;
        case TokenType::GREATER_EQUAL: emit_op(OpCode::OP_GREATER_EQUAL); break;
        default:
            // Unknown binary op -> fall back to AST
            emit_op(OpCode::OP_POP);
            emit_op(OpCode::OP_POP);
            emit_op_operand(OpCode::OP_EVAL_AST, add_ast_node_raw(n));
            break;
    }
}

void Compiler::compile_logical(LogicalOpNode* n) {
    compile_expression(n->left);
    if (n->op.type == TokenType::AND) {
        size_t end_jump = emit_jump(OpCode::OP_JUMP_IF_FALSE_KEEP);
        emit_op(OpCode::OP_POP);
        compile_expression(n->right);
        patch_jump(end_jump);
    } else {  // OR
        size_t end_jump = emit_jump(OpCode::OP_JUMP_IF_TRUE_KEEP);
        emit_op(OpCode::OP_POP);
        compile_expression(n->right);
        patch_jump(end_jump);
    }
}

void Compiler::compile_typecast(TypeConversionNode* n) {
    compile_expression(n->expression);
    // Encode the target type as the TokenType value directly in the operand.
    emit_op_operand(OpCode::OP_CAST,
        static_cast<uint32_t>(n->type_keyword.type));
}

void Compiler::compile_assignment(AssignmentNode* n) {
    // Handle different target types.
    if (auto var = dynamic_cast<VariableNode*>(n->target.get())) {
        // store_variable uses peek-based stores (OP_SET_LOCAL_KEEP,
        // OP_SET_GLOBAL, OP_SET_UPVALUE) that do NOT pop, so the assigned
        // value remains on the stack as the expression result. No DUP needed.
        compile_expression(n->value);
        store_variable(var->name, n->line);
    } else if (auto sub = dynamic_cast<SubscriptNode*>(n->target.get())) {
        if (sub->is_slice) {
            // Slice assignment: obj[start:end:step] = value
            compile_expression(sub->object);
            if (sub->start) compile_expression(sub->start); else emit_op(OpCode::OP_NULL);
            if (sub->end) compile_expression(sub->end); else emit_op(OpCode::OP_NULL);
            if (sub->step) compile_expression(sub->step); else emit_op(OpCode::OP_NULL);
            compile_expression(n->value);
            emit_op(OpCode::OP_SET_SLICE);
            emit_op(OpCode::OP_NULL);  // assignment returns null
        } else {
            compile_expression(sub->object);
            compile_expression(sub->start);
            compile_expression(n->value);
            emit_op(OpCode::OP_SET_INDEX);
            emit_op(OpCode::OP_NULL);
        }
    } else if (auto get = dynamic_cast<GetNode*>(n->target.get())) {
        compile_expression(get->object);
        compile_expression(n->value);
        uint32_t name_idx = add_string_constant(get->name);
        emit_op_operand(OpCode::OP_SET_PROPERTY, name_idx);
        emit_op(OpCode::OP_NULL);
    } else {
        // Complex target -> AST fallback
        emit_op_operand(OpCode::OP_EVAL_AST, add_ast_node_raw(n));
    }
}

void Compiler::compile_lambda(LambdaNode* n) {
    compile_function_body("<lambda>", n->params, n->body, true);
}

void Compiler::compile_call(CallNode* n) {
    compile_expression(n->callee);
    for (const auto& arg : n->arguments) {
        compile_expression(arg);
    }
    emit_op_operand(OpCode::OP_CALL, static_cast<uint32_t>(n->arguments.size()));
}

void Compiler::compile_subscript(SubscriptNode* n) {
    compile_expression(n->object);
    if (n->is_slice) {
        if (n->start) compile_expression(n->start); else emit_op(OpCode::OP_NULL);
        if (n->end) compile_expression(n->end); else emit_op(OpCode::OP_NULL);
        if (n->step) compile_expression(n->step); else emit_op(OpCode::OP_NULL);
        emit_op(OpCode::OP_GET_SLICE);
    } else {
        compile_expression(n->start);
        emit_op(OpCode::OP_GET_INDEX);
    }
}

void Compiler::compile_get(GetNode* n) {
    compile_expression(n->object);
    uint32_t name_idx = add_string_constant(n->name);
    emit_op_operand(OpCode::OP_GET_PROPERTY, name_idx);
}

// ============================================================================
// Function body compilation (shared by fn def and lambda)
// ============================================================================
void Compiler::compile_function_body(const std::string& name,
                                     const std::vector<ParameterDefinition>& params,
                                     const std::vector<AstNodePtr>& body,
                                     bool is_lambda) {
    size_t saved_index = func_stack.size() - 1;
    func_stack.emplace_back();
    current = &func_stack.back();
    current->func = std::make_shared<CompiledFunction>();
    current->func->name = name;
    current->func->is_lambda = is_lambda;
    current->scope_depth = 0;
    current->locals.push_back({"", 0, false});  // slot 0 reserved

    int required = 0;
    for (const auto& p : params) {
        declare_local(p.name);
        CompiledFunction::ParamInfo pi;
        pi.type_keyword = p.type_keyword;
        pi.name = p.name;
        pi.default_value = p.default_value;
        pi.default_expr = p.default_expr;
        pi.has_default = p.has_default;
        current->func->params.push_back(pi);
        if (!p.has_default) required++;
    }
    current->func->arity = required;
    current->func->num_params = static_cast<int>(params.size());

    // Compile body statements directly (without begin/end_scope) so that
    // function-level locals persist in local_names for the VM to size the
    // frame's locals vector correctly.
    for (const auto& stmt : body) {
        compile_statement(stmt);
    }
    emit_op(OpCode::OP_RETURN_NULL);

    CompiledFunctionPtr fn = current->func;
    fn->upvalue_count = static_cast<int>(current->upvalues.size());
    fn->upvalue_descriptors = current->upvalues;
    // local_names is populated incrementally in declare_local().
    func_stack.pop_back();
    current = &func_stack[saved_index];

    uint32_t fn_idx = static_cast<uint32_t>(chunk_root->all_functions.size());
    chunk_root->all_functions.push_back(fn);
    emit_op_operand(OpCode::OP_CLOSURE, fn_idx);
}
