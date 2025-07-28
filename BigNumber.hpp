#ifndef BIG_INT_HPP
#define BIG_INT_HPP

#include <string>
#include <vector>
#include <stdexcept>
#include <algorithm>
#include <cctype>
#include <iostream>
#include <cmath>
#include <cstdint> // 用于 uint32_t 和 uint64_t
#include <iomanip> // 用于 std::setw 和 std::setfill
#include <sstream> // 用于 std::stringstream

class BigNumber {
private:
    std::string digits; // 数字部分, 例如 "12345"
    bool is_negative;   // 是否为负数
    int decimal_pos;    // 小数点的位置（从右边数起）

    // 规范化数字（移除多余的0）
    void normalize() {
        if (digits.empty() || !std::all_of(digits.begin(), digits.end(), ::isdigit)) {
            digits = "0";
            is_negative = false;
            decimal_pos = 0;
            return;
        }
        if (decimal_pos > 0) {
            size_t last_digit = digits.find_last_not_of('0');
            if (last_digit == std::string::npos) { digits = "0"; decimal_pos = 0; }
            else { int to_remove = digits.length() - 1 - last_digit; if (to_remove > decimal_pos) to_remove = decimal_pos; digits.erase(last_digit + 1); decimal_pos -= to_remove; if (decimal_pos < 0) decimal_pos = 0; }
        }
        if (digits.length() > (size_t)decimal_pos) {
            size_t first_digit = digits.find_first_not_of('0');
            if (first_digit == std::string::npos || (int)first_digit >= (int)digits.length() - decimal_pos) { if (decimal_pos > 0) { digits.erase(0, digits.length() - decimal_pos -1); if (digits.empty() || (digits.length() > 0 && digits[0] == '.')) digits.insert(0, "0"); } else { digits = "0"; } }
            else { digits.erase(0, first_digit); }
        }
        if (digits == "0" || digits.empty()) { is_negative = false; decimal_pos = 0; digits = "0"; }
    }
    
    // 比较两个绝对值大小: 1 (this > other), -1 (this < other), 0 (this == other)
    int compare_abs(const BigNumber& other) const {
        BigNumber a = *this, b = other;
        int max_dec = std::max(a.decimal_pos, b.decimal_pos);
        a.digits.append(max_dec - a.decimal_pos, '0'); a.decimal_pos = max_dec;
        b.digits.append(max_dec - b.decimal_pos, '0'); b.decimal_pos = max_dec;

        int int_len1 = a.digits.length() - a.decimal_pos;
        int int_len2 = b.digits.length() - b.decimal_pos;

        if (int_len1 != int_len2) return int_len1 > int_len2 ? 1 : -1;
        
        if (a.digits > b.digits) return 1;
        if (a.digits < b.digits) return -1;
        return 0;
    }
    
    static BigNumber add_abs(BigNumber a, BigNumber b) {
        int max_dec = std::max(a.decimal_pos, b.decimal_pos);
        a.digits.append(max_dec - a.decimal_pos, '0'); b.digits.append(max_dec - b.decimal_pos, '0');
        std::string res = ""; int carry = 0; int i = a.digits.length() - 1, j = b.digits.length() - 1;
        while (i >= 0 || j >= 0 || carry) { int sum = carry; if (i >= 0) sum += a.digits[i--] - '0'; if (j >= 0) sum += b.digits[j--] - '0'; res += std::to_string(sum % 10); carry = sum / 10; }
        std::reverse(res.begin(), res.end());
        return BigNumber(res, false, max_dec);
    }
    
    static BigNumber subtract_abs(BigNumber a, BigNumber b) {
        int max_dec = std::max(a.decimal_pos, b.decimal_pos);
        a.digits.append(max_dec - a.decimal_pos, '0'); b.digits.append(max_dec - b.decimal_pos, '0');
        std::string res = ""; int borrow = 0; int i = a.digits.length() - 1, j = b.digits.length() - 1;
        while (i >= 0) { int diff = (a.digits[i] - '0') - borrow; if (j >= 0) diff -= (b.digits[j] - '0'); if (diff < 0) { diff += 10; borrow = 1; } else { borrow = 0; } res += std::to_string(diff); i--; j--; }
        std::reverse(res.begin(), res.end());
        return BigNumber(res, false, max_dec);
    }

    static const uint32_t CHUNK_BASE = 1000000000;
    static const int CHUNK_DIGITS = 9;

    static std::vector<uint32_t> stringToVector(const std::string& s) {
        std::vector<uint32_t> vec;
        if (s == "0") {
            return {0};
        }
        for (int i = s.length(); i > 0; i -= CHUNK_DIGITS) {
            std::string chunk_str = s.substr(std::max(0, i - CHUNK_DIGITS), std::min((int)s.length(), i));
            vec.push_back(std::stoul(chunk_str));
        }
        return vec;
    }

