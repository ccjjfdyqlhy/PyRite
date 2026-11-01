#include "VM.hpp"
#include "CastTypes.hpp"
#include "OpCodeInfo.hpp"
#include "Interpreter.hpp"
#include "msgs.hpp"
#include <sstream>
#include <cmath>
#include <chrono>
#include <iostream>

// A table descriptor holds the slots of its rows; the rows stay in the
// constant pool and are opened one at a time.  This builds the list the
// script asked for.
//
// A row of a table can be a table itself -- a table of rows is what a
// story keeps -- so this goes all the way down.  Without that, the
// inner rows stayed descriptors and the script read a dictionary where
// it asked for a list.
static ValuePtr build_table(CompiledFunction* fn,
                            const ValuePtr& value) {
    auto descriptor = ptr_cast<DimValue>(value);
    if (!descriptor) return value;
    auto found = descriptor->dict.find(table_key());
    if (found == descriptor->dict.end()) return value;
    auto rows = ptr_cast<LnValue>(found->second);
    if (!rows) return value;
    std::vector<ValuePtr> built;
    for (const auto& row : rows->elements) {
        auto slot_value = ptr_cast<NumberValue>(row);
        if (!slot_value) continue;
        size_t slot = static_cast<size_t>(slot_value->value.toLongLong());
        if (slot < fn->constants.size()) {
            built.push_back(build_table(fn, fn->constants[slot]));
        }
    }
    return make_value<LnValue>(built);
}

// ============================================================================
// Constructor
// ============================================================================
VM::VM() : had_error(false), interpreter(nullptr) {
    stack.reserve(256);
}

// ============================================================================
// Error handling
// ============================================================================
void VM::runtime_error(int line, const std::string& msg) {
    had_error = true;
    std::ostringstream oss;
    oss << ::msg(Msg::RUNTIME_PREFIX) << line << ": " << msg;
    error_message = oss.str();
    throw RuntimeError(line, msg);
}

void VM::set_error(int line, const std::string& msg) {
    had_error = true;
    std::ostringstream oss;
    oss << ::msg(Msg::RUNTIME_PREFIX) << line << ": " << msg;
    error_message = oss.str();
}

int VM::current_line() {
    if (frame_top == 0) return 0;
    CallFrame& f = frames[frame_top - 1];
    if (!f.function || f.function->code.empty()) return 0;
    size_t offset = static_cast<size_t>(f.ip - f.code_start);
    if (offset == 0) return 0;
    // ip points to the NEXT instruction; the current instruction is at offset-1.
    size_t idx = offset - 1;
    if (idx < f.function->lines.size()) return f.function->lines[idx];
    return 0;
}

bool VM::handle_exception(ValuePtr exc_value) {
    // Discard handlers registered by frames that have already returned:
    // they would otherwise catch exceptions from unrelated code.
    while (!try_handlers.empty() && try_handlers.back().frame_depth > (size_t)frame_top) {
        try_handlers.pop_back();
    }
    if (try_handlers.empty()) return false;
    TryHandler handler = try_handlers.back();
    try_handlers.pop_back();

    // Unwind frames to the handler's frame depth.
    while (frame_top > handler.frame_depth) {
        frame_top--;
        arena_top = frames[frame_top].locals_base;
    }
    if (frame_top == 0) return false;

    // Restore the stack to the depth it had when the try was entered,
    // then push the exception value.
    if (stack.size() > handler.stack_depth) {
        stack.resize(handler.stack_depth);
    }
    push(exc_value);

    // Jump to the catch handler.
    frames[frame_top - 1].ip = handler.catch_ip;
    return true;
}

// ============================================================================
// Shared immutable results. Comparisons can only produce these two
// numbers and null never varies, so the hot opcodes hand out references
// instead of allocating a fresh object every time.
static const ValuePtr BOOL_TRUE = make_value<NumberValue>(1);
static const ValuePtr BOOL_FALSE = make_value<NumberValue>(0);
static const ValuePtr NULL_SINGLETON = make_value<NullValue>();

// Truthiness
// ============================================================================
bool VM::is_truthy(const ValuePtr& v) {
    if (!v) return false;
    if (v->kind() == VTag::Number) return !static_cast<const NumberValue*>(v.get())->value.isZero();
    return v->isTruthy();
}

// Resolves one encoded side of a fused instruction: a cached global, a
// local slot, an inline small integer or a constants-pool entry.
void VM::resolve_side(CallFrame& frame, uint32_t enc, ValuePtr& out) {
    if (enc & 0x800000) {                     // global
        uint32_t idx = (enc >> 12) & 0x7FF;
        if (idx >= frame.function->gcache.size()) frame.function->gcache.resize(frame.function->constants.size(), nullptr);
        const ValuePtr* cell = frame.function->gcache[idx];
        if (!cell) {
            std::string name = ptr_cast<StringValue>(
                frame.function->constants[idx])->value;
            cell = globals->cell(name);
            frame.function->gcache[idx] = cell;
        }
        out = cell ? *cell : nullptr;
        return;
    }
    uint32_t kind = (enc >> 9) & 0x7;
    uint32_t pay = enc & 0x1FF;
    if (kind == 1) { out = frame.locals[pay]; return; }
    if (kind == 3) { out = intern_small((long long)pay); return; }
    out = frame.function->constants[pay];
}

