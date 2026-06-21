#pragma once
#include <string>
#include <vector>
#include <memory>
#include <map>
#include <set>
#include <chrono>
#include "Environment.hpp"
#include "Ast.hpp"
#include "Parser.hpp"

class RuntimeError : public std::runtime_error {
public:
    int line;
    RuntimeError(int l, const std::string& msg) : std::runtime_error(msg), line(l) {}
};

class ReturnValueException : public std::runtime_error {
public:
    ValuePtr value;
    ReturnValueException(ValuePtr v) : std::runtime_error(""), value(v) {}
};

class PyRiteRaiseException : public std::runtime_error {
public:
    ValuePtr value;
    PyRiteRaiseException(ValuePtr v) : std::runtime_error(""), value(v) {}
};

class BreakException : public std::runtime_error {
public:
    BreakException() : std::runtime_error("") {}
};
class ContinueException : public std::runtime_error {
public:
    ContinueException() : std::runtime_error("") {}
};

class Interpreter {
public:
    Interpreter();
    std::string base_path;
    void interpret(const std::vector<AstNodePtr>& statements);
    void execute(const AstNodePtr& stmt);
    ValuePtr evaluate(const AstNodePtr& expr);
    void execute_block(const std::vector<AstNodePtr>& statements, std::shared_ptr<Environment> block_env);
    void check_timeout(int line);
    void assignToLValue(AstNodePtr target, ValuePtr val, int line);
    ValuePtr load_module(class ModuleProxy* proxy);
    std::string resolve_module_path(const std::string& path);

    std::chrono::time_point<std::chrono::high_resolution_clock> start_time;
    long long time_limit_ms;
    struct CallInfo { std::string function_name; int call_site_line; };
    std::vector<CallInfo> call_stack;
    std::shared_ptr<Environment> globals;
    std::shared_ptr<Environment> environment;
    std::string repl_buffer;

private:
    void define_native_functions();
    void print_stack_trace();
    std::set<std::string> loading_modules;
};

// All AST accept() implementations are in Interpreter.cpp
