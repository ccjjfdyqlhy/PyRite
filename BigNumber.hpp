#ifndef BIG_INT_HPP
#define BIG_INT_HPP

#include <string>
#include <vector>
#include <stdexcept>
#include <algorithm>
#include <cctype>
#include <iostream>
#include <cmath>
#include <cstdint>      // For uint32_t and uint64_t
#include <iomanip>      // For std::setw and std::setfill
#include <sstream>      // For std::stringstream
#include <complex>      // For std::complex
#include <cstring>      // For memcmp
#include <cstdio>       // For sprintf

// Internal namespace for high-performance arithmetic using FFT
namespace FastMath {

class UnsignedDigit; // Forward declaration

typedef std::complex<double> cd;
const int BASE = 5, MOD = 100000, LGM = 17;
const double PI = 3.1415926535897932384626;

// Forward declaration for friend access
namespace DivHelper { UnsignedDigit quasiInv(const UnsignedDigit& v); }

class UnsignedDigit {
public:
	std::vector<int> digits;

public:
	UnsignedDigit() : digits(1, 0) {}

	UnsignedDigit(const std::vector<int>& digits);
	UnsignedDigit(long long x);
	UnsignedDigit(std::string str);

	std::string toString() const;
	int size() const { return digits.size(); }
	void trim();

	bool operator<(const UnsignedDigit& rhs) const;
	bool operator<=(const UnsignedDigit& rhs) const;
	bool operator==(const UnsignedDigit& rhs) const;

	UnsignedDigit operator+(const UnsignedDigit& rhs) const;
	UnsignedDigit operator-(const UnsignedDigit& rhs) const;
	UnsignedDigit operator*(const UnsignedDigit& rhs) const;
	UnsignedDigit operator/(const UnsignedDigit& rhs) const;
	UnsignedDigit operator/(int v) const;

	UnsignedDigit move(int k) const;

	friend UnsignedDigit DivHelper::quasiInv(const UnsignedDigit& v);
	friend void swap(UnsignedDigit& lhs, UnsignedDigit& rhs) { swap(lhs.digits, rhs.digits); }
};

namespace ConvHelper {
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
				swap(a[brev[i]], a[i]);
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