// ============================================================================
// Run: main execution loop
// ============================================================================
bool VM::run(BytecodeChunkPtr c) {
    chunk = c;
    had_error = false;
    error_message.clear();
    stack.clear();
    open_upvalues.clear();

    // One arena for every frame's locals, sized once for the worst case
    // (FRAMES_MAX frames of the largest body): a call never allocates and
    // locals pointers stay valid for the whole run.
    size_t chunk_max_locals = 4;
    for (auto& fn : chunk->all_functions)
        if (fn->max_locals > (int)chunk_max_locals) chunk_max_locals = (size_t)fn->max_locals;
    arena.assign((size_t)FRAMES_MAX * chunk_max_locals + 16, nullptr);
    arena_top = 0;
    frame_top = 0;
    ClosurePtr script_closure = make_value<Closure>(chunk->script);
    CallFrame& f = frames[frame_top++];
    f.function = chunk->script.get();
    f.code_start = chunk->script->code.data();
    f.ip = f.code_start;
    f.upvalues = nullptr;
    f.upvalue_count = 0;
    f.locals_base = arena_top;
    f.nlocals = (uint32_t)(chunk->script->max_locals > 0 ? chunk->script->max_locals : chunk->script->local_names.size());
    f.locals = arena.data() + arena_top;
    arena_top += f.nlocals;

    const BigNumber* pb_r = nullptr;   // second operand of the _R ops

    // Main dispatch loop.
    // The outer loop allows resuming execution after a caught exception.
    while (true) {
    try {
    while (true) {
        CallFrame& frame = frames[frame_top - 1];
        uint8_t instruction = *frame.ip++;
        OpCode op = static_cast<OpCode>(instruction);

        switch (op) {
            // ---- Constants ----
            case OpCode::OP_CONSTANT: {
                uint32_t idx = read_operand24(frame.ip);
                frame.ip += 3;
                ValuePtr value = frame.function->constants[idx];
                // A table descriptor: the constant holds the slots of the
                // rows, and the list the script asked for is built here.
                // The JavaScript VM builds the same list lazily - the
                // values are the same, only the memory strategy differs.
                if (auto d = ptr_cast<DimValue>(value)) {
                    if (d->dict.count(table_key())) {
                        push(build_table(frame.function, value));
                        break;
                    }
                }
                push(value);
                break;
            }
            case OpCode::OP_NULL:
                push(NULL_SINGLETON);
                break;
            case OpCode::OP_TRUE:
                push(BOOL_TRUE);
                break;
            case OpCode::OP_FALSE:
                push(BOOL_FALSE);
                break;

            // ---- Stack ----
            case OpCode::OP_POP:
                pop();
                break;
            case OpCode::OP_DUP:
                push(peek());
                break;

            // ---- Locals ----
            case OpCode::OP_GET_LOCAL: {
                uint32_t idx = read_operand24(frame.ip);
                frame.ip += 3;
                push(frame.locals[idx]);
                break;
            }
            case OpCode::OP_SET_LOCAL: {
                uint32_t idx = read_operand24(frame.ip);
                frame.ip += 3;
                if (idx >= frame.nlocals) {
                    runtime_error(0, "SET_LOCAL: slot " + std::to_string(idx) +
                        " out of bounds (locals size=" + std::to_string(frame.nlocals) +
                        ", func=" + frame.function->name + ")");
                    return false;
                }
                frame.locals[idx] = pop();
                break;
            }
            case OpCode::OP_SET_LOCAL_KEEP: {
                uint32_t idx = read_operand24(frame.ip);
                frame.ip += 3;
                frame.locals[idx] = peek();
                break;
            }

            // ---- Globals ----
            case OpCode::OP_GET_GLOBAL: {
                uint32_t idx = read_operand24(frame.ip);
                frame.ip += 3;
                if (idx >= frame.function->gcache.size()) frame.function->gcache.resize(frame.function->constants.size(), nullptr);
                const ValuePtr* cell = frame.function->gcache[idx];
                if (!cell) {
                    // First execution of this call site: resolve the name
                    // once and remember where the value lives.
                    std::string name = ptr_cast<StringValue>(
                        frame.function->constants[idx])->value;
                    cell = globals->cell(name);
                    frame.function->gcache[idx] = cell;   // may stay null: still undefined
                }
                ValuePtr val = cell ? *cell : nullptr;
                if (!val) {
                    std::string name = ptr_cast<StringValue>(
                        frame.function->constants[idx])->value;
                    runtime_error(current_line(), fmt(Msg::UNDEFINED_VAR, name));
                    return false;
                }
                // Lazy-load module proxies.
                if (val->kind() == VTag::ModuleProxy) {
                    auto proxy = static_cast<ModuleProxy*>(val.get());
                    if (!proxy->loaded && interpreter) {
                        std::string name = ptr_cast<StringValue>(
                            frame.function->constants[idx])->value;
                        val = interpreter->load_module(proxy);
                        globals->assign(name, val);
                    }
                }
                push(val);
                break;
            }
            case OpCode::OP_SET_GLOBAL: {
                uint32_t idx = read_operand24(frame.ip);
                frame.ip += 3;
                if (idx >= frame.function->wcache.size()) frame.function->wcache.resize(frame.function->constants.size(), nullptr);
                ValuePtr* cell = frame.function->wcache[idx];
                if (!cell) {
                    std::string name = ptr_cast<StringValue>(
                        frame.function->constants[idx])->value;
                    cell = const_cast<ValuePtr*>(globals->cell(name));
                    if (!cell) cell = globals->cell_mut(name);
                    frame.function->wcache[idx] = cell;
                }
                *cell = peek();
                break;
            }
            case OpCode::OP_DEFINE_GLOBAL: {
                uint32_t idx = read_operand24(frame.ip);
                frame.ip += 3;
                if (idx >= frame.function->wcache.size()) frame.function->wcache.resize(frame.function->constants.size(), nullptr);
                ValuePtr* cell = frame.function->wcache[idx];
                if (!cell) {
                    std::string name = ptr_cast<StringValue>(
                        frame.function->constants[idx])->value;
                    cell = globals->cell_mut(name);
                    frame.function->wcache[idx] = cell;
                }
                *cell = pop();
                break;
            }

            // ---- Upvalues ----
            case OpCode::OP_GET_UPVALUE: {
                uint32_t idx = read_operand24(frame.ip);
                frame.ip += 3;
                push(*frame.upvalues[idx]->location);
                break;
            }
            case OpCode::OP_SET_UPVALUE: {
                uint32_t idx = read_operand24(frame.ip);
                frame.ip += 3;
                *frame.upvalues[idx]->location = peek();
                break;
            }
            case OpCode::OP_CLOSE_UPVALUE: {
                uint32_t slot = read_operand24(frame.ip);
                frame.ip += 3;
                close_upvalues(static_cast<int>(slot));
                break;
            }

            // ---- Collections ----
            case OpCode::OP_GET_INDEX: {
                ValuePtr index = pop();
                ValuePtr obj = pop();
                // Use the existing subscript mechanism via a temporary AST node
                // is too slow; instead, handle common cases directly.
                const VTag otag = obj->kind(), itag = index->kind();
                if (otag == VTag::Ln) {
                    auto ln = static_cast<LnValue*>(obj.get());
                    if (itag == VTag::Number) {
                        auto num = static_cast<NumberValue*>(index.get());
                        long long i = num->value.toLongLong();
                        long long len = static_cast<long long>(ln->elements.size());
                        if (i < 0) i += len;
                        if (i < 0 || i >= len) {
                            runtime_error(current_line(), msg(Msg::LN_IDX_OOB));
                            return false;
                        }
                        push(ln->elements[i]);
                    } else {
                        runtime_error(current_line(), msg(Msg::LN_IDX_NUM));
                        return false;
                    }
                } else if (otag == VTag::Dim) {
                    auto dm = static_cast<DimValue*>(obj.get());
                    if (itag == VTag::String) {
                        auto str = static_cast<StringValue*>(index.get());
                        auto it = dm->dict.find(str->value);
                        if (it == dm->dict.end()) {
                            runtime_error(current_line(), fmt(Msg::DIM_KEY_MISS, str->value));
                            return false;
                        }
                        push(it->second);
                    } else {
                        runtime_error(current_line(), msg(Msg::DIM_KEY_TYPE));
                        return false;
                    }
                } else if (otag == VTag::String) {
                    auto str = static_cast<StringValue*>(obj.get());
                    if (itag == VTag::Number) {
                        auto num = static_cast<NumberValue*>(index.get());
                        long long i = num->value.toLongLong();
                        long long len = static_cast<long long>(str->value.size());
                        if (i < 0) i += len;
                        if (i < 0 || i >= len) {
                            runtime_error(current_line(), msg(Msg::LN_IDX_OOB));
                            return false;
                        }
                        push(make_value<StringValue>(std::string(1, str->value[i])));
                    } else {
                        runtime_error(current_line(), msg(Msg::LN_IDX_NUM));
                        return false;
                    }
                } else {
                    runtime_error(current_line(), msg(Msg::NOT_SUBSCR));
                    return false;
                }
                break;
            }
            case OpCode::OP_SET_INDEX: {
                ValuePtr val = pop();
                ValuePtr index = pop();
                ValuePtr obj = pop();
                const VTag otag = obj->kind(), itag = index->kind();
                if (otag == VTag::Ln) {
                    auto ln = static_cast<LnValue*>(obj.get());
                    if (itag == VTag::Number) {
                        auto num = static_cast<NumberValue*>(index.get());
                        long long i = num->value.toLongLong();
                        long long len = static_cast<long long>(ln->elements.size());
                        if (i < 0) i += len;
                        if (i < 0 || i >= len) {
                            runtime_error(current_line(), msg(Msg::LN_IDX_OOB));
                            return false;
                        }
                        ln->elements[i] = val;
                    } else {
                        runtime_error(current_line(), msg(Msg::LN_IDX_NUM));
                        return false;
                    }
                } else if (otag == VTag::Dim) {
                    if (itag == VTag::String) {
                        static_cast<DimValue*>(obj.get())->dict[static_cast<StringValue*>(index.get())->value] = val;
                    } else {
                        runtime_error(current_line(), msg(Msg::DIM_KEY_TYPE));
                        return false;
                    }
                } else {
                    runtime_error(current_line(), msg(Msg::NOT_SUBSCR));
                    return false;
                }
                push(val);
                break;
            }
            case OpCode::OP_GET_PROPERTY: {
                uint32_t idx = read_operand24(frame.ip);
                frame.ip += 3;
                std::string name = ptr_cast<StringValue>(
                    frame.function->constants[idx])->value;
                ValuePtr obj = pop();
                // `.length` is a plain read on a container: no member value
                // needs to materialize for it.
                if (name == "length") {
                    VTag otag = obj->kind();
                    if (otag == VTag::Ln) {
                        push(make_value<NumberValue>(BigNumber(
                            (long long)static_cast<LnValue*>(obj.get())->elements.size())));
                        break;
                    }
                    if (otag == VTag::String) {
                        push(make_value<NumberValue>(BigNumber(
                            (long long)static_cast<StringValue*>(obj.get())->value.size())));
                        break;
                    }
                }
                if (auto inst = dynamic_cast<Instance*>(obj.get())) {
                    ValuePtr val = inst->get(name);
                    push(val);
                } else if (auto dm = dynamic_cast<DimValue*>(obj.get())) {
                    // A key of the dictionary comes first: a dictionary
                    // may hold a key called "keys".
                    auto it = dm->dict.find(name);
                    if (it != dm->dict.end()) push(it->second);
                    else if (auto member = dm->getMember(name)) push(member);
                    else push(make_value<NullValue>());
                } else if (auto member = obj->getMember(name)) {
                    // The members of a list or a string.
                    push(member);
                } else {
                    runtime_error(current_line(), fmt(Msg::NO_PROP, name));
                    return false;
                }
                break;
            }
            case OpCode::OP_SET_PROPERTY: {
                uint32_t idx = read_operand24(frame.ip);
                frame.ip += 3;
                std::string name = ptr_cast<StringValue>(
                    frame.function->constants[idx])->value;
                ValuePtr val = pop();
                ValuePtr obj = pop();
                if (auto inst = dynamic_cast<Instance*>(obj.get())) {
                    inst->set(name, val);
                } else if (auto dm = dynamic_cast<DimValue*>(obj.get())) {
                    dm->dict[name] = val;
                } else {
                    runtime_error(current_line(), msg(Msg::NO_SET_PROP));
                    return false;
                }
                push(val);
                break;
            }
            case OpCode::OP_BUILD_LIST: {
                uint32_t n = read_operand24(frame.ip);
                frame.ip += 3;
                auto ln = make_value<LnValue>(std::vector<ValuePtr>{});
                ln->elements.resize(n);
                for (uint32_t i = 0; i < n; ++i) {
                    ln->elements[n - 1 - i] = pop();
                }
                push(ln);
                break;
            }
            case OpCode::OP_BUILD_DICT: {
                uint32_t n = read_operand24(frame.ip);
                frame.ip += 3;
                auto dm = make_value<DimValue>();
                for (uint32_t i = 0; i < n; ++i) {
                    ValuePtr val = pop();
                    ValuePtr key = pop();
                    if (auto str = dynamic_cast<StringValue*>(key.get())) {
                        dm->dict[str->value] = val;
                    }
                }
                push(dm);
                break;
            }
            case OpCode::OP_GET_LEN: {
                ValuePtr obj = pop();
                if (auto ln = dynamic_cast<LnValue*>(obj.get())) {
                    push(make_value<NumberValue>(BigNumber(static_cast<long long>(ln->elements.size()))));
                } else if (auto dm = dynamic_cast<DimValue*>(obj.get())) {
                    push(make_value<NumberValue>(BigNumber(static_cast<long long>(dm->dict.size()))));
                } else if (auto str = dynamic_cast<StringValue*>(obj.get())) {
                    push(make_value<NumberValue>(BigNumber(static_cast<long long>(str->value.size()))));
                } else {
                    runtime_error(current_line(), "Argument to len() must be a string or a list.");
                    return false;
                }
                break;
            }
            case OpCode::OP_GET_SLICE: {
                ValuePtr step_v = pop();
                ValuePtr end_v = pop();
                ValuePtr start_v = pop();
                ValuePtr obj = pop();
                long long start = 0, end = 0, step = 1;
                bool start_null = !dynamic_cast<NumberValue*>(start_v.get());
                bool end_null = !dynamic_cast<NumberValue*>(end_v.get());
                bool step_null = !dynamic_cast<NumberValue*>(step_v.get());
                if (!start_null) start = ptr_cast<NumberValue>(start_v)->value.toLongLong();
                if (!end_null) end = ptr_cast<NumberValue>(end_v)->value.toLongLong();
                if (!step_null) step = ptr_cast<NumberValue>(step_v)->value.toLongLong();
                if (auto str = dynamic_cast<StringValue*>(obj.get())) {
                    long long len = static_cast<long long>(str->value.size());
                    if (start_null) start = 0;
                    if (end_null) end = len;
                    if (step_null) step = 1;
                    if (start < 0) start += len;
                    if (end < 0) end += len;
                    if (start < 0) start = 0;
                    if (end > len) end = len;
                    std::string result;
                    if (step > 0) {
                        for (long long i = start; i < end; i += step) result += str->value[i];
                    }
                    push(make_value<StringValue>(result));
                } else if (auto ln = dynamic_cast<LnValue*>(obj.get())) {
                    long long len = static_cast<long long>(ln->elements.size());
                    if (start_null) start = 0;
                    if (end_null) end = len;
                    if (step_null) step = 1;
                    if (start < 0) start += len;
                    if (end < 0) end += len;
                    if (start < 0) start = 0;
                    if (end > len) end = len;
                    auto result = make_value<LnValue>(std::vector<ValuePtr>{});
                    if (step > 0) {
                        for (long long i = start; i < end; i += step) result->elements.push_back(ln->elements[i]);
                    }
                    push(result);
                } else {
                    push(make_value<NullValue>());
                }
                break;
            }
            case OpCode::OP_SET_SLICE: {
                // Defer to AST for slice assignment (rare)
                runtime_error(0, "Slice assignment not supported in VM");
                return false;
            }

            // ---- Arithmetic ----
            case OpCode::OP_ADD: case OpCode::OP_SUB: case OpCode::OP_MUL:
            case OpCode::OP_DIV: case OpCode::OP_MOD: case OpCode::OP_POW: {
                ValuePtr b = pop();
                ValuePtr a = pop();
                push(binary_op(op, a, b, 0));
                break;
            }
            case OpCode::OP_CMP_JUMP: {
                uint32_t lenc = read_operand24(frame.ip); frame.ip += 3;
                uint32_t renc = read_operand24(frame.ip); frame.ip += 3;
                uint32_t cmp  = read_operand24(frame.ip); frame.ip += 3;
                uint32_t target = read_operand24(frame.ip); frame.ip += 3;
                // Resolve both sides.
                ValuePtr lv, rv;
                resolve_side(frame, lenc, lv);
                resolve_side(frame, renc, rv);
                bool cond = false;
                bool fast = lv && rv && lv->kind() == VTag::Number && rv->kind() == VTag::Number;
                if (fast) {
                    const BigNumber& x = static_cast<NumberValue*>(lv.get())->value;
                    const BigNumber& y = static_cast<NumberValue*>(rv.get())->value;
                    switch (cmp) {
                        case 0: cond = x <  y; break;
                        case 1: cond = x <= y; break;
                        case 2: cond = x >  y; break;
                        case 3: cond = x >= y; break;
                        case 4: cond = x == y; break;
                        default: cond = x != y; break;
                    }
                } else {
                    // Generic comparison with the exact stack shape of the
                    // unfused [expr] [JUMP_IF_FALSE] sequence.
                    if (!lv || !rv) { runtime_error(current_line(), "undefined variable"); return false; }
                    // Equality never goes through binary_op: it works on
                    // every value type via isEqualTo.
                    bool truth;
                    if (cmp == 4) {
                        truth = lv->isEqualTo(*rv);
                    } else if (cmp == 5) {
                        truth = !lv->isEqualTo(*rv);
                    } else {
                        OpCode cop = cmp == 0 ? OpCode::OP_LESS : cmp == 1 ? OpCode::OP_LESS_EQUAL
                                   : cmp == 2 ? OpCode::OP_GREATER : OpCode::OP_GREATER_EQUAL;
                        ValuePtr res = binary_op(cop, lv, rv, current_line());
                        truth = is_truthy(res);
                    }
                    if (truth) break;                   // fall through to body
                    frame.ip = frame.code_start + target;
                    break;
                }
                if (cond) {
                    // Fall through with no stack traffic at all.
                } else {
                    // The value rides the stack into the false exit, where
                    // the unfused epilogue's POP expects it.
                    push(BOOL_FALSE);
                    frame.ip = frame.code_start + target;
                }
                break;
            }
            case OpCode::OP_ADD_R: case OpCode::OP_SUB_R: case OpCode::OP_MUL_R: {
                // word1: gbit(23) | dst(22-13) | a_kind(12-11) | a_pay(10-0)
                // word2: b_kind(23-22) | b_pay(21-11)
                uint32_t w1 = read_operand24(frame.ip); frame.ip += 3;
                uint32_t w2 = read_operand24(frame.ip); frame.ip += 3;
                bool dglobal = w1 & 0x800000;
                uint32_t dst = (w1 >> 13) & 0x3FF;
                // aenc/benc are 12-bit words: kind(11-10) + pay(9-0)
                uint32_t akind = (w1 >> 10) & 0x3;
                uint32_t apay = w1 & 0x3FF;
                uint32_t bkind = (w2 >> 10) & 0x3;
                uint32_t bpay = w2 & 0x3FF;
                // Resolve both sides.
                const BigNumber* pa;
                BigNumber tmp_a, tmp_b;
                if (akind == 0) {
                    if (dst >= frame.nlocals && false) {}
                    if (apay >= frame.nlocals) { runtime_error(current_line(), "r-op: bad slot"); return false; }
                    ValuePtr& r = frame.locals[apay];
                    if (r->kind() != VTag::Number) goto r_slow;
                    pa = &static_cast<NumberValue*>(r.get())->value;
                } else if (akind == 1) { tmp_a = BigNumber((long long)apay); pa = &tmp_a; }
                else {
                    const ValuePtr* c = global_read_cell(frame, apay);
                    ValuePtr v = c ? *c : nullptr;
                    if (!v || v->kind() != VTag::Number) goto r_slow;
                    pa = &static_cast<NumberValue*>(v.get())->value;
                }
                if (bkind == 0) {
                    if (bpay >= frame.nlocals) { runtime_error(current_line(), "r-op: bad slot"); return false; }
                    ValuePtr& r = frame.locals[bpay];
                    if (r->kind() != VTag::Number) goto r_slow;
                    pb_r = &static_cast<NumberValue*>(r.get())->value;
                } else if (bkind == 1) { tmp_b = BigNumber((long long)bpay); pb_r = &tmp_b; }
                else {
                    const ValuePtr* c = global_read_cell(frame, bpay);
                    ValuePtr v = c ? *c : nullptr;
                    if (!v || v->kind() != VTag::Number) goto r_slow;
                    pb_r = &static_cast<NumberValue*>(v.get())->value;
                }
                {
                    // Destination: local slot or cached global cell.
                    ValuePtr* dslot; ValuePtr* dcell = nullptr;
                    if (dglobal) dcell = global_write_cell(frame, dst);
                    else {
                        if (dst >= frame.nlocals) { runtime_error(current_line(), "r-op: bad dst"); return false; }
                        dslot = &frame.locals[dst];
                    }
                    ValuePtr old = dglobal ? *dcell : *dslot;
                    BigNumber result;
                    if (op == OpCode::OP_ADD_R) result = *pa + *pb_r;
                    else if (op == OpCode::OP_SUB_R) result = *pa - *pb_r;
                    else result = *pa * *pb_r;
                    if (old && old->kind() == VTag::Number) {
                        // rewrite in place when the slot owns the object
                        if (dglobal) *dcell = nullptr; else *dslot = nullptr;
                        if (old.use_count() == 1) {
                            static_cast<NumberValue*>(old.get())->value = std::move(result);
                            if (dglobal) *dcell = old; else *dslot = old;
                            break;
                        }
                        if (dglobal) *dcell = old; else *dslot = old;
                    }
                    long long iv;
            ValuePtr fresh = (result.is_int_fast(iv) && iv >= -4096 && iv <= 4095)
                ? intern_small(iv) : make_value<NumberValue>(std::move(result));
                    if (dglobal) *dcell = fresh; else *dslot = fresh;
                    break;
                }
                r_slow: ;
                // Non-number sides: rebuild the plain sequence.
                {
                    ValuePtr av, bv;
                    if (akind == 0) av = frame.locals[apay];
                    else if (akind == 1) av = make_value<NumberValue>(BigNumber((long long)apay));
                    else { const ValuePtr* c = global_read_cell(frame, apay); av = c ? *c : nullptr; }
                    if (bkind == 0) bv = frame.locals[bpay];
                    else if (bkind == 1) bv = make_value<NumberValue>(BigNumber((long long)bpay));
                    else { const ValuePtr* c = global_read_cell(frame, bpay); bv = c ? *c : nullptr; }
                    if (!av || !bv) { runtime_error(current_line(), "undefined variable"); return false; }
                    OpCode plain = op == OpCode::OP_ADD_R ? OpCode::OP_ADD
                                 : op == OpCode::OP_SUB_R ? OpCode::OP_SUB : OpCode::OP_MUL;
                    ValuePtr res = binary_op(plain, av, bv, current_line());
                    bool dglobal2 = w1 & 0x800000;
                    uint32_t dst2 = (w1 >> 13) & 0x3FF;
                    if (dglobal2) *global_write_cell(frame, dst2) = res;
                    else frame.locals[dst2] = res;
                    break;
                }
            }
            case OpCode::OP_RETURN_R: {
                uint32_t enc = read_operand24(frame.ip); frame.ip += 3;
                // operand: gbit(23) | kind(12-11) | pay(10-0); kind 0 local, 2 global
                ValuePtr result;
                if (((enc >> 11) & 0x3) == 2) {
                    const ValuePtr* c = global_read_cell(frame, enc & 0x7FF);
                    result = c ? *c : nullptr;
                } else {
                    uint32_t slot = enc & 0x7FF;
                    if (slot >= frame.nlocals) { runtime_error(current_line(), "return: bad slot"); return false; }
                    result = frame.locals[slot];
                }
                if (!result) { runtime_error(current_line(), "undefined variable"); return false; }
                close_upvalues(0);
                while (!try_handlers.empty() && try_handlers.back().frame_depth >= (size_t)frame_top) {
                    try_handlers.pop_back();
                }
                bool was_script = (frame_top == 1);
                if (!was_script) {
                    CallFrame& rf = frames[frame_top - 1];
                    for (uint32_t k = 0; k < rf.nlocals; ++k) rf.locals[k] = nullptr;
                    arena_top = rf.locals_base;
                }
                frame_top--;
                if (was_script) return true;
                push(std::move(result));
                break;
            }
            case OpCode::OP_CALLR: {
                // word1: gbit(23) | kind(22-21) | pay(20-9) | nargs(8-0)
                // words: two arg descriptors each: kind(13-12) + pay(11-0)
                uint32_t w1 = read_operand24(frame.ip); frame.ip += 3;
                uint32_t nargs = w1 & 0x1FF;
                uint32_t ckind = (w1 >> 21) & 0x3;
                uint32_t cpay = (w1 >> 9) & 0xFFF;
                ValuePtr callee;
                if (ckind == 1) {
                    if (cpay >= frame.nlocals) { runtime_error(current_line(), "callr: bad slot"); return false; }
                    callee = frame.locals[cpay];
                } else {
                    const ValuePtr* c = global_read_cell(frame, cpay);
                    callee = c ? *c : nullptr;
                }
                if (!callee) { runtime_error(current_line(), "undefined variable"); return false; }
                uint32_t desc[4] = {0x3000, 0x3000, 0x3000, 0x3000};   // kind 3 = absent
                for (uint32_t w = 0; w < 4; w += 2) {   // fixed two descriptor words
                    uint32_t word = read_operand24(frame.ip); frame.ip += 3;
                    desc[w] = word >> 12;
                    desc[w + 1] = word & 0xFFF;
                }
                // Resolve the arguments (kind 3 = absent filler).
                ValuePtr argv[4];
                for (uint32_t i = 0; i < nargs; ++i) {
                    uint32_t kind = (desc[i] >> 10) & 0x3;
                    uint32_t pay = desc[i] & 0x3FF;
                    if (kind == 0) {
                        if (pay >= frame.nlocals) { runtime_error(current_line(), "callr: bad arg slot"); return false; }
                        argv[i] = frame.locals[pay];
                    } else if (kind == 1) {
                        argv[i] = intern_small((long long)pay);
                    } else if (kind == 2) {
                        const ValuePtr* c = global_read_cell(frame, pay);
                        argv[i] = c ? *c : nullptr;
                    } else {
                        runtime_error(current_line(), "callr: bad arg"); return false;
                    }
                    if (!argv[i]) { runtime_error(current_line(), "undefined variable"); return false; }
                }
                if (callee->kind() == VTag::Closure) {
                    push_frame_from_slots(static_cast<Closure*>(callee.get()), argv, nargs, current_line());
                } else {
                    std::vector<ValuePtr> native_args(argv, argv + nargs);
                    if (!call_value(callee, native_args, current_line())) return false;
                }
                break;
            }
            case OpCode::OP_INPLACE_AM: {
                uint32_t packed_dst = read_operand24(frame.ip); frame.ip += 3;
                uint32_t packed_src = read_operand24(frame.ip); frame.ip += 3;
                // dst += src * k for numbers; anything else falls back to
                // the exact unfused sequence.
                bool dst_global = packed_dst & 0x800000;
                bool no_push = packed_dst & 0x400000;
                uint32_t dsti = (packed_dst >> 12) & 0x3FF;
                bool src_global = packed_src & 0x800000;
                (void)0;
                uint32_t srci = (packed_src >> 12) & 0x7FF;
                long long k = (long long)(packed_src & 0xFFF);
                ValuePtr* dcell = nullptr; const ValuePtr* scell = nullptr;
                ValuePtr* dslot = nullptr; ValuePtr* sslot = nullptr;
                if (dst_global) { dcell = global_write_cell(frame, dsti); }
                else { if (dsti >= frame.nlocals) { runtime_error(current_line(), "inplace: bad slot"); return false; } dslot = &frame.locals[dsti]; }
                if (src_global) { scell = global_read_cell(frame, srci); }
                else { if (srci >= frame.nlocals) { runtime_error(current_line(), "inplace: bad rhs slot"); return false; } sslot = &frame.locals[srci]; }
                ValuePtr a = dst_global ? *dcell : *dslot;
                ValuePtr sv = src_global ? (scell ? *scell : nullptr) : *sslot;
                if (a && sv && a->kind() == VTag::Number && sv->kind() == VTag::Number) {
                    NumberValue* an = static_cast<NumberValue*>(a.get());
                    // Machine-int fast path: dst += src * k in one __int128
                    // step, one result object (or an intern table hit).
                    long long ia, isrc;
                    if (an->value.is_int_fast(ia) &&
                        static_cast<NumberValue*>(sv.get())->value.is_int_fast(isrc)) {
                        __int128 r = (__int128)ia + (__int128)isrc * k;
                        if (r <= (__int128)LLONG_MAX && r >= (__int128)LLONG_MIN) {
                            long long rv = (long long)r;
                            ValuePtr fresh = (rv >= -4096 && rv <= 4095)
                                ? intern_small(rv) : make_value<NumberValue>(BigNumber(rv));
                            if (dst_global) *dcell = fresh; else *dslot = fresh;
                            if (!no_push) push(fresh);
                            break;
                        }
                    }
                    BigNumber prod = static_cast<NumberValue*>(sv.get())->value * BigNumber(k);
                    BigNumber result = an->value + prod;
                    if (dst_global) *dcell = nullptr; else *dslot = nullptr;
                    if (a.use_count() == 1) {
                        an->value = std::move(result);
                        if (dst_global) *dcell = a; else *dslot = a;
                        if (!no_push) push(a);
                    } else {
                        long long iv;
            ValuePtr fresh = (result.is_int_fast(iv) && iv >= -4096 && iv <= 4095)
                ? intern_small(iv) : make_value<NumberValue>(std::move(result));
                        if (dst_global) *dcell = fresh; else *dslot = fresh;
                        if (!no_push) push(fresh);
                    }
                    break;
                }
                // Fallback: [dst] [src] [k] [MUL] [ADD] [SET keep]
                if (!a || !sv) { runtime_error(current_line(), "undefined variable"); return false; }
                push(a);
                push(sv);
                push(make_value<NumberValue>(BigNumber(k)));
                {
                    ValuePtr kb = pop(); ValuePtr sb = pop();
                    push(binary_op(OpCode::OP_MUL, sb, kb, current_line()));
                    ValuePtr mb = pop(); ValuePtr ab = pop();
                    ValuePtr res = binary_op(OpCode::OP_ADD, ab, mb, current_line());
                    if (dst_global) *dcell = res; else *dslot = res;
                    if (!no_push) push(res);
                }
                break;
            }
            case OpCode::OP_INPLACE_ADD: case OpCode::OP_INPLACE_SUB:
            case OpCode::OP_INPLACE_MUL: {
                uint32_t packed = read_operand24(frame.ip);
                frame.ip += 3;
                if (!exec_inplace(op, packed, frame, 0)) {
                    // Fallback: the plain [load dst] [load rhs] [op] [keep]
                    // sequence, byte-for-byte what the unfused compiler emit
                    // would have produced.
                    uint32_t dst = (packed >> 12) & 0x3FF;
                    bool dst_global = packed & 0x800000;
                    bool np = packed & 0x400000;
                    uint32_t rkind = (packed >> 9) & 0x7;
                    uint32_t rpay = packed & 0x1FF;
                    ValuePtr stack_rhs;
                    if (rkind == 2) stack_rhs = pop();
                    ValuePtr lhs;
                    if (dst_global) {
                        if (dst >= frame.function->wcache.size()) frame.function->wcache.resize(frame.function->constants.size(), nullptr);
                        ValuePtr* cell = frame.function->wcache[dst];
                        if (!cell) {
                            std::string name = ptr_cast<StringValue>(
                                frame.function->constants[dst])->value;
                            cell = const_cast<ValuePtr*>(globals->cell(name));
                            if (!cell) cell = globals->cell_mut(name);
                            frame.function->wcache[dst] = cell;
                        }
                        lhs = *cell;
                        push(lhs);
                    } else {
                        lhs = frame.locals[dst];
                        push(lhs);
                    }
                    if (rkind == 1) push(frame.locals[rpay]);
                    else if (rkind == 2) push(stack_rhs);
                    else if (rkind == 3) push(intern_small((long long)rpay));
                    else push(frame.function->constants[rpay]);
                    ValuePtr rb2 = pop();
                    ValuePtr ra2 = pop();
                    OpCode plain = op == OpCode::OP_INPLACE_ADD ? OpCode::OP_ADD
                                 : op == OpCode::OP_INPLACE_SUB ? OpCode::OP_SUB : OpCode::OP_MUL;
                    ValuePtr res = binary_op(plain, ra2, rb2, 0);
                    // The slot store always happens; only the leftover
                    // stack copy is skipped for the no-push variant.
                    if (dst_global) {
                        if (dst >= frame.function->wcache.size()) frame.function->wcache.resize(frame.function->constants.size(), nullptr);
                        ValuePtr* cell = frame.function->wcache[dst];
                        if (!cell) {
                            std::string name = ptr_cast<StringValue>(
                                frame.function->constants[dst])->value;
                            cell = const_cast<ValuePtr*>(globals->cell(name));
                            if (!cell) cell = globals->cell_mut(name);
                            frame.function->wcache[dst] = cell;
                        }
                        *cell = res;
                    } else {
                        frame.locals[dst] = res;
                    }
                    if (!np) push(res);
                }
                break;
            }
            case OpCode::OP_NEGATE: {
                ValuePtr a = pop();
                push(unary_op(op, a, 0));
                break;
            }
            case OpCode::OP_NOT: {
                ValuePtr a = pop();
                push(is_truthy(a) ? BOOL_FALSE : BOOL_TRUE);
                break;
            }

            // ---- Comparisons ----
            case OpCode::OP_EQUAL: {
                ValuePtr b = pop();
                ValuePtr a = pop();
                push(a->isEqualTo(*b) ? BOOL_TRUE : BOOL_FALSE);
                break;
            }
            case OpCode::OP_NOT_EQUAL: {
                ValuePtr b = pop();
                ValuePtr a = pop();
                push(a->isEqualTo(*b) ? BOOL_FALSE : BOOL_TRUE);
                break;
            }
            case OpCode::OP_LESS: case OpCode::OP_LESS_EQUAL:
            case OpCode::OP_GREATER: case OpCode::OP_GREATER_EQUAL: {
                ValuePtr b = pop();
                ValuePtr a = pop();
                push(binary_op(op, a, b, 0));
                break;
            }

            // ---- Control flow ----
            case OpCode::OP_JUMP: {
                uint32_t target = read_operand24(frame.ip);
                frame.ip = frame.code_start + target;
                break;
            }
            case OpCode::OP_JUMP_IF_FALSE: {
                uint32_t target = read_operand24(frame.ip);
                frame.ip += 3;
                if (!is_truthy(peek())) frame.ip = frame.code_start + target;
                break;
            }
            case OpCode::OP_JUMP_IF_TRUE: {
                uint32_t target = read_operand24(frame.ip);
                frame.ip += 3;
                if (is_truthy(peek())) frame.ip = frame.code_start + target;
                break;
            }
            case OpCode::OP_JUMP_IF_FALSE_KEEP: {
                uint32_t target = read_operand24(frame.ip);
                frame.ip += 3;
                if (!is_truthy(peek())) frame.ip = frame.code_start + target;
                break;
            }
            case OpCode::OP_JUMP_IF_TRUE_KEEP: {
                uint32_t target = read_operand24(frame.ip);
                frame.ip += 3;
                if (is_truthy(peek())) frame.ip = frame.code_start + target;
                break;
            }
            case OpCode::OP_LOOP: {
                uint32_t target = read_operand24(frame.ip);
                frame.ip = frame.code_start + target;
                break;
            }

            // ---- Functions ----
            case OpCode::OP_CLOSURE: {
                uint32_t fn_idx = read_operand24(frame.ip);
                frame.ip += 3;
                CompiledFunctionPtr fn = chunk->all_functions[fn_idx];
                ClosurePtr closure = make_value<Closure>(fn);
                closure->upvalues.resize(fn->upvalue_count);
                // Capture upvalues
                for (int i = 0; i < fn->upvalue_count; ++i) {
                    const auto& desc = fn->upvalue_descriptors[i];
                    if (desc.is_local) {
                        closure->upvalues[i] = capture_upvalue(desc.index);
                    } else {
                        if (desc.index < static_cast<int>(frame.upvalue_count)) {
                            closure->upvalues[i] = frame.upvalues[desc.index];
                        }
                    }
                }
                push(closure);
                break;
            }
            case OpCode::OP_CALL: {
                uint32_t arg_count = read_operand24(frame.ip);
                frame.ip += 3;
                // Args are on the stack: [callee, arg1, ..., argN]
                ValuePtr callee = peek(arg_count);
                (void)0;
                if (callee->kind() == VTag::Closure) {
                    // The callee stays on the stack (keeping the closure
                    // alive) while the frame copies what it needs; only
                    // then is it popped.
                    push_frame_from_stack(static_cast<Closure*>(callee.get()), arg_count, 0);
                    for (uint32_t i = 0; i <= arg_count; ++i) pop();
                } else {
                    // Native/AST callee: collect the arguments from the
                    // stack before popping anything.
                    const size_t base = stack.size() - arg_count;
                    std::vector<ValuePtr> native_args(stack.begin() + base, stack.end());
                    for (uint32_t i = 0; i <= arg_count; ++i) pop();
                    if (!call_value(callee, native_args, 0)) return false;
                }
                break;
            }
            case OpCode::OP_CALL_MEMBER: {
                uint32_t name_idx = read_operand24(frame.ip); frame.ip += 3;
                uint32_t arg_count = read_operand24(frame.ip); frame.ip += 3;
                std::string name = ptr_cast<StringValue>(
                    frame.function->constants[name_idx])->value;
                ValuePtr obj = peek(arg_count);
                // Hot container members resolve and run inline: the
                // generic path below materializes a bound native value
                // (one allocation) per call, which is what loops feel.
                const size_t call_base = stack.size() - arg_count;
                VTag otag = obj->kind();
                if (otag == VTag::Ln) {
                    auto* lv = static_cast<LnValue*>(obj.get());
                    if (name == "length" && arg_count == 0) {
                        stack.resize(call_base - 1);
                        push(make_value<NumberValue>(BigNumber((long long)lv->elements.size())));
                        break;
                    }
                    if ((name == "push" || name == "append") && arg_count == 1) {
                        lv->elements.push_back(stack.back());
                        stack.resize(call_base - 1);
                        push(make_value<NumberValue>(BigNumber((long long)lv->elements.size())));
                        break;
                    }
                    if (name == "pop" && arg_count == 0) {
                        ValuePtr out;
                        if (lv->elements.empty()) out = make_value<NullValue>();
                        else { out = lv->elements.back(); lv->elements.pop_back(); }
                        stack.resize(call_base - 1);
                        push(out);
                        break;
                    }
                } else if (otag == VTag::Dim) {
                    if (name == "has" && arg_count == 1 && stack.back()->kind() == VTag::String) {
                        bool hit = static_cast<DimValue*>(obj.get())->dict.count(
                            static_cast<StringValue*>(stack.back().get())->value) != 0;
                        stack.resize(call_base - 1);
                        push(hit ? BOOL_TRUE : BOOL_FALSE);
                        break;
                    }
                } else if (otag == VTag::String && name == "length" && arg_count == 0) {
                    stack.resize(call_base - 1);
                    push(make_value<NumberValue>(BigNumber(
                        (long long)static_cast<StringValue*>(obj.get())->value.size())));
                    break;
                }
                // Same member resolution as OP_GET_PROPERTY: instance
                // methods and dict keys come first, then value members.
                ValuePtr member;
                if (obj->kind() == VTag::Instance) {
                    member = static_cast<Instance*>(obj.get())->get(name);
                } else if (obj->kind() == VTag::Dim) {
                    auto dm = static_cast<DimValue*>(obj.get());
                    auto it = dm->dict.find(name);
                    if (it != dm->dict.end()) member = it->second;
                    else member = dm->getMember(name);
                } else {
                    member = obj->getMember(name);
                }
                if (!member) {
                    runtime_error(current_line(), fmt(Msg::NO_PROP, name));
                    return false;
                }
                // Native and AST callees need the args as a vector: move
                // them off the stack first. A closure callee reads them
                // straight off the stack, so nothing is touched before it.
                const size_t base = stack.size() - arg_count;
                if (member->kind() == VTag::NativeFn) {
                    std::vector<ValuePtr> native_args(
                        std::make_move_iterator(stack.begin() + base),
                        std::make_move_iterator(stack.end()));
                    stack.resize(base - 1);        // drop the args and obj
                    push(static_cast<NativeFnValue*>(member.get())->fn(native_args));
                } else if (member->kind() == VTag::Closure) {
                    push_frame_from_stack(static_cast<Closure*>(member.get()), arg_count, current_line());
                    for (uint32_t i = 0; i <= arg_count; ++i) pop();  // arg slots (moved out) + obj
                } else {
                    std::vector<ValuePtr> native_args(
                        std::make_move_iterator(stack.begin() + base),
                        std::make_move_iterator(stack.end()));
                    stack.resize(base - 1);
                    if (!call_value(member, native_args, current_line())) return false;
                }
                break;
            }

            case OpCode::OP_RETURN: {
                ValuePtr result = pop();
                close_upvalues(0);  // close all upvalues pointing to this frame
                while (!try_handlers.empty() && try_handlers.back().frame_depth >= (size_t)frame_top) {
                    try_handlers.pop_back();   // handler lived in the returning frame
                }
                bool was_script = (frame_top == 1);
                if (!was_script) {
                    // Roll the arena back. Slots are not re-initialized on
                    // entry (the compiler emits an assignment before any
                    // read), so stale references here are never observed -
                    // they only delay reclamation until the slot is reused.
                    CallFrame& rf = frames[frame_top - 1];
                    arena_top = rf.locals_base;
                }
                frame_top--;
                if (was_script) return true;
                push(std::move(result));
                break;
            }
            case OpCode::OP_RETURN_NULL: {
                close_upvalues(0);
                while (!try_handlers.empty() && try_handlers.back().frame_depth >= (size_t)frame_top) {
                    try_handlers.pop_back();
                }
                bool was_script = (frame_top == 1);
                if (!was_script) {
                    CallFrame& rf = frames[frame_top - 1];
                    arena_top = rf.locals_base;
                }
                frame_top--;
                if (was_script) return true;
                push(NULL_SINGLETON);
                break;
            }

            // ---- I/O ----
            case OpCode::OP_PRINT: {
                ValuePtr val = pop();
                std::cout << val->toString() << std::endl;
                break;
            }
            case OpCode::OP_INPUT: {
                ValuePtr promptVal = pop();
                std::string prompt;
                if (auto s = ptr_cast<StringValue>(promptVal)) {
                    prompt = s->value;
                } else {
                    prompt = promptVal->toString();
                }
                std::cout << prompt;
                std::string line;
                std::getline(std::cin, line);
                push(make_value<StringValue>(line));
                break;
            }

            // ---- Type cast ----
            case OpCode::OP_CAST: {
                uint32_t idx = read_operand24(frame.ip);
                frame.ip += 3;
                CastType type = static_cast<CastType>(idx);
                ValuePtr val = pop();
                // Perform type conversion, mirroring the AST interpreter's
                // TypeConversionNode::accept logic (for `as` casts) and
                // VarDeclarationNode::accept logic (for variable declarations).
                if (type == CastType::CAST_DEC) {
                    if (dynamic_cast<NumberValue*>(val.get())) {
                        push(val);
                    } else if (auto s = dynamic_cast<StringValue*>(val.get())) {
                        try { push(make_value<NumberValue>(BigNumber(s->value))); }
                        catch (const std::invalid_argument&) {
                            runtime_error(current_line(), std::string("Cannot convert string '") + s->value + "' to a number.");
                            return false;
                        }
                    } else if (auto b = dynamic_cast<BinaryValue*>(val.get())) {
                        push(make_value<NumberValue>(b->toBigNumber()));
                    } else if (dynamic_cast<NullValue*>(val.get())) {
                        // `as dec` on null fails, like the AST interpreter.
                        runtime_error(current_line(), "Unsupported conversion to 'dec'.");
                        return false;
                    } else {
                        runtime_error(current_line(), "Unsupported conversion to 'dec'.");
                        return false;
                    }
                } else if (type == CastType::CAST_DEC_DECL) {
                    // Declaration of a dec: strict, but null becomes 0.
                    if (dynamic_cast<NumberValue*>(val.get())) {
                        push(val);
                    } else if (auto s2 = dynamic_cast<StringValue*>(val.get())) {
                        try { push(make_value<NumberValue>(BigNumber(s2->value))); }
                        catch (const std::invalid_argument&) {
                            runtime_error(current_line(), std::string("Cannot convert string '") + s2->value + "' to a number.");
                            return false;
                        }
                    } else if (auto b2 = dynamic_cast<BinaryValue*>(val.get())) {
                        push(make_value<NumberValue>(b2->toBigNumber()));
                    } else if (dynamic_cast<NullValue*>(val.get())) {
                        push(make_value<NumberValue>(BigNumber(0)));
                    } else if (dynamic_cast<LnValue*>(val.get()) || dynamic_cast<DimValue*>(val.get())) {
                        // Containers are what strictness is about: dec is
                        // the language's default type and legitimately
                        // holds functions, closures and instances.
                        runtime_error(current_line(), msg(Msg::DEC_INIT_NUM));
                        return false;
                    } else {
                        push(val);
                    }
                } else if (type == CastType::CAST_BIN_DECL) {
                    // Declaration of a bin: strict, but null becomes 00.
                    if (dynamic_cast<BinaryValue*>(val.get())) {
                        push(val);
                    } else if (auto s3 = dynamic_cast<StringValue*>(val.get())) {
                        try { push(make_value<BinaryValue>(s3->value)); }
                        catch (...) {
                            runtime_error(current_line(), std::string("Cannot convert string '") + s3->value + "' to binary. Expected '0x...' format.");
                            return false;
                        }
                    } else if (dynamic_cast<NullValue*>(val.get())) {
                        push(make_value<BinaryValue>(std::vector<uint8_t>{0}));
                    } else {
                        runtime_error(current_line(), msg(Msg::BIN_INIT_BIN));
                        return false;
                    }
                } else if (type == CastType::CAST_STR) {
                    push(make_value<StringValue>(val->toString()));
                } else if (type == CastType::CAST_BIN) {
                    if (dynamic_cast<BinaryValue*>(val.get())) {
                        push(val);
                    } else if (auto s = dynamic_cast<StringValue*>(val.get())) {
                        try { push(make_value<BinaryValue>(s->value)); }
                        catch (...) {
                            runtime_error(current_line(), std::string("Cannot convert string '") + s->value + "' to binary. Expected '0x...' format.");
                            return false;
                        }
                    } else if (dynamic_cast<NullValue*>(val.get())) {
                        push(make_value<BinaryValue>(std::vector<uint8_t>{0}));
                    } else {
                        runtime_error(current_line(), "Unsupported conversion to 'bin'.");
                        return false;
                    }
                } else if (type == CastType::CAST_LN) {
                    if (dynamic_cast<LnValue*>(val.get())) {
                        push(val);
                    } else if (dynamic_cast<NullValue*>(val.get())) {
                        push(make_value<LnValue>(std::vector<ValuePtr>{}));
                    } else {
                        runtime_error(current_line(), "Unsupported conversion to 'ln'.");
                        return false;
                    }
                } else if (type == CastType::CAST_DIM) {
                    if (dynamic_cast<DimValue*>(val.get())) {
                        push(val);
                    } else if (dynamic_cast<NullValue*>(val.get())) {
                        push(make_value<DimValue>());
                    } else {
                        push(make_value<DimValue>());
                    }
                } else {
                    // ANY or unknown: keep value as-is
                    push(val);
                }
                break;
            }

            // ---- Swap ----
            case OpCode::OP_SWAP: {
                ValuePtr b = pop();
                ValuePtr a = pop();
                push(b);
                push(a);
                break;
            }

            // ---- AST fallback ----
            case OpCode::OP_EVAL_AST: {
                uint32_t idx = read_operand24(frame.ip);
                frame.ip += 3;
                ValuePtr result = eval_ast(frame.function->ast_nodes[idx]);
                if (had_error) return false;
                push(result);
                break;
            }

            // ---- Halt ----
            // ---- Exception handling ----
            case OpCode::OP_RAISE: {
                ValuePtr exc = pop();
                throw PyRiteRaiseException(exc);
            }
            case OpCode::OP_TRY_BEGIN: {
                uint32_t target = read_operand24(frame.ip);
                frame.ip += 3;
                try_handlers.push_back({(size_t)frame_top, stack.size(), frame.code_start + target});
                break;
            }
            case OpCode::OP_TRY_END: {
                // Try block completed without exception; pop the handler.
                if (!try_handlers.empty()) try_handlers.pop_back();
                break;
            }
            case OpCode::OP_CATCH: {
                // The exception value is already on the stack (pushed by
                // handle_exception). Nothing to do here.
                break;
            }

            case OpCode::OP_DEF_STRUCT: {
                uint32_t idx = read_operand24(frame.ip);
                frame.ip += 3;
                ValuePtr desc = frame.function->constants[idx];
                auto d = ptr_cast<DimValue>(desc);
                if (!d) { runtime_error(current_line(), "bad struct descriptor"); return false; }
                std::string name = ptr_cast<StringValue>(d->dict["name"])->value;
                auto fields_ln = ptr_cast<LnValue>(d->dict["fields"]);
                std::vector<ParameterDefinition> fields;
                if (fields_ln) {
                    for (auto& fv : fields_ln->elements) {
                        auto fd = ptr_cast<DimValue>(fv);
                        if (!fd) continue;
                        std::string fname = ptr_cast<StringValue>(fd->dict["name"])->value;
                        long long code = ptr_cast<NumberValue>(fd->dict["type"])->value.toLongLong();
                        long long has_def = ptr_cast<NumberValue>(fd->dict["has_default"])->value.toLongLong();
                        // The stable cast code maps back onto its token so
                        // field type checks keep working.
                        TokenType tk = TokenType::ANY;
                        switch (static_cast<CastType>(code)) {
                            case CastType::CAST_DEC: tk = TokenType::DEC; break;
                            case CastType::CAST_STR: tk = TokenType::STR; break;
                            case CastType::CAST_BIN: tk = TokenType::BIN; break;
                            case CastType::CAST_LN: tk = TokenType::LN; break;
                            case CastType::CAST_DIM: tk = TokenType::DIM; break;
                            default: tk = TokenType::ANY; break;
                        }
                        ValuePtr defv = fd->dict.count("default") ? fd->dict["default"] : nullptr;
                        if (has_def && defv && !dynamic_cast<NullValue*>(defv.get()))
                            fields.push_back(ParameterDefinition(tk, fname, defv));
                        else
                            fields.push_back(ParameterDefinition(tk, fname));
                    }
                }
                auto klass = make_value<Class>(name, fields,
                    std::vector<AstNodePtr>{},
                    std::map<std::string, std::shared_ptr<Function>>{},
                    globals);
                push(klass);
                break;
            }

            case OpCode::OP_HALT:
                return true;

            default:
                runtime_error(0, "Unknown opcode");
                return false;
        }
    }
    } catch (const PyRiteRaiseException& e) {
        // Try to handle the exception via a try-handler.
        if (handle_exception(e.value)) {
            continue;  // resume execution at the catch handler
        }
        // Unhandled raise: print the error and terminate.
        std::string m = e.value ? e.value->toString() : "";
        int line = current_line();
        set_error(line, msg(Msg::UNCAUGHT_EX) + m);
        return false;
    } catch (const RuntimeError& e) {
        // Use the exception's line if available, otherwise the current VM line.
        int line = e.line > 0 ? e.line : current_line();
        // Wrap in ExceptionValue to match interpreter behavior
        ValuePtr exc = make_value<ExceptionValue>(make_value<StringValue>(e.what()));
        // Try to handle the runtime error via a try-handler (catchable).
        if (handle_exception(exc)) {
            continue;  // resume execution at the catch handler
        }
        set_error(line, e.what());
        return false;
    } catch (const std::exception& e) {
        int line = current_line();
        set_error(line, e.what());
        return false;
    }
    }  // end outer while (exception resume loop)
}

