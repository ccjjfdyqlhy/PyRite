# PyRite — 轻量级脚本语言，内置高精度运算

[**English**](README.md) | **中文**

PyRite 是一门解释型脚本语言，语法融合 Python、C++ 和 BASIC 的特点。其核心特色是**透明的高精度算术**：所有数字默认使用 `BigNumber`，无需 import、无需 `Decimal()`、没有精度上限。

版本 **v0.31.0** — 混合 AST/字节码解释器，支持 `.prc`/`.prt` 序列化。

---

## 语法设计理念

### 自然语言块边界

PyRite 使用英文关键词作为块边界，而非 `{}` 或缩进，使语言对初学者友好且不受格式化影响。

```
if score >= 90 then
  say("优秀")
elif score >= 60 then
  say("及格")
else
  say("不及格")
end
```

所有块可以用通用 `end` 关闭，也可用具体关键词（`endif`、`endfn`、`endwhile`、`endtry`、`endins`）以提高可读性。

### 显式类型关键字，自动转换

变量声明时需写上类型关键字（`dec`、`str`、`bin`、`ln`、`dim`、`any`），但赋值时值会自动转换：

```
dec x = "123"       // 字符串 → 数字（自动转换）
str y = 456         // 数字 → 字符串（自动转换）
any z = some_fn()   // 接受任意类型
```

介于静态类型和动态类型之间：声明表明意图，运行时处理转换。

### 透明的高精度运算

所有数字都是任意精度的。无需特殊语法，无需额外库：

```
fn fact(dec n) do
  if n == 0 then return 1 else return n * fact(n - 1) endif
end
say(fact(100))
// 93326215443944152681699238856266700490715968264381621468592963895217599993229915608941463976156518286253697920827223758251185210916864000000000000000000000000
```

### 头等公民 `swap()` 和 `using-as`

```
swap(a, b)                  // 交换两个变量
using very_long_name as v   // 创建短别名
```

### Lambda 和箭头函数

```
fn double(dec x) -> x * 2          // 命名简写
dec f = fn(dec n) -> n * n         // 匿名 lambda
apply(5, fn(dec n) -> n * 3)       // 高阶函数
```

---

## 功能概览

| 类别 | 功能 |
|----------|------|
| **类型** | `dec`（高精度数）、`str`、`bin`（十六进制）、`ln`（列表）、`dim`（字典）、`nul`、`any` |
| **运算符** | `+` `-` `*` `/` `^` `%`、比较、`not` `and` `or`、`+=` `-=` 等复合赋值 |
| **控制流** | `if`/`elif`/`else`、`while`/`fin`、`loop`/`for`/`until`、`for-in`、`break`/`continue` |
| **函数** | `fn` 定义、`->` 简写、lambda、闭包、默认参数、类型提示 |
| **面向对象** | `struct`（纯数据）、`ins`（含方法的类）、`new()`、`this` |
| **错误处理** | `try`/`catch`/`fin`、`raise` |
| **模块** | `require`（懒加载）、单文件和目录模块、`expose`、`_on_load()` 钩子 |
| **I/O** | `say()`、`ask()` |
| **异步** | `await`/`then`（轮询式） |

---

## 快速开始

### 构建

```bash
make release    # 优化构建  (-O2 -DNDEBUG)
make debug      # 调试构建  (-O0 -g -DDEBUG)
make clean      # 清除构建产物
```

需要 C++11 编译器（g++ 或 clang++）。

### 运行

```bash
./PyRite script.pr          # 运行源文件（AST 解释器）
./PyRite --vm script.pr     # 通过字节码 VM 运行
./PyRite script.prc         # 加载并运行已编译的字节码
./PyRite script.prt         # 加载并运行序列化的 AST
./PyRite                    # 启动 REPL
```

### 导出

```bash
./PyRite --emit-bytecode out.prc script.pr   # 编译 + 保存 .prc + 运行
./PyRite --save-bytecode out.prc script.pr   # 仅编译保存
./PyRite --emit-ast out.prt script.pr        # 保存 .prt + 运行
./PyRite --save-ast out.prt script.pr        # 仅保存 .prt
```

---

## REPL 命令

| 命令 | 说明 |
|---------|------|
| `run()` | 执行缓冲区代码 |
| `run(tick=1)` | 执行并显示耗时 |
| `run(limit=1000)` | 设置超时（毫秒） |
| `halt()` | 退出 |
| `about()` | 版本信息 |
| `help()` | 列出内置函数 |
| `help("abs")` | 查看特定函数帮助 |
| `$ code` | 执行并加入缓冲区 |
| `$# code` | 执行但不加入缓冲区 |

---

## 实现架构

```
源文件 (.pr)
    │
    ▼
┌────────────┐
│  Tokenizer │  词法分析：字符流 → Token 流
└─────┬──────┘       (src/Tokenizer.hpp, src/Tokenizer.cpp)
      │
      ▼
┌────────────┐
│   Parser   │  递归下降：Token → AST（35 种节点类型）
└─────┬──────┘       (src/Parser.hpp, src/Parser.cpp)
      │
      ▼
┌────────────┐     ┌──────────────────┐     ┌──────────────┐
│  Compiler  │────▶│ BytecodeChunk    │────▶│     VM       │
│ AST → 字节码│     │ (CompiledFunction)│     │ 基于栈的     │
└────────────┘     └──────────────────┘     │ 字节码 VM    │
      │                                      └──────┬───────┘
      │                                             │
      ▼                                             ▼
┌────────────┐                             ┌────────────────┐
│ Interpreter│  混合桥接：                   │  Serializer    │
│ AST 遍历   │  VM 通过 OP_EVAL_AST         │ .prc / .prt    │
│            │  回退到 Interpreter          │ 二进制 I/O     │
└────────────┘                             └────────────────┘
      │
      ▼
┌────────────┐
│ Environment│  词法作用域、闭包、OOP（Class/Instance）
└────────────┘       (src/Environment.hpp, src/Environment.cpp)
      │
      ▼
┌────────────┐
│   Value    │  运行时类型系统：Null、Number（BigNumber）、
│            │  String、Binary、Ln（列表）、Dim（字典）、
│            │  Function、NativeFn、BoundMethod、Exception、
│            │  ModuleProxy、Closure
└────────────┘       (src/Value.hpp, src/Value.cpp, src/BigNumber.hpp)
```

