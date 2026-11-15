// Black Hole Lab (0.54.3) physics tests, written before the code (src/blackhole). Independent references only (plan section 5, native/PLAN_BLACKHOLE.md):
//  - special radii: the limits printed in Bardeen, Press & Teukolsky 1972 at a = 0 and a = M (p. 351), their root conditions, Page & Thorne's (15k) form, James et al. (A.6);
//  - the light path at a = 0: critical impact parameter and Luminet 1979's near-critical form (7) and weak limit (p. 229); the closed forms against a direct
//    quadrature of Luminet (3); the integrator (James et al. (A.15)) against the closed forms over the whole range of impact parameters, from several cameras;
//  - conservation laws along integrated rays (null condition, Carter's q) at zero and nonzero spin; a ray integrated back and forth returns to the camera;
//  - the shadow: Perlick & Tsupko's Synge formula (6), (7) and Zeldovich & Novikov's form (3); far away 3 sqrt(3) M / r (their (9), Gralla & Lupsasca 2020b (71));
//    half the sky at 3M (Riazuelo 2019, Fig. 20); the integrator's capture at the formula's edge; the sky map's edge on screen at 1280x800 and 3840x2160;
//  - the hovering camera's blueshift: Riazuelo 2019 Fig. 20 caption and (B19): -0.293, -0.423, -0.553 at 4M, 3M, 2.5M;
//  - repeated images: each further half turn thinner by e^-pi (Gralla & Lupsasca 2020b (43), (72): gamma = pi at a = 0; Luminet (7), (8));
//  - stars: two images on opposite sides, the primary outside the Einstein ring (James et al. section 3.1); magnification = ratio of solid angles by
//    neighbouring rays (Riazuelo (45)-(51)); light conserved over the whole sky;
//  - the flat limit: no hole gives exactly the night sky's own projection (orbit/sky.h skyDirection);
//  - the presets: the saved papers' tables read as received (data/blackhole/arxiv/*.txt.gz), the derived sizes, and agreement with orbit/sky_data.inc and far_masses.h.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "blackhole/elliptic.h"
#include "blackhole/geodesic.h"
#include "blackhole/kerr.h"
#include "blackhole/presets.h"
#include "blackhole/schwarzschild.h"
#include "orbit/far_masses.h"
#include "blackhole/lab.h"
#include "orbit/sky.h"
#include "orbit/units.h"

using namespace blackhole;

static int g_failed = 0, g_checks = 0;
static void check(bool ok, const std::string& what) {
    g_checks++;
    if (!ok) { g_failed++; std::printf("FAIL: %s\n", what.c_str()); }
}
static std::string f(double v) { char b[64]; std::snprintf(b, sizeof b, "%.10g", v); return b; }
static const double kPi = 3.14159265358979323846;

static std::string readBytes(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    std::stringstream s; s << in.rdbuf();
    return s.str();
}
// the extract of a saved paper (data/blackhole/arxiv/<paper>.extract.txt: the few lines of its pdftotext text that this test reads, copied exactly; the paper itself is in
// the private sources repo, native/data/SOURCES.md); CR removed (a Windows checkout may add them to text files)
static std::string readExtract(const std::string& rel) {
    std::string t = readBytes(std::string(PL_DATA_DIR) + "/" + rel);
    t.erase(std::remove(t.begin(), t.end(), '\r'), t.end());
    return t;
}
static std::string readSrc(const std::string& rel) {
    std::string t = readBytes(std::string(PL_SRC_DIR) + "/" + rel);
    t.erase(std::remove(t.begin(), t.end(), '\r'), t.end());
    return t;
}
static std::vector<double> numbersAfter(const std::string& t, const std::string& key, int n) {
    std::vector<double> v;
    const std::size_t at = t.find(key);
    if (at == std::string::npos) return v;
    const char* p = t.c_str() + at + key.size();
    while (static_cast<int>(v.size()) < n && *p && *p != '\n') {
        if ((*p >= '0' && *p <= '9') || (*p == '-' && p[1] >= '0' && p[1] <= '9')) { char* e; v.push_back(std::strtod(p, &e)); p = e; }
        else p++;
    }
    return v;
}

static double angleBetween(const Vec3& a, const Vec3& b) { return std::atan2(length(cross(a, b)), dot(a, b)); }

// ---------------------------------------------------------------------------------------------------------------------------------------------
static void testElliptic() {
    // identities of Legendre's F and K (no physics): F(phi, 0) = phi; F(phi, 1) = atanh(sin phi); K(0) = pi/2; F(pi/2, k) = K; F(phi + pi) = F(phi) + 2K
    for (double ph : {0.1, 0.7, 1.2, 1.5}) {
        check(std::fabs(ellF(ph, 0) - ph) < 1e-15, "F(phi,0) = phi at " + f(ph));
        check(std::fabs(ellF(ph, 1) - std::atanh(std::sin(ph))) < 1e-13 * std::atanh(std::sin(ph)), "F(phi,1) = atanh(sin phi) at " + f(ph));
    }
    check(std::fabs(ellK(0) - kPi / 2) < 1e-15, "K(0) = pi/2");
    for (double k : {0.3, 0.9, 0.999999}) {
        check(std::fabs(ellF(kPi / 2, k) - ellK(k)) < 1e-14 * ellK(k), "F(pi/2,k) = K(k), k " + f(k));
        check(std::fabs(ellF(1.0 + kPi, k) - ellF(1.0, k) - 2 * ellK(k)) < 1e-13 * ellK(k), "F(phi+pi) = F(phi) + 2K, k " + f(k));
        // a direct quadrature of the integral (Gauss-Legendre in 400 panels of 5 points)
        const double ph = 1.3, xg[5] = {-0.906179845938664, -0.538469310105683, 0, 0.538469310105683, 0.906179845938664},
                     wg[5] = {0.236926885056189, 0.478628670499366, 0.568888888888889, 0.478628670499366, 0.236926885056189};
        double q = 0;
        for (int i = 0; i < 400; i++) {
            const double a = ph * i / 400, b = ph * (i + 1) / 400;
            for (int j = 0; j < 5; j++) { const double t = 0.5 * (a + b) + 0.5 * (b - a) * xg[j]; q += 0.5 * (b - a) * wg[j] / std::sqrt(1 - k * k * std::sin(t) * std::sin(t)); }
        }
        check(std::fabs(q - ellF(ph, k)) < 1e-11 * q, "F(1.3,k) = quadrature, k " + f(k) + ": " + f(ellF(ph, k)) + " against " + f(q));
    }
}

