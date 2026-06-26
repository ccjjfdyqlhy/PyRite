#pragma once
#include "msgs.hpp"

inline const char* msg(Msg id) {
    switch (id) {
        case Msg::ADD_TYPE:  return "Unsupported operand types for '+'.";
        case Msg::SUB_TYPE:  return "Unsupported operand types for '-'.";
        case Msg::MUL_TYPE:  return "Unsupported operand types for '*'.";
        case Msg::DIV_TYPE:  return "Unsupported operand types for '/'.";
        case Msg::POW_TYPE:  return "Unsupported operand types for '^'.";
        case Msg::MOD_TYPE:  return "Unsupported operand types for '%'.";
        case Msg::CMP_TYPE:  return "Unsupported comparison operand types.";
        case Msg::NOT_SUBSCR: return "Object is not subscriptable.";
        case Msg::NO_ITEM_SET: return "Object does not support item assignment.";
        case Msg::LN_REPEAT_INT: return "ln repetition count must be an integer.";
        case Msg::LN_IDX_NUM: return "ln index must be a number.";
        case Msg::LN_IDX_OOB: return "ln index out of range.";
        case Msg::LN_IDX_INV: return "Invalid ln index.";
        case Msg::HEX_PREFIX: return "Hex string must start with '0x'.";

        case Msg::RUNTIME_PREFIX: return "[Runtime Error] Line ";
        case Msg::UNDEFINED_VAR: return "Undefined variable '{}'.";
        case Msg::UNCAUGHT_EX: return "[Uncaught Exception] ";
        case Msg::STACK_TRACE: return "Stack trace:";
        case Msg::STACK_ENTRY: return "  in {} (line {})";
        case Msg::TIMEOUT: return "Execution timeout ({} ms).";
        case Msg::INVALID_ASSIGN: return "Invalid assignment target.";
        case Msg::STR_TO_NUM: return "Cannot convert string '{}' to number.";
        case Msg::STR_TO_BIN: return "Cannot convert string '{}' to binary. Must be '0x...' format.";
        case Msg::LN_INIT_LN: return "Can only initialize an ln variable with an ln.";
        case Msg::SET_NODE_ERR: return "SetNode should be handled by AssignmentNode.";
        case Msg::NO_PROP: return "Only instances have properties. Cannot get '{}'.";
        case Msg::NO_SET_PROP: return "Only instances can set properties.";
        case Msg::UNDEF_PROP: return "Undefined property '{}'.";
        case Msg::UNDEF_FIELD: return "Cannot set undefined field '{}'.";
        case Msg::FIELD_TYPE: return "Field '{}' type mismatch.";
        case Msg::CALL_ONLY: return "Can only call functions and methods. Got '{}'.";

        case Msg::ARGS_AT_LEAST: return "Expected at least ";
        case Msg::ARGS_EXACT: return "Expected ";
        case Msg::ARGS_AT_MOST: return "Expected at most ";
        case Msg::ARGS_GOT: return " arguments but got ";
        case Msg::ARG_TYPE: return "Arg {} type mismatch (function '{}', param '{}', expected '{}', got '{}').";
        case Msg::SWAP_TWO_VARS: return "swap() requires 2 arguments.";
        case Msg::SWAP_VARS: return "swap() arguments must be variables or ln elements.";
        case Msg::NEW_CLASS: return "new() first argument must be a class.";

        case Msg::NATIVE_ARGS: return "() requires ";
        case Msg::NATIVE_MIN_ARGS: return "() requires at least ";
        case Msg::NATIVE_NUM: return "Argument must be a number.";
        case Msg::NATIVE_LN: return "Argument must be an ln.";
        case Msg::NATIVE_STR: return "Argument must be a string.";
        case Msg::NATIVE_RT: return "rt() requires 1 or 2 arguments.";
        case Msg::NATIVE_MINMAX_EMPTY: return "min/max requires at least one argument.";
        case Msg::NATIVE_MINMAX_LIST: return "Cannot find min/max in empty list.";
        case Msg::NATIVE_MINMAX_CMP: return "min/max arguments must be comparable.";
        case Msg::NATIVE_TIMER: return "Timer function takes no arguments.";
        case Msg::NATIVE_LOG_POS: return "log() argument must be positive.";

        case Msg::PARSE_PREFIX: return "[Parse Error] Line ";
        case Msg::PARSE_UNEXPECTED: return "Unexpected character.";
        case Msg::PARSE_UNTERM_STR: return "Unterminated string.";
        case Msg::PARSE_EXPR: return "Expected expression.";
        case Msg::PARSE_VAR_NAME: return "Expected variable name.";
        case Msg::PARSE_PARAM_TYPE: return "Expected parameter type (dec, str, bin, ln, dim, any).";
        case Msg::PARSE_PARAM_NAME: return "Expected parameter name.";
        case Msg::PARSE_DEFAULT_VAL: return "Default value must be a literal.";
        case Msg::PARSE_DEFAULT_LN: return "Non-empty ln default not supported.";
        case Msg::PARSE_TOO_MANY_PARAMS: return "Cannot have more than 255 parameters.";
        case Msg::PARSE_TOO_MANY_FIELDS: return "Cannot have more than 255 fields.";
        case Msg::PARSE_TOO_MANY_ARGS: return "Cannot have more than 255 arguments.";
        case Msg::PARSE_FUNC_NAME: return "Expected function name.";
        case Msg::PARSE_METHOD_NAME: return "Expected method name.";
        case Msg::PARSE_LPAREN_NAME: return "Expected '(' after name.";
        case Msg::PARSE_RPAREN_PARAMS: return "Expected ')' after parameters.";
        case Msg::PARSE_DO_BODY: return "Expected 'do' before function body.";
        case Msg::PARSE_CLASS_NAME: return "Expected class name.";
        case Msg::PARSE_RPAREN_FIELDS: return "Expected ')' after fields.";
        case Msg::PARSE_CONTAINS: return "Expected 'contains' after class definition.";
        case Msg::PARSE_ENDINS: return "Expected 'endins' after class body.";
        case Msg::PARSE_ONLY_METHODS: return "Only method definitions (fn) allowed in class body.";
        case Msg::PARSE_THEN_IF: return "Expected 'then' after if condition.";
        case Msg::PARSE_ENDIF: return "Expected 'endif' after if block.";
        case Msg::PARSE_DO_WHILE: return "Expected 'do' after while condition.";
        case Msg::PARSE_ENDWHILE: return "Expected 'endwhile' after while block.";
        case Msg::PARSE_THEN_AWAIT: return "Expected 'then' after await condition.";
        case Msg::PARSE_ENDAWAIT: return "Expected 'endawait' after await block.";
        case Msg::PARSE_CATCH_TRY: return "Expected 'catch' after try block.";
        case Msg::PARSE_VAR_CATCH: return "Expected variable name after 'catch'.";
        case Msg::PARSE_ENDTRY: return "Expected 'endtry' after try block.";
        case Msg::PARSE_LPAREN_SAY: return "Expected '(' for 'say'.";
        case Msg::PARSE_RPAREN_EXPR: return "Expected ')' after expression.";
        case Msg::PARSE_LPAREN_ASK: return "Expected '(' for 'ask'.";
        case Msg::PARSE_RPAREN_PROMPT: return "Expected ')' after prompt.";
        case Msg::PARSE_RBRACKET_LN: return "Expected ']' after ln literal.";
        case Msg::PARSE_RBRACKET_IDX: return "Expected ']' after index.";
        case Msg::PARSE_PROP_NAME: return "Expected property name.";
        case Msg::PARSE_STRUCT_NAME: return "Expected struct name.";
        case Msg::PARSE_RPAREN_STRUCT: return "Expected ')' after struct fields.";

        case Msg::SLICE_TOO_LARGE: return "Slice index too large.";
        case Msg::SLICE_MUST_NUM: return "Slice indices must be numbers.";
        case Msg::SLICE_STEP_ZERO: return "Slice step cannot be zero.";
        case Msg::SLICE_UNSUPPORTED: return "This type does not support slicing.";
        case Msg::SLICE_ASSIGN: return "This type does not support slice assignment.";
        case Msg::SLICE_LN_ONLY: return "Can only assign an ln to a slice.";

        case Msg::DIM_KEY_MISS: return "Key '{}' not found in dim.";
        case Msg::DIM_KEY_TYPE: return "Dim subscript must be a string key.";

        case Msg::LOOP_MUST_NUM: return "loop...for count must be a number.";
        case Msg::LOOP_TOO_LARGE: return "loop...for count is too large.";

        case Msg::DIM_INIT_DIM: return "Dim variable must be initialized with a dim literal.";

        case Msg::REPL_WELCOME1: return "PyRite Interpreter ";
        case Msg::REPL_DEBUG: return " [Debug Mode]";
        case Msg::REPL_WELCOME3: return "Type help(), about() or an expression to start.\n";
        case Msg::REPL_HALTED: return "Exiting REPL...";
        case Msg::REPL_NO_CODE: return "No code in buffer to run.";
        case Msg::REPL_TICK: return "run() tick argument must be 0 or 1.";
        case Msg::REPL_LIMIT_LIT: return "run() limit must be a number literal.";
        case Msg::REPL_LIMIT_INV: return "run() limit argument is invalid.";
        case Msg::REPL_TIME: return "Execution time: {} ms.";
        case Msg::ABOUT_HEADER: return "----------------------------------------\n";
        case Msg::ABOUT_LINE1: return " PyRite Language Interpreter ";
        case Msg::ABOUT_LINE2: return "\n (c) 2024-2025. DarkstarXD.\n";
        case Msg::ABOUT_LINE3: return " A surprisingly simple programming language?!\n";
        case Msg::MAIN_USAGE: return "Usage: ";
        case Msg::MAIN_SCRIPT: return " [script.src]";
        case Msg::MAIN_OPEN: return "Error: Cannot open file '{}'.";
        case Msg::REPL_HELP_NOT_FOUND: return "Help not found for function '{}'.";
        case Msg::REPL_HELP_SUGGESTION: return "Type help() to list all available functions.";
        case Msg::MAIN_HELP_HEADER: return "Usage: {} [options] [script.pr|.prc|.prt]\n\nOptions:\n";
        case Msg::MAIN_HELP_VM: return "  --vm                     Execute using the bytecode VM (faster)\n";
        case Msg::MAIN_HELP_AST: return "  --ast                    Execute using the AST interpreter (default)\n";
        case Msg::MAIN_HELP_DUMP: return "  --dump-bytecode          Compile and print bytecode disassembly\n";
        case Msg::MAIN_HELP_EMIT_BC: return "  --emit-bytecode <file>   Compile and save .prc bytecode, then run\n";
        case Msg::MAIN_HELP_EMIT_AST: return "  --emit-ast <file>        Compile and save .prt AST, then run\n";
        case Msg::MAIN_HELP_SAVE_BC: return "  --save-bytecode <file>   Compile and save .prc bytecode only\n";
        case Msg::MAIN_HELP_SAVE_AST: return "  --save-ast <file>        Compile and save .prt AST only\n";
        case Msg::MAIN_HELP_HELP: return "  --help, -h               Show this help message\n";
        case Msg::MAIN_HELP_FORMATS: return "\nFile formats:\n";
        case Msg::MAIN_HELP_PR: return "  .pr    PyRite source script\n";
        case Msg::MAIN_HELP_PRC: return "  .prc   Compiled bytecode (load and run via VM)\n";
        case Msg::MAIN_HELP_PRT: return "  .prt   Serialized AST (load and run via interpreter)\n";
        case Msg::MAIN_LOAD_ERR: return "Error loading {}: {}";
        case Msg::MAIN_COMPILE_ERR: return "Compilation error: {}";
        case Msg::MAIN_OPT_NEED_ARG: return "{} requires a filename argument.";
        case Msg::MAIN_UNKNOWN_OPT: return "Unknown option: {}";
        case Msg::MAIN_HELP_HINT: return "Use --help for usage information.";
        case Msg::MAIN_SAVE_NEED_SRC: return "A source file must be specified when using emit/save options.";
        case Msg::MAIN_SAVE_OK_BC: return "Bytecode saved to {}.";
        case Msg::MAIN_SAVE_OK_AST: return "AST saved to {}.";
        case Msg::MAIN_SAVE_ERR_BC: return "Error saving bytecode: {}";
        case Msg::MAIN_SAVE_ERR_AST: return "Error saving AST: {}";
    }
    return "";
}

