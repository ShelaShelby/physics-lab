#include "blackhole/kerr_closed.h"

#include <algorithm>
#include <cmath>

#include "blackhole/elliptic.h"
#include "blackhole/kerr.h"

namespace blackhole {

namespace {
constexpr double kPi = 3.14159265358979323846;

// Pi(n; psi | m) for any real psi, n < 1, with 1 - n given (no rounding when n is close to 1): psi = j pi + p, |p| <= pi/2, Pi = Pi(p) + 2 j Pi(n | m) (GLa (53c))
double piAny(double n, double nc, double psi, double kc2) {
    const double j = std::floor(psi / kPi + 0.5), p = psi - j * kPi, s = std::sin(p), c = std::cos(p), as = std::fabs(s);
    const double c2 = c * c, d2 = c2 + kc2 * s * s;
    const double v = as * carlsonRF(c2, d2, 1) + n / 3 * as * as * as * carlsonRJ(c2, d2, 1, nc + n * c2);   // 1 - n s^2 = (1 - n) + n c^2
    const double full = j != 0 ? carlsonRF(0, kc2, 1) + n / 3 * carlsonRJ(0, kc2, 1, nc) : 0;
    return (s < 0 ? -v : v) + 2 * j * full;
}

// ---- the polar motion (GLa Sec. III; kerr_closed.h) ----
struct Polar {
    bool flat = false, vort = false, noG = false;   // noG: lambda = 0 (through the pole): G_phi is multiplied by lambda = 0 and Pi(1; ...) is not finite
    double nu = 1, h = 1, up = 0, um = 0, sq = 0, kc2 = 1, n = 0, nc = 1, F0 = 0, ph0 = 0, K = 0, G0 = 0, gscale = 1, cth = 0;
    bool init(double a, double lam, double eta, double th, double pth, double nuTh) {
        nu = nuTh;
        cth = std::cos(th);
        const double A = a * a - eta - lam * lam, root = std::sqrt(A * A + 4 * a * a * eta);
        if (eta > 0) {
            const double ma2 = A < 0 ? (root - A) / 2 : 2 * a * a * eta / (A + root);    // -u- a^2 (stable either sign of A)
            up = eta / ma2;
            sq = std::sqrt(ma2);
            kc2 = 1 + up * a * a / ma2;                                                   // 1 - u+/u-, u+/u- = -u+ a^2 / (-u- a^2)
            n = up;
            nc = lam * lam / (a * a + ma2);                                               // 1 - u+ = lambda^2 / (a^2 (1 - u-)) ((18) at u = 1)
            // Psi_0 = -nu arcsin(cos theta_s / u+^(1/2)); its cosine from u+ - cos^2 theta_s = sin^2 theta_s Theta_s / (a^2 cos^2 theta_s - u- a^2) ((18))
            const double s0 = std::clamp(cth / std::sqrt(up), -1.0, 1.0);
            const double c02 = std::sin(th) * std::sin(th) * pth * pth / ((a * a * cth * cth + ma2) * up);
            const double c0 = std::sqrt(std::clamp(c02, 0.0, 1.0));
            ph0 = -nu * std::asin(s0);
            F0 = ellFang(-nu * s0, c0, kc2);
            K = ellKc(kc2);
            noG = !(nc > 0);
            G0 = noG ? 0 : piAny(n, nc, ph0, kc2);
            gscale = 1 / sq;
            return std::isfinite(F0) && std::isfinite(G0);
        }
        if (eta < 0) {
            vort = true;
            h = cth >= 0 ? 1 : -1;
            const double u2 = -2 * a * a * eta / (A + root);                              // u- a^2 > 0
            um = u2 / (a * a);
            up = -eta / u2;                                                               // u+ u- = -eta / a^2
            sq = std::sqrt(u2);
            kc2 = up / um;                                                                // 1 - (1 - u+/u-)
            n = (up - um) / (1 - um);
            nc = (1 - up) / (1 - um);
            const double s2 = std::clamp((cth * cth - um) / (up - um), 0.0, 1.0);
            const double s1 = std::sqrt(s2), c1 = std::sqrt(1 - s2);
            ph0 = -h * nu * std::asin(s1);
            F0 = ellFang(-h * nu * s1, c1, kc2);
            noG = !(nc > 0);
            G0 = noG ? 0 : piAny(n, nc, ph0, kc2);
            gscale = 1 / ((1 - um) * sq);
            return std::isfinite(F0) && std::isfinite(G0);
        }
        flat = true;                                                                      // eta = 0: theta stays where it is (the plane, or a principal ray)
        return true;
    }
    // cos theta, d cos theta / ds and G_phi at Mino time tau
    void at(double tau, double sinTh0, double& mu, double& dmu, double& G) const {
        if (flat) { mu = cth; dmu = 0; G = tau / (sinTh0 * sinTh0); return; }
        const double ps = jacobiAm(sq * tau + F0, kc2), s = std::sin(ps), c = std::cos(ps), dn = std::sqrt(1 - (1 - kc2) * s * s);
        if (!vort) {
            mu = -nu * std::sqrt(up) * s;
            dmu = -nu * std::sqrt(up) * c * sq * dn;
        } else {
            const double w = std::sqrt(um + (up - um) * s * s);
            mu = h * w;
            dmu = h * (up - um) * s * c * sq * dn / w;
        }
        G = noG ? 0 : (piAny(n, nc, ps, kc2) - G0) * gscale;
    }
    // lambda = 0 (noG): the ray passes through the axis, where the turning angle is the pole (u+ = 1); each passage turns phi by pi (the limit of lambda G_phi
    // from either side: G_phi's jump over a passage is pi / |lambda| to leading order). The passages: Psi crossing pi/2 + j pi between its start and tau.
    int poles(double tau) const {
        if (!noG || flat) return 0;
        const double ps = jacobiAm(sq * tau + F0, kc2), off = vort ? 0 : kPi / 2;
        return static_cast<int>(std::floor((ps - off) / kPi) - std::floor((ph0 - off) / kPi));
    }
    // the Mino times of the first crossings of the equator before tauMax (type A only)
    int crossings(double tauMax, int maxN, double* taus) const {
        if (flat || vort) return 0;
        int found = 0;
        for (int j = F0 < 0 ? 0 : 1, it = 0; found < maxN && it < 64; j++, it++) {
            const double t = (2 * j * K - F0) / sq;
            if (t >= tauMax) break;
            if (t > 0) taus[found++] = t;
        }
        return found;
    }
};

// ---- the radial roots (GLa (78)-(95)) ----
struct Roots { double r[4] = {}; double b1 = 0, a1 = 0, b2 = 0, a2 = 0; bool real12 = false, real34 = false; };
Roots rootsOf(double a, double lam, double eta) {
    Roots o;
    const double A = a * a - eta - lam * lam, B = 2 * (eta + (lam - a) * (lam - a)), C = -a * a * eta;
    const double P = -A * A / 12 - C, Q = -A / 3 * ((A / 6) * (A / 6) - C) - B * B / 8;
    const double D = (P / 3) * (P / 3) * (P / 3) + (Q / 2) * (Q / 2);              // -(discriminant (92)) / 108
    double t;
    if (D >= 0) {
        const double sd = std::sqrt(D);
        t = std::cbrt(-Q / 2 + sd) + std::cbrt(-Q / 2 - sd);
    } else {   // three real roots: the largest, in the trigonometric form (P < 0)
        t = 2 * std::sqrt(-P / 3) * std::cos(std::acos(std::clamp(3 * Q / (2 * P) * std::sqrt(-3 / P), -1.0, 1.0)) / 3);
    }
    double xi = t - A / 3;
    for (int it = 0; it < 2; it++) {   // Newton on the resolvent cubic (83)
        const double f = ((xi + A) * xi + (A * A / 4 - C)) * xi - B * B / 8, df = (3 * xi + 2 * A) * xi + (A * A / 4 - C);
        if (df != 0) xi -= f / df;
    }
    const double z = std::sqrt(std::max(xi, 0.0) / 2);
    const double d12 = -A / 2 - z * z + B / (4 * z), d34 = -A / 2 - z * z - B / (4 * z);
    o.b1 = z; o.b2 = -z;
    o.real12 = d12 >= 0; o.real34 = d34 >= 0;
    if (o.real12) { const double s = std::sqrt(d12); o.r[0] = -z - s; o.r[1] = -z + s; } else { o.r[0] = o.r[1] = -z; o.a2 = std::sqrt(-d12); }
    if (o.real34) { const double s = std::sqrt(d34); o.r[2] = z - s; o.r[3] = z + s; } else { o.r[2] = o.r[3] = z; o.a1 = std::sqrt(-d34); }
    return o;
}

// ---- the radial motion (GLa App. B; kerr_closed.h) ----
struct Radial {
    int sub = 0;                // 1..4; 5 = substitution (2) in case II
    double a = 0, lam = 0, eta = 0;
    double r1 = 0, r2 = 0, r3 = 0, r4 = 0, rp = 0, rm = 0;
    double pref = 1, kc2 = 1, K = 0;
    double AA = 0, BB = 0;      // (3): A, B (B57)
    double b1 = 0, a2 = 0, CC = 0, DD = 0, g0 = 0;   // (4)
    bool init(double a_, double lam_, double eta_, double rc) {
        a = a_; lam = lam_; eta = eta_;
        rp = horizon(a); rm = 2 - rp;   // r- = M - (M^2 - a^2)^(1/2) = 2M - r+
        const Roots o = rootsOf(a, lam, eta);
        if (!o.real12) {
            sub = 4;
            b1 = o.b1; a2 = o.a2;
            const double a1 = o.a1, bb = o.b1 - o.b2;
            CC = std::sqrt((a1 - a2) * (a1 - a2) + bb * bb); DD = std::sqrt((a1 + a2) * (a1 + a2) + bb * bb);
            pref = 2 / (CC + DD);
            kc2 = (DD - CC) * (DD - CC) / ((CC + DD) * (CC + DD));
            g0 = std::sqrt((4 * a2 * a2 - (CC - DD) * (CC - DD)) / ((CC + DD) * (CC + DD) - 4 * a2 * a2));
        } else {
            r1 = o.r[0]; r2 = o.r[1];
            if (!o.real34) {
                sub = 3;
                const double a1 = o.a1;
                b1 = o.b1;
                AA = std::sqrt((b1 - r2) * (b1 - r2) + a1 * a1); BB = std::sqrt((b1 - r1) * (b1 - r1) + a1 * a1);
                pref = 1 / std::sqrt(AA * BB);
                const double r21 = r2 - r1, dAB = std::fabs(AA - BB);
                kc2 = (r21 - dAB) * (r21 + dAB) / (4 * AA * BB);
            } else {
                r3 = o.r[2]; r4 = o.r[3];
                const double r31 = r3 - r1, r42 = r4 - r2, r21 = r2 - r1, r43 = r4 - r3;
                pref = 2 / std::sqrt(r31 * r42);
                kc2 = r21 * r43 / (r31 * r42);
                if (r4 > rp) sub = rc <= r3 ? 1 : 2;
                else sub = 5;
                if (sub == 2 && rc < r4) return false;   // the camera between r3 and r4 (R < 0): not a light ray
            }
        }
        K = ellKc(kc2);
        return std::isfinite(pref) && std::isfinite(kc2) && kc2 > 0;
    }
    bool two() const { return sub == 2 || sub == 5; }
    // the amplitude (sin, cos) of the substitution at r (r = infinity for inf)
    void amp(double r, bool inf, double& s, double& c) const {
        if (sub == 1) {
            const double r31 = r3 - r1, r32 = r3 - r2;
            const double s2 = (r - r2) * r31 / ((r - r1) * r32), c2 = (r2 - r1) * (r3 - r) / ((r - r1) * r32);
            s = std::sqrt(std::clamp(s2, 0.0, 1.0)); c = std::sqrt(std::clamp(c2, 0.0, 1.0));
        } else if (two()) {
            const double r31 = r3 - r1, r41 = r4 - r1, r43 = r4 - r3;
            const double s2 = inf ? r31 / r41 : (r - r4) * r31 / ((r - r3) * r41), c2 = inf ? r43 / r41 : r43 * (r - r1) / ((r - r3) * r41);
            s = std::sqrt(std::max(s2, 0.0)); c = std::sqrt(std::max(c2, 0.0));
        } else if (sub == 3) {
            if (inf) { c = (AA - BB) / (AA + BB); s = 2 * std::sqrt(AA * BB) / (AA + BB); return; }
            const double p = AA * (r - r1), q = BB * (r - r2), den = p + q;
            c = (p - q) / den; s = 2 * std::sqrt(p * q) / den;
        } else {
            const double n0 = std::sqrt(1 + g0 * g0), s2 = g0 / n0, c2 = 1 / n0;
            if (inf) { s = c2; c = -s2; return; }
            const double x = (r + b1) / a2, n1 = std::sqrt(1 + x * x), s1 = x / n1, c1 = 1 / n1;
            s = s1 * c2 + c1 * s2; c = c1 * c2 - s1 * s2;
        }
    }
    double I0(double s, double c) const { return pref * ellFang(s, c, kc2); }
    // R1 (B61)-(B65) and S1 (B91)-(B95) for amplitude (s, c), s >= 0; j = 1 - kc2
    double R1(double al, double s, double c) const {
        const double j = 1 - kc2, al2 = al * al, w = j + (1 - j) * al2, p1 = std::sqrt((al2 - 1) / w), d = std::sqrt(c * c + kc2 * s * s);
        const double pl = p1 * d + s;
        const double f1 = p1 / 2 * std::log(std::fabs(pl * pl * w / (al2 * c * c - 1)));   // (p1 d + s) / (p1 d - s), p1^2 d^2 - s^2 = (al^2 c^2 - 1) / w
        return (ellPiang(al2 / (al2 - 1), s, c, kc2) - al * f1) / (1 - al2);
    }
    // r+- within a relative 1e-? of r2 (case (3) at small spin: r- and r2 are both of order a^2, so alpha_- = -1/x3(r-) is close to -1 and (B62)'s two
    // terms, each of order 1/(1 - alpha^2), cancel to the result's size). Then I_- is taken as J = Int dr / ((r - r2) sqrt R), exact in closed form (derived
    // here), and the rest, (r- - r2) Int dr / ((r - r2)(r - r-) sqrt R), of relative size (r- - r2) / (r - r2) <= (r- - r2) / (r+ - r2), is left out: with
    // kNearR2 (|alpha^2 - 1| ~ 2 (r- - r2) / (r - r1)) that is below 1e-5 of I_-, whose weight in I_phi (B2), r- - a lambda / 2, is of order a^2 there.
    //   Derivation: with c = x3(r) = cos t (B55), (r - r2) = r21 A (1 - c) / ((B - A) + (B + A) c), and dr / sqrt R = dt / (sqrt(AB) d) (B71), d = (1 - k3 sin^2 t)^(1/2):
    //   J = 1 / (sqrt(AB) r21 A) Int [(B - A) + (B + A) c] / ((1 - c) d) dt = 1 / (sqrt(AB) r21 A) [-(B + A) F + 2 B Int dt / ((1 - c) d)], and
    //   Int dt / ((1 - cos t) d) = -d cot(t/2) + F(t) - E(t) (differentiate: d/dt(-d cot(t/2)) = d / (1 - c) + j c (1 + c) / d with j = k3, and
    //   1 / ((1 - c) d) = d / (1 - c) + j (1 + c) / d, so the rest is j s^2 / d, whose integral is F - E). E (the second kind) by Carlson:
    //   E = s R_F(c^2, d^2, 1) - (j/3) s^3 R_D(c^2, d^2, 1), R_D(x, y, z) = R_J(x, y, z, z).
    static constexpr double kNearR2 = 1e-4;
    double nearR2(double s, double c, double F) const {
        const double j = 1 - kc2, c2 = c * c, d2 = c2 + kc2 * s * s, d = std::sqrt(d2), as = s;
        double E = as * carlsonRF(c2, d2, 1) - j / 3 * as * as * as * carlsonRJ(c2, d2, 1, 1);
        if (c < 0) E = 2 * (carlsonRF(0, kc2, 1) - j / 3 * carlsonRJ(0, kc2, 1, 1)) - E;
        const double Ft = F / pref, cot = (1 + c) / s;
        const double R1m = -d * cot + Ft - E;
        return pref / ((r2 - r1) * AA) * (-(BB + AA) * Ft + 2 * BB * R1m);
    }
    double S1(double al, double s, double c) const {
        const double j = 1 - kc2, al2 = al * al, p2 = std::sqrt((1 + al2) / (1 - j + al2)), d = std::sqrt(c * c + kc2 * s * s);
        const double f2 = p2 / 2 * std::log(std::fabs((1 + p2 * d) * (1 + p2 * d) / ((1 + p2) * (1 + p2) * ((1 + al2) * s * s - 1))));
        return (ellFang(s, c, kc2) + al2 * ellPiang(1 + al2, s, c, kc2) - al * f2) / (1 + al2);
    }
    // the antiderivatives of 1 / ((r - r+-) sqrt R) at the amplitude (s, c) and the matching r (needed by (4) only through x4(r+-))
    void Ipm(double s, double c, double& ip, double& im) const {
        const double F = I0(s, c);
        for (int k = 0; k < 2; k++) {
            const double rr = k == 0 ? rp : rm;
            double v = 0;
            if (two()) {
                const double r3_ = rr - r3, r4_ = rr - r4, r31 = r3 - r1, r41 = r4 - r1, r43 = r4 - r3;
                const double n = r3_ * r41 / (r4_ * r31);
                v = -pref * r43 / (r3_ * r4_) * ellPiang(n, s, c, kc2) - F / r3_;                              // (B39), (B43)
            } else if (sub == 3) {
                const double q1 = rr - r1, q2 = rr - r2, al = (BB * q2 + AA * q1) / (BB * q2 - AA * q1);
                if (std::fabs(al * al - 1) < kNearR2) { v = nearR2(s, c, F); (k == 0 ? ip : im) = v; continue; }
                v = -1 / (BB * q2 + AA * q1) * ((BB + AA) * F + 2 * (r2 - r1) * std::sqrt(AA * BB) / (BB * q2 - AA * q1) * R1(al, s, c));   // (B70)
            } else if (sub == 4) {
                const double x = (rr + b1) / a2, gp = (g0 * x - 1) / (g0 + x);
                // (B100) as printed has g0 (g0 + x4(r+-)) in the second term's denominator; derived here (kerr_closed.h): a2 (1 - g0 x4(r+-)) (g0 + x4(r+-))
                v = g0 / (a2 * (1 - g0 * x)) * F - pref * (1 + g0 * g0) / (a2 * (1 - g0 * x) * (g0 + x)) * S1(gp, s, c);
            }
            (k == 0 ? ip : im) = v;
        }
    }
    // r at Mino time tau from the camera (rc, initial sign nu): (B27), (B46), (B75), (B109)
    double rOf(double tau, double F0, double nu) const {
        if (sub == 4) {
            const double ps = jacobiAm(nu * tau / pref + F0, kc2);
            return a2 * std::tan(ps - std::atan(g0)) - b1;
        }
        const double ps = jacobiAm(tau / pref + nu * F0, kc2);
        if (sub == 3) {
            const double cn = std::cos(ps);
            return ((BB * r2 - AA * r1) + (BB * r2 + AA * r1) * cn) / ((BB - AA) + (BB + AA) * cn);
        }
        const double s2 = std::sin(ps) * std::sin(ps);
        if (sub == 1) return ((r2 * (r3 - r1)) - r1 * (r3 - r2) * s2) / ((r3 - r1) - (r3 - r2) * s2);
        return (r4 * (r3 - r1) - r3 * (r4 - r1) * s2) / ((r3 - r1) - (r4 - r1) * s2);
    }
    double R(double r) const {
        const double P = r * r + a * a - a * lam, D = r * r - 2 * r + a * a;
        return P * P - D * (eta + (lam - a) * (lam - a));
    }
};
}  // namespace

ClosedRadial closedRoots(double a, double lambda, double eta) {
    const Roots o = rootsOf(a, lambda, eta);
    ClosedRadial c{};
    c.r1 = o.r[0]; c.r2 = o.r[1]; c.r3 = o.r[2]; c.r4 = o.r[3];
    c.im1 = o.real12 ? 0 : -o.a2; c.im2 = o.real12 ? 0 : o.a2; c.im3 = o.real34 ? 0 : -o.a1; c.im4 = o.real34 ? 0 : o.a1;
    Radial rd;
    c.sub = rd.init(a, lambda, eta, 1e30) ? rd.sub : 0;
    return c;
}

bool closedRadialIntegrals(double a, double lambda, double eta, double rs, double r, double& I0, double& Ip, double& Im) {
    Radial rd;
    if (!rd.init(a, lambda, eta, rs)) return false;
    const bool inf = !std::isfinite(r);
    double s0, c0, s1, c1, p0, m0, p1, m1;
    rd.amp(rs, false, s0, c0);
    rd.amp(inf ? 0 : r, inf, s1, c1);
    I0 = rd.I0(s1, c1) - rd.I0(s0, c0);
    if (rd.sub == 1) { Ip = Im = 0; return true; }
    rd.Ipm(s0, c0, p0, m0);
    rd.Ipm(s1, c1, p1, m1);
    Ip = p1 - p0; Im = m1 - m0;
    return true;
}

ClosedEnd traceClosed(double a, const Camera& cam, const Vec3& look, int maxCross) {
    ClosedEnd out;
    const KerrBL ch(a);
    Ray ray;
    if (!ch.launch(cam, look, ray)) return out;
    const double rc = ray.y[0], th = ray.y[1], pr = ray.y[3], pth = ray.y[4], lam = ray.c[0], eta = ray.c[1];
    out.b = lam; out.q = eta;
    const double nuR = pr > 0 ? -1 : 1, nuTh = pth > 0 ? -1 : 1;
    Polar pol;
    Radial rad;
    if (!pol.init(a, lam, eta, th, pth, nuTh) || !rad.init(a, lam, eta, rc)) return out;
    out.radial = rad.sub;
    out.vortical = pol.vort;
    // where the ray goes and the Mino time it takes (kerr_closed.h)
    const double rEnd = horizon(a) + 1e-3;
    double sc, cc, se, ce;
    rad.amp(rc, false, sc, cc);
    const double Fc = rad.I0(sc, cc) / rad.pref;             // the camera's amplitude F (the inversions' start)
    bool sky;
    double tau;
    if (rad.sub == 1) {
        sky = false;
        rad.amp(rEnd, false, se, ce);
        tau = nuR < 0 ? rad.I0(sc, cc) - rad.I0(se, ce) : 2 * rad.pref * rad.K - rad.I0(sc, cc) - rad.I0(se, ce);
    } else if (nuR > 0 || rad.sub == 2) {
        sky = true;
        rad.amp(0, true, se, ce);
        tau = rad.I0(se, ce) + (nuR > 0 ? -1 : 1) * rad.I0(sc, cc);
    } else {
        sky = false;
        rad.amp(rEnd, false, se, ce);
        tau = rad.I0(sc, cc) - rad.I0(se, ce);
    }
    if (!(tau > 0) || !std::isfinite(tau)) return out;
    // the crossings of the equator before the end
    double taus[kClosedCross];
    const int nc = pol.crossings(tau, std::min(maxCross, kClosedCross), taus);
    for (int i = 0; i < nc; i++) {
        const double r = rad.rOf(taus[i], rad.sub == 4 ? Fc : Fc, nuR);
        if (!(r > 0) || !std::isfinite(r)) break;
        out.u[out.nCross++] = 1 / r;
    }
    double mu, dmu, G;
    pol.at(tau, std::sin(th), mu, dmu, G);
    mu = std::clamp(mu, -1.0, 1.0);
    if (sky) {
        double ip0, im0, ip1, im1;
        rad.Ipm(sc, cc, ip0, im0);
        rad.Ipm(se, ce, ip1, im1);
        const double sg = nuR > 0 ? -1 : 1;                    // inward to r4 and out again: I(camera) + I(infinity)
        const double Ip = ip1 + sg * ip0, Im = im1 + sg * im0;
        const double rp = rad.rp, rm = rad.rm;
        const double Iphi = 2 * a / (rp - rm) * ((rp - a * lam / 2) * Ip - (rm - a * lam / 2) * Im);   // (B2)
        const double phi = cam.phi - Iphi - lam * G + kPi * pol.poles(tau);
        const double sn = std::sqrt(1 - mu * mu);
        out.dir = Vec3{sn * std::cos(phi), sn * std::sin(phi), mu};
        out.pz = -dmu;                                         // du/ds = -1 at u = 0 (U(0) = 1, mino.h)
        out.kind = RayEnd::Sky;
    } else {
        const double drds = -std::sqrt(std::max(rad.R(rEnd), 0.0)), sn = std::sqrt(std::max(1 - mu * mu, 1e-300));
        out.rEnd = rEnd;
        out.thEnd = std::acos(mu);
        out.dthdr = drds != 0 ? (-dmu / sn) / drds : 0;
        out.kind = RayEnd::Horizon;
    }
    if (!std::isfinite(out.dir.x + out.dir.y + out.dir.z + out.dthdr)) out.kind = RayEnd::Lost;
    return out;
}

}  // namespace blackhole
