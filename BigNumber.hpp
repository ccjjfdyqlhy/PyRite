#ifndef BIG_INT_HPP
#define BIG_INT_HPP

#include <string>
#include <vector>
#include <stdexcept>
#include <algorithm>
#include <cctype>
#include <iostream>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <complex>

// =================================================================================
// BEGIN: Integrated high-performance implementation from source snippet
// All implementation details are encapsulated within the BigNumberDetail namespace.
// =================================================================================

namespace BigNumberDetail {

    typedef long long ll;
    typedef std::complex<double> cd;

    const int BASE = 5;       // Each digit in UnsignedDigit stores up to 5 decimal digits
    const int MOD = 100000; // The base for our big number representation (10^BASE)
    const int LGM = 17;
    const double PI = 3.1415926535897932384626;

    class UnsignedDigit;

    namespace DivHelper { UnsignedDigit quasiInv(const UnsignedDigit& v); }

    // Represents a large unsigned integer using a vector of digits in a given base (MOD).
    class UnsignedDigit {
    public:
        std::vector<int> digits;

    public:
        UnsignedDigit() : digits(1, 0) {}
        UnsignedDigit(const std::vector<int>& digits);
        UnsignedDigit(ll x);
        UnsignedDigit(std::string str);

        std::string toString() const;
        int size() const { return digits.size(); }
        bool isZero() const { return digits.size() == 1 && digits[0] == 0; }

        bool operator<(const UnsignedDigit& rhs) const;
        bool operator<=(const UnsignedDigit& rhs) const;
        bool operator==(const UnsignedDigit& rhs) const;

        UnsignedDigit operator+(const UnsignedDigit& rhs) const;
        UnsignedDigit operator-(const UnsignedDigit& rhs) const;
        UnsignedDigit operator*(const UnsignedDigit& rhs) const;
        UnsignedDigit operator/(const UnsignedDigit& rhs) const;
        UnsignedDigit operator/(int v) const;

        // Equivalent to multiplication by MOD^k
        UnsignedDigit move(int k) const;

        friend UnsignedDigit DivHelper::quasiInv(const UnsignedDigit& v);
        friend void swap(UnsignedDigit& lhs, UnsignedDigit& rhs) { std::swap(lhs.digits, rhs.digits); }

    public:
        void trim();
    };

    namespace ConvHelper { // FFT-based convolution for fast multiplication

        void fft(cd* a, int lgn, int d) {
            int n = 1 << lgn;
            static std::vector<int> brev;
            if (n != (int)brev.size()) {
                brev.resize(n);
                for (int i = 0; i < n; ++i)
                    brev[i] = (brev[i >> 1] >> 1) | ((i & 1) << (lgn - 1));
            }
            for (int i = 0; i < n; ++i)
                if (brev[i] < i)
                    std::swap(a[brev[i]], a[i]);
            
            for (int t = 1; t < n; t <<= 1) {
                cd omega(cos(PI / t), sin(PI * d / t));
                for (int i = 0; i < n; i += t << 1) {
                    cd* p = a + i;
                    cd w(1);
                    for (int j = 0; j < t; ++j) {
                        cd x = p[j + t] * w;
                        p[j + t] = p[j] - x;
                        p[j] += x;
                        w *= omega;
                    }
                }
            }
            if (d == -1) {
                for (int i = 0; i < n; ++i)
                    a[i] /= n;
            }
        }

        std::vector<ll> conv(const std::vector<int>& a, const std::vector<int>& b) {
            int n = a.size() - 1, m = b.size() - 1;
            if (n < 1000 / (m + 1) || n < 10 || m < 10) {
                std::vector<ll> ret(n + m + 1);
                for (int i = 0; i <= n; ++i)
                    for (int j = 0; j <= m; ++j)
                        ret[i + j] += a[i] * (ll)b[j];
                return ret;
            }
            int lgn = 0;
            while ((1 << lgn) <= n + m)
                ++lgn;
            std::vector<cd> ta(a.begin(), a.end()), tb(b.begin(), b.end());
            ta.resize(1 << lgn);
            tb.resize(1 << lgn);
            fft(ta.data(), lgn, 1);
            fft(tb.data(), lgn, 1);
            for (int i = 0; i < (1 << lgn); ++i)
                ta[i] *= tb[i];
            fft(ta.data(), lgn, -1);
            std::vector<ll> ret(n + m + 1);
            for (int i = 0; i <= n + m; ++i)
                ret[i] = ta[i].real() + 0.5;
            return ret;
        }

    } // namespace ConvHelper