inline std::string fmt(Msg id, const std::string& arg) {
    switch (id) {
        case Msg::UNDEFINED_VAR: return std::string("Undefined variable '") + arg + "'.";
        case Msg::STR_TO_NUM: return std::string("Cannot convert string '") + arg + "' to number.";
        case Msg::STR_TO_BIN: return std::string("Cannot convert string '") + arg + "' to binary.";
        case Msg::NO_PROP: return std::string("Only instances have properties. Cannot get '") + arg + "'.";
        case Msg::UNDEF_PROP: return std::string("Undefined property '") + arg + "'.";
        case Msg::UNDEF_FIELD: return std::string("Cannot set undefined field '") + arg + "'.";
        case Msg::CALL_ONLY: return std::string("Can only call functions and methods. Got '") + arg + "'.";
        case Msg::DIM_KEY_MISS: return std::string("Key '") + arg + "' not found in dim.";
        case Msg::MAIN_OPEN: return std::string("Error: Cannot open file '") + arg + "'.";
        case Msg::REPL_HELP_NOT_FOUND: return std::string("Help not found for function '") + arg + "'.";
        case Msg::MAIN_COMPILE_ERR: return std::string("Compilation error: ") + arg;
        case Msg::MAIN_UNKNOWN_OPT: return std::string("Unknown option: ") + arg;
        case Msg::MAIN_OPT_NEED_ARG: return arg + std::string(" requires a filename argument.");
        case Msg::MAIN_SAVE_ERR_BC: return std::string("Error saving bytecode: ") + arg;
        case Msg::MAIN_SAVE_ERR_AST: return std::string("Error saving AST: ") + arg;
        default: return arg;
    }
}