// ============================================================================
// Upvalue management
// ============================================================================
std::shared_ptr<UpvalueObj> VM::capture_upvalue(int slot) {
    // The slot index refers to the current frame's locals vector.
    CallFrame& f = frames[frame_top - 1];
    ValuePtr* loc = &f.locals[slot];

    // Reuse an existing open upvalue pointing to the same location.
    for (auto& uv : open_upvalues) {
        if (uv->location == loc) return uv;
    }

    auto uv = std::make_shared<UpvalueObj>(loc);
    open_upvalues.push_back(uv);
    return uv;
}

void VM::close_upvalues(int slot) {
    // Close all open upvalues that point into the *current* frame's locals
    // at index >= slot. "Closing" means copying the live value into the
    // upvalue's `closed` field and redirecting `location` to point at it,
    // so the upvalue remains valid after the frame's locals vector is
    // destroyed.
    if (frame_top == 0) return;
    CallFrame& f = frames[frame_top - 1];
    ValuePtr* frame_begin = f.locals;
    ValuePtr* frame_end = frame_begin + f.nlocals;

    auto it = open_upvalues.begin();
    while (it != open_upvalues.end()) {
        auto& uv = *it;
        if (uv->location >= frame_begin && uv->location < frame_end) {
            int uv_slot = static_cast<int>(uv->location - frame_begin);
            if (uv_slot >= slot) {
                uv->closed = *uv->location;
                uv->location = &uv->closed;
                it = open_upvalues.erase(it);
                continue;
            }
        }
        ++it;
    }
}

