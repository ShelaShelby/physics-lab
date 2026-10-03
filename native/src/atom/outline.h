// The 90 % outline of the electron cloud: the boundary of the smallest region that holds 90 % of the probability of finding the electron.
// No graphics in here, so it can be tested.
//
// `orb` below is a real orbital's index or a complex state's id (atom_core.h): the region is {|psi|^2 >= c} for either.
// The model is the one the cloud already uses (atom_core.h psi()): ONE electron, non-relativistic, hydrogen-like orbitals with the nuclear charge Z.
// The region is {|psi|^2 >= c}, with c chosen so that the region holds exactly 90 % of the probability: for a fixed probability the region where the density
// is highest is the one with the smallest volume (the standard textbook definition of "the boundary surface that contains 90 % of the electron density").
// It is NOT a sphere with 90 % inside, except for s orbitals, where the two are the same. Orbitals with nodes (2s, 3s, 3p, ...) can give an inner and
// outer surfaces; all of them are in the region's boundary. For an atom with more electrons the real shape differs.
//
// psi(Z) is psi(1) with all lengths divided by Z (psi(orb, Z, x, y, z) = psi(orb, 1, Zx, Zy, Zz) up to a constant), so the outline is worked out once per
// orbital at Z = 1 (lengths in Bohr radii) and only scaled by 1/Z for the atom on screen. It is worked out on first use and kept.
#pragma once

#include <vector>

namespace atom {

struct OutSeg2 { float x0, y0, x1, y1; };
struct OutSeg3 { float x0, y0, z0, x1, y1, z1; };

constexpr int kOutlineGrid = 64;           // points per side of the grid the region is found on
constexpr double kOutlineProb = 0.9;

struct Outline {
    int orb = -1;
    double norm = 0;                       // integral of |psi|^2 over all space (psi is not normalised), a0^3
    double c = 0;                          // the level: the region is |psi|^2 >= c (the same units as psi^2)
    double half = 0;                       // the grid covers [-half, half]^3, a0
    double mass = 0;                       // the probability the grid says is inside the region (about 0.9)
    double rmax = 0;                       // the farthest the region reaches from the nucleus, a0 (to a grid step)
    std::vector<OutSeg3> wire;             // contour lines of the region's boundary in 5 planes (0, +-1/3, +-2/3 of its reach) for each of x, y, z = const (a faint wireframe for the 3D view), a0
    std::vector<OutSeg2> side;             // the outline of the region seen from the side (projected along y, in the x-z plane), a0
};

double orbitalNorm(int orb);               // integral of |psi|^2 over all space at Z = 1, a0^3 (radial integral × angular average)
const Outline& outlineOf(int orb);         // cached
// the contour |psi|^2 = c in a plane: vertical (y = offset, axes x and z) or horizontal (z = offset, axes x and y); offset in a0 at Z = 1.
std::vector<OutSeg2> outlineSlice(int orb, bool horizontal, double offset, int n = 200);

}  // namespace atom
