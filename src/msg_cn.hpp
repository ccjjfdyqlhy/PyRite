#pragma once
#include "msgs.hpp"

inline const char* msg(Msg id) {
    switch (id) {
        case Msg::ADD_TYPE:  return "不支持的 '+' 操作数类型。";
        case Msg::SUB_TYPE:  return "不支持的 '-' 操作数类型。";
        case Msg::MUL_TYPE:  return "不支持的 '*' 操作数类型。";
        case Msg::DIV_TYPE:  return "不支持的 '/' 操作数类型。";
        case Msg::POW_TYPE:  return "不支持的 '^' 操作数类型。";
        case Msg::MOD_TYPE:  return "不支持的 '%' 操作数类型。";
        case Msg::CMP_TYPE:  return "不支持的比较操作数类型。";
        case Msg::NOT_SUBSCR: return "对象不可下标访问。";
        case Msg::NO_ITEM_SET: return "对象不支持元素赋值。";
        case Msg::LN_REPEAT_INT: return "ln 重复次数必须为整数。";
        case Msg::LN_IDX_NUM: return "ln 索引必须是数字。";
        case Msg::LN_IDX_OOB: return "ln 索引超出范围。";
        case Msg::LN_IDX_INV: return "无效的 ln 索引。";
        case Msg::HEX_PREFIX: return "十六进制字符串必须以 '0x' 开头。";

        case Msg::RUNTIME_PREFIX: return "[运行时错误] 第 ";
        case Msg::UNDEFINED_VAR: return "未定义的变量 '{}'。";
        case Msg::UNCAUGHT_EX: return "[未捕获的异常] ";
        case Msg::STACK_TRACE: return "堆栈跟踪:";
        case Msg::STACK_ENTRY: return "  在 {} (第 {} 行)";
        case Msg::TIMEOUT: return "执行超时 ({} 毫秒)。";
        case Msg::INVALID_ASSIGN: return "无效的赋值目标。";
        case Msg::STR_TO_NUM: return "无法将字符串 '{}' 转换为数字。";
        case Msg::STR_TO_BIN: return "无法将字符串 '{}' 转换为二进制对象。必须为 '0x...' 格式。";
        case Msg::LN_INIT_LN: return "只能用 ln 初始化 ln 变量。";
        case Msg::SET_NODE_ERR: return "SetNode 应通过 AssignmentNode 处理。";
        case Msg::NO_PROP: return "只有实例才能拥有属性。无法获取 '{}'。";
        case Msg::NO_SET_PROP: return "只有实例才能设置属性。";
        case Msg::UNDEF_PROP: return "未定义的属性 '{}'。";
        case Msg::UNDEF_FIELD: return "无法设置未定义的字段 '{}'。";
        case Msg::FIELD_TYPE: return "字段 '{}' 类型不匹配。";
        case Msg::CALL_ONLY: return "只能调用函数或方法。被调用者是 '{}'。";

        case Msg::ARGS_AT_LEAST: return "至少需要 ";
        case Msg::ARGS_EXACT: return "需要 ";
        case Msg::ARGS_AT_MOST: return "最多接受 ";
        case Msg::ARGS_GOT: return " 个参数, 但收到了 ";
        case Msg::ARG_TYPE: return "参数 {} 类型不匹配 (在函数 '{}', 参数 '{}', 期望 '{}', 得到 '{}')。";
        case Msg::SWAP_TWO_VARS: return "swap() 需要 2 个变量作为参数。";
        case Msg::SWAP_VARS: return "swap() 的参数必须是变量或 ln 元素。";
        case Msg::NEW_CLASS: return "new() 的第一个参数必须是类。";

        case Msg::NATIVE_ARGS: return "() 需要 ";
        case Msg::NATIVE_MIN_ARGS: return "() 至少需要 ";
        case Msg::NATIVE_NUM: return "参数必须是数字。";
        case Msg::NATIVE_LN: return "参数必须是 ln。";
        case Msg::NATIVE_STR: return "参数必须是字符串。";
        case Msg::NATIVE_RT: return "rt() 需要 1 或 2 个参数。";
        case Msg::NATIVE_MINMAX_EMPTY: return "min/max 至少需要一个参数。";
        case Msg::NATIVE_MINMAX_LIST: return "无法在空列表中找 min/max。";
        case Msg::NATIVE_MINMAX_CMP: return "min/max 的参数必须是可比较的类型。";
        case Msg::NATIVE_TIMER: return "计时器函数不接受参数。";
        case Msg::NATIVE_LOG_POS: return "log() 的参数必须是正数。";

        case Msg::PARSE_PREFIX: return "[解析错误] 在 行 ";
        case Msg::PARSE_UNEXPECTED: return "意外的字符。";
        case Msg::PARSE_UNTERM_STR: return "未终止的字符串。";
        case Msg::PARSE_EXPR: return "缺少表达式。";
        case Msg::PARSE_VAR_NAME: return "缺少变量名。";
        case Msg::PARSE_PARAM_TYPE: return "参数缺少类型关键字 (dec, str, bin, ln, dim, any)。";
        case Msg::PARSE_PARAM_NAME: return "缺少参数名。";
        case Msg::PARSE_DEFAULT_VAL: return "默认值必须是字面量。";
        case Msg::PARSE_DEFAULT_LN: return "不支持非空 ln 作为默认值。";
        case Msg::PARSE_TOO_MANY_PARAMS: return "参数不能超过 255 个。";
        case Msg::PARSE_TOO_MANY_FIELDS: return "字段不能超过 255 个。";
        case Msg::PARSE_TOO_MANY_ARGS: return "参数不能超过 255 个。";
        case Msg::PARSE_FUNC_NAME: return "缺少函数名。";
        case Msg::PARSE_METHOD_NAME: return "缺少方法名。";
        case Msg::PARSE_LPAREN_NAME: return "名称后缺少 '('。";
        case Msg::PARSE_RPAREN_PARAMS: return "参数后缺少 ')'。";
        case Msg::PARSE_DO_BODY: return "函数体前缺少 'do'。";
        case Msg::PARSE_CLASS_NAME: return "缺少类名。";
        case Msg::PARSE_RPAREN_FIELDS: return "字段后缺少 ')'。";
        case Msg::PARSE_CONTAINS: return "类定义后缺少 'contains'。";
        case Msg::PARSE_ENDINS: return "类体后缺少 'endins'。";
        case Msg::PARSE_ONLY_METHODS: return "类体中只允许方法定义 (fn)。";
        case Msg::PARSE_THEN_IF: return "if 条件后缺少 'then'。";
        case Msg::PARSE_ENDIF: return "if 块后缺少 'endif'。";
        case Msg::PARSE_DO_WHILE: return "while 条件后缺少 'do'。";
        case Msg::PARSE_ENDWHILE: return "while 块后缺少 'endwhile'。";
        case Msg::PARSE_THEN_AWAIT: return "await 条件后缺少 'then'。";
        case Msg::PARSE_ENDAWAIT: return "await 块后缺少 'endawait'。";
        case Msg::PARSE_CATCH_TRY: return "'try' 块后缺少 'catch'。";
        case Msg::PARSE_VAR_CATCH: return "'catch' 后缺少变量名。";
        case Msg::PARSE_ENDTRY: return "try 块后缺少 'endtry'。";
        case Msg::PARSE_LPAREN_SAY: return "'say' 需要 '('。";
        case Msg::PARSE_RPAREN_EXPR: return "表达式后缺少 ')'。";
        case Msg::PARSE_LPAREN_ASK: return "'ask' 需要 '('。";
        case Msg::PARSE_RPAREN_PROMPT: return "提示后缺少 ')'。";
        case Msg::PARSE_RBRACKET_LN: return "ln 字面量后缺少 ']'。";
        case Msg::PARSE_RBRACKET_IDX: return "下标后缺少 ']'。";
        case Msg::PARSE_PROP_NAME: return "缺少属性名。";
        case Msg::PARSE_STRUCT_NAME: return "缺少结构体名。";
        case Msg::PARSE_RPAREN_STRUCT: return "结构体字段后缺少 ')'。";

        case Msg::SLICE_TOO_LARGE: return "切片索引过大。";
        case Msg::SLICE_MUST_NUM: return "切片索引必须是数字。";
        case Msg::SLICE_STEP_ZERO: return "切片步长不能为 0。";
        case Msg::SLICE_UNSUPPORTED: return "该类型不支持切片。";
        case Msg::SLICE_ASSIGN: return "该类型不支持切片赋值。";
        case Msg::SLICE_LN_ONLY: return "切片只能赋值为 ln。";

        case Msg::DIM_KEY_MISS: return "键 '{}' 不在 dim 中。";
        case Msg::DIM_KEY_TYPE: return "dim 下标必须是字符串键。";

        case Msg::LOOP_MUST_NUM: return "loop...for 的次数必须是数字。";
        case Msg::LOOP_TOO_LARGE: return "loop...for 的次数过大。";

        case Msg::DIM_INIT_DIM: return "dim 变量必须用 dim 字面量初始化。";

        case Msg::REPL_WELCOME1: return "PyRite 解释器 ";
        case Msg::REPL_DEBUG: return " [调试模式]";
        case Msg::REPL_WELCOME3: return "输入 help()、about() 或表达式以开始。\n";
        case Msg::REPL_HALTED: return "退出 REPL 模式...";
        case Msg::REPL_NO_CODE: return "缓冲区中没有可执行的代码。";
        case Msg::REPL_TICK: return "run() 的 tick 参数必须是 0 或 1。";
        case Msg::REPL_LIMIT_LIT: return "run() 的 limit 参数必须是数字。";
        case Msg::REPL_LIMIT_INV: return "run() 的 limit 参数无效。";
        case Msg::REPL_TIME: return "代码执行时间: {} 毫秒。";
        case Msg::ABOUT_HEADER: return "----------------------------------------\n";
        case Msg::ABOUT_LINE1: return " PyRite 语言解释器 ";
        case Msg::ABOUT_LINE2: return "\n (c) 2024-2025. DarkstarXD.\n";
        case Msg::ABOUT_LINE3: return " 一种简单到神奇的编程语言?!\n";
        case Msg::MAIN_USAGE: return "用法: ";
        case Msg::MAIN_SCRIPT: return " [脚本.src]";
        case Msg::MAIN_OPEN: return "错误: 无法打开文件 '{}'。";
        case Msg::REPL_HELP_NOT_FOUND: return "未找到函数 '{}' 的帮助信息。";
        case Msg::REPL_HELP_SUGGESTION: return "输入 help() 查看所有可用函数。";
        case Msg::MAIN_HELP_HEADER: return "用法: {} [选项] [script.pr|.prc|.prt]\n\n选项:\n";
        case Msg::MAIN_HELP_VM: return "  --vm                    使用字节码 VM 执行（更快）\n";
        case Msg::MAIN_HELP_AST: return "  --ast                   使用 AST 解释器执行（默认）\n";
        case Msg::MAIN_HELP_DUMP: return "  --dump-bytecode         编译并打印字节码反汇编\n";
        case Msg::MAIN_HELP_EMIT_BC: return "  --emit-bytecode <file>  编译并保存 .prc 字节码，然后执行\n";
        case Msg::MAIN_HELP_EMIT_AST: return "  --emit-ast <file>       编译并保存 .prt AST，然后执行\n";
        case Msg::MAIN_HELP_SAVE_BC: return "  --save-bytecode <file>  编译并保存 .prc 字节码（不执行）\n";
        case Msg::MAIN_HELP_SAVE_AST: return "  --save-ast <file>       编译并保存 .prt AST（不执行）\n";
        case Msg::MAIN_HELP_HELP: return "  --help, -h              显示此帮助信息\n";
        case Msg::MAIN_HELP_FORMATS: return "\n文件格式:\n";
        case Msg::MAIN_HELP_PR: return "  .pr    PyRite 源脚本\n";
        case Msg::MAIN_HELP_PRC: return "  .prc   编译后的字节码（通过 VM 加载执行）\n";
        case Msg::MAIN_HELP_PRT: return "  .prt   序列化 AST（通过解释器加载执行）\n";
        case Msg::MAIN_LOAD_ERR: return "加载 {} 时出错: {}";
        case Msg::MAIN_COMPILE_ERR: return "编译错误: {}";
        case Msg::MAIN_OPT_NEED_ARG: return "{} 需要一个文件名参数。";
        case Msg::MAIN_UNKNOWN_OPT: return "未知选项: {}";
        case Msg::MAIN_HELP_HINT: return "使用 --help 查看帮助信息。";
        case Msg::MAIN_SAVE_NEED_SRC: return "使用 emit/save 选项时必须指定源文件。";
        case Msg::MAIN_SAVE_OK_BC: return "字节码已保存到 {}。";
        case Msg::MAIN_SAVE_OK_AST: return "AST 已保存到 {}。";
        case Msg::MAIN_SAVE_ERR_BC: return "保存字节码时出错: {}";
        case Msg::MAIN_SAVE_ERR_AST: return "保存 AST 时出错: {}";
    }
    return "";
}