// ============================================================================
// OP_EVAL_AST: bridge to the AST interpreter
// ============================================================================
ValuePtr VM::eval_ast(const AstNodePtr& node) {
    if (!interpreter) {
        runtime_error(0, "No interpreter available for OP_EVAL_AST");
        return make_value<NullValue>();
    }

    CallFrame& f = frames[frame_top - 1];

    // Build a temporary environment from the current locals.
    auto env = std::make_shared<Environment>(globals);
    for (size_t i = 0; i < f.function->local_names.size(); ++i) {
        const std::string& name = f.function->local_names[i];
        if (!name.empty()) {
            env->define(name, f.locals[i]);
        }
    }

    // Save and restore the interpreter's environment.
    auto saved_env = interpreter->environment;
    interpreter->environment = env;

    ValuePtr result = make_value<NullValue>();
    try {
        // Check if it's a statement or expression
        if (dynamic_cast<ExpressionStatementNode*>(node.get()) ||
            dynamic_cast<VarDeclarationNode*>(node.get()) ||
            dynamic_cast<IfStatementNode*>(node.get()) ||
            dynamic_cast<WhileStatementNode*>(node.get()) ||
            dynamic_cast<LoopForNode*>(node.get()) ||
            dynamic_cast<ForInNode*>(node.get()) ||
            dynamic_cast<LoopUntilNode*>(node.get()) ||
            dynamic_cast<BreakNode*>(node.get()) ||
            dynamic_cast<ContinueNode*>(node.get()) ||
            dynamic_cast<SayNode*>(node.get()) ||
            dynamic_cast<FnDefNode*>(node.get()) ||
            dynamic_cast<ReturnNode*>(node.get()) ||
            dynamic_cast<TryCatchNode*>(node.get()) ||
            dynamic_cast<ClassDefNode*>(node.get()) ||
            dynamic_cast<StructDefNode*>(node.get()) ||
            dynamic_cast<RequireNode*>(node.get()) ||
            dynamic_cast<UsingNode*>(node.get()) ||
            dynamic_cast<AwaitStatementNode*>(node.get()) ||
            dynamic_cast<RaiseNode*>(node.get()) ||
            dynamic_cast<SwapNode*>(node.get())) {
            interpreter->execute(node);
        } else {
            result = interpreter->evaluate(node);
        }
    } catch (ReturnValueException& e) {
        result = e.value;
    } catch (PyRiteRaiseException& e) {
        interpreter->environment = saved_env;
        // Re-throw for the caller to handle
        throw;
    } catch (BreakException& e) {
        interpreter->environment = saved_env;
        throw;
    } catch (ContinueException& e) {
        interpreter->environment = saved_env;
        throw;
    } catch (RuntimeError& e) {
        interpreter->environment = saved_env;
        // Re-throw for the VM's run() loop to handle (may be caught by try/catch)
        throw;
    }

    interpreter->environment = saved_env;

    // Sync locals back from the environment.
    for (size_t i = 0; i < f.function->local_names.size(); ++i) {
        const std::string& name = f.function->local_names[i];
        if (!name.empty()) {
            ValuePtr val = env->get(name);
            if (val) f.locals[i] = val;
        }
    }

    // Sync any new variable definitions (e.g. class/struct definitions,
    // require imports) from the temporary environment to globals.
    for (const auto& kv : env->get_values()) {
        // Only sync to globals if it's not a local variable.
        bool is_local = false;
        for (size_t i = 0; i < f.function->local_names.size(); ++i) {
            if (f.function->local_names[i] == kv.first) { is_local = true; break; }
        }
        if (!is_local && globals) {
            globals->define(kv.first, kv.second);
        }
    }

    return result;
}

