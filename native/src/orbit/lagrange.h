// 0.92.0: the five Lagrange points of the circular restricted three-body problem (no graphics, so it can be tested), and the Kirkwood gaps of the belt.
//
// The model: two masses on circular orbits about their centre of mass, and a third body of no mass. Distances are in units of the separation a of the two masses and
// the frame turns with them at angular speed w = sqrt(G (M1 + M2) / a^3) (so w = 1 here). mu = M2 / (M1 + M2) with M2 the smaller mass. M1 is at (-mu, 0), M2 at
// (1 - mu, 0), the centre of mass at the origin. L1 lies between them, L2 beyond M2, L3 beyond M1, L4 and L5 at the corners of the equilateral triangles on the line M1-M2.
// What it leaves out: orbits that are not circular, the third body's own mass, any other body, radiation pressure.
//
// Source: J. Tatum, "Celestial Mechanics", University of Victoria, chapter 16 (The Restricted Three-Body Problem), saved as data/celestial/tatum_celmechs_ch16.pdf in the
// sources repo. Equation 16.1.9 is the balance of the pulls and the centrifugal force on the line (L1), 16.1.10 to 16.1.23 the same as a quintic for each point (the tests use
// them), 16.1.25 the equivalent potential, 16.2.26 and 16.2.27 the motion near L4 and L5 and its condition for stability (Routh's criterion). Tatum numbers his
// points differently (his L3 is the inner point, his L1 the outer point beside M1, his L2 the outer point beside M2) and writes mu for M1 / (M1 + M2). Here the
// usual names are used (L1 inner), and the tests convert.
#pragma once

#include <string>
#include <vector>

namespace orbit {
namespace lagrange {

struct Points {
    double mu = 0;
    double x[5] = {}, y[5] = {};            // L1 .. L5, in units of the separation, centre of mass at the origin
};
// The five points for mass fraction mu = M2 / (M1 + M2) in (0, 0.5]. The collinear roots are found by bisection on the force on the line (Tatum 16.1.9), then polished by Newton's method.
Points points(double mu);
// The net acceleration of a body at rest at (x, y) in the rotating frame (gravity of both masses plus the centrifugal pull; the Coriolis force is zero at rest), units a w^2. Zero at a Lagrange point.
void restAcceleration(double mu, double x, double y, double& ax, double& ay);
// Routh's criterion for L4 and L5: they are stable (small orbits about them exist) when 27 mu (1 - mu) < 1, i.e. mu below this value, (1 - sqrt(23/27)) / 2 = 0.0385209.
double routhCriticalMu();
bool trianglePointsStable(double mu);
// Tatum 16.2.26 and 16.2.27: the two frequencies of a small orbit about L4 or L5 in the rotating frame, in units of w; ok is false when mu is above the critical value.
struct Libration { bool ok = false; double fast = 0, slow = 0; };
Libration libration(double mu);
// The Hill radius of M2 in units of the separation, (mu / 3)^(1/3).
double hillRadius(double mu);

// The pairs the lab offers: each Sun-planet pair of the planets (the planet's whole system, so Earth's is Earth plus Moon) and each planet with its big moons.
struct System {
    std::string name;         // "Sun - Earth", "Earth - Moon"
    std::string heavy, light;
    double mu = 0;            // light / (heavy + light), from GM values
    double sepKm = 0;         // the separation a: the planet's semi-major axis (JPL elements) or the moon's mean distance (NASA fact sheets)
    double gmHeavy = 0, gmLight = 0;   // km^3/s^2
    std::string source;
};
const std::vector<System>& systems();
// The points of a system in kilometres from the centre of mass: x along the line from the heavy mass to the light one.
struct Table { double xKm[5], yKm[5], inSep[5], fromLightKm[5]; };
Table table(const System& s);

}  // namespace lagrange

namespace kirkwood {

// A mean-motion resonance with Jupiter p:q (the rock goes round p times while Jupiter goes round q times): by Kepler's third law (T ~ a^(3/2)) a = a_J (q / p)^(2/3).
struct Resonance { int p = 0, q = 0; double aAU = 0; };
const std::vector<Resonance>& resonances();       // 4:1, 3:1, 5:2, 7:3, 2:1, with a_J from the lab's Jupiter
double jupiterAU();
// How many of the lab's main-belt rocks have a semi-major axis in each bin [lo + i w, lo + (i + 1) w); below and above count the rest of the group.
struct Histogram { double lo = 0, w = 0; std::vector<int> count; int below = 0, above = 0, total = 0; };
Histogram mainBelt(double lo, double hi, int bins);

}  // namespace kirkwood
}  // namespace orbit
