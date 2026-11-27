// 0.92.0: Lambert's problem (the orbit through two places in a given time) and the porkchop plot of Earth-to-Mars style transfers (no graphics, so it can be tested).
//
// The solver is Izzo's: D. Izzo, "Revisiting Lambert's problem", Celestial Mechanics and Dynamical Astronomy 121 (2015) 1-15 (arXiv:1403.2705v2, saved in the sources repo as
// data/celestial/izzo_2015_arxiv1403.2705.pdf, and its text as .txt). It iterates on the Lancaster-Blanchard variable x with Householder's method (section 4.1) on the time of flight
// T(x) of Eq. (18), whose derivatives are Eq. (22); the start is Eq. (30) with the single-revolution improvement that follows it; near x = 1 the time of flight is computed with
// Battin's series, Eq. (20) (the hypergeometric function there is 2F1(3, 1; 5/2; S1): the paper's "1F2" is a misprint, checked against Eq. (18) by the tests); the velocities come from
// Algorithm 1. ZERO REVOLUTIONS ONLY: the transfer goes less than one turn about the Sun, the short and the long way as the geometry decides, in the direction of the planets' own
// motion (counter-clockwise seen from the ecliptic north). One body pulls (the Sun): PATCHED CONICS, no planet's gravity during the transfer, the planets' own pull only sets the C3 and the v_inf.
#pragma once

#include <cstdint>
#include <vector>

#include "vec3.h"

namespace orbit {
namespace lambert {

struct Solution {
    bool ok = false;
    int iterations = 0;
    double x = 0;             // Izzo's variable: below 1 an ellipse, 1 a parabola, above 1 a hyperbola
    double T = 0, dT = 0;     // the non-dimensional time of flight and its slope dT/dx at the solution (Izzo's T and Eq. (22)): T / |dT| is how a rounding of the time of flight moves x
    Vec3 v1, v2;              // the velocities at r1 and at r2
};
// Izzo's non-dimensional time of flight T(x) for lambda = lam, zero revolutions: mode 1 Eq. (18) always, mode 2 Battin's series (Eq. (20)) always, 0 as the solver (the series within 0.01 of x = 1).
double timeOfFlightT(double lam, double x, int mode = 0);

// r1, r2 in any length unit, tof in the matching time unit, mu in length^3 / time^2; the velocities come out in length / time. Counter-clockwise about +z (the short or long way as the angle
// from r1 to r2 in that sense is below or above pi). Fails (ok = false) when tof <= 0, r1 or r2 is 0 or the two are along one line (the plane is not defined).
Solution solve(const Vec3& r1, const Vec3& r2, double tof, double mu);

// The transfer orbit drawn: n + 1 places along the conic from r1 (with velocity v1) over the time tof, equally spaced in the eccentric (hyperbolic) anomaly; the last one is r2. mu as for solve.
std::vector<Vec3> transferArc(const Vec3& r1, const Vec3& v1, double mu, double tof, int n);

// ---- the porkchop grid ----
// Planets on the lab's own table of JPL elements (Simple physics: orbit::planetStateAt) at the dates of a grid. Departure dates JD0 + i dDep, arrival dates JD1 + j dArr (Julian days TDB).
struct PorkchopSetup {
    int dep = 2, arr = 3;                  // planet numbers 0 Mercury ... 7 Neptune (Earth 2, Mars 3)
    double depJD0 = 0, depStep = 1, arrJD0 = 0, arrStep = 1;     // days
    int nDep = 0, nArr = 0;
};
// The planets' states at every date of the grid, in the units the solver is run in: length AU, time the day / (sqrt(AU^3 / GM_sun) in days) so that mu = 1 (the Sun's GM is the lab's kGMSun).
// Six numbers a date: x y z vx vy vz. The same arrays go to the GPU as floats.
struct PorkchopInput {
    PorkchopSetup s;
    std::vector<double> dep, arr;          // 6 * nDep, 6 * nArr
    double timeUnitDays = 0;               // one canonical time unit in days
    double velUnitKmS = 0;                 // one canonical speed in km/s
};
PorkchopInput porkchopInput(const PorkchopSetup& s);
struct PorkchopGrid {
    int nDep = 0, nArr = 0;
    std::vector<float> c3;                 // km^2/s^2, departure: |v1 - v_planet|^2, nDep * nArr, row i = departure date i; NaN where there is no transfer
    std::vector<float> vinf;               // km/s, arrival: |v2 - v_planet|
};
// The reference: every cell solved in double precision (and the fallback where the card cannot run it).
PorkchopGrid porkchopCpu(const PorkchopInput& in);
// One cell exactly as the grid has it (also what the drawn transfer uses): false where tof <= 0 or the solver fails.
bool porkchopCell(const PorkchopInput& in, int i, int j, double* c3KmS2, double* vinfKmS, double* tofDays);

// ---- the plot ----
// The plot's colour for t in [0, 1] (0: the lowest value, 1: three times that or more): a plain ramp from dark blue through teal and green to yellow, 8 bits a channel. The panel draws with it and
// the card's grid is compared with the processor's through it. A cell with no transfer is kNoTransferRGB.
void rampRGB(double t, unsigned char rgb[3]);
constexpr unsigned char kNoTransferRGB[3] = {20, 20, 24};
// The plot's pixels (one a cell, value range from the lowest finite value of `ref` to three times it) of two grids: how many differ in any channel by more than `levels` (a cell with a number
// in one grid and none in the other always counts).
long plotPixelsOver(const PorkchopGrid& ref, const PorkchopGrid& other, int levels, bool vinf = false);

// ---- the card's grid against the reference ----
// The bound on the card's C3 and v_inf (single precision against double). A cell's v_inf has the error
//     dv = K eps vk (1 + kappa) (1 + 1 / sigma^2),   eps = 2^-24, vk the speed unit (29.78 km/s),
// the product of the conditioning of the two stages that carry the rounding: kappa = T / |dT/dx| is how far a rounding of the time of flight (and of Eq. (18) itself, about 3e-7 in
// single precision) moves x, and 1 / sigma^2 = 1 / (1 - rho^2) of Algorithm 1 is how the velocities' radial and tangential parts blow up for a short arc (the two places nearly on
// one ray from the Sun, where rho = (r1 - r2) / c goes to -1). C3 = |v1 - v_planet|^2 then differs by at most 2 v_inf dv + dv^2. The form is derived; the constant K = 3000 is not: counting
// operations gave K = 64, which the four check grids of tests/vk_compare.cpp exceed (their worst ratio is 1926 with this form, in cells whose C3 is far above the plotted range), so K was
// set from them with a margin of 1.5.
constexpr double kBoundK = 3000;
double cardBoundVinf(double kappa, double sigma2, double velUnitKmS);   // dv, km/s
double cardBoundC3(double c3Ref, double dv);                            // km^2/s^2
struct GridCompare {
    long cells = 0, both = 0, flips = 0, flipLow = 0, over = 0;     // flips: solved by one side only; flipLow: of them, where the solved side's C3 is under 100 km^2/s^2; over: cells beyond the bound
    double worstC3OfBound = 0, worstC3 = 0, worstC3At = 0, worstVinf = 0, worstVinfOfBound = 0, worstKappa = 0;
};
GridCompare compareGrids(const PorkchopInput& in, const PorkchopGrid& ref, const PorkchopGrid& card);

}  // namespace lambert
}  // namespace orbit
