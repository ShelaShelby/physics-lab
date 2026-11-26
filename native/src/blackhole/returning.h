// Black Hole Lab (0.87.0, native/PLAN_REVIEW.md R11): RETURNING RADIATION. Physics only, no graphics. G = c = 1, lengths in M.
//
// SOURCE: C. Cunningham, ApJ 208, 534 (1976), "Returning radiation in accretion disks around black holes" (data/blackhole/scans, typed from the page images;
//   extract). Light that a thin disc sends out comes back to it, bent by the hole ("gravitational focusing"): "unimportant for thin disks around non-rotating
//   black holes" but significant for a/M ~ 1 (abstract). With y = (r/r+)^-2 (6b) and fluxes in units of F* = (3/8pi) Mdot M r^-3 (6c), (6e):
//     T_f(r_o, r_e) = (2 pi)^-1 r+^-2 r_o^3 Int cos n_o g^4 I [dOmega / d(g, r_e)] dg   (7a),   f_o(y_o) = Int_0^1 T_f(y_o, y_e) [f_o(y_e) + f_g(y_e)] dy_e   (8a)
//   n_o: the angle from the disc's normal at which the light arrives, in the gas's frame; g: the energy received over the energy sent (both in the gas's
//   frames); I = pi I_e / F_e (2): 1 for isotropic emission, as Tables 1-4 assume (pp. 536-537).
// WRITTEN HERE AS SUMS OVER RAYS. (7a) and (8a) together are an integral over the gas's sky at r_o (dOmega, its upper half):
//   f_o(r_o) = r_o^3 (1/pi) Int mu g^4 r_e^-3 f_e(r_e) dOmega, f_e = f_g + f_o (0 for a ray that does not come from the disc). The lab's directions: mu = t^2,
//   t = (i + 1/2) / nMu, psi = 2 pi (j + 1/2) / nPsi, each with the weight 4 t^3 (= 2 mu dmu/dt: the mean of the weights times the rest is the integral over
//   pi); mu = t^2 puts the rays where the returning light arrives, near the disc's plane. The same mean with f_e replaced by the indicator of a y_e bin, over
//   the bin's width, is T_f (the tests, there with cosine-weighted directions).
// THE RAYS. Each is followed from the disc at r_o into its past (spin_sky.h: the CPU's closed forms spinRayClosed, or the GPU's double-precision path,
//   spin_card.h cardRays) from a camera at rest in the FIDO's frame on the equator; the gas's directions are turned into the FIDO's by aberration (spin.h
//   aberrate; the gas moves along e_phi at V = (Omega - omega) varpi / alpha). The light comes from the disc where the ray first meets the plane, if that is at
//   r_e >= r_ms (the gas orbits there; C75 (A9) for its g: g = g(r_e, b) / g(r_o, b), spin.h discShiftKerr). A ray that first meets the plane inside r_ms is
//   taken as absorbed and not re-sent: the gas there falls in (Cunningham: it "remains optically thick"; the lab does not draw it). Rays to the sky or the
//   horizon bring nothing.
// SOLVED by iteration of (8a) on nodes even in (ln(r / r_ms))^(1/2) from r_ms to kReturnFar (f_o between them linear in ln r, constant beyond); f_g = the generated flux
//   without the returning light's torque: Cunningham's (8b) also lowers f_g by the stress of the returning light, which the lab leaves out (a stated limit).
// THE DISC THEN: the flux it sends is F_g + F_o (the returning light absorbed and sent again, Cunningham's assumption), so its temperature is
//   ((F_g + F_o) / sigma)^(1/4). Limits (the app says them): thin disc, steady, the returning light absorbed and re-emitted, no torque term (8b), the gas inside
//   r_ms dark, the emission isotropic for the transfer (Cunningham: limb darkening changes T_f by "not more than a few percent").
#pragma once

#include <memory>
#include <vector>

#include "blackhole/geodesic.h"

