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
//  - 0.55.3, the looks (looks.h): every number of the Film look read from James et al. 2015 (the extract; with PL_SOURCES_DIR set, the saved original itself,
//    and every block of the extract found in it); the Film disc's colour = the CIE black body at the paper's temperature through the paper's white balance;
//    no shift: the same light from the approaching and the receding side (and Real not); the glow's reference adds light only and spreads it without loss.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <functional>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

#include "blackhole/disc.h"
#include "blackhole/elliptic.h"
#include "blackhole/gas.h"
#include "blackhole/geodesic.h"
#include "blackhole/kerr.h"
#include "blackhole/kerr_closed.h"
#include "blackhole/mino.h"
#include "blackhole/presets.h"
#include "blackhole/schwarzschild.h"
#include "blackhole/spin.h"
#include "blackhole/spin_sky.h"
#include "orbit/far_masses.h"
#include "blackhole/lab.h"
#include "blackhole/looks.h"
#include "blackhole/light.h"
#include "blackhole/tone.h"
#include "orbit/sky.h"
#include "orbit/units.h"

// 0.55.3: stb_image's zlib reader (static, so it cannot clash with the app's copy) for the saved paper's .txt.gz in the private sources repo
#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#define STBI_NO_LINEAR
#define STBI_NO_HDR
#include "stb_image.h"

using namespace blackhole;

