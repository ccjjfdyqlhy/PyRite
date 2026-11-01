#include "Interpreter.hpp"
#include "ModulePath.hpp"
#include "msg_cn.hpp"
#include <iostream>
#include <sstream>
#include <algorithm>
#include <set>
#include <thread>
#include <cstdlib>
#include <cctype>
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
    return make_value<LnValue>(evaluated_elements);
}

ValuePtr DimLiteralNode::accept(Interpreter& visitor) {
    std::unordered_map<std::string, ValuePtr> result;
    for (const auto& entry : entries) {
        ValuePtr key_val = visitor.evaluate(entry.first);
        std::string key = key_val->toString();
        ValuePtr val = visitor.evaluate(entry.second);
        result[key] = val;
    }
    return make_value<DimValue>(result);
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
            if (auto instance = ptr_cast<Instance>(object)) {
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
    ValuePtr val = make_value<NullValue>();
    if (initializer) { val = visitor.evaluate(initializer); }
    if (keyword.type == TokenType::DEC) {
        if (auto s_val = dynamic_cast<StringValue*>(val.get())) {
            try { val = make_value<NumberValue>(BigNumber(s_val->value)); }
            catch (const std::invalid_argument&) { throw RuntimeError(line, fmt(Msg::STR_TO_NUM, s_val->value)); }
        } else if (auto b_val = dynamic_cast<BinaryValue*>(val.get())) { val = make_value<NumberValue>(b_val->toBigNumber()); }
        else if (dynamic_cast<NullValue*>(val.get())) { val = make_value<NumberValue>(0); }
        else if (dynamic_cast<LnValue*>(val.get()) || dynamic_cast<DimValue*>(val.get())) { throw RuntimeError(line, msg(Msg::DEC_INIT_NUM)); }
    } else if (keyword.type == TokenType::STR) {
        val = make_value<StringValue>(val->toString());
    } else if (keyword.type == TokenType::BIN) {
        if (auto s_val = dynamic_cast<StringValue*>(val.get())) { try { val = make_value<BinaryValue>(s_val->value); } catch(...) { throw RuntimeError(line, fmt(Msg::STR_TO_BIN, s_val->value)); } }
        else if (dynamic_cast<NullValue*>(val.get())) { val = make_value<BinaryValue>(std::vector<uint8_t>{0}); }
        else if (!dynamic_cast<BinaryValue*>(val.get())) { throw RuntimeError(line, msg(Msg::BIN_INIT_BIN)); }
    } else if (keyword.type == TokenType::LN) {
        if (!dynamic_cast<LnValue*>(val.get()) && !dynamic_cast<NullValue*>(val.get())) { throw RuntimeError(line, msg(Msg::LN_INIT_LN)); }
        if (dynamic_cast<NullValue*>(val.get())) { val = make_value<LnValue>(std::vector<ValuePtr>{}); }
    } else if (keyword.type == TokenType::DIM) {
        if (!dynamic_cast<DimValue*>(val.get()) && !dynamic_cast<NullValue*>(val.get())) { throw RuntimeError(line, msg(Msg::DIM_INIT_DIM)); }
        if (dynamic_cast<NullValue*>(val.get())) { val = make_value<DimValue>(); }
    } else if (keyword.type == TokenType::ANY) {
        // any type: keep value as-is
    }
    visitor.environment->define(name, val);
    return make_value<NullValue>();
}

ValuePtr UsingNode::accept(Interpreter& visitor) {
    try {
        ValuePtr val = visitor.environment->get(original_name);
        visitor.environment->define(alias_name, val);
    } catch (RuntimeError& e) { throw RuntimeError(line, e.what()); }
    return make_value<NullValue>();
}

ValuePtr UnaryOpNode::accept(Interpreter& visitor) {
    ValuePtr right_val = visitor.evaluate(right);
    switch (op.type) {
        case TokenType::MINUS: {
            auto zero = make_value<NumberValue>(0);
            try { return zero->subtract(*right_val); }
            catch (const std::runtime_error& e) { throw RuntimeError(op.line, e.what()); }
        }
        case TokenType::NOT: return make_value<NumberValue>(right_val->isTruthy() ? 0 : 1);
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
            case TokenType::EQUAL_EQUAL: return make_value<NumberValue>(left_val->isEqualTo(*right_val) ? 1 : 0);
            case TokenType::BANG_EQUAL: return make_value<NumberValue>(!left_val->isEqualTo(*right_val) ? 1 : 0);
            case TokenType::LESS: return make_value<NumberValue>(left_val->isLessThan(*right_val) ? 1 : 0);
            case TokenType::LESS_EQUAL: return make_value<NumberValue>(!right_val->isLessThan(*left_val) ? 1 : 0);
            case TokenType::GREATER: return make_value<NumberValue>(right_val->isLessThan(*left_val) ? 1 : 0);
            case TokenType::GREATER_EQUAL: return make_value<NumberValue>(!left_val->isLessThan(*right_val) ? 1 : 0);
            default: break;
        }
    } catch (const std::runtime_error& e) { throw RuntimeError(op.line, e.what()); }
    return make_value<NullValue>();
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
            if (auto s_val = dynamic_cast<StringValue*>(val.get())) { try { return make_value<NumberValue>(BigNumber(s_val->value)); } catch (const std::invalid_argument&) { throw RuntimeError(line, std::string("Cannot convert string '") + s_val->value + "' to a number."); } }
            if (auto b_val = dynamic_cast<BinaryValue*>(val.get())) { return make_value<NumberValue>(b_val->toBigNumber()); }
            throw RuntimeError(line, "Unsupported conversion to 'dec'.");
        case TokenType::STR:
            return make_value<StringValue>(val->toString());
        case TokenType::BIN:
            if (dynamic_cast<BinaryValue*>(val.get())) return val;
            if (auto s_val = dynamic_cast<StringValue*>(val.get())) { try { return make_value<BinaryValue>(s_val->value); } catch (...) { throw RuntimeError(line, std::string("Cannot convert string '") + s_val->value + "' to binary. Expected '0x...' format."); } }
            throw RuntimeError(line, "Unsupported conversion to 'bin'.");
        case TokenType::LN:
            if (dynamic_cast<LnValue*>(val.get())) return val;
            if (dynamic_cast<NullValue*>(val.get())) return make_value<LnValue>(std::vector<ValuePtr>{});
            throw RuntimeError(line, "Unsupported conversion to 'ln'.");
        case TokenType::DIM:
            if (dynamic_cast<DimValue*>(val.get())) return val;
            if (dynamic_cast<NullValue*>(val.get())) return make_value<DimValue>();
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
            ValuePtr start_v = this->start ? visitor.evaluate(this->start) : make_value<NullValue>();
            ValuePtr end_v = this->end ? visitor.evaluate(this->end) : make_value<NullValue>();
            ValuePtr step_v = this->step ? visitor.evaluate(this->step) : make_value<NullValue>();
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
    auto klass = make_value<Class>(name, fields, initializer_body, methods_map, visitor.environment);
    visitor.environment->define(name, klass);
    return make_value<NullValue>();
}