    namespace DivHelper { // Newton-Raphson division

        UnsignedDigit quasiInv(const UnsignedDigit& v) {
            if (v.digits.size() == 1) {
                UnsignedDigit tmp;
                tmp.digits.assign(3, 0);
                tmp.digits[2] = 1;
                return tmp / v.digits[0];
            }
            if (v.digits.size() == 2) {
                UnsignedDigit sum = 0, go = 1;
                std::vector<int> tmp(4);
                go = go.move(4);
                std::vector<UnsignedDigit> db(LGM);
                db[0] = v;
                for (int i = 1; i < LGM; ++i)
                    db[i] = db[i - 1] + db[i - 1];
                for (int i = 3; i >= 0; --i) {
                    for (int k = LGM - 1; k >= 0; --k)
                        if (sum + db[k].move(i) <= go) {
                            sum = sum + db[k].move(i);
                            tmp[i] |= 1 << k;
                        }
                }
                return tmp;
            }
            int n = v.digits.size(), k = (n + 2) / 2;
            UnsignedDigit tmp = quasiInv(std::vector<int>(v.digits.data() + n - k, v.digits.data() + n));
            return (UnsignedDigit(2) * tmp).move(n - k) - (v * tmp * tmp).move(-2 * k);
        }

    } // namespace DivHelper

    UnsignedDigit::UnsignedDigit(ll x) {
        if (x == 0) {
            digits.push_back(0);
        } else {
            while (x > 0) {
                digits.push_back(x % MOD);
                x /= MOD;
            }
        }
    }

    UnsignedDigit UnsignedDigit::move(int k) const {
        if (k == 0) return *this;
        if (isZero()) return UnsignedDigit();

        if (k < 0) {
            if (-k >= (int)digits.size()) return UnsignedDigit();
            return std::vector<int>(digits.begin() - k, digits.end());
        }
        
        UnsignedDigit ret;
        ret.digits.assign(k + digits.size(), 0);
        std::copy(digits.begin(), digits.end(), ret.digits.begin() + k);
        return ret;
    }

    bool UnsignedDigit::operator<(const UnsignedDigit& rhs) const {
        int n = digits.size(), m = rhs.digits.size();
        if (n != m) return n < m;
        for (int i = n - 1; i >= 0; --i)
            if (digits[i] != rhs.digits[i])
                return digits[i] < rhs.digits[i];
        return false;
    }

    bool UnsignedDigit::operator<=(const UnsignedDigit& rhs) const {
        int n = digits.size(), m = rhs.digits.size();
        if (n != m) return n < m;
        for (int i = n - 1; i >= 0; --i)
            if (digits[i] != rhs.digits[i])
                return digits[i] < rhs.digits[i];
        return true;
    }

    bool UnsignedDigit::operator==(const UnsignedDigit& rhs) const {
        return digits == rhs.digits;
    }

    UnsignedDigit UnsignedDigit::operator+(const UnsignedDigit& rhs) const {
        int n = digits.size(), m = rhs.digits.size();
        std::vector<int> tmp(std::max(n, m) + 1, 0);
        const std::vector<int>& a = (n > m) ? digits : rhs.digits;
        const std::vector<int>& b = (n > m) ? rhs.digits : digits;
        int max_len = a.size(), min_len = b.size();

        for (int i = 0; i < min_len; ++i) {
            tmp[i] += a[i] + b[i];
            if (tmp[i] >= MOD) {
                tmp[i] -= MOD;
                tmp[i + 1]++;
            }
        }
        for (int i = min_len; i < max_len; ++i) {
            tmp[i] += a[i];
            if (tmp[i] >= MOD) {
                tmp[i] -= MOD;
                tmp[i + 1]++;
            }
        }
        return tmp;
    }

    UnsignedDigit UnsignedDigit::operator-(const UnsignedDigit& rhs) const {
        UnsignedDigit ret(*this);
        int n = rhs.digits.size();
        for (int i = 0; i < n; ++i) {
            ret.digits[i] -= rhs.digits[i];
            if (ret.digits[i] < 0) {
                ret.digits[i] += MOD;
                ret.digits[i + 1]--;
            }
        }
        for (size_t i = n; i < ret.digits.size() - 1 && ret.digits[i] < 0; ++i) {
            ret.digits[i] += MOD;
            ret.digits[i + 1]--;
        }
        ret.trim();
        return ret;
    }

