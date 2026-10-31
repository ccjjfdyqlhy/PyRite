#include "Compiler.hpp"
#include "CastTypes.hpp"
#include "Parser.hpp"
#include "ModulePath.hpp"
#include <sstream>
#include <fstream>
#include <cstdlib>
#include <sys/stat.h>
#include <unistd.h>

// ============================================================================
// Constructor
// ============================================================================
Compiler::Compiler() : had_error(false), current(nullptr), current_line(0) {
    func_stack.reserve(16);
}

// ============================================================================
// Top-level entry: compile a program into a BytecodeChunk
// ============================================================================
BytecodeChunkPtr Compiler::compile(const std::vector<AstNodePtr>& statements,
                                  const std::string& source_path) {
    source_path_ = source_path;
    source_dir_ = source_path.empty() ? "" : ModulePath::dir_of(source_path);
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
    return add_constant(make_value<StringValue>(s));
}

// Like add_constant, but never shares the entry with anything else. Used for
// spoken lines, so that watermarking one can never affect an identifier or a
// call argument that happens to carry the same text.
uint32_t Compiler::add_display_constant(ValuePtr value) {
    auto& consts = current->func->constants;
    consts.push_back(value);
    return static_cast<uint32_t>(consts.size() - 1);
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
    if (auto n = dynamic_cast<RequireNode*>(node.get())) { compile_require(n); return; }
    if (auto n = dynamic_cast<StructDefNode*>(node.get())) { compile_struct_def(node, n); return; }
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
        // A table in a local is display data, exactly like a table that
        // is returned wholesale: its strings are the lines the player
        // reads, and the packaging step has to watermark them.
        //
        // This is what lets a story keep its tables inside the function
        // that hands them out one row at a time: the rows are still
        // marked, and the table is still out of reach by name.
        const bool display_literal =
            dynamic_cast<ListLiteralNode*>(n->initializer.get()) != nullptr ||
            dynamic_cast<DimLiteralNode*>(n->initializer.get()) != nullptr;
        if (display_literal) ++display_literal_depth_;
        compile_expression(n->initializer);
        if (display_literal) --display_literal_depth_;
    } else {
        emit_op(OpCode::OP_NULL);
    }
    // Apply type conversion based on the declaration keyword (dec/str/bin/ln/dim/any),
    // mirroring the AST interpreter's VarDeclarationNode::accept logic.
    //
    // The operand is a stable cast code (see CastTypes.hpp), NOT a TokenType
    // value: TokenType is a tokenizer implementation detail whose numbering
    // shifts whenever a keyword is added, which would silently change the
    // meaning of every already-compiled .prc file.
    uint32_t cast_code = static_cast<uint32_t>(CastType::CAST_ANY);
    cast_code_for(n->keyword.type, cast_code);
    // A declaration is stricter than an `as` cast: dec/bin accept only
    // what really converts (null becomes zero), everything else fails.
    if (cast_code == static_cast<uint32_t>(CastType::CAST_DEC))
        cast_code = static_cast<uint32_t>(CastType::CAST_DEC_DECL);
    else if (cast_code == static_cast<uint32_t>(CastType::CAST_BIN))
        cast_code = static_cast<uint32_t>(CastType::CAST_BIN_DECL);
    emit_op_operand(OpCode::OP_CAST, cast_code);
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
    // Fused compare-and-jump when the condition is a plain comparison of
    // variables/literals: same stack contract as the unfused form (the
    // condition value rides the stack into the false branch).
    int fused_patch = try_compile_cmp_jump(n->condition.get());
    size_t else_jump;
    if (fused_patch >= 0) {
        else_jump = static_cast<size_t>(fused_patch);
    } else {
        compile_expression(n->condition);
        else_jump = emit_jump(OpCode::OP_JUMP_IF_FALSE);
        emit_op(OpCode::OP_POP);  // pop condition
    }
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
    int fused_patch = try_compile_cmp_jump(n->condition.get());
    size_t exit_jump;
    if (fused_patch >= 0) {
        exit_jump = static_cast<size_t>(fused_patch);
    } else {
        compile_expression(n->condition);
        exit_jump = emit_jump(OpCode::OP_JUMP_IF_FALSE);
        emit_op(OpCode::OP_POP);
    }

    std::vector<size_t> breaks, continues;
    current->loop_starts.push_back(loop_start);
    current->loop_entry_try_depths.push_back(current->try_depth);
    current->break_patches_ptr = &breaks;
    current->continue_patches_ptr = &continues;

    compile_block(n->do_branch);

    current->loop_starts.pop_back();
    current->loop_entry_try_depths.pop_back();
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
    current->loop_entry_try_depths.push_back(current->try_depth);
    current->break_patches_ptr = &breaks;
    current->continue_patches_ptr = &continues;

    compile_block(n->body);

    current->loop_starts.pop_back();
    current->loop_entry_try_depths.pop_back();
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
    current->loop_entry_try_depths.push_back(current->try_depth);
    current->break_patches_ptr = &breaks;
    current->continue_patches_ptr = &continues;

    compile_block(n->body);

    current->loop_starts.pop_back();
    current->loop_entry_try_depths.pop_back();
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
    current->loop_entry_try_depths.push_back(current->try_depth);
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
    current->loop_entry_try_depths.pop_back();
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
    // Leaving the loop also leaves its nested try blocks: pop their
    // handlers on the way out or the jump will strand them.
    while (current->try_depth > current->loop_entry_try_depths.back()) {
        emit_op(OpCode::OP_TRY_END);
        current->try_depth--;
    }
    current->break_patches_ptr->push_back(emit_jump(OpCode::OP_JUMP));
}

