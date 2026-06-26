#include "VM.hpp"
#include "Interpreter.hpp"
#include "msgs.hpp"
#include <sstream>
#include <cmath>
#include <chrono>
#include <iostream>

// ============================================================================
// Constructor
// ============================================================================
VM::VM() : had_error(false), interpreter(nullptr) {
    stack.reserve(256);
    frames.reserve(64);
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
    if (frames.empty()) return 0;
    CallFrame& f = frames.back();
    if (!f.function || f.function->code.empty()) return 0;
    size_t offset = static_cast<size_t>(f.ip - f.code_start);
    if (offset == 0) return 0;
    // ip points to the NEXT instruction; the current instruction is at offset-1.
    size_t idx = offset - 1;
    if (idx < f.function->lines.size()) return f.function->lines[idx];
    return 0;
}

bool VM::handle_exception(ValuePtr exc_value) {
    if (try_handlers.empty()) return false;
    TryHandler handler = try_handlers.back();
    try_handlers.pop_back();

    // Unwind frames to the handler's frame depth.
    while (frames.size() > handler.frame_depth) {
        frames.pop_back();
    }
    if (frames.empty()) return false;

    // Restore the stack to the depth it had when the try was entered,
    // then push the exception value.
    if (stack.size() > handler.stack_depth) {
        stack.resize(handler.stack_depth);
    }
    push(exc_value);

    // Jump to the catch handler.
    frames.back().ip = handler.catch_ip;
    return true;
}

// ============================================================================
// Truthiness
// ============================================================================
bool VM::is_truthy(const ValuePtr& v) {
    if (!v) return false;
    return v->isTruthy();
}

