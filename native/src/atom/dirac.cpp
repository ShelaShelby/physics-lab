#include "dirac.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace atom {

// ---- the level (see the header for the formula, its source and its limits) ----
// Every quantity here is written WITHOUT subtracting two nearly equal numbers (0.10.8 stage 2): the naive forms lose digits in proportion to how small (Z alpha)^2 is (1 minus sqrt(1 - alpha^2) is 2.7e-5,
// so subtracting 1 from sqrt(1 - alpha^2) throws away 11 of 16 digits; for n = 6 and Z = 1 the energy of the naive form is off by 1e-10). Measured by the tests against a double-double reference.
namespace {
double deltaOf(double x2, double jh) { return x2 / (jh + std::sqrt(jh * jh - x2)); }   // (j + 1/2) - sqrt((j + 1/2)^2 - (Z alpha)^2) = (Z alpha)^2 / ((j + 1/2) + sqrt(...)): the difference of two nearly equal numbers, as a quotient
struct Core { double x2, delta, k, t, s; };         // (Z alpha)^2, delta, k = n - delta, t = (Z alpha)^2 / k^2, s = sqrt(1 + t) = 1 / f
Core core(int Z, int n, int twoJ) {
    Core c;
    const double x = Z * kAlpha, jh = (twoJ + 1) * 0.5;
    c.x2 = x * x;
    c.delta = deltaOf(c.x2, jh);
    c.k = n - c.delta;
    c.t = c.x2 / (c.k * c.k);
    c.s = std::sqrt(1 + c.t);
    return c;
}
}  // namespace
double diracDelta(int Z, int twoJ) { const double x = Z * kAlpha; return deltaOf(x * x, (twoJ + 1) * 0.5); }
double diracInfiniteEV(int Z, int n, int twoJ) {
    const Core c = core(Z, n, twoJ);
    return -kMecEV * c.t / (c.s * (1 + c.s));               // m c^2 (1/s - 1) = -m c^2 t / (s (1 + s)): no subtraction of 1
}
double diracCorrectionEV(int Z, int n, int twoJ) {
    // E_Dirac - E_NR = m c^2 [ (1/s - 1 + t/2) + ((Z alpha)^2 / 2 n^2 - t/2) ]. The first bracket is (s - 1)^2 (s + 2) / (2 s) (from t = s^2 - 1), with s - 1 = t / (s + 1); the second is
    // -((Z alpha)^2 / 2) delta (2n - delta) / (n^2 k^2) (from n^2 - k^2 = delta (2n - delta)). Both are products and quotients; each is of the order (Z alpha)^4, and they only partly cancel (a factor 4 at most).
    const Core c = core(Z, n, twoJ);
    const double sm1 = c.t / (c.s + 1), a = sm1 * sm1 * (c.s + 2) / (2 * c.s), b = 0.5 * c.x2 * c.delta * (2.0 * n - c.delta) / (double(n) * n * c.k * c.k);
    return kMecEV * (a - b);
}
double diracEV(int Z, int N, int n, int twoJ) { return levelEV(Z, N, n) + diracCorrectionEV(Z, n, twoJ); }

bool diracAllowed(int lLo, int jLo2, int lHi, int jHi2) {
    auto real = [](int l, int j2) { return l >= 0 && j2 >= 1 && (j2 == 2 * l + 1 || j2 == 2 * l - 1); };
    return real(lLo, jLo2) && real(lHi, jHi2) && std::abs(lHi - lLo) == 1 && std::abs(jHi2 - jLo2) <= 2;
}

// ---- the lines ----
std::vector<DiracLine> diracLines(int Z, int N) {
    const double R = rydM(Z, N) * Z * Z;                                  // the same number specLines starts from
    constexpr int kTop = 25;                                              // the highest upper level of specLines (5 + 20)
    static_assert(kTop == 5 + 20, "the upper level of specLines reaches 5 + 20");
    double corr[kTop + 1][2 * kTop];                                      // corr[n][j2]: the relativistic correction of level (n, j2 / 2), eV
    for (int n = 1; n <= kTop; n++)
        for (int j2 = 1; j2 < 2 * n; j2 += 2) corr[n][j2] = diracCorrectionEV(Z, n, j2);
    std::vector<DiracLine> out;
    for (int lo = 1; lo <= 5; lo++) {
        for (int hi = lo + 1; hi <= lo + 20; hi++) {
            std::vector<DiracLine> group;
            for (int jl = 1; jl < 2 * lo; jl += 2)
                for (int jh = 1; jh < 2 * hi; jh += 2) {
                    if (std::abs(jh - jl) > 2) continue;
                    bool found = false; int bl = 0, bh = 0;
                    for (int lL = (jl - 1) / 2; lL <= (jl + 1) / 2 && !found; lL++) {   // the two l that give this j: j - 1/2 and j + 1/2
                        if (lL > lo - 1) continue;
                        for (int lH = (jh - 1) / 2; lH <= (jh + 1) / 2 && !found; lH++)
                            if (lH <= hi - 1 && diracAllowed(lL, jl, lH, jh)) { found = true; bl = lL; bh = lH; }
                    }
                    if (!found) continue;
                    const double E = R * (1.0 / (lo * lo) - 1.0 / (hi * hi)) + (corr[hi][jh] - corr[lo][jl]);
                    group.push_back({lo, hi, bl, bh, jl, jh, kHcEVnm / E});
                }
            std::stable_sort(group.begin(), group.end(), [](const DiracLine& u, const DiracLine& v) { return u.lam < v.lam; });
            out.insert(out.end(), group.begin(), group.end());
        }
        for (int jl = 1; jl < 2 * lo; jl += 2) out.push_back({lo, 0, (jl - 1) / 2, -1, jl, 0, kHcEVnm / (R / (lo * lo) - corr[lo][jl])});   // the limit: the electron leaves from this level
    }
    return out;
}

