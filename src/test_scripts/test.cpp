#include <cstdio>
#include <cstring>

#include <algorithm>
#include <iostream>
#include <complex>
#include <string>
#include <vector>

#define LOG(FMT...) fprintf(stderr, FMT)

using namespace std;

typedef long long ll;
typedef complex<double> cd;

const int BASE = 5, MOD = 100000, LGM = 17;
const double PI = 3.1415926535897932384626;

class UnsignedDigit;

namespace DivHelper { UnsignedDigit quasiInv(const UnsignedDigit& v); }

class UnsignedDigit {
public:
	vector<int> digits;

public:
	UnsignedDigit() : digits(1) {}

	UnsignedDigit(const vector<int>& digits);

	UnsignedDigit(ll x);

	UnsignedDigit(string str);

	string toString() const;

	int size() const { return digits.size(); }

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

public:
	void trim();
};

class UnsignedDecimal {};

class Int {};

class Decimal {};

namespace ConvHelper {

	void fft(cd* a, int lgn, int d) {
		int n = 1 << lgn;
		{
			static vector<int> brev;
			if (n != brev.size()) {
				brev.resize(n);
				for (int i = 0; i < n; ++i)
					brev[i] = (brev[i >> 1] >> 1) | ((i & 1) << (lgn - 1));
			}
			for (int i = 0; i < n; ++i)
				if (brev[i] < i)
					swap(a[brev[i]], a[i]);
		}
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

	vector<ll> conv(const vector<int>& a, const vector<int>& b) {
		int n = a.size() - 1, m = b.size() - 1;
		if (n < 1000 / (m + 1) || n < 10 || m < 10) {
			vector<ll> ret(n + m + 1);
			for (int i = 0; i <= n; ++i)
				for (int j = 0; j <= m; ++j)
					ret[i + j] += a[i] * (ll)b[j];
			return ret;
		}
		int lgn = 0;
		while ((1 << lgn) <= n + m)
			++lgn;
		vector<cd> ta(a.begin(), a.end()), tb(b.begin(), b.end());
		ta.resize(1 << lgn);
		tb.resize(1 << lgn);
		fft(ta.begin().base(), lgn, 1);
		fft(tb.begin().base(), lgn, 1);
		for (int i = 0; i < (1 << lgn); ++i)
			ta[i] *= tb[i];
		fft(ta.begin().base(), lgn, -1);
		vector<ll> ret(n + m + 1);
		for (int i = 0; i <= n + m; ++i)
			ret[i] = ta[i].real() + 0.5;
		return ret;
	}

}

namespace DivHelper {

