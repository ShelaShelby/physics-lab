// 0.44: the Accurate dots by momentum fraction x (quark_tomo.h) against exact sum rules and the published numbers. Written BEFORE the drawing code.
// Reference numbers are typed from the saved papers (native/data/qcd/tomography/, README.txt there), never taken from the app or from the tool's output.
#include "atom/quark_tomo.h"

// integral of g(x) f(x) dx over the table = integral of g(x) * [x f(x)] d ln x (trapezoid on the ln x grid)
template <class G> static double tomoInt(const double* xf, G g) {
    double s = 0;
    const double h = -std::log(atom::kTxMin) / (atom::kTxN - 1);
    for (int i = 0; i + 1 < atom::kTxN; i++) {
        const double x0 = std::exp(atom::pkLnX(i)), x1 = std::exp(atom::pkLnX(i + 1));
        s += 0.5 * h * (g(x0) * xf[i] + g(x1) * xf[i + 1]);
    }
    return s;
}

// Exact: a proton has two more u quarks than u-bar, one more d than d-bar (number sum rules), and its partons carry all of its momentum (momentum sum rule).
static void testTomoSumRules() {
    using namespace atom;
    checkNum("u_v: 2 valence up quarks (number sum rule; table from x = 1e-5)", tomoInt(kTx_uv, [](double) { return 1.0; }), 2.0, 2e-3);
    checkNum("d_v: 1 valence down quark", tomoInt(kTx_dv, [](double) { return 1.0; }), 1.0, 2e-3);
    const double sum = pkShare(Pk::Uv) + pkShare(Pk::Dv) + pkShare(Pk::Usea) + pkShare(Pk::Ubar) + pkShare(Pk::Dsea) + pkShare(Pk::Dbar) + pkShare(Pk::G) + kTxCum_sc[kTxN - 1];
    checkNum("all the partons together carry the whole momentum (momentum sum rule)", sum, 1.0, 2e-3);
    checkTrue("the cumulative tables agree with their densities (gluons)", std::fabs(tomoInt(kTx_g, [](double x) { return x; }) - pkShare(Pk::G)) < 2e-3 * pkShare(Pk::G));
    // the d-bar excess of the sea (E866, SeaQuest: d-bar > u-bar), which the dots must show
    checkTrue("more d-bar than u-bar momentum in the sea (E866 / SeaQuest)", pkShare(Pk::Dbar) > pkShare(Pk::Ubar));
    std::printf("momentum shares at 2 GeV (ABM11 NLO): u_v %.4f, d_v %.4f, u sea %.4f + u-bar %.4f, d sea %.4f + d-bar %.4f, gluons %.4f, s + c %.4f\n", pkShare(Pk::Uv), pkShare(Pk::Dv),
                pkShare(Pk::Usea), pkShare(Pk::Ubar), pkShare(Pk::Dsea), pkShare(Pk::Dbar), pkShare(Pk::G), kTxCum_sc[kTxN - 1]);
}