    UnsignedDigit UnsignedDigit::operator*(const UnsignedDigit& rhs) const {
        std::vector<ll> tmp = ConvHelper::conv(digits, rhs.digits);
        for (size_t i = 0; i + 1 < tmp.size(); ++i) {
            tmp[i + 1] += tmp[i] / MOD;
            tmp[i] %= MOD;
        }
        while (tmp.back() >= MOD) {
            ll remain = tmp.back() / MOD;
            tmp.back() %= MOD;
            tmp.push_back(remain);
        }
        std::vector<int> result(tmp.begin(), tmp.end());
        return result;
    }

    UnsignedDigit UnsignedDigit::operator/(const UnsignedDigit& rhs) const {
        int m = digits.size(), n = rhs.digits.size(), t = 0;
        if (m < n) return 0;
        if (m > n * 2) t = m - 2 * n;
        UnsignedDigit sv = DivHelper::quasiInv(rhs.move(t));
        UnsignedDigit ret = move(t) * sv;
        ret = ret.move(-2 * (n + t));
        if ((ret + 1) * rhs <= *this) {
             ret = ret + 1;
        }
        return ret;
    }

    UnsignedDigit UnsignedDigit::operator/(int k) const {
        UnsignedDigit ret;
        int n = digits.size();
        ret.digits.resize(n);
        ll r = 0;
        for (int i = n - 1; i >= 0; --i) {
            r = r * MOD + digits[i];
            ret.digits[i] = r / k;
            r %= k;
        }
        ret.trim();
        return ret;
    }

    UnsignedDigit::UnsignedDigit(const std::vector<int>& digits) : digits(digits) {
        if (this->digits.empty())
            this->digits.assign(1, 0);
        trim();
    }

    void UnsignedDigit::trim() {
        while (digits.size() > 1 && digits.back() == 0)
            digits.pop_back();
    }

    std::string UnsignedDigit::toString() const {
        std::stringstream ss;
        ss << digits.back();
        for (int i = (int)digits.size() - 2; i >= 0; --i) {
            ss << std::setw(BASE) << std::setfill('0') << digits[i];
        }
        return ss.str();
    }

    UnsignedDigit::UnsignedDigit(std::string str) {
        if (str.empty() || std::any_of(str.begin(), str.end(), [](char c){ return !isdigit(c); })) {
            digits.assign(1, 0);
            return;
        }
        reverse(str.begin(), str.end());
        digits.resize((str.size() + BASE - 1) / BASE, 0);
        int cur = 1;
        for (size_t i = 0; i < str.size(); ++i) {
            if (i > 0 && i % BASE == 0)
                cur = 1;
            digits[i / BASE] += cur * (str[i] - '0');
            cur *= 10;
        }
        trim();
    }

    UnsignedDigit pow(UnsignedDigit x, int k) {
        UnsignedDigit ret = 1;
        while (k) {
            if (k & 1) ret = ret * x;
            if (k >>= 1) x = x * x;
        }
        return ret;
    }

} // namespace BigNumberDetail

// =================================================================================
// END: Integrated implementation
// =================================================================================


class BigNumber {
private:
    BigNumberDetail::UnsignedDigit magnitude;
    bool is_negative;
    int decimal_pos; // Number of digits after the decimal point

    // Helper to get a reference to the static default precision value
    static int& get_default_precision_ref() {
        static int default_precision = 50;
        return default_precision;
    }

    // Normalizes the number: trims leading/trailing zeros and handles "0" case.
    void normalize() {
        magnitude.trim();
        if (magnitude.isZero()) {
            is_negative = false;
            decimal_pos = 0;
        }
    }