// ---------------------------------------------------------------------------------------------------------------------------------------------
static void testRadii() {
    // BPT p. 351 (and p. 349 under (2.6)): the printed limits
    check(horizon(0) == 2 && horizon(1) == 1, "horizon 2M at a = 0, M at a = M (BPT (2.6))");
    check(std::fabs(staticLimit(1, kPi / 2) - 2) < 1e-15 && std::fabs(staticLimit(0.7, 0) - horizon(0.7)) < 1e-15, "static limit: 2M at the equator for a = M, the horizon at the pole (BPT (2.7))");
    check(std::fabs(photonOrbit(0, true) - 3) < 1e-14 && std::fabs(photonOrbit(0, false) - 3) < 1e-14, "photon orbit 3M at a = 0 (BPT (2.18))");
    check(std::fabs(photonOrbit(1, true) - 1) < 1e-14 && std::fabs(photonOrbit(1, false) - 4) < 1e-14, "photon orbit M and 4M at a = M (BPT (2.18))");
    check(std::fabs(marginallyBound(0, true) - 4) < 1e-14, "marginally bound 4M at a = 0 (BPT (2.19))");
    check(std::fabs(marginallyBound(1, true) - 1) < 1e-14 && std::fabs(marginallyBound(1, false) - 5.83) < 0.005, "marginally bound M and 5.83M at a = M (BPT (2.19), printed to 3 digits): " + f(marginallyBound(1, false)));
    check(std::fabs(innermostStable(0, true) - 6) < 1e-13 && std::fabs(innermostStable(0, false) - 6) < 1e-13, "innermost stable 6M at a = 0 (BPT (2.21))");
    check(std::fabs(innermostStable(1, true) - 1) < 1e-13 && std::fabs(innermostStable(1, false) - 9) < 1e-13, "innermost stable M and 9M at a = M (BPT (2.21))");
    for (double a : {0.0, 0.1, 0.3, 0.5, 0.7, 0.9, 0.99, 0.999}) {
        for (bool pro : {true, false}) {
            const std::string w = std::string(pro ? " prograde" : " retrograde") + " a " + f(a);
            const double rms = innermostStable(a, pro), rph = photonOrbit(a, pro), rmb = marginallyBound(a, pro);
            check(std::fabs(iscoCondition(rms, a, pro)) < 1e-12 * rms * rms, "r_ms is the root of r^2 - 6r +- 8a r^1/2 - 3a^2 (BPT p. 351)" + w);
            check(iscoCondition(rms * 1.01, a, pro) > 0, "orbits just outside r_ms are stable (BPT (2.20))" + w);
            check(std::fabs(photonCondition(rph, a, pro)) < 1e-12 * rph, "r_ph is the root of (2.17)" + w);
            check(std::fabs(iscoPageThorne(pro ? a : -a) - rms) < 1e-12 * rms, "Page & Thorne (15k) gives BPT's r_ms" + w);
            check(horizon(a) <= rph && rph < rmb && rmb < rms, "r+ <= r_ph < r_mb < r_ms (BPT section II)" + w);
        }
        // James et al. (A.6): r1 = 2{1 + cos[2/3 arccos(-a)]}, r2 = 2{1 + cos[2/3 arccos(+a)]} are BPT's prograde and retrograde photon orbits
        const double r1 = 2 * (1 + std::cos(2.0 / 3 * std::acos(-a))), r2 = 2 * (1 + std::cos(2.0 / 3 * std::acos(a)));
        check(std::fabs(r1 - photonOrbit(a, true)) < 1e-14 && std::fabs(r2 - photonOrbit(a, false)) < 1e-14, "J15 (A.6) = BPT (2.18), a " + f(a));
    }
}

// ---------------------------------------------------------------------------------------------------------------------------------------------
// Luminet (3) integrated directly: phi from u = 0 to u_end of du / sqrt(1/b^2 - u^2 + 2u^3) (M = 1), with u = u_end (1 - w^2) near a turning point
static double quadSweep(double b, double uEnd, bool turning) {
    const auto g = [&](double u) { return 1 / (b * b) - u * u + 2 * u * u * u; };
    const int N = 20000;
    double s = 0;
    for (int i = 0; i < N; i++) {                                  // midpoint in w, u = uEnd (1 - w^2) when it ends at a root, else plain midpoint in u
        const double w = (i + 0.5) / N;
        if (turning) { const double u = uEnd * (1 - w * w); s += 2 * uEnd * w / std::sqrt(g(u)) / N; }
        else { const double u = uEnd * w; s += uEnd / std::sqrt(g(u)) / N; }
    }
    return s;
}

static void testLightPath() {
    const double bc = criticalImpact();
    check(std::fabs(bc - 3 * std::sqrt(3.0)) < 1e-15, "b_c = 3 sqrt(3) M (Luminet p. 229, Gralla & Lupsasca 2020b (71))");
    std::printf("info: Luminet 1979 p. 229 prints b_c = 3 sqrt(3) M = 5.19695 M; 3 sqrt(3) = %.6f (the printed decimal is off in the 4th place)\n", bc);
    // the closed forms against Luminet (3) integrated directly
    for (double b : {5.3, 6.0, 10.0, 100.0}) {
        const double P = periastron(b), phiInf = (deflectionFar(b) + kPi) / 2;
        check(std::fabs(b * b - P * P * P / (P - 2)) < 1e-12 * b * b, "periastron solves b^2 = P^3 / (P - 2M) (Luminet (5)), b " + f(b));
        const double q = quadSweep(b, 1 / P, true);
        check(std::fabs(q - phiInf) < 2e-6 * phiInf, "Luminet (6) against a quadrature of (3), b " + f(b) + ": " + f(phiInf) + " against " + f(q));
    }
    for (double rO : {2.5, 3.0, 4.0, 20.0}) {
        for (double frac : {0.2, 0.6, 0.95}) {                     // outward rays with b < b_c: Gralla & Lupsasca case (3)
            const double b = frac * bc, psi = std::asin(b * std::sqrt(1 - 2 / rO) / rO);
            const Bend B = bendFromCamera(rO, psi);
            const double q = quadSweep(b, 1 / rO, false);
            check(!B.captured && std::fabs(B.dphi - q) < 1e-7 * q, "GL case (3) against a quadrature, rO " + f(rO) + " b " + f(b) + ": " + f(B.dphi) + " against " + f(q));
        }
    }
    // near-critical: b = b_c + 3.4823 M e^-mu (Luminet (7)); the coefficient is printed to 5 digits
    for (double d : {1e-4, 1e-6, 1e-8}) {
        const double mu = deflectionFar(bc + d), pred = 3.4823 * std::exp(-mu);
        check(std::fabs(pred - d) < 3e-5 * d + 10 * d * d, "b - b_c = 3.4823 e^-mu (Luminet (7)) at b - b_c = " + f(d) + ": " + f(pred));
    }
    // weak field: mu -> 4M/P (Luminet p. 229); the next term is of order (M/P)^2, so mu P / 4 - 1 is of order M/P
    double last = 1;
    for (double P : {1e3, 1e5, 1e7}) {
        const double b = std::sqrt(P * P * P / (P - 2)), mu = deflectionFar(b), e = std::fabs(mu * P / 4 - 1);
        check(e < 5 / P && e < last, "mu -> 4M/P far out (Luminet p. 229), P " + f(P) + ": mu P/4 - 1 = " + f(e));
        last = e;
    }
}