    static std::string vectorToString(std::vector<uint32_t>& v) {
        while (v.size() > 1 && v.back() == 0) {
            v.pop_back();
        }
        if (v.empty()) return "0";

        std::stringstream ss;
        ss << v.back(); // 输出最高位的块，不带前导零

        // 输出其余的块，并用前导零填充至9位
        for (int i = v.size() - 2; i >= 0; --i) {
            ss << std::setw(CHUNK_DIGITS) << std::setfill('0') << v[i];
        }
        return ss.str();
    }
    
    // 对两个使用向量表示的数进行乘法运算
    static std::vector<uint32_t> multiply_vectors(const std::vector<uint32_t>& a, const std::vector<uint32_t>& b) {
        if ((a.size() == 1 && a[0] == 0) || (b.size() == 1 && b[0] == 0)) return {0};
        
        std::vector<uint32_t> result(a.size() + b.size(), 0);
        
        for (size_t i = 0; i < a.size(); ++i) {
            uint64_t carry = 0;
            for (size_t j = 0; j < b.size(); ++j) {
                // 使用64位整数来防止乘法和加法溢出
                uint64_t p = static_cast<uint64_t>(a[i]) * b[j] + result[i + j] + carry;
                result[i + j] = p % CHUNK_BASE;
                carry = p / CHUNK_BASE;
            }
            if (carry > 0) {
                result[i + b.size()] += carry;
            }
        }
        
        // 移除结果中可能存在的前导零块
        while (result.size() > 1 && result.back() == 0) {
            result.pop_back();
        }  
        
        return result;
    }

public:
    BigNumber() : is_negative(false), decimal_pos(0), digits("0") {}
    BigNumber(long long n) : is_negative(n < 0), decimal_pos(0), digits(std::to_string(n == 0 ? 0 : (n < 0 ? -n : n))) {}
    BigNumber(const std::string& s, bool neg, int dec_pos) : digits(s), is_negative(neg), decimal_pos(dec_pos) { normalize(); }
    BigNumber(std::string s) {
        if (s.empty()) { is_negative = false; decimal_pos = 0; digits = "0"; return; }
        if (s[0] == '-') { is_negative = true; s.erase(0, 1); } else { is_negative = false; }
        for (size_t i = 0; i < s.length(); ++i) { if (!isdigit(s[i]) && s[i] != '.') throw std::invalid_argument("Invalid character in number string."); }
        size_t dot_pos = s.find('.');
        if (dot_pos == std::string::npos) { decimal_pos = 0; digits = s; } 
        else { decimal_pos = s.length() - dot_pos - 1; s.erase(dot_pos, 1); digits = s; }
        normalize();
    }

    std::string toString() const {
        if (digits == "0") return "0";
        std::string s = digits;
        if (decimal_pos > 0) { if ((int)s.length() <= decimal_pos) { s.insert(0, decimal_pos - s.length() + 1, '0'); } s.insert(s.length() - decimal_pos, "."); }
        if (is_negative) { s.insert(0, "-"); }
        return s;
    }
    
    long long toLongLong() const {
        std::string int_part = digits;
        if(decimal_pos > 0) {
            if ((int)int_part.length() <= decimal_pos) return 0;
            int_part = int_part.substr(0, int_part.length() - decimal_pos);
        }
        try {
            long long val = std::stoll(int_part);
            return is_negative ? -val : val;
        } catch(...) {
            throw std::runtime_error("BigNumber too large to fit in long long.");
        }
    }
    bool isInteger() const { return decimal_pos == 0; }