	std::vector<long long> conv(const std::vector<int>& a, const std::vector<int>& b) {
		int n = a.size() - 1, m = b.size() - 1;
		if (n < 1000 / (m + 1) || n < 10 || m < 10) {
			std::vector<long long> ret(n + m + 1);
			for (int i = 0; i <= n; ++i)
				for (int j = 0; j <= m; ++j)
					ret[i + j] += (long long)a[i] * b[j];
			return ret;
		}
		int lgn = 0;
		while ((1 << lgn) <= n + m) ++lgn;
		std::vector<cd> ta(a.begin(), a.end()), tb(b.begin(), b.end());
		ta.resize(1 << lgn);
		tb.resize(1 << lgn);
		fft(&ta[0], lgn, 1);
		fft(&tb[0], lgn, 1);
		for (int i = 0; i < (1 << lgn); ++i)
			ta[i] *= tb[i];
		fft(&ta[0], lgn, -1);
		std::vector<long long> ret(n + m + 1);
		for (int i = 0; i <= n + m; ++i)
			ret[i] = ta[i].real() + 0.5;
		return ret;
	}
}

namespace DivHelper {
	UnsignedDigit quasiInv(const UnsignedDigit& v) {
		if (v.digits.size() == 1) {
			UnsignedDigit tmp;
			tmp.digits.assign(3, 0);
			tmp.digits[2] = 1;
			return tmp / v.digits[0];
		}
		int n = v.digits.size(), k = (n + 1) / 2;
        UnsignedDigit v_sub(std::vector<int>(v.digits.begin() + (n - k), v.digits.end()));
		UnsignedDigit tmp = quasiInv(v_sub);
		UnsignedDigit term1 = (UnsignedDigit(2) * tmp).move(n - k);
        UnsignedDigit term2_mult = v * tmp;
        UnsignedDigit term2_mult_sq = term2_mult * tmp;
        UnsignedDigit term2 = term2_mult_sq.move(-2 * k);

        if (term1 < term2) {
             return UnsignedDigit(0); 
        }
		return term1 - term2;
	}
}

void UnsignedDigit::trim() {
	while (digits.size() > 1 && digits.back() == 0)
		digits.pop_back();
}

UnsignedDigit::UnsignedDigit(const std::vector<int>& d) : digits(d) {
	if (this->digits.empty())
		this->digits.assign(1, 0);
	trim();
}

UnsignedDigit::UnsignedDigit(long long x) {
    if (x == 0) {
        digits.push_back(0);
        return;
    }
	while (x > 0) {
		digits.push_back(x % MOD);
		x /= MOD;
	}
	if (digits.empty())
		digits.push_back(0);
}

UnsignedDigit::UnsignedDigit(std::string str) {
	if (str.empty() || str == "0") {
        digits.push_back(0);
        return;
    }
    std::reverse(str.begin(), str.end());
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

std::string UnsignedDigit::toString() const {
    if (digits.empty() || (digits.size() == 1 && digits[0] == 0)) return "0";
    std::stringstream ss;
    ss << digits.back();
    for (int i = (int)digits.size() - 2; i >= 0; --i) {
        ss << std::setw(BASE) << std::setfill('0') << digits[i];
    }
    return ss.str();
}

UnsignedDigit UnsignedDigit::move(int k) const {
	if (k == 0) return *this;
	if (k < 0) {
		if (-k >= (int)digits.size()) return UnsignedDigit();
		return std::vector<int>(digits.begin() + (-k), digits.end());
	}
	if (digits.size() == 1 && digits[0] == 0) return UnsignedDigit();
	UnsignedDigit ret;
	ret.digits.assign(k, 0);
	ret.digits.insert(ret.digits.end(), digits.begin(), digits.end());
	return ret;
}

bool UnsignedDigit::operator<(const UnsignedDigit& rhs) const {
	int n = digits.size(), m = rhs.digits.size();
	if (n != m) return n < m;
	for (int i = n - 1; i >= 0; --i)
		if (digits[i] != rhs.digits[i]) return digits[i] < rhs.digits[i];
	return false;
}

bool UnsignedDigit::operator<=(const UnsignedDigit& rhs) const {
	int n = digits.size(), m = rhs.digits.size();
	if (n != m) return n < m;
	for (int i = n - 1; i >= 0; --i)
		if (digits[i] != rhs.digits[i]) return digits[i] < rhs.digits[i];
	return true;
}

bool UnsignedDigit::operator==(const UnsignedDigit& rhs) const {
	return digits == rhs.digits;
}

UnsignedDigit UnsignedDigit::operator+(const UnsignedDigit& rhs) const {
	int n = digits.size(), m = rhs.digits.size();
	std::vector<int> tmp_digits = digits;
    tmp_digits.resize(std::max(n, m) + 1, 0);

    for (size_t i = 0; i < rhs.digits.size(); ++i) {
        tmp_digits[i] += rhs.digits[i];
    }

    for (size_t i = 0; i + 1 < tmp_digits.size(); ++i) {
        if (tmp_digits[i] >= MOD) {
            tmp_digits[i + 1] += tmp_digits[i] / MOD;
            tmp_digits[i] %= MOD;
        }
    }
	return UnsignedDigit(tmp_digits);
}

UnsignedDigit UnsignedDigit::operator-(const UnsignedDigit& rhs) const {
	UnsignedDigit ret(*this);
	for (size_t i = 0; i < rhs.digits.size(); ++i) {
        ret.digits[i] -= rhs.digits[i];
    }
    for(size_t i = 0; i < ret.digits.size() - 1; ++i) {
        if (ret.digits[i] < 0) {
            ret.digits[i] += MOD;
            ret.digits[i + 1]--;
        }
    }
	ret.trim();
	return ret;
}

UnsignedDigit UnsignedDigit::operator*(const UnsignedDigit& rhs) const {
	std::vector<long long> tmp = ConvHelper::conv(digits, rhs.digits);
	for (size_t i = 0; i + 1 < tmp.size(); ++i) {
		tmp[i + 1] += tmp[i] / MOD;
		tmp[i] %= MOD;
	}
	while (tmp.back() >= MOD) {
		long long remain = tmp.back() / MOD;
		tmp.back() %= MOD;
		tmp.push_back(remain);
	}
    std::vector<int> result_digits;
    result_digits.reserve(tmp.size());
    for(long long val : tmp) {
        result_digits.push_back(static_cast<int>(val));
    }
	return UnsignedDigit(result_digits);
}

UnsignedDigit UnsignedDigit::operator/(const UnsignedDigit& rhs) const {
    if (rhs.size() == 1 && rhs.digits[0] == 0) throw std::runtime_error("FastMath::UnsignedDigit division by zero.");
	int m = digits.size(), n = rhs.digits.size();
	if (m < n) return UnsignedDigit(0);
    
    UnsignedDigit a = *this;
    UnsignedDigit b = rhs;

    int prec_shift = std::max(0, m - n + 5);
    a = a.move(prec_shift);
    UnsignedDigit b_inv = DivHelper::quasiInv(b);
    
    UnsignedDigit res = a * b_inv;
    res = res.move(-(n + prec_shift));

    UnsignedDigit check = res * rhs;
    if (*this < check) {
        res = res - UnsignedDigit(1);
    } else {
        check = (res + UnsignedDigit(1)) * rhs;
        if (check <= *this) {
            res = res + UnsignedDigit(1);
        }
    }
	return res;
}

UnsignedDigit UnsignedDigit::operator/(int k) const {
    if (k == 0) throw std::runtime_error("FastMath::UnsignedDigit division by zero.");
	UnsignedDigit ret;
	int n = digits.size();
	ret.digits.resize(n);
	long long r = 0;
	for (int i = n - 1; i >= 0; --i) {
		r = r * MOD + digits[i];
		ret.digits[i] = r / k;
		r %= k;
	}
	ret.trim();
	return ret;
}

UnsignedDigit pow(UnsignedDigit x, long long k) {
	UnsignedDigit ret = 1;
	while (k > 0) {
		if (k & 1) ret = ret * x;
		if (k >>= 1) x = x * x;
	}
	return ret;
}

} // namespace FastMath

class BigNumber {
private:
    std::string digits;
    bool is_negative;
    int decimal_pos;

