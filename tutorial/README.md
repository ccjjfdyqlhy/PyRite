# PyRite 精讲（配合源码实测）

> 这份教程不是照抄 `guide.md`——每一段行为都在本仓库的 `PyRite` 二进制上跑过。
> 凡是与 `guide.md` 不一致的地方，本文都会标出来（第 12 章「踩坑清单」）。

---

## 0. 先看清楚这个项目

PyRite 不是一个「用 Python 写的脚本」，而是一套**从零实现的语言 + 运行时**：

| 层 | 文件 | 干什么 |
|----|------|--------|
| BigNumber | `src/BigNumber.hpp` | 纯 C++ 实现的任意精度十进制运算（无 GMP 依赖），所有数字的内核 |
| Value | `src/Value.*` | 运行时类型系统：12 个具体子类型（Number/String/Binary/Ln/Dim/Null/Function/NativeFn/BoundMethod/Exception/ModuleProxy/Closure…） |
| Tokenizer | `src/Tokenizer.*` | 字符流 → Token 流 |
| Parser | `src/Parser.*` | 递归下降 → 35 种 AST 节点 |
| Environment | `src/Environment.*` | 词法作用域链、闭包、Class/Instance |
| Interpreter | `src/Interpreter.*` | AST 遍历解释器（**默认执行引擎**），内建函数都在这里定义 |
| Compiler | `src/Compiler.*` | AST → 字节码 |
| VM | `src/VM.*` | 栈式字节码虚拟机（`--vm`），约 50 条指令 |
| Serializer | `src/Serializer.*` | `.prc`（字节码）/ `.prt`（AST）二进制序列化 |
| JsBytecodeSerializer | `src/JsBytecodeSerializer.*` | 导出 PRC1 JSON，给浏览器里的 JS VM 用（见 `read/` 与 PyRiteJS 仓库） |

执行路径：

```
.pr ──Tokenizer──▶ Token ──Parser──▶ AST ──┬──▶ Interpreter   （默认，直接遍历）
                                            └──▶ Compiler ──▶ BytecodeChunk ──▶ VM （--vm，更快）
```

混合执行是它的特色：编译器给算术、局部变量、控制流发纯字节码；遇到类、try/catch、模块、await、raise 等复杂特性，就发一条 `OP_EVAL_AST` 让 VM 回退给 AST 解释器。所以 `--vm` 和默认模式输出完全一致，只是热路径更快。

### 构建与运行

```bash
make release            # -O2 -DNDEBUG
make debug              # -O0 -g -DDEBUG
make clean
make check-opcodes      # 校验 opcodes.def 与生成代码是否同步

./PyRite script.pr          # AST 解释器
./PyRite --vm script.pr     # 字节码 VM
./PyRite script.prc         # 跑已编译字节码
./PyRite script.prt         # 跑已序列化 AST
./PyRite                    # REPL
```

**一句话记住这门语言**：语法像 Python，块结构像 BASIC，数字像科学计算器。

---

## 1. 语法骨架

### 1.1 自由格式

没有花括号，也没有缩进要求（缩进只是给人看的）。块由**关键字**开启、由 `end` 闭合：

```python
if score >= 90 then
  say("优秀")
elif score >= 60 then
  say("及格")
else
  say("不及格")
end
```

`end` 是通用关闭词，也可以用更具体的 `endif` / `endfn` / `endwhile` / `endtry` / `endawait` / `endins` / `endloop`。
**但一个块只能关一次**——`guide.md` 第 4 章那段

```python
fn add(dec a, dec b) do
  return a + b
endfn
end        # ← 多余，会报 [解析错误] 缺少表达式
```

是错的，二选一即可。

### 1.2 注释

```python
// 行注释，到行尾结束
say("hello")  // 行尾注释

# 块注释，用 # 包围 #
   可以跨行
#
```

⚠️ **没有分号**。一行一条语句，`say(1);` 会解析失败（Python 用户最容易踩的坑）。

### 1.3 关键字全表（来自 `src/Tokenizer.cpp`）

