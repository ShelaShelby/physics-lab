// Black Hole Lab, 0.56.3 research (PLAN_MOVINGGAS.md): moving gas worked out on the processor, a tool run by hand (not part of the app).
//   movinggas --check          the self-checks against independent references (seconds; ctest movinggas_check, label quick)
//   movinggas --table          the true speeds and times (plan Q1, Q2) for 10 Suns, Sagittarius A* and M87*, spin 0 and 0.9
//   movinggas --render <dir>   the view of plan Q3-Q5 (spin 0.6, camera hovering at 60 M, 10 degrees above the disc): every pixel's ray traced once with the app's
//                              reference integrator (geodesic.h KerrBL, Dormand-Prince) plus the time it takes; then the frames, light curves and the structure
//                              model written as raw float32 files for tools/movinggas_pictures.py
// G = c = 1, lengths and times in M (one M of time = GM/c^3).
// 0.56.4: the gas (clocks, the clump, the pattern) and the integrator with t moved into the app (src/blackhole/gas.h); this tool calls them (one copy).
//   The checks of the clocks and the published periods moved to tests/test_blackhole.cpp (testGas); --check keeps those of the integrator's time.
//
// TIME ALONG A RAY. Per unit of the ray's affine parameter (J15's zeta, the app's KerrBL), with p_t = -1 and b = p_phi:
//   dt/dzeta = [ (r^2 + a^2) P / Delta + a (b - a sin^2 theta) ] / rho^2,   P = r^2 + a^2 - a b.
//   It is -dH/dp_t of J15 (A.15)'s Hamiltonian (KerrBL's dphi/dzeta is -dH/dp_phi the same way), and it is GLa (12): t_o - t_s = I_t + a^2 G_t with the
//   integrands of (13c) and (13f): (r^2 + a^2) P / Delta + a b - a^2 = [r^2 Delta + 2 M r (r^2 + a^2 - a lambda)] / Delta, and a^2 - a^2 sin^2 = a^2 cos^2.
//   (S. E. Gralla, A. Lupsasca, PRD 101, 044032 (2020), arXiv:1910.12881, "GLa".) Checked below against GLb (72): successive images near the critical curve
//   are delayed by tau_0 = 3 sqrt(3) pi M at a = 0 (S. E. Gralla, A. Lupsasca, PRD 101, 044031 (2020), arXiv:1910.12873, "GLb").
// THE GAS. Circular orbits, Omega = 1 / (r^(3/2) + a) (BPT (2.16), disc.h orbitOmega); speed in the locally non-rotating frame V = (varpi / alpha)(Omega - omega)
//   (C75 (A7c) = J15 (A.7)); the gas's own clock dtau/dt = 1 / u^t = alpha (1 - V^2)^(1/2) (C75 (A7a)), checked against the metric's normalisation (J15 (A.1)).
//   The hovering camera's clock: dtau/dt = alpha (J15 (A.1)-(A.3); kerr.h hoverShift(b = 0) = 1 / alpha).
// THE CLUMP (a labelled model). A. E. Broderick, A. Loeb, MNRAS 367, 905 (2006), arXiv:astro-ph/0509237 ("BL06") (5): a spot of extra emission, Gaussian in
//   the distance measured in its own rest frame, n = exp(-[dx.dx + (u.dx)^2] / (2 R_S^2)), centred on a point on a circular orbit, R_S = 1.5 M (BL06 section 2.3
//   and its 2005 paper); it keeps its shape (it does not shear). On the thin disc's face: dx = (0, dr, 0, dphi), so dx.dx + (u.dx)^2 = g_rr dr^2 + (g_phph + u_phi^2) dphi^2.
//   Brightness: the disc's PT flux times (1 + A n), A = 4 here (a choice: its centre 5 times the gas around it); the light reaching the camera per pixel g^4 I (bolometric, I_nu / nu^3 conserved).
// THE STRUCTURE (a labelled model). J. D. Schnittman, J. H. Krolik, J. F. Hawley, ApJ 651, 1031 (2006), arXiv:astro-ph/0606615 ("SKH06") section 5: in their
//   simulated (MHD, Schwarzschild) disc, bright patches are self-similar with dphi = 25 degrees and dr / r = 0.3 (k_eta break 5.5, lambda_eta = 0.26), and live
//   T_life = 0.3 T_orb (36). Here: patches born at random places and times (a hash, so a graphics card can make the same ones), each a Gaussian in (ln r, phi) of
//   full width 0.26 and 25 degrees at birth, its light growing and fading as x e^(1 - x), x = (t - t_birth) / T_life(r_birth); each piece of it carried round at
//   its own radius's Omega(r): phi(t) = phi_birth + Omega(r) (t - t_birth), so it shears into a trailing spiral at the true speeds. Brightness factor exp(c S) / mean.
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

