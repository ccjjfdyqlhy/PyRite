#include <iostream>
#include <fstream>
#include <string>
#include <chrono>
#include <algorithm>
#include <cctype>
#include <map>
#include "Interpreter.hpp"
#include "Compiler.hpp"
#include "VM.hpp"
#include "Serializer.hpp"
#include "help_cn.hpp"
#include "msg_cn.hpp"

#ifndef DEBUG
constexpr bool DEBUG = false;
#endif

std::string VERSION = "v0.31.0";

// --- Helper Functions ---
std::string trim(const std::string& str) {
    const std::string whitespace = " \t\n\r";
    const auto strBegin = str.find_first_not_of(whitespace);
    if (strBegin == std::string::npos) return "";
    const auto strEnd = str.find_last_not_of(whitespace);
    const auto strRange = strEnd - strBegin + 1;
    return str.substr(strBegin, strRange);
}

bool is_simple_identifier(const std::string& s) {
    if (s.empty() || !(isalpha(s[0]) || s[0] == '_')) return false;
    for (char c : s) { if (!isalnum(c) && c != '_') return false; }
    return true;
}

bool starts_with(const std::string& str, const std::string& prefix) {
    return str.size() >= prefix.size() && str.compare(0, prefix.size(), prefix) == 0;
}

bool ends_with(const std::string& str, const std::string& suffix) {
    return str.size() >= suffix.size() && str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0;
}

void run_prc_file(const std::string& filename, Interpreter& interpreter) {
    BytecodeChunkPtr chunk;
    try {
        chunk = load_bytecode(filename);
    } catch (const std::exception& e) {
        std::cerr << fmt2(Msg::MAIN_LOAD_ERR, filename, e.what()) << std::endl;
        exit(1);
    }
    VM vm;
    vm.set_interpreter(&interpreter);
    vm.set_globals(interpreter.globals);
    if (!vm.run(chunk)) {
        std::cerr << vm.error_message << std::endl;
        exit(1);
    }
}

void run_prt_file(const std::string& filename, Interpreter& interpreter) {
    std::vector<AstNodePtr> statements;
    try {
        statements = load_ast(filename);
    } catch (const std::exception& e) {
        std::cerr << fmt2(Msg::MAIN_LOAD_ERR, filename, e.what()) << std::endl;
        exit(1);
    }
    interpreter.interpret(statements);
}