| 类别 | 关键字 |
|------|--------|
| 类型 | `dec` `str` `bin` `ln` `dim` `nul` `any` |
| 块 | `if` `then` `elif` `else` `endif` `end` |
| 循环 | `while` `do` `fin`/`finally` `endwhile` `loop` `for` `times` `until` `endloop` `break` `continue` `in` |
| 函数 | `fn` `endfn` `return` |
| 类 | `ins` `contains` `endins` `struct` |
| 错误 | `try` `catch` `endtry` `raise` |
| 异步 | `await` `then` `endawait` |
| 模块 | `require` `expose` `using` `as` |
| I/O | `say` `ask` |
| 逻辑 | `not` `and` `or` |

---

## 2. 数据类型与转换

| 类型 | 关键字 | 内部 | 示例 |
|------|--------|------|------|
| Decimal | `dec` | BigNumber（任意精度） | `123` `-45.67` |
| String | `str` | std::string | `"hello"` `'world'` |
| Binary | `bin` | 字节序列 | `0xFF0A` |
| Linear | `ln` | 列表（也是栈/队列） | `[1, "two", 3]` |
| Dim | `dim` | 字典（键为字符串） | `{"k": "v"}` |
| Null | `nul` | 空 | `nul` |
| Any | `any` | 不限 | `any x = f()` |

### 2.1 声明 = 运行时转换契约（关键理解）

```python
dec x = "123"       // 字符串 → 数字，运行时转换
str y = 456         // 数字 → 字符串
ln  z = [1,2]       // 列表 → 列表
```

**这不是静态类型检查，而是运行期强制转换。** 转换失败会报运行时错误：

```python
dec bad = "abc"    // [运行时错误] 无法将字符串 'abc' 转换为数字
ln  bad2 = 5       // [运行时错误] 只能用 ln 初始化 ln 变量
```

各类型的转换规则：

| 声明 | 规则 |
|------|------|
| `dec` | 字符串/十六进制 → 数字；`nul` → 0 |
| `str` | 任何值 → `toString()` |
| `bin` | 十六进制字符串 → 字节；`nul` → `0x00` |
| `ln` | 必须是 `ln`，或 `nul` → `[]` |
| `dim` | 必须是 `dim`，或 `nul` → `{}` |
| `any` | 原样保留，不转换 |

### 2.2 显式转换 `as`

```python
say("123" as dec)     // 123
say(456 as str)       // 456
say("0xFF" as bin)    // 0xff
say([1,2,3] as str)   // [1, 2, 3]
say(nul as ln)        // []
say(nul as dim)       // {}
```

### 2.3 真值规则

只有 `0` 和 `nul` 是假，其余一律为真（包括空字符串 `""` 和空列表 `[]`）。

```python
if score >= 90 then     // 数字比较
if items then          // [] 也是真！判空要写 len(items) > 0
```

---

## 3. 运算符

| 类别 | 运算符 |
|------|--------|
| 算术 | `+` `-` `*` `/` `^` `%` |
| 比较 | `==` `!=` `<` `>` `<=` `>=` |
| 逻辑 | `not` `and` `or` |
| 复合赋值 | `+=` `*=` `/=` `^=` `%=`（**`-=` 目前不可用，见第 12 章**） |
| 下标/切片 | `a[i]` `a[start:end:step]` |
| 属性 | `obj.field` |

几个必须知道的细节：

```python
say(1 and 2)     // 2  —— and/or 返回操作数本身，不是布尔
say(0 or 3)      // 3
say("5" == 5)    // 0  —— 比较不做跨类型转换
say(5 == 5.0)    // 1
say(10.5 % 3)    // [运行时错误] 取模要求整数
say(7 / 2)       // 3.5000…（50 位小数，没有整数除法）
```

⚠️ `//` 是**行注释**，不是整数除法。想做整数除法用 `rt(x, 1)` 或先取整。

---

## 4. 控制流

### 4.1 if / then / elif / else / end

```python
if score >= 90 then
  say("优秀")
elif score >= 60 then
  say("及格")
else
  say("不及格")
end
```

单行写法：`if cond then stmt endif`。

### 4.2 while / do / fin / end