std::vector<SpecLine> specLinesFor(int Z, int N, bool dirac) {
    if (!dirac) return specLines(Z, N);
    std::vector<SpecLine> out;
    for (const DiracLine& l : diracLines(Z, N)) out.push_back({l.lo, l.hi, l.lam});
    return out;
}
std::vector<SpecLine> visibleLinesFor(int Z, int N, bool dirac) {
    if (!dirac) return visibleLines(Z, N);
    std::vector<SpecLine> v;
    for (const SpecLine& l : specLinesFor(Z, N, true)) if (l.hi != 0 && isVisibleNm(l.lam)) v.push_back(l);
    return v;
}

// ---- the levels in words ----
namespace {
std::string jText(int j2) { return std::to_string(j2) + "/2"; }
std::string shells(int n, int j2) {                                      // the subshells that share this (n, j): l = j - 1/2 and j + 1/2, where l < n
    std::string s;
    for (int l = (j2 - 1) / 2; l <= (j2 + 1) / 2; l++) if (l <= n - 1) s += (s.empty() ? "" : ", ") + std::to_string(n) + "spdfghi"[l];
    return s;
}
std::string gapText(double eV) {                                         // a small energy with its frequency
    const double a = std::fabs(eV);
    const std::string e = a < 1e-3 ? jsNum(a * 1e6, 4) + " µeV" : a < 1 ? jsNum(a * 1e3, 4) + " meV" : jsNum(a, 4) + " eV";
    return e + " (" + jsNum(a / kPlanckEVs * 1e-9, 4) + " GHz)";
}
}  // namespace
std::vector<std::string> diracLevelRows(int Z, int N) {
    std::vector<std::string> rows;
    for (int n = 1; n <= 3; n++) {
        for (int j2 = 1; j2 < 2 * n; j2 += 2)
            rows.push_back("n = " + std::to_string(n) + ": " + shells(n, j2) + " (j = " + jText(j2) + "): −" + jsNum(-diracEV(Z, N, n, j2), 8) + " eV");
        if (n >= 2) rows.push_back("n = " + std::to_string(n) + " gap (j = " + jText(2 * n - 1) + " above j = 1/2): " + gapText(diracCorrectionEV(Z, n, 2 * n - 1) - diracCorrectionEV(Z, n, 1)));
    }
    return rows;
}

// ---- the words ----
const char* const kDiracLabel = "Relativity (Dirac levels)";
std::string diracExplain() {
    return "Changes the energy levels and lines in the two spectrum cards: a level then depends on j as well as n, so lines split. Does not change the pictures (still non-relativistic), the readouts or the photon jumps, "
           "and adds no Lamb shift and no size of the nucleus.";
}
std::string diracOffNote() { return "the lab uses the non-relativistic levels, one for each n, as it always did."; }
std::string diracOnNote(int Z) {
    return "On: the levels and lines come from the Dirac equation for one electron round a point nucleus. A level now depends on j as well as n, so lines split, and the split grows with Z (for " + std::string(name(Z)) + ": 2p j = 3/2 above 2p j = 1/2 by " +
           gapText(diracCorrectionEV(Z, 2, 3) - diracCorrectionEV(Z, 2, 1)) + "). The clouds, orbitals, outlines and radial graph are still the non-relativistic wave functions: only the energies and lines are Dirac. "
           "The main energy keeps the reduced mass; the relativistic correction is worked out for an infinitely heavy nucleus, and further recoil effects are not included. "
           "In Dirac theory 2s and 2p (j = 1/2) have exactly the same energy; in reality the Lamb shift (a QED effect) separates them. QED and the size of the nucleus also move every line a little, so NIST's measured lines still differ slightly from these: "
           "for Lyman-alpha, NIST's wavelength lies between the Dirac line and the non-relativistic one, for every ion NIST has. Photon jumps, their ▼ marks and the energy readouts keep the non-relativistic values (the lab's orbitals have no spin or j).";
}

}  // namespace atom
