#include "Interpreter.hpp"
#include "msg_cn.hpp"
#include <iostream>
#include <sstream>
#include <algorithm>
#include <set>
#include <thread>
#include <cstdlib>
#include <fstream>
#include <sys/stat.h>

#ifndef DEBUG
constexpr bool DEBUG = false;
#endif

// ===== AST accept() implementations =====

ValuePtr LiteralNode::accept(Interpreter& visitor) {
    return value;
}

ValuePtr ListLiteralNode::accept(Interpreter& visitor) {
    std::vector<ValuePtr> evaluated_elements;
    for (const auto& elem : elements) evaluated_elements.push_back(visitor.evaluate(elem));
    return std::make_shared<LnValue>(evaluated_elements);
}

ValuePtr DimLiteralNode::accept(Interpreter& visitor) {
    std::map<std::string, ValuePtr> result;
    for (const auto& entry : entries) {
        ValuePtr key_val = visitor.evaluate(entry.first);
        std::string key = key_val->toString();
        ValuePtr val = visitor.evaluate(entry.second);
        result[key] = val;
    }
    return std::make_shared<DimValue>(result);
}

ValuePtr VariableNode::accept(Interpreter& visitor) {
    try {
        ValuePtr val = visitor.environment->get(name);
        if (auto proxy = dynamic_cast<ModuleProxy*>(val.get())) {
            if (!proxy->loaded) {
                ValuePtr result = visitor.load_module(proxy);
                visitor.environment->assign(name, result);
                return result;
            }
        }
        return val;
    }
    catch (RuntimeError& e) { throw RuntimeError(line, e.what()); }
}

ValuePtr AssignmentNode::accept(Interpreter& visitor) {
    auto val = visitor.evaluate(value);
    if (auto get_node = dynamic_cast<GetNode*>(target.get())) {
        try {
            auto object = visitor.evaluate(get_node->object);
            if (auto instance = std::dynamic_pointer_cast<Instance>(object)) {
                instance->set(get_node->name, val);
            } else {
                throw RuntimeError(line, msg(Msg::NO_SET_PROP));
            }
        } catch (const std::runtime_error& e) {
            throw RuntimeError(line, e.what());
        }
    } else {
        visitor.assignToLValue(target, val, line);
    }
    return val;
}

ValuePtr VarDeclarationNode::accept(Interpreter& visitor) {
    ValuePtr val = std::make_shared<NullValue>();
    if (initializer) { val = visitor.evaluate(initializer); }
    if (keyword.type == TokenType::DEC) {
        if (auto s_val = dynamic_cast<StringValue*>(val.get())) {
            try { val = std::make_shared<NumberValue>(BigNumber(s_val->value)); }
            catch (const std::invalid_argument&) { throw RuntimeError(line, fmt(Msg::STR_TO_NUM, s_val->value)); }
        } else if (auto b_val = dynamic_cast<BinaryValue*>(val.get())) { val = std::make_shared<NumberValue>(b_val->toBigNumber()); }
        else if (dynamic_cast<NullValue*>(val.get())) { val = std::make_shared<NumberValue>(0); }
    } else if (keyword.type == TokenType::STR) {
        val = std::make_shared<StringValue>(val->toString());
    } else if (keyword.type == TokenType::BIN) {
        if (auto s_val = dynamic_cast<StringValue*>(val.get())) { try { val = std::make_shared<BinaryValue>(s_val->value); } catch(...) { throw RuntimeError(line, fmt(Msg::STR_TO_BIN, s_val->value)); } }
        else if (dynamic_cast<NullValue*>(val.get())) { val = std::make_shared<BinaryValue>(std::vector<uint8_t>{0}); }
    } else if (keyword.type == TokenType::LN) {
        if (!dynamic_cast<LnValue*>(val.get()) && !dynamic_cast<NullValue*>(val.get())) { throw RuntimeError(line, msg(Msg::LN_INIT_LN)); }
        if (dynamic_cast<NullValue*>(val.get())) { val = std::make_shared<LnValue>(std::vector<ValuePtr>{}); }
    } else if (keyword.type == TokenType::DIM) {
        if (!dynamic_cast<DimValue*>(val.get()) && !dynamic_cast<NullValue*>(val.get())) { throw RuntimeError(line, msg(Msg::DIM_INIT_DIM)); }
        if (dynamic_cast<NullValue*>(val.get())) { val = std::make_shared<DimValue>(); }
    } else if (keyword.type == TokenType::ANY) {
        // any type: keep value as-is
    }
    visitor.environment->define(name, val);
    return std::make_shared<NullValue>();
}

ValuePtr UsingNode::accept(Interpreter& visitor) {
    try {
        ValuePtr val = visitor.environment->get(original_name);
        visitor.environment->define(alias_name, val);
    } catch (RuntimeError& e) { throw RuntimeError(line, e.what()); }
    return std::make_shared<NullValue>();
}

ValuePtr UnaryOpNode::accept(Interpreter& visitor) {
    ValuePtr right_val = visitor.evaluate(right);
    switch (op.type) {
        case TokenType::MINUS: {
            auto zero = std::make_shared<NumberValue>(0);
            try { return zero->subtract(*right_val); }
            catch (const std::runtime_error& e) { throw RuntimeError(op.line, e.what()); }
        }
        case TokenType::NOT: return std::make_shared<NumberValue>(right_val->isTruthy() ? 0 : 1);
        default: throw RuntimeError(op.line, "Invalid unary operator.");
    }
}