static int g_failed = 0, g_checks = 0;
static void check(bool ok, const std::string& what) {
    g_checks++;
    if (!ok) { g_failed++; std::printf("FAIL: %s\n", what.c_str()); }
    else if (std::getenv("PL_BH_VERBOSE")) std::printf("ok: %s\n", what.c_str());
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
    {   // 0.70.0 (a deliberate change of the starting view, lab.h kTiltDefaultDeg): turned towards the galactic centre, tilted to 100 degrees from the disc's
        // axis (the camera south of the disc; the picture's middle 80 degrees from the axis, the galactic centre 95.6): the middle's angle from the galactic
        // centre is the difference of the two directions' angles from the axis (the same azimuth), 15.5 degrees
        const Vec3 gc = orbit::skyData().galCentre;
        const double want = std::fabs(std::acos(gc.z) - std::acos(-std::cos(kTiltDefaultDeg * kPi / 180)));
        check(std::fabs(angleBetween(mid, gc) - want) < 1e-12 && d.pitch == kTiltDefaultDeg * kPi / 180,
              "the default view: towards the galactic centre, 100 degrees from the disc's axis (" + f(angleBetween(mid, gc) * 180 / kPi) + " degrees off it)"); }
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
    check(std::fabs(400 / hv.K - kFovDefault * kPi / 360) < 1e-12, "the field of view spans the height");
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
    // the tables made once for many pixels against the full sums
    double wy = 0, wc = 0;
    for (double T = 400; T < 9e8; T *= 1.37) {
        wy = std::max(wy, std::fabs(visibleVsSunFast(T) / visibleVsSun(T) - 1));
        const auto a = blackBodyLinearFast(T), b = blackBodyLinear(T);
        const double m = std::max({b[0], b[1], b[2]});
        for (int k = 0; k < 3; k++) wc = std::max(wc, std::fabs(a[k] - b[k]) / m);
    }
    check(wy < 1e-4 && wc < 1e-4, "the light tables = the full sums within 1e-4: " + f(wy) + ", " + f(wc));
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

static void testDiscSettings() {
    // presets (owner's decision of 15 Nov 2026): the real holes open with the disc off, the feeding one with it on; a changed mass drops the preset
    Settings s = settingsDefaults();
    check(!s.disc && s.feed == kFeedDefault && s.outer == kOuterDefault && s.expAuto, "defaults: no disc, 0.1 L_Edd, the outer edge kOuterDefault, automatic exposure");
    // 0.58.0: the outer edge (until 0.57: 30 M, L79's "living core"; reference changed on purpose): Goodman 2003's "a thousand Schwarzshild radii" (R_S = 2 GM/c^2)
    {
        const std::string g = readExtract("blackhole/arxiv/astro-ph_0201001_goodman2003.extract.txt");
        check(g.find("thin disks may not exist beyond a thousand Schwarzshild radii or so") != std::string::npos && kOuterDefault == 2 * 1000.0 && kOuterMax >= kOuterDefault,
              "the outer edge: 1000 Schwarzschild radii = 2000 GM/c^2 (Goodman 2003's abstract)");
        // its section 1: Q = 1 at about 10^-2 pc for 10^8 Suns: 0.01 pc in GM/c^2 of 10^8 Suns (pc = 648000/pi au, au = 149,597,870,700 m (IAU 2012 B2, 2015 B2), GM_sun from JPL DE440 (orbit/units.h))
        const double gmc2 = gravRadiusM(1e8), pcM = 3.0856775814913673e16;
        check(g.find("Q falls to unity at a distance r ") != std::string::npos && g.find(" pc from a central black hole of\n") != std::string::npos && std::fabs(0.01 * pcM / gmc2 / kOuterDefault - 1) < 0.1,
              "and 0.01 pc for 10^8 Suns is " + f(0.01 * pcM / gmc2) + " GM/c^2, within 10 % of it");
        Settings fd = settingsDefaults(); fd.outer = 44; applyPreset(fd, kFeedingKey);
        check(fd.outer == kOuterDefault, "the feeding preset sets the outer edge to it");
    }
    check(applyPreset(s, kFeedingKey) && s.disc && s.massSun == kFeedingMass && s.preset == kFeedingKey, "the feeding preset: the disc on, 1e8 Suns");
    check(readout(s).hasPreset && !readout(s).realPreset && readout(s).presetName == kFeedingName, "the feeding preset is not a measured hole");
    applyPreset(s, "m87");
    check(!s.disc && readout(s).realPreset, "M87*: the disc off");
    applyPreset(s, kFeedingKey); setMass(s, 2e8);
    check(s.preset.empty() && s.disc, "a new mass drops the feeding preset, the disc stays");
    // the ranges where the model applies; the settings file
    setFeed(s, 0.9); check(s.feed == kThinMax, "the feeding rate stops at 0.3 L_Edd");
    setFeed(s, 1e-4); check(s.feed == kThinMin, "and at 0.015");
    setOuter(s, 3); check(s.outer == kOuterMin, "the outer edge stays outside the inner one");
    setExposure(s, 2.25); check(!s.expAuto && s.expEV == 2.25 && exposureStops(s) == 2.25, "a chosen exposure (0.70.0: stops from automatic)");
    {   Settings e = s;                                                 // 0.70.0: the chosen stops start from 0 again when the preset, the look or the disc changes
        setExposure(e, 99); const bool top = e.expEV == kEvMax;
        Settings a = e, l = e, d = e, same = e;
        applyPreset(a, "m87"); setLook(l, Look::Film); setDisc(d, false); setDisc(same, true); setLook(same, Look::Real);
        check(top && a.expEV == 0 && l.expEV == 0 && d.expEV == 0 && same.expEV == kEvMax && !a.expAuto, "the stops: clamped; back to 0 on a new preset, look or disc, kept otherwise");
        std::map<std::string, std::string> old{{"exposure", "2.25"}};      // a 0.69 file: an absolute screen's white
        const Settings o = loadSettings(old);
        check(!o.expAuto && o.expEV == 0, "an old file's absolute exposure is read as your choice at 0 stops"); }
    std::map<std::string, std::string> m;
    s.feed = 0.05; s.outer = 44;
    saveSettings(s, m);
    const Settings b = loadSettings(m);
    check(b == s && m["disc"] == "on" && m["exposure"] == "stops" && m["exposurestops"] == "2.25", "the disc's settings round trip");
    m["feed"] = "0.5"; m["outer"] = "abc"; m["exposure"] = "auto"; m["disc"] = "maybe";
    const Settings c = loadSettings(m);
    check(c.feed == kFeedDefault && c.outer == kOuterDefault && c.expAuto && !c.disc, "bad disc values keep the defaults");
    // the readout: from the module
    Settings f = settingsDefaults(); applyPreset(f, kFeedingKey);
    const Readout r = readout(f);
    DiscPhys p; p.massSun = 1e8; p.lEdd = 0.1;
    double rh;
    check(r.disc && r.rIn == discInner(0) && std::fabs(r.rIn - 6) < 1e-12 && r.tHot == p.hottest(&rh) && r.rHot == rh && r.efficiency == discEfficiency(0) && r.lumW == 0.1 * eddingtonLuminosity(1e8), "the disc's readout is the module's");
    // the view: the disc's axis on the view frame is the sky frame's z (lab.h)
    const HoleView v = holeView(f, 1280, 800, 640, 400);
    const Vec3 N = discAxisView(v), mid = viewMiddle(v);
    check(std::fabs(N.z - (-mid.z)) < 1e-15, "the axis against the camera's direction: e_r . z");
}

// =============================================================================================================================================
// 0.55.2: the spinning hole (plan step 3, section 5's rows "shadow, spin", "repeated images", "disc shifts, spin", "camera motion", "star brightness")

// J15 (A.5), (A.6) against BPT (2.18) and GLb (38)-(40); the cubic of R' against (A.4)
static void testSpinClosedForms() {
    for (double a : {1e-6, 0.1, 0.5, 0.9, 0.99, 0.999}) {
        check(std::fabs(trappedRadius1(a) - photonOrbit(a, true)) < 1e-14 && std::fabs(trappedRadius2(a) - photonOrbit(a, false)) < 1e-14,
              "J15 (A.6) r1, r2 = BPT (2.18) prograde and retrograde photon orbits, a " + f(a));
        check(std::fabs(criticalRadiusMinus(a) - trappedRadius1(a)) < 1e-14 && std::fabs(criticalRadiusPlus(a) - trappedRadius2(a)) < 1e-14, "GLb (40) = J15 (A.6), a " + f(a));
        const double r1 = trappedRadius1(a), r2 = trappedRadius2(a);
        double worst = 0, worstQ = 0;
        for (int i = 0; i <= 20; i++) {
            const double r = r1 + (r2 - r1) * i / 20.0;
            worst = std::max(worst, std::fabs(trappedB(a, r) - criticalLambda(a, r)) / (1 + std::fabs(trappedB(a, r))));
            worstQ = std::max(worstQ, std::fabs(trappedQ(a, r) - criticalEta(a, r)) / 27);
            if (i > 0 && i < 20) {                                  // the inverse
                const double rb = trappedRadiusOfB(a, trappedB(a, r));
                worst = std::max(worst, std::fabs(rb - r) / r);
            }
        }
        if (a >= 0.1) check(worst < 1e-9 && worstQ < 1e-9, "J15 (A.5) b_o, q_o = GLb (38), (39) over the photon shell, and b_o inverted, a " + f(a) + ": " + f(worst) + " " + f(worstQ));
        else {   // (GLb's printed forms cancel at small spin; the factored (A.5) against the a = 0 photon sphere: b^2 + q = 27 M^2, Gralla & Lupsasca (71))
            double w = 0;
            for (int i = 0; i <= 20; i++) { const double r = r1 + (r2 - r1) * i / 20.0, b = trappedB(a, r); w = std::max(w, std::fabs(b * b + trappedQ(a, r) - 27) / 27); }
            check(w < 1e-5, "spin 1e-6: J15 (A.5) gives the photon sphere's b^2 + q = 27 (GLb (71)): " + f(w));
        }
        // at the shell's ends the orbits are circular and equatorial (GLb below (40)): q_o = 0; both bound orbits have R = R' = 0 (GLb (37))
        check(std::fabs(trappedQ(a, r1)) < 1e-6 && std::fabs(trappedQ(a, r2)) < 1e-6, "q_o = 0 at r1 and r2 (equatorial circular photon orbits), a " + f(a));
        const double rm = 0.5 * (r1 + r2), bm = trappedB(a, rm), qm = trappedQ(a, rm);
        const double R0 = radialR(a, bm, qm, rm), dR = (radialR(a, bm, qm, rm * (1 + 1e-6)) - radialR(a, bm, qm, rm * (1 - 1e-6))) / (2e-6 * rm);
        check(std::fabs(R0) < 1e-9 * rm * rm * rm * rm && std::fabs(dR) < 1e-5 * rm * rm * rm, "a trapped orbit is a double root of R (GLb (37)), a " + f(a));
    }
    // R' = 4 r^3 + 2 (a^2 - b^2 - q) r + 2 ((b - a)^2 + q) (the cubic in spin.cpp) against a difference of (A.4)
    double worst = 0;
    for (double a : {0.3, 0.9})
        for (double b : {-6.0, -1.0, 2.0, 5.0})
            for (double q : {0.0, 3.0, 20.0})
                for (double r : {1.8, 3.0, 7.0}) {
                    const double h = 1e-5, d = (radialR(a, b, q, r + h) - radialR(a, b, q, r - h)) / (2 * h);
                    const double c = 4 * r * r * r + 2 * (a * a - b * b - q) * r + 2 * ((b - a) * (b - a) + q);
                    worst = std::max(worst, std::fabs(d - c) / (1 + std::fabs(c)));
                }
    check(worst < 1e-7, "R' as a cubic = d/dr of J15 (A.4): " + f(worst));
}

// the closed test (J15 (v)) against the integrator (J15 (A.15)) for several spins, distances and latitudes: random directions and a fine band round the edge
static void testSpinShadow() {
    int wrong = 0, total = 0, lost = 0, multi = 0;
    for (double a : {0.5, 0.9, 0.99, 0.999})
        for (double rc : {2.5, 4.0, 20.0, 1000.0})
            for (double th : {kPi / 2, 1.0, 0.2, 2.5}) {
                const Camera cam{rc, th, 0};
                const KerrBL ch(a);
                unsigned seed = 12345;
                const auto rnd = [&] { seed = seed * 1103515245u + 12345u; return ((seed >> 8) & 0xffffff) / 16777216.0; };
                const auto one = [&](double psi, double chi) {
                    const Vec3 look = lookAt(psi, chi);
                    const RayEnd e = trace(ch, cam, look);
                    if (e.kind == RayEnd::Lost) { lost++; return; }
                    total++;
                    if ((e.kind == RayEnd::Horizon) != lookFromHorizon(a, cam, look)) wrong++;
                };
                for (int k = 0; k < 12; k++) one(std::acos(1 - 2 * rnd()), 2 * kPi * rnd());
                for (int k = 0; k < 4; k++) {
                    const double chi = 2 * kPi * (k + 0.37) / 4, pe = shadowEdge(a, cam, chi);
                    for (double x : {1e-2, 1e-4, 1e-6}) { one(pe + x, chi); one(pe - x, chi); }
                }
                // one change along each azimuth (the table's assumption, spin_sky.h): the closed test at 1000 angles along 24 azimuths
                for (int k = 0; k < 24; k++) {
                    const double chi = 2 * kPi * k / 24;
                    int changes = 0;
                    bool prev = true;
                    for (int i = 1; i < 1000; i++) {
                        const bool h = lookFromHorizon(a, cam, lookAt(kPi * i / 1000, chi));
                        if (h != prev) changes++;
                        prev = h;
                    }
                    if (changes != 1) multi++;
                }
            }
    check(wrong == 0 && lost == 0 && total == 4 * 4 * 4 * 36, "the integrator's horizon or sky = J15's closed test (A.5), (A.6), (A.13), (A.14), " + f(total) + " rays incl. 1e-6 rad from the edge: " + f(wrong) + " wrong, " + f(lost) + " lost");
    check(multi == 0, "the shadow's edge is crossed once along every azimuth from the hole's direction (64 cameras x 24 azimuths): " + f(multi) + " not");
    // a -> 0: the circle of Synge's size (Perlick & Tsupko (6), (7))
    for (double rc : {2.5, 4.0, 20.0, 1000.0})
        for (double th : {kPi / 2, 0.7}) {
            double worst = 0;
            for (int k = 0; k < 16; k++) worst = std::max(worst, std::fabs(shadowEdge(1e-6, Camera{rc, th, 0}, 2 * kPi * k / 16) - shadowAngle(rc)));
            check(worst < 1e-5, "spin 1e-6: the edge is Synge's circle, r " + f(rc) + " theta " + f(th) + ": " + f(worst));
        }
}

// the far-away edge: GLb's critical curve (55) and PeT's Table III and II
static void testSpinFarEdge() {
    const double rc = 1e6;
    for (double a : {0.5, 0.9, 0.99})
        for (double thDeg : {17.0, 60.0, 90.0}) {
            const double th = thDeg * kPi / 180;
            const Camera cam{rc, th, 0};
            double worst = 0;
            int n = 0;
            const double rm = criticalRadiusMinus(a), rp = criticalRadiusPlus(a);
            for (int i = 1; i < 40; i++) {
                const double rt = rm + (rp - rm) * i / 40.0;
                double al, b2;
                if (!criticalScreen(a, rt, th, al, b2)) continue;
                const double be = std::sqrt(b2), chi = std::atan2(al, -be);   // N_phi = alpha / r, N_theta = -beta / r for a far FIDO (J15 (A.11), (A.12))
                const double psi = shadowEdge(a, cam, chi);
                worst = std::max(worst, std::fabs(rc * std::tan(psi) / std::hypot(al, be) - 1));
                n++;
            }
            check(n >= 10 && worst < 1e-5, "far camera (1e6 M): the closed test's edge = GLb's critical curve (38)-(40), (55), a " + f(a) + " incl " + f(thDeg) + ": " + f(worst));
        }
    // PeT Table III (diameters, in M) and Table II (a = M from the equator spans alpha -2M .. 7M)
    const auto range = [](double a, double th, double& amin, double& amax, double& bmax) {
        amin = 1e9; amax = -1e9; bmax = 0;
        const double rm = criticalRadiusMinus(a), rp = criticalRadiusPlus(a);
        for (int i = 0; i <= 200000; i++) {
            const double rt = rm + (rp - rm) * i / 200000.0;
            double al, b2;
            if (!criticalScreen(a, rt, th, al, b2)) continue;
            amin = std::min(amin, al); amax = std::max(amax, al); bmax = std::max(bmax, std::sqrt(b2));
        }
    };
    double amin, amax, bmax;
    range(1 - 1e-10, kPi / 2, amin, amax, bmax);
    check(std::fabs(amax - amin - 9) < 2e-3 && std::fabs(2 * bmax - 6 * std::sqrt(3.0)) < 1e-6, "PeT Table III: a = M from the equator, 9M across, 6 sqrt(3) M up-down: " + f(amax - amin) + " " + f(2 * bmax));
    check(std::fabs(amin + 2) < 2e-3 && std::fabs(amax - 7) < 2e-3, "PeT Table II: a = M from the equator, alpha from -2M to 7M: " + f(amin) + " .. " + f(amax));
    for (double a : {0.3, 0.7, 0.99}) {
        range(a, kPi / 2, amin, amax, bmax);
        check(std::fabs(2 * bmax - 6 * std::sqrt(3.0)) < 1e-6, "PeT Table III: from the equator 6 sqrt(3) M up-down at any spin, a " + f(a) + ": " + f(2 * bmax));
    }
    {   // along the axis, a = M: a circle of diameter 4 (1 + sqrt(2)) M: alpha = 0 needs lambda~ = 0, then beta^2 = eta~ + a^2 ((55) at theta_o = 0)
        const double a = 1 - 1e-10;
        double lo = criticalRadiusMinus(a), hi = criticalRadiusPlus(a);
        for (int i = 0; i < 200; i++) { const double m = 0.5 * (lo + hi); (criticalLambda(a, m) > 0 ? lo : hi) = m; }
        const double d = 2 * std::sqrt(criticalEta(a, lo) + a * a);
        check(std::fabs(d - 4 * (1 + std::sqrt(2.0))) < 1e-6, "PeT Table III: a = M along the axis, 4 (1 + sqrt(2)) M across: " + f(d));
    }
    // which side is flat (seen from the equator, a = 0.99): the prograde side alpha < 0 (lambda > 0) is the near, flat one (Table II: -2M against 7M);
    // on the camera's sky that is N_phi < 0 (chi = -pi/2); and from near the axis the shadow is round
    {
        const Camera eq{1e4, kPi / 2, 0};
        const double flat = shadowEdge(0.99, eq, -kPi / 2), round = shadowEdge(0.99, eq, kPi / 2);
        check(flat < round && std::fabs(flat * 1e4 - 2.0) < 0.4 && std::fabs(round * 1e4 - 7.0) < 0.2, "from the equator the shadow is flattened on the side that moves towards the camera (N_phi < 0): " + f(flat * 1e4) + " M against " + f(round * 1e4) + " M");
        const Camera ax{1e4, 1e-3, 0};
        double lo2 = 1e9, hi2 = 0;
        for (int k = 0; k < 32; k++) { const double e = shadowEdge(0.99, ax, 2 * kPi * k / 32); lo2 = std::min(lo2, e); hi2 = std::max(hi2, e); }
        check(hi2 / lo2 - 1 < 2e-3, "from near the axis (1e-3 rad) the shadow is round: " + f(hi2 / lo2 - 1));
    }
    // the Lyapunov exponent (GLb (43)) -> pi at a -> 0 (GLb (72))
    check(std::fabs(lyapunov(1e-6, 3.0) - kPi) < 1e-5, "GLb (43) at a -> 0 is pi (72): " + f(lyapunov(1e-6, 3.0)));
}

// further images: each thinner by e^-gamma (GLb (42), (43)): the screen offsets from the edge at which the ray makes one more half-orbit (one more crossing of the equator)
static void testSpinImages() {
    const double rc = 1e4;
    for (double a : {0.5, 0.9})
        for (double thDeg : {60.0, 90.0}) {
            const double th = thDeg * kPi / 180;
            const Camera cam{rc, th, 0};
            const KerrBL ch(a);
            const double rt = 0.5 * (criticalRadiusMinus(a) + criticalRadiusPlus(a));
            double al, b2;
            if (!criticalScreen(a, rt, th, al, b2)) { check(false, "a critical point on the screen"); continue; }
            const double chi = std::atan2(al, -std::sqrt(b2)), pe = shadowEdge(a, cam, chi);
            TraceOptions o;
            o.rtol = 1e-13; o.atol = 1e-14;
            std::vector<double> cr;
            o.cross = &cr; o.maxCross = 60;
            const auto crossings = [&](double x) { cr.clear(); trace(ch, cam, lookAt(pe + x, chi), o); return static_cast<int>(cr.size() / 5); };
            // x_k: the largest x with at least k crossings, for k = n0 .. n0 + 3
            const int n0 = crossings(1e-4 * pe) + 1;
            std::vector<double> xs;
            for (int k = n0; k < n0 + 4; k++) {
                double lo = 1e-12 * pe, hi = 1e-4 * pe;              // lo: >= k crossings
                if (crossings(lo) < k) break;
                for (int it = 0; it < 60; it++) { const double m = std::sqrt(lo * hi); (crossings(m) >= k ? lo : hi) = m; }
                xs.push_back(lo);
            }
            const double g = lyapunov(a, rt);
            bool ok = xs.size() == 4;
            std::string got;
            for (std::size_t k = 1; k < xs.size(); k++) { const double ratio = xs[k] / xs[k - 1]; got += " " + f(ratio); if (std::fabs(ratio / std::exp(-g) - 1) > 0.03) ok = false; }
            check(ok, "each further image thinner by e^-gamma (GLb (42), (43): " + f(std::exp(-g)) + "), a " + f(a) + " incl " + f(thDeg) + ":" + got);
        }
}

// the gas's shift at spin: C75 (A9) against the 4-velocity's norm and BPT's energy (2.12); a = 0 = L79 (19); C75's two printed ratios (p. 792)
static void testSpinShift() {
    double worst = 0, worstE = 0;
    for (double a : {0.0, 0.5, 0.9, 0.998})
        for (double r : {discInner(a), 3.0 + discInner(a), 10.0, 100.0})
            for (double b : {-5.0, 0.0, 4.0}) {
                const KerrMetric m = kerrMetric(a, r, kPi / 2);
                const double Om = orbitOmega(r, a);
                const double gtt = -m.alpha * m.alpha + m.omega * m.omega * m.varpi * m.varpi, gtp = -m.omega * m.varpi * m.varpi, gpp = m.varpi * m.varpi;   // (A.1)
                const double ut = 1 / std::sqrt(-(gtt + 2 * Om * gtp + Om * Om * gpp));   // u . u = -1
                worst = std::max(worst, std::fabs(discShiftKerr(r, b, a) * ut * (1 - Om * b) - 1));
                worstE = std::max(worstE, std::fabs(-(gtt * ut + gtp * ut * Om) / orbitEnergy(r, a) - 1));
            }
    check(worst < 1e-12, "C75 (A9) = 1 / (u^t (1 - Omega b)) with u normalised in J15's metric (A.1): " + f(worst));
    check(worstE < 1e-12, "that orbit's -u_t = BPT (2.12)'s energy: " + f(worstE));
    double w0 = 0;
    for (double r : {6.0, 10.0, 50.0}) for (double b : {-5.0, 3.0}) w0 = std::max(w0, std::fabs(discShiftKerr(r, b, 0) / discShift(r, b) - 1));
    check(w0 < 1e-13, "a = 0: C75 (A9) = L79 (19) (disc.h discShift): " + f(w0));
    // C75 p. 792: g_max(r_ms, equatorial) / g(r_ms, axial) "is 15 for a/M = 0.9981, compared with 1.7 for a/M = 0". The axial g is (A9) with lambda = 0
    // (Fig. 1: one curve); the equatorial one is C75's own family (A5) at theta_o = pi/2 (spin.h): the ray that makes half an oscillation over the plane.
    // a = 0: r_ms = 6 lies between C75's tabulated radii (19 values, (400M/r)^(1/2) = 1 .. 19; Table 1 columns); at the nearest one, 6.25 M, the ratio is
    // 1.73, at r_ms itself 1.79: "1.7" is read as the tabulated point (REPORT 0.55.2).
    {
        const double r = 6.25, lam = c75EquatorialLambda(r, 0), ratio = discShiftKerr(r, lam, 0) / discShiftKerr(r, 0, 0);
        check(ratio >= 1.7 && ratio < 1.75, "C75 p. 792: 1.7 at a = 0 (C75's tabulated r = 6.25 M next to r_ms): " + f(ratio));
        const double rm = discInner(0), l2 = c75EquatorialLambda(rm, 0);
        check(std::fabs(discShiftKerr(rm, l2, 0) / discShiftKerr(rm, 0, 0) - 1.79) < 0.01, "the same at r_ms = 6M: 1.79 (C75's 1.7 is the table's point, not r_ms): " + f(discShiftKerr(rm, l2, 0) / discShiftKerr(rm, 0, 0)));
    }
    {
        const double a = 0.9981, r = discInner(a), lam = c75EquatorialLambda(r, a), ratio = discShiftKerr(r, lam, a) / discShiftKerr(r, 0, a);
        check(std::fabs(ratio - 15) < 0.5, "C75 p. 792: 15 at a = 0.9981 (at r_ms): " + f(ratio));
    }
    // the disc with spin: the inner edge at BPT's r_ms (2.21), the hottest ring further in and hotter, the light still efficiency x feeding rate
    {
        DiscPhys p0, p9;
        p0.aSigned = 0; p9.aSigned = 0.9;
        double w0r, w9r;
        const double t0 = p0.hottest(&w0r), t9 = p9.hottest(&w9r);
        check(std::fabs(discInner(0.9) - innermostStable(0.9, true)) < 1e-12, "the disc's inner edge at spin 0.9 = BPT (2.21): " + f(discInner(0.9)));
        check(w9r < w0r && t9 > t0, "spin 0.9 against 0 (10^8 Suns, 0.1 L_Edd): the hottest ring moves in (" + f(w0r) + " -> " + f(w9r) + " M) and is hotter (" + f(t0) + " -> " + f(t9) + " K)");
        const double L = discLuminosity(0.9, 1e300) * p9.mdot() * 299792458.0 * 299792458.0, want = p9.lEdd * eddingtonLuminosity(p9.massSun);
        check(std::fabs(L / want - 1) < 1e-7, "spin 0.9: the disc's light = efficiency x feeding rate (PT (30), (31a)): " + f(L / want));
    }
}

// the camera: J15 (A.9), (A.10) as special-relativity aberration; a static observer's own frame built from the metric gives the same direction
static void testSpinCamera() {
    unsigned seed = 7;
    const auto rnd = [&] { seed = seed * 1103515245u + 12345u; return ((seed >> 8) & 0xffffff) / 16777216.0 * 2 - 1; };
    double w1 = 0, w2 = 0;
    for (int k = 0; k < 200; k++) {
        Vec3 B{rnd(), rnd(), rnd()}, N{rnd(), rnd(), rnd()};
        B = B * (1 / length(B)); N = N * (1 / length(N));
        const double beta = 0.95 * std::fabs(rnd());
        const Vec3 n = aberrate(N, beta, B);
        // J15 (A.9), (A.10) literally: N's Cartesian components on e_x, e_y = B, e_z (the triad (A.10) is built from)
        const double kap = std::sqrt(1 - B.y * B.y);
        const Vec3 ex{B.z / kap, 0, -B.x / kap}, ez{B.x * B.y / kap, -kap, B.y * B.z / kap};   // the columns of (A.10): e_r, e_theta, e_phi = (x, y, z)
        const double Nx = dot(N, ex), Ny = dot(N, B), Nz = dot(N, ez), den = 1 - beta * Ny, g = std::sqrt(1 - beta * beta);
        const double nFy = (-Ny + beta) / den, nFx = -g * Nx / den, nFz = -g * Nz / den;
        const Vec3 lit{B.z / kap * nFx + B.x * nFy + B.x * B.y / kap * nFz, B.y * nFy - kap * nFz, -B.x / kap * nFx + B.z * nFy + B.y * B.z / kap * nFz};
        w1 = std::max(w1, length(n - lit));
        // special relativity: the light's angle to the motion, cos' = (cos + beta) / (1 + beta cos) with cos = -N . B (its direction of motion on the camera's axes)
        const double c = -Ny, cF = (c + beta) / (1 + beta * c);
        w2 = std::max(w2, std::fabs(dot(n, B) - cF) + std::fabs(length(n) - 1));
    }
    check(w1 < 1e-13, "aberrate = J15 (A.9) with (A.10) written out: " + f(w1));
    check(w2 < 1e-13, "it is special-relativity aberration (angle to the motion, unit length): " + f(w2));
    check(length(aberrate(Vec3{0.3, -0.4, std::sqrt(0.75)}, 0, Vec3{0, 0, 1}) + Vec3{0.3, -0.4, std::sqrt(0.75)}) < 1e-15, "beta = 0: n_F = -N (the hovering FIDO)");
    // a static observer (Omega = 0) moves against the FIDO at beta = -omega varpi / alpha along e_phi ((A.7)'s speed with Omega = 0); its own frame from the
    // metric: u = d/dt / (-g_tt)^(1/2), e_phi' the unit vector in the (t, phi) plane orthogonal to u. The light's direction cosine along e_phi' measured
    // there must be the camera's N_phi after launch with that beta.
    double w3 = 0;
    for (double a : {0.5, 0.99})
        for (double r : {2.5, 6.0, 30.0})
            for (int k = 0; k < 8; k++) {
                const double th = 1.1;
                const KerrMetric m = kerrMetric(a, r, th);
                Camera cam{r, th, 0};
                cam.beta = -m.omega * m.varpi / m.alpha; cam.B = Vec3{0, 0, 1};
                if (cam.beta < 0) { cam.beta = -cam.beta; cam.B = Vec3{0, 0, -1}; }
                const Vec3 N = lookAt(0.3 + 0.35 * k, 0.8 * k);
                const KerrBL ch(a);
                Ray ray;
                ch.launch(cam, N, ray);
                const double b = ray.c[0], pth = ray.y[4], pr = ray.y[3];
                const double gtt = -m.alpha * m.alpha + m.omega * m.omega * m.varpi * m.varpi, gtp = -m.omega * m.varpi * m.varpi, gpp = m.varpi * m.varpi;
                // contravariant p from p_t = -1, p_phi = b: p^t = g^tt p_t + g^tphi p_phi etc. with the inverse of the (t, phi) block
                const double det = gtt * gpp - gtp * gtp, pt_ = (gpp * -1 - gtp * b) / det, pp_ = (-gtp * -1 + gtt * b) / det;
                const double ut = 1 / std::sqrt(-gtt);
                const double E = -(-1) * ut;                               // -p . u = -p_t u^t
                // e_phi' = (g_tphi d/dt - g_tt d/dphi) / norm, orthogonal to d/dt; its covariant pairing with p: p_t e^t + p_phi e^phi
                const double ex_t = gtp, ex_p = -gtt, nrm = std::sqrt(std::fabs(gtt * ex_t * ex_t + 2 * gtp * ex_t * ex_p + gpp * ex_p * ex_p));
                const double pPhi = (-1 * ex_t + b * ex_p) / nrm;
                const double nPhi = pPhi / E;                             // the light's direction of motion along e_phi' (sign: e_phi' points to +phi)
                (void)pt_; (void)pp_; (void)pth; (void)pr;
                w3 = std::max(w3, std::fabs(-nPhi - N.z));
            }
    check(w3 < 1e-12, "a static camera: launch with beta = -omega varpi / alpha sees the same N_phi as its own frame built from the metric: " + f(w3));
    // J15 (A.16) at beta = 0 = kerr.h hoverShift
    check(std::fabs(cameraShift(0.9, Camera{5, 1.2, 0}, Vec3{0, 0, 1}, 2.0) - hoverShift(0.9, 5, 1.2, 2.0)) < 1e-15, "J15 (A.16) at beta = 0 = the FIDO's shift");
}

// symmetries and the spin -> 0 limit of the rays (no table)
static void testSpinSymmetry() {
    double wNS = 0, wLR = 0, wU = 0;
    for (double a : {0.6, 0.95})
        for (double rc : {4.0, 20.0})
            for (int k = 0; k < 6; k++) {
                const double th = 1.1, psi = 0.2 + 0.45 * k, chi = 0.4 + 0.9 * k;
                const SpinHit h = spinRay(a, Camera{rc, th, 0}, psi, chi);
                const SpinHit s = spinRay(a, Camera{rc, kPi - th, 0}, psi, kPi - chi);    // north-south mirror: N_theta -> -N_theta
                const SpinHit m = spinRay(-a, Camera{rc, th, 0}, psi, -chi);              // spin reversed: N_phi -> -N_phi
                if (h.sky != s.sky || h.sky != m.sky) { wNS = wLR = 1; continue; }
                wNS = std::max({wNS, h.sky ? length(h.dir - Vec3{s.dir.x, s.dir.y, -s.dir.z}) : 0.0, std::fabs(h.b - s.b)});
                wLR = std::max({wLR, h.sky ? length(h.dir - Vec3{m.dir.x, -m.dir.y, m.dir.z}) : 0.0, std::fabs(h.b + m.b)});
                for (int q = 0; q < kSpinCross; q++) { wU = std::max({wU, std::fabs(h.u[q] - s.u[q]), std::fabs(h.u[q] - m.u[q])}); }
            }
    check(wNS < 1e-7, "north-south mirror: the camera at pi - theta sees the mirror picture (far direction, b): " + f(wNS));
    check(wLR < 1e-7, "reversing the spin mirrors the picture left-right (far direction, b): " + f(wLR));
    check(wU < 1e-5, "both mirrors: the disc crossings' u = 1/r (and their continuations, spin_sky.h) agree: " + f(wU));
    // spin 1e-6 against the closed forms of a = 0 (schwarzschild.h bendFromCamera: the far direction; disc.h discRay: the disc's radius and g)
    double wd = 0, wr = 0, wg = 0;
    int n = 0;
    for (double rc : {4.0, 20.0})
        for (int k = 0; k < 10; k++) {
            const double th = 1.3, psi = 0.15 + 0.29 * k, chi = 0.3 + 0.61 * k;
            const Camera cam{rc, th, 0};
            const SpinHit h = spinRay(1e-6, cam, psi, chi);
            // the a = 0 closed form: psi_s from the outward radial direction is pi - psi; the ray bends in the plane of e_r and the look direction
            const Vec3 er{std::sin(th), 0, std::cos(th)}, et{std::cos(th), 0, -std::sin(th)}, ep{0, 1, 0};
            const Vec3 N = lookAt(psi, chi), look = er * N.x + et * N.y + ep * N.z;
            const Bend bd = bendFromCamera(rc, kPi - psi);
            if (bd.captured != !h.sky) { wd = 1; continue; }
            if (h.sky) wd = std::max(wd, angleBetween(h.dir, bentDirection(er, look, N.x, bd.dphi)));
            // the disc: A = e_r . z, B = t . z with t the unit look direction across e_r; Lz / b = (t x e_r) . z
            Vec3 t = look - er * N.x;
            t = t * (1 / length(t));
            DiscGeom g{discInner(0), 1000};
            const DiscHit dh = discRay(rc, kPi - psi, er.z, t.z, cross(t, er).z, g);
            const SpinDisc sd = spinDisc(h, 1e-6, g.rIn, g.rOut);
            if (dh.order != sd.order) { wr = 1; continue; }
            if (sd.order >= 0) { wr = std::max(wr, std::fabs(sd.r / dh.r - 1)); wg = std::max(wg, std::fabs(sd.g / dh.g - 1)); n++; }
        }
    check(wd < 2e-6, "spin 1e-6: the integrator's far direction = the a = 0 closed form (Luminet, Gralla & Lupsasca): " + f(wd));
    check(n >= 5 && wr < 2e-6 && wg < 2e-6, "spin 1e-6: the disc's radius and g = the a = 0 reference (disc.h discRay), " + f(n) + " hits: r " + f(wr) + " g " + f(wg));
}

// the table: against the integrator away from the edges, spin -> 0 against the closed forms' picture, the stars by inverting it
static void testSpinTable() {
    unsigned seed = 99;
    const auto rnd = [&] { seed = seed * 1103515245u + 12345u; return ((seed >> 8) & 0xffffff) / 16777216.0; };
    // two fine tables: a moderate spin and the top of the control (kSpinTop), close in and far out, near the plane
    struct Case { double a, rc, th; };
    for (const Case cs : {Case{0.9, 20, 1.3}, Case{kSpinTop, 6, 1.4}}) {
        const SpinTable T(cs.a, cs.rc, cs.th, kSpinChiFine, kSpinSFine, kSpinEdge, nullptr, 0, kSpinFineFill);
        const std::string at = "a " + f(cs.a) + " r " + f(cs.rc) + ": ";
        check(T.ok() && T.mismatches() == 0, at + "a fine table (" + f(T.rays()) + " rays, " + f(T.fillMs()) + " ms): every ray ends on its side of the closed edge: " + f(T.mismatches()));
        const Camera cam{cs.rc, cs.th, 0};
        double band[3] = {0, 0, 0}, wu[2] = {0, 0}, wb = 0, wg[2] = {0, 0}, we = 0;
        for (int k = 0; k < 150; k++) {
            const double chi = 2 * kPi * rnd(), pe = T.edge(chi);
            we = std::max(we, std::fabs(pe - shadowEdge(cs.a, cam, chi)));
            const int b = k % 3;
            const double x = pe * std::pow(10.0, b == 0 ? -5 + 2 * rnd() : b == 1 ? -3 + 2 * rnd() : -1 + rnd());   // x / psi_e: 1e-5..1e-3, 1e-3..1e-1, 0.1..1
            for (int side = 0; side < 2; side++) {
                const double psi = side == 0 ? pe + x : pe - x;
                if (psi <= 0 || psi >= kPi) continue;
                const SpinHit ref = spinRay(cs.a, cam, psi, chi, 1e-12), tb = T.lookup(psi, chi);
                if (side == 0) band[b] = std::max(band[b], angleBetween(ref.dir, tb.dir));
                wb = std::max(wb, std::fabs(ref.b - tb.b) / (1 + std::fabs(ref.b)));
                const double rin = discInner(cs.a);
                const SpinDisc dr = spinDisc(ref, cs.a, rin, 100), dt = spinDisc(tb, cs.a, rin, 100);
                // away from the disc's edges in r and from the folds where a ray grazes the plane (two crossings at nearly one radius: a pair of images begins there)
                bool fold = false;
                for (int q = 0; q + 1 < kSpinCross; q++) if (ref.u[q] > 0 && ref.u[q + 1] > 0 && std::fabs(ref.u[q] - ref.u[q + 1]) < 0.3 * ref.u[q]) fold = true;
                if (dr.order >= 0 && dr.order == dt.order && !fold && std::fabs(dr.r - rin) > 0.05 && std::fabs(dr.r - 100) > 1) {
                    const int bd = dr.order <= 1 && x >= 1e-2 ? 0 : 1;
                    wu[bd] = std::max(wu[bd], std::fabs(dt.r / dr.r - 1)); wg[bd] = std::max(wg[bd], std::fabs(dt.g / dr.g - 1));
                }
            }
        }
        check(we < 1e-9, at + "the edge's cubic through 2048 samples = the closed test: " + f(we) + " rad");
        check(band[0] < 2e-2 && band[1] < 2e-3 && band[2] < 2e-3, at + "the table's far direction against the integrator, x / psi_e 1e-5..1e-3: " + f(band[0]) + ", 1e-3..0.1: " + f(band[1]) + ", 0.1..1: " + f(band[2]) + " rad");
        check(wb < 1e-4, at + "the table's b against the integrator (its effect on g is in the next two checks): " + f(wb));
        check(wu[0] < 5e-3 && wg[0] < 5e-3, at + "the disc's radius and g, direct and second image beyond 1e-2 rad from the shadow (away from the disc's edges and folds): r " + f(wu[0]) + " g " + f(wg[0]));
        check(wu[1] < 5e-2 && wg[1] < 5e-2, at + "the same for the further images and within 1e-2 rad of the shadow: r " + f(wu[1]) + " g " + f(wg[1]));
    }
    const SpinTable T(0.9, 20, 1.3, kSpinChiFine, kSpinSFine, kSpinEdge, nullptr, 0, kSpinFineFill);
    const Camera cam{20, 1.3, 0};
    // spin 1e-6: the table against the closed forms of 0.55.1 (LensTable: far direction; DiscTable: radius and g)
    {
        const double rc = 20, th = 1.3;
        const SpinTable S(1e-6, rc, th, kSpinChiFine, kSpinSFine, kSpinEdge, nullptr, 0, kSpinFineFill);
        const LensTable L(rc);
        const DiscTable D(rc);
        double wd = 0, wr = 0, wgg = 0;
        int hits = 0;
        for (int k = 0; k < 120; k++) {
            const double chi = 2 * kPi * rnd(), psi = kPi * rnd();
            const Vec3 er{std::sin(th), 0, std::cos(th)}, et{std::cos(th), 0, -std::sin(th)}, ep{0, 1, 0};
            const Vec3 N = lookAt(psi, chi), look = er * N.x + et * N.y + ep * N.z;
            const SpinHit h = S.lookup(psi, chi);
            const double x = psi - shadowAngle(rc);
            if (std::fabs(x) < 1e-3) continue;
            double dphi;
            const bool sky = L.lookupX(kPi - shadowAngle(rc) - (kPi - psi), dphi);
            if (sky != h.sky) { wd = 1; continue; }
            if (sky) wd = std::max(wd, angleBetween(h.dir, bentDirection(er, look, N.x, dphi)));
            Vec3 t = look - er * N.x;
            t = t * (1 / length(t));
            const DiscGeom g{discInner(0), 100};
            const DiscHit dh = discRayTable(D, x, er.z, t.z, cross(t, er).z, g);
            const SpinDisc sd = spinDisc(h, 1e-6, g.rIn, g.rOut);
            if (dh.order >= 0 && sd.order == dh.order && std::fabs(dh.r - g.rIn) > 0.05 && std::fabs(dh.r - g.rOut) > 1) {
                wr = std::max(wr, std::fabs(sd.r / dh.r - 1)); wgg = std::max(wgg, std::fabs(sd.g / dh.g - 1)); hits++;
            }
        }
        check(wd < 3e-4, "spin 1e-6: the spin table's far direction = 0.55.1's closed-form picture (LensTable), |x| > 1e-3: " + f(wd));
        check(hits >= 10 && wr < 5e-3 && wgg < 5e-3, "spin 1e-6: the spin table's disc radius and g = 0.55.1's (DiscTable), " + f(hits) + " hits: " + f(wr) + " " + f(wgg));
    }
    // the stars: a uniformly bright sky stays uniformly bright (lensing keeps surface brightness): averaged over far directions, the summed magnification of
    // all images = the camera's sky outside the shadow over the whole sky
    {
        const SpinStars St(T);
        double sum = 0;
        const int n = 4000;
        std::vector<SpinImage> im;
        for (int k = 0; k < n; k++) {
            const double z = 1 - 2 * (k + 0.5) / n, ph = k * 2.399963229728653, s = std::sqrt(1 - z * z);   // a Fibonacci sphere
            im.clear();
            St.images(Vec3{s * std::cos(ph), s * std::sin(ph), z}, im);
            for (const SpinImage& q : im) sum += q.mag;
        }
        double shadow = 0;
        const int m = 4096;
        for (int k = 0; k < m; k++) shadow += (1 - std::cos(T.edge(2 * kPi * (k + 0.5) / m))) * 2 * kPi / m;   // the cap under the edge
        const double want = (4 * kPi - shadow) / (4 * kPi), got = sum / n;
        check(std::fabs(got / want - 1) < 0.01, "a uniform sky stays uniform: summed magnifications over the far sky = the sky outside the shadow, " + f(got) + " against " + f(want));
        // a star's magnification from the table = the ratio of solid angles from neighbouring rays of the integrator (Riazuelo (51))
        double wm = 0;
        int tested = 0;
        for (int k = 0; k < 6; k++) {
            const double chi = 0.5 + k, pe = T.edge(chi), psi = pe + 0.05 + 0.3 * k;
            const SpinHit c0 = spinRay(0.9, cam, psi, chi, 1e-12);
            im.clear();
            St.images(c0.dir, im);
            const SpinImage* best = nullptr;
            for (const SpinImage& q : im) if (!best || std::hypot(q.psi - psi, std::remainder(q.chi - chi, 2 * kPi)) < std::hypot(best->psi - psi, std::remainder(best->chi - chi, 2 * kPi))) best = &q;
            if (!best) { wm = 1; continue; }
            const double h = 1e-5;
            const Vec3 a1 = spinRay(0.9, cam, psi + h, chi, 1e-12).dir, a0 = spinRay(0.9, cam, psi - h, chi, 1e-12).dir;
            const Vec3 b1 = spinRay(0.9, cam, psi, chi + h, 1e-12).dir, b0 = spinRay(0.9, cam, psi, chi - h, 1e-12).dir;
            const double far = length(cross(a1 - a0, b1 - b0)) / (4 * h * h), camA = std::sin(psi);   // dOmega_far / (dpsi dchi) and dOmega_cam / (dpsi dchi)
            wm = std::max(wm, std::fabs(best->mag / (camA / far) - 1));
            tested++;
        }
        check(tested == 6 && wm < 0.02, "a star's magnification from the table = ratio of solid angles of the integrator's neighbouring rays: " + f(wm));
        // 0.55.6: the bins built on all cores give every star the same images as 0.55.5's one-thread build (kept as SpinStars::kReference): the same
        // number, in the same order, at the same place and brightness, to the last bit; for every catalogue star and 4000 directions over the whole sky
        const SpinStars R(T, SpinStars::kReference), P2(T, 2), P0(T);
        long dirs = 0, imgs = 0, diff = 0;
        std::vector<SpinImage> ia, ib, ic;
        const auto same = [&](const Vec3& d) {
            ia.clear(); ib.clear(); ic.clear();
            R.images(d, ia); P2.images(d, ib); P0.images(d, ic);
            dirs++;
            imgs += static_cast<long>(ia.size());
            if (ia.size() != ib.size() || ia.size() != ic.size()) { diff++; return; }
            for (std::size_t q = 0; q < ia.size(); q++)
                if (ia[q].psi != ib[q].psi || ia[q].chi != ib[q].chi || ia[q].mag != ib[q].mag || ia[q].psi != ic[q].psi || ia[q].chi != ic[q].chi || ia[q].mag != ic[q].mag) diff++;
        };
        for (const orbit::SkyStar& st : orbit::skyData().stars) same(st.dir);
        for (int k = 0; k < n; k++) { const double z = 1 - 2 * (k + 0.5) / n, ph = k * 2.399963229728653, s = std::sqrt(1 - z * z); same(Vec3{s * std::cos(ph), s * std::sin(ph), z}); }
        check(diff == 0 && imgs > 10000, "the stars' bins on all cores = 0.55.5's one-thread build: " + f(dirs) + " directions, " + f(imgs) + " images, " + f(diff) + " differ (place or brightness)");
    }
}

// 0.55.8: Mino's form of the ray equations (mino.h) against KerrBL (geodesic.h, J15 (A.15)), the integrator of every earlier version
static void testMino() {
    unsigned seed = 2024;
    const auto rnd = [&] { seed = seed * 1103515245u + 12345u; return ((seed >> 8) & 0xffffff) / 16777216.0; };
    // 1. the same equations: at random states on a light ray, Mino's rates = rho^2 times KerrBL's, turned into u = 1/r, mu = cos theta, s = -lambda:
    //    du/ds = rho^2 u^2 dr/dzeta, dmu/ds = rho^2 sin theta dtheta/dzeta, dphi/ds = -rho^2 dphi/dzeta, and the second derivative of u from KerrBL's chain rule
    //    d(du/ds)/ds = -rho^2 d/dzeta (u^2 Delta p_r) = -rho^2 [(2/r^2 - 2 a^2/r^3) dr/dzeta p_r + (1 - 2/r + a^2/r^2) dp_r/dzeta]; of mu likewise from
    //    dmu/ds = sin theta p_theta: -rho^2 [cos theta dtheta/dzeta p_theta + sin theta dp_theta/dzeta]
    double w = 0;
    for (int k = 0; k < 200; k++) {
        const double a = std::vector<double>{0.01, 0.6, 0.9, 0.99}[static_cast<std::size_t>(k % 4)];
        const KerrBL ch(a);
        const Camera cam{horizon(a) + 0.5 + 40 * rnd(), 0.2 + 2.7 * rnd(), 0};
        const Vec3 look = lookAt(kPi * rnd(), 2 * kPi * rnd());
        Ray ray;
        if (!ch.launch(cam, look, ray)) continue;
        const double* y = ray.y;
        double dz[kMaxState];
        ch.rhs(ray, y, dz);
        const double r = y[0], th = y[1], u = 1 / r, c = std::cos(th), sn = std::sin(th), rho2 = r * r + a * a * c * c, D = r * r - 2 * r + a * a;
        const double ym[5] = {u, u * u * D * y[3], c, sn * y[4], y[2]};
        double dm[5];
        minoRhs(a, ray.c[0], ray.c[1], ym, dm);
        const double want[5] = {rho2 * u * u * dz[0], -rho2 * ((2 / (r * r) - 2 * a * a / (r * r * r)) * dz[0] * y[3] + (1 - 2 / r + a * a / (r * r)) * dz[3]),
                                rho2 * sn * dz[1], -rho2 * (c * dz[1] * y[4] + sn * dz[4]), -rho2 * dz[2]};
        for (int i = 0; i < 5; i++) w = std::max(w, std::fabs(dm[i] - want[i]) / (1e-12 + std::fabs(want[i]) + std::fabs(dm[i]) + 1e-3));
    }
    check(w < 1e-9, "Mino's form: its rates = rho^2 times KerrBL's (J15 (A.15)) at 200 states on light rays, spin 0.01 to 0.99: " + f(w));
    // 2. the rays against KerrBL's trace at 1e-12, both at the fine table's tolerance 1e-10 (the far direction, b, the equator's crossings as u = 1/r, where the ray
    //    ends) and the conservation laws: the two null conditions at the ray's end, relative to their terms
    struct Case { double a, rc, th; };
    for (const Case cs : {Case{0.01, 20, 1.3}, Case{0.6, 74.1, 1.5107}, Case{0.9, 20, 1.396}, Case{0.99, 6, 1.4}, Case{0.9, 8, 0.4}}) {
        const Camera cam{cs.rc, cs.th, 0};
        double dir[2] = {0, 0}, du[2] = {0, 0}, db = 0, nu = 0, nm = 0;
        int ends = 0, n = 0, pole = 0;
        for (int k = 0; k < 48; k++) {
            const double chi = k < 4 ? kPi / 2 * k : 2 * kPi * rnd(), pe = shadowEdge(cs.a, cam, chi);   // (chi 0 and pi: rays over the pole, KerrBL's)
            for (int band = 0; band < 2; band++) for (int side = 0; side < 2; side++) {
                const double x = pe * std::pow(10.0, band == 0 ? -6 + 2 * rnd() : -2 + 2 * rnd()), psi = side ? pe - x : pe + x;
                if (psi <= 0 || psi >= kPi) continue;
                const SpinHit ref = spinRay(cs.a, cam, psi, chi, 1e-12), m = spinRayMino(cs.a, cam, psi, chi, 1e-10);
                TraceOptions o;
                o.rtol = 1e-10; o.atol = 1e-11;
                const MinoEnd e = traceMino(cs.a, cam, lookAt(psi, chi), o);
                n++;
                if (e.nearPole) { pole++; continue; }
                if (m.sky != ref.sky) { ends++; continue; }
                if (ref.sky) dir[band] = std::max(dir[band], angleBetween(m.dir, ref.dir));
                db = std::max(db, std::fabs(m.b - ref.b));
                for (int q = 0; q < kSpinCross; q++) if (ref.u[q] > 0) du[band] = std::max(du[band], std::fabs(m.u[q] / ref.u[q] - 1));
                double eu, em;
                minoNull(cs.a, e.b, e.q, e.y, eu, em);
                nu = std::max(nu, std::fabs(eu)); nm = std::max(nm, std::fabs(em));
            }
        }
        const std::string at = "Mino's form, a " + f(cs.a) + " r " + f(cs.rc) + " theta " + f(cs.th) + ", " + f(n) + " rays (" + f(pole) + " over the pole: KerrBL's): ";
        check(ends == 0 && db == 0, at + "every ray ends where KerrBL's does (" + f(ends) + " differ), the same b (" + f(db) + ")");
        check(dir[0] < 3e-5 && dir[1] < 5e-8, at + "the far direction against KerrBL at 1e-12, x / psi_e 1e-6..1e-4: " + f(dir[0]) + ", 1e-2..1: " + f(dir[1]) + " rad");
        check(du[0] < 1.5e-3 && du[1] < 1.5e-3, at + "the equator's crossings' radii against KerrBL at 1e-12, near: " + f(du[0]) + ", far: " + f(du[1]));
        check(nu < 3e-9 && nm < 3e-9, at + "the null conditions (du/ds)^2 = U(u), (dmu/ds)^2 = Mu(mu) at every ray's end: " + f(nu) + ", " + f(nm));
    }
    // 3. the pole: a ray passing the axis within sin^2 theta = kMinoPole is left to KerrBL (its phi turns by about pi in a stretch shorter than a step); just outside
    //    that, Mino's far direction against KerrBL at 1e-12 (rows at chi near 0 and pi, the camera's own plane through the axis)
    {
        double wp = 0, near = 1;
        for (double a : {0.01, 0.9}) for (double th : {1.3, 0.4}) for (double base : {0.0, kPi}) for (double dc = 1e-6; dc < 0.3; dc *= 1.5) {
            const Camera cam{20, th, 0};
            const double chi = base + dc, pe = shadowEdge(a, cam, chi), psi = pe + 0.05 * (kPi - pe);
            TraceOptions o;
            o.rtol = 1e-10; o.atol = 1e-11;
            const MinoEnd e = traceMino(a, cam, lookAt(psi, chi), o);
            if (e.nearPole || e.kind != RayEnd::Sky) continue;
            const double c3 = e.q + e.b * e.b - a * a, m2 = 2 * e.q / (c3 + std::sqrt(c3 * c3 + 4 * a * a * e.q));
            near = std::min(near, 1 - m2);
            wp = std::max(wp, angleBetween(e.dir, spinRay(a, cam, psi, chi, 1e-12).dir));
        }
        check(near < 2 * kMinoPole && wp < 1e-6, "Mino's form next to the pole (sin^2 theta down to " + f(near) + "): the far direction against KerrBL at 1e-12: " + f(wp) + " rad");
    }
}

// ---------------------------------------------------------------------------------------------------------------------------------------------
// 0.55.9: the spinning hole's closed forms (kerr_closed.h, Gralla & Lupsasca 2020a) and what they need beyond F (elliptic.h)
static double gauss5(const std::function<double(double)>& g, double a, double b, int panels) {
    const double xg[5] = {-0.906179845938664, -0.538469310105683, 0, 0.538469310105683, 0.906179845938664},
                 wg[5] = {0.236926885056189, 0.478628670499366, 0.568888888888889, 0.478628670499366, 0.236926885056189};
    double q = 0;
    for (int i = 0; i < panels; i++) {
        const double l = a + (b - a) * i / panels, r = a + (b - a) * (i + 1) / panels;
        for (int j = 0; j < 5; j++) q += 0.5 * (r - l) * wg[j] * g(0.5 * (l + r) + 0.5 * (r - l) * xg[j]);
    }
    return q;
}
static void testClosedElliptic() {
    // R_C in closed form against the duplication method
    double w = 0;
    for (double x : {0.0, 1e-9, 0.3, 0.999, 0.9995, 1.0, 1.0004, 1.002, 3.0, 1e6}) for (double y : {1e-6, 1.0, 7.0}) w = std::max(w, std::fabs(carlsonRC(x * y, y) / carlsonRCdup(x * y, y) - 1));
    check(w < 1e-14, "R_C in closed form = the duplication method: " + f(w));
    // Pi(n; phi | m), n < 1, any amplitude, m from -30 to 0.99, against a direct quadrature of its integral
    w = 0;
    for (double m : {-30.0, -2.0, -0.3, 0.0, 0.4, 0.9, 0.99}) for (double n : {-5.0, -0.5, 0.3, 0.95}) for (double ph : {0.3, 1.2, 1.57, 2.0, 2.9, -2.5}) {
        const double q = gauss5([&](double t) { const double s = std::sin(t); return 1 / ((1 - n * s * s) * std::sqrt(1 - m * s * s)); }, 0, ph, 4000);
        w = std::max(w, std::fabs(ellPiang(n, std::sin(ph), std::cos(ph), 1 - m) / q - 1));
    }
    check(w < 1e-12, "Pi(n < 1; phi | m) = its quadrature, 168 cases (m -30 to 0.99, phi -2.5 to 2.9): " + f(w));
    // n > 1: the antiderivative continued past the pole (elliptic.h): its difference between two amplitudes on one side of a pole = the quadrature there
    w = 0;
    int nn = 0;
    for (double m : {-30.0, -2.0, 0.0, 0.4, 0.9}) for (double n : {1.5, 4.0, 30.0}) {
        const double p = std::asin(1 / std::sqrt(n));
        const double iv[4][2] = {{0.05, p - 0.05}, {p + 0.03, 1.5}, {1.7, kPi - p - 0.04}, {kPi - p + 0.02, 3.0}};
        for (const auto& ab : iv) {
            if (ab[1] <= ab[0]) continue;
            const double q = gauss5([&](double t) { const double s = std::sin(t); return 1 / ((1 - n * s * s) * std::sqrt(1 - m * s * s)); }, ab[0], ab[1], 20000);
            const double v = ellPiang(n, std::sin(ab[1]), std::cos(ab[1]), 1 - m) - ellPiang(n, std::sin(ab[0]), std::cos(ab[0]), 1 - m);
            w = std::max(w, std::fabs(v / q - 1)); nn++;
        }
    }
    check(nn >= 50 && w < 1e-12, "Pi(n > 1): differences on either side of the poles = the quadrature, " + f(nn) + " intervals: " + f(w));
    // the amplitude: F(am(u | m) | m) = u, m from -30 to 1 - 1e-8, u from -7 to 40 (several periods)
    w = 0;
    for (double m : {-30.0, -2.0, -0.3, 0.0, 0.4, 0.9, 0.99999999}) for (double u : {-7.0, -0.3, 0.1, 1.0, 3.3, 12.0, 40.0}) {
        const double ph = jacobiAm(u, 1 - m), j = std::floor(ph / kPi + 0.5), p = ph - j * kPi;
        w = std::max(w, std::fabs((ellFang(std::sin(p), std::cos(p), 1 - m) + 2 * j * ellKc(1 - m)) / u - 1));
    }
    check(w < 1e-12, "F(am(u | m) | m) = u, m -30 to 1 - 1e-8, u -7 to 40: " + f(w));
}

static void testClosed() {
    testClosedElliptic();
    // 1. the radial roots (GLa (95)): their sum is 0 (GLa (96)), R vanishes at each real one; on the critical curve (GLa (107), (108) at r~ in [r~2, r~3])
    //    r3 = r4 = r~ (GLa: "On the critical curve C+, we have r3 = r4 > r+")
    double ws = 0, wr = 0, wc = 0;
    for (double a : {0.01, 0.6, 0.9, 0.99}) for (int k = 0; k < 40; k++) {
        const double lam = -10 + 20.0 * k / 39, eta = 0.5 + 0.7 * k;
        const ClosedRadial c = closedRoots(a, lam, eta);
        ws = std::max(ws, std::fabs(c.r1 + c.r2 + c.r3 + c.r4) / (std::fabs(c.r1) + std::fabs(c.r4)));
        const auto R = [&](double r) { const double P = r * r + a * a - a * lam, D = r * r - 2 * r + a * a; return P * P - D * (eta + (lam - a) * (lam - a)); };
        for (double r : {c.r1, c.r2, c.r3, c.r4}) if (c.im1 == 0 && c.im3 == 0) wr = std::max(wr, std::fabs(R(r)) / (r * r * r * r + std::fabs(lam * lam + eta) * r * r + 1));
        const double r2t = 2 * (1 + std::cos(2 * std::acos(-a) / 3)), r3t = 2 * (1 + std::cos(2 * std::acos(a) / 3)), rt = r2t + (r3t - r2t) * (k + 0.5) / 40;
        const ClosedRadial cc = closedRoots(a, criticalLambda(a, rt), criticalEta(a, rt));
        wc = std::max(wc, std::max(std::fabs(cc.r3 - rt), std::fabs(cc.r4 - rt)) / rt);
    }
    check(ws < 1e-13 && wr < 1e-12, "the radial roots: r1 + r2 + r3 + r4 = 0 (GLa (96)): " + f(ws) + "; R(r_i) = 0: " + f(wr));
    check(wc < 1e-5, "on the critical curve (GLa (107), (108)) r3 = r4 = r~ (a double root; found to the square root of the rounding): " + f(wc));
    // 2. the radial path integrals from a radius to infinity (GLa App. B, every substitution that reaches infinity) against a direct quadrature of (B4)
    {
        unsigned seed = 11;
        const auto rnd = [&] { seed = seed * 1103515245u + 12345u; return ((seed >> 8) & 0xffffff) / 16777216.0; };
        double wi[6][3] = {};
        int cnt[6] = {};
        for (int k = 0; k < 6000; k++) {
            const double a = std::vector<double>{0.01, 0.6, 0.9, 0.99}[static_cast<std::size_t>(k % 4)];
            const double lam = k % 8 < 4 ? -12 + 24 * rnd() : -2 + 4 * rnd(), eta = k % 8 < 4 ? -1 + 40 * rnd() : -1 + 4 * rnd();
            if (eta < 0 && (std::fabs(lam) > a || eta < -(std::fabs(lam) - a) * (std::fabs(lam) - a))) continue;   // outside the allowed region (GLa (24))
            const double rp = horizon(a), rm = 2 - rp, rs = rp + 0.05 + 30 * rnd();
            const auto R = [&](double r) { const double P = r * r + a * a - a * lam, D = r * r - 2 * r + a * a; return P * P - D * (eta + (lam - a) * (lam - a)); };
            if (R(rs) <= 0) continue;
            const ClosedRadial c = closedRoots(a, lam, eta);
            const int sub = c.im1 != 0 ? 4 : c.im3 != 0 ? 3 : c.r4 > rp ? (rs <= c.r3 ? 1 : 2) : 5;
            if (sub == 1) continue;
            double I0, Ip, Im;
            if (!closedRadialIntegrals(a, lam, eta, rs, std::numeric_limits<double>::infinity(), I0, Ip, Im)) { check(false, "radial integrals: no answer"); continue; }
            double q[3] = {0, 0, 0};
            for (int j = 0; j < 3; j++) q[j] = gauss5([&](double t) { const double r = rs + t / (1 - t), g = 1 / ((1 - t) * (1 - t) * std::sqrt(R(r))); return j == 0 ? g : g / (r - (j == 1 ? rp : rm)); }, 0, 1, 2000);
            const double e[3] = {std::fabs(I0 / q[0] - 1), std::fabs(Ip / q[1] - 1), std::fabs(Im / q[2] - 1)};
            cnt[sub]++;
            for (int j = 0; j < 3; j++) wi[sub][j] = std::max(wi[sub][j], e[j]);
        }
        for (int sub : {2, 3, 4, 5}) {
            // (case (2): near lambda = (r+^2 + a^2) / a the root r3 meets r+ (R(r+) = P(r+)^2 = 0) and the two terms of I+ (B39) cancelled; 0.56.2 writes
            // (B39) without the division by r+ - r3 (kerr_closed.cpp Ipm), so I+ is held to 1e-8 there too (0.55.9 allowed 1e-2))
            // (case (3) at small spin: r- within a relative 1e-4 of r2, I_- is taken as the integral with r2 in its place (kerr_closed.cpp nearR2): 2e-5 of I_-,
            // whose weight in I_phi is of order a^2)
            const double lim = 1e-8, limM = sub == 3 ? 2e-5 : 1e-8;
            check(cnt[sub] > 0 && wi[sub][0] < 1e-11 && wi[sub][1] < lim && wi[sub][2] < limM,
                  "radial path integrals to infinity, substitution " + f(sub) + " (5: case II), " + f(cnt[sub]) + " cases: I_r " + f(wi[sub][0]) + ", I_+ " + f(wi[sub][1]) + ", I_- " + f(wi[sub][2]));
        }
    }
    // 3. every ray of the camera against KerrBL at 1e-12 (the reference of testMino): spin 0.01, 0.6, 0.9, 0.99, cameras far and close in (inside the
    //    photon shell: case (1)), near the axis (vortical rays, case (4)); rays near and far from the shadow's edge, both sides, the rows over the poles
    //    (chi 0, pi: lambda = 0). Counted per substitution and polar type, so that no case is left untested.
    {
        unsigned seed = 7;
        const auto rnd = [&] { seed = seed * 1103515245u + 12345u; return ((seed >> 8) & 0xffffff) / 16777216.0; };
        struct Case { double a, rc, th; };
        int tot[6] = {}, vort = 0, all = 0;
        for (const Case cs : {Case{0.01, 20, 1.3}, Case{0.6, 74.1, 1.5107}, Case{0.9, 20, 1.396}, Case{0.99, 6, 1.4}, Case{0.9, 8, 0.4}, Case{0.9, 2.5, 1.2}, Case{0.6, 3, 0.3}}) {
            const Camera cam{cs.rc, cs.th, 0};
            double dir[2] = {0, 0}, du = 0, db = 0;
            int ends = 0, lost = 0, n = 0, crossN = 0;
            for (int k = 0; k < 60; k++) {
                const double chi = k < 4 ? kPi / 2 * k : 2 * kPi * rnd(), pe = shadowEdge(cs.a, cam, chi);
                for (int band = 0; band < 2; band++) for (int side = 0; side < 2; side++) {
                    const double x = pe * std::pow(10.0, band == 0 ? -6 + 2 * rnd() : -2 + 2 * rnd());
                    const double psi = side == 0 && band == 1 ? pe + (kPi - pe) * rnd() : side ? pe - x : pe + x;
                    if (psi <= 0 || psi >= kPi) continue;
                    const SpinHit ref = spinRay(cs.a, cam, psi, chi, 1e-12);
                    const ClosedEnd e = traceClosed(cs.a, cam, lookAt(psi, chi), kSpinCross);
                    n++;
                    if (e.kind == RayEnd::Lost) { lost++; continue; }
                    tot[e.radial]++; vort += e.vortical;
                    if ((e.kind == RayEnd::Sky) != ref.sky) { ends++; continue; }
                    db = std::max(db, std::fabs(e.b - ref.b));
                    if (ref.sky) dir[band] = std::max(dir[band], angleBetween(e.dir, ref.dir));
                    for (int q = 0; q < kSpinCross; q++) {
                        if (!(ref.u[q] > 0 && ref.u[q] < 1 / (horizon(cs.a) + 2e-3))) continue;   // (the real crossings; spinRay continues the rest inside the horizon)
                        crossN++;
                        du = std::max(du, q < e.nCross ? std::fabs(e.u[q] / ref.u[q] - 1) : 1.0);
                    }
                }
            }
            all += n;
            const std::string at = "closed forms, a " + f(cs.a) + " r " + f(cs.rc) + " theta " + f(cs.th) + ", " + f(n) + " rays: ";
            check(lost == 0 && ends == 0 && db == 0, at + "every ray answered (" + f(lost) + " not), ends where KerrBL's does (" + f(ends) + " differ), the same b (" + f(db) + ")");
            check(dir[0] < 3e-6 && dir[1] < 5e-8, at + "the far direction against KerrBL at 1e-12, x / psi_e 1e-6..1e-4: " + f(dir[0]) + " rad (testMino's limit 3e-5), farther: " + f(dir[1]) + " (5e-8)");
            check(du < 1e-6, at + f(crossN) + " crossings of the equator, their radii against KerrBL at 1e-12: " + f(du) + " (testMino's limit 1.5e-3)");
        }
        std::printf("info: closed forms against KerrBL: %d rays; substitution (1) %d, (2) %d, (3) %d, (4) %d, case II %d; polar type A %d, B (vortical) %d\n", all, tot[1], tot[2],
                    tot[3], tot[4], tot[5], all - vort, vort);
        check(tot[1] > 0 && tot[2] > 0 && tot[3] > 0 && tot[4] > 0 && tot[5] > 0 && vort > 0, "closed forms: every radial substitution, case II and both polar types met by the rays above");
    }
    // 4. spin towards 0 against the non-spinning closed form (schwarzschild.h, Luminet 1979; testIntegratorVsClosed's rays and limit, camera in the plane).
    //    Near the edge the far direction changes as 1/x with b, and the spin moves b's critical value by about a: so spin 1e-12 for the limit, and the
    //    difference at spins 1e-9, 1e-10, 1e-11 shrinks in proportion to the spin (the first-order effect of the spin, not a rounding floor).
    //    (i = 0, psi = 0: a ray straight out along e_r has eta + (lambda - a)^2 = 0, GLa's excluded principal null ray (77); not used.)
    double wa[4] = {0, 0, 0, 0};
    for (int ia = 0; ia < 4; ia++) {
        const double a = std::vector<double>{1e-12, 1e-11, 1e-10, 1e-9}[static_cast<std::size_t>(ia)];
        double worst = 0;
        int n = 0;
        for (double rO : {2.5, 3.5, 6.0, 30.0, 1000.0}) {
            const Camera cam{rO, kPi / 2, 0};
            const double edge = kPi - shadowAngle(rO);
            for (int i = 1; i < 31; i++) {
                const double x = edge * std::pow(1e-4, i / 29.0), psi = edge - x, tilt = 0.37 + 0.9 * i;
                const Vec3 look{std::cos(psi), std::sin(psi) * std::sin(tilt), std::sin(psi) * std::cos(tilt)};
                const ClosedEnd e = traceClosed(a, cam, look, 0);
                const Bend B = bendFromCamera(rO, psi);
                if (e.kind != RayEnd::Sky || B.captured) { check(false, "spin " + f(a) + ": a ray outside the shadow escapes, rO " + f(rO)); continue; }
                const Vec3 d = bentDirection(Vec3{1, 0, 0}, Vec3{look.x, look.z, -look.y}, look.x, B.dphi);
                worst = std::max(worst, angleBetween(d, e.dir) / (2e-8 + 1e-11 / x));
                n++;
            }
        }
        wa[ia] = worst;
        if (ia == 0) check(n == 150 && worst < 1, "closed forms at spin 1e-12 = the non-spinning closed form (schwarzschild.h), 150 rays to 1e-4 rad from the edge: worst " + f(worst) + " of the limit 2e-8 + 1e-11/x rad");
    }
    check(std::fabs(wa[2] / wa[1] - 10) < 0.5 && std::fabs(wa[3] / wa[2] - 10) < 0.5, "closed forms: the difference from the non-spinning form grows in proportion to the spin (1e-11, 1e-10, 1e-9: "
          + f(wa[1]) + ", " + f(wa[2]) + ", " + f(wa[3]) + " of the limit)");
}

// 0.55.8: the table made while the view moves (kSpinMoving) against the integrator at full tolerance (KerrBL, 1e-12): no worse than 0.55.7's coarse table
// (KerrBL at 1e-8 for every ray, the same grid) on the same rays, in each band of x / psi_e, its b, and its disc's radius and g (the measures of testSpinTable;
// within 2 %: two integrators of one tolerance, sampled at 480 places); each table's numbers are printed (REPORT 0.55.8)
static void testMovingTable() {
    struct Case { double a, rc, th; };
    for (const Case cs : {Case{0.01, 20, 1.3}, Case{0.6, 74.1, 1.5107}, Case{0.9, 20, 1.396}, Case{0.99, 6, 1.4}, Case{0.9, 8, 0.4}}) {
        const Camera cam{cs.rc, cs.th, 0};
        unsigned seed = 7;
        const auto rnd = [&] { seed = seed * 1103515245u + 12345u; return ((seed >> 8) & 0xffffff) / 16777216.0; };
        struct P { double psi, chi, x; int side; SpinHit h; };
        std::vector<P> ps;
        for (int k = 0; k < 240; k++) {
            const double chi = 2 * kPi * rnd(), pe = shadowEdge(cs.a, cam, chi);
            const int b = k % 4;
            const double x = pe * std::pow(10.0, b == 0 ? -5 + 2 * rnd() : b == 1 ? -3 + 2 * rnd() : b == 2 ? -1 + rnd() : -0.5 + 0.5 * rnd());
            for (int side = 0; side < 2; side++) {
                const double psi = side == 0 ? pe + x : pe - x;
                if (psi > 0 && psi < kPi) ps.push_back({psi, chi, x / pe, side, spinRay(cs.a, cam, psi, chi, 1e-12)});
            }
        }
        const double rin = discInner(cs.a);
        const auto measure = [&](const SpinTable& T, double out[8]) {
            for (int i = 0; i < 8; i++) out[i] = 0;
            for (const P& q : ps) {
                const SpinHit tb = T.lookup(q.psi, q.chi);
                const int b = q.x < 1e-3 ? 0 : q.x < 1e-1 ? 1 : 2;
                if (q.side == 0) out[b] = std::max(out[b], angleBetween(q.h.dir, tb.dir));
                out[3] = std::max(out[3], std::fabs(q.h.b - tb.b) / (1 + std::fabs(q.h.b)));
                const SpinDisc dr = spinDisc(q.h, cs.a, rin, 100), dt = spinDisc(tb, cs.a, rin, 100);
                bool fold = false;
                for (int k = 0; k + 1 < kSpinCross; k++) if (q.h.u[k] > 0 && q.h.u[k + 1] > 0 && std::fabs(q.h.u[k] - q.h.u[k + 1]) < 0.3 * q.h.u[k]) fold = true;
                if (dr.order >= 0 && dr.order == dt.order && !fold && std::fabs(dr.r - rin) > 0.05 && std::fabs(dr.r - 100) > 1) {
                    const int bd = dr.order <= 1 && q.x >= 1e-2 ? 0 : 1;
                    out[4 + bd] = std::max(out[4 + bd], std::fabs(dt.r / dr.r - 1)); out[6 + bd] = std::max(out[6 + bd], std::fabs(dt.g / dr.g - 1));
                }
            }
        };
        // (0.55.7's coarse table at its own size, 32 x 64 x 256; 0.55.9's moving table has more rows, spin_sky.h)
        const SpinTable O(cs.a, cs.rc, cs.th, 32, 64, 256, nullptr, 0, SpinFill{1e-8, false}), M(cs.a, cs.rc, cs.th, kSpinChiCoarse, kSpinSCoarse,
                          kSpinEdgeCoarse, nullptr, 0, kSpinMoving);
        double o[8], m[8];
        measure(O, o); measure(M, m);
        bool ok = M.ok() && M.mismatches() == 0;
        std::string txt;
        const char* name[8] = {"dir x/psi_e 1e-5..1e-3", "dir 1e-3..0.1", "dir 0.1..1", "b", "disc r near", "disc r far/further", "disc g near", "disc g far/further"};
        for (int i = 0; i < 8; i++) { ok = ok && m[i] <= 1.02 * o[i] + 1e-9; txt += std::string(" ") + name[i] + " " + f(m[i]) + " (0.55.7 " + f(o[i]) + ");"; }
        check(ok, "the moving table, a " + f(cs.a) + " r " + f(cs.rc) + " theta " + f(cs.th) + ": " + f(M.fillMs()) + " ms (0.55.7's coarse " + f(O.fillMs()) + " ms), mismatches " +
                  f(M.mismatches()) + ", against KerrBL at 1e-12:" + txt);
        std::printf("info: moving table a %g r %g: %.1f ms (0.55.7 %.1f ms)%s\n", cs.a, cs.rc, M.fillMs(), O.fillMs(), txt.c_str());
    }
}

// the lab's spin setting and readout (lab.h): all from the module
static void testSpinLab() {
    Settings s = settingsDefaults();
    check(s.spin == 0, "the lab opens at spin 0 (the real holes' spin is not measured)");
    setSpin(s, 2); check(s.spin == kSpinTop, "the spin is clamped to the tested top " + f(kSpinTop));
    setSpin(s, -1); check(s.spin == 0, "and to 0 below");
    setSpin(s, 0.7);
    std::map<std::string, std::string> kv;
    saveSettings(s, kv);
    check(loadSettings(kv) == s && loadSettings(kv).spin == 0.7, "the spin survives a restart (settings key blackhole.spin)");
    kv["spin"] = "1.5"; check(loadSettings(kv).spin == 0, "a spin above the top in the file keeps the default");
    for (const char* key : {"sgra", "m87", kFeedingKey}) { Settings p = s; applyPreset(p, key); check(p.spin == 0, std::string("the preset ") + key + " starts at spin 0"); }
    setSpin(s, 0.9);
    s.disc = true;
    setDistance(s, 20);
    const Readout r = readout(s);
    check(r.horizonM == horizon(0.9) && r.rIn == discInner(0.9) && r.efficiency == discEfficiency(0.9), "the readout's horizon, inner edge and efficiency at spin 0.9 are BPT (2.6), (2.21) and 1 - E(r_ms)");
    const KerrMetric m = kerrMetric(0.9, 20, s.pitch);
    check(std::fabs(r.omega - 2 * 0.9 * 20 / m.Sigma2) < 1e-15 && std::fabs(r.turnSeconds - 2 * kPi / r.omega * r.rgM / 299792458.0) < 1e-9 * r.turnSeconds,
          "the camera is carried round at omega = 2ar / Sigma^2 (J15 (A.2)): one turn in " + f(r.turnSeconds) + " s for Sagittarius A*'s mass");
    const Camera cam{20, s.pitch, 0};
    check(std::fabs(r.shadowWideDeg - (shadowEdge(0.9, cam, kPi / 2) + shadowEdge(0.9, cam, -kPi / 2)) * 180 / kPi) < 1e-9 &&
          std::fabs(r.shadowHighDeg - (shadowEdge(0.9, cam, 0) + shadowEdge(0.9, cam, kPi)) * 180 / kPi) < 1e-9, "the shadow's width and height from the closed test: " + f(r.shadowWideDeg) + " x " + f(r.shadowHighDeg));
    const DiscPhys dp = discPhys(s);
    check(dp.aSigned == 0.9 && discGeom(s).rIn == discInner(0.9), "the disc's physics and edges follow the spin (the gas orbits with it)");
    {   // Sagittarius A* seen from Earth at spin 0.99 with the spin axis across the line of sight: 6 sqrt(3) GM/c^2 up-down (PeT Table III), narrower across
        Settings e = settingsDefaults();
        setSpin(e, 0.99);
        e.pitch = kPi / 2;
        const Readout q = readout(e);
        const double perM = q.earthShadowMuas / (2 * std::sqrt(27.0));
        check(std::fabs(q.earthHighMuas / (6 * std::sqrt(3.0) * perM) - 1) < 1e-6 && q.earthWideMuas < q.earthHighMuas, "Sagittarius A* at spin 0.99 seen side-on from Earth: " + f(q.earthWideMuas) + " x " + f(q.earthHighMuas) + " microarcsec (6 sqrt(3) M up-down, PeT Table III)");
    }
    Settings z = s; setSpin(z, 0);
    const Readout r0 = readout(z);
    check(std::fabs(r0.shadowDeg - 2 * shadowAngle(20) * 180 / kPi) < 1e-12 && std::fabs(r0.rIn - 6) < 1e-12, "spin 0 keeps the closed forms of 0.55.1 (Synge's shadow, r_ms = 6)");
}


// ---------------------------------------------------------------------------------------------------------------------------------------------
// 0.55.3: the looks. The Film look's numbers against James et al. 2015 (J15): from the extract next to the module's data
// (data/blackhole/arxiv/1502.03808_james2015.extract.txt), or, with PL_SOURCES_DIR set, from the saved original in the sources repo (and every block of the
// extract must be found in it, line ends as LF and trailing blanks dropped, as tools/check_sources.py does).
static std::string gunzip(const std::string& gz) {
    if (gz.size() < 18 || static_cast<unsigned char>(gz[0]) != 0x1f || static_cast<unsigned char>(gz[1]) != 0x8b || gz[2] != 8) return "";
    const int flg = static_cast<unsigned char>(gz[3]);
    std::size_t at = 10;
    if (flg & 4) at += 2 + (static_cast<unsigned char>(gz[at]) | (static_cast<unsigned char>(gz[at + 1]) << 8));
    if (flg & 8) { while (at < gz.size() && gz[at]) at++; at++; }
    if (flg & 16) { while (at < gz.size() && gz[at]) at++; at++; }
    if (flg & 2) at += 2;
    if (at >= gz.size()) return "";
    int n = 0;
    char* out = stbi_zlib_decode_noheader_malloc(gz.data() + at, static_cast<int>(gz.size() - at), &n);
    if (!out) return "";
    std::string s(out, static_cast<std::size_t>(n));
    STBI_FREE(out);
    return s;
}
static std::string stripTrailing(const std::string& t) {
    std::string o;
    std::size_t i = 0;
    while (i <= t.size()) {
        std::size_t e = t.find('\n', i);
        if (e == std::string::npos) e = t.size();
        std::string ln = t.substr(i, e - i);
        while (!ln.empty() && (ln.back() == ' ' || ln.back() == '\t' || ln.back() == '\r')) ln.pop_back();
        o += ln;
        if (e < t.size()) o += '\n';
        i = e + 1;
    }
    return o;
}
static void testLooks() {
    const std::string ex = readExtract("blackhole/arxiv/1502.03808_james2015.extract.txt");
    std::string text = ex;
    if (const char* src = std::getenv("PL_SOURCES_DIR")) {
        const std::string orig = stripTrailing(gunzip(readBytes(std::string(src) + "/data/blackhole/arxiv/1502.03808_james2015.txt.gz")));
        check(orig.size() > 100000, "J15: the saved original read from the sources repo (" + std::to_string(orig.size()) + " bytes)");
        int blocks = 0, found = 0;
        for (std::size_t b = ex.find("\n#--- begin"); b != std::string::npos; b = ex.find("\n#--- begin", b + 1)) {
            const std::size_t s0 = ex.find('\n', b + 1) + 1, e0 = ex.find("#--- end", s0);
            if (e0 == std::string::npos) break;
            blocks++;
            if (orig.find(ex.substr(s0, e0 - s0)) != std::string::npos) found++;
        }
        check(blocks >= 10 && found == blocks, "J15: every block of the extract is in the original (" + std::to_string(found) + " of " + std::to_string(blocks) + ")");
        text = orig;
    }
    const FilmChoices& c = filmChoices();
    const auto one = [&](const char* key, int n, int k) { const std::vector<double> v = numbersAfter(text, key, n); return static_cast<int>(v.size()) > k ? v[static_cast<std::size_t>(k)] : -1.0; };
    check(one("slowed the spin to a/M = ", 1, 0) == c.spin, "Film spin = J15 section 4.1.1 p. 26 (" + f(c.spin) + ")");
    check(one("time losses in Interstellar, to a/M = ", 1, 0) == c.spin && one("has a moderate spin a/M = ", 1, 0) == c.spin, "Film spin = J15 section 1.2 and App. A.3.1");
    check(one("slowed from a/M = ", 1, 0) == c.paperSpin, "the paper's own disc figures' spin = Figure 15's caption (0.999)");
    check(text.find("with a/M = 0.999, as viewed") != std::string::npos, "Figure 13 drawn at a/M = 0.999 (its caption)");
    check(one("position-independent temperature T = ", 1, 0) == c.discT, "Film disc temperature = J15 section 4.1.1 p. 26 (" + f(c.discT) + " K)");
    check(text.find("at which it emits a black-body spectrum") != std::string::npos && text.find("It is physically thin and marginally") != std::string::npos, "J15: black body, thin (section 4.1.1)");
    check(one("outer radii r = ", 1, 0) == c.rIn && one("\nand r = ", 1, 0) == c.rOut, "Film disc edges = Figure 13's caption (" + f(c.rIn) + ", " + f(c.rOut) + " M)");
    check(one("a camera at rc = ", 1, 0) == c.camR && one("M and θc = ", 2, 0) == c.camThetaPrinted && one("M and θc = ", 2, 1) == c.camThetaDeg, "the paper's view = Figure 13's caption (74.1 M, 1.511 rad, 86.56 deg)");
    // the caption's two angles agree only one way: 86.56 deg = 1.51076 rad rounds to the printed 1.511, while 1.511 rad = 86.574 deg does not round to 86.56;
    // the lab uses the degrees (the more digits), REPORT 0.55.3
    check(std::round(c.camThetaDeg * kPi / 180 * 1000) / 1000 == c.camThetaPrinted && std::fabs(c.camTheta - c.camThetaDeg * kPi / 180) < 1e-15, "Figure 13's 86.56 deg is its 1.511 rad to the printed digits; the camera at 86.56 deg");
    check(one("to render a ", 1, 0) == c.whiteT && text.find("spectrum with equal red, green and blue pixel values") != std::string::npos, "Film white balance = App. A.6 p. 43 (" + f(c.whiteT) + " K)");
    check(one("multiplicative factors of order ", 2, 0) == c.approachShift && one("multiplicative factors of order ", 2, 1) == c.recedeShift && one("speeds of roughly ", 1, 0) == c.gasSpeed,
          "the shifts the film left out = J15 section 4.1.2 (1.5, 0.4 at 0.55 c)");
    check(text.find("Figure 15: (a) The moderately realistic accretion disk of Figure 14") != std::string::npos && text.find("(no colour or brightness shifts) with lens") != std::string::npos,
          "no shifts in the film's disc (Figures 15a, 16)");
    check(text.find("lens flare to an image by convolving it with this point spread function") != std::string::npos, "the flare: a measured point-spread function (section 4.3.1)");
    // the rule
    const LookRule F = lookRule(Look::Film), R = lookRule(Look::Real);
    check(!F.shifts && F.oneTemperature && F.discT == c.discT && F.whiteBalance && F.whiteT == c.whiteT && F.glow && F.fixedSpin && F.spin == c.spin && F.rIn == c.rIn && F.rOut == c.rOut,
          "lookRule(Film) = the paper's choices");
    check(R.shifts && !R.oneTemperature && !R.whiteBalance && !R.glow && !R.fixedSpin, "lookRule(Real): the model, unchanged (shifts on, no white balance, no glow)");
    Look l;
    check(lookFromKey("film", l) && l == Look::Film && lookFromKey("real", l) && l == Look::Real && !lookFromKey("Film", l) && std::string(lookKey(Look::Film)) == "film", "look keys");
    // the colour: the CIE black body at 4500 K (disc.h blackBodyLinear) through one gain per channel that makes 6500 K grey with its own luminance
    const std::array<double, 3> g = whiteBalanceGains(c.whiteT), w = blackBodyLinear(c.whiteT), b = blackBodyLinear(c.discT), d = filmDiscLinear();
    const double Yw = visibleVsSun(c.whiteT);
    check(std::fabs(g[0] * w[0] / Yw - 1) < 1e-12 && std::fabs(g[1] * w[1] / Yw - 1) < 1e-12 && std::fabs(g[2] * w[2] / Yw - 1) < 1e-12, "white balance: a 6500 K black body has equal channels (its Y)");
    check(std::fabs((0.2126 * w[0] + 0.7152 * w[1] + 0.0722 * w[2]) / Yw - 1) < 2e-4, "the sRGB luminance row gives the CIE Y of the table (" + f((0.2126 * w[0] + 0.7152 * w[1] + 0.0722 * w[2]) / Yw - 1) + ")");
    check(std::fabs(d[0] / (g[0] * b[0]) - 1) < 1e-14 && std::fabs(d[1] / (g[1] * b[1]) - 1) < 1e-14 && std::fabs(d[2] / (g[2] * b[2]) - 1) < 1e-14, "the Film disc = the CIE black body at 4500 K, white-balanced");
    {   const std::array<double, 3> X = blackBodyXYZ(c.discT);           // its chromaticity from the CIE sums directly (IEC 61966-2-1's matrix), the same gains
        const double lr = 3.2406 * X[0] - 1.5372 * X[1] - 0.4986 * X[2], lg = -0.9689 * X[0] + 1.8758 * X[1] + 0.0415 * X[2], lb = 0.0557 * X[0] - 0.2040 * X[1] + 1.0570 * X[2];
        check(std::fabs((b[0] / b[1]) / (lr / lg) - 1) < 2e-3 && std::fabs((b[2] / b[1]) / (lb / lg) - 1) < 2e-3, "4500 K: blackBodyLinear's colour = the CIE 1931 sums through the sRGB matrix");
        check(d[0] > d[1] && d[1] > d[2] && d[2] > 0, "4500 K through a 6500 K white balance is warm (red > green > blue)"); }
    // no shift: the same light whatever the ray's g and wherever on the disc; Real: brighter and bluer on the approaching side
    Settings s = settingsDefaults();
    applyPreset(s, kFeedingKey);
    setSpin(s, 0.3);
    DiscPixel a1, a2, a3, r1, r2;
    Settings sf = s; setLook(sf, Look::Film);
    const Settings v = shown(sf);
    discLight(v, 10, c.approachShift, a1); discLight(v, 10, c.recedeShift, a2); discLight(v, 17, 1.0, a3);
    check(a1.rgb == a2.rgb && a1.Y == a2.Y && a1.rgb == a3.rgb && a1.T == c.discT && a1.Tobs == c.discT && a1.rgb == d && a1.Y == filmDiscY(), "Film: the same light on the approaching and the receding side and at every radius");
    discLight(s, 10, c.approachShift, r1); discLight(s, 10, c.recedeShift, r2);
    check(r1.Y > 2 * r2.Y && r1.rgb[2] / r1.rgb[0] > r2.rgb[2] / r2.rgb[0], "Real: the approaching side brighter and bluer (" + f(r1.Y / r2.Y) + " x)");
    {   const DiscPhys P = discPhys(s);                                    // Real's rule is 0.55.2's, to the bit
        const double T = P.temperature(10);
        check(r1.T == T && r1.Tobs == c.approachShift * T && r1.rgb == blackBodyLinearFast(c.approachShift * T) && r1.Y == visibleVsSunFast(c.approachShift * T), "Real: the light of 0.55.2 (T(r), g T, the fast table)"); }
    // the settings: shown(), the locked spin and disc, the paper's view, the file
    check(v.spin == c.spin && v.disc && v.look == Look::Film && sf.spin == 0.3 && shown(s) == s, "Film shows the film's spin and disc; the user's spin is kept; Real shows itself");
    sf.disc = false;
    check(shown(sf).disc && discGeom(shown(sf)).rIn == c.rIn && discGeom(shown(sf)).rOut == c.rOut && discGeom(s).rIn == discInner(0.3), "Film's disc edges; Real's at r_ms");
    Settings pv = s;
    paperView(pv);
    check(pv.dist == c.camR && pv.pitch == c.camTheta && pv.yaw == s.yaw && pv.fovDeg == s.fovDeg, "the paper's view: distance and angle from the spin axis, the turn and field of view kept");
    std::map<std::string, std::string> m;
    saveSettings(sf, m);
    check(m["look"] == "film" && loadSettings(m).look == Look::Film && loadSettings(m).spin == 0.3, "the look is saved and read back (with the user's own spin)");
    m.erase("look");
    check(loadSettings(m).look == Look::Real, "a settings file without the look opens as Real");
    m["look"] = "cinema";
    check(loadSettings(m).look == Look::Real, "an unknown look opens as Real");
    const Readout rf = readout(shown(sf));
    check(rf.look == Look::Film && rf.filmT == c.discT && rf.discEdgeIn == c.rIn && rf.discEdgeOut == c.rOut && rf.spin == c.spin, "the readout in the Film look");
    // the glow's reference (the app's own rule, looks.h): adds light only; spreads the light above the threshold without loss away from the picture's edges
    {   const int W = 320, H = 200;
        std::vector<double> img(static_cast<std::size_t>(W) * H * 3, 0.1);
        double above = 0;
        for (int y = 90; y < 110; y++)
            for (int x = 150; x < 170; x++)
                for (int k = 0; k < 3; k++) { const double q = k == 0 ? 1.0 : k == 1 ? 0.8 : 0.45; img[(static_cast<std::size_t>(y) * W + x) * 3 + k] = q; above += std::max(q - kGlowThreshold, 0.0); }
        const std::vector<double> gl = glowReference(img, W, H);
        double sum = 0, mn = 1e9;
        for (double q : gl) { sum += q; mn = std::min(mn, q); }
        check(mn >= 0, "glow: adds light only (smallest " + f(mn) + ")");
        check(std::fabs(sum / above - 1) < 2e-3, "glow: the light above the threshold spread without loss away from the edges (" + f(sum / above) + ")");
        check(glowRadiusBlocks(800) == 8 && std::fabs(glowSigmaBlocks(800) - 2.5) < 1e-15, "glow: sigma H/80 = 10 px at 800 (2.5 blocks), reach 3 sigma");
        std::vector<double> dim(img.size(), kGlowThreshold);
        const std::vector<double> g0 = glowReference(dim, W, H);
        check(*std::max_element(g0.begin(), g0.end()) == 0, "glow: nothing above the threshold, no glow");
        double peak = 0, far = 0;
        for (int k = 0; k < 3; k++) { peak = std::max(peak, gl[(static_cast<std::size_t>(100) * W + 160) * 3 + k]); far = std::max(far, gl[(static_cast<std::size_t>(100) * W + 10) * 3 + k]); }
        check(peak > 0.1 && far == 0, "glow: round the bright part, none beyond 3 sigma + a block (" + f(peak) + ", " + f(far) + ")");
    }
}

// ---------------------------------------------------------------------------------------------------------------------------------------------
// 0.55.4, one true exposure (light.h): the Sun's V magnitude from Willmer 2018 (the extract; the saved original with PL_SOURCES_DIR), the catalogue's mag is V
// (the HYG README), the bridge's identity, Pogson's rule, the screen's faintest step, the readout's numbers
static std::string checkedExtract(const std::string& rel, const std::string& orig, const std::string& who) {
    const std::string ex = readExtract(rel);
    std::string text = ex;
    if (const char* src = std::getenv("PL_SOURCES_DIR")) {
        const std::string raw = readBytes(std::string(src) + "/data/" + orig);
        const std::string o = stripTrailing(orig.size() > 3 && orig.compare(orig.size() - 3, 3, ".gz") == 0 ? gunzip(raw) : raw);
        int blocks = 0, found = 0;
        for (std::size_t b = ex.find("\n#--- begin"); b != std::string::npos; b = ex.find("\n#--- begin", b + 1)) {
            const std::size_t s0 = ex.find('\n', b + 1) + 1, e0 = ex.find("#--- end", s0);
            if (e0 == std::string::npos) break;
            blocks++;
            if (o.find(ex.substr(s0, e0 - s0)) != std::string::npos) found++;
        }
        check(o.size() > 1000 && blocks >= 2 && found == blocks, who + ": every block of the extract is in the saved original (" + std::to_string(found) + " of " + std::to_string(blocks) + ")");
        text = o;
    }
    return text;
}

static void testTrueLight() {
    const std::string w = checkedExtract("photometry/willmer2018_arxiv1804.07788.extract.txt", "photometry/willmer2018_arxiv1804.07788.txt.gz", "Willmer 2018");
    const std::vector<double> row = numbersAfter(w, "Johnson V  ", 10);
    check(row.size() == 10 && row[0] == kSunAbsV && row[3] == kSunV, "the Sun's V magnitudes = Willmer 2018 Table 3, Johnson V: absolute (2) " + f(row.size() > 0 ? row[0] : 0) +
          ", apparent (5) " + f(row.size() > 3 ? row[3] : 0));
    check(w.find("App         App       App") != std::string::npos && w.find("(Vega)     (AB)     (ST)     (Vega)") != std::string::npos && w.find("the ap-") != std::string::npos,
          "Table 3: columns (2) and (5) are the absolute and the apparent magnitude in the Vega system");
    check(numbersAfter(w, "the Sun are MB = 5.44, MV = ", 1) == std::vector<double>{kSunAbsV}, "the abstract's M_V = 4.81");
    // Willmer's two columns differ by the distance modulus of 1 AU: 5 log10(10 pc / 1 AU), the parsec 648000/pi au (presets.h parsecM): both printed to 0.01
    const double dm = 5 * std::log10(10 * parsecM() / units::kAU);
    check(std::fabs((kSunAbsV - kSunV) - dm) <= 0.01, "absolute - apparent = the distance modulus of 1 AU (" + f(kSunAbsV - kSunV) + " against " + f(dm) + ")");
    const std::string hyg = checkedExtract("stars/hyg_readme.extract.txt", "stars/hyg_readme.md", "HYG README");
    check(hyg.find("`mag`: The star's apparent visual magnitude.") != std::string::npos && hyg.find("adjusted to the V magnitude scale to match the \"mag\" field") != std::string::npos,
          "the catalogue's mag is the V magnitude (HYG README fields 12 and 25)");
    // the bridge
    const double om = sunSolidAngle(), a = units::kSunRadius / units::kAU;
    check(std::fabs(om / (kPi * a * a) - 1) < 1e-15 && std::fabs(om - 6.8e-5) < 0.1e-5, "the Sun's disc seen from 1 AU: pi (R_Sun / au)^2 = " + f(om) + " sr");
    check(std::fabs(starSurface(kSunV, om) - 1) < 1e-14, "the identity: a star of the Sun's magnitude spread over the Sun's disc is the Sun's surface (" + f(starSurface(kSunV, om)) + ")");
    check(std::fabs(starLight(1.0) / starLight(6.0) / 100 - 1) < 1e-13 && std::fabs(starLight(-1.46) / starLight(0.0) / std::pow(10.0, 0.4 * 1.46) - 1) < 1e-13,
          "the stars' relative light is still Pogson's rule (5 magnitudes = 100 times)");
    {   // the night sky's rule (orbit/sky.h) is one function, the tone map is the true rule's input
        bool same = true;
        for (double m : {-1.5, 0.0, 3.3, 6.6}) { const orbit::StarDraw a1 = orbit::starDraw(m), a2 = orbit::starDrawLight(orbit::kStarFaintLight * std::pow(10.0, -0.4 * orbit::kStarGamma * (m - orbit::kStarFaintMag))); same = same && a1.radius == a2.radius && a1.alpha == a2.alpha; }
        check(same, "starDraw = starDrawLight of the tone-mapped light (one rule)"); }
    // the screen's faintest step: 0.5 of 255 after the sRGB linear segment
    check(std::fabs(kScreenStep * 12.92 * 255 - 0.5) < 1e-15, "the faintest step: 12.92 x 255 x L = 0.5 (rounds to code 1)");
    // a star at the middle of a 1280 x 800 picture, field of view 90 degrees (K = 400 / (pi / 4) points per radian)
    const double K = 400 / (kPi / 4), pt = 1 / (K * K), sirius = -1.46;
    check(std::fabs(pointSolidAngle(K, 640, 400, 640, 400) - pt) < 1e-20 && pointSolidAngle(K, 640, 400, 1280, 800) < pt, "a square point's solid angle: 1 / K^2 at the middle, less towards the edge (sin th / th)");
    const TrueSkyNumbers t = trueSkyNumbers(sirius, 100, 100, pt);
    check(std::fabs(t.starY - starLight(sirius) / (starDotArea() * pt)) < 1e-30 && std::fabs(t.fainter * t.starY / 100 - 1) < 1e-12, "the readout: the star's dot's luminance and the disc's over it (" + f(t.fainter) + " times)");
    check(std::fabs(t.belowStep - kScreenStep * 100 / t.starY) < 1e-9 * t.belowStep && t.belowStep > 1e6, "at a white of 100 Suns' surfaces Sirius is far below the screen's step (" + f(t.belowStep) + " times)");
    check(!starVisible(sirius, t.showsAtWhite * 1.001, pt) && starVisible(sirius, t.showsAtWhite * 0.999, pt), "the star shows exactly from the exposure the readout names (" + f(t.showsAtWhite) + ")");
    check(std::log2(t.belowStep) < kEvMax, "that exposure is inside the slider's range (0.70.0: " + f(std::log2(t.belowStep)) + " stops brighter)");
    check(starTrueLight(sirius, 1, pt) / starTrueLight(sirius, 10, pt) == 10, "ten times the white, a tenth of the light on the screen");
    // the settings
    Settings s = settingsDefaults();
    applyPreset(s, kFeedingKey);
    check(!s.trueSky && s.glare && !trueSkyOn(s) && glareOn(s), "defaults: the sky Shown, the eye's glare on (with the disc, Real)");
    s.trueSky = true;
    check(trueSkyOn(s), "True brightness on with the disc and Real");
    Settings f2 = s; setLook(f2, Look::Film);
    check(!trueSkyOn(f2) && !glareOn(f2), "Film: neither (the film's own look)");
    Settings d2 = s; d2.disc = false;
    check(!trueSkyOn(d2) && !glareOn(d2), "no disc: neither");
    std::map<std::string, std::string> m;
    s.glare = false;
    saveSettings(s, m);
    check(m["skybeside"] == "true" && m["glare"] == "off" && loadSettings(m).trueSky && !loadSettings(m).glare, "saved and read back");
    m.erase("skybeside"); m.erase("glare");
    check(!loadSettings(m).trueSky && loadSettings(m).glare, "a 0.55.3 file: Sky Shown, the eye's glare (the default)");
}

// 0.55.4, the eye's glare (light.h): the CIE general disability glare equation as both saved papers print it, its range and the app's eye; conservation by
// quadrature; the veil on a picture against the direct sum, the function itself from a point, the light conserved (light leaving the picture counted)
static void testGlare() {
    const std::string b = checkedExtract("blackhole/glare/bara2023_arxiv2212.09103.extract.txt", "blackhole/glare/bara2023_arxiv2212.09103.txt.gz", "Bara & Bao-Varela 2023");
    const std::string v = checkedExtract("blackhole/glare/vandenberg2013_straylight_review.extract.txt", "blackhole/glare/vandenberg2013_straylight_review.txt.gz", "van den Berg et al. 2013");
    check(b.find("10     5 0.1 · 𝑝          𝐴 4") != std::string::npos && b.find("+ (   +       ) [1 + (     ) ] + 2.5 · 10−3 · 𝑝 ,") != std::string::npos &&
          b.find("𝜃3    𝜃2   𝜃             62.5") != std::string::npos, "Bara Eq. (2): 10/theta^3 + (5/theta^2 + 0.1 p/theta)[1 + (A/62.5)^4] + 2.5e-3 p");
    check(v.find("= 10/θ 3 + [5/θ 2 + 0.1 · p/θ] · 1 + [A/62.5]4") != std::string::npos && v.find("+ 2.5 · 10−3 · p,") != std::string::npos,
          "van den Berg et al. 2013 p. 15 prints the same equation (CIE report equation 11)");
    check(b.find("valid for 𝜃 ranging from 0.1°") != std::string::npos && b.find("to the limit of the visual field (~100°)") != std::string::npos && b.find("units of inverse steradians") != std::string::npos,
          "its range, 0.1 to 100 degrees, and its unit, sr^-1 (Bara section 2)");
    check(b.find("observer of age 35 yr and pigmentation factor p=0.5") != std::string::npos && kEyeAge == 35 && kEyePigment == 0.5 &&
          b.find("equal to 0 for dark eyes, 0.5 for brown, and 1.0 for blue-green ones.") != std::string::npos, "the app's eye: 35 years, p = 0.5 (brown; Figure 2's average observer)");
    check(b.find("normalized such that its integral across the visual field is unity") != std::string::npos && v.find("At 90 degrees the integrated value is 1 by") != std::string::npos,
          "the whole PSF is normalised to 1 (both papers)");
    // the formula, written here, at angles across its range, three eyes
    double worst = 0;
    for (double A : {20.0, 35.0, 80.0})
        for (double p : {0.0, 0.5, 1.0})
            for (double th : {0.1, 0.25, 1.0, 3.0, 10.0, 30.0, 60.0, 100.0}) {
                const double want = 10 / std::pow(th, 3) + (5 / std::pow(th, 2) + 0.1 * p / th) * (1 + std::pow(A / 62.5, 4)) + 2.5e-3 * p;
                worst = std::max(worst, std::fabs(glarePsf(th, A, p) / want - 1));
            }
    check(worst < 1e-14, "Psi = the printed formula at 8 angles from 0.1 to 100 degrees, ages 20, 35, 80, p 0, 0.5, 1 (" + f(worst) + ")");
    check(glarePsf(0.0999) == 0 && glarePsf(100.01) == 0 && glarePsf(0.1) > 0 && glarePsf(100) > 0, "outside 0.1 to 100 degrees: nothing (the model's range)");
    // conservation: the share between 0.1 and 100 degrees by two quadratures; the rest stays within 0.1 degrees, so the whole integrates to one
    const double s = glareShare(), sg = glareShareGauss(kEyeAge, kEyePigment);
    check(std::fabs(s - sg) < 1e-9 && s > 0 && s < 1, "the share spread from 0.1 to 100 degrees, s = " + f(s) + " (Simpson in ln theta) = " + f(sg) + " (Gauss-Legendre); 1 - s = " + f(1 - s) +
          " stays within 0.1 degrees: the whole is 1");
    check(glareShare(80, 1) > glareShare(20, 0), "an older, lighter eye spreads more (" + f(glareShare(20, 0)) + " at 20 and p 0, " + f(glareShare(80, 1)) + " at 80 and p 1)");
    // a single bright point at the middle of a fisheye picture that holds the whole 100 degrees: its light conserved, the function itself given back
    {   const int W = 600, H = 600;
        const double K = 300 / (100 * kPi / 180) * 1.02;   // 100 degrees at 294 pixels from the middle
        const GlareGrid g = glareGrid(W, H, 300, 300, K);
        std::vector<float> src(static_cast<std::size_t>(g.nx) * g.ny * 3, 0.0f), veil;
        const int ci = static_cast<int>(300 / g.b), cj = ci;
        for (int k = 0; k < 3; k++) src[3 * (static_cast<std::size_t>(cj) * g.nx + ci) + k] = 1000.0f;
        GlareSums su;
        glareVeil(g, src, veil, &su);
        const double E = 1000 * glareBlockSolidAngle(g, ci, cj);
        check(std::fabs(su.spread / (3 * s * E) - 1) < 1e-12, "a point: the light spread is s x its light (three channels)");
        check(std::fabs(su.laid / su.spread - 1) < 0.01, "a point: the veil lays the light it spreads, the whole 100 degrees inside the picture (" + f(su.laid / su.spread) + ", limit 1 %); " +
              std::to_string(g.nx) + " x " + std::to_string(g.ny) + " blocks of " + std::to_string(g.b) + " px, " + f(su.ms) + " ms");
    }
    {   // the function itself: a point at the middle of a finer picture (0.1 degrees a pixel, 60 degrees to the edge), the veil along a row against E Psi
        const int W = 1201, H = 1201;
        const double K = 600 / (60 * kPi / 180);
        const GlareGrid g = glareGrid(W, H, 600.5, 600.5, K);
        std::vector<float> src(static_cast<std::size_t>(g.nx) * g.ny * 3, 0.0f), veil;
        const int ci = static_cast<int>(600.5 / g.b), cj = ci;
        for (int k = 0; k < 3; k++) src[3 * (static_cast<std::size_t>(cj) * g.nx + ci) + k] = 1000.0f;
        glareVeil(g, src, veil);
        const double E = 1000 * glareBlockSolidAngle(g, ci, cj), X0 = (ci + 0.5) * g.b;
        double wf = 0;
        std::string at;
        for (double th : {1.0, 2.0, 3.0, 5.0, 8.0, 10.0, 12.0, 16.0, 20.0, 25.0, 30.0, 35.0, 40.0, 55.0}) {
            const int i = static_cast<int>(std::lround((X0 + th * kPi / 180 * K) / g.b - 0.5));
            const double thAt = ((i + 0.5) * g.b - X0) / K * 180 / kPi;
            const double r = veil[3 * (static_cast<std::size_t>(cj) * g.nx + i)] / (E * glarePsf(thAt));
            wf = std::max(wf, std::fabs(r - 1));
            at += " " + f(thAt) + ":" + f(r);
        }
        check(wf < 0.03, "a point gives the function itself back from 1 to 55 degrees (blocks of " + std::to_string(g.b) + " px = " + f(g.b / K * 180 / kPi) +
              " deg; veil / (E Psi):" + at + "; limit 3 %)");
    }
    // the coarse levels against the direct sum of every pair (a small picture, a bright ring like a disc's image)
    {   const int W = 128, H = 80;
        const double K = 40 / (kPi / 4);
        const GlareGrid g = glareGrid(W, H, 64, 40, K);
        std::vector<float> src(static_cast<std::size_t>(g.nx) * g.ny * 3, 0.0f), veil, ref;
        for (int j = 0; j < g.ny; j++)
            for (int i = 0; i < g.nx; i++) {
                const double r = std::hypot((i + 0.5) * g.b - 64, ((j + 0.5) * g.b - 40) * 3);
                if (r > 20 && r < 30) for (int k = 0; k < 3; k++) src[3 * (static_cast<std::size_t>(j) * g.nx + i) + k] = static_cast<float>(1 + 0.5 * k);
            }
        GlareSums su;
        glareVeil(g, src, veil, &su);
        glareVeilDirect(g, src, ref);
        double mx = 0, worst = 0, tv = 0, tr = 0;
        std::size_t wq = 0;
        for (float q : ref) mx = std::max(mx, static_cast<double>(q));
        for (std::size_t q = 0; q < ref.size(); q++) {
            if (ref[q] > 1e-2 * mx && std::fabs(static_cast<double>(veil[q]) / ref[q] - 1) > worst) { worst = std::fabs(static_cast<double>(veil[q]) / ref[q] - 1); wq = q; }
            tv += static_cast<double>(veil[q]); tr += static_cast<double>(ref[q]);
        }
        check(worst < 0.03 && std::fabs(tv / tr - 1) < 0.005, "the levels against every pair directly: largest difference " + f(worst) + " at block " + std::to_string(wq / 3 % g.nx) + ", " + std::to_string(wq / 3 / g.nx) +
              " of " + std::to_string(g.nx) + " x " + std::to_string(g.ny) + " (veil " + f(ref[wq] / mx) + " of the largest; limit 3 % where the veil is above 1 % of its largest: farther out, near the model's 100 degrees, coarse blocks straddle its edge), total " + f(tv / tr - 1) + " (limit 0.5 %)");
    }
    // light leaving the picture counted: a point near the edge of a 90 degree picture; the part of Psi outside the picture by quadrature over the sphere
    {   const int W = 400, H = 250;
        const double K = 125 / (kPi / 4), cx = 200, cy = 125;
        const GlareGrid g = glareGrid(W, H, cx, cy, K);
        const int ci = g.nx - 3, cj = g.ny / 2;
        std::vector<float> src(static_cast<std::size_t>(g.nx) * g.ny * 3, 0.0f), veil;
        for (int k = 0; k < 3; k++) src[3 * (static_cast<std::size_t>(cj) * g.nx + ci) + k] = 1.0f;
        GlareSums su;
        glareVeil(g, src, veil, &su);
        const double E = glareBlockSolidAngle(g, ci, cj);
        int x0 = ci * g.b, x1 = std::min(W, x0 + g.b), y0 = cj * g.b, y1 = std::min(H, y0 + g.b);
        const double X = (x0 + x1) / 2.0, Y = (y0 + y1) / 2.0, dx = (X - cx) / K, dy = (Y - cy) / K, th0 = std::hypot(dx, dy);
        const Vec3 S{std::sin(th0) / th0 * dx, std::sin(th0) / th0 * dy, -std::cos(th0)};
        const Vec3 c1 = cross(S, Vec3{0, 0, 1}), e1 = c1 * (1 / length(c1));
        const Vec3 e2 = cross(S, e1);
        double out = 0;
        const int nt = 4000, np = 720;
        const double l0 = std::log(kGlareMinDeg), l1 = std::log(kGlareMaxDeg), du = (l1 - l0) / nt;
        for (int a = 0; a < nt; a++) {
            const double th = std::exp(l0 + (a + 0.5) * du), tr = th * kPi / 180;
            for (int c = 0; c < np; c++) {
                const double ph = (c + 0.5) * 2 * kPi / np;
                const Vec3 d = S * std::cos(tr) + (e1 * std::cos(ph) + e2 * std::sin(ph)) * std::sin(tr);
                const double rho = std::hypot(d.x, d.y), t2 = std::atan2(rho, -d.z), Xd = cx + d.x / rho * t2 * K, Yd = cy + d.y / rho * t2 * K;
                if (Xd < 0 || Xd >= W || Yd < 0 || Yd >= H) out += glarePsf(th) * std::sin(tr) * tr * du * (2 * kPi / np);
            }
        }
        check(out > 0.05 * s && std::fabs((su.laid / (3 * E) + out) / s - 1) < 0.01, "a point at the edge: laid " + f(su.laid / (3 * E)) + " + outside the picture " + f(out) + " = s " + f(s) +
              " (limit 1 %)");
    }
}

// ---------------------------------------------------------------------------------------------------------------------------------------------
// 0.56.4: moving gas, step 1 (PLAN_MOVINGGAS section 7; gas.h): the clocks and periods (moved from tools/movinggas --check, the same limits), the clump's light
// curve, the pattern
static void testGas() {
    // 1. Kepler far out: the period from BPT (2.16) in seconds against 2 pi (r^3 / GM)^(1/2) in SI (r in metres), 10 Suns, r = 1e6 M
    {
        const double m = 10, r = 1e6, rm = r * gravRadiusM(m);
        const double tBPT = gasPeriod(r, 0.9) * massTimeSeconds(m), tKep = 2 * kPi * std::sqrt(rm * rm * rm / (m * units::kGMSun));
        check(std::fabs(tBPT / tKep - 1) < 1e-8, "gas: Kepler's law far out (r = 1e6 M, spin 0.9): " + f(tBPT) + " s against " + f(tKep) + " s");
    }
    // 2. the gas's clock against the normalisation u.u = -1 with J15 (A.1)'s metric, its energy -u_t against BPT (2.12); the camera's clock against 1 / hoverShift
    {
        double w1 = 0, w2 = 0, w3 = 0;
        for (double a : {0.0, 0.6, 0.9, 0.998})
            for (double r : {innermostStable(a, true), 6.0, 10.0, 30.0}) {
                const KerrMetric mt = kerrMetric(a, r, kPi / 2);
                const double W = gasOmega(r, a), gtt = -mt.alpha * mt.alpha + mt.omega * mt.omega * mt.varpi * mt.varpi, gtp = -mt.omega * mt.varpi * mt.varpi,
                             gpp = mt.varpi * mt.varpi;
                const double utNorm = 1 / std::sqrt(-(gtt + 2 * gtp * W + gpp * W * W)), ut = 1 / gasClockRate(r, a);
                w1 = std::max(w1, std::fabs(ut / utNorm - 1));
                w2 = std::max(w2, std::fabs(-(gtt * ut + gtp * W * ut) / orbitEnergy(r, a) - 1));
                for (double th : {0.3, 1.2, kPi / 2}) w3 = std::max(w3, std::fabs(cameraClockRate(a, 3 * r, th) * hoverShift(a, 3 * r, th, 0) - 1));
            }
        check(w1 < 1e-12, "gas clock: dtau/dt = alpha (1 - V^2)^(1/2) (C75 (A7a)) = 1 / u^t of the normalisation with J15 (A.1): " + f(w1));
        check(w2 < 1e-12, "gas clock: -u_t = E / mu of BPT (2.12): " + f(w2));
        check(w3 < 1e-14, "camera clock: alpha = 1 / hoverShift(b = 0) (J15 (A.1)-(A.3)): " + f(w3));
        check(std::fabs(1 / gasClockRate(6, 0) - std::sqrt(2.0)) < 1e-12, "gas clock: a = 0, r = 6 M: u^t = (1 - 3M/r)^(-1/2) = 2^(1/2)");
        check(std::fabs(gasSpeed(6, 0) - 0.5) < 1e-12, "gas: a = 0, r = 6 M: V = 1/2 (the innermost stable orbit at half the speed of light)");
    }
    // 3. published periods (their numbers read from the extracts of the saved papers). GRAVITY 2018: "4 million solar masses near the ISCO ... an orbital period of
    //    31 minutes" (p. 2; their M 4.14e6, section 1); "R ~ 7 +- 0.5 Rg ... P = 40+-8" (p. 3). Wielgus 2022 section 3: M = 4.155e6, t_g = 20.46 s, 74 +- 6 min
    //    = 10.0 - 11.2 r_g (a = 0), 9.8 - 11.0 r_g (a = 1).
    {
        const std::string gr = readExtract("blackhole/arxiv/1810.12641_gravity2018.extract.txt"), wi = readExtract("blackhole/arxiv/2209.09926_wielgus2022.extract.txt");
        const std::vector<double> m414 = numbersAfter(gr, "concentration of ", 1), m4 = numbersAfter(gr, "black hole of ", 1), p31 = numbersAfter(gr, "orbital period of ", 1);
        const std::vector<double> r7 = numbersAfter(gr, "yield R ", 2), p40 = numbersAfter(gr, "period of P = ", 2);
        const std::vector<double> mw = numbersAfter(wi, "Assuming M", 1), tgw = numbersAfter(wi, "we find tg = ", 1), pw = numbersAfter(wi, "measured LP is a co-", 2);
        const std::vector<double> r0 = numbersAfter(wi, "two components, we can", 2), r1 = numbersAfter(wi, "in the loopy period", 2);
        const bool read = m414.size() == 1 && m4.size() == 1 && p31.size() == 1 && r7.size() == 2 && p40.size() == 2 && mw.size() == 1 && tgw.size() == 1 && pw.size() == 2 &&
                          r0.size() == 2 && r1.size() == 2;
        check(read && m414[0] == 4.14 && m4[0] == 4 && p31[0] == 31 && r7[0] == 7 && p40[0] == 40 && p40[1] == 8 && mw[0] == 4.155 && tgw[0] == 20.46 && pw[0] == 74 &&
                  pw[1] == 6 && r0[0] == 10.0 && r0[1] == 11.2 && r1[0] == 9.8 && r1[1] == 11.0,
              "gas: the numbers of GRAVITY 2018 and Wielgus 2022 read from their extracts");
        if (read) {
            const double p = gasPeriod(6, 0) * massTimeSeconds(m4[0] * 1e6) / 60;
            check(std::fabs(p - p31[0]) < 1, "GRAVITY 2018: a = 0, r_ms, 4 million Suns: " + f(p) + " min against \"31 minutes\" (their rounding: 4.14e6 gives " +
                                                 f(gasPeriod(6, 0) * massTimeSeconds(m414[0] * 1e6) / 60) + ")");
            const double p7 = gasPeriod(r7[0], 0) * massTimeSeconds(m414[0] * 1e6) / 60;
            check(std::fabs(p7 - p40[0]) < p40[1], "GRAVITY 2018: a = 0, r = 7 M, 4.14e6 Suns: " + f(p7) + " min, inside 40 +- 8");
            const double tg = massTimeSeconds(mw[0] * 1e6);
            check(std::fabs(tg - tgw[0]) < 0.01, "Wielgus 2022: t_g for 4.155e6 Suns = " + f(tg) + " s against their printed 20.46 s (to its last digit)");
            const double lo0 = gasPeriod(r0[0], 0) * tg / 60, hi0 = gasPeriod(r0[1], 0) * tg / 60, lo1 = gasPeriod(r1[0], 1) * tg / 60, hi1 = gasPeriod(r1[1], 1) * tg / 60;
            const double lo = pw[0] - pw[1], hi = pw[0] + pw[1];
            check(std::fabs(lo0 - lo) < 0.5 && std::fabs(hi0 - hi) < 0.5 && std::fabs(lo1 - lo) < 0.5 && std::fabs(hi1 - hi) < 0.5,
                  "Wielgus 2022: 74 +- 6 min at 10.0 / 11.2 r_g (a = 0): " + f(lo0) + " / " + f(hi0) + "; at 9.8 / 11.0 r_g (a = 1): " + f(lo1) + " / " + f(hi1));
        }
    }
    // 4. the clump's light curve (BL06 (5) on the disc's face, each pixel at its own emission time T - t from the closed forms): spin 0.6, the camera hovering at
    //    60 M 10 degrees above the disc (the plan's view), 120 x 72 pixels, the clump at 6 M. It repeats after one orbit 2 pi / Omega(r_c) (to rounding) and
    //    not after half (by more than a tenth of its peak)
    {
        const double a = 0.6, rIn = innermostStable(a, true), rOut = 12;
        const Camera cam{60, 80 * kPi / 180, 0};
        const int W = 120, H = 72;
        const double fpx = (W / 2.0) / std::tan(12.6 * kPi / 180);
        struct Px { double r, phi, t, w; };
        std::vector<Px> px;
        int orders[3] = {0, 0, 0};
        for (int y = 0; y < H; y++)
            for (int x = 0; x < W; x++) {
                const double U = (x + 0.5 - W / 2.0) / fpx, V = -(y + 0.5 - H / 2.0) / fpx, rho = std::sqrt(U * U + V * V);
                const ClosedEnd e = traceClosed(a, cam, lookAt(std::atan(rho), std::atan2(U, -V)), 3, true);
                for (int k = 0; k < e.nCross; k++) {
                    const double r = 1 / e.u[k];
                    if (r < rIn || r > rOut) continue;
                    const double g = discShiftKerr(r, e.b, a), w = 1 / ((1 + rho * rho) * std::sqrt(1 + rho * rho) * fpx * fpx);   // the pixel's solid angle
                    px.push_back(Px{r, e.phi[k], e.t[k], w * std::pow(g, 4) * discFlux(r, a)});
                    orders[std::min(k, 2)]++;
                    break;
                }
            }
        Clump cl;
        cl.a = a; cl.rc = 6; cl.init();
        const double Tc = cl.period();
        const auto L = [&](double T) { double s = 0; for (const Px& p : px) s += p.w * cl.A * cl.density(p.r, p.phi, T - p.t); return s; };
        double peak = 0, dOrbit = 0, dHalf = 0;
        for (int i = 0; i < 60; i++) {
            const double T = 5000 + Tc * i / 60, l = L(T);
            peak = std::max(peak, l);
            dOrbit = std::max(dOrbit, std::fabs(L(T + Tc) - l));
            dHalf = std::max(dHalf, std::fabs(L(T + Tc / 2) - l));
        }
        check(peak > 0 && dOrbit < 1e-9 * peak && dHalf > 0.1 * peak, "clump light curve (spin 0.6, camera at 60 M, 10 degrees above the disc; " + f(orders[0]) +
              " pixels see the disc directly, " + f(orders[1] + orders[2]) + " through a later crossing): one orbit later it differs by " + f(dOrbit / peak) +
              " of its peak, half an orbit later by " + f(dHalf / peak));
    }
    // 5. the pattern (SKH06): (a) a mark that does not fade comes back to its phi after one orbit T = 2 pi (r^(3/2) + a) (BPT (2.16)) and is half a turn on
    //    after half; (b) the brightness factor averages 1 weighted by the disc's light (r F(r) dr dphi: the disc's total light unchanged), over points and times
    //    not used for the norm, within 3 standard errors of the sample; (c) the face-on 6-10 M ring's light varies by SKH06 Table 2's rms within the plan's 20 %
    {
        const double a = 0.9, r = 8, T = 2 * kPi * (std::pow(r, 1.5) + a);
        GasPatch p;
        p.eta = std::log(r); p.phi = 1.0; p.tBirth = 100; p.life = 1;
        const double back = GasPattern::patch(p, r, 1.0, 100 + T, a, false), half = GasPattern::patch(p, r, 1.0 + kPi, 100 + T / 2, a, false);
        const double lateFade = GasPattern::patch(p, r, 1.0, 100 + T, a, true);
        check(std::fabs(back - 1) < 1e-12 && std::fabs(half - 1) < 1e-12 && lateFade < 1e-30, "pattern: an unfading mark at 8 M (spin 0.9) is back at its phi after T_orb (" +
              f(back) + ") and half a turn on after half (" + f(half) + "); with the fading of SKH06 (36) it is gone after " + f(T) + " M (" + f(lateFade) + ")");
        GasPattern pat;
        pat.a = 0; pat.rIn = 6; pat.rOut = 25;
        pat.normalise();
        // the test's own samples: as normalise() (r stratified, weighted by r F(r), times 10 lifetimes of the outermost bins apart: independent) at other
        // places and times; the standard error of the difference is that of both means
        const double gap = 10 * GasPattern::kLife * gasPeriod(1.3 * pat.rOut * std::exp(GasPattern::kWidthEta), 0);
        double sw = 0, sx = 0, sx2 = 0, sw2 = 0;
        const int n = 20000;
        for (int i = 0; i < n; i++) {
            const std::uint32_t h = gasHash(13u, 17u, static_cast<std::uint32_t>(i));
            const double rr = pat.rIn + (pat.rOut - pat.rIn) * gasU01(h * 2891336453u + 7u), w = rr * discFlux(rr, 0);
            const double x = pat.factor(rr, 2 * kPi * gasU01(h), 2e8 + gap * (i + gasU01(h * 747796405u + 1u)));
            sw += w; sx += w * x; sx2 += w * x * x; sw2 += w * w;
        }
        const double mean = sx / sw, var = sx2 / sw - mean * mean, se = std::sqrt(var * sw2) / sw, se2 = std::sqrt(se * se + pat.normErr * pat.normErr);
        check(std::fabs(mean - 1) < 3 * se2, "pattern: the brightness factor weighted by the disc's light averages " + f(mean) + " at 20000 independent places and times (1 within 3 standard errors of it and the norm, " +
              f(3 * se2) + "; norm " + f(pat.norm) + ")");
        const std::string skh = readExtract("blackhole/arxiv/astro-ph_0606615_schnittman_krolik_hawley2006.extract.txt");
        const std::vector<double> row = numbersAfter(skh, "   6-10 ", 1);
        double rms = 0;
        {
            std::vector<double> Ls;
            for (int i = 0; i < 400; i++) {
                const double t = 2000 + i * 8.0;
                double s = 0;
                for (double rr = 6.1; rr < 10; rr += 0.2)
                    for (int j = 0; j < 90; j++) s += discFlux(rr, 0) * rr * pat.factor(rr, 2 * kPi * j / 90, t);
                Ls.push_back(s);
            }
            double m = 0, v = 0;
            for (double x : Ls) m += x;
            m /= static_cast<double>(Ls.size());
            for (double x : Ls) v += (x - m) * (x - m);
            rms = std::sqrt(v / static_cast<double>(Ls.size())) / m;
        }
        check(row.size() == 1 && row[0] == 43.8 && std::fabs(rms / (row[0] / 100) - 1) < 0.2, "pattern: the face-on 6-10 M ring (spin 0, contrast " + f(pat.contrast) +
              ") varies by " + f(100 * rms) + " % rms over 3200 M against SKH06 Table 2's " + (row.empty() ? std::string("?") : f(row[0])) + " % (thermal, > 10 Hz; the plan's 20 %)");
    }
}

// 0.72.0: the moving gas as the lab draws it (written before the card's code; limits fixed before looking).
//  1. the cells the card looks at (gas.cpp sumAt's window) hold every patch that reaches a point: the window's sum = the sum over every cell of every slot, to
//     the bit, for the full pattern and the one the lab draws, at 4000 points over 1.2 M to 2000 M and spins 0 to 0.998; the most cells per bin and slot
//  2. the norms: the full pattern's kGasNorm and the lab's kGasNormCard against normalise(40000) here (spin 0.6, 6-25 M: a disc not used for them) within
//     3 standard errors; the lab's pattern averages 1 weighted by the disc's light at 20000 independent places and times (3 standard errors); its face-on
//     6-10 M ring varies by SKH06 Table 2's rms within the plan's 20 %
//  3. the time for the card (gas.h gasCard): after 10^6 M (and 10^9 M) of running, each bin's E / T_life = K + f and the card's tau (both as the card's
//     floats) give back T / T_life, and the clump's phase at the epoch plus Omega tau = phi0 + Omega T (mod 2 pi), within 1e-5 (of a lifetime; rad): tau below
//     1024 M is rounded to 6.1e-5 M as a float, over the shortest lifetime here (8.4 M) 7e-6, times Omega (0.2 at most at 8 M) 1.2e-5 rad, and the phase at the epoch as a float (2 pi x 6e-8): 2e-5 rad; the limits does not
//     grow with the running time (the test's first limit, 1e-6, was below this float rounding: corrected with this derivation, REPORT 0.72.0)
//  4. the light's travel time at spin 0 (disc.h rayTime, DiscTable::lookupT): against the radial quadrature t = Int dr / ((1 - 2/r) (1 - b^2 (1 - 2/r) / r^2)^(1/2))
//     on the inward part of three rays (1e-8 relative); the table's lookup against rayTime at 200 points on the disc's first crossings (1e-3 relative)
static void testGasCard() {
    {   int worstCells = 0;
        double worst = 0;
        long n = 0;
        for (int v = 0; v < 2; v++)
            for (double a : {0.0, 0.5, 0.9, 0.998}) {
                GasPattern p = v == 0 ? GasPattern() : GasPattern::reduced();
                p.a = a; p.rIn = iscoPageThorne(a); p.rOut = 2000;
                for (int i = 0; i < 500; i++) {
                    const double fr = i * 0.618034 - std::floor(i * 0.618034), r = p.rIn * std::exp(std::log(p.rOut / p.rIn) * fr * fr);
                    const double phi = 2 * kPi * (i * 0.7548777 - std::floor(i * 0.7548777)), t = 1e5 * (i * 0.5698403 - std::floor(i * 0.5698403));
                    int looked = 0, all = 0;
                    const double s = p.sumAt(r, phi, t, 0, 0, &looked), b = p.sumAt(r, phi, t, 0, 0, &all, true);
                    worst = std::max(worst, std::fabs(s - b));
                    n++;
                }
                const double dp = 2 * kPi / p.cells, w = p.cutPhi * GasPattern::kWidthPhi / 2.3548;
                for (double eta = std::log(p.rIn); eta < std::log(p.rOut); eta += 0.01) {
                    const double r = std::exp(eta);
                    const int ie = static_cast<int>(std::floor(eta / 0.13));
                    for (int je = ie - 2; je <= ie + 2; je++) {
                        if (je < p.binLo() || je > p.binHi()) continue;
                        const double del = std::fabs(p.binLife(je) * (gasOmega(r, a) - p.binOmega(je)));
                        worstCells = std::max(worstCells, static_cast<int>(std::ceil((del + 2 * w) / dp)) + 1);
                    }
                }
            }
        check(worst == 0, "gas: the cells looked at hold every patch that reaches a point (" + f(static_cast<double>(n)) + " points, the window's sum - every cell's: " + f(worst) +
              "); at most " + f(worstCells) + " cells per bin and slot");
    }
    {   GasPattern full;
        full.a = 0.6; full.rIn = iscoPageThorne(0.6); full.rOut = 25;
        GasPattern lab = GasPattern::reduced();
        lab.a = full.a; lab.rIn = full.rIn; lab.rOut = full.rOut;
        GasPattern fn = full, ln = lab;
        fn.normalise(40000); ln.normalise(40000);
        check(std::fabs(fn.norm - kGasNorm) < 3 * fn.normErr * fn.norm && std::fabs(ln.norm - kGasNormCard) < 3 * ln.normErr * ln.norm,
              "gas: the norms kGasNorm " + f(kGasNorm) + " and kGasNormCard " + f(kGasNormCard) + " = normalise(40000) at spin 0.6, 6-25 M (" + f(fn.norm) + " +- " + f(fn.normErr * fn.norm) +
              ", " + f(ln.norm) + " +- " + f(ln.normErr * ln.norm) + ") within 3 standard errors");
        const double gap = 10 * GasPattern::kLife * gasPeriod(1.3 * lab.rOut * std::exp(GasPattern::kWidthEta), lab.a);
        double sw = 0, sx = 0, sx2 = 0, sw2 = 0;
        for (int i = 0; i < 20000; i++) {
            const std::uint32_t h = gasHash(19u, 23u, static_cast<std::uint32_t>(i));
            const double rr = lab.rIn + (lab.rOut - lab.rIn) * gasU01(h * 2891336453u + 7u), w = rr * discFlux(rr, lab.a);
            const double x = lab.factor(rr, 2 * kPi * gasU01(h), 3e8 + gap * (i + gasU01(h * 747796405u + 1u)));
            sw += w; sx += w * x; sx2 += w * x * x; sw2 += w * w;
        }
        const double mean = sx / sw, se = std::sqrt((sx2 / sw - mean * mean) * sw2) / sw, se2 = std::sqrt(se * se + std::pow(0.01124 / kGasNormCard, 2));
        check(std::fabs(mean - 1) < 3 * se2, "gas: the lab's pattern averages " + f(mean) + " weighted by the disc's light (1 within 3 standard errors, " + f(3 * se2) + ")");
        GasPattern ring = GasPattern::reduced();
        ring.a = 0; ring.rIn = 6; ring.rOut = 25;
        std::vector<double> Ls;
        for (int i = 0; i < 400; i++) {
            double s = 0;
            for (double rr = 6.1; rr < 10; rr += 0.2)
                for (int j = 0; j < 90; j++) s += discFlux(rr, 0) * rr * ring.factor(rr, 2 * kPi * j / 90, 2000 + i * 8.0);
            Ls.push_back(s);
        }
        double m = 0, v = 0;
        for (double x : Ls) m += x;
        m /= static_cast<double>(Ls.size());
        for (double x : Ls) v += (x - m) * (x - m);
        const double rms = std::sqrt(v / static_cast<double>(Ls.size())) / m;
        const std::vector<double> row = numbersAfter(readExtract("blackhole/arxiv/astro-ph_0606615_schnittman_krolik_hawley2006.extract.txt"), "   6-10 ", 1);
        check(row.size() == 1 && std::fabs(rms / (row[0] / 100) - 1) < 0.2, "gas: the lab's pattern: the face-on 6-10 M ring varies by " + f(100 * rms) + " % rms against SKH06 Table 2's " +
              (row.empty() ? std::string("?") : f(row[0])) + " % (the plan's 20 %)");
    }
    {   double worstT = 0, worstPhi = 0;
        for (double T : {1e6 + 345.6, 1e9 + 77.7}) {
            GasPattern p = GasPattern::reduced();
            p.a = 0.9; p.rIn = iscoPageThorne(0.9); p.rOut = 2000;
            Clump c;
            c.a = 0.9; c.rc = 8; c.phi0 = 0.3;
            c.init();
            GasCard k;
            gasCard(&p, &c, T, k);
            for (int i = 0; i < k.N[1]; i++) {
                const double life = p.binLife(k.N[0] + i);
                const std::uint32_t K = static_cast<std::uint32_t>(k.bin[4 * i + 1]) * 65536u + static_cast<std::uint32_t>(k.bin[4 * i + 2]);
                const double kd = std::floor(T / life);
                const std::uint32_t want = static_cast<std::uint32_t>(static_cast<std::int64_t>(std::floor(k.epoch / life)));
                const double back = static_cast<double>(k.bin[4 * i]) + static_cast<double>(k.tau) / life;   // T / T_life - K, as the card has it
                worstT = std::max(worstT, K == want ? std::fabs(back - (T / life - std::floor(k.epoch / life))) : 1.0);
                (void)kd;
            }
            worstPhi = std::max(worstPhi, std::fabs(std::remainder(static_cast<double>(k.C[1]) + c.omega * k.tau - (c.phi0 + c.omega * T), 2 * kPi)));
        }
        check(worstT < 1e-5 && worstPhi < 2e-5, "gas: the card's time after 10^6 and 10^9 M of running: T / T_life back to " + f(worstT) + " of a lifetime (limit 1e-5), the clump's phase to " + f(worstPhi) +
              " rad (limit 2e-5: tau's float rounding times Omega, with the epoch's phase's own)");
    }
    {   double worstQ = 0, worstL = 0;
        for (double psi : {2.0, 2.4, 2.8}) {
            const double rO = 30, b = rO * std::sin(psi) / std::sqrt(1 - 2 / rO);
            for (double ph : {0.05, 0.15}) {
                const double r = rayRadius(rO, psi, ph), t = rayTime(rO, psi, ph);
                double q = 0;                                                         // Gauss-Legendre, 64 pieces of 8 points from r to rO
                static const double x8[8] = {-0.9602898564975363, -0.7966664774136267, -0.5255324099163290, -0.1834346424956498, 0.1834346424956498, 0.5255324099163290, 0.7966664774136267, 0.9602898564975363};
                static const double w8[8] = {0.1012285362903763, 0.2223810344533745, 0.3137066458778873, 0.3626837833783620, 0.3626837833783620, 0.3137066458778873, 0.2223810344533745, 0.1012285362903763};
                for (int s = 0; s < 64; s++) {
                    const double lo = r + (rO - r) * s / 64, hi = r + (rO - r) * (s + 1) / 64;
                    for (int j = 0; j < 8; j++) {
                        const double rr = 0.5 * (lo + hi) + 0.5 * (hi - lo) * x8[j], fct = 1 - 2 / rr;
                        q += 0.5 * (hi - lo) * w8[j] / (fct * std::sqrt(1 - b * b * fct / (rr * rr)));
                    }
                }
                worstQ = std::max(worstQ, std::fabs(t - q) / q);
            }
        }
        const DiscTable T(30, 256, 128);
        for (int i = 0; i < 200; i++) {
            const double x = (T.alphaSh() - 0.02) * (i + 0.5) / 200 - 0.01, psi = (kPi - T.alphaSh()) - x;
            double tl = 0;
            const double ph = 0.2 + 2.5 * (i * 0.618034 - std::floor(i * 0.618034));
            if (!T.lookupT(x, ph, tl)) continue;
            double u = 0;
            T.lookup(x, ph, u);
            if (!(u > 1.0 / 30 && u < 0.5)) continue;
            worstL = std::max(worstL, std::fabs(tl - rayTime(30, psi, ph)) / rayTime(30, psi, ph));
        }
        check(worstQ < 1e-8 && worstL < 1e-3, "gas: the light's travel time at spin 0 against the radial quadrature (" + f(worstQ) + ", limit 1e-8) and the table's lookup against it (" + f(worstL) + ", limit 1e-3)");
    }
}

// 0.56.4: the azimuth and the time the light took at each disc crossing, in closed form (kerr_closed.h "TIME AND AZIMUTH"), against GLb (72), radial light, and
// the integrator with t added (gas.h KerrBLt)
// the integrator's state over the axis: KerrBL lets theta run past 0 or pi; a point at -theta is the one at theta, phi + pi (the closed forms count the passage)
static void foldAxis(double& th, double& ph) {
    th = std::remainder(th, 2 * kPi);
    if (th < 0) { th = -th; ph += kPi; }
}
class KerrBLtz : public KerrBLt {   // KerrBLt with the affine parameter itself as y[7] (to stop a looser run at the same zeta)
public:
    explicit KerrBLtz(double a) : KerrBLt(a) {}
    bool launch(const Camera& cam, const Vec3& look, Ray& ray) const override {
        if (!KerrBLt::launch(cam, look, ray)) return false;
        ray.n = 8; ray.y[7] = 0;
        return true;
    }
    void rhs(const Ray& ray, const double* y, double* dy) const override { KerrBLt::rhs(ray, y, dy); dy[7] = 1; }
};
static void testClosedTime() {
    // 1. GLb (72), a = 0: successive crossings of the plane by a ray just outside the shadow's edge are delayed by tau_0 = 3 sqrt(3) pi M, held to the limit of
    //    tools/movinggas's integrator check (1e-4: the delay tends to tau_0 as the ray nears the edge)
    {
        const Camera cam{1e4, 1.0, 0};
        const double edge = std::asin(3 * std::sqrt(3.0) * std::sqrt(1 - 2 / cam.r) / cam.r);   // Synge's edge (b = 3 sqrt(3) M; schwarzschild.h cameraImpact)
        const ClosedEnd e = traceClosed(0.0, cam, lookAt(edge * (1 + 4e-9), 0.3), kClosedCross, true);
        int kb = 0;
        for (int k = 0; k + 1 < e.nCross; k++)
            if (std::fabs(1 / e.u[k] - 3) + std::fabs(1 / e.u[k + 1] - 3) < std::fabs(1 / e.u[kb] - 3) + std::fabs(1 / e.u[kb + 1] - 3)) kb = k;
        const double tau0 = 3 * std::sqrt(3.0) * kPi, d = e.nCross >= 5 ? e.t[kb + 1] - e.t[kb] : 0;
        check(e.kind == RayEnd::Sky && e.nCross >= 5 && std::fabs(d / tau0 - 1) < 1e-4, "closed t: GLb (72), a = 0: the delay between successive crossings near the edge " + f(d) +
              " M against 3 sqrt(3) pi = " + f(tau0) + " (" + f(e.nCross) + " crossings, at r = " + f(1 / e.u[kb]) + ", " + f(1 / e.u[kb + 1]) + ")");
    }
    // 2. nearly radial light at a = 0 (psi = 1e-5 from the hole's direction: b = 1e-3 M; the radial ray itself is GLa's excluded principal ray (77)): from 100 M
    //    to r2 against t = r1 - r2 + 2 ln((r1 - 2)/(r2 - 2)) (dt/dr = 1 / (1 - 2/r)); the ray's angular momentum adds at most b^2 / (2 r2) (dt/dr =
    //    1 / ((1 - 2/r)(1 - b^2 (1 - 2/r) / r^2)^(1/2))): the limit b^2 / r2
    {
        const Camera cam{100, 1.2, 0};
        double worst = 0, lim = 0;
        for (double tau : {0.02, 0.09, 0.2}) {
            ClosedState st;
            if (!closedStateAt(0.0, cam, lookAt(1e-5, 0.4), tau, st)) { worst = 1e9; continue; }
            const double ref = cam.r - st.r + 2 * std::log((cam.r - 2) / (st.r - 2)), b = cam.r * 1e-5 / std::sqrt(1 - 2 / cam.r);
            worst = std::max(worst, std::fabs(st.t - ref) / (b * b / st.r));
            lim = b * b / st.r;
        }
        check(worst < 1, "closed t: nearly radial light at a = 0 against its closed form: the worst difference " + f(worst) + " of b^2 / r2 (" + f(lim) + " M at the last)");
    }
    // 3. against the integrator (KerrBLt at rtol 1e-12) for every radial substitution and both polar types, spins 0.01, 0.6, 0.9, 0.99 (testClosed's cameras):
    //    at the crossings (r, phi, t) and at three points along each ray (r, cos theta, phi, t at the integrator's own Mino time; vortical rays never cross).
    //    THE LIMIT is what the integrator reaches: per camera and per number, the largest difference between the integrator at 1e-10 and at 1e-12 (the same
    //    crossings; along the ray, at the same Mino time). The closed forms are held to it against the 1e-12 run. Plus, at spins 0.6, 0.9, 0.99 and 0.01 (camera at
    //    1000 M there), rays with lambda = 2 r+ / a exactly (r3 = r+, the cancellation 0.56.2 found) and at 1 +- 1e-6 of it.
    {
        unsigned seed = 5;
        const auto rnd = [&] { seed = seed * 1103515245u + 12345u; return ((seed >> 8) & 0xffffff) / 16777216.0; };
        struct Case { double a, rc, th; int nray; };
        int subs[6] = {}, vort = 0, crossAll = 0;
        for (const Case cs : {Case{0.01, 20, 1.3, 14}, Case{0.6, 74.1, 1.5107, 14}, Case{0.9, 20, 1.396, 14}, Case{0.99, 6, 1.4, 14}, Case{0.9, 8, 0.4, 14},
                              Case{0.9, 2.5, 1.2, 14}, Case{0.6, 3, 0.3, 14}, Case{0.6, 30, 1.3, -1}, Case{0.9, 30, 1.3, -1}, Case{0.99, 30, 1.3, -1}, Case{0.01, 1000, 1.3, -1}}) {
            const Camera cam{cs.rc, cs.th, 0};
            const KerrBLtz ch(cs.a);
            std::vector<Vec3> looks;
            if (cs.nray > 0) {
                for (int k = 0; k < cs.nray; k++) {
                    const double chi = k < 4 ? kPi / 2 * k : 2 * kPi * rnd(), pe = shadowEdge(cs.a, cam, chi);
                    for (int band = 0; band < 2; band++) for (int side = 0; side < 2; side++) {
                        const double x = pe * std::pow(10.0, band == 0 ? -5 + 2 * rnd() : -2 + 2 * rnd());
                        const double psi = side == 0 && band == 1 ? pe + (kPi - pe) * rnd() : side ? pe - x : pe + x;
                        if (psi > 0 && psi < kPi) looks.push_back(lookAt(psi, chi));
                    }
                }
                // and up to 3 rays of each substitution and of the vortical type that a 60 x 60 grid over the camera's sky meets (case II and (4) are rare)
                int found[7] = {};
                for (int i = 0; i < 60; i++)
                    for (int j = 0; j < 60; j++) {
                        const double psi = kPi * (j + 0.5) / 60, chi = 2 * kPi * (i + 0.5) / 60;
                        const ClosedEnd e = traceClosed(cs.a, cam, lookAt(psi, chi), 0);
                        const int kind = e.kind == RayEnd::Lost ? 0 : e.vortical ? 6 : e.radial;
                        if (kind > 0 && found[kind] < 3) { found[kind]++; looks.push_back(lookAt(psi, chi)); }
                    }
            } else {
                // lambda = 2 r+ / a: on the rows chi = +-pi/2, the first psi from the edge outward where lambda passes the target (a scan of 2000, then bisection),
                // times 1 and 1 +- 1e-6
                const double target = 2 * horizon(cs.a) / cs.a;
                for (double fac : {1.0, 1 - 1e-6, 1 + 1e-6}) {
                    bool got = false;
                    for (double chi : {kPi / 2, -kPi / 2}) {
                        const double pe = shadowEdge(cs.a, cam, chi);
                        const auto lamAt = [&](double psi) { Ray rr; ch.launch(cam, lookAt(psi, chi), rr); return rr.c[0] - fac * target; };
                        double lo = pe * (1 + 1e-9), slo = lamAt(lo), hi = lo;
                        for (int k = 1; k <= 2000 && !got; k++) {
                            hi = pe + (kPi - pe) * k / 2000.0;
                            if (lamAt(hi) * slo <= 0) got = true; else lo = hi;
                        }
                        if (!got) continue;
                        for (int it = 0; it < 200; it++) { const double mid = 0.5 * (lo + hi); if (lamAt(mid) * slo > 0) lo = mid; else hi = mid; }
                        const double psi = 0.5 * (lo + hi);
                        if (fac == 1.0) check(std::fabs(lamAt(psi) / target) < 1e-13, "closed t: the ray with lambda = 2 r+ / a found at spin " + f(cs.a) + ": " + f(lamAt(psi) / target));
                        looks.push_back(lookAt(psi, chi));
                        break;
                    }
                    if (!got) check(false, "closed t: no ray with lambda = 2 r+ / a seen from r " + f(cs.rc) + " at spin " + f(cs.a));
                }
            }
            // per number: [0] r (relative), [1] phi (rad), [2] t (M), [3] cos theta: the integrator's spread and the closed forms' difference
            double spread[4] = {0, 0, 0, 0}, diff[4] = {0, 0, 0, 0};
            int nC = 0, nP = 0, bad = 0;
            for (const Vec3& look : looks) {
                const ClosedEnd e = traceClosed(cs.a, cam, look, kClosedCross, true);
                if (e.kind == RayEnd::Lost) { bad++; continue; }
                subs[e.radial]++;
                vort += e.vortical;
                std::vector<double> cr12, cr10, path;
                TraceOptions o12;
                o12.rtol = 1e-12; o12.atol = 1e-12; o12.maxCross = kClosedCross; o12.cross = &cr12; o12.maxSteps = 400000;
                TraceOptions o10 = o12;
                o10.rtol = 1e-10; o10.atol = 1e-10; o10.cross = &cr10;
                const RayEnd end12 = trace(ch, cam, look, o12, &path);
                trace(ch, cam, look, o10);
                const int n12 = static_cast<int>(cr12.size() / 8), n10 = static_cast<int>(cr10.size() / 8);
                const double rEnd = horizon(cs.a) + 1e-3;
                for (int k = 0; k < n12 && k < n10; k++) {
                    double A[8], B[8];
                    std::copy(&cr12[8 * static_cast<std::size_t>(k)], &cr12[8 * static_cast<std::size_t>(k)] + 8, A);
                    std::copy(&cr10[8 * static_cast<std::size_t>(k)], &cr10[8 * static_cast<std::size_t>(k)] + 8, B);
                    foldAxis(A[1], A[2]);
                    foldAxis(B[1], B[2]);
                    if (!(A[0] > rEnd)) continue;
                    spread[0] = std::max(spread[0], std::fabs(B[0] / A[0] - 1));
                    spread[1] = std::max(spread[1], std::fabs(std::remainder(B[2] - A[2], 2 * kPi)));
                    spread[2] = std::max(spread[2], std::fabs(B[5] - A[5]));
                    if (k >= e.nCross) { bad++; continue; }
                    nC++;
                    diff[0] = std::max(diff[0], std::fabs(1 / e.u[k] / A[0] - 1));
                    diff[1] = std::max(diff[1], std::fabs(std::remainder(e.phi[k] - A[2], 2 * kPi)));
                    diff[2] = std::max(diff[2], std::fabs(e.t[k] - (-A[5])));
                }
                // three points along the ray (a quarter, half, three quarters of its accepted steps; outside 1.05 r+ and inside 200 M)
                const std::size_t ns = path.size() / 8;
                for (std::size_t q = 1; q <= 3 && ns > 8; q++) {
                    double A[8];
                    std::copy(&path[8 * (ns * q / 4)], &path[8 * (ns * q / 4)] + 8, A);
                    foldAxis(A[1], A[2]);
                    if (A[0] < 1.05 * horizon(cs.a) || A[0] > 200 || end12.kind == RayEnd::Lost) continue;
                    Ray rb;
                    ch.launch(cam, look, rb);
                    TraceOptions oS;
                    oS.rtol = 1e-10; oS.atol = 1e-10;
                    integrateSpan(ch, rb, A[7], oS);
                    // the looser run moved along its ray to the 1e-12 run's Mino time (the closed forms are compared at a Mino time, whose own integration error
                    // the comparison at one zeta would leave out): q(tau) = q + (dq/dzeta) / (dtau/dzeta) (tau_12 - tau_10)
                    double dy[8];
                    ch.rhs(rb, rb.y, dy);
                    const double sh = (A[6] - rb.y[6]) / dy[6];
                    double y10[8];
                    for (int q = 0; q < 8; q++) y10[q] = rb.y[q] + dy[q] * sh;
                    foldAxis(y10[1], y10[2]);
                    spread[0] = std::max(spread[0], std::fabs(y10[0] / A[0] - 1));
                    spread[1] = std::max(spread[1], std::fabs(std::remainder(y10[2] - A[2], 2 * kPi)));
                    spread[2] = std::max(spread[2], std::fabs(y10[5] - A[5]));
                    spread[3] = std::max(spread[3], std::fabs(std::cos(y10[1]) - std::cos(A[1])));
                    ClosedState st;
                    if (!closedStateAt(cs.a, cam, look, -A[6], st)) { bad++; continue; }
                    nP++;
                    diff[0] = std::max(diff[0], std::fabs(st.r / A[0] - 1));
                    diff[1] = std::max(diff[1], std::fabs(std::remainder(st.phi - A[2], 2 * kPi)));
                    diff[2] = std::max(diff[2], std::fabs(st.t - (-A[5])));
                    diff[3] = std::max(diff[3], std::fabs(st.mu - std::cos(A[1])));
                }
            }
            crossAll += nC;
            const std::string at = "closed t and phi, a " + f(cs.a) + " r " + f(cs.rc) + " theta " + f(cs.th) + (cs.nray < 0 ? " (lambda = 2 r+ / a and 1 +- 1e-6)" : "") + ", " +
                                   f(static_cast<double>(looks.size())) + " rays, " + f(nC) + " crossings, " + f(nP) + " points: ";
            check(bad == 0 && nC + nP > 0, at + "every crossing and point answered (" + f(bad) + " not)");
            check(diff[0] <= spread[0] && diff[1] <= spread[1] && diff[2] <= spread[2] && diff[3] <= spread[3],
                  at + "against the integrator at 1e-12: r " + f(diff[0]) + " (its own spread 1e-10 to 1e-12: " + f(spread[0]) + "), phi " + f(diff[1]) + " rad (" + f(spread[1]) +
                      "), t " + f(diff[2]) + " M (" + f(spread[2]) + "), cos theta " + f(diff[3]) + " (" + f(spread[3]) + ")");
        }
        std::printf("info: closed t and phi against the integrator: %d crossings; rays by substitution (1) %d, (2) %d, (3) %d, (4) %d, case II %d; vortical %d\n", crossAll,
                    subs[1], subs[2], subs[3], subs[4], subs[5], vort);
        check(subs[1] > 0 && subs[2] > 0 && subs[3] > 0 && subs[4] > 0 && subs[5] > 0 && vort > 0, "closed t and phi: every radial substitution, case II and both polar types met");
    }
    // 4. the processor's spin table (the fallback path) stores each ray's phi and t at its crossings: the same as the closed forms at its own entries
    {
        const double a = 0.9;
        const Camera cam{20, 1.396, 0};
        SpinFill timedFill = kSpinFineFill;   // 0.57.2: phi and t asked for
        timedFill.timed = true;
        const SpinTable tb(a, cam.r, cam.theta, 8, 12, 64, nullptr, 0, timedFill);
        double worst = 0;
        int n = 0, untimed = 0;
        for (int side = 0; side < 2; side++)
            for (int i = 0; i < tb.nChi(); i++)
                for (int j = 0; j < tb.nS(); j++) {
                    const SpinHit& h = tb.node(side, i, j);
                    if (!h.timed) { untimed++; continue; }
                    const ClosedEnd e = traceClosed(a, cam, lookAt(tb.nodePsi(side, i, j), tb.rowChi(i)), kSpinCross, true);
                    if (std::min(e.nCross, kSpinCross) != h.nCross) { worst = 1e9; continue; }
                    for (int k = 0; k < h.nCross; k++) { worst = std::max(worst, std::max(std::fabs(h.phi[k] - e.phi[k]), std::fabs(h.t[k] - e.t[k]))); n++; }
                }
        check(tb.ok() && n > 0 && worst == 0 && untimed == static_cast<int>(tb.fallbacks()), "spin table: phi and t at " + f(n) + " crossings of its entries = the closed forms (" +
              f(worst) + "); entries without them = the rays followed by the integrator (" + f(untimed) + ")");
    }
}

// 0.57.2: phi and t only on request (traceClosed's timed, SpinFill::timed). 0.56.4 took them at every crossing of every ray, and the spin tables took five times
// as long (REPORT 0.57.2). The reference is this build's own timed run: without the request every other number is the same bit for bit (the session that made
// 0.57.2 also compared the untimed numbers with 0.56.3's build on the same machine: equal bit for bit once 0.56.4's Psi_0 by atan2 is put back to asin, REPORT
// 0.57.2), and phi and t are not made (0, timed false); with it, the moving gas's tests above read them (testClosedTime, testDiscFlux's crossings).
static bool sameBits(double x, double y) { return std::memcmp(&x, &y, sizeof x) == 0; }
static bool sameBits(const Vec3& x, const Vec3& y) { return sameBits(x.x, y.x) && sameBits(x.y, y.y) && sameBits(x.z, y.z); }
static void testTimedOnRequest() {
    const double spins[4] = {0, 0.6, 0.9, 0.99};
    const Camera cams[4] = {{20, 80 * kPi / 180, 0}, {74.1, 86.56 * kPi / 180, 0}, {6, 1.2, 0}, {60, kPi / 2, 0}};   // the benchmark's, the Film's (paper), close, in the plane
    int rays = 0, diff = 0, made = 0, timedCross = 0;
    for (double a : spins)
        for (const Camera& c : cams)
            for (int j = 0; j < 24; j++) {
                const double chi = 2 * kPi * j / 24 + 0.01, pe = shadowEdge(a, c, chi);
                const double ps[10] = {0.3 * pe, 0.9 * pe, pe * (1 - 1e-6), pe * (1 - 1e-9), pe * (1 + 1e-9), pe * (1 + 1e-6), pe * (1 + 1e-3), 1.1 * pe, 2 * pe, 0.5 * (kPi + pe)};
                for (double psi : ps) {
                    const ClosedEnd u = traceClosed(a, c, lookAt(psi, chi), kClosedCross), w = traceClosed(a, c, lookAt(psi, chi), kClosedCross, true);
                    rays++;
                    bool same = u.kind == w.kind && sameBits(u.dir, w.dir) && sameBits(u.b, w.b) && sameBits(u.q, w.q) && u.nCross == w.nCross && sameBits(u.pz, w.pz) &&
                                sameBits(u.rEnd, w.rEnd) && sameBits(u.thEnd, w.thEnd) && sameBits(u.dthdr, w.dthdr) && u.radial == w.radial && u.vortical == w.vortical;
                    for (int k = 0; k < kClosedCross; k++) {
                        same = same && sameBits(u.u[k], w.u[k]);
                        if (u.phi[k] != 0 || u.t[k] != 0) made++;
                    }
                    if (u.timed || !w.timed) made++;
                    timedCross += w.nCross;
                    if (!same) diff++;
                }
            }
    check(diff == 0 && made == 0 && timedCross > 0, "traceClosed without phi and t: every other number the same bit for bit as with them (" + f(rays) + " rays, " + f(diff) +
                                                     " differ), phi and t not made (" + f(made) + ")");
    // the tables: the moving (coarse) one at every spin from the benchmark's and the Film's cameras, the fine one at the benchmark's spin 0.9
    for (double a : spins)
        for (int q = 0; q < 2; q++)
            for (int fine = 0; fine < 2; fine++) {
                if (fine && !(a == 0.9 && q == 0)) continue;
                const Camera& c = cams[q];
                SpinFill fu = fine ? kSpinFineFill : kSpinMoving, fw = fu;
                fw.timed = true;
                const int nChi = fine ? kSpinChiFine : kSpinChiCoarse, nS = fine ? kSpinSFine : kSpinSCoarse, nE = fine ? kSpinEdge : kSpinEdgeCoarse;
                const SpinTable U(a, c.r, c.theta, nChi, nS, nE, nullptr, 0, fu), W(a, c.r, c.theta, nChi, nS, nE, nullptr, 0, fw);
                int bad = 0, untimed = 0, timedBad = 0;
                for (int i = 0; i < nE; i++) bad += !sameBits(U.edgeSample(i), W.edgeSample(i));
                for (int i = 0; i < nChi; i++) bad += !sameBits(U.rowChi(i), W.rowChi(i)) + !sameBits(U.rowEdge(i), W.rowEdge(i));
                for (int s = 0; s < 2; s++)
                    for (int i = 0; i < nChi; i++)
                        for (int j = 0; j < nS; j++) {
                            const SpinHit &x = U.node(s, i, j), &y = W.node(s, i, j);
                            bool same = sameBits(U.nodePsi(s, i, j), W.nodePsi(s, i, j)) && x.sky == y.sky && sameBits(x.dir, y.dir) && sameBits(x.b, y.b) &&
                                        x.fallback == y.fallback && x.nCross == y.nCross;
                            for (int k = 0; k < kSpinCross; k++) same = same && sameBits(x.u[k], y.u[k]) && x.phi[k] == 0 && x.t[k] == 0;
                            bad += !same;
                            untimed += x.timed;
                            timedBad += y.timed == y.fallback;
                        }
                const std::vector<float>* tu[4] = {&U.texA(), &U.texB(), &U.texEdge(), &U.texTau()};
                const std::vector<float>* tw[4] = {&W.texA(), &W.texB(), &W.texEdge(), &W.texTau()};
                for (int k = 0; k < 4; k++) bad += *tu[k] != *tw[k];
                check(U.ok() && W.ok() && bad == 0 && untimed == 0 && timedBad == 0 && U.rays() == W.rays() && U.fallbacks() == W.fallbacks() && U.mismatches() == W.mismatches(),
                      std::string(fine ? "fine" : "moving") + " spin table at spin " + f(a) + " from r " + f(c.r) + " without phi and t: the same bit for bit as with them (" +
                          f(bad) + " numbers differ), none timed (" + f(untimed) + "); with them, every closed-form entry timed (" + f(timedBad) + " not)");
            }
}

// 0.58.0, "Light: All light" (lab.h allLight, disc.h allLightLinear). Tests written before the code's use in the picture:
// (a) the observed total intensity g^4 sigma T^4 / pi = g^4 F / pi (L79 p. 233, "the inverse 4th power of the redshift factor 1+z"; Riazuelo (53)) at chosen points,
//     spin 0 (F from L79 (15) with PT's factor sqrt(3)/2, written out here) and spin 0.9 (PT (15n)), against the Sun's sigma T_sun^4 / pi; the colour the eye's;
// (b) the whole disc's light over all directions against the efficiency: NOT tested, see below;
// (c) L79's Figs 9 and 10 for a far observer (spin 0), the direct image;
// (d) the eye's picture unchanged: What an eye sees (the default) is 0.57's rule bit for bit.
static void testAllLight() {
    const double sig = stefanBoltzmann(), Ts = sunTeff(), Ts4 = Ts * Ts * Ts * Ts;
    // (a)
    Settings s = settingsDefaults(); applyPreset(s, kFeedingKey);
    check(!s.allLight, "What an eye sees is the default");
    {   std::map<std::string, std::string> m; s.allLight = true; saveSettings(s, m);
        check(m["light"] == "all" && loadSettings(m).allLight && allLightOn(s) && !glareOn(s) && !trueSkyOn(s), "All light: saved, no eye's glare, no true sky beside it");
        m["light"] = "x"; check(!loadSettings(m).allLight, "a bad value: the eye's");
        s.allLight = false; }
    double worstY = 0, worstC = 0, worstLum = 0; int n = 0;
    for (double a : {0.0, 0.9}) {
        Settings q = s; q.spin = a;
        const DiscPhys P = discPhys(q);
        const double rIn = discInner(a);
        for (double r : {rIn * 1.05, 9.55, 30.0, 400.0, 1900.0})
            for (double g : {0.45, 0.8, 1.0, 1.3, 1.7}) {
                double Fc;                                                             // W/m^2, the closed forms
                if (a == 0) {
                    const double sr = std::sqrt(r), s3 = std::sqrt(3.0), s6 = std::sqrt(6.0);
                    Fc = 3 / (8 * kPi) / ((r - 3) * std::pow(r, 2.5)) * (sr - s6 + s3 / 2 * std::log((sr + s3) * (s6 - s3) / ((sr - s3) * (s6 + s3))));
                } else Fc = ptF(r, a) / (4 * kPi * r);
                Fc *= P.fluxScale();
                const double want = std::pow(g, 4) * Fc / kPi / (sig * Ts4 / kPi);      // g^4 sigma T^4 / pi over sigma T_sun^4 / pi
                DiscPixel e, all; Settings qa = q; qa.allLight = true;
                discLight(q, r, g, e); discLight(qa, r, g, all);
                worstY = std::max(worstY, std::fabs(all.Y / want - 1));
                const double lum = kLumR * all.rgb[0] + kLumG * all.rgb[1] + kLumB * all.rgb[2];
                worstLum = std::max(worstLum, std::fabs(lum / want - 1));
                const double se = e.rgb[0] + e.rgb[1] + e.rgb[2], sa = all.rgb[0] + all.rgb[1] + all.rgb[2];
                for (int k = 0; k < 3; k++) worstC = std::max(worstC, std::fabs(all.rgb[k] / sa - e.rgb[k] / se));
                check(all.Tobs == e.Tobs, "the same observed temperature in both modes");
                n++;
            }
    }
    check(n == 50 && worstY < 1e-9 && worstLum < 1e-9, "All light = g^4 sigma T^4 / pi against the Sun's sigma T_sun^4 / pi, spins 0 and 0.9, 50 points: " + f(worstY) + ", on the screen " + f(worstLum));
    check(worstC < 1e-12, "All light keeps the eye's colour: " + f(worstC));
    check(bolometricVsSun(Ts) == 1 && allLightLinear(100).at(1) == 0, "the Sun's surface is 1; below the colour table's range: no light");
    // (b) The light of the whole disc summed over every direction far away is NOT compared with the efficiency 1 - E(r_ms) (disc.h discLuminosity, tested in
    // testDiscFlux): part of the light falls into the hole and part falls back onto the disc, and no saved source gives those shares; far out, where neither
    // matters, the shift is too small (g - 1 ~ (M/r)^(1/2)) for the sum to tell g^4 from another power. The law itself is tested in (a) and by L79 in (c).
    // (c) L79 Figs 9, 10: the observed bolometric flux of the direct image in units of F_S max, a far observer 30 and 10 degrees above the plane
    const std::string ex = readExtract("blackhole/scans/luminet_1979_AA75_228.extract.txt");
    check(ex.find("the inverse 4th\npower of the redshift factor 1+z") != std::string::npos && ex.find("F0=FS/(1+z)4") != std::string::npos, "L79: F0 = FS / (1 + z)^4 (the extract)");
    const double Fmax = discFlux(fluxMaxRadius(0), 0);
    const auto maxF0 = [&](double aboveDeg, double& rAt, double& alphaAt) {
        const double th0 = (90 - aboveDeg) * kPi / 180, rO = 1e8;
        const auto at = [&](double b, double al, double& r) {
            const DiscHit h = discRay(rO, kPi - std::asin(b / rO), std::cos(th0), std::cos(al) * std::sin(th0), -std::sin(al) * std::sin(th0), DiscGeom{6, 1e9});
            if (h.order != 0) return -1.0;
            r = h.r;
            return std::pow(h.g, 4) * discFlux(h.r, 0) / Fmax;
        };
        double best = -1, bb = 0, ba = 0, r = 0;
        for (double b = 6; b <= 20; b += 0.5) for (int i = 0; i < 180; i++) { const double v = at(b, i * kPi / 90, r); if (v > best) { best = v; bb = b; ba = i * kPi / 90; rAt = r; } }
        double db = 0.5, da = kPi / 90;
        for (int it = 0; it < 60 && db > 1e-6; it++) {
            bool better = false;
            for (int i = -1; i <= 1; i++) for (int j = -1; j <= 1; j++) {
                const double v = at(bb + i * db, ba + j * da, r);
                if (v > best) { best = v; bb += i * db; ba += j * da; rAt = r; better = true; }
            }
            if (!better) { db /= 2; da /= 2; }
        }
        alphaAt = ba;
        return best;
    };
    double r30, a30, r10, a10;
    const double m30 = maxF0(30, r30, a30), m10 = maxF0(10, r10, a10);
    // Fig. 9 (30 degrees): its largest labelled curve is 1.80, with a small unlabelled closed curve inside it (the next level of its steps of 0.30, 2.10): the largest
    // value just above 2.10. Fig. 10 (10 degrees): "The maximum value within the area limited by F0=2.10 is 2.62".
    check(ex.find("1.80, 1.50, 1.20, 0.90, 0.60, 0.30") != std::string::npos && m30 > 2.10 && m30 < 2.40, "L79 Fig. 9, 30 degrees: the largest value just above 2.10 (no 2.40 curve): " + f(m30));
    check(ex.find("The maximum value within the area limited by F0=2.10 is 2.62") != std::string::npos && m10 > 2.10, "L79 Fig. 10, 10 degrees: a region above 2.10: largest " + f(m10));
    // L79: "the flux is of course maximum in the regions where the spectral shift is a blueshift, occurring at a mean distance corresponding roughly to maximum
    // intrinsic luminosity" (r* ~ 9.55): on the approaching side (Lz > 0, here sin alpha < 0), near 9.55
    check(std::sin(a30) < 0 && std::sin(a10) < 0 && std::fabs(r30 - 9.55) < 1 && std::fabs(r10 - 9.55) < 1, "the largest where the gas approaches, at r " + f(r30) + " and " + f(r10));
    // NOT reproduced (REPORT 0.58.0): L79's 2.62 at 10 degrees; this app gives the number printed here. Its own Fig. 10 draws no 2.40 curve inside 2.10, which a
    // largest value of 2.62 (steps of 0.30) would need too; so the paper's figure and its sentence do not agree with each other either.
    std::printf("info: L79 Fig. 10 prints 2.62 as the largest F0 / F_S max at 10 degrees; here %.4f at r = %.3f (and %.4f at 30 degrees, r = %.3f)\n", m10, r10, m30, r30);
    // (d) the eye's picture: discLight is 0.57's rule bit for bit
    for (double r : {6.5, 9.55, 30.0, 1500.0}) for (double g : {0.6, 1.0, 1.4}) {
        DiscPixel e; discLight(s, r, g, e);
        const double T = discPhys(s).temperature(r) * g;
        const auto c = blackBodyLinearFast(T);
        check(e.Tobs == T && e.rgb == c && e.Y == visibleVsSunFast(T), "What an eye sees: unchanged at r " + f(r) + ", g " + f(g));
    }
}

// ---------------------------------------------------------------------------------------------------------------------------------------------
// 0.70.0, the Real look's tone curve (tone.h): Reinhard, Stark, Shirley & Ferwerda 2002 (the extract; the saved original with PL_SOURCES_DIR): the key value
// and the default burn-out white read from the paper's lines; eqs. (1), (2), (4) written here again at hand-picked inputs; monotonic; chromaticity kept
static void testTone() {
    const std::string w = checkedExtract("blackhole/tone/reinhard2002_uucs-02-01.extract.txt", "blackhole/tone/reinhard2002_uucs-02-01.txt.gz", "Reinhard et al. 2002");
    check(numbersAfter(w, "is a scaled luminance and a = ", 1) == std::vector<double>{kToneKey}, "the key value a = 0.18 (eq. (2))");
    check(w.find("By default we set Lwhite to the") != std::string::npos && w.find("maximum luminance in the scene. If this default") != std::string::npos,
          "the burn-out white's default: the scene's largest luminance (eq. (4))");
    check(w.find("is a small value to avoid") != std::string::npos && w.find("log (\xce\xb4 + Lw (x, y))") != std::string::npos, "eq. (1): log(delta + L_w), delta small");
    // eq. (1): the log-average (delta 0: the geometric mean)
    check(std::fabs(logAverage({1, 100, 10000}, 0) / 100 - 1) < 1e-14 && std::fabs(logAverage({2, 8}, 0) / 4 - 1) < 1e-14, "eq. (1): the log-average of 1, 100, 10^4 is 100");
    check(std::fabs(logAverage({0, 3}, 1) / 2 - 1) < 1e-14, "eq. (1) with delta: exp((ln 1 + ln 4) / 2) = 2");
    // eq. (4), written here: Ld = L (1 + L / Lw^2) / (1 + L)
    bool same = true;
    for (double L : {1e-6, 0.01, 0.18, 0.5, 1.0, 3.0, 47.0, 1e4})
        for (double Lw : {0.3, 1.0, 5.0, 1e3}) {
            const double want = L * (1 + L / (Lw * Lw)) / (1 + L);
            same = same && std::fabs(toneCurve(L, Lw) / want - 1) < 1e-14;
        }
    check(same, "eq. (4) at 32 hand-picked (L, Lwhite)");
    check(std::fabs(toneCurve(5, 5) - 1) < 1e-15 && std::fabs(toneCurve(0.4, 0.4) - 1) < 1e-15 && toneCurve(4.999, 5) < 1, "eq. (4): Lwhite is the smallest luminance mapped to white");
    check(std::fabs(toneCurve(3, 0) - 0.75) < 1e-15, "eq. (3) when Lwhite is infinite: L / (1 + L)");
    // eq. (2) through the exposure: the log-average is drawn at the key (middle grey), one stop doubles it; the brightest sample is the burn-out white
    const std::vector<double> Y{0.5, 2, 30, 400, 0};
    const ToneExposure e = toneFromSamples(Y, 0), e1 = toneFromSamples(Y, 1);
    const double lb = logAverage(Y, 400 * kToneDelta);
    check(e.ok && std::fabs(e.k * lb - kToneKey) < 1e-15 && std::fabs(e1.k / e.k - 2) < 1e-14 && std::fabs(e.lw - e.k * 400) < 1e-12 && e.logAvg == lb,
          "eq. (2): k Lbar = a (" + f(e.k * lb) + "), +1 stop = twice, Lwhite = k Ymax");
    check(!toneFromSamples({0, 0}, 0).ok && !toneFromSamples({}, 0).ok, "no light: no exposure (the caller keeps the last good one)");
    // monotonic: a brighter input is never shown darker (each channel and the luminance), also where the colour is scaled down
    bool mono = true;
    for (double Lw : {0.4, 2.0, 50.0}) {
        double prevL = -1;
        std::array<double, 3> prev{-1, -1, -1};
        for (int i = 0; i <= 4000; i++) {
            const double t = std::pow(10.0, -6 + 10.0 * i / 4000);
            const double L = toneCurve(t, Lw);
            mono = mono && L >= prevL;
            prevL = L;
            const std::array<double, 3> o = toneScaled({0.6 * t, 0.9 * t, 1.3 * t}, Lw);
            for (int k = 0; k < 3; k++) mono = mono && o[static_cast<std::size_t>(k)] >= prev[static_cast<std::size_t>(k)] - 1e-15;
            prev = o;
        }
    }
    check(mono, "monotonic: the curve and every channel never fall as the light grows");
    // the approaching side stays brighter: the feeding hole's disc at r = 10 seen with shift 1.3 and 0.7, through the curve at the exposure of its brightest
    {   Settings s = settingsDefaults();
        applyPreset(s, kFeedingKey);
        DiscPixel a, b;
        discLight(s, 10, 1.3, a);
        discLight(s, 10, 0.7, b);
        const ToneExposure x = toneExposure(b.Y, a.Y * 1.2, 0);
        const auto ta = toneRGB(a.rgb, x), tb = toneRGB(b.rgb, x);
        check(lumaSrgb(ta) > lumaSrgb(tb) && a.Y > b.Y, "the approaching side (g = 1.3) is shown brighter than the receding one (g = 0.7): " + f(lumaSrgb(ta)) + " > " + f(lumaSrgb(tb)));
    }
    // chromaticity kept (before 8-bit rounding): x and y of the output = the input's within 1e-6
    double worst = 0;
    for (double T : {3000.0, 6500.0, 2e4, 1e5})
        for (double sc : {1e-3, 1.0, 1e3, 1e6}) {
            const auto c0 = blackBodyLinear(T);
            const std::array<double, 3> c{c0[0] * sc, c0[1] * sc, c0[2] * sc};
            const auto o = toneRGB(c, toneExposure(1.0, 1e3, 0));
            const double si = c[0] + c[1] + c[2], so = o[0] + o[1] + o[2];
            for (int k = 0; k < 2; k++) worst = std::max(worst, std::fabs(c[static_cast<std::size_t>(k)] / si - o[static_cast<std::size_t>(k)] / so));
        }
    check(worst < 1e-6, "chromaticity kept through the curve (" + f(worst) + ")");
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
    testDiscSettings();
    testSpinClosedForms();
    testSpinShadow();
    testSpinFarEdge();
    testSpinImages();
    testSpinShift();
    testSpinCamera();
    testSpinSymmetry();
    testSpinTable();
    testMino();
    testClosed();
    testMovingTable();
    testSpinLab();
    testLooks();
    testTrueLight();
    testGlare();
    testGas();
    testGasCard();
    testClosedTime();
    testTimedOnRequest();
    testAllLight();
    testTone();
    std::printf("%d checks, %d failed\n", g_checks, g_failed);
    return g_failed ? 1 : 0;
}
