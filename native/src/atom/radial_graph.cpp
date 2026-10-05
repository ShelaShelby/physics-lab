#include "radial_graph.h"

#include <algorithm>
#include <cmath>

#include "atom_core.h"
#include "outline.h"

namespace atom {

namespace {

// The lab's radial(o, q) is NOT the whole textbook R_nl: it is  sgn L^(2l+1)_(n-l-1)(2q/n) e^(-q/n)  (q = Z r in a0, standard generalised Laguerre polynomial), WITHOUT the factor q^l (that
// factor sits in the angular polynomial of psi, which is r^l times a function of direction) and WITHOUT a normalising constant. So the textbook R_nl(q) is proportional to q^l radial(o, q), and
//     P(r) = r^2 R_nl(r)^2 = Z q^(2l+2) radial(o, q)^2 / I,   q = Z r,
// where I = integral over q of q^(2l+2) radial^2 dq. With x = 2q/n this is (n/2)^(2l+3) times the standard integral of x^(2l+2) e^(-x) [L^(2l+1)_(n-l-1)(x)]^2 dx = 2n (n+l)! / (n-l-1)!, which is the
// normalisation of the hydrogen radial functions (Griffiths, Introduction to Quantum Mechanics, ch. 4; Bethe & Salpeter, both already in About, "Where the facts come from"). So the area under P is
// exactly 1 by this closed form, not by the curve's own quadrature; the tests check it against Simpson and against the textbook R10..R32.
double normI(const Orbital& o) {
    double ratio = 1;                                                    // (n + l)! / (n - l - 1)!  =  (n - l)(n - l + 1) ... (n + l)
    for (int k = o.n - o.l; k <= o.n + o.l; k++) ratio *= k;
    return std::pow(o.n / 2.0, 2 * o.l + 3) * 2 * o.n * ratio;
}
double pq(const Orbital& o, double q, double invI) {                     // P per unit q at Z = 1
    const double R = radial(o, q);
    return std::pow(q, 2 * o.l + 2) * R * R * invI;
}
// H(q) = R (( l + 1 ) R + q R'): its sign is the sign of dP/dq (P = q^(2l+2) R^2 / I, P' = 2 q^(2l+1) R ((l + 1) R + q R') / I). A local maximum of P is where H goes from + to -.
// R' from the Laguerre polynomial: R = sgn L(x) e^(-q/n), x = 2q/n, R' = sgn e^(-q/n) (2 L'(x) / n - L(x) / n).
double slopeSign(const Orbital& o, double q) {
    const double x = 2 * q / o.n;
    double L = 0, dL = 0;
    for (int i = int(o.lag.size()) - 1; i >= 0; i--) L = L * x + o.lag[i];
    for (int i = int(o.lag.size()) - 1; i >= 1; i--) dL = dL * x + i * o.lag[i];
    const double e = std::exp(-q / o.n), R = o.sgn * L * e, dR = o.sgn * e * (2 * dL - L) / o.n;
    return R * ((o.l + 1) * R + q * dR);
}

}  // namespace

double meanRadius(int n, int l, int Z) { return (3.0 * n * n - l * (l + 1)) / 2 / Z; }   // textbook <r> for a hydrogen-like orbital: a0 (3n^2 - l(l+1)) / 2Z (Griffiths ch. 4); the readout's "theory" value

double radialDensity(int state, int Z, double r) {
    const Orbital& o = orbitals()[stateOrbital(state)];
    return r <= 0 ? 0.0 : Z * pq(o, Z * r, 1 / normI(o));
}

// everything but the 90 % mark (which needs the outline, which takes a 64^3 pass the first time)
static RadialGraph radialGraphCore(int state, int Z) {
    const Orbital& o = orbitals()[stateOrbital(state)];
    RadialGraph g;
    g.n = o.n; g.l = o.l; g.Z = Z;
    const double invI = 1 / normI(o), qTop = 2.5 * kExt[o.n] + 20;       // the same reach as orbitalNorm's integral: far beyond every orbital's edge
    // the right edge: where the share kRadialCover of the probability is inside (trapezoid rule in q, step 0.005 a0; it only places the edge)
    const double dq = 0.005;
    double cdf = 0, qEnd = qTop;
    for (double q = dq; q <= qTop; q += dq) {
        const double a = pq(o, q - dq, invI), b = pq(o, q, invI), add = 0.5 * (a + b) * dq;
        if (cdf + add >= kRadialCover) { qEnd = q - dq + dq * (kRadialCover - cdf) / add; break; }
        cdf += add;
    }
    g.rEnd = qEnd / Z;
    g.r.resize(kRadialPoints + 1); g.P.resize(kRadialPoints + 1);
    for (int i = 0; i <= kRadialPoints; i++) { g.r[i] = g.rEnd * i / kRadialPoints; g.P[i] = radialDensity(state, Z, g.r[i]); }
    // the maxima: sign changes of H from + to - on a fine grid, then bisection (to rounding)
    const double ds = 0.01;
    double prev = slopeSign(o, ds * 0.5);
    for (double k = 1; k * ds <= qTop; k++) {
        const double q = k * ds, cur = slopeSign(o, q);
        if (prev > 0 && cur <= 0) {
            double a = q - ds, b = q;
            for (int it = 0; it < 100; it++) { const double m = 0.5 * (a + b); (slopeSign(o, m) > 0 ? a : b) = m; }
            g.maxima.push_back(0.5 * (a + b) / Z);
        }
        prev = cur;
    }
    g.peak = g.maxima.empty() ? 0 : g.maxima[0];
    for (double m : g.maxima) if (radialDensity(state, Z, m) > radialDensity(state, Z, g.peak)) g.peak = m;
    g.mean = meanRadius(o.n, o.l, Z);
    return g;
}

RadialGraph radialGraph(int state, int Z) {
    RadialGraph g = radialGraphCore(state, Z);
    g.reach90 = outlineOf(state).rmax / Z;                               // the outline is worked out once at Z = 1; psi(Z) is it scaled by 1/Z (outline.h)
    return g;
}

RadialGraph radialGraphNoWait(int state, int Z, bool useGpu) {           // 0.13 stage 3: the 90 % mark may come a frame or two later, when the GPU has made the outline (it never waits)
    RadialGraph g = radialGraphCore(state, Z);
    if (const Outline* o = outlineReady(state, useGpu)) g.reach90 = o->rmax / Z;
    else g.reach90Pending = true;
    return g;
}

bool completeReach90(RadialGraph& g, int state, int Z, bool useGpu) {
    if (!g.reach90Pending) return true;
    const Outline* o = outlineReady(state, useGpu);
    if (!o) return false;
    g.reach90 = o->rmax / Z; g.reach90Pending = false;
    return true;
}

double radialTickStep(double rEnd) {
    for (double e = 1e-6; e < 1e6; e *= 10)
        for (double m : {1.0, 2.0, 5.0}) if (rEnd / (m * e) <= 8) return m * e;
    return 1;
}

const char* const kRadialNote =
    "P(r) dr is the chance of finding the electron between r and r + dr from the nucleus, in any direction; the area under the curve is 1. The curve depends on n, l and Z only, so every orientation "
    "(m = 0, ±1, …) of a shell gives the same curve. The 90 % mark is how far the 90 % outline reaches (its farthest point); in general that is not the distance inside which 90 % of P(r) lies. "
    "Model: one electron, non-relativistic, point nucleus; atoms with more electrons differ.";
const char* const kRadialAxisNote = "Distance r from the nucleus, in Bohr radii (a₀ ≈ 0.529 Å); each mark also gives Å. The right edge is where 99.9 % of the probability lies.";

std::string radialMarkText(const char* what, double rA0) { return std::string(what) + " " + jsNum(rA0, 3) + " a₀ = " + fmtLen(rA0 * kA0Ang); }

}  // namespace atom