void run_file(const char* filename, Interpreter& interpreter, bool use_vm, bool dump_bytecode) {
    std::string fname(filename);

    // Auto-detect format by extension.
    if (ends_with(fname, ".prc")) {
        run_prc_file(fname, interpreter);
        return;
    }
    if (ends_with(fname, ".prt")) {
        run_prt_file(fname, interpreter);
        return;
    }

    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << fmt(Msg::MAIN_OPEN, filename) << std::endl;
        exit(1);
    }
    std::string source_code((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    file.close();
    Parser parser(source_code);
    auto statements = parser.parse();
        if (parser.has_error()) { exit(1); }

        if (use_vm) {
            // Compile AST -> Bytecode, then execute on the VM.
            Compiler compiler;
            BytecodeChunkPtr chunk = compiler.compile(statements);
            if (compiler.has_error()) {
                std::cerr << fmt(Msg::MAIN_COMPILE_ERR, compiler.get_error()) << std::endl;
                exit(1);
            }
        if (dump_bytecode) {
            VM vm;
            vm.disassemble(chunk);
            return;
        }
        VM vm;
        vm.set_interpreter(&interpreter);
        vm.set_globals(interpreter.globals);
        if (!vm.run(chunk)) {
            std::cerr << vm.error_message << std::endl;
            exit(1);
        }
    } else {
        interpreter.interpret(statements);
    }
}

void run_repl(Interpreter& interpreter) {
    interpreter.repl_buffer.clear();
    int line_number = 1;
    std::vector<std::string> env_stack = {"void"};
    std::cout << msg(Msg::REPL_WELCOME1) << VERSION
              << (DEBUG ? msg(Msg::REPL_DEBUG) : "") << "。\n";
    std::cout << msg(Msg::REPL_WELCOME3);
    std::cout << std::endl;

    while (true) {
        std::string current_env = env_stack.back();
        std::string env_display = "(" + current_env + ")";
        std::string line_num_str = std::to_string(line_number);
        std::stringstream prompt_ss;
        int total_width = 12;
        int env_len = env_display.length();
        int num_len = line_num_str.length();
        int padding = total_width - env_len - num_len;
        if (padding < 1) padding = 1;
        prompt_ss << env_display << std::string(padding, ' ') << line_num_str;
        std::cout << prompt_ss.str() << "| ";

        std::string line_input;
        if (!std::getline(std::cin, line_input)) break;
        std::string trimmed_line = trim(line_input);

        if (trimmed_line == "halt()") break;
        if (trimmed_line == "about()") {
            std::cout << msg(Msg::ABOUT_HEADER)
                      << msg(Msg::ABOUT_LINE1) << VERSION << (DEBUG ? msg(Msg::REPL_DEBUG) : "")
                      << msg(Msg::ABOUT_LINE2)
                      << msg(Msg::ABOUT_LINE3)
                      << msg(Msg::ABOUT_HEADER);
            continue;
        }
        if (trimmed_line == "help()") {
            std::cout << HelpMessagesCN::get_all_functions() << std::endl;
            continue;
        }
        if (starts_with(trimmed_line, "help(") && ends_with(trimmed_line, ")")) {
            size_t arg_start = trimmed_line.find('(') + 1;
            size_t arg_end = trimmed_line.rfind(')');
            std::string args_str = trim(trimmed_line.substr(arg_start, arg_end - arg_start));
            bool found = false;
            if (!args_str.empty()) {
                std::string func_name = args_str;
                if (func_name.length() >= 2 && ((func_name.front() == '"' && func_name.back() == '"') || (func_name.front() == '\'' && func_name.back() == '\''))) {
                    func_name = func_name.substr(1, func_name.length() - 2);
                }
                std::string help_text = HelpMessagesCN::get_help(func_name);
                if (!help_text.empty()) {
                    std::cout << help_text << std::endl;
                    found = true;
                } else {
                    std::cout << fmt(Msg::REPL_HELP_NOT_FOUND, func_name) << std::endl;
                }
            }
            if (!found) { std::cout << msg(Msg::REPL_HELP_SUGGESTION) << std::endl; }
            continue;
        }

        if (is_simple_identifier(trimmed_line)) {
            try {
                ValuePtr val = interpreter.environment->get(trimmed_line);
                std::cout << val->repr() << std::endl;
                continue;
            } catch (const RuntimeError&) {}
        }

        if (starts_with(trimmed_line, "run(") && ends_with(trimmed_line, ")")) {
            if (interpreter.repl_buffer.empty()) {
                std::cout << msg(Msg::REPL_NO_CODE) << std::endl;
                continue;
            }
            bool tick_enabled = false;
            long long time_limit = 0;
            // Simple parsing for run(tick=1, limit=1000)
            size_t open_paren = trimmed_line.find('(');
            size_t close_paren = trimmed_line.rfind(')');
            if (open_paren != std::string::npos && close_paren != std::string::npos) {
                std::string args = trim(trimmed_line.substr(open_paren + 1, close_paren - open_paren - 1));
                if (!args.empty()) {
                    size_t pos = 0;
                    while (pos < args.length()) {
                        size_t eq_pos = args.find('=', pos);
                        if (eq_pos == std::string::npos) break;
                        std::string key = trim(args.substr(pos, eq_pos - pos));
                        size_t comma_pos = args.find(',', eq_pos + 1);
                        std::string val_str = trim(args.substr(eq_pos + 1, comma_pos == std::string::npos ? std::string::npos : comma_pos - eq_pos - 1));
                        if (key == "tick" && (val_str == "1" || val_str == "true")) tick_enabled = true;
                        if (key == "limit") {
                            try { time_limit = std::stoll(val_str); }
                            catch (...) { std::cerr << msg(Msg::REPL_LIMIT_INV) << std::endl; }
                        }
                        pos = (comma_pos == std::string::npos) ? args.length() : comma_pos + 1;
                    }
                }
            }
            Parser parser(interpreter.repl_buffer);
            auto statements = parser.parse();
            if (!parser.has_error()) {
                auto start_time = std::chrono::high_resolution_clock::now();
                interpreter.start_time = start_time;
                interpreter.time_limit_ms = time_limit;
                interpreter.interpret(statements);
                if (tick_enabled) {
                    auto end_time = std::chrono::high_resolution_clock::now();
                    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
                    std::cout << fmt_int(Msg::REPL_TIME, duration) << std::endl;
                }
            }
            interpreter.repl_buffer.clear();
            line_number = 1;
            env_stack = {"void"};
            std::cout << std::endl;
            continue;
        }

        std::string first_word;
        size_t word_end = trimmed_line.find_first_of(" \t\n\r(");
        first_word = (word_end == std::string::npos) ? trimmed_line : trimmed_line.substr(0, word_end);

        if (first_word == "fn" && trimmed_line.find("->") != std::string::npos) {
            // Arrow function (single-line, no end keyword needed).
        } else if (first_word == "if" || first_word == "while" || first_word == "fn" || first_word == "await" || first_word == "try" || first_word == "ins" || first_word == "loop") {
            env_stack.push_back(first_word);
        } else if (first_word == "endif" && env_stack.back() == "if") {
            if (env_stack.size() > 1) env_stack.pop_back();
        } else if (first_word == "elif" || first_word == "else") {
            // elif/else don't change nesting in REPL
        } else if (first_word == "endwhile" && env_stack.back() == "while") {
            if (env_stack.size() > 1) env_stack.pop_back();
        } else if (first_word == "endfn" && env_stack.back() == "fn") {
            if (env_stack.size() > 1) env_stack.pop_back();
        } else if (first_word == "endawait" && env_stack.back() == "await") {
            if (env_stack.size() > 1) env_stack.pop_back();
        } else if (first_word == "endtry" && env_stack.back() == "try") {
            if (env_stack.size() > 1) env_stack.pop_back();
        } else if (first_word == "endins" && env_stack.back() == "ins") {
            if (env_stack.size() > 1) env_stack.pop_back();
        } else if ((first_word == "endloop" || first_word == "until" || first_word == "for") && env_stack.back() == "loop") {
            if (env_stack.size() > 1) env_stack.pop_back();
        } else if (first_word == "end" && env_stack.size() > 1) {
            env_stack.pop_back();
        }

        if (!trimmed_line.empty() && trimmed_line.front() == '$') {
            std::string code_to_run;
            bool is_temp_exec_only = false;
            if (trimmed_line.length() > 1 && trimmed_line[1] == '#') {
                code_to_run = trimmed_line.substr(2);
                is_temp_exec_only = true;
            } else {
                code_to_run = trimmed_line.substr(1);
            }
            Parser p(code_to_run);
            auto stmts = p.parse();
            if (!p.has_error()) interpreter.interpret(stmts);
            if (is_temp_exec_only) {
                interpreter.repl_buffer += "#" + code_to_run + "#\n";
            } else {
                interpreter.repl_buffer += line_input + "\n";
            }
            line_number++;
            continue;
        }

        interpreter.repl_buffer += line_input + "\n";
        line_number++;
    }
    std::cout << msg(Msg::REPL_HALTED) << std::endl;
}

int main(int argc, char* argv[]) {
    std::ios_base::sync_with_stdio(false);
    Interpreter interpreter;
    std::string executable_path = argv[0];
    size_t last_slash_pos = executable_path.find_last_of("/\\");
    std::string executable_dir = ".";
    if (std::string::npos != last_slash_pos) {
        executable_dir = executable_path.substr(0, last_slash_pos);
    }
    interpreter.base_path = executable_dir;

    bool use_vm = false;
    bool dump_bytecode = false;
    bool emit_only = false;
    std::string script_file;
    std::string emit_bytecode_path;
    std::string emit_ast_path;

    // Parse command-line arguments.
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--vm") {
            use_vm = true;
        } else if (arg == "--dump-bytecode") {
            use_vm = true;
            dump_bytecode = true;
        } else if (arg == "--ast") {
            use_vm = false;
        } else if (arg == "--emit-bytecode" || arg == "--save-bytecode") {
            if (++i >= argc) {
                std::cerr << fmt(Msg::MAIN_OPT_NEED_ARG, arg) << std::endl;
                return 1;
            }
            emit_bytecode_path = argv[i];
            if (arg == "--save-bytecode") emit_only = true;
        } else if (arg == "--emit-ast" || arg == "--save-ast") {
            if (++i >= argc) {
                std::cerr << fmt(Msg::MAIN_OPT_NEED_ARG, arg) << std::endl;
                return 1;
            }
            emit_ast_path = argv[i];
            if (arg == "--save-ast") emit_only = true;
        } else if (arg == "--help" || arg == "-h") {
            std::cout << fmt(Msg::MAIN_HELP_HEADER, argv[0])
                      << msg(Msg::MAIN_HELP_VM)
                      << msg(Msg::MAIN_HELP_AST)
                      << msg(Msg::MAIN_HELP_DUMP)
                      << msg(Msg::MAIN_HELP_EMIT_BC)
                      << msg(Msg::MAIN_HELP_EMIT_AST)
                      << msg(Msg::MAIN_HELP_SAVE_BC)
                      << msg(Msg::MAIN_HELP_SAVE_AST)
                      << msg(Msg::MAIN_HELP_HELP)
                      << msg(Msg::MAIN_HELP_FORMATS)
                      << msg(Msg::MAIN_HELP_PR)
                      << msg(Msg::MAIN_HELP_PRC)
                      << msg(Msg::MAIN_HELP_PRT);
            return 0;
        } else if (arg[0] == '-') {
            std::cerr << fmt(Msg::MAIN_UNKNOWN_OPT, arg) << "\n"
                      << msg(Msg::MAIN_HELP_HINT) << std::endl;
            return 1;
        } else {
            if (!script_file.empty()) {
                std::cerr << msg(Msg::MAIN_USAGE) << argv[0] << msg(Msg::MAIN_SCRIPT) << std::endl;
                return 1;
            }
            script_file = arg;
        }
    }

    // If we have source output paths, load source, parse, emit, optionally run.
    if (!emit_bytecode_path.empty() || !emit_ast_path.empty()) {
        if (script_file.empty()) {
            std::cerr << msg(Msg::MAIN_SAVE_NEED_SRC) << std::endl;
            return 1;
        }
        std::ifstream file(script_file);
        if (!file.is_open()) {
            std::cerr << fmt(Msg::MAIN_OPEN, script_file) << std::endl;
            return 1;
        }
        std::string source_code((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        file.close();
        Parser parser(source_code);
        auto statements = parser.parse();
        if (parser.has_error()) { return 1; }

        if (!emit_bytecode_path.empty()) {
            Compiler compiler;
            BytecodeChunkPtr chunk = compiler.compile(statements);
            if (compiler.has_error()) {
                std::cerr << fmt(Msg::MAIN_COMPILE_ERR, compiler.get_error()) << std::endl;
                return 1;
            }
            try {
                save_bytecode(emit_bytecode_path, chunk);
                std::cout << fmt(Msg::MAIN_SAVE_OK_BC, emit_bytecode_path) << std::endl;
            } catch (const std::exception& e) {
                std::cerr << fmt(Msg::MAIN_SAVE_ERR_BC, e.what()) << std::endl;
                return 1;
            }
        }

        if (!emit_ast_path.empty()) {
            try {
                save_ast(emit_ast_path, statements);
                std::cout << fmt(Msg::MAIN_SAVE_OK_AST, emit_ast_path) << std::endl;
            } catch (const std::exception& e) {
                std::cerr << fmt(Msg::MAIN_SAVE_ERR_AST, e.what()) << std::endl;
                return 1;
            }
        }

        if (emit_only) return 0;
        // Fall through to run the source.
    }

    if (!script_file.empty()) {
        run_file(script_file.c_str(), interpreter, use_vm, dump_bytecode);
    } else {
        run_repl(interpreter);
    }
    return 0;
}