ValuePtr BinaryOpNode::accept(Interpreter& visitor) {
    auto left_val = visitor.evaluate(left);
    auto right_val = visitor.evaluate(right);
    try {
        switch (op.type) {
            case TokenType::PLUS: return left_val->add(*right_val);
            case TokenType::MINUS: return left_val->subtract(*right_val);
            case TokenType::STAR: return left_val->multiply(*right_val);
            case TokenType::SLASH: return left_val->divide(*right_val);
            case TokenType::CARET: return left_val->power(*right_val);
            case TokenType::MODULO: return left_val->modulo(*right_val);
            case TokenType::EQUAL_EQUAL: return std::make_shared<NumberValue>(left_val->isEqualTo(*right_val) ? 1 : 0);
            case TokenType::BANG_EQUAL: return std::make_shared<NumberValue>(!left_val->isEqualTo(*right_val) ? 1 : 0);
            case TokenType::LESS: return std::make_shared<NumberValue>(left_val->isLessThan(*right_val) ? 1 : 0);
            case TokenType::LESS_EQUAL: return std::make_shared<NumberValue>(!right_val->isLessThan(*left_val) ? 1 : 0);
            case TokenType::GREATER: return std::make_shared<NumberValue>(right_val->isLessThan(*left_val) ? 1 : 0);
            case TokenType::GREATER_EQUAL: return std::make_shared<NumberValue>(!left_val->isLessThan(*right_val) ? 1 : 0);
            default: break;
        }
    } catch (const std::runtime_error& e) { throw RuntimeError(op.line, e.what()); }
    return std::make_shared<NullValue>();
}

ValuePtr LogicalOpNode::accept(Interpreter& visitor) {
    ValuePtr left_val = visitor.evaluate(left);
    if (op.type == TokenType::OR) {
        if (left_val->isTruthy()) return left_val;
    } else {
        if (!left_val->isTruthy()) return left_val;
    }
    return visitor.evaluate(right);
}

ValuePtr TypeConversionNode::accept(Interpreter& visitor) {
    ValuePtr val = visitor.evaluate(expression);
    switch (type_keyword.type) {
        case TokenType::DEC:
            if (dynamic_cast<NumberValue*>(val.get())) return val;
            if (auto s_val = dynamic_cast<StringValue*>(val.get())) { try { return std::make_shared<NumberValue>(BigNumber(s_val->value)); } catch (const std::invalid_argument&) { throw RuntimeError(line, std::string("Cannot convert string '") + s_val->value + "' to a number."); } }
            if (auto b_val = dynamic_cast<BinaryValue*>(val.get())) { return std::make_shared<NumberValue>(b_val->toBigNumber()); }
            throw RuntimeError(line, "Unsupported conversion to 'dec'.");
        case TokenType::STR:
            return std::make_shared<StringValue>(val->toString());
        case TokenType::BIN:
            if (dynamic_cast<BinaryValue*>(val.get())) return val;
            if (auto s_val = dynamic_cast<StringValue*>(val.get())) { try { return std::make_shared<BinaryValue>(s_val->value); } catch (...) { throw RuntimeError(line, std::string("Cannot convert string '") + s_val->value + "' to binary. Expected '0x...' format."); } }
            throw RuntimeError(line, "Unsupported conversion to 'bin'.");
        case TokenType::LN:
            if (dynamic_cast<LnValue*>(val.get())) return val;
            if (dynamic_cast<NullValue*>(val.get())) return std::make_shared<LnValue>(std::vector<ValuePtr>{});
            throw RuntimeError(line, "Unsupported conversion to 'ln'.");
        case TokenType::DIM:
            if (dynamic_cast<DimValue*>(val.get())) return val;
            if (dynamic_cast<NullValue*>(val.get())) return std::make_shared<DimValue>();
            throw RuntimeError(line, "Unsupported conversion to 'dim'.");
        default:
            throw RuntimeError(line, "Invalid type for 'as' conversion.");
    }
}

ValuePtr SubscriptNode::accept(Interpreter& visitor) {
    try {
        auto object = visitor.evaluate(this->object);
        if (!is_slice) {
            auto index = visitor.evaluate(this->start);
            return object->getSubscript(*index);
        } else {
            ValuePtr start_v = this->start ? visitor.evaluate(this->start) : std::make_shared<NullValue>();
            ValuePtr end_v = this->end ? visitor.evaluate(this->end) : std::make_shared<NullValue>();
            ValuePtr step_v = this->step ? visitor.evaluate(this->step) : std::make_shared<NullValue>();
            return object->getSlice(start_v, end_v, step_v);
        }
    } catch (const std::runtime_error& e) { throw RuntimeError(line, e.what()); }
}

ValuePtr ClassDefNode::accept(Interpreter& visitor) {
    std::map<std::string, std::shared_ptr<Function>> methods_map;
    for (const auto& method_ast : methods) {
        auto func_def_node = std::dynamic_pointer_cast<FnDefNode>(method_ast);
        if (func_def_node) {
            auto method_func = std::make_shared<Function>(
                func_def_node->name, func_def_node->params, func_def_node->body, visitor.environment);
            methods_map[func_def_node->name] = method_func;
        }
    }
    auto klass = std::make_shared<Class>(name, fields, initializer_body, methods_map, visitor.environment);
    visitor.environment->define(name, klass);
    return std::make_shared<NullValue>();
}

