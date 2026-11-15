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

#include "blackhole/disc.h"
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

// ---------------------------------------------------------------------------------------------------------------------------------------------
// 0.55.1: the thin gas disc (src/blackhole/disc.h). References: Page & Thorne 1974 (PT), Luminet 1979 (L79), Bardeen, Press & Teukolsky 1972 (BPT),
// Abramowicz & Fragile 2013 (AF, its extract), Yuan & Narayan 2014 (YN, its extract), Cunningham 1975 (C75), Riazuelo 2019 (53), the ray integrator (geodesic.h).
// The numbers typed below were read from the page images of the scans (the private sources repo, data/blackhole/scans) at the place named.

// AF section 5.3 (97)-(99): F = [7e26] (mdot/m) r*^-3 B^-1 C^-1/2 Q with the functions under (99) (y = (r/M)^(1/2)); returns r*^-3 B^-1 C^-1/2 Q
static double afShape(double r, double a) {
    const double y = std::sqrt(r), y0 = std::sqrt(discInner(a));
    const double B = 1 + a / (y * y * y), C = 1 - 3 / (y * y) + 2 * a / (y * y * y);
    const double Q0 = (1 + a / (y * y * y)) / (y * std::sqrt(C));
    const double th = std::acos(a) / 3;
    const double ys[3] = {2 * std::cos(th - kPi / 3), 2 * std::cos(th + kPi / 3), -2 * std::cos(th)};   // "y1 = 2 cos[(cos^-1 a* - pi)/3]" ...
    double br = y - y0 - 1.5 * a * std::log(y / y0);
    for (int i = 0; i < 3; i++) {
        const double yi = ys[i], yj = ys[(i + 1) % 3], yk = ys[(i + 2) % 3];
        if (yi == 0) continue;
        br -= 3 * (yi - a) * (yi - a) / (yi * (yi - yj) * (yi - yk)) * std::log((y - yi) / (y0 - yi));
    }
    return std::pow(r, -3) / B / std::sqrt(C) * Q0 * br;
}

