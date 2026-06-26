# PyRite 语言入门指南 (v0.31.0)

## 目录

1. [语言简介](#1-语言简介)
2. [核心语法](#2-核心语法)
   - [注释](#注释)
   - [数据类型](#数据类型)
   - [变量声明与赋值](#变量声明与赋值)
   - [运算符](#运算符)
   - [复合赋值](#复合赋值)
   - [类型转换](#类型转换)
3. [控制流](#3-控制流)
   - [if-then-elif-else-end](#if-then-elif-else-end)
   - [while-do-fin-end](#while-do-fin-end)
   - [loop-for/until/endloop](#loop-foruntilendloop)
   - [for-in 迭代](#for-in-迭代)
   - [break / continue](#break--continue)
   - [await-then-endawait](#await-then-endawait)
4. [函数](#4-函数)
   - [标准定义](#标准定义)
   - [简写 fn->](#简写-fn-)
   - [Lambda / 匿名函数](#lambda--匿名函数)
   - [参数与默认值](#参数与默认值)
5. [面向对象](#5-面向对象)
   - [struct 结构体](#struct-结构体)
   - [ins 类](#ins-类)
   - [实例化 new()](#实例化-new)
6. [错误处理](#6-错误处理)
   - [try-catch-fin-endtry](#try-catch-fin-endtry)
   - [raise](#raise)
7. [内置功能](#7-内置功能)
   - [输入输出 say / ask](#输入输出-say--ask)
   - [变量交换 swap()](#变量交换-swap)
   - [别名 using-as](#别名-using-as)
8. [内置函数](#8-内置函数)
9. [REPL](#9-repl)
10. [完整例程](#10-完整例程)

---

### 1. 语言简介

PyRite 是一门解释型脚本语言，语法融合 Python/C++/BASIC，核心特色是**内置 BigNumber 高精度运算**。

```
fn fact(dec n) -> n == 0 ? 1 else n * fact(n - 1)
say(fact(100))  # 任意精度 #
```

**设计要点：**
- 自由格式，不依赖缩进
- 类型关键词声明，自动类型转换
- `end` 作为通用块闭包，也可用具体关键词（`endif`/`endfn` 等）
- `->` 箭头定义单表达式函数
- `//` 行注释，`#...#` 块注释
- 所有数字都是高精度 BigNumber

### 2. 核心语法

#### 注释

```python
// 行注释，到行尾自动结束
say("hello")  // 行尾注释

# 块注释，用 # 包围 #
#
  多行注释
#
```

#### 数据类型

| 类型 | 关键字 | 说明 | 示例 |
|------|--------|------|------|
| Decimal | `dec` | 任意精度整数/小数 | `123`, `-45.67`, `1e100` |
| String | `str` | 字符串 | `"hello"`, `'world'` |
| Binary | `bin` | 十六进制二进制数据 | `0xFF0A` |
| Linear | `ln` | 线性序列（列表/栈/队列） | `[1, "two", 3]` |
| Dim | `dim` | 字典/映射 | `{"k": "v"}` |
| Null | `nul` | 空值 | `nul` |
| Any | `any` | 任意类型 | `any x = some_fn()` |

#### 变量声明与赋值

```python
dec age = 25
str name = "PyRite"
ln items = [10, 20, 30]
dim info = {"lang": "PyRite", "year": 2025}
any result = some_function()

age = 26                    // 重新赋值
items[0] = 99               // ln 下标赋值
info["lang"] = "Pro"        // dim 键赋值
```

#### 运算符

| 类别 | 运算符 |
|------|--------|
| 算术 | `+` `-` `*` `/` `^` `%` |
| 比较 | `==` `!=` `<` `>` `<=` `>=` |
| 逻辑 | `not` `and` `or` |
| 成员 | `.` 属性访问 |
| 下标 | `[]` ln 索引 / dim 键访问 |
| 切片 | `ln[start:end:step]` |
| 箭头 | `->` 函数简写 |

#### 复合赋值

```python
a += 5    // a = a + 5
a -= 3    // a = a - 3
a *= 2    // a = a * 2
a /= 4    // a = a / 4
a ^= 2    // a = a ^ 2
a %= 3    // a = a % 3
```

#### 类型转换

```python
"123" as dec     // → 123 (数字)
456 as str       // → "456"
"0xFF" as bin    // → 0xFF
[1,2,3] as str   // → "[1, 2, 3]"
nul as ln        // → []
nul as dim       // → {}
```

### 3. 控制流

#### if-then-elif-else-end

条件为 `0` 或 `nul` 时假，其他为真。支持 `elif` 链：

```python
dec score = 85
if score >= 90 then
  say("优秀")
elif score >= 60 then
  say("及格")
else
  say("不及格")
end
```

#### while-do-fin-end

```python
dec i = 0
while i < 5 do
  say(i)
  i = i + 1
fin
  say("循环结束")
end
// 或用 finally / endwhile
```

#### loop-for/until/endloop

```python
loop
  say("Hello")
for 5 times

loop
  say(i)
  i = i - 1
until i < 0

loop(i) for 5 times       // 带索引变量
  say("索引: " + (i as str))
end
```

#### for-in 迭代

遍历 ln 列表：

```python
ln items = ["A", "B", "C"]
for x in items do
  say(x)            // A, B, C
end
```

#### break / continue

```python
dec i = 0
while i < 10 do
  i = i + 1
  if i == 3 then continue endif   // 跳过 3
  if i > 5 then break endif        // 超过 5 退出
  say(i)
end
// 输出: 1, 2, 4, 5
```

#### await-then-endawait

```python
dec t = countdown(3)
say("等待 3 秒...")
await t() then
  say("时间到！")
end
```

### 4. 函数

#### 标准定义

```python
fn add(dec a, dec b) do
  return a + b
endfn         // 也用 end
end
```

#### 简写 fn->

单表达式函数，自动 return：

```python
fn double(dec x) -> x * 2
fn greet(str name) -> "hello " + name
fn add(dec a, dec b) -> a + b

say(double(5))        // 10
say(greet("alice"))   // hello alice
```

#### Lambda / 匿名函数

```python
// 箭头 lambda
dec square = fn(dec x) -> x * x

// 多语句匿名函数
dec greet = fn(str name) do
  say("hello " + name)
end

// 高阶函数
fn apply(dec x, any f) -> f(x)
say(apply(5, fn(dec n) -> n * 3))    // 15

// 立即调用
say((fn(dec a, dec b) -> a + b)(10, 20))  // 30
```

#### 参数与默认值

参数必须声明类型，支持默认值（字面量或表达式）：

```python
fn f(dec x = 42, str s = "default") do
  say(x)
endfn

fn g(dec n = some_global) -> n * 2   // 表达式默认值
```

### 5. 面向对象

#### struct 结构体

纯数据容器，只有字段没有方法：

```python
struct Point(dec x, dec y)
struct Config(str name = "default", dec version = 1)

dec p = new(Point)
p.x = 10
p.y = 20
say(p.x)   // 10

dec c = new(Config)
say(c.name)   // "default"
```

配合操作函数使用更灵活：

```python
struct Stack(ln items = [])
fn push(any s, any item) -> s.items = s.items + [item]
fn pop(any s) do
  any v = s.items[len(s.items)-1]
  s.items = s.items[0:len(s.items)-1]
  return v
endfn

dec s = new(Stack)
push(s, 10)
push(s, 20)
say(pop(s))   // 20
```

#### ins 类

方法定义在 `contains` 块中：

```python
ins Counter(dec val = 0) contains
  fn inc() do this.val = this.val + 1 endfn
  fn get() -> this.val
endins

dec c = new(Counter)
c.inc()
c.inc()
say(c.get())   // 2
```

`contains` 前可放初始化代码：

```python
ins Test(dec a = 100) contains
  say("初始化中...")
  this.a = this.a + 1
contains
  fn get_a() -> this.a
endins
```

#### 实例化 new()

```python
dec p = new(Point)
dec c = new(Counter)
```

### 6. 错误处理

#### try-catch-fin-endtry

```python
try
  dec r = 10 / 0
catch err
  say("出错: " + (err as str))
fin         // 或用 finally
  say("清理")
end
```

#### raise

```python
fn check(dec age) do
  if age < 18 then raise("未成年") endif
endfn

try
  check(15)
catch msg
  say("拒绝: " + msg)
end
```

### 7. 内置功能

#### 输入输出 say / ask

```python
say("hello")                    // 打印
str name = ask("你的名字: ")    // 输入
ask("年龄: ") as str age        // 输入并赋值
```

#### 变量交换 swap()

```python
dec a = 10
dec b = 20
swap(a, b)        // a=20, b=10

ln items = [1, 2, 3]
swap(items[0], items[2])  // [3, 2, 1]
```

#### 别名 using-as

```python
dec very_long_name = 42
using very_long_name as v
say(v)   // 42
```

### 8. 内置函数

| 函数 | 说明 |
|------|------|
| `abs(n)` | 绝对值 |
| `len(x)` | 字符串长度或 ln 元素数 |
| `rt(n, k=2)` | k 次方根 |
| `sort(ln)` | 排序（返回新 ln） |
| `setify(ln)` | 去重 |
| `max(a...)` / `min(a...)` | 最大/最小值 |
| `countdown(s)` | 返回计时器函数 |
| `hash(data, key)` | djb2 哈希 |
| `sin/cos/tan/log(x)` | 数学函数 |
| `new(Class)` | 创建实例 |
| `set_precision(n)` | 设置 BigNumber 精度 |
| `get_precision()` | 获取精度 |
| `approx(n, p)` | 按精度截断 |
| `is_int/is_neg(n)` | 类型判断 |
| `halt()` | 退出解释器 |
| `Exception(payload)` | 创建异常对象 |

### 9. REPL

```bash
./PyRite       # 进入 REPL
```

| 命令 | 说明 |
|------|------|
| `run()` | 执行缓冲区代码 |
| `run(tick=1)` | 执行并显示耗时 |
| `run(limit=1000)` | 设置超时毫秒 |
| `halt()` | 退出 |
| `about()` | 版本信息 |
| `help()` | 函数列表 |
| `help("abs")` | 函数帮助 |
| `$ code` | 执行并加入缓冲区 |
| `$# code` | 执行不加入缓冲区 |

**示例：**
```
(void)     1| fn fact(dec n) -> n == 0 ? 1 else n * fact(n - 1)
(fn)       2| end
(void)     3| run()
(void)     4| say(fact(10))
3628800
(void)     5| halt()
退出 REPL 模式...
```

### 10. 完整例程

#### 阶乘（简写 + 递归）

```python
fn fact(dec n) -> n == 0 ? 1 else n * fact(n - 1)
say(fact(100))
```

#### 斐波那契

```python
fn fib(dec n) do
  if n <= 1 then return n endif
  ln seq = [0, 1]
  dec i = 2
  while i <= n do
    seq = seq + [seq[i-1] + seq[i-2]]
    i = i + 1
  endwhile
  return seq[n]
endfn
say(fib(10))  // 55
```

#### 冒泡排序（复合赋值 + swap）

```python
fn sort(ln arr) do
  dec n = len(arr)
  dec i = 0
  while i < n - 1 do
    dec j = 0
    while j < n - i - 1 do
      if arr[j] > arr[j+1] then swap(arr[j], arr[j+1]) endif
      j += 1
    endwhile
    i += 1
  endwhile
  return arr
endfn

ln data = [64, 34, 25, 12, 22, 11, 90]
say(sort(data))  // [11, 12, 22, 25, 34, 64, 90]
```

#### for-in 迭代求和

```python
fn sum(ln items) do
  dec total = 0
  for x in items do total += x end
  return total
endfn
say(sum([1, 2, 3, 4, 5]))  // 15
```

#### 栈（struct + 操作函数）

```python
struct Stack(ln items = [])
fn push(any s, any item) -> s.items = s.items + [item]
fn top(any s) -> len(s.items) == 0 ? nul else s.items[len(s.items)-1]
fn pop(any s) do
  if len(s.items) == 0 then raise("empty") endif
  any v = top(s)
  s.items = s.items[0:len(s.items)-1]
  return v
endfn

dec s = new(Stack)
push(s, 10); push(s, 20); push(s, 30)
say(pop(s))  // 30
say(pop(s))  // 20
```

#### 二叉搜索树

```python
struct Node(dec key, any left = nul, any right = nul)
struct BST(any root = nul)

fn bst_insert(any self, dec key) do
  if self.root == nul then
    dec n = new(Node); n.key = key; self.root = n
  else
    fn ins(any node, dec k) do
      if k < node.key then
        if node.left == nul then dec n = new(Node); n.key = k; node.left = n
        else ins(node.left, k) endif
      else
        if node.right == nul then dec n = new(Node); n.key = k; node.right = n
        else ins(node.right, k) endif
      endif
    endfn
    ins(self.root, key)
  endif
endfn

fn bst_search(any self, dec key) do
  fn srch(any node, dec k) do
    if node == nul then return 0 endif
    if node.key == k then return 1 endif
    if k < node.key then return srch(node.left, k)
    else return srch(node.right, k) endif
  endfn
  return srch(self.root, key)
endfn

dec bst = new(BST)
bst_insert(bst, 50); bst_insert(bst, 30); bst_insert(bst, 70)
say(bst_search(bst, 40))  // 0
say(bst_search(bst, 50))  // 1
```

---

### 11. 模块系统

#### 基本用法

```python
require "path/to/module"            // 单文件引入
require "path/to/module" as alias   // 带别名引入
require "path/to/dir"               // 目录引入（入口 _index.pr）
```

`require` 是声明，不是函数调用，可出现在文件的任意位置。

#### 懒加载

`require` 执行时**不加载文件**，仅注册占位符。当该变量**首次被访问**时才真正加载：

```python
require "utils/math" as m    // 不加载任何文件
say(m.add(1, 2))             // 此时才读取、解析、执行 utils/math.pr
```

#### 单文件模块

```python
// utils/math.pr
fn add(dec a, dec b) -> a + b
dec PI = 3.14159
```

**所有顶层变量自动导出**，不需要额外关键字。

#### 目录模块

```
utils/
├── _index.pr      // 入口文件
├── string.pr
└── file.pr
```

目录模块的 `_index.pr` **必须用 `expose` 声明导出项**：

```python
// utils/_index.pr
expose fn add(dec a, dec b) -> a + b
expose dec PI = 3.14159
expose fn helper()

fn _on_load() do
  say("utils 模块已加载")
endfn
```

未标 `expose` 的顶层变量不会进入模块字典。

#### expose 关键字

`expose` 是声明修饰符，放在 `fn`/`dec`/`ins`/`struct` 前面：

```python
expose dec VERSION = "1.0"     // 导出变量
expose fn helper() -> ...      // 导出函数
expose ins MyClass(...) ...    // 导出类
expose struct Point(dec x, dec y)  // 导出结构体
```

- 单文件模块：所有顶层变量自动导出，`expose` 可选
- 目录模块的 `_index.pr`：必须用 `expose`

#### _on_load() 钩子

模块加载后，如果定义了 `_on_load()` 函数，会自动调用：

```python
// database.pr
fn connect() -> ...
fn _on_load() do
  say("数据库模块已加载")
endfn
```

#### 模块解析路径

- 相对路径：相对于当前工作目录
- 以 `/` 开头：绝对路径
- 自动尝试 `path`、`path.pr`、`path/_index.pr`

#### 循环引用保护

模块系统会检测循环引用并抛出运行时错误。

#### 完整示例

```
// utils/math.pr
fn add(dec a, dec b) -> a + b
fn mul(dec a, dec b) -> a * b
dec PI = 3.14159

// main.pr
require "utils/math" as m
say(m.add(10, 20))    // 30
say(m.PI)             // 3.14159
```

---

*PyRite v0.20.2 — 持续开发中*
