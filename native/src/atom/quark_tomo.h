// 0.44: the Accurate quark style's dots: where the proton's MOMENTUM is, by kind of parton and by momentum fraction x ("nucleon tomography").
// Seen along the motion of a very fast proton (the frame of quark_density.h). A dot is one piece of the proton's momentum, found at the distance b
// from its centre of momentum, carried by one kind of parton. Sources: native/data/qcd/tomography/README.txt.
//   How much momentum each kind carries, and at which x: ABM11 NLO at 2 GeV (quark_tomo_data.h, tools/make_parton_tomography.py).
//   Where in b a parton of momentum fraction x sits (a Gaussian in b with mean square <b^2>(x)):
//     valence u and d: the GPD fit of Diehl and Kroll, EPJC 73, 2397 (2013), arXiv:1302.4604, eq. (36) <b^2>_x = 4 f_q(x), eq. (44) and table 11 and (50)
//       (the default fit "ABM 1", made with the same ABM11 densities): f_q = alpha'_q (1-x)^3 ln(1/x) + B_q (1-x)^3 + A_q x (1-x)^2.
//     sea quarks and antiquarks: measured, deeply virtual Compton scattering: <r_T^2> = 2B: COMPASS, PLB 793, 188 (2019), arXiv:1802.02739: B = 4.3 GeV^-2
//       at x = 0.028 (x_Bj / 2); H1, PLB 681, 391 (2009), arXiv:0907.5289: B = 5.41 GeV^-2 at x = 1.2e-3.
//     gluons: from J/psi photoproduction, as summarised by Frankfurt, Strikman and Weiss, Ann. Rev. Nucl. Part. Sci. 55, 403 (2005), arXiv:hep-ph/0507286, p. 25:
//       <rho^2> = 0.28 fm^2 at x ~ 0.1 and about 0.35 fm^2 at x ~ 1e-2 .. 1e-3.
//     Sea and gluons between their measured points: linear in ln x (a choice). Below the smallest x: the end value. ABOVE the largest measured x (0.45: most of the sea's
//     momentum, 77 to 81 %, and a third of the gluons' lie there: extrapolated, not measured) the width shrinks as (1 - x)^2 from its last measured value: b is the distance
//     from the centre of momentum, and a parton carrying nearly all the momentum is that centre, so <b^2> ~ (1 - x)^2 as x -> 1 (Diehl and Kroll, section 7.3; the same
//     factor as in their valence widths). The form of the factor is a choice; until 0.44 the width stayed at its last value, which drew fast gluons too far out.
//   Left out: strange and charm (4.5 % of the momentum; the strange sea's size is debated); the spin (an unpolarised proton).
//   In a neutron the up and down roles swap (isospin).
#pragma once
#include "quark_tomo_data.h"
#include <algorithm>
#include <cmath>

