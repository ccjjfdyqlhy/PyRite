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
        // Fourteen inline limbs cover every decimal the interpreter churns
        // through in loops (default precision 50 needs eleven), so the hot
        // arithmetic paths never touch the heap at all.
        static const size_t INLINE_CAP = 14;
        SmallVecInt() : size_(0), data_(inline_), cap_(INLINE_CAP) {}
        SmallVecInt(const SmallVecInt& o) : SmallVecInt() { assign_from(o.data_, o.size_); }
        SmallVecInt(SmallVecInt&& o) noexcept : SmallVecInt() {
            if (o.data_ == o.inline_) { assign_from(o.data_, o.size_); }
            else { data_ = o.data_; size_ = o.size_; cap_ = o.cap_; o.data_ = o.inline_; o.size_ = 0; o.cap_ = INLINE_CAP; }
        }
        SmallVecInt& operator=(const SmallVecInt& o) {
            if (this != &o) assign_from(o.data_, o.size_);
            return *this;
        }
        SmallVecInt& operator=(SmallVecInt&& o) noexcept {
            if (this != &o) {
                if (data_ != inline_) std::free(data_);
                if (o.data_ == o.inline_) { data_ = inline_; cap_ = INLINE_CAP; assign_from(o.data_, o.size_); }
                else { data_ = o.data_; size_ = o.size_; cap_ = o.cap_; o.data_ = o.inline_; o.size_ = 0; o.cap_ = INLINE_CAP; }
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
        // Drop one trailing zero limb (the top of a little-endian limb
        // vector); addition can produce one, and the digit vectors must
        // stay trimmed for the size-based comparisons.
        void trim_top() {
            while (size_ > 1 && data_[size_ - 1] == 0) size_--;
        }
        // Shrink to exactly n limbs (n <= size_): the preallocated fill of
        // the single-limb multiply runs past the true length.
        void trim_to(size_t n) {
            if (n <= size_) size_ = n;
            trim_top();
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
        int inline_[INLINE_CAP];
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

        // sqrt(-1) in the Goldilocks field: the radix-4 butterfly needs
        // the 90-degree rotation, which is an element with i^2 = p - 1.
        static const uint64_t NT_I = []() {
            uint64_t i = 1;
            // nt_pow is declared below; compute lazily on first use.
            (void)i;
            return (uint64_t)0;
        }();

        // Radix-4 transform pair. Forward = decimation in frequency
        // (natural order in, bit-reversed spectrum out); inverse =
        // decimation in time (bit-reversed spectrum in, natural order
        // out, scaled by 1/n). The two conventions compose without any
        // bit-reversal pass, and radix-4 spends three multiplies per
        // four elements where radix-2 spends four: about a quarter
        // fewer multiplications and half the memory passes over a
        // multi-million-limb convolution.
        // Transform pair for exact convolution. Both directions are
        // radix-2 throughout - the forward is Gentleman-Sande (natural
        // order in, bit-reversed spectrum out), the inverse Cooley-Tukey
        // (bit-reversed in, natural out, scaled by 1/n) - but each TWO
        // consecutive stages are fused into one pass over the data: four
        // elements are loaded once, carried through both butterflies in
        // registers, and stored once. That halves the memory traffic,
        // which is what a multi-million-limb transform is bound by. The
        // odd-position differences carry twiddle factors with a 90-degree
        // rotation folded in at table-fill time, so the hot loop is pure
        // multiply-add against sequential table streams.
        __attribute__((noinline, optimize("O3")))
        inline void ntt_dif4(uint64_t* a, int lgn, int d) {
            int n = 1 << lgn;
            if (lgn == 0) return;
            static std::vector<uint64_t> tw;   // per-stage twiddle scratch
            if (lgn == 1) {                    // plain radix-2 butterfly
                uint64_t x = a[0], y = a[1];
                if (d == 1) { a[0] = nt_add(x, y); a[1] = nt_sub(x, y); }
                else {
                    uint64_t ninv = nt_pow(2, NT_P - 2);
                    a[0] = nt_mul(nt_add(x, y), ninv);
                    a[1] = nt_mul(nt_sub(x, y), ninv);
                }
                return;
            }
            if (d == 1) {
                // ---- forward: fused stage pairs, L from n down to 4 ----
                const uint64_t I = nt_pow(NT_ROOT, (NT_P - 1) / 4);   // i
                for (int L = n; L >= 4; L >>= 2) {
                    int q = L >> 2;
                    uint64_t wl = nt_pow(NT_ROOT, (NT_P - 1) / L);
                    uint64_t w2l = nt_mul(wl, wl);     // w_(L/2)
                    tw.resize(3 * q);
                    uint64_t* wa = tw.data();
                    uint64_t* wb = wa + q;
                    uint64_t* wc = wb + q;
                    wa[0] = 1; wb[0] = I; wc[0] = 1;   // wb[j] = w_L^(j+q), w_L^q = i
                    for (int j = 1; j < q; ++j) {
                        wa[j] = nt_mul(wa[j - 1], wl);
                        wb[j] = nt_mul(wb[j - 1], wl);
                        wc[j] = nt_mul(wc[j - 1], w2l);
                    }
                    for (int i = 0; i < n; i += L) {
                        uint64_t* g = a + i;
                        for (int j = 0; j < q; ++j) {
                            uint64_t a0 = g[j];
                            uint64_t a1 = g[j + q];
                            uint64_t a2 = g[j + 2 * q];
                            uint64_t a3 = g[j + 3 * q];
                            uint64_t p = nt_mul(nt_sub(a0, a2), wa[j]);
                            uint64_t s = nt_add(a0, a2);
                            uint64_t r = nt_mul(nt_sub(a1, a3), wb[j]);
                            uint64_t t = nt_add(a1, a3);
                            g[j] = nt_add(s, t);
                            g[j + q] = nt_mul(nt_sub(s, t), wc[j]);
                            g[j + 2 * q] = nt_add(p, r);
                            g[j + 3 * q] = nt_mul(nt_sub(p, r), wc[j]);
                        }
                    }
                }
                if (lgn & 1) {                 // one twiddle-free split left
                    for (int i = 0; i < n; i += 2) {
                        uint64_t x = a[i], y = a[i + 1];
                        a[i] = nt_add(x, y);
                        a[i + 1] = nt_sub(x, y);
                    }
                }
            } else {
                // ---- inverse: transposed pairs, L ascending, then 1/n ----
                const uint64_t I = nt_pow(NT_ROOT, (NT_P - 1) / 4);   // i
                if (lgn & 1) {   // undo the forward's final twiddle-free split first
                    for (int i = 0; i < n; i += 2) {
                        uint64_t x = a[i], y = a[i + 1];
                        a[i] = nt_add(x, y);
                        a[i + 1] = nt_sub(x, y);
                    }
                }
                for (int L = (lgn & 1) ? 8 : 4; L <= n; L <<= 2) {
                    int q = L >> 2;
                    uint64_t wl = nt_pow(NT_ROOT, NT_P - 1 - (NT_P - 1) / L);
                    uint64_t w2l = nt_mul(wl, wl);
                    tw.resize(3 * q);
                    uint64_t* wa = tw.data();
                    uint64_t* wb = wa + q;
                    uint64_t* wc = wb + q;
                    wa[0] = 1; wb[0] = nt_sub(0, I); wc[0] = 1;   // conjugate
                    for (int j = 1; j < q; ++j) {
                        wa[j] = nt_mul(wa[j - 1], wl);
                        wb[j] = nt_mul(wb[j - 1], wl);
                        wc[j] = nt_mul(wc[j - 1], w2l);
                    }

                    for (int i = 0; i < n; i += L) {
                        uint64_t* g = a + i;
                        for (int j = 0; j < q; ++j) {
                            // Transpose of the fused forward pair: the
                            // neighbour pairs mix through wc, the outer
                            // differences carry wa / wb, then the sums
                            // and differences recombine.
                            uint64_t o0 = g[j];
                            uint64_t o1 = g[j + q];
                            uint64_t o2 = g[j + 2 * q];
                            uint64_t o3 = g[j + 3 * q];
                            uint64_t v0 = nt_add(o0, nt_mul(o1, wc[j]));
                            uint64_t v1 = nt_sub(o0, nt_mul(o1, wc[j]));
                            uint64_t v2 = nt_add(o2, nt_mul(o3, wc[j]));
                            uint64_t v3 = nt_sub(o2, nt_mul(o3, wc[j]));
                            uint64_t w2 = nt_mul(v2, wa[j]);
                            uint64_t w3 = nt_mul(v3, wb[j]);
                            g[j] = nt_add(v0, w2);
                            g[j + 2 * q] = nt_sub(v0, w2);
                            g[j + q] = nt_add(v1, w3);
                            g[j + 3 * q] = nt_sub(v1, w3);
                        }
                    }
                }
                uint64_t ninv = nt_pow(n, NT_P - 2);
                for (int j = 0; j < n; ++j) a[j] = nt_mul(a[j], ninv);
            }
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
        __attribute__((noinline, optimize("O3")))
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
            ntt_dif4(ta.data(), lgn, 1);
            if (square) {
                for (size_t i = 0; i < len; ++i) ta[i] = nt_mul(ta[i], ta[i]);
            } else {
                tb.assign(len, 0);
                for (int i = 0; i <= m; ++i) tb[i] = (uint64_t)b[i];
                ntt_dif4(tb.data(), lgn, 1);
                for (size_t i = 0; i < len; ++i) ta[i] = nt_mul(ta[i], tb[i]);
            }
            ntt_dif4(ta.data(), lgn, -1);
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
        SmallVecInt tmp;
        tmp.resize((size_t)(n > m ? n : m) + 1, 0);
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
        tmp.trim_top();
        return UnsignedDigit(std::move(tmp));
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
        // carry pass straight into a preallocated result, no convolution
        // scratch and no reallocation chain.
        if (rhs.digits.size() == 1) {
            ll m = rhs.digits[0];
            if (m == 0) return UnsignedDigit();
            size_t n = digits.size();
            SmallVecInt r;
            r.resize(n + 2, 0);
            ll carry = 0;
            for (size_t i = 0; i < n; ++i) {
                ll cur = (ll)digits[i] * m + carry;
                r[i] = (int)(cur % MOD);
                carry = cur / MOD;
            }
            size_t sz = n;
            while (carry) { r[sz++] = (int)(carry % MOD); carry /= MOD; }
            r.trim_to(sz);
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
    // mutable: the decimal and binary limb views are interchangeable
    // caches of one another; either may be rebuilt inside const readers.
    mutable BigNumberDetail::UnsignedDigit magnitude;
    bool is_negative;
    int decimal_pos; // Number of digits after the decimal point
    // Memoized machine-int form, written by is_int_fast and read on every
    // later visit of the same object. 0 = unknown, 1 = is an int64 (value
    // in icv_), 2 = is not. Copies inherit the cache; a value can only
    // change its limbs through assignment, which brings a fresh cache.
    mutable long long icv_;
    mutable signed char icv_state_;
    // Binary shadow: a base-2^32 view of the magnitude. Multiplying a
    // huge value by small integers over and over (factorial-shaped
    // loops) pays a divide chain per limb in the decimal view; in the
    // binary view the carry is a shift, which runs ~4x faster. The
    // shadow is created lazily (only while the value is small, so a
    // one-off multiply never pays a conversion) and the decimal limbs
    // are rebuilt by repeated division whenever some other operation
    // needs them - both conversions are cheap linear-ish passes.
    mutable std::vector<uint32_t> bin_;
    mutable bool dec_valid_ = true;
    mutable bool bin_valid_ = false;
    // Deferred small multiplier: consecutive multiplications by machine
    // ints fold into this product and hit the binary limbs in one pass
    // once it would overflow. The value's true magnitude is bin_ times
    // pending_mul_; every reader flushes first.
    mutable unsigned long long pending_mul_ = 1;

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
        if (!dec_valid_) ensure_dec();
        if (!other.dec_valid_) other.ensure_dec();
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

    BigNumber() : is_negative(false), decimal_pos(0), magnitude(0LL), icv_(0), icv_state_(1) {}
    BigNumber(long long n) : is_negative(n < 0), decimal_pos(0), magnitude(n < 0 ? -n : n), icv_(n), icv_state_(1) {}
    BigNumber(std::string s) : icv_(0), icv_state_(0) {
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
    BigNumber(BigNumberDetail::UnsignedDigit mag, bool neg, int dec_pos) : magnitude(mag), is_negative(neg), decimal_pos(dec_pos), icv_(0), icv_state_(0) { normalize(); }
    // Copies carry the cache over: the limbs do not change, so the memo
    // stays valid for the copy as well.
    BigNumber(const BigNumber& o) = default;
    BigNumber& operator=(const BigNumber& o) = default;

    // Extracts the value when it is a plain integer under ~1e15 (three
    // limbs). Most arithmetic in interpreted loops runs on such values,
    // where the digit-vector machinery is pure overhead.
    static bool small_int(const BigNumber& x, long long& out) { return x.is_int_fast(out); }
    // Rebuild the decimal limbs from the binary shadow: divide the
    // binary value by 10^5 repeatedly; the remainders are the base-10^5
    // limbs, least significant first. The running remainder stays below
    // 2^32, so every step is one 128-bit shift-or plus one small divide.
    void ensure_dec() const {
        if (dec_valid_) return;
        flush_pending();
        std::vector<uint32_t> cur = bin_;
        std::vector<uint32_t> chunks;               // little-endian, 9 digits each
        while (!(cur.size() == 1 && cur[0] == 0)) {
            uint64_t rem = 0;
            for (size_t k = cur.size(); k-- > 0; ) {
                uint64_t curv = (rem << 32) | cur[k];   // rem < 1e9 < 2^30
                cur[k] = (uint32_t)(curv / 1000000000u);
                rem = curv % 1000000000u;
            }
            while (cur.size() > 1 && cur.back() == 0) cur.pop_back();
            chunks.push_back((uint32_t)rem);
        }
        // Re-assemble chunks into base-10^5 limbs through a decimal
        // digit window: 9-digit chunks and 5-digit limbs misalign, so a
        // running accumulator carries the residual digits between them
        // (45 digits = 9 chunks = 9 limbs brings both back in step).
        static const uint64_t P10[5] = {1, 10, 100, 1000, 10000};
        BigNumberDetail::SmallVecInt mag;
        mag.resize(chunks.size() * 2 + 2, 0);
        size_t mag_n = 0;
        uint64_t acc = 0;
        int shift = 0;                      // decimal digits currently in acc
        for (size_t k = 0; k < chunks.size(); ++k) {
            acc += (uint64_t)chunks[k] * P10[shift];
            shift += 9;
            while (shift >= 5) {
                mag[mag_n++] = (int)(acc % 100000);
                acc /= 100000;
                shift -= 5;
            }
        }
        if (acc) mag[mag_n++] = (int)acc;
        if (mag_n == 0) mag[mag_n++] = 0;
        mag.trim_to(mag_n);
        magnitude = BigNumberDetail::UnsignedDigit(std::move(mag));
        icv_state_ = 0;
        dec_valid_ = true;
    }
    // Build the binary shadow from the decimal limbs: divide the value
    // by 2^32 repeatedly; the remainders are the binary limbs, least
    // significant first. The quotient limb stays under 10^5 because the
    // dividend limb is.
    void ensure_bin() const {
        if (bin_valid_) return;
        std::vector<uint32_t> b;
        BigNumberDetail::SmallVecInt cur = magnitude.digits;
        while (!(cur.size() == 1 && cur[0] == 0)) {
            unsigned __int128 rem = 0;
            for (size_t k = cur.size(); k-- > 0; ) {
                unsigned __int128 curv = rem * BigNumberDetail::MOD + (unsigned long long)cur[k];
                unsigned long long q = (uint64_t)(curv >> 32);
                cur[k] = (int)q;
                rem = curv - ((unsigned __int128)q << 32);
            }
            while (cur.size() > 1 && cur.back() == 0) cur.pop_back();
            b.push_back((uint32_t)(uint64_t)rem);
        }
        if (b.empty()) b.push_back(0);
        bin_ = std::move(b);
        bin_valid_ = true;
    }
    // Fold the deferred multiplier into the binary limbs.
    void flush_pending() const {
        if (pending_mul_ == 1) return;
        unsigned long long pm = pending_mul_;
        pending_mul_ = 1;
        const_cast<BigNumber*>(this)->mul_bin_inplace(pm);
    }
    // Multiply the binary shadow by a single-limb integer, in place. The
    // carry is a shift - no divide anywhere in the chain.
    void mul_bin_inplace(unsigned long long um) {
        uint64_t carry = 0;
        for (size_t i = 0; i < bin_.size(); ++i) {
            uint64_t cur = (uint64_t)bin_[i] * um + carry;
            bin_[i] = (uint32_t)cur;
            carry = cur >> 32;
        }
        while (carry) { bin_.push_back((uint32_t)carry); carry >>= 32; }
    }

    // True when the value is an integer inside the native long long
    // True when the value is an integer inside the native long long
    // range. Everything in [-9223372036854775808, 9223372036854775807]
    // routes through plain C++ arithmetic; only wider values (or real
    // decimals) use the limb machinery. The answer is memoized on the
    // object: a loop counter is tested by several opcodes per iteration
    // and none of them should re-walk the limbs.
    bool is_int_fast(long long& out) const {
        if (icv_state_ == 1) { out = icv_; return true; }
        if (icv_state_ == 2) return false;
        if (decimal_pos != 0) { icv_state_ = 2; return false; }
        if (!dec_valid_) ensure_dec();
        const BigNumberDetail::SmallVecInt& d = magnitude.digits;
        size_t n = d.size();
        if (n > 4) { icv_state_ = 2; return false; }
        if (n <= 3) {
            long long v = 0;
            for (size_t i = n; i-- > 0; ) v = v * BigNumberDetail::MOD + d[i];
            out = is_negative ? -v : v;
            icv_ = out; icv_state_ = 1;
            return true;
        }
        // Four limbs: up to 1e20, which may or may not fit.
        unsigned __int128 w = 0;
        for (size_t i = n; i-- > 0; ) w = w * BigNumberDetail::MOD + (unsigned long long)d[i];
        __int128 sgn = is_negative ? -(__int128)w : (__int128)w;
        if (sgn > (__int128)LLONG_MAX || sgn < (__int128)LLONG_MIN) { icv_state_ = 2; return false; }
        out = (long long)sgn;
        icv_ = out; icv_state_ = 1;
        return true;
    }
    // Builds from a value that is already known to fit in a long long.
    static BigNumber from_ll(long long v) { return BigNumber(v); }
    // Rewrites the value in place from a machine int. The int cache stays
    // warm, and small values never leave the inline limbs.
    void set_ll(long long v) {
        is_negative = v < 0;
        decimal_pos = 0;
        unsigned long long m = v < 0 ? 0ULL - (unsigned long long)v : (unsigned long long)v;
        BigNumberDetail::SmallVecInt& d = magnitude.digits;
        size_t k = 0;
        for (unsigned long long t = m; t > 0; t /= BigNumberDetail::MOD) k++;
        if (k == 0) k = 1;
        if (d.size() < k) d.resize(k, 0);
        size_t i = 0;
        while (m > 0) { d[i++] = (int)(m % BigNumberDetail::MOD); m /= BigNumberDetail::MOD; }
        if (i == 0) d[0] = 0;
        d.trim_to(k);
        bin_valid_ = false;
        icv_ = v; icv_state_ = 1;
    }
    // In-place multiply by a single-limb integer (|m| < 10^5). The limbs
    // are rewritten from the bottom up, which cannot overlap the reads.
    // Used by fused `big = big * small` sites; returns false when the
    // caller should fall back to the general path.
    bool mul_small_inplace(long long m) {
        if (m == 0) { *this = BigNumber(); return true; }
        unsigned long long um = m < 0 ? 0ULL - (unsigned long long)m : (unsigned long long)m;
        if (um >= (unsigned long long)BigNumberDetail::MOD) return false;
        // Binary-shadow path: repeated small multiplies (factorial-shaped
        // loops) run their carry chain as shifts instead of divides. The
        // shadow is only created while the value is small, so a one-off
        // multiply on a huge decimal never pays a conversion.
        if (bin_valid_ || magnitude.digits.size() <= 64) {
            ensure_bin();
            if (pending_mul_ <= 0xFFFFFFFFULL / (unsigned long long)um) {
                pending_mul_ *= um;            // deferred: one pass later
            } else {
                mul_bin_inplace(pending_mul_);
                pending_mul_ = um;
            }
            dec_valid_ = false;
            if (icv_state_ == 1) {
                __int128 r = (__int128)icv_ * m;
                if (r <= (__int128)LLONG_MAX && r >= (__int128)LLONG_MIN) icv_ = (long long)r;
                else icv_state_ = 2;
            }
            return true;
        }
        BigNumberDetail::SmallVecInt& d = magnitude.digits;
        size_t n = d.size();
        // reserve one extra limb for the final carry
        if (d.size() < n + 1) d.resize(n + 1, 0);
        // multiply from the top down is WRONG for in-place (carry flows up);
        // bottom-up reads digit i before writing it - safe.
        unsigned long long carry = 0;
        for (size_t i = 0; i < n; ++i) {
            unsigned long long cur = (unsigned long long)(unsigned int)d[i] * um + carry;
            d[i] = (int)(cur % BigNumberDetail::MOD);
            carry = cur / BigNumberDetail::MOD;
        }
        size_t sz = n;
        while (carry) {
            if (sz >= d.size()) d.resize(sz + 1, 0);
            d[sz++] = (int)(carry % BigNumberDetail::MOD);
            carry /= BigNumberDetail::MOD;
        }
        d.trim_to(sz);
        if (m < 0 && !magnitude.isZero()) is_negative = !is_negative;
        // refresh the int cache when both factors were cached ints
        if (icv_state_ == 1) {
            __int128 r = (__int128)icv_ * m;
            if (r <= (__int128)LLONG_MAX && r >= (__int128)LLONG_MIN) { icv_ = (long long)r; }
            else icv_state_ = 2;
        }
        return true;
    }

    BigNumber& operator+=(const BigNumber& other) { *this = *this + other; return *this; }
    BigNumber& operator-=(const BigNumber& other) { *this = *this - other; return *this; }
    BigNumber& operator*=(const BigNumber& other) { *this = *this * other; return *this; }
    BigNumber& operator/=(const BigNumber& other) { *this = *this / other; return *this; }

    // 2. Basic Arithmetic Operations

    BigNumber operator+(const BigNumber& other) const {
        if (!dec_valid_) ensure_dec();
        if (!other.dec_valid_) other.ensure_dec();
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
        if (!dec_valid_) ensure_dec();
        if (!other.dec_valid_) other.ensure_dec();
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
        inverted_other.icv_ = -inverted_other.icv_;   // sign flip: keep the memo true
        if(inverted_other.magnitude.isZero()) inverted_other.is_negative = false;
        return *this + inverted_other;
    }
    BigNumber operator*(const BigNumber& other) const {
        if (!dec_valid_) ensure_dec();
        if (!other.dec_valid_) other.ensure_dec();
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
        if (!dec_valid_) ensure_dec();
        if (!other.dec_valid_) other.ensure_dec();
        if (small_int(*this, sa) && small_int(other, sb) && sb != 0) {
            int precision = get_default_precision();
            unsigned long long ua = sa < 0 ? 0ULL - (unsigned long long)sa : (unsigned long long)sa;
            unsigned long long ub = sb < 0 ? 0ULL - (unsigned long long)sb : (unsigned long long)sb;
            // Single-shot path: when ua * 10^precision still fits an
            // unsigned __int128, the whole expansion is one 128-bit
            // division and one rounding step - no limb vectors. This is
            // the shape of everyday small-integer division (i / 7).
            // The scale and the ua limit depend only on the precision,
            // which almost never changes, so they are cached.
            static int fits_p = -1;
            static unsigned __int128 fits_scale = 0;
            static unsigned long long fits_limit = 0;
            if (precision != fits_p) {
                fits_p = precision;
                fits_scale = 1;
                for (int i = 0; i < precision; ++i) {
                    if (fits_scale > (~(unsigned __int128)0) / 10) { fits_scale = 0; break; }
                    fits_scale *= 10;
                }
                fits_limit = fits_scale
                    ? (unsigned long long)((~(unsigned __int128)0) / fits_scale) : 0;
            }
            if (fits_limit && ua <= fits_limit) {
                unsigned __int128 n = (unsigned __int128)ua * fits_scale;
                unsigned __int128 q = n / ub;
                unsigned __int128 r = n % ub;
                if (r * 2 >= ub) ++q;          // round half up, on the magnitude
                if (q > 0) {
                    BigNumberDetail::SmallVecInt rq;
                    while (q > 0) { rq.push_back((int)(q % BigNumberDetail::MOD)); q /= BigNumberDetail::MOD; }
                    return BigNumber(BigNumberDetail::UnsignedDigit(std::move(rq)), (sa < 0) != (sb < 0), precision);
                }
                // q == 0 (only reachable at very low precision): fall
                // through so the zero result keeps the general limb shape.
            }
            // numerator limbs, then the whole 10^k scaling applied by
            // placing the limbs at the top of a zero-padded buffer
            BigNumberDetail::SmallVecInt num;
            size_t nlimbs = 0;
            unsigned long long uat = ua;
            while (uat > 0) { nlimbs++; uat /= BigNumberDetail::MOD; }
            int whole = precision / BigNumberDetail::BASE, rem = precision % BigNumberDetail::BASE;
            num.resize(whole + nlimbs + 1, 0);
            for (size_t k = 0; k < nlimbs; ++k) { num[whole + k] = (int)(ua % BigNumberDetail::MOD); ua /= BigNumberDetail::MOD; }
            if (rem) {
                static const int P10[BigNumberDetail::BASE] = {1, 10, 100, 1000, 10000};
                long long carry = 0;
                for (size_t k = 0; k < whole + nlimbs; ++k) {
                    long long v = (long long)num[k] * P10[rem] + carry;
                    num[k] = (int)(v % BigNumberDetail::MOD);
                    carry = v / BigNumberDetail::MOD;
                }
                size_t k = whole + nlimbs;
                while (carry) { if (k >= num.size()) num.resize(k + 1, 0); num[k] = (int)(carry % BigNumberDetail::MOD); carry /= BigNumberDetail::MOD; k++; }
            }
            // One long-division pass over the scaled numerator keeps the
            // final remainder, so rounding needs no second quotient pass:
            // dropping the (p+1)-th digit rounds up exactly when the
            // remainder of ua*10^p / ub is at least half the divisor.
            BigNumberDetail::SmallVecInt q; q.assign(num.size(), 0);
            if (ub <= 0xFFFFFFFFULL) {
                // Divisor fits 32 bits. A hardware `div` sits on the
                // carry chain of the long division (20+ cycles each);
                // a precomputed round-up reciprocal turns the step into
                // one multiply-shift-subtract chain (~8 cycles). The
                // magic is cached per divisor, which is the shape of a
                // loop dividing by a constant.
                static unsigned long long rc_d = 0;
                static unsigned long long rc_M = 0;   // ceil(2^64 / ub)
                static unsigned __int128 rc_M128 = 0; // wide fallback
                static int rc_k = 0;
                if (rc_d != ub) {
                    rc_d = ub;
                    // Round-up reciprocal over 2^64: exact whenever
                    // ub^2 * 1e5 fits the word (ub <= 1.36e7), which is
                    // every divide-by-small-limb the loops ever issue.
                    // The quotient is then the HIGH word of one mulq -
                    // no shift, no 128-bit multiply.
                    if (ub <= 13590000ULL) {
                        rc_M = (unsigned long long)(((unsigned __int128)1 << 64) / ub)
                             + ( (((unsigned __int128)1 << 64) % ub) ? 1 : 0);
                        rc_M128 = 0;
                    } else {
                        rc_M = 0;
                        int bits = 0;
                        for (unsigned long long t = ub; t > 0; t >>= 1) bits++;
                        rc_k = 2 * bits + 18;          // exact: ub^2 * 1e5 <= 2^k
                        unsigned __int128 m = (((unsigned __int128)1 << rc_k) + ub - 1) / ub;
                        rc_M128 = (unsigned long long)m;   // < 2^(k-bits) <= 2^50
                    }
                }
                // Zero limbs above the top of the numerator contribute
                // nothing: start the chain at the highest real limb and
                // build only that many quotient limbs.
                int hi = (int)num.size() - 1;
                while (hi > 0 && num[hi] == 0) hi--;
                BigNumberDetail::SmallVecInt q2;
                q2.resize(hi + 1, 0);
                q.swap(q2);
                unsigned long long r64 = 0;
                if (rc_M) {
                    for (int i = hi; i >= 0; --i) {
                        unsigned long long cur = r64 * (unsigned long long)BigNumberDetail::MOD
                                               + (unsigned long long)num[i];
                        // quotient = high word of cur * rc_M
                        unsigned __int128 pr = (unsigned __int128)cur * rc_M;
                        unsigned long long qt = (unsigned long long)(pr >> 64);
                        r64 = cur - qt * ub;
                        q[i] = (int)qt;
                    }
                } else {
                    for (int i = hi; i >= 0; --i) {
                        unsigned long long cur = r64 * (unsigned long long)BigNumberDetail::MOD
                                               + (unsigned long long)num[i];
                        unsigned long long qt = (unsigned long long)
                            (((unsigned __int128)cur * rc_M128) >> rc_k);
                        r64 = cur - qt * ub;
                        q[i] = (int)qt;
                    }
                }
                if (r64 * 2 >= ub) {    // round half up, on the magnitude
                    for (size_t i = 0; i < q.size(); ++i) {
                        q[i] += 1;
                        if (q[i] >= BigNumberDetail::MOD) { q[i] -= BigNumberDetail::MOD; } else break;
                    }
                }
            } else {
                unsigned long long r = 0;
                for (int i = (int)num.size() - 1; i >= 0; --i) {
                    unsigned __int128 cur = (unsigned __int128)r * BigNumberDetail::MOD + num[i];
                    q[i] = (int)(cur / (unsigned long long)ub);
                    r = (unsigned long long)(cur % (unsigned long long)ub);
                }
                if ((unsigned __int128)r * 2 >= ub) {   // round half up, on the magnitude
                    for (size_t i = 0; i < q.size(); ++i) {
                        q[i] += 1;
                        if (q[i] >= BigNumberDetail::MOD) { q[i] -= BigNumberDetail::MOD; } else break;
                    }
                }
            }
            return BigNumber(BigNumberDetail::UnsignedDigit(std::move(q)), (sa < 0) != (sb < 0), precision);
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
        if (!dec_valid_) ensure_dec();
        if (!other.dec_valid_) other.ensure_dec();
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
        if (!dec_valid_) ensure_dec();
        if (!exp.dec_valid_) exp.ensure_dec();
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
	 
    BigNumber abs() const { BigNumber res = *this; res.is_negative = false; res.icv_ = res.icv_ < 0 ? -res.icv_ : res.icv_; return res; }
    bool isZero() const { if (!dec_valid_) ensure_dec(); return magnitude.isZero(); }
    BigNumber operator-() const { BigNumber res = *this; if (!res.magnitude.isZero()) { res.is_negative = !res.is_negative; res.icv_ = -res.icv_; } return res; }
    
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
        if (!dec_valid_) ensure_dec();

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
        if (!dec_valid_) ensure_dec();
        if (magnitude.isZero()) return "0";
        // Machine-int numbers print straight from the integer: string
        // building in hot loops ("k" + i) skips the limb formatter.
        if (decimal_pos == 0) {
            long long v;
            if (is_int_fast(v)) return std::to_string(v);
        }
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
        // Machine ints answer from the memo: list indexing in a loop hits
        // this for every element.
        long long fast;
        if (is_int_fast(fast)) return fast;
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
        if (decimal_pos == 0) return !dec_valid_ ? (ensure_dec(), true) : true;
        if (!dec_valid_) ensure_dec();
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
    if (!dec_valid_) ensure_dec();
    if (!other.dec_valid_) other.ensure_dec();
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