// ============================================================================
// Call dispatch
// ============================================================================
bool VM::call_value(ValuePtr callee, const std::vector<ValuePtr>& args, int line) {
    if (auto native = dynamic_cast<NativeFnValue*>(callee.get())) {
        try {
            ValuePtr result = native->fn(args);
            push(result);
        } catch (RuntimeError& e) {
            // Re-throw for the VM's run() loop to handle (may be caught by try/catch)
            throw;
        }
        return true;
    }
    if (auto func_val = dynamic_cast<FunctionValue*>(callee.get())) {
        // AST function: delegate to interpreter
        if (!interpreter) {
            runtime_error(line, "No interpreter for AST function call");
            return false;
        }
        auto function = func_val->value;
        auto call_env = std::make_shared<Environment>(function->closure);
        // Bind parameters
        const auto& params = function->params;
        for (size_t i = 0; i < params.size(); ++i) {
            if (i < args.size()) {
                call_env->define(params[i].name, args[i]);
            } else if (params[i].has_default) {
                if (params[i].default_value) {
                    call_env->define(params[i].name, params[i].default_value);
                } else if (params[i].default_expr) {
                    auto saved = interpreter->environment;
                    interpreter->environment = call_env;
                    ValuePtr dv = interpreter->evaluate(params[i].default_expr);
                    interpreter->environment = saved;
                    call_env->define(params[i].name, dv);
                }
            }
        }
        if (++interpreter->call_depth > Interpreter::MAX_CALL_DEPTH) {
            interpreter->call_depth--;
            runtime_error(line, "Recursion depth exceeds the limit (" + std::to_string(Interpreter::MAX_CALL_DEPTH) + ").");
            return false;
        }
        auto saved = interpreter->environment;
        interpreter->environment = call_env;
        ValuePtr result = make_value<NullValue>();
        try {
            interpreter->execute_block(function->body, call_env);
        } catch (ReturnValueException& e) {
            result = e.value;
        } catch (RuntimeError& e) {
            interpreter->call_depth--;
            interpreter->environment = saved;
            // Re-throw for the VM's run() loop to handle (may be caught by try/catch)
            throw;
        }
        interpreter->call_depth--;
        interpreter->environment = saved;
        push(result);
        return true;
    }
    if (auto bound = dynamic_cast<BoundMethodValue*>(callee.get())) {
        if (!interpreter) {
            runtime_error(line, "No interpreter for bound method call");
            return false;
        }
        auto function = bound->method;
        auto call_env = std::make_shared<Environment>(function->closure);
        call_env->define("this", bound->instance);
        const auto& params = function->params;
        for (size_t i = 0; i < params.size(); ++i) {
            if (i < args.size()) {
                call_env->define(params[i].name, args[i]);
            } else if (params[i].has_default) {
                if (params[i].default_value) {
                    call_env->define(params[i].name, params[i].default_value);
                } else if (params[i].default_expr) {
                    auto saved = interpreter->environment;
                    interpreter->environment = call_env;
                    ValuePtr dv = interpreter->evaluate(params[i].default_expr);
                    interpreter->environment = saved;
                    call_env->define(params[i].name, dv);
                }
            }
        }
        auto saved = interpreter->environment;
        interpreter->environment = call_env;
        ValuePtr result = make_value<NullValue>();
        if (++interpreter->call_depth > Interpreter::MAX_CALL_DEPTH) {
            interpreter->call_depth--;
            runtime_error(line, "Recursion depth exceeds the limit (" + std::to_string(Interpreter::MAX_CALL_DEPTH) + ").");
            return false;
        }
        try {
            interpreter->execute_block(function->body, call_env);
        } catch (ReturnValueException& e) {
            result = e.value;
        } catch (RuntimeError& e) {
            interpreter->call_depth--;
            interpreter->environment = saved;
            // Re-throw for the VM's run() loop to handle (may be caught by try/catch)
            throw;
        }
        interpreter->call_depth--;
        interpreter->environment = saved;
        push(result);
        return true;
    }
    runtime_error(line, "Cannot call this value");
    return false;
}