ValuePtr GetNode::accept(Interpreter& visitor) {
    auto object_val = visitor.evaluate(object);
    if (auto instance = ptr_cast<Instance>(object_val)) {
        try { return instance->get(name); }
        catch (const std::runtime_error& e) { throw RuntimeError(line, e.what()); }
    }
    if (auto dim_val = ptr_cast<DimValue>(object_val)) {
        // A key of the dictionary comes first: a dictionary may hold a
        // key called "keys".
        auto it = dim_val->dict.find(name);
        if (it != dim_val->dict.end()) return it->second;
        if (auto member = dim_val->getMember(name)) return member;
        throw RuntimeError(line, fmt(Msg::NO_PROP, name));
    }
    // The members of a list or a string.
    if (auto member = object_val->getMember(name)) return member;
    throw RuntimeError(line, fmt(Msg::NO_PROP, name));
}

ValuePtr SetNode::accept(Interpreter& visitor) {
    throw RuntimeError(line, msg(Msg::SET_NODE_ERR));
}

ValuePtr LambdaNode::accept(Interpreter& visitor) {
    auto function = std::make_shared<Function>("", params, body, visitor.environment);
    return make_value<FunctionValue>(function);
}

ValuePtr StructDefNode::accept(Interpreter& visitor) {
    auto klass = make_value<Class>(name, fields,
        std::vector<AstNodePtr>{},
        std::map<std::string, std::shared_ptr<Function>>{},
        visitor.environment);
    visitor.environment->define(name, klass);
    return make_value<NullValue>();
}

ValuePtr SwapNode::accept(Interpreter& visitor) {
    ValuePtr val1 = visitor.evaluate(left);
    ValuePtr val2 = visitor.evaluate(right);
    visitor.assignToLValue(left, val2, line);
    visitor.assignToLValue(right, val1, line);
    return make_value<NullValue>();
}

ValuePtr RequireNode::accept(Interpreter& visitor) {
    std::string alias = alias_name;
    if (alias.empty()) {
        size_t pos = module_path.find_last_of("/\\");
        alias = (pos == std::string::npos) ? module_path : module_path.substr(pos + 1);
    }
    auto proxy = make_value<ModuleProxy>(module_path);
    // The module is loaded right here, not when it is first used: the
    // compiler puts the module into the program where the require
    // stands, and the two have to behave the same way.
    ValuePtr loaded = visitor.load_module(proxy.get());
    visitor.environment->define(alias, loaded);
    return make_value<NullValue>();
}

ValuePtr IfStatementNode::accept(Interpreter& visitor) {
    visitor.check_timeout(line);
    auto condition_val = visitor.evaluate(condition);
    if (condition_val->isTruthy()) {
        visitor.execute_block(then_branch, std::make_shared<Environment>(visitor.environment));
    } else if (!else_branch.empty()) {
        visitor.execute_block(else_branch, std::make_shared<Environment>(visitor.environment));
    }
    return make_value<NullValue>();
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
    return make_value<NullValue>();
}