    void normalize() {
        if (digits.empty() || !std::all_of(digits.begin(), digits.end(), ::isdigit)) {
            digits = "0"; is_negative = false; decimal_pos = 0; return;
        }
        if (decimal_pos > 0) {
            size_t last_digit = digits.find_last_not_of('0');
            if (last_digit == std::string::npos) { digits = "0"; decimal_pos = 0; }
            else { int to_remove = digits.length() - 1 - last_digit; if (to_remove > decimal_pos) to_remove = decimal_pos; digits.erase(last_digit + 1); decimal_pos -= to_remove; if (decimal_pos < 0) decimal_pos = 0; }
        }
        if (digits.length() > (size_t)decimal_pos) {
            size_t first_digit = digits.find_first_not_of('0');
            if (first_digit == std::string::npos || (int)first_digit >= (int)digits.length() - decimal_pos) {
                if (decimal_pos > 0) {
                    digits.erase(0, digits.length() - decimal_pos);
                    if (digits.empty()) digits = "0";
                } else {
                    digits = "0";
                }
            } else {
                digits.erase(0, first_digit);
            }
        }
        if (digits == "0" || digits.empty()) { is_negative = false; decimal_pos = 0; digits = "0"; }
    }
    
    int compare_abs(const BigNumber& other) const {
        BigNumber a = *this, b = other;
        int max_dec = std::max(a.decimal_pos, b.decimal_pos);
        if (a.decimal_pos < max_dec) a.digits.append(max_dec - a.decimal_pos, '0');
        if (b.decimal_pos < max_dec) b.digits.append(max_dec - b.decimal_pos, '0');

        int len1 = a.digits.length();
        int len2 = b.digits.length();

        if (len1 != len2) return len1 > len2 ? 1 : -1;
        if (a.digits > b.digits) return 1;
        if (a.digits < b.digits) return -1;
        return 0;
    }
    
