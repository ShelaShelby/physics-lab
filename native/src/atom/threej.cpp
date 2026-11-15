// The exact Wigner 3j symbol (threej.h). Moved unchanged from src/app/calc.cpp in 0.54.0.
#include "atom/threej.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <vector>

namespace atom {
namespace {
// 0.50.2: a whole number of any size (base 2^32, lowest limb first), just what the 3j symbol needs: times and exactly-divided-by a small number, + and -.
struct Big {
    std::vector<std::uint32_t> d;
    bool zero() const { return d.empty(); }
    void mul(std::uint32_t m) {
        std::uint64_t carry = 0;
        for (std::uint32_t& x : d) { const std::uint64_t t = std::uint64_t(x) * m + carry; x = static_cast<std::uint32_t>(t); carry = t >> 32; }
        if (carry) d.push_back(static_cast<std::uint32_t>(carry));
        if (m == 0) d.clear();
    }
    void div(std::uint32_t m) {                                                 // exact division (the caller knows m divides it)
        std::uint64_t rem = 0;
        for (std::size_t i = d.size(); i-- > 0;) { const std::uint64_t t = (rem << 32) | d[i]; d[i] = static_cast<std::uint32_t>(t / m); rem = t % m; }
        while (!d.empty() && d.back() == 0) d.pop_back();
    }
    static int cmp(const Big& a, const Big& b) {
        if (a.d.size() != b.d.size()) return a.d.size() < b.d.size() ? -1 : 1;
        for (std::size_t i = a.d.size(); i-- > 0;) if (a.d[i] != b.d[i]) return a.d[i] < b.d[i] ? -1 : 1;
        return 0;
    }
    void add(const Big& b) {
        if (d.size() < b.d.size()) d.resize(b.d.size(), 0);
        std::uint64_t carry = 0;
        for (std::size_t i = 0; i < d.size(); i++) {
            const std::uint64_t t = std::uint64_t(d[i]) + (i < b.d.size() ? b.d[i] : 0) + carry;
            d[i] = static_cast<std::uint32_t>(t); carry = t >> 32;
        }
        if (carry) d.push_back(static_cast<std::uint32_t>(carry));
    }
    void sub(const Big& b) {                                                    // *this >= b
        std::int64_t borrow = 0;
        for (std::size_t i = 0; i < d.size(); i++) {
            std::int64_t t = std::int64_t(d[i]) - (i < b.d.size() ? b.d[i] : 0) - borrow;
            borrow = t < 0 ? 1 : 0;
            if (t < 0) t += std::int64_t(1) << 32;
            d[i] = static_cast<std::uint32_t>(t);
        }
        while (!d.empty() && d.back() == 0) d.pop_back();
    }
    // the value as m × 2^e with 0.5 <= m < 1, from its top three limbs (96 bits: two roundings, within 2.2e-16)
    double mantissa(long long& e) const {
        const std::size_t n = d.size();
        const auto limb = [&](std::size_t k) { return k < n ? static_cast<double>(d[n - 1 - k]) : 0.0; };
        const double x = (limb(0) * 4294967296.0 + limb(1)) * 4294967296.0 + limb(2);
        int ee = 0;
        const double m = std::frexp(x, &ee);
        e = 32LL * (static_cast<long long>(n) - 3) + ee;
        return m;
    }
};
// the power of the prime p in n! (Legendre's formula)
long long primePower(int n, int p) { long long e = 0; for (long long q = p; q <= n; q *= p) e += n / q; return e; }

// The Wigner 3j symbol from Racah's formula (A. R. Edmonds, "Angular Momentum in Quantum Mechanics", eq. (3.7.3); DLMF 34.2.4), all arguments doubled (2j, 2m) so
// half-integers are exact. 0 when a selection rule fails (m1 + m2 + m3 != 0, the triangle rule, |m| > j).
// 0.50.2: done exactly, so it keeps its digits at large j (0.50.1 summed the alternating series in doubles: 1e-9 wrong at j = 40, and an overflow at j = 50).
// With N = the factorials' common multiple M times the alternating sum (a whole number, done with Big) and R = Δ F / M² (Δ the triangle factor, F the six (j ± m)!)
// written as powers of primes, the symbol is ± N √R: one rounding for N and one or two for each prime's power, so about 14 good digits (tested to j = 100).
}  // namespace

double threeJ(int j1, int j2, int j3, int m1, int m2, int m3) {
    if (m1 + m2 + m3 != 0) return 0;
    if (j3 < std::abs(j1 - j2) || j3 > j1 + j2 || ((j1 + j2 + j3) % 2) != 0) return 0;
    if (std::abs(m1) > j1 || std::abs(m2) > j2 || std::abs(m3) > j3) return 0;
    // the sum over k of (-1)^k / [k! (A1+k)! (A2+k)! (B1-k)! (B2-k)! (B3-k)!]
    const int A1 = (j3 - j2 + m1) / 2, A2 = (j3 - j1 - m2) / 2, B1 = (j1 + j2 - j3) / 2, B2 = (j1 - m1) / 2, B3 = (j2 + m2) / 2;
    const int kmin = std::max({0, -A1, -A2}), kmax = std::min({B1, B2, B3});
    if (kmin > kmax) return 0;
    // T_k = M / (that denominator), M = kmax! (A1+kmax)! (A2+kmax)! (B1-kmin)! (B2-kmin)! (B3-kmin)!: T_kmin = the three rising products, then
    // T_(k+1) = T_k (B1-k)(B2-k)(B3-k) / ((k+1)(A1+k+1)(A2+k+1)) (each division exact: T_(k+1) is whole and the divisors go one at a time)
    Big T;
    T.d = {1};
    for (int x = kmin + 1; x <= kmax; x++) T.mul(static_cast<std::uint32_t>(x));
    for (int x = A1 + kmin + 1; x <= A1 + kmax; x++) T.mul(static_cast<std::uint32_t>(x));
    for (int x = A2 + kmin + 1; x <= A2 + kmax; x++) T.mul(static_cast<std::uint32_t>(x));
    Big pos, neg;
    for (int k = kmin;; k++) {
        (k % 2 ? neg : pos).add(T);
        if (k == kmax) break;
        T.mul(static_cast<std::uint32_t>(B1 - k)); T.mul(static_cast<std::uint32_t>(B2 - k)); T.mul(static_cast<std::uint32_t>(B3 - k));
        T.div(static_cast<std::uint32_t>(k + 1)); T.div(static_cast<std::uint32_t>(A1 + k + 1)); T.div(static_cast<std::uint32_t>(A2 + k + 1));
    }
    double sign = 1;
    if (Big::cmp(pos, neg) >= 0) pos.sub(neg); else { neg.sub(pos); pos = neg; sign = -1; }
    if (pos.zero()) return 0;
    // R = Δ F / M² as prime powers: Δ = B1! ((j1-j2+j3)/2)! ((-j1+j2+j3)/2)! / ((j1+j2+j3)/2 + 1)!, F = (j1±m1)! (j2±m2)! (j3±m3)!
    const int up[] = {B1, (j1 - j2 + j3) / 2, (-j1 + j2 + j3) / 2, (j1 + m1) / 2, (j1 - m1) / 2, (j2 + m2) / 2, (j2 - m2) / 2, (j3 + m3) / 2, (j3 - m3) / 2};
    const int down = (j1 + j2 + j3) / 2 + 1;
    const int twice[] = {kmax, A1 + kmax, A2 + kmax, B1 - kmin, B2 - kmin, B3 - kmin};
    int top = down;
    for (int x : up) top = std::max(top, x);
    for (int x : twice) top = std::max(top, x);
    long long e2 = 0;
    double m = pos.mantissa(e2);
    std::vector<char> composite(static_cast<std::size_t>(top) + 1, 0);
    for (int p = 2; p <= top; p++) {
        if (composite[p]) continue;
        for (long long q = 1LL * p * p; q <= top; q += p) composite[static_cast<std::size_t>(q)] = 1;
        long long e = -primePower(down, p);
        for (int x : up) e += primePower(x, p);
        for (int x : twice) e -= 2 * primePower(x, p);
        if (e == 0) continue;
        if (p == 2) { e2 += e / 2; if (e % 2) m *= e > 0 ? std::sqrt(2.0) : std::sqrt(0.5); continue; }
        double half = e / 2.0;                                                  // p^(e/2) in pieces small enough not to overflow
        while (half != 0) {
            const double piece = std::max(-60.0, std::min(60.0, half));
            m *= std::pow(static_cast<double>(p), piece);
            half -= piece;
            int ee = 0;
            m = std::frexp(m, &ee);
            e2 += ee;
        }
    }
    const int phase = (j1 - j2 - m3) / 2;                                       // (-1)^(j1 - j2 - m3)
    if (e2 > 2000 || e2 < -2000) return e2 > 0 ? INFINITY : 0;
    return ((phase % 2) != 0 ? -1.0 : 1.0) * sign * std::ldexp(m, static_cast<int>(e2));
}


double clebschGordan(int j1, int m1, int j2, int m2, int J, int M) {
    const double w = threeJ(j1, j2, J, m1, m2, -M);
    const int ph = (j1 - j2 + M) / 2;                                           // (j1 - j2 + M) is even whenever w != 0
    return ((ph % 2) != 0 ? -1.0 : 1.0) * std::sqrt(J + 1.0) * w;
}

}  // namespace atom
