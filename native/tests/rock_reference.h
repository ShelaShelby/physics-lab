// The textbook Kepler reference for the belt rocks, shared by tests/test_rocks.inc (orbit_tests) and tests/gl_compare.cpp (the GPU against it). 0.13 stage 4 part A.
// SOURCE (already in the About page, "Space and orbits"): NASA/JPL, "Keplerian Elements for Approximate Positions of the Major Planets" (https://ssd.jpl.nasa.gov/planets/approx_pos.html, page read
// 5 Oct 2026): Kepler's equation M = E - e sin E; x' = a (cos E - e), y' = a sqrt(1 - e^2) sin E (x' towards perihelion). n = sqrt(GM / a^3) is Kepler's third law. E comes from BISECTION: E - e sin E - M
// rises with E for every e < 1, so there is exactly one root and halving cannot miss it, however close e is to 1. Nothing here calls the lab's own solver.
// Needs: <cmath>, orbit/belts.h, orbit/units.h and `using namespace orbit` before it is included.
#pragma once

inline double rkBisect(double e, double M) {                              // E for M in [-pi, pi] (M brought into that range first)
    const double twoPi = 2 * units::kPi;
    M -= twoPi * std::floor(M / twoPi + 0.5);
    double lo = -units::kPi, hi = units::kPi;
    for (int k = 0; k < 200; k++) { const double mid = (lo + hi) / 2; if (mid - e * std::sin(mid) < M) lo = mid; else hi = mid; }
    return (lo + hi) / 2;
}

// the textbook position of a rock with these elements (all as the lab stores them: floats, widened), t seconds after the start
inline Vec3 rkTextbook(const BeltGroup& g, int k, double t) {
    const double AU = units::kAU, aN = double(g.aN[k]) * AU, a = double(g.a[k]) * AU, e = g.e[k];
    const double n = std::sqrt(units::kGMSun / (aN * aN * aN));            // Kepler's third law
    double M = double(g.M0[k]) + n * t;
    if (g.libA[k] != 0) M += double(g.libA[k]) * std::sin(t / units::kYear / 150 * 6.2832 + double(g.libPh[k]));   // the Trojans' sway (a lab rule, not a textbook one: the web's, 150-year period)
    const double E = rkBisect(e, M);
    const double xp = a * (std::cos(E) - e), yp = a * std::sqrt(1 - e * e) * std::sin(E);
    return {xp * g.Px[k] + yp * g.Qx[k], xp * g.Py[k] + yp * g.Qy[k], xp * g.Pz[k] + yp * g.Qz[k]};
}

// a one-rock group on a given orbit in the ecliptic tilted by `incl` (P, Q: the orbit plane's unit vectors, from the node and perihelion angles as the web builds them)
inline BeltGroup rkGroup(double aAU, double e, double M0, double incl, double om, double Om, double libA = 0, double libPh = 0) {
    BeltGroup g; g.key = "test"; g.color = "#ffffff"; g.n = 1;
    for (auto* v : {&g.aN, &g.a, &g.e, &g.M0, &g.libA, &g.libPh, &g.Px, &g.Py, &g.Pz, &g.Qx, &g.Qy, &g.Qz}) v->resize(1);
    const double co = std::cos(om), so = std::sin(om), cO = std::cos(Om), sO = std::sin(Om), cI = std::cos(incl), sI = std::sin(incl);
    g.aN[0] = g.a[0] = float(aAU); g.e[0] = float(e); g.M0[0] = float(M0); g.libA[0] = float(libA); g.libPh[0] = float(libPh);
    g.Px[0] = float(co * cO - so * sO * cI); g.Py[0] = float(co * sO + so * cO * cI); g.Pz[0] = float(so * sI);
    g.Qx[0] = float(-so * cO - co * sO * cI); g.Qy[0] = float(-so * sO + co * cO * cI); g.Qz[0] = float(co * sI);
    return g;
}