// ============================================================================
// Push a new call frame for a VM closure
// ============================================================================
void VM::push_frame(Closure* closure, const ValuePtr* args, size_t nargs, int line) {
    if (frame_top >= FRAMES_MAX) {
        runtime_error(line, "Recursion depth exceeds the limit (" + std::to_string(Interpreter::MAX_CALL_DEPTH) + ").");
        return;
    }
    CompiledFunction* fn = closure->function.get();
    if (nargs < static_cast<size_t>(fn->arity)) {
        runtime_error(line, "Too few arguments to " + fn->name);
        return;
    }
    size_t nlocals = fn->max_locals > 0 ? (size_t)fn->max_locals : fn->local_names.size();
    if (arena_top + nlocals > arena.size()) {
        runtime_error(line, "frame arena exhausted");
        return;
    }
    CallFrame& f = frames[frame_top++];
    f.function = fn;
    f.code_start = fn->code.data();
    f.ip = f.code_start;
    f.upvalues = closure->upvalues.empty() ? nullptr : closure->upvalues.data();
    f.upvalue_count = (uint32_t)closure->upvalues.size();
    f.locals_base = arena_top;
    f.nlocals = (uint32_t)nlocals;
    f.locals = arena.data() + arena_top;
    arena_top += nlocals;
    // Slots are not re-initialized: the compiler emits an assignment
    // before any read of a slot, so stale values are never observed.
    for (size_t i = 0; i < fn->params.size(); ++i) {
        if (i < nargs) {
            f.locals[i + 1] = args[i];
        } else if (fn->params[i].has_default) {
            if (fn->params[i].default_value) {
                f.locals[i + 1] = fn->params[i].default_value;
            }
            // Expression defaults are handled at call time (rare in VM)
        }
    }
}