// a = 0 integrator against the closed form, from several cameras, in tilted planes
static void testIntegratorVsClosed() {
    KerrBL bl(0);
    double worst = 0;
    int n = 0;
    for (double rO : {2.2, 2.5, 3.0, 3.5, 6.0, 30.0, 1000.0}) {
        Camera cam; cam.r = rO;
        const double edge = kPi - shadowAngle(rO);
        for (int i = 0; i < 40; i++) {
            const double x = edge * std::pow(1e-4, i / 39.0);          // from psi = 0 to 1e-4 rad from the shadow's edge
            const double psi = edge - x, tilt = 0.37 + 0.9 * i;
            const Vec3 look{std::cos(psi), std::sin(psi) * std::sin(tilt), std::sin(psi) * std::cos(tilt)};   // (N_r, N_theta, N_phi)
            const RayEnd e = trace(bl, cam, look);
            const Bend B = bendFromCamera(rO, psi);
            if (e.kind != RayEnd::Sky || B.captured) { check(false, "a ray outside the shadow escapes, rO " + f(rO) + " psi " + f(psi)); continue; }
            // closed form in the camera's Cartesian frame at (r, pi/2, 0): e_r = x, e_theta = -z, e_phi = y
            const Vec3 er{1, 0, 0}, lk{look.x, look.z, -look.y};
            const Vec3 d = bentDirection(er, lk, look.x, B.dphi);
            const double ang = angleBetween(d, e.dir), tol = 2e-8 + 1e-11 / x;   // the closed form's rounding grows as 1/x near the edge (b - b_c ~ x)
            worst = std::max(worst, ang / tol);
            n++;
            if (ang > tol) check(false, "integrator = closed form, rO " + f(rO) + " psi " + f(psi) + " x " + f(x) + ": " + f(ang) + " rad apart");
        }
    }
    check(n == 280, "integrator against the closed form: all rays compared");
    std::printf("info: integrator against the closed form: %d rays, worst difference %.3g of its limit (2e-8 + 1e-11/x rad)\n", n, worst);
}

static void testCaptureEdge() {
    KerrBL bl(0);
    for (double rO : {2.2, 2.5, 3.0, 4.0, 10.0, 100.0}) {
        Camera cam; cam.r = rO;
        const double edge = kPi - shadowAngle(rO);
        for (double eps : {1e-3, 1e-6}) {
            const RayEnd in = trace(bl, cam, Vec3{std::cos(edge + eps), 0, std::sin(edge + eps)});
            const RayEnd out = trace(bl, cam, Vec3{std::cos(edge - eps), 0, std::sin(edge - eps)});
            check(in.kind == RayEnd::Horizon, "integrator: a ray " + f(eps) + " rad inside Synge's edge falls in, rO " + f(rO));
            check(out.kind == RayEnd::Sky, "integrator: a ray " + f(eps) + " rad outside Synge's edge escapes, rO " + f(rO));
        }
    }
    // far away: b_c from the integrator (a ray at b = b_c (1 -+ 1e-6) seen from 1e4 M)
    Camera far; far.r = 1e4;
    for (double s : {-1.0, 1.0}) {
        const double b = criticalImpact() * (1 + s * 1e-6), psi = kPi - std::asin(b * std::sqrt(1 - 2 / far.r) / far.r);
        const RayEnd e = trace(bl, far, Vec3{std::cos(psi), 0, std::sin(psi)});
        check(e.kind == (s < 0 ? RayEnd::Horizon : RayEnd::Sky), std::string("integrator: b = b_c ") + (s < 0 ? "- " : "+ ") + "1e-6 is " + (s < 0 ? "captured" : "not captured"));
    }
}

// ---------------------------------------------------------------------------------------------------------------------------------------------
static void testConservation() {
    for (double a : {0.0, 0.5, 0.9, 0.999}) {
        KerrBL bl(a);
        // the hand-written derivatives against finite differences of H (A.15)
        {
            const double b = 2.1, q = 7.3, y[5] = {5.5, 1.1, 0.3, 0.4, -1.2};
            Ray ray; ray.n = 5; ray.c[0] = b; ray.c[1] = q;
            double dy[5]; bl.rhs(ray, y, dy);
            const auto H = [&](int i, double h) { double z[5]; std::copy(y, y + 5, z); z[i] += h; return bl.hamiltonian(b, q, z); };
            const double h = 1e-6;
            const double dHr = (H(0, h) - H(0, -h)) / (2 * h), dHt = (H(1, h) - H(1, -h)) / (2 * h);
            const double dHpr = (H(3, h) - H(3, -h)) / (2 * h), dHpt = (H(4, h) - H(4, -h)) / (2 * h);
            const double dHb = (bl.hamiltonian(b + h, q, y) - bl.hamiltonian(b - h, q, y)) / (2 * h);
            check(std::fabs(dy[3] - dHr) < 1e-7 && std::fabs(dy[4] - dHt) < 1e-7, "dp/dzeta = dH/dx (A.15), a " + f(a));
            check(std::fabs(dy[0] + dHpr) < 1e-7 && std::fabs(dy[1] + dHpt) < 1e-7 && std::fabs(dy[2] + dHb) < 1e-7, "dx/dzeta = -dH/dp, dphi/dzeta = -dH/db (A.15), a " + f(a));
        }
        double worstH = 0, worstQ = 0, worstBack = 0;
        int rays = 0;
        for (double r : {4.0, 8.0, 30.0})
            for (double th : {0.6, 1.2, kPi / 2})
                for (int k = 0; k < 6; k++) {
                    Camera cam; cam.r = r; cam.theta = th;
                    const double u = 0.3 + 1.1 * k, v = 2.0 + 0.7 * k;
                    const Vec3 look{std::cos(u), std::sin(u) * std::cos(v), std::sin(u) * std::sin(v)};
                    std::vector<double> path;
                    Ray ray;
                    if (!bl.launch(cam, look, ray)) continue;
                    trace(bl, cam, look, {}, &path);
                    for (std::size_t i = 0; i + 5 <= path.size(); i += 5) {
                        if (path[i] > 1e4) break;
                        worstH = std::max(worstH, std::fabs(bl.nullCondition(ray, &path[i])));
                        worstQ = std::max(worstQ, std::fabs(bl.carterQ(ray.c[0], &path[i]) - ray.c[1]) / std::max(1.0, std::fabs(ray.c[1])));
                    }
                    // integrate back 30 M of zeta-time and forward again: back at the camera
                    Ray r2 = ray;
                    integrateSpan(bl, r2, -std::min(30.0, 0.5 * r), {});
                    integrateSpan(bl, r2, std::min(30.0, 0.5 * r), {});
                    worstBack = std::max({worstBack, std::fabs(r2.y[0] - ray.y[0]) / ray.y[0], std::fabs(r2.y[1] - ray.y[1]), std::fabs(r2.y[2] - ray.y[2])});
                    rays++;
                }
        check(rays == 54, "conservation: all rays launched, a " + f(a));
        check(worstH < 1e-9, "null condition along the rays (A.15), a " + f(a) + ": worst " + f(worstH));
        check(worstQ < 1e-9, "Carter's q from (theta, p_theta) stays the ray's q (A.12), a " + f(a) + ": worst " + f(worstQ));
        check(worstBack < 1e-8, "a ray integrated back and forward returns to the camera, a " + f(a) + ": worst " + f(worstBack));
    }
}