    // Compares absolute values: 1 (this > other), -1 (this < other), 0 (this == other)
    int compare_abs(const BigNumber& other) const {
        int this_int_len = magnitude.toString().length() - decimal_pos;
        int other_int_len = other.magnitude.toString().length() - other.decimal_pos;

        if (this_int_len != other_int_len) {
            return this_int_len > other_int_len ? 1 : -1;
        }

        BigNumberDetail::UnsignedDigit a_mag = this->magnitude;
        BigNumberDetail::UnsignedDigit b_mag = other.magnitude;
        
        int diff = this->decimal_pos - other.decimal_pos;
        if (diff > 0) {
            b_mag = b_mag * BigNumberDetail::pow(BigNumberDetail::UnsignedDigit(10), diff);
        } else if (diff < 0) {
            a_mag = a_mag * BigNumberDetail::pow(BigNumberDetail::UnsignedDigit(10), -diff);
        }

        if (a_mag < b_mag) return -1;
        if (b_mag < a_mag) return 1;
        return 0;
    }

public:
    // 1. Construction & Assignment

    BigNumber() : is_negative(false), decimal_pos(0), magnitude(0LL) {}
    BigNumber(long long n) : is_negative(n < 0), decimal_pos(0), magnitude(n < 0 ? -n : n) {}
    BigNumber(std::string s) {
        if (s.empty()) { *this = BigNumber(); return; }
        if (s[0] == '-') { is_negative = true; s.erase(0, 1); } else { is_negative = false; }
        
        std::string s_digits = s;
        size_t dot_pos = s.find('.');
        if (dot_pos == std::string::npos) {
            decimal_pos = 0;
        } else {
            decimal_pos = s.length() - dot_pos - 1;
            s_digits.erase(dot_pos, 1);
        }

        if (s_digits.empty()) { // Handle cases like "." or "-."
            s_digits = "0";
        }
        
        if (std::any_of(s_digits.begin(), s_digits.end(), [](char c){ return !isdigit(c); })) {
            throw std::invalid_argument("Invalid character in number string.");
        }
        magnitude = BigNumberDetail::UnsignedDigit(s_digits);
        normalize();
    }
    // Internal constructor for performance
    BigNumber(BigNumberDetail::UnsignedDigit mag, bool neg, int dec_pos) : magnitude(mag), is_negative(neg), decimal_pos(dec_pos) { normalize(); }

    BigNumber& operator+=(const BigNumber& other) { *this = *this + other; return *this; }
    BigNumber& operator-=(const BigNumber& other) { *this = *this - other; return *this; }
    BigNumber& operator*=(const BigNumber& other) { *this = *this * other; return *this; }
    BigNumber& operator/=(const BigNumber& other) { *this = *this / other; return *this; }

    // 2. Basic Arithmetic Operations

    BigNumber operator+(const BigNumber& other) const {
        if (this->is_negative == other.is_negative) {
            int max_dec = std::max(this->decimal_pos, other.decimal_pos);
            BigNumberDetail::UnsignedDigit a = this->magnitude;
            BigNumberDetail::UnsignedDigit b = other.magnitude;
            
            int a_shift = max_dec - this->decimal_pos;
            int b_shift = max_dec - other.decimal_pos;
            if (a_shift > 0) {
                a = a * BigNumberDetail::pow(BigNumberDetail::UnsignedDigit(10), a_shift);
            }
            if (b_shift > 0) {
                b = b * BigNumberDetail::pow(BigNumberDetail::UnsignedDigit(10), b_shift);
            }

            return BigNumber(a + b, this->is_negative, max_dec);
        }
        if (this->compare_abs(other) >= 0) {
            int max_dec = std::max(this->decimal_pos, other.decimal_pos);
            BigNumberDetail::UnsignedDigit a = this->magnitude;
            BigNumberDetail::UnsignedDigit b = other.magnitude;
            
            int a_shift = max_dec - this->decimal_pos;
            int b_shift = max_dec - other.decimal_pos;
            if (a_shift > 0) {
                a = a * BigNumberDetail::pow(BigNumberDetail::UnsignedDigit(10), a_shift);
            }
            if (b_shift > 0) {
                b = b * BigNumberDetail::pow(BigNumberDetail::UnsignedDigit(10), b_shift);
            }

            return BigNumber(a - b, this->is_negative, max_dec);
        }
        return other + *this;
    }
    BigNumber operator-(const BigNumber& other) const {
        BigNumber inverted_other = other;
        inverted_other.is_negative = !inverted_other.is_negative;
        if(inverted_other.magnitude.isZero()) inverted_other.is_negative = false;
        return *this + inverted_other;
    }
    BigNumber operator*(const BigNumber& other) const {
        BigNumberDetail::UnsignedDigit res_mag = this->magnitude * other.magnitude;
        return BigNumber(res_mag, this->is_negative != other.is_negative, this->decimal_pos + other.decimal_pos);
    }
    BigNumber operator/(const BigNumber& other) const {
        if (other.magnitude.isZero()) throw std::runtime_error("Division by zero.");
        
        bool result_is_negative = this->is_negative != other.is_negative;
        
        int precision = get_default_precision();
        int scale_factor = precision + other.decimal_pos - this->decimal_pos + 5; // +5 for rounding buffer
        
        BigNumberDetail::UnsignedDigit num = this->magnitude;
        if (scale_factor > 0) {
            num = num * BigNumberDetail::pow(BigNumberDetail::UnsignedDigit(10), scale_factor);
        }
        
        BigNumberDetail::UnsignedDigit quotient;
        if (scale_factor < 0) {
            quotient = num / (other.magnitude * BigNumberDetail::pow(BigNumberDetail::UnsignedDigit(10), -scale_factor));
        } else {
            quotient = num / other.magnitude;
        }

        int new_decimal_pos = precision + 5;
        BigNumber result(quotient, result_is_negative, new_decimal_pos);
        return result.approx(precision);
    }
    BigNumber operator%(const BigNumber& other) const {
        if (!this->isInteger() || !other.isInteger()) {
            throw std::runtime_error("Operands for modulo must be integers.");
        }
        if (other.magnitude.isZero()) {
            throw std::runtime_error("Modulo by zero.");
        }
        BigNumber div_res = *this / other;
        BigNumber integer_quotient(div_res.toString().substr(0, div_res.toString().find('.')));
        return *this - (integer_quotient * other);
    }
    
