#ifndef HELP_CN_HPP
#define HELP_CN_HPP

#include <string>
#include <map>

namespace HelpMessagesCN {

const std::map<std::string, std::string> HELP_MESSAGES = {
    {"Exception", R"(
Exception(message)
  创建一个异常对象。

  参数:
    message - 异常消息，可以是任意类型的值

  返回值:
    异常对象

  示例:
    throw Exception("发生错误")
    throw Exception(404)
)"},

    {"abs", R"(
abs(number)
  返回数字的绝对值。

  参数:
    number - 一个数字

  返回值:
    该数字的绝对值

  示例:
    abs(-5)      # 返回 5
    abs(3.14)    # 返回 3.14
)"},

    {"len", R"(
len(value)
  返回字符串或列表的长度。

  参数:
    value - 字符串或列表

  返回值:
    长度（数字）

  示例:
    len("hello")      # 返回 5
    len([1, 2, 3])    # 返回 3
)"},

    {"rt", R"(
rt(number, n=2)
  计算数字的n次方根。

  参数:
    number - 被开方数
    n - 可选，根的次数，默认为2（平方根）

  返回值:
    计算结果

  示例:
    rt(9)       # 返回 3（平方根）
    rt(8, 3)    # 返回 2（立方根）
)"},

    {"sort", R"(
sort(list)
  对列表进行排序并返回新列表。

  参数:
    list - 要排序的列表

  返回值:
    排序后的新列表（原列表不变）

  示例:
    sort([3, 1, 2])    # 返回 [1, 2, 3]
)"},

    {"setify", R"(
setify(list)
  移除列表中的重复元素，返回新列表。

  参数:
    list - 要处理的列表

  返回值:
    去重后的新列表

  示例:
    setify([1, 2, 2, 3])    # 返回 [1, 2, 3]
)"},

    {"max", R"(
max(values...)
  返回最大值。

  参数:
    values... - 多个值，或一个列表

  返回值:
    最大值

  示例:
    max(1, 5, 3)         # 返回 5
    max([1, 5, 3])       # 返回 5
)"},

    {"min", R"(
min(values...)
  返回最小值。

  参数:
    values... - 多个值，或一个列表

  返回值:
    最小值

  示例:
    min(1, 5, 3)         # 返回 1
    min([1, 5, 3])       # 返回 1
)"},

    {"countdown", R"(
countdown(seconds)
  创建一个倒计时器函数。

  参数:
    seconds - 倒计时秒数

  返回值:
    一个计时器函数，调用该函数返回：
      - 1 表示时间到
      - 0 表示还在倒计时中

  示例:
    timer = countdown(5)
    # 5秒内调用 timer() 返回 0
    # 5秒后调用 timer() 返回 1
)"},

    {"hash", R"(
hash(data, key)
  使用给定密钥计算数据的哈希值。

  参数:
    data - 要哈希的数据（任意类型）
    key - 哈希密钥（数字）

  返回值:
    哈希值（数字）

  示例:
    hash("hello", 123)    # 返回哈希值
)"},

    {"sin", R"(
sin(angle)
  计算角度的正弦值。

  参数:
    angle - 角度值（数字）

  返回值:
    正弦值

  示例:
    sin(0)         # 返回 0
    sin(90)        # 返回 1
)"},

    {"cos", R"(
cos(angle)
  计算角度的余弦值。

  参数:
    angle - 角度值（数字）

  返回值:
    余弦值

  示例:
    cos(0)         # 返回 1
    cos(90)        # 返回 0
)"},

    {"tan", R"(
tan(angle)
  计算角度的正切值。

  参数:
    angle - 角度值（数字）

  返回值:
    正切值

  示例:
    tan(0)         # 返回 0
    tan(45)        # 返回 1
)"},

    {"log", R"(
log(number)
  计算数字的自然对数。

  参数:
    number - 正数

  返回值:
    自然对数值

  示例:
    log(1)         # 返回 0
    log(2.718)     # 返回约 1
)"},

    {"new", R"(
new(Class, ...)
  创建类的新实例。

  参数:
    Class - 类对象
    ...   - 传递给初始化器的参数

  返回值:
    类实例

  示例:
    class Person:
        name = ""
    end
    p = new(Person)
)"},

    {"set_precision", R"(
set_precision(digits)
  设置BigNumber的默认精度。

  参数:
    digits - 小数位数

  返回值:
    null

  示例:
    set_precision(50)    # 设置50位精度
)"},

    {"get_precision", R"(
get_precision()
  获取当前BigNumber的默认精度。

  参数:
    无

  返回值:
    当前精度（数字）

  示例:
    get_precision()    # 返回当前精度
)"},

    {"approx", R"(
approx(number, precision)
  将数字近似到指定精度。

  参数:
    number    - 要近似的数字
    precision - 小数位数

  返回值:
    近似后的数字

  示例:
    approx(3.14159, 2)    # 返回 3.14
)"},

    {"is_int", R"(
is_int(number)
  检查数字是否为整数。

  参数:
    number - 要检查的数字

  返回值:
    1 表示是整数，0 表示不是

  示例:
    is_int(5)       # 返回 1
    is_int(5.5)     # 返回 0
)"},

    {"is_neg", R"(
is_neg(number)
  检查数字是否为负数。

  参数:
    number - 要检查的数字

  返回值:
    1 表示是负数，0 表示不是

  示例:
    is_neg(-5)      # 返回 1
    is_neg(5)       # 返回 0
)"},

    {"to_double", R"(
to_double(number)
  将BigNumber转换为双精度浮点数表示。

  参数:
    number - BigNumber数字

  返回值:
    转换后的数字（以浮点数表示）

  示例:
    to_double(3.14159265358979)    # 返回浮点数表示
)"}
};

inline std::string get_help(const std::string& func_name) {
    auto it = HELP_MESSAGES.find(func_name);
    if (it != HELP_MESSAGES.end()) {
        return it->second;
    }
    return "";
}

inline std::string get_all_functions() {
    std::string result = "可用内置函数列表:\n";
    for (const auto& pair : HELP_MESSAGES) {
        result += "  - " + pair.first + "\n";
    }
    result += "\n输入 help(\"函数名\") 查看详细帮助。\n";
    result += "例如: help(\"abs\")";
    return result;
}

}

#endif