ValuePtr ForInNode::accept(Interpreter& visitor) {
    ValuePtr iter_val = visitor.evaluate(iterable);

    // Same iterable semantics as the bytecode for-in expansion: lists walk
    // their elements, strings their characters; anything else fails with
    // the message the VM produces for the same value.
    if (auto ln_val = ptr_cast<LnValue>(iter_val)) {
        for (size_t i = 0; i < ln_val->elements.size(); ++i) {
            visitor.check_timeout(line);
            try {
                auto block_env = std::make_shared<Environment>(visitor.environment);
                block_env->define(var_name, ln_val->elements[i]);
                visitor.execute_block(body, block_env);
            } catch (const BreakException&) { break; }
            catch (const ContinueException&) { continue; }
        }
        return make_value<NullValue>();
    }
    if (auto str_val = ptr_cast<StringValue>(iter_val)) {
        for (size_t i = 0; i < str_val->value.size(); ++i) {
            visitor.check_timeout(line);
            try {
                auto block_env = std::make_shared<Environment>(visitor.environment);
                block_env->define(var_name, make_value<StringValue>(std::string(1, str_val->value[i])));
                visitor.execute_block(body, block_env);
            } catch (const BreakException&) { break; }
            catch (const ContinueException&) { continue; }
        }
        return make_value<NullValue>();
    }
    // The VM's expansion does GET_LEN (dicts answer with their size) and
    // then GET_INDEX with a number, which fails with this exact message.
    if (ptr_cast<DimValue>(iter_val)) {
        throw RuntimeError(line, msg(Msg::DIM_KEY_TYPE));
    }
    throw RuntimeError(line, "Argument to len() must be a string or a list.");
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
                block_env->define(index_var_name, make_value<NumberValue>(BigNumber(std::to_string(i))));
            }
            visitor.execute_block(body, block_env);
        } catch (const BreakException&) { break; }
        catch (const ContinueException&) { /* continue to next iteration */ }
    }
    return make_value<NullValue>();
}

