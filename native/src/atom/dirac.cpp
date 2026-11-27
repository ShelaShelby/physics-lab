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
Core core(int Z, int n, int twoJ, double alpha = kAlpha) {
    Core c;
    const double x = Z * alpha, jh = (twoJ + 1) * 0.5;
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
double diracInfiniteRy(int Z, int n, int twoJ, double alpha) {       // 0.86.0: E_D / (h c R_inf), h c R_inf = alpha^2 m c^2 / 2 (YS15's table's unit)
    const Core c = core(Z, n, twoJ, alpha);
    return -2 * c.t / (alpha * alpha * c.s * (1 + c.s));
}
double diracCorrectionEV(int Z, int n, int twoJ) {
    // E_Dirac - E_NR = m c^2 [ (1/s - 1 + t/2) + ((Z alpha)^2 / 2 n^2 - t/2) ]. The first bracket is (s - 1)^2 (s + 2) / (2 s) (from t = s^2 - 1), with s - 1 = t / (s + 1); the second is
    // -((Z alpha)^2 / 2) delta (2n - delta) / (n^2 k^2) (from n^2 - k^2 = delta (2n - delta)). Both are products and quotients; each is of the order (Z alpha)^4, and they only partly cancel (a factor 4 at most).
    const Core c = core(Z, n, twoJ);
    const double sm1 = c.t / (c.s + 1), a = sm1 * sm1 * (c.s + 2) / (2 * c.s), b = 0.5 * c.x2 * c.delta * (2.0 * n - c.delta) / (double(n) * n * c.k * c.k);
    return kMecEV * (a - b);
}
double diracMassRy(int Z, int n, int twoJ, double x, double alpha) {
    // Eq. (3) over h c R_inf = m c^2 alpha^2 / 2: x Z^2 / N^2 - (m_r / m) x^2 Z^2 / n^2, m_r / m = 1 / (1 + x); N^2 = k^2 + (Z alpha)^2 with k = n_r + gamma (Core)
    const Core c = core(Z, n, twoJ, alpha);
    const double N2 = c.k * c.k + c.x2;
    return x * double(Z) * Z / N2 - x * x * double(Z) * Z / ((1 + x) * double(n) * n);
}
double diracRecoilEV(int Z, int N, int n, int twoJ) {
    // E_D + E_M = (E_NR,inf + corr) + m c^2 x (Z a)^2 / (2 N^2) - m_r c^2 x^2 (Z a)^2 / (2 n^2), and levelEV = E_NR,inf + m_r c^2 x (Z a)^2 / (2 n^2) (m - m_r = m_r x).
    // The rest: (Z a)^2 c^2 x / 2 [m / N^2 - m_r (1 + x) / n^2] = m c^2 x (Z a)^2 / 2 (n^2 - N^2) / (n^2 N^2), and n^2 - N^2 = 2 n_r (Z a)^2 / (|kappa| + gamma)
    // (from |kappa| - gamma = (Z a)^2 / (|kappa| + gamma)): m c^2 x (Z a)^4 n_r / (n^2 N^2 (|kappa| + gamma)).
    const Core c = core(Z, n, twoJ);
    const double ak = (twoJ + 1) * 0.5, nr = n - ak, gam = ak - c.delta, N2 = c.k * c.k + c.x2, x = 1 / nuclearMassMe(Z, N);
    return kMecEV * x * c.x2 * c.x2 * nr / (double(n) * n * N2 * (ak + gam));
}
double diracEV(int Z, int N, int n, int twoJ) { return levelEV(Z, N, n) + diracCorrectionEV(Z, n, twoJ) + diracRecoilEV(Z, N, n, twoJ); }

// ---- 0.49.3: the Lamb shift (see the header) ----
namespace {
struct LambRow { int Z, A; const char* sym; double rFm, s1s, s2s, s2p1, s2p3; DiracPrinted dirac[3]; };   // shifts in h c R_inf; 0.86.0: the DIRAC row
#include "lamb_data.inc"
const LambRow* lambRow(int Z) { return Z >= 1 && Z <= 110 && kLambRows[Z - 1].Z == Z ? &kLambRows[Z - 1] : nullptr; }
}  // namespace
const DiracPrinted* diracPrinted(int Z) { const LambRow* r = lambRow(Z); return r ? r->dirac : nullptr; }
bool lambKnown(int Z, int n) { return (n == 1 || n == 2) && lambRow(Z) != nullptr; }
double lambShiftEV(int Z, int n, int l, int twoJ) {
    const LambRow* r = lambRow(Z);
    if (!r || n < 1 || n > 2) return 0;
    double v = 0;
    if (n == 1 && l == 0 && twoJ == 1) v = r->s1s;
    else if (n == 2 && l == 0 && twoJ == 1) v = r->s2s;
    else if (n == 2 && l == 1 && twoJ == 1) v = r->s2p1;
    else if (n == 2 && l == 1 && twoJ == 3) v = r->s2p3;
    return v * kRydInf;
}
int lambIsotopeA(int Z) { const LambRow* r = lambRow(Z); return r ? r->A : 0; }
double lambRadiusFm(int Z) { const LambRow* r = lambRow(Z); return r ? r->rFm : NAN; }
double diracQedEV(int Z, int N, int n, int l, int twoJ) { return diracEV(Z, N, n, twoJ) + lambShiftEV(Z, n, l, twoJ); }

bool diracAllowed(int lLo, int jLo2, int lHi, int jHi2) {
    auto real = [](int l, int j2) { return l >= 0 && j2 >= 1 && (j2 == 2 * l + 1 || j2 == 2 * l - 1); };
    return real(lLo, jLo2) && real(lHi, jHi2) && std::abs(lHi - lLo) == 1 && std::abs(jHi2 - jLo2) <= 2;
}

// ---- the lines ----
std::vector<DiracLine> diracLines(int Z, int N) {
    // 0.49.3: every allowed pair of (l, j) levels is worked out with its Lamb shift (diracQedEV); pairs of exactly the same energy are ONE line, named by the first pair (the smallest l), as before.
    // Without a Lamb shift (n >= 3, Z > 110) the energy depends on n and j only, so this gives the lines of 0.10.8 bit for bit; with it, 2s1/2 and 2p1/2 differ and their lines separate.
    constexpr int kTop = 25;                                              // the highest upper level of specLines (5 + 20)
    static_assert(kTop == 5 + 20, "the upper level of specLines reaches 5 + 20");
    const double R = rydM(Z, N) * Z * Z;                                  // the same number specLines starts from
    double corr[kTop + 1][2 * kTop];                                      // corr[n][j2]: the relativistic correction of level (n, j2 / 2), eV
    for (int n = 1; n <= kTop; n++)
        for (int j2 = 1; j2 < 2 * n; j2 += 2) corr[n][j2] = diracCorrectionEV(Z, n, j2) + diracRecoilEV(Z, N, n, j2);   // (0.91.0: with the recoil beyond the reduced mass)
    auto shift = [&](int n, int l, int j2) { return corr[n][j2] + lambShiftEV(Z, n, l, j2); };   // the level minus its non-relativistic main term
    auto exists = [](int n, int l, int j2) { return l >= 0 && l < n && j2 >= 1 && (j2 == 2 * l + 1 || j2 == 2 * l - 1); };
    std::vector<DiracLine> out;
    for (int lo = 1; lo <= 5; lo++) {
        for (int hi = lo + 1; hi <= lo + 20; hi++) {
            std::vector<DiracLine> group;
            std::vector<double> groupE;
            for (int jl = 1; jl < 2 * lo; jl += 2)
                for (int jh = 1; jh < 2 * hi; jh += 2) {
                    if (std::abs(jh - jl) > 2) continue;
                    for (int lL = (jl - 1) / 2; lL <= (jl + 1) / 2; lL++) {   // the two l that give this j: j - 1/2 and j + 1/2
                        if (!exists(lo, lL, jl)) continue;
                        for (int lH = (jh - 1) / 2; lH <= (jh + 1) / 2; lH++) {
                            if (!exists(hi, lH, jh) || !diracAllowed(lL, jl, lH, jh)) continue;
                            const double E = R * (1.0 / (lo * lo) - 1.0 / (hi * hi)) + (shift(hi, lH, jh) - shift(lo, lL, jl));
                            bool same = false;
                            for (std::size_t k = 0; k < group.size(); k++) if (group[k].jLo2 == jl && group[k].jHi2 == jh && groupE[k] == E) same = true;
                            if (!same) { group.push_back({lo, hi, lL, lH, jl, jh, kHcEVnm / E}); groupE.push_back(E); }
                        }
                    }
                }
            std::stable_sort(group.begin(), group.end(), [](const DiracLine& u, const DiracLine& v) { return u.lam < v.lam; });
            out.insert(out.end(), group.begin(), group.end());
        }
        for (int jl = 1; jl < 2 * lo; jl += 2) {                          // the limit: the electron leaves from this level (by j; with a Lamb shift also by l)
            double prevE = NAN;
            for (int lL = (jl - 1) / 2; lL <= (jl + 1) / 2; lL++) {
                if (!exists(lo, lL, jl)) continue;
                const double E = R / (lo * lo) - shift(lo, lL, jl);
                if (E == prevE) continue;
                out.push_back({lo, 0, lL, -1, jl, 0, kHcEVnm / E});
                prevE = E;
            }
        }
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
    const std::string e = a < 1e-3 ? numText(a * 1e6, 4) + " µeV" : a < 1 ? numText(a * 1e3, 4) + " meV" : numText(a, 4) + " eV";
    return e + " (" + numText(a / kPlanckEVs * 1e-9, 4) + " GHz)";
}
}  // namespace
std::vector<std::string> diracLevelRows(int Z, int N) {
    // 0.49.3: where the Lamb shift is known (n = 1, 2) every (l, j) is its own row, and the Lamb gap 2s - 2p1/2 has a row; for n = 3 the levels of the same j share one energy, as in Dirac theory.
    std::vector<std::string> rows;
    for (int n = 1; n <= 3; n++) {
        const bool qed = lambKnown(Z, n);
        for (int j2 = 1; j2 < 2 * n; j2 += 2) {
            if (!qed) { rows.push_back("n = " + std::to_string(n) + ": " + shells(n, j2) + " (j = " + jText(j2) + "): −" + numText(-diracEV(Z, N, n, j2), 8) + " eV"); continue; }
            for (int l = (j2 - 1) / 2; l <= (j2 + 1) / 2 && l < n; l++)
                rows.push_back("n = " + std::to_string(n) + ": " + std::to_string(n) + "spdfghi"[l] + " (j = " + jText(j2) + "): −" + numText(-diracQedEV(Z, N, n, l, j2), 8) + " eV");
        }
        if (n >= 2) {
            const int top = 2 * n - 1, topL = n - 1;
            rows.push_back("n = " + std::to_string(n) + " gap (j = " + jText(top) + " above j = 1/2): " +
                           gapText(qed ? diracQedEV(Z, N, n, topL, top) - diracQedEV(Z, N, n, 1, 1) : diracCorrectionEV(Z, n, top) - diracCorrectionEV(Z, n, 1)));
        }
        if (n == 2 && qed) rows.push_back("n = 2 Lamb shift (2s above 2p, both j = 1/2): " + gapText(lambShiftEV(Z, 2, 0, 1) - lambShiftEV(Z, 2, 1, 1)));
    }
    return rows;
}

// ---- the words ----
const char* const kDiracLabel = "Relativity (Dirac levels)";
std::string diracExplain() {
    return "Changes the energy levels and lines in the two spectrum cards: a level then depends on j as well as n, and for n = 1 and 2 on l too (the Lamb shift, with the size of the nucleus: Yerokhin & Shabaev 2015), "
           "so lines split. The cloud has its own setting (Cloud model: Dirac or non-relativistic pictures).";   // 0.46.1: was "or the readouts", untrue from Z = 27 since 0.46
}
std::string diracOffNote() { return "the spectrum cards use the non-relativistic levels, one for each n (fine structure and the Lamb shift left out)."; }
std::string diracOnNote(int Z) {
    // 0.49.3: with the Lamb shift of n = 1, 2 (where the source has it) and the Cloud model setting
    const bool lamb = lambKnown(Z, 2);
    return "On: the levels and lines come from the Dirac equation for one electron round a point nucleus. A level now depends on j as well as n, so lines split, and the split grows with Z (for " + std::string(name(Z)) + ": 2p j = 3/2 above 2p j = 1/2 by " +
           gapText(lamb ? diracQedEV(Z, 0, 2, 1, 3) - diracQedEV(Z, 0, 2, 1, 1) : diracCorrectionEV(Z, 2, 3) - diracCorrectionEV(Z, 2, 1)) + "). " +
           (lamb ? "The Lamb shift (QED: self-energy and vacuum polarisation, with the finite size of the nucleus) is added to the levels n = 1 and 2 from Yerokhin & Shabaev, J. Phys. Chem. Ref. Data 44, 033103 (2015), "
                   "computed there for one isotope (" + std::to_string(lambIsotopeA(Z)) + std::string(symbol(Z)) + "): 2s lies above 2p (j = 1/2) by " + gapText(lambShiftEV(Z, 2, 0, 1) - lambShiftEV(Z, 2, 1, 1)) + ". The levels n = 3 and up have no Lamb shift here, "
                 : "No Lamb shift for Z above 110 (the source stops there): 2s and 2p (j = 1/2) have exactly the same energy, ") +
           "so NIST's measured lines still differ slightly. The main energy keeps the reduced mass; the relativistic correction is worked out for an infinitely heavy nucleus, and further recoil effects are not included. "
           "The cloud is drawn by the Cloud model setting (Dirac from cobalt, Z = 27, on, by default); its photons use these lines when it is a Dirac cloud.";
}

}  // namespace atom