// ---------------------------------------------------------------------------------------------------------------------------------------------
static void testShadow() {
    const double bc = criticalImpact();
    for (double r : {1e4, 1e6, 1e8}) check(std::fabs(shadowAngle(r) * r / bc - 1) < 2 / r, "far away alpha_sh -> 3 sqrt(3) M / r (Perlick & Tsupko (9)), r " + f(r));
    check(std::fabs(shadowAngle(3) - kPi / 2) < 1e-15, "at the photon orbit the shadow is exactly half the sky (Riazuelo Fig. 20; Perlick & Tsupko (7))");
    for (double r : {2.01, 2.3, 2.9, 3.1, 4.0, 6.0, 30.0, 1e3}) {
        const double a = shadowAngle(r), zn = shadowAngleZN(r);
        check(std::fabs(a - zn) < 1e-13, "Synge (6), (7) = Zeldovich-Novikov (3), r " + f(r));
        check(std::fabs(std::sin(a) - bc * std::sqrt(1 - 2 / r) / r) < 1e-15 && (r < 3) == (a > kPi / 2), "Synge's sine and branch (Perlick & Tsupko (6), (7)), r " + f(r));
    }
    // the sky map's edge on screen: the hole at the middle of the picture, its edge at alpha_sh K pixels from the middle (K = hypot(W, H) / 2 / 1.75, sky.cpp)
    for (int sz = 0; sz < 2; sz++) {
        const double W = sz ? 3840 : 1280, H = sz ? 2160 : 800, K = std::hypot(W, H) / 2 / 1.75;
        for (double rO : {2.8, 3.0, 27.0, 1000.0}) {                 // (inside about 2.75M the shadow reaches past the map's 100 degrees)
            LensTable T(rO);
            Vec3 mid;
            orbit::skyDirection(W / 2, H / 2, W, H, 1, 0, 1, 0, mid);
            LensView V(T, mid, W, H, 1, 0, 1, 0);
            // along the diagonal from the middle to a corner the angle from the middle is t * 1.75 rad
            double lo = 0, hi = 0.99999;                          // shadow at lo, sky at hi
            Vec3 d;
            const auto at = [&](double t) { return V.pixel(W / 2 + t * W / 2, H / 2 + t * H / 2, d); };
            check(at(lo) == 1 && at(hi) == 0, "the middle is shadow and the corner sky, rO " + f(rO));
            for (int i = 0; i < 100; i++) { const double m = 0.5 * (lo + hi); (at(m) == 1 ? lo : hi) = m; }
            const double err = (lo * 1.75 - shadowAngle(rO)) * K;
            check(std::fabs(err) < 1e-6, "the map's shadow edge is at Synge's angle to 1e-6 pixel, " + f(W) + "x" + f(H) + " rO " + f(rO) + ": " + f(err));
        }
    }
}

static void testShift() {
    // Riazuelo Fig. 20 caption: z_grav ~ -0.293, -0.423, -0.553 at r = 4M, 3M, 2.5M ((B19): z = -1 + (1 - 2M/r)^(1/2)); f_c/f' = 1/(1 + z)
    const double r[3] = {4, 3, 2.5}, z[3] = {-0.293, -0.423, -0.553};
    for (int i = 0; i < 3; i++) {
        const double s = hoverShift(0, r[i], kPi / 2, 1.7);
        check(std::fabs((1 / s - 1) - z[i]) < 5e-4, "hovering blueshift at " + f(r[i]) + "M (Riazuelo Fig. 20): z = " + f(1 / s - 1));
        LensTable T(r[i]);
        Vec3 h{1, 0, 0};
        LensView V(T, h, 100, 100, 1, 0, 1, 0);
        check(std::fabs(V.shift() - s) < 1e-15, "the sky map returns the camera's shift apart, r " + f(r[i]));
    }
}

// ---------------------------------------------------------------------------------------------------------------------------------------------
static void testRepeatedImages() {
    for (double rO : {10.0, 1000.0}) {
        LensTable T(rO);
        const Vec3 hole{0, 0, -1};
        // a small patch of sky across chi = pi/2: each image is the band between the images of its two edges; widths in psi
        std::vector<StarImage> a, b;
        const double chi0 = kPi / 2 - 0.005, chi1 = kPi / 2 + 0.005;
        starImages(T, hole, Vec3{std::sin(chi0), 0, std::cos(chi0)}, 0, 1e9, a);
        starImages(T, hole, Vec3{std::sin(chi1), 0, std::cos(chi1)}, 0, 1e9, b);
        check(a.size() >= 6 && b.size() >= 6, "at least six images of each edge, rO " + f(rO));
        if (a.size() < 6 || b.size() < 6) continue;
        for (int j = 3; j <= 4; j++) {
            const double wj = std::fabs(a[j].psi - b[j].psi), wn = std::fabs(a[j + 1].psi - b[j + 1].psi), ratio = wn / wj;
            check(std::fabs(ratio / std::exp(-kPi) - 1) < 2e-3, "each further image thinner by e^-pi (Gralla & Lupsasca 2020b (43), (72)), rO " + f(rO) + " image " + f(j + 1) + ": ratio " + f(ratio));
        }
    }
    // a patch elsewhere: a full turn later (same side) thinner by e^-2pi (Luminet (7), (8)); the law holds as b -> b_c, so images 4 and 6 (b - b_c ~ 1e-5),
    // from cameras near enough that image 6 lies more than 1e-9 rad from the edge (the closed form's floor)
    for (double rO : {6.0, 10.0}) {
        LensTable T(rO);
        const Vec3 hole{0, 0, -1};
        std::vector<StarImage> c, d;
        starImages(T, hole, Vec3{std::sin(2.0), 0, std::cos(2.0)}, 0, 1e9, c);
        starImages(T, hole, Vec3{std::sin(2.01), 0, std::cos(2.01)}, 0, 1e9, d);
        if (c.size() >= 7 && d.size() >= 7) {
            const double ratio = std::fabs(c[6].psi - d[6].psi) / std::fabs(c[4].psi - d[4].psi);
            check(std::fabs(ratio / std::exp(-2 * kPi) - 1) < 2e-3, "one more turn: thinner by e^-2pi (Luminet (8)), rO " + f(rO) + ": " + f(ratio));
        } else check(false, "seven images of a star at chi = 2, rO " + f(rO));
    }
}