ValuePtr GetNode::accept(Interpreter& visitor) {
    auto object_val = visitor.evaluate(object);
    if (auto instance = std::dynamic_pointer_cast<Instance>(object_val)) {
        try { return instance->get(name); }
        catch (const std::runtime_error& e) { throw RuntimeError(line, e.what()); }
    }
    if (auto dim_val = std::dynamic_pointer_cast<DimValue>(object_val)) {
        auto key = std::make_shared<StringValue>(name);
        try { return dim_val->getSubscript(*key); }
        catch (const std::runtime_error& e) { throw RuntimeError(line, e.what()); }
    }
    throw RuntimeError(line, fmt(Msg::NO_PROP, name));
}

ValuePtr SetNode::accept(Interpreter& visitor) {
    throw RuntimeError(line, msg(Msg::SET_NODE_ERR));
}

ValuePtr LambdaNode::accept(Interpreter& visitor) {
    auto function = std::make_shared<Function>("", params, body, visitor.environment);
    return std::make_shared<FunctionValue>(function);
}

ValuePtr StructDefNode::accept(Interpreter& visitor) {
    auto klass = std::make_shared<Class>(name, fields,
        std::vector<AstNodePtr>{},
        std::map<std::string, std::shared_ptr<Function>>{},
        visitor.environment);
    visitor.environment->define(name, klass);
    return std::make_shared<NullValue>();
}

ValuePtr SwapNode::accept(Interpreter& visitor) {
    ValuePtr val1 = visitor.evaluate(left);
    ValuePtr val2 = visitor.evaluate(right);
    visitor.assignToLValue(left, val2, line);
    visitor.assignToLValue(right, val1, line);
    return std::make_shared<NullValue>();
}

ValuePtr RequireNode::accept(Interpreter& visitor) {
    std::string alias = alias_name;
    if (alias.empty()) {
        size_t pos = module_path.find_last_of("/\\");
        alias = (pos == std::string::npos) ? module_path : module_path.substr(pos + 1);
    }
    auto proxy = std::make_shared<ModuleProxy>(module_path);
    visitor.environment->define(alias, proxy);
    return std::make_shared<NullValue>();
}

ValuePtr IfStatementNode::accept(Interpreter& visitor) {
    visitor.check_timeout(line);
    auto condition_val = visitor.evaluate(condition);
    if (condition_val->isTruthy()) {
        visitor.execute_block(then_branch, std::make_shared<Environment>(visitor.environment));
    } else if (!else_branch.empty()) {
        visitor.execute_block(else_branch, std::make_shared<Environment>(visitor.environment));
    }
    return std::make_shared<NullValue>();
}

ValuePtr WhileStatementNode::accept(Interpreter& visitor) {
    while (true) {
        auto condition_val = visitor.evaluate(condition);
        if (!condition_val->isTruthy()) break;
        visitor.check_timeout(line);
        try { visitor.execute_block(do_branch, std::make_shared<Environment>(visitor.environment)); }
        catch (const BreakException&) { break; }
        catch (const ContinueException&) { /* skip to next iteration */ }
    }
    if (!finally_branch.empty()) {
        visitor.execute_block(finally_branch, std::make_shared<Environment>(visitor.environment));
    }
    return std::make_shared<NullValue>();
}

ValuePtr ForInNode::accept(Interpreter& visitor) {
    ValuePtr iter_val = visitor.evaluate(iterable);
    LnValue* ln_val = dynamic_cast<LnValue*>(iter_val.get());
    if (!ln_val) throw RuntimeError(line, "for-in requires an ln (list) value.");
    
    for (size_t i = 0; i < ln_val->elements.size(); ++i) {
        visitor.check_timeout(line);
        try {
            auto block_env = std::make_shared<Environment>(visitor.environment);
            block_env->define(var_name, ln_val->elements[i]);
            visitor.execute_block(body, block_env);
        } catch (const BreakException&) { break; }
        catch (const ContinueException&) { continue; }
    }
    return std::make_shared<NullValue>();
}

ValuePtr LoopForNode::accept(Interpreter& visitor) {
    ValuePtr count_val = visitor.evaluate(count_expr);
    auto num_val = dynamic_cast<NumberValue*>(count_val.get());
    if (!num_val) throw RuntimeError(line, msg(Msg::LOOP_MUST_NUM));
    long long count = 0;
    try { count = num_val->value.toLongLong(); }
    catch (...) { throw RuntimeError(line, msg(Msg::LOOP_TOO_LARGE)); }

    for (long long i = 0; i < count; ++i) {
        visitor.check_timeout(line);
        try {
            auto block_env = std::make_shared<Environment>(visitor.environment);
            if (!index_var_name.empty()) {
                block_env->define(index_var_name, std::make_shared<NumberValue>(BigNumber(std::to_string(i))));
            }
            visitor.execute_block(body, block_env);
        } catch (const BreakException&) { break; }
        catch (const ContinueException&) { /* continue to next iteration */ }
    }
    return std::make_shared<NullValue>();
}

ValuePtr LoopUntilNode::accept(Interpreter& visitor) {
    long long i = 0;
    while (true) {
        visitor.check_timeout(line);
        auto block_env = std::make_shared<Environment>(visitor.environment);
        if (!index_var_name.empty()) {
            block_env->define(index_var_name, std::make_shared<NumberValue>(BigNumber(std::to_string(i++))));
        }
        std::shared_ptr<Environment> previous = visitor.environment;
        try {
            visitor.environment = block_env;
            for (const auto& stmt : body) { visitor.execute(stmt); }
            if (condition) {
                ValuePtr condition_val = visitor.evaluate(condition);
                if (condition_val->isTruthy()) { visitor.environment = previous; break; }
            }
            visitor.environment = previous;
        } catch (const BreakException&) { visitor.environment = previous; break; }
        catch (const ContinueException&) { visitor.environment = previous; /* continue */ }
        catch (...) { visitor.environment = previous; throw; }
    }
    return std::make_shared<NullValue>();
}