    static BigNumber add_abs(BigNumber a, BigNumber b) {
        int max_dec = std::max(a.decimal_pos, b.decimal_pos);
        if (a.decimal_pos < max_dec) a.digits.append(max_dec - a.decimal_pos, '0');
        if (b.decimal_pos < max_dec) b.digits.append(max_dec - b.decimal_pos, '0');
        std::string res = ""; int carry = 0; int i = a.digits.length() - 1, j = b.digits.length() - 1;
        while (i >= 0 || j >= 0 || carry) { int sum = carry; if (i >= 0) sum += a.digits[i--] - '0'; if (j >= 0) sum += b.digits[j--] - '0'; res += std::to_string(sum % 10); carry = sum / 10; }
        std::reverse(res.begin(), res.end());
        return BigNumber(res, false, max_dec);
    }
    
    static BigNumber subtract_abs(BigNumber a, BigNumber b) {
        int max_dec = std::max(a.decimal_pos, b.decimal_pos);
        if (a.decimal_pos < max_dec) a.digits.append(max_dec - a.decimal_pos, '0');
        if (b.decimal_pos < max_dec) b.digits.append(max_dec - b.decimal_pos, '0');
        std::string res = ""; int borrow = 0; int i = a.digits.length() - 1, j = b.digits.length() - 1;
        while (i >= 0) { int diff = (a.digits[i] - '0') - borrow; if (j >= 0) diff -= (b.digits[j] - '0'); if (diff < 0) { diff += 10; borrow = 1; } else { borrow = 0; } res += std::to_string(diff); i--; j--; }
        std::reverse(res.begin(), res.end());
        return BigNumber(res, false, max_dec);
    }
    
    FastMath::UnsignedDigit toUnsignedDigit() const {
        if (this->decimal_pos > 0) {
            throw std::runtime_error("toUnsignedDigit called on non-integer BigNumber");
        }
        return FastMath::UnsignedDigit(this->digits);
    }
    
    static BigNumber fromUnsignedDigit(const FastMath::UnsignedDigit& ud) {
        return BigNumber(ud.toString());
    }

public:
    BigNumber() : is_negative(false), decimal_pos(0), digits("0") {}
    BigNumber(long long n) : is_negative(n < 0), decimal_pos(0), digits(std::to_string(n == 0 ? 0 : (n < 0 ? -n : n))) {}
    BigNumber(const std::string& s, bool neg, int dec_pos) : digits(s), is_negative(neg), decimal_pos(dec_pos) { normalize(); }
    BigNumber(std::string s) {
        if (s.empty()) { is_negative = false; decimal_pos = 0; digits = "0"; return; }
        if (s[0] == '-') { is_negative = true; s.erase(0, 1); } else { is_negative = false; }
        for (char c : s) { if (!isdigit(c) && c != '.') throw std::invalid_argument("Invalid character in number string."); }
        size_t dot_pos = s.find('.');
        if (dot_pos == std::string::npos) { decimal_pos = 0; digits = s; } 
        else { decimal_pos = s.length() - dot_pos - 1; s.erase(dot_pos, 1); digits = s; }
        if (digits.empty() || digits == ".") digits = "0";
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
        if (int_part.empty()) return 0;
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
        int dec_shift = std::max(a.decimal_pos, b.decimal_pos);
        if (a.decimal_pos < dec_shift) a.digits.append(dec_shift - a.decimal_pos, '0');
        if (b.decimal_pos < dec_shift) b.digits.append(dec_shift - b.decimal_pos, '0');
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
        if (!exp.isInteger()) throw std::runtime_error("Exponent must be an integer for ^ operator.");
        long long e_val = exp.toLongLong();
        
        if (e_val == 0) return BigNumber(1);
        if (*this == BigNumber("0")) return BigNumber("0");
        
        bool exp_is_neg = e_val < 0;
        if (exp_is_neg) e_val = -e_val;
        
        BigNumber base_abs = this->abs();
        FastMath::UnsignedDigit base_ud(base_abs.digits);
        FastMath::UnsignedDigit res_ud = FastMath::pow(base_ud, e_val);
        
        BigNumber result = fromUnsignedDigit(res_ud);
        result.decimal_pos = this->decimal_pos * e_val;
        result.is_negative = this->is_negative && (exp.toLongLong() % 2 != 0);
        
        if (exp_is_neg) {
            return BigNumber(1) / result;
        }
        
        result.normalize();
        return result;
    }
	 