    BigNumber operator+(const BigNumber& other) const {
        if (this->is_negative == other.is_negative) { BigNumber res = add_abs(*this, other); res.is_negative = this->is_negative; return res; }
        else { if (this->compare_abs(other) >= 0) { BigNumber res = subtract_abs(*this, other); res.is_negative = this->is_negative; return res; } else { BigNumber res = subtract_abs(other, *this); res.is_negative = other.is_negative; return res; } }
    }
    BigNumber operator-(const BigNumber& other) const { BigNumber inv = other; inv.is_negative = !inv.is_negative; return *this + inv; }
    BigNumber operator*(const BigNumber& other) const {
        std::string res_digits(this->digits.length() + other.digits.length(), '0');
        for (int i = this->digits.length() - 1; i >= 0; i--) { int carry = 0; for (int j = other.digits.length() - 1; j >= 0; j--) { int p = (this->digits[i]-'0') * (other.digits[j]-'0') + (res_digits[i+j+1]-'0') + carry; res_digits[i+j+1] = (p % 10) + '0'; carry = p / 10; } res_digits[i] += carry; }
        return BigNumber(res_digits, this->is_negative != other.is_negative, this->decimal_pos + other.decimal_pos);
    }
    BigNumber operator/(const BigNumber& other) const {
        if (other.digits == "0") throw std::runtime_error("Division by zero.");
        BigNumber a = *this, b = other; a.is_negative = false; b.is_negative = false;
        int dec_shift = std::max(a.decimal_pos, b.decimal_pos); a.digits.append(dec_shift - a.decimal_pos, '0'); b.digits.append(dec_shift - b.decimal_pos, '0');
        a.decimal_pos = 0; b.decimal_pos = 0; a.normalize(); b.normalize();
        const int precision = 50; a.digits.append(precision, '0');
        if (b.digits.length() > a.digits.length() && b.digits != "0") { a.digits.append(b.digits.length() - a.digits.length(), '0'); }
        std::string quotient = "", current_dividend_str = "";
        for (char digit : a.digits) {
            current_dividend_str += digit; BigNumber current_dividend(current_dividend_str); int q_digit = 0;
            while (current_dividend.compare_abs(b) >= 0) { current_dividend = current_dividend - b; q_digit++; }
            quotient += std::to_string(q_digit); current_dividend_str = current_dividend.digits == "0" ? "" : current_dividend.digits;
        }
        return BigNumber(quotient, this->is_negative != other.is_negative, precision);
    }

    BigNumber operator^(const BigNumber& exp) const {
        if (!exp.isInteger()) {
            throw std::runtime_error("Exponent must be an integer for ^ operator.");
        }
        long long e_val;
        try {
            e_val = exp.toLongLong();
        } catch(...) {
            throw std::runtime_error("Exponent is too large.");
        }
        
        if (e_val == 0) return BigNumber(1);
        if (*this == BigNumber("0")) return BigNumber("0");
        
        bool exp_is_neg = e_val < 0;
        if (exp_is_neg) e_val = -e_val;
        
        // 核心逻辑：使用平方求幂算法，但其内部乘法采用高效的向量运算
        std::vector<uint32_t> base_vec = stringToVector(this->digits);
        std::vector<uint32_t> res_vec = {1};
        
        while (e_val > 0) {
            if (e_val % 2 == 1) {
                res_vec = multiply_vectors(res_vec, base_vec);
            }
            base_vec = multiply_vectors(base_vec, base_vec);
            e_val /= 2;
        }
        
        std::string res_digits = vectorToString(res_vec);
        
        // 计算结果的小数位数和符号
        int final_decimal_pos = this->decimal_pos * (exp_is_neg ? -e_val : e_val);
        bool final_is_negative = this->is_negative && (exp.toLongLong() % 2 != 0);
        
        BigNumber result(res_digits, final_is_negative, final_decimal_pos);
        
        // 如果指数为负，返回 1 / result
        if (exp_is_neg) {
            return BigNumber(1) / result;
        }
        
        return result;
    }
	 
    BigNumber abs() const { BigNumber res = *this; res.is_negative = false; return res; }
    
    // n-th root using Newton's method
    static BigNumber root(const BigNumber& num, const BigNumber& n, int precision = 50) {
        if (num.is_negative && n.toLongLong() % 2 == 0) throw std::runtime_error("Even root of a negative number is not real.");
        if (n <= BigNumber(0)) throw std::runtime_error("Root must be a positive integer.");
        
        BigNumber x = num.abs() / n; // Initial guess
        if (x == BigNumber(0)) x = BigNumber(1);
        BigNumber n_minus_1 = n - BigNumber(1);
        BigNumber limit("1", false, precision + 5); // Convergence limit

        for (int i=0; i < 100; ++i) { // Limit iterations
            BigNumber x_pow_n_minus_1 = x ^ n_minus_1;
            BigNumber f_x = (x ^ n) - num.abs();
            BigNumber f_prime_x = n * x_pow_n_minus_1;
            if (f_prime_x == BigNumber(0)) break; // Avoid division by zero
            BigNumber delta = f_x / f_prime_x;
            x = x - delta;
            if (delta.abs() < limit) break;
        }
        x.is_negative = num.is_negative;
        return x;
    }

    bool operator==(const BigNumber& other) const { return this->is_negative == other.is_negative && this->compare_abs(other) == 0; }
    bool operator!=(const BigNumber& other) const { return !(*this == other); }
    bool operator<(const BigNumber& other) const { if (this->is_negative != other.is_negative) return this->is_negative; return this->is_negative ? this->compare_abs(other) > 0 : this->compare_abs(other) < 0; }
    bool operator>(const BigNumber& other) const { return other < *this; }
    bool operator<=(const BigNumber& other) const { return !(other < *this); }
    bool operator>=(const BigNumber& other) const { return !(*this < other); }
};

#endif // BIG_INT_HPP
