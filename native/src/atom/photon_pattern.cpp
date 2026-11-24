#include "photon_pattern.h"

#include <algorithm>
#include <cstddef>

#include <array>
#include <cmath>
#include <map>
#include <utility>
#include <vector>

#include "atom_core.h"

namespace atom {

namespace {
constexpr double kPiP = 3.14159265358979323846;
std::complex<double> angOf(int id, double x, double y, double z) {
    const Orbital& o = orbitals()[std::size_t(stateOrbital(id))];
    return isComplexState(id) ? angularComplex(o.l, stateM(id), x, y, z) : std::complex<double>(angular(o.ang, x, y, z), 0);
}
}  // namespace

bool dipoleUnit(int fromId, int toId, std::complex<double> u[3]) {
    static const std::vector<std::array<double, 3>> sphere = [] {
        std::vector<std::array<double, 3>> P;
        const int N = 20000;
        const double g = kPiP * (3 - std::sqrt(5.0));
        for (int i = 0; i < N; i++) {
            const double z = 1 - 2 * (i + 0.5) / N, r = std::sqrt(1 - z * z);
            P.push_back({r * std::cos(g * i), r * std::sin(g * i), z});
        }
        return P;
    }();
    // d and its size against the two angular parts' own sizes, as dipoleAngularSq (atom_core.cpp): |d|^2 / (sum |A|^2 sum |B|^2)
    static std::map<std::pair<int, int>, std::pair<std::array<std::complex<double>, 3>, double>> cache;
    auto it = cache.find({fromId, toId});
    if (it == cache.end()) {
        std::array<std::complex<double>, 3> d{};
        double na = 0, nb = 0;
        for (const auto& v : sphere) {
            const std::complex<double> A = angOf(fromId, v[0], v[1], v[2]), B = angOf(toId, v[0], v[1], v[2]);
            const std::complex<double> a = std::conj(B) * A;
            for (int c = 0; c < 3; c++) d[std::size_t(c)] += a * v[std::size_t(c)];
            na += std::norm(A); nb += std::norm(B);
        }
        const double rel = (std::norm(d[0]) + std::norm(d[1]) + std::norm(d[2])) / (na * nb + 1e-300);
        it = cache.emplace(std::make_pair(fromId, toId), std::make_pair(d, rel)).first;
    }
    const std::array<std::complex<double>, 3>& d = it->second.first;
    if (!(it->second.second > 1e-9)) return false;                     // (no dipole line: the integral is rounding only)
    const double n2 = std::norm(d[0]) + std::norm(d[1]) + std::norm(d[2]);
    const double s = 1 / std::sqrt(n2);
    for (int c = 0; c < 3; c++) u[c] = d[std::size_t(c)] * s;
    return true;
}

double dipolePattern(const std::complex<double> u[3], const double n[3]) {
    const std::complex<double> nd = n[0] * u[0] + n[1] * u[1] + n[2] * u[2];
    return 3 / (8 * kPiP) * (1 - std::norm(nd));
}

void drawDipoleDirection(const std::complex<double> u[3], const std::function<double()>& rnd, double n[3]) {
    for (int tries = 0; tries < 1000; tries++) {                       // (the pattern's top is 3 / (8 pi): accepted with p / top = 1 - |n . u|^2, at least 2/3 of the time on average)
        const double z = 2 * rnd() - 1, ph = 2 * kPiP * rnd(), r = std::sqrt(std::max(0.0, 1 - z * z));
        n[0] = r * std::cos(ph); n[1] = r * std::sin(ph); n[2] = z;
        const std::complex<double> nd = n[0] * u[0] + n[1] * u[1] + n[2] * u[2];
        if (rnd() < 1 - std::norm(nd)) return;
    }
}

}  // namespace atom