ValuePtr BreakNode::accept(Interpreter& visitor) { throw BreakException(); }
ValuePtr ContinueNode::accept(Interpreter& visitor) { throw ContinueException(); }

ValuePtr AwaitStatementNode::accept(Interpreter& visitor) {
    while (!visitor.evaluate(condition)->isTruthy()) {
        visitor.check_timeout(line);
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    visitor.execute_block(then_branch, std::make_shared<Environment>(visitor.environment));
    return std::make_shared<NullValue>();
}

ValuePtr SayNode::accept(Interpreter& visitor) {
    auto val = visitor.evaluate(expression);
    std::cout << val->toString() << std::endl;
    return std::make_shared<NullValue>();
}

ValuePtr InpNode::accept(Interpreter& visitor) {
    auto prompt = visitor.evaluate(expression);
    std::cout << prompt->toString();
    std::string input;
    std::getline(std::cin, input);
    return std::make_shared<StringValue>(input);
}

ValuePtr FnDefNode::accept(Interpreter& visitor) {
    auto function = std::make_shared<Function>(name, params, body, visitor.environment);
    visitor.environment->define(name, std::make_shared<FunctionValue>(function));
    return std::make_shared<NullValue>();
}

ValuePtr CallNode::accept(Interpreter& visitor) {
    visitor.check_timeout(line);
    auto callee_val = visitor.evaluate(callee);
    std::vector<ValuePtr> arg_values;
    for (const auto& arg_expr : arguments) { arg_values.push_back(visitor.evaluate(arg_expr)); }

    if (auto native_fn = dynamic_cast<NativeFnValue*>(callee_val.get())) {
        visitor.call_stack.push_back({native_fn->name, line});
        try {
            ValuePtr result = native_fn->call(arg_values);
            visitor.call_stack.pop_back();
            return result;
        } catch (const RuntimeError& re) {
            visitor.call_stack.pop_back();
            throw;
        } catch (const std::exception& e) {
            visitor.call_stack.pop_back();
            throw RuntimeError(line, e.what());
        }
    }

    // Bound method call
    if (auto bound_method = dynamic_cast<BoundMethodValue*>(callee_val.get())) {
        auto function = bound_method->method;
        auto call_env = std::make_shared<Environment>(function->closure);
        call_env->define("this", bound_method->instance);

        const auto& param_defs = function->params;
        size_t num_provided_args = arg_values.size();
        size_t num_required_params = 0;
        for (const auto& p : param_defs) { if (!p.has_default) num_required_params++; }

        if (num_provided_args < num_required_params) {
            std::stringstream ss;
            ss << "Method '" << function->name << fmt_int(Msg::ARGS_AT_LEAST, num_required_params) << num_provided_args << ".";
            throw RuntimeError(line, ss.str());
        }
        if (num_provided_args > param_defs.size()) {
            std::stringstream ss;
            ss << "Method '" << function->name << fmt_int(Msg::ARGS_AT_MOST, param_defs.size()) << num_provided_args << ".";
            throw RuntimeError(line, ss.str());
        }

        for (size_t i = 0; i < function->params.size(); ++i) {
            ValuePtr current_arg_value;
            if (i < num_provided_args) { current_arg_value = arg_values[i]; }
            else if (param_defs[i].default_expr) { current_arg_value = visitor.evaluate(param_defs[i].default_expr); }
            else { current_arg_value = param_defs[i].default_value->clone(); }
            if (!is_type_compatible(param_defs[i].type_keyword, current_arg_value)) {
                std::stringstream ss;
                ss << fmt3(Msg::ARG_TYPE, std::to_string(i + 1), function->name, param_defs[i].name)
                   << "期望 " << token_type_to_string(param_defs[i].type_keyword) << ", 得到 ";
                if (dynamic_cast<NumberValue*>(current_arg_value.get())) ss << "dec";
                else if (dynamic_cast<StringValue*>(current_arg_value.get())) ss << "str";
                else if (dynamic_cast<BinaryValue*>(current_arg_value.get())) ss << "bin";
                else if (dynamic_cast<LnValue*>(current_arg_value.get())) ss << "ln";
                else if (dynamic_cast<DimValue*>(current_arg_value.get())) ss << "dim";
                else ss << "unknown";
                ss << "。";
                throw RuntimeError(line, ss.str());
            }
            call_env->define(param_defs[i].name, current_arg_value);
        }

        visitor.call_stack.push_back({function->name, line});
        ValuePtr return_val = std::make_shared<NullValue>();
        try { visitor.execute_block(function->body, call_env); }
        catch (const ReturnValueException& rv) { return_val = rv.value; }
        visitor.call_stack.pop_back();
        return return_val;
    }

    // Regular function call
    if (auto func_val = dynamic_cast<FunctionValue*>(callee_val.get())) {
        auto function = func_val->value;
        auto call_env = std::make_shared<Environment>(function->closure);

        const auto& param_defs = function->params;
        size_t num_provided_args = arg_values.size();
        size_t num_required_params = 0;
        for (const auto& p : param_defs) { if (!p.has_default) num_required_params++; }

        if (num_provided_args < num_required_params) {
            std::stringstream ss;
            ss << "Function '" << function->name << fmt_int(Msg::ARGS_AT_LEAST, num_required_params) << num_provided_args << ".";
            throw RuntimeError(line, ss.str());
        }
        if (num_provided_args > param_defs.size()) {
            std::stringstream ss;
            ss << "Function '" << function->name << fmt_int(Msg::ARGS_AT_MOST, param_defs.size()) << num_provided_args << ".";
            throw RuntimeError(line, ss.str());
        }

        for (size_t i = 0; i < function->params.size(); ++i) {
            ValuePtr current_arg_value;
            if (i < num_provided_args) { current_arg_value = arg_values[i]; }
            else if (param_defs[i].default_expr) { current_arg_value = visitor.evaluate(param_defs[i].default_expr); }
            else { current_arg_value = param_defs[i].default_value->clone(); }
            if (!is_type_compatible(param_defs[i].type_keyword, current_arg_value)) {
                std::stringstream ss;
                ss << fmt3(Msg::ARG_TYPE, std::to_string(i + 1), function->name, param_defs[i].name)
                   << "期望 " << token_type_to_string(param_defs[i].type_keyword) << ", 得到 ";
                if (dynamic_cast<NumberValue*>(current_arg_value.get())) ss << "dec";
                else if (dynamic_cast<StringValue*>(current_arg_value.get())) ss << "str";
                else if (dynamic_cast<BinaryValue*>(current_arg_value.get())) ss << "bin";
                else if (dynamic_cast<LnValue*>(current_arg_value.get())) ss << "ln";
                else if (dynamic_cast<DimValue*>(current_arg_value.get())) ss << "dim";
                else ss << "unknown";
                ss << "。";
                throw RuntimeError(line, ss.str());
            }
            call_env->define(param_defs[i].name, current_arg_value);
        }

        visitor.call_stack.push_back({function->name, line});
        ValuePtr return_val = std::make_shared<NullValue>();
        try { visitor.execute_block(function->body, call_env); }
        catch (const ReturnValueException& rv) { return_val = rv.value; }
        visitor.call_stack.pop_back();
        return return_val;
    }

    throw RuntimeError(line, fmt(Msg::CALL_ONLY, callee_val->repr()));
}

ValuePtr ReturnNode::accept(Interpreter& visitor) {
    ValuePtr val = std::make_shared<NullValue>();
    if (value) { val = visitor.evaluate(value); }
    throw ReturnValueException(val);
}

ValuePtr RaiseNode::accept(Interpreter& visitor) {
    throw PyRiteRaiseException(visitor.evaluate(expression));
}

ValuePtr TryCatchNode::accept(Interpreter& visitor) {
    std::unique_ptr<std::exception_ptr> captured_exception = nullptr;
    try {
        try {
            visitor.execute_block(try_branch, std::make_shared<Environment>(visitor.environment));
        } catch (const PyRiteRaiseException& ex) {
            auto catch_env = std::make_shared<Environment>(visitor.environment);
            catch_env->define(exception_var, ex.value);
            visitor.execute_block(catch_branch, catch_env);
        } catch (const RuntimeError& ex) {
            auto exception_obj = std::make_shared<ExceptionValue>(std::make_shared<StringValue>(ex.what()));
            auto catch_env = std::make_shared<Environment>(visitor.environment);
            catch_env->define(exception_var, exception_obj);
            visitor.execute_block(catch_branch, catch_env);
        }
    } catch (...) {
        captured_exception.reset(new std::exception_ptr(std::current_exception()));
    }
    if (!finally_branch.empty()) {
        visitor.execute_block(finally_branch, std::make_shared<Environment>(visitor.environment));
    }
    if (captured_exception) { std::rethrow_exception(*captured_exception); }
    return std::make_shared<NullValue>();
}

ValuePtr ExpressionStatementNode::accept(Interpreter& visitor) {
    visitor.evaluate(expression);
    return std::make_shared<NullValue>();
}

// ===== Interpreter implementation =====

Interpreter::Interpreter() : globals(std::make_shared<Environment>()), environment(globals), time_limit_ms(0) {
    define_native_functions();
}

void Interpreter::interpret(const std::vector<AstNodePtr>& statements) {
    try {
        for (const auto& stmt : statements) {
            check_timeout(stmt->line);
            execute(stmt);
        }
    } catch (const BreakException&) {
        std::cerr << msg(Msg::RUNTIME_PREFIX) << "0: break used outside loop." << std::endl;
        print_stack_trace();
    } catch (const ContinueException&) {
        std::cerr << msg(Msg::RUNTIME_PREFIX) << "0: continue used outside loop." << std::endl;
        print_stack_trace();
    } catch (const PyRiteRaiseException& ex) {
        std::cerr << msg(Msg::UNCAUGHT_EX) << ex.value->repr() << std::endl;
        print_stack_trace();
    } catch (const RuntimeError& error) {
        std::cerr << msg(Msg::RUNTIME_PREFIX) << error.line << ": " << error.what() << std::endl;
        print_stack_trace();
    }
}

void Interpreter::execute(const AstNodePtr& stmt) {
    stmt->accept(*this);
}

ValuePtr Interpreter::evaluate(const AstNodePtr& expr) {
    return expr->accept(*this);
}

void Interpreter::execute_block(const std::vector<AstNodePtr>& statements, std::shared_ptr<Environment> block_env) {
    std::shared_ptr<Environment> previous = this->environment;
    try {
        this->environment = block_env;
        for (const auto& stmt : statements) {
            check_timeout(stmt->line);
            execute(stmt);
        }
    } catch (...) {
        this->environment = previous;
        throw;
    }
    this->environment = previous;
}

void Interpreter::check_timeout(int line) {
    if (time_limit_ms > 0) {
        auto now = std::chrono::high_resolution_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time).count();
        if (elapsed >= time_limit_ms) {
            throw RuntimeError(line, fmt_int(Msg::TIMEOUT, time_limit_ms));
        }
    }
}

