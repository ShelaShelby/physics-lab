// Black Hole Lab (0.54.3): the ray integrator, the reference for every picture (double precision, CPU). Physics only, no graphics.
// Units: G = c = 1, lengths in M.
//
// HOW IT IS BUILT SO A SECOND CHART CAN BE ADDED (plan section 1, last paragraph): nothing here hard-wires Boyer-Lindquist coordinates.
//   - Chart: one coordinate chart of one spacetime. It turns a camera's look direction into a ray (state + constants of motion), gives the ray
//     equations d(state)/dzeta, says when a ray has ended (far sky or horizon) and turns the end state into a direction on the far sky.
//   - Camera: an event (the chart's coordinates) and a 4-velocity. 0.54.3 has the hovering camera only (the FIDO of J15 (A.3)); a moving camera adds its
//     speed beta and direction B relative to the FIDO (J15 (A.9), (A.10)) as new fields that default to 0, so callers do not change.
//   - trace(chart, camera, look) -> RayEnd {sky direction | horizon}. Step 5 (falling in) adds a chart that crosses the horizon (ingoing Kerr; Kruskal for
//     a = 0, Riazuelo 2019 (27)-(36)) as a second Chart; callers keep calling trace(). A chart switch along one ray would be a Chart that holds both and
//     changes the state's meaning inside rhs/ended; the interface does not need to change for it.
//
// THE KERR CHART (KerrBL): J15 = O. James, E. von Tunzelmann, P. Franklin, K. S. Thorne, Class. Quantum Grav. 32, 065001 (2015), Appendix A.1 (read from the PDF):
//   the camera's look direction N on its local sky (unit vector on the FIDO's basis e_r, e_theta, e_phi of (A.3)); the light's own direction of motion is
//   n_F = -N for a hovering camera ((A.9) with beta = 0).
//   Momenta, with -p_t = 1:  p_r = E_F rho / Delta^(1/2) n_F,r,  p_theta = E_F rho n_F,theta,  p_phi = E_F varpi n_F,phi,  E_F = 1 / (alpha + omega varpi n_F,phi)  (A.11)
//   Constants:  b = p_phi,  q = p_theta^2 + cos^2 theta (b^2 / sin^2 theta - a^2)                                                                          (A.12)
//   With P = r^2 + a^2 - a b, R = P^2 - Delta [(b - a)^2 + q], Theta = q - cos^2 theta (b^2 / sin^2 theta - a^2)                                           (A.4)
//   the ray equations (A.15), integrated backward in time (zeta decreasing):
//     dr/dzeta = Delta p_r / rho^2,  dtheta/dzeta = p_theta / rho^2,  dphi/dzeta = -d/db [(R + Delta Theta) / (2 Delta rho^2)],
//     dp_r/dzeta = d/dr [H],  dp_theta/dzeta = d/dtheta [H],  H = -Delta p_r^2 / (2 rho^2) - p_theta^2 / (2 rho^2) + (R + Delta Theta) / (2 Delta rho^2).
//   The derivatives are written out by hand in geodesic.cpp; tests check them against finite differences of H and check that along every ray
//   the null condition H = 0 holds and q recomputed from (theta, p_theta) by (A.12) stays the q the ray started with (conservation laws).
//   The step: Dormand-Prince 5(4) with step control (J15 App. A.4 used Runge-Kutta-Fehlberg; same idea, an embedded pair of orders 5 and 4).
//   0.54.3 tests it at a = 0 against the closed forms (schwarzschild.h) and at a = 0.5, 0.9, 0.999 by its conservation laws only; the spinning hole's closed
//   shadow test (J15 (A.5), (A.6), (A.13), (A.14)) and the far critical curve (Gralla & Lupsasca 2020b (38)-(40), (55)) are the spin step's (plan step 3) references.
//   A ray has reached the far sky at r = rFar (the larger of the chart's rFar, default 1e5 M, and 1e4 times the camera's radius): its direction there is the
//   direction of its straight-line motion, which differs from the direction at infinity by about M b / rFar^2 (the rest of the weak bending; b is at most
//   about the camera's radius), under 1e-8 rad.
//   It has reached the horizon when it is inside r+ + 1e-3 M going inward (in BL coordinates p_r grows without bound there). A practical cut: R(r+) = P(r+)^2 >= 0
//   by (A.4), so a ray turns around that close to the horizon only for the rare b with P(r+) ~ 0; tested at a = 0 against Synge's edge (test_blackhole.cpp).
#pragma once