static void testDiscFlux() {
    // the inner edge: 0.54.3's radii (BPT (2.21) = PT (15k)); 1 - E^2 = (2/3) M / r_ms there (BPT (2.20) with equality); E = PT (15g) C^-1/2 G
    for (double a : {0.0, 0.5, 0.9, 0.998, -0.5, -0.9}) {
        const double r = discInner(a);
        check(std::fabs(r - innermostStable(std::fabs(a), a >= 0)) < 1e-10, "disc inner edge = BPT (2.21), a " + f(a));
        const double E = orbitEnergy(r, a);
        check(std::fabs(1 - E * E - 2 / (3 * r)) < 1e-12, "BPT (2.20) at r_ms, a " + f(a));
        for (double rr : {r, 2 * r, 50.0}) {
            const double x = std::sqrt(rr), C = 1 - 3 / (x * x) + 2 * a / (x * x * x), G = 1 - 2 / (x * x) + a / (x * x * x);
            check(std::fabs(orbitEnergy(rr, a) - G / std::sqrt(C)) < 1e-14, "BPT (2.12) = PT (15g), a " + f(a));
            const double Bf = 1 + a / (x * x * x), Ff = 1 - 2 * a / (x * x * x) + a * a / (x * x * x * x);
            check(std::fabs(orbitAngMom(rr, a) - x * Ff / std::sqrt(C)) < 1e-12 * rr, "BPT (2.13) = PT (15h), a " + f(a));
            check(std::fabs(orbitOmega(rr, a) - 1 / (x * x * x * Bf)) < 1e-15, "BPT (2.16) = PT (15f), a " + f(a));
        }
        check(discFlux(r, a) == 0 && discFlux(r - 0.1, a) == 0 && discFlux(r * 1.0001, a) > 0, "no light at and inside the inner edge, a " + f(a));
    }
    check(std::fabs(discInner(0) - 6) < 1e-13, "r_ms = 6M at a = 0 (BPT p. 351)");
    check(std::fabs(discEfficiency(0) - (1 - std::sqrt(8.0 / 9))) < 1e-14, "efficiency at a = 0: 1 - E(6M) = 1 - (8/9)^(1/2)");
    // L79 p. 233: "The flux is maximum at r* ~ 9.55, for which (8 pi / 3 M Mdot) F_max ~ 1.146 10^-4"
    const double rm = fluxMaxRadius(0), Fm = 8 * kPi / 3 * discFlux(rm, 0);
    check(std::fabs(rm - 9.55) < 0.005, "F is largest at 9.55 M (L79 p. 233): " + f(rm));
    check(std::fabs(Fm - 1.146e-4) < 0.0005e-4, "(8 pi / 3 M Mdot) F_max = 1.146e-4 (L79 p. 233): " + f(Fm));
    // L79's printed (15) with 3^(1/2)/3 before the Log does NOT give its own maximum; with 3^(1/2)/2 (PT) it is PT's F at every radius
    double printedMax = 0;
    for (double r = 6.001; r < 40; r += 0.001) printedMax = std::max(printedMax, 8 * kPi / 3 * luminetFlux(r, std::sqrt(3.0) / 3));
    check(std::fabs(printedMax - 1.146e-4) > 0.05e-4, "L79 (15) as printed (sqrt(3)/3) misses its own maximum: " + f(printedMax));
    double worst = 0;                                                                 // (against F_max: both forms cancel to 0 at the inner edge)
    for (double r = 6.0001; r < 2000; r *= 1.01) worst = std::max(worst, std::fabs(luminetFlux(r, std::sqrt(3.0) / 2) - discFlux(r, 0)) / discFlux(rm, 0));
    check(worst < 1e-11, "L79 (15) with sqrt(3)/2 = PT (11b), (15n) at a = 0: " + f(worst));
    // AF's form (97) (r*^-3 B^-1 C^-1/2 Q, prefactor 3/(8 pi) Mdot/M^2 in these units) = PT, any spin
    worst = 0;
    for (double a : {0.0, 0.5, 0.9, -0.5})
        for (double r = discInner(a) * 1.001; r < 3000; r *= 1.05) worst = std::max(worst, std::fabs(3 / (8 * kPi) * afShape(r, a) / discFlux(r, a) - 1));
    check(worst < 1e-10, "AF (97)-(99) = PT (11b), (15n): " + f(worst));
    // the fall far out: F -> 3 M Mdot / (8 pi r^3) (L79 (15) as r* -> infinity)
    check(std::fabs(discFlux(1e6, 0) * 8 * kPi / 3 * 1e18 - 1) < 1e-2, "F falls like 3 Mdot / (8 pi r^3) far out");
    check(std::fabs(discFlux(1e8, 0.9) * 8 * kPi / 3 * 1e24 - 1) < 1e-3, "F falls like 3 Mdot / (8 pi r^3) far out, a = 0.9");
    // total light = efficiency x feeding rate (PT (30), (31a) with W(r_ms) = 0): a conservation law, by quadrature
    for (double a : {0.0, 0.5, 0.9, -0.9}) {
        const double L = discLuminosity(a, 1e300), eta = discEfficiency(a);
        check(std::fabs(L / eta - 1) < 1e-8, "light of both faces = (1 - E(r_ms)) Mdot, a " + f(a) + ": " + f(L) + " vs " + f(eta));
    }
    std::printf("info: disc at a = 0: F largest at %.4f M, (8 pi/3) F_max = %.5g; efficiency %.6f (a = 0), %.6f (0.9)\n", rm, Fm, discEfficiency(0), discEfficiency(0.9));
}