void Compiler::compile_continue(ContinueNode* n) {
    if (!current->continue_patches_ptr) {
        error("'continue' used outside of a loop");
        return;
    }
    while (current->try_depth > current->loop_entry_try_depths.back()) {
        emit_op(OpCode::OP_TRY_END);
        current->try_depth--;
    }
    current->continue_patches_ptr->push_back(emit_jump(OpCode::OP_JUMP));
}

void Compiler::compile_say(SayNode* n) {
    // A direct string literal is a spoken line. Give it its own constant
    // (never shared with an identifier or a call argument) and remember it,
    // so the packaging step can watermark exactly the dialogue.
    if (auto lit = dynamic_cast<LiteralNode*>(n->expression.get())) {
        if (lit->value && dynamic_cast<StringValue*>(lit->value.get())) {
            uint32_t idx = add_display_constant(lit->value);
            emit_op_operand(OpCode::OP_CONSTANT, idx);
            current->func->display_constants.push_back(static_cast<int>(idx));
            emit_op(OpCode::OP_PRINT);
            return;
        }
    }
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

// A struct travels as a descriptor constant: { name, fields: [ { name,
// type (a stable cast code), default, has_default } ] }.  OP_DEF_STRUCT
// turns it back into a Class at runtime, so struct declarations are plain
// bytecode - no AST fallback - and survive the .prc / JSON pipelines.
void Compiler::compile_struct_def(const AstNodePtr& node, StructDefNode* n) {
    // A descriptor constant can carry literal defaults only. A struct
    // with an expression default keeps the AST fallback so the
    // expression still evaluates per instantiation.
    for (const auto& f : n->fields) {
        if (f.default_expr) {
            // Exactly the generic statement fallback: the AST evaluator
            // runs the definition and registers the class; nothing extra
            // goes on the stack.
            emit_op_operand(OpCode::OP_EVAL_AST, add_ast_node(node));
            emit_op(OpCode::OP_POP);
            return;
        }
    }
    auto desc = make_value<DimValue>();
    desc->dict["name"] = make_value<StringValue>(n->name);
    auto fields = make_value<LnValue>(std::vector<ValuePtr>{});
    for (const auto& f : n->fields) {
        auto fd = make_value<DimValue>();
        fd->dict["name"] = make_value<StringValue>(f.name);
        uint32_t code = static_cast<uint32_t>(CastType::CAST_ANY);
        cast_code_for(f.type_keyword, code);
        fd->dict["type"] = make_value<NumberValue>(BigNumber((long long)code));
        fd->dict["default"] = f.default_value ? f.default_value : make_value<NullValue>();
        fd->dict["has_default"] = make_value<NumberValue>(f.default_value ? 1 : 0);
        fields->elements.push_back(fd);
    }
    desc->dict["fields"] = fields;
    uint32_t desc_idx = add_constant(desc);
    emit_op_operand(OpCode::OP_DEF_STRUCT, desc_idx);
    // The Class lands on the stack; bind the name like a fn definition.
    if (func_stack.size() == 1) {
        uint32_t name_idx = add_string_constant(n->name);
        emit_op_operand(OpCode::OP_DEFINE_GLOBAL, name_idx);
    } else {
        declare_local(n->name);
        int slot = resolve_local(n->name);
        emit_op_operand(OpCode::OP_SET_LOCAL, static_cast<uint32_t>(slot));
    }
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
    // Returning leaves every try block of this function: unwind their
    // handlers here, or the VM keeps stale handlers after the jump.
    while (current->try_depth > 0) {
        emit_op(OpCode::OP_TRY_END);
        current->try_depth--;
    }
    if (n->value) {
        // A wholesale-returned literal is display data: the host shows it to
        // the player (a lyric table, a staged dialogue sequence). Its string
        // literals get dedicated constants the packaging step can watermark.
        // The author keeps keys and directive names out of the literal
        // (positional arrays, numeric opcodes) or they get marked too.
        const bool display_literal =
            dynamic_cast<ListLiteralNode*>(n->value.get()) != nullptr ||
            dynamic_cast<DimLiteralNode*>(n->value.get()) != nullptr;
        if (display_literal) ++display_literal_depth_;
        compile_expression(n->value);
        if (display_literal) --display_literal_depth_;
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
    current->try_depth++;
    for (const auto& stmt : n->try_branch) {
        compile_statement(stmt);
    }
    current->try_depth--;

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
    // `s = s + i * 3;` as a statement: the fused in-place form leaves the
    // assigned value on the stack only to have it popped here. When the
    // statement IS a fused assignment, run the no-push variant instead.
    if (auto asg = dynamic_cast<AssignmentNode*>(n->expression.get())) {
        if (auto var = dynamic_cast<VariableNode*>(asg->target.get())) {
            if (try_compile_inplace_np(var->name, asg->value.get())) return;
        }
    }
    compile_expression(n->expression);
    emit_op(OpCode::OP_POP);
}

// ============================================================================
// Modules
// ============================================================================
//
// A require is resolved and inlined at COMPILE time.  The module becomes a
// function of the program; its exports become a dictionary that the alias
// points at.
//
// Why compile time and not run time: the shipped program is one file.  The
// page loads a bundle and the browser VM has no file system, so a module
// that is read at run time would have to travel next to the program -- and
// then the program is no longer the whole story.  Inlining keeps the
// program self-contained and keeps the module's code inside the encrypted
// pool.
//
// The browser VM cannot execute the AST fallbacks (classes, structs,
// try/catch, await), so a module that uses them cannot be packaged.  The
// JSON exporter refuses such a program and says which feature it is.

void Compiler::compile_require(RequireNode* n) {
    std::string resolved;
    bool is_dir_module = false;
    std::string tried;
    if (!ModulePath::resolve(n->module_path, source_dir_, resolved,
                             is_dir_module, tried)) {
        error("cannot find the module '" + n->module_path +
              "'; looked in: " + tried);
        return;
    }
    for (const auto& seen : module_stack_) {
        if (seen == resolved) {
            error("circular require: '" + n->module_path +
                  "' is already being compiled (" + resolved + ")");
            return;
        }
    }

    std::ifstream file(resolved);
    if (!file.is_open()) {
        error("cannot open the module '" + resolved + "'");
        return;
    }
    std::string source_code((std::istreambuf_iterator<char>(file)),
                            std::istreambuf_iterator<char>());
    file.close();

    Parser parser(source_code);
    auto statements = parser.parse();
    if (parser.has_error()) {
        error("the module '" + resolved + "' does not parse");
        return;
    }

    std::string alias = n->alias_name;
    if (alias.empty()) {
        size_t pos = n->module_path.find_last_of("/\\");
        alias = (pos == std::string::npos) ? n->module_path
                                           : n->module_path.substr(pos + 1);
    }

    // The module's own requires are relative to the module.
    std::string saved_dir = source_dir_;
    source_dir_ = ModulePath::dir_of(resolved);
    module_stack_.push_back(resolved);
    compile_module(resolved, is_dir_module, statements, alias);
    module_stack_.pop_back();
    source_dir_ = saved_dir;
}

void Compiler::compile_module(const std::string& resolved, bool is_dir_module,
                              const std::vector<AstNodePtr>& statements,
                              const std::string& alias) {
    // What the module hands out.  A single file exports everything it
    // defines; a directory module (an _index.pr) exports only what it
    // marked with expose.  This is the same rule the interpreter uses.
    std::vector<std::string> exports;
    for (const auto& stmt : statements) {
        if (auto fn = dynamic_cast<FnDefNode*>(stmt.get())) {
            if (!is_dir_module || fn->is_exposed) exports.push_back(fn->name);
        } else if (auto var = dynamic_cast<VarDeclarationNode*>(stmt.get())) {
            if (!is_dir_module || var->is_exposed) exports.push_back(var->name);
        }
    }

    // The module becomes a function of the program.  Its statements are the
    // body, so its functions are closures over its own variables, exactly
    // like the module environment the interpreter builds.
    size_t saved_index = func_stack.size() - 1;
    func_stack.emplace_back();
    current = &func_stack.back();
    current->func = std::make_shared<CompiledFunction>();
    current->func->name = "__module_" + alias + "_" +
                          std::to_string(module_counter_++);
    current->scope_depth = 0;
    current->locals.push_back({"", 0, false});

    for (const auto& stmt : statements) {
        compile_statement(stmt);
    }

    // A module may have a hook that runs when it is loaded.  The
    // interpreter calls it when the module opens; here the module is
    // part of the program, so it runs where the require stands.
    for (const auto& stmt : statements) {
        auto fn = dynamic_cast<FnDefNode*>(stmt.get());
        if (!fn || fn->name != "_on_load") continue;
        int slot = resolve_local("_on_load");
        if (slot < 0) break;
        emit_op_operand(OpCode::OP_GET_LOCAL, static_cast<uint32_t>(slot));
        emit_op_operand(OpCode::OP_CALL, 0);
        emit_op(OpCode::OP_POP);
        break;
    }

    // The exports: a dictionary the alias points at.
    //     dim __exports = {}
    //     __exports["name"] = name
    uint32_t dict_idx = add_constant(make_value<DimValue>());
    emit_op_operand(OpCode::OP_CONSTANT, dict_idx);
    emit_op_operand(OpCode::OP_CAST, static_cast<uint32_t>(CastType::CAST_DIM));
    declare_local("__exports");
    int exports_slot = resolve_local("__exports");
    emit_op_operand(OpCode::OP_SET_LOCAL, static_cast<uint32_t>(exports_slot));

    for (const auto& name : exports) {
        emit_op_operand(OpCode::OP_GET_LOCAL,
                        static_cast<uint32_t>(exports_slot));
        uint32_t key_idx = add_constant(make_value<StringValue>(name));
        emit_op_operand(OpCode::OP_CONSTANT, key_idx);
        load_variable(name, current_line);
        emit_op(OpCode::OP_SET_INDEX);
    }

    // return __exports
    emit_op_operand(OpCode::OP_GET_LOCAL, static_cast<uint32_t>(exports_slot));
    emit_op(OpCode::OP_RETURN);

    CompiledFunctionPtr fn = current->func;
    fn->upvalue_count = static_cast<int>(current->upvalues.size());
    fn->upvalue_descriptors = current->upvalues;
    func_stack.pop_back();
    current = &func_stack[saved_index];

    uint32_t fn_idx = static_cast<uint32_t>(chunk_root->all_functions.size());
    chunk_root->all_functions.push_back(fn);
    emit_op_operand(OpCode::OP_CLOSURE, fn_idx);
    // Call it right away: the module is part of the program, so its
    // exports exist from the first line.
    emit_op_operand(OpCode::OP_CALL, 0);
    // Bind the exports to the alias.  This is a definition, not an
    // assignment: the name does not exist yet.  Both stores pop the
    // value, so the stack is balanced afterwards.
    if (func_stack.size() == 1) {
        uint32_t name_idx = add_string_constant(alias);
        emit_op_operand(OpCode::OP_DEFINE_GLOBAL, name_idx);
    } else {
        declare_local(alias);
        int slot = resolve_local(alias);
        emit_op_operand(OpCode::OP_SET_LOCAL, static_cast<uint32_t>(slot));
    }
}

// ============================================================================
// Expression compilers
// ============================================================================
void Compiler::compile_literal(LiteralNode* n) {
    if (!n->value) { emit_op(OpCode::OP_NULL); return; }
    if (auto num = ptr_cast<NumberValue>(n->value)) {
        if (num->value == BigNumber(0)) { emit_op(OpCode::OP_FALSE); return; }
        if (num->value == BigNumber(1)) { emit_op(OpCode::OP_TRUE); return; }
    }
    if (ptr_cast<NullValue>(n->value)) { emit_op(OpCode::OP_NULL); return; }
    if (display_literal_depth_ > 0 &&
        ptr_cast<StringValue>(n->value)) {
        uint32_t idx = add_display_constant(n->value);
        current->func->display_constants.push_back(static_cast<int>(idx));
        emit_op_operand(OpCode::OP_CONSTANT, idx);
        return;
    }
    uint32_t idx = add_constant(n->value);
    emit_op_operand(OpCode::OP_CONSTANT, idx);
}


int32_t Compiler::table_constant(ListLiteralNode* n) {
    // Build the constant of a table of literals.
    //
    // The rows stay in the constant pool: the descriptor holds their
    // indices, and the runtime opens a row when the script reads it.  A
    // table of dialogue therefore never sits in memory as a whole.
    if (n->elements.empty()) return -1;
    std::vector<ValuePtr> index_values;
    for (const auto& el : n->elements) {
        int64_t slot = -1;
        if (auto lit = std::dynamic_pointer_cast<LiteralNode>(el)) {
            if (ptr_cast<StringValue>(lit->value)) {
                // A string in a table is a line the player reads: it gets
                // its own constant and the packaging step can watermark
                // it.
                uint32_t idx = add_display_constant(lit->value);
                current->func->display_constants.push_back(static_cast<int>(idx));
                slot = static_cast<int64_t>(idx);
            } else {
                slot = static_cast<int64_t>(add_constant(lit->value));
            }
        } else if (auto lst = std::dynamic_pointer_cast<ListLiteralNode>(el)) {
            int32_t nested = table_constant(lst.get());
            if (nested < 0) return -1;
            slot = static_cast<int64_t>(nested);
        } else {
            // Not a literal: the list is built the old way.
            return -1;
        }
        index_values.push_back(make_value<NumberValue>(BigNumber(slot)));
    }
    std::map<std::string, ValuePtr> descriptor;
    descriptor[table_key()] = make_value<LnValue>(index_values);
    return static_cast<int32_t>(add_constant(make_value<DimValue>(descriptor)));
}

void Compiler::compile_list_literal(ListLiteralNode* n) {
    int32_t table = table_constant(n);
    if (table >= 0) {
        // A table: one constant, and the rows are opened when they are
        // read.
        emit_op_operand(OpCode::OP_CONSTANT, static_cast<uint32_t>(table));
        return;
    }
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
    // Encode the target type as a stable cast code (see CastTypes.hpp).
    uint32_t cast_code = static_cast<uint32_t>(CastType::CAST_ANY);
    cast_code_for(n->type_keyword.type, cast_code);
    emit_op_operand(OpCode::OP_CAST, cast_code);
}

// Operand layout of the OP_INPLACE_* instructions: the single 24-bit
// operand packs the destination and the right-hand side.
//   bit 23: destination is a global (bits 22-12 hold its name constant index)
//   otherwise: destination is a local slot (bits 22-12)
//   bits 11-9: rhs kind - 0 constants-pool index, 1 local slot,
//              2 value already on the stack top, 3 inline small integer
//   bits  8-0: rhs payload (index or 0..511 value)
// Anything that does not fit falls back to the plain sequence.
static constexpr uint32_t INPLACE_GLOBAL_MASK = 0x800000;
static constexpr uint32_t INPLACE_DST_MASK    = 0x7FF800;
static constexpr uint32_t INPLACE_DST_SHIFT   = 12;
static constexpr uint32_t INPLACE_KIND_SHIFT  = 9;
static constexpr uint32_t INPLACE_PAY_MASK    = 0x01FF;
static constexpr uint32_t INPLACE_KIND_CONST  = 0;
static constexpr uint32_t INPLACE_KIND_LOCAL  = 1;
static constexpr uint32_t INPLACE_KIND_STACK  = 2;
static constexpr uint32_t INPLACE_KIND_INLINE = 3;

// Encodes one side of a fused arithmetic/compare instruction.
// bit23: global (bits 22-12 = name constant index) else local slot;
// bits 11-9: 0 constant, 1 local, 3 inline small integer; bits 8-0 payload.
// Returns false when the side cannot be encoded.
bool Compiler::encode_value_side(AstNode* node, bool allow_inline, uint32_t& out) {
    if (auto var = dynamic_cast<VariableNode*>(node)) {
        int slot = resolve_local(var->name);
        if (slot >= 0 && slot <= 511) {
            out = (1 << 9) | static_cast<uint32_t>(slot);   // kind 1 = local, payload = slot
            return true;
        }
        if (slot < 0 && resolve_upvalue(current, var->name) == -1) {
            uint32_t idx = add_string_constant(var->name);
            if (idx > 2047) return false;
            out = 0x800000 | ((idx & 0x7FF) << 12);         // kind 0 payload unused: name index rides in bits 22-12
            return true;
        }
        return false;
    }
    if (allow_inline) {
        if (auto lit = dynamic_cast<LiteralNode*>(node)) {
            if (auto num = ptr_cast<NumberValue>(lit->value)) {
                long long v = 0;
                if (num->value.is_int_fast(v) && v >= 0 && v <= 511) {
                    out = (3 << 9) | static_cast<uint32_t>(v);
                    return true;
                }
                uint32_t idx = add_constant(lit->value);
                if (idx > 511) return false;
                out = idx;
                return true;
            }
        }
    }
    return false;
}

int Compiler::try_compile_cmp_jump(AstNode* condition) {
    if (!allow_fusion) return -1;
    auto bin = dynamic_cast<BinaryOpNode*>(condition);
    if (!bin) return -1;
    uint32_t cmp;
    switch (bin->op.type) {
        case TokenType::LESS:          cmp = 0; break;
        case TokenType::LESS_EQUAL:    cmp = 1; break;
        case TokenType::GREATER:       cmp = 2; break;
        case TokenType::GREATER_EQUAL: cmp = 3; break;
        case TokenType::EQUAL_EQUAL:   cmp = 4; break;
        case TokenType::BANG_EQUAL:    cmp = 5; break;
        default: return -1;
    }
    // Both sides must be encodable before anything goes on the wire.
    uint32_t lenc, renc;
    if (!encode_value_side(bin->left.get(), false, lenc)) return -1;
    if (!encode_value_side(bin->right.get(), true, renc)) return -1;
    auto raw24 = [this](uint32_t v) {
        emit_byte(static_cast<uint8_t>((v >> 16) & 0xFF));
        emit_byte(static_cast<uint8_t>((v >> 8) & 0xFF));
        emit_byte(static_cast<uint8_t>(v & 0xFF));
    };
    emit_op(OpCode::OP_CMP_JUMP);
    raw24(lenc);
    raw24(renc);
    raw24(cmp);
    size_t patch_loc = current->func->code.size();
    raw24(0);                                     // target placeholder
    return static_cast<int>(patch_loc);
}

bool Compiler::try_compile_inplace(const std::string& name, AstNode* value, int line) {
    return try_compile_inplace_common(name, value, line, false);
}

bool Compiler::try_compile_inplace_np(const std::string& name, AstNode* value) {
    return try_compile_inplace_common(name, value, 0, true);
}

bool Compiler::try_compile_inplace_common(const std::string& name, AstNode* value, int line, bool no_push) {
    (void)line;
    if (!allow_fusion) return false;
    uint32_t dst;
    int slot = resolve_local(name);
    if (slot >= 0 && slot <= 1023) {
        dst = INPLACE_DST_SHIFT <= 22 ? (static_cast<uint32_t>(slot) << INPLACE_DST_SHIFT) : 0;
    } else if (slot < 0) {
        if (resolve_upvalue(current, name) != -1) return false;   // closure variable: plain path
        // Globals take the fused path too - that is what script-scope
        // variables actually are.
        uint32_t idx = add_string_constant(name);
        if (idx > 1023) return false;
        dst = INPLACE_GLOBAL_MASK | (idx << INPLACE_DST_SHIFT);
    } else {
        return false;
    }
    if (no_push) dst |= 1u << 22;   // no-push variant marker (bit 22; dst is bits 21-12)
    auto bin = dynamic_cast<BinaryOpNode*>(value);
    if (!bin) return false;
    OpCode op;
    switch (bin->op.type) {
        case TokenType::PLUS:  op = OpCode::OP_INPLACE_ADD; break;
        case TokenType::MINUS: op = OpCode::OP_INPLACE_SUB; break;
        case TokenType::STAR:  op = OpCode::OP_INPLACE_MUL; break;
        default: return false;
    }
    auto lhs = dynamic_cast<VariableNode*>(bin->left.get());
    if (!lhs || lhs->name != name) return false;

    uint32_t kind, pay;
    if (auto rvar = dynamic_cast<VariableNode*>(bin->right.get())) {
        int rslot = resolve_local(rvar->name);
        if (rslot < 0 || rslot > INPLACE_PAY_MASK) return false;   // global/upvalue rhs: plain path
        kind = INPLACE_KIND_LOCAL; pay = static_cast<uint32_t>(rslot);
    } else if (auto lit = dynamic_cast<LiteralNode*>(bin->right.get())) {
        if (auto num = ptr_cast<NumberValue>(lit->value)) {
            long long v = 0;
            if (num->value.is_int_fast(v) && v >= 0 && v <= INPLACE_PAY_MASK) {
                kind = INPLACE_KIND_INLINE; pay = static_cast<uint32_t>(v);
            } else {
                // Big or negative literal: via the constants pool when it fits.
                uint32_t idx = add_constant(lit->value);
                if (idx > INPLACE_PAY_MASK) return false;
                kind = INPLACE_KIND_CONST; pay = idx;
            }
        } else if (ptr_cast<StringValue>(lit->value)) {
            uint32_t idx = add_constant(lit->value);
            if (idx > INPLACE_PAY_MASK) return false;
            kind = INPLACE_KIND_CONST; pay = idx;
        } else {
            return false;
        }
    } else if (auto rbin = dynamic_cast<BinaryOpNode*>(bin->right.get())) {
        // Accumulator idiom: s = s + i * 3 (and s * i).
        if (rbin->op.type != TokenType::STAR) return false;
        VariableNode* rv = nullptr;
        LiteralNode* rlit = nullptr;
        if (auto rl = dynamic_cast<VariableNode*>(rbin->left.get())) {
            rv = rl; rlit = dynamic_cast<LiteralNode*>(rbin->right.get());
        } else if (auto rr = dynamic_cast<VariableNode*>(rbin->right.get())) {
            rv = rr; rlit = dynamic_cast<LiteralNode*>(rbin->left.get());
        }
        if (!rv || !rlit) return false;
        auto rnum = ptr_cast<NumberValue>(rlit->value);
        if (!rnum) return false;
        long long k = 0;
        if (op == OpCode::OP_INPLACE_ADD && rnum->value.is_int_fast(k) && k >= 0 && k <= 4095) {
            // dst += src * k in one instruction when the source is a
            // variable (local slot or global name).
            uint32_t senc;
            if (encode_value_side(rv, false, senc)) {
                emit_op(OpCode::OP_INPLACE_AM);
                auto raw24 = [this](uint32_t v) {
                    emit_byte(static_cast<uint8_t>((v >> 16) & 0xFF));
                    emit_byte(static_cast<uint8_t>((v >> 8) & 0xFF));
                    emit_byte(static_cast<uint8_t>(v & 0xFF));
                };
                raw24(dst);
                raw24(senc | static_cast<uint32_t>(k));
                return true;
            }
        }
        // General product: compile i * k the ordinary way, consume it
        // from the stack top.
        compile_expression(bin->right);
        kind = INPLACE_KIND_STACK; pay = 0;
    } else {
        return false;
    }

    uint32_t operand = dst | (kind << INPLACE_KIND_SHIFT) | pay;
    emit_op_operand(op, operand);
    return true;
}

void Compiler::compile_assignment(AssignmentNode* n) {
    // Handle different target types.
    if (auto var = dynamic_cast<VariableNode*>(n->target.get())) {
        // Fused in-place arithmetic: one instruction instead of four,
        // and no result object when the slot owns its number.
        if (try_compile_inplace(var->name, n->value.get(), n->line)) return;
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
    // obj.member(args) fuses into one instruction: the VM resolves the
    // member and calls it without materializing the bound function value.
    if (allow_fusion) {
    if (auto get = dynamic_cast<GetNode*>(n->callee.get())) {
        compile_expression(get->object);
        for (const auto& arg : n->arguments) {
            compile_expression(arg);
        }
        uint32_t name_idx = add_string_constant(get->name);
        emit_op(OpCode::OP_CALL_MEMBER);
        auto raw24 = [this](uint32_t v) {
            emit_byte(static_cast<uint8_t>((v >> 16) & 0xFF));
            emit_byte(static_cast<uint8_t>((v >> 8) & 0xFF));
            emit_byte(static_cast<uint8_t>(v & 0xFF));
        };
        raw24(name_idx);
        raw24(static_cast<uint32_t>(n->arguments.size()));
        return;
    }
    }
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
