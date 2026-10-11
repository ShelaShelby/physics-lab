#include "quark_wander.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>

namespace atom {
namespace {

// The radial profile drawn for each quark before the three are centred: density ∝ e^(−r/b) out to kRawCut, so the distance has the
// distribution r² e^(−r/b) (the Gamma(3) law). The scales b are FITTED (not measured): the three are then centred on the nucleon and pulled in
// to stay inside kQuarkCutR, which shrinks every distance and pulls the u and d means together, so b is chosen so that, AFTER that, the mean
// distance is kQuarkMeanR and mean d / mean u = 1.7^(1/3) (checked by atom_tests, testQuarkWander; the fit is in docs/QUARKS.md).
constexpr double kRawCut = 0.8;                       // fm
constexpr double kB[2][2] = {{0.128, 0.314},          // proton: b_u, b_d (fm)
                             {0.104, 0.207}};         // neutron: b_u, b_d
constexpr int kTab = 256;

double gammaCdf(double x) { return 1 - std::exp(-x) * (1 + x + 0.5 * x * x); }

// r for the quantile u of the cut profile with scale b: a table made once, read with linear steps (continuous, so the quark never jumps)
struct Inv { std::array<double, kTab + 1> r; };
Inv makeInv(double b) {
    Inv v{};
    const double X = kRawCut / b, top = gammaCdf(X);
    for (int i = 0; i <= kTab; i++) {
        const double want = top * i / kTab;
        double lo = 0, hi = X;
        for (int k = 0; k < 60; k++) { const double mid = 0.5 * (lo + hi); (gammaCdf(mid) < want ? lo : hi) = mid; }
        v.r[i] = 0.5 * (lo + hi) * b;
    }
    return v;
}
double invAt(const Inv& v, double u) {
    const double s = std::fmin(std::fmax(u, 0.0), 1.0) * kTab;
    const int i = std::min(kTab - 1, int(s));
    return v.r[i] + (v.r[i + 1] - v.r[i]) * (s - i);
}
const Inv& inv(int nucleon, int flavour) {
    static const Inv t[2][2] = {{makeInv(kB[0][0]), makeInv(kB[0][1])}, {makeInv(kB[1][0]), makeInv(kB[1][1])}};
    return t[nucleon][flavour];
}

double hash(double x) {   // a fixed number in [0, 1) for each seed (splitmix64 of the seed's bits: no trigonometry, it runs for every nucleon every frame)
    std::uint64_t z;
    std::memcpy(&z, &x, sizeof z);
    z += 0x9E3779B97F4A7C15ull;
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    z ^= z >> 31;
    return double(z >> 11) * (1.0 / 9007199254740992.0);
}
double tri(double x) { return 2 * std::fabs(x - std::floor(x + 0.5)); }                           // 0 → 1 → 0 once per unit: spends equal time at every value

}  // namespace

QuarkWander accurateQuarks(const Nucleon& q, double t) {
    QuarkWander W{};
    W.f[0] = 'u'; W.f[1] = q.isP ? 'u' : 'd'; W.f[2] = 'd';
    double X[3][3];
    for (int i = 0; i < 3; i++) {
        // each quark: its own slow, smooth wander (seeds from q.ph). The quantile of its distance runs evenly up and down, so over time the distance follows
        // the profile above. The direction is a fixed start turned by three steady turns (about z, then x, then z) at unrelated rates: it covers the sphere
        // with no preferred side and never moves faster than the turns (a height swept evenly in −1..1 would whip round near the poles).
        const double s = q.ph * 7.31 + i * 3.17;
        const double wr = 0.03 + 0.015 * hash(s + 1);
        const double u = tri(t * wr + 0.4 * std::sin(t * 0.23 + 6.28 * hash(s + 5)) / 6.28 + hash(s + 6));
        const double r = invAt(inv(q.isP ? 0 : 1, W.f[i] == 'u' ? 0 : 1), u);
        const double cc0 = 1 - 2 * hash(s + 7), sc0 = std::sqrt(std::fmax(0.0, 1 - cc0 * cc0));   // the start's height: even in −1..1
        const double g = 6.2832 * (t * (0.025 + 0.015 * hash(s + 2)) + hash(s + 8)), b = 6.2832 * (t * (0.02 + 0.015 * hash(s + 3)) + hash(s + 9)),
                     a = 6.2832 * (t * (0.015 + 0.015 * hash(s + 4)) * (hash(s + 10) < 0.5 ? 1 : -1) + hash(s + 11));
        double x = sc0 * std::cos(g), y = sc0 * std::sin(g), z = cc0;   // Rz(g) of the start (sc0, 0, cc0)
        const double y1 = y * std::cos(b) - z * std::sin(b), z1 = y * std::sin(b) + z * std::cos(b);   // Rx(b)
        y = y1; z = z1;
        const double x2 = x * std::cos(a) - y * std::sin(a), y2 = x * std::sin(a) + y * std::cos(a);  // Rz(a)
        X[i][0] = r * x2; X[i][1] = r * y2; X[i][2] = r * z;
    }
    double m = 0;   // centre the three on the nucleon (their mean is its middle), then pull all three in by one factor when one is far out (keeps the centre)
    for (int i = 0; i < 3; i++) {
        double d2 = 0;
        for (int a = 0; a < 3; a++) { W.x[i][a] = X[i][a] - (X[0][a] + X[1][a] + X[2][a]) / 3; d2 += W.x[i][a] * W.x[i][a]; }
        m = std::fmax(m, std::sqrt(d2));
    }
    constexpr double m0 = 0.5;   // fm: below this nothing is pulled; above, the farthest quark goes smoothly toward kQuarkCutR and never past it
    if (m > m0) {
        const double g = m0 + (kQuarkCutR - m0) * std::tanh((m - m0) / (kQuarkCutR - m0)), k = g / m;
        for (auto& p : W.x) for (double& c : p) c *= k;
    }
    return W;
}

}  // namespace atom
