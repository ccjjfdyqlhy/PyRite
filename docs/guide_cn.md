
## PyRite 语言简易指南 (v0.20.1)

### 目录

1.  [语言简介](#1-语言简介)
2.  [核心语法与概念](#2-核心语法与概念)
    *   [注释](#注释)
    *   [数据类型](#数据类型)
    *   [变量声明与赋值](#变量声明与赋值)
    *   [运算符](#运算符)
    *   [类型转换](#类型转换)
3.  [控制流](#3-控制流)
    *   [条件语句 `if-then-else-endif`](#条件语句-if-then-else-endif)
    *   [循环语句](#循环语句)
    *   [等待语句 `await-then-endawait`](#等待语句-await-then-endawait)
4.  [函数 (Function)](#4-函数-function)
    *   [函数定义](#函数定义)
    *   [参数](#参数)
    *   [返回值](#返回值)
    *   [函数调用](#函数调用)
5.  [面向对象编程 (OOP)](#5-面向对象编程-oop)
    *   [类的定义 `ins-contains-endins`](#类的定义-ins-contains-endins)
    *   [实例化 `new()`](#实例化-new)
    *   [字段与方法](#字段与方法)
    *   [`this` 关键字](#this-关键字)
6.  [错误处理](#6-错误处理)
    *   [`try-catch-finally-endtry`](#try-catch-finally-endtry)
    *   [`raise`](#raise)
7.  [内置功能与命令](#7-内置功能与命令)
    *   [输入/输出](#输入输出)
    *   [特殊命令 `swap()`](#特殊命令-swap)
    *   [变量别名 `using-as`](#变量别名-using-as)
8.  [内置函数库](#8-内置函数库)
9.  [交互式控制台 (REPL)](#9-交互式控制台-repl)
10. [代码例程：算法与数据结构](#10-代码例程算法与数据结构)
    *   [例程1：阶乘 (递归与迭代)](#例程1阶乘-递归与迭代)
    *   [例程2：斐波那契数列](#例程2斐波那契数列)
    *   [例程3：冒泡排序](#例程3冒泡排序)
    *   [例程4：线性搜索](#例程4线性搜索)
    *   [例程5：二分搜索](#例程5二分搜索)
    *   [例程6：栈 (Stack) 的实现](#例程6栈-stack-的实现)
    *   [例程7：队列 (Queue) 的实现](#例程7队列-queue-的实现)
    *   [例程8：二叉搜索树 (BST) 的实现](#例程8二叉搜索树-bst-的实现)

---

### 1. 语言简介

PyRite 是一种解释型编程语言，其语法设计融合了 Python、C++ 和 BASIC 的特点。它支持强类型变量声明、面向对象编程、函数式编程以及结构化的错误处理。其核心特色是内置了对大数（BigNumber）的无缝支持，使得进行高精度计算变得异常简单。

### 2. 核心语法与概念

#### 注释

PyRite 使用 `#` 作为块注释的开始和结束符。

**语法:**
```python
# 这是一个单行注释 #
#
  这是一个
  多行注释
#
```

#### 数据类型

PyRite 有五种基本数据类型：

| 类型名 | 关键字 | 描述 | 示例 |
| :--- | :--- | :--- | :--- |
| **Decimal** | `dec` | 十进制数字，支持任意精度的整数和小数（由 BigNumber 库支持）。 | `123`, `-45.67` |
| **String** | `str` | 字符串，由单引号 `'` 或双引号 `"` 包围。 | `"hello"`, `'world'` |
| **Binary** | `bin` | 二进制数据，以 `0x` 开头的十六进制字面量表示。 | `0xFF0A`, `0x00` |
| **List** | `list` | 列表，可以容纳不同类型元素的有序集合。 | `[1, "two", 0x03]` |
| **Null** | `nul` | 表示“无”或“空”的特殊值。 | `nul` |

#### 变量声明与赋值

变量在使用前必须用类型关键字声明。声明时可以进行初始化。

**语法:**
```python
# 声明变量
dec my_number
str my_string
list my_list

# 声明并初始化
dec age = 25
str name = "PyRite"
list items = [10, 20, 30]

# 赋值
age = 26
```

#### 运算符

| 类别 | 运算符 | 描述 |
| :--- | :--- | :--- |
| **算术** | `+`, `-`, `*`, `/`, `^` | 加、减、乘、除、幂 |
| **比较** | `==`, `!=`, `<`, `<=`, `>`, `>=` | 等于、不等于、小于、小于等于、大于、大于等于 |
| **赋值** | `=` | 将右侧表达式的值赋给左侧变量 |
| **成员访问**| `.` | 访问类实例的字段或方法 |
| **下标** | `[]` | 访问列表元素或进行切片 |

**运算规则:**
*   `+` 可用于数字相加、字符串拼接、列表合并。
*   `*` 可用于数字相乘、列表重复。
*   比较运算符返回 `1` (真) 或 `0` (假)。

#### 类型转换

使用 `as` 关键字可以显式地将一个值转换为另一种类型。

**语法:**
```python
dec d = "123" as dec   # 字符串转数字
str s = 456 as str     # 数字转字符串
bin b = "0xAF" as bin  # 字符串转二进制

say(d + 1) # 输出: 124
```

### 3. 控制流

#### 条件语句 `if-then-else-endif`

用于根据条件的真假执行不同的代码块。条件为 `0` 或 `nul` 时为假，其他情况为真。

**语法:**
```python
if condition then
  # 如果条件为真，执行这里的代码
else
  # (可选) 如果条件为假，执行这里的代码
endif
```
**示例:**
```python
dec score = 85
if score >= 90 then
  say("优秀")
else
  if score >= 60 then
    say("及格")
  else
    say("不及格")
  endif
endif
```

#### 循环语句

##### `while-do-finally-endwhile`

当条件为真时，重复执行 `do` 代码块。循环结束后（无论正常结束还是被 `break`），都会执行 `finally` 代码块。

**语法:**
```python
while condition do
  # 循环体
finally
  # (可选) 循环结束后执行
endwhile
```
**示例:**
```python
dec i = 0
while i < 5 do
  say(i)
  i = i + 1
finally
  say("循环结束")
endwhile
```

##### `repeat-for-times` / `repeat-until`

`repeat` 提供了两种循环模式。

*   **计数循环:** 重复执行固定次数。
*   **条件循环:** 重复执行直到某个条件为真。
*   **无限循环:** 如果没有 `for` 或 `until`，则为无限循环，需要 `break` 退出。

**语法:**
```python
# 计数循环
repeat
  # 循环体
for count_expression times

# 条件循环
repeat
  # 循环体
until condition_expression
```
**示例:**
```python
# 重复5次
repeat
  say("Hello")
for 5 times

# 倒计时
dec counter = 10
repeat
  say(counter)
  counter = counter - 1
until counter < 0
```

##### `break` 语句

用于立即跳出当前所在的 `while` 或 `repeat` 循环。

```python
dec i = 0
while 1 do # 无限循环
  say(i)
  if i >= 10 then
    break
  endif
  i = i + 1
endwhile
```

#### 等待语句 `await-then-endawait`

暂停执行，直到某个条件表达式为真，然后执行 `then` 块内的代码。这在等待异步操作或定时任务时非常有用。

**语法:**```python
await condition then
  # 条件满足后执行
endawait
```
**示例:**
```python
# countdown(秒数) 返回一个函数，该函数在倒计时结束后返回1
dec timer_is_done = countdown(3)
say("等待3秒...")
await timer_is_done() then
  say("3秒已到！")
endawait
```

### 4. 函数 (Function)

#### 函数定义

使用 `fn` 关键字定义函数，以 `endfn` 结束。

**语法:**
```python
fn function_name(parameter_list) do
  # 函数体
  return return_value # (可选)
endfn
```

#### 参数

*   参数必须声明类型 (`dec`, `str`, `bin`, `list`, `any`)。`any` 表示接受任意类型。
*   可以为参数提供默认值，使其成为可选参数。

**示例:**
```python
# 带有类型和默认值的参数
fn greet(str name, str greeting = "Hello") do
  say(greeting + ", " + name + "!")
endfn
```

#### 返回值

使用 `return` 关键字从函数返回值。如果函数没有 `return` 语句，则默认返回 `nul`。

```python
fn add(dec a, dec b) do
  return a + b
endfn

dec result = add(10, 20) # result is 30
```

#### 函数调用

使用函数名后跟括号 `()` 来调用函数，并传入参数。

```python
greet("Alice")          # 输出: Hello, Alice!
greet("Bob", "Hi")      # 输出: Hi, Bob!
```

### 5. 面向对象编程 (OOP)

#### 类的定义 `ins-contains-endins`

使用 `ins` (instance) 关键字定义一个类，`contains` 关键字之后是类的主体，以 `endins` 结束。

**语法:**
```python
ins ClassName(field_definitions) contains
  # 方法定义
endins
```

#### 实例化 `new()`

使用内置的 `new()` 函数并传入类名来创建类的实例（对象）。

**语法:**
```python
dec my_object = new(ClassName)
```

#### 字段与方法

*   **字段 (Fields):** 在类名后的括号中定义，类似于函数参数，可以有类型和默认值。它们是每个实例的属性。
*   **方法 (Methods):** 在 `contains` 块内定义的 `fn` 函数。它们是类的行为。

**示例:**
```python
# 定义一个简单的计数器类
ins Counter(dec initial_value = 0) contains
  # 字段: initial_value 在实例化时被设置

  fn increment() do
    # 'this' 关键字用于引用实例自身
    this.initial_value = this.initial_value + 1
  endfn

  fn get_value() do
    return this.initial_value
  endfn
endins

# 实例化
dec c = new(Counter)
c.increment()
c.increment()
say(c.get_value()) # 输出: 2
```

#### `this` 关键字

在方法内部，`this` 关键字是对当前实例的引用，用于访问该实例的字段或调用其其他方法。

### 6. 错误处理

PyRite 提供了一套完整的结构化异常处理机制。

#### `try-catch-finally-endtry`

*   `try`: 包含可能引发异常的代码。
*   `catch`: 如果 `try` 块中发生异常，则执行 `catch` 块。异常对象会被赋给指定的变量。
*   `finally`: 无论是否发生异常，`finally` 块中的代码总会被执行。

**语法:**
```python
try
  # 可能出错的代码
catch exception_variable
  # 异常处理代码
finally
  # (可选) 总是执行的代码
endtry
```
**示例:**
```python
try
  dec result = 10 / 0
catch err
  say("发生错误: " + (err as str))
finally
  say("清理工作完成")
endtry
```

#### `raise`

手动抛出一个异常。可以抛出任何类型的值。

**语法:**
```python
raise value_to_raise
```
**示例:**
```python
fn check_age(dec age) do
  if age < 18 then
    raise("年龄必须大于等于18")
  endif
  return 1
endfn

try
  check_age(16)
catch msg
  say("验证失败: " + msg)
endtry
```

### 7. 内置功能与命令

#### 输入/输出

*   `say(value)`: 向控制台打印一个值。
*   `ask(prompt)`: 显示提示信息 `prompt`，等待用户输入，并返回用户输入的字符串。`ask` 语句还可以使用 `as` 关键字直接赋值。

**示例:**
```python
say("你好, PyRite!")

# 传统用法
str user_name = ask("请输入你的名字: ")
say("你好, " + user_name)

# 使用 as 关键字
ask("请输入你的年龄: ") as str user_age
say("你 " + user_age + " 岁了。")
```

#### 特殊命令 `swap()`

`swap()` 是一个特殊的语言结构，用于交换两个变量的值，它会尝试保持变量的原有类型。

**语法:**
```python
swap(variable1, variable2)
```**示例:**
```python
dec a = 10
dec b = 20
swap(a, b)
say(a) # 输出: 20
say(b) # 输出: 10
```

#### 变量别名 `using-as`

为一个已存在的变量创建一个新的别名。

**语法:**
```python
using original_variable as alias_name
```
**示例:**
```python
dec long_variable_name = 123
using long_variable_name as lvn
say(lvn) # 输出: 123
```

### 8. 内置函数库

| 函数 | 描述 | 示例 |
| :--- | :--- | :--- |
| `Exception(payload)` | 创建一个标准的异常对象。 | `raise(Exception("自定义错误"))` |
| `abs(number)` | 返回数字的绝对值。 | `abs(-10)` returns `10` |
| `len(string_or_list)` | 返回字符串的长度或列表的元素个数。 | `len("abc")` returns `3` |
| `rt(number, n=2)` | 计算 `number` 的 `n` 次方根，`n` 默认为2（平方根）。 | `rt(16)` returns `4`, `rt(27, 3)` returns `3` |
| `sort(list)` | 返回一个已排序的新列表（不改变原列表）。 | `sort([3, 1, 2])` returns `[1, 2, 3]` |
| `setify(list)` | 移除列表中的重复元素，并返回一个新列表。 | `setify([1, 2, 2, 3])` returns `[1, 2, 3]` |
| `max(args...)` / `max(list)` | 返回参数或列表中的最大值。 | `max(1, 5, 2)` returns `5` |
| `min(args...)` / `min(list)` | 返回参数或列表中的最小值。 | `min(1, 5, 2)` returns `1` |
| `countdown(seconds)` | 返回一个计时器函数。该函数在倒计时结束前返回0，结束后返回1。 | `dec t = countdown(5); await t() then ...` |
| `hash(data, key)` | 使用 djb2 算法和一个数字密钥计算 `data` 的哈希值。 | `hash("hello", 123)` |
| `sin(x)`, `cos(x)`, `tan(x)` | 三角函数，`x` 为弧度。 | `sin(0)` |
| `log(x)` | 计算 `x` 的自然对数。 | `log(10)` |
| `new(ClassName)` | 创建一个类的实例。 | `dec c = new(Counter)` |

### 9. 交互式控制台 (REPL)

直接运行 PyRite 解释器而不带任何文件参数，将进入 REPL 模式。

*   **多行输入:** REPL 支持 `if`, `while`, `fn` 等多行代码块的输入。
*   `run()`: 执行在缓冲区中输入的所有代码。
    *   `run(tick=1)`: 执行代码并在结束后显示执行耗时。
    *   `run(limit=1000)`: 设置超时时间（毫秒），超时会中断执行。
*   `halt()`: 退出 REPL。
*   `about()`: 显示关于 PyRite 解释器的信息。
*   `$ <command>`: 立即执行单行命令，并将其加入缓冲区。
*   `$# <command>`: 立即执行单行命令，但**不**将其加入缓冲区（用于临时调试）。
*   `compile(...)`: 将 PyRite 代码编译成可执行文件。
    *   `compile()`: 编译缓冲区中的代码。
    *   `compile(route="path/to/script.pyr", args="-static")`: 编译指定文件，并可以附加额外的 C++ 编译器参数。

### 10. 代码例程：算法与数据结构

#### 例程1：阶乘 (递归与迭代)

```python
# 迭代实现
fn factorial_iter(dec n) do
  dec result = 1
  dec i = 1
  while i <= n do
    result = result * i
    i = i + 1
  endwhile
  return result
endfn

# 递归实现
fn factorial_rec(dec n) do
  if n == 0 then
    return 1
  else
    return n * factorial_rec(n - 1)
  endif
endfn

say("5! (iter) = " + (factorial_iter(5) as str))
say("5! (rec)  = " + (factorial_rec(5) as str))
```

#### 例程2：斐波那契数列

```python
fn fibonacci(dec n) do
  list sequence = [0, 1]
  if n <= 1 then
    return n
  endif

  dec i = 2
  while i <= n do
    dec next_fib = sequence[i-1] + sequence[i-2]
    list new_elements = sequence + [next_fib]
    sequence = new_elements
    i = i + 1
  endwhile
  
  return sequence[n]
endfn

say("第10个斐波那契数是: " + (fibonacci(10) as str))
```

#### 例程3：冒泡排序

```python
fn bubble_sort(list arr) do
  dec n = len(arr)
  dec i = 0
  while i < n - 1 do
    dec j = 0
    while j < n - i - 1 do
      if arr[j] > arr[j+1] then
        swap(arr[j], arr[j+1])
      endif
      j = j + 1
    endwhile
    i = i + 1
  endwhile
  return arr
endfn

list my_list = [64, 34, 25, 12, 22, 11, 90]
say("排序前: " + (my_list as str))
list sorted_list = bubble_sort(my_list)
say("排序后: " + (sorted_list as str))
```

#### 例程4：线性搜索

```python
fn linear_search(list arr, any target) do
  dec i = 0
  while i < len(arr) do
    if arr[i] == target then
      return i # 返回索引
    endif
    i = i + 1
  endwhile
  return -1 # 未找到
endfn

list items = ["apple", "banana", "cherry"]
dec index = linear_search(items, "banana")
say("'banana' 的索引是: " + (index as str))
```

#### 例程5：二分搜索

```python
# 注意：二分搜索要求列表已排序
fn binary_search(list sorted_arr, dec target) do
  dec low = 0
  dec high = len(sorted_arr) - 1

  while low <= high do
    dec mid = low + (high - low) / 2
    if sorted_arr[mid] == target then
      return mid
    endif
    if sorted_arr[mid] < target then
      low = mid + 1
    else
      high = mid - 1
    endif
  endwhile

  return -1 # 未找到
endfn

list data = [2, 5, 8, 12, 16, 23, 38, 56, 72, 91]
dec key = 23
dec result = binary_search(data, key)
say("数字 " + (key as str) + " 的索引是: " + (result as str))
```

#### 例程6：栈 (Stack) 的实现

```python
ins Stack() contains
  list items = []

  fn push(any item) do
    this.items = this.items + [item]
  endfn

  fn pop() do
    if this.is_empty() then
      raise("无法从空栈中弹出元素")
    endif
    dec last_index = len(this.items) - 1
    dec last_item = this.items[last_index]
    this.items = this.items[0:last_index] # 切片以移除最后一个元素
    return last_item
  endfn

  fn peek() do
    if this.is_empty() then
      return nul
    endif
    return this.items[len(this.items) - 1]
  endfn

  fn is_empty() do
    return len(this.items) == 0
  endfn
endins

dec s = new(Stack)
s.push(10)
s.push(20)
say("栈顶元素: " + (s.peek() as str)) # 20
say("弹出: " + (s.pop() as str))     # 20
say("栈是否为空: " + (s.is_empty() as str)) # 0 (false)
```

#### 例程7：队列 (Queue) 的实现

```python
ins Queue() contains
  list items = []

  fn enqueue(any item) do
    this.items = this.items + [item]
  endfn

  fn dequeue() do
    if this.is_empty() then
      raise("无法从空队列中出队")
    endif
    dec first_item = this.items[0]
    this.items = this.items[1:] # 切片以移除第一个元素
    return first_item
  endfn

  fn front() do
    if this.is_empty() then
      return nul
    endif
    return this.items[0]
  endfn

  fn is_empty() do
    return len(this.items) == 0
  endfn
endins

dec q = new(Queue)
q.enqueue("A")
q.enqueue("B")
say("队首元素: " + q.front()) # A
say("出队: " + q.dequeue())    # A
say("队首元素: " + q.front()) # B
```

#### 例程8：二叉搜索树 (BST) 的实现

```python
# 节点类
ins Node(dec key, any left = nul, any right = nul) contains
endins

# 二叉搜索树类
ins BinarySearchTree() contains
  dec root = nul

  fn insert(dec key) do
    if this.root == nul then
      this.root = new(Node(key=key))
    else
      this._insert_recursive(this.root, key)
    endif
  endfn
  
  fn _insert_recursive(any node, dec key) do
      if key < node.key then
          if node.left == nul then
              node.left = new(Node(key=key))
          else
              this._insert_recursive(node.left, key)
          endif
      else
          if node.right == nul then
              node.right = new(Node(key=key))
          else
              this._insert_recursive(node.right, key)
          endif
      endif
  endfn

  fn search(dec key) do
    return this._search_recursive(this.root, key)
  endfn
  
  fn _search_recursive(any node, dec key) do
    if node == nul or node.key == key then
      return node != nul
    endif
    
    if key < node.key then
      return this._search_recursive(node.left, key)
    else
      return this._search_recursive(node.right, key)
    endif
  endfn

endins

dec bst = new(BinarySearchTree)
bst.insert(50)
bst.insert(30)
bst.insert(70)
bst.insert(20)
bst.insert(40)
bst.insert(60)

say("树中是否存在 40? " + (bst.search(40) as str)) # 1 (true)
say("树中是否存在 99? " + (bst.search(99) as str)) # 0 (false)
```