`fin`（或 `finally`）是循环结束后的收尾块，正常退出和 `break` 都会执行它：

```python
dec i = 0
while i < 5 do
  say(i)
  i = i + 1
fin
  say("循环结束")
end
```

### 4.3 loop 三种形态

```python
loop say("Hello") for 5 times        // 计数

loop                              // 多行计数
  say("world")
for 3 times

loop(idx)                         // 带索引，索引变量作用域在循环体内
  say(idx)                       // 0,1,2
for 5 times

dec i = 5                         // 直到条件成立（do-while 语义，body 至少执行一次）
loop
  i = i - 1
until i <= 0
```

### 4.4 for-in

只能遍历 `ln`：

```python
for x in items do say(x) end
```

### 4.5 break / continue

```python
dec i = 0
while i < 10 do
  i = i + 1
  if i == 3 then continue endif
  if i > 5 then break endif
  say(i)          // 1 2 4 5
end
```

### 4.6 await / then

`await` 是**轮询式**的：每 20ms 评估一次条件，为真才执行 `then` 块。

```python
dec t = countdown(3)
say("等待 3 秒...")
await t() then
  say("时间到！")
end
```

---

## 5. 函数

### 5.1 两种定义

```python
fn add(dec a, dec b) do
  return a + b
end

fn double(dec x) -> x * 2      // 单表达式，自动 return
```

箭头形式**只能有一个表达式**，不能写赋值或控制流。

### 5.2 lambda / 高阶 / 立即调用

```python
dec square = fn(dec x) -> x * x
fn apply(dec x, any f) -> f(x)
say(apply(5, fn(dec n) -> n * 3))      // 15
say((fn(dec a, dec b) -> a + b)(10, 20)) // 30
```

### 5.3 默认参数

```python
fn f(dec x = 42, str s = "default") do … end
fn g(dec n = some_global) -> n * 2
```

### 5.4 闭包

内层函数捕获外层变量，并保留其状态：

```python
fn makeCounter() do
  dec count = 0
  fn inc() do
    count = count + 1
    return count
  end
  return inc
end
dec c = makeCounter()
say(c())   // 1
say(c())   // 2
```

### 5.5 作用域

函数体、`if` 分支、循环体、`try` 块都会开**新的 Environment**（子作用域）。
所以循环索引变量不会污染外层同名变量。

没有 `return` 的函数返回 `nul`。

---

## 6. 面向对象

### 6.1 struct：纯数据

```python
struct Point(dec x, dec y)
struct Config(str name = "default", dec version = 1)

dec p = new(Point)
p.x = 10
p.y = 20
say(p.x)
```

### 6.2 ins：带方法的类

```python
ins Counter(dec val = 0) contains
  fn inc() do this.val = this.val + 1 end
  fn get() -> this.val
endins

dec c = new(Counter)
c.inc()
say(c.get())
```

**初始化代码写在第一个 `contains` 之前**（类体内只允许 `fn` 方法定义）：

```python
ins Greeter(str who = "世界")
  say("你好，" + this.who)      // ← 初始化代码
contains
  fn hello() -> "hello " + this.who
endins
```

### 6.3 struct + 外部函数（经典数据结构范式）

```python
struct Stack(ln items = [])
fn push(any self, any item) do
  self.items = self.items + [item]
end
fn pop(any self) do
  if len(self.items) == 0 then raise("empty") endif
  any v = self.items[len(self.items) - 1]
  self.items = self.items[0:len(self.items) - 1]
  return v
end
dec s = new(Stack)
push(s, 10)
say(pop(s))   // 10
```

---

## 7. 错误处理

```python
try
  dec r = 10 / 0
catch err
  say("出错: " + (err as str))
fin
  say("清理")
end

fn check(dec age) do
  if age < 18 then raise("未成年") endif
end

try
  check(15)
catch msg
  say("拒绝: " + msg)
end
```

- `catch` 拿到的是异常对象，`as str` 后拼进字符串。
- `raise("...")` 抛出字符串载荷；`raise(Exception("..."))` 抛出异常对象。
- `fin` 块在 try/catch 之后执行（无论是否出错）。

