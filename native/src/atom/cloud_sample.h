#pragma once
// 0.37: the electron cloud's dots as EXACT, INDEPENDENT samples of |psi|^2 (inverse transform and rejection sampling), in place of the Metropolis chain of 0.1-0.36.
//
// Why: the chain's dots were spread like |psi|^2 on average, but neighbours were close to each other, so the 12,000 dots on screen were worth only about 100-350
// independent ones (26 for "which lobe of 2pz"), and one lobe often looked clearly denser than the other (research report, 6 Nov 2026). Each dot made here is drawn on
// its own, from the exact density, so any number of them is an honest picture of |psi|^2 with only the usual 1/sqrt(N) scatter.
//
// How (exact, no tables, no cut-off): psi = R(r) P(x, y, z) with P the angular polynomial (it holds r^l), so |psi|^2 d^3r factorises into a radial and an angular part.
//  - radius: with x = 2 Z r / n the radial density is f(x) e^(-x) dx, f(x) = x^(2l+2) L(x)^2, L = L_(n-l-1)^(2l+1) (atom_core.h, lagCoef). Its distribution function has a closed form
//    (f is a polynomial, and the upper incomplete gamma function of whole order is e^(-x) times a polynomial), so x is found by inverting it at a uniform random number (Newton's method in
//    a bracket, to 1e-13): the inverse-transform method, exact up to rounding, one random number per radius;
//  - direction: uniform on the sphere, kept with probability |P(u)|^2 / M, where M = (2l+1)/(4 pi) times the sphere integral of |P|^2 (outline.cpp, angularNormSq) bounds |P(u)|^2:
//    P / sqrt(norm) is a normalised combination of the Y_l^m, and for those |f(u)|^2 <= (2l+1)/(4 pi) (the addition theorem, sum over m of |Y_l^m|^2 = (2l+1)/(4 pi), with
//    Cauchy-Schwarz). This holds for the real orbitals and the complex m states alike.
// The direction step is textbook rejection sampling and the radius step the inverse-transform method: the point has exactly the target density. The tests compare the samples with exact moments.

#include <vector>

#include "orbit/belts.h"

namespace atom {

struct CloudPoint { double x = 0, y = 0, z = 0; };       // Bohr radii

// One exact sample of |psi|^2 of a real orbital or a complex state (atom_core.h ids), for nuclear charge Z. Uses a variable count of random numbers from r.
CloudPoint sampleState(int id, int Z, orbit::Mulberry32& r);

// The Slice view (0.37): points ON the cut plane {x[axis] = off} (axis 1: y, 2: z; off in Bohr radii), with exactly the density |psi|^2 restricted to the plane (the conditional density), so
// a node on the plane is empty to the last dot. One attempt: true and a point, or false (rejected; try again). planeMass is the mass of the attempt's envelope on the plane (with the
// normalisation of normFactor), which a mixture of two states needs to pick a component (cloud_sample.cpp).
bool tryStateOnPlane(int id, int Z, int axis, double off, orbit::Mulberry32& r, CloudPoint& out);
double planeMass(int id, int Z, int axis, double off);
// 0.57.0: the same attempt with what depends only on the state and the plane worked out once (a batch of attempts on one plane): tryPoint gives the same point from
// the same random numbers as tryStateOnPlane, bit for bit. One attempt takes exactly three random numbers, except where the radius comes out 0 (two; probability ~2^-32).
struct PlaneSampler {
    int id = 0, Z = 1, axis = 1, n = 1, l = 0, ang = 0, m = 0;
    bool complex = false;
    double off = 0, x0 = 0, g0 = 0, bound = 1;
    const void* cdf = nullptr;                             // the radial tail's table (cloud_sample.cpp)
    bool tryPoint(orbit::Mulberry32& r, CloudPoint& out) const;
    const std::vector<double>& tailPoly() const;           // h_j of the tail G(x) = e^(-x) sum h_j x^j on the plane (the card's copy, 0.57.0)
};
PlaneSampler planeSampler(int id, int Z, int axis, double off);

// 0.39: the direction step alone (a unit vector with density |P(u)|^2 / norm): a many-electron orbital (atom/hf.h) takes its radius from its own Hartree-Fock radial function and its
// direction from the one-electron orbital of the same l and orientation (the angular part does not depend on the radial one).
void sampleDirection(int id, orbit::Mulberry32& r, double& ux, double& uy, double& uz);

// The bound M on |P(u)|^2 used above (exposed for the tests).
double angularBound(int id);

}  // namespace atom