static void testStars() {
    for (double rO : {2.6, 6.0, 27.0, 300.0}) {
        LensTable T(rO);
        const Vec3 hole{0.6, 0.8, 0};
        const Vec3 er = hole * -1.0;
        // the Einstein ring: dphi = pi
        double lo = 0, hi = T.edge();
        for (int i = 0; i < 200; i++) { const double m = 0.5 * (lo + hi); (bendFromCamera(rO, m).dphi < kPi ? lo : hi) = m; }
        const double ring = lo;
        for (double chi : {0.5, 2.0, 2.9, 3.1}) {
            const Vec3 side{0, 0, 1};
            const Vec3 s = er * std::cos(chi) + side * std::sin(chi);
            std::vector<StarImage> im;
            starImages(T, hole, s, 0, 1e9, im);
            check(im.size() >= 2, "a star has at least two images, rO " + f(rO) + " chi " + f(chi));
            if (im.size() < 2) continue;
            const double s0 = dot(im[0].dir, side), s1 = dot(im[1].dir, side);
            check(s0 > 0 && s1 < 0, "primary and secondary on opposite sides of the hole (James et al. 3.1), rO " + f(rO) + " chi " + f(chi));
            check(im[0].psi < ring && im[1].psi > ring, "the primary outside the Einstein ring, the secondary inside, rO " + f(rO) + " chi " + f(chi));
            // every image maps back to the star (closed form)
            for (const auto& g : im) {
                const Bend B = bendFromCamera(rO, g.psi);
                const Vec3 d = bentDirection(er, g.dir, std::cos(g.psi), B.dphi);
                if (angleBetween(d, s) > 1e-9 * std::max(1.0, 1e-6 / (T.edge() - g.psi))) check(false, "image " + f(g.order) + " maps back onto its star, rO " + f(rO) + ": " + f(angleBetween(d, s)));
            }
            // the magnification against Riazuelo (45)-(51): the solid angles of three neighbouring rays, before and after
            for (std::size_t j = 0; j < std::min<std::size_t>(im.size(), 4); j++) {
                const Vec3 n = im[j].dir, n0 = std::fabs(n.x) < 0.5 ? Vec3{1, 0, 0} : Vec3{0, 0, 1};
                const Vec3 p1 = cross(n, n0) * (1 / length(cross(n, n0))), p2 = cross(n, p1);
                const double dl = 1e-3 * std::min(0.1, T.edge() - im[j].psi);   // central differences: error (dl / x)^2
                const auto map = [&](const Vec3& v0) {
                    const Vec3 v = v0 * (1 / length(v0));
                    const double c = dot(v, er), ps = std::atan2(length(cross(v, er)), c);
                    return bentDirection(er, v, c, bendFromCamera(rO, ps).dphi);
                };
                const Vec3 a1 = map(n + p1 * dl), b1 = map(n - p1 * dl), a2 = map(n + p2 * dl), b2 = map(n - p2 * dl);
                const double Om = dl * dl;                                                   // n . (dl p1 x dl p2)
                const double OmS = std::fabs(dot(map(n), cross(a1 - b1, a2 - b2))) / 4;
                const double fNum = Om / OmS;
                check(std::fabs(fNum / im[j].magnification - 1) < 1e-4, "magnification = ratio of solid angles (Riazuelo (51)), rO " + f(rO) + " chi " + f(chi) + " image " + f(static_cast<double>(j)) + ": " + f(im[j].magnification) + " against " + f(fNum));
            }
        }
        // light conserved: the images of the whole sky cover the camera's sky outside the shadow, Sum_j Int f_j dOmega* = 4 pi - 2 pi (1 - cos alpha_sh)
        const int N = 3000;
        double sum = 0;
        for (int i = 0; i < N; i++) {
            const double chi = kPi * (i + 0.5) / N;
            std::vector<StarImage> im;
            starImages(T, hole, er * std::cos(chi) + Vec3{0, 0, 1} * std::sin(chi), 0, 1e9, im);
            double fs = 0;
            for (const auto& g : im) fs += g.magnification;
            sum += 2 * kPi * std::sin(chi) * fs * kPi / N;
        }
        const double want = 4 * kPi - 2 * kPi * (1 - std::cos(shadowAngle(rO)));
        check(std::fabs(sum / want - 1) < 1e-4, "light conserved: the bent sky covers the sky outside the shadow, rO " + f(rO) + ": " + f(sum) + " against " + f(want));
    }
    // the images and the sky map agree: the map at an image's pixel looks at its star (to the table's accuracy)
    {
        const double W = 1280, H = 800;
        Vec3 mid; orbit::skyDirection(W / 2, H / 2, W, H, 1, 0, 1, 0, mid);
        LensTable T(27);
        LensView V(T, mid, W, H, 1, 0, 1, 0);
        const Vec3 er = mid * -1.0, side{0, 0, 1};
        double worst = 0;
        for (double chi : {2.9, 3.0, 3.1}) {
            const Vec3 s = er * std::cos(chi) + side * std::sin(chi);
            std::vector<StarImage> im;
            starImages(T, mid, s, 0, 1e9, im);
            for (std::size_t j = 0; j < std::min<std::size_t>(im.size(), 3); j++) {
                double X, Y; Vec3 d;
                if (!orbit::skyScreen(im[j].dir, W, H, 1, 0, 1, 0, X, Y) || V.pixel(X, Y, d) != 0) { check(false, "image on the map"); continue; }
                worst = std::max(worst, angleBetween(d, s) * std::exp(-kPi * static_cast<double>(j)));   // a further image is magnified less: its sky moves e^pi more per pixel
            }
        }
        check(worst < 1e-6, "the sky map at a star's image looks at the star: worst " + f(worst) + " rad (scaled by e^-pi per image)");
    }
}