---

## 8. 模块系统

```python
require "utils/math"            // 变量名 = 路径最后一段
require "utils/math" as m       // 指定别名
require "utils/dir"             // 目录模块（入口 _index.pr）
```

三条规则：

1. **懒加载**：`require` 只注册占位符，第一次访问该变量时才真正读文件、解析、执行。
2. **单文件模块**：顶层变量**全部自动导出**。
3. **目录模块**：入口 `_index.pr` **必须用 `expose`** 标记导出项。

```python
// utils/_index.pr
expose fn add(dec a, dec b) -> a + b
expose dec PI = 3
fn _on_load() do say("utils 已加载") end
```

⚠️ **路径相对当前工作目录（CWD）解析，不是相对脚本所在目录。** 所以要在模块所在目录里跑，或者用相对 CWD 的路径。

---

## 9. 高精度数字

所有数字默认就是 BigNumber，没有 `Decimal()`、没有 import、没有精度上限。

```python
fn fact(dec n) do
  if n == 0 then return 1 else return n * fact(n - 1) endif
end
say(fact(100))   // 158 位整数，精确无误
```

除法默认保留 **50 位小数**，可调：

```python
say(get_precision())     // 50
set_precision(4)
say(1 / 3)               // 0.3333
set_precision(50)
say(approx(3.14159, 2))  // 3.14
say(to_double(1 / 3))    // 0.333333（转双精度）
```

---

## 10. 内置函数速查

| 函数 | 说明 |
|------|------|
| `abs(n)` | 绝对值 |
| `len(x)` | 字符串长度 / 列表元素数 |
| `rt(n, k=2)` | k 次方根 |
| `sort(ln)` | 排序，返回新列表 |
| `setify(ln)` | 去重 |
| `max(a...)` / `min(a...)` | 最大 / 最小（可传列表或多个值） |
| `countdown(s)` | 返回计时器函数；到期后返回 1，否则 0 |
| `hash(data, key)` | djb2 哈希 |
| `sin/cos/tan/log(x)` | 三角函数 / 自然对数（**角度制**） |
| `new(Class, ...)` | 实例化 |
| `set_precision(n)` / `get_precision()` | BigNumber 精度 |
| `approx(n, p)` | 按位数近似 |
| `is_int(n)` / `is_neg(n)` | 类型判断 |
| `to_double(n)` | 转双精度 |
| `hash` `Exception(payload)` `halt()` | 异常对象 / 退出 |

语言级内建语句：`say(x)` 打印、`ask("提示")` 读一行输入、`swap(a, b)` 交换、`using 长名 as 短名` 起别名。

```python
swap(a, b)               // 交换两个变量
swap(items[0], items[2])  // 甚至能交换元素
using very_long_name as v // 短别名
```

REPL 里还有：`help()`、`help("abs")`、`about()`、`run()`、`halt()`。

---

## 11. 命令行与 REPL

```bash
./PyRite script.pr          # AST 解释器（默认）
./PyRite --vm script.pr     # 字节码 VM
./PyRite --dump-bytecode script.pr        # 打印字节码反汇编
./PyRite --emit-bytecode out.prc script.pr # 编译存 .prc 并运行
./PyRite --save-bytecode out.prc script.pr # 只存不跑
./PyRite --emit-ast out.prt script.pr
./PyRite --save-ast out.prt script.pr
./PyRite --emit-json out.json script.pr    # 导出给浏览器 JS VM 用的 PRC1 JSON
```

REPL：

```
$ say(1+1)          # 立即执行
$# say(99)          # 立即执行但不进缓冲区
say(1+1)            # 进缓冲区
run()               # 执行缓冲区
help() / about() / halt()
```

多行块在 REPL 里靠 `fn`/`if`/`while`/`try`/`ins`/`loop` 自动续行，用 `end` 收尾。

---

## 12. 踩坑清单（全部实测）