// ============================================================================
// Run: main execution loop
// ============================================================================
bool VM::run(BytecodeChunkPtr c) {
    chunk = c;
    had_error = false;
    error_message.clear();
    stack.clear();
    frames.clear();
    open_upvalues.clear();

    // Set up the initial call frame for the script.
    ClosurePtr script_closure = std::make_shared<Closure>(chunk->script);
    frames.push_back(CallFrame{});
    CallFrame& f = frames.back();
    f.function = chunk->script;
    f.code_start = chunk->script->code.data();
    f.ip = f.code_start;
    f.locals.resize(chunk->script->max_locals > 0 ? chunk->script->max_locals : chunk->script->local_names.size());
    for (auto& l : f.locals) l = std::make_shared<NullValue>();

    // Main dispatch loop.
    // The outer loop allows resuming execution after a caught exception.
    while (true) {
    try {
    while (true) {
        CallFrame& frame = frames.back();
        uint8_t instruction = *frame.ip++;
        OpCode op = static_cast<OpCode>(instruction);

        switch (op) {
            // ---- Constants ----
            case OpCode::OP_CONSTANT: {
                uint32_t idx = read_operand24(frame.ip);
                frame.ip += 3;
                push(frame.function->constants[idx]);
                break;
            }
            case OpCode::OP_NULL:
                push(std::make_shared<NullValue>());
                break;
            case OpCode::OP_TRUE:
                push(std::make_shared<NumberValue>(1));
                break;
            case OpCode::OP_FALSE:
                push(std::make_shared<NumberValue>(0));
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
                if (idx >= frame.locals.size()) {
                    runtime_error(0, "SET_LOCAL: slot " + std::to_string(idx) +
                        " out of bounds (locals size=" + std::to_string(frame.locals.size()) +
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
                std::string name = std::dynamic_pointer_cast<StringValue>(
                    frame.function->constants[idx])->value;
                ValuePtr val = globals->get(name);
                if (!val) {
                    runtime_error(current_line(), fmt(Msg::UNDEFINED_VAR, name));
                    return false;
                }
                // Lazy-load module proxies.
                if (auto proxy = dynamic_cast<ModuleProxy*>(val.get())) {
                    if (!proxy->loaded && interpreter) {
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
                std::string name = std::dynamic_pointer_cast<StringValue>(
                    frame.function->constants[idx])->value;
                globals->assign(name, peek());
                break;
            }
            case OpCode::OP_DEFINE_GLOBAL: {
                uint32_t idx = read_operand24(frame.ip);
                frame.ip += 3;
                std::string name = std::dynamic_pointer_cast<StringValue>(
                    frame.function->constants[idx])->value;
                globals->define(name, pop());
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
                if (auto ln = dynamic_cast<LnValue*>(obj.get())) {
                    if (auto num = dynamic_cast<NumberValue*>(index.get())) {
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
                } else if (auto dm = dynamic_cast<DimValue*>(obj.get())) {
                    if (auto str = dynamic_cast<StringValue*>(index.get())) {
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
                } else if (auto str = dynamic_cast<StringValue*>(obj.get())) {
                    if (auto num = dynamic_cast<NumberValue*>(index.get())) {
                        long long i = num->value.toLongLong();
                        long long len = static_cast<long long>(str->value.size());
                        if (i < 0) i += len;
                        if (i < 0 || i >= len) {
                            runtime_error(current_line(), msg(Msg::LN_IDX_OOB));
                            return false;
                        }
                        push(std::make_shared<StringValue>(std::string(1, str->value[i])));
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
                if (auto ln = dynamic_cast<LnValue*>(obj.get())) {
                    if (auto num = dynamic_cast<NumberValue*>(index.get())) {
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
                } else if (auto dm = dynamic_cast<DimValue*>(obj.get())) {
                    if (auto str = dynamic_cast<StringValue*>(index.get())) {
                        dm->dict[str->value] = val;
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
                std::string name = std::dynamic_pointer_cast<StringValue>(
                    frame.function->constants[idx])->value;
                ValuePtr obj = pop();
                if (auto inst = dynamic_cast<Instance*>(obj.get())) {
                    ValuePtr val = inst->get(name);
                    push(val);
                } else if (auto dm = dynamic_cast<DimValue*>(obj.get())) {
                    auto it = dm->dict.find(name);
                    if (it != dm->dict.end()) push(it->second);
                    else push(std::make_shared<NullValue>());
                } else {
                    runtime_error(current_line(), fmt(Msg::NO_PROP, name));
                    return false;
                }
                break;
            }
            case OpCode::OP_SET_PROPERTY: {
                uint32_t idx = read_operand24(frame.ip);
                frame.ip += 3;
                std::string name = std::dynamic_pointer_cast<StringValue>(
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
                auto ln = std::make_shared<LnValue>(std::vector<ValuePtr>{});
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
                auto dm = std::make_shared<DimValue>();
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
                    push(std::make_shared<NumberValue>(BigNumber(static_cast<long long>(ln->elements.size()))));
                } else if (auto dm = dynamic_cast<DimValue*>(obj.get())) {
                    push(std::make_shared<NumberValue>(BigNumber(static_cast<long long>(dm->dict.size()))));
                } else if (auto str = dynamic_cast<StringValue*>(obj.get())) {
                    push(std::make_shared<NumberValue>(BigNumber(static_cast<long long>(str->value.size()))));
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
                if (!start_null) start = std::dynamic_pointer_cast<NumberValue>(start_v)->value.toLongLong();
                if (!end_null) end = std::dynamic_pointer_cast<NumberValue>(end_v)->value.toLongLong();
                if (!step_null) step = std::dynamic_pointer_cast<NumberValue>(step_v)->value.toLongLong();
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
                    push(std::make_shared<StringValue>(result));
                } else if (auto ln = dynamic_cast<LnValue*>(obj.get())) {
                    long long len = static_cast<long long>(ln->elements.size());
                    if (start_null) start = 0;
                    if (end_null) end = len;
                    if (step_null) step = 1;
                    if (start < 0) start += len;
                    if (end < 0) end += len;
                    if (start < 0) start = 0;
                    if (end > len) end = len;
                    auto result = std::make_shared<LnValue>(std::vector<ValuePtr>{});
                    if (step > 0) {
                        for (long long i = start; i < end; i += step) result->elements.push_back(ln->elements[i]);
                    }
                    push(result);
                } else {
                    push(std::make_shared<NullValue>());
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
            case OpCode::OP_NEGATE: {
                ValuePtr a = pop();
                push(unary_op(op, a, 0));
                break;
            }
            case OpCode::OP_NOT: {
                ValuePtr a = pop();
                push(std::make_shared<NumberValue>(is_truthy(a) ? 0 : 1));
                break;
            }

            // ---- Comparisons ----
            case OpCode::OP_EQUAL: {
                ValuePtr b = pop();
                ValuePtr a = pop();
                push(std::make_shared<NumberValue>(a->isEqualTo(*b) ? 1 : 0));
                break;
            }
            case OpCode::OP_NOT_EQUAL: {
                ValuePtr b = pop();
                ValuePtr a = pop();
                push(std::make_shared<NumberValue>(a->isEqualTo(*b) ? 0 : 1));
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
                ClosurePtr closure = std::make_shared<Closure>(fn);
                closure->upvalues.resize(fn->upvalue_count);
                // Capture upvalues
                for (int i = 0; i < fn->upvalue_count; ++i) {
                    const auto& desc = fn->upvalue_descriptors[i];
                    if (desc.is_local) {
                        closure->upvalues[i] = capture_upvalue(desc.index);
                    } else {
                        if (desc.index < static_cast<int>(frame.upvalues.size())) {
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
                std::vector<ValuePtr> args;
                args.reserve(arg_count);
                for (uint32_t i = 0; i < arg_count; ++i) {
                    args.push_back(peek(arg_count - 1 - i));
                }
                if (auto closure = dynamic_cast<Closure*>(callee.get())) {
                    // Pop callee and args
                    for (uint32_t i = 0; i <= arg_count; ++i) pop();
                    push_frame(std::make_shared<Closure>(*closure), args, 0);
                } else {
                    // Native or AST function: call synchronously
                    for (uint32_t i = 0; i <= arg_count; ++i) pop();
                    if (!call_value(callee, args, 0)) return false;
                }
                break;
            }
            case OpCode::OP_RETURN: {
                ValuePtr result = pop();
                close_upvalues(0);  // close all upvalues pointing to this frame
                bool was_script = (frames.size() == 1);
                frames.pop_back();
                if (was_script) return true;
                push(result);
                break;
            }
            case OpCode::OP_RETURN_NULL: {
                close_upvalues(0);
                bool was_script = (frames.size() == 1);
                frames.pop_back();
                if (was_script) return true;
                push(std::make_shared<NullValue>());
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
                if (auto s = std::dynamic_pointer_cast<StringValue>(promptVal)) {
                    prompt = s->value;
                } else {
                    prompt = promptVal->toString();
                }
                std::cout << prompt;
                std::string line;
                std::getline(std::cin, line);
                push(std::make_shared<StringValue>(line));
                break;
            }

            // ---- Type cast ----
            case OpCode::OP_CAST: {
                uint32_t idx = read_operand24(frame.ip);
                frame.ip += 3;
                TokenType type = static_cast<TokenType>(idx);
                ValuePtr val = pop();
                // Perform type conversion, mirroring the AST interpreter's
                // TypeConversionNode::accept logic (for `as` casts) and
                // VarDeclarationNode::accept logic (for variable declarations).
                if (type == TokenType::DEC) {
                    if (dynamic_cast<NumberValue*>(val.get())) {
                        push(val);
                    } else if (auto s = dynamic_cast<StringValue*>(val.get())) {
                        try { push(std::make_shared<NumberValue>(BigNumber(s->value))); }
                        catch (const std::invalid_argument&) {
                            runtime_error(current_line(), std::string("Cannot convert string '") + s->value + "' to a number.");
                            return false;
                        }
                    } else if (auto b = dynamic_cast<BinaryValue*>(val.get())) {
                        push(std::make_shared<NumberValue>(b->toBigNumber()));
                    } else if (dynamic_cast<NullValue*>(val.get())) {
                        // VarDeclaration allows null -> 0
                        push(std::make_shared<NumberValue>(BigNumber(0)));
                    } else {
                        // VarDeclaration keeps as-is; TypeConversion throws.
                        // Since we can't distinguish, keep as-is (lenient).
                        push(val);
                    }
                } else if (type == TokenType::STR) {
                    push(std::make_shared<StringValue>(val->toString()));
                } else if (type == TokenType::BIN) {
                    if (dynamic_cast<BinaryValue*>(val.get())) {
                        push(val);
                    } else if (auto s = dynamic_cast<StringValue*>(val.get())) {
                        try { push(std::make_shared<BinaryValue>(s->value)); }
                        catch (...) {
                            runtime_error(current_line(), std::string("Cannot convert string '") + s->value + "' to binary. Expected '0x...' format.");
                            return false;
                        }
                    } else if (dynamic_cast<NullValue*>(val.get())) {
                        push(std::make_shared<BinaryValue>(std::vector<uint8_t>{0}));
                    } else {
                        push(std::make_shared<BinaryValue>(std::vector<uint8_t>{0}));
                    }
                } else if (type == TokenType::LN) {
                    if (dynamic_cast<LnValue*>(val.get())) {
                        push(val);
                    } else if (dynamic_cast<NullValue*>(val.get())) {
                        push(std::make_shared<LnValue>(std::vector<ValuePtr>{}));
                    } else {
                        // VarDeclaration wraps in list; TypeConversion throws.
                        // Since we can't distinguish, wrap in list (lenient).
                        push(std::make_shared<LnValue>(std::vector<ValuePtr>{val}));
                    }
                } else if (type == TokenType::DIM) {
                    if (dynamic_cast<DimValue*>(val.get())) {
                        push(val);
                    } else if (dynamic_cast<NullValue*>(val.get())) {
                        push(std::make_shared<DimValue>());
                    } else {
                        push(std::make_shared<DimValue>());
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
                try_handlers.push_back({frames.size(), stack.size(), frame.code_start + target});
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
        ValuePtr exc = std::make_shared<ExceptionValue>(std::make_shared<StringValue>(e.what()));
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
    CallFrame& f = frames.back();
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
    if (frames.empty()) return;
    CallFrame& f = frames.back();
    ValuePtr* frame_begin = f.locals.data();
    ValuePtr* frame_end = frame_begin + f.locals.size();

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
        return std::make_shared<NullValue>();
    }

    CallFrame& f = frames.back();

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

    ValuePtr result = std::make_shared<NullValue>();
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
        auto saved = interpreter->environment;
        interpreter->environment = call_env;
        ValuePtr result = std::make_shared<NullValue>();
        try {
            interpreter->execute_block(function->body, call_env);
        } catch (ReturnValueException& e) {
            result = e.value;
        } catch (RuntimeError& e) {
            interpreter->environment = saved;
            // Re-throw for the VM's run() loop to handle (may be caught by try/catch)
            throw;
        }
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
        ValuePtr result = std::make_shared<NullValue>();
        try {
            interpreter->execute_block(function->body, call_env);
        } catch (ReturnValueException& e) {
            result = e.value;
        } catch (RuntimeError& e) {
            interpreter->environment = saved;
            // Re-throw for the VM's run() loop to handle (may be caught by try/catch)
            throw;
        }
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
void VM::push_frame(ClosurePtr closure, const std::vector<ValuePtr>& args, int line) {
    CompiledFunctionPtr fn = closure->function;
    if (static_cast<int>(args.size()) < fn->arity) {
        runtime_error(line, "Too few arguments to " + fn->name);
        return;
    }
    frames.push_back(CallFrame{});
    CallFrame& f = frames.back();
    f.function = fn;
    f.code_start = fn->code.data();
    f.ip = f.code_start;
    f.upvalues = closure->upvalues;
    f.locals.resize(fn->max_locals > 0 ? fn->max_locals : fn->local_names.size());
    for (auto& l : f.locals) l = std::make_shared<NullValue>();
    // Bind parameters (locals[0] is reserved, params start at slot 1)
    for (size_t i = 0; i < fn->params.size(); ++i) {
        if (i < args.size()) {
            f.locals[i + 1] = args[i];
        } else if (fn->params[i].has_default) {
            if (fn->params[i].default_value) {
                f.locals[i + 1] = fn->params[i].default_value;
            }
            // Expression defaults are handled at call time (rare in VM)
        }
    }
}

// ============================================================================
// Binary/unary operations
// ============================================================================
ValuePtr VM::binary_op(OpCode op, const ValuePtr& a, const ValuePtr& b, int line) {
    auto na = dynamic_cast<NumberValue*>(a.get());
    auto nb = dynamic_cast<NumberValue*>(b.get());
    auto sa = dynamic_cast<StringValue*>(a.get());
    auto sb = dynamic_cast<StringValue*>(b.get());
    auto ba = dynamic_cast<BinaryValue*>(a.get());
    auto bb = dynamic_cast<BinaryValue*>(b.get());
    auto la = dynamic_cast<LnValue*>(a.get());
    auto lb = dynamic_cast<LnValue*>(b.get());

    if (op == OpCode::OP_ADD) {
        if (na && nb) return std::make_shared<NumberValue>(na->value + nb->value);
        // Binary + Number or Number + Binary → number
        if (ba && nb) return std::make_shared<NumberValue>(ba->toBigNumber() + nb->value);
        if (na && bb) return std::make_shared<NumberValue>(na->value + bb->toBigNumber());
        if (la && lb) {
            auto result = std::make_shared<LnValue>(std::vector<ValuePtr>{});
            result->elements = la->elements;
            for (auto& v : lb->elements) result->elements.push_back(v);
            return result;
        }
        // String or Binary with anything else → string concatenation
        if (sa || sb || ba || bb) {
            return std::make_shared<StringValue>(a->toString() + b->toString());
        }
    }
    if (op == OpCode::OP_MUL) {
        if (na && nb) return std::make_shared<NumberValue>(na->value * nb->value);
        // List * Number → repetition
        if (la && nb) {
            long long times = nb->value.toLongLong();
            if (times < 0) times = 0;
            auto result = std::make_shared<LnValue>(std::vector<ValuePtr>{});
            for (long long i = 0; i < times; ++i) {
                for (const auto& elem : la->elements) result->elements.push_back(elem->clone());
            }
            return result;
        }
        // Number * List → repetition (commutative)
        if (na && lb) {
            long long times = na->value.toLongLong();
            if (times < 0) times = 0;
            auto result = std::make_shared<LnValue>(std::vector<ValuePtr>{});
            for (long long i = 0; i < times; ++i) {
                for (const auto& elem : lb->elements) result->elements.push_back(elem->clone());
            }
            return result;
        }
    }
    if (na && nb) {
        switch (op) {
            case OpCode::OP_SUB: return std::make_shared<NumberValue>(na->value - nb->value);
            case OpCode::OP_DIV:
                if (nb->value == BigNumber(0)) { runtime_error(line, "Division by zero."); return std::make_shared<NullValue>(); }
                return std::make_shared<NumberValue>(na->value / nb->value);
            case OpCode::OP_MOD:
                if (nb->value == BigNumber(0)) { runtime_error(line, "Modulo by zero."); return std::make_shared<NullValue>(); }
                return std::make_shared<NumberValue>(na->value % nb->value);
            case OpCode::OP_POW: return std::make_shared<NumberValue>(na->value ^ nb->value);
            case OpCode::OP_LESS: return std::make_shared<NumberValue>(na->value < nb->value ? 1 : 0);
            case OpCode::OP_LESS_EQUAL: return std::make_shared<NumberValue>(na->value <= nb->value ? 1 : 0);
            case OpCode::OP_GREATER: return std::make_shared<NumberValue>(na->value > nb->value ? 1 : 0);
            case OpCode::OP_GREATER_EQUAL: return std::make_shared<NumberValue>(na->value >= nb->value ? 1 : 0);
            default: break;
        }
    }
    // Binary vs Number comparisons (convert binary to number)
    if (ba && nb) {
        BigNumber bv = ba->toBigNumber();
        switch (op) {
            case OpCode::OP_LESS: return std::make_shared<NumberValue>(bv < nb->value ? 1 : 0);
            case OpCode::OP_LESS_EQUAL: return std::make_shared<NumberValue>(bv <= nb->value ? 1 : 0);
            case OpCode::OP_GREATER: return std::make_shared<NumberValue>(bv > nb->value ? 1 : 0);
            case OpCode::OP_GREATER_EQUAL: return std::make_shared<NumberValue>(bv >= nb->value ? 1 : 0);
            default: break;
        }
    }
    if (na && bb) {
        BigNumber bv = bb->toBigNumber();
        switch (op) {
            case OpCode::OP_LESS: return std::make_shared<NumberValue>(na->value < bv ? 1 : 0);
            case OpCode::OP_LESS_EQUAL: return std::make_shared<NumberValue>(na->value <= bv ? 1 : 0);
            case OpCode::OP_GREATER: return std::make_shared<NumberValue>(na->value > bv ? 1 : 0);
            case OpCode::OP_GREATER_EQUAL: return std::make_shared<NumberValue>(na->value >= bv ? 1 : 0);
            default: break;
        }
    }
    if (sa && sb) {
        switch (op) {
            case OpCode::OP_LESS: return std::make_shared<NumberValue>(sa->value < sb->value ? 1 : 0);
            case OpCode::OP_LESS_EQUAL: return std::make_shared<NumberValue>(sa->value <= sb->value ? 1 : 0);
            case OpCode::OP_GREATER: return std::make_shared<NumberValue>(sa->value > sb->value ? 1 : 0);
            case OpCode::OP_GREATER_EQUAL: return std::make_shared<NumberValue>(sa->value >= sb->value ? 1 : 0);
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
    return std::make_shared<NullValue>();
}

ValuePtr VM::unary_op(OpCode op, const ValuePtr& a, int line) {
    if (op == OpCode::OP_NEGATE) {
        if (auto na = dynamic_cast<NumberValue*>(a.get())) {
            return std::make_shared<NumberValue>(BigNumber(0) - na->value);
        }
    }
    runtime_error(line, msg(Msg::SUB_TYPE));
    return std::make_shared<NullValue>();
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
            // Check if this opcode has a 3-byte operand
            switch (op) {
                case OpCode::OP_CONSTANT: case OpCode::OP_GET_LOCAL:
                case OpCode::OP_SET_LOCAL: case OpCode::OP_SET_LOCAL_KEEP:
                case OpCode::OP_GET_GLOBAL: case OpCode::OP_SET_GLOBAL:
                case OpCode::OP_DEFINE_GLOBAL:
                case OpCode::OP_GET_UPVALUE: case OpCode::OP_SET_UPVALUE:
                case OpCode::OP_CLOSE_UPVALUE:
                case OpCode::OP_GET_PROPERTY: case OpCode::OP_SET_PROPERTY:
                case OpCode::OP_BUILD_LIST: case OpCode::OP_BUILD_DICT:
                case OpCode::OP_JUMP: case OpCode::OP_JUMP_IF_FALSE:
                case OpCode::OP_JUMP_IF_TRUE:
                case OpCode::OP_JUMP_IF_FALSE_KEEP:
                case OpCode::OP_JUMP_IF_TRUE_KEEP:
                case OpCode::OP_LOOP:
                case OpCode::OP_CALL: case OpCode::OP_CLOSURE:
                case OpCode::OP_INPUT: case OpCode::OP_CAST:
                case OpCode::OP_TRY_BEGIN:
                case OpCode::OP_EVAL_AST: {
                    uint32_t operand = read_operand24(&fn->code[ip]);
                    std::cout << " " << operand;
                    ip += 3;
                    break;
                }
                default:
                    break;
            }
            std::cout << std::endl;
        }
    }
}