static void testFlatLimit() {
    const double W = 1280, H = 800, ca = std::cos(0.4), sa = std::sin(0.4), cb = std::cos(-0.3), sb = std::sin(-0.3);
    LensTable none(1e301);
    Vec3 hole; orbit::skyDirection(700, 300, W, H, ca, sa, cb, sb, hole);
    LensView V(none, hole, W, H, ca, sa, cb, sb);
    LensTable farT(1e9);
    LensView F(farT, hole, W, H, ca, sa, cb, sb);
    const double K = std::hypot(W, H) / 2 / 1.75;
    bool same = true;
    double worst = 0;
    for (int y = 0; y < 800; y += 7)
        for (int x = 0; x < 1280; x += 7) {
            Vec3 a, b, c;
            const bool ok = orbit::skyDirection(x + 0.5, y + 0.5, W, H, ca, sa, cb, sb, a);
            const int k = V.pixel(x + 0.5, y + 0.5, b);
            if (!ok || k != 0 || a.x != b.x || a.y != b.y || a.z != b.z) same = false;
            if (std::hypot(x + 0.5 - 700, y + 0.5 - 300) > 5 && F.pixel(x + 0.5, y + 0.5, c) == 0) worst = std::max(worst, angleBetween(a, c) * K);
        }
    check(same, "no hole (mass zero): the map is exactly the night sky's own projection (orbit/sky.h skyDirection)");
    check(worst < 1e-3, "a camera 1e9 M away: the map within 1e-3 pixel of the unbent sky beyond 5 pixels from the hole: " + f(worst));
}

// ---------------------------------------------------------------------------------------------------------------------------------------------
static void testPresets() {
    const std::string g22 = readExtract("blackhole/arxiv/2112.07478_gravity2022.extract.txt");
    const std::string sgr = readExtract("blackhole/arxiv/2311.08680_eht_sgra_I.extract.txt");
    std::string m87 = readExtract("blackhole/arxiv/1906.11238_eht_m87_I.extract.txt");
    for (std::size_t p; (p = m87.find("\xef\x82\xa0")) != std::string::npos;) m87.erase(p, 3);   // the PDF's private-use spaces (U+F0A0) around the +- and x signs
    check(!g22.empty() && !sgr.empty() && !m87.empty(), "the saved papers' text can be read");
    const Preset* S = preset("sgra");
    const Preset* M = preset("m87");
    check(S && M, "presets sgra and m87");
    if (!S || !M) return;
    // GRAVITY 2022 Table B.1, the "this paper" columns
    const auto tb = g22.find("Table B.1: Best-fit orbit parameters");
    const std::string t = tb == std::string::npos ? "" : g22.substr(tb);
    const auto mrow = numbersAfter(t, "M\xe2\x80\xa2 [106 M ]", 2), rrow = numbersAfter(t, "R0 [pc]", 2);
    check(mrow.size() == 2 && mrow[0] * 1e6 == S->massSun && mrow[1] * 1e6 == S->massErr, "Sgr A* mass = GRAVITY 2022 Table B.1");
    check(rrow.size() == 2 && rrow[0] == S->distPc && rrow[1] == S->distErr, "Sgr A* distance = GRAVITY 2022 Table B.1");
    // EHT Sgr A* paper I Table 1
    check(numbersAfter(sgr, "Diameter, d", 2) == std::vector<double>{S->ringMuas, S->ringErr}, "Sgr A* ring 51.8 +- 2.3 microarcsec (EHT 2022 Table 1)");
    const auto th = numbersAfter(sgr, "Stellar orbits (VLTI)d", 3);
    check(th.size() == 3 && th[0] == S->thetaGMuas && th[1] == S->thetaGErr, "Sgr A* theta_g 5.125 +- 0.009 microarcsec (EHT 2022 Table 1)");
    // EHT M87* paper I Table 1
    check(m87.find("(6.5\xc2\xb1" "0.7)\xc3\x97" "109") != std::string::npos && M->massSun == 6.5e9 && M->massErr == 0.7e9, "M87* mass (6.5 +- 0.7) x 10^9 (EHT 2019 Table 1)");
    check(m87.find("(16.8\xc2\xb1" "0.8) Mpc") != std::string::npos && M->distPc == 16.8e6 && M->distErr == 0.8e6, "M87* distance 16.8 +- 0.8 Mpc (EHT 2019 Table 1)");
    check(m87.find("42\xc2\xb1" "3 \xce\xbc" "as") != std::string::npos && M->ringMuas == 42 && M->ringErr == 3, "M87* ring 42 +- 3 microarcsec (EHT 2019 Table 1)");
    check(m87.find("3.8\xc2\xb1" "0.4 \xce\xbc" "as") != std::string::npos && M->thetaGMuas == 3.8 && M->thetaGErr == 0.4, "M87* theta_g 3.8 +- 0.4 microarcsec (EHT 2019 Table 1)");
    // derived sizes
    for (const Preset* p : {S, M}) {
        const double rg = gravRadiusM(p->massSun);
        check(std::fabs(rg - p->massSun * 1.32712440041279419e20 / (299792458.0 * 299792458.0)) < 1e-9 * rg, std::string(p->name) + ": r_g = GM/c^2");
        check(std::fabs(horizonKm(*p) - 2 * rg / 1000) < 1e-9 * rg, std::string(p->name) + ": horizon 2 r_g");
        const double tg = thetaGMuas(*p);
        check(std::fabs(tg - p->thetaGMuas) < (p == S ? 0.009 + 0.020 : 0.4), std::string(p->name) + ": GM/(D c^2) from the mass and distance = the published theta_g within its error: " + f(tg));
        const double sh = shadowDiameterMuas(*p), flat = 2 * std::sqrt(27.0) * tg;
        check(std::fabs(sh / flat - 1) < 1e-9, std::string(p->name) + ": shadow diameter = 2 sqrt(27) theta_g far away");
        std::printf("info: %s: horizon (a = 0) %.4g km = %.4g AU; shadow seen from Earth %.1f microarcsec (2 sqrt(27) x the published theta_g %.3g: %.1f); measured ring %.1f +- %.1f (1.3 mm)\n",
                    p->name, horizonKm(*p), horizonAU(*p), sh, p->thetaGMuas, 2 * std::sqrt(27.0) * p->thetaGMuas, p->ringMuas, p->ringErr);
    }
    // the same Sgr A* elsewhere in the app: sky_data.inc's 26996 light years and far_masses.h's 4.30 million Suns
    const std::string inc = readSrc("orbit/sky_data.inc");
    const auto row = numbersAfter(inc, "{\"sgra\", \"Sagittarius A*\", \"bh\",", 3);
    const double ly = S->distPc * parsecM() / (units::kLightYearAU * units::kAU);
    check(row.size() == 3 && std::fabs(row[2] - ly) < 0.5, "sky_data.inc's Sagittarius A* distance = 8277 pc in light years, rounded: " + f(ly));
    bool massOk = false;
    for (const auto& m : orbit::kFarMass) if (std::strcmp(m.key, "sgra") == 0) massOk = std::fabs(m.value - std::round(S->massSun / 1e4) * 1e4) < 1;
    check(massOk, "far_masses.h's Sagittarius A* mass = 4.297 million rounded to 4.30 million");
}

