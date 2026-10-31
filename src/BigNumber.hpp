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
#include <cstring>
#include <climits>
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

    // A vector of ints with a four-slot inline buffer. Nearly every
    // BigNumber in a running script keeps one to three limbs; without the
    // buffer each of them pays a heap round trip per operation. The
    // allocation itself goes through the global allocator, so the
    // interpreter's small-object pool serves the overflow case.
    class SmallVecInt {
    public:
        SmallVecInt() : size_(0), data_(inline_), cap_(4) {}
        SmallVecInt(const SmallVecInt& o) : SmallVecInt() { assign_from(o.data_, o.size_); }
        SmallVecInt(SmallVecInt&& o) noexcept : SmallVecInt() {
            if (o.data_ == o.inline_) { assign_from(o.data_, o.size_); }
            else { data_ = o.data_; size_ = o.size_; cap_ = o.cap_; o.data_ = o.inline_; o.size_ = 0; o.cap_ = 4; }
        }
        SmallVecInt& operator=(const SmallVecInt& o) {
            if (this != &o) assign_from(o.data_, o.size_);
            return *this;
        }
        SmallVecInt& operator=(SmallVecInt&& o) noexcept {
            if (this != &o) {
                if (data_ != inline_) std::free(data_);
                if (o.data_ == o.inline_) { data_ = inline_; cap_ = 4; assign_from(o.data_, o.size_); }
                else { data_ = o.data_; size_ = o.size_; cap_ = o.cap_; o.data_ = o.inline_; o.size_ = 0; o.cap_ = 4; }
            }
            return *this;
        }
        ~SmallVecInt() { if (data_ != inline_) std::free(data_); }

        explicit SmallVecInt(const std::vector<int>& v) : SmallVecInt() { assign_from(v.data(), v.size()); }

        // prepend k copies of v (used by limb shifting)
        void insert_front(size_t k, int v) {
            if (k == 0) return;
            size_t n = size_ + k;
            if (n > cap_) grow(n);
            std::memmove(data_ + k, data_, size_ * sizeof(int));
            for (size_t i = 0; i < k; ++i) data_[i] = v;
            size_ = n;
        }
        int* begin() { return data_; }
        int* end() { return data_ + size_; }
        const int* begin() const { return data_; }
        const int* end() const { return data_ + size_; }
        size_t size() const { return size_; }
        bool empty() const { return size_ == 0; }
        int& operator[](size_t i) { return data_[i]; }
        const int& operator[](size_t i) const { return data_[i]; }
        int& back() { return data_[size_ - 1]; }
        const int& back() const { return data_[size_ - 1]; }
        int* data() { return data_; }
        const int* data() const { return data_; }

        void push_back(int v) {
            if (size_ == cap_) grow(size_ + 1);
            data_[size_++] = v;
        }
        void pop_back() { size_--; }
        void resize(size_t n, int fill = 0) {
            if (n > cap_) grow(n);
            for (size_t i = size_; i < n; ++i) data_[i] = fill;
            size_ = n;
        }
        void assign(size_t n, int v) {
            if (n > cap_) grow(n);
            for (size_t i = 0; i < n; ++i) data_[i] = v;
            size_ = n;
        }
        void assign_from(const int* src, size_t n) {
            if (n > cap_) grow(n);
            std::memcpy(data_, src, n * sizeof(int));
            size_ = n;
        }
        bool operator==(const SmallVecInt& o) const {
            if (size_ != o.size_) return false;
            return std::memcmp(data_, o.data_, size_ * sizeof(int)) == 0;
        }
        void swap(SmallVecInt& o) {
            if (data_ != inline_ && o.data_ != o.inline_) {
                std::swap(data_, o.data_); std::swap(size_, o.size_); std::swap(cap_, o.cap_);
                return;
            }
            SmallVecInt tmp(*this); *this = o; o = tmp;
        }

    private:
        void grow(size_t need) {
            size_t ncap = cap_ * 2;
            while (ncap < need) ncap *= 2;
            int* nd = static_cast<int*>(std::malloc(ncap * sizeof(int)));
            if (!nd) throw std::bad_alloc();
            if (size_) std::memcpy(nd, data_, size_ * sizeof(int));
            if (data_ != inline_) std::free(data_);
            data_ = nd; cap_ = ncap;
        }
        int inline_[4];
        size_t size_;
        int* data_;
        size_t cap_;
    };

    class UnsignedDigit;

    namespace DivHelper { UnsignedDigit quasiInv(const UnsignedDigit& v); }

    // Represents a large unsigned integer using a vector of digits in a given base (MOD).
    class UnsignedDigit {
    public:
        SmallVecInt digits;

    public:
        UnsignedDigit() { digits.assign(1, 0); }
        UnsignedDigit(const std::vector<int>& digits);
        explicit UnsignedDigit(SmallVecInt&& v) : digits(std::move(v)) {}
        UnsignedDigit(ll x);
        UnsignedDigit(std::string str);

        std::string toString() const;
        int size() const { return digits.size(); }
        bool isZero() const { return digits.size() == 1 && digits[0] == 0; }
        int decimalDigitCount() const {
            if (isZero()) return 0;
            int count = (digits.size() - 1) * BASE;
            int top = digits.back();
            if (top == 0) return count;
            while (top > 0) { count++; top /= 10; }
            return count;
        }

        bool operator<(const UnsignedDigit& rhs) const;
        bool operator<=(const UnsignedDigit& rhs) const;
        bool operator==(const UnsignedDigit& rhs) const;

        UnsignedDigit operator+(const UnsignedDigit& rhs) const;
        UnsignedDigit operator-(const UnsignedDigit& rhs) const;
        UnsignedDigit operator*(const UnsignedDigit& rhs) const;
        UnsignedDigit square() const;
        UnsignedDigit operator/(const UnsignedDigit& rhs) const;
        UnsignedDigit operator/(int v) const;

        // Equivalent to multiplication by MOD^k
        UnsignedDigit move(int k) const;

        friend UnsignedDigit DivHelper::quasiInv(const UnsignedDigit& v);
        friend void swap(UnsignedDigit& lhs, UnsignedDigit& rhs) { std::swap(lhs.digits, rhs.digits); }

    public:
        void trim();
    };

    namespace ConvHelper { // Exact NTT convolution for fast multiplication

        // The old core ran a complex<double> FFT over base-1e5 limbs. A
        // double carries 53 significant bits, so once the operands grew
        // past a few tens of thousands of digits the convolution
        // coefficients left that budget and the low digits of the product
        // came back silently wrong (2^100000000 even ended in "570").
        //
        // This core multiplies in the "Goldilocks" prime field instead:
        // p = 2^64 - 2^32 + 1. It is prime, 2^32 divides p-1 (transform
        // sizes up to 2^32) and 7 is a primitive root. Limbs are < 1e5, so
        // every convolution coefficient is below n * (1e5)^2, which stays
        // far under p for any realistic size - the result is exact, no
        // rounding anywhere.

        const uint64_t NT_P    = 18446744069414584321ULL; // 2^64 - 2^32 + 1
        const uint64_t NT_EPS  = 0xFFFFFFFFULL;           // 2^64 - p
        const uint64_t NT_ROOT = 7ULL;                    // primitive root mod p

        static inline uint64_t nt_pow(uint64_t base, uint64_t e) {
            uint64_t r = 1;
            while (e) {
                if (e & 1) { unsigned __int128 t = (unsigned __int128)r * base; r = (uint64_t)(t % NT_P); }
                unsigned __int128 t = (unsigned __int128)base * base;
                base = (uint64_t)(t % NT_P);
                e >>= 1;
            }
            return r;
        }

        static inline uint64_t nt_add(uint64_t a, uint64_t b) {
            uint64_t s = a + b;               // a, b < p, so at most one wrap
            if (s < a) s += NT_EPS;           // 2^64 ≡ NT_EPS (mod p); s >= p here, no second wrap
            else if (s >= NT_P) s -= NT_P;
            return s;
        }

        static inline uint64_t nt_sub(uint64_t a, uint64_t b) {
            uint64_t d = a - b;               // mod 2^64
            if (a < b) d -= NT_EPS;           // a - b + 2^64 - (2^64 - p) = a - b + p
            return d;
        }

        static inline uint64_t nt_mul(uint64_t a, uint64_t b) {
            unsigned __int128 x = (unsigned __int128)a * b;
            uint64_t lo = (uint64_t)x;
            uint64_t hi = (uint64_t)(x >> 64);
            // x = hi * 2^64 + lo ≡ lo - hi + hi * 2^32  (mod p)
            //   = lo - hi_hi + hi_lo * NT_EPS         (mod p)
            // The signed value sits in (-2^32, 2^65), so folding is a
            // couple of conditional steps, no 128-bit division.
            __int128 M = (__int128)lo - (__int128)(hi >> 32)
                       + (__int128)((unsigned __int128)(hi & NT_EPS) * NT_EPS);
            if (M >= (__int128)NT_P) M -= NT_P;
            if (M >= (__int128)NT_P) M -= NT_P;
            if (M < 0) M += NT_P;
            return (uint64_t)M;
        }

        inline void ntt(uint64_t* a, int lgn, int d) {
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

            // Twiddles are precomputed per level: a running product inside
            // the butterfly loop serializes on the modulus reduction, which
            // costs far more than the extra loads.
            static std::vector<uint64_t> tw;
            tw.resize(n / 2 > 0 ? (size_t)n / 2 : 1);
            uint64_t w_n = nt_pow(NT_ROOT, (NT_P - 1) / n);
            if (d == -1) w_n = nt_pow(w_n, NT_P - 2);
            for (int t = 1; t < n; t <<= 1) {
                uint64_t omega = nt_pow(w_n, n / (t << 1));
                tw[0] = 1;
                for (int j = 1; j < t; ++j) tw[j] = nt_mul(tw[j - 1], omega);
                for (int i = 0; i < n; i += t << 1) {
                    uint64_t* p = a + i;
                    for (int j = 0; j < t; ++j) {
                        uint64_t x = nt_mul(p[j + t], tw[j]);
                        p[j + t] = nt_sub(p[j], x);
                        p[j] = nt_add(p[j], x);
                    }
                }
            }
            if (d == -1) {
                uint64_t n_inv = nt_pow(n, NT_P - 2);
                for (int i = 0; i < n; ++i) a[i] = nt_mul(a[i], n_inv);
            }
        }

        // coefficients of a*b (or a*a when square is set - one forward
        // transform fewer, which is the dominant cost of exponentiation)
        inline std::vector<ll> conv(const SmallVecInt& a, const SmallVecInt& b, bool square = false) {
            int n = a.size() - 1, m = b.size() - 1;
            if (n < 1000 / (m + 1) || n < 10 || m < 10) {
                std::vector<ll> ret(n + m + 1);
                if (square) {
                    for (int i = 0; i <= n; ++i) {
                        ret[2 * i] += (ll)a[i] * a[i];
                        for (int j = i + 1; j <= n; ++j)
                            ret[i + j] += 2 * ((ll)a[i] * a[j]);
                    }
                } else {
                    for (int i = 0; i <= n; ++i)
                        for (int j = 0; j <= m; ++j)
                            ret[i + j] += a[i] * (ll)b[j];
                }
                return ret;
            }
            int lgn = 0;
            while ((1 << lgn) <= n + m)
                ++lgn;
            size_t len = (size_t)1 << lgn;
            static std::vector<uint64_t> ta, tb;   // reused across calls; single-threaded runtime
            ta.assign(len, 0);
            for (int i = 0; i <= n; ++i) ta[i] = (uint64_t)a[i];
            ntt(ta.data(), lgn, 1);
            if (square) {
                for (size_t i = 0; i < len; ++i) ta[i] = nt_mul(ta[i], ta[i]);
            } else {
                tb.assign(len, 0);
                for (int i = 0; i <= m; ++i) tb[i] = (uint64_t)b[i];
                ntt(tb.data(), lgn, 1);
                for (size_t i = 0; i < len; ++i) ta[i] = nt_mul(ta[i], tb[i]);
            }
            ntt(ta.data(), lgn, -1);
            // True coefficients are < n * (1e5)^2 < 2^63, so the narrowing
            // to ll loses nothing.
            std::vector<ll> ret(n + m + 1);
            for (int i = 0; i <= n + m; ++i)
                ret[i] = (ll)ta[i];
            return ret;
        }

    } // namespace ConvHelper

    namespace DivHelper { // Newton-Raphson division

        inline UnsignedDigit quasiInv(const UnsignedDigit& v) {
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

    inline UnsignedDigit::UnsignedDigit(ll x) {
        if (x == 0) {
            digits.push_back(0);
        } else {
            while (x > 0) {
                digits.push_back(x % MOD);
                x /= MOD;
            }
        }
    }

    inline UnsignedDigit UnsignedDigit::move(int k) const {
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

    inline bool UnsignedDigit::operator<(const UnsignedDigit& rhs) const {
        int n = digits.size(), m = rhs.digits.size();
        if (n != m) return n < m;
        for (int i = n - 1; i >= 0; --i)
            if (digits[i] != rhs.digits[i])
                return digits[i] < rhs.digits[i];
        return false;
    }

    inline bool UnsignedDigit::operator<=(const UnsignedDigit& rhs) const {
        int n = digits.size(), m = rhs.digits.size();
        if (n != m) return n < m;
        for (int i = n - 1; i >= 0; --i)
            if (digits[i] != rhs.digits[i])
                return digits[i] < rhs.digits[i];
        return true;
    }

    inline bool UnsignedDigit::operator==(const UnsignedDigit& rhs) const {
        return digits == rhs.digits;
    }

    inline UnsignedDigit UnsignedDigit::operator+(const UnsignedDigit& rhs) const {
        int n = digits.size(), m = rhs.digits.size();
        std::vector<int> tmp(std::max(n, m) + 1, 0);
        const SmallVecInt& a = (n > m) ? digits : rhs.digits;
        const SmallVecInt& b = (n > m) ? rhs.digits : digits;
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

    inline UnsignedDigit UnsignedDigit::operator-(const UnsignedDigit& rhs) const {
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

    inline UnsignedDigit UnsignedDigit::operator*(const UnsignedDigit& rhs) const {
        // Single-limb multiplier (loop counters, small constants): one
        // carry pass straight into the result, no convolution scratch.
        if (rhs.digits.size() == 1) {
            ll m = rhs.digits[0];
            if (m == 0) return UnsignedDigit();
            SmallVecInt r;
            ll carry = 0;
            for (size_t i = 0; i < digits.size(); ++i) {
                ll cur = (ll)digits[i] * m + carry;
                r.push_back((int)(cur % MOD));
                carry = cur / MOD;
            }
            while (carry) { r.push_back((int)(carry % MOD)); carry /= MOD; }
            return UnsignedDigit(std::move(r));
        }
        std::vector<ll> tmp = ConvHelper::conv(digits, rhs.digits, this == &rhs);
        for (size_t i = 0; i + 1 < tmp.size(); ++i) {
            tmp[i + 1] += tmp[i] / MOD;
            tmp[i] %= MOD;
        }
        while (tmp.back() >= MOD) {
            ll remain = tmp.back() / MOD;
            tmp.back() %= MOD;
            tmp.push_back(remain);
        }
        SmallVecInt result; result.resize(tmp.size());
        for (size_t i = 0; i < tmp.size(); ++i) result[i] = (int)tmp[i];
        return UnsignedDigit(std::move(result));
    }

    inline UnsignedDigit UnsignedDigit::square() const {
        std::vector<ll> tmp = ConvHelper::conv(digits, digits, true);
        for (size_t i = 0; i + 1 < tmp.size(); ++i) {
            tmp[i + 1] += tmp[i] / MOD;
            tmp[i] %= MOD;
        }
        while (tmp.back() >= MOD) {
            ll remain = tmp.back() / MOD;
            tmp.back() %= MOD;
            tmp.push_back(remain);
        }
        SmallVecInt result; result.resize(tmp.size());
        for (size_t i = 0; i < tmp.size(); ++i) result[i] = (int)tmp[i];
        return UnsignedDigit(std::move(result));
    }

    inline UnsignedDigit UnsignedDigit::operator/(const UnsignedDigit& rhs) const {
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

    inline UnsignedDigit UnsignedDigit::operator/(int k) const {
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

    inline UnsignedDigit::UnsignedDigit(const std::vector<int>& digits) : digits(digits) {
        if (this->digits.empty())
            this->digits.assign(1, 0);
        trim();
    }

    inline void UnsignedDigit::trim() {
        while (digits.size() > 1 && digits.back() == 0)
            digits.pop_back();
    }

    inline std::string UnsignedDigit::toString() const {
        // Manual limb printing: a stringstream with setw/setfill per limb
        // is measurably slower, and this runs for every number a script
        // ever converts to text.
        std::string s = std::to_string(digits.back());
        if (digits.size() > 1) {
            s.reserve(s.size() + (digits.size() - 1) * BASE);
            char buf[BASE];
            for (int i = (int)digits.size() - 2; i >= 0; --i) {
                int v = digits[i];
                for (int k = BASE - 1; k >= 0; --k) { buf[k] = (char)('0' + v % 10); v /= 10; }
                s.append(buf, BASE);
            }
        }
        return s;
    }

    inline UnsignedDigit::UnsignedDigit(std::string str) {
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

    inline UnsignedDigit pow(UnsignedDigit x, int k) {
        UnsignedDigit ret = 1;
        while (k) {
            if (k & 1) ret = ret * x;
            if (k >>= 1) x = x.square();
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
        int this_int_len = magnitude.decimalDigitCount() - decimal_pos;
        int other_int_len = other.magnitude.decimalDigitCount() - other.decimal_pos;

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

    // Extracts the value when it is a plain integer under ~1e15 (three
    // limbs). Most arithmetic in interpreted loops runs on such values,
    // where the digit-vector machinery is pure overhead.
    static bool small_int(const BigNumber& x, long long& out) { return x.is_int_fast(out); }
    // True when the value is an integer inside the native long long
    // range. Everything in [-9223372036854775808, 9223372036854775807]
    // routes through plain C++ arithmetic; only wider values (or real
    // decimals) use the limb machinery.
    bool is_int_fast(long long& out) const {
        if (decimal_pos != 0) return false;
        const BigNumberDetail::SmallVecInt& d = magnitude.digits;
        size_t n = d.size();
        if (n > 4) return false;
        if (n <= 3) {
            long long v = 0;
            for (size_t i = n; i-- > 0; ) v = v * BigNumberDetail::MOD + d[i];
            out = is_negative ? -v : v;
            return true;
        }
        // Four limbs: up to 1e20, which may or may not fit.
        unsigned __int128 w = 0;
        for (size_t i = n; i-- > 0; ) w = w * BigNumberDetail::MOD + (unsigned long long)d[i];
        __int128 sgn = is_negative ? -(__int128)w : (__int128)w;
        if (sgn > (__int128)LLONG_MAX || sgn < (__int128)LLONG_MIN) return false;
        out = (long long)sgn;
        return true;
    }
    // Builds from a value that is already known to fit in a long long.
    static BigNumber from_ll(long long v) { return BigNumber(v); }

    BigNumber& operator+=(const BigNumber& other) { *this = *this + other; return *this; }
    BigNumber& operator-=(const BigNumber& other) { *this = *this - other; return *this; }
    BigNumber& operator*=(const BigNumber& other) { *this = *this * other; return *this; }
    BigNumber& operator/=(const BigNumber& other) { *this = *this / other; return *this; }

    // 2. Basic Arithmetic Operations

    BigNumber operator+(const BigNumber& other) const {
        long long sa, sb;
        if (small_int(*this, sa) && small_int(other, sb)) {
            __int128 r = (__int128)sa + sb;
            if (r <= (__int128)LLONG_MAX && r >= (__int128)LLONG_MIN) return from_ll((long long)r);
            // Wider than long long: build the limbs straight from the sum.
            bool neg = r < 0;
            unsigned __int128 m = neg ? (unsigned __int128)(-r) : (unsigned __int128)r;
            BigNumberDetail::SmallVecInt d;
            while (m > 0) { d.push_back((int)(m % BigNumberDetail::MOD)); m /= BigNumberDetail::MOD; }
            if (d.empty()) d.push_back(0);
            return BigNumber(BigNumberDetail::UnsignedDigit(std::move(d)), neg, 0);
        }
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
        long long sa, sb;
        if (small_int(*this, sa) && small_int(other, sb)) {
            __int128 r = (__int128)sa - sb;
            if (r <= (__int128)LLONG_MAX && r >= (__int128)LLONG_MIN) return from_ll((long long)r);
            bool neg = r < 0;
            unsigned __int128 m = neg ? (unsigned __int128)(-r) : (unsigned __int128)r;
            BigNumberDetail::SmallVecInt d;
            while (m > 0) { d.push_back((int)(m % BigNumberDetail::MOD)); m /= BigNumberDetail::MOD; }
            if (d.empty()) d.push_back(0);
            return BigNumber(BigNumberDetail::UnsignedDigit(std::move(d)), neg, 0);
        }
        BigNumber inverted_other = other;
        inverted_other.is_negative = !inverted_other.is_negative;
        if(inverted_other.magnitude.isZero()) inverted_other.is_negative = false;
        return *this + inverted_other;
    }
    BigNumber operator*(const BigNumber& other) const {
        long long sa, sb;
        if (small_int(*this, sa) && small_int(other, sb)) {
            unsigned long long ua = sa < 0 ? 0ULL - (unsigned long long)sa : (unsigned long long)sa;
            unsigned long long ub = sb < 0 ? 0ULL - (unsigned long long)sb : (unsigned long long)sb;
            unsigned __int128 prod = (unsigned __int128)ua * (unsigned __int128)ub;
            // |sa|, |sb| < 1e15 -> prod < 1e30, up to six limbs.
            BigNumberDetail::SmallVecInt d;
            while (prod > 0) { d.push_back((int)(prod % BigNumberDetail::MOD)); prod /= BigNumberDetail::MOD; }
            if (d.empty()) d.push_back(0);
            return BigNumber(BigNumberDetail::UnsignedDigit(std::move(d)), (sa < 0) != (sb < 0), 0);
        }
        BigNumberDetail::UnsignedDigit res_mag = this->magnitude * other.magnitude;
        return BigNumber(res_mag, this->is_negative != other.is_negative, this->decimal_pos + other.decimal_pos);
    }
    BigNumber operator/(const BigNumber& other) const {
        long long sa, sb;
        // Fast path: two small integers, divisor small enough for limb-wise
        // long division. Reproduces the general path exactly: the general
        // path computes floor(a*10^(p+5)/b) and rounds at five digits,
        // which equals floor(a*10^(p+1)/b) rounded at one digit.
        if (small_int(*this, sa) && small_int(other, sb) && sb != 0) {
            int precision = get_default_precision();
            unsigned long long ua = sa < 0 ? 0ULL - (unsigned long long)sa : (unsigned long long)sa;
            unsigned long long ub = sb < 0 ? 0ULL - (unsigned long long)sb : (unsigned long long)sb;
            BigNumberDetail::SmallVecInt num;
            while (ua > 0) { num.push_back((int)(ua % BigNumberDetail::MOD)); ua /= BigNumberDetail::MOD; }
            int shift = precision + 1;
            int whole = shift / BigNumberDetail::BASE, rem = shift % BigNumberDetail::BASE;
            if (rem) {
                static const int P10[BigNumberDetail::BASE] = {1, 10, 100, 1000, 10000};
                long long carry = 0;
                for (size_t i = 0; i < num.size(); ++i) {
                    long long v = (long long)num[i] * P10[rem] + carry;
                    num[i] = (int)(v % BigNumberDetail::MOD);
                    carry = v / BigNumberDetail::MOD;
                }
                while (carry) { num.push_back((int)(carry % BigNumberDetail::MOD)); carry /= BigNumberDetail::MOD; }
            }
            num.insert_front((size_t)whole, 0);
            if (num.empty()) num.push_back(0);
            BigNumberDetail::SmallVecInt q; q.assign(num.size(), 0);
            long long r = 0;
            for (int i = (int)num.size() - 1; i >= 0; --i) {
                unsigned __int128 cur = (unsigned __int128)r * BigNumberDetail::MOD + num[i];
                q[i] = (int)(cur / (unsigned long long)ub);
                r = (long long)(cur % (unsigned long long)ub);
            }
            // divide q by 10, remembering the digit that drops out
            int digit = q[0] % 10;
            BigNumberDetail::SmallVecInt rq; rq.assign(q.size(), 0);
            long long carry2 = 0;
            for (int i = (int)q.size() - 1; i >= 0; --i) {
                long long cur = carry2 * BigNumberDetail::MOD + q[i];
                rq[i] = (int)(cur / 10);
                carry2 = cur % 10;
            }
            if (digit >= 5) {           // round half up, on the magnitude
                for (size_t i = 0; i < rq.size(); ++i) {
                    rq[i] += 1;
                    if (rq[i] >= BigNumberDetail::MOD) { rq[i] -= BigNumberDetail::MOD; } else break;
                }
            }
            return BigNumber(BigNumberDetail::UnsignedDigit(std::move(rq)), (sa < 0) != (sb < 0), precision);
        }
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
        long long sa, sb;
        if (small_int(*this, sa) && small_int(other, sb)) {
            if (sb == 0) throw std::runtime_error("Modulo by zero.");
            return from_ll(sa % sb);
        }
        if (!this->isInteger() || !other.isInteger()) {
            throw std::runtime_error("Operands for modulo must be integers.");
        }
        if (other.magnitude.isZero()) {
            throw std::runtime_error("Modulo by zero.");
        }
        BigNumber quotient = this->exact_division(other);
        return *this - (quotient * other);
    }
    
    BigNumber exact_division(const BigNumber& other) const; // to remove the extra zeros.

    // 3. Comparison Operations

    bool operator==(const BigNumber& other) const {
        long long sa, sb;
        if (small_int(*this, sa) && small_int(other, sb)) return sa == sb;
        return this->is_negative == other.is_negative && this->compare_abs(other) == 0;
    }
    bool operator!=(const BigNumber& other) const { return !(*this == other); }
    bool operator<(const BigNumber& other) const {
        long long sa, sb;
        if (small_int(*this, sa) && small_int(other, sb)) return sa < sb;
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
    bool isZero() const { return magnitude.isZero(); }
    BigNumber operator-() const { BigNumber res = *this; if (!res.magnitude.isZero()) res.is_negative = !res.is_negative; return res; }
    
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
        int pos = decimal_pos;
        // The fraction may end in zeros that say nothing: 12.50 is 12.5,
        // and 12.000 is 12.  The browser runtime prints numbers that
        // way, so the same number has to look the same in both.
        while (pos > 0 && (int)s.length() > pos && s[s.length() - 1] == '0') {
            s.erase(s.length() - 1);
            pos--;
        }
        if (pos > 0) {
            if ((int)s.length() <= pos) {
                s.insert(0, pos - s.length(), '0');
                s.insert(0, "0.");
            } else {
                s.insert(s.length() - pos, ".");
            }
        }
        if (is_negative) {
            s.insert(0, "-");
        }
        return s;
    }
    
    long long toLongLong() const {
        const BigNumberDetail::SmallVecInt& d = magnitude.digits;
        if (d.size() <= 3) {
            long long v = 0;
            for (size_t i = d.size(); i-- > 0; ) v = v * BigNumberDetail::MOD + d[i];
            if (decimal_pos > 0) {
                if (decimal_pos >= 15) return 0;
                static const int P10[BigNumberDetail::BASE] = {1, 10, 100, 1000, 10000};
                int whole = decimal_pos / BigNumberDetail::BASE, rem = decimal_pos % BigNumberDetail::BASE;
                for (int i = 0; i < whole; ++i) v /= BigNumberDetail::MOD;
                if (rem) v /= P10[rem];
            }
            return is_negative ? -v : v;
        }
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
        if (decimal_pos == 0) return true;
        const BigNumberDetail::SmallVecInt& d = magnitude.digits;
        if (magnitude.isZero()) return true;
        int whole = decimal_pos / BigNumberDetail::BASE, rem = decimal_pos % BigNumberDetail::BASE;
        if (whole >= (int)d.size()) return magnitude.isZero();
        static const int P10[BigNumberDetail::BASE] = {1, 10, 100, 1000, 10000};
        for (int i = 0; i < whole; ++i)
            if (d[i] != 0) return false;
        if (rem && d[whole] % P10[rem] != 0) return false;
        return true;
    }
};

inline BigNumber BigNumber::exact_division(const BigNumber& other) const {
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