inline std::string fmt(Msg id, const std::string& arg) {
    switch (id) {
        case Msg::UNDEFINED_VAR: return std::string("未定义的变量 '") + arg + "'。";
        case Msg::STR_TO_NUM: return std::string("无法将字符串 '") + arg + "' 转换为数字。";
        case Msg::STR_TO_BIN: return std::string("无法将字符串 '") + arg + "' 转换为二进制。";
        case Msg::NO_PROP: return std::string("只有实例才能拥有属性。无法获取 '") + arg + "'。";
        case Msg::UNDEF_PROP: return std::string("未定义的属性 '") + arg + "'。";
        case Msg::UNDEF_FIELD: return std::string("无法设置未定义的字段 '") + arg + "'。";
        case Msg::CALL_ONLY: return std::string("只能调用函数或方法。被调用者是 '") + arg + "'。";
        case Msg::DIM_KEY_MISS: return std::string("键 '") + arg + "' 不在 dim 中。";
        case Msg::MAIN_OPEN: return std::string("错误: 无法打开文件 '") + arg + "'。";
        case Msg::REPL_HELP_NOT_FOUND: return std::string("未找到函数 '") + arg + "' 的帮助信息。";
        case Msg::MAIN_COMPILE_ERR: return std::string("编译错误: ") + arg;
        case Msg::MAIN_UNKNOWN_OPT: return std::string("未知选项: ") + arg;
        case Msg::MAIN_OPT_NEED_ARG: return arg + std::string(" 需要一个文件名参数。");
        case Msg::MAIN_HELP_HEADER: return std::string("用法: ") + arg + std::string(" [选项] [script.pr|.prc|.prt]\n\n选项:\n");
        case Msg::MAIN_SAVE_ERR_BC: return std::string("保存字节码时出错: ") + arg;
        case Msg::MAIN_SAVE_ERR_AST: return std::string("保存 AST 时出错: ") + arg;
        default: return arg;
    }
}