static void testDiscNumbers() {
    const std::string af = readExtract("blackhole/arxiv/1104.5499_abramowicz_fragile2013.extract.txt");
    const std::string yn = readExtract("blackhole/arxiv/1401.0586_yuan_narayan2014.extract.txt");
    // the Eddington luminosity: YN footnote 1, "LEdd = 4πGM c/κes ... usually taken to be 0.4 cm2 g−1"; AF footnote 3: "1.2 × 1038 M/M [erg/s]"
    check(yn.find("4πGM c/κes") != std::string::npos && yn.find("usually taken to be 0.4 cm2 g−1") != std::string::npos, "YN footnote 1 in its extract");
    check(af.find("1.2 × 1038 M/M [erg/s]") != std::string::npos, "AF footnote 3 in its extract");
    const double Le = eddingtonLuminosity(1) * 1e7;                                  // erg/s
    check(std::round(Le / 1e37) == 12, "L_Edd(1 Sun) rounds to AF's 1.2e38 erg/s: " + f(Le));
    check(yn.find("10LEdd /c2 = 1.39 × 1018 (M/M⊙ ) g s−1") != std::string::npos, "YN's Eddington rate in its extract");
    check(std::round(eddingtonRate(1) * 1e3 / 1e16) == 139, "YN's Mdot_Edd = 10 L_Edd / c^2 = 1.39e18 g/s: " + f(eddingtonRate(1) * 1e3));
    check(af.find("[7 × 1026 erg cm−2 s−1 ]") != std::string::npos, "AF (97) in its extract");
    check(std::round(afFluxCoefficient() / 1e26) == 7, "AF's 7e26 erg cm^-2 s^-1 = 3 L_Edd c^4 / (8 pi (G M_sun)^2): " + f(afFluxCoefficient()));
    // Stefan-Boltzmann against CODATA 2022 (5.670 374 419... x 10^-8, exact; P. J. Mohr et al., arXiv:2409.03787)
    check(std::fabs(stefanBoltzmann() / 5.670374419e-8 - 1) < 2e-10, "sigma = CODATA 2022's 5.670374419e-8: " + f(stefanBoltzmann()));
    // where the model applies: YN 4.2.1 "down to ∼ 1.5%LEdd", AF 6 "(L ≈ 0.3 LEdd )"
    check(yn.find("found at luminosities down to ∼ 1.5%LEdd") != std::string::npos && kThinMin == 0.015, "the lowest feeding rate is YN's 1.5% of L_Edd");
    check(af.find("At some luminosity (L ≈ 0.3 LEdd )") != std::string::npos && kThinMax == 0.3, "the highest is AF's 0.3 L_Edd");
    // temperature: sigma T^4 = F; at a fixed r/M, T ~ (mdot / m)^(1/4)
    DiscPhys p; p.massSun = 1e8; p.lEdd = 0.1;
    DiscPhys q = p; q.massSun = 16e8;
    DiscPhys w = p; w.lEdd = 0.1 * 16;
    for (double r : {7.0, 9.55, 30.0}) {
        check(std::fabs(q.temperature(r) * 2 / p.temperature(r) - 1) < 1e-12, "T ~ m^(-1/4) at fixed r/M");
        check(std::fabs(w.temperature(r) / 2 / p.temperature(r) - 1) < 1e-12, "T ~ (L/L_Edd)^(1/4)");
        check(std::fabs(stefanBoltzmann() * std::pow(p.temperature(r), 4) / p.flux(r) - 1) < 1e-12, "sigma T^4 = F");
    }
    // the light out of the disc in watts = L/L_Edd x L_Edd (both faces, by quadrature, then times (energy at infinity / the gas's) as PT (30))
    check(std::fabs(discLuminosity(0, 1e300) * p.mdot() * 299792458.0 * 299792458.0 / (0.1 * eddingtonLuminosity(1e8)) - 1) < 1e-8, "L = 0.1 L_Edd for lEdd = 0.1");
    // YN section 1: "the gas temperature in this model lies in the range 104 − 107 K" (10^4 to 10^7 K), depending on the mass: AF section 1's masses (about 10 Suns for
    // the stellar holes, 10^6 to 10^9 for quasars) over the feeding rates where the model applies
    check(yn.find("lies in the range 104 − 107 K") != std::string::npos, "YN's temperature range in its extract");
    double tLo = 1e300, tHi = 0;
    for (double m : {10.0, 1e6, 1e9})
        for (double l : {kThinMin, kThinMax}) { DiscPhys d; d.massSun = m; d.lEdd = l; const double T = d.hottest(); tLo = std::min(tLo, T); tHi = std::max(tHi, T); }
    check(tLo >= 1e4 && tHi <= 1e7, "the hottest ring stays within YN's 10^4 - 10^7 K: " + f(tLo) + " .. " + f(tHi));
    std::printf("info: hottest ring at L = 0.1 L_Edd, a = 0: 10 Suns %.3g K, 4.297e6 %.3g K, 1e8 %.3g K, 6.5e9 %.3g K\n",
                DiscPhys{10, 0.1, 0}.hottest(), DiscPhys{4.297e6, 0.1, 0}.hottest(), DiscPhys{1e8, 0.1, 0}.hottest(), DiscPhys{6.5e9, 0.1, 0}.hottest());
}