#include "blackhole/disc.h"
#include "blackhole/gas.h"
#include "blackhole/geodesic.h"
#include "blackhole/kerr.h"
#include "blackhole/presets.h"
#include "blackhole/spin.h"
#include "orbit/units.h"

using namespace blackhole;
constexpr double kPi = 3.14159265358979323846;

// ---------------------------------------------------------------- the gas (blackhole/gas.h)
static double orbitPeriod(double r, double a) { return gasPeriod(r, a); }
static double gasUt(double r, double a) { return 1 / gasClockRate(r, a); }
static double tgSeconds(double massSun) { return massTimeSeconds(massSun); }

// ---------------------------------------------------------------- checks
static int fails = 0;
static void check(bool ok, const std::string& what) { std::printf("%s %s\n", ok ? "pass" : "FAIL", what.c_str()); if (!ok) fails++; }
static std::string f(double x) { char b[64]; std::snprintf(b, sizeof b, "%.6g", x); return b; }

static int runChecks() {
    // 4. the time along a ray: GLb (72), a = 0: successive crossings of the plane by a ray just outside the shadow's edge differ by tau_0 = 3 sqrt(3) pi M
    {
        const KerrBLt ch(0.0);
        Camera cam; cam.r = 1e4; cam.theta = 1.0;
        const double edge = std::asin(3 * std::sqrt(3.0) * std::sqrt(1 - 2 / cam.r) / cam.r);   // Synge's edge (b = 3 sqrt(3) M; schwarzschild.h cameraImpact)
        std::vector<double> cr;
        TraceOptions o; o.maxCross = 8; o.cross = &cr; o.maxSteps = 400000;
        const RayEnd e = trace(ch, cam, lookAt(edge * (1 + 4e-9), 0.3), o);
        const int n = static_cast<int>(cr.size() / kTState);
        bool ok = e.kind == RayEnd::Sky && n >= 5;
        double d = 0;
        int kb = 0;   // the two successive crossings nearest the photon sphere (around the ray's closest approach)
        for (int k = 0; ok && k + 1 < n; k++)
            if (std::fabs(cr[kTState * k] - 3) + std::fabs(cr[kTState * (k + 1)] - 3) < std::fabs(cr[kTState * kb] - 3) + std::fabs(cr[kTState * (kb + 1)] - 3)) kb = k;
        if (ok) d = cr[kTState * kb + 5] - cr[kTState * (kb + 1) + 5];   // t falls along the ray followed into the past
        const double tau0 = 3 * std::sqrt(3.0) * kPi;
        check(ok && std::fabs(d / tau0 - 1) < 1e-4, "GLb (72): the delay between two successive crossings of a ray near the edge = " + f(d) + " M against 3 sqrt(3) pi = " +
                                                     f(tau0) + " (" + std::to_string(n) + " crossings)");
        // the same ray: the radius at those crossings is the photon sphere's 3 M
        if (ok) check(std::fabs(cr[kTState * kb] - 3) < 1e-3 && std::fabs(cr[kTState * (kb + 1)] - 3) < 1e-3, "those crossings at r = " + f(cr[kTState * kb]) + ", " + f(cr[kTState * (kb + 1)]) + " M (the photon sphere: 3 M)");
    }
    // 5. a ray along the hole's direction from far away at a = 0 (b = 0, radial): the time from r1 to the crossing radius equals the closed form
    //    t = r1 - r2 + 2 ln((r1 - 2) / (r2 - 2)) of dt/dr = 1 / (1 - 2/r) (Schwarzschild, radial light). Camera on the plane looking straight in: it falls to
    //    the horizon; the time to r = 3 M is checked through the state at the first crossing of a plane tilted... (simpler: the camera at theta = pi/2 looking
    //    in has its ray IN the plane; use theta slightly off and psi = 0: the ray stays at the camera's theta and never crosses). So: integrate the radial ray
    //    with the app's integrateSpan and read r and t.
    {
        const KerrBLt ch(0.0, 1e5);
        Camera cam; cam.r = 100; cam.theta = 1.2;
        Ray ray;
        ch.launch(cam, lookAt(0, 0), ray);
        integrateSpan(ch, ray, -30.0);
        const double r2 = ray.y[0], t = -ray.y[5];
        const double ref = cam.r - r2 + 2 * std::log((cam.r - 2) / (r2 - 2));
        check(std::fabs(t / ref - 1) < 1e-9, "radial light, a = 0, from 100 M to " + f(r2) + " M: " + f(t) + " M against r1 - r2 + 2 ln((r1 - 2)/(r2 - 2)) = " + f(ref));
    }
    std::printf("%s: %d failed\n", fails ? "FAILED" : "all passed", fails);
    return fails ? 1 : 0;
}