| # | 现象 | 原因 / 正确写法 |
|---|------|----------------|
| 1 | `a -= 1` 解析失败 | **Tokenizer 没实现 `-=`**（`src/Tokenizer.cpp:39` 只处理了 `->`）。用 `a = a - 1`。`guide.md` 把 `-=` 列为可用，是文档错误。 |
| 2 | `1e100` 解析失败 | 分词器不支持科学计数法。写 `1000000...` 或 `10 * 10 * ...`。 |
| 3 | `guide.md` 里 `endfn` 后面又跟 `end` | 一个块只能关一次。 |
| 4 | `"abc".upper()` 报「只有实例才能拥有属性」 | **没有字符串方法**。字符串只能用 `+` 拼接、`[]` 取下标、切片、`len()`、`as dec`。 |
| 5 | `"a" * 3` 报「不支持的 '*' 操作数类型」 | 字符串乘法不支持；列表可以（`[1,2] * 2` → `[1,2,1,2]`）。 |
| 6 | `10.5 % 3` 运行时报错 | 取模只支持整数。 |
| 7 | `7 / 2` 得到一长串小数 | 默认 50 位精度，`set_precision(n)` 可调。 |
| 8 | `rt(27, 3)` 得 `3.0000…0` | 数值分析的固有误差，末位有残差。 |
| 9 | `dec x = "abc"` 报运行时错误 | 类型关键字是运行期转换契约，不是静态检查。 |
| 10 | `ln x = 5` 报运行时错误 | `ln`/`dim` 必须用同类初始化。 |
| 11 | `require "utils/math"` 找不到文件 | 路径相对 **CWD** 解析，不是相对脚本目录。 |
| 12 | 目录模块里的函数取不到 | 目录模块的 `_index.pr` 必须写 `expose`。 |
| 13 | `ins` 类体里写普通语句报错 | 类体只允许 `fn`；初始化代码要放在第一个 `contains` **之前**。 |
| 14 | `if items then` 对空列表仍为真 | 真值规则：只有 `0` 和 `nul` 为假。判空写 `len(items) > 0`。 |
| 15 | REPL 里 `run()` 报解析错误 | 缓冲区把 `$` 前缀也存进去了；用不带 `$` 的行配 `run()`，或直接用 `$` 立即执行。 |
| 16 | `loop(idx)` 后外层 `idx` 不变 | 循环体开新作用域，索引变量是循环内局部变量。 |
| 17 | `say(1);` 解析失败 | **分号不是语句分隔符**（Tokenzier 没有 `;`）。Python 用户最容易踩这个坑——一行一条语句。 |
| 18 | `dim` 取不存在的键直接抛错 | 没有 `has()`/`keys()`/`values()` 内置函数。用 try/catch 包一层来判断键是否存在（见 `tutorial/answers.pr` 第 5 题）。 |

---

## 13. 练习

见 `tutorial/exercises.pr`（题目）与 `tutorial/answers.pr`（参考答案）。
全部示例脚本在 `tutorial/ex/`，可直接运行：

```bash
./tutorial/run_examples.sh
```

### 推荐学习路线

1. **第 1 天**：`tutorial/ex/01_basics.pr` + `02_control.pr` —— 把语法骨架和控制流吃透。
2. **第 2 天**：`03_functions.pr` + `04_oop.pr` —— 函数、闭包、类。
3. **第 3 天**：`05_errors.pr` + `06_modules/` —— 工程化能力。
4. **第 4 天**：`07_bignum.pr` + `08_await.pr`，然后自己实现一个 JSON 序列化器 / 简易解释器。
5. **进阶**：读 `read/`（源码精读笔记）理解实现，再看 PyRiteJS 仓库了解字节码如何搬到浏览器里。

### 延伸阅读

| 文件 | 内容 |
|------|------|
| `guide.md` | 官方语言手册（注意第 12 章列出的 3 处错误） |
| `README_zh.md` | 项目总览与架构 |
| `read/0_overview.md` ~ `read/3_operations.md` | 源码精读笔记（Interpreter 内部机制） |
| `opcodes.def` | 字节码指令的唯一真源（`make check-opcodes` 可校验） |
| `editor/editor.html` | 浏览器版编辑器，单文件、离线可用 |
