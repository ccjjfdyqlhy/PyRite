# PyRite 速查表

## 声明与类型
```python
dec x = 42          # 任意精度数
str s = "hi"        # 字符串
bin b = 0xFF0A      # 字节
ln  a = [1, 2, 3]   # 列表
dim d = {"k": 1}    # 字典
any v = f()         # 任意
nul                 # 空值
```

## 控制流
```python
if c then … elif c2 then … else … end
while c do … fin … end
loop … for N times
loop(idx) … for N times
loop … until c
for x in list do … end
break / continue
await cond() then … end
try … catch e … fin … end
raise("msg")
```

## 函数
```python
fn f(dec a, dec b = 1) do … end
fn f(dec x) -> expr
dec g = fn(dec x) -> expr
```

## 类型
```python
struct Name(字段…)
ins Name(字段…) 初始化代码 contains fn 方法() … end endins
new(Name)
this.field
```

## 运算
```python
+ - * / ^ %        == != < > <= >=        not and or
+= *= /= ^= %=     (没有 -=！)
a[i]  a[s:e:step]  obj.field
"x" as dec   123 as str   nul as ln
```

## 内置
```python
say(x)  ask("提示")  swap(a,b)  using 长名 as 短名
abs len rt sort setify max min countdown hash
sin cos tan log new set_precision get_precision
approx is_int is_neg to_double Exception halt
upper lower trim split join replace substring char ord
range reverse keys values has type
is_null is_number is_string is_list is_dict is_function
```

## 模块与库
```python
require "list" as list       // 取一个模块（编译时就放进程序里）
require "lib" as std          // 取整个标准库

list.sum  list.map  list.filter  list.fold  list.take  list.skip
text.starts_with  text.pad  text.lines  text.words
dict.get  dict.merge  dict.sorted_keys
num.clamp  num.gcd  num.lcm  num.pick  num.shuffle
flow.cursor  flow.next  flow.some  flow.every
```

## 文本里的值与成员
```python
"你好，{name}！"              // 运行时把变量放进去
"{{name}}"                    // 花括号本身

xs.length  xs.push(v)  xs.pop()  xs.join(sep)  xs.slice(a,b)  xs.indexOf(v)
s.length  s.upper()  s.lower()  s.trim()  s.split(sep)  s.replace(a,b)
s.startsWith(p)  s.endsWith(p)  s.indexOf(sub)
d.keys()  d.values()  d.has(k)
```

## 真值
只有 `0` 和 `nul` 为假，其余全真。

## 命令行
```bash
./PyRite s.pr        # AST 解释器（默认）
./PyRite --vm s.pr   # 字节码 VM
./PyRite s.prc       # 预编译字节码
./PyRite s.prt       # 预编译 AST
./PyRite             # REPL
```
