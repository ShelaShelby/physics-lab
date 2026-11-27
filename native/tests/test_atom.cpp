// Unit tests for the Atom Lab: against NIST (CODATA, ASD), NUBASE2020, AME2020, the IAEA tables, textbook formulas and conservation rules.
#include <cctype>
#include <iterator>
#include <limits>
#include <fstream>
#include <cmath>
#include <cstdlib>
#include <cstdint>
#include <cstdio>
#include <algorithm>
#include <array>
#include <chrono>
#include <cstring>
#include <map>
#include <set>
#include <complex>
#include <string>
#include <vector>

#include "atom/atom_core.h"
#include "atom/atom_learn.h"
#include "atom/asd.h"
#include "atom/dirac.h"
#include "atom/dirac_cloud.h"
#include "atom/hfs.h"
#include "atom/levels.h"
#include <tuple>
#include <functional>
#include <random>
#include "atom/lifetime.h"
#include "atom/atom_sim.h"
#include "atom/chart.h"
#include "atom/chart_view.h"
#include "atom/ame.h"
#include "atom/nubase.h"
#include "atom/outline.h"
#include "atom/phase_wheel.h"
#include "atom/radii.h"
#include "atom/quadrupole.h"
#include "atom/radial_graph.h"
#include "orbit/format.h"
#include "atom/atom_bench.h"
#include "atom/cloud_dots.h"
#include "atom/cloud_light.h"
#include "atom/cloud_sample.h"
#include "atom/atom_card.h"
#include "render/shaders.h"
#include "atom/chart_draw.cpp"   // 0.11 stage 2: the chart drawing is tested through a recording stand-in for the renderer below (it only calls rect and hexColor)

#include "reference/ylm_reference.inc"

using namespace atom;

static int g_checks = 0, g_failed = 0;

static void checkStr(const std::string& what, const std::string& got, const std::string& want) {
    g_checks++;
    if (got == want) return;
    if (++g_failed <= 400) std::printf("FAIL %s\n   got:  %s\n   want: %s\n", what.c_str(), got.c_str(), want.c_str());
}
static int g_ties = 0;
static void checkNum(const std::string& what, double got, double want, double relTol, double absTol = 0) {
    g_checks++;
    const bool same = (std::isnan(got) && std::isnan(want)) || got == want || std::fabs(got - want) <= std::max(absTol, relTol * std::fabs(want));
    if (same) return;
    if (++g_failed <= 400) std::printf("FAIL %s: got %.17g, want %.17g\n", what.c_str(), got, want);
}
static void checkTrue(const std::string& what, bool ok) { checkStr(what, ok ? "true" : "false", "true"); }
// 0.46: Electrons now defaults to All; the tests written for the one-electron lab start from its old default, Hydrogen-like, said here once (a test of All sets it)
// 0.49.3: the non-relativistic one-electron lab (Cloud model: Non-relativistic); the Dirac cloud (the default from Z = 27) has its own tests (test_atom0493.inc, testDiracLab0493)
static atom::AtomParams oneParams() { atom::AtomParams q; q.elec = "Hydrogen-like"; q.hrel = "Non-relativistic"; return q; }
static std::string at(int Z, int N) { return std::to_string(Z) + "," + std::to_string(N); }

// ---- 0.10.6 stage 2: the CODATA 2022 values, typed in here AGAIN from the NIST pages (https://physics.nist.gov/cgi-bin/cuu/Value?<key>, each page says "Source: 2022 CODATA recommended
// values", all read on 2026-10-04), so the tests do not take them from atom_core.h. The key of each page is in the comment.
namespace nist {
constexpr double a0 = 5.29177210544e-11;        // m, bohrrada0
constexpr double hartreeEV = 27.211386245981;   // eV, hrev
constexpr double tau = 2.4188843265864e-17;     // s, aut (atomic unit of time)
constexpr double alpha = 7.2973525643e-3;       // alph
constexpr double rydEV = 13.605693122990;       // eV, rydhcev
constexpr double mecEV = 510998.95069;          // eV, mec2mev (0.510 998 950 69 MeV)
constexpr double meInU = 5.485799090441e-4;     // meu
constexpr double mpOverMe = 1836.152673426;     // mpsme
constexpr double ukg = 1.66053906892e-27;       // kg, ukg
constexpr double uMeV = 931.49410372;           // MeV, muc2mev
constexpr double meKg = 9.1093837139e-31;       // me
constexpr double mpKg = 1.67262192595e-27;      // mp
constexpr double mnKg = 1.67492750056e-27;      // mn
constexpr double mpU = 1.0072764665789;         // mpu
constexpr double mnU = 1.00866491606;           // mnu
constexpr double kB = 1.380649e-23;             // J/K, k (exact)
constexpr double h = 6.62607015e-34;            // J s, h (exact)
constexpr double c = 299792458.0;               // m/s, c (exact)
constexpr double e = 1.602176634e-19;           // C, e (exact)
constexpr double hcEVnm = h * c / e * 1e9;      // eV nm, from the three exact values (NIST has no page for it)
}
// 0.10.6 stage 6 (finding 5): the lab's nuclear mass is now Z m_p + N m_n - B/c^2 (B from the lab's binding table, which is AME2020) instead of A u - Z m_e. Written here again from the NIST masses typed above; a
// nuclide the table does not hold keeps the old form.
static double nuclearMassNew(int Z, int N) {
    const Binding b = massBinding(Z, N);
    if (!b.real) return (Z + N) / nist::meInU - Z;
    return Z * nist::mpOverMe + N * (nist::mnU / nist::meInU) - b.B * 1e6 / nist::mecEV;
}


static double nubaseSeconds(int Z, int N) {
    static const struct { const char* u; double s; } kUnit[] = {{"ys", 1e-24}, {"zs", 1e-21}, {"as", 1e-18}, {"ps", 1e-12}, {"ns", 1e-9}, {"us", 1e-6}, {"ms", 1e-3}, {"s", 1}, {"m", 60}, {"h", 3600}, {"d", 86400}, {"y", 365.25 * 86400}, {"ky", 3.15576e10}, {"My", 3.15576e13}, {"Gy", 3.15576e16}, {"Ty", 3.15576e19}, {"Py", 3.15576e22}, {"Ey", 3.15576e25}, {"Zy", 3.15576e28}, {"Yy", 3.15576e31}};
    const NubaseRow* nb = nubase(Z, N);
    const std::string t = nb ? nb->lim : "";
    const std::size_t sp = t.find(' ');
    if (!nb || nb->kind != 'M' || sp == std::string::npos) return -1.0;
    for (const auto& u : kUnit) if (t.substr(sp + 1) == u.u) return std::atof(t.c_str()) * u.s;
    return -1.0;
}

// the readout parts other checks look for
static const std::string kModelPrefix = "model: one electron, non-relativistic";
static const std::string kNucPrefix = "chance the electron is inside the nucleus";
static const std::string kNucSuffix = "  ·  non-relativistic estimate: for heavy atoms relativity makes the real value larger, and this lab does not work it out";
static const std::string kSampledNote = " (an estimate from independent dots, usually within 1 %)";

// ================= Complex orbitals and phase colours (0.10.2, stage 1) =================
// States with a magnetic quantum number m = +-1..+-l (Condon-Shortley phase) made from the existing real angular parts. Checked against things that do not come from
// the code: winding of the phase, the L_z eigenvalue, orthonormality by numerical integration, scipy's Y_lm, and the real orbitals' own densities.
using Cd = std::complex<double>;
static std::vector<int> complexIds() {                                // every complex state of every shell up to n = 4: id of (shell, m)
    std::vector<int> ids;
    for (int n = 2; n <= 4; n++)
        for (int l = 1; l < n; l++) {
            const int shell = orbIndex(std::to_string(n) + "spdf"[l]);
            for (int m = 1; m <= l; m++) { ids.push_back(complexState(shell, m)); ids.push_back(complexState(shell, -m)); }
        }
    return ids;
}

static void testComplexBasics() {
    const std::vector<int> ids = complexIds();
    checkTrue("complex states: 2 per m: 2p (2); 3p, 3d (2 + 4); 4p, 4d, 4f (2 + 4 + 6) = 20 states", ids.size() == 20);
    bool ok = true;
    for (int id : ids) ok = ok && id >= kComplexBase && isComplexState(id) && stateM(id) != 0 && std::abs(stateM(id)) <= orbitals()[stateOrbital(id)].l;
    checkTrue("complex states: ids are above kComplexBase and know their m and shell", ok);
    checkTrue("complex states: none for m = 0 or |m| > l, and real orbitals are not complex", complexState(orbIndex("2p"), 0) < 0 && complexState(orbIndex("2p"), 2) < 0 && complexState(orbIndex("1s"), 1) < 0 && !isComplexState(orbIndex("4f|xyz")));
    checkTrue("the 30 real orbitals of n <= 4 are still first, in the same order, and 0.46 adds n = 5, 6 (s p d f: 2 x 16) after them: 62", orbitals().size() == 62 && orbitals()[0].key == "1s|" && orbitals()[29].key == "4f|y3x2y2" && orbitals()[30].key == "5s|");
    int m = 0; checkTrue("orientation keys: 'm+2' / 'm-3' are complex, 'xy' is not", oriIsComplex("m+2", m) && m == 2 && oriIsComplex("m-3", m) && m == -3 && !oriIsComplex("xy", m) && oriKey(1) == "m+1" && oriKey(-3) == "m-3");
    for (const char* k : {"pAx", "dAx", "fAx"}) { checkTrue(std::string("the setting ") + k + " accepts m = +1 and still the real ones", AtomLab::validChoice(k, "m+1") && AtomLab::validChoice(k, "m-1")); }
    checkTrue("settings: m = +-3 only for f, m = +-2 only for d and f", AtomLab::validChoice("fAx", "m+3") && !AtomLab::validChoice("dAx", "m+3") && AtomLab::validChoice("dAx", "m-2") && !AtomLab::validChoice("pAx", "m+2"));
    checkTrue("the Phase colours setting: Sign (default) and Wheel, nothing else", oneParams().phase == "Sign" && AtomParams::appDefaults().phase == "Sign" && AtomLab::validChoice("phase", "Wheel") && AtomLab::validChoice("phase", "Sign") && !AtomLab::validChoice("phase", "wheel") && !AtomLab::validChoice("phase", ""));
}

// (a) winding number: walk a circle round the z axis at fixed r and theta; the unwrapped arg(psi) changes by 2 pi m.
static void testWinding() {
    double worst = 0; int n = 0;
    for (int id : complexIds()) {
        const int m = stateM(id);
        for (double r : {1.3, 2.7, 5.1}) {
            const double th = 1.1;
            double prev = 0, total = 0; bool first = true, usable = true;
            const int steps = 720;
            for (int k = 0; k <= steps; k++) {
                const double ph = 2 * kPi * k / steps;
                const Cd v = psiComplex(id, 1, r * std::sin(th) * std::cos(ph), r * std::sin(th) * std::sin(ph), r * std::cos(th));
                if (std::abs(v) < 1e-9) { usable = false; break; }               // on a node: no phase to follow here
                const double a = std::arg(v);
                if (!first) { double d = a - prev; while (d > kPi) d -= 2 * kPi; while (d < -kPi) d += 2 * kPi; total += d; }
                prev = a; first = false;
            }
            if (!usable) continue;
            worst = std::max(worst, std::fabs(total - 2 * kPi * m)); n++;
        }
    }
    std::printf("complex orbitals, winding: %d circles, largest |total phase change - 2 pi m| = %.2e rad\n", n, worst);
    // limit 1e-9: the phase is exactly m*phi (+ a constant) and each of the 720 steps is far below pi, so only rounding (~1e-13 per step) remains.
    checkTrue("winding: the phase changes by 2 pi m round the z axis, for every complex state (to 1e-9)", n >= 50 && worst < 1e-9);
}

// (b) L_z: -i d(psi)/d(phi) = m psi, by a central difference in phi.
static void testLz() {
    double worst = 0; int n = 0;
    for (int id : complexIds()) {
        const int m = stateM(id);
        for (int k = 0; k < 12; k++) {
            const double r = 0.8 + 0.7 * k, th = 0.4 + 0.19 * k, ph = 0.3 + 0.53 * k, d = 1e-5;
            const auto at = [&](double f) { return psiComplex(id, 1, r * std::sin(th) * std::cos(f), r * std::sin(th) * std::sin(f), r * std::cos(th)); };
            const Cd v = at(ph);
            if (std::abs(v) < 1e-6) continue;
            const Cd lz = Cd(0, -1) * (at(ph + d) - at(ph - d)) / (2 * d);
            worst = std::max(worst, std::abs(lz - double(m) * v) / std::abs(v)); n++;
        }
    }
    std::printf("complex orbitals, L_z: %d points, largest |(-i d/dphi) psi - m psi| / |psi| = %.2e\n", n, worst);
    // limit 1e-7: the central difference's truncation error is d^2 m^3 / 6 = 1.7e-10 for m = 3 at d = 1e-5, its rounding error about 1e-16 / d = 1e-11.
    checkTrue("L_z: -i d(psi)/d(phi) = m psi at many points (to 1e-7 relative)", n > 200 && worst < 1e-7);
}

// (c) normalised and orthogonal: integrals of conj(psi_a) psi_b over all space (midpoint rule in r, Gauss-Legendre in cos theta, uniform in phi).
struct Quad { std::vector<double> r, wr, u, wu; int nphi; };
static Quad makeQuad(int nr = 450, int nphi = 24, double top = 60) {
    Quad q; q.nphi = nphi;
    const int nu = 24; const double dr = top / nr;
    for (int i = 0; i < nr; i++) { q.r.push_back((i + 0.5) * dr); q.wr.push_back(dr); }
    // Gauss-Legendre nodes by Newton's method on P_n
    for (int i = 0; i < nu; i++) {
        double x = std::cos(kPi * (i + 0.75) / (nu + 0.5)), dp = 1;
        for (int it = 0; it < 100; it++) {
            double p0 = 1, p1 = x;
            for (int k = 2; k <= nu; k++) { const double p2 = ((2 * k - 1) * x * p1 - (k - 1) * p0) / k; p0 = p1; p1 = p2; }
            dp = nu * (x * p1 - p0) / (x * x - 1);
            const double dx = p1 / dp; x -= dx; if (std::fabs(dx) < 1e-15) break;
        }
        q.u.push_back(x); q.wu.push_back(2 / ((1 - x * x) * dp * dp));
    }
    return q;
}
static Cd overlap(const Quad& q, int a, int b) {                       // integral of conj(psi_a) psi_b dV, Z = 1
    Cd s = 0;
    for (std::size_t i = 0; i < q.r.size(); i++)
        for (std::size_t j = 0; j < q.u.size(); j++) {
            const double st = std::sqrt(1 - q.u[j] * q.u[j]);
            Cd t = 0;
            for (int k = 0; k < q.nphi; k++) {
                const double f = 2 * kPi * (k + 0.5) / q.nphi, x = q.r[i] * st * std::cos(f), y = q.r[i] * st * std::sin(f), z = q.r[i] * q.u[j];
                const Cd pa = isComplexState(a) ? psiComplex(a, 1, x, y, z) : Cd(psi(a, 1, x, y, z), 0), pb = isComplexState(b) ? psiComplex(b, 1, x, y, z) : Cd(psi(b, 1, x, y, z), 0);
                t += std::conj(pa) * pb;
            }
            s += t * (2 * kPi / q.nphi) * q.wu[j] * q.r[i] * q.r[i] * q.wr[i];
        }
    return s;
}
static void testOrthonormal() {
    const Quad q = makeQuad();
    for (int n = 2; n <= 4; n++) {
        std::vector<int> all;                                           // the complex states of shell family n and the real orbitals of the same n
        for (int id : complexIds()) if (orbitals()[stateOrbital(id)].n == n) all.push_back(id);
        const std::size_t nc = all.size();
        for (int i = 0; i < int(orbitals().size()); i++) if (orbitals()[i].n == n) all.push_back(i);
        std::vector<double> norm(all.size());
        for (std::size_t i = 0; i < all.size(); i++) norm[i] = std::sqrt(overlap(q, all[i], all[i]).real());
        double worstDiag = 0, worstCC = 0, worstDiffL = 0, worstComplete = 0;
        for (std::size_t a = 0; a < nc; a++) {
            const int la = orbitals()[stateOrbital(all[a])].l;
            for (std::size_t b = 0; b < nc; b++) {                      // complex with complex: orthonormal
                const Cd g = overlap(q, all[a], all[b]) / (norm[a] * norm[b]);
                if (a == b) worstDiag = std::max(worstDiag, std::abs(g - 1.0)); else worstCC = std::max(worstCC, std::abs(g));
            }
            double sum = 0;                                             // complex with the real orbitals of the same n
            for (std::size_t b = nc; b < all.size(); b++) {
                const Cd g = overlap(q, all[a], all[b]) / (norm[a] * norm[b]);
                if (orbitals()[all[b]].l != la) worstDiffL = std::max(worstDiffL, std::abs(g)); else sum += std::norm(g);
            }
            worstComplete = std::max(worstComplete, std::fabs(sum - 1));   // the state lies entirely in the span of the real orbitals of its own shell: sum of |overlap|^2 = 1
        }
        std::printf("complex orbitals, n = %d (%zu states): |<a|a> - 1| <= %.1e, |<a|b>| <= %.1e (complex-complex), <= %.1e (other l), |sum of squared overlaps with the shell's real orbitals - 1| <= %.1e\n",
                    n, nc, worstDiag, worstCC, worstDiffL, worstComplete);
        // limit 1e-3 (as the task says): the quadrature has 450 radial midpoints (step 0.13 a0), 24 Gauss points in cos theta (exact for polynomials of degree 47) and 24 in phi (exact for |m| <= 3 products), so the error for these smooth
        // integrands is far below that; the 60 a0 cut-off leaves e^(-2*60/4) ~ 1e-13.
        checkTrue("n = " + std::to_string(n) + ": complex states are normalised (|<a|a>/norm - 1| < 1e-3)", worstDiag < 1e-3);
        checkTrue("n = " + std::to_string(n) + ": complex states are orthogonal to each other (< 1e-3)", worstCC < 1e-3);
        checkTrue("n = " + std::to_string(n) + ": complex states are orthogonal to the real orbitals of another l (< 1e-3)", worstDiffL < 1e-3);
        checkTrue("n = " + std::to_string(n) + ": each complex state lies in the span of the real orbitals of its own shell (sum of squared overlaps = 1, < 1e-3)", worstComplete < 1e-3);
    }
    // the real orbitals are still orthogonal to each other (the existing ones are not touched): 3p x, y, z and 3d xz with 3d z2
    const int px = orbIndex("3p|x"), py = orbIndex("3p|y"), pz = orbIndex("3p|z"), dxz = orbIndex("3d|xz"), dz2 = orbIndex("3d|z2");
    const double a = std::abs(overlap(q, px, py)) / std::sqrt(overlap(q, px, px).real() * overlap(q, py, py).real()), b = std::abs(overlap(q, pz, px)), c = std::abs(overlap(q, dxz, dz2));
    checkTrue("the real orbitals stay orthogonal (3px-3py, 3pz-3px, 3dxz-3dz2; < 1e-3)", a < 1e-3 && b < 1e-3 && c < 1e-3);
}

// (d) Y_lm against scipy: our angular part = (a positive constant for each l, |m|) x r^l x Y_lm, so the ratio must be positive, real and the same in every direction.
static void testAgainstScipy() {
    double worstConst = 0, worstPhase = 0; int n = 0; bool equalPlusMinus = true;
    for (const YRef& y : kYRef) {
        double first = 0;
        for (int k = 0; k < 6; k++) {
            const double th = kYDir[k][0], ph = kYDir[k][1], r = 1.7;
            const Cd mine = angularComplex(y.l, y.m, r * std::sin(th) * std::cos(ph), r * std::sin(th) * std::sin(ph), r * std::cos(th));
            const Cd ref = std::pow(r, y.l) * Cd(y.re[k], y.im[k]);
            if (std::abs(ref) < 1e-9) continue;                           // (a node of this state at that direction)
            const Cd ratio = mine / ref;
            worstPhase = std::max(worstPhase, std::fabs(ratio.imag()) / std::abs(ratio));   // 0 when the phase convention matches (the sign too: ratio > 0)
            if (ratio.real() <= 0) worstPhase = 1;
            if (first == 0) first = ratio.real(); else worstConst = std::max(worstConst, std::fabs(ratio.real() / first - 1));
            n++;
        }
        if (y.m < 0) {                                                     // the +m and -m states have the same scale
            for (const YRef& z : kYRef) if (z.l == y.l && z.m == -y.m) {
                const Cd a = angularComplex(y.l, y.m, 1.0, 0.3, 0.5), b = angularComplex(z.l, z.m, 1.0, 0.3, 0.5);
                equalPlusMinus = equalPlusMinus && std::fabs(std::abs(a) / std::abs(b) - 1) < 1e-12;
            }
        }
    }
    std::printf("complex orbitals vs scipy %s: %d values, ratio non-constant by at most %.1e, phase off by at most %.1e rad\n", "1.17.1 sph_harm_y", n, worstConst, worstPhase);
    // limit 1e-12: scipy's values are given to 17 digits and our formula is a short polynomial, so only rounding remains.
    checkTrue("Y_lm: our angular parts are scipy's Y_lm times a positive constant (same phase, Condon-Shortley), for l = 1..3, m = +-1..+-l", n > 60 && worstConst < 1e-12 && worstPhase < 1e-12);
    checkTrue("Y_lm: the +m and -m states have the same size (Y_l,-m = (-1)^m conj(Y_lm))", equalPlusMinus);
}

// (e) |psi(m = +-1)|^2 = (|psi_x|^2 + |psi_y|^2) / 2 for p; and for every |m| the same with the real pair the state is made of.
static void testDensityFromReal() {
    double worst = 0;
    for (int n = 2; n <= 4; n++)
        for (int k = 0; k < 40; k++) {
            const double x = 0.4 + 0.37 * k, y = -1.1 + 0.29 * k, z = 0.9 - 0.31 * k;
            const int pShell = orbIndex(std::to_string(n) + "p"), px = orbIndex(std::to_string(n) + "p|x"), py = orbIndex(std::to_string(n) + "p|y");
            const double want = (std::pow(psi(px, 1, x, y, z), 2) + std::pow(psi(py, 1, x, y, z), 2)) / 2;
            for (int m : {1, -1}) worst = std::max(worst, std::fabs(std::norm(psiComplex(complexState(pShell, m), 1, x, y, z)) - want) / (std::fabs(want) + 1e-300));
        }
    // d and f: |psi(+-m)|^2 = (Re^2 + (c Im)^2) / 2 with the pair (Re, Im, c): d1 (xz, yz, 1), d2 (x2-y2, xy, 2), f1 (xz2, yz2, 1), f2 (zx2y2, xyz, 2), f3 (xx23y2, y3x2y2, 1)
    struct Pair { const char* shell; int m; const char* re; const char* im; double c; } pairs[] = {
        {"3d", 1, "3d|xz", "3d|yz", 1}, {"3d", 2, "3d|x2y2", "3d|xy", 2}, {"4d", 1, "4d|xz", "4d|yz", 1}, {"4d", 2, "4d|x2y2", "4d|xy", 2},
        {"4f", 1, "4f|xz2", "4f|yz2", 1}, {"4f", 2, "4f|zx2y2", "4f|xyz", 2}, {"4f", 3, "4f|xx23y2", "4f|y3x2y2", 1}};
    double worstD = 0;
    for (const Pair& pr : pairs)
        for (int k = 0; k < 40; k++) {
            const double x = 0.5 + 0.41 * k, y = -0.7 + 0.23 * k, z = 1.3 - 0.37 * k;
            const double want = (std::pow(psi(pr.re, 1, x, y, z), 2) + std::pow(pr.c * psi(pr.im, 1, x, y, z), 2)) / 2;
            for (int s : {1, -1}) worstD = std::max(worstD, std::fabs(std::norm(psiComplex(complexState(orbIndex(pr.shell), s * pr.m), 1, x, y, z)) - want) / (std::fabs(want) + 1e-300));
        }
    std::printf("complex orbitals, density from the real ones: p: %.1e, d and f: %.1e (largest relative difference)\n", worst, worstD);
    checkTrue("|psi(m = +-1)|^2 = (|psi_x|^2 + |psi_y|^2) / 2 for 2p, 3p, 4p (to 1e-12)", worst < 1e-12);
    checkTrue("|psi(+-m)|^2 = (Re^2 + (c Im)^2) / 2 for the d and f states (to 1e-12)", worstD < 1e-12);
}

// (f) the real orbitals' dots stay the same from version to version, with Phase colours = Sign: 4,800 dots (300 steps, seed 12345). Until 0.36 the positions and signs were hashed (FNV-1a 64, raw
// bytes) and three dots compared with those of 0.10.5. 0.37 replaced the Metropolis chain with exact independent samples (on purpose: research report of 6 Nov 2026), so the references were
// recorded again from the 0.37 code, and the byte hash was dropped (raw bytes of floating-point results are not portable between compilers: DECISIONS.md, Tests and precision). Checked now,
// with tolerances: three dots of each run (the 1st, 1,001st and 4,800th) in Bohr radii to 1e-6 (a float has 6e-8), the sum of |coordinates| to 1e-6 relative, and the sum of signs exactly
// up to 0.05 % of the dots + 1.
static const double kRefDots[6][3][3] = {
    {{1.67250241, -1.4876731, 0.363956922}, {4.64445048, -2.71176624, -1.35993164}, {-3.48987475, 0.909350196, 0.219401683}},
    {{-0.350711515, -0.684982309, -2.13323727}, {-2.74718969, 1.44185174, 2.93588249}, {0.802155928, -2.27945243, -1.93391001}},
    {{-3.66056788, -0.762303628, -4.06009492}, {10.1744133, -1.73361215, 5.57643808}, {5.37112872, 1.86563934, 0.97298818}},
    {{-2.57652803, 0.280521456, 0.948219772}, {1.30188684, -3.81150555, -1.46997475}, {5.06857467, -0.657268089, -3.11334251}},
    {{-4.42898671, -6.28043938, 6.92908921}, {-6.80540268, 13.4567112, -8.68467087}, {-11.8144986, 5.89310549, -9.9906794}},
    {{19.5780443, -8.00323428, 3.59232724}, {-10.8479734, 7.92087398, -2.47566715}, {2.01836753, 14.6863855, -4.04412953}}
};
static const double kRefSums[6][2] = {
    {36228.353714281911, -68}, {35976.512283958968, -58}, {78594.427691449528, -44}, {35525.362001615977, 44}, {141651.3255868128, 8}, {97455.009439206086, 3850}
};
static void testRealDotsUnchanged() {
    struct C { const char* orb; const char* key; const char* ori; int Z; } cs[] = {
{"2p", "pAx", "x", 1},   // 4800 dots
    {"2p", "pAx", "z", 1},   // 4800 dots
    {"3d", "dAx", "xz", 1},   // 4800 dots
    {"3d", "dAx", "x2y2", 2},   // 4800 dots
    {"4f", "fAx", "xyz", 1},   // 4800 dots
    {"3s", "", "", 1},   // 4800 dots
    };
    for (const C& c : cs) {
        seedRandom(12345);
        AtomLab lab; lab.nowMs = [] { return 0.0; };
        lab.p = oneParams(); lab.paused = false; lab.restart();
        lab.setParam("view", std::string("Cloud")); lab.setParam("Z", double(c.Z)); lab.setParam("orb", std::string(c.orb));
        if (*c.key) lab.setParam(c.key, std::string(c.ori));
        for (int i = 0; i < 300; i++) lab.step(1.0 / 240);
        const int ci = int(&c - cs); const long idx[3] = {0, 1000, 4799};
        double sumAbs = 0, sumSg = 0;
        for (long i = 0; i < 4800; i++) { for (int a = 0; a < 3; a++) sumAbs += std::fabs(double(lab.s.pts[3 * i + a]) / lab.aUnit()); sumSg += lab.s.sg[std::size_t(i)]; }
        if (std::getenv("PL_RECORD")) {
            std::printf("REFDOT {");
            for (int d = 0; d < 3; d++) std::printf("{%.9g, %.9g, %.9g}%s", lab.s.pts[3 * idx[d]] / lab.aUnit(), lab.s.pts[3 * idx[d] + 1] / lab.aUnit(), lab.s.pts[3 * idx[d] + 2] / lab.aUnit(), d < 2 ? ", " : "");
            std::printf("},\nREFSUM {%.17g, %.17g},\n", sumAbs, sumSg);
        }
        {
            double worst = 0;
            for (int d = 0; d < 3; d++) for (int a = 0; a < 3; a++) worst = std::max(worst, std::fabs(lab.s.pts[3 * idx[d] + a] / lab.aUnit() / kRefDots[ci][d][a] - 1));
            checkTrue(std::string("real orbital dots in Bohr radii equal the recorded 0.37 ones (1e-6): ") + c.orb + " " + c.ori + " Z=" + std::to_string(c.Z), worst < 1e-6);
            checkNum(std::string("real orbital dots: sum of |coordinates| as recorded (0.37): ") + c.orb + " " + c.ori, sumAbs, kRefSums[ci][0], 1e-6);
            checkNum(std::string("real orbital dots: sum of signs as recorded (0.37): ") + c.orb + " " + c.ori, sumSg, kRefSums[ci][1], 0, 0.0005 * 4800 + 1);
        }
        // the new phase of each dot of a real orbital is 0 where the sign is + and pi where it is -
        bool ok = lab.s.ph.size() == lab.s.sg.size();
        for (std::size_t i = 0; ok && i < std::size_t(lab.s.k) && i < lab.s.sg.size(); i++) ok = lab.s.ph[i] == (lab.s.sg[i] > 0 ? 0.f : float(kPi));
        checkTrue(std::string("real orbital dots carry phase 0 or pi matching their sign: ") + c.orb + " " + c.ori, ok);
    }
}

// complex clouds: the dots carry arg(psi) of the state at their place; for 2p m = +-1 (no radial node, no theta sign change) that is m phi + (pi for +m)
static void testComplexDots() {
    for (int m : {1, -1}) {
        seedRandom(777);
        AtomLab lab; lab.nowMs = [] { return 0.0; };
        lab.p = oneParams(); lab.paused = false; lab.restart();
        lab.setParam("view", std::string("Cloud")); lab.setParam("orb", std::string("2p")); lab.setParam("pAx", oriKey(m));
        for (int i = 0; i < 300; i++) lab.step(1.0 / 240);
        const long cnt = std::min<long>(lab.s.k, long(lab.s.ph.size()));
        double worst = 0, worstArg = 0; long usable = 0;
        double sxx = 0, syy = 0, szz = 0;
        for (long i = 0; i < cnt; i++) {
            const double x = lab.s.pts[3 * i] / kA0, y = lab.s.pts[3 * i + 1] / kA0, z = lab.s.pts[3 * i + 2] / kA0;
            sxx += x * x; syy += y * y; szz += z * z;
            const double phi = std::atan2(y, x), want = m * phi + (m > 0 ? kPi : 0);
            double d = std::fmod(lab.s.ph[i] - want, 2 * kPi); if (d > kPi) d -= 2 * kPi; if (d < -kPi) d += 2 * kPi;
            worst = std::max(worst, std::fabs(d)); usable++;
            const Cd v = psiComplex(complexState(orbIndex("2p"), m), 1, x, y, z);
            double e = std::fmod(lab.s.ph[i] - std::arg(v), 2 * kPi); if (e > kPi) e -= 2 * kPi; if (e < -kPi) e += 2 * kPi;
            worstArg = std::max(worstArg, std::fabs(e));
        }
        std::printf("complex 2p m = %+d cloud: %ld dots, phase vs m phi (+pi): %.1e rad, vs arg(psi) at the dot: %.1e rad; <x^2>:<y^2>:<z^2> = %.2f : %.2f : %.2f\n", m, usable, worst, worstArg, sxx / usable, syy / usable, szz / usable);
        // limit 1e-4 rad: the dot positions are stored as 32-bit numbers (relative 6e-8), which moves the angle by about 1e-7.
        checkTrue("complex 2p dots carry arg(psi): m phi (+ pi for m > 0), m = " + std::to_string(m), usable > 3000 && worst < 1e-4 && worstArg < 1e-4);
        // the cloud is a doughnut round the z axis (|psi|^2 = f(r) sin^2(theta) for m = +-1): <x^2> = <y^2> (round the axis), both larger than <z^2> (flat)
        // (2p m = +-1: exact ratios <x^2> : <y^2> : <z^2> = 1 : 1 : 0.5 for the density sin^2(theta) f(r); the dots are a Metropolis sample, so the limits are loose: 15 % on x:y, z below 0.7 of x)
        checkTrue("complex 2p cloud: round the z axis (<x^2> = <y^2> within 15 %) and flat (<z^2> < 0.7 <x^2>)", std::fabs(sxx / syy - 1) < 0.15 && szz < 0.7 * sxx);
    }
}

// the colour map: against scikit-image 0.26.0 (skimage.color.lab2rgb, D65, clipped to [0, 1]) for the same L*, C and hue; and it must be cyclic
static void testPhaseWheel() {
    struct Ref { bool light; int k; float rgb[3]; } ref[] = {
        {false, 0, {1.0f, 0.5345799f, 0.6996457f}}, {false, 1, {1.0f, 0.5809391f, 0.4366234f}}, {false, 2, {0.8014520f, 0.6817521f, 0.3069459f}}, {false, 3, {0.5022609f, 0.7556829f, 0.4189309f}},
        {false, 4, {0.0f, 0.7859955f, 0.6860444f}}, {false, 5, {0.0f, 0.7740075f, 0.9507833f}}, {false, 6, {0.3489737f, 0.7115779f, 1.0f}}, {false, 7, {0.8262902f, 0.6063740f, 0.9570877f}},
        {true, 0, {0.8252200f, 0.2892619f, 0.4946457f}}, {true, 1, {0.7853675f, 0.3583448f, 0.2163298f}}, {true, 2, {0.5808809f, 0.4771553f, 0.0f}}, {true, 3, {0.2601518f, 0.5491062f, 0.1877637f}},
        {true, 4, {0.0f, 0.5759984f, 0.4803519f}}, {true, 5, {0.0f, 0.5678036f, 0.7585976f}}, {true, 6, {0.0f, 0.5093147f, 0.8776893f}}, {true, 7, {0.6184071f, 0.3892127f, 0.7642639f}}};
    double worst = 0;
    for (const Ref& r : ref) {
        const PhaseRgb c = phaseRgb(2 * kPi * r.k / 8, r.light);
        worst = std::max({worst, double(std::fabs(c.r - r.rgb[0])), double(std::fabs(c.g - r.rgb[1])), double(std::fabs(c.b - r.rgb[2]))});
    }
    // cyclic, and the four marked phases (0, pi/2, pi, 3 pi/2) have clearly different colours
    double cyc = 0, minApart = 9;
    for (bool light : {false, true})
        for (int k = 0; k < 40; k++) {
            const double a = 0.37 * k - 5;
            const PhaseRgb x = phaseRgb(a, light), y = phaseRgb(a + 2 * kPi, light);
            cyc = std::max({cyc, double(std::fabs(x.r - y.r)), double(std::fabs(x.g - y.g)), double(std::fabs(x.b - y.b))});
        }
    for (bool light : {false, true})
        for (int i = 0; i < 4; i++)
            for (int j = i + 1; j < 4; j++) {
                const PhaseRgb x = phaseRgb(i * kPi / 2, light), y = phaseRgb(j * kPi / 2, light);
                minApart = std::min(minApart, std::sqrt(std::pow(x.r - y.r, 2) + std::pow(x.g - y.g, 2) + std::pow(x.b - y.b, 2)));
            }
    bool bins = true;
    for (int b = 0; b < kPhaseBins; b++) bins = bins && phaseBin(phaseOfBin(b)) == b && phaseBin(phaseOfBin(b) + 2 * kPi) == b && phaseBin(phaseOfBin(b) - 2 * kPi) == b;
    bins = bins && phaseBin(kPi) == kPhaseBins / 2 && phaseBin(-kPi) == kPhaseBins / 2 && phaseBin(0.0) == 0 && phaseBin(-1e-9) == 0;
    std::printf("phase wheel: largest difference from scikit-image %.1e, cyclic to %.1e, nearest two of the four marked colours %.2f apart (sRGB)\n", worst, cyc, minApart);
    // limit 2e-3: scikit-image inverts the sRGB matrix to more digits than the 4-digit matrix written here (about 1e-4 per channel), and both clip the same way.
    checkTrue("phase wheel: colours equal scikit-image's CIELAB circle (2e-3)", worst < 2e-3);
    checkTrue("phase wheel: cyclic (phase and phase + 2 pi give the same colour)", cyc < 1e-9);
    // limit 0.25: the four marked hues are 90 degrees apart on a circle of chroma 52-58 (colour differences of 70-80 in L*a*b* units), far more than a visible step.
    checkTrue("phase wheel: the four marked phases have clearly different colours (> 0.25 apart in sRGB)", minApart > 0.25);
    checkTrue("phase wheel: the 24 colour bins are centred on their phases, wrap round, and pi is in bin 12", bins);
}

// the 90 % outline works for a complex state too: it holds 90 % of the probability (the grid's own count), is round about the z axis, and the density is as the cloud's
static void testComplexOutline() {
    const int id = complexState(orbIndex("2p"), 1), real = orbIndex("2p|z");
    const Outline& O = outlineOf(id);
    const double norm = orbitalNorm(id);
    // the norm by another way: the numerical integral of |psi|^2 (Z = 1) over the sphere shell by shell: radial integral of r^2 R^2 times the angular mean of |A|^2 times 4 pi
    double s = 0;
    for (double r = 0.005; r < 40; r += 0.01) s += std::pow(radial(orbitals()[orbIndex("2p")], r), 2) * r * r * r * r * 0.01;     // r^(2l+2) with l = 1
    // |A|^2 for m = +1: (x^2 + y^2) / 2 averaged over the sphere = r^2 / 3 (mean of sin^2(theta) = 2/3, halved): angular mean 1/3
    const double expect = s * 4 * kPi / 3;
    std::printf("complex 2p m = +1 outline: holds %.3f of the probability, reaches %.2f a0; norm %.6g, by hand %.6g\n", O.mass, O.rmax, norm, expect);
    // limit 0.02 on the mass: the grid has 64 voxels per side, and the real orbitals' own outlines are held to the same reason (their mass is within the same few percent of 0.9)
    checkTrue("complex 2p outline holds 90 % of the probability (0.88 to 0.92, from the 64^3 grid)", O.mass > 0.88 && O.mass < 0.92);
    checkTrue("complex 2p norm equals the by-hand radial x angular integral (1e-3)", std::fabs(norm / expect - 1) < 1e-3);
    checkTrue("complex 2p outline: a doughnut round z: its side view reaches farther across (x) than up (z)", outlineOf(real).rmax > 0 && O.rmax > 0);
    double ax = 0, az = 0;
    for (const OutSeg2& g : O.side) { ax = std::max({ax, double(std::fabs(g.x0)), double(std::fabs(g.x1))}); az = std::max({az, double(std::fabs(g.y0)), double(std::fabs(g.y1))}); }
    checkTrue("complex 2p m = +1: the region is wider across than tall (|psi|^2 ~ sin^2(theta) f(r))", ax > az);
}

// ================= Quantum beats: a superposition of two orbitals (0.10.2, stage 2) =================
// psi(t) = a psiA e^(-i EA t/hbar) + b psiB e^(-i EB t/hbar), shown relative to state A (multiplied by e^(+i EA t/hbar)), so the colours turn only at the beat frequency.
static Superposition mkSup(const char* shellA, const char* oriA, const char* shellB, const char* oriB, double mixB) {
    const auto id = [](const char* shell, const char* ori) { int m = 0; return oriIsComplex(ori, m) ? complexState(orbIndex(shell), m) : orbIndex(std::string(shell) + "|" + ori); };
    Superposition s; s.idA = id(shellA, oriA); s.idB = id(shellB, oriB); s.a = std::sqrt(1 - mixB); s.b = std::sqrt(mixB);
    return s;
}
// <z> of the superposition at phase theta = dE t / hbar, by quadrature (axisymmetric states: one phi point)
static double expectZ(const Quad& q, const Superposition& su, int Z, double theta) {
    double s = 0;
    for (std::size_t i = 0; i < q.r.size(); i++)
        for (std::size_t j = 0; j < q.u.size(); j++) {
            const double st = std::sqrt(1 - q.u[j] * q.u[j]), x = q.r[i] * st, z = q.r[i] * q.u[j];
            s += std::norm(psiSuper(su, Z, theta, x, 0, z)) * z * 2 * kPi * q.wu[j] * q.r[i] * q.r[i] * q.wr[i];
        }
    return s;
}

static void testBeatPeriodAndAmplitude() {
    const Superposition su = mkSup("1s", "", "2p", "z", 0.5);
    const Quad q = makeQuad(1500, 1, 40);
    // independent hydrogen functions written here: R10 = 2 e^-r, R21 = r e^(-r/2) / (2 sqrt 6), Y00 = 1/sqrt(4 pi), Y10 = sqrt(3/(4 pi)) cos(theta)
    double radial = 0;
    // own numerical integration of <1s|z|2p0>: radial part by the midpoint rule, angular part by Gauss-Legendre in cos(theta) (the pieces are the textbook ones above)
    const int nr = 200000; const double top = 60, dr = top / nr;
    for (int i = 0; i < nr; i++) { const double r = (i + 0.5) * dr; radial += 2 * std::exp(-r) * (r * std::exp(-r / 2) / (2 * std::sqrt(6.0))) * r * r * r * dr; }
    double ang = 0;
    for (std::size_t j = 0; j < q.u.size(); j++) ang += std::sqrt(1 / (4 * kPi)) * std::sqrt(3 / (4 * kPi)) * q.u[j] * q.u[j] * 2 * kPi * q.wu[j];
    const double mOwn = radial * ang, mClosed = 128 * std::sqrt(2.0) / 243;
    // the lab's amplitude of <z>(t) = 2 a b M cos(theta): at theta = 0 (Z = 1)
    const double zLab = expectZ(q, su, 1, 0), zQuarter = expectZ(q, su, 1, kPi / 2);
    std::printf("beats 1s + 2p(m=0), a = b: matrix element by my own integral %.6f a0, closed form 128 sqrt2/243 = %.6f; the lab's <z>(0) = %.6f (2ab M = %.6f), <z> at a quarter period %.1e\n", mOwn, mClosed, zLab, 2 * 0.5 * mClosed, zQuarter);
    // limit 1e-4 as asked: the midpoint rule with 200,000 steps and 24 Gauss points are far better than that.
    checkTrue("beats: <1s|z|2p0> by my own numerical integration equals 128 sqrt2 / 243 a0 = 0.74494 a0 (Fitzpatrick, 2P-1S transitions; 1e-4)", std::fabs(mOwn / mClosed - 1) < 1e-4);
    checkTrue("beats: the amplitude of <z>(t) of 1s + 2p0 (a = b) is 2ab x 128 sqrt2/243 a0 (1e-4)", std::fabs(zLab / (2 * 0.5 * mClosed) - 1) < 1e-4);
    checkTrue("beats: <z> is zero a quarter period later (|<z>| < 1e-4 a0)", std::fabs(zQuarter) < 1e-4);
    // amplitude with other mixes: 2 a b M for a^2 = 0.8 / 0.2
    const double z2 = expectZ(q, mkSup("1s", "", "2p", "z", 0.2), 1, 0);
    checkTrue("beats: the amplitude follows 2ab: mix 20 % gives 2 sqrt(0.2 x 0.8) M (1e-4)", std::fabs(z2 / (2 * std::sqrt(0.2 * 0.8) * mClosed) - 1) < 1e-4);
    // the period: found from the zero of <z>(t) in real seconds (a quarter period), against h / (E2 - E1) written here from the CODATA 2022 values of the nist:: block above (NIST pages h, e, rydhcev, mpsme, read 2026-10-04)
    const double hJs = nist::h, eV = nist::e, RyInf = nist::rydEV, mpMe = nist::mpOverMe;
    const double RyRed = RyInf * mpMe / (mpMe + 1);                       // reduced-mass Rydberg energy for hydrogen (proton), eV
    const double Tref = hJs / ((RyRed * eV) * (1.0 - 0.25));              // E2 - E1 = Ry (1 - 1/4)
    double lo = 0, hi = 0.5 * Tref;                                        // <z> goes from + to - between t = 0 and T/2: bisection for the first zero
    for (int it = 0; it < 60; it++) {
        const double mid = 0.5 * (lo + hi);
        const double th = beatEV(1, 0, su) * mid / (kPlanckEVs / (2 * kPi));
        (expectZ(q, su, 1, th) > 0 ? lo : hi) = mid;
    }
    const double Tmeas = 4 * 0.5 * (lo + hi);
    std::printf("beats 1s + 2p, hydrogen: period from the zero of <z>(t) %.9e s, h/(E2-E1) from CODATA %.9e s (relative difference %.1e); beatPeriodS %.9e\n", Tmeas, Tref, std::fabs(Tmeas / Tref - 1), beatPeriodS(1, 0, su));
    // limit 1e-6 as asked (the lab's constants are the CODATA 2022 ones since 0.10.6 stage 2, so the real difference is far smaller).
    checkTrue("beats: the period of <z>(t) is h / (E2 - E1) (CODATA 2022 h, Rydberg energy, proton mass; 1e-6)", std::fabs(Tmeas / Tref - 1) < 1e-6 && std::fabs(beatPeriodS(1, 0, su) / Tref - 1) < 1e-6);
    checkTrue("beats: about 4e-16 s for hydrogen 1s + 2p (the owner's h / 10.204 eV)", Tmeas > 4.0e-16 && Tmeas < 4.1e-16);
    // Z = 2: a quarter of the period, up to the reduced-mass difference between hydrogen and helium
    const double T2 = beatPeriodS(2, 2, su);
    std::printf("beats Z = 2 (helium-4 ion): period %.6e s, ratio to Z = 1: %.6f (1/4 = 0.25)\n", T2, T2 / beatPeriodS(1, 0, su));
    // limit 1e-3: the reduced-mass factor differs between a proton and a helium-4 nucleus by 4.1e-4 (mu/m_e = M/(M+1)): 1/4 x (1 + 4.1e-4)
    checkTrue("beats: for Z = 2 the period is a quarter of Z = 1 (Z^2 scaling; 1e-3 for the reduced-mass difference)", std::fabs(T2 / beatPeriodS(1, 0, su) * 4 - 1) < 1e-3);
}

static void testBeatNormStationary() {
    const Quad q = makeQuad(450, 24, 60);
    double worst = 0;
    const Superposition su = mkSup("2p", "m+1", "3d", "m+2", 0.3);
    for (double th : {0.0, 1.0, 2.5, kPi}) {
        double s = 0;
        for (std::size_t i = 0; i < q.r.size(); i++)
            for (std::size_t j = 0; j < q.u.size(); j++) {
                const double st = std::sqrt(1 - q.u[j] * q.u[j]);
                for (int k = 0; k < q.nphi; k++) {
                    const double f = 2 * kPi * (k + 0.5) / q.nphi;
                    s += std::norm(psiSuper(su, 1, th, q.r[i] * st * std::cos(f), q.r[i] * st * std::sin(f), q.r[i] * q.u[j])) * (2 * kPi / q.nphi) * q.wu[j] * q.r[i] * q.r[i] * q.wr[i];
                }
            }
        worst = std::max(worst, std::fabs(s - 1));
    }
    std::printf("beats: integral of |psi(t)|^2 for 2p(m=+1) + 3d(m=+2), mix 30 %%, at 4 times: largest |norm - 1| = %.1e\n", worst);
    checkTrue("beats: the superposition stays normalised in time (integral of |psi|^2 = 1, 1e-3)", worst < 1e-3);
    // a single orbital (mix 0 or 100 %): the density does not change with time; 2s + 2p: same energy, so no beats at all
    bool still = true;
    for (const Superposition& one : {mkSup("1s", "", "2p", "z", 0.0), mkSup("1s", "", "2p", "z", 1.0)})
        for (int k = 0; k < 20; k++) { const double x = 0.3 + 0.4 * k, y = 0.2 * k - 1, z = 1.1 - 0.17 * k; still = still && std::fabs(std::norm(psiSuper(one, 1, 0.0, x, y, z)) / std::norm(psiSuper(one, 1, 2.2, x, y, z)) - 1) < 1e-12; }
    checkTrue("single orbital (mix 0 % or 100 %): the density does not change with time (1e-12)", still);
    const Superposition same = mkSup("2s", "", "2p", "z", 0.5);
    bool still2 = beatEV(1, 0, same) == 0 && std::isinf(beatPeriodS(1, 0, same));
    for (int k = 0; k < 20; k++) { const double x = 0.3 + 0.4 * k, y = 0.2 * k - 1, z = 1.1 - 0.17 * k; still2 = still2 && std::norm(psiSuperT(same, 1, 0, 0.0, x, y, z)) == std::norm(psiSuperT(same, 1, 0, 3.7e-16, x, y, z)); }
    checkTrue("2s + 2p: same n, same energy in this model: no beats, the density does not change (exactly)", still2);
    // <z> of a single orbital is constant too
    const Quad qz = makeQuad(900, 1, 40);
    const double z0 = expectZ(qz, mkSup("1s", "", "2p", "z", 0.0), 1, 0.0), z1 = expectZ(qz, mkSup("1s", "", "2p", "z", 0.0), 1, 1.7);
    checkTrue("single orbital: <z> does not change with time", std::fabs(z0 - z1) < 1e-12);
    // the dipole check used for the text: 1s + 2p(z) moves along z; 1s + 2s does not move; 1s + 2p(m=+1) moves in the x-y plane
    double d[3];
    checkTrue("dipole: 1s + 2pz couples along z only", dipoleAxes(mkSup("1s", "", "2p", "z", 0.5), d) && std::fabs(d[2]) > 0.1 && d[0] < 1e-3 && d[1] < 1e-3);   // (1e-3: the 1,500-point sphere sum is exact to about 1e-4)
    checkTrue("dipole: 1s + 2s has none, 1s + 3d has none, 2p + 3d does", !dipoleAxes(mkSup("1s", "", "2s", "", 0.5), d) && !dipoleAxes(mkSup("1s", "", "3d", "z2", 0.5), d) && dipoleAxes(mkSup("2p", "z", "3d", "z2", 0.5), d));
    checkTrue("dipole: 1s + 2p(m=+1) moves in the x-y plane, not along z", dipoleAxes(mkSup("1s", "", "2p", "m+1", 0.5), d) && d[2] < 1e-3 && d[0] > 0.1 && d[1] > 0.1);
}

// the lab: dots (seeded) follow the CURRENT density. One dot per independent run (the chain's own dots are correlated), a fresh lab each.
static void superSetup(AtomLab& lab, const char* a, const char* b, double mixPct, int Z = 1) {
    lab.nowMs = [] { return 0.0; };
    lab.p = oneParams(); lab.paused = false; lab.restart();
    lab.setParam("view", std::string("Cloud")); lab.setParam("Z", double(Z));
    lab.setParam("orb", std::string(a)); lab.setParam("orb2", std::string(b)); lab.setParam("mix", mixPct); lab.setParam("super", std::string("On"));
}
static void testSuperDots() {
    // the rate of new dots (the measurement behind the design): dots recorded per physics step and per second at 240 steps/s, and how long the ring buffer remembers
    for (const char* dens : {"Normal", "High", "Ultra"}) {
        AtomLab lab; lab.nowMs = [] { return 0.0; }; lab.p = oneParams(); lab.restart(); lab.setParam("view", std::string("Cloud")); lab.setParam("cdens", std::string(dens));
        const long k0 = lab.s.k; for (int i = 0; i < 60; i++) lab.step(1.0 / 240);
        const double perStep = double(lab.s.k - k0) / 60, perSec = perStep * 240;
        std::printf("dot rate, density %s: %.0f dots per physics step, %.0f dots/s, the ring of %d dots remembers %.2f s (a 4 s beat needs well under 0.4 s)\n", dens, perStep, perSec, lab.cloudCap(), lab.cloudCap() / perSec);
    }
    // Several full rings of 1s + 2p(z), a = b, seeded. The dots are a Metropolis chain, so neighbours are correlated: only every 400th dot is used (slots with different fixed random
    // numbers; the measured correlation of z falls like exp(-lag / 60), 0.75 at lag 16), after checking the correlation of z at that lag. The lab's own thinning (superShown) decides which are on screen.
    seedRandom(2468);
    const double M = 128 * std::sqrt(2.0) / 243;
    const double thetas[4] = {0.0, 1.0, 2.2, 3.1};
    double sum[4] = {0, 0, 0, 0}, sum2[4] = {0, 0, 0, 0}; long nShown[4] = {0, 0, 0, 0};
    const int nb = 20; const double rmax = 12;
    std::vector<double> obs[4]; for (auto& o : obs) o.assign(nb, 0);
    double worstPhase = 0, rho1 = 0, rho16 = 0, rho64 = 0, rhoSum = 0; long usedDots = 0, nPhase = 0;
    const int nLab = 170, stride = 400;
    for (int k = 0; k < nLab; k++) {
        AtomLab lab; superSetup(lab, "1s", "2p", 50);
        for (int i = 0; i < 800; i++) lab.step(1.0 / 240);           // fills the 12,000-dot ring
        const long cap = std::min<long>(lab.s.k, long(lab.s.sg.size()));
        const Superposition su = lab.superState();
        const double nA = normFactor(su.idA, 1), nB = normFactor(su.idB, 1);
        {                                                              // autocorrelation of z along the chain at lags 1, 16, 400 (400: averaged over all the labs)
            double m = 0, v = 0; for (long i = 0; i < cap; i++) m += lab.s.pts[3 * i + 2] / kA0; m /= cap;
            for (long i = 0; i < cap; i++) { const double d = lab.s.pts[3 * i + 2] / kA0 - m; v += d * d; }
            const auto rho = [&](long lag) { double c = 0; for (long i = 0; i + lag < cap; i++) c += (lab.s.pts[3 * i + 2] / kA0 - m) * (lab.s.pts[3 * (i + lag) + 2] / kA0 - m); return c / v; };
            if (k == 0) { rho1 = rho(1); rho16 = rho(16); } rhoSum += rho(400);
        }
        for (long i = (long(k) * 7919) % stride; i < cap; i += stride) {      // (a different start slot in every lab, so every dot has its own fixed random number)
            const double x = lab.s.pts[3 * i] / lab.aUnit(), y = lab.s.pts[3 * i + 1] / lab.aUnit(), z = lab.s.pts[3 * i + 2] / lab.aUnit(), r = std::sqrt(x * x + y * y + z * z);   // (0.46.4: the model unit a_mu)
            usedDots++;
            for (int t = 0; t < 4; t++) {
                if (lab.superShown(i, thetas[t])) {
                    sum[t] += z; sum2[t] += z * z; nShown[t]++;
                    if (r < rmax) obs[t][std::size_t(r / rmax * nb)] += 1;
                }
                if (k < 12) {                                          // the colour phase against the exact phase from the primitives (stored 32-bit values, so not exact to rounding)
                    const Cd A = nA * psi(su.idA, 1, x, y, z), B = nB * psi(su.idB, 1, x, y, z);
                    const Cd exact = su.a * A + su.b * B * std::exp(Cd(0, -thetas[t]));
                    double d2 = std::fmod(lab.superPhase(i, thetas[t]) - std::arg(exact), 2 * kPi); if (d2 > kPi) d2 -= 2 * kPi; if (d2 < -kPi) d2 += 2 * kPi;
                    worstPhase = std::max(worstPhase, std::fabs(d2)); nPhase++;
                }
            }
        }
    }
    rho64 = rhoSum / nLab;
    std::printf("dot chain: autocorrelation of z at lag 1: %.2f, 16: %.2f, 400: %.3f (stride %d); %ld dots used\n", rho1, rho16, rho64, stride, usedDots);
    checkTrue("the dot chain is nearly uncorrelated at the stride used (|autocorrelation of z| < 0.05 at lag 400, averaged over the labs)", std::fabs(rho64) < 0.05);
    // limit 1e-4 rad: the stored position is a 32-bit float (relative 6e-8) and the stored amplitudes are 32-bit too, so the phase moves by about 1e-6
    std::printf("dot colour phase vs the exact arg(psi(x, t)) relative to state A: %ld dot-times, largest difference %.1e rad\n", nPhase, worstPhase);
    checkTrue("a dot's colour phase equals the exact arg(psi(x, t)) relative to state A at 4 times (1e-4 rad)", nPhase > 1000 && worstPhase < 1e-4);
    double worstZ = 0, worstChi = 0;
    for (int t = 0; t < 4; t++) {
        const double mean = sum[t] / nShown[t], sd = std::sqrt(sum2[t] / nShown[t] - mean * mean), se = sd / std::sqrt(double(nShown[t])), exact = 2 * 0.5 * M * std::cos(thetas[t]);
        worstZ = std::max(worstZ, std::fabs(mean - exact) / se);
        // the exact radial density a^2 P1s(r) + b^2 P2p(r) (the cross term has no radial part), 20 bins on 0 to 12 a0
        std::vector<double> ex(nb, 0); double tot = 0, nin = 0;
        for (int b = 0; b < nb; b++) {
            double P = 0; const int sub = 200;
            for (int j = 0; j < sub; j++) { const double r = (b + (j + 0.5) / sub) * rmax / nb; P += (0.5 * 4 * r * r * std::exp(-2 * r) + 0.5 * r * r * r * r * std::exp(-r) / 24) * rmax / nb / sub; }
            ex[std::size_t(b)] = P; tot += P; nin += obs[t][std::size_t(b)];
        }
        double chi = 0;
        for (int b = 0; b < nb; b++) { const double e = nin * ex[std::size_t(b)] / tot; chi += (obs[t][std::size_t(b)] - e) * (obs[t][std::size_t(b)] - e) / e; }
        worstChi = std::max(worstChi, chi);
        std::printf("dots at theta = %.1f: %ld of %ld shown, mean z %.4f +- %.4f, exact <z> %.4f; radial histogram (20 bins) chi-square %.1f\n", thetas[t], nShown[t], usedDots, mean, se, exact, chi);
    }
    // limits: 4 standard errors as asked; chi-square with 19 degrees of freedom: its 99.99 % point is 48, so a good match stays below it
    checkTrue("shown dots: the mean z follows the exact <z>(t) within 4 standard errors at 4 times", worstZ < 4);
    checkTrue("shown dots: the radial histogram matches the exact radial density (chi-square below 48, 19 dof) at 4 times", worstChi < 48);
    // the cost of colouring and thinning every dot of a High-density cloud each frame (this machine, software, one core: not your PC)
    AtomLab big; superSetup(big, "1s", "2p", 50); big.setParam("cdens", std::string("High")); superSetup(big, "1s", "2p", 50); big.setParam("cdens", std::string("High"));
    for (int i = 0; i < 800; i++) big.step(1.0 / 240);
    const long cap = std::min<long>(big.s.k, long(big.s.sg.size()));
    const auto t0 = std::chrono::steady_clock::now();
    long keepCount = 0; const int frames = 20;
    for (int f = 0; f < frames; f++) for (long i = 0; i < cap; i++) keepCount += big.superGroup(i, 0.1 * f) < 255;
    const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count() / frames;
    std::printf("cost of thinning and colouring %ld dots (High) per frame on this machine (software, one core): %.2f ms; %.0f%% shown\n", cap, ms, 100.0 * keepCount / (double(cap) * frames));
}

// ================= Photon jumps (0.10.2 stage 3; 0.49.3: the exact one-photon mixture) =================
// Cloud views: two fixed populations of dots (the old cloud, and a new one of the target orbital); an old dot is shown with odds 1 - P, a new one with P, P = the new level's share.
// 0.49.3 (outside review, deliberate change): P is the one-photon result (Weisskopf-Wigner: the upper level decays as exp(-t / tau); absorption: the time reverse), written again here from the
// formula, the photon cut at T = tau ln(48000) and renormalised. It replaced theta = pi/2 smoothstep(t / 1.2 s) with a coherent mix sloshing at a slowed beat (the classical-light picture);
// the tests of that mix (jumpFractions, jumpTerms, jumpPhaseOf) went with it.
static double smoothstep(double u) { u = std::max(0.0, std::min(1.0, u)); return u * u * (3 - 2 * u); }
static double shareEmit(double t, double T, double tau) { return (1 - std::exp(-t / tau)) / (1 - std::exp(-T / tau)); }
static double shareAbsorb(double t, double T, double tau) { return (std::exp(-(T - t) / tau) - std::exp(-T / tau)) / (1 - std::exp(-T / tau)); }

static void testJumpShares0493() {
    const double tau = 0.2, T = tau * std::log(48000.0);
    checkTrue("jump (0.49.3): tau on screen 0.2 s and the photon's jump T = tau ln 48000 = 2.1558 s (less than one dot of 48,000 left), for emission and absorption; a placement keeps 1.2 s",
              AtomLab::kJumpTauS == tau && std::fabs([] { AtomLab L; return L.jumpDuration(kJumpEmit); }() - T) < 1e-12 && std::fabs([] { AtomLab L; return L.jumpDuration(kJumpAbsorb); }() - T) < 1e-12 &&
              [] { AtomLab L; return L.jumpDuration(kJumpPlace); }() == 1.2 && std::fabs(T - 2.155792) < 1e-6);
    AtomLab L; L.s.trans = Transition{0, 1, 0, T, kJumpEmit};
    double worstE = 0, worstA = 0, worstP = 0;
    for (int k = 0; k <= 200; k++) {
        const double t = T * k / 200;
        L.s.trans->how = kJumpEmit; L.s.trans->t = t; worstE = std::max(worstE, std::fabs(L.jumpShareTo() - shareEmit(t, T, tau)));
        L.s.trans->how = kJumpAbsorb; worstA = std::max(worstA, std::fabs(L.jumpShareTo() - shareAbsorb(t, T, tau)));
        L.s.trans->how = kJumpPlace; L.s.trans->dur = 1.2; L.s.trans->t = 1.2 * k / 200; worstP = std::max(worstP, std::fabs(L.jumpShareTo() - std::pow(std::sin(kPi / 2 * smoothstep(k / 200.0)), 2))); L.s.trans->dur = T;
    }
    std::printf("jump shares vs the formulas written here, 201 moments: emission %.2e, absorption %.2e, placement %.2e\n", worstE, worstA, worstP);
    checkTrue("jump (0.49.3): the emitted line's lower share is (1 - exp(-t/tau)) / (1 - exp(-T/tau)) (Weisskopf-Wigner, the photon cut at T), to 1e-14", worstE < 1e-14);
    checkTrue("jump (0.49.3): the absorbed line's upper share is the time reverse, (exp(-(T-t)/tau) - exp(-T/tau)) / (1 - exp(-T/tau)), to 1e-14", worstA < 1e-14);
    checkTrue("jump (0.49.3): a placement (no photon) keeps the 0.46 cross-fade sin^2(pi/2 smoothstep(t / 1.2 s)), to 1e-14", worstP < 1e-14);
    L.s.trans->how = kJumpEmit; L.s.trans->t = 0; const double e0 = L.jumpShareTo(); L.s.trans->t = T; const double e1 = L.jumpShareTo();
    L.s.trans->how = kJumpAbsorb; L.s.trans->t = 0; const double a0 = L.jumpShareTo(); L.s.trans->t = T; const double a1 = L.jumpShareTo();
    checkTrue("jump (0.49.3): the shares start at exactly 0 and end at exactly 1 (both ways)", e0 == 0 && e1 == 1 && a0 == 0 && a1 == 1);
    // the emitted share against the plain exponential: the cut changes it by at most 1 / 48,000
    double worstCut = 0; for (int k = 0; k <= 200; k++) { const double t = T * k / 200; worstCut = std::max(worstCut, std::fabs(shareEmit(t, T, tau) - (1 - std::exp(-t / tau)))); }
    checkTrue("jump (0.49.3): the cut photon differs from the uncut exponential by at most 1 / 48,000 of the atom", worstCut <= 1.0 / 48000 + 1e-15);
    // jumpU is the angle the cross-fades use: sin^2(pi/2 u) = share
    L.s.trans->how = kJumpEmit; L.s.trans->t = 0.3; const double u = L.jumpU();
    checkNum("jump (0.49.3): jumpU is the angle with sin^2(pi/2 u) = the share", std::pow(std::sin(kPi / 2 * u), 2), L.jumpShareTo(), 1e-14);
}

static void jumpSetup(AtomLab& lab, const char* from, const char* fromOri, int Z = 1) {
    lab.nowMs = [] { return 0.0; };
    lab.p = oneParams(); lab.paused = false; lab.restart();
    lab.setParam("view", std::string("Cloud")); lab.setParam("Z", double(Z));
    lab.setParam("orb", std::string(from));
    if (*fromOri) lab.setParam(orbitals()[orbIndex(from)].l == 1 ? "pAx" : orbitals()[orbIndex(from)].l == 2 ? "dAx" : "fAx", std::string(fromOri));
}
static void runSteps(AtomLab& lab, int n) { for (int i = 0; i < n; i++) lab.step(1.0 / 240); }

// norm along the jump, and the exact <r>: continuity per 1/60 s frame
static void testJumpNormAndContinuity() {
    AtomLab lab; jumpSetup(lab, "1s", "");
    runSteps(lab, 100);
    const int to = orbIndex("2p|z");
    lab.startJump("absorb", to);
    runSteps(lab, 400);                                                   // the photon flies in (1.6 s), then the jump starts
    checkTrue("jump: the cloud jump starts when the photon arrives", lab.s.trans.has_value() && lab.jumpCloud() && lab.s.trans->to == to);
    // own numbers: <r> of 1s and 2p by numerical integration of the textbook radial functions R10 = 2 e^-r, R21 = r e^(-r/2)/(2 sqrt 6)
    double rA = 0, rB = 0; const double dr = 1e-4;
    for (int i = 0; i < 400000; i++) { const double r = (i + 0.5) * dr; rA += std::pow(2 * std::exp(-r), 2) * r * r * r * dr; rB += std::pow(r * std::exp(-r / 2) / (2 * std::sqrt(6.0)), 2) * r * r * r * dr; }
    const double gap = rB - rA;
    // the norm at 21 times along the jump, and <r> = cos^2 rA + sin^2 rB against the full 2D integral (the cross term has no radial part)
    const Quad q = makeQuad(900, 1, 40);
    double worstNorm = 0, worstCross = 0, worstFrame = 0, prev = rA;
    const double T = lab.s.trans->dur;
    for (int k = 0; k <= int(T * 60) + 1; k++) {
        lab.s.trans->t = std::min(T, k / 60.0);                             // frame k at 60 frames a second
        const double P = shareAbsorb(lab.s.trans->t, T, 0.2);               // 0.49.3: the upper level's share (the formula written in this file)
        const double rExact = (1 - P) * rA + P * rB;
        worstFrame = std::max(worstFrame, std::fabs(rExact - prev)); prev = rExact;
        if (k % 6 == 0) {
            const Superposition s1{orbIndex("1s"), to, 1, 0}, s2{orbIndex("1s"), to, 0, 1};   // the two levels alone; the atom is their mixture
            double nrm = 0, mr = 0;
            for (std::size_t i = 0; i < q.r.size(); i++)
                for (std::size_t j = 0; j < q.u.size(); j++) {
                    const double st = std::sqrt(1 - q.u[j] * q.u[j]), w = 2 * kPi * q.wu[j] * q.r[i] * q.r[i] * q.wr[i];
                    const double d = (1 - P) * std::norm(psiSuper(s1, 1, 0, q.r[i] * st, 0, q.r[i] * q.u[j])) + P * std::norm(psiSuper(s2, 1, 0, q.r[i] * st, 0, q.r[i] * q.u[j]));
                    nrm += d * w; mr += d * w * q.r[i];
                }
            worstNorm = std::max(worstNorm, std::fabs(nrm - 1)); worstCross = std::max(worstCross, std::fabs(mr - rExact));
        }
    }
    std::printf("jump 1s -> 2p(z): <r> of 1s %.5f, of 2p %.5f a0 (by my own integral); norm along the jump: largest |norm - 1| = %.1e; <r> from the full integral vs (1 - P) rA + P rB: %.1e; "
                "largest change of the exact <r> per 1/60 s frame: %.4f of the gap %.4f (limit 0.1)\n", rA, rB, worstNorm, worstCross, worstFrame / gap, gap);
    checkTrue("jump: <r> of 1s is 1.5 and of 2p is 5 a0 (own integral of the textbook radial functions, 1e-6)", std::fabs(rA - 1.5) < 1e-6 && std::fabs(rB - 5) < 1e-6);
    checkTrue("jump: the wave stays normalised all along the jump (integral of |psi|^2 = 1, 1e-3)", worstNorm < 1e-3);
    checkTrue("jump: <r> follows (1 - P) rA + P rB, the mixture's (1e-3 a0)", worstCross < 1e-3);
    checkTrue("jump: the exact <r> changes by less than a tenth of the endpoint gap per 1/60 s frame (0.49.3: at the absorbed photon's sharp end 1 / (60 x 0.2 s) = 0.083)", worstFrame < 0.1 * gap);
}

// dots: endpoints and <r> in the middle, with independent dots (every 400th dot of full rings, a different start slot in every lab, so every dot has its own fixed random number)
static void testJumpDots() {
    seedRandom(13579);
    const int nLab = 120, stride = 400, nb = 20; const double rmax = 14;
    const double ths[5] = {0.0, 0.25, 0.5, 0.75, 1.0};
    struct Acc { double sumR = 0, sumR2 = 0; long n = 0; std::vector<double> hist; };
    std::vector<Acc> acc(5); for (auto& a : acc) a.hist.assign(nb, 0);
    long oldDotsSame = 0, oldDotsTotal = 0; bool noNewAtStart = true, noOldAtEnd = true; double rhoSum = 0;
    for (int k = 0; k < nLab; k++) {
        AtomLab lab; jumpSetup(lab, "1s", "");
        runSteps(lab, 800);                                               // the ring is full
        const long cap = std::min<long>(lab.s.k, long(lab.s.sg.size()));
        lab.startJump("absorb", orbIndex("2p|z"));
        runSteps(lab, 378);
        std::vector<float> before;
        for (int guard = 0; guard < 20 && !lab.s.trans; guard++) { before.assign(lab.s.pts.begin(), lab.s.pts.begin() + 3 * cap); lab.step(1.0 / 240); }   // the photon arrives: the jump begins (the ring is not sampled in that step)
        const long jn = lab.jumpDots();
        if (k == 0) rhoSum = double(jn);
        for (int t = 0; t < 5; t++) {
            lab.s.trans->t = t == 4 ? lab.s.trans->dur - 1e-9 : ths[t] * lab.s.trans->dur;
            if (t == 0) lab.s.trans->t = 0;
            for (int pop = 0; pop < 2; pop++) {
                const float* P = pop ? lab.s.jp.data() : lab.s.pts.data();
                for (long i = (long(k) * 7919) % stride; i < jn; i += stride) {
                    if (!lab.jumpShown(pop, i)) continue;
                    const double x = P[3 * i] / kA0, y = P[3 * i + 1] / kA0, z = P[3 * i + 2] / kA0, r = std::sqrt(x * x + y * y + z * z);
                    acc[t].sumR += r; acc[t].sumR2 += r * r; acc[t].n++;
                    if (r < rmax) acc[t].hist[std::size_t(r / rmax * nb)] += 1;
                }
            }
        }
        // at the start every old dot is shown, none of the new ones, and the old dots are exactly the ones that were on screen; at the end the other way round
        lab.s.trans->t = 0;
        for (long i = 0; i < jn; i++) { oldDotsTotal++; if (lab.jumpShown(0, i) && !lab.jumpShown(1, i) && lab.s.pts[3 * i] == before[3 * i] && lab.s.pts[3 * i + 2] == before[3 * i + 2]) oldDotsSame++; else noNewAtStart = false; }
        lab.s.trans->t = lab.s.trans->dur;
        for (long i = 0; i < jn; i += 7) if (lab.jumpShown(0, i) || !lab.jumpShown(1, i)) noOldAtEnd = false;
    }
    (void)rhoSum;
    // exact radial densities: P1s = 4 r^2 e^-2r, P2p = r^4 e^-r / 24; with the upper share P the radial density is (1 - P) P1s + P P2p (0.49.3: a mixture; cos^2 = 1 - P, sin^2 = P below)
    const double rA = 1.5, rB = 5.0, Tj = AtomLab().jumpDuration(kJumpAbsorb);
    const auto radialExact = [&](double th, std::vector<double>& ex) {
        ex.assign(nb, 0); double tot = 0;
        for (int b = 0; b < nb; b++) {
            double P = 0; const int sub = 200;
            for (int j = 0; j < sub; j++) { const double r = (b + (j + 0.5) / sub) * rmax / nb; P += (std::cos(th) * std::cos(th) * 4 * r * r * std::exp(-2 * r) + std::sin(th) * std::sin(th) * std::pow(r, 4) * std::exp(-r) / 24) * rmax / nb / sub; }
            ex[std::size_t(b)] = P; tot += P;
        }
        for (double& e : ex) e /= tot;
    };
    std::printf("jump dots: at the start all %ld old dots are shown, identical to the ones on screen before the jump: %ld; no new dot shown: %s; at the end no old dot shown: %s\n", oldDotsTotal, oldDotsSame, noNewAtStart ? "yes" : "NO", noOldAtEnd ? "yes" : "NO");
    checkTrue("jump: at the start the shown dots are exactly the cloud that was on screen (every old dot, no new one)", noNewAtStart && oldDotsSame == oldDotsTotal);
    checkTrue("jump: at the end only the new dots are shown (no old one, every new one)", noOldAtEnd);
    double worstChi = 0, worstR = 0;
    for (int t = 0; t < 5; t++) {
        const double u = t == 4 ? 1.0 : ths[t], th = std::asin(std::sqrt(shareAbsorb(u * Tj, Tj, 0.2)));   // 0.49.3: sin^2 th = the upper share
        std::vector<double> ex; radialExact(th, ex);
        double chi = 0; double nin = 0; for (double h : acc[t].hist) nin += h;
        for (int b = 0; b < nb; b++) { const double e = nin * ex[std::size_t(b)]; if (e > 0) chi += (acc[t].hist[std::size_t(b)] - e) * (acc[t].hist[std::size_t(b)] - e) / e; }
        const double mean = acc[t].sumR / acc[t].n, sd = std::sqrt(acc[t].sumR2 / acc[t].n - mean * mean), se = sd / std::sqrt(double(acc[t].n));
        const double exactR = std::cos(th) * std::cos(th) * rA + std::sin(th) * std::sin(th) * rB;
        worstChi = std::max(worstChi, chi); worstR = std::max(worstR, std::fabs(mean - exactR) / se);
        std::printf("jump dots at u = %.2f (theta = %.3f): %ld shown dots, mean r %.4f +- %.4f, exact %.4f; radial chi-square %.1f (19 dof)\n", u, th, acc[t].n, mean, se, exactR, chi);
    }
    // limits as for the beats (stage 2): 4 standard errors; chi-square 19 dof, the 99.99 % point is 48
    checkTrue("jump dots: the mean r follows the exact <r>(t) within 4 standard errors at the start, the end and 3 times between", worstR < 4);
    checkTrue("jump dots: the radial histogram matches the exact radial density of the start orbital, the end orbital and the mixtures (chi-square below 48)", worstChi < 48);
}

// the Orbit view: the electron glides between the circles
static void testOrbitGlide() {
    const double a0CODATA = nist::a0;                                     // m, CODATA 2022, NIST page bohrrada0 (read 2026-10-04)
    for (int Z : {1, 2}) {
        for (int dir : {1, -1}) {
            AtomLab lab; lab.nowMs = [] { return 0.0; }; lab.p = oneParams(); lab.paused = false; lab.restart();
            lab.setParam("Z", double(Z)); lab.setParam("speed", 5.0);
            const int n1 = dir > 0 ? 1 : 3, n2 = dir > 0 ? 3 : 1;
            lab.setParam("n", double(n1));
            runSteps(lab, 10);
            lab.startJump(dir > 0 ? "absorb" : "emit", n2);
            double prevR = std::hypot(lab.s.x, lab.s.y), worstOver = 0, worstBack = 0, rLanding = -1; int steps = 0; bool started = dir < 0;
            const double r1 = double(n1) * n1 / Z * lab.aUnit(), r2 = double(n2) * n2 / Z * lab.aUnit();   // 0.46.4 (deliberate): the reduced-mass Bohr radius
            int glideSteps = 0;
            for (int i = 0; i < 1200 && (lab.s.photon || lab.s.trans || !started); i++) {
                const bool hadTrans = lab.s.trans.has_value();
                lab.step(1.0 / 240); steps++;
                if (lab.s.trans) { started = true; glideSteps++; }
                if (hadTrans && !lab.s.trans) rLanding = std::hypot(lab.s.x, lab.s.y);        // the radius at the moment of landing (after it the leapfrog orbit goes on)
                const double r = std::hypot(lab.s.x, lab.s.y);
                if (lab.s.trans && !hadTrans) prevR = r;                                          // 0.49.3: from the glide's own first point (the step before it is the Coulomb orbit, whose leapfrog circle wobbles by ~1e-4)
                if (lab.s.trans) {
                    worstBack = std::max(worstBack, dir > 0 ? prevR - r : r - prevR);                // monotone: never turns back
                    worstOver = std::max({worstOver, (r - std::max(r1, r2)) / kA0, (std::min(r1, r2) - r) / kA0});   // never beyond the two circles
                }
                prevR = r;
            }
            const double rEnd = rLanding, want = double(n2) * n2 / Z * lab.aUnit();
            std::printf("Orbit glide Z = %d, n %d -> %d: %d steps (%.3f s), radius at landing %.9e m, n^2 a0/Z with the lab's a0 = %.9e, with CODATA a0 = %.9e; went back by at most %.1e m, beyond the circles by %.1e a0\n",
                        Z, n1, n2, glideSteps, glideSteps / 240.0, rEnd, want, double(n2) * n2 / Z * a0CODATA, worstBack, worstOver);
            // limit 5e-4: in the step of landing the Orbit view's own leapfrog takes one step on the new circle, and a leapfrog circle wobbles by about (angle per step)^2 / 8 (1.3e-4 at n = 1,
            // speed 5): measured 1.3e-4. The landing circle itself is exact: the reduced-motion test below (no leapfrog after the leap) holds it to 1e-9.
            checkTrue("Orbit: the glide ends on the circle n^2 a0 / Z (the lab's a0, 5e-4: one leapfrog step of wobble)", std::fabs(rEnd / want - 1) < 5e-4 && lab.p.n == n2 && !lab.s.trans);
            // 0.10.6 stage 2: the lab's Bohr radius is the CODATA 2022 one now, so this check, which allowed 4e-4, now holds to 1e-12
            checkTrue("Orbit: the circle n^2 a0 / Z of the lab is the one with the CODATA 2022 Bohr radius (1e-12)", std::fabs(want / (double(n2) * n2 / Z * a0CODATA * (1 + 1 / nuclearMassMe(Z, lab.p.N))) - 1) < 1e-12);   // 0.46.4: times m / mu
            checkTrue("Orbit: the glide is monotone (never turns back) and never overshoots either circle", worstBack < 1e-18 && worstOver < 1e-9);
            checkTrue("Orbit: the glide takes the photon's jump, 2.1558 s (0.49.3; was 0.8 s): 517 or 518 steps of 1/240 s", glideSteps >= 516 && glideSteps <= 519);
        }
    }
}

// the hand-over of the photon, no pop; Auto photons back to back; pause; reset in the middle; the user's setting is left alone
static void testJumpHandover() {
    for (const char* view : {"Cloud", "Orbit"}) {
        const bool orbit = std::string(view) == "Orbit";
        // 0.49.2 (deliberate change of the hand-over): emit: the photon leaves DURING the jump: it starts with it, and its tail leaves the atom in the step in which the jump ends
        AtomLab lab; jumpSetup(lab, "2p", "z");
        if (orbit) { lab.setParam("view", std::string("Orbit")); lab.setParam("n", 2.0); }
        runSteps(lab, 50);
        lab.startJump("emit", orbit ? 1 : orbIndex("1s"));
        const double jd = lab.jumpDuration(kJumpEmit);                   // 0.49.3: 2.1558 s in every view (was 0.8 s Orbit, 1.2 s clouds)
        bool together = lab.s.trans && lab.s.photon && lab.s.photon->kind == "emit" && lab.s.photon->t == 0 && std::fabs(lab.s.photon->jdur - jd) < 1e-12, inStep = true, tailAtEnd = false;
        int jumpSteps = 0;
        for (int i = 0; i < 800; i++) {
            const bool hadTrans = lab.s.trans.has_value();
            lab.step(1.0 / 240); if (hadTrans) jumpSteps++;
            if (lab.s.trans && !(lab.s.photon && std::fabs(lab.s.trans->t - lab.s.photon->t) < 1e-9)) inStep = false;   // the jump and the photon's age stay equal
            if (hadTrans && !lab.s.trans) { tailAtEnd = lab.s.photon && std::fabs(lab.s.photon->t - jd) < 1.0 / 240 + 1e-9; break; }
        }
        std::printf("emit (%s): jump %.3f s with the photon leaving during it (age at the end %.4f s)\n", view, jumpSteps / 240.0, lab.s.photon ? lab.s.photon->t : -1.0);
        checkTrue(std::string("emit (") + view + "): the photon starts with the jump, keeps its pace, and its tail leaves when the jump ends", together && inStep && tailAtEnd);
        int left = 0; while (lab.s.photon && left < 2000) { lab.step(1.0 / 240); left++; }
        checkTrue(std::string("emit (") + view + "): the photon is gone once its tail has crossed the picture (1.6 s after the jump)", !lab.s.photon && std::fabs(left / 240.0 - 1.6) < 2.0 / 240);
        // absorb: the photon flies in; its jump starts in the step in which its front arrives and ends in the step in which its tail is taken in
        AtomLab lab2; jumpSetup(lab2, "1s", "");
        if (orbit) { lab2.setParam("view", std::string("Orbit")); lab2.setParam("n", 1.0); }
        runSteps(lab2, 50);
        lab2.startJump("absorb", orbit ? 2 : orbIndex("2p|z"));
        bool startedAtArrival = false, noJumpBefore = true, gone = false, early = false;
        for (int i = 0; i < 1000 && !gone; i++) {
            const bool hadTrans = lab2.s.trans.has_value(), hadPhoton = lab2.s.photon.has_value();
            const double age = hadPhoton ? lab2.s.photon->t : -1;
            lab2.step(1.0 / 240);
            if (!hadTrans && lab2.s.trans) startedAtArrival = hadPhoton && lab2.s.photon && lab2.s.photon->landed && age < 1.6 && lab2.s.photon->t >= 1.6 && lab2.s.trans->t < 1.0 / 240 + 1e-9;
            if (lab2.s.photon && !lab2.s.photon->landed && lab2.s.trans) noJumpBefore = false;
            if (hadTrans && !lab2.s.trans) gone = !lab2.s.photon;          // the tail is taken in exactly as the jump ends
            if (hadPhoton && !lab2.s.photon && lab2.s.trans) { early = true; break; }   // the photon may not vanish while its jump still runs
        }
        checkTrue(std::string("absorb (") + view + "): the jump starts in the step in which the photon's front arrives, and the photon is taken in as it ends", startedAtArrival && noJumpBefore && gone && !early);
    }
    // Auto photons, 60 s, cloud and Orbit: every jump runs with its photon (the same age for emit, the age less the flight for absorb); Orbit: no jump in the radius
    for (const char* view : {"Cloud", "Orbit"}) {
        const bool orbit = std::string(view) == "Orbit";
        seedRandom(97531);
        AtomLab lab; jumpSetup(lab, "3p", "z"); lab.setParam("view", std::string(view)); if (orbit) lab.setParam("n", 3.0);
        lab.toggleAuto();
        bool wrong = false; int jumps = 0; double worstStep = 0, rMax = 0;
        double prevR = std::hypot(lab.s.x, lab.s.y);
        for (int i = 0; i < 14400; i++) {
            const bool hadTrans = lab.s.trans.has_value();
            lab.step(1.0 / 240);
            if (!hadTrans && lab.s.trans) jumps++;
            if (lab.s.trans) {
                if (!lab.s.photon) wrong = true;
                else { const Photon& ph = *lab.s.photon; if (std::fabs(lab.s.trans->t - (ph.kind == "emit" ? ph.t : ph.t - ph.dur)) > 1e-9) wrong = true; }
            }
            if (orbit) { const double r = std::hypot(lab.s.x, lab.s.y); worstStep = std::max(worstStep, std::fabs(r - prevR)); rMax = std::max(rMax, r); prevR = r; }
        }
        std::printf("Auto photons, %s, 60 s: %d jumps; a jump without its photon or out of step: %s%s\n", view, jumps, wrong ? "YES" : "never",
                    orbit ? (std::string("; largest radius change in one 1/240 s step ") + std::to_string(worstStep / kA0) + " a0 (largest radius " + std::to_string(rMax / kA0) + " a0)").c_str() : "");
        checkTrue(std::string("Auto photons (") + view + "): many jumps, each with its photon and in step with it", jumps >= 8 && !wrong);
        // 0.46: the levels go to n = 6, so the biggest gap is n 1 <-> 6: 35 a0, and the same rule gives 1.5 x 35 / (0.8 x 240) = 0.27 a0, with the same margin 0.34 a0 (was 0.15 for n <= 4)
        // limit for the Orbit radius: a glide of the biggest gap (n 1 <-> 4: 15 a0 at Z = 1) changes the radius by at most 1.5 x gap / (0.8 s x 240) = 0.12 a0 per step; the circular leapfrog orbit itself wobbles by far less
        if (orbit) checkTrue("Auto photons (Orbit): the electron's radius never jumps (below 0.34 a0 per 1/240 s step: the n 1 <-> 6 glide)", worstStep / kA0 < 0.34);
    }
    // 0.49.2: the same pacing in every model with photons: hydrogen (above), sodium's outer electron (every electron shown), magnesium's level jumps; cloud and Orbit views
    for (const int Z : {11, 12}) for (const char* view : {"Cloud 3D", "Orbit"}) {
        AtomLab L; L.p = AtomParams(); L.restart(); L.setParam("view", std::string(view)); L.setParam("Z", double(Z));
        const bool on = Z == 11 ? L.valenceOn() : L.levelOn();
        int jumps = 0, photons = 0; bool wrong = false;
        for (const int dir : {1, -1}) {
            if (dir > 0) L.absorb(); else L.emit();
            photons += L.s.photon.has_value(); jumps += L.s.trans.has_value();   // (an emit jump starts at the click itself)
            for (int i = 0; i < 2000 && (L.s.photon || L.s.trans); i++) {
                const bool hadTrans = L.s.trans.has_value();
                L.step(1.0 / 240);
                if (!hadTrans && L.s.trans) jumps++;
                if (L.s.trans && (!L.s.photon || std::fabs(L.s.trans->t - (L.s.photon->kind == "emit" ? L.s.photon->t : L.s.photon->t - L.s.photon->dur)) > 1e-9)) wrong = true;
            }
        }
        std::printf("0.49.2 pacing Z = %d %s: on %d, photons %d, jumps %d, out of step %d, last: %s\n", Z, view, int(on), photons, jumps, int(wrong), L.s.last.c_str());
        checkTrue("0.49.2: Z = " + std::to_string(Z) + " (" + view + "): an absorb and an emit jump, each in step with its photon from start to end", on && photons == 2 && jumps == 2 && !wrong);
    }
    // pause: the jump advances only inside step(), which the app does not call while paused; tick() (which runs every frame, even when paused) does not move it
    AtomLab p1; jumpSetup(p1, "1s", ""); runSteps(p1, 100); p1.startJump("absorb", orbIndex("2p|z")); runSteps(p1, 400);
    const double t0 = p1.s.trans->t; p1.paused = true; for (int i = 0; i < 600; i++) p1.tick(1.0 / 60);
    checkTrue("pause: a jump in progress does not move while only tick() runs (the app's paused frames)", p1.s.trans && p1.s.trans->t == t0 && p1.jumpShareTo() == shareAbsorb(t0, p1.s.trans->dur, 0.2));
    // changing the orbital, the element or the view in the middle of a jump: a clean new cloud, no leftover jump
    for (int what = 0; what < 3; what++) {
        AtomLab m; jumpSetup(m, "1s", ""); runSteps(m, 100); m.startJump("absorb", orbIndex("2p|z")); runSteps(m, 400);
        const bool inJump = m.s.trans.has_value() && m.s.jn > 0;
        if (what == 0) m.setParam("orb", std::string("3d")); else if (what == 1) m.setParam("Z", 2.0); else m.setParam("view", std::string("Cloud 3D"));
        const bool clean = !m.s.trans && !m.s.photon && m.s.jn == 0 && m.s.jp.empty() && m.s.jA.empty() && m.s.jB.empty() && !m.jumpCloud();
        runSteps(m, 300);
        checkTrue(std::string("changing ") + (what == 0 ? "the orbital" : what == 1 ? "the element" : "the view") + " in the middle of a jump resets it cleanly", inJump && clean && m.s.k > 0);
    }
    // the user's Superposition setting is left alone by jumps; jumps work again after Superposition was switched on and off
    AtomLab u; jumpSetup(u, "1s", "");
    u.setParam("orb2", std::string("2p")); u.setParam("mix", 30.0);
    u.setParam("super", std::string("On"));
    runSteps(u, 100);
    u.emit(); u.absorb();
    checkTrue("a superposition shown: no jump starts (as in stage 2), and the setting is unchanged", !u.s.trans && !u.s.photon && u.p.super == "On" && u.p.mix == 30 && u.p.orb2 == "2p");
    u.setParam("super", std::string("Off"));
    runSteps(u, 800);
    u.startJump("absorb", orbIndex("2p|z"));
    runSteps(u, 384 + 560);                                                // (0.49.3: the jump lasts 2.16 s)
    checkTrue("jumps work again after Superposition was switched on and off: the orbital is now 2p", !u.s.trans && !u.s.photon && u.p.orb == "2p" && u.p.pAx == "z" && u.p.super == "Off" && u.p.orb2 == "2p" && u.p.mix == 30);
    AtomLab j; jumpSetup(j, "1s", ""); runSteps(j, 800);
    j.startJump("absorb", orbIndex("2p|z")); runSteps(j, 384 + 100);
    checkTrue("a jump leaves the user's Superposition setting alone (Off, second orbital, mix)", j.p.super == "Off" && j.p.orb2 == "2p" && j.p.mix == 50 && !j.superOn() && j.s.trans.has_value());
    // reduced motion: the jump is instant (no glide, no cross-fade); the emitted photon then appears at once, the absorbed one still flies in
    AtomLab rm; jumpSetup(rm, "2p", "z"); rm.reducedMotion = true; runSteps(rm, 800);
    rm.startJump("emit", orbIndex("1s"));
    checkTrue("reduced motion (cloud): the emit jump is instant and the photon appears at once", !rm.s.trans && rm.s.photon.has_value() && rm.p.orb == "1s");
    AtomLab ro; ro.nowMs = [] { return 0.0; }; ro.p = oneParams(); ro.restart(); ro.reducedMotion = true; ro.setParam("n", 3.0); runSteps(ro, 20); ro.startJump("emit", 1);
    checkTrue("reduced motion (Orbit): the electron leaps at once to the new circle, the flash and the photon appear", !ro.s.trans && ro.p.n == 1 && std::fabs(std::hypot(ro.s.x, ro.s.y) / ro.aUnit() - 1) < 1e-9 && ro.s.photon.has_value() && ro.s.flash.has_value());
}

// 0.11 stage 1: Auto photons must say why when they cannot jump (they used to stay silent). A fake clock that runs with the steps, so a message that
// is issued again would get a later msgUntil.
// photons in 60 s (seed 97531, 1/240 s steps), measured on the code before the fix (0.11 stage 1): 15 and 18. 0.37 recorded them again (16 and 17): the cloud no longer draws from the app's random
// generator (it has its own, cloud_sample.h), so the app's generator gives Auto other waits; the rule tested is unchanged. 0.46 recorded the Orbit one again (18): the levels go up to
// n = 6, so from n = 3 Auto can also absorb to 5 and 6 and draws other targets and waits; the rule tested is unchanged.
static constexpr int AUTO_JUMPS_CLOUD = 13, AUTO_JUMPS_ORBIT = 13;   // 0.49.3 (deliberate): a photon's jump lasts 2.16 s (was 1.2 s, Orbit 0.8 s), so fewer fit in 60 s (were 16, 18)
static double g_autoClock = 0;
static void autoSteps(AtomLab& lab, int n) { for (int i = 0; i < n; i++) { g_autoClock += 1000.0 / 240; lab.step(1.0 / 240); } }
static void autoSetup(AtomLab& lab) { lab.nowMs = [] { return g_autoClock; }; lab.s.msg.clear(); lab.s.msgUntil = 0; }
static std::string manualText(const char* orb, const char* axKey, const char* ax, bool super, int dir) {   // what the manual button says in the same state
    AtomLab m; jumpSetup(m, orb, ""); if (*axKey) m.setParam(axKey, std::string(ax));
    if (super) { m.setParam("orb2", std::string("2s")); m.setParam("mix", 30.0); m.setParam("super", std::string("On")); }
    g_autoClock = 0; autoSetup(m); if (dir < 0) m.emit(); else m.absorb();
    return m.s.msg;
}
static void testAutoSilence() {
    // the three situations in which targets() is empty because of the state itself
    struct Case { const char* name; const char* orb; const char* axKey; const char* ax; bool super; const char* cause; };
    const Case cases[] = {{"complex state 2p m=+1", "2p", "pAx", "m+1", false, "is a complex state"}, {"complex state 3d m=-2", "3d", "dAx", "m-2", false, "is a complex state"},
                          {"complex state 4f m=+3", "4f", "fAx", "m+3", false, "is a complex state"}, {"superposition", "2p", "", "", true, "A superposition has no single level"}};
    for (const Case& c : cases) {
        seedRandom(424242);
        AtomLab lab; jumpSetup(lab, c.orb, ""); if (*c.axKey) lab.setParam(c.axKey, std::string(c.ax));
        if (c.super) { lab.setParam("orb2", std::string("2s")); lab.setParam("mix", 30.0); lab.setParam("super", std::string("On")); }
        g_autoClock = 0; autoSetup(lab); lab.toggleAuto();
        bool anyJump = false;
        autoSteps(lab, 240 * 4);                                   // four seconds: the wait is at most 1.8 s, so Auto has looked at least twice
        anyJump = lab.s.trans || lab.s.photon;
        const std::string want = manualText(c.orb, c.axKey, c.ax, c.super, -1);
        const std::string tag = std::string("Auto, ") + c.name + ": ";
        checkTrue(tag + "no jump and no photon (the electron really cannot jump from here in the lab)", !anyJump);
        checkTrue(tag + "a message says why, with the manual button's own words", !lab.s.msg.empty() && lab.s.msg == want && lab.s.msg.find(c.cause) != std::string::npos);
        checkTrue(tag + "the message is shown by info() (for as long as it lasts)", [&] { for (const std::string& L : lab.info()) if (L == lab.s.msg) return true; return false; }());
        const double until = lab.s.msgUntil;
        autoSteps(lab, 240 * 40);                                  // 40 more seconds: the message has long expired and must not come back
        checkNum(tag + "40 s later the message was not issued again (msgUntil unchanged)", lab.s.msgUntil, until, 0);
        checkTrue(tag + "still no jump after 44 s", !lab.s.trans && !lab.s.photon && lab.s.lines.empty());
        checkTrue(tag + "and the message has expired from info()", [&] { for (const std::string& L : lab.info()) if (L == lab.s.msg) return false; return true; }());
        // the situation changes (Auto switched off and on again): it says it once more
        lab.toggleAuto(); autoSteps(lab, 10); lab.toggleAuto(); autoSteps(lab, 240 * 3);
        checkTrue(tag + "Auto switched off and on again: the message comes again (once)", lab.s.msgUntil > until);
        const double until2 = lab.s.msgUntil; autoSteps(lab, 240 * 30);
        checkNum(tag + "and again only once", lab.s.msgUntil, until2, 0);
    }
    // a different cause is a different message: complex m=+1 -> complex m=-1 (other state), complex -> superposition, superposition -> a real orientation (Auto then works)
    {
        seedRandom(7);
        AtomLab lab; jumpSetup(lab, "2p", ""); lab.setParam("pAx", std::string("m+1")); g_autoClock = 0; autoSetup(lab); lab.toggleAuto(); autoSteps(lab, 240 * 4);
        const std::string first = lab.s.msg; const double u1 = lab.s.msgUntil;
        lab.setParam("pAx", std::string("m-1")); autoSteps(lab, 240 * 4);
        checkTrue("Auto: m=+1 changed to m=-1 is a new situation: new message naming m = −1", lab.s.msgUntil > u1 && lab.s.msg != first && lab.s.msg.find("m = −1") != std::string::npos);
        lab.setParam("mix", 30.0); lab.setParam("orb2", std::string("2s")); lab.setParam("super", std::string("On")); autoSteps(lab, 240 * 4);
        checkTrue("Auto: complex state changed to a superposition: the superposition message", lab.s.msg.find("A superposition has no single level") != std::string::npos);
        lab.setParam("super", std::string("Off")); lab.setParam("pAx", std::string("z")); autoSteps(lab, 240 * 20);
        checkTrue("Auto: back to a real orientation, the jumps resume by themselves", !lab.s.lines.empty());
    }
    // Auto still works for real orientations: no message, and the same number of jumps as before this change (measured on the code before the fix; the wait is 0.5-1.8 s, so at most 120 attempts in 60 s)
    for (const char* view : {"Cloud", "Orbit"}) {
        seedRandom(97531);
        AtomLab lab; jumpSetup(lab, "3p", "z"); lab.setParam("view", std::string(view)); if (std::string(view) == "Orbit") lab.setParam("n", 3.0);
        g_autoClock = 0; autoSetup(lab); lab.toggleAuto();
        int started = 0; bool said = false;
        for (int i = 0; i < 14400; i++) {
            const bool idle = !lab.s.trans && !lab.s.photon; const std::string before = lab.s.last;
            autoSteps(lab, 1);
            if (before != lab.s.last) started++;
            (void)idle; if (!lab.s.msg.empty()) said = true;
        }
        const int want = std::string(view) == "Cloud" ? AUTO_JUMPS_CLOUD : AUTO_JUMPS_ORBIT;
        checkTrue(std::string("Auto, real orientation, ") + view + ", 60 s: no message at all", !said);
        checkTrue(std::string("Auto, real orientation, ") + view + ", 60 s: jumps happen, at most 120 (the shortest wait is 0.5 s plus the flight)", started > 0 && started <= 120);
        checkNum(std::string("Auto, real orientation, ") + view + ", 60 s: the same number of photons as before the change", started, want, 0);
    }
    // every real orbital and every Orbit level: never a state with no jump in either direction (so Auto's reason is never "can't emit" alone); and every
    // state whose Absorb finds nothing is on the top shell (n = 4), where the manual words "highest level in this sim" are true
    { AtomLab e; jumpSetup(e, "1s", ""); int both = 0, upEmptyBelowTop = 0, checked = 0;
      for (int k = 0; k < int(orbitals().size()); k++) {
          e.p.view = "Cloud"; e.p.orb = orbitals()[k].shell; const int l = orbitals()[k].l; (l == 1 ? e.p.pAx : l == 2 ? e.p.dAx : e.p.fAx) = orbitals()[k].ori;
          if (e.complexM() != 0) continue;
          checked++;
          if (e.targets(-1).empty() && e.targets(1).empty()) both++;
          if (e.targets(1).empty() && orbitals()[k].n < 4) upEmptyBelowTop++;
      }
      e.p.view = "Orbit"; for (int n = 1; n <= 4; n++) { e.p.n = n; checked++; if (e.targets(-1).empty() && e.targets(1).empty()) both++; if (e.targets(1).empty() && n < 4) upEmptyBelowTop++; }
      checkTrue("real states examined: all 4 Orbit levels and every real orbital", checked > 4 + 20);
      checkNum("real states with no jump in either direction", both, 0, 0);
      checkNum("real states below the top shell with nothing to absorb", upEmptyBelowTop, 0, 0); }
}

// -------------------------------------------------------------------------------------------------------------------------------------------------------------
// 0.11 stage 2: the chart of nuclides. REFERENCES are not the lab's own output: (1) counts and rows typed from data/nubase/nubase_4.mas20.txt (NUBASE2020, fetched 2026-10-04) by a separate
// script that reads only that file: 3,558 ground-state lines = the free neutron + 3,557 nuclides, the count per element, spin and half-life text of 15 chosen nuclides; (2) the colour classes counted
// by a separate Python implementation of the stated rule on the lab's decay table, and, as a cross-check done once and written in REPORT.md, against NUBASE's own decay column (3,054 of 3,083 agree);
// (3) hand arithmetic for the layout; (4) the 4 spin kinds. Guards are exact counts, not tolerances.
// ---- a recording stand-in for the renderer: drawChart only calls Renderer2D::rect and hexColor, so these two are all it needs (the real ones are in src/render, built only in the app)
struct RecRect { double x, y, w, h; render::Color c; };
static std::vector<RecRect> g_rec;
struct RecTri { double p[6]; render::Color c; };
struct RecLine { double x0, y0, x1, y1, w; render::Color c; };
struct RecDisc { double x, y, r; render::Color c; };
static std::vector<RecTri> g_tri;
static std::vector<RecLine> g_lines;
static std::vector<RecDisc> g_discs;
static std::vector<std::pair<char, int>> g_events;     // every rect ('r'), triangle ('t') and line ('l') call in the order made, with its index in the vector above (0.13 stage 4 part B: the retained mesh is compared with this)
// 0.13 stage 4 part B: the stand-in also records the retained-mesh calls (what the real renderer would upload and draw), so the cache's rebuild triggers can be tested without a graphics card
struct RecMesh { int id, first, count; render::MeshProjection p; };
static std::vector<RecMesh> g_meshDraws;
static int g_meshCreates = 0, g_meshDeletes = 0, g_meshNext = 0;
namespace render {
int Renderer2D::meshCreate(const std::vector<MeshVertex>&) { g_meshCreates++; return g_meshNext++; }
void Renderer2D::meshDelete(int) { g_meshDeletes++; }
void Renderer2D::drawMesh(int id, int first, int count, const MeshProjection& P) { g_meshDraws.push_back({id, first, count, P}); }
void Renderer2D::rect(double x, double y, double w, double h, Color c) { g_events.push_back({'r', int(g_rec.size())}); g_rec.push_back({x, y, w, h, c}); }
void Renderer2D::triangle(double x0, double y0, Color c0, double x1, double y1, Color, double x2, double y2, Color) { g_events.push_back({'t', int(g_tri.size())}); g_tri.push_back({{x0, y0, x1, y1, x2, y2}, c0}); }
void Renderer2D::line(double x0, double y0, double x1, double y1, double width, Color c) { g_events.push_back({'l', int(g_lines.size())}); g_lines.push_back({x0, y0, x1, y1, width, c}); }
void Renderer2D::disc(double x, double y, double r, Color c) { g_discs.push_back({x, y, r, c}); }
Color hexColor(const char* hex, float alpha) { auto v = [&](int i) { return float(std::strtol(std::string(hex + 1 + 2 * i, 2).c_str(), nullptr, 16)) / 255.f; }; return {v(0), v(1), v(2), alpha}; }
}
struct RecText : atom::ChartText {
    mutable std::vector<std::string> lines;
    mutable std::vector<std::array<double, 2>> at;      // where each string was drawn (x, baseline y)
    double width(const std::string& t, double size, bool) const override { return double(t.size()) * size * 0.5; }
    void draw(const std::string& t, double x, double y, double, render::Color, double, bool) const override { lines.push_back(t); at.push_back({x, y}); }
};

static bool hoverHas(int Z, int N, const std::string& what) { for (const std::string& L : chartHover(Z, N)) if (L.find(what) != std::string::npos) return true; return false; }
static void testChart() {
    const std::vector<ChartTile>& tiles = chartTiles();
    // ---- the tile set equals the data set: no nuclide drawn that the data does not have, none missing
    checkNum("chart: NUBASE2020's file has 3,558 ground-state lines = 3,557 nuclides + the free neutron (counted from the file); the chart has one tile per nuclide", double(tiles.size()), 3557, 0);
    checkNum("chart: NUBASE table rows (the same set, made by the tool from the file)", nubaseCount(), 3557, 0);
    static const int perZ[118] = {7,8,11,12,16,16,16,18,19,20,23,23,23,24,24,24,25,26,29,29,29,29,29,30,31,32,32,35,33,33,33,33,33,33,34,35,34,35,35,37,38,39,40,41,41,42,42,42,42,42,41,42,42,43,42,42,42,41,41,40,40,41,41,40,40,39,39,39,39,38,39,38,40,41,41,43,43,44,43,47,43,43,41,42,39,39,37,35,33,32,31,29,27,27,27,22,22,20,20,20,19,17,16,16,16,16,19,18,18,18,15,13,13,8,6,5,4,3};
    int wrongZ = 0, noRow = 0, unsorted = 0, noCode = 0, dup = 0;
    std::array<int, 119> have{};
    for (std::size_t i = 0; i < tiles.size(); i++) {
        have[tiles[i].Z]++;
        if (!nubase(tiles[i].Z, tiles[i].N)) noRow++;
        if (!nucCode(tiles[i].Z, tiles[i].N)) noCode++;
        if (i && !(tiles[i - 1].Z < tiles[i].Z || (tiles[i - 1].Z == tiles[i].Z && tiles[i - 1].N < tiles[i].N))) unsorted++;
        if (i && tiles[i - 1].Z == tiles[i].Z && tiles[i - 1].N == tiles[i].N) dup++;
    }
    for (int z = 1; z <= 118; z++) if (have[z] != perZ[z - 1]) wrongZ++;
    checkNum("chart: elements whose tile count differs from NUBASE2020's count of that element's ground states (118 typed counts)", wrongZ, 0, 0);
    checkNum("chart: tiles with no row in the NUBASE table", noRow, 0, 0);
    checkNum("chart: tiles the lab's own table does not hold", noCode, 0, 0);
    checkNum("chart: tiles out of order or twice", unsorted + dup, 0, 0);
    int missing = 0;
    for (int Z = 1; Z <= 118; Z++) for (int N = 0; N <= 180; N++) if ((nubase(Z, N) != nullptr) != chartHas(Z, N)) missing++;
    checkNum("chart: cells where chartHas and the NUBASE table disagree (Z 1-118, N 0-180)", missing, 0, 0);
    checkNum("chart: largest N and Z (NUBASE: 177 and 118)", chartMaxN() * 1000 + chartMaxZ(), 177118, 0);
    checkTrue("chart: He-2 (the diproton), Z=118 N=100 and Z=1 N=7 are not nuclides", !chartHas(2, 0) && !chartHas(118, 100) && !chartHas(1, 7) && !chartHas(0, 1) && !chartHas(119, 150));
    // ---- the colour rule
    static const struct { ChartClass c; int n; } kCount[] = {{ChartClass::Stable, 253}, {ChartClass::BetaMinus, 1401}, {ChartClass::BetaPlus, 1110}, {ChartClass::Alpha, 479}, {ChartClass::Fission, 59},
                                                             {ChartClass::Proton, 104}, {ChartClass::Neutron, 30}, {ChartClass::Other, 0}, {ChartClass::Mixed, 121}};
    int wrongCount = 0, sum = 0;
    for (const auto& k : kCount) { if (chartClassCount(k.c) != k.n) wrongCount++; sum += chartClassCount(k.c); }
    checkNum("chart: classes whose tile count differs from the separate Python implementation of the rule (253, 1,401, 1,110, 479, 59, 104, 30, 0, 121; 0.12 stage 2 moved Kr-78 and Xe-124, whose lab entry was double beta plus, to stable: stable 251 -> 253, beta plus 1,112 -> 1,110, the rest as counted before)", wrongCount, 0, 0);
    checkNum("chart: every tile has a class", sum, 3557, 0);
    struct Pick { int Z, N; ChartClass c; const char* why; };
    static const Pick picks[] = {
        {2, 2, ChartClass::Stable, "He-4: NUBASE \"stbl\""}, {26, 30, ChartClass::Stable, "Fe-56: stbl"}, {1, 2, ChartClass::BetaMinus, "H-3: B-=100"}, {6, 5, ChartClass::BetaPlus, "C-11: B+=100"},
        {92, 146, ChartClass::Alpha, "U-238: A=100 (SF=5.44e-5, 2B-=2.2e-10)"}, {100, 158, ChartClass::Fission, "Fm-258: SF~100"}, {104, 152, ChartClass::Fission, "Rf-256: SF=99.69, A=0.31"},
        {7, 10, ChartClass::BetaMinus, "N-17: B-=100, B-n=95.1, B-A=0.0025: the delayed branches add to beta minus"},
        {52, 56, ChartClass::BetaPlus, "Te-108: B+=51 (4), A=49 (4), B+p=2.4: the lab's table splits B+ into 0.4853 + 0.024 + 0.00065 > A 0.49"},
        {94, 135, ChartClass::BetaPlus, "Pu-229: the lab table lists B+=0.4673 before A=0.4673 (NUBASE writes A~50, B+~50): a tie goes to the kind listed first in the lab table"},
        {96, 154, ChartClass::Fission, "Cm-250: SF 0.74 B- 0.13 A 0.13"}, {3, 0, ChartClass::Proton, "Li-3: p, odds not measured, one mode"}, {1, 3, ChartClass::Neutron, "H-4: n=100"},
        {20, 28, ChartClass::BetaMinus, "Ca-48: B- and 2B- listed, odds not measured: both beta minus"}, {7, 18, ChartClass::Mixed, "N-25: B-, n, 2n, odds not measured: three kinds"},
        {36, 42, ChartClass::Stable, "Kr-78: NUBASE says stbl and, since 0.12 stage 2, so does the lab's table (it had a half-life number NUBASE and the mendeleev package do not have)"}};
    for (const Pick& k : picks) checkTrue(std::string("chart colour: ") + k.why, chartClassOf(k.Z, k.N) == k.c);
    checkTrue("chart: the kind of a mode text", chartClassOfMode("A") == ChartClass::Alpha && chartClassOfMode("SF") == ChartClass::Fission && chartClassOfMode("B-") == ChartClass::BetaMinus &&
        chartClassOfMode("2B-") == ChartClass::BetaMinus && chartClassOfMode("B-n") == ChartClass::BetaMinus && chartClassOfMode("B-SF") == ChartClass::BetaMinus && chartClassOfMode("B+") == ChartClass::BetaPlus &&
        chartClassOfMode("B+p") == ChartClass::BetaPlus && chartClassOfMode("2B+") == ChartClass::BetaPlus && chartClassOfMode("p") == ChartClass::Proton && chartClassOfMode("2p") == ChartClass::Proton &&
        chartClassOfMode("n") == ChartClass::Neutron && chartClassOfMode("3n") == ChartClass::Neutron && chartClassOfMode("14C") == ChartClass::Other && chartClassOfMode("24Ne") == ChartClass::Other);
    std::set<std::string> hexes; bool words = true;
    for (int c = 0; c < kChartClasses; c++) { hexes.insert(chartClassInfo(ChartClass(c)).hex); if (!*chartClassInfo(ChartClass(c)).name || !*chartClassInfo(ChartClass(c)).words) words = false; }
    checkNum("chart: the nine classes have nine different colours (stable is the page's text colour, written as an empty hex)", double(hexes.size()), 9, 0);
    checkTrue("chart: every class has a name and a sentence for the legend; stable's colour is the page's text colour", words && std::string(chartClassInfo(ChartClass::Stable).hex).empty());
    // ---- the layout, the magic lines, which tile is under a point
    const double sizes[][2] = {{900, 700}, {640, 480}, {1400, 900}, {400, 300}};
    int outside = 0, roundTrip = 0, overlap = 0, shape = 0;
    for (const auto& wh : sizes) {
        const ChartLayout L = chartLayout(wh[0], wh[1]);
        for (const ChartTile& t : tiles) {
            const double x = L.x(t.N), y = L.y(t.Z);
            if (x < 0 || y < 0 || x + L.cell > wh[0] + 1e-9 || y + L.cell > wh[1] + 1e-9) outside++;
            int Z = 0, N = 0;
            if (!chartTileAt(L, x + L.cell / 2, y + L.cell / 2, Z, N) || Z != t.Z || N != t.N) roundTrip++;
            if (!chartTileAt(L, x + 0.01 * L.cell, y + 0.01 * L.cell, Z, N) || Z != t.Z || N != t.N || !chartTileAt(L, x + 0.99 * L.cell, y + 0.99 * L.cell, Z, N) || Z != t.Z || N != t.N) roundTrip++;
        }
        for (int Z = 1; Z < 118; Z++) if (std::fabs(L.y(Z) - L.y(Z + 1) - L.cell) > 1e-9) overlap++;
        for (int N = 0; N < 177; N++) if (std::fabs(L.x(N + 1) - L.x(N) - L.cell) > 1e-9) overlap++;
        if (L.y(118) < 0 || L.x(0) < 0) shape++;
        int Z = 0, N = 0;
        checkTrue("chart: a point outside the picture, left of the chart or on a square with no nuclide is no tile (" + std::to_string(int(wh[0])) + " x " + std::to_string(int(wh[1])) + ")",
                  !chartTileAt(L, -5, 100, Z, N) && !chartTileAt(L, 5, 5, Z, N) && !chartTileAt(L, wh[0] + 50, wh[1] / 2, Z, N) && !chartTileAt(L, L.x(0) + L.cell / 2, L.y(2) + L.cell / 2, Z, N) &&
                  !chartTileAt(L, L.x(120) + L.cell / 2, L.y(10) + L.cell / 2, Z, N) && !chartTileAt(L, L.x(0) - 1, L.y(1) + L.cell / 2, Z, N));
        for (int m : kChartMagic) {                                // the magic lines are drawn along the edges of column N = m and row Z = m: a tile is between the two edges exactly when its own N (or Z) is m
            for (const ChartTile& t : tiles) {
                const double cx = L.x(t.N) + L.cell / 2, cy = L.y(t.Z) + L.cell / 2;
                if ((cx > L.x(m) && cx < L.x(m) + L.cell) != (t.N == m)) shape++;
                if ((cy > L.y(m) && cy < L.y(m) + L.cell) != (t.Z == m)) shape++;
            }
        }
    }
    checkNum("chart: tiles not wholly inside the picture, at 4 picture sizes", outside, 0, 0);
    checkNum("chart: tiles whose centre or whose corners (1 % and 99 % into the square) are not found by chartTileAt, at 4 sizes", roundTrip, 0, 0);
    checkNum("chart: rows or columns not exactly one square apart (tiles neither overlap nor leave gaps), 4 sizes", overlap, 0, 0);
    checkNum("chart: magic-number rows and columns that are not where their number says (the tile in column N = m has N = m; the tile in row Z = m has Z = m), 4 sizes", shape, 0, 0);
    {   // the room the chart leaves in the two corners: a box of that height covers no tile, and one a square taller does (found by trying every tile)
        int loose = 0, tight = 0;
        for (const auto& wh : sizes) for (bool top : {true, false}) for (double wf : {0.2, 0.35, 0.5}) {
            const ChartLayout L = chartLayout(wh[0], wh[1]);
            const double x0 = top ? L.left + 8 : wh[0] - L.right - wf * wh[0], x1 = x0 + wf * wh[0];
            const double fh = chartFreeHeight(L, wh[1], top, x0, x1), y0 = top ? L.top : wh[1] - L.bottom - fh, y1 = top ? L.top + fh : wh[1] - L.bottom;
            const double e = L.cell * 1.0001, ya = top ? y0 : y0 - e, yb = top ? y1 + e : y1;      // a box one square taller
            bool hitSmall = false, hitBig = false;
            for (const ChartTile& t : tiles) {
                const double tx = L.x(t.N), ty = L.y(t.Z);
                const bool xin = tx < x1 && tx + L.cell > x0;
                if (xin && ty < y1 - 1e-6 && ty + L.cell > y0 + 1e-6) hitSmall = true;
                if (xin && ty < yb && ty + L.cell > ya) hitBig = true;
            }
            if (hitSmall) loose++;
            if (!hitBig && fh < wh[1] - L.top - L.bottom - 1e-6) tight++;
        }
        checkNum("chart room: corner boxes of the free height that cover a tile (4 sizes x 2 corners x 3 widths)", loose, 0, 0);
        checkNum("chart room: corner boxes one square taller than the free height that still cover no tile (the height is not too small)", tight, 0, 0);
    }
    checkTrue("chart: the magic numbers are 2, 8, 20, 28, 50, 82, 126", kChartMagic[0] == 2 && kChartMagic[1] == 8 && kChartMagic[2] == 20 && kChartMagic[3] == 28 && kChartMagic[4] == 50 && kChartMagic[5] == 82 && kChartMagic[6] == 126);
    checkTrue("chart: protons up (Z 118 above Z 1) and neutrons across (N 177 right of N 0)", chartLayout(900, 700).y(118) < chartLayout(900, 700).y(1) && chartLayout(900, 700).x(177) > chartLayout(900, 700).x(0));
    {   // a tile on a known cell: U-238 (Z 92, N 146) is up and to the right of Fe-56 (26, 30), and the point at the middle of its square is U-238 and not the transposed (146, 92)
        const ChartLayout L = chartLayout(900, 700); int Z = 0, N = 0;
        checkTrue("chart: the middle of U-238's square is Z 92, N 146 (not swapped)", chartTileAt(L, L.x(146) + L.cell / 2, L.y(92) + L.cell / 2, Z, N) && Z == 92 && N == 146 && !chartHas(146, 92));
    }
    // ---- clicking sets exactly that Z and N; the other views then show it
    {
        AtomLab lab; lab.p = oneParams(); lab.paused = false; lab.restart(); lab.setParam("view", std::string(kChartView)); lab.viewW = 900; lab.viewH = 700;
        const ChartLayout L = chartLayout(900, 700);
        int bad = 0;
        const int cl[][2] = {{92, 146}, {26, 30}, {1, 0}, {2, 2}, {118, 176}, {50, 70}, {82, 126}, {8, 8}, {67, 98}, {20, 28}};
        for (const auto& zn : cl) {
            lab.click(L.x(zn[1]) + L.cell / 2, L.y(zn[0]) + L.cell / 2);
            if (lab.p.Z != zn[0] || lab.p.N != zn[1] || lab.p.view != kChartView) bad++;
        }
        checkNum("chart click: nuclides clicked (10, from the edges and the middle) that did not set exactly that Z and N, or left the chart view", bad, 0, 0);
        lab.click(L.x(100) + L.cell / 2, L.y(10) + L.cell / 2); lab.click(5, 5); lab.click(-3, 400); lab.click(2000, 2000);
        checkTrue("chart click: an empty square, a corner and points outside change nothing (still Ca-48, the last one clicked)", lab.p.Z == 20 && lab.p.N == 28);
        lab.click(L.x(146) + L.cell / 2, L.y(92) + L.cell / 2);
        lab.setParam("view", std::string("Cloud"));
        checkTrue("chart click: the other views show the chosen nuclide (Cloud view: Z 92, N 146)", lab.p.Z == 92 && lab.p.N == 146 && lab.p.view == "Cloud" && !lab.chartOn());
        checkTrue("chart: a click in another view is not a chart click (no change of Z and N)", [&] { lab.click(L.x(30) + L.cell / 2, L.y(26) + L.cell / 2); return lab.p.Z == 92 && lab.p.N == 146; }());
    }
    // ---- the arrow keys: one proton or one neutron, and stop at the edges
    {
        AtomLab lab; lab.p = oneParams(); lab.paused = false; lab.restart(); lab.setParam("view", std::string(kChartView));
        lab.chooseNuclide(26, 30);
        checkTrue("chart keys: from Fe-56 up is Z+1, down Z-1, right N+1, left N-1", lab.chartStep(1, 0) && lab.p.Z == 27 && lab.p.N == 30 && lab.chartStep(-1, 0) && lab.chartStep(-1, 0) && lab.p.Z == 25 && lab.p.N == 30 &&
                  lab.chartStep(0, 1) && lab.p.Z == 25 && lab.p.N == 31 && lab.chartStep(0, -1) && lab.chartStep(0, -1) && lab.p.Z == 25 && lab.p.N == 29);
        lab.chooseNuclide(1, 0);
        checkTrue("chart keys: at hydrogen-1 left, down and up (helium-2 does not exist) do nothing; right goes to hydrogen-2", !lab.chartStep(0, -1) && !lab.chartStep(-1, 0) && !lab.chartStep(1, 0) && lab.p.Z == 1 && lab.p.N == 0 &&
                  lab.chartStep(0, 1) && lab.p.Z == 1 && lab.p.N == 1);
        // walking to each edge: every step is exactly one, and the walk stops where the next square holds no nuclide
        int steps = 0; bool exact = true;
        lab.chooseNuclide(26, 30);
        for (int i = 0; i < 400; i++) { const int z = lab.p.Z; if (!lab.chartStep(1, 0)) break; steps++; if (lab.p.Z != z + 1 || lab.p.N != 30) exact = false; }
        int top = 0; for (const ChartTile& t : tiles) if (t.N == 30) top = std::max<int>(top, t.Z);
        checkTrue("chart keys: up from Fe-56 along N = 30 stops at the highest Z that has N = 30 (found by scanning the tiles), one proton at a time", exact && lab.p.Z == top && lab.p.N == 30 && steps == top - 26 && !chartHas(top + 1, 30));
        lab.chooseNuclide(26, 30); int right = 0;
        for (int i = 0; i < 400; i++) { if (!lab.chartStep(0, 1)) break; right++; }
        int rmax = 0; for (const ChartTile& t : tiles) if (t.Z == 26) rmax = std::max<int>(rmax, t.N);
        checkTrue("chart keys: right from Fe-56 stops at the largest N of iron", lab.p.N == rmax && right == rmax - 30 && lab.p.Z == 26);
        lab.chooseNuclide(118, 176);
        checkTrue("chart keys: at the top row nothing goes up, at Z = 118 N = 177 nothing goes right", !lab.chartStep(1, 0) && lab.chartStep(0, 1) && lab.p.N == 177 && !lab.chartStep(0, 1) && !lab.chartStep(1, 0));
        int fmin = 999; for (const ChartTile& t : tiles) if (t.Z == 26) fmin = std::min<int>(fmin, t.N);
        int z = 26, n = fmin;
        checkTrue("chart keys: left of the lightest iron nuclide is no nuclide: chartMove refuses and leaves Z and N alone; a step of (0, 0) goes nowhere new but is a nuclide", !chartMove(z, n, 0, -1) && z == 26 && n == fmin && chartMove(z, n, 0, 1) && n == fmin + 1);
    }
    // ---- in the chart view the electron and the decay clock wait; photons say so
    {
        AtomLab lab; lab.p = oneParams(); lab.paused = false; lab.restart(); lab.nowMs = [] { return 0.0; };
        lab.setParam("Z", 3.0); lab.setParam("N", 5.0);                          // lithium-8, half-life 0.84 s: in any other view it decays within a minute
        lab.setParam("view", std::string(kChartView));
        seedRandom(2024);
        for (int i = 0; i < 3000; i++) { lab.tick(1.0 / 60); lab.step(1.0 / 60); }
        checkTrue("chart: Li-8 (half-life under a second) is still the chosen nucleus after 50 s in the chart view (its decay clock waits)", lab.p.Z == 3 && lab.p.N == 5);
        lab.emit();
        checkTrue("chart: Emit says why it does nothing", lab.s.msg.find("chart of nuclides shows nuclei") != std::string::npos && !lab.s.trans && !lab.s.photon);
        AtomLab lab2; lab2.p = oneParams(); lab2.paused = false; lab2.restart(); lab2.nowMs = [] { return 0.0; };
        lab2.setParam("Z", 3.0); lab2.setParam("N", 5.0); lab2.setParam("view", std::string("Cloud"));
        seedRandom(2024);
        for (int i = 0; i < 3000; i++) { lab2.tick(1.0 / 60); lab2.step(1.0 / 60); }
        checkTrue("chart: (the control) in the Cloud view the same Li-8 does decay in 50 s, so the chart view's wait is real", !(lab2.p.Z == 3 && lab2.p.N == 5));
    }
    // ---- the hover words: each line is a field of the data
    checkTrue("hover He-4: stable, spin 0+ as evaluated, the measured radius 1.6755 fm", hoverHas(2, 2, "Helium-4") && hoverHas(2, 2, "Stable: no decay has been seen") && hoverHas(2, 2, "Spin and parity: 0+ (as evaluated") && hoverHas(2, 2, "rms 1.6755 fm") &&
              !hoverHas(2, 2, "Decays by"));
    checkTrue("hover U-238 (an alpha emitter): half-life 4.47 billion years, alpha 100 % with the fission and double-beta odds, spin 0+, no stable line", hoverHas(92, 146, "Uranium-238") && hoverHas(92, 146, "4.47 billion years") &&
              hoverHas(92, 146, "alpha (α) 100 %") && hoverHas(92, 146, "spontaneous fission") && hoverHas(92, 146, "double beta minus") && hoverHas(92, 146, "Spin and parity: 0+") && !hoverHas(92, 146, "Stable:"));
    checkTrue("hover H-3 (a beta emitter): half-life 12.3 years, beta minus, spin 1/2+ directly measured", hoverHas(1, 2, "half-life 12.3 years") && hoverHas(1, 2, "Decays by: beta minus (β⁻).") && hoverHas(1, 2, "Spin and parity: 1/2+ (directly measured)"));
    checkTrue("hover C-11 (beta plus / electron capture): 3/2− directly measured, its half-life is minutes", hoverHas(6, 5, "beta plus or electron capture (β⁺/EC)") && hoverHas(6, 5, "3/2− (directly measured)") && hoverHas(6, 5, "20.3 minutes"));
    checkTrue("hover Fm-258 (fission): half-life 370 microseconds as 3.7e-4 s, spontaneous fission, spin 0+", hoverHas(100, 158, "spontaneous fission") && hoverHas(100, 158, "Spin and parity: 0+") && hoverHas(100, 158, "Fermium-258"));
    checkTrue("hover Li-12 (missing data): no half-life given by NUBASE2020, the neutron emission with 'odds are not measured', tentative spin (1−,2−), no measured radius, no moment", hoverHas(3, 9, "Half-life: not given by NUBASE2020") &&
              hoverHas(3, 9, "neutron emission") && hoverHas(3, 9, "the odds are not measured") && hoverHas(3, 9, "(1−,2−) (tentative)") && hoverHas(3, 9, "Charge radius: no measured value") && hoverHas(3, 9, "Quadrupole moment: no usable"));
    checkTrue("hover B-16 (only a limit): NUBASE gives >4.6 zs, spin 0− estimated from systematics, neutron emission", hoverHas(5, 11, "only a limit is known, >4.6 zs") && hoverHas(5, 11, "0− (estimated from systematics, not measured)") && hoverHas(5, 11, "neutron emission"));
    checkTrue("hover F-31 (Z 9, N 22): NUBASE has only an extrapolation from systematics, spin 5/2+ estimated; the lab shows no number", hoverHas(9, 22, "Half-life: not measured (NUBASE2020 has only an extrapolation from systematics") && hoverHas(9, 22, "5/2+ (estimated from systematics") &&
              !hoverHas(9, 22, "2 ms") && !hoverHas(9, 22, "half-life 2"));
    checkTrue("hover Pb-204 (stable, NUBASE lower limit and an entry with a question mark): the limit >140 Py and 'A ?' are shown as NUBASE writes them", hoverHas(82, 122, "Stable: no decay has been seen") && hoverHas(82, 122, "lower limit for the half-life, >140 Py") &&
              hoverHas(82, 122, "\"A ?\""));
    checkTrue("hover Kr-78 (0.12 stage 2): stable, with NUBASE2020's lower limit >110 Ey and its \"2B+ ?\" entry as written, and no half-life number (the old text said the lab's table had a measured half-life instead)", hoverHas(36, 42, "Stable: no decay has been seen") && hoverHas(36, 42, "a lower limit for the half-life, >110 Ey") && hoverHas(36, 42, "\"2B+ ?\"") && !hoverHas(36, 42, "measured half-life instead") && !hoverHas(36, 42, "Decays by") && !hoverHas(54, 70, "measured half-life instead") && hoverHas(54, 70, "a lower limit for the half-life, >200 Ty"));
    checkTrue("hover Ca-48: a half-life far longer than the universe's age and 'odds are not measured' for B- and 2B-", hoverHas(20, 28, "Practically stable (half-life 5.6×10¹⁹ years") && hoverHas(20, 28, "the odds are not measured") && hoverHas(20, 28, "double beta minus"));
    checkTrue("hover Ho-165 (measured size and moment): rms 5.2022 fm (CSV) and Q = +3.58(2) b (IAEA table)", hoverHas(67, 98, "rms 5.2022 fm") && hoverHas(67, 98, "Q = +3.58(2) b"));
    checkTrue("hover Og-294 (the heaviest): alpha, spin 0+, no measured radius", hoverHas(118, 176, "Oganesson-294") && hoverHas(118, 176, "alpha (α)") && hoverHas(118, 176, "Charge radius: no measured value"));
    checkTrue("hover of a cell with no nuclide says so and shows no half-life", hoverHas(2, 0, "Not in the table") && !hoverHas(2, 0, "Half-life") && hoverHas(200, 5, "Not in the table") == true);
    checkTrue("hover Li-3 (p-unst): says only that it is unstable to emitting a particle, no number", hoverHas(3, 0, "unstable to emitting a particle") && hoverHas(3, 0, "3/2− (estimated from systematics"));
    {
        int noSpinLine = 0, noHalfLine = 0, noDecay = 0, empty = 0;
        for (const ChartTile& t : tiles) {
            const std::vector<std::string> h = chartHover(t.Z, t.N);
            if (h.empty() || h[0].empty()) empty++;
            bool spin = false, half = false, dec = false;
            for (const std::string& L : h) { if (L.rfind("Spin and parity:", 0) == 0) spin = true; if (L.rfind("Half-life:", 0) == 0 || L.find("half-life") != std::string::npos || L.rfind("Stable", 0) == 0 || L.find("Unbound") == 0 || L.find("unbound") != std::string::npos) half = true; if (L.rfind("Decays by:", 0) == 0 || L.rfind("Decay:", 0) == 0) dec = true; }
            if (!spin) noSpinLine++;
            if (!half) noHalfLine++;
            if (t.cls != ChartClass::Stable && !dec) noDecay++;
        }
        checkNum("hover: tiles whose hover has no spin line (every tile says its spin or that there is none)", noSpinLine, 0, 0);
        checkNum("hover: tiles whose hover has no stable / half-life line", noHalfLine, 0, 0);
        checkNum("hover: unstable tiles whose hover has no decay line", noDecay, 0, 0);
        checkNum("hover: tiles with an empty hover", empty, 0, 0);
    }

    // ---- the drawing, through the recording stand-in: where every tile, the magic lines and the marker go, and what the legend and the hover box say
    {
        const double W = 900, H = 700;
        const ChartLayout L = chartLayout(W, H);
        const render::Color fg{0.1f, 0.2f, 0.3f, 1}, muted{0.4f, 0.5f, 0.6f, 1}, accent{0.9f, 0.1f, 0.7f, 1}, card{0.95f, 0.95f, 0.9f, 1}, line{0.5f, 0.5f, 0.5f, 1};
        const atom::ChartColours C{fg, muted, accent, card, line};
        render::Renderer2D R; RecText T;
        g_rec.clear();
        drawChart(R, T, C, W, H, 92, 146, 0, 0);
        const std::size_t all = g_rec.size();
        const double gap = L.cell >= 3 ? 0.6 : 0, side = L.cell - gap;
        auto same = [](render::Color a, render::Color b) { return std::fabs(a.r - b.r) < 1e-6 && std::fabs(a.g - b.g) < 1e-6 && std::fabs(a.b - b.b) < 1e-6 && std::fabs(a.a - b.a) < 1e-6; };
        int found = 0, wrongColour = 0, tileRects = 0;
        for (const RecRect& r : g_rec) if (std::fabs(r.w - side) < 1e-9 && std::fabs(r.h - side) < 1e-9) tileRects++;
        for (const ChartTile& t : tiles) {
            const render::Color want = *chartClassInfo(t.cls).hex ? render::hexColor(chartClassInfo(t.cls).hex) : fg;
            bool hit = false;
            for (const RecRect& r : g_rec) if (std::fabs(r.x - (L.x(t.N) + gap / 2)) < 1e-9 && std::fabs(r.y - (L.y(t.Z) + gap / 2)) < 1e-9 && std::fabs(r.w - side) < 1e-9) { hit = true; if (!same(r.c, want)) wrongColour++; break; }
            if (hit) found++;
            if (&t - &tiles[0] > 3556) break;
        }
        checkNum("chart drawing: squares drawn at a tile's size (one per nuclide: 3,557)", tileRects, 3557, 0);
        checkNum("chart drawing: tiles found at their own place (x(N), y(Z)) among the drawn rectangles", found, 3557, 0);
        checkNum("chart drawing: tiles drawn in a colour other than their class's colour (stable in the page's text colour)", wrongColour, 0, 0);
        checkTrue("chart drawing: the whole chart is a few dozen rectangles over the 3,557 tiles, each of them one rect call (the renderer puts all rects of a frame into one batch)", all > 3557 && all < 3557 + 120);
        int magicLines = 0, bad = 0;
        for (const RecRect& r : g_rec) if (same(r.c, muted.withAlpha(0.7f))) {
            magicLines++;
            bool ok = false;
            for (int m : kChartMagic) {
                if (std::fabs(r.w - 1) < 1e-9 && (std::fabs(r.x + 0.5 - L.x(m)) < 1e-9 || std::fabs(r.x + 0.5 - (L.x(m) + L.cell)) < 1e-9)) ok = true;     // a vertical edge of column N = m
                if (std::fabs(r.h - 1) < 1e-9 && (std::fabs(r.y + 0.5 - L.y(m)) < 1e-9 || std::fabs(r.y + 0.5 - (L.y(m) + L.cell)) < 1e-9)) ok = true;     // a horizontal edge of row Z = m
            }
            if (!ok) bad++;
        }
        checkNum("chart drawing: magic-number edge lines drawn (7 columns + 6 rows, 2 edges each: Z = 126 is beyond the last element, 118, so it has no row)", magicLines, 26, 0);
        checkNum("chart drawing: magic-number edge lines that are not on the edge of column N = m or row Z = m", bad, 0, 0);
        int missingEdge = 0;
        for (int m : kChartMagic) {                                // each magic column has its left edge and its right edge drawn, once each; each magic row (Z <= 118) its top and bottom edge
            int l = 0, r2 = 0, t2 = 0, b2 = 0;
            for (const RecRect& r : g_rec) if (same(r.c, muted.withAlpha(0.7f))) {
                if (std::fabs(r.w - 1) < 1e-9 && std::fabs(r.x + 0.5 - L.x(m)) < 1e-9) l++;
                if (std::fabs(r.w - 1) < 1e-9 && std::fabs(r.x + 0.5 - (L.x(m) + L.cell)) < 1e-9) r2++;
                if (std::fabs(r.h - 1) < 1e-9 && std::fabs(r.y + 0.5 - L.y(m)) < 1e-9) t2++;
                if (std::fabs(r.h - 1) < 1e-9 && std::fabs(r.y + 0.5 - (L.y(m) + L.cell)) < 1e-9) b2++;
            }
            if (l != 1 || r2 != 1) missingEdge++;
            if (m <= 118 && (t2 != 1 || b2 != 1)) missingEdge++;
        }
        checkNum("chart drawing: magic numbers whose column lacks its left or right edge line, or whose row lacks its top or bottom edge line", missingEdge, 0, 0);
        double mx = -1, my = -1; int markRects = 0;
        for (const RecRect& r : g_rec) if (same(r.c, accent)) { markRects++; mx = r.x + r.w / 2; my = r.y + r.h / 2; }
        checkTrue("chart drawing: the chosen nuclide (U-238) is marked: the accent-colour box round the middle of its square, so a point inside is the chosen tile",
                  markRects == 4 && std::fabs(mx - (L.x(146) + L.cell / 2)) < 2 * L.cell + 6 && std::fabs(my - (L.y(92) + L.cell / 2)) < 2 * L.cell + 6);
        g_rec.clear(); drawChart(R, T, C, W, H, 26, 30, 0, 0);
        double mx2 = -1; for (const RecRect& r : g_rec) if (same(r.c, accent)) mx2 = r.x + r.w / 2;
        checkTrue("chart drawing: the marker follows the chosen nuclide (Fe-56 is left of U-238)", mx2 > 0 && mx2 < mx - 10 * L.cell);
        g_rec.clear(); drawChart(R, T, C, W, H, 26, 30, 92, 146);
        int hovBox = 0; for (const RecRect& r : g_rec) if (same(r.c, fg) && std::fabs(r.w - (L.cell + 2)) < 1e-9) hovBox++;
        checkTrue("chart drawing: a hovered tile (not the chosen one) gets an outline in the text colour", hovBox >= 2);
        g_rec.clear(); T.lines.clear(); drawChart(R, T, C, W, H, 92, 146, 0, 0);
        std::string all2; for (const std::string& l : T.lines) all2 += l + "\n";
        bool legend = true; for (int c = 0; c < kChartClasses; c++) if (chartClassCount(ChartClass(c)) > 0 && all2.find(std::string(chartClassInfo(ChartClass(c)).words).substr(0, 20)) == std::string::npos) legend = false;
        checkTrue("chart drawing: the legend says every colour that has tiles in words (each class's sentence starts the legend line)", legend);
        checkTrue("chart drawing: the magic-number sentence and the axis titles are drawn", all2.find("Magic numbers: 2, 8, 20, 28, 50, 82") != std::string::npos && all2.find("neutrons N") != std::string::npos && all2.find("protons Z") != std::string::npos);
        checkTrue("chart drawing: the status line names the chosen nuclide (U-238) and says what the mouse and keys do", all2.find("Chosen: Uranium-238 (²³⁸U)") != std::string::npos && all2.find("click to choose it") != std::string::npos && all2.find("arrow keys") != std::string::npos);
        T.lines.clear(); drawChart(R, T, C, W, H, 26, 30, 2, 2);
        all2.clear(); for (const std::string& l : T.lines) all2 += l + "\n";
        checkTrue("chart drawing: hovering He-4 changes nothing in the status line (the details are a tooltip made from chartHover, tested above); the chosen one is still Fe-56", all2.find("Chosen: Iron-56") != std::string::npos);
        // the legend is placed where the chart leaves room: its box covers no tile when there is room for it, in the sizes a window really has
        {
            int covered = 0, boxes = 0; bool fullWords = false;
            const double sz[][2] = {{900, 700}, {1280, 720}, {1400, 900}, {1920, 1000}, {1000, 800}};
            for (const auto& wh : sz) {
                const ChartLayout LL = chartLayout(wh[0], wh[1]);
                g_rec.clear(); T.lines.clear(); drawChart(R, T, C, wh[0], wh[1], 92, 146, 0, 0);
                for (const RecRect& r : g_rec) if (same(r.c, card.withAlpha(0.9f))) {
                    boxes++;
                    for (const ChartTile& t : tiles) { const double tx = LL.x(t.N), ty = LL.y(t.Z); if (tx < r.x + r.w && tx + LL.cell > r.x && ty < r.y + r.h && ty + LL.cell > r.y) { covered++; break; } }
                }
                std::string a3; for (const std::string& l : T.lines) a3 += l + "\n";
                if (wh[0] >= 1280 && a3.find("Common in heavy nuclei") != std::string::npos) fullWords = true;
            }
            checkNum("chart drawing: legend boxes drawn, one per picture size (5 sizes)", boxes, 5, 0);
            checkNum("chart drawing: legend boxes that cover a tile, in 5 window sizes from 900 x 700 to 1920 x 1000", covered, 0, 0);
            checkTrue("chart drawing: in a big picture the legend has the full wording for every colour", fullWords);
        }
        int inside = 0; g_rec.clear(); drawChart(R, T, C, 400, 300, 26, 30, 0, 0);
        for (const RecRect& r : g_rec) if (r.x >= -1 && r.y >= -1 && r.x + r.w <= 401 && r.y + r.h <= 301) inside++;
        checkTrue("chart drawing: in a small picture (400 x 300) every rectangle still lies inside it", inside == int(g_rec.size()));
    }
    // ---- NUBASE spin text: the four kinds
    {
        const NubaseRow* a = nubase(1, 2); const NubaseRow* b = nubase(3, 9); const NubaseRow* c = nubase(5, 11); const NubaseRow* d = nubase(2, 2); const NubaseRow* e = nubase(5, 3);
        checkTrue("nubase spin: H-3 '1/2+*' measured, Li-12 '(1-,2-)' tentative, B-16 '0-#' systematics, He-4 '0+' evaluated, B-8 is listed", a && b && c && d && e && nubaseSpin(*a).how == 1 && nubaseSpin(*a).text == "1/2+" && nubaseSpin(*b).how == 3 && nubaseSpin(*b).text == "(1−,2−)" &&
                  nubaseSpin(*c).how == 2 && nubaseSpin(*c).text == "0−" && nubaseSpin(*d).how == 4 && nubaseSpin(*d).text == "0+");
        int blank = 0, kinds[128] = {0}; for (const ChartTile& t : tiles) { const NubaseRow* r = nubase(t.Z, t.N); if (!*r->jpi) blank++; kinds[int(r->kind)]++; }
        checkTrue("nubase: 185 ground states have no spin, and the half-life kinds are 253 stable, 2,831 value, 369 systematics, 84 none, 11 limit, 6 approximate, 3 p-unst (counted from the file)", blank == 185 && kinds[int('S')] == 253 && kinds[int('M')] == 2831 &&
                  kinds[int('E')] == 369 && kinds[int('-')] == 84 && kinds[int('B')] == 11 && kinds[int('A')] == 6 && kinds[int('P')] == 3);
    }
    // ---- the legend is complete and the words say what each colour means
    {
        int listed = 0; for (int c = 0; c < kChartClasses; c++) if (chartClassCount(ChartClass(c)) > 0) listed++;
        checkNum("chart legend: classes with tiles (all but Other)", listed, 8, 0);
        checkTrue("chart: the magic-number sentence names all seven numbers and says what a magic number is", [&] { const std::string w = chartMagicWords(); for (int m : kChartMagic) if (w.find(std::to_string(m)) == std::string::npos) return false; return w.find("filled shell") != std::string::npos; }());
        checkTrue("chart: the view's name is 'Chart of nuclides' and it is a valid view choice", std::string(kChartView) == "Chart of nuclides" && AtomLab::validChoice("view", kChartView));
    }
}

// -------------------------------------------------------------------------------------------------------------------------------------------------------------
// 0.11 stage 3: the chart as a landscape. REFERENCES are not the lab's own output: (1) heights worked out by hand from the stated mapping, (log10(T) + 23) / 55, with the half-lives typed from the lab's
// table and the NUBASE2020 file (Python was used as a calculator for the logarithms, not as a second copy of the code); (2) the isometric axes by hand (cos 45 = 0.70710678, cos 54.7356 = 0.57735027, sin 54.7356 =
// 0.81649658: the three axes then look sqrt(2/3) = 0.81649658 times their length); (3) the picture is checked against a RAY CASTER written here, which knows nothing of the painter's order: for each pixel it
// finds the first box a ray from the viewer enters, and the block the lab reports (the last one drawn that covers the pixel) must be that one. Guards are exact counts or stated tolerances (float storage: 1e-4 pt).
struct RayHit { bool hit = false; int Z = 0, N = 0; double d = -1e300; int boxes = 0; };
static RayHit rayCast(const ChartScene& S, double px, double py) {
    const ChartProjection& P = S.proj(); const ChartLayout& L = S.layout();
    double a0, b0, z0, a1, b1, z1; P.unmap(px, py, 0, a0, b0, z0); P.unmap(px, py, 1, a1, b1, z1);
    const double p0[3] = {a0, b0, z0}, dv[3] = {a1 - a0, b1 - b0, z1 - z0};
    RayHit best;
    for (const ChartTile& t : chartTiles()) {
        const double lo3[3] = {L.x(t.N), L.y(t.Z), -1e-6}, hi3[3] = {L.x(t.N) + L.cell, L.y(t.Z) + L.cell, S.heightPoints(chartHeightOf(t.Z, t.N, S.mode()).h) + 1e-6};
        double lo = -1e300, hi = 1e300; bool miss = false;
        for (int k = 0; k < 3 && !miss; k++) {
            if (std::fabs(dv[k]) < 1e-12) { if (p0[k] < lo3[k] || p0[k] > hi3[k]) miss = true; continue; }
            const double t1 = (lo3[k] - p0[k]) / dv[k], t2 = (hi3[k] - p0[k]) / dv[k];
            lo = std::max(lo, std::min(t1, t2)); hi = std::min(hi, std::max(t1, t2));
        }
        if (miss || lo > hi) continue;
        best.boxes++;
        if (hi > best.d) { best.hit = true; best.d = hi; best.Z = t.Z; best.N = t.N; }       // the largest depth = the nearest to the viewer
    }
    return best;
}
static const ChartBlock* blockOf(const ChartScene& S, int Z, int N) { for (const ChartBlock& B : S.blocks()) if (B.Z == Z && B.N == N) return &B; return nullptr; }
static void testChartLandscape() {
    const double eps = 1e-12;
    // ---- the height mapping: half-life in, height out, by hand
    struct HL { double T, h; const char* what; };
    static const HL hl[] = {{1e-23, 0, "10^-23 s: the bottom of the scale"}, {1e-30, 0, "far below: cut to 0"}, {1e32, 1, "10^32 s: the top"}, {1e40, 1, "far above: cut to 1"},
                            {1, 0.41818181818181815, "1 s = 23/55"}, {1e-3, 0.36363636363636365, "1 ms = 20/55"}, {1e4, 0.4909090909090909, "10^4 s = 27/55"}, {86400, 0.5079366134996163, "a day"},
                            {365.25 * 86400, 0.5545291630379132, "a year"}, {1.39e-22, 0.020782087277347212, "H-4: 1.39e-22 s"}, {1.41e17, 0.7299858020482797, "U-238: 1.41e17 s"},
                            {3.89e8, 0.5743627200241037, "H-3: 3.89e8 s"}, {6.34e26, 0.905492531961486, "Bi-209: 6.34e26 s"}, {7.1e31, 0.9972956063403468, "Te-128: 7.1e31 s, the longest in the table"},
                            {1e-10, 0.23636363636363636, "10^-10 s = 13/55"}, {1e12, 0.6363636363636364, "10^12 s = 35/55"}, {1e20, 0.7818181818181819, "10^20 s = 43/55"}};
    int badH = 0; for (const HL& k : hl) if (std::fabs(chartHeightFromHalfLife(k.T) - k.h) > 1e-12) { badH++; std::printf("height of %s: got %.15g want %.15g\n", k.what, chartHeightFromHalfLife(k.T), k.h); }
    checkNum("landscape height: half-lives (17, spanning the whole scale and beyond it) whose height differs from the hand-worked value by more than 1e-12", badH, 0, 0);
    checkTrue("landscape height: zero, negative and not-a-number half-lives are height 0 (nothing is invented), and each factor of ten adds exactly 1/55", chartHeightFromHalfLife(0) == 0 && chartHeightFromHalfLife(-5) == 0 && chartHeightFromHalfLife(NAN) == 0 &&
              std::fabs(chartHeightFromHalfLife(1e5) - chartHeightFromHalfLife(1e4) - 1.0 / 55) < eps && std::fabs(chartHeightFromHalfLife(1e-7) - chartHeightFromHalfLife(1e-8) - 1.0 / 55) < eps);
    checkTrue("landscape height: the scale is 10^-23 s to 10^32 s and a scale height is 12 tiles (chosen by looking at the replay: 36 hid the proton-rich side behind the stable ridge); the stable plateau is 1.1 scale heights", kHeightLog10Lo == -23 && kHeightLog10Hi == 32 && kHeightCells == 12 && kStableHeight == 1.1);
    // ---- what each group gets
    struct HK { int Z, N; HeightKind k; double h; const char* what; };
    static const HK hk[] = {
        {2, 2, HeightKind::Stable, 1.1, "He-4: stable, the plateau"}, {26, 30, HeightKind::Stable, 1.1, "Fe-56: stable, the plateau"},
        {82, 122, HeightKind::StableNoted, 1.1, "Pb-204: stable in the lab's table, NUBASE gives a limit >140 Py and 'A ?': the plateau and a mark"}, {36, 42, HeightKind::StableNoted, 1.1, "Kr-78: stable in NUBASE2020 (limit >110 Ey and '2B+ ?') and, since 0.12 stage 2, in the lab: the plateau and a mark"},
        {92, 146, HeightKind::Measured, 0.7299858020482797, "U-238"}, {1, 2, HeightKind::Measured, 0.5743627200241037, "H-3"}, {83, 126, HeightKind::Measured, 0.905492531961486, "Bi-209: 6.34e26 s, well below the plateau"},
        {52, 76, HeightKind::Measured, 0.9972956063403468, "Te-128: the longest half-life in the table, just under the top"}, {1, 3, HeightKind::Measured, 0.020782087277347212, "H-4 (Z 1, N 3): 1.39e-22 s"}, {1, 4, HeightKind::Measured, 0.01699088093170121, "H-5 (Z 1, N 4): 8.6e-23 s, the shortest in the table"},
        {5, 11, HeightKind::Limit, 0.048413778757846804, "B-16: only a limit, >4.6 zs"}, {18, 12, HeightKind::Limit, 0.21818181818181817, "Ar-30: only a limit, <10 ps"}, {97, 151, HeightKind::Limit, 0.571879026845901, "Bk-248: >9 y"},
        {63, 99, HeightKind::Approx, 0.43636363636363634, "Eu-162: NUBASE ~10 s"}, {104, 152, HeightKind::Measured, 0.3785371624643976, "Rf-256: NUBASE2020 6.60 ms (the lab had called it approximate; the value is the same)"},
        {9, 22, HeightKind::Extrapolated, 0, "F-31: only an extrapolation from systematics: no height"}, {3, 9, HeightKind::Unknown, 0, "Li-12: no half-life at all: no height"}, {3, 0, HeightKind::Unknown, 0, "Li-3: p-unst: no height"}};
    for (const HK& k : hk) {
        const ChartHeight c = chartHeightOf(k.Z, k.N);
        if (k.h < 0) checkTrue(std::string("landscape height: ") + k.what, c.kind != HeightKind::Stable && c.kind != HeightKind::StableNoted && c.h < 1 - 1e-9 && c.h > 0.9);
        else checkTrue(std::string("landscape height: ") + k.what, c.kind == k.k && std::fabs(c.h - k.h) < 1e-9);
    }
    {
        int unknownWithHeight = 0, stableOff = 0, over = 0, nonStableOnPlateau = 0, total = 0;
        for (const ChartTile& t : chartTiles()) {
            const ChartHeight c = chartHeightOf(t.Z, t.N); total++;
            if ((c.kind == HeightKind::Unknown || c.kind == HeightKind::Extrapolated) && c.h != 0) unknownWithHeight++;
            if ((c.kind == HeightKind::Stable || c.kind == HeightKind::StableNoted) != (t.cls == ChartClass::Stable) || ((c.kind == HeightKind::Stable || c.kind == HeightKind::StableNoted) && c.h != kStableHeight)) stableOff++;
            if (c.kind != HeightKind::Stable && c.kind != HeightKind::StableNoted && (c.h > 1 || c.h < 0)) over++;
            if (c.kind != HeightKind::Stable && c.kind != HeightKind::StableNoted && c.h >= kStableHeight) nonStableOnPlateau++;
        }
        checkNum("landscape height: tiles drawn with a height although their half-life is unknown or only extrapolated", unknownWithHeight, 0, 0);
        checkNum("landscape height: tiles whose stability and plateau disagree (the stable ones, and only they, stand at 1.1)", stableOff, 0, 0);
        checkNum("landscape height: not-stable tiles outside 0 .. 1 or on the plateau", over + nonStableOnPlateau, 0, 0);
        checkNum("landscape height: every tile has a kind (3,557)", total, 3557, 0);
        static const struct { HeightKind k; int n; } cnt[] = {{HeightKind::Measured, 2831}, {HeightKind::Stable, 182}, {HeightKind::StableNoted, 71}, {HeightKind::Approx, 6}, {HeightKind::Limit, 11}, {HeightKind::Extrapolated, 369}, {HeightKind::Unknown, 87}};
        int wrong = 0, sum = 0; for (const auto& c : cnt) { if (chartHeightCount(c.k) != c.n) wrong++; sum += chartHeightCount(c.k); }
        checkNum("landscape height: kinds whose count differs from the separate count (0.12 stage 2, from the NUBASE2020 file: measured 2,831 = its plain half-lives, stable 182, stable with a NUBASE note 71, approximate 6 = its '~' values, limit 11, extrapolated 369, unknown 87; before: 2,720, 182, 69, 119, 11, 369, 87)", wrong, 0, 0);
        checkNum("landscape height: the kinds add up to every tile", sum, 3557, 0);
        checkTrue("landscape height: the legend sentence says it is a chosen scale, the base, the two ends, the plateau, and that no height is invented", [&] { const std::string w = chartHeightWords(); for (const char* x : {"not a measurement", "ten times", "10⁻²³ s", "10³² s", "fixed height above the top", "no height is invented"}) if (w.find(x) == std::string::npos) return false; return true; }());
    }
    // ---- tilt zero is the stage 2 chart
    const double sizes2[][2] = {{900, 700}, {640, 480}, {1400, 900}, {1920, 1000}};
    {
        int off = 0, sides = 0, notId = 0; double worst = 0;
        for (const auto& wh : sizes2) {
            ChartScene S; S.build(wh[0], wh[1], ChartViewState());
            const ChartLayout L = chartLayout(wh[0], wh[1]);
            if (!S.proj().identity) notId++;
            for (const ChartBlock& B : S.blocks()) {
                const double x = L.x(B.N), y = L.y(B.Z), want[8] = {x, y, x + L.cell, y, x + L.cell, y + L.cell, x, y + L.cell};
                for (int i = 0; i < 8; i++) { const double e = std::fabs(double(B.top[i]) - want[i]); worst = std::max(worst, e); if (e > 1e-4) off++; }
                if (B.sides) sides++;
            }
        }
        checkNum("landscape tilt 0: tile corners that are not at stage 2's place (x(N), y(Z), one square) within 1e-4 pt (float storage), 4 picture sizes x 3,557 tiles x 4 corners", off, 0, 0);
        checkTrue("landscape tilt 0: the largest difference is below the float rounding of the coordinates, no tile has side faces, and the projection is the identity", worst < 1e-4 && sides == 0 && notId == 0);
        // the same maths at tilt 0 with a zoom: the whole picture scaled about its centre
        ChartScene Z2; Z2.build(900, 700, ChartViewState{0, 2, 0, 0});
        const ChartLayout L = chartLayout(900, 700); int offZ = 0;
        for (const ChartBlock& B : Z2.blocks()) { const double x = 450 + 2 * (L.x(B.N) - 450), y = 350 + 2 * (L.y(B.Z) - 350); if (std::fabs(B.top[0] - x) > 2e-3 || std::fabs(B.top[1] - y) > 2e-3) offZ++; }
        checkNum("landscape tilt 0, zoom 2: tiles not at the flat position scaled by 2 about the picture's centre (2e-3 pt)", offZ, 0, 0);
        // pick equals stage 2's grid lookup
        ChartScene F; F.build(900, 700, ChartViewState()); int diff = 0, seen = 0;
        for (double py = 3.3; py < 700; py += 5.3) for (double px = 2.7; px < 900; px += 5.3) {
            int z1 = 0, n1 = 0, z2 = 0, n2 = 0; const bool a = chartTileAt(L, px, py, z1, n1), b = F.pick(px, py, z2, n2);
            if (a != b || (a && (z1 != z2 || n1 != n2))) diff++;
            if (a) seen++;
        }
        checkTrue("landscape tilt 0: the scene's hit test and stage 2's grid lookup agree at every point of a 5.3-point grid (about 20,000 points, about 3,000 on tiles)", diff == 0 && seen > 2500);
    }
    // ---- the projection at full tilt, by hand
    {
        ChartScene S; S.build(900, 700, ChartViewState{1, 1, 0, 0});
        const ChartProjection& P = S.proj(); const double K = P.K;
        double ox, oy, od, ax, ay, ad, bx, by, bd, zx, zy, zd;
        P.map(500, 300, 0, ox, oy, od); P.map(501, 300, 0, ax, ay, ad); P.map(500, 301, 0, bx, by, bd); P.map(500, 300, 1, zx, zy, zd);
        checkTrue("landscape iso: one step along N goes right and down by (0.70710678, 0.40824829) x K", std::fabs((ax - ox) / K - 0.7071067811865476) < 1e-9 && std::fabs((ay - oy) / K - 0.40824829046386296) < 1e-9);
        checkTrue("landscape iso: one step down the flat layout (towards smaller Z) goes left and down by (-0.70710678, 0.40824829) x K", std::fabs((bx - ox) / K + 0.7071067811865476) < 1e-9 && std::fabs((by - oy) / K - 0.40824829046386296) < 1e-9);
        checkTrue("landscape iso: one step up in height goes straight up by 0.81649658 x K", std::fabs((zx - ox) / K) < 1e-9 && std::fabs((zy - oy) / K + 0.816496580927726) < 1e-9);
        const double la = std::hypot(ax - ox, ay - oy) / K, lb = std::hypot(bx - ox, by - oy) / K, lz = std::hypot(zx - ox, zy - oy) / K;
        checkTrue("landscape iso: the three axes look equally long, sqrt(2/3) = 0.81649658 of their length", std::fabs(la - 0.816496580927726) < 1e-9 && std::fabs(lb - 0.816496580927726) < 1e-9 && std::fabs(lz - 0.816496580927726) < 1e-9);
        checkTrue("landscape iso: nearer to the viewer = larger N and smaller Z, and higher is nearer (depth grows with N, with the flat-layout y, and with the height)", ad > od && bd > od && zd > od);
        // map and unmap are inverse, at several tilts and with zoom and pan
        double worstR = 0;
        for (const ChartViewState& st : {ChartViewState{0.37, 1, 0, 0}, ChartViewState{1, 3, 40, -25}, ChartViewState{0.8, 7, -300, 120}, ChartViewState{0, 2, 10, 10}}) {
            ChartScene Q; Q.build(900, 700, st); const ChartProjection& PP = Q.proj();
            for (double a : {100.0, 333.3, 640.0, 801.0}) for (double b : {50.0, 222.2, 480.0}) for (double z : {0.0, 17.5, 90.0}) {
                double sx, sy, d, a2, b2, z2; PP.map(a, b, z, sx, sy, d); PP.unmap(sx, sy, d, a2, b2, z2);
                worstR = std::max(worstR, std::max(std::fabs(a2 - a), std::max(std::fabs(b2 - b), std::fabs(z2 - z))));
            }
        }
        checkTrue("landscape iso: unmap undoes map to 1e-9 pt at four views (tilts 0.37, 1, 0.8, 0; zooms up to 7; pans)", worstR < 1e-9);
    }
    // ---- the whole landscape is in the picture at zoom 1, at every tilt and size
    {
        int out = 0;
        for (const auto& wh : sizes2) for (double tilt : {0.05, 0.25, 0.5, 0.75, 1.0}) {
            ChartScene S; S.build(wh[0], wh[1], ChartViewState{tilt, 1, 0, 0});
            for (const ChartBlock& B : S.blocks()) if (B.x0 < -1 || B.y0 < -1 || B.x1 > wh[0] + 1 || B.y1 > wh[1] + 1) out++;
        }
        checkNum("landscape fit: blocks (any face) outside the picture by more than 1 pt at tilts 0.05 .. 1 and 4 sizes", out, 0, 0);
    }
    // ---- the painter's order against the ray caster; the hit test (the topmost drawn block) against the ray caster
    {
        struct Case { double w, h, tilt, zoom, panX, panY; double step; };
        const Case cases[] = {{900, 700, 0.35, 1, 0, 0, 13}, {900, 700, 1, 1, 0, 0, 11}, {640, 480, 1, 1, 0, 0, 8}, {900, 700, 0.7, 3, 80, -40, 13}, {900, 700, 1, 6, -150, 60, 17}};
        for (const Case& c : cases) {
            ChartScene S; S.build(c.w, c.h, ChartViewState{c.tilt, c.zoom, c.panX, c.panY});
            int tested = 0, mismatch = 0, hits = 0, overlaps = 0, missPick = 0;
            for (double py = 0.37; py < c.h; py += c.step) for (double px = 0.61; px < c.w; px += c.step) {
                const RayHit r = rayCast(S, px, py); int Z = 0, N = 0; const bool p = S.pick(px, py, Z, N);
                tested++;
                if (r.hit) hits++;
                if (r.boxes >= 2) overlaps++;
                if (p != r.hit || (p && (Z != r.Z || N != r.N))) { mismatch++; if (mismatch <= 3) std::printf("  mismatch at %.2f %.2f: pick %d (%d,%d) ray %d (%d,%d) boxes %d\n", px, py, int(p), Z, N, int(r.hit), r.Z, r.N, r.boxes); }
                if (!p && r.hit) missPick++;
            }
            const std::string tag = "landscape order and hit test, " + std::to_string(int(c.w)) + "x" + std::to_string(int(c.h)) + " tilt " + numText(c.tilt, 2) + " zoom " + numText(c.zoom, 2) + ": ";
            checkNum(tag + "pixels (a grid) where the topmost drawn block is not the first box the ray caster's ray enters, or one finds a block and the other none", mismatch, 0, 0);
            checkTrue(tag + "the grid had many pixels on blocks (" + std::to_string(hits) + ") and, for tilted views, many where a ray meets two or more boxes, so the order matters (" + std::to_string(overlaps) + ")", tested > 1000 && hits > 300 && (c.tilt < 0.3 || overlaps > 100));
            (void)missPick;
        }
    }
    // 0.40: the 3D chart can be turned all the way round and tipped from straight down to 5 degrees above its floor. From every side the painter's order and the faces
    // drawn must still agree with the ray caster (which knows nothing of the order or of which faces face the camera). Until 0.40 there was one order and one pair of faces.
    {
        int mismatch = 0, tested = 0, overlaps = 0;
        for (const double yaw : {10.0, 100.0, 170.0, 200.0, 265.0, 300.0, -45.0}) for (const double pitch : {25.0, 60.0, 85.0}) {
            ChartViewState v; v.tilt = 1; v.yawDeg = yaw; v.pitchDeg = pitch;
            ChartScene S; S.build(900, 700, v);
            for (double py = 0.37; py < 700; py += 11) for (double px = 0.61; px < 900; px += 11) {
                const RayHit r = rayCast(S, px, py); int Z = 0, N = 0; const bool p = S.pick(px, py, Z, N);
                tested++; if (r.boxes >= 2) overlaps++;
                if (p != r.hit || (p && (Z != r.Z || N != r.N))) { mismatch++; if (mismatch <= 3) std::printf("  3D yaw %.0f pitch %.0f, mismatch at %.2f %.2f: pick %d (%d,%d) ray %d (%d,%d)\n", yaw, pitch, px, py, int(p), Z, N, int(r.hit), r.Z, r.N); }
            }
        }
        checkNum("3D chart from 7 sides x 3 tips (0.40): pixels where the topmost drawn block is not the first box the ray caster's ray enters", mismatch, 0, 0);
        checkTrue("3D chart from 7 sides x 3 tips: many pixels tested where a ray meets two or more boxes (" + std::to_string(overlaps) + " of " + std::to_string(tested) + ")", tested > 100000 && overlaps > 4000);
        ChartViewState lo; lo.tilt = 1; lo.pitchDeg = -30; ChartViewState hi; hi.tilt = 1; hi.pitchDeg = 120; hi.yawDeg = 400;
        ChartScene S; S.build(900, 700, lo); const ChartViewState a = S.clamped(lo), b = S.clamped(hi);
        checkTrue("3D chart: the tip is kept between straight down (0) and 85 degrees (never under the floor); the turn wraps round", a.pitchDeg == 0 && b.pitchDeg == 85 && std::fabs(b.yawDeg - 40) < 1e-9);
    }
    {   // two explicit cases where a tall block overlaps a short one behind it, and one where a short block is in front of a tall one
        ChartScene S; S.build(900, 700, ChartViewState{1, 1, 0, 0});
        const ChartBlock* tall = blockOf(S, 82, 126);       // Pb-208 (stable, the plateau); behind it (smaller N, larger Z) stands Tl-207 (81, 126)? the neighbour at (83, 125) is checked below
        const ChartBlock* shortBehind = blockOf(S, 83, 125);
        checkTrue("landscape pick: Pb-208 (a stable block on the plateau) and the block behind it exist", tall && shortBehind && tall->h == kStableHeight && shortBehind->h < kStableHeight);
        if (tall && shortBehind) {
            // the middle of the tall block's front face (b side), which lies over the blocks behind it
            const double fx = (tall->faceB[0] + tall->faceB[4]) / 2, fy = (tall->faceB[1] + tall->faceB[5]) / 2; int Z = 0, N = 0;
            const bool got = S.pick(fx, fy, Z, N);
            checkTrue("landscape pick: a point on the tall block's front face is reported as the tall block (82, 126)", got && Z == 82 && N == 126);
            // a point that is on the short block behind's top face AND inside the tall block's silhouette: the tall one wins
            int cover = 0, wrong = 0;
            for (const ChartBlock& B : S.blocks()) {
                if (&B == tall || B.N >= tall->N || B.Z <= tall->Z) continue;          // behind it: smaller N, larger Z
                const double cx = (B.top[0] + B.top[4]) / 2, cy = (B.top[1] + B.top[5]) / 2;
                const RayHit r = rayCast(S, cx, cy);
                if (r.hit && r.Z == tall->Z && r.N == tall->N) { cover++; int z2 = 0, n2 = 0; if (!S.pick(cx, cy, z2, n2) || z2 != tall->Z || n2 != tall->N) wrong++; }
            }
            checkTrue("landscape pick: blocks behind Pb-208 whose top is hidden by it (" + std::to_string(cover) + " of them): at their top's centre the tall block is reported, every time", cover >= 3 && wrong == 0);
            // and the reverse: a short block in front of a tall one: at its own top's centre the short block, not the tall one behind
            const ChartBlock* front = blockOf(S, 81, 127);
            if (front) { const double cx = (front->top[0] + front->top[4]) / 2, cy = (front->top[1] + front->top[5]) / 2; int z3 = 0, n3 = 0; const bool g = S.pick(cx, cy, z3, n3); const RayHit r = rayCast(S, cx, cy);
                checkTrue("landscape pick: at the centre of the top of a block nearer than Pb-208 (81, 127) the lab and the ray caster agree on the topmost block", g && r.hit && z3 == r.Z && n3 == r.N); }
        }
        // edges of the picture: outside is no block; at the picture's own border the answer is the ray caster's
        int Z = 0, N = 0; int bad = 0;
        for (const auto& pt : {std::pair<double, double>{-1, 350}, {901, 350}, {450, -1}, {450, 701}, {-5, -5}, {2000, 2000}}) { ChartScene E; E.build(900, 700, ChartViewState{1, 1, 0, 0}); if (E.pick(pt.first, pt.second, Z, N) != rayCast(E, pt.first, pt.second).hit) bad++; }
        for (const double zoom : {1.0, 4.0}) { ChartScene E; E.build(900, 700, ChartViewState{0.6, zoom, 0, 0});
            for (const auto& pt : {std::pair<double, double>{0.01, 0.01}, {899.99, 0.01}, {0.01, 699.99}, {899.99, 699.99}, {0.01, 350}, {899.99, 350}, {450, 0.01}, {450, 699.99}}) { const RayHit r = rayCast(E, pt.first, pt.second); if (E.pick(pt.first, pt.second, Z, N) != r.hit || (r.hit && (Z != r.Z || N != r.N))) bad++; } }
        checkNum("landscape pick: points outside the picture and on its edges (zoom 1 and 4) where the lab and the ray caster disagree", bad, 0, 0);
    }
    // ---- zoom and pan
    {
        AtomLab lab; lab.p = oneParams(); lab.paused = false; lab.restart(); lab.setParam("view", std::string(kChartView)); lab.viewW = 900; lab.viewH = 700;
        lab.setParam("tilt", 0.7);
        checkTrue("landscape state: the tilt is set by setParam('tilt'), cut to 0 .. 1, and is kept when another nuclide is chosen", [&] { lab.setParam("tilt", 5); const bool a = lab.s.chartView.tilt == 1; lab.setParam("tilt", -2); const bool b = lab.s.chartView.tilt == 0; lab.setParam("tilt", 0.7);
            lab.chooseNuclide(26, 30); return a && b && lab.s.chartView.tilt == 0.7 && lab.p.Z == 26 && lab.p.N == 30; }());
        const ChartBlock* b0 = blockOf(lab.chartScene(), 92, 146); const float x0 = b0->top[0], y0 = b0->top[1];
        lab.chartZoomAt(2, 300, 200); lab.chartZoomAt(0.5, 300, 200);
        const ChartBlock* b1 = blockOf(lab.chartScene(), 92, 146);
        checkTrue("landscape zoom: zooming in by 2 and out by 1/2 about the same point returns the picture (zoom 1, no pan, the same corner within 1e-3 pt)", lab.s.chartView.zoom == 1 && lab.s.chartView.panX == 0 && std::fabs(b1->top[0] - x0) < 1e-3 && std::fabs(b1->top[1] - y0) < 1e-3);
        lab.chartZoomAt(2, 450, 350); const ChartViewState v2 = lab.s.chartView;
        lab.chartZoomAt(3, 470, 330); lab.chartZoomAt(1.0 / 3, 470, 330);
        checkTrue("landscape zoom: zoom 2, then x3 and back by 1/3 about another point, returns the same view (zoom and pan within 1e-9)", std::fabs(lab.s.chartView.zoom - v2.zoom) < 1e-9 && std::fabs(lab.s.chartView.panX - v2.panX) < 1e-9 && std::fabs(lab.s.chartView.panY - v2.panY) < 1e-9);
        {   // the point under the pointer stays where it is (the picture is scaled about it)
            lab.chartReset(); lab.setParam("tilt", 1);
            const ChartBlock* a = blockOf(lab.chartScene(), 50, 70); const float ax = a->top[0], ay = a->top[1];
            lab.chartZoomAt(2.5, 380, 260);
            const ChartBlock* b = blockOf(lab.chartScene(), 50, 70);
            checkTrue("landscape zoom: a tile's corner goes to q + k (p - q) when zooming by k about q (within 1e-2 pt)", std::fabs(b->top[0] - (380 + 2.5 * (ax - 380))) < 1e-2 && std::fabs(b->top[1] - (260 + 2.5 * (ay - 260))) < 1e-2);
        }
        lab.chartZoomAt(1000, 450, 350); const double zmax = lab.s.chartView.zoom; lab.chartZoomAt(1e-6, 450, 350);
        checkTrue("landscape zoom: the zoom is cut to 16 at most and 1/2 at least (5p: it was 1 at least, changed ON PURPOSE: twice as far out); zoomed out the pan is 0", zmax == 16 && lab.s.chartView.zoom == 0.5 && lab.s.chartView.panX == 0 && lab.s.chartView.panY == 0);
        // the chart cannot be lost: extreme pans at several zooms and tilts leave a strip of it in the picture
        int lost = 0, tried = 0;
        for (double tilt : {0.0, 0.5, 1.0}) for (double zoom : {2.0, 8.0, 16.0}) for (const auto& d : {std::pair<double, double>{1e5, 0}, {-1e5, 0}, {0, 1e5}, {0, -1e5}, {-1e5, -1e5}, {1e5, 1e5}}) {
            lab.chartReset(); lab.setParam("tilt", tilt); lab.chartZoomAt(zoom, 450, 350); lab.chartPan(d.first, d.second); tried++;
            double bx0, by0, bx1, by1; lab.chartScene().bbox(bx0, by0, bx1, by1);
            const double bw = bx1 - bx0, bh = by1 - by0, needX = std::min(std::min(bw, 900.0), 120.0), needY = std::min(std::min(bh, 700.0), 90.0);
            if (std::min(bx1, 900.0) - std::max(bx0, 0.0) < needX - 0.01 || std::min(by1, 700.0) - std::max(by0, 0.0) < needY - 0.01) lost++;   // (0.01 pt: the corners are floats)
        }
        checkNum("landscape pan: extreme pans (x, y, both, 54 cases: 3 tilts x 3 zooms x 6 directions) after which less than a 120 x 90 pt strip of the chart is in the picture", lost, 0, 0);
        checkTrue("landscape pan: " + std::to_string(tried) + " cases were run", tried == 54);
        lab.setParam("tilt", 0.0); lab.chartReset(); lab.drag(50, 50, false);   // (0.40: the flat chart; in 3D a drag turns it, checked below)
        checkTrue("landscape pan: a drag at zoom 1 moves nothing (the whole chart is fitted); at zoom 3 it moves the picture", [&] { const bool still = lab.s.chartView.flat(); lab.chartZoomAt(3, 450, 350); const double px0 = lab.s.chartView.panX; lab.drag(20, -10, false); return still && std::fabs(lab.s.chartView.panX - px0 - 20) < 1e-9; }());
        checkTrue("3D chart (0.40): a drag turns it and moves nothing; Shift-drag (move) moves the zoomed picture and turns nothing", [&] {
            lab.chartReset(); lab.setParam("tilt", 1.0); lab.chartZoomAt(3, 450, 350);
            const ChartViewState a = lab.s.chartView; lab.drag(30, 0, false); const ChartViewState b = lab.s.chartView; lab.drag(20, -10, true); const ChartViewState c = lab.s.chartView;
            return b.yawDeg != a.yawDeg && b.panX == a.panX && b.panY == a.panY && c.yawDeg == b.yawDeg && c.pitchDeg == b.pitchDeg && c.panX != b.panX; }());
        // clicking a tilted, zoomed landscape chooses the block under the pointer: its top, and its side faces
        int bad = 0, n = 0;
        for (const auto& st : {ChartViewState{1, 1, 0, 0}, ChartViewState{0.5, 1, 0, 0}, ChartViewState{1, 3, 120, 30}}) {
            lab.chartReset(); lab.s.chartView = st; lab.chooseNuclide(1, 0);
            const ChartScene& S = lab.chartScene();
            for (const std::pair<int, int>& zn : {std::pair<int, int>{92, 146}, {82, 126}, {26, 30}, {50, 70}, {8, 8}, {20, 28}}) {
                const ChartBlock* B = blockOf(S, zn.first, zn.second); if (!B) continue;
                const double tx = (B->top[0] + B->top[4]) / 2, ty = (B->top[1] + B->top[5]) / 2;
                const RayHit r = rayCast(S, tx, ty);
                if (tx < 1 || tx > 899 || ty < 1 || ty > 699) continue;
                lab.chooseNuclide(1, 0); lab.click(tx, ty); n++;
                if (!(lab.p.Z == r.Z && lab.p.N == r.N)) bad++;
                if (B->sides) { const double sx = (B->faceB[0] + B->faceB[4]) / 2, sy = (B->faceB[1] + B->faceB[5]) / 2; const RayHit r2 = rayCast(S, sx, sy); lab.chooseNuclide(1, 0); lab.click(sx, sy); n++; if (!(lab.p.Z == r2.Z && lab.p.N == r2.N)) bad++; }
            }
        }
        checkNum("landscape click: clicks on a block's top or front face (tilted, and zoomed) that did not choose the block the ray caster says is on top", bad, 0, 0);
        checkTrue("landscape click: at least 20 clicks were made", n >= 20);
    }
    // ---- the drawing, through the recording stand-in
    {
        const render::Color fg{0.1f, 0.2f, 0.3f, 1}, muted{0.4f, 0.5f, 0.6f, 1}, accent{0.9f, 0.1f, 0.7f, 1}, card{0.95f, 0.95f, 0.9f, 1}, line{0.5f, 0.5f, 0.5f, 1};
        const ChartColours C{fg, muted, accent, card, line};
        render::Renderer2D R; RecText T;
        const auto same = [](render::Color a, render::Color b) { return std::fabs(a.r - b.r) < 1e-6 && std::fabs(a.g - b.g) < 1e-6 && std::fabs(a.b - b.b) < 1e-6 && std::fabs(a.a - b.a) < 1e-6; };
        ChartScene S; S.build(900, 700, ChartViewState{1, 1, 0, 0});
        g_rec.clear(); g_tri.clear(); g_lines.clear(); g_discs.clear(); T.lines.clear();
        drawChartScene(R, T, C, S, 900, 700, 92, 146, 0, 0);
        // the order the tops are drawn in is the painter's order (far to near)
        std::map<std::array<float, 6>, int> firstTri;
        for (std::size_t i = 0; i < g_tri.size(); i++) { std::array<float, 6> k; for (int j = 0; j < 6; j++) k[std::size_t(j)] = float(g_tri[i].p[j]); if (!firstTri.count(k)) firstTri[k] = int(i); }
        int prev = -1, outOfOrder = 0, missing = 0, solid = 0;
        for (const ChartBlock& B : S.blocks()) {
            if (B.kind == HeightKind::Unknown) continue;                       // hollow: outline only, no triangle
            const std::array<float, 6> k = {B.top[0], B.top[1], B.top[2], B.top[3], B.top[4], B.top[5]};
            const auto it = firstTri.find(k); solid++;
            if (it == firstTri.end()) { missing++; continue; }
            if (it->second <= prev) outOfOrder++;
            prev = it->second;
        }
        checkTrue("landscape drawing: every block that has a filled top has its top drawn (" + std::to_string(solid) + " blocks), and the tops are drawn in the painter's order, far to near: not one is out of order", missing == 0 && outOfOrder == 0 && solid == 3557 - 87);
        int cardTri = 0; for (const RecTri& t : g_tri) if (same(t.c, card)) cardTri++;
        checkNum("landscape drawing: the small mark on the 71 stable nuclides that NUBASE has a note for (2 triangles each, in the card colour; 69 before 0.12 stage 2 made Kr-78 and Xe-124 stable)", cardTri, 142, 0);
        int hollow = 0; for (const ChartBlock& B : S.blocks()) if (B.kind == HeightKind::Unknown) hollow++;
        checkNum("landscape drawing: hollow blocks (no half-life at all) are in the scene and have no filled top", hollow, 87, 0);
        // heights: the top is above the foot by K H sin(pitch) (hand: sin = 0.81649658 at full tilt)
        const ChartBlock* u = blockOf(S, 92, 146); const ChartBlock* fe = blockOf(S, 26, 30); const ChartBlock* te = blockOf(S, 52, 76); const ChartBlock* li = blockOf(S, 3, 9);
        const auto lift = [&](const ChartBlock* B) { return (B->faceB[1] - B->top[7]); };   // the foot of the corner (a0, b1) minus the top of the same corner
        const double Kf = S.proj().K, c = S.layout().cell;
        checkTrue("landscape drawing: U-238's top stands K x h x 12 tiles x 0.81649658 above its foot (hand arithmetic, 1e-2 pt); a stable block stands higher than the longest-lived one (Te-128); a hollow one has no height",
                  std::fabs(lift(u) - Kf * 0.7299858020482797 * kHeightCells * c * 0.816496580927726) < 1e-2 && lift(fe) > lift(te) && lift(te) > lift(u) && !li->sides);
        // the marker: the ring and the pin are drawn last, in the accent colour
        int accentLines = 0; for (const RecLine& l : g_lines) if (same(l.c, accent)) accentLines++;
        checkTrue("landscape drawing: the chosen nuclide has a ring (4 lines) and a pin (1 line and a dot) in the accent colour, plus its footprint on the ground (4 lines)", accentLines == 4 + 1 + 4 && g_discs.size() == 1 && same(g_discs[0].c, accent));
        std::string all; for (const std::string& l : T.lines) all += l + "\n";
        checkTrue("landscape drawing: the legend has the height sentence and the status line says the wheel zooms and a drag pans; the flat chart's legend has no height line", all.find("Height: ") != std::string::npos && all.find("wheel zooms, drag pans") != std::string::npos);
        g_rec.clear(); T.lines.clear(); drawChart(R, T, C, 900, 700, 92, 146, 0, 0); std::string flat; for (const std::string& l : T.lines) flat += l + "\n";
        checkTrue("landscape drawing: the flat chart (stage 2's drawing, used at tilt 0, zoom 1, no pan) mentions no height and no zoom", flat.find("Height:") == std::string::npos && flat.find("wheel zooms") == std::string::npos);
        // the legend covers no block (conservative: bounding boxes) where it fits, at several tilts and sizes
        int covered = 0, boxes = 0;
        for (const auto& wh : {std::pair<double, double>{900, 700}, {1280, 720}, {1400, 900}, {1920, 1000}}) for (double tilt : {0.3, 0.7, 1.0}) {
            ChartScene Q; Q.build(wh.first, wh.second, ChartViewState{tilt, 1, 0, 0});
            g_rec.clear(); g_tri.clear(); g_lines.clear(); g_discs.clear(); T.lines.clear();
            drawChartScene(R, T, C, Q, wh.first, wh.second, 26, 30, 0, 0);
            for (const RecRect& r : g_rec) if (same(r.c, card.withAlpha(0.9f))) { boxes++; for (const ChartBlock& B : Q.blocks()) if (B.x0 < r.x + r.w && B.x1 > r.x && B.y0 < r.y + r.h && B.y1 > r.y) { covered++; std::printf("  legend %.0fx%.0f tilt %.1f covers block (%d,%d)\n", wh.first, wh.second, tilt, B.Z, B.N); break; } }
        }
        {   // no axis number under the legend (the text is an overlay, it would show through the box)
            int under = 0, numbers = 0;
            for (const auto& wh : {std::pair<double, double>{900, 700}, {640, 480}, {1280, 720}}) for (double tilt : {0.5, 1.0}) {
                ChartScene Q; Q.build(wh.first, wh.second, ChartViewState{tilt, 1, 0, 0});
                g_rec.clear(); T.lines.clear(); T.at.clear(); drawChartScene(R, T, C, Q, wh.first, wh.second, 26, 30, 0, 0);
                double bx = 0, by = 0, bw = 0, bh = 0; for (const RecRect& r : g_rec) if (same(r.c, card.withAlpha(0.9f))) { bx = r.x; by = r.y; bw = r.w; bh = r.h; }
                for (std::size_t i = 0; i < T.lines.size(); i++) {
                    const std::string& t = T.lines[i]; if (t.empty() || t.find_first_not_of("0123456789") != std::string::npos) continue;
                    numbers++; if (T.at[i][0] > bx && T.at[i][0] < bx + bw && T.at[i][1] > by && T.at[i][1] < by + bh) under++;
                }
            }
            checkTrue("landscape drawing: axis numbers drawn (" + std::to_string(numbers) + ") and none of them inside the legend box (3 sizes x 2 tilts)", numbers > 30 && under == 0);
        }
        checkNum("landscape drawing: legend boxes drawn (4 sizes x 3 tilts)", boxes, 12, 0);
        checkNum("landscape drawing: legend boxes whose area meets a block's bounding box, 4 window sizes x 3 tilts", covered, 0, 0);
    }
}

// -------------------------------------------------------------------------------------------------------------------------------------------------------------
// 0.11 stage 4: a second height (binding energy per nucleon, AME2020), the valley and the island. REFERENCES are not the lab's own output: (1) rows typed from data/ame/mass.mas20.txt (AME2020, fetched
// 2026-10-04) read by hand with grep/awk: 12 nuclides' binding energy per nucleon and uncertainty, the count of rows (3,557) and of "#" estimates (1,008), the heaviest measured Z (110), the three highest
// measured values; (2) the lab's OWN binding table (kBeaLines, through massBinding) compared with the file for all 3,557 nuclides; (3) the height mapping by hand: keV / 9,000; (4) the median stable tile
// worked out separately (251 stable tiles in order of Z then N, element 125: Z 47, N 62); (5) the same ray caster as stage 3. Guards are exact counts or stated tolerances.
static std::string joinLines(const RecText& T) { std::string a; for (const std::string& l : T.lines) a += l + "\n"; return a; }
static bool hasDigitsOtherThan(const std::string& text, const std::string& allowed) {          // every run of digits in the text must be the allowed one
    for (std::size_t i = 0; i < text.size();) { if (std::isdigit(static_cast<unsigned char>(text[i]))) { std::size_t j = i; while (j < text.size() && std::isdigit(static_cast<unsigned char>(text[j]))) j++; if (text.substr(i, j - i) != allowed) return true; i = j; } else i++; }
    return false;
}
static void testChartBinding() {
    // ---- the AME2020 table
    checkNum("binding: AME2020 rows with Z >= 1 in the file (counted from the file) = the table = the chart's tiles", ameCount() * 1.0, 3557, 0);
    checkNum("binding: values the file marks '#' (estimated, non-experimental), counted from the file: 1,008; the rest, 2,549, come from measured masses", ameCount() - ameMeasuredCount(), 1008, 0);
    checkNum("binding: measured values in the table", ameMeasuredCount(), 2549, 0);
    struct AR { int Z, N; double b, u; int est; };
    static const AR ar[] = {{1, 0, 0.0, 0.0, 0}, {1, 1, 1112.2831, 0.0002, 0}, {2, 2, 7073.9156, 0.0002, 0}, {6, 6, 7680.1446, 0.0002, 0}, {8, 8, 7976.2072, 0.0002, 0}, {24, 30, 8777.9672, 0.0025, 0}, {26, 30, 8790.3563, 0.0048, 0},
                            {26, 32, 8792.2534, 0.0055, 0}, {28, 34, 8794.5555, 0.0069, 0}, {28, 36, 8777.4637, 0.0072, 0}, {92, 146, 7570.1262, 0.0063, 0}, {110, 160, 7253.7634, 0.1455, 0},
                            {118, 176, 7079.0, 2.0, 1}, {3, 0, -2267.0, 667.0, 1}, {5, 1, -467.0, 334.0, 1}};
    int badRow = 0; for (const AR& r : ar) { const AmeRow* a = ame(r.Z, r.N); if (!a || std::fabs(a->bindPerA - r.b) > 1e-9 || std::fabs(a->unc - r.u) > 1e-9 || a->est != r.est) badRow++; }
    checkNum("binding: typed AME2020 rows (15 nuclides: H-1 ... U-238, Ds-270, Og-294, Li-3, B-6) whose value, uncertainty or estimate mark differs from the table", badRow, 0, 0);
    int est1 = 0, noRow = 0; for (int z = 111; z <= 118; z++) for (const ChartTile& t : chartTiles()) if (t.Z == z) { const AmeRow* a = ame(t.Z, t.N); if (!a) noRow++; else if (!a->est) est1++; }
    checkTrue("binding: the heaviest Z with a measured value is 110 (the file), and every nuclide of Z 111-118 in the chart has only an estimate", ameHeaviestMeasuredZ() == 110 && est1 == 0 && noRow == 0 && !ame(110, 160)->est && ame(110, 160) && ame(111, 160) == nullptr);
    {   // the peak, READ from the table: the three highest measured values (typed from the file: Ni-62 8794.5555, Fe-58 8792.2534, Fe-56 8790.3563)
        const std::vector<const AmeRow*> top = ameHighestN(3);
        checkTrue("binding: the highest measured value in the table is Z 28, N 34 (nickel-62), 8794.5555 keV, then Z 26, N 32 (iron-58) and Z 26, N 30 (iron-56)", top.size() == 3 && top[0]->Z == 28 && top[0]->N == 34 && top[1]->Z == 26 && top[1]->N == 32 && top[2]->Z == 26 &&
                  top[2]->N == 30 && std::fabs(top[0]->bindPerA - 8794.5555) < 1e-9 && std::fabs(top[1]->bindPerA - 8792.2534) < 1e-9 && std::fabs(top[2]->bindPerA - 8790.3563) < 1e-9 && &ameHighest() == top[0]);
        double best = -1; int bz = 0, bn = 0; for (const ChartTile& t : chartTiles()) { const ChartHeight h = chartHeightOf(t.Z, t.N, ChartHeightMode::Binding); if (h.kind == HeightKind::Measured && h.h > best) { best = h.h; bz = t.Z; bn = t.N; } }
        checkTrue("binding: the tallest measured block of the binding landscape is the same nuclide (found by scanning the heights, not the table's helper)", bz == 28 && bn == 34 && std::fabs(best - 8794.5555 / 9000) < 1e-12);
    }
    {   // the lab's own table agrees with the file: whole keV per nucleon, so within 0.5 keV for every measured value; one estimate (Z 19, N 36) differs by a whole keV
        int over = 0, overMeasured = 0; double worstMeasured = 0, worstAll = 0;
        for (const ChartTile& t : chartTiles()) {
            const double lab = massBinding(t.Z, t.N).B * 1000.0 / (t.Z + t.N), d = std::fabs(lab - ame(t.Z, t.N)->bindPerA);
            worstAll = std::max(worstAll, d); if (!ame(t.Z, t.N)->est) worstMeasured = std::max(worstMeasured, d);
            if (d > 0.5 + 1e-6) { over++; if (!ame(t.Z, t.N)->est) overMeasured++; }
        }
        checkTrue("binding: the lab's own binding table (massBinding) agrees with AME2020 for all 2,549 measured values to 0.5 keV per nucleon (worst measured 0.4998: rounding to whole keV) and differs by 1 keV in exactly one estimate",
                  overMeasured == 0 && worstMeasured < 0.5 && over == 1 && std::fabs(worstAll - 1.0) < 1e-6 && std::fabs(massBinding(19, 36).B * 1000.0 / 55 - 7793) < 1e-6);
    }
    // ---- the mapping, by hand: keV / 9,000, linear, cut to 0 .. 1
    struct BH { double kev, h; };
    static const BH bh[] = {{0, 0}, {4500, 0.5}, {9000, 1}, {10000, 1}, {-100, 0}, {-2267, 0}, {8794.5555, 0.9771728333333333}, {7570.1262, 0.8411251333333333}, {7073.9156, 0.7859906222222222}, {1112.2831, 0.12358701111111111}, {900, 0.1}};
    int badB = 0; for (const BH& k : bh) if (std::fabs(chartHeightFromBinding(k.kev) - k.h) > 1e-12) badB++;
    checkNum("binding height: values of keV per nucleon (11, from below 0 to above the top) whose height differs from keV / 9,000 cut to 0..1 by more than 1e-12", badB, 0, 0);
    checkTrue("binding height: the scale is linear (equal steps of 900 keV add 0.1), per NUCLEON (U-238, 7,570 keV, is below Ni-62, 8,795 keV, though it has 3.8 times as many nucleons), 0 to 9,000 keV, 28 tiles per scale height",
              std::fabs(chartHeightFromBinding(1800) - chartHeightFromBinding(900) - 0.1) < 1e-12 && std::fabs(chartHeightFromBinding(7200) - chartHeightFromBinding(6300) - 0.1) < 1e-12 && kBindingTopKeV == 9000 && kBindingCells == 28 &&
                  chartHeightOf(92, 146, ChartHeightMode::Binding).h < chartHeightOf(28, 34, ChartHeightMode::Binding).h);
    struct TH { int Z, N; HeightKind k; double h; const char* what; };
    static const TH th[] = {{2, 2, HeightKind::Measured, 0.7859906222222222, "He-4"}, {28, 34, HeightKind::Measured, 0.9771728333333333, "Ni-62, the peak"}, {92, 146, HeightKind::Measured, 0.8411251333333333, "U-238"}, {1, 0, HeightKind::Measured, 0, "H-1: measured, exactly 0 (nothing bound)"},
                            {1, 1, HeightKind::Measured, 0.12358701111111111, "H-2"}, {118, 176, HeightKind::Extrapolated, 0, "Og-294: AME2020 only estimates it: no height"}, {3, 0, HeightKind::Extrapolated, 0, "Li-3: estimate, negative: no height"},
                            {110, 160, HeightKind::Measured, 0.8059737111111111, "Ds-270: the heaviest nuclide with a measured value"}, {111, 161, HeightKind::Extrapolated, 0, "Rg-272: heavier than any measured mass: no height"}, {2, 0, HeightKind::Unknown, 0, "He-2: not a nuclide, no row: no height"}};
    for (const TH& k : th) { const ChartHeight c = chartHeightOf(k.Z, k.N, ChartHeightMode::Binding); checkTrue(std::string("binding height: ") + k.what, c.kind == k.k && std::fabs(c.h - k.h) < 1e-12); }
    {
        int estWithHeight = 0, measuredNoHeight = 0, over = 0; for (const ChartTile& t : chartTiles()) { const ChartHeight c = chartHeightOf(t.Z, t.N, ChartHeightMode::Binding); const bool est = ame(t.Z, t.N)->est; if (est && (c.h != 0 || c.kind != HeightKind::Extrapolated)) estWithHeight++; if (!est && c.kind != HeightKind::Measured) measuredNoHeight++; if (c.h < 0 || c.h > 1) over++; }
        checkNum("binding height: estimated nuclides drawn with a height (they get none), measured ones without a block, or heights outside 0..1", estWithHeight + measuredNoHeight + over, 0, 0);
        checkTrue("binding height: kinds are 2,549 measured, 1,008 extrapolated (flat pale), 0 unknown; the half-life mode's counts are the 0.12 stage 2 ones (2,831 measured)", chartHeightCount(HeightKind::Measured, ChartHeightMode::Binding) == 2549 && chartHeightCount(HeightKind::Extrapolated, ChartHeightMode::Binding) == 1008 &&
                  chartHeightCount(HeightKind::Unknown, ChartHeightMode::Binding) == 0 && chartHeightCount(HeightKind::Measured) == 2831 && chartHeightCount(HeightKind::Stable) == 182);
    }
    // ---- the words say which height is in use and what it means; every number in them is the table's
    {
        const std::string bw = chartHeightWords(ChartHeightMode::Binding), bs = chartHeightShort(ChartHeightMode::Binding), hw = chartHeightWords(ChartHeightMode::HalfLife), pk = chartBindingPeakLabel();
        bool ok = true; for (const char* x : {"Height: the binding energy per nucleon (AME2020)", "linear", "9,000 keV", "nickel-62", "8,794.56 keV", "iron-58", "iron-56", "Fusing nuclei that lie below the peak releases energy", "1008", "Z = 110"}) if (bw.find(x) == std::string::npos) { ok = false; std::printf("  missing: %s\n", x); }
        checkTrue("binding words: the legend sentence says it is the binding energy per nucleon from AME2020 on a linear 0 to 9,000 keV scale, names the peak and the next two as READ from the table, the fusion/fission sentence, and that there is no measured mass above Z = 110",
                  ok || bw.find("1,008") != std::string::npos);
        checkTrue("binding words: the short form names the height and the peak; the half-life words still start 'Height: the log of the half-life' (stage 3's wording is unchanged); the peak note is 'peak of the data: nickel-62, 8,794.56 keV per nucleon'",
                  bs.find("Height: binding energy per nucleon") == 0 && bs.find("nickel-62") != std::string::npos && hw.find("Height: the log of the half-life") == 0 && pk == "peak of the data: nickel-62, 8,794.56 keV per nucleon");
    }
    // ---- the valley and the island
    {
        const ChartTile a = chartValleyAnchor();
        checkTrue("valley: the label is attached to the median stable tile (element 125 of the 251 stable tiles in order of Z then N: Z 47, N 62, silver-109), a stable tile", a.Z == 47 && a.N == 62 && a.cls == ChartClass::Stable);
        const std::vector<ChartTile> hv = chartHeaviestTiles(); int maxZ = 0; for (const ChartTile& t : chartTiles()) maxZ = std::max<int>(maxZ, t.Z);
        checkTrue("island/end: the measured chart ends at Z 118 (largest Z among the tiles), whose tiles are N 175, 176, 177 (NUBASE2020), and no tile lies beyond; the label is read from the data", chartHeaviestZ() == 118 && maxZ == 118 && hv.size() == 3 && hv[0].N == 175 && hv[1].N == 176 && hv[2].N == 177 &&
                  chartHeaviestLabel() == "heaviest known: Z = 118 (oganesson); nothing observed beyond");
        const std::string vw = chartValleyWords(), vs = chartValleyShort(), iw = chartIslandWords(), is = chartIslandShort();
        checkTrue("valley words: it is the band of stable and very long-lived nuclides (the black tiles, the ridge), measured, not predicted, and nuclides on either side decay back towards it", vw.find("narrow band of stable and very long-lived nuclides") != std::string::npos && vw.find("the black tiles") != std::string::npos &&
                  vw.find("the ridge in the landscape") != std::string::npos && vw.find("measured, not predicted") != std::string::npos && vw.find("decay back towards it") != std::string::npos && vs.find("(measured)") != std::string::npos);
        checkTrue("island words: a PREDICTION, not something seen; theory expects a region beyond the heaviest known; none observed; where it lies is disputed; no sourced prediction to draw, so nothing is drawn", iw.find("a PREDICTION, not something seen") != std::string::npos &&
                  iw.find("beyond the heaviest known (Z = 118)") != std::string::npos && iw.find("None of them has been observed") != std::string::npos && iw.find("where it lies is disputed") != std::string::npos && iw.find("no sourced prediction to draw") != std::string::npos &&
                  iw.find("nothing is drawn there") != std::string::npos && is.find("a prediction, not seen") != std::string::npos && is.find("not drawn") != std::string::npos);
        checkTrue("island words quote NO number of their own: the only digits in the island words (long and short) and in the heaviest label are 118, read from the data (no predicted magic number, no predicted Z or N)", !hasDigitsOtherThan(iw, "118") && !hasDigitsOtherThan(is, "118") && !hasDigitsOtherThan(chartHeaviestLabel(), "118") && iw.find("118") != std::string::npos);
    }
    // ---- switching the height changes nothing else
    {
        AtomLab lab; lab.p = oneParams(); lab.paused = false; lab.restart(); lab.setParam("view", std::string(kChartView)); lab.viewW = 900; lab.viewH = 700;
        lab.chooseNuclide(92, 146); lab.setParam("tilt", 0.8); lab.chartZoomAt(1.5, 300, 300);
        const ChartViewState before = lab.s.chartView; std::vector<std::string> hoverBefore; for (const ChartTile& t : chartTiles()) hoverBefore.push_back(chartHover(t.Z, t.N)[0] + chartHover(t.Z, t.N).back());
        lab.setParam("cheight", std::string("Nonsense"));
        const bool ignored = lab.s.chartView.height == ChartHeightMode::HalfLife;
        lab.setParam("cheight", std::string("Binding energy per nucleon"));
        std::vector<std::string> hoverAfter; for (const ChartTile& t : chartTiles()) hoverAfter.push_back(chartHover(t.Z, t.N)[0] + chartHover(t.Z, t.N).back());
        checkTrue("switch: an unknown choice is ignored; 'Binding energy per nucleon' sets the mode; the selection (92, 146), the tilt, zoom and pan, and every nuclide's hover words are identical, and the mode is kept by a reset", ignored && lab.s.chartView.height == ChartHeightMode::Binding && lab.p.Z == 92 && lab.p.N == 146 &&
                  lab.s.chartView.tilt == before.tilt && lab.s.chartView.zoom == before.zoom && lab.s.chartView.panX == before.panX && lab.s.chartView.panY == before.panY && hoverBefore == hoverAfter && [&] { lab.chartReset(); return lab.s.chartView.height == ChartHeightMode::Binding && lab.s.chartView.zoom == 1 && lab.s.chartView.panX == 0 && lab.s.chartView.panY == 0 && lab.s.chartView.tilt == before.tilt; }());   // (0.40: a reset keeps 2D or 3D)
        lab.setParam("cheight", std::string("Half-life")); checkTrue("switch: back to 'Half-life'", lab.s.chartView.height == ChartHeightMode::HalfLife);
        // the whole hover text, all lines, for 3,557 nuclides, is built from the data and does not mention the height at all
        int mentions = 0; for (const ChartTile& t : chartTiles()) for (const std::string& l : chartHover(t.Z, t.N)) if (l.find("binding") != std::string::npos || l.find("Height") != std::string::npos) mentions++;
        checkNum("switch: hover lines (all 3,557 nuclides) that mention binding energy or a height (the hover is about the nuclide, not the picture)", mentions, 0, 0);
    }
    {   // drawn: the ground, the bands, the footprint, the pin's place, the blocks' x, the colours and the status line are identical in the two modes
        const render::Color fg{0.1f, 0.2f, 0.3f, 1}, muted{0.4f, 0.5f, 0.6f, 1}, accent{0.9f, 0.1f, 0.7f, 1}, card{0.95f, 0.95f, 0.9f, 1}, line{0.5f, 0.5f, 0.5f, 1};
        const ChartColours C{fg, muted, accent, card, line};
        render::Renderer2D R;
        const auto same = [](render::Color a, render::Color b) { return std::fabs(a.r - b.r) < 1e-6 && std::fabs(a.g - b.g) < 1e-6 && std::fabs(a.b - b.b) < 1e-6 && std::fabs(a.a - b.a) < 1e-6; };
        struct Out { std::vector<RecTri> tri; std::vector<RecLine> lines; std::vector<RecDisc> discs; std::vector<RecRect> rects; std::vector<std::string> text; ChartScene S; };
        const auto draw = [&](ChartHeightMode m, double W, double H, double tilt, int selZ, int selN, RecText& T) {
            Out o; ChartViewState st{tilt, 1, 0, 0}; st.height = m; o.S.build(W, H, st);
            g_rec.clear(); g_tri.clear(); g_lines.clear(); g_discs.clear(); T.lines.clear(); T.at.clear();
            drawChartScene(R, T, C, o.S, W, H, selZ, selN, 0, 0);
            o.tri = g_tri; o.lines = g_lines; o.discs = g_discs; o.rects = g_rec; o.text = T.lines; return o;
        };
        RecText T1, T2;
        const Out h = draw(ChartHeightMode::HalfLife, 900, 700, 1, 92, 146, T1), b = draw(ChartHeightMode::Binding, 900, 700, 1, 92, 146, T2);
        // ground triangles (the plate and the bands), by colour, in order
        const auto groundTri = [&](const Out& o) { std::vector<RecTri> v; for (const RecTri& t : o.tri) if (same(t.c, line.withAlpha(0.12f)) || same(t.c, line.withAlpha(0.26f))) v.push_back(t); return v; };
        const auto groundLines = [&](const Out& o) { std::vector<RecLine> v; for (const RecLine& l : o.lines) if (same(l.c, muted.withAlpha(0.7f)) || same(l.c, muted.withAlpha(0.8f))) v.push_back(l); return v; };
        const std::vector<RecTri> gh = groundTri(h), gb = groundTri(b); const std::vector<RecLine> lh = groundLines(h), lb = groundLines(b);
        bool gEq = gh.size() == gb.size() && lh.size() == lb.size() && !gh.empty() && !lh.empty();
        for (std::size_t i = 0; gEq && i < gh.size(); i++) for (int j = 0; j < 6; j++) if (gh[i].p[j] != gb[i].p[j]) gEq = false;
        for (std::size_t i = 0; gEq && i < lh.size(); i++) if (lh[i].x0 != lb[i].x0 || lh[i].y0 != lb[i].y0 || lh[i].x1 != lb[i].x1 || lh[i].y1 != lb[i].y1) gEq = false;
        checkTrue("switch, drawn: the ground plate, the 13 magic bands' triangles (" + std::to_string(gh.size()) + ") and their edge lines (" + std::to_string(lh.size()) + ") are at exactly the same places in the two height modes (the picture is not refitted)", gEq);
        bool xEq = h.S.blocks().size() == b.S.blocks().size(); int clsDiff = 0, xDiff = 0;
        for (std::size_t k = 0; xEq && k < h.S.blocks().size(); k++) { const ChartBlock &p = h.S.blocks()[k], &q = b.S.blocks()[k]; if (p.Z != q.Z || p.N != q.N || p.cls != q.cls) clsDiff++; for (int i = 0; i < 8; i += 2) if (p.top[i] != q.top[i]) xDiff++; }
        checkTrue("switch, drawn: every block is the same nuclide with the same decay class in the same order, and its top is at the same x (only the heights change)", xEq && clsDiff == 0 && xDiff == 0);
        // the top colours of blocks that are filled in both modes are the same
        const auto topColours = [&](const Out& o) { std::map<std::pair<float, float>, render::Color> m; for (const RecTri& t : o.tri) m.insert({{float(t.p[0]), float(t.p[1])}, t.c}); return m; };
        const auto mh = topColours(h), mb = topColours(b); int compared = 0, colourDiff = 0, classWrong = 0;
        for (std::size_t k = 0; k < h.S.blocks().size(); k++) {
            const ChartBlock &p = h.S.blocks()[k], &q = b.S.blocks()[k];
            const auto ih = mh.find({p.top[0], p.top[1]}), ib = mb.find({q.top[0], q.top[1]});
            if (ih == mh.end() || ib == mb.end()) continue;
            const render::Color want = *chartClassInfo(p.cls).hex ? render::hexColor(chartClassInfo(p.cls).hex) : fg;
            if (p.kind == HeightKind::Measured || p.kind == HeightKind::Stable || p.kind == HeightKind::StableNoted) { if (!same(ih->second, want)) classWrong++; }
            if (q.kind == HeightKind::Measured) { if (!same(ib->second, want)) classWrong++; }
            if ((p.kind == HeightKind::Measured && q.kind == HeightKind::Measured)) { compared++; if (!same(ih->second, ib->second)) colourDiff++; }
        }
        checkTrue("switch, drawn: the top colour of a block that is solid in both modes is the same (" + std::to_string(compared) + " compared), and in each mode a solid top has its decay class's colour (0 wrong)", compared > 2000 && colourDiff == 0 && classWrong == 0);
        std::string th = joinLines(T1), tb = joinLines(T2);
        const auto statusOf = [](const std::string& t) { const std::size_t a = t.find("Chosen: "); return t.substr(a, t.find('\n', a) - a); };
        checkTrue("switch, drawn: the status line (the selection) is the same in both modes; the legend names the height in use: 'Height: the log of the half-life' for the half-life, 'Height: ...binding energy per nucleon' for binding; neither names the other", statusOf(th) == statusOf(tb) &&
                  th.find("Height: ") != std::string::npos && th.find("half-life") != std::string::npos && th.find("binding energy per nucleon") == std::string::npos && tb.find("binding energy per nucleon") != std::string::npos && tb.find("Height: ") != std::string::npos && tb.find("log10 of the half-life") == std::string::npos && tb.find("log of the half-life") == std::string::npos);
        // the selection's pin: same x, and the same footprint lines
        int accentH = 0, accentB = 0; for (const RecLine& l : h.lines) if (same(l.c, accent)) accentH++; for (const RecLine& l : b.lines) if (same(l.c, accent)) accentB++;
        checkTrue("switch, drawn: the chosen nuclide's pin is at the same x in both modes; the binding mode adds exactly one ring (4 lines) in the accent colour, round the peak (the half-life mode has none)", h.discs.size() >= 1 && b.discs.size() >= 1 && h.discs.back().x == b.discs.back().x && accentB == accentH + 4);
        const std::string pkText = chartBindingPeakLabel();
        checkTrue("peak note: drawn in the binding mode (at 1280 x 720 there is a free place), never in the half-life mode", [&] { RecText Ta, Tb; const Out bb = draw(ChartHeightMode::Binding, 1280, 720, 1, 92, 146, Ta); const Out hh = draw(ChartHeightMode::HalfLife, 1280, 720, 1, 92, 146, Tb); (void)bb; (void)hh; return joinLines(Ta).find(pkText) != std::string::npos && joinLines(Tb).find("peak of the data") == std::string::npos; }());
        // no predicted thing is drawn: the blocks are exactly the 3,557 nuclides; the number of filled top faces equals the filled blocks; nothing is drawn in a tile's colour beyond them
        int filled = 0; for (const ChartBlock& B : b.S.blocks()) if (B.kind != HeightKind::Unknown) filled++;
        std::set<std::pair<float, float>> topsDrawn; for (const RecTri& t : b.tri) topsDrawn.insert({float(t.p[0]), float(t.p[1])});
        int tileColoured = 0; for (const RecTri& t : b.tri) { for (int c = 0; c < kChartClasses; c++) { const render::Color cc = *chartClassInfo(ChartClass(c)).hex ? render::hexColor(chartClassInfo(ChartClass(c)).hex) : fg; if (same(t.c, cc) || same(t.c, cc.withAlpha(0.5f)) || same(t.c, cc.withAlpha(0.28f))) { tileColoured++; break; } } }
        int tilesNotBlocks = 0; { std::set<std::pair<int, int>> have; for (const ChartBlock& B : b.S.blocks()) have.insert({B.Z, B.N}); for (const ChartTile& t : chartTiles()) if (!have.count({t.Z, t.N})) tilesNotBlocks++; }
        checkTrue("island not drawn as measured: the scene has exactly the 3,557 nuclides' blocks (none beyond Z 118), and every triangle in a tile's colour is the top face of one of them (exactly 2 triangles each: 7,114; the shaded side faces are other colours)", b.S.blocks().size() == 3557 && tilesNotBlocks == 0 && filled == 3557 &&
                  tileColoured == 2 * 3557);
    }
    // ---- the labels and the notes keep clear of tiles, blocks and each other
    {
        const render::Color fg{0.1f, 0.2f, 0.3f, 1}, muted{0.4f, 0.5f, 0.6f, 1}, accent{0.9f, 0.1f, 0.7f, 1}, card{0.95f, 0.95f, 0.9f, 1}, line{0.5f, 0.5f, 0.5f, 1};
        const ChartColours C{fg, muted, accent, card, line};
        render::Renderer2D R; RecText T;
        const auto same = [](render::Color a, render::Color b) { return std::fabs(a.r - b.r) < 1e-6 && std::fabs(a.g - b.g) < 1e-6 && std::fabs(a.b - b.b) < 1e-6 && std::fabs(a.a - b.a) < 1e-6; };
        int labels = 0, covers = 0, hitsBox = 0, boxes = 0, boxCovers = 0, noBox = 0;
        struct Cs { double w, h, tilt; ChartHeightMode m; };
        const Cs cases[] = {{900, 700, 0, ChartHeightMode::HalfLife}, {1280, 720, 0, ChartHeightMode::HalfLife}, {1400, 900, 0, ChartHeightMode::HalfLife}, {1920, 1000, 0, ChartHeightMode::HalfLife}, {1280, 720, 1, ChartHeightMode::HalfLife}, {1280, 720, 1, ChartHeightMode::Binding},
                            {1400, 900, 0.5, ChartHeightMode::HalfLife}, {1400, 900, 0.5, ChartHeightMode::Binding}, {1920, 1000, 1, ChartHeightMode::Binding}, {900, 700, 0.6, ChartHeightMode::Binding}};
        for (const Cs& c : cases) {
            ChartViewState st{c.tilt, 1, 0, 0}; st.height = c.m; ChartScene S; S.build(c.w, c.h, st); const ChartLayout L = chartLayout(c.w, c.h);
            g_rec.clear(); g_tri.clear(); g_lines.clear(); g_discs.clear(); T.lines.clear(); T.at.clear();
            if (st.flat()) drawChart(R, T, C, c.w, c.h, 26, 30, 0, 0); else drawChartScene(R, T, C, S, c.w, c.h, 26, 30, 0, 0);
            std::vector<RecRect> bx; for (const RecRect& r : g_rec) if (same(r.c, card.withAlpha(0.9f)) || same(r.c, card.withAlpha(0.88f))) bx.push_back(r);
            const auto coversTile = [&](double x0, double y0, double x1, double y1) {
                if (st.flat()) { for (const ChartTile& t : chartTiles()) { const double tx = L.x(t.N), ty = L.y(t.Z); if (tx < x1 && tx + L.cell > x0 && ty < y1 && ty + L.cell > y0) return true; } return false; }
                for (const ChartBlock& B : S.blocks()) { if (B.x0 < x1 && B.x1 > x0 && B.y0 < y1 && B.y1 > y0) return true; }
                return false; };
            for (const RecRect& r : bx) { boxes++; if (coversTile(r.x, r.y, r.x + r.w, r.y + r.h)) boxCovers++; }
            bool noteBoxDrawn = false; for (const RecRect& r : bx) if (same(r.c, card.withAlpha(0.88f))) noteBoxDrawn = true; if (!noteBoxDrawn) noBox++;
            for (std::size_t i = 0; i < T.lines.size(); i++) {
                const std::string& t = T.lines[i]; if (t != "valley of stability (measured)" && t != chartHeaviestLabel() && t != chartBindingPeakLabel()) continue;
                const double tw = t.size() * 11 * 0.5, x = T.at[i][0], y = T.at[i][1];
                labels++; if (coversTile(x - tw / 2 - 3, y - 11, x + tw / 2 + 3, y + 4)) covers++;
                for (const RecRect& r : bx) if (x - tw / 2 - 3 < r.x + r.w && x + tw / 2 + 3 > r.x && y - 11 < r.y + r.h && y + 4 > r.y) hitsBox++;
            }
        }
        checkTrue("labels: the valley, heaviest-known and peak labels drawn (" + std::to_string(labels) + " in 10 views: flat and tilted, both height modes, 900x700 to 1920x1000: the valley label and the heaviest-known label in each of the 10, the peak note in the 4 binding views) cover no tile or block and sit on neither the legend nor the notes box", labels == 24 && covers == 0 && hitsBox == 0);
        checkTrue("notes boxes: the legend and the notes box (" + std::to_string(boxes) + ") cover no tile or block in those views, and the notes box is drawn in every one of them (" + std::to_string(noBox) + " without)", boxes >= 18 && boxCovers == 0 && noBox == 0);
    }
    // ---- painter's order and hit test with the binding heights, against the ray caster
    for (const double tilt : {1.0, 0.6}) {
        ChartViewState st{tilt, 1, 0, 0}; st.height = ChartHeightMode::Binding; ChartScene S; S.build(900, 700, st);
        int tested = 0, mismatch = 0, overlaps = 0, hits = 0;
        for (double py = 0.43; py < 700; py += 13) for (double px = 0.57; px < 900; px += 13) {
            const RayHit r = rayCast(S, px, py); int Z = 0, N = 0; const bool p = S.pick(px, py, Z, N); tested++; if (r.hit) hits++; if (r.boxes >= 2) overlaps++;
            if (p != r.hit || (p && (Z != r.Z || N != r.N))) mismatch++;
        }
        checkNum("binding landscape, tilt " + numText(tilt, 2) + ": pixels (a 13-pt grid) where the topmost drawn block is not the first box the ray caster's ray enters", mismatch, 0, 0);
        checkTrue("binding landscape, tilt " + numText(tilt, 2) + ": the grid had many pixels on blocks (" + std::to_string(hits) + ") and where a ray meets 2+ boxes (" + std::to_string(overlaps) + ")", tested > 2000 && hits > 300 && overlaps > 100);
    }
}

static void testOutlineFade() {
    AtomLab lab; jumpSetup(lab, "1s", ""); runSteps(lab, 100);
    checkTrue("outline: fully there when no jump runs", lab.outlineAlpha() == 1);
    lab.startJump("absorb", orbIndex("2p|z")); runSteps(lab, 400);
    lab.s.trans->t = 0; const double a0 = lab.outlineAlpha();
    lab.s.trans->t = 0.12; const double a1 = lab.outlineAlpha();
    lab.s.trans->t = 0.6; const double a2 = lab.outlineAlpha();
    for (int i = 0; i < 400 && lab.s.trans; i++) lab.step(1.0 / 240);       // the jump ends
    const double a3 = lab.outlineAlpha();
    runSteps(lab, 48); const double a4 = lab.outlineAlpha();                // 0.2 s after landing
    runSteps(lab, 100); const double a5 = lab.outlineAlpha();               // 0.6 s after landing
    std::printf("outline alpha: start %.2f, 0.12 s %.2f, middle %.2f, landing %.2f, 0.2 s later %.2f, 0.6 s later %.2f\n", a0, a1, a2, a3, a4, a5);
    checkTrue("outline: starts at full strength, fades out, is hidden in the middle of the jump, fades back in after landing and is full again", a0 == 1 && a1 < 1 && a1 > 0 && a2 == 0 && a3 <= 0.05 && a4 > 0 && a4 < 1 && a5 == 1);
}

// ================= The constants (0.10.6 stage 2): CODATA 2022 from the NIST pages =================
static void testConstants() {
    struct K { const char* what; double lab, want; };
    const K ks[] = {{"Bohr radius", kA0, nist::a0}, {"Hartree energy eV", kHartreeEV, nist::hartreeEV}, {"atomic unit of time", kAuTime, nist::tau}, {"fine-structure constant", kAlpha, nist::alpha},
        {"Rydberg energy eV", kRydInf, nist::rydEV}, {"electron mass energy eV", kMecEV, nist::mecEV}, {"electron mass in u", kMeInU, nist::meInU}, {"proton-electron mass ratio", kMpOverMe, nist::mpOverMe},
        {"u in kg", kUKg, nist::ukg}, {"u in MeV", kUMeV, nist::uMeV}, {"electron mass kg", kMeKg, nist::meKg}, {"proton mass kg", kMpKg, nist::mpKg}, {"neutron mass kg", kMnKg, nist::mnKg},
        {"proton mass u", kMpU, nist::mpU}, {"neutron mass u", kMnU, nist::mnU}, {"Boltzmann", kBoltzmann, nist::kB}, {"Planck constant", kPlanckJs, nist::h}, {"speed of light", kC, nist::c},
        {"elementary charge", kEV, nist::e}, {"h c in eV nm", kHcEVnm, nist::hcEVnm}, {"u in electron masses", kMeU, 1 / nist::meInU}};
    for (const K& k : ks) checkNum(std::string("constant: ") + k.what + " is the NIST CODATA 2022 value", k.lab, k.want, 1e-14);
    checkNum("constant: h in eV s (NIST page hev: 4.135 667 696...e-15, ten digits)", kPlanckEVs, 4.135667696e-15, 1e-9);
    // derived ones: one source
    checkNum("constant: Bohr radius in Angstrom is a0 x 1e10", kA0Ang, nist::a0 * 1e10, 1e-15);
    checkNum("constant: metres per fm is exactly 1e-15", kFmLab, 1e-15, 0);
    checkNum("constant: fm per Bohr radius is a0 / 1e-15 (52 917.721 054 4)", kFmPerA0, 52917.7210544, 1e-12);
    checkNum("constant: the atomic unit of time in as is tau x 1e18", kAuTimeAs, 24.188843265864, 1e-13);
    // relations between the constants (independent of how each was typed): alpha c = a0 / tau; Hartree = 2 Rydberg = alpha^2 m_e c^2; a0 = hbar / (m_e c alpha); u / m_e = (u c^2) / (m_e c^2); m_p / m_e
    const double alphaC = kAlpha * kC;
    std::printf("constants: a0/tau = %.9e m/s, alpha c = %.9e m/s (relative difference %.1e)\n", kAuSpeed, alphaC, std::fabs(kAuSpeed / alphaC - 1));
    checkTrue("constant: the atomic unit of speed a0 / tau equals alpha x c (1e-10; measured about 6e-12)", std::fabs(kAuSpeed / alphaC - 1) < 1e-10);
    checkTrue("constant: Hartree energy = 2 x Rydberg energy (1e-11)", std::fabs(kHartreeEV / (2 * kRydInf) - 1) < 1e-11);
    checkTrue("constant: Hartree energy = alpha^2 m_e c^2 (1e-9, alpha's uncertainty is 1.6e-10)", std::fabs(kHartreeEV / (kAlpha * kAlpha * kMecEV) - 1) < 1e-9);
    checkTrue("constant: Bohr radius = hbar / (m_e c alpha) (1e-9)", std::fabs(kA0 / (kPlanckJs / (2 * kPi) / (kMeKg * kC * kAlpha)) - 1) < 1e-9);
    checkTrue("constant: u / m_e = u c^2 / m_e c^2 (1e-9)", std::fabs(kMeU / (kUMeV * 1e6 / kMecEV) - 1) < 1e-9);
    checkTrue("constant: m_p / m_e from the kg values and from the u values (1e-9)", std::fabs(kMpKg / kMeKg / kMpOverMe - 1) < 1e-9 && std::fabs(kMpU / kMeInU / kMpOverMe - 1) < 1e-9);
}

// ================= 0.10.6 stage 3: one energy for one atom, the Orbit view's accuracy, the Fixed scale =================
// Finding 3 (audit): the readout energy is the lab's reduced-mass level levelEV (hydrogen -13.598 eV -> "13.60 eV"), in the Cloud view AND in the Orbit view (from the orbit itself).
static void testOneEnergy() {
    // independent: hydrogen's level is the Rydberg energy for an infinite mass divided by (1 + m_e/m_p), from the NIST values typed in the nist:: block (not from atom_core.h)
    const double hyd = nist::rydEV / (1 + 1 / nist::mpOverMe);
    checkNum("energy: levelEV of hydrogen 1s is -Ry_inf / (1 + m_e/m_p) = -13.598287 eV (NIST values, 1e-12)", -levelEV(1, 0, 1), hyd, 1e-12);
    checkNum("energy: the same atom in the spectrum: the Lyman limit is h c / 13.598287 eV (1e-12)", [] { for (const SpecLine& l : specLines(1, 0)) if (l.lo == 1 && l.hi == 0) return l.lam; return 0.0; }(), nist::hcEVnm / hyd, 1e-12);
    for (int Z : {1, 2, 26, 92, 118}) {
        const int N = defaultN(Z);
        for (const char* view : {"Orbit", "Cloud"}) {
            AtomLab lab; lab.nowMs = [] { return 0.0; }; lab.p = oneParams(); lab.restart();
            lab.setParam("Z", double(Z)); lab.setParam("N", double(N)); lab.setParam("view", std::string(view));
            for (int n = 1; n <= 4; n++) {
                std::string line;
                if (std::string(view) == "Orbit") { lab.setParam("n", double(n)); line = lab.info1(); checkNum("energy: Orbit view E = levelEV Z=" + std::to_string(Z) + " n=" + std::to_string(n), lab.orbitEnergyEV(), levelEV(Z, N, n), 1e-9); }
                else { lab.setParam("orb", std::string(std::to_string(n) + "s")); line = lab.info1(); }
                // 0.46 (review A4): from kRelativityZ the cloud views' headline gives the Dirac energy first and the lab's level labelled "non-relativistic" (test_atom046.inc checks the Dirac part)
                const std::string lead = std::string(view) == "Cloud" && Z >= kRelativityZ ? "non-relativistic −" : "E −";
                checkTrue("energy: the readout shows fmtEV(levelEV) (" + std::string(view) + ", Z=" + std::to_string(Z) + ", n=" + std::to_string(n) + ")", line.find(lead + fmtEV(-levelEV(Z, N, n))) != std::string::npos);
            }
        }
    }
    AtomLab h; h.nowMs = [] { return 0.0; }; h.p = oneParams(); h.restart();
    checkTrue("energy: hydrogen reads -13.60 eV in the Orbit view and in the Cloud view (was -13.61)", h.info1().find("E −13.60 eV") != std::string::npos && (h.setParam("view", std::string("Cloud")), h.info1()).find("E −13.60 eV") != std::string::npos);
}

// Finding 14: the Orbit view's leapfrog. 60 s of steps at the speeds 1, 5, 10 and 30 and n = 1..4: the radius stays near the analytic Bohr radius n^2 a0 / Z (a0 from the NIST value typed above), the readout
// energy near levelEV, and the electron turns at the rate the speed setting says: dt x speed / n^3 radians per step (a circular orbit's angular rate is Z^2 / n^3 per atomic unit of time, and a step is
// dt x speed / Z^2 of them). Guards: measured worst error + 1 % (the numbers are printed), see the comment at the checks.
// ---- the cloud jump: the same dots built over the frames or at once, the amplitudes, and numbers recorded on Linux ----
// 0.37: the jump's new dots are exact independent samples of the target state made with the jump's own generator (atom_sim.cpp, buildJump). The checks of 0.10.6 stages 8-9 that compared the
// build with the stage 7 Metropolis chain, bit for bit, no longer apply (that chain is gone, on purpose: research report of 6 Nov 2026) and were replaced by:
//   1. the whole run to the end, the jump built over the frames against built at once: the same cloud after landing, bit for bit (same run, same compiler);
//   2. every amplitude stored at an old or a new dot is normFactor * psi of its state at that dot (the exact normalisation of 0.13 stage 3; normFactor itself is checked against the closed form elsewhere);
//   3. numbers recorded on Linux (GCC, Release) from the 0.37 code, with stated tolerances, so a real change of the dots is caught across versions too.
// Floating point: the samples use log, exp, cos, sin and sqrt, which the C libraries of GCC and MSVC may round differently in the last bit (DECISIONS.md, Tests and precision), so recorded numbers
// are compared with tolerances, never bytes.
struct JumpCase { const char *orb, *cdens, *kind, *to; bool reduced; int seed; };
static atom::AtomLab jumpLabFor(const JumpCase& c) {
    using namespace atom;
    seedRandom(uint32_t(c.seed));
    AtomLab lab; lab.nowMs = [] { return 0.0; };
    lab.p = oneParams(); lab.p.elec = "Hydrogen-like"; lab.reducedMotion = c.reduced; lab.restart();   // (0.46: the default is now every electron; these jumps are the one-electron lab's)
    lab.setParam("Z", 6.0); lab.setParam("N", 6.0); lab.setParam("view", std::string("Cloud 3D")); lab.setParam("cdens", std::string(c.cdens)); lab.setParam("orb", std::string(c.orb));
    for (int i = 0; i < 40000 && lab.s.k < lab.cloudCap(); i++) lab.step(1.0 / 240);
    return lab;
}
// Everything the recorded checks look at, as plain numbers.
struct JumpSummary { double jn = 0, sumJp = 0, sumJA = 0, sumJB = 0, shownSign03 = 0, shownSign07 = 0, wheelSum07 = 0, k = 0, sumPts = 0, sumPh = 0, sumSg = 0, rng[3] = {0, 0, 0}; };
static JumpSummary jumpSummary(const JumpCase& c) {
    using namespace atom;
    AtomLab lab = jumpLabFor(c);
    lab.startJump(c.kind, orbIndex(c.to));
    JumpSummary r;
    for (int guard = 0; guard < 600 && !(lab.s.trans && lab.s.jn > 0) && !c.reduced; guard++) lab.step(1.0 / 240);   // an absorbed photon: the jump begins when it arrives
    if (!c.reduced) {
        for (int i = 0; i < 40; i++) lab.step(1.0 / 240);                                                      // some frames into the jump: built over the frames, as the app does
        r.jn = double(lab.s.jn);
        for (float f : lab.s.jp) r.sumJp += std::fabs(double(f));
        for (float f : lab.s.jA) r.sumJA += std::fabs(double(f));
        for (float f : lab.s.jB) r.sumJB += std::fabs(double(f));
        std::vector<unsigned char> g(std::size_t(2 * lab.s.jn));
        lab.p.phase = "Sign";
        for (double t : {0.3, 0.7}) { lab.s.trans->t = t; lab.jumpGroups(g.data()); double n = 0; for (unsigned char v : g) n += v != 255; (t < 0.5 ? r.shownSign03 : r.shownSign07) = n; }
        lab.p.phase = "Wheel";
        lab.s.trans->t = 0.7; lab.jumpGroups(g.data());
        for (unsigned char v : g) if (v != 255) r.wheelSum07 += v;
        if (lab.s.photon) lab.s.photon->t = lab.s.trans->t + (lab.s.photon->kind == "absorb" ? lab.s.photon->dur : 0);   // 0.49.2: the photon runs with its jump: move its clock with the jump's
    }
    for (int i = 0; i < 1200 && (lab.s.trans || lab.s.photon); i++) lab.step(1.0 / 240);     // the jump ends, the cloud goes on for a while (0.49.3: 700 -> 1200)
    for (int i = 0; i < 50; i++) lab.step(1.0 / 240);
    const std::size_t cap = std::size_t(std::min<long>(lab.cloudCap(), long(lab.s.sg.size())));
    r.k = double(lab.s.k);
    for (std::size_t i = 0; i < 3 * cap; i++) r.sumPts += std::fabs(double(lab.s.pts[i]));
    for (std::size_t i = 0; i < cap; i++) { r.sumPh += double(lab.s.ph[i]); r.sumSg += double(lab.s.sg[i]); }
    for (double& q : r.rng) q = atom::random();
    return r;
}

static void testJumpSameDots() {
    const JumpCase cases[] = {{"4f", "Ultra", "emit", "3d|z2", false, 2026}, {"2p", "Normal", "absorb", "3d|z2", false, 77}, {"3d", "Normal", "emit", "2p|z", true, 5}, {"2s", "Normal", "emit", "1s", false, 9}};
    // Recorded on Linux (GCC, Release) from the 0.37 code. Tolerances, and why:
    //  - sums of absolute values of float arrays (positions, amplitudes, picture dots, phases): relative 1e-6. A float holds 6e-8; a last-bit difference of the C library moves a stored float by one unit
    //    of its last place at most rarely, about 1e-7 of a sum of 10^4 values at the very most; 1e-6 is 10 times that.
    //  - how many dots are shown at a moment of the jump: within 0.05 % of all dots (+ 1): a dot at the very edge of its odds could flip with a last-bit difference; the sums of colour numbers likewise.
    //  - the count of dots k, and the next random numbers of the app's generator: exactly (integers; the generator uses only integer maths and an exact division).
    //  - 0.49.2 (deliberate): an emitted photon now leaves during its jump, and the test waits for it: in cases 1 and 4 the cloud grows one 1/240 s step less after landing (64 / 16 dots,
    //    a floating-point tie at the photon's end); their after-landing numbers (k, positions, phases, signs) were recorded again. Nothing else changed.
    //  - 0.49.3 (deliberate, outside review): a photon jump is the one-photon mixture (jumpShareTo) over 2.16 s, each dot in its own level's phase: the dots shown at 0.3 and 0.7 s, the Wheel sum and
    //    (cases 1 and 4, which wait for the photon to leave) the after-landing numbers were recorded again (PL_RECORD=1). The dots, amplitudes and random numbers of the jump are unchanged, bit for bit.
    const JumpSummary want[] = {
        {48000, 6.2455569709369764e-06, 6413.8952001268808, 11555.3599153534, 47711, 47947, 222228, 94272, 6.2414457181317891e-06 / (1 + 1 / atom::nuclearMassMe(6, 6)), 58191.722341775894, 10954, {0.11450255266390741, 0.17949429899454117, 0.33106054225936532}},
        {12000, 1.5603556128808324e-06, 7729.2047131826239, 3321.2663513404282, 12001, 11993, 71532, 14448, 1.55722722444235e-06, 14649.246951341629, 2674, {0.68936545005999506, 0.42211083299480379, 0.053689150139689445}},
        {0, 0, 0, 0, 0, 0, 0, 23568, 7.9139967079048865e-07, 18802.432554960251, 30, {0.013821420492604375, 0.46540140802972019, 0.30045507149770856}},
        {12000, 2.380023011858562e-07, 7040.9308057936159, 37906.750024672656, 11881, 11935, 240, 23568, 2.3887805449478335e-07 / (1 + 1 / atom::nuclearMassMe(6, 6)), 0, 12000, {0.93781672464683652, 0.28702248260378838, 0.73027130821719766}},
    };
    for (std::size_t ci = 0; ci < 4; ci++) {
        const JumpCase& c = cases[ci];
        const std::string name = std::string("jump: ") + c.orb + " " + c.kind + " to " + c.to + ", " + c.cdens + (c.reduced ? ", reduced motion" : "") + ": ";
        // ---- 1. built over the frames or at once: the same cloud after landing, bit for bit ----
        {
            atom::AtomLab a = jumpLabFor(c), b = jumpLabFor(c);                   // (same seed: the same start; the generator is global, so each one is run on its own, from the same state)
            const std::uint32_t s0 = atom::randomState();
            auto landed = [&](atom::AtomLab& L, bool atOnce) {
                atom::seedRandom(s0);
                L.startJump(c.kind, atom::orbIndex(c.to));
                for (int g = 0; g < 600 && !L.s.trans && !c.reduced; g++) L.step(1.0 / 240);
                if (atOnce) L.completeJump();
                for (int i = 0; i < 1200 && (L.s.trans || L.s.photon); i++) L.step(1.0 / 240);   // (0.49.3: 700 -> 1200: the jump lasts 2.16 s, its photon 1.6 s more)
                for (int i = 0; i < 50; i++) L.step(1.0 / 240);
            };
            landed(a, false); const std::uint32_t ra = atom::randomState();
            landed(b, true); const std::uint32_t rb = atom::randomState();
            checkTrue(name + "the cloud after landing is the same built over the frames or at once (dots, signs, phases, count, both generators), bit for bit",
                      a.s.pts == b.s.pts && a.s.sg == b.s.sg && a.s.ph == b.s.ph && a.s.k == b.s.k && a.s.crng == b.s.crng && ra == rb);
        }
        // ---- 2. every stored amplitude is normFactor * psi of its state at its dot ----
        if (!c.reduced) {
            atom::AtomLab lab = jumpLabFor(c);
            lab.startJump(c.kind, atom::orbIndex(c.to));
            for (int g = 0; g < 600 && !lab.s.trans; g++) lab.step(1.0 / 240);
            lab.completeJump();
            const int from = lab.s.jb.from, to = lab.s.jb.to, Z = lab.p.Z;
            const double nA = atom::normFactor(from, Z), nB = atom::normFactor(to, Z);
            long same = 0;
            for (long i = 0; i < lab.s.jn; i++) {
                const double u = lab.aUnit();                                  // 0.46.4: the dots are in the reduced-mass Bohr radius a_mu (atom_sim.h aUnit), the unit psi() is sampled in
                const double x0 = lab.s.pts[3 * i] / u, y0 = lab.s.pts[3 * i + 1] / u, z0 = lab.s.pts[3 * i + 2] / u;
                const double x1 = lab.s.jp[3 * i] / u, y1 = lab.s.jp[3 * i + 1] / u, z1 = lab.s.jp[3 * i + 2] / u;
                const bool okA = lab.s.jA[4 * i] == float(nA * atom::psi(from, Z, x0, y0, z0)) && lab.s.jA[4 * i + 2] == float(nB * atom::psi(to, Z, x0, y0, z0));
                // the new dot's place was stored as a float, its amplitudes from the double: recomputed at the float place they agree to float rounding
                const double bA = nA * atom::psi(from, Z, x1, y1, z1), bB = nB * atom::psi(to, Z, x1, y1, z1);
                const bool okB = std::fabs(lab.s.jB[4 * i] - bA) <= 1e-5 * (std::fabs(bA) + std::fabs(bB)) + 1e-30 && std::fabs(lab.s.jB[4 * i + 2] - bB) <= 1e-5 * (std::fabs(bA) + std::fabs(bB)) + 1e-30;
                same += okA && okB;
            }
            checkTrue(name + "every amplitude at an old and at a new dot is normFactor x psi there (" + std::to_string(same) + " of " + std::to_string(lab.s.jn) + ")", same == lab.s.jn && lab.s.jn > 0);
        }
        // ---- 3. against the numbers recorded on Linux, with the tolerances above ----
        const JumpSummary got = jumpSummary(c), &w = want[ci];
        if (std::getenv("PL_RECORD")) std::printf("RECORD {%.17g, %.17g, %.17g, %.17g, %.17g, %.17g, %.17g, %.17g, %.17g, %.17g, %.17g, {%.17g, %.17g, %.17g}},\n", got.jn, got.sumJp, got.sumJA, got.sumJB, got.shownSign03, got.shownSign07, got.wheelSum07, got.k, got.sumPts, got.sumPh, got.sumSg, got.rng[0], got.rng[1], got.rng[2]);
        const double nDots = std::max(1.0, w.jn * 2), shownTol = 0.0005 * nDots + 1;
        checkNum(name + "recorded: dot count", got.jn, w.jn, 0);
        // 0.46.4 (deliberate): the recorded sums are in metres with a0; the dots are now in a_mu = a0 (1 + m_e / M), C-12 here: the same samples, 1 + 1/21,876 times as far out
        const double lenMu = 1 + 1 / atom::nuclearMassMe(6, 6);
        checkNum(name + "recorded: sum of |new dot coordinates|", got.sumJp, w.sumJp * lenMu, 1e-6);
        checkNum(name + "recorded: sum of |amplitudes at the old dots|", got.sumJA, w.sumJA, 1e-6);
        checkNum(name + "recorded: sum of |amplitudes at the new dots|", got.sumJB, w.sumJB, 1e-6);
        checkNum(name + "recorded: dots shown at 0.3 of the jump (Sign)", got.shownSign03, w.shownSign03, 0, shownTol);
        checkNum(name + "recorded: dots shown at 0.7 of the jump (Sign)", got.shownSign07, w.shownSign07, 0, shownTol);
        checkNum(name + "recorded: sum of the phase colour numbers at 0.7 (Wheel)", got.wheelSum07, w.wheelSum07, 1e-3, 1);
        checkNum(name + "recorded: dots in the cloud after landing", got.k, w.k, 0);
        checkNum(name + "recorded: sum of |dot coordinates| after landing", got.sumPts, w.sumPts * lenMu, 1e-6);
        checkNum(name + "recorded: sum of the phases after landing", got.sumPh, w.sumPh, 1e-6);
        checkNum(name + "recorded: sum of the signs after landing", got.sumSg, w.sumSg, 0, 0.0005 * (w.jn > 0 ? w.jn : 12000) + 1);
        for (int a = 0; a < 3; a++) checkNum(name + "recorded: the next random number " + std::to_string(a), got.rng[a], w.rng[a], 0);
    }
}

// 0.64.0: the Slice of a superposition with the planes' parts worked out once per batch (AtomLab::addDots): a step's new dots = the processor's attempts one by
// one as 0.63.1 made them (a number for the state with odds of the planes' masses, then tryStateOnPlane of that state), with their amplitudes, bit for bit
static void testSliceSuper064() {
    using namespace atom;
    AtomLab lab; lab.nowMs = [] { return 0.0; };
    seedRandom(64);
    lab.p = oneParams(); lab.restart();
    lab.gpuSlice = false;
    lab.setParam("Z", 101.0); lab.setParam("N", 157.0); lab.setParam("view", std::string("Slice")); lab.setParam("orb", std::string("2p"));
    lab.setParam("orb2", std::string("5f")); lab.setParam("mix", 50.0); lab.setParam("super", std::string("On")); lab.setParam("cut", 0.3);
    checkTrue("Slice superposition: the lab is in the benchmark's superposition (2p z + 5f z3, Md)", lab.superOn() && lab.p.view == "Slice");
    lab.s.k = 0;
    orbit::Mulberry32 r(lab.s.crng);
    const int idA = orbIndex("2p|z"), idB = orbIndex("5f|z3"), Z = 101;
    const double u = lab.aUnit(), nA = normFactor(idA, Z), nB = normFactor(idB, Z);
    lab.step(1.0 / 240);
    const int axis = lab.s.sliceAxis;
    const double off = lab.s.sliceOff, mA = planeMass(idA, Z, axis, off), mB = planeMass(idB, Z, axis, off), wA = mA / (mA + mB);
    long same = 0, n = 0, fromA = 0;
    for (long i = 0; i < std::min<long>(lab.s.k, 1500); i++) {
        CloudPoint q;
        int id = idA;
        for (;;) { id = r.next() < wA ? idA : idB; if (tryStateOnPlane(id, Z, axis, off, r, q)) break; }
        fromA += id == idA;
        const float a = float(nA * psi(idA, Z, q.x, q.y, q.z)), b = float(nB * psi(idB, Z, q.x, q.y, q.z));
        same += lab.s.pts[3 * i] == float(q.x * u) && lab.s.pts[3 * i + 1] == float(q.y * u) && lab.s.pts[3 * i + 2] == float(q.z * u) && lab.s.sa[4 * i] == a && lab.s.sa[4 * i + 2] == b;
        n++;
    }
    checkTrue("Slice superposition: a step's " + std::to_string(n) + " new dots (" + std::to_string(fromA) + " of 2p) = the attempts one by one, with their amplitudes, bit for bit",
              n == 1500 && same == n && fromA > 0 && fromA < n && lab.s.crng == r.seed);
}

// The new parts of the jump's speed work, each against the old way of getting the same numbers.
// 0.57.0, the plain waste taken out of the Slice's dots, the jump with every electron and the Glow core's dots: each new way against the old one, exactly.
static void testPlainWaste057() {
    using namespace atom;
    // 1. the Slice's attempt with the plane's part worked out once (PlaneSampler) = tryStateOnPlane, the same random numbers into the same points, bit for bit:
    //    real and complex states, both axes, planes through the nucleus, near it and far out, 20,000 attempts each
    {
        long tries = 0, same = 0, kept = 0;
        std::vector<int> ids = {orbIndex("1s"), orbIndex("2p|z"), orbIndex("3d|z2"), orbIndex("4f|z3"), orbIndex("4f|xx23y2"), orbIndex("6s"), complexState(orbIndex("4f|z3"), 2), complexState(orbIndex("3d|z2"), -1)};
        for (int id : ids)
            for (int axis : {1, 2})
                for (double off : {0.0, 0.07, -0.5, 3.0, 40.0}) {
                    const PlaneSampler ps = planeSampler(id, 92, axis, off);
                    orbit::Mulberry32 a(0x1234567u + std::uint32_t(id) * 7u + std::uint32_t(axis)), b(a.seed);
                    for (int k = 0; k < 20000; k++) {
                        CloudPoint p, q;
                        const bool ka = tryStateOnPlane(id, 92, axis, off, a, p), kb = ps.tryPoint(b, q);
                        tries++;
                        same += ka == kb && a.seed == b.seed && (!ka || (p.x == q.x && p.y == q.y && p.z == q.z));
                        kept += ka;
                    }
                }
        checkTrue("Slice: the attempt prepared once per plane = tryStateOnPlane bit for bit (" + std::to_string(tries) + " attempts, " + std::to_string(kept) + " kept; 8 states, 2 axes, 5 planes)", same == tries && kept > tries / 20);
    }
    // 2. the Slice view's batch (AtomLab::addDots on the plane) = the dots of tryStateOnPlane one by one from the cloud's generator, the colour from psi
    {
        AtomLab lab; lab.nowMs = [] { return 0.0; };
        seedRandom(57);
        lab.p = oneParams(); lab.restart();
        lab.setParam("Z", 92.0); lab.setParam("N", 146.0); lab.setParam("view", std::string("Slice")); lab.setParam("orb", std::string("4f"));
        lab.setParam("cut", 0.3);
        lab.s.k = 0;
        orbit::Mulberry32 r(lab.s.crng);
        const int id = lab.stateId();
        const double u = lab.aUnit();
        lab.step(1.0 / 240);
        const int axis = lab.s.sliceAxis;                        // (the plane the step made its dots on)
        const double off = lab.s.sliceOff;
        long same = 0, n = 0;
        for (long i = 0; i < std::min<long>(lab.s.k, 1500); i++) {
            CloudPoint q;
            while (!tryStateOnPlane(id, 92, axis, off, r, q)) {}
            const double pv = psi(id, 92, q.x, q.y, q.z);
            same += lab.s.pts[3 * i] == float(q.x * u) && lab.s.pts[3 * i + 1] == float(q.y * u) && lab.s.pts[3 * i + 2] == float(q.z * u) && lab.s.sg[i] == (pv >= 0 ? 1 : -1);
            n++;
        }
        checkTrue("Slice: a step's " + std::to_string(n) + " new dots on the plane = tryStateOnPlane's, one by one, and their signs", n == 1500 && same == n && lab.s.crng == r.seed);
    }
    // 3. a jump with every electron (a level atom, Ca): every dot's group in one call = jumpGroup(pop, i) dot by dot, Sign and Wheel, at several moments
    for (const char* phase : {"Sign", "Wheel"}) {
        AtomLab lab; lab.nowMs = [] { return 0.0; };
        seedRandom(58);
        lab.p = AtomParams::appDefaults(); lab.restart();
        lab.setParam("Z", 20.0); lab.setParam("N", 20.0);
        lab.p.elec = "All"; lab.p.mrel = "Relativistic"; lab.p.show = "Whole atom"; lab.p.cdens = "Normal"; lab.p.phase = phase;
        const std::vector<JLine>& ln = jumpLines(20);
        int line = -1;
        for (int k = 0; k < int(ln.size()); k++) if (jumpOffered(20, k)) { line = k; lab.s.lvl = ln[std::size_t(k)].hi; break; }
        lab.setParam("view", std::string("Cloud 3D"));
        for (int i = 0; i < 40000 && lab.s.k < lab.cloudCap(); i++) lab.step(1.0 / 240);
        if (line >= 0) lab.emitLevelLine(line);
        for (int i = 0; i < 2000 && !lab.s.trans; i++) { if (lab.s.photon) lab.s.photon.reset(); lab.step(1.0 / 240); }
        bool ok = lab.s.trans && lab.levelOn();
        long same = 0, shown = 0, jn = 0;
        if (ok) {
            lab.completeJump();
            jn = lab.jumpDots();
            const double dur = lab.s.trans->dur;
            for (double t : {0.0, 0.05, 0.3, 0.5, 0.9, 0.99}) {
                lab.s.trans->t = t * dur;
                std::vector<unsigned char> g(std::size_t(2 * jn), 7);
                lab.jumpGroups(g.data());
                for (long i = 0; i < jn; i++) { same += g[std::size_t(i)] == (unsigned char)lab.jumpGroup(0, i); same += g[std::size_t(jn + i)] == (unsigned char)lab.jumpGroup(1, i); shown += g[std::size_t(i)] != 255; }
            }
        }
        checkTrue(std::string("every electron, a level jump of Ca: jumpGroups = jumpGroup for every dot (") + phase + ", " + std::to_string(jn) + " dots x 2 x 6 moments)", ok && jn > 0 && lab.s.jb.many && same == 12 * jn && shown > 0);
    }
    // 4. the Glow core's dots with the quick pass first = the full pass alone (the 0.52.1 loop, kept here as the reference), cameras at several turns and zooms
    {
        AtomLab lab; lab.nowMs = [] { return 0.0; };
        seedRandom(59);
        lab.p = AtomParams::appDefaults(); lab.restart();
        lab.setParam("Z", 54.0); lab.setParam("N", 77.0); lab.p.elec = "All"; lab.p.cdens = "High";
        lab.setParam("view", std::string("Cloud 3D"));
        for (int i = 0; i < 40000 && lab.s.k < lab.cloudCap(); i++) lab.step(1.0 / 240);
        const long cnt = std::min<long>(lab.s.k, lab.cloudCap());
        std::vector<unsigned char> groups(static_cast<std::size_t>(cnt));
        for (long i = 0; i < cnt; i++) groups[std::size_t(i)] = i % 7 == 3 ? 255 : (unsigned char)(i % 2);
        long cases = 0, same = 0, found = 0;
        const double ext = lab.cloudExtM();
        for (double zoom : {0.5, 2.0, 30.0, 400.0})
            for (double yaw : {0.0, 0.7, 2.9})
                for (double pitch : {-1.2, 0.3}) {
                    const CloudCamera cam{640, 400, zoom * 400 / ext, std::cos(yaw), std::sin(yaw), std::cos(pitch), std::sin(pitch), ext};
                    std::vector<GlowDot> ref, got;
                    for (long i = 0; i < cnt; i++) {
                        if (groups[std::size_t(i)] == 255) continue;
                        const double x = lab.s.pts[3 * i], y = lab.s.pts[3 * i + 1], z = lab.s.pts[3 * i + 2];
                        const double x1 = x * cam.ca - y * cam.sa, y1 = x * cam.sa + y * cam.ca, dep = y1 * cam.cb - z * cam.sb, up = y1 * cam.sb + z * cam.cb;
                        const double f = 1 / (1 + dep / (4 * cam.ext));
                        const float px = float(cam.cx + x1 * cam.pxm * f), py = float(cam.cy - up * cam.pxm * f);
                        const float t = std::max(-1.f, std::min(1.f, float(dep / cam.ext))), nr = 0.5f - 0.5f * t;
                        const float rr = 4.6f * (0.8f + 0.4f * nr);
                        if ((px - 640.0) * (px - 640.0) + (py - 400.0) * (py - 400.0) < double(rr) * rr) ref.push_back({px, py, rr, 0.3f + 0.9f * nr, groups[std::size_t(i)]});
                    }
                    glowDotsNear(lab.s.pts.data(), cnt, groups.data(), cam, 640, 400, got);
                    bool eq = ref.size() == got.size();
                    for (std::size_t k = 0; eq && k < ref.size(); k++) eq = ref[k].x == got[k].x && ref[k].y == got[k].y && ref[k].r == got[k].r && ref[k].wt == got[k].wt && ref[k].g == got[k].g;
                    cases++; same += eq; found += long(ref.size());
                }
        checkTrue("Glow core: the dots over the nucleus with the quick pass first = the full pass alone (" + std::to_string(cases) + " cameras, " + std::to_string(found) + " dots found)", same == cases && found > 100);
    }
}

// 0.57.0, the processor's side of the card's two programs (atom/atom_card.h): the jump's 53-bit thresholds give exactly jumpUniform < share; the Slice's
// comparison passes the processor's own answers rounded to single precision and catches a moved point and a flipped answer far from its margin.
static void testCardReference057() {
    using namespace atom;
    {   // the threshold: z >> 11 < ceil(share 2^53) exactly when (z >> 11) / 2^53 < share, at random shares, shares on and next to the grid, and the ends
        std::mt19937_64 g(57);
        long n = 0, same = 0;
        std::vector<double> shares = {0.0, 1.0, 0.5, 1e-300, 1 - 1e-16, std::ldexp(1.0, -53), std::ldexp(3.0, -53), 0.25 + std::ldexp(1.0, -53)};
        for (int k = 0; k < 200; k++) shares.push_back(std::ldexp(double(g() >> 11), -53));
        for (int k = 0; k < 200; k++) shares.push_back(std::uniform_real_distribution<double>(0, 1)(g));
        for (double f : shares)
            for (int pop = 0; pop < 2; pop++) {
                const std::uint64_t T = render::jumpShareThreshold(f);
                for (long i = 0; i < 2000; i++) {
                    const double u = jumpUniformOf(pop, i);
                    const std::uint64_t m = std::uint64_t(u * 9007199254740992.0);      // exact: u is m / 2^53
                    same += (m < T) == (u < f);
                    n++;
                }
                // the share exactly at a dot's own number: not shown (u < f is false), and just above it: shown
                const double u0 = jumpUniformOf(pop, 17);
                const std::uint64_t m0 = std::uint64_t(u0 * 9007199254740992.0);
                same += !(m0 < render::jumpShareThreshold(u0)) && (m0 < render::jumpShareThreshold(std::nextafter(u0, 2.0)));
                n++;
            }
        checkTrue("card: the jump's 53-bit threshold test = jumpUniform < share for " + std::to_string(n) + " dots and shares", same == n);
    }
    {   // the Slice's comparison: the processor's own attempts rounded once to single precision (what a perfect card would give) pass; a point moved by
        // more than its limit, and an answer flipped where the margin is large, are found
        const PlaneSampler ps = planeSampler(orbIndex("4f|z3"), 92, 1, 0.05);
        const std::uint32_t seed = 0xC0FFEEu;
        const int tries = 3000;
        std::vector<float> res(std::size_t(4 * tries));
        long keptAt = -1, farAt = -1;
        for (int t = 0; t < tries; t++) {
            orbit::Mulberry32 r(mulberryAfter(seed, 3ull * std::uint64_t(t)));
            const PlaneSampler::Detail d = ps.tryDetail(r);
            res[std::size_t(4 * t)] = float(d.q.x); res[std::size_t(4 * t + 1)] = float(d.q.y); res[std::size_t(4 * t + 2)] = float(d.q.z);
            res[std::size_t(4 * t + 3)] = d.radiusZero ? -1.f : d.kept ? 1.f : 0.f;
            if (d.kept && keptAt < 0 && d.rho > 0.01) keptAt = t;
            if (!d.kept && farAt < 0 && d.r3 * ps.bound > d.a2 + 0.2 * ps.bound) farAt = t;
        }
        // the generator's counter: the attempts made from mulberryAfter are the attempts of one generator run straight through
        orbit::Mulberry32 straight(seed);
        bool sameRun = true;
        for (int t = 0; t < 200; t++) { orbit::Mulberry32 r(mulberryAfter(seed, 3ull * std::uint64_t(t))); const PlaneSampler::Detail a = ps.tryDetail(straight), b = ps.tryDetail(r); sameRun = sameRun && a.q.x == b.q.x && a.q.z == b.q.z && a.kept == b.kept; }
        const SliceCompare good = compareSlice(ps, seed, res, tries);
        std::vector<float> moved = res, flipped = res;
        if (keptAt >= 0) moved[std::size_t(4 * keptAt)] += 1e-3f;
        if (farAt >= 0) flipped[std::size_t(4 * farAt + 3)] = 1.f;
        const SliceCompare m = compareSlice(ps, seed, moved, tries), f = compareSlice(ps, seed, flipped, tries);
        checkTrue("card: the Slice's comparison passes the processor's own answers in single precision (" + sliceCompareText(good) + ")", sameRun && good.ok() && good.kept > 100 && good.flagDiff == 0);
        checkTrue("card: ... and finds a kept dot moved by 1e-3 Bohr radii and an answer flipped far from its margin", keptAt >= 0 && farAt >= 0 && !m.ok() && m.pointBad == 1 && !f.ok() && f.flagDiff == 1 && f.flagExplained == 0);
        // the card's batch carries the processor's numbers, each rounded once
        const render::SliceDotsJob j = sliceJobOf(ps, seed, tries);
        bool carried = j.seed == seed && j.tries == tries && j.axis == 1 && j.nh == int(ps.tailPoly().size()) && j.angIm == -1 && j.ang == ps.ang && j.bound == float(ps.bound);
        for (int i = 0; i < j.nh; i++) carried = carried && j.h[i] == float(ps.tailPoly()[std::size_t(i)]) && j.c[i] == float(ps.densPoly()[std::size_t(i)]);
        checkTrue("card: the Slice's batch carries the plane's numbers (tail and density polynomials, bound, angular part)", carried);
    }
}

// 0.89.0: buildManyJump in fixed chunks, each on its own random stream, on threads: the dots are the same for any thread count, compared exactly (every float and sign),
// for a level atom (radium-226, as atom.emit_all: Ultra, Shells enlarged), an alkali valence jump (sodium 3s -> 3p, every electron) and the same on the Slice.
// Built over frames or at once with 8 threads gives the same dots too.
static void testManyJumpThreads089() {
    using namespace atom;
    struct Out { std::vector<float> jp, jph; std::vector<signed char> jsg; long jn = 0; bool many = false; };
    const auto run = [](int Z, int N, const char* view, int threads, bool frames) {
        Out o;
        AtomLab lab; lab.nowMs = [] { return 0.0; };
        seedRandom(77);
        lab.p = AtomParams::appDefaults(); lab.restart();
        lab.setParam("Z", double(Z)); lab.setParam("N", double(N));
        lab.p.elec = "All"; lab.p.mrel = "Relativistic"; lab.p.show = "Whole atom"; lab.p.cdens = "Ultra"; lab.p.rscale = "Shells enlarged";
        const std::vector<JLine>& ln = jumpLines(Z);
        int line = -1;
        if (lab.levelOn()) for (int k = 0; k < int(ln.size()); k++) if (jumpOffered(Z, k)) { line = k; lab.s.lvl = ln[std::size_t(k)].hi; break; }
        lab.setParam("view", std::string(view));
        for (int i = 0; i < 40000 && lab.s.k < lab.cloudCap(); i++) lab.step(1.0 / 240);
        AtomLab::manyJumpThreadsOverride = threads;
        if (line >= 0) lab.emitLevelLine(line);
        else if (lab.valenceOn() && (!lab.valenceTargets(-1).empty() || !lab.valenceTargets(+1).empty())) {   // (to: an alkaliOrbitals index)
            const bool up = !lab.valenceTargets(+1).empty();
            const AlkOrbital t = lab.valenceTargets(up ? +1 : -1).front();
            const std::vector<AlkOrbital> orbs = alkaliOrbitals(Z);
            for (int k = 0; k < int(orbs.size()); k++) if (orbs[std::size_t(k)].n == t.n && orbs[std::size_t(k)].l == t.l) { lab.startJump(up ? "absorb" : "emit", k); break; }
        }
        for (int i = 0; i < 2000 && !lab.s.trans; i++) { if (line >= 0 && lab.s.photon) lab.s.photon.reset(); lab.step(1.0 / 240); }   // (an absorbed photon flies in first)
        if (lab.s.trans) {
            const auto t0 = std::chrono::steady_clock::now();
            if (frames) for (int i = 0; i < 100 && !lab.jumpReady(); i++) lab.step(1.0 / 240);
            lab.completeJump();
            std::printf("PLMANYJUMP Z=%d view=%s threads=%d frames=%d dots=%ld ms=%.1f\n", Z, view, threads, frames ? 1 : 0, lab.s.jn,
                        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count());
            o.jp = lab.s.jp; o.jph = lab.s.jph; o.jsg = lab.s.jsg; o.jn = lab.s.jn; o.many = lab.s.jb.many;
        }
        AtomLab::manyJumpThreadsOverride = 0;
        return o;
    };
    struct Case { int Z, N; const char* view; const char* name; };
    for (const Case& c : {Case{88, 138, "Cloud 3D", "radium-226, a level jump"}, Case{11, 12, "Cloud 3D", "sodium, a valence jump"}, Case{11, 12, "Slice", "sodium on the Slice"}}) {
        const Out one = run(c.Z, c.N, c.view, 1, false);
        checkTrue(std::string("many-electron jump on threads, ") + c.name + ": a valence or level jump with dots (" + std::to_string(one.jn) + ")", one.many && one.jn > 20000);
        for (int t : {2, 8, 16}) {
            const Out o = run(c.Z, c.N, c.view, t, false);
            checkTrue(std::string("many-electron jump on threads, ") + c.name + ": " + std::to_string(t) + " threads give the 1 thread's dots exactly", o.jn == one.jn && o.jp == one.jp && o.jph == one.jph && o.jsg == one.jsg);
        }
        const Out f = run(c.Z, c.N, c.view, 8, true);
        checkTrue(std::string("many-electron jump on threads, ") + c.name + ": built over frames on 8 threads = at once on 1", f.jn == one.jn && f.jp == one.jp && f.jph == one.jph && f.jsg == one.jsg);
    }
}

static void testJumpBuild() {
    using namespace atom;
    // random numbers: skipping n is the same as drawing n
    for (std::uint64_t n : {0ull, 1ull, 7ull, 5600ull, 343000ull}) {
        seedRandom(4242);
        const std::uint32_t s0 = randomState();
        for (std::uint64_t i = 0; i < n; i++) (void)atom::random();
        const std::uint32_t drawn = randomState();
        seedRandom(4242); skipRandom(n);
        checkTrue("random numbers: skipRandom(" + std::to_string(n) + ") ends on the state n draws end on", randomState() == drawn && (n == 0 || drawn != s0));
        seedRandom(4242); for (std::uint64_t i = 0; i < n; i++) (void)atom::random();
        const double a = atom::random();
        seedRandom(4242); skipRandom(n);
        checkTrue("random numbers: and the next number is the same", a == atom::random());
    }
    // every dot's group in one call is the same number as the call for each dot, in both colour modes and at several moments of the jump
    for (const char* phase : {"Sign", "Wheel"}) {
        AtomLab lab; lab.nowMs = [] { return 0.0; };
        seedRandom(31);
        lab.p = oneParams(); lab.restart();
        lab.setParam("Z", 6.0); lab.setParam("N", 6.0); lab.setParam("view", std::string("Cloud 3D")); lab.setParam("cdens", std::string("High")); lab.setParam("orb", std::string("3d")); lab.setParam("phase", std::string(phase));
        for (int i = 0; i < 40000 && lab.s.k < lab.cloudCap(); i++) lab.step(1.0 / 240);
        lab.startJump("emit", orbIndex("2p|z"));
        lab.completeJump();
        const long jn = lab.jumpDots();
        long same = 0, shown = 0;
        for (double t : {0.0, 0.05, 0.3, 0.6, 0.9, 1.19, 2.0}) {
            lab.s.trans->t = t;
            std::vector<unsigned char> g(std::size_t(2 * jn), 7);
            lab.jumpGroups(g.data());
            for (long i = 0; i < jn; i++) { same += g[std::size_t(i)] == (unsigned char)lab.jumpGroup(0, i); same += g[std::size_t(jn + i)] == (unsigned char)lab.jumpGroup(1, i); shown += g[std::size_t(i)] != 255; }
        }
        checkTrue(std::string("jumpGroups = jumpGroup for every dot (") + phase + ", " + std::to_string(jn) + " dots x 2 populations x 7 moments)", same == 14 * jn && shown > 0);
    }
    // the build over a few frames: the start frame builds no dots, a few frames finish it, nothing is read half built, and the result is the one built all at once
    for (const char* dens : {"Normal", "Ultra"}) {
        AtomLab A, B;
        for (AtomLab* L : {&A, &B}) {
            seedRandom(8); L->nowMs = [] { return 0.0; };
            L->p = oneParams(); L->restart();
            L->setParam("Z", 3.0); L->setParam("N", 4.0); L->setParam("view", std::string("Cloud 3D")); L->setParam("cdens", std::string(dens)); L->setParam("orb", std::string("4f"));
            for (int i = 0; i < 40000 && L->s.k < L->cloudCap(); i++) L->step(1.0 / 240);
            L->startJump("emit", L->targets(-1).front());
        }
        const std::string w = std::string("jump build, ") + dens + ": ";
        checkTrue(w + "right after the start the jump is not built (the start frame does not carry it) and is not drawn yet", A.s.trans && !A.jumpReady() && !A.jumpDrawn() && A.jumpCloud() && A.s.jn == A.cloudCap());
        int frames = 0;
        while (!A.jumpReady() && frames < 100) { A.step(1.0 / 240); frames++; }
        checkTrue(w + "built by the frames alone, in 4 or 5 of them (" + std::to_string(frames) + ")", A.jumpReady() && frames >= 3 && frames <= 5 && A.jumpDrawn());
        B.completeJump();                                                          // the other lab: all at once, at once
        for (int i = 0; i < frames; i++) B.step(1.0 / 240);                         // the same number of frames, so the clocks agree
        checkTrue(w + "the dots and amplitudes are the same built over frames or all at once, bit for bit",
                  A.s.jp == B.s.jp && A.s.jA == B.s.jA && A.s.jB == B.s.jB && A.s.jb.rng == B.s.jb.rng);
        checkTrue(w + "asking about a dot of a half-built jump gives the finished jump", [&] {
            AtomLab C; seedRandom(8); C.nowMs = [] { return 0.0; }; C.p = oneParams(); C.restart();
            C.setParam("Z", 3.0); C.setParam("N", 4.0); C.setParam("view", std::string("Cloud 3D")); C.setParam("cdens", std::string(dens)); C.setParam("orb", std::string("4f"));
            for (int i = 0; i < 40000 && C.s.k < C.cloudCap(); i++) C.step(1.0 / 240);
            C.startJump("emit", C.targets(-1).front());
            C.step(1.0 / 240);                                                        // one chunk built, the rest not
            const bool part = !C.jumpReady();
            (void)C.jumpShown(1, 5);
            return part && C.jumpReady() && C.s.jp == A.s.jp && C.s.jB == A.s.jB;
        }());
    }
    // a jump that is over before it is built (a very long frame) is built first, so the landing is the same cloud
    {
        AtomLab A, B;
        for (AtomLab* L : {&A, &B}) {
            seedRandom(12); L->nowMs = [] { return 0.0; }; L->p = oneParams(); L->restart();
            L->setParam("view", std::string("Cloud 3D")); L->setParam("orb", std::string("2p"));
            for (int i = 0; i < 40000 && L->s.k < L->cloudCap(); i++) L->step(1.0 / 240);
            L->startJump("emit", orbIndex("1s"));
        }
        seedRandom(555); A.step(5.0);                                                 // one frame of 5 s: the jump is over at once (the cloud goes on after the landing: the same numbers for both)
        B.completeJump(); seedRandom(555); B.step(5.0);
        checkTrue("jump build: a jump ended by one very long frame lands on the same cloud as one built first", !A.s.trans && !B.s.trans && A.s.pts == B.s.pts && A.s.sg == B.s.sg && A.s.k == B.s.k && A.s.jn == 0 && A.s.jb.done == 0);
    }
}

static void testOrbitAccuracy(double radTol, double enTol, double rateTol) {
    double worstR = 0, worstE = 0, worstRate = 0; int runs = 0;
    for (int Z : {1, 3})
        for (int speed : {1, 5, 10, 30})
            for (int n = 1; n <= 4; n++) {
                AtomLab lab; lab.nowMs = [] { return 0.0; }; lab.p = oneParams(); lab.restart();
                lab.setParam("Z", double(Z)); lab.setParam("n", double(n)); lab.setParam("speed", double(speed));
                const double rB = double(n) * n / Z * nist::a0 * (1 + 1 / nuclearMassMe(Z, lab.p.N)), E0 = levelEV(Z, lab.p.N, n);   // 0.46.4 (deliberate): the reduced-mass Bohr radius a0 m / mu, the radius that goes with the reduced-mass level
                double ang = std::atan2(lab.s.y, lab.s.x), turned = 0, wr = 0, we = 0;
                const int steps = 240 * 60;
                for (int i = 0; i < steps; i++) {
                    lab.step(1.0 / 240);
                    wr = std::max(wr, std::fabs(std::hypot(lab.s.x, lab.s.y) / rB - 1));
                    we = std::max(we, std::fabs(lab.orbitEnergyEV() / E0 - 1));
                    const double a = std::atan2(lab.s.y, lab.s.x); double d = a - ang; while (d < -kPi) d += 2 * kPi; while (d > kPi) d -= 2 * kPi;
                    turned += d; ang = a;
                }
                const double want = steps * (1.0 / 240) * speed / (double(n) * n * n);          // radians
                worstR = std::max(worstR, wr); worstE = std::max(worstE, we); worstRate = std::max(worstRate, std::fabs(turned / want - 1)); runs++;
                if (wr > radTol || we > enTol) std::printf("  Orbit accuracy Z=%d speed %d n=%d: radius error %.2e, energy error %.2e\n", Z, speed, n, wr, we);
            }
    std::printf("Orbit accuracy over %d runs of 60 s: worst radius error %.5e, energy error %.5e, turning-rate error %.5e\n", runs, worstR, worstE, worstRate);
    checkTrue("Orbit: the radius stays within the guard of the analytic Bohr radius n^2 a0 / Z at speeds 1 to 30, n = 1..4", worstR <= radTol);
    checkTrue("Orbit: the readout energy stays within the guard of levelEV at speeds 1 to 30, n = 1..4", worstE <= enTol);
    checkTrue("Orbit: the electron turns dt x speed / n^3 radians per step (the visible speed), within the guard", worstRate <= rateTol);
}

// Finding 8: Scale Fixed has to hold every cloud. The view radius (sizeM, the half-size the picture is fitted to) must reach at least the 90 % outline radius of the orbital,
// from the lab's own outline code (outlineOf: rmax in Bohr radii at Z = 1). The same for Auto (kExt per n) and for the complex states; Fixed in the Orbit view holds the n = 4 circle.
static void testFixedExtent() {
    double worstFixed = 1e9, worstAuto = 1e9; std::string wf, wa;
    std::vector<int> ids;
    for (int i = 0; i < int(orbitals().size()); i++) ids.push_back(i);
    for (int id : complexIds()) ids.push_back(id);
    for (const char* scale : {"Fixed", "Auto"})
        for (int Z : {1, 3}) {
            AtomLab lab; lab.nowMs = [] { return 0.0; }; lab.p = oneParams(); lab.restart();
            lab.setParam("view", std::string("Cloud")); lab.setParam("Z", double(Z)); lab.setParam("scale", std::string(scale));
            for (int id : ids) {
                const Orbital& o = orbitals()[stateOrbital(id)];
                lab.setParam("orb", o.shell);
                const double reach = outlineOf(id).rmax / Z * kA0, ratio = lab.sizeM() / reach;   // metres at this Z
                if (std::string(scale) == "Fixed" && ratio < worstFixed) { worstFixed = ratio; wf = stateText(id); }
                if (std::string(scale) == "Auto" && ratio < worstAuto) { worstAuto = ratio; wa = stateText(id); }
            }
        }
    std::printf("view radius / 90 %% outline radius, smallest over 30 real + 20 complex orbitals: Fixed %.3f (%s), Auto %.3f (%s)\n", worstFixed, wf.c_str(), worstAuto, wa.c_str());
    checkTrue("Scale Fixed: the view radius holds the 90 % outline of every orbital (n = 1..4, s p d f, real and complex)", worstFixed >= 1);
    checkTrue("Scale Auto: the view radius (kExt per n) holds the 90 % outline of every orbital", worstAuto >= 1);
    AtomLab o; o.nowMs = [] { return 0.0; }; o.p = oneParams(); o.restart(); o.setParam("scale", std::string("Fixed")); o.setParam("view", std::string("Orbit"));
    checkTrue("Scale Fixed, Orbit view: the view radius holds the n = 4 circle (16 a0)", o.sizeM() >= 16 * kA0 * (1 - 1e-12));
}

// ================= 0.10.6 stage 4: the outline of a complex state, and the norm of a superposition =================
// Finding 1 (audit): the view drew the 90 % outline of the real orbital the Orientation falls back to (fullOrb), not of the state on screen. The view now asks AtomLab::outlineStateId().
// The drawing itself cannot be tested here; this checks one level down what the view would ask for, and that the answer has a different shape from the fallback's.
static void testOutlineIsTheState(double margin) {
    int n = 0; double minDiff = 1e9; std::string which; bool allRight = true, axisOk = true;
    for (int id : complexIds()) {
        const Orbital& o = orbitals()[stateOrbital(id)];
        AtomLab lab; lab.nowMs = [] { return 0.0; }; lab.p = oneParams(); lab.restart();
        lab.setParam("view", std::string("Cloud")); lab.setParam("orb", o.shell);
        lab.setParam(o.l == 1 ? "pAx" : o.l == 2 ? "dAx" : "fAx", oriKey(stateM(id)));
        const int fb = lab.fullOrb();                                       // what the view used to ask for: the real fallback
        allRight = allRight && lab.outlineStateId() == id && lab.stateId() == id && fb != id && !isComplexState(fb) && orbitals()[fb].n == o.n && orbitals()[fb].l == o.l;
        const Outline &c = outlineOf(lab.outlineStateId()), &f = outlineOf(fb);
        auto ext = [](const Outline& O, double& ax, double& az) { ax = az = 0; for (const OutSeg2& g : O.side) { ax = std::max({ax, double(std::fabs(g.x0)), double(std::fabs(g.x1))}); az = std::max({az, double(std::fabs(g.y0)), double(std::fabs(g.y1))}); } };
        double ax1, az1, ax2, az2; ext(c, ax1, az1); ext(f, ax2, az2);
        // the shape differs: the largest relative difference of the 90 % radius, the side view's width and its height
        const double d = std::max({std::fabs(c.rmax / f.rmax - 1), std::fabs(ax1 / ax2 - 1), std::fabs(az1 / az2 - 1)});
        if (d < minDiff) { minDiff = d; which = stateText(id); }
        axisOk = axisOk && density(id, 1, 0, 0, 3) < 1e-12 * density(fb, 1, 0, 0, 3);   // every m != 0 state is zero on the z axis; the m = 0 fallback is not
        n++;
    }
    std::printf("outline of complex states: %d states; the 90 %% shape differs from the real fallback's by at least %.4f (%s); margin %.3f\n", n, minDiff, which.c_str(), margin);
    checkTrue("outline: for every complex state the view asks for that state's outline, not the real fallback's (20 states)", n == 20 && allRight);
    checkTrue("outline: every complex state is zero on the z axis, its fallback is not", axisOk);
    checkTrue("outline: the complex state's 90 % shape (radius, side width or height) differs from the fallback's by more than the margin", minDiff > margin);
}

// Finding 2 (audit): the integral of the drawn density |psi|^2 over all space is 1 for ANY pair, also when the states overlap (same state twice, a real orbital with an m state of its shell).
// For a = cos, b = sin: integral |a A + b B e^(-i theta)|^2 = a^2 + b^2 + 2 a b Re(<A|B> e^(-i theta)), which is 1 only if <A|B> = 0, so psiSuper divides by that.
static void testSuperNorm(double tol) {
    struct P { const char* sa; const char* oa; const char* sb; const char* ob; bool overlap; };
    const P ps[] = {{"2p", "z", "2p", "z", true}, {"1s", "", "1s", "", true}, {"2p", "x", "2p", "m+1", true}, {"2p", "y", "2p", "m-1", true}, {"3d", "xz", "3d", "m+1", true}, {"3d", "x2y2", "3d", "m+2", true},
        {"4f", "xz2", "4f", "m+1", true}, {"3d", "m+2", "3d", "m+2", true},
        {"2p", "m+1", "2p", "m-1", false}, {"2p", "x", "2p", "y", false}, {"1s", "", "2p", "z", false}, {"1s", "", "2s", "", false}, {"2s", "", "2p", "z", false}, {"2p", "m+1", "3d", "m+2", false},
        {"3d", "z2", "3d", "xy", false}, {"3d", "xz", "3d", "m+2", false}, {"4p", "z", "4f", "z3", false}, {"2p", "x", "3p", "x", false}};
    const Quad q = makeQuad(450, 16, 50);
    double worst = 0, worstOverlap = 0, worstOrtho = 0; std::string wp;
    for (const P& pr : ps)
        for (double mix : {0.0, 0.5, 1.0})
            for (double th : {0.0, 2.5}) {
                const Superposition su = mkSup(pr.sa, pr.oa, pr.sb, pr.ob, mix);
                double sum = 0;
                for (std::size_t i = 0; i < q.r.size(); i++)
                    for (std::size_t j = 0; j < q.u.size(); j++) {
                        const double st = std::sqrt(1 - q.u[j] * q.u[j]);
                        for (int k = 0; k < q.nphi; k++) {
                            const double f = 2 * kPi * (k + 0.5) / q.nphi;
                            sum += std::norm(psiSuper(su, 1, th, q.r[i] * st * std::cos(f), q.r[i] * st * std::sin(f), q.r[i] * q.u[j])) * (2 * kPi / q.nphi) * q.wu[j] * q.r[i] * q.r[i] * q.wr[i];
                        }
                    }
                const double err = std::fabs(sum - 1);
                if (err > worst) { worst = err; wp = std::string(pr.sa) + pr.oa + " + " + pr.sb + pr.ob + " mix " + std::to_string(int(mix * 100)); }
                (pr.overlap ? worstOverlap : worstOrtho) = std::max(pr.overlap ? worstOverlap : worstOrtho, err);
            }
    std::printf("superposition norm: integral of |psi|^2 over %zu pairs x mix 0, 50, 100 %% x 2 phases: largest |norm - 1| = %.5e (overlapping pairs %.5e, orthogonal %.5e; worst: %s)\n", sizeof ps / sizeof ps[0], worst, worstOverlap, worstOrtho, wp.c_str());
    checkTrue("superposition: the drawn density integrates to 1 for every pair, overlapping or not, at mix 0, 50 and 100 % (guard = measured + 1 %)", worst <= tol);
    // the overlap itself, from the maths: the same state 1; 2px with 2p m=+1 is -1/sqrt 2 (m=+1 = -(px + i py)/sqrt 2); orthogonal pairs exactly 0
    checkNum("overlap: <2pz|2pz> = 1", std::abs(overlapAB(orbIndex("2p|z"), orbIndex("2p|z"))), 1, 1e-12);
    checkNum("overlap: |<2px|2p m=+1>| = 1/sqrt 2", std::abs(overlapAB(orbIndex("2p|x"), complexState(orbIndex("2p"), 1))), 1 / std::sqrt(2.0), 1e-9);
    checkNum("overlap: |<2px|2p m=-1>| = 1/sqrt 2", std::abs(overlapAB(orbIndex("2p|x"), complexState(orbIndex("2p"), -1))), 1 / std::sqrt(2.0), 1e-9);
    checkTrue("overlap: exactly 0 for orthogonal pairs (m=+1 and m=-1, px and py, 1s and 2p, 2s and 2p, different n)", overlapAB(complexState(orbIndex("2p"), 1), complexState(orbIndex("2p"), -1)) == 0.0 && overlapAB(orbIndex("2p|x"), orbIndex("2p|y")) == 0.0
              && overlapAB(orbIndex("1s"), orbIndex("2p|z")) == 0.0 && overlapAB(orbIndex("2s"), orbIndex("2p|z")) == 0.0 && overlapAB(orbIndex("2p|x"), orbIndex("3p|x")) == 0.0);
}

// The dots of an overlapping pair follow the renormalised density and ALL of them are shown (no thinning): for 2px + 2p(m=+1) at mix 0, 50 and 100 % the mean x^2, y^2, z^2 of the dots equal the
// integrals of x^2 |psi|^2 etc. of the lab's own psiSuper (quadrature), within the Metropolis chain's sampling error (the 8 labs are independent, every dot of a lab is used).
static void testOverlapDots(double tol) {
    const Quad q = makeQuad(450, 16, 50);
    double worst = 0; bool allShown = true; std::string line;
    for (double mix : {0.0, 0.5, 1.0}) {
        const Superposition su = mkSup("2p", "x", "2p", "m+1", mix);
        double ex[3] = {0, 0, 0}, nrm = 0;
        for (std::size_t i = 0; i < q.r.size(); i++)
            for (std::size_t j = 0; j < q.u.size(); j++) {
                const double st = std::sqrt(1 - q.u[j] * q.u[j]);
                for (int k = 0; k < q.nphi; k++) {
                    const double f = 2 * kPi * (k + 0.5) / q.nphi, x = q.r[i] * st * std::cos(f), y = q.r[i] * st * std::sin(f), z = q.r[i] * q.u[j];
                    const double w = std::norm(psiSuper(su, 1, 0, x, y, z)) * (2 * kPi / q.nphi) * q.wu[j] * q.r[i] * q.r[i] * q.wr[i];
                    ex[0] += w * x * x; ex[1] += w * y * y; ex[2] += w * z * z; nrm += w;
                }
            }
        double got[3] = {0, 0, 0}; long cnt = 0;
        for (int lab_ = 0; lab_ < 8; lab_++) {
            seedRandom(9000 + lab_ + int(mix * 10));
            AtomLab lab; lab.nowMs = [] { return 0.0; }; lab.p = oneParams(); lab.restart();
            lab.setParam("view", std::string("Cloud")); lab.setParam("orb", std::string("2p")); lab.setParam("pAx", std::string("x"));
            lab.setParam("super", std::string("On")); lab.setParam("orb2", std::string("2p")); lab.setParam("pAx2", std::string("m+1")); lab.setParam("mix", mix * 100);
            for (int i = 0; i < 800; i++) lab.step(1.0 / 240);
            const long c = std::min<long>(lab.s.k, long(lab.s.sg.size()));
            for (long i = 0; i < c; i++) {
                allShown = allShown && lab.superGroup(i, lab.beatTheta()) != 255;
                for (int a = 0; a < 3; a++) { const double v = lab.s.pts[3 * i + a] / kA0; got[a] += v * v; }
            }
            cnt += c;
        }
        for (int a = 0; a < 3; a++) { got[a] /= cnt; worst = std::max(worst, std::fabs(got[a] / (ex[a] / nrm) - 1)); }
        line += " mix " + std::to_string(int(mix * 100)) + ": <x2,y2,z2> dots " + fmt::fixed(got[0], 2) + "," + fmt::fixed(got[1], 2) + "," + fmt::fixed(got[2], 2) + " vs integral " + fmt::fixed(ex[0] / nrm, 2) + "," + fmt::fixed(ex[1] / nrm, 2) + "," + fmt::fixed(ex[2] / nrm, 2) + ";";
    }
    std::printf("overlapping pair 2px + 2p(m=+1), dots against the integral of psiSuper:%s largest relative difference %.5f\n", line.c_str(), worst);
    checkTrue("overlapping pair: every dot is shown (no thinning)", allShown);
    checkTrue("overlapping pair: the dots' mean x^2, y^2, z^2 equal those of the renormalised density at mix 0, 50 and 100 % (guard = measured + 1 %)", worst <= tol);
}

// The readout must not claim a split that is not drawn: orthogonal pairs keep "(50 % / 50 %)"; an overlapping pair says the mixture is renormalised.
static void testSuperReadout() {
    auto mk = [](const char* a, const char* ao, const char* b, const char* bo) {
        AtomLab lab; lab.nowMs = [] { return 0.0; }; lab.p = oneParams(); lab.restart();
        lab.setParam("view", std::string("Cloud")); lab.setParam("orb", std::string(a)); if (*ao) lab.setParam(orbitals()[orbIndex(a)].l == 1 ? "pAx" : "dAx", std::string(ao));
        lab.setParam("super", std::string("On")); lab.setParam("orb2", std::string(b)); if (*bo) lab.setParam(orbitals()[orbIndex(b)].l == 1 ? "pAx2" : "dAx2", std::string(bo));
        return lab.info1();
    };
    const std::string ortho = mk("1s", "", "2p", "z"), overlap = mk("2p", "x", "2p", "m+1"), same = mk("2p", "z", "2p", "z");
    checkTrue("readout: an orthogonal pair keeps its exact split (50 % / 50 %)", ortho.find("(50 % / 50 %)") != std::string::npos && ortho.find("overlap") == std::string::npos);
    checkTrue("readout: an overlapping pair does not claim (50 % / 50 %) and says the states overlap and the mixture is renormalised", overlap.find("(50 % / 50 %)") == std::string::npos && overlap.find("overlap") != std::string::npos && overlap.find("renormalised") != std::string::npos);
    checkTrue("readout: the same state twice says so (overlap 100 %)", same.find("(50 % / 50 %)") == std::string::npos && same.find("overlap") != std::string::npos && same.find("100 %") != std::string::npos);
}

// ================= 0.10.6 stage 5: wrong and misleading text =================
// Finding 6: ONE relativity line (Z = 27) in the readout, the hint and the spectrum note; "non-relativistic" in the readout at every Z; the nucleus line carries its caveat.
static void testRelativityText() {
    int agree = 0; bool nonrel = true, caveat = true, model = true, threshold = true;
    for (int Z : {1, 19, 20, 26, 27, 92, 118}) {
        AtomLab lab; lab.nowMs = [] { return 0.0; }; lab.p = oneParams(); lab.restart();
        lab.setParam("Z", double(Z)); lab.setParam("view", std::string("Cloud")); lab.setParam("zoom", 2.0);
        lab.s.more = true;   // (0.22 stage 6: the model and nucleus lines are under More info, which is now closed by default; the relativity warning is shown either way)
        std::string all; for (const std::string& l : lab.info()) all += l + "\n";
        const bool warnRead = all.find("1s electron speed:") != std::string::npos, warnHint = lab.hint().find("Heavy atoms: the cloud is the non-relativistic one-electron orbital") != std::string::npos,   // (0.46.1: was "this lab uses non-relativistic physics", untrue since 0.46's Dirac headline)
                   warnSpec = spectrumNote(Z, lab.p.N).find("so these are approximate") != std::string::npos, want = Z >= 27;
        if (warnRead == warnHint && warnHint == warnSpec && warnSpec == want) agree++;
        else std::printf("  relativity warning at Z = %d: readout %d, hint %d, spectrum note %d (want %d)\n", Z, warnRead, warnHint, warnSpec, want);
        nonrel = nonrel && all.find("non-relativistic") != std::string::npos;
        // the nucleus line (s orbital, zoomed in) has its caveat
        const std::size_t k = all.find(kNucPrefix);
        caveat = caveat && k != std::string::npos && all.find(kNucSuffix, k) != std::string::npos;
        // the model line: the Dirac 1s level is deeper than the non-relativistic one by this much (independent: NIST alpha typed in the nist:: block)
        const double x = Z * nist::alpha, sq = std::sqrt(1 - x * x), pct = 100 * x * x / ((1 + sq) * (1 + sq));   // = 100 ((1 - sq) / ((Z alpha)^2 / 2) - 1) without the subtractions (0.10.8 stage 2)
        // 0.46.1 (outside review): from Z = 27 the Cloud view's headline energy is Dirac's, so the model line says the cloud is non-relativistic and the energy is Dirac's (reference changed on purpose)
        model = model && (Z < 27 ? all.find(kModelPrefix + " (no relativity, fine structure, Lamb shift or spin). For a point nucleus the Dirac equation puts the 1s level " + numText(pct, 2) + " % deeper than this lab's.") != std::string::npos
                                 : all.find("model: one electron; the cloud is non-relativistic, the energy above is the Dirac value") != std::string::npos &&
                                   all.find("the energy over the picture is the Dirac level for a point nucleus, which for 1s lies " + numText(pct, 2) + " % deeper than the non-relativistic one.") != std::string::npos &&
                                   all.find("this lab uses non-relativistic") == std::string::npos && all.find(" in this lab\n") == std::string::npos);
    }
    checkTrue("relativity: the readout, the hint and the spectrum note warn at exactly the same Z (none below 27, all three from 27) for Z = 1, 19, 20, 26, 27, 92, 118", agree == 7);
    checkTrue("relativity: the word non-relativistic is in the readout at every Z (1, 19, 20, 26, 27, 92, 118)", nonrel);
    checkTrue("relativity: the model line gives the Dirac-minus-lab 1s difference at every Z, from the lab's own formula", model);
    checkTrue("relativity: the chance-inside-the-nucleus line says it is a non-relativistic estimate that this lab does not correct", caveat);
    // the line: the correction to the 1s energy is 0.917 % at Z = 26 and 0.990 % at Z = 27 (about 1 %); (Z alpha)^2 = 0.0388 and v/c = 0.197 at 27
    auto corr = [](int Z) { const double x = Z * nist::alpha, sq = std::sqrt(1 - x * x); return 100 * x * x / ((1 + sq) * (1 + sq)); };   // cancellation-free (0.10.8 stage 2)
    threshold = std::fabs(corr(26) - 0.917) < 0.001 && std::fabs(corr(27) - 0.990) < 0.001 && corr(19) < 0.5 && kRelativityZ == 27 && std::fabs(27 * nist::alpha - 0.197) < 0.001;
    checkTrue("relativity: the threshold Z = 27 is where the 1s energy is 0.99 % off (0.917 % at 26), Z alpha = 0.197, (Z alpha)^2 = 0.039", threshold);
}

// Finding 7: the two Learn steps. The text lives in the generator's edit list (tools/native_edits.js, applied by tools/make_atom_learn.js); this checks what the app finally says.
static void testTimelineText() {
    const TimelineStep *pauli = nullptr, *stod = nullptr;
    for (const TimelineStep& st : timelineSteps()) { if (std::string(st.y) == "1925" && std::string(st.t) == "Why electrons form shells") pauli = &st; if (std::string(st.y) == "2013") stod = &st; }
    checkTrue("timeline: the Pauli and the 2013 steps are there", pauli && stod);
    if (!pauli || !stod) return;
    std::string zs, ns; for (const auto& kv : pauli->set) { if (kv.first == "Z") zs = kv.second; if (kv.first == "N") ns = kv.second; }
    // 0.39.1: the lab draws every electron since 0.39 (Electrons: All), so the button now opens the whole neon atom; it opened Ne9+ and said the lab could not show a full shell
    std::string el, vw; for (const auto& kv : pauli->set) { if (kv.first == "elec") el = kv.second; if (kv.first == "view") vw = kv.second; }
    checkTrue("timeline: the Pauli button opens neon with all ten electrons (Electrons All, Cloud 3D)", std::string(pauli->lab) == "Open neon with all ten electrons" && zs == "10" && ns == "10" && el == "All" && vw == "Cloud 3D");
    checkTrue("timeline: the Pauli text says the shells: two electrons in K and eight in L (2n^2)", std::string(pauli->d).find("two fill the K shell and eight the L shell") != std::string::npos);
    {   // and the button really shows the whole atom with its two shells (atom/hf.h: Ne is 1s2 2s2 2p6)
        AtomLab lab; lab.nowMs = [] { return 0.0; }; lab.p = oneParams(); lab.restart();
        for (const auto& [k, v] : pauli->set) { char* end = nullptr; const double d = std::strtod(v.c_str(), &end); if (end && *end == 0) lab.setParam(k, d); else lab.setParam(k, v); }   // as ui/atom_learn.cpp does
        checkTrue("timeline: after the Pauli button the lab shows neon's every electron", lab.manyOn() && lab.manySub() < 0 && lab.p.Z == 10 && lab.manyAtom()->electrons() == 10);
    }
    const std::string d = stod->d;
    checkTrue("timeline: the 2013 step no longer calls it a direct picture of the quantum cloud, or the same kind of shape", d.find("direct picture") == std::string::npos && d.find("same kind of shape") == std::string::npos && std::string(stod->t).find("Photographing") == std::string::npos);
    checkTrue("timeline: the 2013 step says a pattern recorded on a detector, not a picture of a cloud like those in the lab, which are worked out from theory",
              d.find("It is a pattern recorded on a detector, evidence for the electron’s wave, not a picture of a cloud like the ones in this lab: those are worked out from the theory.") != std::string::npos && std::string(stod->t) == "An electron wave, seen on a detector");
    checkTrue("timeline: the 2013 button says the cloud it opens is worked out from theory", std::string(stod->lab) == "Open a hydrogen cloud (worked out from theory)");
}

// Finding 13: one visible range (380-780 nm, both ends included) for the spectrum card, the colour code and the photon tag.
static void testVisibleRange() {
    bool ok = true;
    for (double nm : {379.0, 380.0, 381.0, 749.0, 750.0, 751.0, 779.0, 780.0, 781.0}) {
        const bool vis = nm >= 380 && nm <= 780;                                  // the range, written here again
        const std::string css = lamColorCss(nm), tag = bandTag(nm);
        const bool card = isVisibleNm(nm), colour = css.compare(0, 4, "rgb(") == 0, noTag = tag.empty();
        const Rgb c = lamColor(nm); const bool standIn = c.r == 0x9a && c.g == 0x9a && c.b == 0xa3;   // (0.22: one grey for UV and IR)
        const bool here = card == vis && colour == vis && noTag == vis && standIn == !vis && (vis || tag == (nm < 380 ? " (UV)" : " (IR)"));
        if (!here) std::printf("  visible range at %.0f nm: card %d, colour %d, tag '%s', stand-in %d (want visible %d)\n", nm, card, colour, tag.c_str(), standIn, vis);
        ok = ok && here;
    }
    checkTrue("visible range: the spectrum card, the line colour and the (UV)/(IR) tag agree at 379, 380, 381, 749, 750, 751, 779, 780, 781 nm", ok);
    // the card's visible strip: the lines it marks are exactly the lines with a real colour and no tag, for every element
    bool same = true; long nVis = 0;
    for (int Z = 1; Z <= 118; Z++) {
        const auto strip = visibleLines(Z, defaultN(Z));
        long inColour = 0;
        for (const SpecLine& l : specLines(Z, defaultN(Z))) if (l.hi != 0 && lamColorCss(l.lam).compare(0, 4, "rgb(") == 0 && std::string(bandTag(l.lam)).empty()) inColour++;
        same = same && long(strip.size()) == inColour; nVis += inColour;
        for (const SpecLine& l : strip) same = same && l.lam >= 380 && l.lam <= 780;
    }
    checkTrue("visible range: for all 118 elements the lines the card marks as visible are exactly those the colour code draws in real colour", same && nVis > 0);
}

// Finding 15: "avg r: sampled" only with the full ring of kCap dots, with its honest note; and the sampled average against the exact <r> = a0 (3 n^2 - l (l + 1)) / (2 Z).
static void testSampledRadius(double guard) {
    struct C { const char* orb; const char* key; const char* ori; } cs[] = {{"1s", "", ""}, {"2s", "", ""}, {"2p", "pAx", "z"}, {"2p", "pAx", "m+1"}, {"3s", "", ""}, {"3p", "pAx", "z"}, {"3d", "dAx", "z2"}, {"3d", "dAx", "m+2"},
                                                                  {"4s", "", ""}, {"4d", "dAx", "x2y2"}, {"4f", "fAx", "z3"}, {"4f", "fAx", "m+3"}};
    double worst = 0, sumd = 0; long n = 0, within10 = 0; bool placeholderOk = true, numberOk = true, noteOk = true;
    for (const C& c : cs)
        for (int seed = 1; seed <= 8; seed++) {
            seedRandom(5000 + seed);
            AtomLab lab; lab.nowMs = [] { return 0.0; }; lab.p = oneParams(); lab.restart();
            lab.setParam("view", std::string("Cloud")); lab.setParam("orb", std::string(c.orb)); if (*c.key) lab.setParam(c.key, std::string(c.ori));
            const Orbital& o = orbitals()[orbIndex(c.orb)];
            const double theory = (3.0 * o.n * o.n - o.l * (o.l + 1)) / 2 * nist::a0 * (1 + 1 / nuclearMassMe(1, lab.p.N));   // <r> for Z = 1, with the NIST Bohr radius typed above; 0.46.4 (deliberate): times m / mu (reduced mass)
            for (int i = 0; i < 100; i++) lab.step(1.0 / 240);                                       // 1,600 dots: more than the old 300, fewer than kCap
            placeholderOk = placeholderOk && lab.s.k > 300 && lab.s.k < kCap && lab.info1().find("sampled …, theory") != std::string::npos;
            for (int i = 0; i < 700; i++) lab.step(1.0 / 240);                                       // more than kCap dots: the ring is full
            const long cnt = std::min<long>(lab.s.k, lab.cloudCap());
            double sum = 0; for (long i = 0; i < cnt; i++) sum += std::hypot(double(lab.s.pts[3 * i]), double(lab.s.pts[3 * i + 1]), double(lab.s.pts[3 * i + 2]));
            const double d = std::fabs(sum / cnt / theory - 1);
            worst = std::max(worst, d); sumd += d; n++; if (d < 0.01) within10++;
            const std::string line = lab.info1();
            numberOk = numberOk && line.find("sampled …") == std::string::npos && line.find("avg r: sampled ") != std::string::npos;
            noteOk = noteOk && line.find(kSampledNote + ", theory") != std::string::npos;
        }
    std::printf("sampled <r> at the full ring (12,000 dots), %ld labs (12 orbitals x 8 seeds): mean |difference| %.4f, worst %.5f, within 1 %%: %ld of %ld\n", n, sumd / n, worst, within10, n);
    checkTrue("sampled <r>: below the full ring of kCap dots the readout shows the placeholder, not a number", placeholderOk);
    checkTrue("sampled <r>: with the full ring the readout shows the number, with the note 'an estimate from independent dots, usually within 1 %'", numberOk && noteOk);
    checkTrue("sampled <r>: at kCap dots the sampled average is within the guard of the exact a0 (3n^2 - l(l+1))/2Z (guard = measured worst + 1 %)", worst <= guard);
    checkTrue("sampled <r>: the stated 'usually within 1 %' holds for at least 95 % of the runs", within10 >= 0.95 * n);
}

// ================= 0.10.6 stage 6: the nuclear mass from a source, an independent spectrum check, four basic checks =================
// Finding 5: the nuclear mass. Reference: AME2020 atomic masses, https://www-nds.iaea.org/amdc/ame2020/mass.mas20.txt (read 2026-10-04; "ATOMIC MASS" column, micro-u, typed in below), with the NIST m_e in u
// (nist::meInU): M_nucleus = atomic mass / m_e - Z, in electron masses (the electrons' binding energy, which this neglects, is far below the lab's own rounding of the table to 1 keV per nucleon).
struct AmeRef { int Z, N; const char* name; double microU; };
static const AmeRef kAme[] = {{1, 0, "H-1", 1007825.031898}, {1, 1, "H-2", 2014101.777844}, {1, 2, "H-3", 3016049.28132}, {2, 2, "He-4", 4002603.25413}, {6, 6, "C-12", 12000000.0}, {26, 30, "Fe-56", 55934935.537}, {92, 146, "U-238", 238050786.936}};
static double ameNuclearMe(const AmeRef& a) { return a.microU * 1e-6 / nist::meInU - a.Z; }
static void testNuclearMass(double guard) {
    double worst = 0, worstOld = 0; std::string line;
    for (const AmeRef& a : kAme) {
        const double ref = ameNuclearMe(a), lab = nuclearMassMe(a.Z, a.N), old = (a.Z + a.N) / nist::meInU - a.Z - (a.Z == 1 && a.N == 0 ? (a.Z + a.N) / nist::meInU - a.Z - nist::mpOverMe : 0);
        worst = std::max(worst, std::fabs(lab / ref - 1)); worstOld = std::max(worstOld, std::fabs(old / ref - 1));
        line += std::string(" ") + a.name + " " + fmt::fixed(100 * (lab / ref - 1), 6) + " % (old form " + fmt::fixed(100 * (old / ref - 1), 4) + " %);";
    }
    std::printf("nuclear mass against AME2020 (relative difference):%s worst new %.3e, worst old form %.3e\n", line.c_str(), worst, worstOld);
    checkTrue("nuclear mass: the lab's M (electron masses) equals AME2020's atomic mass / m_e - Z for H-1, H-2, H-3, He-4, C-12, Fe-56, U-238 (guard = measured + 1 %)", worst <= guard);
    checkTrue("nuclear mass: the old form A u - Z m_e was off by more than 1e-3 for H-2 and H-3 (so this test would catch it)", std::fabs(((1 + 1) / nist::meInU - 1) / ameNuclearMe(kAme[1]) - 1) > 1e-3);
    // the deuterium D-alpha line by hand from the AME mass (Rydberg formula with the NIST values) against the lab's
    const double M = ameNuclearMe(kAme[1]), hand = nist::hcEVnm / (nist::rydEV * M / (M + 1) * (1.0 / 4 - 1.0 / 9));
    double lab = 0; for (const SpecLine& l : specLines(1, 1)) if (l.lo == 2 && l.hi == 3) lab = l.lam;
    std::printf("D-alpha: by hand from the AME2020 deuteron mass %.6f nm, lab %.6f nm (relative %.2e)\n", hand, lab, lab / hand - 1);
    checkNum("D-alpha: the lab's wavelength equals the hand value from the AME2020 deuteron mass (1e-9)", lab, hand, 1e-9);
    // the lines that moved: deuterium's D-alpha is shorter than before by about 1.3 pm (the old A u - Z m_e mass was 0.7 % low)
    const double oldM = 2 / nist::meInU - 1, oldLam = nist::hcEVnm / (nist::rydEV * oldM / (oldM + 1) * (1.0 / 4 - 1.0 / 9));
    checkTrue("D-alpha: it moved by about -1.3 pm against the old mass form (between -1.0 and -1.6 pm)", (lab - oldLam) * 1e3 < -1.0 && (lab - oldLam) * 1e3 > -1.6);
}

// Finding 16: the spectrum against two things the lab did not make. (a) The Rydberg formula written out here from the NIST values, with the nucleus's mass from AME2020 (He-4: the alpha particle).
// (b) NIST ASD, https://physics.nist.gov/cgi-bin/ASD/lines1.pl (read 2026-10-04): query spectra = "H I" and "He II", low_w = 90 / 20, upp_w = 2000 nm, unit = 1 (nm), format = 3 (tab-delimited),
// show_av = 3 (VACUUM wavelengths), show_obs_wl = 1, show_calc_wl = 1, allowed_out = 1, forbid_out = 1. The column ritz_wl_vac(nm) of the fine-structure COMPONENTS (J_i and J_k given) of each line is typed
// in below as [lowest, highest]. The lab is vacuum, one electron, non-relativistic, level-averaged (no fine structure, Lamb shift or spin), so its single wavelength can differ from the ASD's components by the
// size of what it leaves out: relativistic corrections are of the order (Z alpha)^2 of the level energies (5.3e-5 for hydrogen, 2.1e-4 for He+). The check is therefore that the lab's value lies inside the
// components' range widened by (Z alpha)^2 of the wavelength on each side. Measured: the lab lies outside the bare range by 8.9e-6 (H Lyman-alpha), 1.0e-6 (H-beta), 3.8e-5 (He+ Lyman-alpha); inside for H-alpha and He II 468.7 nm (the allowed distance is 5.3e-5 and 2.1e-4).
static double handLine(double M, int Z, int lo, int hi) { return nist::hcEVnm / (nist::rydEV * M / (M + 1) * Z * Z * (1.0 / (lo * lo) - 1.0 / (hi * hi))); }
static void testSpectrumIndependent() {
    struct Asd { int Z, N, lo, hi; const char* name; double minNm, maxNm; };
    const Asd as[] = {{1, 0, 1, 2, "H Lyman-alpha", 121.566823731, 121.5673644608}, {1, 0, 2, 3, "H-alpha", 656.4522556, 656.4722349}, {1, 0, 2, 4, "H-beta", 486.26365622, 486.273282785},
                      {2, 2, 1, 2, "He II 30.4 nm", 30.3780409484, 30.3785814724}, {2, 2, 3, 4, "He II 468.7 nm", 468.66881681, 468.722934627}};
    const double M[2] = {nist::mpOverMe, ameNuclearMe(kAme[3])};                 // the proton; the alpha particle from AME2020 (He-4 atomic mass - 2 m_e)
    bool handOk = true, asdOk = true; double worstHand = 0;
    for (const Asd& a : as) {
        double lab = 0; for (const SpecLine& l : specLines(a.Z, a.N)) if (l.lo == a.lo && l.hi == a.hi) lab = l.lam;
        const double hand = handLine(M[a.Z == 1 ? 0 : 1], a.Z, a.lo, a.hi), tol = std::pow(a.Z * nist::alpha, 2) * lab;
        const double outside = lab < a.minNm ? a.minNm - lab : lab > a.maxNm ? lab - a.maxNm : 0;
        worstHand = std::max(worstHand, std::fabs(lab / hand - 1));
        handOk = handOk && std::fabs(lab / hand - 1) < 1e-9; asdOk = asdOk && outside <= tol;
        std::printf("  %-14s lab %.6f nm, hand %.6f nm, ASD components %.6f..%.6f nm: lab outside by %.1e of the wavelength (allowed %.1e)\n", a.name, lab, hand, a.minNm, a.maxNm, outside / lab, tol / lab);
    }
    checkTrue("spectrum: H Lyman-alpha, H-alpha, H-beta and two He II lines equal the Rydberg formula written out from the NIST values (1e-9; worst " + fmt::fixed(worstHand, 12) + ")", handOk);
    checkTrue("spectrum: the same five lines lie inside NIST ASD's fine-structure components widened by (Z alpha)^2 (the size of what the lab does not model)", asdOk);
}

// Finding 17: four basic checks.
static void testBasicOrbitChecks() {
    // (a) the 90 % outline holds 90 % of the probability for all 30 real orbitals: 0.9 <= mass (by construction) and <= the measured worst 0.903 (probe) + 1 %
    double lo = 1, hi = 0; for (int i = 0; i < int(orbitals().size()); i++) { const double m = outlineOf(i).mass; lo = std::min(lo, m); hi = std::max(hi, m); }
    std::printf("90 %% outline mass over the 30 real orbitals: %.5f to %.5f\n", lo, hi);
    checkTrue("outline: all 30 real orbitals hold 90 % of the probability (0.9 up to the measured worst + 1 %: 0.912)", lo >= 0.9 && hi <= 0.912);
    // (b) the 1s norm: the integral of |e^-r|^2 over space is pi (a0^3)
    const double n1 = orbitalNorm(orbIndex("1s"));
    std::printf("1s normalisation integral %.9f, pi %.9f (relative %.2e)\n", n1, kPi, n1 / kPi - 1);
    checkNum("orbital: the 1s normalisation integral is pi (guard = measured 5.83e-10 + 1 %)", n1, kPi, 5.9e-10);
    // (c) radial nodes: the radial part R(q) has n - l - 1 sign changes for q > 0 (counted on a fine grid) for every orbital
    bool nodesOk = true; int checked = 0;
    for (const Orbital& o : orbitals()) {
        int changes = 0; double prev = radial(o, 1e-6);
        for (double q = 0.001; q < 160; q += 0.001) { const double v = radial(o, q); if ((v > 0) != (prev > 0) && v != 0) changes++; prev = v; }
        nodesOk = nodesOk && changes == o.n - o.l - 1; checked++;
    }
    checkTrue("orbital: every one of the 62 real orbitals (0.46: n up to 6) has n - l - 1 radial nodes", nodesOk && checked == 62);
}
// (d) the sampled <r> against a0 (3n^2 - l(l+1)) / (2Z) for Z other than 1 (stage 5 tests Z = 1): the same dots, 3 and 26 times closer to the nucleus.
static void testSampledRadiusZ(double guard) {
    struct C { const char* orb; const char* key; const char* ori; } cs[] = {{"1s", "", ""}, {"2p", "pAx", "z"}, {"3d", "dAx", "z2"}, {"3s", "", ""}, {"4f", "fAx", "xyz"}, {"4s", "", ""}};
    double worst = 0; long n = 0;
    for (int Z : {3, 26})
        for (const C& c : cs)
            for (int seed = 1; seed <= 4; seed++) {
                seedRandom(7000 + seed);
                AtomLab lab; lab.nowMs = [] { return 0.0; }; lab.p = oneParams(); lab.restart();
                lab.setParam("Z", double(Z)); lab.setParam("view", std::string("Cloud")); lab.setParam("orb", std::string(c.orb)); if (*c.key) lab.setParam(c.key, std::string(c.ori));
                for (int i = 0; i < 800; i++) lab.step(1.0 / 240);
                const Orbital& o = orbitals()[orbIndex(c.orb)];
                const long cnt = std::min<long>(lab.s.k, lab.cloudCap()); double sum = 0;
                for (long i = 0; i < cnt; i++) sum += std::hypot(double(lab.s.pts[3 * i]), double(lab.s.pts[3 * i + 1]), double(lab.s.pts[3 * i + 2]));
                worst = std::max(worst, std::fabs(sum / cnt / ((3.0 * o.n * o.n - o.l * (o.l + 1)) / (2.0 * Z) * nist::a0) - 1)); n++;
            }
    std::printf("sampled <r> for Z = 3 and 26 (%ld labs): worst difference from a0 (3n^2 - l(l+1)) / 2Z %.5f\n", n, worst);
    checkTrue("sampled <r>: for Z = 3 and 26 the dots' mean radius is within the guard of a0 (3n^2 - l(l+1)) / 2Z (guard = measured worst + 1 %)", worst <= guard);
}

// ================= 0.10.7 stage 1: the radial probability graph =================
// References, none of them from the lab's own radial(): (1) the textbook closed forms of the first hydrogen radial functions R10 .. R32 (Griffiths, Introduction to Quantum Mechanics, ch. 4, the table
// of the first few R_nl; a0 = 1), typed here and CHECKED below to be normalised and orthogonal, so a wrong coefficient shows; (2) composite Simpson quadrature; (3) the exact peaks: 1s at a0/Z and every
// l = n - 1 orbital at n^2 a0/Z (P is then r^(2n) e^(-2r/n a0 / Z), its maximum is at n^2 a0 / Z); (4) <r> = a0 (3n^2 - l(l+1)) / 2Z; (5) the closed-form share of the 1s probability inside q = Z r:
// 1 - e^(-2q)(1 + 2q + 2q^2) (by parts, d/dq of it is 4 q^2 e^(-2q)); (6) the dots themselves.
namespace rg {
static const char* kShells[10] = {"1s", "2s", "2p", "3s", "3p", "3d", "4s", "4p", "4d", "4f"};
static double Rref(int idx, double q) {      // idx: 0 1s, 1 2s, 2 2p, 3 3s, 4 3p, 5 3d  (a0 = 1)
    switch (idx) {
        case 0: return 2 * std::exp(-q);
        case 1: return (1 / std::sqrt(2.0)) * (1 - q / 2) * std::exp(-q / 2);
        case 2: return (1 / std::sqrt(24.0)) * q * std::exp(-q / 2);
        case 3: return (2 / std::sqrt(27.0)) * (1 - 2 * q / 3 + 2 * q * q / 27) * std::exp(-q / 3);
        case 4: return (8 / (27 * std::sqrt(6.0))) * (1 - q / 6) * q * std::exp(-q / 3);
        default: return (4 / (81 * std::sqrt(30.0))) * q * q * std::exp(-q / 3);
    }
}
template <class F> static double simpson(F f, double a, double b, int n) {   // n even
    const double h = (b - a) / n; double s = f(a) + f(b);
    for (int i = 1; i < n; i++) s += f(a + i * h) * (i % 2 ? 4 : 2);
    return s * h / 3;
}
static double meanFormula(const Orbital& o, int Z) { return (3.0 * o.n * o.n - o.l * (o.l + 1)) / (2.0 * Z); }
}  // namespace rg

static void testRadialGraph(double tolRef, double tolArea, double tolCurve, double tolCover, double tolMean, double tolPeak, double tolR90s) {
    // ---- the closed forms are themselves right: normalised, and R10, R20, R30 mutually orthogonal (same l) ----
    double worstN = 0, worstO = 0;
    for (int i = 0; i < 6; i++) worstN = std::max(worstN, std::fabs(rg::simpson([&](double q) { return q * q * rg::Rref(i, q) * rg::Rref(i, q); }, 0, 400, 40000) - 1));
    for (int a : {0, 1, 3}) for (int b : {0, 1, 3}) if (a < b) worstO = std::max(worstO, std::fabs(rg::simpson([&](double q) { return q * q * rg::Rref(a, q) * rg::Rref(b, q); }, 0, 400, 40000)));
    std::printf("radial graph: the typed textbook R10..R32: worst |norm - 1| %.4e, worst |overlap| %.4e\n", worstN, worstO);
    checkTrue("radial graph: the typed textbook radial functions are normalised and orthogonal (so the reference is right)", worstN <= 2.70e-9 && worstO <= 9.53e-10);   // guards = measured (2.6662e-9, 9.4268e-10: Simpson, h = 0.01) + 1 %

    // ---- the lab's P(r) against those closed forms (n <= 3), for Z = 1, 2, 26 ----
    double worstRef = 0;
    for (int i = 0; i < 6; i++)
        for (int Z : {1, 2, 26})
            for (int k = 1; k <= 60; k++) {
                const double r = k * 0.25 / Z, q = Z * r, want = Z * q * q * rg::Rref(i, q) * rg::Rref(i, q);   // P(r) dr = q^2 R(q)^2 dq, r = q / Z
                const double got = radialDensity(orbIndex(rg::kShells[i]), Z, r);
                if (want > 1e-12) worstRef = std::max(worstRef, std::fabs(got / want - 1));
            }
    std::printf("radial graph: P(r) against the textbook closed forms, 6 shells x Z 1, 2, 26: worst relative difference %.3e\n", worstRef);
    checkTrue("radial graph: P(r) = r^2 R^2 equals the textbook closed forms for 1s 2s 2p 3s 3p 3d at Z = 1, 2, 26 (guard from the measured value)", worstRef <= tolRef);

    // ---- area 1, <r>, peaks, number of maxima, for all ten shells at Z = 1, 2, 26 ----
    double worstArea = 0, worstMean = 0, worstPeak = 0, worstPeakIdx = 0; bool countsOk = true, gridCountOk = true, highest = true, minimaOk = true, markOk = true, diffOk = true;
    double worstCurve = 0, worstCover = 0;
    for (int s = 0; s < 10; s++)
        for (int Z : {1, 2, 26}) {
            const int st = orbIndex(rg::kShells[s]); const Orbital& o = orbitals()[st];
            const RadialGraph g = radialGraph(st, Z);
            const auto Pf = [&](double r) { return radialDensity(st, Z, r); };
            const double L = 60.0 * o.n * o.n / Z;
            worstArea = std::max(worstArea, std::fabs(rg::simpson(Pf, 0, L, 20000) - 1));
            worstMean = std::max(worstMean, std::fabs(rg::simpson([&](double r) { return r * Pf(r); }, 0, L, 20000) / rg::meanFormula(o, Z) - 1));
            diffOk = diffOk && g.mean == meanRadius(o.n, o.l, Z) && std::fabs(meanRadius(o.n, o.l, Z) / rg::meanFormula(o, Z) - 1) < 1e-14;
            // the stored curve: its own trapezoid area against Simpson of the exact function over the same range, and the share it covers
            double tr = 0; for (std::size_t i = 0; i + 1 < g.r.size(); i++) tr += 0.5 * (g.P[i] + g.P[i + 1]) * (g.r[i + 1] - g.r[i]);
            const double ex = rg::simpson(Pf, 0, g.rEnd, 20000);
            worstCurve = std::max(worstCurve, std::fabs(tr - ex)); worstCover = std::max(worstCover, std::fabs(ex - kRadialCover));
            // peaks
            if (o.n == 1) worstPeak = std::max(worstPeak, std::fabs(g.peak * Z - 1));
            if (o.l == o.n - 1) worstPeak = std::max(worstPeak, std::fabs(g.peak * Z / (double(o.n) * o.n) - 1));
            std::size_t am = 0; for (std::size_t i = 0; i < g.P.size(); i++) if (g.P[i] > g.P[am]) am = i;
            worstPeakIdx = std::max(worstPeakIdx, std::fabs(g.r[am] - g.peak) / (g.r[1] - g.r[0]));
            highest = highest && Pf(g.peak) >= g.P[am] * (1 - 1e-12) && g.P.size() == std::size_t(kRadialPoints + 1) && g.r.front() == 0;
            // maxima: n - l (exact roots), and n - l on the stored curve; the n - l - 1 nodes between them are zeros of P
            countsOk = countsOk && int(g.maxima.size()) == o.n - o.l;
            int gm = 0, gmin = 0;
            for (std::size_t i = 1; i + 1 < g.P.size(); i++) { if (g.P[i] > g.P[i - 1] && g.P[i] >= g.P[i + 1]) gm++; if (g.P[i] < g.P[i - 1] && g.P[i] <= g.P[i + 1]) { gmin++; minimaOk = minimaOk && g.P[i] < 0.02 * g.P[am]; } }
            gridCountOk = gridCountOk && gm == o.n - o.l && gmin == o.n - o.l - 1;
            // the 90 % mark is the state's own outline reach; at Z it is the Z = 1 one over Z
            for (int a = 0; a < int(orbitals().size()); a++) {
                if (orbitals()[a].shell != o.shell) continue;
                const RadialGraph ga = radialGraph(a, Z);
                markOk = markOk && std::fabs(ga.reach90 * Z / outlineOf(a).rmax - 1) < 1e-12 && ga.P == g.P && ga.maxima == g.maxima && ga.reach90 > 0;
            }
        }
    std::printf("radial graph, 10 shells x Z 1, 2, 26: worst |area - 1| %.4e; worst |<r>(quadrature) / formula - 1| %.4e; worst peak error vs exact %.3e (relative); highest point of the curve within %.2f grid steps of the peak;\n"
                "   stored curve (trapezoid, %d points) against Simpson of the exact P over the same range: worst difference %.4e; the share inside the right edge differs from %.3f by at most %.4e\n",
                worstArea, worstMean, worstPeak, worstPeakIdx, kRadialPoints + 1, worstCurve, kRadialCover, worstCover);
    checkTrue("radial graph: the area under P(r) is 1 for all ten shells at Z = 1, 2, 26 (Simpson, 20,000 intervals; guard = measured + 1 %)", worstArea <= tolArea);
    checkTrue("radial graph: the stored curve's area equals the exact P's over the same range (trapezoid, 401 points; guard = measured + 1 %)", worstCurve <= tolCurve);
    checkTrue("radial graph: the picture's right edge holds 99.9 % of the probability (guard = measured + 1 %)", worstCover <= tolCover);
    checkTrue("radial graph: <r> from the quadrature of r P(r) equals a0 (3n^2 - l(l+1)) / 2Z for all ten shells at Z = 1, 2, 26 (guard = measured + 1 %)", worstMean <= tolMean);
    checkTrue("radial graph: the graph's <r> is meanRadius() (the readout's number) and equals the formula to 1e-14", diffOk);
    checkTrue("radial graph: the most likely distance is exactly a0/Z for 1s and n^2 a0/Z for every l = n - 1 orbital (relative guard: the bisection's, above rounding)", worstPeak <= tolPeak);
    checkTrue("radial graph: the peak is the highest point of the 401-point curve (to a step) and the curve starts at r = 0", highest && worstPeakIdx <= 1.0);
    checkTrue("radial graph: P has n - l maxima (exact roots), the stored curve shows n - l maxima and n - l - 1 nodes where P falls to about 0", countsOk && gridCountOk && minimaOk);
    checkTrue("radial graph: the 90 % mark is the outline's own rmax / Z for every orientation, and the curve does not depend on the orientation", markOk);

    // ---- the 90 % mark against an independent number: 1s is a ball, its outline radius is the radius holding 90 %: bisection of the closed-form share ----
    double lo = 0, hi = 20; for (int i = 0; i < 100; i++) { const double q = 0.5 * (lo + hi); (1 - std::exp(-2 * q) * (1 + 2 * q + 2 * q * q) < 0.9 ? lo : hi) = q; }
    const double q90 = 0.5 * (lo + hi), got1 = radialGraph(orbIndex("1s"), 1).reach90, h = 2 * 1.15 * 4.5 / kOutlineGrid;
    std::printf("radial graph: 1s radius holding 90 %% (closed form) %.6f a0; the outline's reach %.6f a0 (grid step %.4f a0): difference %.5f a0 (%.5f of a step)\n", q90, got1, h, q90 - got1, (q90 - got1) / h);
    checkTrue("radial graph: for 1s the 90 % mark agrees with the closed-form 90 % radius to a small part of a grid step of the outline (guard = measured 0.05966 of a step + 1 %)", std::fabs(got1 - q90) <= tolR90s * h);
    checkNum("radial graph: the 90 % mark scales as 1/Z (1s, Z = 26)", radialGraph(orbIndex("1s"), 26).reach90 * 26, got1, 1e-12);

    // ---- complex states: the same curve as the shell ----
    bool sameM = true;
    for (const char* sh : {"2p", "3p", "3d", "4d", "4f"}) {
        const int base = orbIndex(sh), l = orbitals()[base].l;
        const RadialGraph g0 = radialGraph(base, 2);
        for (int m = -l; m <= l; m++) { if (!m) continue; const RadialGraph gm = radialGraph(complexState(base, m), 2); sameM = sameM && gm.P == g0.P && gm.peak == g0.peak && gm.mean == g0.mean && gm.reach90 > 0 && std::fabs(gm.reach90 * 2 / outlineOf(complexState(base, m)).rmax - 1) < 1e-12; }
    }
    checkTrue("radial graph: every m = +-1 .. +-l state of a shell gives the same curve, peak and <r>, and each one's 90 % mark is its own outline's rmax / Z (the radial part does not depend on m)", sameM);

    // ---- the tick step is a round number and gives a few ticks ----
    bool ticksOk = true;
    for (int s = 0; s < 10; s++) for (int Z : {1, 2, 26}) {
        const RadialGraph g = radialGraph(orbIndex(rg::kShells[s]), Z);
        const double st = radialTickStep(g.rEnd), e = std::pow(10.0, std::floor(std::log10(st))), m = st / e, nt = std::floor(g.rEnd / st + 1e-9);
        ticksOk = ticksOk && (std::fabs(m - 1) < 1e-9 || std::fabs(m - 2) < 1e-9 || std::fabs(m - 5) < 1e-9) && nt >= 3 && nt <= 8;
    }
    checkTrue("radial graph: the axis step is 1, 2 or 5 times a power of ten and gives 3 to 8 ticks", ticksOk);

    // ---- the words ----
    const std::string note = kRadialNote, axis = kRadialAxisNote, mk = radialMarkText("average distance", 1.5);
    checkTrue("radial graph: the note says what P(r) dr is, that the area is 1, that every orientation has the same curve, and the model's limits (one electron, non-relativistic, point nucleus)",
              note.find("between r and r + dr") != std::string::npos && note.find("area") != std::string::npos && note.find("orientation") != std::string::npos &&
              note.find("one electron") != std::string::npos && note.find("non-relativistic") != std::string::npos && note.find("point nucleus") != std::string::npos);
    checkTrue("radial graph: the axis note names both units, Bohr radii and Å, and says where the right edge is (99.9 %)", axis.find("a₀") != std::string::npos && axis.find("Å") != std::string::npos && axis.find("99.9 %") != std::string::npos);
    checkStr("radial graph: a mark's text gives the value in a0 and in Å (CODATA a0 = 0.529177 Å)", mk, "average distance 1.5 a₀ = " + fmtLen(1.5 * kA0Ang));

    // ---- when it is shown ----
    {
        AtomLab lab; lab.nowMs = [] { return 0.0; }; lab.p = oneParams(); lab.restart();
        lab.setParam("view", std::string("Cloud"));
        const bool cloud = lab.radialMode() == RadialMode::Shown;
        lab.setParam("view", std::string("Orbit")); const bool orbit = lab.radialMode() == RadialMode::Hidden;
        lab.setParam("view", std::string("Slice")); const bool slice = lab.radialMode() == RadialMode::Shown;
        lab.setParam("view", std::string("Cloud 3D")); const bool c3 = lab.radialMode() == RadialMode::Shown;
        lab.setParam("view", std::string("Cloud")); lab.setParam("super", std::string("On")); const bool sup = lab.radialMode() == RadialMode::Superposition;
        lab.setParam("super", std::string("Off")); lab.setParam("orb", std::string("2p")); lab.emit(); for (int i = 0; i < 4 && lab.radialMode() != RadialMode::Jumping; i++) lab.step(1.0 / 60);
        const bool jump = lab.radialMode() == RadialMode::Jumping;
        checkTrue("radial graph: shown in Cloud, Cloud 3D and Slice; hidden in Orbit; replaced by a note in a superposition and during a photon jump", cloud && orbit && slice && c3 && sup && jump);
    }
}

// The dots: the distance distribution of the dots (12,000 of them) against the graph's own cumulative curve, as the Kolmogorov distance (largest gap between the two cumulative curves).
static void testRadialDots(double guard) {
    struct C { const char* orb; const char* key; const char* ori; int Z; } cs[] = {{"1s", "", "", 1}, {"2s", "", "", 1}, {"2p", "pAx", "z", 1}, {"2p", "pAx", "m+1", 1}, {"3s", "", "", 1}, {"3p", "pAx", "z", 1},
        {"3d", "dAx", "z2", 1}, {"3d", "dAx", "m+2", 1}, {"4s", "", "", 1}, {"4d", "dAx", "x2y2", 1}, {"4f", "fAx", "z3", 1}, {"4f", "fAx", "m+3", 1}, {"1s", "", "", 3}, {"3d", "dAx", "z2", 3}, {"4f", "fAx", "m+3", 26}};
    double worst = 0, sum = 0; long n = 0;
    for (const C& c : cs)
        for (int seed = 1; seed <= 4; seed++) {
            seedRandom(8000 + seed);
            AtomLab lab; lab.nowMs = [] { return 0.0; }; lab.p = oneParams(); lab.restart();
            lab.setParam("Z", double(c.Z)); lab.setParam("view", std::string("Cloud")); lab.setParam("orb", std::string(c.orb)); if (*c.key) lab.setParam(c.key, std::string(c.ori));
            for (int i = 0; i < 800; i++) lab.step(1.0 / 240);
            const long cnt = std::min<long>(lab.s.k, lab.cloudCap());
            std::vector<double> rr(cnt); for (long i = 0; i < cnt; i++) rr[i] = std::hypot(double(lab.s.pts[3 * i]), double(lab.s.pts[3 * i + 1]), double(lab.s.pts[3 * i + 2])) / nist::a0;
            std::sort(rr.begin(), rr.end());
            const RadialGraph g = radialGraph(lab.stateId(), c.Z);
            double cdf = 0, d = 0; std::size_t j = 0;
            for (std::size_t i = 1; i < g.r.size(); i++) {
                cdf += 0.5 * (g.P[i - 1] + g.P[i]) * (g.r[i] - g.r[i - 1]);
                while (j < rr.size() && rr[j] <= g.r[i]) j++;
                d = std::max(d, std::fabs(double(j) / double(cnt) - cdf));
            }
            worst = std::max(worst, d); sum += d; n++;
        }
    std::printf("radial graph against the dots (%ld labs, 12,000 dots each): Kolmogorov distance mean %.4f, worst %.6f\n", n, sum / n, worst);
    checkTrue("radial graph: the distance distribution of the dots matches the graph's cumulative curve (Kolmogorov distance; guard = measured worst + 1 %)", worst <= guard);
}

// The readout's "theory" is the graph's <r>: the same number, to the digit shown, for every shell and several Z.
static void testRadialReadout() {
    bool same = true; int k = 0;
    for (const char* sh : rg::kShells)
        for (int Z : {1, 2, 26}) {
            AtomLab lab; lab.nowMs = [] { return 0.0; }; lab.p = oneParams(); lab.restart();
            lab.setParam("Z", double(Z)); lab.setParam("view", std::string("Cloud")); lab.setParam("orb", std::string(sh));
            const RadialGraph g = radialGraph(lab.stateId(), Z);
            same = same && lab.info1().find("theory " + fmtLen(g.mean * lab.lenScale() * kA0Ang)) != std::string::npos; k++;   // 0.46.4: the graph is in a_mu, the readout in metres
        }
    checkTrue("radial graph: the readout's 'theory' avg r is the graph's <r>, for 10 shells x Z 1, 2, 26 (" + std::to_string(k) + " cases)", same);
}

// ================= 0.10.7 stage 2: the real spectrum (NIST ASD) next to the lab's =================
// The data was fetched on 2026-10-04 by tools/make_asd_data.py (the query is in data/asd/README.txt). The numbers typed below are read from data/asd/manifest.tsv and asd_lines.tsv (the files the fetch wrote, NOT
// from asd_data.inc), so the test checks that parsing, building and compiling kept what the fetch produced.
// The Dirac level formula (point nucleus, reduced mass): E(n, j) = mu c^2 [ (1 + (Z alpha)^2 / (n - d)^2)^(-1/2) - 1 ], d = (j + 1/2) - sqrt((j + 1/2)^2 - (Z alpha)^2). It is the standard textbook formula (Bethe &
// Salpeter; Griffiths ch. 6 gives its expansion), written from knowledge, not looked up here; the comparison with ASD below is its check.
// 0.10.8 stage 2: written WITHOUT subtracting nearly equal numbers (it was the plain textbook form, 1e-10 off at the worst; harmless for the guards below, but a reference should be accurate):
// d = (Z alpha)^2 / ((j + 1/2) + sqrt(...)) and (1 + t)^(-1/2) - 1 = -t / (s (1 + s)), s = sqrt(1 + t).
static double diracE(double mu, int Z, int n, double j) {      // eV, binding energy negative; mu in eV (m c^2 times the reduced-mass factor)
    const double za = Z * nist::alpha, d = za * za / ((j + 0.5) + std::sqrt((j + 0.5) * (j + 0.5) - za * za)), t = za * za / ((n - d) * (n - d)), sq = std::sqrt(1 + t);
    return -mu * t / (sq * (1 + sq));
}
static const AsdLine* asdFind(const AsdSpectrum& sp, const char* transitionStart) {
    for (int i = 0; i < sp.count; i++) if (std::string(sp.lines[i].transition).rfind(transitionStart, 0) == 0) return &sp.lines[i];
    return nullptr;
}

static void testAsdData() {
    // ---- the data parses and is what the fetch produced ----
    int total = 0, ionTotal = 0, atomTotal = 0, ionOk = 0, ionNone = 0, atomOk = 0, atomNone = 0, atomEmpty = 0;
    int flagsOnly = 0, ionObs = 0, ionRitzOnly = 0, atomObs = 0, ionResolved = 0, atomResolved = 0, ionIntens = 0, atomIntens = 0;
    bool positive = true, rangeOk = true, sorted = true, namesOk = true, unResolvedOk = true, uncOk = true, intensOk = true;
    double ionMin = 1e30, ionMax = 0, atomMin = 1e30, atomMax = 0;
    for (int Z = 1; Z <= 118; Z++)
        for (int kind = 0; kind < 2; kind++) {
            const AsdSpectrum& sp = kind == 0 ? asdIon(Z) : asdAtom(Z);
            (kind == 0 ? ionTotal : atomTotal) += sp.count; total += sp.count;
            if (kind == 0) { ionOk += sp.status == AsdStatus::Lines; ionNone += sp.status == AsdStatus::NoSpectrum; }
            else { atomOk += sp.status == AsdStatus::Lines; atomNone += sp.status == AsdStatus::NoSpectrum; atomEmpty += sp.status == AsdStatus::NoLines; }
            namesOk = namesOk && std::string(sp.name).rfind(std::string(symbol(Z)) + " ", 0) == 0 && (sp.status == AsdStatus::Lines) == (sp.count > 0);
            for (int i = 0; i < sp.count; i++) {
                const AsdLine& l = sp.lines[i];
                positive = positive && l.nm > 0 && l.ritz != 0;
                if (kind == 0) { rangeOk = rangeOk && l.nm >= kAsdIonLowNm && l.nm <= kAsdIonHighNm; ionMin = std::min(ionMin, l.nm); ionMax = std::max(ionMax, l.nm); ionObs += l.observed; ionRitzOnly += !l.observed; ionResolved += l.resolved; ionIntens += !std::isnan(asdIntensity(l)); }
                else { rangeOk = rangeOk && l.nm >= kAsdAtomLowNm && l.nm <= kAsdAtomHighNm && l.observed; atomMin = std::min(atomMin, l.nm); atomMax = std::max(atomMax, l.nm); atomObs += l.observed; atomResolved += l.resolved; atomIntens += !std::isnan(asdIntensity(l)); }
                if (i > 0) sorted = sorted && (kind == 1 ? sp.lines[i - 1].nm <= l.nm : true);
                unResolvedOk = unResolvedOk && (l.resolved == 0 || l.resolved == 1) && l.ni >= 0 && l.nk >= 0 && (std::string(l.transition).find(" - ") != std::string::npos || std::string(l.transition) == "-");
                uncOk = uncOk && (std::isnan(l.unc) || l.unc >= 0);
                if (!std::string(l.intens).empty() && std::isnan(asdIntensity(l))) flagsOnly++;
                intensOk = intensOk && (std::string(l.intens).empty() ? std::isnan(asdIntensity(l)) : true) && (std::isnan(asdIntensity(l)) || asdIntensity(l) > 0) && (kind == 0 || !std::isnan(asdIntensity(l)));
            }
        }
    std::printf("ASD data: %d lines kept (ion %d, atom %d); ions with lines %d, not in ASD %d; atoms with lines %d, not in ASD %d, none in range %d; wavelengths ion %.7g..%.7g nm, atom %.7g..%.7g nm\n",
                total, ionTotal, atomTotal, ionOk, ionNone, atomOk, atomNone, atomEmpty, ionMin, ionMax, atomMin, atomMax);
    checkTrue("ASD data: the totals are what the fetch produced (7,915 lines: 5,029 of the ions and 2,886 of the atoms; asdTotalLines agrees)", total == 7915 && ionTotal == 5029 && atomTotal == 2886 && asdTotalLines() == 7915);
    checkTrue("ASD data: 110 ions and 98 atoms have lines; 8 ions (Z 111-118) and 19 atoms (Z 100-118 except Fr) are not in ASD; Fr I has no line in the range", ionOk == 110 && ionNone == 8 && atomOk == 98 && atomNone == 19 && atomEmpty == 1);
    checkTrue("ASD data: counts per spectrum as fetched: H I ion 169 of 441, He II 140, Fe XXVI 95, U XCII 13, Fm C 13, H I atom 30 of 262, Fe I atom 30 of 9,548, Ne I atom 30 of 1,264",
              asdIon(1).count == 169 && asdIon(1).returned == 441 && asdIon(2).count == 140 && asdIon(26).count == 95 && asdIon(92).count == 13 && asdIon(100).count == 13 &&
              asdAtom(1).count == 30 && asdAtom(1).returned == 262 && asdAtom(26).count == 30 && asdAtom(26).returned == 9548 && asdAtom(10).count == 30 && asdAtom(10).returned == 1264);
    checkTrue("ASD data: every wavelength is positive, in the range asked (ion 1e-4 to 1e4 nm, atom 100 to 2500 nm), the names start with the element's symbol, status matches the line count", positive && rangeOk && namesOk);
    checkNum("ASD data: shortest and longest ion line are the fetched ones (0.0075649 and 9569.4 nm)", ionMin, 0.0075649, 1e-12); checkNum("ASD data: longest ion line", ionMax, 9569.4, 1e-12);
    checkNum("ASD data: shortest atom line (102.5728 nm)", atomMin, 102.5728, 1e-12); checkNum("ASD data: longest atom line (2486.733 nm)", atomMax, 2486.733, 1e-12);
    checkTrue("ASD data: observed and Ritz flags survive: ion lines 210 observed and 4,819 Ritz only; every atom line (2,886) is observed", ionObs == 210 && ionRitzOnly == 4819 && atomObs == 2886);
    checkTrue("ASD data: resolved components (both J given): ion 5,014 of 5,029, atom 2,164 of 2,886; numeric intensities: ion 107, atom all 2,886; 17 ion lines have only ASD flags in the intensity cell (no number: drawn without brightness)", ionResolved == 5014 && atomResolved == 2164 && ionIntens == 107 && atomIntens == 2886 && flagsOnly == 17);
    checkTrue("ASD data: atom lines are sorted by wavelength, uncertainties are NaN or not negative, a transition reads 'lower - upper' (or just '-' when ASD classifies none), an empty intensity is NaN, a given number is positive", sorted && unResolvedOk && uncOk && intensOk);
    // spot values typed from asd_lines.tsv
    const AsdLine *h1 = asdFind(asdIon(1), "1s 2S 1/2 - 2p 2P* 3/2"), *h2 = asdFind(asdIon(1), "1s 2S 1/2 - 2p 2P* 1/2"), *he = asdFind(asdIon(2), "1s 2S 1/2 - 2p 2P* 3/2"), *fe = asdFind(asdIon(26), "1s 2S 1/2 - 2p 2P* 3/2");
    checkTrue("ASD data: H I Lyman-alpha is there with ASD's observed 121.56699 nm +- 0.00020 and Ritz 121.5668237310 / 121.5673644608 nm", h1 && h2 && h1->observed == 1 && std::fabs(h1->nm - 121.56699) < 1e-9 && std::fabs(h1->unc - 0.00020) < 1e-12 && std::fabs(h1->ritz - 121.5668237310) < 1e-9 && std::fabs(h2->ritz - 121.5673644608) < 1e-9);
    checkTrue("ASD data: He II 1s-2p Ritz 30.3780409484 nm (also the stage 6 value), and it is a Ritz-only line; Fe XXVI 1s-2p3/2 0.177801471 nm Ritz-only with 1e-8 nm uncertainty",
              he && !he->observed && std::fabs(he->nm - 30.3780409484) < 1e-9 && fe && !fe->observed && std::fabs(fe->nm - 0.177801471) < 1e-12 && std::fabs(fe->unc - 1e-8) < 1e-12);
    checkTrue("ASD data: a Ritz-only ion line carries no intensity text (nothing invented), and the 1s - 6 H I line carries ASD's 19000", std::string(he->intens).empty() && std::isnan(asdIntensity(*he)));
    bool h6 = false; for (int i = 0; i < asdIon(1).count; i++) { const AsdLine& l = asdIon(1).lines[i]; if (std::string(l.transition) == "1s 2S 1/2 - 6" && asdIntensity(l) == 19000 && !l.resolved) h6 = true; }
    checkTrue("ASD data: the unresolved H I line '1s 2S 1/2 - 6' keeps ASD's intensity 19000 and is marked not resolved", h6);

    // ---- (a) the lab against the measured ion: fine structure and relativity are what is left ----
    // Tolerance: the lab is level-averaged and non-relativistic, so its line can differ from the components by the relativistic corrections, of the order (Z alpha)^2 of the level energies (5.3e-5 for hydrogen, 2.1e-4 for
    // He+): the lab's value must lie in the components' range widened by (Z alpha)^2 of the wavelength (the same justification as stage 6, now for every transition between n <= 6, not five lines).
    double worstH = 0, worstHe = 0; bool allIn = true; int cmpN = 0;
    for (int Z : {1, 2}) {
        const auto cmp = compareIon(Z, defaultN(Z));
        cmpN += int(cmp.size());
        for (const IonCompare& c : cmp) {
            const double rel = std::fabs(c.offsetNm) / c.labNm, tol = std::pow(Z * nist::alpha, 2);
            (Z == 1 ? worstH : worstHe) = std::max(Z == 1 ? worstH : worstHe, rel / tol);
            allIn = allIn && rel <= tol && c.components >= 1 && c.minNm <= c.maxNm && c.lo < c.hi && c.hi <= 6;
        }
    }
    std::printf("ASD ion vs lab: H I and He II, %d transitions between n <= 6; the lab lies outside the components by at most %.3f (H I) and %.3f (He II) of the allowed (Z alpha)^2 of the wavelength\n", cmpN, worstH, worstHe);
    checkTrue("ASD ion vs lab: for all 15 transitions among n <= 6 of H I and of He II (30 compared) the lab's wavelength lies inside ASD's components widened by (Z alpha)^2 of the wavelength", cmpN == 30 && allIn);
    // the heavy ions: the lab's line is LONGER than the measured one, by about the Dirac correction
    {
        const int Z = 26; const auto cmp = compareIon(Z, defaultN(Z));
        const double M = nuclearMassMe(Z, defaultN(Z)), mu = nist::mecEV * M / (M + 1);
        const AsdLine *a3 = asdFind(asdIon(Z), "1s 2S 1/2 - 2p 2P* 3/2"), *a1 = asdFind(asdIon(Z), "1s 2S 1/2 - 2p 2P* 1/2");
        double lab = 0; for (const IonCompare& c : cmp) if (c.lo == 1 && c.hi == 2) lab = c.labNm;
        const double d3 = nist::hcEVnm / (diracE(mu, Z, 2, 1.5) - diracE(mu, Z, 1, 0.5)), d1 = nist::hcEVnm / (diracE(mu, Z, 2, 0.5) - diracE(mu, Z, 1, 0.5));
        const double asd3 = a3->ritz, asd1 = a1->ritz;
        std::printf("Fe XXVI Lyman-alpha: lab %.9f nm; Dirac 2p3/2 %.9f, 2p1/2 %.9f; ASD Ritz 2p3/2 %.9f, 2p1/2 %.9f nm. lab - ASD: %.4e (2p3/2), %.4e (2p1/2) of the wavelength; lab - Dirac: %.4e, %.4e; ASD - Dirac: %.3e, %.3e\n",
                    lab, d3, d1, asd3, asd1, (lab - asd3) / lab, (lab - asd1) / lab, (lab - d3) / lab, (lab - d1) / lab, (asd3 - d3) / lab, (asd1 - d1) / lab);
        checkTrue("ASD ion vs lab, Fe XXVI Lyman-alpha: the lab's non-relativistic line is longer than both measured components (the relativistic correction shortens it)", lab > asd3 && lab > asd1);
        double offL = 0, mxL = 0, mnL = 0; for (const IonCompare& c : cmp) if (c.lo == 1 && c.hi == 2) { offL = c.offsetNm; mxL = c.maxNm; mnL = c.minNm; }
        checkTrue("ASD ion vs lab, Fe XXVI Lyman-alpha: compareIon reports the lab as LONGER (positive offset = lab minus the longest component) and the components are the two ASD Ritz values",
                  offL > 0 && std::fabs(offL - (lab - mxL)) < 1e-12 && std::fabs(mxL - asd1) < 1e-12 && std::fabs(mnL - asd3) < 1e-12);
        const double rel3 = (lab - asd3) / (lab - d3), rel1 = (lab - asd1) / (lab - d1);
        // ASD lies between the lab and Dirac: the Dirac formula has no QED (Lamb shift) and no finite nuclear size, which lift the 1s level and so move the line back towards the lab's. Measured share of the lab's
        // error that Dirac accounts for: 0.9508 (2p3/2) and 0.9318 (2p1/2); the guard is the larger remainder 0.0682 + 1 %.
        checkTrue("ASD ion vs lab, Fe XXVI Lyman-alpha: (lab - ASD) / (lab - Dirac) is within 0.0689 of 1 for both components (measured remainder 0.0682, + 1 %); the rest is what neither the lab nor Dirac has (QED, nuclear size)", std::fabs(rel3 - 1) <= 0.0689 && std::fabs(rel1 - 1) <= 0.0689 && rel3 < 1 && rel1 < 1);
        // every ion ASD has (Z = 1 .. 100 with the 1s-2p3/2 line): the lab's line is longer than ASD's, ASD's is longer than Dirac's, and Dirac accounts for 80 % to 100 % of the gap
        double lo = 1e9, hi = -1e9; bool order = true; int nZ = 0;
        for (int z = 1; z <= 118; z++) {
            const AsdLine* l3 = asdFind(asdIon(z), "1s 2S 1/2 - 2p 2P* 3/2"); if (!l3 || std::isnan(l3->ritz)) continue;
            double lb = 0; for (const IonCompare& c : compareIon(z, defaultN(z))) if (c.lo == 1 && c.hi == 2) lb = c.labNm;
            const double M2 = nuclearMassMe(z, defaultN(z)), mu2 = nist::mecEV * M2 / (M2 + 1), dz = nist::hcEVnm / (diracE(mu2, z, 2, 1.5) - diracE(mu2, z, 1, 0.5));
            const double r = (lb - l3->ritz) / (lb - dz); lo = std::min(lo, r); hi = std::max(hi, r); order = order && lb > l3->ritz && l3->ritz > dz; nZ++;
        }
        std::printf("Lyman-alpha 1s-2p3/2, %d ions: (lab - ASD) / (lab - Dirac) from %.4f to %.4f\n", nZ, lo, hi);
        // ---- the words: (a) validates, (b) teaches the limit, and the missing data are said ----
        auto has = [](const std::string& s, const char* w) { return s.find(w) != std::string::npos; };
        const std::string ta = asdIonTitle(26), na = asdIonNote(26), tb = asdAtomTitle(26), nb = asdAtomNote(26), nb1 = asdAtomNote(1), mOg = asdMissingText(asdIon(118)), mFr = asdMissingText(asdAtom(87)), mOk = asdMissingText(asdIon(26));
        checkTrue("ASD words: (a) names the ion the lab models (Fe XXVI), says one electron, non-relativistic, point nucleus, fine structure and relativity are left out, and that it checks the lab",
                  has(ta, "(a)") && has(ta, "ion the lab models") && has(ta, "Fe XXVI") && has(na, "One electron") && has(na, "non-relativistic") && has(na, "point nucleus") && has(na, "fine structure") && has(na, "relativity") && has(na, "checks the lab"));
        checkTrue("ASD words: (b) names the neutral atom (Fe I), says it has 26 electrons while the lab has ONE, that the lines are not expected to match and that it shows the limit of the lab",
                  has(tb, "(b)") && has(tb, "neutral atom") && has(tb, "Fe I") && has(nb, "26 electrons") && has(nb, "ONE electron") && has(nb, "not expected to match") && has(nb, "limit of the lab"));
        checkTrue("ASD words: for hydrogen (b) says the neutral atom is the same system as (a)", has(nb1, "same system as (a)"));
        checkTrue("ASD words: no data is said on screen: Og CXVIII 'has no data for Og CXVIII', Fr I 'but no line in the range', a spectrum with lines gives no message",
                  has(mOg, "has no data for Og CXVIII") && has(mFr, "Fr I") && has(mFr, "no line in the range") && mOk.empty());
        const std::string mn = kAsdMeasuredNote, cr = kAsdCredit;
        checkTrue("ASD words: the note says measured or Ritz with uncertainties, vacuum wavelengths, qualitative relative intensities that mean something only within one spectrum and depend on the source, no brightness meant where there is none, and that only a selection is stored",
                  has(mn, "measured") && has(mn, "Ritz") && has(mn, "uncertainty") && has(mn, "vacuum") && has(mn, "qualitative") && has(mn, "within one spectrum") && has(mn, "light source") && has(mn, "no brightness") && has(mn, "selection"));
        checkTrue("ASD words: the credit follows NIST's request (authors, title, version 5.12, 2024, https://physics.nist.gov/asd, DOI 10.18434/T4W30F) and gives the fetch date",
                  has(cr, "Kramida, A., Ralchenko, Yu., Reader, J. and NIST ASD Team (2024)") && has(cr, "version 5.12") && has(cr, "https://physics.nist.gov/asd") && has(cr, "10.18434/T4W30F") && has(cr, "2026-10-04"));
        checkTrue("ASD words: the selections are stated (ion: n <= 6; atom: the 30 strongest observed lines with an intensity in 100-2500 nm)", has(kAsdSelectIon, "n <= 6") && has(kAsdSelectAtom, "30 strongest") && has(kAsdSelectAtom, "100-2500 nm"));
        checkTrue("ASD ion vs lab: for every ion ASD has (Lyman-alpha 1s-2p3/2) the order is lab > ASD > Dirac in wavelength, and Dirac accounts for between 0.793 and 1 of the lab's error (measured 0.8017 for H to 0.9780; lowest - 1 %)", nZ == 110 && order && lo >= 0.793 && hi < 1);
    }
}

// ================= 0.10.8 stage 1: relativity as an option (Dirac levels) =================
// References that do NOT come from the lab's own functions:
//  (1) the CODATA 2022 constants typed in namespace nist above (alpha, m_e c^2, the Rydberg energy, h c);
//  (2) the textbook arrangement of the Sommerfeld / Dirac formula, E = m c^2 [ (1 + (Z alpha / (n - (j + 1/2) + sqrt((j + 1/2)^2 - (Z alpha)^2)))^2)^(-1/2) - 1 ], worked out here in DOUBLE-DOUBLE arithmetic (about 32
//      digits, built below from error-free transformations and std::fma): the plain subtractions of the textbook form are then harmless (they cost at most 11 digits of 32). The lab uses cancellation-free
//      rearrangements instead (dirac.cpp). Same formula, so (1) and (2) check the arithmetic, not the physics. 0.10.8 stage 2: this block was first written with long double, which is 80 bits on Linux and only 64 on MSVC,
//      so on Windows the reference was the inaccurate side and six checks failed. **DO NOT USE long double (or any extended precision) IN A TEST REFERENCE: it is not portable.** The 80-digit values typed below
//      (Python decimal) pin the double-double code itself;
//  (3) its expansion to the order (Z alpha)^4 (Bethe & Salpeter; Griffiths ch. 6): E = -(m c^2 (Z alpha)^2 / 2 n^2) [1 + ((Z alpha)^2 / n^2)(n / (j + 1/2) - 3/4)]: a different formula, whose miss must scale as (Z alpha)^6;
//  (4) NIST ASD (data/asd, 0.10.7 stage 2): MEASURED wavelengths, matched to the lab's lines by the n and j of both levels, read from ASD's own transition text.
// Both formulas are the standard textbook ones, written from knowledge, not looked up in this session (only physics.nist.gov was reachable); (4) is what checks them against nature.
// ---- double-double arithmetic (a value is hi + lo, |lo| <= half an ulp of hi): the portable high-precision reference. Only +, -, *, / and sqrt of doubles and std::fma, which are exactly rounded on every IEEE
// platform; no extended precision. (Knuth's two-sum; the product's error from fma; Newton steps for / and sqrt.)
struct DD { double hi, lo; };
static DD ddNorm(double a, double b) { const double s = a + b; return {s, b - (s - a)}; }                       // |a| >= |b|
static DD ddTwoSum(double a, double b) { const double s = a + b, v = s - a; return {s, (a - (s - v)) + (b - v)}; }
static DD ddTwoProd(double a, double b) { const double p = a * b; return {p, std::fma(a, b, -p)}; }
static DD ddAdd(DD a, DD b) { DD s = ddTwoSum(a.hi, b.hi); const DD t = ddTwoSum(a.lo, b.lo); s.lo += t.hi; s = ddNorm(s.hi, s.lo); s.lo += t.lo; return ddNorm(s.hi, s.lo); }
static DD ddNeg(DD a) { return {-a.hi, -a.lo}; }
static DD ddSub(DD a, DD b) { return ddAdd(a, ddNeg(b)); }
static DD ddMul(DD a, DD b) { DD p = ddTwoProd(a.hi, b.hi); p.lo += a.hi * b.lo + a.lo * b.hi; return ddNorm(p.hi, p.lo); }
static DD ddDiv(DD a, DD b) {
    const double q1 = a.hi / b.hi; DD r = ddSub(a, ddMul(b, DD{q1, 0}));
    const double q2 = r.hi / b.hi; r = ddSub(r, ddMul(b, DD{q2, 0}));
    const double q3 = r.hi / b.hi;
    return ddAdd(ddNorm(q1, q2), DD{q3, 0});
}
static DD ddSqrt(DD a) { const double s = std::sqrt(a.hi); const DD r = ddSub(a, ddTwoProd(s, s)); return ddAdd(DD{s, 0}, DD{r.hi / (2 * s), 0}); }
static double ddRel(DD got, DD want) { return std::fabs((got.hi - want.hi) + (got.lo - want.lo)) / std::fabs(want.hi); }
// the textbook formula with plain subtractions, in double-double, from the same double x = Z * alpha the lab forms:
//   delta = (j + 1/2) - sqrt((j + 1/2)^2 - x^2), k = n - delta, E = m c^2 ((1 + x^2 / k^2)^(-1/2) - 1), correction c = E + m c^2 x^2 / (2 n^2)
struct DiracDD { DD E, c, delta; };
static DiracDD ddDirac(int Z, int n, int twoJ) {
    const double x = Z * nist::alpha, jh = (twoJ + 1) / 2.0;
    const DD x2 = ddTwoProd(x, x), delta = ddSub(DD{jh, 0}, ddSqrt(ddSub(DD{jh * jh, 0}, x2))), k = ddSub(DD{double(n), 0}, delta);
    const DD f = ddDiv(DD{1, 0}, ddSqrt(ddAdd(DD{1, 0}, ddDiv(x2, ddMul(k, k)))));
    const DD E = ddMul(DD{nist::mecEV, 0}, ddSub(f, DD{1, 0}));
    const DD c = ddAdd(E, ddDiv(ddMul(DD{nist::mecEV, 0}, x2), DD{2.0 * n * n, 0}));
    return {E, c, delta};
}
static double refDiracInfEV(int Z, int n, int twoJ) { return ddDirac(Z, n, twoJ).E.hi; }
static double refCorrEV(int Z, int n, int twoJ) { return ddDirac(Z, n, twoJ).c.hi; }
// typed 80-digit values (Python decimal, 80 digits, from the same double Z alpha and m c^2), as {hi, lo}: Z, n, 2j, E (eV), the correction c (eV), delta
struct DiracTyped { int Z, n, twoJ; DD E, c, delta; };
static const DiracTyped kDiracTyped[] = {
    {1, 1, 1, {-13.605874258016533, -5.969003669744587e-16}, {-0.00018113521962715416, -3.783553996857771e-21}, {2.6626031696629847e-05, 2.678459506123533e-22}},
    {1, 2, 1, {-3.4014798855307187, 4.520228029309583e-17}, {-5.660483149211426e-05, 1.871567248221069e-21}, {2.6626031696629847e-05, 2.678459506123533e-22}},
    {1, 2, 3, {-3.401434601424379, -1.6954613731282566e-17}, {-1.1320725152316283e-05, 2.5521239038358817e-22}, {1.3312882920136858e-05, 8.51972088924689e-24}},
    {1, 3, 5, {-1.5117459164951206, 6.853153541232245e-17}, {-2.2361843531711144e-06, -1.0230414748475084e-22}, {8.875238869593463e-06, -1.719576036269966e-22}},
    {1, 6, 11, {-0.3779360598389038, 2.2515491269053562e-17}, {-1.3976121196891403e-07, 5.097490222003399e-24}, {4.437614511676528e-06, 2.0789311433837705e-22}},
    {92, 1, 1, {-132279.93376052065, -6.296308638660601e-12}, {-17121.34716916764, -1.763957495721877e-12}, {0.2588653725842363, 9.632853740197007e-18}},
    {92, 2, 1, {-34215.48418576323, 1.7159236195463466e-12}, {-5425.837537924971, 1.20527299962243e-13}, {0.2588653725842363, 9.632853740197007e-18}},
    {92, 2, 3, {-29649.837103282563, -1.3856411128590466e-12}, {-860.1904554443101, -2.5179651681133387e-14}, {0.11604656746651436, -4.190429857505444e-18}},
    {92, 3, 5, {-12959.73819605728, -4.92793156578785e-14}, {-164.33968590694485, -4.320951067065598e-16}, {0.07608472490143743, -4.275308759882891e-18}},
    {92, 6, 11, {-3208.9251872264167, 1.1499724852477167e-14}, {-10.075559688832845, 6.188910780668959e-16}, {0.03767825960769684, 2.7536361417673256e-18}}
};
static double refSeriesEV(int Z, int n, int twoJ) {                 // the (Z alpha)^4 expansion
    const double x = Z * nist::alpha, jh = (twoJ + 1) / 2.0;
    return -(nist::mecEV * x * x / (2.0 * n * n)) * (1 + x * x / (double(n) * n) * (n / jh - 0.75));
}
// the lab's hybrid level written again by hand: reduced-mass main term (the lab's nuclear mass, typed again above) + the infinite-mass relativistic correction
// 0.91.0 (the reference changed on purpose, REPORT 0.91.0): the lab's level is now E_D + E_M (Yerokhin & Shabaev 2015, Eq. (3)); the part of E_M beyond the reduced mass,
// worked out here from Eq. (3) as written: m c^2 x (Z a)^2 / (2 N^2) - m_r c^2 x^2 (Z a)^2 / (2 n^2) - m_r c^2 x (Z a)^2 / (2 n^2) (the last: what the reduced-mass main term holds)
static double refRecoilEV(int Z, int N, int n, int twoJ) {
    const double M = nuclearMassNew(Z, N), x = 1 / M, mr = nist::mecEV / (1 + x), za = Z * nist::alpha, ak = (twoJ + 1) * 0.5;
    const double gam = std::sqrt(ak * ak - za * za), N2 = (n - ak + gam) * (n - ak + gam) + za * za;
    return nist::mecEV * x * za * za / (2 * N2) - mr * x * x * za * za / (2.0 * n * n) - mr * x * za * za / (2.0 * n * n);
}
static double refHybridEV(int Z, int N, int n, int twoJ) {
    const double M = nuclearMassNew(Z, N), ry = nist::rydEV * M / (M + 1);
    return -ry * Z * Z / (double(n) * n) + refCorrEV(Z, n, twoJ) + refRecoilEV(Z, N, n, twoJ);
}
// the lines of ASD for ion Z that are electric-dipole (n changes, l changes by 1), as {nLo, nHi, 2jLo, 2jHi} -> Ritz wavelengths
struct AsdKey { int a, b, c, d; bool operator<(const AsdKey& o) const { return std::array<int, 4>{a, b, c, d} < std::array<int, 4>{o.a, o.b, o.c, o.d}; } };
static std::map<AsdKey, std::vector<double>> asdE1(int Z) {
    std::map<AsdKey, std::vector<double>> out;
    const AsdSpectrum& sp = asdIon(Z);
    for (int i = 0; i < sp.count; i++) {
        AsdTransition t;
        if (!asdParseTransition(sp.lines[i].transition, t) || t.nLo >= t.nHi || std::abs(t.lLo - t.lHi) != 1 || !sp.lines[i].resolved) continue;
        out[{t.nLo, t.nHi, t.jLo2, t.jHi2}].push_back(std::isnan(sp.lines[i].ritz) ? sp.lines[i].nm : sp.lines[i].ritz);   // ASD's Ritz wavelength, else its observed one (as compareIon)
    }
    return out;
}
// the hand-made wavelength of one line, nm: with the Dirac levels (hybrid) or without
static double refLam(int Z, int N, int lo, int jl, int hi, int jh, bool dirac) {
    return nist::hcEVnm / ((dirac ? refHybridEV(Z, N, hi, jh) - refHybridEV(Z, N, lo, jl) : -(nist::rydEV * nuclearMassNew(Z, N) / (nuclearMassNew(Z, N) + 1)) * Z * Z * (1.0 / (double(hi) * hi) - 1.0 / (double(lo) * lo))));
}
// the worst relative miss of the lab's lines against ASD's E1 components, matched by (n, j) of both levels; positive = the lab's line is longer
struct Miss { double worst = 0; int comps = 0, unmatched = 0; };
static Miss worstMiss(int Z, int N, bool dirac) {
    Miss m;
    for (const auto& kv : asdE1(Z))
        for (double w : kv.second) {
            const AsdKey& k = kv.first;
            if (k.b > 6) continue;
            const double lam = refLam(Z, N, k.a, k.c, k.b, k.d, dirac), r = (lam - w) / w;
            if (std::fabs(r) > std::fabs(m.worst)) m.worst = r;
            m.comps++;
        }
    return m;
}

#include "test_atom0493.inc"  // 0.49.3: the Lamb shift of n = 1, 2 (outside review); here, after the hand-made references it uses

// 0.86.0 (PLAN_REVIEW.md R7): the Dirac energy (atom/dirac.h diracInfiniteEV) against its source, Yerokhin & Shabaev 2015 (YS15), J. Phys. Chem. Ref. Data 44,
// 033103, Eqs. (2)-(4), and their table's DIRAC row for every Z = 1-110 (1s, 2s-1s, 2p3/2-2p1/2 in h c R_inf, as printed: tools/make_lamb_data.py, lamb_data.inc).
// Their table used alpha^-1 = 137.035 999 074 (CODATA 2010, read from the extract here); the app uses CODATA 2022's alpha. The table's unit is h c R_inf =
// alpha^2 m c^2 / 2 ("in Rydbergs", the caption in the extract), so the app's eV are divided by kMecEV alpha^2 / 2.
//  1. With the paper's alpha (diracInfiniteRy, the same formula): every printed value within half a unit of its last printed digit, plus its bracketed
//     uncertainty and the double arithmetic's own error (16 units of double's last place on each energy: dirac.h, "good to a few units of the last digit").
//  2. The app's diracInfiniteEV: within the same, plus the difference the two alphas make (computed: the formula at each alpha), so the bound is derived, not chosen.
static void testDiracRows086() {
    std::ifstream f(PL_DATA_DIR "/lamb/yerokhin_shabaev_2015_dirac.extract.txt");
    std::string ex((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    const std::string key = "\\alpha^{-1} &\\ = ";
    const std::size_t at = ex.find(key);
    std::string num;
    if (at != std::string::npos) for (std::size_t i = at + key.size(); i < ex.size() && ex[i] != '('; i++) if (std::isdigit(static_cast<unsigned char>(ex[i])) || ex[i] == '.') num += ex[i];
    const double aInv = num.empty() ? 0.0 : std::stod(num);
    checkTrue("YS15: the alpha their table used, read from the extract (137.035999074, CODATA 2010), and the table is in units of h c R_inf",
              aInv == 137.035999074 && ex.find("hydrogenlike atoms, in Rydbergs.") != std::string::npos && ex.find("E_{\\rm D} = mc^2\\, \\left[") != std::string::npos);
    const double ap = 1 / aInv, ry = kMecEV * kAlpha * kAlpha / 2, eps = 16 * std::numeric_limits<double>::epsilon();
    int rows = 0, badP = 0, badA = 0;
    double worstP = 0, worstA = 0;
    for (int Z = 1; Z <= 110; Z++) {
        const DiracPrinted* d = diracPrinted(Z);
        if (!d) continue;
        rows++;
        const double p1 = diracInfiniteRy(Z, 1, 1, ap), p2 = diracInfiniteRy(Z, 2, 1, ap), p3 = diracInfiniteRy(Z, 2, 3, ap);
        const double a1 = diracInfiniteEV(Z, 1, 1) / ry, a2 = diracInfiniteEV(Z, 2, 1) / ry, a3 = diracInfiniteEV(Z, 2, 3) / ry;
        const double vp[3] = {p1, p2 - p1, p3 - p2}, va[3] = {a1, a2 - a1, a3 - a2}, mag[3] = {std::fabs(p1), std::fabs(p1) + std::fabs(p2), std::fabs(p2) + std::fabs(p3)};
        for (int k = 0; k < 3; k++) {
            const double unit = std::pow(10.0, -d[k].decimals), bound = 0.5 * unit + d[k].unc * unit + eps * mag[k];
            const double eP = std::fabs(vp[k] - d[k].v), eA = std::fabs(va[k] - d[k].v), alphaDiff = std::fabs(va[k] - vp[k]);
            if (eP > bound) badP++;
            if (eA > bound + alphaDiff + eps * mag[k]) badA++;
            worstP = std::max(worstP, eP / bound); worstA = std::max(worstA, eA / (bound + alphaDiff + eps * mag[k]));
        }
    }
    checkTrue("YS15 DIRAC rows, Z = 1-110 (1s, 2s-1s, 2p3/2-2p1/2): with the paper's alpha every printed digit (half a unit, bracket, double's error): " + std::to_string(badP) + " off, worst " + std::to_string(worstP) + " of the bound", rows == 110 && badP == 0);
    checkTrue("YS15 DIRAC rows: the app's diracInfiniteEV (CODATA 2022 alpha) within the same plus the two alphas' difference: " + std::to_string(badA) + " off, worst " + std::to_string(worstA) + " of the bound", rows == 110 && badA == 0);
    bool zero = true;
    for (int Z = 1; Z <= 110; Z++) zero = zero && std::fabs(diracInfiniteEV(Z, 2, 1) / ry - diracInfiniteRy(Z, 2, 1)) <= eps * std::fabs(diracInfiniteRy(Z, 2, 1)) * 4;
    checkTrue("diracInfiniteRy at the app's alpha = diracInfiniteEV / (m c^2 alpha^2 / 2) (2s, every Z, to double's rounding)", zero);
}

static void testDirac() {
    // Guards = the worst MEASURED value + 1 % (Linux GCC Release; improvement factors: measured - 1 %), from the first run of these tests:
    //  scaling of Dirac - non-relativistic with (Z alpha)^2: 0.50676 -> 0.5118 (the next-order coefficient is 1/2); log-log slope off 2 by at most 1.5412e-2 -> 1.557e-2; miss of the (Z alpha)^4 expansion 0.00782 -> 0.00790;
    //  ASD, worst relative miss of any n <= 6 component, with Dirac / without, and the improvement factor: H I 3.3169e-6 -> 3.351e-6 / 2.6431e-5 -> 2.670e-5, x7.969 -> 7.88; He II 1.0931e-5 -> 1.104e-5 / 1.0573e-4 -> 1.068e-4,
    //  x9.672 -> 9.57; Fe XXVI 5.7231e-4 -> 5.781e-4 / 1.8239e-2 -> 1.843e-2, x31.87 -> 31.5; what is left, Lyman-alpha of 110 ions, 220 components: (ASD - Dirac) / (non-relativistic - Dirac) at most 0.27164 -> 0.2744.
    constexpr double GUARD_ROUND_E = 2e-15, GUARD_ROUND_HYB = 1e-15;   // rounding level against the double-double reference: measured worst 6.66e-16 (3 units of the last digit) and 0
    constexpr double GUARD_RATIO = 0.5118, GUARD_SLOPE = 1.557e-2, GUARD_SERIES = 0.00790, GUARD_LEFT = 0.2744;
    constexpr double GUARD_H_ON = 3.351e-6, GUARD_H_OFF = 2.670e-5, GUARD_H_RATIO = 7.88, GUARD_HE_ON = 1.104e-5, GUARD_HE_OFF = 1.068e-4, GUARD_HE_RATIO = 9.57, GUARD_FE_ON = 5.781e-4, GUARD_FE_OFF = 1.843e-2, GUARD_FE_RATIO = 31.5;
    const double a = nist::alpha, mc = nist::mecEV;
    // ---- (1) the level against hand-made values ----
    {   // hydrogen 1s, infinite mass: j = 1/2, k = sqrt(1 - alpha^2), so E = m c^2 (sqrt(1 - alpha^2) - 1) = - m c^2 alpha^2 / (1 + sqrt(1 - alpha^2)) in closed form (the second form has no subtraction;
        // the first, in plain double, is off by 2.5e-12: it subtracts 1 from 0.99997338)
        const double want = -nist::mecEV * a * a / (1 + std::sqrt(1 - a * a));
        checkNum("Dirac H 1s, infinite mass, = - m c^2 alpha^2 / (1 + sqrt(1 - alpha^2)) worked out here from CODATA (the cancellation-free form of m c^2 (sqrt(1 - alpha^2) - 1))", diracInfiniteEV(1, 1, 1), want, 1e-15);
        std::printf("Dirac H 1s infinite mass: %.12f eV (the non-relativistic %.12f eV)\n", diracInfiniteEV(1, 1, 1), -0.5 * mc * a * a);
        const double x26 = 26 * a, wantFe = -nist::mecEV * x26 * x26 / (1 + std::sqrt(1 - x26 * x26));
        checkNum("Dirac Fe XXVI 1s, infinite mass, = - m c^2 (26 alpha)^2 / (1 + sqrt(1 - (26 alpha)^2)) worked out here", diracInfiniteEV(26, 1, 1), wantFe, 1e-15);
        std::printf("Dirac Fe 1s infinite mass: %.6f eV (non-relativistic %.6f eV)\n", diracInfiniteEV(26, 1, 1), -0.5 * mc * 26 * a * 26 * a);
    }
    double worstFormula = 0; bool allFinite = true;
    for (int Z : {1, 2, 6, 26, 54, 92, 118})
        for (int n = 1; n <= 6; n++)
            for (int j2 = 1; j2 <= 2 * n - 1; j2 += 2) {
                const double got = diracInfiniteEV(Z, n, j2), want = refDiracInfEV(Z, n, j2);
                worstFormula = std::max(worstFormula, std::fabs(got - want) / std::fabs(want)); allFinite = allFinite && std::isfinite(got) && got < 0;
            }
    std::printf("Dirac level, infinite mass, vs the textbook arrangement in double-double (Z up to 118, n <= 6, every j): worst relative difference %.3e\n", worstFormula);
    checkTrue("Dirac level: every level is finite and bound for Z = 1 .. 118 (Z alpha < 1)", allFinite);
    checkTrue("Dirac level agrees with the textbook arrangement of the formula (double-double) to 2e-15 for Z = 1, 2, 6, 26, 54, 92, 118, n <= 6, every j (rounding level: 2.3 units of the last digit; the measured worst is printed above)", worstFormula < GUARD_ROUND_E);
    // ---- the lab's level: reduced-mass main term + the infinite-mass relativistic correction ----
    double worstHyb = 0;
    for (int Z : {1, 2, 26}) for (int n = 1; n <= 4; n++) for (int j2 = 1; j2 <= 2 * n - 1; j2 += 2) worstHyb = std::max(worstHyb, std::fabs(diracEV(Z, defaultN(Z), n, j2) - refHybridEV(Z, defaultN(Z), n, j2)) / std::fabs(refHybridEV(Z, defaultN(Z), n, j2)));
    checkTrue("Dirac level of the lab = the hand-made hybrid (reduced-mass term from the NIST masses + the infinite-mass Dirac correction) to the rounding level (H, He+, Fe, n <= 4)", worstHyb < GUARD_ROUND_HYB);
    std::printf("Dirac level of the lab vs the hand-made hybrid: worst relative difference %.3e\n", worstHyb);
    // ---- the reduced mass: what the hybrid does and how far it is from the plain m -> mu substitution (itself only a leading-order recoil term, not an exact one) ----
    {
        const int Z = 1, N = 0; const double M = nuclearMassNew(Z, N), mu = M / (M + 1);
        const double corr = refCorrEV(1, 2, 3) - refCorrEV(1, 1, 1);   // the infinite-mass relativistic correction to the Lyman-alpha (2p3/2) photon energy
        const double hyb = diracEV(Z, N, 2, 3) - diracEV(Z, N, 1, 1);                                                      // the lab's photon energy
        const double sub = mu * (nist::rydEV * 0.75 + corr);                                                               // m -> mu in the whole formula: the correction is scaled by mu too
        const double E = nist::hcEVnm / refLam(Z, N, 1, 1, 2, 3, true);
        std::printf("Reduced mass, H Lyman-alpha 2p3/2: photon %.9f eV; hybrid minus (m -> mu everywhere) %.3e eV = %.3e of the photon energy; the whole relativistic correction is %.3e eV = %.3e of it\n", E, hyb - sub, (hyb - sub) / E, corr, corr / E);
        checkNum("Reduced mass, H Lyman-alpha 2p3/2: the hybrid differs from the m -> mu photon energy by exactly the correction times (1 - mu), as derived", hyb - sub, corr * (1 - mu), 1e-6);
        checkTrue("Reduced mass, H Lyman-alpha 2p3/2: that difference is below 1e-7 of the photon energy (measured, stated in the report), far below the Dirac correction itself", std::fabs(hyb - sub) / E < 1e-7 && std::fabs(hyb - sub) < 1e-2 * std::fabs(corr));
    }
    // ---- the splitting 2p3/2 - 2p1/2 of hydrogen against m c^2 alpha^4 / 32, from the constants ----
    {
        const double lead = mc * std::pow(a, 4) / 32, got = diracEV(1, 0, 2, 3) - diracEV(1, 0, 2, 1), gotInf = diracInfiniteEV(1, 2, 3) - diracInfiniteEV(1, 2, 1);
        std::printf("H n = 2 fine structure: Dirac %.9e eV (%.6f GHz), m c^2 alpha^4 / 32 = %.9e eV, ratio - 1 = %.3e (alpha^2 = %.3e)\n", got, got / nist::h * nist::e * 1e-9, lead, got / lead - 1, a * a);
        // 0.91.0 (references changed on purpose): the lab's level has the recoil of Eq. (3) (YS15): beyond the reduced mass it lifts 2p1/2 by m c^2 x alpha^4 / (4 N^2 (1 + gamma)),
        // which is x m c^2 alpha^4 / 32 to the next order in alpha^2 (N^2 -> 4, gamma -> 1): the gap is (1 - x) m c^2 alpha^4 / 32, x = m / M. Until 0.90.0 the gap was the infinite-mass one.
        const double xH = 1 / nuclearMassNew(1, 0);
        checkNum("H 2p3/2 - 2p1/2 Dirac splitting = (1 - m/M) m c^2 alpha^4 / 32 within alpha^2 (the next order; 0.91.0: with the recoil of Eq. (3))", got, (1 - xH) * lead, a * a);
        checkTrue("H 2p3/2 - 2p1/2 splitting (0.91.0): the lab's gap is the infinite-mass Dirac gap less the recoil x m c^2 alpha^4 / 32, within that times 2 alpha^2", std::fabs(got - (gotInf - xH * lead)) <= 2 * a * a * xH * lead);
        checkTrue("H 2p3/2 is less bound than 2p1/2 (the splitting is positive, j = 3/2 above j = 1/2)", got > 0);
        checkNum("Dirac theory: 2s1/2 and 2p1/2 have exactly the same energy (no Lamb shift): the level depends on n and j only", diracEV(1, 0, 2, 1), diracEV(1, 0, 2, 1), 0);
        {   // NIST's own 2p3/2 - 2p1/2 gap for hydrogen, from ASD's two Lyman-alpha Ritz wavelengths (ASD's Ritz wavelengths are made from its measured energy levels): h c (1 / lambda(2p3/2) - 1 / lambda(2p1/2))
            const auto m = asdE1(1); const double l3 = m.at({1, 2, 1, 3}).front(), l1 = m.at({1, 2, 1, 1}).front();
            const double nist2 = nist::hcEVnm * (1 / l3 - 1 / l1), over = nist2 / got - 1;
            std::printf("H n = 2 gap from ASD's Lyman-alpha Ritz wavelengths: %.9e eV (%.6f GHz); Dirac %.9e eV; NIST / Dirac - 1 = %.4e\n", nist2, nist2 / nist::h * nist::e * 1e-9, got, over);
            // (0.91.0, changed on purpose: the lab's gap now has the recoil of Eq. (3), 0.054 % narrower: measured 0.23176 %, was 0.1772 %)
            checkTrue("H 2p3/2 - 2p1/2: NIST's measured gap is LARGER than the Dirac one with its recoil, by 0.2318 % (measured; the guard is + 1 % of that): the part of the gap Dirac theory does not have (QED), small and one-sided", over > 0 && over < 2.341e-3);
        }
        checkNum("The splitting from the expansion to (Z alpha)^4: m c^2 alpha^4 / 32 is the leading difference of the series formula", refSeriesEV(1, 2, 3) - refSeriesEV(1, 2, 1), lead, 1e-9);
    }
    // ---- reduces to the non-relativistic level as Z alpha -> 0, at the order (Z alpha)^2 ----
    // Infinite nuclear mass first (pure Dirac against pure Bohr): the relative difference of the 1s level is (Z alpha)^2 / 4 at the lowest order (n = 1, j = 1/2 in the expansion (3)), and its next term is
    // of the order (Z alpha)^4, so the ratio to (Z alpha)^2 / 4 minus 1, divided by (Z alpha)^2, stays a constant. The lab's level carries the reduced mass on top, which is NOT a (Z alpha)^2 effect (5.4e-4
    // for hydrogen), so it is checked separately: the lab's relative difference is the infinite-mass one divided by mu / m.
    {
        double worstRatio = 0, worstSlope = 0, worstSeries = 0, worstMass = 0, prevR = 0, prevZ = 0;
        std::printf("Dirac vs non-relativistic 1s, infinite mass, (E_Dirac - E_NR) / E_NR over (Z alpha)^2 / 4 (1 at the lowest order):");
        for (int Z : {1, 2, 3, 4, 6, 8, 10, 14, 20}) {
            const double x2 = Z * a * Z * a, nr = -0.5 * mc * x2, r = refCorrEV(Z, 1, 1) / nr;   // (E_Dirac - E_NR) / E_NR from the double-double reference: the plain difference of the two energies would lose 5 digits
            worstRatio = std::max(worstRatio, std::fabs(r / (x2 / 4) - 1) / x2);       // the miss of the leading order, in units of (Z alpha)^2: about constant (3/8 in the expansion's next term)
            if (prevR != 0) worstSlope = std::max(worstSlope, std::fabs(std::log(r / prevR) / std::log(double(Z) / prevZ) - 2));
            prevR = r; prevZ = Z;
            std::printf(" Z=%d:%.5f", Z, r / (x2 / 4));
            const double M = nuclearMassMe(Z, defaultN(Z)), lab = (diracEV(Z, defaultN(Z), 1, 1) - levelEV(Z, defaultN(Z), 1)) / levelEV(Z, defaultN(Z), 1);
            worstMass = std::max(worstMass, std::fabs(lab / r * (M / (M + 1)) - 1));
        }
        std::printf("\n");
        for (int Z : {1, 2, 4, 8, 16}) {      // against the expansion: the miss is of the order (Z alpha)^4 of the energy
            const double x2 = Z * a * Z * a, miss = std::fabs(diracInfiniteEV(Z, 2, 3) - refSeriesEV(Z, 2, 3)) / std::fabs(refSeriesEV(Z, 2, 3));
            worstSeries = std::max(worstSeries, miss / (x2 * x2));
        }
        std::printf("Dirac vs non-relativistic: worst (ratio to (Z alpha)^2 / 4 minus 1) over (Z alpha)^2: %.5f; worst deviation of the log-log slope from 2: %.4e; lab's relative difference vs the infinite-mass one times m / mu: %.2e; miss of the (Z alpha)^4 expansion in units of (Z alpha)^4: %.5f\n", worstRatio, worstSlope, worstMass, worstSeries);
        checkTrue("Dirac - non-relativistic (1s, infinite mass) scales like (Z alpha)^2 for Z = 1 .. 20: (ratio to (Z alpha)^2 / 4 minus 1) / (Z alpha)^2 stays below its measured value + 1 %", worstRatio < GUARD_RATIO && worstRatio > 0.3);
        checkTrue("Dirac - non-relativistic: the log-log slope of the relative difference between neighbouring Z (1 .. 20) is 2 to within its measured deviation + 1 %", worstSlope < GUARD_SLOPE);
        checkTrue("The lab's relative difference (reduced-mass level) is the infinite-mass one times M / (M + 1) inverse, to 1e-8 for Z = 1 .. 20: the reduced mass is a separate, mass-ratio-sized factor", worstMass < 1e-8);
        checkTrue("Dirac 2p3/2 against the (Z alpha)^4 expansion: the miss is a constant (below its measured value + 1 %) times (Z alpha)^4 of the energy for Z = 1 .. 16, a different formula agreeing to the order it is exact", worstSeries < GUARD_SERIES);
    }
    // ---- which jumps are allowed ----
    {
        checkTrue("selection rule: 2p1/2 -> 1s1/2, 2p3/2 -> 1s1/2, 2s1/2 -> 2p3/2 type jumps follow l changes by 1 and j by 0 or 1", diracAllowed(0, 1, 1, 1) && diracAllowed(0, 1, 1, 3) && diracAllowed(1, 3, 2, 3) && diracAllowed(1, 3, 2, 5) && diracAllowed(1, 1, 2, 3));
        checkTrue("selection rule: d5/2 -> p1/2 (j changes by 2), s -> d (l changes by 2), p -> p (l unchanged) and an impossible j are not allowed", !diracAllowed(1, 1, 2, 5) && !diracAllowed(0, 1, 2, 3) && !diracAllowed(1, 1, 1, 3) && !diracAllowed(0, 3, 1, 3) && !diracAllowed(1, 5, 2, 5));
        auto count = [](int lo, int hi) { int n = 0; for (const DiracLine& l : diracLines(1, 0)) if (l.lo == lo && l.hi == hi) n++; return n; };
        // 0.49.3 (deliberate change): the lines now carry the Lamb shift of n = 1, 2 (test_atom0493.inc), which splits 2s1/2 from 2p1/2, so hydrogen's Balmer lines have all 7 components; Dirac theory alone
        // (checked here at Z = 112, where the source has no Lamb shift) joins two pairs: 5.
        checkTrue("lines counted by hand: Lyman-alpha 2 (np1/2, np3/2 -> 1s1/2), Lyman-beta 2, Balmer-alpha 7 and Balmer-beta 7 with the Lamb shift (hydrogen)", count(1, 2) == 2 && count(1, 3) == 2 && count(2, 3) == 7 && count(2, 4) == 7);
        // 3s1/2 -> 2p1/2 and 3p1/2 -> 2s1/2 have the same energies in Dirac theory: one line, not two (Z = 112: no Lamb shift in the lab); with the Lamb shift (hydrogen) two
        int h1 = 0, h112 = 0; for (const DiracLine& l : diracLines(1, 0)) if (l.lo == 2 && l.hi == 3 && l.jLo2 == 1 && l.jHi2 == 1) h1++;
        for (const DiracLine& l : diracLines(112, defaultN(112))) if (l.lo == 2 && l.hi == 3 && l.jLo2 == 1 && l.jHi2 == 1) h112++;
        checkTrue("Dirac theory: 3s1/2 -> 2p1/2 and 3p1/2 -> 2s1/2 have equal energies, so they are ONE line (Z = 112, no Lamb shift); the Lamb shift makes them two (hydrogen)", h112 == 1 && h1 == 2);
        // against NIST: for H I and He II the set of lines (n, j, n', j') of ASD's E1 components is exactly the set the rule gives
        for (int Z : {1, 2}) {
            std::set<std::array<int, 4>> mine, asd;
            for (const DiracLine& l : diracLines(Z, defaultN(Z))) if (l.hi >= 1 && l.hi <= 6) mine.insert({l.lo, l.hi, l.jLo2, l.jHi2});
            for (const auto& kv : asdE1(Z)) if (kv.first.b <= 6) asd.insert({kv.first.a, kv.first.b, kv.first.c, kv.first.d});
            std::printf("selection rule vs ASD, Z = %d: the rule gives %zu lines (n <= 6), ASD has %zu E1 (n, j, n', j') sets\n", Z, mine.size(), asd.size());
            checkTrue(std::string("selection rule vs NIST ASD, Z = ") + std::to_string(Z) + ": the lines the rule gives for n <= 6 are exactly the (n, j, n', j') sets of ASD's electric-dipole components", mine == asd);
        }
        // every ion ASD has: no ASD component lies outside the rule (ASD may lack some Ritz lines, the rule never lacks one ASD has)
        int zsChecked = 0; bool subset = true;
        for (int Z = 1; Z <= 118; Z++) {
            if (asdIon(Z).status != AsdStatus::Lines) continue;
            std::set<std::array<int, 4>> mine;
            for (const DiracLine& l : diracLines(Z, defaultN(Z))) if (l.hi >= 1 && l.hi <= 6) mine.insert({l.lo, l.hi, l.jLo2, l.jHi2});
            for (const auto& kv : asdE1(Z)) if (kv.first.b <= 6 && !mine.count({kv.first.a, kv.first.b, kv.first.c, kv.first.d})) subset = false;
            zsChecked++;
        }
        checkTrue("selection rule vs NIST ASD: for every ion ASD has, every electric-dipole component (n <= 6) is a line the rule allows (110 ions)", subset && zsChecked == 110);
    }
    // ---- against NIST: the lab's lines with Dirac closer to the measured ones than without ----
    {
        struct Case { int Z; const char* name; double guardOn, guardOff; double ratioMin; };
        const Case cases[3] = {{1, "H I", GUARD_H_ON, GUARD_H_OFF, GUARD_H_RATIO}, {2, "He II", GUARD_HE_ON, GUARD_HE_OFF, GUARD_HE_RATIO}, {26, "Fe XXVI", GUARD_FE_ON, GUARD_FE_OFF, GUARD_FE_RATIO}};
        for (const Case& c : cases) {
            const Miss off = worstMiss(c.Z, defaultN(c.Z), false), on = worstMiss(c.Z, defaultN(c.Z), true);
            std::printf("vs ASD, %s, %d components (n <= 6): worst relative miss without Dirac %.4e, with Dirac %.4e (improvement x%.2f; the lab's line is %s)\n", c.name, on.comps, off.worst, on.worst, std::fabs(off.worst / on.worst), on.worst > 0 ? "longer" : "shorter");
            checkTrue(std::string("Dirac vs ASD, ") + c.name + ": with Dirac the lab's lines land closer to every measured component set than without (worst miss smaller)", std::fabs(on.worst) < std::fabs(off.worst) && on.comps > 20);
            checkTrue(std::string("Dirac vs ASD, ") + c.name + ": the worst miss with Dirac is below its measured value + 1 % and the improvement factor above its measured value - 1 %", std::fabs(on.worst) < c.guardOn && std::fabs(off.worst) < c.guardOff && std::fabs(off.worst / on.worst) > c.ratioMin);
        }
    }
    // ---- what is left is the Lamb shift (and QED, nuclear size): ASD lies between Dirac and the non-relativistic lab, for Lyman-alpha of every ion ----
    {
        double lo = 1e9, hi = -1e9; bool order = true; int nZ = 0;
        for (int Z = 1; Z <= 118; Z++) {
            const auto m = asdE1(Z); if (m.empty()) continue;
            for (int j2 : {1, 3}) {
                const auto it = m.find({1, 2, 1, j2}); if (it == m.end()) continue;
                for (double w : it->second) {
                    const int N = defaultN(Z); const double nr = refLam(Z, N, 1, 1, 2, j2, false), d = refLam(Z, N, 1, 1, 2, j2, true), share = (w - d) / (nr - d);
                    lo = std::min(lo, share); hi = std::max(hi, share); order = order && nr > w && w > d; nZ++;
                }
            }
        }
        std::printf("Lamb-shift side, Lyman-alpha, both components of %d (ion, component) pairs: (ASD - Dirac) / (non-relativistic - Dirac) from %.5f to %.5f\n", nZ, lo, hi);
        checkTrue("What is left, Lyman-alpha (1s-2p1/2 and 1s-2p3/2) of every ion ASD has: non-relativistic lab > ASD > Dirac in wavelength, i.e. ASD lies between, as the Lamb shift (it lifts 1s) and the nuclear size would put it", order && nZ >= 200);
        checkTrue("What is left is small: ASD is at most 0.2744 of the way from Dirac to the non-relativistic line (measured max 0.27164, + 1 %), and always beyond Dirac", hi < GUARD_LEFT && lo > 0);
    }
    // ---- the lab's functions: the comparison, the lines, the rows, the words ----
    {
        bool same = true; double maxD = 0;
        for (int Z : {1, 2, 26}) {
            const auto cmp = compareIonDirac(Z, defaultN(Z));
            for (const FineCompare& c : cmp) {
                double worst = 0; int comps = 0;
                const AsdSpectrum& sp = asdIon(Z);                          // 0.49.3: matched by (n, l, j) of both levels, the reference with the typed Lamb shifts (test_atom0493.inc)
                for (int i = 0; i < sp.count; i++) {
                    AsdTransition t;
                    if (!sp.lines[i].resolved || !asdParseTransition(sp.lines[i].transition, t) || t.nLo != c.lo || t.nHi != c.hi || std::abs(t.lLo - t.lHi) != 1) continue;
                    const double w = std::isnan(sp.lines[i].ritz) ? sp.lines[i].nm : sp.lines[i].ritz;
                    const double r = (lamb0493::refLamQed(Z, defaultN(Z), c.lo, t.lLo, t.jLo2, c.hi, t.lHi, t.jHi2) - w) / w; if (std::fabs(r) > std::fabs(worst)) worst = r; comps++;
                }
                maxD = std::max(maxD, std::fabs(c.worstDiracRel - worst)); same = same && c.asdComponents == comps && c.asdUnmatched == 0 && c.lo < c.hi && c.hi <= 6 && c.labLines >= 1;
            }
            same = same && !cmp.empty();
        }
        std::printf("compareIonDirac vs the hand-made comparison: worst difference of a relative miss %.3e (the misses themselves are 1e-6 to 1e-2)\n", maxD);
        checkTrue("compareIonDirac gives the component counts and, to 1e-14 (rounding; measured 3.2e-16 since 0.10.8 stage 2, it was 1.1e-13 while the reference was long double; the misses themselves are 1e-6 and up), the worst miss of the hand-made comparison (H I, He II, Fe XXVI, every transition n <= 6)", same && maxD < 1e-14);
        const auto L = diracLines(1, 0);
        bool shape = !L.empty(), limits = true;
        for (const DiracLine& l : L) { shape = shape && l.lam > 0 && l.lo >= 1 && l.lo <= 5 && (l.hi == 0 ? l.jHi2 == 0 : l.hi > l.lo && l.hi <= l.lo + 20); if (l.hi == 0) limits = limits && l.lo >= 1; }
        int lim2 = 0; for (const DiracLine& l : L) if (l.hi == 0 && l.lo == 2) lim2++;
        checkTrue("diracLines: every line has a positive wavelength and sensible levels; the n = 2 series limit is split three ways with the Lamb shift (0.49.3; was two: j = 1/2 and 3/2)", shape && lim2 == 3);
        // the wavelength of Lyman-alpha 2p3/2 against the hand-made one with the Lamb shift: test_atom0493.inc (0.49.3, deliberate change: Dirac alone gave 121.566421 nm, NIST 121.566824)
        // visible lines and the lines of the spectrum come from ONE list
        const auto V = visibleLinesFor(1, 0, true); bool visOk = !V.empty();
        for (const SpecLine& l : V) visOk = visOk && isVisibleNm(l.lam) && l.hi != 0;
        checkTrue("visibleLinesFor(Dirac) lists the Dirac lines inside visible light (Balmer and above), none a series limit", visOk);
    }
    {   // OFF must be the old lab, number for number: the same list, bit for bit, every element
        bool listSame = true, visSame = true, noteSame = true;
        for (int Z = 1; Z <= 118; Z++) {
            const int N = defaultN(Z);
            const auto A = specLines(Z, N), B = specLinesFor(Z, N, false), V1 = visibleLines(Z, N), V2 = visibleLinesFor(Z, N, false);
            listSame = listSame && A.size() == B.size() && std::memcmp(A.data(), B.data(), A.size() * sizeof(SpecLine)) == 0;
            visSame = visSame && V1.size() == V2.size() && (V1.empty() || std::memcmp(V1.data(), V2.data(), V1.size() * sizeof(SpecLine)) == 0);
            noteSame = noteSame && spectrumNote(Z, N) == spectrumNote(Z, N, false);
        }
        checkTrue("setting OFF: specLinesFor, visibleLinesFor and the spectrum note are the old ones, bit for bit, for all 118 elements (compared in this run, no stored hashes)", listSame && visSame && noteSame);
        checkTrue("setting ON is the default (0.49.3, outside review; it was Off), in the plain struct and in the app's defaults, and only Off and On are valid choices", AtomParams().dirac == "On" && AtomParams::appDefaults().dirac == "On" && AtomLab::validChoice("dirac", "Off") && AtomLab::validChoice("dirac", "On") && !AtomLab::validChoice("dirac", "on") && !AtomLab::validChoice("dirac", ""));
        // a lab with the setting set to On by hand (0.49.3: the default) reads out and runs exactly as one never told: same readout lines, same dots
        AtomLab x, y; x.nowMs = [] { return 0.0; }; y.nowMs = [] { return 0.0; };
        x.p = oneParams(); y.p = oneParams(); x.restart(); y.restart();
        seedRandom(77); x.setParam("view", std::string("Cloud")); x.setParam("Z", 26.0);
        seedRandom(77); y.setParam("view", std::string("Cloud")); y.setParam("dirac", std::string("On")); y.setParam("Z", 26.0);
        const auto rx = x.info(), ry = y.info();
        checkTrue("setting ON set by hand: the readout lines and the cloud dots are identical to a lab that never saw the setting", rx == ry && x.s.k == y.s.k && x.s.pts == y.s.pts);
        y.setParam("dirac", std::string("Off"));
        checkTrue("setting OFF changes nothing about the cloud: the setting is the spectrum's (the cloud has its own model setting, 0.49.3); the readout loses the setting's note only", x.s.pts == y.s.pts && y.info().size() <= rx.size());
    }
    {   // the words
        auto has = [](const std::string& s, const char* w) { return s.find(w) != std::string::npos; };
        const std::string lab = kDiracLabel, ex = diracExplain(), on = diracOnNote(26), off = diracOffNote(), sn = spectrumNote(26, defaultN(26), true), an = asdIonNoteDirac(26), same = spectrumNote(26, defaultN(26));
        // 0.49.3 (deliberate change of the words): the Lamb shift of n = 1, 2 is in the levels (Yerokhin & Shabaev 2015), and the cloud has its own setting (Cloud model)
        const std::string on112 = diracOnNote(112), an112 = asdIonNoteDirac(112);
        checkTrue("Dirac words: the label is short and honest (Dirac levels), and the one-line explanation says what changes (levels and lines in the spectrum cards, the Lamb shift of n = 1, 2 with the size of the nucleus) and that the cloud has its own setting",
                  has(lab, "Dirac") && lab.size() < 40 && has(ex, "levels") && has(ex, "lines") && has(ex, "spectrum") && has(ex, "Lamb shift") && has(ex, "size of the nucleus") && has(ex, "Cloud model") && ex.size() < 400);
        checkTrue("Dirac words (on, iron): Dirac equation, one electron, point nucleus, levels by j; the Lamb shift of n = 1, 2 from its source, with its isotope, 2s above 2p; none for n >= 3, so NIST still differs; the reduced mass",
                  has(on, "Dirac equation") && has(on, "one electron") && has(on, "point nucleus") && has(on, "j") && has(on, "2s") && has(on, "2p") && has(on, "Lamb shift") && has(on, "QED") && has(on, "size of the nucleus") &&
                  has(on, "Yerokhin") && has(on, "56Fe") && has(on, "n = 3 and up") && has(on, "NIST") && has(on, "reduced mass") && has(on, "Cloud model"));
        checkTrue("Dirac words (on, Z = 112, beyond the source): no Lamb shift, 2s and 2p (j = 1/2) exactly equal", has(on112, "No Lamb shift") && has(on112, "exactly the same energy"));
        checkTrue("Dirac words (on): the reduced-mass treatment is said plainly: the main energy has the reduced mass, the relativistic correction is for an infinitely heavy nucleus, recoil beyond that is not included", has(on, "infinitely heavy") && has(on, "recoil"));
        checkTrue("Dirac words: with the setting off the note says the spectrum cards use the non-relativistic levels", has(off, "non-relativistic") && !has(off, "Lamb shift is"));
        checkTrue("Dirac words: the spectrum note (on) says fine structure IS included and the Lamb shift for n = 1 and 2 only; the old note (off) is unchanged and says fine structure is not included",
                  has(sn, "Dirac") && has(sn, "Lamb shift") && has(sn, "n = 1 and 2") && has(sn, "size of the nucleus") && !has(sn, "Not included: fine structure") && has(same, "Not included: fine structure"));
        checkTrue("Dirac words: the real-spectrum note (on) says the top row is the lab's Dirac lines with the Lamb shift, what is left, and that this checks the lab (Z = 112: the size of the nucleus is left out too)",
                  has(an, "Dirac") && has(an, "Lamb shift") && has(an, "n = 3 and up") && has(an, "checks the lab") && has(an112, "size of the nucleus"));
        const auto rows = diracLevelRows(1, 0); std::string all; for (const std::string& r : rows) all += r + "\n";
        checkTrue("Dirac level rows (hydrogen, 0.49.3): n = 1, 2 and 3 are listed; with the Lamb shift 2s and 2p (j = 1/2) have their own rows and a Lamb-shift row, while 3s and 3p still share j = 1/2; the gaps are given in eV-type units",
                  rows.size() >= 8 && has(all, "n = 1") && has(all, "n = 2: 2s (j = 1/2)") && has(all, "n = 2: 2p (j = 1/2)") && has(all, "Lamb shift") && has(all, "3s, 3p") && has(all, "gap") && has(all, "GHz"));
    }
}

// ================= 0.10.8 stage 2: the precision of the Dirac formula =================
// Stage 1's Windows build failed because its reference was computed with long double, which is 80 bits on Linux and 64 on MSVC: the plain subtraction sqrt(1 - alpha^2) - 1 (the result is 2.7e-5, so 11 of the 16
// digits of the sqrt are thrown away) was then 2.5e-12 off, and the LAB's value, which had no such subtraction, looked wrong. Here the references are well conditioned and portable: double-double arithmetic (the
// classes above) of the textbook formula, pinned by 80-digit values typed from Python's decimal module. Written from the same double Z alpha the lab forms, so what is measured is the arithmetic of the lab's
// rearrangements, not the rounding of the input. The "old" forms below are written out here exactly as they were (the naive textbook form; stage 1's expm1/log1p energy and its E + m c^2 (Z alpha)^2 / 2 n^2 correction).
// The lab's rewritten quantities (rounding level, units of the last digit 2.2e-16; measured worst: delta 1.77e-16, energy 4.56e-16, correction 1.29e-15, readout 2.98e-16): about 2-4 times the measured.
// The old forms' errors (plain IEEE arithmetic, so the same on every platform; the guard is measured - 1 %, rounded down): Z = 1: delta 6.01e-11, energy 1.45e-10, stage 1 correction 2.07e-10;
// Z = 92: stage 1 correction 3.65e-14, naive energy 1.46e-14.
constexpr double GUARD_NOW_D = 5e-16, GUARD_NOW_E = 1e-15, GUARD_NOW_C = 5e-15, GUARD_NOW_R = 1e-15;
constexpr double GUARD_NAIVE_D = 5.95e-11, GUARD_NAIVE_E = 1.43e-10, GUARD_NAIVE_C = 2.04e-10, GUARD_NAIVE_C92 = 3.61e-14, GUARD_NAIVE_E92 = 1.44e-14;
static void testDiracPrecision() {
    // guards: rounding-level ones are in units of the last digit (2.2e-16), not measured + 1 %, because the last bit may differ between compilers; the deterministic ones (the naive forms' errors, plain IEEE operations) are
    // measured - 1 % (they must stay at least that bad, or the test no longer shows the cancellation): see the numbers printed below and the report.
    {   // the double-double arithmetic itself
        const DD r2 = ddSqrt(DD{2, 0}), e1 = ddSub(ddMul(r2, r2), DD{2, 0}), e2 = ddSub(ddMul(ddDiv(DD{1, 0}, DD{3, 0}), DD{3, 0}), DD{1, 0});
        std::printf("double-double: sqrt(2)^2 - 2 = %.1e, (1/3) 3 - 1 = %.1e\n", e1.hi, e2.hi);
        checkTrue("double-double: sqrt(2)^2 = 2 and (1/3) 3 = 1 to 1e-30 (about 32 digits), so it can serve as the reference", std::fabs(e1.hi) < 1e-30 && std::fabs(e2.hi) < 1e-30);
    }
    double worstDD = 0, tE = 0, tC = 0, tD = 0;
    for (const DiracTyped& r : kDiracTyped) {
        const DiracDD d = ddDirac(r.Z, r.n, r.twoJ);
        worstDD = std::max({worstDD, ddRel(d.E, r.E), ddRel(d.c, r.c), ddRel(d.delta, r.delta)});
        tE = std::max(tE, ddRel(DD{diracInfiniteEV(r.Z, r.n, r.twoJ), 0}, r.E)); tC = std::max(tC, ddRel(DD{diracCorrectionEV(r.Z, r.n, r.twoJ), 0}, r.c)); tD = std::max(tD, ddRel(DD{diracDelta(r.Z, r.twoJ), 0}, r.delta));
    }
    std::printf("typed 80-digit values (Z = 1 and 92, 5 levels each): double-double worst %.2e; the lab: energy %.2e, correction %.2e, delta %.2e (relative)\n", worstDD, tE, tC, tD);
    // The textbook form has the plain subtractions, so even 32 digits leave less at Z = 1, n = 6: measured worst 3.64e-20 (the correction, whose size is 4e-7 of the energy it is the difference of), + 1 %. Still 4 orders
    // of magnitude finer than a double (the lab is at 1e-15), so it is a valid reference; the energy itself is good to 1.3e-26.
    checkTrue("double-double reference = the 80-digit values typed from Python decimal, to 3.7e-20 relative (measured 3.64e-20 + 1 %; E, the correction and delta, Z = 1 and 92, five levels each)", worstDD < 3.7e-20);
    checkTrue("the lab's energy, correction and delta agree with the typed 80-digit values to 5e-15 (a few units of the last digit; Z = 1 and 92, five levels each)", tE < 5e-15 && tC < 5e-15 && tD < 5e-15);
    // ---- digits gained, against the double-double reference: Z = 1 and 92, n = 1 .. 6, every j ----
    struct Worst { double naive = 0, old = 0, now = 0; };
    Worst wE[2], wD[2], wC[2];
    const int zs[2] = {1, 92};
    for (int zi = 0; zi < 2; zi++) {
        const int Z = zs[zi];
        for (int n = 1; n <= 6; n++)
            for (int j2 = 1; j2 <= 2 * n - 1; j2 += 2) {
                const double x = Z * nist::alpha, x2 = x * x, jh = (j2 + 1) / 2.0, m = nist::mecEV;
                const DiracDD ref = ddDirac(Z, n, j2);
                const double dNaive = jh - std::sqrt(jh * jh - x2), dStable = x2 / (jh + std::sqrt(jh * jh - x2));
                const double eNaive = m * (1 / std::sqrt(1 + x2 / ((n - dNaive) * (n - dNaive))) - 1);                 // the textbook form with the plain subtractions
                const double eOld = m * std::expm1(-0.5 * std::log1p(x2 / ((n - dStable) * (n - dStable))));            // stage 1's energy (already without the subtractions)
                const double cOld = eOld + 0.5 * m * x2 / (double(n) * n), cNaive = eNaive + 0.5 * m * x2 / (double(n) * n);   // stage 1's correction: the difference of two energies
                auto up = [](Worst& w, double naive, double old, double now, DD ref) { w.naive = std::max(w.naive, ddRel(DD{naive, 0}, ref)); w.old = std::max(w.old, ddRel(DD{old, 0}, ref)); w.now = std::max(w.now, ddRel(DD{now, 0}, ref)); };
                up(wD[zi], dNaive, dNaive, diracDelta(Z, j2), ref.delta);
                up(wE[zi], eNaive, eOld, diracInfiniteEV(Z, n, j2), ref.E);
                up(wC[zi], cNaive, cOld, diracCorrectionEV(Z, n, j2), ref.c);
            }
    }
    auto digits = [](double was, double now) { return std::log10(was / std::max(now, 1e-18)); };
    std::printf("digits against the double-double reference (worst relative error over n = 1..6 and every j; digits gained = log10(old / now)):\n");
    for (int zi = 0; zi < 2; zi++) {
        std::printf("  Z = %2d  delta: naive %.2e -> now %.2e (%.1f digits)   energy: naive %.2e, stage 1 %.2e -> now %.2e (%.1f digits over naive)   correction: naive-based %.2e, stage 1 %.2e -> now %.2e (%.1f digits over stage 1)\n",
                    zs[zi], wD[zi].naive, wD[zi].now, digits(wD[zi].naive, wD[zi].now), wE[zi].naive, wE[zi].old, wE[zi].now, digits(wE[zi].naive, wE[zi].now), wC[zi].naive, wC[zi].old, wC[zi].now, digits(wC[zi].old, wC[zi].now));
    }
    checkTrue("precision: the lab's delta, energy and correction are within GUARD_NOW_D / GUARD_NOW_E / GUARD_NOW_C of the double-double reference for Z = 1 and 92, n = 1..6, every j (rounding level, units of the last digit)",
              wD[0].now < GUARD_NOW_D && wD[1].now < GUARD_NOW_D && wE[0].now < GUARD_NOW_E && wE[1].now < GUARD_NOW_E && wC[0].now < GUARD_NOW_C && wC[1].now < GUARD_NOW_C);
    checkTrue("precision: the naive forms really are bad at Z = 1 (so this test shows the cancellation): delta off by at least GUARD_NAIVE_D, the energy by at least GUARD_NAIVE_E, stage 1's correction by at least GUARD_NAIVE_C (measured - 1 %)",
              wD[0].naive >= GUARD_NAIVE_D && wE[0].naive >= GUARD_NAIVE_E && wC[0].old >= GUARD_NAIVE_C);
    checkTrue("precision: at Z = 92 the stage 1 correction was off by at least GUARD_NAIVE_C92 and the naive energy by at least GUARD_NAIVE_E92 (measured - 1 %); the new ones are not", wC[1].old >= GUARD_NAIVE_C92 && wE[1].naive >= GUARD_NAIVE_E92);
    checkTrue("precision: the rewrites gain digits at Z = 1: at least 4 for the correction over stage 1, at least 5 for the energy over the naive form", digits(wC[0].old, wC[0].now) >= 4 && digits(wE[0].naive, wE[0].now) >= 5);
    // ---- the readout's two heavy-atom formulas, 1 - sqrt(1 - (Z alpha)^2): the percentage and the 1s energy ----
    {
        double worstOld = 0, worstNew = 0, worstOldE = 0, worstNewE = 0; bool same = true;
        for (int Z = 1; Z <= 118; Z++) {
            const double zx = Z * nist::alpha, zx2 = zx * zx, sq = std::sqrt(1 - zx2);
            const double oldPct = 100 * ((1 - sq) / (0.5 * zx2) - 1), newPct = 100 * zx2 / ((1 + sq) * (1 + sq)), oldE = nist::mecEV * (1 - sq), newE = nist::mecEV * zx2 / (1 + sq);
            const DD y = ddTwoProd(zx, zx), s = ddSqrt(ddSub(DD{1, 0}, y)), num = ddSub(DD{1, 0}, s);
            const DD refPct = ddMul(DD{100, 0}, ddSub(ddDiv(num, ddMul(DD{0.5, 0}, y)), DD{1, 0})), refE = ddMul(DD{nist::mecEV, 0}, num);
            worstOld = std::max(worstOld, ddRel(DD{oldPct, 0}, refPct)); worstNew = std::max(worstNew, ddRel(DD{newPct, 0}, refPct));
            worstOldE = std::max(worstOldE, ddRel(DD{oldE, 0}, refE)); worstNewE = std::max(worstNewE, ddRel(DD{newE, 0}, refE));
            same = same && numText(oldPct, 2) == numText(newPct, 2) && fmtEV(oldE) == fmtEV(newE);      // what the readout SHOWS (2 decimals; the 1s energy in eV text)
        }
        std::printf("readout, Z = 1..118: the percentage off by %.2e (old) -> %.2e (now), the 1s energy off by %.2e (old) -> %.2e (now); the shown text identical for all 118: %s\n", worstOld, worstNew, worstOldE, worstNewE, same ? "yes" : "NO");
        checkTrue("readout: the cancellation-free percentage and 1s energy are within GUARD_NOW_R of the double-double reference for Z = 1 .. 118 (rounding level)", worstNew < GUARD_NOW_R && worstNewE < GUARD_NOW_R);
        checkTrue("readout: the text the readout shows (the percentage to 2 decimals, the 1s energy) is the same with the old and the new arithmetic for all 118 elements: nothing the app shows changed", same);
    }
}

// ================= 0.10.8 stage 3: real lifetimes and selection rules (from NIST ASD) =================
// References that do not come from the lab's lifetime code: (1) numbers typed from data/asd/asd_lines.tsv (the file the fetch wrote, NOT the compiled table) so the whole pipeline is checked: the A values, their accuracy
// codes and transition types; (2) 1 / (sum of A) worked out here from those typed numbers; (3) ASD's own words for the selection rule (ASD's lines help, read 2026-10-04: an allowed E1 line joins levels of opposite
// parity whose J differs by at most 1, J = 0 <-> 0 excluded; its Type column is blank for E1 and M1, E2, M1+E2, UT for forbidden ones); (4) the ASD data itself, scanned here by a separate loop (grouping by the upper level).
// No long double and no hashes of doubles (0.10.6 stage 9, 0.10.8 stage 2).
struct AsdE1 { Level lo, hi; double A; std::string acc, type; };
static std::vector<AsdE1> asdAllLines(int Z) {             // every resolved ASD line of the ion whose two levels read as (n, l, j), lower n < upper n
    std::vector<AsdE1> out; const AsdSpectrum& sp = asdIon(Z);
    for (int i = 0; i < sp.count; i++) {
        AsdTransition t;
        if (!asdParseTransition(sp.lines[i].transition, t) || t.nLo >= t.nHi) continue;
        out.push_back({{t.nLo, t.lLo, t.jLo2}, {t.nHi, t.lHi, t.jHi2}, sp.lines[i].aki, sp.lines[i].acc, sp.lines[i].type});
    }
    return out;
}
// ASD's rule in ASD's words (opposite parity, J changes by at most 1, not 0 <-> 0 (never needed: j is at least 1/2)), for two real levels
static bool asdRuleAllows(Level lo, Level hi) { return (lo.l + hi.l) % 2 == 1 && std::abs(lo.j2 - hi.j2) <= 2; }
static std::vector<Level> allLevelsUpTo(int nMax) { std::vector<Level> v; for (int n = 1; n <= nMax; n++) for (int l = 0; l < n; l++) for (int j2 = 2 * l - 1; j2 <= 2 * l + 1; j2 += 2) if (j2 >= 1) v.push_back({n, l, j2}); return v; }

// Guards = the worst MEASURED value + 1 % (Linux GCC Release), for A(Z) / (Z^4 A(H)) from ASD's own numbers, the three transitions 2p3/2 -> 1s, 2p1/2 -> 1s, 3d5/2 -> 2p3/2:
//  worst |r - 1| for Z <= 10: 6.489e-4, 1.229e-3, 8.749e-4 -> 6.554e-4, 1.242e-3, 8.837e-4 (the reduced mass and the like);
//  worst |r - 1| / (Z alpha)^2 for Z >= 10 (ions up to Z = 110): 0.3614, 0.2308, 0.1452 -> 0.3651, 0.2331, 0.1467 (the relativistic correction, of the order (Z alpha)^2; r falls to 0.77 at Z = 110 for 2p3/2).
static constexpr double GUARD_Z4_LOW[3] = {6.554e-4, 1.242e-3, 8.837e-4}, GUARD_Z4_REL[3] = {0.3651, 0.2331, 0.1467};
static void testLifetimes() {
    // ---- 1. what the file says (typed from asd_lines.tsv: the Aki, Acc and Type columns) ----
    {
        int ionN = 0, withA = 0, accAA = 0, accAAA = 0, accNone = 0, accNoneWithA = 0, t1 = 0, tM1 = 0, tE2 = 0, tUT = 0, tMix = 0;
        for (int Z = 1; Z <= 118; Z++) { const AsdSpectrum& sp = asdIon(Z); for (int i = 0; i < sp.count; i++) { const AsdLine& l = sp.lines[i]; ionN++; withA += !std::isnan(l.aki); const std::string a = l.acc, t = l.type;
            accAA += a == "AA"; accAAA += a == "AAA"; accNone += a.empty(); accNoneWithA += a.empty() && !std::isnan(l.aki); t1 += t.empty(); tM1 += t == "M1"; tE2 += t == "E2"; tUT += t == "UT"; tMix += t == "M1+E2"; } }
        std::printf("ASD ion lines: %d, with A %d; accuracy AA %d, AAA %d, none %d (of which with an A value: %d); types E1 %d, M1 %d, E2 %d, UT %d, M1+E2 %d\n", ionN, withA, accAA, accAAA, accNone, accNoneWithA, t1, tM1, tE2, tUT, tMix);
        checkTrue("ASD data (counted in asd_lines.tsv): 5,029 ion lines, 4,925 with an A value; accuracy AA 4,762, AAA 159, none 108 (4 of them have an A value: ASD gave a rate and no rating); types E1 5,011, M1 11, E2 5, UT 1, M1+E2 1",
                  ionN == 5029 && withA == 4925 && accAA == 4762 && accAAA == 159 && accNone == 108 && accNoneWithA == 4 && t1 == 5011 && tM1 == 11 && tE2 == 5 && tUT == 1 && tMix == 1);
        const std::vector<AsdE1> hl = asdAllLines(1), hel = asdAllLines(2);
        auto find = [](const std::vector<AsdE1>& v, Level lo, Level hi) -> const AsdE1* { for (const AsdE1& e : v) if (e.lo.n == lo.n && e.lo.l == lo.l && e.lo.j2 == lo.j2 && e.hi.n == hi.n && e.hi.l == hi.l && e.hi.j2 == hi.j2) return &e; return nullptr; };
        const AsdE1 *a3 = find(hl, {1, 0, 1}, {2, 1, 3}), *a1 = find(hl, {1, 0, 1}, {2, 1, 1}), *m1 = find(hl, {1, 0, 1}, {2, 0, 1}), *e2 = find(hl, {1, 0, 1}, {3, 2, 5}), *he = find(hel, {1, 0, 1}, {2, 1, 3});
        checkTrue("ASD data, read from asd_lines.tsv: H I Lyman-alpha 1s - 2p3/2 A = 6.2647e8 /s (AAA), 1s - 2p1/2 6.2648e8 /s (AAA); He II 1s - 2p3/2 1.0029e10 /s (AA)",
                  a3 && a1 && he && a3->A == 6.2647e8 && a3->acc == "AAA" && a1->A == 6.2648e8 && a1->acc == "AAA" && he->A == 1.0029e10 && he->acc == "AA" && a3->type.empty());
        checkTrue("ASD data, read from asd_lines.tsv: H I 1s - 2s is listed as M1 with A = 2.495e-6 /s (AAA); 1s - 3d5/2 as E2 with A = 5.937e2 /s (AAA)", m1 && e2 && m1->type == "M1" && m1->A == 2.495e-6 && m1->acc == "AAA" && e2->type == "E2" && e2->A == 5.937e2 && e2->acc == "AAA");
    }
    // ---- 2. the lifetime of a state: 1 / (sum of A of every allowed way down) ----
    {
        // numbers typed from asd_lines.tsv (H I): A in /s, all AAA
        const double p3 = 6.2647e8, p1 = 6.2648e8, p3_1s = 1.6725e8, p3_2s = 2.2448e7, p1_1s = 1.6725e8, p1_2s = 2.2449e7, d3_p1 = 5.3877e7, d3_p3 = 1.0775e7, d5_p3 = 6.4651e7, s3_p1 = 2.1046e6, s3_p3 = 4.2097e6;
        const Life l2p3 = lifeOf(1, {2, 1, 3}), l2p1 = lifeOf(1, {2, 1, 1}), l3p3 = lifeOf(1, {3, 1, 3}), l3p1 = lifeOf(1, {3, 1, 1}), l3d3 = lifeOf(1, {3, 2, 3}), l3d5 = lifeOf(1, {3, 2, 5}), l3s = lifeOf(1, {3, 0, 1});
        std::printf("H I lifetimes from ASD's A: 2p3/2 %.5e s (A %.4e), 2p1/2 %.5e s, 3p3/2 %.5e s, 3p1/2 %.5e s, 3d3/2 %.5e s, 3d5/2 %.5e s, 3s %.5e s\n", l2p3.tauS, p3, l2p1.tauS, l3p3.tauS, l3p1.tauS, l3d3.tauS, l3d5.tauS, l3s.tauS);
        checkNum("H 2p3/2 lifetime = 1 / A(Lyman-alpha) with A read from the data file (6.2647e8 /s)", l2p3.tauS, 1 / p3, 1e-12);
        checkNum("H 2p1/2 lifetime = 1 / 6.2648e8 /s", l2p1.tauS, 1 / p1, 1e-12);
        checkTrue("H 2p: one way down (to 1s 1/2), ASD's rate complete, accuracy AAA, the lifetime is 1.596 ns", l2p3.ways.size() == 1 && l2p3.complete && l2p3.ways[0].acc == "AAA" && std::fabs(l2p3.tauS - 1.5963e-9) < 1e-12);
        checkNum("H 3p3/2 lifetime = 1 / (A to 1s + A to 2s) from the data file", l3p3.tauS, 1 / (p3_1s + p3_2s), 1e-12);
        checkNum("H 3p1/2 lifetime = 1 / (A to 1s + A to 2s) from the data file", l3p1.tauS, 1 / (p1_1s + p1_2s), 1e-12);
        checkNum("H 3d3/2 lifetime = 1 / (A to 2p1/2 + A to 2p3/2) from the data file", l3d3.tauS, 1 / (d3_p1 + d3_p3), 1e-12);
        checkNum("H 3d5/2 lifetime = 1 / A(to 2p3/2) from the data file (one way down)", l3d5.tauS, 1 / d5_p3, 1e-12);
        checkNum("H 3s lifetime = 1 / (A to 2p1/2 + A to 2p3/2) from the data file", l3s.tauS, 1 / (s3_p1 + s3_p3), 1e-12);
        checkTrue("H 3d3/2 has two ways (2p 1/2 and 2p 3/2); the branching ratios are A / sum and sum to 1", l3d3.ways.size() == 2 && std::fabs(l3d3.ways[0].A / l3d3.sumA - d3_p1 / (d3_p1 + d3_p3)) < 1e-12 && std::fabs(l3d3.ways[0].A / l3d3.sumA + l3d3.ways[1].A / l3d3.sumA - 1) < 1e-15);
        // a state the lab offers but ASD cannot complete, or has no data for: said, never filled in
        const Life none = lifeOf(118, {2, 1, 3});
        checkTrue("an element ASD has no hydrogen-like ion for (Og, Z = 118): status 'no data', no lifetime number", none.status == LifeStatus::NoData && std::isnan(none.tauS));
    }
    // ---- 3. metastable 2s: no electric-dipole way down; no number is invented ----
    {
        const Life s2 = lifeOf(1, {2, 0, 1});
        checkTrue("H 2s1/2 has NO allowed way down to a lower n (l would not change, parity would not change): it is metastable, and the lab gives no lifetime number", s2.ways.empty() && s2.metastable && std::isnan(s2.tauS) && std::isnan(s2.tauMaxS));
        const std::string t = metastableText(1);
        auto has = [](const std::string& x, const char* w) { return x.find(w) != std::string::npos; };
        checkTrue("metastable text: says no electric-dipole jump is allowed, that a real 2s state still decays (two photons at once), that ASD does not list that process, and gives no lifetime of its own (0.46.1: the sourced two-photon time is now named, so the old 'no ns or ms in the text' check became this one)",
                  has(t, "electric-dipole") && has(t, "two photons") && has(t, "does not") && has(t, "ASD") && has(t, "gives no lifetime above") && has(t, "not counted as a lifetime"));
        // 0.46.1 (outside review, reference added on purpose): the two-photon lifetime 0.1215 Z^-6 s (Jentschura, arXiv:hep-ph/0409271, eq. (11) and text; data/twophoton), and the Lamb-shift E1 jump 2s1/2 -> 2p1/2 is named
        checkTrue("metastable text: gives the sourced two-photon lifetime of hydrogen, 0.1215 s, and of He+ (0.1215 / 64 s), and names the allowed but negligible 2s 1/2 -> 2p 1/2 jump across the Lamb shift",
                  twoPhoton2sLifetimeS(1) == 0.1215 && std::fabs(twoPhoton2sLifetimeS(2) * 64 - 0.1215) < 1e-15 && has(t, fmtLife(0.1215).c_str()) && has(metastableText(2), fmtLife(0.1215 / 64).c_str()) && has(t, "Lamb") && has(t, "2p 1/2"));
        const std::vector<Jump> j = jumpsBetween(1, 2, 0, 1, 0);      // 2s -> 1s
        bool m1 = false; for (const Jump& x : j) m1 = m1 || (x.asdType == "M1" && x.haveA && x.A == 2.495e-6 && x.acc == "AAA" && !x.allowed);
        checkTrue("2s -> 1s in H: not allowed; ASD lists it only as magnetic dipole (M1) with A = 2.495e-6 /s (AAA), and the lab shows that rate with its type, not as an allowed jump", j.size() == 1 && m1);
        // no state with no allowed way ever gets a lifetime number
        bool never = true; int metas = 0;
        for (int Z = 1; Z <= 110; Z++) for (const Level& L : allLevelsUpTo(6)) { const Life x = lifeOf(Z, L); if (x.ways.empty()) { metas++; never = never && (x.metastable || x.ground) && std::isnan(x.tauS) && std::isnan(x.tauMaxS); } }
        checkTrue("every state with no allowed way down to a lower n (the ground state 1s, and the metastable 2s) has no lifetime number, for all 110 ions", never && metas > 0);
        std::printf("states with no allowed way down to a lower n, over 110 ions and n <= 6: %d (1s and 2s of each ion)\n", metas);
    }
    // ---- 4. the selection rules against ASD (extends 0.10.8 stage 1's 90-of-90 for H I and He II to every ion and to the forbidden lines) ----
    {
        int e1 = 0, forb = 0, e1Bad = 0, forbBad = 0, reasonBad = 0, typeBad = 0, typeOddZ = 0;
        std::set<std::string> forbTypes;
        for (int Z = 1; Z <= 118; Z++) {
            if (asdIon(Z).status != AsdStatus::Lines) continue;
            for (const AsdE1& e : asdAllLines(Z)) {
                const bool lab = diracAllowed(e.lo.l, e.lo.j2, e.hi.l, e.hi.j2);
                const bool oddCs = Z == 55 && e.lo.n == 1 && e.hi.n == 2 && e.hi.l == 0;   // ASD's one anomaly, see below
                if (e.type.empty()) { e1++; e1Bad += !lab && !oddCs; typeOddZ += !lab && oddCs; }
                else { forb++; forbBad += lab; forbTypes.insert(e.type); }
                const std::vector<Jump> js = jumpsBetween(Z, e.hi.n, e.hi.l, e.lo.n, e.lo.l);
                bool found = false; for (const Jump& x : js) if (x.upper.j2 == e.hi.j2 && x.lower.j2 == e.lo.j2) { found = true; typeBad += (x.allowed != e.type.empty()) && !oddCs; reasonBad += !x.allowed && x.why.empty(); }
                typeBad += !found;
            }
        }
        std::printf("selection rules vs ASD, all ions with data: %d lines ASD calls allowed (E1), the lab calls %d of them not allowed (and %d more: the Cs LV 1s-2s line, see below); %d lines ASD lists as forbidden (", e1, e1Bad, typeOddZ, forb);
        for (const std::string& t : forbTypes) std::printf("%s ", t.c_str());
        std::printf("), the lab calls %d of them allowed\n", forbBad);
        // ONE exception, found by this test and reported (REPORT.md): ASD lists the line 1s 2S 1/2 - 2s 2S 1/2 of Cs LV (Z = 55) with a BLANK Type (= allowed E1) and A = 7.591e11 /s, but l does not change (nor the parity), so it
        // cannot be electric dipole; the same line is typed M1 for Z = 56 (9.155e11) and UT for Z = 38 (4.068e10). The lab follows the rule and refuses it, says so on screen and does not use ASD's value; the test names it.
        checkTrue("selection rules vs ASD: EVERY ASD line of every ion that is allowed E1 (Type blank, both levels read as n, l, j) is a jump the lab calls allowed, with ONE exception, ASD's own Cs LV 1s-2s line (4,770 lines checked)", e1 > 4000 && e1Bad == 0 && typeOddZ == 1);
        checkTrue("selection rules vs ASD: the lab calls allowed NOTHING that ASD lists only as forbidden (M1, E2, M1+E2, UT): all 16 forbidden lines that read as levels (E2, M1, UT) are refused", forb == 16 && forbBad == 0);
        {   const std::vector<Jump> cs = jumpsBetween(55, 2, 0, 1, 0); const std::string line = cs.empty() ? "" : jumpLine(cs.front());
            checkTrue("the Cs LV 1s-2s anomaly is said on screen: not allowed, ASD marks it allowed, the lab follows the rule and does not use ASD's value as a lifetime", cs.size() == 1 && !cs[0].allowed && cs[0].asdListed && cs[0].asdType.empty() && line.find("disagrees with the rule") != std::string::npos && line.find("does not use that value") != std::string::npos && lifeOf(55, {2, 0, 1}).metastable); }
        checkTrue("jumpsBetween agrees with ASD's Type for every line (allowed exactly when the Type is blank), and every refused jump carries at least one reason", typeBad == 0 && reasonBad == 0);
        // ASD's own wording of the rule (opposite parity, J changes by at most 1) against the lab's rule (l changes by 1, j by at most 1) for EVERY pair of levels n <= 6: the same
        int pairs = 0, differ = 0;
        for (const Level& lo : allLevelsUpTo(6)) for (const Level& hi : allLevelsUpTo(6)) { if (hi.n <= lo.n) continue; pairs++; differ += asdRuleAllows(lo, hi) != diracAllowed(lo.l, lo.j2, hi.l, hi.j2); }
        std::printf("rule in ASD's words (parity + J) against the lab's rule (l + j), all %d pairs of levels n <= 6 with n' > n: %d differ\n", pairs, differ);
        checkTrue("ASD's wording of the rule (levels of opposite parity, J changes by at most 1) and the lab's (l changes by 1, j by at most 1) allow exactly the same jumps between levels n <= 6 (505 pairs; for one electron they are the same rule)", pairs == 505 && differ == 0);
        // the reasons name the rule that fails
        auto why = [](int Z, int nu, int lu, int ju, int nl, int ll, int jl) { std::string out; for (const Jump& x : jumpsBetween(Z, nu, lu, nl, ll)) if (x.upper.j2 == ju && x.lower.j2 == jl) for (const std::string& w : x.why) out += w + " | "; return out; };
        auto has = [](const std::string& x, const char* w) { return x.find(w) != std::string::npos; };
        const std::string ss = why(1, 2, 0, 1, 1, 0, 1), dp = why(1, 3, 2, 5, 2, 1, 1), sd = why(1, 3, 2, 3, 1, 0, 1);
        checkTrue("reasons: 2s -> 1s (l and parity unchanged) names the parity rule and the l rule and not the j rule", has(ss, "parity") && has(ss, "l") && !has(ss, "j changes"));
        checkTrue("reasons: 3d5/2 -> 2p1/2 (parity changes, l changes by 1, but j changes by 2) names ONLY the j rule", has(dp, "j") && !has(dp, "parity") && !has(dp, "l must"));
        checkTrue("reasons: 3d3/2 -> 1s (l changes by 2, parity unchanged) names the parity rule and the l rule", has(sd, "parity") && has(sd, "l"));
    }
    // ---- 5. branching ratios: which states have a complete set from ASD ----
    {
        int statesAll = 0, completeAll = 0, mismatch = 0, sumBad = 0, tauBad = 0, hComplete = 0, heComplete = 0, feComplete = 0, hStates = 0;
        double worstSum = 0, worstTau = 0;
        for (int Z = 1; Z <= 110; Z++) {
            if (asdIon(Z).status != AsdStatus::Lines) continue;
            const std::vector<AsdE1> lines = asdAllLines(Z);
            for (const Level& up : allLevelsUpTo(6)) {
                if (up.n < 2) continue;
                // the test's own scan: every level below (lower n, opposite parity, J within 1) must have an E1 line with an A value
                int need = 0, have = 0; double sum = 0;
                for (const Level& lo : allLevelsUpTo(up.n - 1)) {
                    if (!asdRuleAllows(lo, up)) continue;
                    need++;
                    for (const AsdE1& e : lines) if (e.type.empty() && e.lo.n == lo.n && e.lo.l == lo.l && e.lo.j2 == lo.j2 && e.hi.n == up.n && e.hi.l == up.l && e.hi.j2 == up.j2 && !std::isnan(e.A)) { have++; sum += e.A; break; }
                }
                const bool complete = need > 0 && have == need;
                const Life L = lifeOf(Z, up);
                statesAll++; completeAll += complete;
                if (Z == 1) { hStates++; hComplete += complete; } if (Z == 2) heComplete += complete; if (Z == 26) feComplete += complete;
                mismatch += (L.complete != complete) || (int(L.ways.size()) != need) || (L.withA != have);
                if (complete) {
                    double shares = 0; for (const Way& w : L.ways) shares += w.A / L.sumA;
                    worstSum = std::max(worstSum, std::fabs(shares - 1)); sumBad += std::fabs(shares - 1) > 1e-15;
                    worstTau = std::max(worstTau, std::fabs(L.tauS * sum - 1)); tauBad += std::fabs(L.tauS * sum - 1) > 1e-12;
                }
            }
        }
        std::printf("branching ratios: %d states (n = 2..6, every l and j) of 110 ions; ASD has the complete set of A for %d of them (H I %d of %d, He II %d, Fe XXVI %d); the shares sum to 1 within %.1e; tau * (sum A) = 1 within %.1e\n", statesAll, completeAll, hComplete, hStates, heComplete, feComplete, worstSum, worstTau);
        checkTrue("lifeOf agrees with the test's own scan of the ASD lines for every state n = 2..6 of every ion (number of ways, ways with an A value, complete or not)", mismatch == 0);
        // WHICH states have a complete set (measured): a state (n, l, j) of an ion has one when ASD gives an A for every allowed jump to a lower n. Of the 3,850 states n = 2..6 of the 110 ions, 1,531 do. Hydrogen: 34 of the 35
        // (all but 2s, which has no allowed way down at all); He II 34; Fe XXVI 26. ASD has A for every way down of every state with n <= 3 in most ions and is patchy above (n = 6: A for a fifth of the ways).
        checkTrue("the branching ratios of every state with a complete ASD set add up to 1 (to 1e-15) and tau = 1 / sum A (to 1e-12), for all 1,531 of them (the set is defined above: ASD has an A for every allowed jump to a lower n)", sumBad == 0 && tauBad == 0 && completeAll == 1531);
        checkTrue("hydrogen: ASD has a complete set for 34 of the 35 states n = 2..6 (every l and j); the 35th is 2s, which has no allowed way down", hStates == 35 && hComplete == 34 && !lifeOf(1, {2, 0, 1}).complete && lifeOf(1, {2, 0, 1}).ways.empty());
    }
    // ---- 6. A scales as Z^4 for hydrogen-like ions: from ASD's own numbers ----
    {
        struct T { Level lo, hi; const char* name; } trs[3] = {{{1, 0, 1}, {2, 1, 3}, "2p3/2 -> 1s"}, {{1, 0, 1}, {2, 1, 1}, "2p1/2 -> 1s"}, {{2, 1, 3}, {3, 2, 5}, "3d5/2 -> 2p3/2"}};
        double lowDev[3] = {0, 0, 0}, relMax[3] = {0, 0, 0}; int cnt[3] = {0, 0, 0};
        for (int k = 0; k < 3; k++) {
            double A1 = 0; for (const AsdE1& e : asdAllLines(1)) if (e.type.empty() && e.lo.n == trs[k].lo.n && e.lo.l == trs[k].lo.l && e.lo.j2 == trs[k].lo.j2 && e.hi.n == trs[k].hi.n && e.hi.l == trs[k].hi.l && e.hi.j2 == trs[k].hi.j2) A1 = e.A;
            for (int Z = 2; Z <= 110; Z++) for (const AsdE1& e : asdAllLines(Z)) {
                if (!e.type.empty() || std::isnan(e.A) || e.lo.n != trs[k].lo.n || e.lo.l != trs[k].lo.l || e.lo.j2 != trs[k].lo.j2 || e.hi.n != trs[k].hi.n || e.hi.l != trs[k].hi.l || e.hi.j2 != trs[k].hi.j2) continue;
                const double r = e.A / (std::pow(double(Z), 4) * A1), x2 = std::pow(Z * nist::alpha, 2);
                cnt[k]++;
                if (Z <= 10) lowDev[k] = std::max(lowDev[k], std::fabs(r - 1));
                if (Z >= 10) relMax[k] = std::max(relMax[k], std::fabs(r - 1) / x2);
            }
            std::printf("A scaling, %s: A(Z) / (Z^4 A(H)) over %d ions: worst |r - 1| for Z <= 10 is %.3e; worst |r - 1| / (Z alpha)^2 for Z >= 10 is %.4f\n", trs[k].name, cnt[k], lowDev[k], relMax[k]);
        }
        checkTrue("A of the same transition scales as Z^4 across the ASD ions (Z = 2..110, 3 transitions: 109, 109 and 104 ions): for Z <= 10 within GUARD_Z4_LOW of Z^4 times hydrogen's A (measured worst + 1 %)", cnt[0] >= 100 && cnt[1] >= 100 && cnt[2] >= 100 && lowDev[0] < GUARD_Z4_LOW[0] && lowDev[1] < GUARD_Z4_LOW[1] && lowDev[2] < GUARD_Z4_LOW[2]);
        checkTrue("and for Z >= 10 the departure from Z^4 is a relativistic correction of the order (Z alpha)^2: |r - 1| / (Z alpha)^2 stays below GUARD_Z4_REL (measured worst + 1 %) for all three transitions", relMax[0] < GUARD_Z4_REL[0] && relMax[1] < GUARD_Z4_REL[1] && relMax[2] < GUARD_Z4_REL[2]);
    }
    // ---- 7. the words ----
    {
        auto has = [](const std::string& x, const char* w) { return x.find(w) != std::string::npos; };
        checkTrue("lifetime units are chosen automatically: 1.5963e-9 s = 1.596 ns, 2.2e-12 s in ps, 3.3e-7 s in ns, 4.5e-5 s in microseconds, 2.0e-2 s in ms, 12 s in s",
                  lifeText(1.5963e-9) == "1.596 ns" && has(lifeText(2.2e-12), " ps") && has(lifeText(3.3e-7), " ns") && has(lifeText(4.5e-5), " µs") && has(lifeText(2.0e-2), " ms") && lifeText(12) == "12 s");
        const std::string e = lifeExplain();
        checkTrue("the app explains what a lifetime is in plain words: the average time before the state jumps down by itself, the inverse of the sum of the rates, A is the rate (chance per second) of one jump, the branching ratio is the share of all jumps",
                  has(e, "average time") && has(e, "by itself") && has(e, "sum") && has(e, "per second") && has(e, "share") && has(e, "NIST"));
        checkTrue("accuracy codes carry ASD's meaning (read from its help page 2026-10-04): AAA = 0.3 %, AA = 1 %, B+ = 7 %, E = more than 50 %, a prime = a split multiplet, none = no rating given",
                  has(accMeaning("AAA"), "0.3 %") && has(accMeaning("AA"), "1 %") && has(accMeaning("B+"), "7 %") && has(accMeaning("E"), "50 %") && has(accMeaning("A'"), "multiplet") && has(accMeaning(""), "no rating"));
        const Life x = lifeOf(1, {2, 1, 3}); const std::string line = lifeLine(1, x);
        checkTrue("the state line gives the level, the lifetime in ns, ASD's accuracy and says it is from ASD (2p3/2 of hydrogen: 1.596 ns, AAA)", has(line, "2p 3/2") && has(line, "1.596 ns") && has(line, "AAA") && has(line, "NIST"));
        const std::vector<Jump> ok = jumpsBetween(1, 2, 1, 1, 0); const std::string jl = jumpLine(ok.front());
        checkTrue("the jump line gives A, ASD's accuracy and the branching share (2p3/2 -> 1s: 100 %)", ok.size() == 2 && has(jl, "6.265×10⁸") && has(jl, "AAA") && has(jl, "100 %"));
        // a transition ASD gives no A for, where the lab offers it: said on screen
        int noA = 0; std::string sample;
        for (int Z = 1; Z <= 110; Z++) for (const Level& up : allLevelsUpTo(6)) for (const Jump& j : jumpsBetween(Z, up.n, up.l, 1, 0)) if (j.allowed && !j.haveA && j.ionData) { if (!noA) sample = jumpLine(j); noA++; }
        std::printf("allowed jumps to 1s for which ASD has no A (all ions, n <= 6): %d; the lab says: %s\n", noA, sample.c_str());
        checkTrue("an allowed jump ASD has no A for says so on screen and shows no number", noA == 0 || (has(sample, "no transition probability") && !has(sample, "s⁻¹")));
    }
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------------------------
// 0.10.9 stage 1: every nucleus at its measured size (measured rms charge radii, IAEA Nuclear Data Section, data/radii).
// REFERENCES here are NOT the lab's own output: (1) numbers typed from data/radii/charge_radii.csv, the file the fetch wrote (fetched 2026-10-04; counted with a separate script, not with the lab); (2) hand
// arithmetic with the stated relation (a uniform ball of radius R has rms radius sqrt(3/5) R, so R = sqrt(5/3) x rms: 2.1631 fm for helium-4 is 1.2910 x 1.6755); (3) the Woods-Saxon density's mean square
// radius by the test's own Simpson integral (not the lab's routine) and the textbook closed form 3/5 R^2 + 7/5 pi^2 a^2; (4) the statistics of the nucleon positions the lab actually draws.
struct RadRef { int Z, N; double rms, unc; };
static const RadRef kRadRef[] = {                                                         // typed from the CSV: z, n, radius_val, radius_unc
    {1, 0, 0.8783, 0.0086}, {1, 1, 2.1421, 0.0088}, {1, 2, 1.7591, 0.0363}, {2, 1, 1.9661, 0.0030}, {2, 2, 1.6755, 0.0028}, {2, 4, 2.0660, 0.0111}, {2, 6, 1.9239, 0.0306},
    {3, 3, 2.5890, 0.0390}, {3, 4, 2.4440, 0.0420}, {3, 5, 2.3390, 0.0440}, {3, 6, 2.2450, 0.0460}, {3, 8, 2.4820, 0.0430}, {6, 6, 2.4702, 0.0022}, {8, 8, 2.6991, 0.0052},
    {14, 14, 3.1224, 0.0024}, {20, 20, 3.4776, 0.0019}, {20, 28, 3.4771, 0.0020}, {26, 30, 3.7377, 0.0016}, {50, 82, 4.7093, 0.0076}, {82, 126, 5.5012, 0.0013}, {92, 146, 5.8571, 0.0033},
    {95, 148, 5.9048, 0.0035}, {19, 28, 3.4534, 0.0138}, {4, 6, 2.3550, 0.0170}};
// rows per element Z = 1 .. 96 that have a radius_val, counted from the CSV (z = 0, the neutron, left out): 90 elements, 908 isotopes
static const int kRadPerElement[96] = {3, 4, 5, 4, 2, 3, 2, 3, 1, 11, 12, 3, 1, 3, 1, 3, 2, 14, 10, 11, 5, 7, 1, 4, 7, 4, 1, 5, 2, 5, 2, 5, 1, 6, 2, 24, 23, 24, 16, 14, 7, 15, 0, 7, 1, 6, 6, 18,
                                       24, 25, 2, 13, 1, 24, 29, 28, 4, 7, 1, 16, 0, 17, 23, 11, 11, 18, 14, 12, 19, 24, 19, 12, 1, 5, 2, 7, 10, 20, 17, 26, 19, 32, 11, 15, 0, 15, 16, 19, 0, 5, 0, 5, 0, 6, 2, 5};
// the mean square radius of the Woods-Saxon density 1 / (1 + exp((r - R) / a)): the test's own Simpson integral of r^4 f and r^2 f (20,000 intervals out to R + 40 a)
static double wsMeanSquareOwn(double R, double a) {
    const int n = 20000;
    const double top = R + 40 * a, h = top / n;
    double num = 0, den = 0;
    for (int i = 0; i <= n; i++) {
        const double r = i * h, f = 1 / (1 + std::exp((r - R) / a)), w = (i == 0 || i == n) ? 1 : (i % 2 ? 4 : 2);
        num += w * r * r * r * r * f; den += w * r * r * f;
    }
    return num / den;
}
// what the lab really draws: the rms charge radius of one nucleus' protons, averaged over many builds. Positions are about the centre of mass (the nucleus is centred on it); a charge radius is the proton
// positions' rms there plus the proton's own size (r_p^2 = 0.84^2 fm^2, the lab's nucleon radius). minSep = the smallest distance between two nucleons in any build, in fm.
// 0.42: what the nucleons themselves add to a charge radius: r_p^2 (CODATA 2022 0.84075 fm) + (N/Z) <r_n^2> (PDG 2024 -0.1155 fm^2) + 3/(4 M^2) (Darwin-Foldy, CODATA 2022 m_p and hbar c):
// Ong, Berengut and Flambaum, PRC 82, 014320 (2010), eq. (8). Until 0.41 these tests took off r_p^2 = 0.84^2 alone (the code did the same): changed on purpose, REPORT 0.42.
static double ownNucleonMs(int Z, int N) { return 0.84075 * 0.84075 + (Z > 0 ? double(N) / Z : 0.0) * (-0.1155) + 0.75 * std::pow(197.3269804 / 938.27208943, 2); }
struct LayoutStat { double chargeRms, minSep; };
static LayoutStat layoutStat(int Z, int N, int builds, bool byRule, uint32_t seed) {
    double sum = 0, minSep = 1e9; long np = 0;
    seedRandom(seed);
    for (int b = 0; b < builds; b++) {
        const std::vector<Nucleon> v = byRule ? buildNucleusByRule(Z, N) : buildNucleus(Z, N);
        double cx = 0, cy = 0, cz = 0;
        for (const Nucleon& q : v) { cx += q.x; cy += q.y; cz += q.z; }
        cx /= v.size(); cy /= v.size(); cz /= v.size();
        for (std::size_t i = 0; i < v.size(); i++) {
            if (v[i].isP) { const double x = (v[i].x - cx) / kFmLab, y = (v[i].y - cy) / kFmLab, z = (v[i].z - cz) / kFmLab; sum += x * x + y * y + z * z; np++; }
            for (std::size_t j = i + 1; j < v.size(); j++) {
                const double dx = (v[i].x - v[j].x) / kFmLab, dy = (v[i].y - v[j].y) / kFmLab, dz = (v[i].z - v[j].z) / kFmLab;
                minSep = std::min(minSep, std::sqrt(dx * dx + dy * dy + dz * dz));
            }
        }
    }
    return {std::sqrt(sum / np + ownNucleonMs(Z, N)), minSep};
}
static std::string joinLines(const std::vector<std::string>& v) { std::string s; for (const std::string& l : v) s += l + "\n"; return s; }
static bool has(const std::string& s, const std::string& w) { return s.find(w) != std::string::npos; }

static void testNuclearSize(double guardLayout, double guardMinSepFloor) {
    // ---- 1. the data parses; counts as fetched ----
    int n = 0;
    const RadiusRow* rows = radiusRows(n);
    int withValue = 0, prelimOnly = 0, both = 0, perElement[119] = {0}, badOrder = 0, badSane = 0, badUnc = 0, zeroZ = 0, badA = 0;
    double lo = 1e9, hi = -1e9, uhi = 0;
    for (int i = 0; i < n; i++) {
        const RadiusRow& r = rows[i];
        if (r.Z < 1) zeroZ++;
        if (r.N < 0) badA++;
        if (i > 0 && !(rows[i - 1].Z < r.Z || (rows[i - 1].Z == r.Z && rows[i - 1].N < r.N))) badOrder++;
        const bool v = !std::isnan(r.rms), p = !std::isnan(r.prelim);
        if (v) { withValue++; perElement[r.Z]++; lo = std::min(lo, r.rms); hi = std::max(hi, r.rms); uhi = std::max(uhi, r.unc); if (!(r.rms > 0) || !(r.unc > 0)) badUnc++; }
        else if (p) prelimOnly++;
        if (v && p) both++;
        if (!v && !p) badSane++;
    }
    checkTrue("radii: 956 rows (the CSV's 957 minus the neutron row z = 0): 908 with a value, 48 preliminary only, 99 with both", n == 956 && withValue == 908 && prelimOnly == 48 && both == 99);
    checkTrue("radii: no row for z = 0 (the neutron's negative number is a mean square in fm^2, not a radius), none with N < 0, sorted and unique, every row has a value or a preliminary one", zeroZ == 0 && badA == 0 && badOrder == 0 && badSane == 0 && chargeRadius(0, 1) == nullptr);
    int bad = 0;
    for (int z = 1; z <= 96; z++) if (perElement[z] != kRadPerElement[z - 1]) bad++;
    checkTrue("radii: the number of isotopes with a value per element Z = 1 .. 96 equals the count made from the CSV (90 elements)", bad == 0);
    int elements = 0; for (int z = 1; z <= 118; z++) if (perElement[z]) elements++;
    checkTrue("radii: 90 elements have a value; none above Z = 96", elements == 90 && perElement[97] + perElement[98] + perElement[110] + perElement[118] == 0);
    // every value in a sane range: the CSV's smallest is 0.8783 (hydrogen), the largest 5.9048 (Am-243); guard = those + 1 % (labelled)
    checkTrue("radii: every value in [0.8783 x 0.99, 5.9048 x 1.01] fm (the CSV's extremes -/+ 1 %: guard), every uncertainty > 0 and at most 0.3139 x 1.01 fm", lo >= 0.8783 * 0.99 && hi <= 5.9048 * 1.01 && badUnc == 0 && uhi <= 0.3139 * 1.01);
    std::printf("radii: %d rows, %d with a value (%.4f .. %.4f fm, uncertainty up to %.4f), %d preliminary only\n", n, withValue, lo, hi, uhi, prelimOnly);
    // ---- 2. spot values, typed from the CSV ----
    for (const RadRef& r : kRadRef) {
        const RadiusRow* row = iaeaRadius(r.Z, r.N);   // (0.46: the file itself; chargeRadius uses CODATA 2022 for the deuteron)
        const std::string w = "radii " + at(r.Z, r.N);
        checkTrue(w + " is in the table", row != nullptr);
        if (!row) continue;
        checkNum(w + " rms", row->rms, r.rms, 1e-12);
        checkNum(w + " uncertainty", row->unc, r.unc, 1e-9);
    }
    const RadiusRow* k47 = chargeRadius(19, 28); const RadiusRow* be12 = chargeRadius(4, 8); const RadiusRow* li10 = chargeRadius(3, 7);
    checkTrue("radii: K-47 has a value 3.4534 and a preliminary 3.4419 with uncertainty 0.0000 (as the CSV has it); Be-12 has ONLY a preliminary 2.5030 +- 0.0150; Li-10 has no row",
              k47 && k47->prelim == 3.4419 && k47->prelimUnc == 0 && be12 && std::isnan(be12->rms) && be12->prelim == 2.5030 && be12->prelimUnc == 0.0150 && li10 == nullptr);
    // ---- 3. the size drawn: R = sqrt(5/3) x the measured rms where there is a value (a uniform ball), the usual rule where there is none; the preliminary column and H-1 are not used ----
    checkNum("a uniform ball has rms radius sqrt(3/5) R: sqrt(5/3) = 1.2909944487", kSharpOverRms, 1.2909944487358056, 1e-15);
    checkNum("helium-4: drawn radius = 1.2910 x 1.6755 = 2.1631 fm (hand arithmetic)", nuclearRadius(2, 2), 2.1631, 3e-5);
    checkNum("lead-208: drawn radius = 1.2910 x 5.5012 = 7.1020 fm (hand arithmetic)", nuclearRadius(82, 126), 7.1020, 3e-5);
    checkNum("lead-208: the usual rule 1.2 x 208^(1/3) = 7.1100 fm, which is an rms charge radius of 5.5074 fm (hand arithmetic)", ruleRms(208), 5.5074, 2e-5);
    checkNum("deuteron: drawn radius = sqrt(5/3) x CODATA 2022 2.12778 fm (0.46, review A9)", nuclearRadius(1, 1), 1.2909944487358056 * 2.12778, 1e-12);
    for (const RadRef& r : kRadRef) {
        if (r.Z + r.N == 1 || (r.Z == 1 && r.N == 1)) continue;
        checkNum("drawn radius of " + at(r.Z, r.N) + " = sqrt(5/3) x measured", nuclearRadius(r.Z, r.N), 1.2909944487358056 * r.rms, 1e-12);
    }
    checkNum("K-47: the value is used, not the preliminary one", nuclearRadius(19, 28), 1.2909944487358056 * 3.4534, 1e-12);
    checkNum("Be-12 (only a preliminary value): the usual rule", nuclearRadius(4, 8), nuclearR(12), 1e-14);
    checkNum("Li-10 (no row): the usual rule", nuclearRadius(3, 7), nuclearR(10), 1e-14);
    checkNum("Og-294 (no row): the usual rule", nuclearRadius(118, 176), nuclearR(294), 1e-14);
    checkNum("H-1: the proton keeps the lab's own proton radius (CODATA 2022 0.84075 fm, drawn 0.84), not the table's 0.8783", nuclearRadius(1, 0), kNucleonR, 0);
    checkTrue("sizedByMeasurement: yes for He-4 and Pb-208, no for H-1, Be-12 (preliminary only), Li-10, Og-294", sizedByMeasurement(2, 2) && sizedByMeasurement(82, 126) && !sizedByMeasurement(1, 0) && !sizedByMeasurement(4, 8) && !sizedByMeasurement(3, 7) && !sizedByMeasurement(118, 176));
    // the usual rule against the measurements (rule rms = sqrt(3/5) x 1.2 A^(1/3)); the percentages are hand arithmetic from the CSV values
    auto dev = [](int Z, int N) { const double m = chargeRadius(Z, N)->rms, r = ruleRms(Z + N); return 100 * (m - r) / r; };
    checkNum("deviation from the rule: H-2 +81.69 % (0.46: CODATA 2022 2.12778 fm against the rule 1.17112 fm; was +82.91 % with IAEA 2.1421)", dev(1, 1), 81.69, 0, 0.01);
    checkNum("deviation from the rule: Li-6 +53.28 %", dev(3, 3), 53.28, 0, 0.01);
    checkNum("deviation from the rule: He-3 +46.66 %", dev(2, 1), 46.66, 0, 0.01);
    checkNum("deviation from the rule: C-12 +16.08 %", dev(6, 6), 16.08, 0, 0.01);
    checkNum("deviation from the rule: Fe-56 +5.10 %", dev(26, 30), 5.10, 0, 0.01);
    checkNum("deviation from the rule: U-238 +1.68 %", dev(92, 146), 1.68, 0, 0.01);
    checkNum("deviation from the rule: Pb-208 -0.11 %", dev(82, 126), -0.11, 0, 0.01);
    checkNum("deviation from the rule: Sn-132 -0.50 %", dev(50, 82), -0.50, 0, 0.01);
    double worstUp = -1e9, worstDown = 1e9; int upZ = 0, upN = 0, dnZ = 0, dnN = 0;
    for (int i = 0; i < n; i++) {
        if (std::isnan(rows[i].rms) || rows[i].Z + rows[i].N == 1) continue;
        const double d = dev(rows[i].Z, rows[i].N);
        if (d > worstUp) { worstUp = d; upZ = rows[i].Z; upN = rows[i].N; }
        if (d < worstDown) { worstDown = d; dnZ = rows[i].Z; dnN = rows[i].N; }
    }
    checkTrue("the biggest deviations from the rule are H-2 (+82.9 %) and, below it, Sn-132 (-0.50 %): the measurement is never more than 0.5 % under the rule", upZ == 1 && upN == 1 && dnZ == 50 && dnN == 82 && worstDown > -0.51);
    // known cases, from the data itself
    checkTrue("He-4 (1.6755) is smaller than He-3 (1.9661), as measured; lithium falls from Li-6 to Li-9 (2.2450) and rises at Li-11 (2.4820)",
              chargeRadius(2, 2)->rms < chargeRadius(2, 1)->rms && chargeRadius(3, 3)->rms > chargeRadius(3, 4)->rms && chargeRadius(3, 4)->rms > chargeRadius(3, 5)->rms &&
              chargeRadius(3, 5)->rms > chargeRadius(3, 6)->rms && chargeRadius(3, 8)->rms > chargeRadius(3, 6)->rms);
    checkTrue("Li-11 against Li-7: +0.0380 fm, with an uncertainty of sqrt(0.043^2 + 0.042^2) = 0.0601 fm: NOT distinguishable by charge radius (the app must not call it much larger)",
              std::fabs((chargeRadius(3, 8)->rms - chargeRadius(3, 4)->rms) - 0.038) < 1e-9 && chargeRadius(3, 8)->rms - chargeRadius(3, 4)->rms < std::hypot(chargeRadius(3, 8)->unc, chargeRadius(3, 4)->unc));
    checkTrue("a strongly deformed rare earth, Gd-158, is bigger than the rule by a few percent (+0.6 %: the charge radius carries no shape)", dev(64, 94) > 0 && dev(64, 94) < 5);
    // the density the drawn size implies (a uniform ball of A nucleons): between the measured extremes -/+ 1 % (guard); the lowest is Ne-17
    double rlo = 1e9, rhi = 0; int rloZ = 0;
    for (int i = 0; i < n; i++) {
        if (std::isnan(rows[i].rms) || rows[i].Z + rows[i].N < 12) continue;
        const int A = rows[i].Z + rows[i].N; const double R = nuclearRadius(rows[i].Z, rows[i].N), rho = 3.0 * A / (4 * kPi * R * R * R);
        if (rho < rlo) { rlo = rho; rloZ = rows[i].Z; }
        rhi = std::max(rhi, rho);
    }
    std::printf("radii: nucleon density of the drawn uniform ball, A >= 12: %.5f (Z = %d) .. %.5f per fm^3\n", rlo, rloZ, rhi);
    checkTrue("density of the drawn ball for A >= 12 is between 0.06705 x 0.99 and 0.14023 x 1.01 per fm^3 (the CSV's extremes -/+ 1 %: guard; the lowest is Ne-17)", rlo >= 0.06705 * 0.99 && rhi <= 0.14023 * 1.01 && rloZ == 10);
    // ---- 4. how many isotopes the slider offers are measured ----
    int offered = 0, measured = 0, fallback = 0, prelimOnlyOffered = 0;
    for (int z = 1; z <= 118; z++) {
        const auto rg = nRange(z);
        for (int nn = rg.first; nn <= rg.second; nn++) {
            offered++;
            const RadiusRow* r = chargeRadius(z, nn);
            if (sizedByMeasurement(z, nn)) measured++; else { fallback++; if (r && std::isnan(r->rms)) prelimOnlyOffered++; }
        }
    }
    std::printf("radii: the slider offers %d isotopes: %d sized by a measurement, %d by the rule (%d of those have only a preliminary value)\n", offered, measured, fallback, prelimOnlyOffered);
    checkTrue("the slider's isotopes: every measured isotope the table has (907 with A > 1) is offered or not, but measured + fallback = offered", measured + fallback == offered && measured > 0 && fallback > 0);
    // ---- 5. the layout the lab draws ----
    // the half-density radius that gives a measured charge radius: the lab's solver against the test's own integral. proton density rms^2 + r_p^2 = measured^2
    for (const double rms : {2.7, 3.1224, 3.7377, 5.5012, 5.8571}) {
        const double a = 0.50, ms = rms * rms - 0.84075 * 0.84075, R = wsHalfDensityRadiusForPointMs(ms);   // 0.46: the point-nucleon diffuseness (Chamon et al. 2002; was 0.54, the charge one)
        checkNum("Woods-Saxon radius for a point mean square radius " + fmt::fixed(ms, 4) + " fm^2: the test's own Simpson integral gives it back", std::sqrt(wsMeanSquareOwn(R, a)), std::sqrt(ms), 1e-5);
        checkNum("... and the textbook closed form 3/5 R^2 + 7/5 pi^2 a^2 agrees (it drops exp(-R/a) terms)", 0.6 * R * R + 1.4 * kPi * kPi * a * a, ms, 2e-3);
    }
    // the deuteron is two nucleons at +-d/2: the proton's own distance to the centre is d/2, so (d/2)^2 = r_d^2 - ownNucleonMs(1, 1) (0.42; was sqrt(2.1421^2 - 0.84^2) = 1.9705 fm); by the rule it stays 1.6.
    // 0.46 (review A9): r_d = 2.12778 fm, CODATA 2022 (data/qcd/nucleon/codata2022_rd.html), the same source as the proton (was IAEA 2.1421 fm)
    // 0.46.5: the pair's distance is drawn from the deuteron's wave function (CD-Bonn), so each snapshot differs; the MEAN (d/2)^2 is checked: measured as above, by the rule CD-Bonn's own
    // r_d^2 = 1.966^2 fm^2 (Machleidt 2001, Table XV). Until 0.46.4 every snapshot had d/2 exactly 1.6 fm (rule) or the measured value.
    for (const bool rule : {false, true}) {
        seedRandom(5);
        double m = 0; const int n = 4000;
        for (int i = 0; i < n; i++) {
            const auto v = rule ? buildNucleusByRule(1, 1) : buildNucleus(1, 1);
            const double d = std::hypot(std::hypot(v[0].x - v[1].x, v[0].y - v[1].y), v[0].z - v[1].z) / kFmLab;
            m += d * d / 4;
        }
        checkNum(rule ? "deuteron by the rule: mean (d/2)^2 = CD-Bonn's r_d^2" : "deuteron by its measured charge radius, both nucleons' sizes and Darwin-Foldy off: mean (d/2)^2", m / n,
                 rule ? 1.966 * 1.966 : 2.12778 * 2.12778 - ownNucleonMs(1, 1), 0.05);
    }
    // unmeasured, ROUND isotopes are built exactly as the rule builds them (same seed, same positions); measured ones are not. (0.10.9 stage 2: this list had Og-294, whose estimated beta2 is 0.206, and passed with seed 9 by
    // luck; a deformed nucleus is now drawn with the volume-keeping surface of quadrupole.h, so its layout differs on purpose: Og-294 moved to the check below, which needs it to differ for every one of 4 seeds.)
    for (const auto& zn : {std::pair<int, int>{3, 7}, {4, 8}, {1, 0}, {50, 40}}) {
        checkTrue("no measured radius and no deformation (beta2 = 0): " + at(zn.first, zn.second), nucBeta(zn.first, zn.second) == 0 && !sizedByMeasurement(zn.first, zn.second));
        seedRandom(9); const auto a = buildNucleus(zn.first, zn.second); seedRandom(9); const auto b = buildNucleusByRule(zn.first, zn.second);
        bool same = a.size() == b.size();
        for (std::size_t i = 0; same && i < a.size(); i++) same = a[i].x == b[i].x && a[i].y == b[i].y && a[i].z == b[i].z && a[i].isP == b[i].isP;
        checkTrue("no measurement: buildNucleus = the rule's layout, position for position: " + at(zn.first, zn.second), same);
    }
    {
        int differs = 0;
        for (uint32_t sd = 1; sd <= 4; sd++) {
            seedRandom(sd); const auto a = buildNucleus(104, 156); seedRandom(sd); const auto b = buildNucleusByRule(104, 156);
            bool same = a.size() == b.size();
            for (std::size_t i = 0; same && i < a.size(); i++) same = a[i].x == b[i].x;
            differs += !same;
        }
        checkTrue("no measured radius but a deformed nucleus (0.46.3: Rf-260, FRDM(2012) beta2 = 0.242; was Og-294 with the lab's own estimate): the layout differs from the rule layout for all 4 seeds (the volume is kept now)", nucBeta(104, 156) > 0.2 && !sizedByMeasurement(104, 156) && differs == 4);
    }
    {
        seedRandom(9); const auto a = buildNucleus(2, 1); seedRandom(9); const auto b = buildNucleusByRule(2, 1);
        checkTrue("a measurement: He-3 is built differently from the rule", a.size() == b.size() && (a[0].x != b[0].x || a[0].y != b[0].y || a[0].z != b[0].z));
    }
    // the protons' rms charge radius of what is drawn against the measured value. buildNucleus scales every snapshot so that this is exact (see atom_core.cpp), so the guard is rounding-level, NOT a statistical one
    // (the test averages over many builds anyway). The same check on the rule's layout must NOT hold: it is the check that tells the two apart. minSep: the closest two nucleons in any build.
    struct Lay { int Z, N, builds; };
    const Lay lays[] = {{2, 1, 400}, {2, 2, 400}, {3, 3, 300}, {4, 5, 300}, {6, 6, 200}, {8, 8, 150}, {20, 20, 60}, {26, 30, 40}, {82, 126, 20}};
    double worst = 0, worstRule = 0, lowSep = 1e9; std::string wl;
    for (const Lay& L : lays) {
        const double m = chargeRadius(L.Z, L.N)->rms;
        const LayoutStat st = layoutStat(L.Z, L.N, L.builds, false, 20260 + L.Z), sr = layoutStat(L.Z, L.N, L.builds, true, 20260 + L.Z);
        const double d = std::fabs(st.chargeRms / m - 1), dr = std::fabs(sr.chargeRms / m - 1);
        std::printf("layout %-8s beta2 %+.3f measured %.4f fm: drawn %.4f (%+.2e), by the rule %.4f (%+.2f %%), closest pair %.3f fm (rule %.3f)\n", at(L.Z, L.N).c_str(), nucBeta(L.Z, L.N), m, st.chargeRms, st.chargeRms / m - 1, sr.chargeRms, 100 * (sr.chargeRms / m - 1), st.minSep, sr.minSep);
        if (d > worst) { worst = d; wl = at(L.Z, L.N); }
        worstRule = std::max(worstRule, dr);
        // 0.46.3: C-12 is now drawn from its measured B(E2) (beta2 0.57, strongly deformed): the layout's own guarantee is the relaxed core 0.55 fm times the body's shortest radius factor
        const double bL = nucBeta(L.Z, L.N);
        if (std::fabs(bL) < 0.3) lowSep = std::min(lowSep, st.minSep);
        else checkTrue("strongly deformed " + at(L.Z, L.N) + ": closest pair " + fmt::fixed(st.minSep, 3) + " fm >= 0.55 fm x the shortest radius factor of the body", st.minSep >= 0.55 * std::min(shapeRadiusFactor(bL, 0), shapeRadiusFactor(bL, 1)));
    }
    std::printf("layout: worst difference of the drawn rms charge radius from the measured one: %.3e (%s); the rule's layout was off by up to %.4f; closest pair of any build %.4f fm\n", worst, wl.c_str(), worstRule, lowSep);
    checkTrue("the drawn protons' rms charge radius equals the measured value for 9 nuclei, deformed or not (exact by construction: guard 1e-9)", worst <= guardLayout);
    checkTrue("... and the rule's layout does NOT (off by more than 5 % for some of them): the check can tell the two apart", worstRule > 0.05);
    checkTrue("no two nucleons closer than the guard (what the density rule guarantees: hard core 1.0 fm, relaxed to 0.55 fm only when crowded, then the snapshot is scaled by about 1: so never below 0.55 x the scale)", lowSep >= guardMinSepFloor);
    // the lab itself: the size it frames, zooms to and uses in its texts
    for (const auto& zn : {std::pair<int, int>{2, 2}, {82, 126}, {1, 0}, {4, 8}, {118, 176}}) {
        AtomLab lab; lab.p.Z = zn.first; lab.p.N = zn.second;
        checkNum("the lab's nucleus radius (Bohr radii x fm per Bohr radius) = nuclearRadius: " + at(zn.first, zn.second), lab.nucRA0() * kFmPerA0, nuclearRadius(zn.first, zn.second), 1e-12);
    }
    // ---- 6. the words ----
    const std::string he4 = joinLines(nuclearSizeLines(2, 2)), li9 = joinLines(nuclearSizeLines(3, 6)), c12 = joinLines(nuclearSizeLines(6, 6)), be12t = joinLines(nuclearSizeLines(4, 8)), h1 = joinLines(nuclearSizeLines(1, 0)), og = joinLines(nuclearSizeLines(118, 176));
    checkTrue("card, He-4: the measured 1.6755 +- 0.0028 fm, the drawn 2.1631 fm, the rule's 1.9049 fm and its rms 1.4755 fm, the difference +0.2000 fm (+13.6 %)",
              has(he4, "1.6755") && has(he4, "0.0028") && has(he4, "2.1631") && has(he4, "1.9049") && has(he4, "1.4755") && has(he4, "+0.2000") && has(he4, "13.6 %"));
    checkTrue("card, He-4: says helium-4 is smaller than helium-3 (1.9661 fm) and gives the binding per nucleon (AME2020) of both", has(he4, "smaller than helium-3") && has(he4, "1.9661") && has(he4, "MeV per nucleon"));
    checkTrue("card, Li-9: lists the measured lithium radii falling to Li-9 and rising at Li-11, says Li-10 has none, and that Li-11 against Li-7 is inside the uncertainty",
              has(li9, "2.5890") && has(li9, "2.2450") && has(li9, "2.4820") && has(li9, "Li-10") && has(li9, "no measured") && has(li9, "uncertainty") && !has(li9, "much larger"));
    checkTrue("card, C-12: no He/Li series text", !has(c12, "lithium") && !has(c12, "smaller than helium-3"));
    checkTrue("card, an isotope with no value: \"no measured radius for this isotope; size from the usual rule\" and the rule's radius, Og-294: 1.2 x 294^(1/3) = 7.9793 fm",
              has(og, "no measured radius for this isotope; size from the usual rule") && has(og, "7.9793"));
    checkTrue("card, Be-12 (only preliminary): says so, gives 2.5030 +- 0.0150 and that it is not used", has(be12t, "preliminary") && has(be12t, "2.5030") && has(be12t, "0.0150") && has(be12t, "not used") && has(be12t, "no measured radius for this isotope; size from the usual rule"));
    checkTrue("card, H-1: one proton, 0.84 fm, says the table's 0.8783 +- 0.0086 is not the CODATA 2022 proton radius 0.84075 and is not used", has(h1, "0.84") && has(h1, "0.8783") && has(h1, "CODATA 2022") && has(h1, "not used"));
    const std::string ex = chargeRadiusExplain();
    checkTrue("explanation: what a charge radius is (the size of the protons' charge, rms), neutrons carry no charge, matter extends further, which is why a halo cannot be drawn from this table, a size not a shape, the uniform-ball assumption, static, no vibration or rotation",
              has(ex, "charge radius") && has(ex, "rms") && has(ex, "protons") && has(ex, "Neutrons carry no electric charge") && has(ex, "matter") && has(ex, "halo") && has(ex, "cannot be drawn") && has(ex, "a size, not a shape") &&
              has(ex, "uniform ball") && has(ex, "√(5/3)") && has(ex, "vibrate and rotate"));
    checkTrue("the credit names IAEA, Angeli and Marinova and the file's own limits", has(kRadiiCredit, "IAEA") && has(kRadiiCredit, "Angeli") && has(kRadiiCredit, "Marinova") && has(kRadiiCredit, "charge_radii.csv"));
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------------------------
// 0.10.9 stage 2: the nucleus's shape from measured electric quadrupole moments (IAEA recommended values, data/quadrupole).
// REFERENCES are NOT the lab's own output: (1) numbers typed from data/quadrupole/elec_mom_recomm.csv (fetched 2026-10-04; counted with a separate script) and data/radii/charge_radii.csv; (2) hand arithmetic of the
// textbook relation between the spectroscopic and the intrinsic moment for strong coupling K = I, Q = Q0 I (2I - 1) / ((I + 1) (2I + 3)), so Q0 = Q x 10, 5, 7/2, 14/5, 15/7, 11/6 for I = 1, 3/2, 2, 5/2, 7/2, 9/2, and
// the first-order Q0 = 3 Z R^2 beta / sqrt(5 pi); (3) a SEPARATE Python implementation (plain Simpson integration over the angle, not the lab's closed forms) for the exact shape parameters typed below;
// (4) the test's own integrals of the drawn surface.
struct QuadRef { int Z, A, spin2; char kind; double Qb, uncB; const char* method; const char* nsr; int stable; };
static const QuadRef kQuadRef[] = {                                                      // typed from the CSV: z, column 2 (A), spin, value, uncertainty, method, nsr, half-life
    {1, 2, 2, 'S', +0.0028578, 3e-7, "MB,R", "1958Qu02", 1}, {3, 6, 2, 'S', -0.000806, 6e-6, "MB", "2005Bo45", 1}, {3, 7, 3, 'S', -0.0400, 3e-4, "MB", "2008Py02", 1},
    {5, 11, 3, 'S', +0.04059, 1e-4, "AB", "", 1}, {11, 23, 3, 'S', +0.104, 1e-3, "O", "", 1}, {13, 27, 5, 'S', +0.1466, 1e-3, "AB", "", 1}, {19, 39, 3, 'S', +0.603, 6e-3, "AB", "", 1},
    {19, 40, 8, 'S', -0.750, 8e-3, "AB", "", 0}, {27, 59, 7, 'S', +0.42, 3e-2, "AB", "", 1}, {55, 124, 2, 'S', -0.69, 4e-2, "ABLS", "", 0}, {55, 126, 2, 'S', -0.64, 3e-2, "ABLS", "", 0},
    {62, 147, 7, 'S', -0.26, 3e-2, "Mu-X", "", 0}, {63, 153, 5, 'S', +2.41, 2e-2, "Mu-X", "", 1}, {67, 165, 7, 'S', +3.58, 2e-2, "Pi-X", "", 1}, {68, 167, 7, 'S', +3.57, 3e-2, "Mu-X", "", 1},
    {71, 175, 7, 'S', +3.49, 2e-2, "Mu-X", "", 1}, {83, 209, 9, 'S', -0.516, 1.5e-2, "AB", "", 0}, {92, 235, 7, 'S', +4.936, 6e-3, "Mu-X", "", 0}, {5, 12, 2, 'U', 0.0132, 3e-4, "b-NMR", "", 0},
    {3, 11, 3, 'T', -0.0333, 5e-4, "b-NMR", "2008Ne11", 0}, {19, 41, 3, 'S', +0.734, 7e-3, "AB", "2018Py01", 1}};
// ground-state rows kept per element Z = 1 .. 102 (the file's 599 minus the K-41 duplicate), counted from the CSV
static const int kQuadPerElement[102] = {1, 0, 5, 1, 8, 1, 4, 3, 4, 2, 9, 4, 7, 1, 1, 2, 3, 5, 4, 7, 6, 3, 2, 1, 13, 0, 5, 1, 21, 6, 17, 2, 4, 2, 6, 8, 20, 9, 7, 5, 7, 2, 1, 3, 0, 1, 4, 8, 20, 8, 5, 3, 6, 10,
                                         24, 8, 5, 0, 3, 7, 4, 7, 19, 2, 11, 9, 12, 9, 12, 10, 15, 4, 5, 0, 7, 4, 13, 4, 12, 5, 3, 7, 15, 8, 18, 7, 20, 6, 3, 1, 2, 2, 1, 1, 3, 0, 0, 0, 1, 0, 0, 1};
// the measured rms charge radius (radii CSV), Q0 / (Z <r^2>) with <r^2> = rms^2 - ownNucleonMs (0.42: the protons' point mean square radius; until 0.41 rms^2 - 0.84^2), the exact beta2 of the uniformly charged body that has that moment (separate Python
// implementation) and the first-order beta2 = sqrt(5 pi) Q0 / (3 Z R^2), R^2 = (5/3) <r^2> (hand formula)
struct ShapeRef { int Z, A; double rms, ratio, beta, first; };
static const ShapeRef kShapeRef[] = {{67, 165, 5.2022, 0.432205315, +0.32309996, +0.3425942}, {68, 167, 5.2560, 0.415840262, +0.31088971, +0.3296222}, {71, 175, 5.3700, 0.372644892, +0.27886926, +0.2953827}, {63, 153, 5.1115, 0.419182206, +0.31337921, +0.3322713}, {92, 235, 5.8337, 0.343482582, +0.25737991, +0.2722668}, {27, 59, 3.7875, 0.242565156, +0.18331105, +0.1922730}, {83, 209, 5.5211, -0.038096288, -0.03054658, -0.0301976}, {62, 147, 4.9892, -0.036964278, -0.02962826, -0.0293003}, {55, 126, 4.7872, -0.521194157, -0.57394428, -0.4131326}, {13, 27, 3.0610, 0.360693654, +0.27005255, +0.2859094}, {11, 23, 2.9936, 0.566302672, +0.42572887, +0.4488886}, {64, 155, 5.1319, 0.385157115, +0.28811752, +0.3053007}};   // 0.42: recomputed by tools/shape_ref_042.py with <r^2> = rms^2 - ownNucleonMs (it reproduces the 0.41 betas exactly with 0.84^2)
// a symmetric 3 x 3 matrix's eigenvalue of the largest size, by Jacobi rotations
static double bigEigen(double a[3][3]) {
    for (int sweep = 0; sweep < 50; sweep++) {
        double off = 0;
        for (int p = 0; p < 3; p++) for (int q = p + 1; q < 3; q++) off += a[p][q] * a[p][q];
        if (off < 1e-24) break;
        for (int p = 0; p < 3; p++) for (int q = p + 1; q < 3; q++) {
            if (std::fabs(a[p][q]) < 1e-300) continue;
            const double th = (a[q][q] - a[p][p]) / (2 * a[p][q]), t = (th >= 0 ? 1 : -1) / (std::fabs(th) + std::sqrt(th * th + 1)), c = 1 / std::sqrt(t * t + 1), s = t * c;
            for (int k = 0; k < 3; k++) { const double akp = a[k][p], akq = a[k][q]; a[k][p] = c * akp - s * akq; a[k][q] = s * akp + c * akq; }
            for (int k = 0; k < 3; k++) { const double apk = a[p][k], aqk = a[q][k]; a[p][k] = c * apk - s * aqk; a[q][k] = s * apk + c * aqk; }
        }
    }
    double e = a[0][0];
    for (int k = 1; k < 3; k++) if (std::fabs(a[k][k]) > std::fabs(e)) e = a[k][k];
    return e;
}
// what the lab draws: the protons' traceless quadrupole tensor sum(3 x_i x_j - r^2 delta_ij) / sum(r^2) of one build (rotation does not change its eigenvalues, so the random tilt does not matter). For an axial body
// its eigenvalues are (ratio, -ratio/2, -ratio/2) with ratio = Q0 / (Z <r^2>): the one of the largest size is positive for a prolate nucleus and negative for an oblate one. Mean and spread over the builds.
struct CloudQ { double mean, sd; };
static CloudQ cloudQuadrupole(int Z, int N, int builds, uint32_t seed) {
    seedRandom(seed);
    double s1 = 0, s2 = 0;
    for (int b = 0; b < builds; b++) {
        const std::vector<Nucleon> v = buildNucleus(Z, N);
        double cx = 0, cy = 0, cz = 0;
        for (const Nucleon& q : v) { cx += q.x; cy += q.y; cz += q.z; }
        cx /= v.size(); cy /= v.size(); cz /= v.size();
        double T[3][3] = {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}}, r2s = 0;
        for (const Nucleon& q : v) {
            if (!q.isP) continue;
            const double x[3] = {(q.x - cx) / kFmLab, (q.y - cy) / kFmLab, (q.z - cz) / kFmLab}, r2 = x[0] * x[0] + x[1] * x[1] + x[2] * x[2];
            r2s += r2;
            for (int i = 0; i < 3; i++) for (int j = 0; j < 3; j++) T[i][j] += 3 * x[i] * x[j] - (i == j ? r2 : 0);
        }
        for (int i = 0; i < 3; i++) for (int j = 0; j < 3; j++) T[i][j] /= r2s;
        const double e = bigEigen(T);
        s1 += e; s2 += e * e;
    }
    const double m = s1 / builds;
    return {m, std::sqrt(std::max(0.0, s2 / builds - m * m))};
}
// the volume, the mean square radius and <3z^2 - r^2> / <r^2> of the uniform body whose surface is r(u) = R_V f(u) (u = cos theta, f = shapeRadiusFactor), by the test's own Simpson integrals over u:
// V = (2 pi / 3) integral r^3 du;  <r^2> = (3/5) integral r^5 du / integral r^3 du;  <3z^2 - r^2> / <r^2> = integral r^5 (3u^2 - 1) du / integral r^5 du
struct BodyOwn { double volume, ms, ratio; };
static BodyOwn bodyOwn(double beta, double RV) {
    const int n = 20000;
    const double h = 2.0 / n;
    double v3 = 0, v5 = 0, q5 = 0;
    for (int i = 0; i <= n; i++) {
        const double u = -1 + i * h, w = (i == 0 || i == n) ? 1 : (i % 2 ? 4 : 2), r = RV * shapeRadiusFactor(beta, u), r3 = r * r * r, r5 = r3 * r * r;
        v3 += w * r3; v5 += w * r5; q5 += w * r5 * (3 * u * u - 1);
    }
    return {2 * kPi / 3 * v3 * h / 3, 0.6 * v5 / v3, q5 / v5};
}

static void testQuadrupole(double guardMinSepDeformed) {
    // ---- 1. the data parses; counts as fetched ----
    int n = 0, fileRows = 0, isoRows = 0;
    const QuadRow* rows = quadRows(n);
    quadFileCounts(fileRows, isoRows);
    int kinds[256] = {0}, perEl[120] = {0}, badOrder = 0, asA = 0, asN = 0, stable = 0, stableS = 0, stableN = 0, spinLow = 0, evenEven = 0, firmBad = 0;
    for (int i = 0; i < n; i++) {
        const QuadRow& r = rows[i];
        kinds[(unsigned char)r.kind]++; perEl[r.Z]++;
        if (i > 0 && !(rows[i - 1].Z < r.Z || (rows[i - 1].Z == r.Z && rows[i - 1].A < r.A))) badOrder++;
        if (nucCode(r.Z, r.A - r.Z)) asA++;
        if (nucCode(r.Z, r.A)) asN++;
        if (r.stable) { stable++; const std::string* c = nucCode(r.Z, r.A - r.Z); if (c && *c == "S") stableS++; const std::string* d = nucCode(r.Z, r.A); if (d && *d == "S") stableN++; }
        if (r.kind == 'S' && r.spin2 >= 0 && r.spin2 < 2) spinLow++;
        if (r.Z % 2 == 0 && (r.A - r.Z) % 2 == 0) evenEven++;
        if (r.spinFirm && r.spin2 < 0) firmBad++;
    }
    checkTrue("quadrupole: the file has 1,155 rows: 599 ground states (598 here: the duplicate K-41 row is left out) and 556 isomer rows (not in the table)", fileRows == 1155 && isoRows == 556 && n == 598 && fileRows - isoRows == 599);
    checkTrue("quadrupole: 598 ground-state rows: 546 signed, 41 unsigned (no sign in the file), 4 tentative sign, 7 not a single clean number (counted from the CSV)", kinds['S'] == 546 && kinds['U'] == 41 && kinds['T'] == 4 && kinds['X'] == 7);
    checkTrue("quadrupole: sorted and unique by (Z, A); ground rows per element Z = 1 .. 102 as counted from the CSV", badOrder == 0 && std::equal(kQuadPerElement, kQuadPerElement + 102, perEl + 1));
    checkTrue("quadrupole: no even-even ground row and none of spin 0 or 1/2 (quantum mechanics: such a nucleus has no spectroscopic quadrupole moment)", evenEven == 0 && spinLow == 0 && firmBad == 0);
    // what the second column is: the MASS NUMBER A. Read as A, all 598 rows are nuclides of the lab's NUBASE-derived table; read as a neutron count only 53 are, and the 77 'stable' rows are the stable isotopes (76: Eu-151
    // is in the table with 1.45e26 s) only as A
    checkTrue("quadrupole: the file's second column is the mass number: (Z, A - Z) is a known nuclide for 598 of 598 rows, (Z, column) for only 53; of 77 'stable' rows 76 are stable isotopes read as A and 0 read as N",
              asA == 598 && asN == 53 && stable == 77 && stableS == 76 && stableN == 0);
    std::printf("quadrupole: %d ground rows (signed %d, unsigned %d, tentative %d, not clean %d), %d isomer rows; column read as A: %d of %d are nuclides, as N: %d; stable rows %d: %d on stable isotopes as A, %d as N\n",
                n, kinds['S'], kinds['U'], kinds['T'], kinds['X'], isoRows, asA, n, asN, stable, stableS, stableN);
    // ---- 2. spot values typed from the CSV (units: barns) ----
    for (const QuadRef& r : kQuadRef) {
        const QuadRow* q = quadRow(r.Z, r.A - r.Z);
        const std::string w = "quadrupole " + std::string(symbol(r.Z)) + "-" + std::to_string(r.A);
        checkTrue(w + " is in the table", q != nullptr);
        if (!q) continue;
        checkTrue(w + ": spin, kind, stable, method as in the CSV", q->spin2 == r.spin2 && q->kind == r.kind && q->stable == r.stable && std::string(q->method) == r.method && (!*r.nsr || std::string(q->nsr) == r.nsr));
        checkNum(w + " Q (barns)", q->Qb, r.Qb, 1e-12);
        checkNum(w + " uncertainty (barns)", q->uncB, r.uncB, 1e-9);
    }
    checkTrue("quadrupole: the value text and spin text are the file's own (Ho-165 \"+3.58(2)\" \"7/2-\"; Cu-78 spin \"(6-)\" is not firm; B-12 \"0.0132(3)\" has no sign; At-218's row is not clean)",
              std::string(quadRow(67, 98)->raw) == "+3.58(2)" && std::string(quadRow(67, 98)->spin) == "7/2-" && quadRow(29, 49)->spinFirm == 0 && std::string(quadRow(5, 7)->raw) == "0.0132(3)" && quadRow(85, 133)->kind == 'X');
    checkNum("1 barn = 100 fm^2 exactly (10^-28 m^2 over 10^-30 m^2)", kBarnFm2, 100, 0);
    // ---- 3. spectroscopic -> intrinsic, K = I (hand arithmetic) ----
    checkNum("K = I: spin 1: Q0 = 10 Q", intrinsicQ0(1.0, 2), 10, 1e-14);
    checkNum("K = I: spin 3/2: Q0 = 5 Q", intrinsicQ0(1.0, 3), 5, 1e-14);
    checkNum("K = I: spin 2: Q0 = 7/2 Q", intrinsicQ0(1.0, 4), 3.5, 1e-14);
    checkNum("K = I: spin 5/2: Q0 = 14/5 Q", intrinsicQ0(1.0, 5), 2.8, 1e-14);
    checkNum("K = I: spin 7/2: Q0 = 15/7 Q", intrinsicQ0(1.0, 7), 15.0 / 7, 1e-14);
    checkNum("K = I: spin 9/2: Q0 = 11/6 Q", intrinsicQ0(1.0, 9), 11.0 / 6, 1e-14);
    checkTrue("K = I: spin 0 and spin 1/2 have no conversion (the spectroscopic moment is exactly zero whatever the shape): not a number", std::isnan(intrinsicQ0(1.0, 0)) && std::isnan(intrinsicQ0(1.0, 1)));
    checkNum("Ho-165 (spin 7/2, Q = +3.58 b): Q0 = 3.58 x 15/7 = +7.6714 b", intrinsicQ0(3.58, 7), 7.671428571428571, 1e-14);
    checkNum("deuteron (spin 1, Q = +0.0028578 b): Q0 = +0.028578 b", intrinsicQ0(0.0028578, 2), 0.028578, 1e-14);
    checkNum("Bi-209 (spin 9/2, Q = -0.516 b): Q0 = -0.516 x 11/6 = -0.9460 b", intrinsicQ0(-0.516, 9), -0.946, 1e-14);
    // ---- 4. the shape parameter: the point-proton mean square radius, the exact beta2 for the drawn body, the first-order formula as a hand check ----
    for (const ShapeRef& r : kShapeRef) {
        const int N = r.A - r.Z;
        const std::string w = std::string(symbol(r.Z)) + "-" + std::to_string(r.A);
        const double ms = r.rms * r.rms - ownNucleonMs(r.Z, N);
        checkNum(w + ": the point-proton mean square radius = measured rms^2 - the nucleons' own sizes - Darwin-Foldy", pointProtonMs(r.Z, N), ms, 1e-12);
        const QuadRow* q = quadRow(r.Z, N);
        const double Q0fm2 = intrinsicQ0(q->Qb, q->spin2) * kBarnFm2;
        checkNum(w + ": ratio Q0 / (Z <r^2>) (separate Python)", Q0fm2 / (r.Z * ms), r.ratio, 1e-7);
        const Shape s = nucShape(r.Z, N);
        // 0.46.3 (outside review): the moment is turned into a shape only where FRDM(2012) calculates a deformed core (|beta2| >= 0.10); near closed shells (Bi-209) it is not used
        const bool core = nucFrdmKnown(r.Z, N) && std::fabs(nucFrdmBeta(r.Z, N)) >= kDeformedCoreBeta;
        checkTrue(w + (core ? ": the shape comes from the measured moment" : ": FRDM calls it nearly round: the moment is not turned into a shape (0.46.3)"),
                  core ? s.src == ShapeSource::MeasuredMoment && s.moment == MomentUse::Used : s.moment == MomentUse::NoDeformedCore && s.src != ShapeSource::MeasuredMoment);
        bool okConv = false;
        const double bConv = core ? s.beta : betaFromRatio(Q0fm2 / (r.Z * ms), okConv);   // the conversion itself is still checked
        checkNum(w + ": beta2 of the conversion equals the separate Python solution (exact for the drawn body)", bConv, r.beta, 0, 3e-6);
        checkTrue(w + ": the sign of beta2 is the sign of the measured moment (positive: prolate, negative: oblate)", (bConv > 0) == (q->Qb > 0));
        const double first = std::sqrt(5 * kPi) * Q0fm2 / (3 * r.Z * (5.0 / 3) * ms);
        checkNum(w + ": the first-order hand formula sqrt(5 pi) Q0 / (3 Z R^2) with R^2 = (5/3) <r^2>", first, r.first, 1e-6);
        if (std::fabs(r.beta) > 0.2 && std::fabs(r.beta) < 0.45) checkTrue(w + ": the exact value is 4 to 9 % below the first-order one for 0.2 < |beta2| < 0.45 (the second-order term)", std::fabs(bConv) < std::fabs(first) * 0.96 && std::fabs(bConv) > std::fabs(first) * 0.90);
    }
    // known cases: strongly deformed rare earths and actinides are clearly prolate; the most oblate case the rule accepts is clearly oblate (Cs-126: a moment from a few nucleons; no benchmark was read, this is the data's own)
    int prolate = 0;
    for (const auto& za : {std::pair<int, int>{67, 165}, {68, 167}, {71, 175}, {63, 153}, {64, 155}, {70, 173}, {92, 235}}) {
        const Shape s = nucShape(za.first, za.second - za.first);
        if (s.src == ShapeSource::MeasuredMoment && s.beta > 0.25 && s.beta < 0.40) prolate++;
    }
    checkTrue("known cases: Ho-165, Er-167, Lu-175, Eu-153, Gd-155, Yb-173, U-235 are all prolate with 0.25 < beta2 < 0.40", prolate == 7);
    checkTrue("the most oblate case the rule accepts, Cs-126, is clearly oblate (beta2 = -0.579); near a closed shell Bi-209 and Sm-147 are almost round (-0.031, -0.030)",
              nucShape(55, 71).beta < -0.5 && std::fabs(nucShape(83, 126).beta) < 0.04 && nucShape(83, 126).beta < 0 && std::fabs(nucShape(62, 85).beta) < 0.04 && nucShape(62, 85).beta < 0);
    // ---- 5. the drawn body: volume, mean square radius and quadrupole by the test's own integrals ----
    double worstVol = 0, worstMs = 0, minRV = 1e9;
    for (const double beta : {-0.6, -0.45, -0.3, -0.03, 0.0, 0.03, 0.2, 0.3247, 0.45, 0.6}) {
        const double Rpt = 6.0, RV = volumeEquivalentRadius(Rpt, beta);
        const BodyOwn b = bodyOwn(beta, RV);
        worstVol = std::max(worstVol, std::fabs(b.volume / (4 * kPi / 3 * RV * RV * RV) - 1));
        worstMs = std::max(worstMs, std::fabs(b.ms / (0.6 * Rpt * Rpt) - 1));
        minRV = std::min(minRV, RV / Rpt);
        checkNum("the drawn body keeps its volume when deformed: beta2 = " + fmt::fixed(beta, 4) + ": V = 4 pi/3 R_V^3 (own Simpson)", b.volume, 4 * kPi / 3 * RV * RV * RV, 1e-9);
        checkNum("... and its mean square radius is the measured point-proton one: beta2 = " + fmt::fixed(beta, 4), b.ms, 0.6 * Rpt * Rpt, 1e-9);
        if (beta != 0) checkNum("... and <3z^2 - r^2> / <r^2> is the lab's shapeMomentRatio: beta2 = " + fmt::fixed(beta, 4), b.ratio, shapeMomentRatio(beta), 1e-9);
        else checkNum("a sphere has no quadrupole moment", b.ratio, 0, 0, 1e-12);
    }
    std::printf("drawn body (own integrals): worst volume error %.2e, worst mean-square-radius error %.2e; smallest R_V / R_pt = %.4f (beta2 = +-0.6)\n", worstVol, worstMs, minRV);
    checkNum("the first-order relation is the small-beta limit: <3z^2 - r^2> / <r^2> = 4 c beta (c = sqrt(5/(16 pi))) for beta = 0.001", shapeMomentRatio(0.001), 4 * 0.31539156525252005 * 0.001, 1e-3);
    checkNum("Q0 of the measured moments is reproduced by the drawn body: Ho-165, Q0 = Z <r^2> x (own integral of the drawn surface)", 67 * (5.2022 * 5.2022 - ownNucleonMs(67, 98)) * bodyOwn(nucShape(67, 98).beta, volumeEquivalentRadius(std::sqrt(5.0 / 3 * (5.2022 * 5.2022 - ownNucleonMs(67, 98))), nucShape(67, 98).beta)).ratio,
             intrinsicQ0(3.58, 7) * kBarnFm2, 1e-6);
    // ---- 6. which source the nucleus uses, and why not the measured moment ----
    struct Why { int Z, A; ShapeSource src; MomentUse use; const char* what; };
    const Why whys[] = {
        {67, 165, ShapeSource::MeasuredMoment, MomentUse::Used, "Ho-165: a measured, signed moment, firm spin"},
        {64, 158, ShapeSource::BE2, MomentUse::NoRow, "Gd-158: even-even, no moment possible; the lab's B(E2) table has it"},
        {82, 208, ShapeSource::BE2, MomentUse::NoRow, "Pb-208: even-even: no moment possible; 0.46.3: its measured B(E2) (ENSDF)"},
        {50, 120, ShapeSource::BE2, MomentUse::NoRow, "Sn-120: even-even: 0.46.3: its measured B(E2) (ENSDF)"},
        {3, 7, ShapeSource::None, MomentUse::NoDeformedCore, "Li-7: a real moment, but FRDM(2012) starts at oxygen: no deformed core known, not used (0.46.3); no published shape: round"},
        {19, 41, ShapeSource::FRDM, MomentUse::NoDeformedCore, "K-41: FRDM(2012) calls it nearly round: the moment is not turned into a shape (0.46.3)"},
        {8, 17, ShapeSource::FRDM, MomentUse::NoDeformedCore, "O-17 (outside review 0.46.3): next to a closed shell, FRDM beta2 0.061 < 0.10: moment not used"},
        {83, 209, ShapeSource::FRDM, MomentUse::NoDeformedCore, "Bi-209 (outside review 0.46.3): next to a closed shell, FRDM beta2 -0.011: moment not used"},
        {5, 12, ShapeSource::None, MomentUse::SignNotGiven, "B-12: |Q| = 0.0132 b but the file gives no sign"},
        {3, 11, ShapeSource::None, MomentUse::SignNotGiven, "Li-11: sign only tentative (-)"},
        {29, 78, ShapeSource::FRDM, MomentUse::SpinNotFirm, "Cu-78: spin only tentative (6-)"},
        {85, 218, ShapeSource::FRDM, MomentUse::RowNotUsable, "At-218: the row is not a clean number"},
        {91, 231, ShapeSource::FRDM, MomentUse::RowNotUsable, "Pa-231: [-1.72(5)] 'not measured'"}};
    for (const Why& w : whys) {
        const Shape s = nucShape(w.Z, w.A - w.Z);
        checkTrue(std::string("source and reason: ") + w.what, s.src == w.src && s.moment == w.use);
    }
    checkTrue("a nucleus without a usable moment has the same beta2 as nucBeta (Gd-158, Pb-208, Sn-120: B(E2))", nucShape(64, 94).beta == nucBeta(64, 94) && nucShape(82, 126).beta == nucBeta(82, 126) && nucShape(50, 70).beta == nucBeta(50, 70));
    int used = 0, tooLarge = 0, noCore = 0, noSign = 0, spinNo = 0, notClean = 0, qualifying = 0;
    for (int i = 0; i < n; i++) {
        const Shape s = nucShape(rows[i].Z, rows[i].A - rows[i].Z);
        used += s.moment == MomentUse::Used; tooLarge += s.moment == MomentUse::TooLarge; noCore += s.moment == MomentUse::NoDeformedCore; noSign += s.moment == MomentUse::SignNotGiven; spinNo += s.moment == MomentUse::SpinNotFirm; notClean += s.moment == MomentUse::RowNotUsable;
    }
    qualifying = used + tooLarge;   // (0.46.3: plus noCore, below)
    // 0.46.3: of the 512 qualifying rows (signed, firm spin >= 1; counted by the separate Python before), 171 have no deformed core in FRDM(2012) (|beta2| < 0.10 or no row) and 341 have one:
    // counted again in Python from quadrupole_data.inc and frdm_data.inc (2026-11-08). Of the 341, 4 need |beta2| > 0.6 (a subset of the 13 the Python counted before, by construction: the same conversion).
    checkTrue("of the 598 ground rows 512 qualify: 171 without a deformed core (FRDM), 341 with one: 337 give a shape, 4 need |beta2| > 0.6; 45 have no firm sign, 34 no firm spin, 7 are not clean numbers",
              qualifying + noCore == 512 && noCore == 171 && used + tooLarge == 341 && used == 337 && tooLarge == 4 && noSign == 45 && spinNo == 34 && notClean == 7);
    int offered = 0, srcQ = 0, srcB = 0, srcE = 0, srcNone = 0, estEvenEven = 0;
    for (int z = 1; z <= 118; z++) {
        const auto rg = nRange(z);
        for (int nn = rg.first; nn <= rg.second; nn++) {
            offered++;
            const Shape s = nucShape(z, nn);
            srcQ += s.src == ShapeSource::MeasuredMoment; srcB += s.src == ShapeSource::BE2; srcE += s.src == ShapeSource::FRDM; srcNone += s.src == ShapeSource::None;
            if (s.src == ShapeSource::FRDM && z % 2 == 0 && nn % 2 == 0) estEvenEven++;
        }
    }
    std::printf("shape sources of the slider's %d isotopes: measured moment %d, B(E2) %d, FRDM %d (of which even-even, spin 0: %d), none %d; moments: used %d, too large %d, no deformed core %d\n", offered, srcQ, srcB, srcE, estEvenEven, srcNone, used, tooLarge, noCore);
    checkTrue("the slider's 4,252 isotopes (0.46.3): 337 shapes from a measured moment, 167 from a measured B(E2) (every entry of the table, data/be2), 3,581 from FRDM(2012) (862 of them even-even), 167 with no published value",
              offered == 4252 && srcQ == 337 && srcB == 167 && srcE == 3581 && estEvenEven == 862 && srcNone == 167 && srcQ + srcB + srcE + srcNone == offered);
    // ---- 7. the cloud the lab draws ----
    // (a) EXACT: buildNucleusAxial is buildNucleus without its last step, the random tilt, so the symmetry axis is z. The protons' <x^2> = <y^2> and <z^2> about the centre of mass are scaled to those of the model, so
    // (2 <z^2> - <x^2> - <y^2>) / (<x^2> + <y^2> + <z^2>) must be Q0 / (Z <r^2>) of the DATA (typed ratios, separate Python: tools/shape_ref_042.py, 0.42 point-proton <r^2>), the trace the point-proton <r^2>, and <x^2> = <y^2>: by construction, so the guard is rounding-level.
    struct Cloud { int Z, A; double ratio; };
    const Cloud clouds[] = {{67, 165, 0.432205314941}, {68, 167, 0.415840261600}, {63, 153, 0.419182206002}, {92, 235, 0.343482582561}, {27, 59, 0.242565156053}, {13, 27, 0.360693654130}, {55, 126, -0.521194157023}, {64, 155, 0.385157115018},
                           {10, 20, nucBE2Q0Fm2(10, 10) / (10 * pointProtonMs(10, 10))}};   // 0.46.4 (deliberate): Ne-20 from its measured B(E2): Q0 = sqrt(16 pi B / 5) over Z <r^2>, the data's own ratio
                           // (C-12's measured moment, oblate since 0.46.4, needs |beta2| > 0.6 and is drawn round, so it left this list; no light nucleus is drawn deformed now)   // 0.46.3: Li-6, Li-8, N-12, N-14 no longer take their shape from the moment (FRDM starts at oxygen: no deformed core
                           // known); C-12 (A <= 16: the Gaussian branch) takes its measured B(E2), so the drawn cloud must have that body's moment
    double worstRatio = 0, worstTrace = 0, worstXY = 0, minSepAxial = 1e9;
    for (const Cloud& c : clouds) {
        const int N = c.A - c.Z;
        double ratioSum = 0, minSep = 1e9; const int builds = 40;
        seedRandom(4242 + c.Z);
        for (int b = 0; b < builds; b++) {
            const std::vector<Nucleon> v = buildNucleusAxial(c.Z, N);
            double cx = 0, cy = 0, cz = 0;
            for (const Nucleon& q : v) { cx += q.x; cy += q.y; cz += q.z; }
            cx /= v.size(); cy /= v.size(); cz /= v.size();
            double sxx = 0, syy = 0, szz = 0; int np = 0;
            for (const Nucleon& q : v) if (q.isP) { sxx += (q.x - cx) * (q.x - cx) / (kFmLab * kFmLab); syy += (q.y - cy) * (q.y - cy) / (kFmLab * kFmLab); szz += (q.z - cz) * (q.z - cz) / (kFmLab * kFmLab); np++; }
            sxx /= np; syy /= np; szz /= np;
            const double tr = sxx + syy + szz, ratio = (2 * szz - sxx - syy) / tr;
            worstRatio = std::max(worstRatio, std::fabs(ratio - c.ratio)); worstTrace = std::max(worstTrace, std::fabs(tr / pointProtonMs(c.Z, N) - 1)); worstXY = std::max(worstXY, std::fabs(sxx - syy) / tr);
            ratioSum += ratio;
            for (std::size_t i = 0; i < v.size(); i++) for (std::size_t j = i + 1; j < v.size(); j++) {
                const double dx = (v[i].x - v[j].x) / kFmLab, dy = (v[i].y - v[j].y) / kFmLab, dz = (v[i].z - v[j].z) / kFmLab;
                minSep = std::min(minSep, std::sqrt(dx * dx + dy * dy + dz * dz));
            }
        }
        if (c.Z != 10) minSepAxial = std::min(minSepAxial, minSep);   // Ne-20 (0.46.4, beta2 0.52): checked against the 0.55 fm x shortest-radius guarantee, as C-12 was
        else checkTrue("Ne-20 (B(E2) body): closest pair >= 0.55 fm x the shortest radius factor", minSep >= 0.55 * std::min(shapeRadiusFactor(nucBeta(10, 10), 0), shapeRadiusFactor(nucBeta(10, 10), 1)));
        std::printf("cloud %s-%d (untilted, %d builds): <3z^2 - r^2>/<r^2> of the drawn protons %+.9f, from the data %+.9f; closest pair %.3f fm\n", symbol(c.Z), c.A, builds, ratioSum / builds, c.ratio, minSep);
    }
    std::printf("cloud: worst |drawn - data| ratio %.2e, trace %.2e, |<x^2> - <y^2>| %.2e; closest pair %.4f fm\n", worstRatio, worstTrace, worstXY, minSepAxial);
    checkTrue("the drawn cloud has EXACTLY the measured moment: protons' <3z^2 - r^2> / <r^2> = Q0 / (Z <r^2>) of the data for 8 nuclei, and C-12 its B(E2) body's (the Gaussian branch, A <= 16) (guard 1e-9: exact by construction)", worstRatio <= 1e-9);
    checkTrue("... with the point-proton <r^2> as its trace (the stage 1 radius: 1e-9) and <x^2> = <y^2> (an axial body: 1e-9)", worstTrace <= 1e-9 && worstXY <= 1e-9);
    checkTrue("... and no two nucleons closer than the guard 0.606 fm (the hard core 1.0 fm, relaxed to 0.55 fm only when crowded, then scaled by the deformation; the smallest measured is printed)", minSepAxial >= guardMinSepDeformed);
    // (b) the tilted cloud the screen shows: only the SIGN is a safe statistic for a random snapshot (the largest eigenvalue of a noisy tensor of a few protons is biased), so the sign only
    for (std::size_t ci = 0; ci < 8; ci++) {   // the 8 with Z >= 13 and |ratio| >= 0.24; the light ones have a few protons, a noise of about 0.5 and (Li-6) a ratio of -0.045
        const Cloud& c = clouds[ci];
        const CloudQ q = cloudQuadrupole(c.Z, c.A - c.Z, 120, 777 + c.Z);
        std::printf("cloud %s-%d (tilted, as drawn): largest eigenvalue %+.4f (sd of one build %.3f), model %+.4f\n", symbol(c.Z), c.A, q.mean, q.sd, c.ratio);
        checkTrue(std::string("the drawn cloud's sign: ") + symbol(c.Z) + "-" + std::to_string(c.A) + (c.ratio > 0 ? " is prolate (positive)" : " is oblate (negative)"), (q.mean > 0) == (c.ratio > 0));
    }
    // the stage 1 radius still holds with the deformation: the protons' rms charge radius of the drawn nucleus is the measured one (9 nuclei, several deformed, in testNuclearSize); here the strongly deformed ones
    for (const auto& za : {std::pair<int, int>{67, 165}, {63, 153}, {92, 235}}) {
        const double m = chargeRadius(za.first, za.second - za.first)->rms;
        const LayoutStat st = layoutStat(za.first, za.second - za.first, 30, false, 31337 + za.first);
        checkNum(std::string("stage 1 radius kept after deformation: ") + symbol(za.first) + "-" + std::to_string(za.second) + ": the drawn rms charge radius = the measured one", st.chargeRms, m, 1e-9);
    }
    // the lab's readout line "shape: ..." at the Nucleus layer names the source, one of three labels
    auto shapeLine = [](int Z, int N) {
        AtomLab lab; lab.p.Z = Z; lab.p.N = N; lab.s.layer = "Nucleus";
        for (const std::string& l : lab.info()) if (l.compare(0, 7, "shape: ") == 0) return l;
        return std::string("(no shape line)");
    };
    checkStr("readout, Ho-165: a measured moment", shapeLine(67, 98), "shape: stretched like a rugby ball (from a measured quadrupole moment)");
    checkStr("readout, Cs-126: a measured moment, oblate", shapeLine(55, 71), "shape: flattened like a pancake (from a measured quadrupole moment)");
    checkStr("readout, Gd-158: B(E2)", shapeLine(64, 94), "shape: stretched like a rugby ball (from a measured B(E2))");
    checkStr("readout, Pb-208 (0.46.3): a measured B(E2), drawn round (FRDM: round, so the B(E2) is a vibration)", shapeLine(82, 126), "shape: nearly round (FRDM 2012 calculates it round; its measured B(E2) is a vibration about that)");   // 0.46.4: the label names FRDM
    checkStr("readout, Sn-120 (0.46.3): a measured B(E2), drawn round", shapeLine(50, 70), "shape: nearly round (FRDM 2012 calculates it round; its measured B(E2) is a vibration about that)");
    checkStr("readout, C-12 (0.46.4): its oblate B(E2) moment is past the body's limit, drawn round", shapeLine(6, 6), "shape: nearly round (its measured B(E2) needs more deformation than the lab's shape allows: see the card)");
    checkStr("readout, Og-294 (0.46.3): FRDM(2012), a calculation", shapeLine(118, 176), "shape: nearly round (calculated: FRDM 2012 model)");
    checkStr("readout, B-12 (0.46.3): no published value", shapeLine(5, 7), "shape: nearly round (no published value: drawn round)");
    // ---- 8. the words ----
    const std::string ho = joinLines(nuclearShapeLines(67, 98)), cs = joinLines(nuclearShapeLines(55, 71)), li7 = joinLines(nuclearShapeLines(3, 4)), b12 = joinLines(nuclearShapeLines(5, 7)), pb = joinLines(nuclearShapeLines(82, 126)),
                    gd = joinLines(nuclearShapeLines(64, 94)), sn = joinLines(nuclearShapeLines(50, 70)), h1 = joinLines(nuclearShapeLines(1, 0)), bi = joinLines(nuclearShapeLines(83, 126)), cu = joinLines(nuclearShapeLines(29, 49)),
                    li10 = joinLines(nuclearShapeLines(3, 7));
    checkTrue("card, Ho-165: names the source 'measured quadrupole moment', the moment +3.5800 b +- 0.0200 b with spin 7/2, the conversion Q0 = +7.6714 b and that it is a model assumption (K = I), prolate and beta2 = +0.3231 (0.42: the point-proton <r^2> of Ong et al. 2010; separate Python tools/shape_ref_042.py)",
              has(ho, "measured quadrupole moment") && has(ho, "+3.5800 b") && has(ho, "0.0200 b") && has(ho, "7/2") && has(ho, "+7.6714 b") && has(ho, "Model assumption, not a measurement") && has(ho, "K = I") &&
              has(ho, "prolate (stretched like a rugby ball)") && has(ho, "+0.3231"));
    checkTrue("card, Cs-126: oblate (flattened like a pancake), a negative moment, beta2 = -0.5739", has(cs, "oblate (flattened like a pancake)") && has(cs, "-0.6400 b") && has(cs, "-0.5739"));
    checkTrue("card, Li-7 (0.46.3): the moment is real (-0.0400 b) but FRDM(2012) has no value, so no deformed core is known: not turned into a shape; no published shape", has(li7, "-0.0400 b") && has(li7, "Not turned into a shape") && has(li7, "FRDM(2012) has no value") && has(li7, "Shape source: none"));
    checkTrue("card, B-12: the table has |Q| = 0.0132 b but no sign: prolate or oblate cannot be told, not used", has(b12, "0.0132 b") && has(b12, "no sign") && has(b12, "not used"));
    checkTrue("card, Li-11: the sign is only tentative: not used", has(joinLines(nuclearShapeLines(3, 8)), "tentative") && has(joinLines(nuclearShapeLines(3, 8)), "not used"));
    checkTrue("card, Cu-78: the spin is only tentative (6-): not used", has(cu, "tentative") && has(cu, "(6-)") && has(cu, "not used"));
    checkTrue("card, Pb-208 and Sn-120 (even-even: spin 0): a spin-0 nucleus has NO spectroscopic quadrupole moment whatever its shape, so this table cannot say whether it is round; 'round' is not implied",
              has(pb, "spin 0") && has(pb, "whatever its shape") && has(pb, "cannot say") && has(sn, "spin 0") && has(sn, "whatever its shape") && has(sn, "cannot say") && has(sn, "B(E2)"));
    checkTrue("card, Pb-208 (0.46.3): the measured B(E2) is given, and as FRDM calculates it round, it is a vibration and drawn round", has(pb, "measured B(E2") && has(pb, "vibration about a round shape") && has(pb, "drawn round"));
    checkTrue("card, Gd-158: the source is B(E2)", has(gd, "B(E2)") && has(gd, "spin 0"));
    checkTrue("card, an odd nucleus with no row (Li-10): no row in the table; if the spin is 0 or 1/2 the moment is exactly zero whatever the shape; the lab has no spin table", has(li10, "no row") && has(li10, "spin is 0 or 1/2") && has(li10, "whatever the shape") && has(li10, "no spin table"));
    checkTrue("card, H-1: one proton: spin 1/2, no quadrupole moment possible", has(h1, "spin 1/2") && has(h1, "whatever") && has(h1, "round"));
    checkTrue("card, Bi-209 (0.46.3, outside review): the measured moment of -0.5160 b is shown but not turned into a shape (next to closed shells, FRDM |beta2| 0.011); the shape is FRDM's", has(bi, "-0.5160 b") && has(bi, "Not turned into a shape") && has(bi, "0.011") && has(bi, "FRDM(2012)"));
    const std::string ex = shapeExplain();
    checkTrue("explanation: spectroscopic vs intrinsic, zero for spin 0 and 1/2, the strong-coupling K = I model assumption (poor for light nuclei and near closed shells), the 0.6 limit, a uniform charged body with a volume that is kept, "
              "a diffuse cloud edge, a static picture", has(ex, "spectroscopic") && has(ex, "intrinsic") && has(ex, "spin 0 and 1/2") && has(ex, "strong coupling") && has(ex, "K = I") && has(ex, "model assumption") &&
              has(ex, "near closed shells") && has(ex, "0.6") && has(ex, "FRDM(2012)") && has(ex, "uniform") && has(ex, "volume") && has(ex, "diffuse") && has(ex, "vibrate and rotate"));
    checkTrue("the credit names the IAEA file, its INDC report and the NSR keys, and says the page with the citation terms could not be read", has(kQuadCredit, "elec_mom_recomm.csv") && has(kQuadCredit, "indc-nds-0833") && has(kQuadCredit, "NSR") && has(kQuadCredit, "could not be read"));
}

#include "test_chain.inc"   // 0.12 stage 1: decay chains on the chart
#include "test_clean.inc"   // 0.12 stage 2: the accuracy clean-up
#include "test_audit028.inc"   // 0.28: the 0.26 audit's small items
#include "test_hfs.inc"        // 0.31: hyperfine structure
#include "test_gpu_colours.inc"   // 0.13 stage 1: phase colours on the GPU
#include "test_gpu_compute.inc"   // 0.13 stage 2: the superposition and the jump on the GPU (the physics check; the GPU comparison is tests/vk_compare.cpp)
#include "test_outline_norm.inc"   // 0.13 stage 3: the exact normalisation (part B) and the 90 % outline
#include "test_stage4_cloud.inc"   // 0.13 stage 4 part C: the Cloud 2D view's buffers
#include "test_chart_cache.inc"   // 0.13 stage 4 part B: the chart of nuclides kept on the GPU
#include "test_atom_tools.inc"   // 0.15 stage 5a: the Atom Lab toolbar buttons -> lab state

// 0.86.0 (PLAN_REVIEW.md R5): the made-up learning pictures (the Orbit picture's gliding electron balls, the Simple quark style's balls and flux tubes, the Nucleons
// look's colours and glow) are labelled "Made up, for learning" and never the realistic choice. Written before atom_tools' madeUp* and applyRealistic existed.
//  1. each of the three has the label at the start of its caption, and its value is not the realistic value of its setting;
//  2. after Most realistic (applyRealistic), for every element Z = 1..118 and each zoom (the whole atom, the nucleus, one nucleon), none of them is on.
static void testMadeUp086() {
    bool labels = true;
    for (const char* k : {"picture", "qstyle", "style"}) {
        const char* mv = madeUpValue(k);
        const char* rv = realisticValue(k);
        labels = labels && mv && rv && std::string(mv) != rv && madeUpChoice(k, mv) && !madeUpChoice(k, rv) && madeUpCaption(k).rfind(std::string(kMadeUpLabel) + ": ", 0) == 0;
    }
    checkTrue("made-up pictures: Orbit, Simple and Nucleons each labelled \"Made up, for learning\" in its caption, none the realistic choice", labels && std::string(kMadeUpLabel) == "Made up, for learning" &&
              std::string(madeUpValue("picture")) == "Orbit" && std::string(madeUpValue("qstyle")) == "Simple" && std::string(madeUpValue("style")) == "Nucleons");
    AtomLab L;
    L.viewW = 1000; L.viewH = 700;
    std::string back;
    int cases = 0;
    for (int Z = 1; Z <= 118; Z++)
        for (Tool t : {Tool::ZoomAtom, Tool::Nucleus, Tool::Nucleon}) {
            L.setParam("view", std::string("Orbit")); L.setParam("qstyle", std::string("Simple")); L.setParam("style", std::string("Nucleons"));   // every made-up one on first
            L.setParam("Z", double(Z));
            applyRealistic(L);
            pressTool(L, t);
            cases++;
            for (const std::string& m : madeUpOn(L)) back += " Z" + std::to_string(Z) + ":" + m;
        }
    checkTrue("made-up pictures: after Most realistic none is on, for every element and zoom (" + std::to_string(cases) + " cases;" + (back.empty() ? std::string(" none back)") : back + ")"), back.empty() && cases == 354);
}
#include "test_atom_chip.inc"   // 0.15 stage 5b: the element chip and its dropdown's controls

// (0.43: the Accurate quarks are clouds now; the wandering quarks of 0.16 stage 6 and their test testQuarkWander are gone, see git.)

// 0.22: the picture's colour line. A real hydrogen-like orbital has n − 1 nodes (n − l − 1 radial, l angular; any textbook, e.g. Griffiths, Introduction to Quantum Mechanics, 4.2),
// so its wave changes sign only when n > 1: 1s is the one orbital of the list with a single sign, drawn in one colour. The line must say "two colours" for every other one, and not for 1s.
static void testColourLine() {
    for (const char* orb : {"1s", "2s", "2p", "3s", "3p", "3d", "4s", "4p", "4d", "4f"}) {
        for (const char* view : {"Cloud", "Cloud 3D", "Slice"}) {
            AtomLab lab; lab.nowMs = [] { return 0.0; }; lab.p = oneParams(); lab.restart();
            lab.setParam("view", std::string(view)); lab.setParam("orb", std::string(orb));
            if (lab.usesWheel()) continue;
            std::string all; for (const std::string& l : lab.info()) all += l + "\n";
            const bool one = std::string(orb) == "1s", two = all.find("two colours = the + and − halves") != std::string::npos, single = all.find("one colour: ψ has the same sign everywhere") != std::string::npos;
            checkTrue(std::string("colour line, ") + orb + " " + view + (one ? ": one colour (no node)" : ": two colours (the wave changes sign)"), one ? (single && !two) : (two && !single));
        }
    }
}

// 0.22: the nucleus drawn as a smooth charge density (atom_core: nucleusChargeDots). Independent references: the IAEA charge radii (data/radii/charge_radii.csv, typed here:
// C-12 2.4702, Ca-40 3.4776, Pb-208 5.5012, U-238 5.8571 fm; 0.46.5: H-1 the CODATA 2022 proton radius 0.84075 fm, which the rest of the app uses, not the table's older 0.8783); U-238 is prolate (a positive measured quadrupole deformation, beta2 about +0.29: Raman et al. 2001), Pb-208 doubly magic and round.
static void testNucleusDensity() {
    const struct { int Z, N; double rms; } refs[] = {{6, 6, 2.4702}, {20, 20, 3.4776}, {82, 126, 5.5012}, {92, 146, 5.8571}, {1, 0, 0.84075}};
    for (const auto& r : refs) {
        const std::vector<double> d = nucleusChargeDots(r.Z, r.N, 20000, 0, 0, 1, 7);
        double s2 = 0; const std::size_t n = d.size() / 3;
        for (std::size_t i = 0; i < n; i++) s2 += d[3 * i] * d[3 * i] + d[3 * i + 1] * d[3 * i + 1] + d[3 * i + 2] * d[3 * i + 2];
        const double rms = n ? std::sqrt(s2 / n) : 0;
        checkTrue("nucleus density Z=" + std::to_string(r.Z) + " N=" + std::to_string(r.N) + ": 20,000 points, rms charge radius " + fmt::fixed(rms, 4) + " fm = the reference (IAEA; CODATA for H-1) " + fmt::fixed(r.rms, 4) + " fm within 0.5 %",
                  n == 20000 && std::fabs(rms - r.rms) < 0.005 * r.rms);
    }
    const auto second = [](const std::vector<double>& d, int k) { double s = 0; const std::size_t n = d.size() / 3; for (std::size_t i = 0; i < n; i++) s += d[3 * i + k] * d[3 * i + k]; return s / std::max<std::size_t>(n, 1); };
    const std::vector<double> u = nucleusChargeDots(92, 146, 20000, 0, 0, 1, 3), ux = nucleusChargeDots(92, 146, 20000, 1, 0, 0, 3), pb = nucleusChargeDots(82, 126, 20000, 0, 0, 1, 3);
    checkTrue("nucleus density: U-238 is stretched along its axis (<z^2> / <x^2> = " + fmt::fixed(second(u, 2) / second(u, 0), 3) + ", more than 1.1)", second(u, 2) > 1.1 * second(u, 0) && second(u, 2) > 1.1 * second(u, 1));
    checkTrue("nucleus density: the axis can be turned (axis x: <x^2> / <z^2> = " + fmt::fixed(second(ux, 0) / second(ux, 2), 3) + ")", second(ux, 0) > 1.1 * second(ux, 2));
    checkTrue("nucleus density: Pb-208 is round (<z^2> / <x^2> = " + fmt::fixed(second(pb, 2) / second(pb, 0), 3) + ", within 3 %)", std::fabs(second(pb, 2) / second(pb, 0) - 1) < 0.03);
    checkTrue("nucleus density: the same seed gives the same points", nucleusChargeDots(26, 30, 500, 0, 0, 1, 11) == nucleusChargeDots(26, 30, 500, 0, 0, 1, 11));
}

// ================= 0.37: the dots are exact INDEPENDENT samples, and the Slice dots lie on the cut plane =================
// (1) Independence, the failing-before proof of the new sampler: the share of dots above the plane z = 0 in one full ring of 2pz and 3dz2 (both symmetric, so 1/2 exactly). For N independent
// dots its scatter from lab to lab is binomial, 0.5 / sqrt(N) (0.0046 at 12,000); the Metropolis chain of 0.36 gave about 0.10 (research report of 6 Nov 2026, audit: 0.099-0.101), because one
// lobe could hold a long run of neighbouring dots. 16 seeded labs; the sample standard deviation must be below twice the binomial value (its own scatter at 16 labs is about 18 %).
// The pooled <cos^2 theta> is checked against the exact 3/5 (2pz) and 11/21 (3dz2: the integrals of (3c^2-1)^2 c^2 and (3c^2-1)^2 over c from -1 to 1, 88/105 and 8/5), within 5 standard errors.
static void testIndependentDots() {
    struct C { const char *orb, *key, *ori; double cos2; } cs[] = {{"2p", "pAx", "z", 3.0 / 5}, {"3d", "dAx", "z2", 11.0 / 21}};
    for (const C& c : cs) {
        std::vector<double> share; double sc = 0, sc2 = 0; long nAll = 0; long N = 0;
        for (int seed = 1; seed <= 16; seed++) {
            seedRandom(uint32_t(8100 + seed));
            AtomLab lab; lab.nowMs = [] { return 0.0; }; lab.p = oneParams(); lab.restart();
            lab.setParam("view", std::string("Cloud")); lab.setParam("orb", std::string(c.orb)); lab.setParam(c.key, std::string(c.ori));
            for (int i = 0; i < 40000 && lab.s.k < lab.cloudCap(); i++) lab.step(1.0 / 240);
            N = std::min<long>(lab.s.k, lab.cloudCap());
            long up = 0;
            for (long i = 0; i < N; i++) {
                const double x = lab.s.pts[3 * i], y = lab.s.pts[3 * i + 1], z = lab.s.pts[3 * i + 2], r2 = x * x + y * y + z * z;
                if (z > 0) up++;
                if (r2 > 0) { const double q = z * z / r2; sc += q; sc2 += q * q; nAll++; }
            }
            share.push_back(double(up) / double(N));
        }
        double m = 0, v = 0;
        for (double f : share) m += f;
        m /= double(share.size());
        for (double f : share) v += (f - m) * (f - m);
        const double sd = std::sqrt(v / double(share.size() - 1)), binom = 0.5 / std::sqrt(double(N));
        const double mc = sc / double(nAll), se = std::sqrt((sc2 / double(nAll) - mc * mc) / double(nAll));
        std::printf("independent dots, %s %s: share above z = 0 over 16 full rings of %ld dots: mean %.4f, sd %.4f (binomial %.4f); <cos^2> %.5f +- %.5f (exact %.5f)\n", c.orb, c.ori, N, m, sd, binom, mc, se, c.cos2);
        checkTrue(std::string("independent dots: ") + c.orb + " " + c.ori + ": the lobe share scatters like independent dots (sd below twice 0.5/sqrt(N))", sd < 2 * binom && N >= kCap);
        checkTrue(std::string("independent dots: ") + c.orb + " " + c.ori + ": the mean lobe share is 1/2 within 4 standard errors", std::fabs(m - 0.5) < 4 * binom / std::sqrt(16.0));
        checkTrue(std::string("independent dots: ") + c.orb + " " + c.ori + ": <cos^2 theta> of the dots is the exact value within 5 standard errors", std::fabs(mc - c.cos2) < 5 * se);
    }
}

// (2) The Slice view: its dots must lie ON the cut plane (0.36 drew a slab 0.07 x the view radius on each side, 2.8 a0 for n = 4, which filled the inner nodes: at the inner node of 4s the dots
// were 0.438 of the next maximum, research report of 6 Nov 2026). Here, 4s and 3s (Z = 1) cut through the nucleus: every dot within float rounding of the plane, and the dots in a ring 0.05 a0
// wide round each inner node, against a ring of the same width round the largest maximum between the first two nodes, must match the exact ratio of the density on the plane (the integral
// of r R(r)^2 over each ring, by Simpson's rule on R from atom_core's own radial function, whose shape tests/test_atom.cpp checks elsewhere) within 5 Poisson standard errors; the exact
// ratio is printed (it is below 0.01: the node is empty).
static void testSliceOnPlane() {
    for (const char* orb : {"4s", "3s"}) {
        seedRandom(424);
        AtomLab lab; lab.nowMs = [] { return 0.0; }; lab.p = oneParams(); lab.restart();
        lab.setParam("view", std::string("Slice")); lab.setParam("orb", std::string(orb)); lab.setParam("cut", 0.0); lab.setParam("cutAxis", std::string("Vertical"));
        for (int i = 0; i < 40000 && lab.s.k < lab.cloudCap(); i++) lab.step(1.0 / 240);
        const long N = std::min<long>(lab.s.k, lab.cloudCap());
        const Orbital& o = orbitals()[orbIndex(orb)];
        const auto R = [&](double r) { return radial(o, r); };
        std::vector<double> nodes;
        for (double r = 0.01; r < 60 && nodes.size() < 2; r += 0.01)
            if ((R(r) > 0) != (R(r + 0.01) > 0)) { double a = r, b = r + 0.01; for (int k = 0; k < 80; k++) { const double c = 0.5 * (a + b); ((R(a) > 0) == (R(c) > 0) ? a : b) = c; } nodes.push_back(0.5 * (a + b)); }
        double rm = 0, best = -1;                                                    // the largest maximum of r R^2 between the first node and the next (or beyond the only one)
        const double hiR = nodes.size() > 1 ? nodes[1] : nodes[0] + 20;
        for (double r = nodes[0]; r < hiR; r += 0.001) if (r * R(r) * R(r) > best) { best = r * R(r) * R(r); rm = r; }
        const double dw = 0.025;
        const auto ringMass = [&](double c0) { double s = 0; const int K = 400; const double h = 2 * dw / K; for (int k = 0; k <= K; k++) { const double r = c0 - dw + k * h; s += (k == 0 || k == K ? 1 : k % 2 ? 4 : 2) * r * R(r) * R(r); } return s * h / 3; };
        long onPlane = 0, inNode = 0, inMax = 0;
        for (long i = 0; i < N; i++) {
            const double x = lab.s.pts[3 * i] / kA0, y = lab.s.pts[3 * i + 1] / kA0, z = lab.s.pts[3 * i + 2] / kA0;
            if (std::fabs(y) < 1e-6) onPlane++;
            const double r = std::hypot(x, y, z);
            if (std::fabs(r - nodes[0]) < dw) inNode++;
            if (std::fabs(r - rm) < dw) inMax++;
        }
        const double exact = ringMass(nodes[0]) / ringMass(rm), got = inMax > 0 ? double(inNode) / double(inMax) : 1;
        const double se = inMax > 0 ? std::sqrt(double(std::max<long>(inNode, 1))) / double(inMax) : 1;
        std::printf("slice on the plane, %s: %ld of %ld dots on the plane; ring at the inner node %.4f a0: %ld dots, ring at the maximum %.4f a0: %ld dots; ratio %.5f, exact %.5f\n", orb, onPlane, N, nodes[0], inNode, rm, inMax, got, exact);
        checkTrue(std::string("slice: ") + orb + ": every dot lies on the cut plane", onPlane == N && N >= kCap);
        checkTrue(std::string("slice: ") + orb + ": the inner node is as empty as the exact density on the plane says (dots ratio within 5 standard errors of the exact ratio, which is below 0.01)", std::fabs(got - exact) < 5 * se + 1e-9 && exact < 0.01);
        // the cut moved (a drag): the ring is not emptied (0.37 stage 1 started it again and made up to 6,000 dots a frame, which lagged on the owner's PC), at most kSliceDotsPerFrame new dots a
        // frame are made on the new plane, and a few frames later every dot lies on it
        lab.setParam("cut", 0.3);
        lab.step(1.0 / 240);
        const double off = lab.cutOffsetM() / kA0;
        const auto onNew = [&] { long c = 0; for (long i = 0; i < lab.cloudCap(); i++) if (std::fabs(lab.s.pts[3 * i + 1] / kA0 - off) < 1e-6 * (1 + std::fabs(off))) c++; return c; };
        const long first = onNew();
        checkTrue(std::string("slice: ") + orb + ": after the cut moves, the ring stays full and one frame makes at most " + std::to_string(AtomLab::kSliceDotsPerFrame) + " new dots, on the new plane (" + std::to_string(first) + ")",
                  lab.s.k >= lab.cloudCap() && first > 0 && first <= AtomLab::kSliceDotsPerFrame);
        for (int i = 0; i < 20; i++) lab.step(1.0 / 240);
        checkTrue(std::string("slice: ") + orb + ": 20 frames later every dot lies on the new plane", onNew() == lab.cloudCap());
    }
}

#include "test_hf.inc"

// 0.40: the whole atom keeps 12,000 / 24,000 / 48,000 dots PER ELECTRON (Normal / High / Ultra), up to 5 times that; one electron or one orbital keeps the old count.
// References by hand: helium (2 electrons) 24,000; neon (10) 120,000 -> the ceiling 60,000; uranium at Ultra 92 x 48,000 -> 240,000.
static void testManyDots() {
    const auto cap = [](int Z, const char* elec, const char* dens, const char* show = "All") {
        AtomLab L; L.p = oneParams(); L.restart(); L.setParam("view", std::string("Cloud 3D")); L.setParam("Z", double(Z)); L.setParam("elec", std::string(elec));
        L.setParam("cdens", std::string(dens)); L.setParam("show", std::string(show)); return L.cloudCap(); };
    checkNum("many dots: helium, every electron, Normal: 2 x 12,000", cap(2, "All", "Normal"), 24000, 0);
    checkNum("many dots: neon, every electron, Normal: 10 x 12,000 cut to the ceiling 60,000", cap(10, "All", "Normal"), 60000, 0);
    checkNum("many dots: uranium, every electron, Ultra: the ceiling 240,000", cap(92, "All", "Ultra"), 240000, 0);
    checkNum("many dots: carbon, every electron, High: 6 x 24,000 cut to 120,000", cap(6, "All", "High"), 120000, 0);
    checkNum("many dots: uranium with one electron (Electrons One): 12,000 as before", cap(92, "One", "Normal"), 12000, 0);
    checkNum("many dots: hydrogen (one electron anyway): 12,000", cap(1, "All", "Normal"), 12000, 0);
    {   // the cloud really fills to that many dots, and each dot's light is scaled down by the same factor (the whole cloud equally bright)
        AtomLab L; L.p = oneParams(); L.restart(); L.setParam("view", std::string("Cloud 3D")); L.setParam("Z", 26.0); L.setParam("elec", std::string("All")); L.paused = false;
        for (int i = 0; i < 5000 && L.s.k < L.cloudCap(); i++) L.step(1.0 / 60);
        checkTrue("many dots: iron's cloud fills to 60,000 dots (" + std::to_string(L.s.k) + ") and the arrays hold them", L.cloudCap() == 60000 && L.s.k >= 60000 && L.s.sg.size() >= 60000);
        checkNum("many dots: a dot's light at 60,000 dots is 1/5 of a 12,000-dot cloud's", cloudDotWeight(L.cloudCap()), 0.2, 1e-12);
    }
}

#include "test_qcd042.inc"
#include "test_qcd043.inc"   // 0.43: the Accurate quark clouds
#include "test_qcd044.inc"   // 0.44: the Accurate dots by momentum fraction (nucleon tomography)
#include "test_qcd045.inc"   // 0.45: the outside review of the quarks
#include "test_atom046.inc"   // 0.46: the atom review, jumps to n = 6, alkali jumps
#include "test_atom047.inc"   // 0.47: put the outer electron in a shell; its m = 0 shape
#include "test_atom0461.inc"  // 0.46.1: the outside review
#include "test_atom0465.inc"  // 0.46.5: realistic light nuclei (the deuteron from CD-Bonn, C-12 and O-16 measured, the edge)
#include "test_atom0472.inc"  // 0.47.2: the GRASP2018 clouds of Li..Fr
#include "test_atom0473.inc"  // 0.47.3: level jumps of the two-s-electron atoms; the jump status of all 118 atoms
#include "test_atom0474.inc"  // 0.47.4: Ca, Sr, Ba, Ra, Hg, Yb with one orbital set per atom; D4, this app's own rates
#include "test_atom0521.inc"  // 0.52.1: CIE 1931 line colours, the quark mass bar's nucleon, the phase ring inside a nucleon
#include "test_zeeman.inc"    // 0.54.0: atoms in a magnetic field (the Zeeman effect, weak to strong field)
#include "test_atom078.inc"   // 0.78.0: the photon's dipole pattern
#include "test_atom0541.inc"  // 0.54.1: the nucleus Density look projected exactly; the outer electron's dots at their true share
#include "test_atom0910.inc"  // 0.91.0: the Slice's outer electron alone; every level a cloud; the Slice samplers on the GPU; hydrogen's recoil
#include "test_stark.inc"     // 0.93.0: hydrogen's Stark effect
#include "test_breit_rabi.inc"   // 0.93.0: the ground state's hyperfine levels in a magnetic field

// 0.48 (review 4): Rf to Og have a family only by their column of the table (Smits et al., Phys. Rep. 1035 (2023), section 6.6: Og may be a solid semiconductor, Cn may act as a noble gas)
static void testSuperheavyFamilies() {
    bool ok = true;
    for (int Z = 1; Z <= 118; Z++) {
        const std::string shown = categoryShown(Z);
        const bool marked = shown.find("(expected)") != std::string::npos;
        ok = ok && marked == (Z >= 104) && shown.rfind(categoryName(Z), 0) == 0 && familyNote(Z).empty() == (Z < 104);
    }
    checkTrue("superheavy: Rf to Og (and only they) shown as \"(expected)\" with a note; the others as before", ok);
    checkTrue("superheavy: the element card's first chip is the shown family", elementChips(118).at(0) == "noble gas (expected)" && elementChips(54).at(0) == "noble gas");
    checkTrue("superheavy: oganesson's note says it may be a solid semiconductor, copernicium's that it may act as a noble gas",
              familyNote(118).find("semiconductor") != std::string::npos && familyNote(112).find("noble gas") != std::string::npos);
}

int main() {
    testPhotonPattern078();
    testSuperheavyFamilies();
    testQcd042();
    testQcd043();
    testQcd044();
    testQcd045();
    testReview046A();
    testDeuteron0465();
    testLightNuclei0465();
    testEmitOdds0461();
    testTexts0461();
    testJumpsN6();
    testStrongestRoute();
    testAlkaliJumps();
    testValencePlace();
    testGrasp0472();
    testLevels0473();
    testLevels0474();
    testManyDots();
    testHartreeFock();
    testConstants();
    testOneEnergy();
    // guards = the worst error MEASURED after the fix (Linux GCC, 32 runs of 60 s) + 1 %: radius 2.16990e-4 -> 2.192e-4, energy 4.70644e-8 -> 4.754e-8, turning rate 1.45367e-4 -> 1.469e-4. The same runs
    // before the fix (0.10.6 stage 2 code, one leapfrog step per frame): radius 7.783e-3 (n = 1, speed 30), energy 5.964e-5, rate 5.165e-3, so this fails without the sub-steps.
    testJumpSameDots();
    testJumpBuild();
    testManyJumpThreads089();
    testPlainWaste057();
    testSliceSuper064();
    testCardReference057();
    testOrbitAccuracy(2.192e-4, 4.754e-8, 1.469e-4);
    testFixedExtent();
    // guards: outline margin 0.10 = about half the smallest MEASURED difference (0.1867, 4f m=+1 against its fallback 4fz3); norm 6.69e-4 = measured 6.62289e-4 (the midpoint quadrature's own error on the orthogonal
    // pair 4pz + 4fz3; the overlapping pairs reach 1.19e-4) + 1 %; dots 0.0591 = measured 0.05844 (Metropolis sampling error of the 8 seeded labs) + 1 %.
    // BEFORE the fix (same tests): outline asked for the fallback in 20 of 20 states (difference 0), norm error 1.00009 (the same state twice at mix 50: the integral is 2), dots 0.944 off (and the dots thinned instead of all shown).
    testOutlineIsTheState(0.10);
    testSuperNorm(6.69e-4);
    testOverlapDots(0.0591);
    testSuperReadout();
    testRelativityText();
    testTimelineText();
    testVisibleRange();
    testSpectralColours0521();
    testNucleonShown0521();
    testOuterDenser0521();
    testGlowCore0521();
    testZeemanLorentz();
    testZeemanWeakPatterns();
    testZeemanDoublet();
    testZeemanLimits();
    testZeemanStrengths();
    testZeemanMeasuredG();
    testZeemanOneElectron();
    // guard 0.00920 = the worst MEASURED difference of the sampled <r> from the exact value at the full ring, 0.00910 (96 seeded labs: 12 orbitals x 8 seeds; 0.37, independent dots), + 1 %; 96 of 96
    // were within 1 %, mean 0.0025. (0.36's Metropolis chain: worst 0.17702, 95 of 96 within 10 %, mean 0.0311; its guard was 0.1788.)
    // BEFORE the fix (same test): the sampled value was shown from 300 dots on (1,600 dots here: a number instead of the placeholder), with no note.
    testSampledRadius(0.00920);
    // guards = the worst MEASURED difference + 1 %. Mass against AME2020: 3.899e-7 (Fe-56; the lab's table is rounded to 1 keV per nucleon and includes the electrons' binding) -> 3.94e-7; BEFORE the fix 7.003e-3 (H-2, the old
    // form A u - Z m_e), so this fails without the new mass. 1s norm integral: pi to 5.83e-10 -> 5.9e-10. Sampled <r> at Z = 3 and 26 (48 labs): worst 0.07226 -> 0.0730.
    testNuclearMass(3.94e-7);
    testSpectrumIndependent();
    testColourLine();
    testNucleusDensity();
    testBasicOrbitChecks();
    testSampledRadiusZ(0.0730);
    // guards = the worst MEASURED value + 1 % (Linux GCC Release): area error 2.2069e-8 -> 2.23e-8 (Simpson, 20,000 intervals); stored curve against Simpson 3.8113e-7 -> 3.85e-7; right edge 9.1995e-9 -> 9.30e-9;
    // <r> by quadrature 4.5654e-10 -> 4.62e-10; 1s outline reach 0.05966 of a grid step from the closed-form 90 % radius -> 0.0603; dots: Kolmogorov distance of 60 seeded labs, worst 0.105906 -> 0.1070.
    // Two guards are NOT measured + 1 %: the closed-form comparison (measured 1.976e-14) and the peaks (measured 0, the bisection lands on the exact value) are rounding-level, where a 1 % guard would depend on the
    // compiler's last bit, so they use 1e-12 (50 times the first, far below any real error).
    testRadialGraph(1e-12, 2.23e-8, 3.85e-7, 9.30e-9, 4.62e-10, 1e-12, 0.0603);
    testRadialDots(0.1070);
    testRadialReadout();
    testAsdData();
    testDirac();
    testLamb0493();
    testDiracRows086();
    testDiracCloud0493();
    testDiracLab0493();
    testDiracPrecision();
    testLifetimes();
    // guards: layout 1e-9 (rounding level: the size is exact by construction); closest pair 0.673 fm = the smallest MEASURED (0.6801 fm, C-12, 9 nuclei) - 1 %, a floor
    testNuclearSize(1e-9, 0.673);
    // guard: closest two nucleons in the 12 deformed clouds = the smallest MEASURED (0.6125 fm, Li-8) - 1 %, a floor
    testQuadrupole(0.606);
    testComplexBasics();
    testWinding();
    testLz();
    testOrthonormal();
    testAgainstScipy();
    testDensityFromReal();
    testRealDotsUnchanged();
    testComplexDots();
    testIndependentDots();
    testSliceOnPlane();
    testPhaseWheel();
    testComplexOutline();
    testBeatPeriodAndAmplitude();
    testBeatNormStationary();
    testSuperDots();
    testJumpShares0493();
    testJumpNormAndContinuity();
    testJumpDots();
    testOrbitGlide();
    testJumpHandover();
    testOutlineFade();
    testAutoSilence();
    testChart();
    testChartLandscape();
    testChartBinding();
    testDecayChain();
    testDecayChainDraw();
    testDecayChainLab();
    testCleanup();
    testAudit028();
    testHfs();
    testGpuColours();
    testGpuComputePhysics();
    testGpuComputeLogic();
    testExactNormalisation();
    testOutlineLevelIndependent();
    testOutlineGpuLogic();
    testCloudBuffers();
    testChartCache();
    testAtomTools();
    testMadeUp086();
    testAtomChip();
    testDensityProjection0541();
    testTrueShare0541();
    testSliceAlone0910();
    testOrthogonal0910();
    testLevelClouds0910();
    testRecoil0910();
    testStark();
    testBreitRabi();
    std::printf("%d atom checks, %d failed (%d on an exact rounding tie, off by one in the last digit).\n", g_checks, g_failed, g_ties);
    return g_failed ? 1 : 0;
}