// Same as push_frame, but the arguments are read straight off the value
// stack (callee at stack[top - nargs - 1]): one copy instead of two.
void VM::push_frame_from_slots(Closure* closure, const ValuePtr* args, size_t nargs, int line) {
    push_frame(closure, args, nargs, line);
}

void VM::push_frame_from_stack(Closure* closure, size_t nargs, int line) {
    // Args are on the value stack above the callee: [.., callee, a1..aN].
    // The moved-out slots stay; the caller pops callee + args.
    const size_t base = stack.size() - nargs;
    ValuePtr argv[4];
    std::vector<ValuePtr> heap_args;
    const ValuePtr* args;
    if (nargs <= 4) {
        for (size_t k = 0; k < nargs; ++k) argv[k] = std::move(stack[base + k]);
        args = argv;
    } else {
        heap_args.reserve(nargs);
        for (size_t k = 0; k < nargs; ++k) heap_args.push_back(std::move(stack[base + k]));
        args = heap_args.data();
    }
    push_frame(closure, args, nargs, line);
}

// ============================================================================
// Binary/unary operations
// ============================================================================
// Interned small integers in [-4096, 4095]: an 8K-entry table, filled
// lazily. A loop counter or a small result finds its value here instead
// of allocating; numbers are immutable so entries never go stale.
ValuePtr VM::intern_small(long long v) {
    static ValuePtr cache[8192];
    size_t idx = (size_t)(v + 4096);
    ValuePtr& slot = cache[idx];
    if (!slot) slot = make_value<NumberValue>(BigNumber(v));
    return slot;
}

// ============================================================================
// Cached global-cell access for the fused instructions
// ============================================================================
ValuePtr* VM::global_write_cell(CallFrame& frame, uint32_t idx) {
    if (idx >= frame.function->wcache.size()) frame.function->wcache.resize(frame.function->constants.size(), nullptr);
    ValuePtr* cell = frame.function->wcache[idx];
    if (!cell) {
        std::string name = ptr_cast<StringValue>(
            frame.function->constants[idx])->value;
        cell = const_cast<ValuePtr*>(globals->cell(name));
        if (!cell) cell = globals->cell_mut(name);
        frame.function->wcache[idx] = cell;
    }
    return cell;
}

const ValuePtr* VM::global_read_cell(CallFrame& frame, uint32_t idx) {
    if (idx >= frame.function->gcache.size()) frame.function->gcache.resize(frame.function->constants.size(), nullptr);
    const ValuePtr* cell = frame.function->gcache[idx];
    if (!cell) {
        std::string name = ptr_cast<StringValue>(
            frame.function->constants[idx])->value;
        cell = globals->cell(name);
        frame.function->gcache[idx] = cell;
    }
    return cell;
}

// ============================================================================
// Fused in-place slot arithmetic (OP_INPLACE_ADD / SUB / MUL)
// ============================================================================
bool VM::exec_inplace(OpCode op, uint32_t packed, CallFrame& frame, int line) {
    bool dst_global = packed & 0x800000;
    bool no_push = packed & 0x400000;
    uint32_t dst = (packed >> 12) & 0x3FF;
    uint32_t rkind = (packed >> 9) & 0x7;
    uint32_t rpay = packed & 0x1FF;

    // Destination: a local slot reference, or the cached cell of a global.
    ValuePtr* slot = nullptr;         // local path
    ValuePtr* gcell = nullptr;        // global path
    if (dst_global) {
        if (dst >= frame.function->wcache.size()) frame.function->wcache.resize(frame.function->constants.size(), nullptr);
        gcell = frame.function->wcache[dst];
        if (!gcell) {
            std::string name = ptr_cast<StringValue>(
                frame.function->constants[dst])->value;
            gcell = const_cast<ValuePtr*>(globals->cell(name));
            if (!gcell) gcell = globals->cell_mut(name);
            frame.function->wcache[dst] = gcell;
        }
    } else {
        if (dst >= frame.nlocals) { runtime_error(line, "inplace: bad slot"); return true; }
        slot = &frame.locals[dst];
    }
    ValuePtr a = dst_global ? *gcell : *slot;

    // Resolve the right-hand side to a BigNumber (numbers) or a string.
    BigNumber rhs_bn;                 // holds materialized values
    const BigNumber* rb = nullptr;
    const std::string* rs = nullptr;  // string rhs
    std::string rhs_str;
    ValuePtr rhs_hold;                // keeps a popped stack value alive

    if (rkind == 3) {                 // inline small non-negative integer
        rhs_bn = BigNumber((long long)rpay);
        rb = &rhs_bn;
    } else if (rkind == 1) {          // local slot
        uint32_t ridx = rpay;
        if (ridx >= frame.nlocals) { runtime_error(line, "inplace: bad rhs slot"); return true; }
        ValuePtr& r = frame.locals[ridx];
        if (r->kind() == VTag::Number) rb = &static_cast<NumberValue*>(r.get())->value;
        else if (r->kind() == VTag::String) { rhs_str = static_cast<StringValue*>(r.get())->value; rs = &rhs_str; }
        else return false;
    } else if (rkind == 2) {          // value already on the stack top
        rhs_hold = pop();
        if (rhs_hold->kind() == VTag::Number) rb = &static_cast<NumberValue*>(rhs_hold.get())->value;
        else if (rhs_hold->kind() == VTag::String) { rhs_str = static_cast<StringValue*>(rhs_hold.get())->value; rs = &rhs_str; }
        else return false;
    } else {                          // constants-pool entry
        const ValuePtr& c = frame.function->constants[rpay];
        if (c->kind() == VTag::Number) rb = &static_cast<NumberValue*>(c.get())->value;
        else if (c->kind() == VTag::String) { rs = &static_cast<StringValue*>(c.get())->value; }
        else return false;
    }

    // Number op number: pure BigNumber work, then commit. When the slot
    // held the only reference to the old object it is rewritten in place,
    // which is what makes `i = i + 1` allocation-free.
    if (a->kind() == VTag::Number && rb) {
        NumberValue* an = static_cast<NumberValue*>(a.get());
        // Machine-int fast path: when both sides and the result fit a
        // long long the limb machinery below is pure overhead. Small
        // results come straight out of the intern table.
        long long ia, ib;
        if (an->value.is_int_fast(ia) && rb->is_int_fast(ib)) {
            __int128 r = op == OpCode::OP_INPLACE_ADD ? (__int128)ia + ib
                       : op == OpCode::OP_INPLACE_SUB ? (__int128)ia - ib
                       : (__int128)ia * ib;
            if (r <= (__int128)LLONG_MAX && r >= (__int128)LLONG_MIN) {
                long long rv = (long long)r;
                ValuePtr fresh = (rv >= -4096 && rv <= 4095)
                    ? intern_small(rv) : make_value<NumberValue>(BigNumber(rv));
                if (dst_global) *gcell = fresh; else *slot = fresh;
                if (!no_push) push(fresh);
                return true;
            }
        }
        BigNumber result;
        switch (op) {
            case OpCode::OP_INPLACE_ADD: result = an->value + *rb; break;
            case OpCode::OP_INPLACE_SUB: result = an->value - *rb; break;
            default:                     result = an->value * *rb; break;
        }
        // Drop the slot's own reference first: `a` is a copy, so a count
        // of 1 afterwards means nothing else in the program shares the
        // object and it may be rewritten in place.
        if (dst_global) *gcell = nullptr; else *slot = nullptr;
        if (a.use_count() == 1) {
            an->value = std::move(result);
            if (dst_global) *gcell = a; else *slot = a;
            if (!no_push) push(a);
        } else {
            long long iv;
            ValuePtr fresh = (result.is_int_fast(iv) && iv >= -4096 && iv <= 4095)
                ? intern_small(iv) : make_value<NumberValue>(std::move(result));
            if (dst_global) *gcell = fresh; else *slot = fresh;
            if (!no_push) push(fresh);
        }
        return true;
    }
    // String op string: append in place under the same rule.
    if (a->kind() == VTag::String && rs) {
        StringValue* as = static_cast<StringValue*>(a.get());
        if (dst_global) *gcell = nullptr; else *slot = nullptr;
        if (a.use_count() == 1) {
            as->value += *rs;
            if (dst_global) *gcell = a; else *slot = a;
            if (!no_push) push(a);
        } else {
            auto fresh = make_value<StringValue>(as->value + *rs);
            if (dst_global) *gcell = fresh; else *slot = fresh;
            if (!no_push) push(fresh);
        }
        return true;
    }
    return false;                     // let the caller run the plain sequence
}