void Interpreter::assignToLValue(AstNodePtr target, ValuePtr val, int line) {
    if (auto var_node = dynamic_cast<VariableNode*>(target.get())) {
        try { this->environment->assign(var_node->name, val); }
        catch (RuntimeError& e) { throw RuntimeError(line, e.what()); }
    } else if (auto sub_node = dynamic_cast<SubscriptNode*>(target.get())) {
        try {
            auto object = this->evaluate(sub_node->object);
            if (!sub_node->is_slice) {
                auto index = this->evaluate(sub_node->start);
                object->setSubscript(*index, val);
            } else {
                ValuePtr start_v = sub_node->start ? this->evaluate(sub_node->start) : std::make_shared<NullValue>();
                ValuePtr end_v = sub_node->end ? this->evaluate(sub_node->end) : std::make_shared<NullValue>();
                ValuePtr step_v = sub_node->step ? this->evaluate(sub_node->step) : std::make_shared<NullValue>();
                object->setSlice(start_v, end_v, step_v, val);
            }
        } catch (const std::runtime_error& e) { throw RuntimeError(line, e.what()); }
    } else {
        throw RuntimeError(line, msg(Msg::INVALID_ASSIGN));
    }
}

void Interpreter::print_stack_trace() {
    if (call_stack.empty()) return;
    std::cerr << msg(Msg::STACK_TRACE) << std::endl;
    for (auto it = call_stack.rbegin(); it != call_stack.rend(); ++it) {
        std::cerr << "  在 " << it->function_name << " (第 " << it->call_site_line << " 行)" << std::endl;
    }
    call_stack.clear();
}