ValuePtr LoopUntilNode::accept(Interpreter& visitor) {
    long long i = 0;
    while (true) {
        visitor.check_timeout(line);
        auto block_env = std::make_shared<Environment>(visitor.environment);
        if (!index_var_name.empty()) {
            block_env->define(index_var_name, make_value<NumberValue>(BigNumber(std::to_string(i++))));
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
    return make_value<NullValue>();
}

ValuePtr BreakNode::accept(Interpreter& visitor) { throw BreakException(); }
ValuePtr ContinueNode::accept(Interpreter& visitor) { throw ContinueException(); }

ValuePtr AwaitStatementNode::accept(Interpreter& visitor) {
    while (!visitor.evaluate(condition)->isTruthy()) {
        visitor.check_timeout(line);
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    visitor.execute_block(then_branch, std::make_shared<Environment>(visitor.environment));
    return make_value<NullValue>();
}

ValuePtr SayNode::accept(Interpreter& visitor) {
    auto val = visitor.evaluate(expression);
    std::cout << val->toString() << std::endl;
    return make_value<NullValue>();
}

ValuePtr InpNode::accept(Interpreter& visitor) {
    auto prompt = visitor.evaluate(expression);
    std::cout << prompt->toString();
    std::string input;
    std::getline(std::cin, input);
    return make_value<StringValue>(input);
}

ValuePtr FnDefNode::accept(Interpreter& visitor) {
    auto function = std::make_shared<Function>(name, params, body, visitor.environment);
    visitor.environment->define(name, make_value<FunctionValue>(function));
    return make_value<NullValue>();
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
        } catch (PyRiteRaiseException& pe) {
            // A raise inside a native call (a class initializer, a host
            // hook) carries its payload in .value, not in what(): rethrow
            // intact so the surrounding catch sees the raised value.
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
            std::vector<std::string> missing;
            for (size_t i = num_provided_args; i < param_defs.size(); ++i)
                if (!param_defs[i].has_default) missing.push_back(param_defs[i].name);
            throw RuntimeError(line, args_missing_text(
                kind_name_text(Msg::KIND_METHOD, function->name), missing,
                num_required_params, num_provided_args));
        }
        if (num_provided_args > param_defs.size()) {
            std::vector<std::string> names;
            for (const auto& p : param_defs) names.push_back(p.name);
            throw RuntimeError(line, args_too_many_text(
                kind_name_text(Msg::KIND_METHOD, function->name), names,
                param_defs.size(), num_provided_args));
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
        if (++visitor.call_depth > Interpreter::MAX_CALL_DEPTH) {
            visitor.call_depth--;
            visitor.call_stack.pop_back();
            throw RuntimeError(line, "Recursion depth exceeds the limit (" + std::to_string(Interpreter::MAX_CALL_DEPTH) + ").");
        }
        ValuePtr return_val = make_value<NullValue>();
        try { visitor.execute_block(function->body, call_env); }
        catch (const ReturnValueException& rv) { return_val = rv.value; }
        visitor.call_depth--;
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
            std::vector<std::string> missing;
            for (size_t i = num_provided_args; i < param_defs.size(); ++i)
                if (!param_defs[i].has_default) missing.push_back(param_defs[i].name);
            throw RuntimeError(line, args_missing_text(
                kind_name_text(Msg::KIND_FN, function->name), missing,
                num_required_params, num_provided_args));
        }
        if (num_provided_args > param_defs.size()) {
            std::vector<std::string> names;
            for (const auto& p : param_defs) names.push_back(p.name);
            throw RuntimeError(line, args_too_many_text(
                kind_name_text(Msg::KIND_FN, function->name), names,
                param_defs.size(), num_provided_args));
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
        if (++visitor.call_depth > Interpreter::MAX_CALL_DEPTH) {
            visitor.call_depth--;
            visitor.call_stack.pop_back();
            throw RuntimeError(line, "Recursion depth exceeds the limit (" + std::to_string(Interpreter::MAX_CALL_DEPTH) + ").");
        }
        ValuePtr return_val = make_value<NullValue>();
        try { visitor.execute_block(function->body, call_env); }
        catch (const ReturnValueException& rv) { return_val = rv.value; }
        visitor.call_depth--;
        visitor.call_stack.pop_back();
        return return_val;
    }

    throw RuntimeError(line, fmt(Msg::CALL_ONLY, callee_val->repr()));
}

ValuePtr ReturnNode::accept(Interpreter& visitor) {
    ValuePtr val = make_value<NullValue>();
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
            auto exception_obj = make_value<ExceptionValue>(make_value<StringValue>(ex.what()));
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
    return make_value<NullValue>();
}

ValuePtr ExpressionStatementNode::accept(Interpreter& visitor) {
    visitor.evaluate(expression);
    return make_value<NullValue>();
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
        had_error = true;
        std::cerr << msg(Msg::RUNTIME_PREFIX) << "0: break used outside loop." << std::endl;
        print_stack_trace();
    } catch (const ContinueException&) {
        had_error = true;
        std::cerr << msg(Msg::RUNTIME_PREFIX) << "0: continue used outside loop." << std::endl;
        print_stack_trace();
    } catch (const PyRiteRaiseException& ex) {
        had_error = true;
        std::cerr << msg(Msg::UNCAUGHT_EX) << ex.value->repr() << std::endl;
        print_stack_trace();
    } catch (const RuntimeError& error) {
        had_error = true;
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
                ValuePtr start_v = sub_node->start ? this->evaluate(sub_node->start) : make_value<NullValue>();
                ValuePtr end_v = sub_node->end ? this->evaluate(sub_node->end) : make_value<NullValue>();
                ValuePtr step_v = sub_node->step ? this->evaluate(sub_node->step) : make_value<NullValue>();
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

// The same resolution the compiler does, so a story that runs here runs
// in the browser too: the directory of the file that asks first, then
// the library paths.
std::string Interpreter::resolve_module_path(const std::string& path) {
    const std::string& relative_to = module_dir_.empty() ? script_dir_ : module_dir_;
    std::string resolved;
    bool is_dir_module = false;
    std::string tried;
    if (ModulePath::resolve(path, relative_to, resolved, is_dir_module, tried)) {
        return resolved;
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
        throw RuntimeError(0, "Cannot open module file '" + resolved +
                              "'.  A require looks in the directory of the "
                              "file that asks for it, then in PYRITE_LIB_PATH, "
                              "then in the lib directory next to the compiler "
                              "and in ~/PyRite/lib.");
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
    // While the module runs, its own requires are relative to it.
    std::string previous_module_dir = module_dir_;
    module_dir_ = ModulePath::dir_of(resolved);
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

    std::unordered_map<std::string, ValuePtr> exports;
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

    auto module_dict = make_value<DimValue>(exports);

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
    module_dir_ = previous_module_dir;
    return module_dict;
}

// ===== Native function definitions =====
void Interpreter::define_native_functions() {
#define REQUIRE_ARGS(name, count) if(args.size() != count) throw std::runtime_error(std::string(name) + fmt_int(Msg::NATIVE_ARGS, count));
#define REQUIRE_MIN_ARGS(name, count) if(args.size() < count) throw std::runtime_error(std::string(name) + fmt_int(Msg::NATIVE_MIN_ARGS, count));
#define GET_NUM(val, var_name) auto var_name = dynamic_cast<NumberValue*>(val.get()); if(!var_name) throw std::runtime_error(msg(Msg::NATIVE_NUM));
#define GET_LN(val, var_name) auto var_name = dynamic_cast<LnValue*>(val.get()); if(!var_name) throw std::runtime_error(msg(Msg::NATIVE_LN));

    globals->define("Exception", make_value<NativeFnValue>("Exception", [](const std::vector<ValuePtr>& args){
        REQUIRE_ARGS("Exception", 1);
        return make_value<ExceptionValue>(args[0]);
    }));
    globals->define("abs", make_value<NativeFnValue>("abs", [](const std::vector<ValuePtr>& args){
        REQUIRE_ARGS("abs", 1); GET_NUM(args[0], num_val);
        return make_value<NumberValue>(num_val->value.abs());
    }));
    globals->define("len", make_value<NativeFnValue>("len", [](const std::vector<ValuePtr>& args) {
        REQUIRE_ARGS("len", 1);
        if (auto str_val = dynamic_cast<StringValue*>(args[0].get()))
            return make_value<NumberValue>(BigNumber(std::to_string(str_val->value.length())));
        if (auto list_val = dynamic_cast<LnValue*>(args[0].get()))
            return make_value<NumberValue>(BigNumber(std::to_string(list_val->elements.size())));
        throw std::runtime_error("Argument to len() must be a string or a list.");
    }));
    globals->define("rt", make_value<NativeFnValue>("rt", [](const std::vector<ValuePtr>& args){
        if (args.size() < 1 || args.size() > 2) throw std::runtime_error(msg(Msg::NATIVE_RT));
        GET_NUM(args[0], num_val);
        BigNumber n = 2;
        if (args.size() == 2) { GET_NUM(args[1], n_val); n = n_val->value; }
        return make_value<NumberValue>(BigNumber::root(num_val->value, n));
    }));
    globals->define("sort", make_value<NativeFnValue>("sort", [](const std::vector<ValuePtr>& args){
        REQUIRE_ARGS("sort", 1); GET_LN(args[0], list_val);
        auto new_list = make_value<LnValue>(list_val->elements);
        std::sort(new_list->elements.begin(), new_list->elements.end(),
            [](const ValuePtr& a, const ValuePtr& b) { try { return a->isLessThan(*b); } catch (...) { return false; } });
        return new_list;
    }));
    globals->define("setify", make_value<NativeFnValue>("setify", [](const std::vector<ValuePtr>& args){
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
        return make_value<LnValue>(unique_elements);
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
    globals->define("max", make_value<NativeFnValue>("max", [min_max_logic](const std::vector<ValuePtr>& args){ return min_max_logic(args, true); }));
    globals->define("min", make_value<NativeFnValue>("min", [min_max_logic](const std::vector<ValuePtr>& args){ return min_max_logic(args, false); }));
    globals->define("countdown", make_value<NativeFnValue>("countdown", [](const std::vector<ValuePtr>& args){
        REQUIRE_ARGS("countdown", 1); GET_NUM(args[0], sec_val);
        auto end_time = std::chrono::high_resolution_clock::now() + std::chrono::milliseconds(sec_val->value.toLongLong() * 1000);
        auto timer_fn_body = [end_time](const std::vector<ValuePtr>& inner_args) -> ValuePtr {
            if (!inner_args.empty()) throw std::runtime_error(msg(Msg::NATIVE_TIMER));
            auto now = std::chrono::high_resolution_clock::now();
            return make_value<NumberValue>(now >= end_time ? 1 : 0);
        };
        return make_value<NativeFnValue>("timer", timer_fn_body);
    }));
    globals->define("hash", make_value<NativeFnValue>("hash", [](const std::vector<ValuePtr>& args){
        REQUIRE_ARGS("hash", 2);
        std::string data_str = args[0]->toString();
        GET_NUM(args[1], key_val);
        long long key = key_val->value.toLongLong();
        unsigned long long hash_val = 5381;
        for (char c : data_str) hash_val = ((hash_val << 5) + hash_val) + c;
        hash_val ^= key;
        return make_value<NumberValue>(BigNumber((long long)hash_val));
    }));
    globals->define("sin", make_value<NativeFnValue>("sin", [](const std::vector<ValuePtr>& args) {
        REQUIRE_ARGS("sin", 1); GET_NUM(args[0], x);
        return make_value<NumberValue>(BigNumber(std::to_string(sin(x->value.toLongLong()))));
    }));
    globals->define("cos", make_value<NativeFnValue>("cos", [](const std::vector<ValuePtr>& args) {
        REQUIRE_ARGS("cos", 1); GET_NUM(args[0], x);
        return make_value<NumberValue>(BigNumber(std::to_string(cos(x->value.toLongLong()))));
    }));
    globals->define("tan", make_value<NativeFnValue>("tan", [](const std::vector<ValuePtr>& args) {
        REQUIRE_ARGS("tan", 1); GET_NUM(args[0], x);
        return make_value<NumberValue>(BigNumber(std::to_string(tan(x->value.toLongLong()))));
    }));
    globals->define("log", make_value<NativeFnValue>("log", [](const std::vector<ValuePtr>& args) {
        REQUIRE_ARGS("log", 1); GET_NUM(args[0], x);
        if (x->value <= BigNumber(0)) throw std::runtime_error(msg(Msg::NATIVE_LOG_POS));
        return make_value<NumberValue>(BigNumber(std::to_string(log(x->value.toLongLong()))));
    }));
    globals->define("new", make_value<NativeFnValue>("new", [this](const std::vector<ValuePtr>& args) -> ValuePtr {
        REQUIRE_MIN_ARGS("new", 1);
        auto class_val = ptr_cast<Class>(args[0]);
        if (!class_val) throw std::runtime_error(msg(Msg::NEW_CLASS));
        if (args.size() - 1 > class_val->fields.size()) {
            std::vector<std::string> names;
            for (const auto& f : class_val->fields) names.push_back(f.name);
            throw std::runtime_error(args_too_many_text(
                kind_name_text(Msg::KIND_CTOR, class_val->name), names,
                class_val->fields.size(), args.size() - 1));
        }
        // ins 类（有方法或初始化代码）的必选字段缺参在构造时报错，与
        // 字节码 VM 的 OP_CALL_CTOR 同一句话；纯数据 struct 保留"先 new
        // 后赋值"的工作流（教程 struct 一节），字段以 null 起步。
        size_t provided = args.size() - 1;
        bool class_like = !class_val->methods.empty() || !class_val->initializer_body.empty();
        if (class_like) {
            size_t required = 0;
            std::vector<std::string> missing;
            for (size_t i = 0; i < class_val->fields.size(); ++i) {
                if (!class_val->fields[i].has_default) required++;
                if (i >= provided && !class_val->fields[i].has_default)
                    missing.push_back(class_val->fields[i].name);
            }
            if (!missing.empty())
                throw std::runtime_error(args_missing_text(
                    kind_name_text(Msg::KIND_CTOR, class_val->name), missing,
                    required, provided));
        }
        auto instance = make_value<Instance>(class_val);
        instance->instance_env->define("this", instance);
        // Constructor arguments fill the declared fields in order, with
        // the same type check a method call applies to its parameters.
        for (size_t i = 1; i < args.size(); ++i) {
            const ParameterDefinition& field = class_val->fields[i - 1];
            if (!is_type_compatible(field.type_keyword, args[i]))
                throw std::runtime_error(class_val->name + " field '" + field.name +
                                         "' expects " + token_type_to_string(field.type_keyword) + ".");
            instance->instance_env->define(field.name, args[i]);
        }
        // Fields without an argument fall back to their defaults; an
        // expression default is evaluated now, in the instance scope. A
        // field with neither an argument nor a default is an error
        // instead of a silent null.
        for (size_t i = args.size() - 1; i < class_val->fields.size(); ++i) {
            const ParameterDefinition& field = class_val->fields[i];
            if (field.default_expr) {
                auto saved = this->environment;
                this->environment = instance->instance_env;
                ValuePtr dv;
                try { dv = this->evaluate(field.default_expr); }
                catch (...) { this->environment = saved; throw; }
                this->environment = saved;
                if (!is_type_compatible(field.type_keyword, dv))
                    throw std::runtime_error(class_val->name + " field '" + field.name +
                                             "' expects " + token_type_to_string(field.type_keyword) + ".");
                instance->instance_env->define(field.name, dv);
            }
            // A field with neither an argument nor a default starts as
            // null - the documented struct workflow assigns fields after
            // new(), so this stays legal (see guide, struct section).
        }
        if (!class_val->initializer_body.empty()) {
            try { this->execute_block(class_val->initializer_body, instance->instance_env); }
            catch (const ReturnValueException&) { throw std::runtime_error("Cannot return a value from an instance initializer."); }
        }
        return instance;
    }));
    globals->define("set_precision", make_value<NativeFnValue>("set_precision", [](const std::vector<ValuePtr>& args){
        REQUIRE_ARGS("set_precision", 1); GET_NUM(args[0], num_val);
        try { BigNumber::set_default_precision(num_val->value.toLongLong()); }
        catch (const std::exception& e) { throw std::runtime_error(e.what()); }
        return make_value<NullValue>();
    }));
    globals->define("get_precision", make_value<NativeFnValue>("get_precision", [](const std::vector<ValuePtr>& args){
        return make_value<NumberValue>(BigNumber(BigNumber::get_default_precision()));
    }));
    globals->define("approx", make_value<NativeFnValue>("approx", [](const std::vector<ValuePtr>& args){
        REQUIRE_ARGS("approx", 2); GET_NUM(args[0], num_to_approx); GET_NUM(args[1], precision_val);
        try { return make_value<NumberValue>(num_to_approx->value.approx(precision_val->value.toLongLong())); }
        catch (const std::exception& e) { throw std::runtime_error(e.what()); }
    }));
    globals->define("is_int", make_value<NativeFnValue>("is_int", [](const std::vector<ValuePtr>& args){
        REQUIRE_ARGS("is_int", 1); GET_NUM(args[0], num_val);
        return make_value<NumberValue>(num_val->value.isInteger() ? 1 : 0);
    }));
    globals->define("is_neg", make_value<NativeFnValue>("is_neg", [](const std::vector<ValuePtr>& args){
        REQUIRE_ARGS("is_neg", 1); GET_NUM(args[0], num_val);
        return make_value<NumberValue>(num_val->value.isNegative() ? 1 : 0);
    }));
    globals->define("to_double", make_value<NativeFnValue>("to_double", [](const std::vector<ValuePtr>& args){
        REQUIRE_ARGS("to_double", 1); GET_NUM(args[0], num_val);
        try { return make_value<NumberValue>(BigNumber(std::to_string(num_val->value.toDouble()))); }
        catch (const std::exception& e) { throw std::runtime_error(e.what()); }
    }));
    globals->define("halt", make_value<NativeFnValue>("halt", [](const std::vector<ValuePtr>& args){
        exit(0);
        return make_value<NullValue>();
    }));
    globals->define("run", make_value<NativeFnValue>("run", [](const std::vector<ValuePtr>& args){
        return make_value<NullValue>();
    }));


    // ===== The helpers the browser runtime has =========================
    //
    // The two runtimes did not have the same builtins: a story that
    // used split() or range() ran in the browser and failed on this
    // machine, so a writer could not try a story out here.  These are
    // the ones a story actually uses, and they behave the way the
    // JavaScript runtime behaves, so the same program says the same
    // thing in both.
    //
    // Two differences stay, and both are about the dictionary: this
    // runtime keeps a dict in a map, so keys() and values() answer in
    // sorted order, while the browser keeps them in the order they
    // were written.  And the case functions here are ASCII, while the
    // browser knows every alphabet.
    {
        auto number = [](long long v) -> ValuePtr {
            return make_value<NumberValue>(BigNumber(v));
        };
        auto text_value = [](const std::string& s) -> ValuePtr {
            return make_value<StringValue>(s);
        };
        auto is_space = [](char c) -> bool {
            return c == ' ' || c == '\t' || c == '\r' || c == '\n' ||
                   c == '\f' || c == '\v';
        };
        auto string_arg = [](const ValuePtr& value, const char* who) -> std::string {
            auto str_val = dynamic_cast<StringValue*>(value.get());
            if (!str_val) throw std::runtime_error(std::string("Argument to ") + who + "() must be a string.");
            return str_val->value;
        };

        globals->define("upper", make_value<NativeFnValue>("upper", [string_arg](const std::vector<ValuePtr>& args){
            REQUIRE_ARGS("upper", 1);
            std::string out = string_arg(args[0], "upper");
            for (char& c : out) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
            return make_value<StringValue>(out);
        }));
        globals->define("lower", make_value<NativeFnValue>("lower", [string_arg](const std::vector<ValuePtr>& args){
            REQUIRE_ARGS("lower", 1);
            std::string out = string_arg(args[0], "lower");
            for (char& c : out) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            return make_value<StringValue>(out);
        }));
        globals->define("trim", make_value<NativeFnValue>("trim", [string_arg, is_space](const std::vector<ValuePtr>& args){
            REQUIRE_ARGS("trim", 1);
            std::string text = string_arg(args[0], "trim");
            size_t start = 0;
            while (start < text.size() && is_space(text[start])) start++;
            size_t stop = text.size();
            while (stop > start && is_space(text[stop - 1])) stop--;
            return make_value<StringValue>(text.substr(start, stop - start));
        }));
        globals->define("split", make_value<NativeFnValue>("split", [string_arg](const std::vector<ValuePtr>& args){
            REQUIRE_MIN_ARGS("split", 1);
            std::string text = string_arg(args[0], "split");
            std::string sep = " ";
            if (args.size() > 1) sep = string_arg(args[1], "split");
            std::vector<ValuePtr> parts;
            if (sep.empty()) {
                for (size_t i = 0; i < text.size(); i++)
                    parts.push_back(make_value<StringValue>(text.substr(i, 1)));
            } else {
                size_t pos = 0;
                while (true) {
                    size_t hit = text.find(sep, pos);
                    if (hit == std::string::npos) {
                        parts.push_back(make_value<StringValue>(text.substr(pos)));
                        break;
                    }
                    parts.push_back(make_value<StringValue>(text.substr(pos, hit - pos)));
                    pos = hit + sep.size();
                }
            }
            return make_value<LnValue>(parts);
        }));
        globals->define("join", make_value<NativeFnValue>("join", [string_arg](const std::vector<ValuePtr>& args){
            REQUIRE_MIN_ARGS("join", 1);
            GET_LN(args[0], list_val);
            std::string sep = "";
            if (args.size() > 1) sep = string_arg(args[1], "join");
            std::string out;
            for (size_t i = 0; i < list_val->elements.size(); i++) {
                if (i > 0) out += sep;
                out += list_val->elements[i]->toString();
            }
            return make_value<StringValue>(out);
        }));
        globals->define("replace", make_value<NativeFnValue>("replace", [string_arg](const std::vector<ValuePtr>& args){
            REQUIRE_ARGS("replace", 3);
            std::string text = string_arg(args[0], "replace");
            std::string from = string_arg(args[1], "replace");
            std::string to = string_arg(args[2], "replace");
            if (from.empty()) return make_value<StringValue>(text);
            std::string out;
            size_t pos = 0;
            while (true) {
                size_t hit = text.find(from, pos);
                if (hit == std::string::npos) {
                    out += text.substr(pos);
                    break;
                }
                out += text.substr(pos, hit - pos);
                out += to;
                pos = hit + from.size();
            }
            return make_value<StringValue>(out);
        }));
        globals->define("contains", make_value<NativeFnValue>("contains", [number](const std::vector<ValuePtr>& args){
            REQUIRE_ARGS("contains", 2);
            if (auto str_val = dynamic_cast<StringValue*>(args[0].get())) {
                auto sub_val = dynamic_cast<StringValue*>(args[1].get());
                if (!sub_val) throw std::runtime_error("Argument to contains() must be a string.");
                return number(str_val->value.find(sub_val->value) != std::string::npos ? 1 : 0);
            }
            if (auto list_val = dynamic_cast<LnValue*>(args[0].get())) {
                for (const auto& element : list_val->elements) {
                    try { if (element->isEqualTo(*args[1])) return number(1); }
                    catch (...) {}
                }
                return number(0);
            }
            if (auto dict_val = dynamic_cast<DimValue*>(args[0].get())) {
                auto key_val = dynamic_cast<StringValue*>(args[1].get());
                if (!key_val) throw std::runtime_error("Argument to contains() must be a string.");
                return number(dict_val->dict.count(key_val->value) ? 1 : 0);
            }
            return number(0);
        }));
        globals->define("substring", make_value<NativeFnValue>("substring", [number](const std::vector<ValuePtr>& args){
            REQUIRE_MIN_ARGS("substring", 1);
            auto str_val = dynamic_cast<StringValue*>(args[0].get());
            if (!str_val) throw std::runtime_error("Argument to substring() must be a string.");
            long long length = static_cast<long long>(str_val->value.size());
            long long start = 0;
            long long stop = length;
            if (args.size() > 1) { GET_NUM(args[1], start_val); start = start_val->value.toLongLong(); }
            if (args.size() > 2) { GET_NUM(args[2], stop_val); stop = stop_val->value.toLongLong(); }
            if (start < 0) start = 0;
            if (start > length) start = length;
            if (stop < 0) stop = 0;
            if (stop > length) stop = length;
            if (start > stop) { long long swap = start; start = stop; stop = swap; }
            return make_value<StringValue>(
                str_val->value.substr(static_cast<size_t>(start),
                                      static_cast<size_t>(stop - start)));
        }));
        globals->define("char", make_value<NativeFnValue>("char", [number](const std::vector<ValuePtr>& args){
            REQUIRE_ARGS("char", 1); GET_NUM(args[0], code_val);
            long long code = code_val->value.toLongLong();
            std::string out(1, static_cast<char>(code & 0xFF));
            return make_value<StringValue>(out);
        }));
        globals->define("ord", make_value<NativeFnValue>("ord", [number](const std::vector<ValuePtr>& args){
            REQUIRE_ARGS("ord", 1);
            auto str_val = dynamic_cast<StringValue*>(args[0].get());
            if (!str_val) throw std::runtime_error("Argument to ord() must be a string.");
            if (str_val->value.empty()) return number(0);
            return number(static_cast<long long>(
                static_cast<unsigned char>(str_val->value[0])));
        }));

        globals->define("range", make_value<NativeFnValue>("range", [number](const std::vector<ValuePtr>& args){
            REQUIRE_MIN_ARGS("range", 1);
            long long start = 0;
            long long stop = 0;
            long long step = 1;
            if (args.size() == 1) {
                GET_NUM(args[0], stop_val); stop = stop_val->value.toLongLong();
            } else if (args.size() == 2) {
                GET_NUM(args[0], start_val); GET_NUM(args[1], stop_val);
                start = start_val->value.toLongLong();
                stop = stop_val->value.toLongLong();
            } else {
                GET_NUM(args[0], start_val); GET_NUM(args[1], stop_val); GET_NUM(args[2], step_val);
                start = start_val->value.toLongLong();
                stop = stop_val->value.toLongLong();
                step = step_val->value.toLongLong();
            }
            std::vector<ValuePtr> out;
            if (step > 0) {
                for (long long i = start; i < stop; i += step) out.push_back(number(i));
            } else if (step < 0) {
                for (long long i = start; i > stop; i += step) out.push_back(number(i));
            }
            return make_value<LnValue>(out);
        }));
        globals->define("reverse", make_value<NativeFnValue>("reverse", [](const std::vector<ValuePtr>& args){
            REQUIRE_ARGS("reverse", 1); GET_LN(args[0], list_val);
            std::vector<ValuePtr> out(list_val->elements.rbegin(), list_val->elements.rend());
            return make_value<LnValue>(out);
        }));
        globals->define("keys", make_value<NativeFnValue>("keys", [](const std::vector<ValuePtr>& args){
            REQUIRE_ARGS("keys", 1);
            auto dict_val = dynamic_cast<DimValue*>(args[0].get());
            if (!dict_val) throw std::runtime_error("Argument to keys() must be a dictionary.");
            std::vector<ValuePtr> out;
            for (const auto& entry : dict_val->sorted_entries())
                out.push_back(make_value<StringValue>(entry.first));
            return make_value<LnValue>(out);
        }));
        globals->define("values", make_value<NativeFnValue>("values", [](const std::vector<ValuePtr>& args){
            REQUIRE_ARGS("values", 1);
            auto dict_val = dynamic_cast<DimValue*>(args[0].get());
            if (!dict_val) throw std::runtime_error("Argument to values() must be a dictionary.");
            std::vector<ValuePtr> out;
            for (const auto& entry : dict_val->sorted_entries()) out.push_back(entry.second);
            return make_value<LnValue>(out);
        }));
        globals->define("has", make_value<NativeFnValue>("has", [number](const std::vector<ValuePtr>& args){
            REQUIRE_ARGS("has", 2);
            auto dict_val = dynamic_cast<DimValue*>(args[0].get());
            if (!dict_val) throw std::runtime_error("Argument to has() must be a dictionary.");
            auto key_val = dynamic_cast<StringValue*>(args[1].get());
            if (!key_val) throw std::runtime_error("Argument to has() must be a string.");
            return number(dict_val->dict.count(key_val->value) ? 1 : 0);
        }));

        globals->define("type", make_value<NativeFnValue>("type", [text_value](const std::vector<ValuePtr>& args){
            REQUIRE_ARGS("type", 1);
            const ValuePtr& value = args[0];
            if (dynamic_cast<NullValue*>(value.get())) return text_value("null");
            if (dynamic_cast<NumberValue*>(value.get())) return text_value("number");
            if (dynamic_cast<StringValue*>(value.get())) return text_value("string");
            if (dynamic_cast<LnValue*>(value.get())) return text_value("list");
            if (dynamic_cast<DimValue*>(value.get())) return text_value("dict");
            if (dynamic_cast<FunctionValue*>(value.get())) return text_value("function");
            if (dynamic_cast<NativeFnValue*>(value.get())) return text_value("native");
            if (dynamic_cast<Class*>(value.get())) return text_value("class");
            if (dynamic_cast<Instance*>(value.get())) return text_value("instance");
            return text_value("unknown");
        }));
        globals->define("is_null", make_value<NativeFnValue>("is_null", [number](const std::vector<ValuePtr>& args){
            REQUIRE_ARGS("is_null", 1);
            return number(dynamic_cast<NullValue*>(args[0].get()) ? 1 : 0);
        }));
        globals->define("is_number", make_value<NativeFnValue>("is_number", [number](const std::vector<ValuePtr>& args){
            REQUIRE_ARGS("is_number", 1);
            return number(dynamic_cast<NumberValue*>(args[0].get()) ? 1 : 0);
        }));
        globals->define("is_string", make_value<NativeFnValue>("is_string", [number](const std::vector<ValuePtr>& args){
            REQUIRE_ARGS("is_string", 1);
            return number(dynamic_cast<StringValue*>(args[0].get()) ? 1 : 0);
        }));
        globals->define("is_list", make_value<NativeFnValue>("is_list", [number](const std::vector<ValuePtr>& args){
            REQUIRE_ARGS("is_list", 1);
            return number(dynamic_cast<LnValue*>(args[0].get()) ? 1 : 0);
        }));
        globals->define("is_dict", make_value<NativeFnValue>("is_dict", [number](const std::vector<ValuePtr>& args){
            REQUIRE_ARGS("is_dict", 1);
            return number(dynamic_cast<DimValue*>(args[0].get()) ? 1 : 0);
        }));
        globals->define("is_function", make_value<NativeFnValue>("is_function", [number](const std::vector<ValuePtr>& args){
            REQUIRE_ARGS("is_function", 1);
            bool is_fn = dynamic_cast<FunctionValue*>(args[0].get()) ||
                         dynamic_cast<NativeFnValue*>(args[0].get());
            return number(is_fn ? 1 : 0);
        }));
    }

#undef REQUIRE_ARGS
#undef REQUIRE_MIN_ARGS
#undef GET_NUM
#undef GET_LN
}