static void testDiscLight() {
    // the colour by tools/star_colour.py's rule: every row of src/orbit/star_colours.inc (its T printed to 0.1 K) within one count
    const std::string t = readSrc("orbit/star_colours.inc");
    int rows = 0, worst = 0;
    for (std::size_t at = t.find("    {"); at != std::string::npos; at = t.find("    {", at + 1)) {
        double bv, T; int R, G, B;
        if (std::sscanf(t.c_str() + at, " {%lf, %lf, %d, %d, %d}", &bv, &T, &R, &G, &B) != 5) continue;
        const auto l = blackBodyLinear(T);
        const double m = std::max({l[0], l[1], l[2]});
        const auto enc = [&](double v) { v /= m; return static_cast<int>(std::lround((v <= 0.0031308 ? 12.92 * v : 1.055 * std::pow(v, 1 / 2.4) - 0.055) * 255)); };
        worst = std::max({worst, std::abs(enc(l[0]) - R), std::abs(enc(l[1]) - G), std::abs(enc(l[2]) - B)});
        rows++;
    }
    check(rows == 241 && worst <= 1, "black-body colours = tools/star_colour.py's table (" + std::to_string(rows) + " rows, worst " + std::to_string(worst) + ")");
    // the yardstick: the Sun's surface at JPL's 5772 K (orbit/sun_facts.h) is 1; the visible light of a very hot black body grows like T (Rayleigh-Jeans)
    check(sunTeff() == 5772 && std::fabs(visibleVsSun(5772) - 1) < 1e-14, "the Sun's surface is the unit of visible brightness");
    check(std::fabs(visibleVsSun(2e8) / visibleVsSun(1e8) - 2) < 1e-3, "far above the visible, brightness grows like T");
    double prev = 0; bool mono = true;
    for (double T = 1000; T < 1e9; T *= 1.1) { const double v = visibleVsSun(T); if (!(v > prev)) mono = false; prev = v; }
    check(mono, "a hotter black body is brighter in visible light");
    // the black body stays a black body (R19 (53)): T_obs = g T. Its brightness for g: Y(gT); for large T, about g Y(T) (not g^4: the visible is the far tail)
    std::printf("info: the visible brightness against the Sun's surface: 3e4 K %.1f, 7e4 K %.1f, 1.5e5 K %.1f, 4e6 K %.0f\n", visibleVsSun(3e4), visibleVsSun(7e4), visibleVsSun(1.5e5), visibleVsSun(4e6));
}