	UnsignedDigit quasiInv(const UnsignedDigit& v) {
		if (v.digits.size() == 1) {
			UnsignedDigit tmp;
			tmp.digits.resize(3);
			tmp.digits[2] = 1;
			return tmp / v.digits[0];
		}
		if (v.digits.size() == 2) {
			UnsignedDigit sum = 0, go = 1;
			vector<int> tmp(4);
			go = go.move(4);
			vector<UnsignedDigit> db(LGM);
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
		UnsignedDigit tmp = quasiInv(vector<int>(v.digits.end().base() - k, v.digits.end().base()));
		return (UnsignedDigit(2) * tmp).move(n - k) - (v * tmp * tmp).move(-2 * k);
	}

}

UnsignedDigit::UnsignedDigit(ll x) {
	while (x) {
		digits.push_back(x % MOD);
		x /= MOD;
	}
	if (digits.empty())
		digits.push_back(0);
}

UnsignedDigit UnsignedDigit::move(int k) const {
	if (k == 0)
		return *this;
	if (k < 0) {
		if (-k >= digits.size())
			return UnsignedDigit();
		return vector<int>(digits.begin().base() + (-k), digits.end().base());
	}
	if (digits.size() == 1 && digits[0] == 0)
		return UnsignedDigit();
	UnsignedDigit ret;
	ret.digits.resize(k, 0);
	ret.digits.insert(ret.digits.end(), digits.begin(), digits.end());
	return ret;
}

bool UnsignedDigit::operator<(const UnsignedDigit& rhs) const {
	int n = digits.size(), m = rhs.digits.size();
	if (n != m)
		return n < m;
	for (int i = n - 1; i >= 0; --i)
		if (digits[i] != rhs.digits[i])
			return digits[i] < rhs.digits[i];
	return false;
}

bool UnsignedDigit::operator<=(const UnsignedDigit& rhs) const {
	int n = digits.size(), m = rhs.digits.size();
	if (n != m)
		return n < m;
	for (int i = n - 1; i >= 0; --i)
		if (digits[i] != rhs.digits[i])
			return digits[i] < rhs.digits[i];
	return true;
}

bool UnsignedDigit::operator==(const UnsignedDigit& rhs) const {
	int n = digits.size(), m = rhs.digits.size();
	if (n != m)
		return false;
	return memcmp(digits.begin().base(), rhs.digits.begin().base(), n) == 0;
}

UnsignedDigit UnsignedDigit::operator+(const UnsignedDigit& rhs) const {
	int n = digits.size(), m = rhs.digits.size();
	vector<int> tmp = digits;
	if (m > n) {
		tmp.resize(m + 1);
		for (int i = 0; i < m; ++i)
			if ((tmp[i] += rhs.digits[i]) >= MOD) {
				tmp[i] -= MOD;
				++tmp[i + 1];
			}
	} else {
		tmp.resize(n + 1);
		for (int i = 0; i < m; ++i)
			if ((tmp[i] += rhs.digits[i]) >= MOD) {
				tmp[i] -= MOD;
				++tmp[i + 1];
			}
		for (int i = m; i < n; ++i)
			if (tmp[i] == MOD) {
				tmp[i] = 0;
				++tmp[i + 1];
			}
	}
	return tmp;
}

UnsignedDigit UnsignedDigit::operator*(const UnsignedDigit& rhs) const {
	vector<ll> tmp = ConvHelper::conv(digits, rhs.digits);
	for (int i = 0; i + 1 < tmp.size(); ++i) {
		tmp[i + 1] += tmp[i] / MOD;
		tmp[i] %= MOD;
	}
	while (tmp.back() >= MOD) {
		ll remain = tmp.back() / MOD;
		tmp.back() %= MOD;
		tmp.push_back(remain);
	}
	return vector<int>(tmp.begin(), tmp.end());
}

UnsignedDigit UnsignedDigit::operator/(const UnsignedDigit& rhs) const {
	int m = digits.size(), n = rhs.digits.size(), t = 0;
	if (m < n)
		return 0;
	if (m > n * 2)
		t = m - 2 * n;
	UnsignedDigit sv = DivHelper::quasiInv(rhs.move(t));
	UnsignedDigit ret = move(t) * sv;
	ret = ret.move(-2 * (n + t));
	if ((ret + 1) * rhs <= *this)
		ret = ret + 1;
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

UnsignedDigit UnsignedDigit::operator-(const UnsignedDigit& rhs) const {
	UnsignedDigit ret(*this);
	int n = rhs.digits.size();
	for (int i = 0; i < n; ++i)
		if ((ret.digits[i] -= rhs.digits[i]) < 0) {
			ret.digits[i] += MOD;
			--ret.digits[i + 1];
		}
	ret.trim();
	return ret;
}

UnsignedDigit::UnsignedDigit(const vector<int>& digits) : digits(digits) {
	if (this->digits.empty())
		this->digits.resize(1);
	trim();
}

void UnsignedDigit::trim() {
	while (digits.size() > 1 && digits.back() == 0)
		digits.pop_back();
}

string UnsignedDigit::toString() const {
	static char buf[BASE + 1];
	sprintf(buf, "%d", digits.back());
	string ret = buf;
	int q = ret.size();
	ret.resize(q + BASE * (digits.size() - 1));
	int j = 0;
	for (int i = (int)digits.size() - 2; i >= 0; --i) {
		sprintf(const_cast<char*>(ret.c_str()) + q + j * BASE, "%05d", digits[i]);
		++j;
	}
	return ret;
}

UnsignedDigit::UnsignedDigit(string str) {
	reverse(str.begin(), str.end());
	digits.resize((str.size() + BASE - 1) / BASE);
	int cur = 1;
	for (int i = 0; i < str.size(); ++i) {
		if (i % BASE == 0)
			cur = 1;
		digits[i / BASE] += cur * (str[i] - '0');
		cur *= 10;
	}
	trim();
}

UnsignedDigit pow(UnsignedDigit x, int k) {
	UnsignedDigit ret = 1;
	while (k) {
		if (k & 1)
			ret = ret * x;
		if (k >>= 1)
			x = x * x;
	}
	return ret;
}

int main() {
	// NEW: C++ iostream an be slow for large I/O, this speeds it up.
	ios_base::sync_with_stdio(false);
	cin.tie(NULL);

	int m;
	cin >> m;
	string s;
	cin >> s;
	if (s == "0") {
		cout << "0" << endl;
		return 0;
	}
	if (m == 1) {
		cout << s << endl;
		return 0;
	}

	// === MODIFICATION START ===
	// Define required precision. We calculate with more digits (CALC_PRECISION)
	// to ensure the final output (OUTPUT_PRECISION) is accurate.
	const int OUTPUT_PRECISION = 20;
	const int CALC_PRECISION = 40; // Using 40 digits for internal calculation

	// Scale the number by 10^(CALC_PRECISION * m) by appending zeros.
	// This transforms the problem from finding a fractional root to finding a large integer root.
	s += string(CALC_PRECISION * m, '0');
	
	UnsignedDigit n(s);
    // The initial guess logic is kept the same. It's robust enough to work on the scaled number.
	UnsignedDigit x(min(n, UnsignedDigit(MOD - 1).move((n.size() + m - 1) / m - 1))), xx;
	{
		int top = x.size() - 1;
		int l = 0, r = MOD - 1;
		while (l < r) {
			int mid = (l + r) / 2;
			x.digits[top] = mid;
			if (pow(x, m) <= n)
				l = mid + 1;
			else
				r = mid;
		}
		x.digits[top] = l;
		x.trim();
	}
    
	// Newton's method iteration remains unchanged, it will converge to the scaled root.
	xx = (x * (m - 1) + n / pow(x, m - 1)) / m;
	while (xx < x) {
		swap(x, xx);
		xx = (x * (m - 1) + n / pow(x, m - 1)) / m;
	}
	
	// Format the output to include a decimal point.
	string result_str = x.toString();

	if (result_str.length() <= CALC_PRECISION) {
		// The result is less than 1 (e.g., sqrt(0.5)).
		cout << "0.";
		// Prepend leading zeros to the fractional part.
		cout << string(CALC_PRECISION - result_str.length(), '0');
		// Print the fractional part, truncated to the desired output precision.
		cout << result_str.substr(0, OUTPUT_PRECISION) << endl;
	} else {
		// The result is >= 1.
		size_t integer_part_len = result_str.length() - CALC_PRECISION;
		string integer_part = result_str.substr(0, integer_part_len);
		string fractional_part = result_str.substr(integer_part_len, OUTPUT_PRECISION);
		cout << integer_part << "." << fractional_part << endl;
	}
	// === MODIFICATION END ===

	return 0;
}
