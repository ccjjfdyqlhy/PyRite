#include <iostream>
#include "BigNumber.hpp" // 假设您的头文件名为这个

int main() {
    BigNumber a("100000000000000000000");
    BigNumber b("8");

    // 使用标准除法，结果会带有很多0
    BigNumber::set_default_precision(20);
    BigNumber standard_div = a / b;
    std::cout << "Standard division: " << standard_div.toString() << std::endl;
    // 输出: Standard division: 12500000000000000000.00000000000000000000

    // 使用精确整数除法，结果是干净的整数
    BigNumber exact_div = a.exact_division(b);
    std::cout << "Exact division:    " << exact_div.toString() << std::endl;
    // 输出: Exact division:    12500000000000000000

    return 0;
}