// the far-away observer's direct and secondary images (L79 (5), (10), (12), (13)): given r and the image angle alpha at inclination theta0 (from the axis), find b.
// gamma from (10) (direct) or 2 pi - gamma (secondary, p. 231); (13) 1/r = -(Q-P+2M)/(4MP) + (Q-P+6M)/(4MP) sn^2(gamma/2 (Q/P)^(1/2) + F(zeta_inf, k), k)
// (see luminetInvR for the corrected argument); b^2 = P^3/(P-2M) (5)
static double snSq(double u, double k) {                       // sn^2(u, k) = sin^2(am), F(am, k) = u, by bisection on ellF
    const double K = ellK(k);
    const double per = 2 * K;
    double v = std::fmod(u, per); if (v < 0) v += per;          // sn^2 has period 2K
    double lo = 0, hi = kPi;
    for (int i = 0; i < 64; i++) { const double m = (lo + hi) / 2; if (ellF(m, k) < v) lo = m; else hi = m; }
    const double s = std::sin((lo + hi) / 2);
    return s * s;
}
static double luminetInvR(double P, double gamma) {
    const double Q = std::sqrt((P - 2) * (P + 6));
    const double k = std::sqrt((Q - P + 6) / (2 * Q));
    const double szi = std::sqrt((Q - P + 2) / (Q - P + 6));
    const double Fzi = ellF(std::asin(szi), k);
    // (13) prints the argument as gamma/2 (P/Q)^(1/2) + F(zeta_inf, k); inverting (11), gamma = 2 (P/Q)^(1/2) {F(zeta_r, k) - F(zeta_inf, k)}, gives
    // gamma/2 (Q/P)^(1/2) + F(zeta_inf, k): (13)'s P/Q is a misprint (at P = 497.2, r = 1000 the printed form is 0.4 % off a direct quadrature of L79 (3);
    // (11) agrees with it, and with schwarzschild.h, tested in 0.54.3). REPORT 0.55.1.
    return -(Q - P + 2) / (4 * P) + (Q - P + 6) / (4 * P) * snSq(gamma / 2 * std::sqrt(Q / P) + Fzi, k);
}
static double luminetImpact(double r, double gamma) {       // P from large (little bending) down to the first P whose ray reaches r at this gamma; then bisection
    const double target = 1 / r;
    double hi = 1e7, lo = hi;
    bool found = false;
    while (lo > 3 + 1e-12) {
        const double next = std::max(3 + 1e-12, lo / 1.05);
        if (luminetInvR(next, gamma) >= target) { hi = lo; lo = next; found = true; break; }
        lo = next;
    }
    if (!found) return std::nan("");                         // no ray with a periastron (b > b_c) reaches r at this gamma: L79's (13) does not cover it
    for (int i = 0; i < 200; i++) {
        const double m = (lo + hi) / 2;
        if (luminetInvR(m, gamma) >= target) lo = m; else hi = m;
    }
    const double P = (lo + hi) / 2;
    return std::sqrt(P * P * P / (P - 2));
}