// ---------------------------------------------------------------- Q1, Q2 table
static std::string human(double s) {
    char b[64];
    if (s < 1e-3) std::snprintf(b, sizeof b, "%.3g ms", s * 1e3);
    else if (s < 1) std::snprintf(b, sizeof b, "%.3g ms", s * 1e3);
    else if (s < 120) std::snprintf(b, sizeof b, "%.3g s", s);
    else if (s < 7200) std::snprintf(b, sizeof b, "%.3g min", s / 60);
    else if (s < 3 * 86400) std::snprintf(b, sizeof b, "%.3g h", s / 3600);
    else std::snprintf(b, sizeof b, "%.3g days", s / 86400);
    return b;
}
static int runTable() {
    int n = 0;
    const Preset* ps = presets(n);
    struct Hole { const char* name; double m; } holes[3] = {{"10 Suns", 10}, {"", 0}, {"", 0}};
    for (int i = 0; i < n && i < 2; i++) { holes[i + 1].name = ps[i].name; holes[i + 1].m = ps[i].massSun; }
    std::printf("one M of time (GM/c^3): ");
    for (auto& h : holes) std::printf("%s %s; ", h.name, human(tgSeconds(h.m)).c_str());
    std::printf("\n| spin | r (M) | V (c) | orbit, far clock (M) | gas clock / far | ");
    for (auto& h : holes) std::printf("%s | ", h.name);
    std::printf("\n");
    for (double a : {0.0, 0.9}) {
        const double rms = innermostStable(a, true);
        const double rs[4] = {rms, 6.0, 10.0, 20.0};
        for (int i = 0; i < 4; i++) {
            const double r = rs[i];
            if (i > 0 && std::fabs(r - rms) < 1e-9) continue;   // a = 0: r_ms is 6 M
            const double T = orbitPeriod(r, a);
            std::printf("| %.1f | %.3f | %.3f | %.2f | %.4f | ", a, r, gasSpeed(r, a), T, 1 / gasUt(r, a));
            for (auto& h : holes) std::printf("%s | ", human(T * tgSeconds(h.m)).c_str());
            std::printf("\n");
        }
    }
    const double a = 0.6, rc = 60, th = 80 * kPi / 180;
    std::printf("hovering camera at %.0f M, spin 0.6, 10 deg above the disc: its clock / far = alpha = %.5f (kerr.h hoverShift(b = 0) = %.5f)\n", rc,
                kerrMetric(a, rc, th).alpha, 1 / hoverShift(a, rc, th, 0));
    const double T = orbitPeriod(6, 0) * tgSeconds(ps[0].massSun) / 60;
    std::printf("example: %s, spin 0, the clump at 6 M: one orbit = %.1f min far away, %.1f min on the clump's own clock\n", ps[0].name, T, T / gasUt(6, 0));
    return 0;
}

// ---------------------------------------------------------------- the view
struct Cross { float r, ph, t, g; };          // one crossing of the disc's plane: radius, BL phi, travel time (M, > 0), shift
constexpr int kOrders = 3;
struct Pix { int order = -1; Cross c[kOrders]; int nc = 0; float w = 0; };   // order: index of the first crossing on the disc (-1: none)

// 0.79.0: the structure is the app's streaks (blackhole/gas.h StreakField; 0.56.4-0.78: SKH06's patches, removed): the same calls as before
struct Structure {
    double a = 0, rIn = 6, rOut = 20, contrast = kStreakC, norm = 1;   // (norm: the streaks' mean is 1 by construction)
    void normalise(int) {}
    double factor(double r, double ph, double t) const { return streakField().factor(r, ph, t, a, 0, 0, contrast); }
};

