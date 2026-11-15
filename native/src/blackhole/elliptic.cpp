#include "blackhole/elliptic.h"

#include <algorithm>
#include <cmath>

namespace blackhole {

// Carlson's duplication: each step replaces (x, y, z) by ((x + l) / 4, ...), l = sqrt(xy) + sqrt(yz) + sqrt(zx), which keeps R_F; once the three are within
// a relative 8e-4 of their mean A, the fifth-order series in the differences ends it (relative error below about 1e-17).
double carlsonRF(double x, double y, double z) {
    for (int it = 0; it < 200; it++) {
        const double A = (x + y + z) / 3, dx = (A - x) / A, dy = (A - y) / A, dz = (A - z) / A;
        if (std::max({std::fabs(dx), std::fabs(dy), std::fabs(dz)}) < 8e-4) {
            const double e2 = dx * dy - dz * dz, e3 = dx * dy * dz;
            return (1 + (e2 / 24 - 0.1 - 3.0 / 44 * e3) * e2 + e3 / 14) / std::sqrt(A);
        }
        const double sx = std::sqrt(x), sy = std::sqrt(y), sz = std::sqrt(z), l = sx * sy + sy * sz + sz * sx;
        x = (x + l) / 4; y = (y + l) / 4; z = (z + l) / 4;
    }
    return std::nan("");
}

double ellKc(double kc2) { return carlsonRF(0, kc2, 1); }
double ellK(double k) { return ellKc(1 - k * k); }
double ellFsc(double s, double c, double kc2) { return s * carlsonRF(c * c, c * c + kc2 * s * s, 1); }   // 1 - k^2 s^2 = c^2 + kc2 s^2

double ellFc(double phi, double kc2) {
    const double kPi = 3.14159265358979323846;
    const double n = std::floor(phi / kPi + 0.5), p = phi - n * kPi;   // p in [-pi/2, pi/2]; F(p + n pi) = F(p) + 2 n K
    const double f = ellFsc(std::sin(p), std::cos(p), kc2);
    return n == 0 ? f : f + 2 * n * ellKc(kc2);
}
double ellF(double phi, double k) { return ellFc(phi, 1 - k * k); }

}  // namespace blackhole