inline std::string fmt_int(Msg id, long long n) {
    std::string ns = std::to_string(n);
    switch (id) {
        case Msg::TIMEOUT: return std::string("执行超时 (") + ns + " 毫秒)。";
        case Msg::ARGS_AT_LEAST: return std::string("至少需要 ") + ns + " 个参数, 但收到了 ";
        case Msg::ARGS_AT_MOST: return std::string("最多接受 ") + ns + " 个参数, 但收到了 ";
        case Msg::ARGS_EXACT: return std::string("需要 ") + ns + " 个参数, 但收到了 ";
        case Msg::NATIVE_ARGS: return std::string("需要 ") + ns + " 个参数。";
        case Msg::NATIVE_MIN_ARGS: return std::string("至少需要 ") + ns + " 个参数。";
        case Msg::REPL_TIME: return std::string("代码执行时间: ") + ns + " 毫秒。";
        default: return ns;
    }
}

inline std::string fmt2(Msg id, const std::string& a1, const std::string& a2) {
    if (id == Msg::MAIN_LOAD_ERR) {
        return std::string("加载 ") + a1 + " 时出错: " + a2;
    }
    return a1 + a2;
}

inline std::string fmt3(Msg id, const std::string& a1, const std::string& a2, const std::string& a3) {
    if (id == Msg::ARG_TYPE) {
        return std::string("参数 ") + a1 + " 类型不匹配 (在函数 '" + a2 + "', 参数 '" + a3 + "')。";
    }
    return a1 + a2 + a3;
}