static void writeRaw(const std::string& path, const std::vector<float>& v) {
    FILE* fp = std::fopen(path.c_str(), "wb");
    if (!fp) { std::fprintf(stderr, "cannot write %s\n", path.c_str()); std::exit(2); }
    std::fwrite(v.data(), sizeof(float), v.size(), fp);
    std::fclose(fp);
}

static int runRender(const std::string& dir) {
    const double a = 0.6, rIn = innermostStable(a, true), rOut = 12;
    Camera cam; cam.r = 60; cam.theta = 80 * kPi / 180; cam.phi = 0;
    const int W = 498, H = 300;
    const double halfW = 12.6 * kPi / 180, fpx = (W / 2.0) / std::tan(halfW);
    const KerrBLt ch(a);
    std::vector<Pix> pix(static_cast<std::size_t>(W) * H);
    unsigned nt = std::max(1u, std::thread::hardware_concurrency());
    std::vector<std::thread> pool;
    for (unsigned k = 0; k < nt; k++)
        pool.emplace_back([&, k] {
            std::vector<double> cr;
            for (int y = static_cast<int>(k); y < H; y += static_cast<int>(nt))
                for (int x = 0; x < W; x++) {
                    const double U = (x + 0.5 - W / 2.0) / fpx, V = -(y + 0.5 - H / 2.0) / fpx;   // right = e_phi, up = -e_theta
                    const double rho = std::sqrt(U * U + V * V), psi = std::atan(rho), chi = std::atan2(U, -V);
                    Pix& p = pix[static_cast<std::size_t>(y) * W + x];
                    p.w = static_cast<float>(1 / (1 + rho * rho) / std::sqrt(1 + rho * rho) / (fpx * fpx));   // the pixel's solid angle: cos^3(psi) / f^2
                    cr.clear();
                    TraceOptions o; o.maxCross = kOrders; o.cross = &cr;
                    Ray ray;
                    if (!ch.launch(cam, lookAt(psi, chi), ray)) continue;
                    const double b = ray.c[0];
                    trace(ch, cam, lookAt(psi, chi), o);
                    p.nc = static_cast<int>(cr.size() / kTState);
                    for (int i = 0; i < p.nc; i++) {
                        const double r = cr[kTState * i];
                        p.c[i] = {static_cast<float>(r), static_cast<float>(cr[kTState * i + 2]), static_cast<float>(-cr[kTState * i + 5]),
                                  static_cast<float>(r > rIn ? discShiftKerr(r, b, a) : 0)};
                        if (p.order < 0 && r >= rIn && r <= rOut) p.order = i;
                    }
                }
        });
    for (auto& t : pool) t.join();

    // the clump (BL06 (5)) at rc
    const double rc = 6, RS = 1.5, A = 4;
    Clump cl; cl.a = a; cl.rc = rc; cl.RS = RS; cl.A = A; cl.init();   // clump at phi = 0 at t = 0 (phi = 0: the camera's side)
    const double Tc = cl.period();
    auto clump = [&](double r, double ph, double tEmit) { return cl.density(r, ph, tEmit); };
    double tConst = 0;
    // brightness of a pixel at observed time T (far clock at the camera): g^4 F (1 + A n) [* structure]
    auto shade = [&](const Pix& p, double T, bool withClump, bool clumpOnly, bool travel, const Structure* st, int onlyOrder) {
        if (p.order < 0) return 0.0;
        if (onlyOrder >= 0 && p.order != onlyOrder) return 0.0;
        const Cross& c = p.c[p.order];
        const double te = travel ? T - c.t : T - tConst;   // without travel time: every pixel at one moment (the direct image's middle travel time)
        const double F = discFlux(c.r, a), g4 = std::pow(static_cast<double>(c.g), 4);
        const double n = withClump ? A * clump(c.r, c.ph, te) : 0;
        const double s = st ? st->factor(c.r, c.ph, te) : 1;
        return g4 * F * (clumpOnly ? n : (s + n));
    };

    FILE* meta = std::fopen((dir + "/meta.txt").c_str(), "w");
    if (!meta) { std::fprintf(stderr, "no dir %s\n", dir.c_str()); return 2; }
    int nP = 0; const Preset* ps = presets(nP);
    const double tg = tgSeconds(ps[0].massSun);
    std::fprintf(meta, "W %d\nH %d\nspin %.3f\nrIn %.4f\nrOut %.2f\ncamR %.1f\ncamDegAbove 10\nrc %.2f\nRS %.2f\nA %.2f\nTc %.6f\ntgSec %.6f\nmassName %s\n",
                 W, H, a, rIn, rOut, cam.r, rc, RS, A, Tc, tg, ps[0].name);
    std::fprintf(meta, "camClock %.6f\nclumpClock %.6f\n", kerrMetric(a, cam.r, cam.theta).alpha, 1 / gasUt(rc, a));

    // Q3: the delays. Over the clump's images (pixels where the clump is bright), the light-weighted travel time per order
    double minD = 1e9, maxD = -1e9;
    const int nFrames = 6;
    for (int fi = 0; fi < 64; fi++) {
        const double T = fi * Tc / 64;
        double s[2] = {0, 0}, st[2] = {0, 0};
        for (const Pix& p : pix) {
            if (p.order < 0 || p.order > 1) continue;
            const double v = shade(p, T, true, true, true, nullptr, -1) * p.w;
            s[p.order] += v; st[p.order] += v * p.c[p.order].t;
        }
        if (s[0] > 0 && s[1] > 0) { const double d = st[1] / s[1] - st[0] / s[0]; minD = std::min(minD, d); maxD = std::max(maxD, d); }
    }
    // across the direct image of the disc: nearest and farthest travel time at the clump's radius
    double tNear = 1e9, tFar = -1e9;
    for (const Pix& p : pix) if (p.order == 0 && std::fabs(p.c[0].r - rc) < 0.05) { tNear = std::min(tNear, static_cast<double>(p.c[0].t)); tFar = std::max(tFar, static_cast<double>(p.c[0].t)); }
    std::fprintf(meta, "delaySecMinM %.4f\ndelaySecMaxM %.4f\ndirectSpreadM %.4f\n", minD, maxD, tFar - tNear);
    std::printf("clump orbit %.2f M (%.1f min for %s); secondary image later than the direct one by %.1f to %.1f M (%.2f to %.2f orbits);\n"
                "the direct image at r = 6 M spans %.1f M of travel time (%.2f orbits)\n",
                Tc, Tc * tg / 60, ps[0].name, minD, maxD, minD / Tc, maxD / Tc, tFar - tNear, (tFar - tNear) / Tc);

    tConst = 0.5 * (tNear + tFar);
    std::vector<float> img(static_cast<std::size_t>(W) * H);
    // light curves over two orbits: direct, secondary, total; total without travel time
    const int nL = 240;
    std::vector<float> lc(static_cast<std::size_t>(nL) * 5);
    for (int i = 0; i < nL; i++) {
        const double T = 2 * Tc * i / nL;
        double d = 0, s2 = 0, tot = 0, noT = 0;
        for (const Pix& p : pix) {
            if (p.order < 0) continue;
            const double v = shade(p, T, true, true, true, nullptr, -1) * p.w;
            tot += v; if (p.order == 0) d += v; else if (p.order == 1) s2 += v;
            noT += shade(p, T, true, true, false, nullptr, -1) * p.w;
        }
        lc[5 * i] = static_cast<float>(T); lc[5 * i + 1] = static_cast<float>(d); lc[5 * i + 2] = static_cast<float>(s2);
        lc[5 * i + 3] = static_cast<float>(tot); lc[5 * i + 4] = static_cast<float>(noT);
    }
    writeRaw(dir + "/lightcurve.f32", lc);
    // the period check: the curve one orbit later is the same curve; half an orbit later it is not
    double dif = 0, half = 0, mx = 0;
    for (int i = 0; i < nL / 2; i++) { dif = std::max(dif, std::fabs(static_cast<double>(lc[5 * i + 3] - lc[5 * (i + nL / 2) + 3]))); mx = std::max(mx, static_cast<double>(lc[5 * i + 3])); }
    for (int i = 0; i < nL / 4; i++) half = std::max(half, std::fabs(static_cast<double>(lc[5 * i + 3] - lc[5 * (i + nL / 4) + 3])));
    std::printf("light curve: one orbit later differs by %.2g of its peak, half an orbit later by %.2g\n", dif / mx, half / mx);
    std::fprintf(meta, "lcPeriodicity %.3g\nlcHalf %.3g\n", dif / mx, half / mx);

    int i2 = 0;
    for (int i = 0; i < nL / 2; i++) if (lc[5 * i + 2] > lc[5 * i2 + 2]) i2 = i;
    const double t2 = std::fmod(static_cast<double>(lc[5 * i2]) + Tc, Tc);
    // the strip: nFrames over one orbit, smooth disc + clump, the third frame at the secondary image's brightest; and the base disc for the exposure
    for (int fi = 0; fi < nFrames; fi++) {
        const double T = t2 + (fi - 2) * Tc / nFrames;
        for (std::size_t i = 0; i < pix.size(); i++) img[i] = static_cast<float>(shade(pix[i], T, true, false, true, nullptr, -1));
        writeRaw(dir + "/strip" + std::to_string(fi) + ".f32", img);
        std::fprintf(meta, "strip%d %.6f\n", fi, T);
    }
    for (std::size_t i = 0; i < pix.size(); i++) img[i] = static_cast<float>(shade(pix[i], 0, false, false, true, nullptr, -1));
    writeRaw(dir + "/smooth.f32", img);
    for (std::size_t i = 0; i < pix.size(); i++) img[i] = static_cast<float>(pix[i].order);
    writeRaw(dir + "/order.f32", img);

    // the structure model: three looks at the same view
    Structure st;
    st.a = a; st.rIn = rIn; st.rOut = rOut;
    st.contrast = kStreakC;   // (gas.h: the face-on 6-10 M annulus varies by about SKH06 Table 2's 43.8 % rms, printed below)
    if (const char* e = std::getenv("PL_MG_CONTRAST")) st.contrast = std::atof(e);   // for trying other strengths by hand
    st.normalise(20000);
    std::fprintf(meta, "structNorm %.4f\n", st.norm);
    // its strength in numbers: the rms of a face-on 6-10 M annulus's light over time (SKH06 Table 2 lists 43.8 % rms above 10 Hz for its thermal model there)
    {
        std::vector<double> L;
        for (int i = 0; i < 200; i++) {
            const double t = 2000 + i * 4.0;
            double s = 0;
            for (double r = 6.1; r < 10; r += 0.2)
                for (int j = 0; j < 90; j++) s += discFlux(r, a) * r * st.factor(r, 2 * kPi * j / 90, t);
            L.push_back(s);
        }
        double m = 0, v = 0;
        for (double x : L) m += x;
        m /= L.size();
        for (double x : L) v += (x - m) * (x - m);
        v = std::sqrt(v / L.size()) / m;
        // local: one point's brightness over time
        double lm = 0, lv = 0; int ln = 0;
        for (int i = 0; i < 4000; i++) { const double x = st.factor(8.0, 0.0, 2000 + i * 0.5); lm += x; lv += x * x; ln++; }
        lm /= ln; lv = std::sqrt(lv / ln - lm * lm) / lm;
        std::printf("structure: contrast %.2f; a point at 8 M varies by %.0f %% rms; the face-on 6-10 M annulus by %.1f %% rms (SKH06 Table 2: 43.8 %%, thermal, > 10 Hz)\n",
                    st.contrast, 100 * lv, 100 * v);
        std::fprintf(meta, "contrast %.2f\nlocalRms %.4f\nannulusRms %.4f\n", st.contrast, lv, v);
    }
    const double tA = 10 * Tc, tB = tA + Tc / 2;
    for (int look = 0; look < 2; look++) {
        const double T = look ? tB : tA;
        for (std::size_t i = 0; i < pix.size(); i++) img[i] = static_cast<float>(shade(pix[i], T, true, false, true, &st, -1));
        writeRaw(dir + "/struct" + std::to_string(look) + ".f32", img);
        std::fprintf(meta, "struct%d %.6f\n", look, T);
    }
    std::fclose(meta);
    return 0;
}

int main(int argc, char** argv) {
    if (argc > 1 && std::strcmp(argv[1], "--check") == 0) return runChecks();
    if (argc > 1 && std::strcmp(argv[1], "--table") == 0) return runTable();
    if (argc > 2 && std::strcmp(argv[1], "--render") == 0) return runRender(argv[2]);
    std::printf("usage: movinggas --check | --table | --render <dir>\n");
    return 2;
}