static void testDiscRays() {
    // 1. the ray's end from Binet's equation = the closed form (schwarzschild.h bendFromCamera) for rays to the far sky
    double worst = 0;
    for (double rO : {2.5, 4.0, 20.0, 1000.0}) {
        const double edge = kPi - shadowAngle(rO);
        for (int i = 0; i < 30; i++) {
            const double x = edge * std::pow(1e-6, (i + 0.5) / 30), psi = edge - x;
            const RayEnd2 e = rayEndAngle(rO, psi);
            const Bend B = bendFromCameraEdge(rO, x);
            const double d = std::fabs(e.phiEnd - B.dphi);
            worst = std::max(worst, d);
            if (e.captured || d > 1e-9) check(false, "ray end = closed form, rO " + f(rO) + " x " + f(x) + ": " + f(e.phiEnd) + " vs " + f(B.dphi));
        }
        const RayEnd2 c = rayEndAngle(rO, edge + 1e-3);
        check(c.captured, "a ray inside the shadow ends at the horizon, rO " + f(rO));
    }
    std::printf("info: ray ends against the closed form: worst %.2e rad\n", worst);
    // 2. far camera against L79's isoradials, direct and secondary image, at his inclinations (Fig. 5: 30 degrees above the disc, theta0 = 60; Fig. 6: 10 above, 80),
    //    and the shift against L79 (19). L79's gamma is measured from the observer's direction at infinity, the module's phi from the camera's direction at rO;
    //    between the two the ray sweeps th = asin(b (1 - 2/rO)^(1/2) / rO), its angle on the camera's sky (plus a bending of order M b / rO^2, 1e-11 here).
    //    The crossing angle itself: firstCrossing(cos theta0, -cos alpha sin theta0) = L79's gamma of (10) (the image direction dn = (sin alpha, -cos alpha) with
    //    the disc's axis N = (0, sin theta0, cos theta0) on the camera's frame: right, up, towards the camera).
    const double rO = 1e6;
    double wr = 0, wg = 0, wc = 0; int n = 0, skipped = 0;
    for (double th0deg : {60.0, 80.0}) {
        const double th0 = th0deg * kPi / 180, A = std::cos(th0);
        for (double r : {6.0, 10.0, 20.0, 30.0})
            for (int ia = 0; ia < 24; ia++) {
                const double al = (ia + 0.37) * 2 * kPi / 24;
                const double cg = std::cos(al) / std::sqrt(std::cos(al) * std::cos(al) + 1 / (std::tan(th0) * std::tan(th0)));
                const double gam = std::acos(std::clamp(cg, -1.0, 1.0));          // L79 (10)
                wc = std::max(wc, std::fabs(firstCrossing(A, -std::cos(al) * std::sin(th0)) - gam));
                for (int img = 0; img < 2; img++) {
                    const double b = luminetImpact(r, img == 0 ? gam : 2 * kPi - gam);
                    if (std::isnan(b)) { skipped++; continue; }           // (b < b_c: compared with the integrator below instead)
                    const double dnx = std::sin(img == 0 ? al : al + kPi);
                    const double LzB = -dnx * std::sin(th0);
                    const double th = std::asin(b * std::sqrt(1 - 2 / rO) / rO);
                    const double phi = (img == 0 ? gam : 2 * kPi - gam) - th;
                    const double rm = rayRadius(rO, kPi - th, phi);
                    wr = std::max(wr, std::fabs(rm / r - 1));
                    if (std::fabs(rm / r - 1) > 1e-6) check(false, "L79 isoradial, theta0 " + f(th0deg) + " r " + f(r) + " alpha " + f(al) + " image " + std::to_string(img) + ": " + f(rm));
                    if (img == 0) {
                        const double z1 = 1 / std::sqrt(1 - 3 / r) * (1 + std::pow(r, -1.5) * b * std::sin(th0) * std::sin(al));   // L79 (19)
                        const double g = discShift(rm, b * LzB);
                        wg = std::max(wg, std::fabs(g * z1 - 1));
                        if (std::fabs(g * z1 - 1) > 1e-6) check(false, "L79 (19), theta0 " + f(th0deg) + " r " + f(r) + " alpha " + f(al));
                    }
                    n++;
                }
            }
    }
    check(wc < 1e-14, "the first crossing = L79 (10)'s gamma: " + f(wc));
    check(n + skipped == 384 && n >= 340, "L79 isoradials compared: " + std::to_string(n));
    std::printf("info: far camera against L79's isoradials: %d image points (%d near-side points of r = 6, 10 have b < b_c: no periastron, outside (13)), radius within %.2e, 1 + z within %.2e\n", n, skipped, wr, wg);
    // the simple far-out form of the direct image (L79 p. 231): b = r (1 + tan^2 theta0 cos^2 alpha)^(-1/2); the difference falls like M/r
    for (double r : {1e3, 1e4}) {
        double w = 0;
        for (int ia = 0; ia < 12; ia++) {
            const double th0 = 60 * kPi / 180, al = (ia + 0.2) * 2 * kPi / 12;
            const double b = r / std::sqrt(1 + std::tan(th0) * std::tan(th0) * std::cos(al) * std::cos(al));
            const double th = std::asin(b * std::sqrt(1 - 2 / rO) / rO);
            const double rm = rayRadius(rO, kPi - th, firstCrossing(std::cos(th0), -std::cos(al) * std::sin(th0)) - th);
            w = std::max(w, std::fabs(rm / r - 1));
        }
        check(w < 4 * std::sqrt(1.0 / r), "far out the direct image is L79's ellipse, r " + f(r) + ": " + f(w));
        std::printf("info: L79's ellipse at r = %g: radius within %.2e\n", r, w);
    }
    // 3. close in: the ray integrator (geodesic.h, J15 (A.15)) at a = 0, cameras inside 6M and at 15M, two inclinations; every crossing of the disc's plane
    KerrBL bl(0);
    double wi = 0; int ni = 0;
    for (double rc : {4.0, 15.0})
        for (double incl : {10.0, 60.0}) {
            Camera cam; cam.r = rc; cam.theta = (90 - incl) * kPi / 180;
            const double edge = kPi - shadowAngle(rc);
            for (int i = 0; i < 24; i++) {
                const double psi = (i < 18 ? 0.3 + (edge - 0.3) * i / 18.0 : edge + 0.05 * (i - 17)), beta = 0.4 + 0.7 * i;
                const Vec3 look{std::cos(psi), std::sin(psi) * std::cos(beta), std::sin(psi) * std::sin(beta)};
                std::vector<double> path;
                Ray ray;
                bl.launch(cam, look, ray);
                trace(bl, cam, look, TraceOptions{}, &path);
                const int ns = static_cast<int>(path.size()) / 5;
                // the module's crossings
                const double A = std::cos(cam.theta), B = -std::sin(cam.theta) * std::cos(beta);
                const RayEnd2 e = rayEndAngle(rc, psi);
                int k = 0;
                for (int s = 0; s + 1 < ns && k < 3; s++) {
                    const double t0 = path[5 * s + 1] - kPi / 2, t1 = path[5 * s + 6] - kPi / 2;
                    if (!(t0 * t1 < 0) && !(t1 == 0)) continue;
                    // refine: Newton on the span from the state before the crossing
                    Ray q = ray;
                    for (int j = 0; j < 5; j++) q.y[j] = path[5 * s + j];
                    double span = 0;
                    for (int it = 0; it < 40; it++) {
                        Ray z = q;
                        if (span != 0) integrateSpan(bl, z, span);
                        double dy[kMaxState];
                        bl.rhs(z, z.y, dy);
                        const double d = (z.y[1] - kPi / 2) / dy[1];
                        span -= d;
                        if (std::fabs(d) < 1e-14) break;
                    }
                    Ray z = q;
                    integrateSpan(bl, z, span);
                    const double phi = firstCrossing(A, B) + k * kPi;
                    if (phi < e.phiEnd) {
                        const double rm = rayRadius(rc, psi, phi), d = std::fabs(rm / z.y[0] - 1);
                        wi = std::max(wi, d); ni++;
                        if (d > 1e-8) check(false, "crossing radius = the integrator's, camera " + f(rc) + " incl " + f(incl) + " psi " + f(psi) + " k " + std::to_string(k) + ": " + f(rm) + " vs " + f(z.y[0]));
                    }
                    k++;
                }
            }
        }
    check(ni >= 40, "crossings compared with the integrator: " + std::to_string(ni));
    std::printf("info: crossings against the ray integrator: %d, radius within %.2e\n", ni, wi);
}

