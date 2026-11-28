#include "molecule/morse.h"

#include <cstdio>

#include <cmath>
#include <iterator>
#include <limits>

#include "molecule/rotation.h"
#include "molecule/vibration.h"

namespace molecule::morse {

namespace {

constexpr double kH = 6.62607015e-34, kC = 299792458.0, kU = 1.66053906892e-27, kPi = 3.14159265358979323846;   // CODATA 2022
constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();

// re (angstrom): the X row of Huber & Herzberg (1979) via the NIST WebBook, the saved pages in data/molecules/vib/webbook_diatomic/ (a subscript digit kept):
//   H2 0.74144, N2 1.09768_5, O2 1.20752, HCl 1.27455_2 (footnote 31: the uncorrected value from Be; Bunker 1972 gives 1.2746149 at the Born-Oppenheimer minimum)
// D0 (cm^-1, from the lowest level, v = 0 and the lowest rotational level, to the separated atoms in their ground levels). 0.97.0 (the outside review of 28 Nov 2026,
// second, part 3): newer values. Until 0.96.0: H2 36118.3 (Huber & Herzberg's footnote 125, an upper limit) and Darwent, NSRDS-NBS 31 (1970): N-N 941.69,
// O-O 493.59 +- 0.4, Cl-H 428.02 +- 0.42 kJ/mol (data/molecules/vib/dissociation/darwent1970_d0.txt, kept for the test that compares them).
//   H2: 36118.06962(37) cm^-1, the dissociation energy of para-H2 (N = 0) of Altmann et al., PRL 120, 043204 (2018), as corrected in note a of Table II of
//       N. Hoelsch et al., arXiv:1902.09471 (2019) (data/molecules/vib/dissociation/holsch_2019_arxiv1902.09471.extract.txt). The same paper measures ortho-H2
//       (N = 1): 35999.582834(11) cm^-1.
//   N2, O2, HCl: Active Thermochemical Tables ver. 1.228 (Ruscic & Bross, Argonne, 2026; atct_v1.228.extract.txt), the enthalpies of formation at 0 K (kJ/mol,
//       95 % limits): N 470.655 +- 0.021, O 246.843 +- 0.0021, H 216.034 (+- < 0.0005), Cl 119.621 +- 0.0011, HCl -91.988 +- 0.0062. D0(N2) = 2 dfH(N) =
//       941.310 +- 0.042, D0(O2) = 2 dfH(O) = 493.686 +- 0.0042, D0(HCl) = dfH(H) + dfH(Cl) - dfH(HCl) = 427.643 +- 0.0063 (the three added in quadrature, ATcT's
//       independent-errors rule; covariances are given only at 298.15 K). Converted with N_A h c (CODATA 2022, exact).
struct Row { const char* id; double re, d0; const char* src; };
const Row kRows[] = {
    {"H2", 0.74144, 36118.06962, "para-H₂ (J = 0): 36,118.06962 ± 0.00037 cm⁻¹ (Altmann et al. 2018, as corrected by Hölsch et al. 2019)"},
    {"N2", 1.097685, 2 * 470.655 * kKJmolToCm, "Active Thermochemical Tables 1.228 (2026): 941.310 ± 0.042 kJ/mol"},
    {"O2", 1.20752, 2 * 246.843 * kKJmolToCm, "Active Thermochemical Tables 1.228 (2026): 493.686 ± 0.004 kJ/mol"},
    {"HCl", 1.274552, (216.034 + 119.621 + 91.988) * kKJmolToCm, "Active Thermochemical Tables 1.228 (2026): 427.643 ± 0.006 kJ/mol"},
};
const Row* row(const std::string& id) {
    for (const Row& r : kRows) if (id == r.id) return &r;
    return nullptr;
}
// H35Cl's band origins nu0(v-0), v = 1..5, cm^-1: Rank, Eastman, Rao & Wiggins, J. Opt. Soc. Am. 52, 1 (1962), page 3, "nu0 Obs"
// (data/molecules/vib/hcl_lines/rank1962_band_origins.txt, read from the saved PDF and checked against the page image)
const double kHClOrigins[] = {2885.9775, 5667.9841, 8346.782, 10922.81, 13396.19};

}  // namespace

bool has(const std::string& id) { return row(id) && vib::anharmonic(id); }

Curve make(double we, double wexe, double re, double muU) {
    Curve c;
    c.we = we; c.wexe = wexe; c.re = re; c.muU = muU;
    c.De = we * we / (4 * wexe);
    const double w0 = 2 * kPi * kC * 100 * we, DeJ = kH * kC * 100 * c.De;
    c.a = w0 * std::sqrt(muU * kU / (2 * DeJ)) * 1e-10;   // 1/angstrom
    return c;
}
Curve curve(const std::string& id) {
    const vib::Anharmonic* a = vib::anharmonic(id);
    const Row* r = row(id);
    if (!a || !r) return Curve{};
    return make(a->we, a->wexe, r->re, rot::rotor(id).muU);
}
double reMeasured(const std::string& id) { const Row* r = row(id); return r ? r->re : kNaN; }

double V(const Curve& c, double rA) {
    const double e = 1 - std::exp(-c.a * (rA - c.re));
    return c.De * e * e;
}
double level(const Curve& c, int v) { const double x = v + 0.5; return c.we * x - c.wexe * x * x; }
int vMax(const Curve& c) { return c.wexe > 0 ? static_cast<int>(std::floor(c.we / (2 * c.wexe) - 0.5)) : 0; }

int measuredTop(const std::string& id) {
    const vib::Anharmonic* a = vib::anharmonic(id);
    if (!a) return -1;
    return a->validTo > 0 ? a->validTo : 1;   // N2, O2: the range is not stated; their pages quote the measured Delta G(1/2) (footnotes 109 and 69)
}
bool isMeasured(const std::string& id, int v) { return v >= 0 && v <= measuredTop(id); }
double levelShown(const std::string& id, int v) {
    const vib::Anharmonic* a = vib::anharmonic(id);
    if (!a) return kNaN;
    return isMeasured(id, v) ? vib::termG(*a, v) : level(curve(id), v);
}
double dissociatedE(const Curve& c) { const int m = vMax(c); return c.De + (level(c, m) - level(c, m - 1)); }

double d0Measured(const std::string& id) { const Row* r = row(id); return r ? r->d0 : kNaN; }
const char* d0Source(const std::string& id) { const Row* r = row(id); return r ? r->src : ""; }
double d0Morse(const Curve& c) { return c.De - level(c, 0); }
MorseGap morseGap(const std::string& id) {
    MorseGap g;
    const double d0 = d0Measured(id);
    if (!has(id) || !std::isfinite(d0)) { g.cm = g.share = kNaN; return g; }
    const double dm = d0Morse(curve(id));
    g.cm = dm - d0; g.share = dm / d0 - 1; g.over = dm > d0;
    return g;
}
std::string morseGapWords(const std::string& id) {
    const MorseGap g = morseGap(id);
    if (!std::isfinite(g.cm)) return std::string();
    char b[400];
    if (g.over)
        std::snprintf(b, sizeof b, "The Morse curve fixed by ωe and ωexe alone overestimates the dissociation energy: its D₀ is %.0f cm⁻¹ (%.1f %%) above "
                      "the measured one. Two constants fix only the bottom of the well: higher up the real rungs close up faster than the Morse ladder's "
                      "straight-line spacing, so the Morse well is too deep.", g.cm, 100 * g.share);
    else
        std::snprintf(b, sizeof b, "The Morse curve fixed by ωe and ωexe alone gives a D₀ %.0f cm⁻¹ (%.1f %%) below the measured one.", -g.cm, -100 * g.share);
    return b;
}

double alphaPekeris(double we, double wexe, double Be) { return 6 * std::sqrt(wexe * Be * Be * Be) / we - 6 * Be * Be / we; }
double alphaPekeris(const std::string& id) {
    const vib::Anharmonic* a = vib::anharmonic(id);
    const rot::Measured* m = rot::measured(id);
    if (!a || !m) return kNaN;
    return alphaPekeris(a->we, a->wexe, m->b0 ? m->Be + m->alphaE / 2 : m->Be);
}
double alphaMeasured(const std::string& id) { const rot::Measured* m = rot::measured(id); return m ? m->alphaE : kNaN; }

Motion motion(const Curve& c, double E) {
    Motion m;
    m.E = E;
    const double eps = E / c.De, s = std::sqrt(eps), w0 = 2 * kPi * kC * 100 * c.we;
    m.bound = eps < 1;
    m.rIn = c.re - std::log1p(s) / c.a;
    if (m.bound) {
        m.rOut = c.re - std::log1p(-s) / c.a;
        m.rMean = c.re + (std::log((1 + std::sqrt(1 - eps)) / 2) - std::log1p(-eps)) / c.a;
        m.periodS = 2 * kPi / (w0 * std::sqrt(1 - eps));
    } else {
        m.rOut = std::numeric_limits<double>::infinity();
    }
    return m;
}
double bondAt(const Curve& c, double E, double t) {
    const double eps = E / c.De, s = std::sqrt(eps), w0 = 2 * kPi * kC * 100 * c.we;
    if (eps < 1) {
        const double w = w0 * std::sqrt(1 - eps);
        const double ph = std::fmod(w * t, 2 * kPi);              // (exact for long runs)
        return c.re + std::log((1 - s * std::cos(ph)) / (1 - eps)) / c.a;
    }
    const double W = w0 * std::sqrt(eps - 1);
    return c.re + std::log((s * std::cosh(W * t) - 1) / (eps - 1)) / c.a;
}
double speedAt(const Curve& c, double E, double t) {
    const double eps = E / c.De, s = std::sqrt(eps), w0 = 2 * kPi * kC * 100 * c.we;
    if (eps < 1) {
        const double w = w0 * std::sqrt(1 - eps), ph = std::fmod(w * t, 2 * kPi);
        return s * w * std::sin(ph) / (c.a * (1 - s * std::cos(ph)));
    }
    const double W = w0 * std::sqrt(eps - 1);
    return s * W * std::sinh(W * t) / (c.a * (s * std::cosh(W * t) - 1));
}

std::vector<double> bandOrigins(const std::string& id) {
    if (id != "HCl") return {};
    return std::vector<double>(std::begin(kHClOrigins), std::end(kHClOrigins));
}

BirgeSponer birgeSponer(const std::string& id) {
    BirgeSponer b;
    const vib::Anharmonic* a = vib::anharmonic(id);
    if (!a) return b;
    const std::vector<double> o = bandOrigins(id);
    if (!o.empty()) {
        b.measuredOrigins = true;
        double prev = 0;
        for (std::size_t v = 0; v < o.size(); v++) { b.x.push_back(static_cast<double>(v) + 1); b.dG.push_back(o[v] - prev); prev = o[v]; }
    } else {
        for (int v = 0; v < measuredTop(id); v++) { b.x.push_back(v + 1); b.dG.push_back(vib::termG(*a, v + 1) - vib::termG(*a, v)); }
    }
    if (b.x.size() < 2) return b;
    double sx = 0, sy = 0, sxx = 0, sxy = 0;
    for (std::size_t i = 0; i < b.x.size(); i++) { sx += b.x[i]; sy += b.dG[i]; sxx += b.x[i] * b.x[i]; sxy += b.x[i] * b.dG[i]; }
    const double n = static_cast<double>(b.x.size());
    b.slope = (n * sxy - sx * sy) / (n * sxx - sx * sx);
    b.intercept = (sy - b.slope * sx) / n;
    if (!(b.slope < 0)) return b;
    b.ok = true;
    b.xZero = -b.intercept / b.slope;
    const double h = b.intercept + b.slope / 2;                // the line's height at v + 1 = 1/2
    b.area = h * h / (-2 * b.slope);
    return b;
}

}  // namespace molecule::morse