// ---------------------------------------------------------------------------------------------------------------------------------------------
// 0.55.0: what the lab adds to the module
// A camera exactly at the photon orbit: b_c - b ~ x^2 there, so the closed form must take b^2 - 27 from b - b_c (0.54.3's table was off by 1e-2 rad
// within 1e-6 rad of the edge). References: the ray integrator (an independent method) down to 1e-6 rad, and the table against the closed form.
static void testPhotonOrbitCamera() {
    KerrBL bl(0);
    Camera cam; cam.r = 3;
    const double edge = kPi - shadowAngle(3.0);
    double worst = 0;
    for (double x : {1e-4, 1e-5, 1e-6}) {
        const double psi = edge - x;
        const Vec3 look{std::cos(psi), 0, std::sin(psi)};
        const RayEnd e = trace(bl, cam, look);
        const Bend B = bendFromCamera(3.0, psi);
        if (e.kind != RayEnd::Sky || B.captured) { check(false, "rO 3: a ray just outside the edge escapes, x " + f(x)); continue; }
        const Vec3 d = bentDirection({1, 0, 0}, {look.x, look.z, -look.y}, look.x, B.dphi);
        const double ang = angleBetween(d, e.dir);
        worst = std::max(worst, ang);
        check(ang < 1e-6, "rO 3M: closed form = integrator at x " + f(x) + ": " + f(ang) + " rad apart");
    }
    LensTable T(3.0);
    double tw = 0;
    for (int i = 0; i <= 4000; i++) {
        const double x = edge * std::pow(1e-8 / edge, i / 4000.0);
        double dl;
        T.lookupX(x, dl);
        tw = std::max(tw, std::fabs(dl - bendFromCameraEdge(3.0, x).dphi));
    }
    check(tw < 1e-6, "rO 3M: the table = the closed form down to 1e-8 rad from the edge: " + f(tw));
    std::printf("info: camera at the photon orbit: integrator against closed form %.2e rad (x 1e-4..1e-6); table against closed form %.2e rad (x down to 1e-8)\n", worst, tw);
}

// The lab's view: with the middle of the picture and the night sky's scale it is orbit/sky.h's projection; lensPixel is LensView::pixel; round trips.
static void testHoleView() {
    const double W = 1280, H = 800, K0 = std::hypot(W, H) / 2 / 1.75;
    HoleView v; v.cx = W / 2; v.cy = H / 2; v.K = K0;
    const double yaw = 0.7, pitch = 1.9;
    v.ca = std::cos(yaw); v.sa = std::sin(yaw); v.cb = std::cos(pitch); v.sb = std::sin(pitch);
    double wd = 0, ws = 0, wl = 0;
    int bad = 0;
    LensTable T(27);
    const Vec3 hole = viewMiddle(v);
    Vec3 mid; orbit::skyDirection(W / 2, H / 2, W, H, v.ca, v.sa, v.cb, v.sb, mid);
    check(angleBetween(mid, hole) < 1e-12, "viewMiddle is the night sky's middle");
    LensView LV(T, hole, W, H, v.ca, v.sa, v.cb, v.sb);
    for (int i = 0; i < 2000; i++) {
        const double X = std::fmod(i * 317.31, W), Y = std::fmod(i * 191.77, H);
        Vec3 a, b;
        if (!orbit::skyDirection(X, Y, W, H, v.ca, v.sa, v.cb, v.sb, a)) continue;
        if (!viewDirection(v, X, Y, b)) { bad++; continue; }
        wd = std::max(wd, angleBetween(a, b));
        double X2, Y2;
        if (!viewScreen(v, b, X2, Y2)) { bad++; continue; }
        ws = std::max(ws, std::hypot(X2 - X, Y2 - Y));
        Vec3 da, db;
        const int ka = LV.pixel(X, Y, da), kb = lensPixel(T, v, X, Y, db);
        const double th = std::hypot(X - W / 2, Y - H / 2) / K0, x = th - shadowAngle(27);
        if (std::fabs(x) < 1e-9) continue;
        if (ka != kb) { bad++; continue; }
        if (ka == 0) wl = std::max(wl, angleBetween(da, db) * std::min(1.0, x / 1e-3));
    }
    check(bad == 0, "the lab's view agrees with the night sky's projection (" + f(bad) + " disagree)");
    check(wd < 1e-12, "viewDirection = orbit::skyDirection: " + f(wd));
    check(ws < 1e-9, "viewScreen(viewDirection) round trip, pixels: " + f(ws));
    check(wl < 1e-9, "lensPixel = LensView::pixel (scaled by x / 1e-3 rad near the edge, where psi's rounding counts): " + f(wl));
    // up to pi from the middle, any scale: round trip
    v.K = 200; double wr = 0;
    for (int i = 0; i < 2000; i++) {
        const double X = W / 2 + 610 * std::cos(i * 0.71) * (i % 97) / 96.0, Y = H / 2 + 610 * std::sin(i * 0.71) * (i % 89) / 88.0;
        Vec3 d; double X2, Y2;
        if (!viewDirection(v, X, Y, d)) continue;
        if (std::hypot(X - W / 2, Y - H / 2) / v.K > kPi - 1e-3) continue;
        if (viewScreen(v, d, X2, Y2)) wr = std::max(wr, std::hypot(X2 - X, Y2 - Y)); else wr = 1e9;
    }
    check(wr < 1e-6, "viewScreen(viewDirection) up to pi from the middle: " + f(wr));
}