    BigNumber exact_division(const BigNumber& other) const; // to remove the extra zeros.

    // 3. Comparison Operations

    bool operator==(const BigNumber& other) const { return this->is_negative == other.is_negative && this->compare_abs(other) == 0; }
    bool operator!=(const BigNumber& other) const { return !(*this == other); }
    bool operator<(const BigNumber& other) const {
        if (this->is_negative != other.is_negative) return this->is_negative;
        int cmp = this->compare_abs(other);
        return this->is_negative ? cmp > 0 : cmp < 0;
    }
    bool operator>(const BigNumber& other) const { return other < *this; }
    bool operator<=(const BigNumber& other) const { return !(other < *this); }
    bool operator>=(const BigNumber& other) const { return !(*this < other); }

    // 4. Mathematical Functions

    BigNumber operator^(const BigNumber& exp) const {
        if (!exp.isInteger()) {
            throw std::runtime_error("Exponent must be an integer for ^ operator.");
        }
        long long e_val = exp.toLongLong();
        if (e_val == 0) return BigNumber(1);
        if (this->magnitude.isZero()) return BigNumber(0);
        
        bool exp_is_neg = e_val < 0;
        if (exp_is_neg) e_val = -e_val;
        
        BigNumberDetail::UnsignedDigit res_mag = BigNumberDetail::pow(this->magnitude, e_val);
        int final_decimal_pos = this->decimal_pos * e_val;
        bool final_is_negative = this->is_negative && (e_val % 2 != 0);
        
        BigNumber result(res_mag, final_is_negative, final_decimal_pos);
        if (exp_is_neg) return BigNumber(1) / result;
        return result;
    }
	 
    BigNumber abs() const { BigNumber res = *this; res.is_negative = false; return res; }
    