static void testDiscShift() {
    // C75 p. 791: "The axial observer sees radiation from a given emission radius to have a unique value of g" and "that seen by the axial observer is always redshifted";
    // at a = 0 that value is (1 - 3M/r)^(1/2) (L79 (19) at theta0 = 0). Face on (A = 1, B = 0): Lz = 0 for every pixel.
    for (double r : {6.0, 9.55, 30.0, 400.0}) {
        check(std::fabs(discShift(r, 0) - std::sqrt(1 - 3 / r)) < 1e-15 && discShift(r, 0) < 1, "face on: g = (1 - 3M/r)^(1/2) < 1, r " + f(r));
    }
    // C75 p. 791: the equatorial observer "sees some blueshifted radiation from every radius outside the radius of marginal stability"
    const double rO = 1e8, th0 = 89.9 * kPi / 180;
    for (double r : {6.01, 8.0, 25.0, 100.0, 400.0}) {
        // alpha = -90 degrees (the approaching side: Lz = b sin theta0): find the direct image's b by bisection on the radius at the first crossing
        const double A = std::cos(th0), B = 0, LzB = std::sin(th0);
        double lo = 1e-3, hi = 3 * r + 10;
        for (int i = 0; i < 200; i++) {
            const double m = (lo + hi) / 2;
            const double rm = rayRadius(rO, kPi - std::asin(m / rO), firstCrossing(A, B));
            if (rm < r) lo = m; else hi = m;
        }
        const double b = (lo + hi) / 2, g = discShift(r, b * LzB);
        check(g > 1, "an equatorial observer sees blueshift from r = " + f(r) + ": g = " + f(g));
        if (r == 6.01) std::printf("info: g on the approaching side at r_ms, nearly edge-on: %.4f; over the face-on value %.4f (C75 p. 792 prints 1.7 for its g_max ratio at a = 0)\n", g, g / std::sqrt(1 - 3 / r));
    }
    // symmetry: face-on rings are circles (the radius depends only on the angle from the hole); left-right mirror: the same radius, Lz of the opposite sign
    const double rc = 20;
    for (double th : {0.2, 0.5, 1.0}) {
        const double psi = kPi - th;
        double r0 = 0;
        for (int ia = 0; ia < 8; ia++) {
            const double a = ia * kPi / 4;
            const double rr = rayRadius(rc, psi, firstCrossing(1, 0) + 0 * a);
            if (ia == 0) r0 = rr; else check(rr == r0, "face on: a circle");
        }
        const double i = 25 * kPi / 180, A = std::sin(i);
        for (double a : {0.3, 1.2, 2.5}) {
            const double dnx = std::sin(a), dny = std::cos(a);
            const DiscHit L = discRay(rc, psi, A, dny * -std::cos(i), -dnx * std::cos(i), DiscGeom{6, 1e9});
            const DiscHit R = discRay(rc, psi, A, dny * -std::cos(i), dnx * std::cos(i), DiscGeom{6, 1e9});
            check(L.order == R.order && L.r == R.r && L.Lz == -R.Lz && (L.order < 0 || L.g != R.g), "mirror: same shape, not the same brightness");
        }
    }
}