namespace blackhole {

// the lab's grid: 32 radii, 32 x 1024 rays each (choices; tests/test_blackhole.cpp testReturning087 (c) holds it within 1 % of 48 radii, 64 x 2048 rays).
// Measured (REPORT 0.87.0): even directions with 48 x 96 rays left f_o 15 % rough from node to node near r_ms; the crowding below took the worst
// difference from a finer grid from 10 % (at r_ms) to 0.6 % (at 1000 M), a = 0.99
constexpr int kReturnNodes = 32, kReturnMu = 32, kReturnPsi = 1024;
constexpr double kReturnFar = 1000;
// the directions (choices, measured: REPORT 0.87.0): mu = t^p, psi = pi (1 + v |v|^(q - 1)) with t even in (0, 1), v even in (-1, 1); at a node a share s of
// the way from r_ms to kReturnFar in ln r, p = 1 + 3 s and q = 1 + 2 s. Far out the returning light arrives within about M / r of the plane and from the
// hole's direction (psi = pi): there the rays crowd; at r_ms it comes from all round. The weights make each choice exact in the limit (the integral's
// change of variables): 2 mu dmu/dt x dpsi/dv / pi.
double returnShare(double a, double r);             // s                 // M: the outermost node (the disc reaches kOuterMax = 2000; f_o held there beyond)

double returnY(double a, double r);                 // y = (r / r+)^-2 (6b)
double returnR(double a, double y);                 // its inverse
double returnFg(double a, double r);                // f_g = F / F*: disc.h discFlux over 3 / (8 pi r^3)
// the gas's directions at a node (above: mu = t^2), mu = cos n_o, as looks on the gas's own axes (e_r, e_theta, e_phi; up = -e_theta), and their weights
std::vector<Vec3> returnLooks(int nMu, int nPsi, double s = 0);
std::vector<double> returnWeights(int nMu, int nPsi, double s = 0);
// the same as (psi, chi) pairs on the FIDO's sky at r on the equator (spin.h lookAt): the gas's looks through aberration
std::vector<double> returnPsiChi(double a, double r, const std::vector<Vec3>& looks);
// one ray's end: rE > 0 where it comes from the disc (r_e >= r_ms), its g^4; rE = 0: nothing comes back along it
struct ReturnHit { double rE = 0, g4 = 0; };
// from b and the crossings' u = 1/r (spin_sky.h SpinHit); a crossing within 1e-7 of r_o first is the ray's start, reported by some paths: the next is taken
ReturnHit returnHitOf(double a, double rO, double b, const double* u, int nU);
// the rays of one node on the CPU (spinRayClosed, the table's tolerance 1e-10), in returnLooks(nMu, nPsi, returnShare(a, rO))' order
std::vector<ReturnHit> returnRaysCpu(double a, double rO, int nMu = kReturnMu, int nPsi = kReturnPsi);

// the returning flux for one spin: f_o at the nodes, and F_o in disc.h discFlux's units (Mdot = M = 1) at any r
struct ReturnFlux {
    double a = 0, rp = 2, rms = 6;
    std::vector<double> lr, fo;                     // nodes (ln r, increasing) and f_o there
    int iterations = 0;
    double fluxAt(double r) const;                  // F_o = f_o(r) x 3 / (8 pi r^3); 0 inside r_ms
    double foAt(double r) const;
};
double returnNode(double a, int k, int nodes);      // r_k = r_ms (kReturnFar / r_ms)^((k / (n - 1))^2)
// the solve from the nodes' rays (hits[node], in returnLooks' order) and their weights (weights[node]); tol: the largest change of f_o between iterations
ReturnFlux returnSolve(double a, const std::vector<std::vector<ReturnHit>>& hits, const std::vector<std::vector<double>>& weights, double tol = 1e-12);
ReturnFlux returnFluxCpu(double a, int nodes = kReturnNodes, int nMu = kReturnMu, int nPsi = kReturnPsi);

// THE LAB'S STORE: one table per spin, made once (on the GPU when its light paths are on, else on the CPU) and kept. A table is never freed while the app
// runs, so a pointer handed out stays good (each is about 400 bytes). Null while the spin's table is not made yet: the disc is drawn without returning light.
const ReturnFlux* returnFluxFor(double a);
void returnFluxStore(const ReturnFlux& f);          // (the GPU job and returnFluxNow)
const ReturnFlux* returnFluxNow(double a);          // made on the CPU now if missing (tests, the picture compare)

}  // namespace blackhole