### 模块详情

| 模块 | 文件 | 职责 |
|--------|-------|------|
| **BigNumber** | `BigNumber.hpp` | 任意精度十进制运算（纯 C++ 实现，无 GMP 依赖） |
| **OpCode** | `OpCode.hpp` | 字节码 VM 的约 50 条指令操作码 |
| **Tokenizer** | `Tokenizer.hpp`、`Tokenizer.cpp` | 字符级扫描；产生 Token 流（25+ 种 token 类型） |
| **Parser** | `Parser.hpp`、`Parser.cpp` | 递归下降解析器；产生 35 种具体 AstNode 子类型 |
| **Ast** | `Ast.hpp` | AST 节点定义：LiteralNode、BinaryOpNode、IfStatementNode、FnDefNode、ClassDefNode 等 |
| **Compiler** | `Compiler.hpp`、`Compiler.cpp` | 遍历 AST，发射字节码到 CompiledFunction，含常量池、upvalue 描述符和 OP_EVAL_AST 侧表 |
| **BytecodeChunk** | `BytecodeChunk.hpp` | CompiledFunction（代码字节、常量、行号、参数、upvalue、局部变量名）+ Closure（函数 + 捕获的 upvalue） |
| **VM** | `VM.hpp`、`VM.cpp` | 基于栈的字节码执行引擎，含调用帧、upvalue 管理、try/catch 处理器和 AST 解释器桥接 |
| **Interpreter** | `Interpreter.hpp`、`Interpreter.cpp` | AST 遍历解释器（通过 `accept()` 实现访问者模式）。定义所有原生函数（abs、len、sin、sort 等）和模块加载 |
| **Environment** | `Environment.hpp`、`Environment.cpp` | 词法作用域链、Class/Instance OOP 模型与方法绑定 |
| **Value** | `Value.hpp`、`Value.cpp` | 多态值层次结构：12 种具体子类型，支持算术、比较、下标和切片操作 |
| **Serializer** | `Serializer.hpp`、`Serializer.cpp` | BytecodeChunk（.prc）和 vector<AstNodePtr>（.prt）的二进制序列化，含魔数验证和版本号 |
| **msgs** | `msgs.hpp`、`msg_cn.hpp`、`msg_en.hpp` | 基于枚举的多语言消息系统（中英文各约 100 条消息） |

### 执行模式

1. **AST 解释器**（`--ast`，默认）：解析 → 直接遍历 AST。完整语言支持，调试简单。
2. **字节码 VM**（`--vm`）：解析 → 编译为字节码 → 在 VM 上执行。热路径（算术、循环、调用）更快。复杂特性（类、try-catch、模块）通过 `OP_EVAL_AST` 回退到 AST 解释器。
3. **预编译字节码**（`.prc`）：加载序列化的 BytecodeChunk → VM。无需解析。
4. **预编译 AST**（`.prt`）：加载序列化的 AST → Interpreter。无需解析。

### 关键设计决策

- **混合执行**：编译器为简单操作（算术、局部变量、控制流）发射纯字节码，为复杂特性发射 `OP_EVAL_AST` 指令。VM 两者都处理，将其局部变量桥接到临时的 `Environment` 供 AST 解释器使用。
- **懒加载模块**：`require` 注册一个 `ModuleProxy`，仅在首次访问时才加载解析目标文件。
- **BigNumber 透明性**：所有数字内部都是 `BigNumber`。`NumberValue` 类包装它，`Value` 上的算术运算符委托给 `BigNumber` 方法。编译后数字操作没有运行时类型分发开销。

---

## 文件参考

| 文件 | 说明 |
|------|------|
| `src/main.cpp` | 入口点、CLI 解析、REPL |
| `src/Tokenizer.*` | 词法分析器 |
| `src/Parser.*` | 语法分析器 |
| `src/Ast.hpp` | AST 节点类型 |
| `src/Compiler.*` | AST → 字节码编译器 |
| `src/BytecodeChunk.hpp` | 字节码数据结构 |
| `src/VM.*` | 虚拟机 |
| `src/Interpreter.*` | AST 解释器 |
| `src/Environment.*` | 作用域 / OOP 运行时 |
| `src/Value.*` | 值类型 |
| `src/BigNumber.hpp` | BigNumber 算术 |
| `src/OpCode.hpp` | 指令集 |
| `src/Serializer.*` | .prc / .prt 序列化 |
| `src/msgs.hpp` | 消息枚举 |
| `src/msg_cn.hpp` | 中文翻译 |
| `src/msg_en.hpp` | 英文翻译 |
| `src/File.hpp` | 文件 I/O 对象（开发中） |

---

## 许可证

(c) 2024-2026 DarkstarXD. All Rights Reserved.