static void testDiscTable() {
    // the card's table against the reference (Binet by Dormand-Prince), away from the very edge of the shadow
    for (double rO : {4.0, 30.0, 1000.0}) {
        const DiscTable T(rO);
        double worst = 0, wend = 0; int n = 0;
        const double al = T.alphaSh();
        for (int i = 0; i < 300; i++) {
            const double sgn = i % 3 == 2 ? -1 : 1;
            const double xmax = sgn > 0 ? kPi - al : al;
            const double fr = (i + 1) * 0.618034 - std::floor((i + 1) * 0.618034), x = sgn * xmax * std::pow(1e-6, fr);
            const double psi = (kPi - al) - x;
            const RayEnd2 e = rayEndAngle(rO, psi);
            wend = std::max(wend, std::fabs(e.phiEnd - T.phiEnd(x)) / e.phiEnd);
            for (int j = 1; j < 8; j++) {
                const double phi = e.phiEnd * j / 8.0;
                double u;
                if (!T.lookup(x, phi, u)) { check(false, "table: inside the ray"); continue; }
                const double ur = 1 / rayRadius(rO, psi, phi);
                worst = std::max(worst, std::fabs(u / ur - 1));
                n++;
            }
        }
        check(worst < 1e-6, "the card's table = the reference within 1e-6 (u = 1/r), rO " + f(rO) + ": " + f(worst));
        check(wend < 1e-6, "the table's ray ends, rO " + f(rO) + ": " + f(wend));
        std::printf("info: disc table rO %g: %dx%dx2 values, filled in %.1f ms; u within %.2e, ends within %.2e (%d points)\n", rO, T.rows(), T.cols(), T.fillMs(), worst, wend, n);
        // the cut-off: how wide (in angle on the camera's sky) the images of a disc from 6 to 20 M are, along the line below the hole, 30 degrees above the disc;
        // each further one about e^-pi (Gralla & Lupsasca 2020b (72)) of the one before
        const double i30 = 30 * kPi / 180, A = std::sin(i30), B = std::cos(i30);          // dn = (0, -1): t . N = -(-cos i) = cos i
        double width[kDiscOrders] = {0, 0, 0, 0};
        for (int side = 0; side < 2; side++) {
            double prevX = 0; int prevK = -2;
            for (int q = 0; q <= 200000; q++) {
                const double ax = std::exp(std::log(1e-9) + (std::log(side ? al : kPi - al) - std::log(1e-9)) * q / 200000.0), x = side ? -ax : ax;
                const DiscHit h = discRayTable(T, x, A, B, 0, DiscGeom{6, 20});
                if (h.order >= 0 && h.order == prevK) width[h.order] += std::fabs(x - prevX);
                prevX = x; prevK = h.order;
            }
        }
        const double ratio = width[3] / width[2];
        check(std::fabs(ratio / std::exp(-kPi) - 1) < 0.25, "each further image about e^-pi as wide, rO " + f(rO) + ": " + f(ratio));
        std::printf("info: disc 6-20 M seen 30 deg above, rO %g: image widths %.3g, %.3g, %.3g, %.3g rad (the 4th: %.2g px at 3840 px across 10 deg); 5th not drawn\n", rO,
                    width[0], width[1], width[2], width[3], width[3] * 2160 / (10 * kPi / 180));
    }
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
    testDiscFlux();
    testDiscNumbers();
    testDiscLight();
    testDiscRays();
    testDiscShift();
    testDiscTable();
    std::printf("%d checks, %d failed\n", g_checks, g_failed);
    return g_failed ? 1 : 0;
}