    // N-th root using Newton's method. A negative precision value uses the default.
    static BigNumber root(const BigNumber& num, const BigNumber& n_big, int precision = -1) {
        if (precision < 0) precision = get_default_precision();
        long m = n_big.toLongLong();
        if (m <= 0) throw std::runtime_error("Root must be a positive integer.");
        if (num.is_negative && m % 2 == 0) throw std::runtime_error("Even root of a negative number is not real.");
        if (num.magnitude.isZero()) return BigNumber(0);

        BigNumberDetail::UnsignedDigit n_val = num.magnitude;
        int current_dec_pos = num.decimal_pos;
        int calc_precision = precision + 5; // Use higher internal precision
        
        // Scale the number to treat it as a large integer
        int scale_factor = calc_precision * m - current_dec_pos;
        if (scale_factor > 0) {
             // ================= FIX START =================
             // Correctly scope UnsignedDigit with its namespace
             n_val = n_val * BigNumberDetail::pow(BigNumberDetail::UnsignedDigit(10), scale_factor);
             // ================= FIX END ===================
        }

        // Improved initial guess
        std::string n_str = n_val.toString();
        // ================= FIX START =================
        // Correctly scope UnsignedDigit with its namespace
        BigNumberDetail::UnsignedDigit x = BigNumberDetail::pow(BigNumberDetail::UnsignedDigit(10), (n_str.length() + m -1) / m);
        // ================= FIX END ===================

        BigNumberDetail::UnsignedDigit xx = (x * (m - 1) + n_val / BigNumberDetail::pow(x, m - 1)) / m;
        while (xx < x) {
            std::swap(x, xx);
            xx = (x * (m - 1) + n_val / BigNumberDetail::pow(x, m - 1)) / m;
        }

        BigNumber result(x, num.is_negative, calc_precision);
        return result.approx(precision);
    }
    
    // 5. Precision and Conversion

    static void set_default_precision(int p) {
        if (p < 0) throw std::invalid_argument("Precision cannot be negative.");
        get_default_precision_ref() = p;
    }
    static int get_default_precision() { return get_default_precision_ref(); }

    BigNumber approx(int precision) const {
        if (precision < 0) throw std::invalid_argument("Precision cannot be negative.");
        if (decimal_pos <= precision) return *this;

        std::string s = magnitude.toString();
        int digits_to_cut = decimal_pos - precision;
        if (digits_to_cut >= (int)s.length()) return BigNumber(0);

        char round_digit = s[s.length() - digits_to_cut];
        std::string new_digits_str = s.substr(0, s.length() - digits_to_cut);
        
        BigNumber result(BigNumberDetail::UnsignedDigit(new_digits_str), is_negative, precision);
        
        if (round_digit >= '5') {
            BigNumber adder(BigNumberDetail::UnsignedDigit(1), false, precision);
            result = is_negative ? (result - adder) : (result + adder);
        }
        return result;
    }

    std::string toString() const {
        if (magnitude.isZero()) return "0";
        std::string s = magnitude.toString();
        if (decimal_pos > 0) {
            if ((int)s.length() <= decimal_pos) {
                s.insert(0, decimal_pos - s.length(), '0');
                s.insert(0, "0.");
            } else {
                s.insert(s.length() - decimal_pos, ".");
            }
        }
        if (is_negative) {
            s.insert(0, "-");
        }
        return s;
    }
    
    long long toLongLong() const {
        std::string s = this->toString();
        size_t dot = s.find('.');
        if(dot != std::string::npos) {
            s = s.substr(0, dot);
        }
        if (s.empty() || (s.length() == 1 && s[0] == '-')) return 0;
        try {
            return std::stoll(s);
        } catch(const std::out_of_range&) {
            throw std::runtime_error("BigNumber too large to fit in long long.");
        }
    }
    double toDouble() const {
        try {
            return std::stod(this->toString());
        } catch (const std::out_of_range&) {
            throw std::runtime_error("BigNumber value is out of range for a double.");
        }
    }

    // 6. State Checks
    bool isNegative() const { return is_negative; }
    bool isInteger() const {
        std::string mag_str = magnitude.toString();
        if(decimal_pos == 0) return true;
        if(decimal_pos >= (int)mag_str.length()) return magnitude.isZero();

        for(int i = 0; i < decimal_pos; ++i){
            if(mag_str[mag_str.length() - 1 - i] != '0') return false;
        }
        return true;
    }
};

BigNumber BigNumber::exact_division(const BigNumber& other) const {
    if (other.magnitude.isZero()) {
        throw std::runtime_error("Division by zero.");
    }
    BigNumber quotient = *this / other;
    if (quotient.decimal_pos == 0) {
        return quotient;
    }
    std::string mag_str = quotient.magnitude.toString();
    if (mag_str.length() <= (size_t)quotient.decimal_pos) {
        return BigNumber(0);
    }
    std::string integer_part_str = mag_str.substr(0, mag_str.length() - quotient.decimal_pos);
    return BigNumber(BigNumberDetail::UnsignedDigit(integer_part_str), quotient.is_negative, 0);
}


#endif // BIG_INT_HPP
