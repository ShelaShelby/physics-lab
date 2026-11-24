// 0.78.0: the direction an emitted (or absorbed) photon takes: the electric-dipole radiation pattern.
// A dipole d oscillating at the line's frequency radiates dP/dOmega proportional to |(n x d) x n|^2 = |d|^2 - |n . d|^2 (n the direction, d complex):
// J. D. Jackson, "Classical Electrodynamics", 3rd ed. (Wiley, 1999), sect. 9.2 (electric dipole fields and radiation). In quantum mechanics d is the
// transition's dipole matrix element <final| r |initial> (B. H. Bransden and C. J. Joachain, "Physics of Atoms and Molecules", 2nd ed. (2003), ch. 4,
// the dipole approximation). Normalised to 1 over the sphere: p(n) = 3 / (8 pi) (1 - |n . u|^2), u = d / |d|.
//   Delta m = 0 (d along z): p = 3 / (8 pi) sin^2 theta about the quantisation axis z: no light along z.
//   Delta m = +-1 (d = (x +- i y) / sqrt 2): p = 3 / (16 pi) (1 + cos^2 theta).
// A real orbital (2p x ...) is a mix of +m and -m: its d is real and points along a fixed axis, p = 3 / (8 pi) sin^2 of the angle from that axis.
// Limits: dipole (E1) light only, no polarisation drawn, one electron (the many-electron atoms do not know m: their photons keep a random direction in the
// picture's plane, and the text says so).
#pragma once

#include <complex>
#include <functional>

namespace atom {

// d = <to| (x, y, z) / r |from>, the angular integral over the sphere (the radial part only scales it), made a unit vector. false: d = 0 (no dipole line).
// Ids are real orbitals or complex states (atom_core.h complexState). Worked out on 20,000 Fibonacci points, cached.
bool dipoleUnit(int fromId, int toId, std::complex<double> u[3]);
// p(n) = 3 / (8 pi) (1 - |n . u|^2): the photons' directions per steradian (n a unit vector)
double dipolePattern(const std::complex<double> u[3], const double n[3]);
// a direction drawn from p (rejection from directions even over the sphere; rnd gives numbers in [0, 1))
void drawDipoleDirection(const std::complex<double> u[3], const std::function<double()>& rnd, double n[3]);

}  // namespace atom
