#include "atom/hfs.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "atom/atom_core.h"

namespace atom::hfs {

namespace {
const Nucleus kTable[] = {
#include "atom/hfs_data.inc"
};
}  // namespace

const Nucleus* find(int Z, int A) {
    const auto it = std::lower_bound(std::begin(kTable), std::end(kTable), std::make_pair(Z, A), [](const Nucleus& n, const std::pair<int, int>& k) { return std::make_pair(n.Z, n.A) < k; });
    return it != std::end(kTable) && it->Z == Z && it->A == A ? &*it : nullptr;
}
int tableSize() { return static_cast<int>(std::size(kTable)); }

double aConstEV(int Z, int N, int n, int l, int twoJ) {
    const Nucleus* nu = find(Z, Z + N);
    if (!nu || nu->twoI <= 0 || n < 1 || l < 0 || l >= n || twoJ < 1) return 0;
    const double j = twoJ / 2.0, gI = nu->mu / (nu->twoI / 2.0);
    const double mr = 1 / (1 + 1 / nuclearMassMe(Z, N));                    // m_r / m_e
    double a = std::pow(kAlpha, 4) * kMecEV * gI / kMpOverMe * Z * Z * Z / (double(n) * n * n * j * (j + 1) * (2 * l + 1)) * mr * mr * mr;
    if (l == 0) a *= (1 + kAe) * relativisticFactorS(Z, n);
    return a;
}
double relativisticFactorS(int Z, int n) {
    const double za2 = std::pow(Z * kAlpha, 2), g = std::sqrt(1 - za2);
    const double N = std::sqrt(double(n) * n - 2 * za2 * (n - 1) / (1 + g));
    return double(n) * n * n * (1 + 2 * std::sqrt(1 - za2 / (N * N))) / (N * N * N * g * (4 * g * g - 1));
}
std::vector<Level> levels(int Z, int N, int n, int l, int twoJ) {
    std::vector<Level> out;
    const Nucleus* nu = find(Z, Z + N);
    const double a = aConstEV(Z, N, n, l, twoJ);
    if (!nu || a == 0) return out;
    const int twoI = nu->twoI;
    const double I = twoI / 2.0, j = twoJ / 2.0;
    for (int twoF = std::abs(twoI - twoJ); twoF <= twoI + twoJ; twoF += 2) {
        const double F = twoF / 2.0;
        out.push_back({twoF, a / 2 * (F * (F + 1) - I * (I + 1) - j * (j + 1))});
    }
    std::sort(out.begin(), out.end(), [](const Level& x, const Level& y) { return x.eEV < y.eEV; });
    return out;
}
double splittingEV(int Z, int N, int n, int l, int twoJ) {
    const std::vector<Level> v = levels(Z, N, n, l, twoJ);
    return v.size() < 2 ? 0 : v.back().eEV - v.front().eEV;
}

namespace {
std::string spinText(int twoI) { return twoI % 2 ? std::to_string(twoI) + "/2" : std::to_string(twoI / 2); }
std::string num(double v, int sig) { char b[48]; std::snprintf(b, sizeof b, "%.*g", sig, v); return b; }
}  // namespace

std::string groundText(int Z, int N) {
    const Nucleus* nu = find(Z, Z + N);
    if (!nu) {
        if (Z % 2 == 0 && N % 2 == 0) return "Hyperfine structure: none. This nucleus has spin 0 (like every nucleus with even numbers of protons and neutrons), so it has no magnetic moment to split the levels.";
        return "Hyperfine structure: not shown. The IAEA table of nuclear magnetic moments has no measured value for this nucleus.";
    }
    const std::vector<Level> v = levels(Z, N, 1, 0, 1);
    if (v.size() < 2) return "Hyperfine structure: none.";
    const double dE = v.back().eEV - v.front().eEV, f = dE * kEVtoMHz, lam = 299792458.0 / (f * 1e6);
    std::string t = "Hyperfine structure: the nucleus has spin " + spinText(nu->twoI) + (nu->spinTentative ? " (tentative)" : "") + " and a magnetic moment of " +
                    (nu->sign == 0 ? "" : nu->mu > 0 ? "+" : "−") + num(std::fabs(nu->mu), 6) + " nuclear magnetons" +
                    (nu->sign == 2 ? " (sign assumed)" : nu->sign == 0 ? " (sign not measured)" : "") + " (IAEA). It splits the ground level 1s into F = " +
                    spinText(v.front().twoF) + " and F = " + spinText(v.back().twoF) + (nu->sign == 0 ? "" : " (lower first)") + ", ";
    t += dE >= 1e-3 ? num(dE * 1e3, 4) + " meV" : num(dE * 1e6, 4) + " µeV";
    t += " apart: a line of " + (f >= 1e6 ? num(f / 1e6, 4) + " THz" : f >= 1e4 ? num(f / 1e3, 5) + " GHz" : num(f, 5) + " MHz") + ", wavelength ";
    t += lam >= 1e-2 ? num(lam * 100, 4) + " cm" : lam >= 1e-3 ? num(lam * 1e3, 4) + " mm" : num(lam * 1e6, 4) + " µm";
    t += ". Higher levels split less (about as 1/n³). Magnetic dipole only, with the relativistic (Breit) factor, which grows with Z (" + (relativisticFactorS(Z, 1) < 1.01 ? "+" + num((relativisticFactorS(Z, 1) - 1) * 100, 2) + " %" : "× " + num(relativisticFactorS(Z, 1), 3)) +
         " here), but a point nucleus: close for light nuclei. For heavy ones the nucleus's size makes the real splitting smaller: its spread-out charge by 4.6 % for holmium-165 "
         "and 11.1 % for bismuth-209, its spread-out magnetism by another 0.9 % and 1.2 % (Shabaev et al. 1997). With the QED terms as well, the measured holmium-165 line is "
         "6.7 % below this lab's";
    // 0.46.3 (outside review: "about 5 % for holmium" read as the whole gap, which the test measures as 6.7 %; "11 % for bismuth" had no source): delta and epsilon from Shabaev,
    // Tomaselli, Kuehl, Artemyev, Yerokhin, Phys. Rev. A 56, 252 (1997), Table 3 (data/moments/shabaev_et_al_1997.pdf): 165Ho delta 0.0456, epsilon 0.0089; 209Bi delta 0.1111,
    // epsilon 0.0118. The 6.7 %: 1 - 2.1645 eV (measured, their Table 4) / the lab's 2.3189 eV; tests/test_hfs.inc checks it against the lab's own value.
    if (Z == 1 && N == 0) t += "; the measured line is 1,420.406 MHz, the famous 21 cm line of radio astronomy";
    return t + ".";
}
}  // namespace atom::hfs