namespace atom {

enum class Pk { Uv, Dv, Usea, Ubar, Dsea, Dbar, G, N };   // valence u, valence d, sea u quark, u-bar, sea d quark, d-bar, gluons (proton's)

inline const double* pkCum(Pk k) {
    switch (k) {
        case Pk::Uv: return kTxCum_uv;
        case Pk::Dv: return kTxCum_dv;
        case Pk::Usea: case Pk::Ubar: return kTxCum_ub;   // a sea quark has the same density as its antiquark (no difference measured that the app could draw)
        case Pk::Dsea: case Pk::Dbar: return kTxCum_db;
        default: return kTxCum_g;
    }
}
inline const double* pkXf(Pk k) {
    switch (k) {
        case Pk::Uv: return kTx_uv;
        case Pk::Dv: return kTx_dv;
        case Pk::Usea: case Pk::Ubar: return kTx_ub;
        case Pk::Dsea: case Pk::Dbar: return kTx_db;
        default: return kTx_g;
    }
}
inline double pkLnX(int i) { return std::log(kTxMin) * (1.0 - double(i) / (kTxN - 1)); }   // the grid: ln x from ln 1e-5 to 0
// the share of the proton's momentum carried by this kind (for the sea: the quarks, or the antiquarks, of that flavour)
inline double pkShare(Pk k) { return pkCum(k)[kTxN - 1]; }

// the momentum fraction x of a piece of momentum of this kind, for u uniform in 0..1 (inverse of the cumulative table, linear in ln x)
inline double pkSampleX(Pk k, double u) {
    const double* c = pkCum(k);
    const double t = std::min(1.0, std::max(0.0, u)) * c[kTxN - 1];
    const int i = int(std::upper_bound(c, c + kTxN, t) - c);
    if (i <= 0) return kTxMin;
    if (i >= kTxN) return 1.0;
    const double d = c[i] - c[i - 1], f = d > 0 ? (t - c[i - 1]) / d : 0;
    return std::exp(pkLnX(i - 1) + f * (pkLnX(i) - pkLnX(i - 1)));
}

constexpr double kHc2 = 0.1973269804 * 0.1973269804;   // (hbar c)^2, GeV^2 fm^2 (CODATA 2022): GeV^-2 -> fm^2

// <b^2>(x), fm^2: the mean square distance from the centre of momentum of partons of this kind with momentum fraction x
inline double pkMeanB2(Pk k, double x) {
    x = std::min(1.0, std::max(1e-9, x));
    const double lx = std::log(1 / x), m = 1 - x;
    auto lerpLn = [&](double x0, double v0, double x1, double v1) {   // linear in ln x between (x0, v0) and (x1, v1); below x0: v0; above x1: v1 (1 - x)^2 / (1 - x1)^2
        const double f = std::min(1.0, std::max(0.0, std::log(x / x0) / std::log(x1 / x0)));
        const double shrink = x > x1 ? (m * m) / ((1 - x1) * (1 - x1)) : 1.0;
        return (v0 + f * (v1 - v0)) * shrink;
    };
    switch (k) {
        case Pk::Uv: return 4 * (0.961 * m * m * m * lx + 0.545 * m * m * m + 1.264 * x * m * m) * kHc2;   // alpha'_u = alpha'_d + 0.1 (eq. 47) = 0.961 GeV^-2
        case Pk::Dv: return 4 * (0.861 * m * m * m * lx + 0.206 * m * m * m + 4.198 * x * m * m) * kHc2;   // alpha'_d = 0.861 GeV^-2 (eq. 50)
        case Pk::G: return lerpLn(0.01, 0.35, 0.1, 0.28);
        default: return lerpLn(1.2e-3, 2 * 5.41 * kHc2, 0.028, 2 * 4.3 * kHc2);
    }
}

// The share of the sea pairs that are down pairs (the Simple style's sea): d-bar over d-bar + u-bar by momentum, ABM11 at 2 GeV (1.26 : 1; 0.44 said 1.5 "inside SeaQuest's
// range", which holds only for 0.13 < x < 0.45). (By number the ratio is near 1: it is dominated by the many slow pairs at small x, where d-bar and u-bar are about equal.)
inline double pkSeaDownShare() { return pkShare(Pk::Dbar) / (pkShare(Pk::Dbar) + pkShare(Pk::Ubar)); }

// The momentum-weighted <b^2> of the dots of the first nk kinds (Uv, Dv, G, Usea, Ubar, Dsea, Dbar: the order the Accurate style draws them), fm^2
inline const Pk* pkDrawOrder() { static const Pk k[7] = {Pk::Uv, Pk::Dv, Pk::G, Pk::Usea, Pk::Ubar, Pk::Dsea, Pk::Dbar}; return k; }
inline double pkWidthAt(int i, Pk k) { return pkMeanB2(k, std::exp(0.5 * (pkLnX(i) + pkLnX(i + 1)))); }
inline double pkCellMomentum(int i, Pk k) { return pkCum(k)[i + 1] - pkCum(k)[i]; }
inline double pkDotsMeanB2(Pk k) {
    double s = 0, m = 0;
    for (int i = 0; i + 1 < kTxN; i++) { const double dm = pkCellMomentum(i, k); s += dm * pkWidthAt(i, k); m += dm; }
    return s / m;
}

// The glow under the dots (0.45): the same density the dots are drawn from, as a sum of a few round Gaussians (the shader's soft dots), each holding an equal share of the
// momentum of the kinds drawn, grouped by width. A Gaussian exp(-b^2 / w) has <b^2> = w; together they keep the dots' momentum-weighted <b^2> exactly.
struct PkGlow { int n; double weight[8], w[8]; };
inline PkGlow pkGlow(int nk, int n = 6) {
    struct Cell { double w, m; };
    static Cell cells[7 * kTxN];
    int nc = 0;
    double tot = 0;
    for (int k = 0; k < nk; k++)
        for (int i = 0; i + 1 < kTxN; i++) { const Cell c{pkWidthAt(i, pkDrawOrder()[k]), pkCellMomentum(i, pkDrawOrder()[k])}; if (c.m > 0) { cells[nc++] = c; tot += c.m; } }
    std::sort(cells, cells + nc, [](const Cell& a, const Cell& b) { return a.w < b.w; });
    PkGlow g{};
    g.n = n;
    double acc = 0, sw = 0, sm = 0;
    int j = 0;
    for (int c = 0; c < nc; c++) {
        acc += cells[c].m; sw += cells[c].m * cells[c].w; sm += cells[c].m;
        if (acc >= tot * (j + 1) / n - 1e-15 || c == nc - 1) { g.weight[j] = sm / tot; g.w[j] = sw / sm; j++; sw = sm = 0; if (j == n) break; }
    }
    g.n = j;
    return g;
}

}  // namespace atom
