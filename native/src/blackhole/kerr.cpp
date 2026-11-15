#include "blackhole/kerr.h"

#include <cmath>

namespace blackhole {

double horizon(double a) { return 1 + std::sqrt(1 - a * a); }
double staticLimit(double a, double theta) { const double c = std::cos(theta); return 1 + std::sqrt(1 - a * a * c * c); }
double photonOrbit(double a, bool pro) { return 2 * (1 + std::cos(2.0 / 3.0 * std::acos(pro ? -a : a))); }
double marginallyBound(double a, bool pro) { const double s = pro ? 1 : -1; return 2 - s * a + 2 * std::sqrt(1 - s * a); }
double innermostStable(double a, bool pro) {
    const double s = pro ? 1 : -1;
    const double z1 = 1 + std::cbrt(1 - a * a) * (std::cbrt(1 + a) + std::cbrt(1 - a)), z2 = std::sqrt(3 * a * a + z1 * z1);
    return 3 + z2 - s * std::sqrt((3 - z1) * (3 + z1 + 2 * z2));
}
double iscoPageThorne(double as) {
    const double z1 = 1 + std::cbrt(1 - as * as) * (std::cbrt(1 + as) + std::cbrt(1 - as)), z2 = std::sqrt(3 * as * as + z1 * z1);
    const double sg = as > 0 ? 1 : (as < 0 ? -1 : 0);
    const double x0 = std::sqrt(3 + z2 - sg * std::sqrt((3 - z1) * (3 + z1 + 2 * z2)));
    return x0 * x0;
}
double iscoCondition(double r, double a, bool pro) { return r * r - 6 * r + (pro ? 8 : -8) * a * std::sqrt(r) - 3 * a * a; }
double photonCondition(double r, double a, bool pro) { return r * std::sqrt(r) - 3 * std::sqrt(r) + (pro ? 2 : -2) * a; }

KerrMetric kerrMetric(double a, double r, double th) {
    const double c = std::cos(th), s = std::sin(th);
    KerrMetric m;
    m.rho2 = r * r + a * a * c * c;
    m.Delta = r * r - 2 * r + a * a;
    m.Sigma2 = (r * r + a * a) * (r * r + a * a) - a * a * m.Delta * s * s;
    m.alpha = std::sqrt(m.rho2 * m.Delta / m.Sigma2);
    m.omega = 2 * a * r / m.Sigma2;
    m.varpi = std::sqrt(m.Sigma2 / m.rho2) * s;
    return m;
}

double hoverShift(double a, double r, double theta, double b) {
    const KerrMetric m = kerrMetric(a, r, theta);
    return (1 - b * m.omega) / m.alpha;
}

}  // namespace blackhole