    BigNumber abs() const { BigNumber res = *this; res.is_negative = false; return res; }
    
    static BigNumber root(const BigNumber& num, const BigNumber& n, int precision = 50) {
        if (num.is_negative && n.toLongLong() % 2 == 0) throw std::runtime_error("Even root of a negative number is not real.");
        if (n <= BigNumber(0)) throw std::runtime_error("Root must be a positive integer.");
        if (num == BigNumber(0)) return BigNumber(0);
        long long m = n.toLongLong();
        const int guard_digits = 5;

        BigNumber scaled_num = num.abs();
        int scale_factor = m * (precision + guard_digits);
        if (scaled_num.decimal_pos < scale_factor) {
            scaled_num.digits.append(scale_factor - scaled_num.decimal_pos, '0');
        } else {
            scaled_num.digits.erase(scaled_num.digits.length() - (scaled_num.decimal_pos - scale_factor));
        }
        scaled_num.decimal_pos = 0;
        scaled_num.normalize();

        FastMath::UnsignedDigit n_ud = scaled_num.toUnsignedDigit();
        
        int root_num_blocks = (n_ud.size() + m - 1) / m;
        FastMath::UnsignedDigit x;
        x.digits.assign(root_num_blocks, 0);
        int top_digit_idx = root_num_blocks - 1;
        
        int l = 1, r = FastMath::MOD;
        while (l < r) {
            int mid = l + (r - l) / 2;
            x.digits[top_digit_idx] = mid;
            if (FastMath::pow(x, m) <= n_ud) l = mid + 1;
            else r = mid;
        }
        x.digits[top_digit_idx] = l - 1;
        x.trim();
        
        FastMath::UnsignedDigit m_ud(m), m_minus_1_ud(m - 1), xx;
        for (int i=0; i < 100; ++i) {
            FastMath::UnsignedDigit x_pow = FastMath::pow(x, m - 1);
            if (x_pow.digits.size() == 1 && x_pow.digits[0] == 0) break;
            xx = (x * m_minus_1_ud + n_ud / x_pow) / m_ud;
            if (x <= xx) break;
            x = xx;
        }
        
        // --- CORRECTED FINALIZATION LOGIC ---
        std::string root_digits = x.toString();
        int total_decimal_places = precision + guard_digits;

        // Create a temporary BigNumber with full precision to correctly place the decimal point.
        BigNumber full_precision_root(root_digits, num.is_negative, total_decimal_places);
        
        // Convert to string and truncate to the final desired precision.
        std::string s = full_precision_root.toString();
        size_t dot_pos = s.find('.');
        if (dot_pos != std::string::npos) {
            if (s.length() - dot_pos - 1 > (size_t)precision) {
                s = s.substr(0, dot_pos + 1 + precision);
            }
        }
        
        // Create the final result from the correctly truncated string.
        return BigNumber(s);
    }

    bool operator==(const BigNumber& other) const { return this->is_negative == other.is_negative && this->compare_abs(other) == 0; }
    bool operator!=(const BigNumber& other) const { return !(*this == other); }
    bool operator<(const BigNumber& other) const { if (this->is_negative != other.is_negative) return this->is_negative; return this->is_negative ? this->compare_abs(other) > 0 : this->compare_abs(other) < 0; }
    bool operator>(const BigNumber& other) const { return other < *this; }
    bool operator<=(const BigNumber& other) const { return !(other < *this); }
    bool operator>=(const BigNumber& other) const { return !(*this < other); }
};

#endif // BIG_INT_HPP