// The fast star images against the closed-form ones (starImages, tested against Riazuelo above): same images, same places, same magnifications.
static void testStarsFast() {
    const orbit::Sky& S = orbit::skyData();
    const Vec3 hole = S.galCentre;
    for (double rO : {2.5, 3.0, 27.0, 1000.0}) {
        LensTable T(rO);
        int n = 0, nimg = 0, countBad = 0;
        double wp = 0, wm = 0;
        for (std::size_t k = 0; k < S.stars.size(); k += 7) {
            std::vector<StarImage> a, b;
            starImages(T, hole, S.stars[k].dir, S.stars[k].mag, kImageMagLimit, a);
            starImagesFast(T, hole, S.stars[k].dir, S.stars[k].mag, kImageMagLimit, b);
            n++;
            if (a.size() != b.size()) {                                       // an image right at the magnitude cut may fall either side
                const StarImage& last = a.size() > b.size() ? a.back() : b.back();
                if (std::fabs(last.mag - kImageMagLimit) > 1e-3) countBad++;
            }
            for (std::size_t j = 0; j < std::min(a.size(), b.size()); j++) {
                nimg++;
                wp = std::max(wp, angleBetween(a[j].dir, b[j].dir));
                wm = std::max(wm, std::fabs(b[j].magnification / a[j].magnification - 1));
            }
        }
        check(countBad == 0, "fast star images: the same images as starImages, rO " + f(rO) + " (" + f(countBad) + " stars differ)");
        check(wp < 1e-7, "fast star images: places within 1e-7 rad of starImages, rO " + f(rO) + ": " + f(wp));
        check(wm < 1e-4, "fast star images: magnifications within 1e-4 of starImages, rO " + f(rO) + ": " + f(wm));
        std::printf("info: fast star images rO %g M: %d stars, %d images, worst place %.2e rad, worst magnification %.2e (relative)\n", rO, n, nimg, wp, wm);
    }
}

// The readout's camera numbers. Riazuelo 2019 Fig. 20 caption: z_grav ~ -0.293, -0.423, -0.553 at 4M, 3M, 2.5M (as testShift); the clock runs at 1 + z
// (the lapse), the light arrives (1 + z)^-1 bluer and (1 + z)^-4 brighter. Half the sky at the photon orbit (Riazuelo Fig. 20; Perlick & Tsupko (7): 90 degrees).
static void testCameraNumbers() {
    const double r[3] = {4, 3, 2.5}, z[3] = {-0.293, -0.423, -0.553};
    for (int i = 0; i < 3; i++) {
        check(std::fabs(hoverClockRate(r[i]) - (1 + z[i])) < 6e-4, "clock rate = 1 + z_grav (Riazuelo Fig. 20) at " + f(r[i]) + ": " + f(hoverClockRate(r[i])));
        check(std::fabs(hoverBlueshift(r[i]) * (1 + z[i]) - 1) < 1.5e-3, "blueshift = 1 / (1 + z) at " + f(r[i]));
        check(std::fabs(hoverBrightening(r[i]) * std::pow(1 + z[i], 4) - 1) < 6e-3, "brightening = (1 + z)^-4 at " + f(r[i]));
    }
    check(std::fabs(shadowSkyFraction(3) - 0.5) < 1e-12, "at the photon orbit the shadow covers half the sky");
    check(shadowSkyFraction(2.5) > 0.5 && shadowSkyFraction(4) < 0.5, "inside 3M more than half, outside less");
}

// The lab's settings: round trip, fallbacks, presets from presets.h, sliders.
static void testLabSettings() {
    Settings d = settingsDefaults();
    check(d.preset == "sgra" && d.massSun == preset("sgra")->massSun && d.dist == kDistDefault && d.bend && d.skyOurs, "defaults: Sagittarius A*, bending on, our sky");
    Vec3 mid = viewMiddle(holeView(d, 1280, 800, 640, 400));
    check(angleBetween(mid, orbit::skyData().galCentre) < 1e-12, "the default view looks at the galactic centre");
    Settings s = d;
    applyPreset(s, "m87"); setDistance(s, 3.7); setFov(s, 133); turn(s, 0.3, -0.2); s.bend = false; s.skyOurs = false;
    std::map<std::string, std::string> m;
    saveSettings(s, m);
    check(loadSettings(m) == s, "settings round trip (M87*, 3.7 M, 133 degrees, turned, bending off, black sky)");
    setMass(s, 12345); saveSettings(s, m);
    const Settings t = loadSettings(m);
    check(t == s && t.preset.empty() && t.massSun == 12345, "a mass of one's own drops the preset and survives a restart");
    std::map<std::string, std::string> bad{{"preset", "vega"}, {"mass", "-5"}, {"dist", "1.5"}, {"fov", "abc"}, {"pitch", "9"}, {"bend", "maybe"}, {"sky", "red"}, {"zzz", "1"}};
    check(loadSettings(bad) == settingsDefaults(), "bad values fall back to the defaults");
    check(loadSettings({}) == settingsDefaults(), "no lines: the defaults");
    for (double v : {2.2, 3.0, 27.0, 1000.0}) check(std::fabs(sliderToDist(distToSlider(v)) / v - 1) < 1e-12, "distance slider round trip " + f(v));
    for (double v : {1.0, 4.297e6, 1e11}) check(std::fabs(sliderToMass(massToSlider(v)) / v - 1) < 1e-12, "mass slider round trip " + f(v));
    // the readout is the module's
    Settings q = d; setDistance(q, 27);
    const Readout r = readout(q);
    check(r.hasPreset && r.presetName == "Sagittarius A*" && r.earthShadowMuas == shadowDiameterMuas(*preset("sgra")) && r.ringMuas == 51.8 && r.ringErr == 2.3, "readout: the preset's numbers");
    check(std::fabs(r.shadowDeg - 2 * shadowAngle(27) * 180 / kPi) < 1e-12 && std::fabs(r.horizonKm - horizonKm(*preset("sgra"))) < 1e-6 * r.horizonKm, "readout: shadow and horizon from the module");
    check(std::fabs(r.distKm - 27 * gravRadiusM(4.297e6) / 1000) < 1e-6 * r.distKm, "readout: distance in km");
    // the field of view: the height spans it, and no corner is ever past pi
    HoleView hv = holeView(q, 1280, 800, 640, 400);
    check(std::fabs(400 / hv.K - 90 * kPi / 360) < 1e-12, "the field of view spans the height");
    setFov(q, 360); hv = holeView(q, 1280, 800, 640, 400);
    check(std::hypot(640.0, 400.0) / hv.K <= kPi + 1e-12, "a wide field of view keeps the corners within pi");
}

int main() {
    testElliptic();
    testRadii();
    testLightPath();
    testIntegratorVsClosed();
    testCaptureEdge();
    testConservation();
    testShadow();
    testShift();
    testRepeatedImages();
    testStars();
    testFlatLimit();
    testPresets();
    testPhotonOrbitCamera();
    testHoleView();
    testStarsFast();
    testCameraNumbers();
    testLabSettings();
    std::printf("%d checks, %d failed\n", g_checks, g_failed);
    return g_failed ? 1 : 0;
}