// The widths against the papers.
static void testTomoWidths() {
    using namespace atom;
    // Diehl and Kroll 2013, table 15 (fit ABM 1, which uses these ABM11 densities): sqrt(<b^2>) of the flavour form factors F1: u 0.627 +- 0.003 fm, d 0.638 +- 0.003 fm.
    // <b^2>_q = integral q_v(x) 4 f_q(x) dx / N_q (eq. 36, 59). Our tables stop at x = 1e-5 and are interpolated: 1 % on the radius.
    const double bu = std::sqrt(tomoInt(kTx_uv, [](double x) { return pkMeanB2(Pk::Uv, x); }) / 2), bd = std::sqrt(tomoInt(kTx_dv, [](double x) { return pkMeanB2(Pk::Dv, x); }));
    checkNum("valence u: average impact parameter of F1u = 0.627 fm (Diehl and Kroll 2013, table 15)", bu, 0.627, 0.01);
    checkNum("valence d: average impact parameter of F1d = 0.638 fm (table 15)", bd, 0.638, 0.01);
    // the 0.43 clouds (BBBA05 + Atac form factors) are an independent fit of the same kind of data: printed for REPORT.md, not checked here (the fits' own proton radii differ:
    // Diehl-Kroll r_E = 0.838 fm, table 14; BBBA05's slope gives 0.86 fm)
    std::printf("valence spread <b^2>: u %.4f fm^2 (0.43 clouds, BBBA05: %.4f), d %.4f fm^2 (%.4f)\n", bu * bu, quarkDensityMeanB2(QdKind::U), bd * bd, quarkDensityMeanB2(QdKind::D));
    checkTrue("fast quarks sit near the centre: <b^2> at x = 0.7 is under a quarter of that at x = 0.05 (both flavours)",
              pkMeanB2(Pk::Uv, 0.7) < 0.25 * pkMeanB2(Pk::Uv, 0.05) && pkMeanB2(Pk::Dv, 0.7) < 0.25 * pkMeanB2(Pk::Dv, 0.05));
    checkTrue("... and <b^2> -> 0 as x -> 1, as (1 - x)^2 (Diehl and Kroll, section 7.3: a finite proton)", pkMeanB2(Pk::Uv, 0.999) < 1e-4 && pkMeanB2(Pk::Dv, 0.999) < 1e-4);
    // sea: COMPASS 2019: sqrt(<r_T^2>) = 0.58 fm at x_Bj / 2 = 0.028 (B = 4.3 GeV^-2); H1 2009: 0.64 fm at x = 1.2e-3 (b = 5.41 GeV^-2; 2b (hbar c)^2 gives 0.649)
    checkNum("sea at x = 0.028: 0.58 fm (COMPASS 2019)", std::sqrt(pkMeanB2(Pk::Ubar, 0.028)), 0.58, 0, 0.006);
    checkNum("sea at x = 1.2e-3: 0.64 fm (H1 2009)", std::sqrt(pkMeanB2(Pk::Dbar, 1.2e-3)), 0.64, 0, 0.01);
    // gluons: Frankfurt, Strikman, Weiss 2005: <rho^2> = 0.28 fm^2 at x ~ 0.1, about 0.35 fm^2 at x ~ 1e-2 .. 1e-3
    checkNum("gluons at x = 0.1: 0.28 fm^2 (Frankfurt, Strikman, Weiss 2005, from J/psi data)", pkMeanB2(Pk::G, 0.1), 0.28, 1e-12);
    checkNum("gluons at x = 0.003: 0.35 fm^2", pkMeanB2(Pk::G, 0.003), 0.35, 1e-12);
    const double gb2 = tomoInt(kTx_g, [](double x) { return x * pkMeanB2(Pk::G, x); }) / pkShare(Pk::G);
    std::printf("gluons, momentum weighted <b^2> = %.4f fm^2 (lattice A_g tripole, 0.43: %.4f)\n", gb2, quarkDensityMeanB2(QdKind::G));
}

// The drawing picks x from the tables: the picked values have the table's mean x (Monte Carlo, fixed seed)
static void testTomoSampling() {
    using namespace atom;
    std::mt19937 rng(44);
    std::uniform_real_distribution<double> U(0, 1);
    for (Pk k : {Pk::Uv, Pk::Dbar, Pk::G}) {
        double s = 0;
        for (int i = 0; i < 200000; i++) s += pkSampleX(k, U(rng));
        const double want = tomoInt(pkXf(k), [](double x) { return x * x; }) / pkShare(k);   // <x> over the momentum: integral x * x f dx / integral x f dx
        checkNum("x picked for kind " + std::to_string(int(k)) + ": mean x weighted by momentum (200,000 picks, 1 %)", s / 200000, want, 0.01);
    }
    checkTrue("the picked x stays inside 1e-5 .. 1", pkSampleX(Pk::G, 0) >= kTxMin * (1 - 1e-12) && pkSampleX(Pk::G, 1) <= 1);
}

static void testTomoWords() {
    const std::string acc = labWords("Accurate", "+ Sea", true), val = labWords("Accurate", "Valence", true);
    checkTrue("Accurate: a dot is a piece of the proton's momentum, not a particle", has(acc, "momentum") && has(acc, "not a quark"));
    checkTrue("Accurate: fast quarks near the centre, slow ones spread out (tomography) is said", has(acc, "near the centre") && has(acc, "spread"));
    checkTrue("Accurate: the sources are named (Diehl and Kroll, ABM11, COMPASS, H1)", has(acc, "Diehl") && has(acc, "ABM11") && has(acc, "COMPASS") && has(acc, "H1"));
    checkTrue("+ Sea: the sea's quark-antiquark pairs are drawn (antiquarks as rings) and strange quarks are said to be left out", has(acc, "ring") && has(acc, "strange"));
    checkTrue("Valence: says the gluons and the sea are hidden, not absent", has(val, "hidden"));
}

static void testQcd044() {
    testTomoSumRules();
    testTomoWidths();
    testTomoSampling();
    testTomoWords();
}
