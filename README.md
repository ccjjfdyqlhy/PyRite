![cover](https://github.com/ccjjfdyqlhy/PyRite/blob/main/cover.png)
# PyRite: 一个简洁到神奇的编程语言。

缺点：目前只支持简体中文……

![Language](https://img.shields.io/badge/language-C%2B%2B11-blue.svg)
![Status](https://img.shields.io/badge/status-in--development-orange.svg)

PyRite 是一个用 C++11 编写的、小巧的动态脚本语言解释器。它的设计目标是提供一个易于学习、语法简洁且可嵌入的语言，同时具备一些现代编程语言的特性，如面向对象、异常处理和高精度数学运算。

## ✨ 主要特性

*   **简洁的语法**: 语法设计受到 Python 和 Lua 的启发，易于阅读和编写。
*   **高精度整数**: 内置 `BigNumber` 支持，可以处理超出标准 64 位整数范围的巨大数字。
*   **动态类型**: 声明时支持类型关键字（`dec`, `str`, `bin`, `list`）以增强可读性和进行隐式类型转换。
*   **面向对象编程 (OOP)**: 支持类（`ins`）、实例、字段、方法和 `this` 关键字。
*   **控制流**: 包含 `if/then/else`, `while/do/finally` 等完整的控制流结构。
*   **函数与闭包**: 支持头等函数、参数类型提示、默认参数值和词法闭包。
*   **异常处理**: 完整的 `try/catch/finally` 机制，用于健壮的错误处理。
*   **丰富的内置函数**: 提供数学、列表操作、哈希、计时等常用功能。
*   **交互式解释器 (REPL)**: 提供一个交互式环境，用于实时测试代码片段。
*   **实验性编译**: 独特的 `compile()` 功能，可将 PyRite 脚本转译为 C++ 代码并编译成本地可执行文件。

## ✨ 未来支持的特性

*   **多语言支持**: 通过加载语言文件修改解释器语言。
*   **代码引用**: 使用 `load()` 与 `dump()` 实现向环境中导入/卸载库或代码。
*   **与 C++ 混合开发**: 支持与底层C++代码共同编码、导入动态链接库等。
*   **更好的I/O**: 支持内存修改和文件读写操作。
*   **深度学习底层支持**: 支持张量、矩阵等基本系统。
*   **更好的流程控制**: 完善 `mark()` 、 `jump()` 和 `await` 的逻辑。

## 🔧 环境与构建

### 先决条件

*   只需要一个支持 C++11 的 C++ 编译器（例如 **g++** 或 **Clang**）。
  
### 构建解释器

**注意：开发版解释器源代码中可能会启用 `DEBUG` ，这将会在终端里释放大量的内部调试信息。如果你不想看到这些额外的内容，请在编译前将 `SimPy.cpp` 中前面位置的 `debug` 设为 `false` 。**

要编译 PyRite 解释器本身，在项目根目录中运行以下命令：

```bash
g++ PyRite.cpp -o PyRite -std=c++11 -O2
```

## 🚀 使用方法

PyRite 解释器支持两种操作模式：

### 1. 交互式解释器 (REPL)

直接运行可执行文件以启动 REPL：

```bash
./PyRite
```

你会看到一个提示符，可以在其中逐行输入代码。输入 `run()` 来执行缓冲区中的所有代码，或输入 `halt()` 退出。

### 2. 执行脚本文件

将你的 PyRite 代码保存到一个文件中，然后将其作为参数传递给解释器：

```bash
./PyRite your_script.src
```

## 📜 PyRite 语法简介

开发该语法旨在能用极为通俗而简洁的语言编写程序。

### 1. 数据类型

*   `dec`: 十进制数（高精度整数或小数）。
*   `str`: 字符串。
*   `bin`: 二进制数据（以 `0x` 开头的十六进制字面量）。
*   `list`: 动态数组/列表。
*   `null`: 表示空值。
*   `function`: 函数对象。
*   `class`: 类定义。
*   `instance`: 类的实例。

### 2. 变量

使用类型关键字声明变量。如果未提供初始值，则默认为该类型的零值或空值。

```src
# 变量声明与赋值 #
dec my_number = 123.456
str greeting = "Hello"
bin data = 0xdeadbeef
list items = [1, "two", 3]
my_number = my_number + 1
```

### 3. 运算符

*   **算术**: `+`, `-`, `*`, `/`, `^` (乘方)
*   **比较**: `==`, `!=`, `<`, `>`, `<=`, `>=`
*   **逻辑**: 逻辑运算通过 `if` 和 `while` 的真值判断实现（0 和 null 为假，其他为真）。

### 4. 控制流

```src
# If-Then-Else #
if x > 10 then
    say("x is large")
else
    say("x is small")
endif

# While-Do-Finally #
dec i = 5
while i > 0 do
    say(i)
    i = i - 1
finally
    say("Loop finished!")
endwhile
```

### 5. 函数

可以有类型和默认值。

```src
# 定义一个带默认参数的函数 #
def greet(str name, str message = "Hello") do
    return message + ", " + name + "!"
enddef

# 调用函数 #
say(greet("Alice"))               # 输出: Hello, Alice! #
say(greet("Bob", "Goodbye"))      # 输出: Goodbye, Bob! #
```

### 6. 类与对象 (OOP)

使用 `ins` 关键字定义类，`contains` 块内定义方法。使用 `new()` 内置函数创建实例。

```src
# 定义一个 Counter 类 #
ins Counter(dec initial = 0) contains
    # 这是一个字段，在 ins(...) 中定义 #

    # 'this' 关键字引用实例本身 #
    dec value = this.initial

    # 定义一个方法 #
    def increment(dec amount) do
        this.value = this.value + amount
    enddef

    def get_value() do
        return this.value
    enddef
endins

# 创建一个 Counter 实例 #
dec my_counter = new(Counter)
my_counter.increment(5)
say(my_counter.get_value()) # 输出: 5 #

# 创建一个带初始值的实例 #
dec another_counter = new(Counter(100))
say(another_counter.get_value()) # 输出: 100 #
```

### 7. 异常处理

使用 `try/catch` 来处理运行时错误或手动抛出的异常。

```src
try
    dec result = 10 / 0
catch e
    say("An error occurred: " + e)
finally
    say("This will always run.")
endtry

# 抛出自定义异常 #
try
    raise(Exception("Something went wrong!"))
catch ex
    say("Caught custom exception: " + ex.payload)
endtry
```

### 8. 内置函数和命令

*   使用 `#` 包括住的部分会被看作注释，不管是否跨越行。

#### REPL 命令

*   `run(tick=0, limit=0)`: 执行缓冲区中的代码。
    *   `tick=1`: 显示执行耗时。
    *   `limit=ms`: 设置执行超时时间（毫秒）。
*   `compile(route="", args="")`: 将代码编译为可执行文件。
    *   `route="path/to/file.pr"`: 指定要编译的源文件路径。如果为空，则编译缓冲区代码。
    *   `args="-luser32"`: 传递给 C++ 编译器的额外参数。
*   `halt()`: 退出 REPL。
*   `about()`: 显示版本信息。
*   `$ <expression>`: 立即执行单行代码并将其添加到缓冲区。
*   `$# <expression>`: 立即执行单行代码但不将其添加到缓冲区（用于临时调试）。

#### 常用函数

*   `say(...)`: 打印一个或多个值到控制台。
*   `inp(prompt)`: 显示提示并从用户处读取一行输入。
*   `abs(number)`: 返回数字的绝对值。
*   `rt(number, n=2)`: 计算 `number` 的 n 次方根（默认为平方根）。
*   `sort(list)`: 返回一个排序后的新列表。
*   `setify(list)`: 返回一个移除了重复元素的新列表。
*   `min(a, b, ...)` 或 `min(list)`: 返回最小值。
*   `max(a, b, ...)` 或 `max(list)`: 返回最大值。
*   `hash(data, key)`: 计算数据的哈希值。
*   `countdown(seconds)`: 返回一个计时器函数，`await` 该函数可实现延时。
*   `new(class)`: 创建一个类的新实例。
*   `Exception(payload)`: 创建一个异常对象，用于 `raise`。

## ⚙️ `compile()` 功能

这是 PyRite 最具实验性的功能。它不是一个真正的编译器，而是一个**转译器**。
**注意：转译模板尚未适配一些REPL中的功能，因此使用这些功能进行代码转译会在输出二进制文件之后报错。**

**工作原理**:
1.  当你调用 `compile()` 时，PyRite 会读取一份名为 `template.cpp` 的 C++ 模板文件。
2.  它将你的 PyRite 源代码作为一个巨大的原始字符串，注入到 `template.cpp` 文件中。
3.  这个 `template.cpp` 文件本身包含一个完整的 PyRite 解释器。当生成的可执行文件运行时，它会立即执行被注入的 PyRite 脚本。
4.  PyRite 调用其内置的 C++ 编译器（`compilers/MinGW64/bin/g++.exe`）来编译这个新生成的 C++ 文件。
5.  最终产物是一个独立的可执行文件，它打包了你的脚本和运行它所需的所有逻辑。

**如何使用**:

1.  确保 `PyRite.exe` 的同级目录下有 `template.cpp` 文件和 `compilers` 文件夹。
2.  在 REPL 中编写或加载你的代码。
3.  调用 `compile()`:

    ```python
    # 编译 REPL 缓冲区中的代码，输出为 buffer.exe
    compile()
    
    # 编译指定文件，并添加链接库
    compile(route="C:/Users/Admin/Desktop/my_game.src", args="-lwinmm")
    ```

## 🤝 贡献

欢迎对 PyRite 项目做出贡献！你可以通过以下方式参与：
*   报告 Bug 或提出新功能建议。
*   **由于代码标准和早期开发阶段的原因，暂时不接受有关代码修改的PR。**
*   撰写文档和示例。