inline std::string fmt_int(Msg id, long long n) {
    std::string ns = std::to_string(n);
    switch (id) {
        case Msg::TIMEOUT: return std::string("Execution timeout (") + ns + " ms).";
        case Msg::ARGS_AT_LEAST: return std::string("Expected at least ") + ns + " arguments but got ";
        case Msg::ARGS_AT_MOST: return std::string("Expected at most ") + ns + " arguments but got ";
        case Msg::ARGS_EXACT: return std::string("Expected ") + ns + " arguments but got ";
        case Msg::NATIVE_ARGS: return std::string(" requires ") + ns + " arguments.";
        case Msg::NATIVE_MIN_ARGS: return std::string(" requires at least ") + ns + " arguments.";
        case Msg::REPL_TIME: return std::string("Execution time: ") + ns + " ms.";
        default: return ns;
    }
}

inline std::string fmt2(Msg id, const std::string& a1, const std::string& a2) {
    if (id == Msg::MAIN_LOAD_ERR) {
        return std::string("Error loading ") + a1 + ": " + a2;
    }
    return a1 + a2;
}

inline std::string fmt3(Msg id, const std::string& a1, const std::string& a2, const std::string& a3) {
    if (id == Msg::ARG_TYPE) {
        return std::string("Arg ") + a1 + " type mismatch (in function '" + a2 + "', param '" + a3 + "').";
    }
    return a1 + a2 + a3;
}