std::string Interpreter::resolve_module_path(const std::string& path) {
    struct stat st;
    if (stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode)) {
        std::string index_path = path + "/_index.pr";
        if (stat(index_path.c_str(), &st) == 0 && S_ISREG(st.st_mode)) {
            return index_path;
        }
    }
    if (stat(path.c_str(), &st) == 0 && S_ISREG(st.st_mode)) {
        return path;
    }
    std::string with_ext = path + ".pr";
    if (stat(with_ext.c_str(), &st) == 0 && S_ISREG(st.st_mode)) {
        return with_ext;
    }
    return path;
}

ValuePtr Interpreter::load_module(ModuleProxy* proxy) {
    if (loading_modules.count(proxy->file_path)) {
        throw RuntimeError(0, "Circular require detected for module '" + proxy->file_path + "'.");
    }
    loading_modules.insert(proxy->file_path);

    std::string resolved = resolve_module_path(proxy->file_path);
    std::ifstream file(resolved);
    if (!file.is_open()) {
        loading_modules.erase(proxy->file_path);
        throw RuntimeError(0, "Cannot open module file '" + resolved + "'.");
    }
    std::string source_code((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    file.close();

    bool is_dir_module = (resolved.find("_index.pr") != std::string::npos);

    Parser parser(source_code);
    auto statements = parser.parse();
    if (parser.has_error()) {
        loading_modules.erase(proxy->file_path);
        throw RuntimeError(0, "Parse error in module '" + proxy->file_path + "'.");
    }

    auto module_env = std::make_shared<Environment>(this->globals);
    std::shared_ptr<Environment> previous = this->environment;
    try {
        this->environment = module_env;
        for (const auto& stmt : statements) {
            check_timeout(stmt->line);
            execute(stmt);
        }
    } catch (...) {
        this->environment = previous;
        loading_modules.erase(proxy->file_path);
        throw;
    }
    this->environment = previous;

    std::map<std::string, ValuePtr> exports;
    if (is_dir_module) {
        for (const auto& stmt : statements) {
            if (auto fn_node = std::dynamic_pointer_cast<FnDefNode>(stmt)) {
                if (fn_node->is_exposed) {
                    try { exports[fn_node->name] = module_env->get(fn_node->name); } catch (...) {}
                }
            } else if (auto var_node = std::dynamic_pointer_cast<VarDeclarationNode>(stmt)) {
                if (var_node->is_exposed) {
                    try { exports[var_node->name] = module_env->get(var_node->name); } catch (...) {}
                }
            }
        }
    } else {
        auto& vals = module_env->get_values();
        for (const auto& kv : vals) {
            exports[kv.first] = kv.second;
        }
    }

    auto module_dict = std::make_shared<DimValue>(exports);

    try {
        ValuePtr on_req = module_env->get("_on_load");
        if (auto fn_val = dynamic_cast<FunctionValue*>(on_req.get())) {
            auto call_env = std::make_shared<Environment>(fn_val->value->closure);
            call_stack.push_back({"_on_load", 0});
            try { execute_block(fn_val->value->body, call_env); }
            catch (const ReturnValueException&) {}
            call_stack.pop_back();
        }
    } catch (const RuntimeError&) {}

    proxy->loaded = true;
    loading_modules.erase(proxy->file_path);
    return module_dict;
}

// ===== Native function definitions =====
void Interpreter::define_native_functions() {
#define REQUIRE_ARGS(name, count) if(args.size() != count) throw std::runtime_error(std::string(name) + fmt_int(Msg::NATIVE_ARGS, count));
#define REQUIRE_MIN_ARGS(name, count) if(args.size() < count) throw std::runtime_error(std::string(name) + fmt_int(Msg::NATIVE_MIN_ARGS, count));
#define GET_NUM(val, var_name) auto var_name = dynamic_cast<NumberValue*>(val.get()); if(!var_name) throw std::runtime_error(msg(Msg::NATIVE_NUM));
#define GET_LN(val, var_name) auto var_name = dynamic_cast<LnValue*>(val.get()); if(!var_name) throw std::runtime_error(msg(Msg::NATIVE_LN));

    globals->define("Exception", std::make_shared<NativeFnValue>("Exception", [](const std::vector<ValuePtr>& args){
        REQUIRE_ARGS("Exception", 1);
        return std::make_shared<ExceptionValue>(args[0]);
    }));
    globals->define("abs", std::make_shared<NativeFnValue>("abs", [](const std::vector<ValuePtr>& args){
        REQUIRE_ARGS("abs", 1); GET_NUM(args[0], num_val);
        return std::make_shared<NumberValue>(num_val->value.abs());
    }));
    globals->define("len", std::make_shared<NativeFnValue>("len", [](const std::vector<ValuePtr>& args) {
        REQUIRE_ARGS("len", 1);
        if (auto str_val = dynamic_cast<StringValue*>(args[0].get()))
            return std::make_shared<NumberValue>(BigNumber(std::to_string(str_val->value.length())));
        if (auto list_val = dynamic_cast<LnValue*>(args[0].get()))
            return std::make_shared<NumberValue>(BigNumber(std::to_string(list_val->elements.size())));
        throw std::runtime_error("Argument to len() must be a string or a list.");
    }));
    globals->define("rt", std::make_shared<NativeFnValue>("rt", [](const std::vector<ValuePtr>& args){
        if (args.size() < 1 || args.size() > 2) throw std::runtime_error(msg(Msg::NATIVE_RT));
        GET_NUM(args[0], num_val);
        BigNumber n = 2;
        if (args.size() == 2) { GET_NUM(args[1], n_val); n = n_val->value; }
        return std::make_shared<NumberValue>(BigNumber::root(num_val->value, n));
    }));
    globals->define("sort", std::make_shared<NativeFnValue>("sort", [](const std::vector<ValuePtr>& args){
        REQUIRE_ARGS("sort", 1); GET_LN(args[0], list_val);
        auto new_list = std::make_shared<LnValue>(list_val->elements);
        std::sort(new_list->elements.begin(), new_list->elements.end(),
            [](const ValuePtr& a, const ValuePtr& b) { try { return a->isLessThan(*b); } catch (...) { return false; } });
        return new_list;
    }));
    globals->define("setify", std::make_shared<NativeFnValue>("setify", [](const std::vector<ValuePtr>& args){
        REQUIRE_ARGS("setify", 1); GET_LN(args[0], list_val);
        struct ValuePtrLess {
            bool operator()(const ValuePtr& a, const ValuePtr& b) const {
                try { return a->isLessThan(*b); } catch (...) { return a.get() < b.get(); }
            }
        };
        std::set<ValuePtr, ValuePtrLess> seen;
        std::vector<ValuePtr> unique_elements;
        for (const auto& elem : list_val->elements) {
            if (seen.find(elem) == seen.end()) { seen.insert(elem); unique_elements.push_back(elem); }
        }
        return std::make_shared<LnValue>(unique_elements);
    }));
    auto min_max_logic = [](const std::vector<ValuePtr>& args, bool is_max) {
        if (args.empty()) throw std::runtime_error(msg(Msg::NATIVE_MINMAX_EMPTY));
        const std::vector<ValuePtr>* values_to_compare;
        std::vector<ValuePtr> temp_list;
        if (args.size() == 1 && dynamic_cast<LnValue*>(args[0].get())) {
            values_to_compare = &dynamic_cast<LnValue*>(args[0].get())->elements;
        } else {
            temp_list = args; values_to_compare = &temp_list;
        }
        if (values_to_compare->empty()) throw std::runtime_error(msg(Msg::NATIVE_MINMAX_LIST));
        ValuePtr extreme = (*values_to_compare)[0];
        for (size_t i = 1; i < values_to_compare->size(); ++i) {
            ValuePtr current = (*values_to_compare)[i];
            try {
                if (is_max) { if (extreme->isLessThan(*current)) extreme = current; }
                else { if (current->isLessThan(*extreme)) extreme = current; }
            } catch (const std::runtime_error&) { throw std::runtime_error(msg(Msg::NATIVE_MINMAX_CMP)); }
        }
        return extreme;
    };
    globals->define("max", std::make_shared<NativeFnValue>("max", [min_max_logic](const std::vector<ValuePtr>& args){ return min_max_logic(args, true); }));
    globals->define("min", std::make_shared<NativeFnValue>("min", [min_max_logic](const std::vector<ValuePtr>& args){ return min_max_logic(args, false); }));
    globals->define("countdown", std::make_shared<NativeFnValue>("countdown", [](const std::vector<ValuePtr>& args){
        REQUIRE_ARGS("countdown", 1); GET_NUM(args[0], sec_val);
        auto end_time = std::chrono::high_resolution_clock::now() + std::chrono::milliseconds(sec_val->value.toLongLong() * 1000);
        auto timer_fn_body = [end_time](const std::vector<ValuePtr>& inner_args) -> ValuePtr {
            if (!inner_args.empty()) throw std::runtime_error(msg(Msg::NATIVE_TIMER));
            auto now = std::chrono::high_resolution_clock::now();
            return std::make_shared<NumberValue>(now >= end_time ? 1 : 0);
        };
        return std::make_shared<NativeFnValue>("timer", timer_fn_body);
    }));
    globals->define("hash", std::make_shared<NativeFnValue>("hash", [](const std::vector<ValuePtr>& args){
        REQUIRE_ARGS("hash", 2);
        std::string data_str = args[0]->toString();
        GET_NUM(args[1], key_val);
        long long key = key_val->value.toLongLong();
        unsigned long long hash_val = 5381;
        for (char c : data_str) hash_val = ((hash_val << 5) + hash_val) + c;
        hash_val ^= key;
        return std::make_shared<NumberValue>(BigNumber((long long)hash_val));
    }));
    globals->define("sin", std::make_shared<NativeFnValue>("sin", [](const std::vector<ValuePtr>& args) {
        REQUIRE_ARGS("sin", 1); GET_NUM(args[0], x);
        return std::make_shared<NumberValue>(BigNumber(std::to_string(sin(x->value.toLongLong()))));
    }));
    globals->define("cos", std::make_shared<NativeFnValue>("cos", [](const std::vector<ValuePtr>& args) {
        REQUIRE_ARGS("cos", 1); GET_NUM(args[0], x);
        return std::make_shared<NumberValue>(BigNumber(std::to_string(cos(x->value.toLongLong()))));
    }));
    globals->define("tan", std::make_shared<NativeFnValue>("tan", [](const std::vector<ValuePtr>& args) {
        REQUIRE_ARGS("tan", 1); GET_NUM(args[0], x);
        return std::make_shared<NumberValue>(BigNumber(std::to_string(tan(x->value.toLongLong()))));
    }));
    globals->define("log", std::make_shared<NativeFnValue>("log", [](const std::vector<ValuePtr>& args) {
        REQUIRE_ARGS("log", 1); GET_NUM(args[0], x);
        if (x->value <= BigNumber(0)) throw std::runtime_error(msg(Msg::NATIVE_LOG_POS));
        return std::make_shared<NumberValue>(BigNumber(std::to_string(log(x->value.toLongLong()))));
    }));
    globals->define("new", std::make_shared<NativeFnValue>("new", [this](const std::vector<ValuePtr>& args) -> ValuePtr {
        REQUIRE_MIN_ARGS("new", 1);
        auto class_val = std::dynamic_pointer_cast<Class>(args[0]);
        if (!class_val) throw std::runtime_error(msg(Msg::NEW_CLASS));
        auto instance = std::make_shared<Instance>(class_val);
        instance->instance_env->define("this", instance);
        if (!class_val->initializer_body.empty()) {
            try { this->execute_block(class_val->initializer_body, instance->instance_env); }
            catch (const ReturnValueException&) { throw std::runtime_error("Cannot return a value from an instance initializer."); }
        }
        return instance;
    }));
    globals->define("set_precision", std::make_shared<NativeFnValue>("set_precision", [](const std::vector<ValuePtr>& args){
        REQUIRE_ARGS("set_precision", 1); GET_NUM(args[0], num_val);
        try { BigNumber::set_default_precision(num_val->value.toLongLong()); }
        catch (const std::exception& e) { throw std::runtime_error(e.what()); }
        return std::make_shared<NullValue>();
    }));
    globals->define("get_precision", std::make_shared<NativeFnValue>("get_precision", [](const std::vector<ValuePtr>& args){
        return std::make_shared<NumberValue>(BigNumber(BigNumber::get_default_precision()));
    }));
    globals->define("approx", std::make_shared<NativeFnValue>("approx", [](const std::vector<ValuePtr>& args){
        REQUIRE_ARGS("approx", 2); GET_NUM(args[0], num_to_approx); GET_NUM(args[1], precision_val);
        try { return std::make_shared<NumberValue>(num_to_approx->value.approx(precision_val->value.toLongLong())); }
        catch (const std::exception& e) { throw std::runtime_error(e.what()); }
    }));
    globals->define("is_int", std::make_shared<NativeFnValue>("is_int", [](const std::vector<ValuePtr>& args){
        REQUIRE_ARGS("is_int", 1); GET_NUM(args[0], num_val);
        return std::make_shared<NumberValue>(num_val->value.isInteger() ? 1 : 0);
    }));
    globals->define("is_neg", std::make_shared<NativeFnValue>("is_neg", [](const std::vector<ValuePtr>& args){
        REQUIRE_ARGS("is_neg", 1); GET_NUM(args[0], num_val);
        return std::make_shared<NumberValue>(num_val->value.isNegative() ? 1 : 0);
    }));
    globals->define("to_double", std::make_shared<NativeFnValue>("to_double", [](const std::vector<ValuePtr>& args){
        REQUIRE_ARGS("to_double", 1); GET_NUM(args[0], num_val);
        try { return std::make_shared<NumberValue>(BigNumber(std::to_string(num_val->value.toDouble()))); }
        catch (const std::exception& e) { throw std::runtime_error(e.what()); }
    }));
    globals->define("halt", std::make_shared<NativeFnValue>("halt", [](const std::vector<ValuePtr>& args){
        exit(0);
        return std::make_shared<NullValue>();
    }));
    globals->define("run", std::make_shared<NativeFnValue>("run", [](const std::vector<ValuePtr>& args){
        return std::make_shared<NullValue>();
    }));

#undef REQUIRE_ARGS
#undef REQUIRE_MIN_ARGS
#undef GET_NUM
#undef GET_LN
}