ValuePtr VM::binary_op(OpCode op, const ValuePtr& a, const ValuePtr& b, int line) {
    // One virtual-free tag read each instead of up to eight dynamic_casts.
    const VTag ta = a->kind(), tb2 = b->kind();
    auto na = ta == VTag::Number ? static_cast<NumberValue*>(a.get()) : nullptr;
    auto nb = tb2 == VTag::Number ? static_cast<NumberValue*>(b.get()) : nullptr;
    auto sa = ta == VTag::String ? static_cast<StringValue*>(a.get()) : nullptr;
    auto sb = tb2 == VTag::String ? static_cast<StringValue*>(b.get()) : nullptr;
    auto ba = ta == VTag::Binary ? static_cast<BinaryValue*>(a.get()) : nullptr;
    auto bb = tb2 == VTag::Binary ? static_cast<BinaryValue*>(b.get()) : nullptr;
    auto la = ta == VTag::Ln ? static_cast<LnValue*>(a.get()) : nullptr;
    auto lb = tb2 == VTag::Ln ? static_cast<LnValue*>(b.get()) : nullptr;

    if (op == OpCode::OP_ADD) {
        if (na && nb) {
            // Machine-int fast path with small-result interning: loop
            // arithmetic never touches the limb machinery here.
            long long ia, ib;
            if (na->value.is_int_fast(ia) && nb->value.is_int_fast(ib)) {
                __int128 r = (__int128)ia + ib;
                if (r <= (__int128)LLONG_MAX && r >= (__int128)LLONG_MIN) {
                    long long rv = (long long)r;
                    if (rv >= -4096 && rv <= 4095) return intern_small(rv);
                    return make_value<NumberValue>(BigNumber(rv));
                }
            }
            return make_value<NumberValue>(na->value + nb->value);
        }
        // Binary + Number or Number + Binary → number
        if (ba && nb) return make_value<NumberValue>(ba->toBigNumber() + nb->value);
        if (na && bb) return make_value<NumberValue>(na->value + bb->toBigNumber());
        if (la && lb) {
            auto result = make_value<LnValue>(std::vector<ValuePtr>{});
            result->elements = la->elements;
            for (auto& v : lb->elements) result->elements.push_back(v);
            return result;
        }
        // String or Binary with anything else → string concatenation
        if (sa || sb || ba || bb) {
            return make_value<StringValue>(a->toString() + b->toString());
        }
    }
    if (op == OpCode::OP_MUL) {
        if (na && nb) {
            long long ia, ib;
            if (na->value.is_int_fast(ia) && nb->value.is_int_fast(ib)) {
                __int128 r = (__int128)ia * ib;
                if (r <= (__int128)LLONG_MAX && r >= (__int128)LLONG_MIN) {
                    long long rv = (long long)r;
                    if (rv >= -4096 && rv <= 4095) return intern_small(rv);
                    return make_value<NumberValue>(BigNumber(rv));
                }
            }
            return make_value<NumberValue>(na->value * nb->value);
        }
        // List * Number → repetition
        if (la && nb) {
            long long times = nb->value.toLongLong();
            if (times < 0) times = 0;
            auto result = make_value<LnValue>(std::vector<ValuePtr>{});
            for (long long i = 0; i < times; ++i) {
                for (const auto& elem : la->elements) result->elements.push_back(elem->clone());
            }
            return result;
        }
        // Number * List → repetition (commutative)
        if (na && lb) {
            long long times = na->value.toLongLong();
            if (times < 0) times = 0;
            auto result = make_value<LnValue>(std::vector<ValuePtr>{});
            for (long long i = 0; i < times; ++i) {
                for (const auto& elem : lb->elements) result->elements.push_back(elem->clone());
            }
            return result;
        }
    }
    if (na && nb) {
        long long ia, ib;
        if ((op == OpCode::OP_SUB) && na->value.is_int_fast(ia) && nb->value.is_int_fast(ib)) {
            __int128 r = (__int128)ia - ib;
            if (r <= (__int128)LLONG_MAX && r >= (__int128)LLONG_MIN) {
                long long rv = (long long)r;
                if (rv >= -4096 && rv <= 4095) return intern_small(rv);
                return make_value<NumberValue>(BigNumber(rv));
            }
        }
        switch (op) {
            case OpCode::OP_SUB: return make_value<NumberValue>(na->value - nb->value);
            case OpCode::OP_DIV:
                if (nb->value.isZero()) { runtime_error(line, "Division by zero."); return NULL_SINGLETON; }
                return make_value<NumberValue>(na->value / nb->value);
            case OpCode::OP_MOD:
                if (nb->value.isZero()) { runtime_error(line, "Modulo by zero."); return NULL_SINGLETON; }
                return make_value<NumberValue>(na->value % nb->value);
            case OpCode::OP_POW: return make_value<NumberValue>(na->value ^ nb->value);
            case OpCode::OP_LESS: return na->value < nb->value ? BOOL_TRUE : BOOL_FALSE;
            case OpCode::OP_LESS_EQUAL: return na->value <= nb->value ? BOOL_TRUE : BOOL_FALSE;
            case OpCode::OP_GREATER: return na->value > nb->value ? BOOL_TRUE : BOOL_FALSE;
            case OpCode::OP_GREATER_EQUAL: return na->value >= nb->value ? BOOL_TRUE : BOOL_FALSE;
            default: break;
        }
    }
    // Binary vs Number comparisons (convert binary to number)
    if (ba && nb) {
        BigNumber bv = ba->toBigNumber();
        switch (op) {
            case OpCode::OP_LESS: return make_value<NumberValue>(bv < nb->value ? 1 : 0);
            case OpCode::OP_LESS_EQUAL: return make_value<NumberValue>(bv <= nb->value ? 1 : 0);
            case OpCode::OP_GREATER: return make_value<NumberValue>(bv > nb->value ? 1 : 0);
            case OpCode::OP_GREATER_EQUAL: return make_value<NumberValue>(bv >= nb->value ? 1 : 0);
            default: break;
        }
    }
    if (na && bb) {
        BigNumber bv = bb->toBigNumber();
        switch (op) {
            case OpCode::OP_LESS: return make_value<NumberValue>(na->value < bv ? 1 : 0);
            case OpCode::OP_LESS_EQUAL: return make_value<NumberValue>(na->value <= bv ? 1 : 0);
            case OpCode::OP_GREATER: return make_value<NumberValue>(na->value > bv ? 1 : 0);
            case OpCode::OP_GREATER_EQUAL: return make_value<NumberValue>(na->value >= bv ? 1 : 0);
            default: break;
        }
    }
    if (sa && sb) {
        switch (op) {
            case OpCode::OP_LESS: return make_value<NumberValue>(sa->value < sb->value ? 1 : 0);
            case OpCode::OP_LESS_EQUAL: return make_value<NumberValue>(sa->value <= sb->value ? 1 : 0);
            case OpCode::OP_GREATER: return make_value<NumberValue>(sa->value > sb->value ? 1 : 0);
            case OpCode::OP_GREATER_EQUAL: return make_value<NumberValue>(sa->value >= sb->value ? 1 : 0);
            default: break;
        }
    }
    // Select appropriate error message based on operation type
    std::string err_msg;
    switch (op) {
        case OpCode::OP_ADD: err_msg = msg(Msg::ADD_TYPE); break;
        case OpCode::OP_SUB: err_msg = msg(Msg::SUB_TYPE); break;
        case OpCode::OP_MUL: err_msg = msg(Msg::MUL_TYPE); break;
        case OpCode::OP_DIV: case OpCode::OP_MOD: case OpCode::OP_POW: err_msg = msg(Msg::DIV_TYPE); break;
        case OpCode::OP_LESS: case OpCode::OP_LESS_EQUAL:
        case OpCode::OP_GREATER: case OpCode::OP_GREATER_EQUAL:
        case OpCode::OP_EQUAL: case OpCode::OP_NOT_EQUAL: err_msg = msg(Msg::CMP_TYPE); break;
        default: err_msg = "Invalid operand types for operator"; break;
    }
    runtime_error(line, err_msg);
    return make_value<NullValue>();
}

ValuePtr VM::unary_op(OpCode op, const ValuePtr& a, int line) {
    if (op == OpCode::OP_NEGATE) {
        if (a->kind() == VTag::Number) {
            auto na = static_cast<NumberValue*>(a.get());
            return make_value<NumberValue>(-na->value);
        }
    }
    runtime_error(line, msg(Msg::SUB_TYPE));
    return make_value<NullValue>();
}

// ============================================================================
// Disassemble (for --dump-bytecode)
// ============================================================================
void VM::disassemble(BytecodeChunkPtr c) {
    chunk = c;
    for (size_t fi = 0; fi < c->all_functions.size(); ++fi) {
        auto& fn = c->all_functions[fi];
        std::cout << "=== Function: " << fn->name << " (arity=" << fn->arity
                  << ", upvalues=" << fn->upvalue_count << ") ===" << std::endl;
        size_t ip = 0;
        while (ip < fn->code.size()) {
            OpCode op = static_cast<OpCode>(fn->code[ip]);
            int line = (ip < fn->lines.size()) ? fn->lines[ip] : 0;
            std::cout << "  " << ip << " [L" << line << "]: " << op_name(op);
            ip++;
            // Walk the operands from the generated table so the listing
            // can never fall out of step with the instruction encoding.
            for (int k = 0; k < OPCODE_OPERAND_COUNT[static_cast<size_t>(op)]; ++k) {
                uint32_t operand = read_operand24(&fn->code[ip]);
                std::cout << " " << operand;
                ip += 3;
            }
            std::cout << std::endl;
        }
    }
}