#include <vector>

#include "orbit/vec3.h"

namespace blackhole {

using orbit::Vec3;

struct Camera {                  // hovering (a FIDO); see the header comment for the moving camera of step 5
    double r = 10, theta = 1.5707963267948966, phi = 0;
};

constexpr int kMaxState = 8;
struct Ray {
    int n = 0;                   // how many numbers of y are used
    double y[kMaxState] = {};    // the state, in the chart's meaning
    double c[4] = {};            // constants of motion, in the chart's meaning
};

struct RayEnd {
    enum Kind { Sky, Horizon, Lost } kind = Lost;   // Lost: the step budget ran out (a ray within about 1e-15 of a trapped photon orbit)
    Vec3 dir;                    // Sky: the unit direction it came from, on the chart's far sky (Cartesian axes of the chart at infinity)
    int steps = 0;
};

class Chart {
public:
    virtual ~Chart() = default;
    virtual bool launch(const Camera& cam, const Vec3& look, Ray& ray) const = 0;   // look: unit vector on the camera's local frame (chart-defined order)
    virtual void rhs(const Ray& ray, const double* y, double* dy) const = 0;         // d y / d zeta
    // 0 = go on, 1 = far sky, 2 = horizon. zetaSign: the direction of integration (-1: backward in time, the photon's past)
    virtual int ended(const Ray& ray, const double* y, const double* dy, double zetaSign) const = 0;
    virtual Vec3 farDirection(const Ray& ray, const double* y, const double* dy, double zetaSign) const = 0;
    virtual double nullCondition(const Ray& ray, const double* y) const = 0;         // 0 for a light ray (a check, not used to integrate)
    virtual double rFar() const = 0;
};

// The Kerr hole in Boyer-Lindquist coordinates, M = 1. State y = (r, theta, phi, p_r, p_theta); constants c = (b, q).
// look = (N_r, N_theta, N_phi) on the FIDO's e_r, e_theta, e_phi (J15 (A.3)): where on its sky the camera looks.
class KerrBL : public Chart {
public:
    explicit KerrBL(double a, double rFar = 1e5) : a_(a), rFar_(rFar) {}
    bool launch(const Camera& cam, const Vec3& look, Ray& ray) const override;
    void rhs(const Ray& ray, const double* y, double* dy) const override;
    int ended(const Ray& ray, const double* y, const double* dy, double zetaSign) const override;
    Vec3 farDirection(const Ray& ray, const double* y, const double* dy, double zetaSign) const override;
    double nullCondition(const Ray& ray, const double* y) const override;
    double rFar() const override { return rFar_; }
    double hamiltonian(double b, double q, const double* y) const;   // H of (A.15), for the finite-difference test of rhs
    double carterQ(double b, const double* y) const;                 // q from (theta, p_theta) by (A.12)
    double spin() const { return a_; }
private:
    double a_, rFar_;
};

struct TraceOptions {
    double rtol = 1e-11, atol = 1e-12;   // per step, on the Dormand-Prince error estimate
    int maxSteps = 200000;
};

// The ray seen in direction `look` by the camera, followed into its past until it ends. If path is given, every accepted state is appended (n numbers each).
RayEnd trace(const Chart& chart, const Camera& cam, const Vec3& look, const TraceOptions& opt = {}, std::vector<double>* path = nullptr);
// Integrate the state over a fixed span of zeta (either sign) with the same steps; for the time-reversal test. Returns the steps taken.
int integrateSpan(const Chart& chart, Ray& ray, double span, const TraceOptions& opt = {});

}  // namespace blackhole
