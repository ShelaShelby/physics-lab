#include "cloud_sample.h"

#include <cmath>
#include <map>
#include <vector>

#include "atom_core.h"

namespace atom {

double angularNormSq(int orb);   // outline.cpp: the sphere integral of the squared angular polynomial (exact)

double angularBound(int id) {
    constexpr double pi = 3.14159265358979323846;
    const int l = orbitals()[stateOrbital(id)].l;
    return angularNormSq(id) * (2 * l + 1) / (4 * pi);
}

namespace {

// The radial distribution of one real orbital in x = 2 Z r / n: f(x) e^(-x) dx with f(x) = x^(2l+2) L(x)^2 = sum c_m x^m (whole space; on a plane, below, the power is 2l+1). Its upper tail has a closed form,
//   G(x) = integral from x to infinity of f(t) e^(-t) dt = e^(-x) H(x),   H(x) = sum_j h_j x^j,   h_j = sum over m >= j of c_m m! / j!
// (from integral_x^inf t^m e^(-t) dt = m! e^(-x) sum_(j=0..m) x^j / j!, the upper incomplete gamma function of whole order), and G(0) = sum c_m m! is the total.
struct RadialCdf { std::vector<double> c, h; double total = 0; };

const RadialCdf& cdfOf(int orb, bool plane) {
    static std::map<int, RadialCdf> cache;
    const int key = 2 * orb + (plane ? 1 : 0);
    auto it = cache.find(key);
    if (it != cache.end()) return it->second;
    const Orbital& o = orbitals()[orb];
    const std::size_t deg = o.lag.size() - 1, m0 = std::size_t(2 * o.l + (plane ? 1 : 2)), mMax = m0 + 2 * deg;
    RadialCdf d;
    d.c.assign(mMax + 1, 0.0);
    for (std::size_t i = 0; i <= deg; i++)
        for (std::size_t j = 0; j <= deg; j++) d.c[m0 + i + j] += o.lag[i] * o.lag[j];
    std::vector<double> fact(mMax + 1, 1.0);
    for (std::size_t m = 1; m <= mMax; m++) fact[m] = fact[m - 1] * double(m);
    d.h.assign(mMax + 1, 0.0);
    for (std::size_t j = 0; j <= mMax; j++)
        for (std::size_t m = j; m <= mMax; m++) d.h[j] += d.c[m] * fact[m] / fact[j];
    d.total = d.h[0];
    return cache.emplace(key, std::move(d)).first->second;
}

double polyAt(const std::vector<double>& a, double x) { double v = 0; for (std::size_t i = a.size(); i-- > 0;) v = v * x + a[i]; return v; }

double tailG(const RadialCdf& d, double x) { return std::exp(-x) * polyAt(d.h, x); }

// The x >= x0 whose upper tail holds the share u of the tail above x0 (u in (0, 1]): G(x) = u G(x0), solved by Newton's method kept inside a bracket (bisection when a step leaves it), to a
// relative step of 1e-13. G falls monotonically (G' = -f e^(-x) <= 0), so the root is unique: this is the inverse of the exact distribution function, one random number per radius.
double radiusX(const RadialCdf& d, double u, double x0 = 0, double g0 = -1) {
    const double target = u * (x0 > 0 ? (g0 >= 0 ? g0 : tailG(d, x0)) : d.total);
    const auto G = [&](double x) { return tailG(d, x); };
    double lo = x0, hi = x0 + 4;
    while (G(hi) > target && hi < 1e4) { lo = hi; hi = x0 + 2 * (hi - x0); }
    double x = 0.5 * (lo + hi);
    for (int it = 0; it < 200; it++) {
        const double g = G(x) - target;
        if (g > 0) lo = x; else hi = x;
        const double fx = polyAt(d.c, x) * std::exp(-x);
        double nx = fx > 0 ? x + g / fx : 0.5 * (lo + hi);                // G' = -f e^(-x)
        if (!(nx > lo && nx < hi)) nx = 0.5 * (lo + hi);
        if (std::fabs(nx - x) <= 1e-13 * (1 + x) || hi - lo <= 1e-13 * (1 + x)) return nx;
        x = nx;
    }
    return x;
}

}  // namespace

void sampleDirection(int id, orbit::Mulberry32& r, double& ux, double& uy, double& uz) {
    constexpr double pi = 3.14159265358979323846;
    const Orbital& o = orbitals()[stateOrbital(id)];
    const double M = angularBound(id);
    const bool cx = isComplexState(id);
    for (;;) {
        const double z = 2 * r.next() - 1, ph = 2 * pi * r.next(), s = std::sqrt(std::max(0.0, 1 - z * z));
        const double x = s * std::cos(ph), y = s * std::sin(ph);
        const double a2 = cx ? std::norm(angularComplex(o.l, stateM(id), x, y, z)) : [&] { const double a = angular(o.ang, x, y, z); return a * a; }();
        if (r.next() * M <= a2) { ux = x; uy = y; uz = z; return; }
    }
}

CloudPoint sampleState(int id, int Z, orbit::Mulberry32& r) {
    const int orb = stateOrbital(id);
    const Orbital& o = orbitals()[orb];
    const double rr = radiusX(cdfOf(orb, false), 1 - r.next()) * o.n / (2.0 * Z);     // 1 - u is in (0, 1]
    double ux, uy, uz;
    sampleDirection(id, r, ux, uy, uz);                  // (0.39: the direction step on its own, for the many-electron orbitals too; the same random numbers in the same order)
    return {rr * ux, rr * uy, rr * uz};
}

// ---- on a plane (the Slice view) ----
// The density on the plane {coordinate `axis` = off} is |psi|^2 = |P(u)|^2 R(s)^2 (Z s)^(2l) at distance s from the nucleus (P the angular polynomial at the unit vector u, R the radial part
// of atom_core.cpp). The envelope M R(s)^2 (Z s)^(2l) (M = angularBound) depends on s alone; in polar coordinates on the plane (rho, phi), s^2 = rho^2 + off^2, rho drho = s ds, so its share of
// a ring is 2 pi s ds times it, which in x = 2 Z s / n is x^(2l+1) L(x)^2 e^(-x) dx, from x0 = 2 Z |off| / n: the same closed-form tail as above with the power 2l+1. One attempt: x from that
// tail (inverse transform), phi uniform, the point (rho cos phi, rho sin phi) on the plane, kept with probability |P(u)|^2 / M. Accepted points have exactly the density on the plane.
static CloudPoint onPlane(int axis, double off, double rho, double phi) {
    const double a = rho * std::cos(phi), b = rho * std::sin(phi);
    return axis == 2 ? CloudPoint{a, b, off} : CloudPoint{a, off, b};
}

double planeMass(int id, int Z, int axis, double off) {
    (void)axis;
    constexpr double pi = 3.14159265358979323846;
    const Orbital& o = orbitals()[stateOrbital(id)];
    const double x0 = 2.0 * Z * std::fabs(off) / o.n;
    const double nf = normFactor(id, Z), half = o.n / 2.0, sc = o.n / (2.0 * Z);
    // psi = nf R(Z s) (Z s)^l P(u); (Z s)^(2l) = (n x / 2)^(2l); s ds = (n / 2Z)^2 x dx
    return nf * nf * angularBound(id) * 2 * pi * std::pow(half, 2 * o.l) * sc * sc * tailG(cdfOf(stateOrbital(id), true), x0);
}

bool tryStateOnPlane(int id, int Z, int axis, double off, orbit::Mulberry32& r, CloudPoint& out) {
    constexpr double pi = 3.14159265358979323846;
    const int orb = stateOrbital(id);
    const Orbital& o = orbitals()[orb];
    const double x0 = 2.0 * Z * std::fabs(off) / o.n;
    const RadialCdf& d = cdfOf(orb, true);
    const double g0 = tailG(d, x0);
    if (!(g0 > 0)) return false;                                             // (the plane is beyond the reach of double numbers)
    const double s = radiusX(d, 1 - r.next(), x0, g0) * o.n / (2.0 * Z);
    const double rho = std::sqrt(std::max(0.0, s * s - off * off));
    const CloudPoint q = onPlane(axis, off, rho, 2 * pi * r.next());
    if (!(s > 0)) return false;
    const double ux = q.x / s, uy = q.y / s, uz = q.z / s;
    const double a2 = isComplexState(id) ? std::norm(angularComplex(o.l, stateM(id), ux, uy, uz)) : [&] { const double a = angular(o.ang, ux, uy, uz); return a * a; }();
    if (r.next() * angularBound(id) > a2) return false;
    out = q;
    return true;
}

}  // namespace atom
