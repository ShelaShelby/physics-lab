// Black Hole Lab (0.55.1): the thin gas disc (plan section 2, step 2). Physics only, no graphics, double precision. Units: G = c = 1, lengths in M.
// Every formula below was read from the paper's page (the scans in the private sources repo, data/blackhole/scans) or its PDF text (data/blackhole/arxiv).
//
// THE GAS. The standard thin disc of Novikov & Thorne as written by D. N. Page, K. S. Thorne, ApJ 191, 499 (1974) ("PT"): gas on circular orbits in the
// equatorial plane, all heat radiated from the two faces where it is made, no torque at the innermost stable orbit, nothing emitted inside it (PT section II).
//   Flux out of EACH face, measured by the gas:  F(r) = (Mdot / 4 pi) e^-(nu + psi + mu) f,  e^(nu + psi + mu) = r                     PT (11b), (15d)
//   f = (3 / 2M) 1 / (x^2 (x^3 - 3x + 2a*)) [ x - x0 - (3/2) a* ln(x/x0) - 3 (x1 - a*)^2 / (x1 (x1 - x2)(x1 - x3)) ln((x - x1)/(x0 - x1))
//         - 3 (x2 - a*)^2 / (x2 (x2 - x1)(x2 - x3)) ln((x - x2)/(x0 - x2)) - 3 (x3 - a*)^2 / (x3 (x3 - x1)(x3 - x2)) ln((x - x3)/(x0 - x3)) ]   PT (15n)
//   x = (r/M)^(1/2), x0 = (r_ms/M)^(1/2) (PT (15k), kerr.h iscoPageThorne), x1, x2, x3 the roots of x^3 - 3x + 2a* = 0:
//   x1 = 2 cos(acos(a*)/3 - pi/3), x2 = 2 cos(acos(a*)/3 + pi/3), x3 = -2 cos(acos(a*)/3) (PT p. 502). a* = a/M > 0: the gas orbits with the hole's spin.
//   At a* = 0 the roots are sqrt(3), 0, -sqrt(3): x2 = 0 and its term is the limit (x2 - a*)^2 / x2 -> 0 (x2 ~ 2a*/3): the code drops a term whose root is 0.
//   J.-P. Luminet, A&A 75, 228 (1979) ("L79") prints the a = 0 case as (15): F = (3 M Mdot / 8 pi) / ((r* - 3) r*^(5/2)) [ r*^(1/2) - 6^(1/2)
//   + (3^(1/2)/3) Log{ ((r*^(1/2) + 3^(1/2))(6^(1/2) - 3^(1/2))) / ((r*^(1/2) - 3^(1/2))(6^(1/2) + 3^(1/2))) } ]  and states (p. 233) "The flux is maximum at
//   r* ~ 9.55, for which (8 pi / 3 M Mdot) F_max ~ 1.146 10^-4". The printed factor 3^(1/2)/3 is a misprint: PT (15n) at a* = 0 gives 3^(1/2)/2, and only
//   3^(1/2)/2 reproduces L79's own maximum (tests/test_blackhole.cpp testDiscFlux). luminetFlux takes the factor as an argument so the test can show both.
//   The total light (energy at infinity, both faces): PT (30), (31a) [Mdot E - 2 pi r W Omega],r = 4 pi r F E with the torque W = 0 at r_ms, so
//   Int_{r_ms}^{inf} 4 pi r F E dr = Mdot (1 - E(r_ms)): the efficiency 1 - E(r_ms) (a conservation law; tested by quadrature).
//   The orbits: E/mu = (r^(3/2) - 2M r^(1/2) +- a M^(1/2)) / (r^(3/4) (r^(3/2) - 3M r^(1/2) +- 2a M^(1/2))^(1/2)),
//   L/mu = +-M^(1/2) (r^2 -+ 2a M^(1/2) r^(1/2) + a^2) / (r^(3/4) (...)^(1/2)), Omega = +-M^(1/2) / (r^(3/2) +- a M^(1/2))
//   (J. M. Bardeen, W. H. Press, S. A. Teukolsky, ApJ 178, 347 (1972) ("BPT") (2.12), (2.13), (2.16); upper sign: with the spin). At r_ms BPT (2.20) holds with
//   equality: 1 - E^2 = (2/3)(M/r_ms) (a check).
//
// IN NUMBERS. M. A. Abramowicz, P. C. Fragile, Living Rev. Relativ. 16, 1 (2013), arXiv:1104.5499 ("AF") section 5.3: F = [7 x 10^26 erg cm^-2 s^-1] (m^-1 mdot)
//   r*^-3 B^-1 C^-1/2 Q with m = M/M_sun, mdot = Mdot c^2 / L_Edd, and its footnote 3: L_Edd = 1.2 x 10^38 M/M_sun erg/s. F. Yuan, R. Narayan, Annu. Rev. Astron.
//   Astrophys. 52, 529 (2014), arXiv:1401.0586 ("YN") footnote 1: L_Edd = 4 pi G M c / kappa_es, kappa_es "usually taken to be 0.4 cm^2 g^-1"; Mdot_Edd = 10 L_Edd / c^2
//   = 1.39 x 10^18 (M/M_sun) g s^-1. The app computes L_Edd by YN's formula with the Sun's GM (orbit/units.h, JPL DE440) and checks both reviews' numbers.
//   Restoring G and c, PT's F in W/m^2 is Mdot c^2 / r_g^2 times the M = 1, Mdot = 1 value (r_g = GM/c^2).
//   Temperature: each face a black body, sigma T^4 = F (plan section 2; YN section 1: "the gas is optically thick and radiates thermal blackbody-like radiation";
//   T in "the range 10^4 - 10^7 K" depending on the mass). sigma = 2 pi^5 k^4 / (15 h^3 c^2) from the exact SI h, k, c (CODATA 2018 and 2022: exact).
//   Where the model applies, as a share of L_Edd: YN section 4.2.1: the thin-disc (soft, thermal) state "is found at luminosities down to ~1.5% L_Edd"; AF section 6:
//   "At some luminosity (L ~ 0.3 L_Edd), however, ... advection", beyond which "the standard, thin disk approach can no longer be applied". So 0.015 to 0.3.
//
// WHERE A RAY MEETS THE DISC (the lab's hovering camera at rO, plan step 2 item 3). The ray seen at angle psi from the outward radial direction moves in the plane of
//   e_r and its look direction; followed back from the camera it sweeps the angle phi from e_r towards t (schwarzschild.h). Its u = 1/r obeys Binet's form of
//   L79 (3), (du/dphi)^2 = 2M u^3 - u^2 + 1/b^2, i.e. d^2u/dphi^2 = 3M u^2 - u, with u(0) = 1/rO and du/dphi(0) = -u0 (1 - 2M u0)^(1/2) cot(psi)
//   (from b = rO sin psi / (1 - 2M/rO)^(1/2), schwarzschild.h cameraImpact). It ends at u = 0 (it came from the far sky; the swept angle is then the closed form
//   bendFromCamera's dphi, tested) or at u = 1/(2M) (the horizon). Integrated here in double precision by an embedded Runge-Kutta pair (Dormand-Prince 5(4)) with
//   a relative error per step of 1e-13: the reference. Its crossings of the disc's plane are where the unit position cos(phi) e_r + sin(phi) t is
//   perpendicular to the disc's axis N: phi_k = phi_0 + k pi, phi_0 in (0, pi]. The disc is opaque: the first crossing with r_in <= r <= r_out is what is seen.
//   Checked against L79's isoradials (direct and secondary image, (5), (10), (12), (13)) for a far camera and against the ray integrator (geodesic.h) close in.
// THE SHIFT. A photon of axial angular momentum L_z (per unit energy at infinity) from gas on a circular orbit arrives with g = 1 / (1 + z) = 1 / (u^t (1 - Omega L_z))
//   (L79 (16): E_em = p_t u^t (1 + Omega p_phi / p_t); at a = 0 u^t = (1 - 3M/r)^(-1/2), Omega = (M/r^3)^(1/2), so (19): 1 + z = (1 - 3M/r)^(-1/2) (1 + (M/r^3)^(1/2)
//   b sin theta0 sin alpha) for a far observer). This g is against a clock at rest far away; the hovering camera's own factor (kerr.h hoverShift) multiplies it and,
//   as for the sky, is a readout only (it comes with the moving cameras).
// WHAT THE EYE GETS. A black body at T seen with the shift g is a black body at g T (A. Riazuelo, IJMPD 28, 1950042 (2019) (53): T_obs = T / (1 + z); I_nu / nu^3 is
//   the same along a ray). Its colour and visible brightness by the rule of tools/star_colour.py: Planck's law at 1 nm steps from 360 to 830 nm under the CIE 1931
//   2-degree observer (render/cie1931_data.inc), XYZ to linear sRGB (IEC 61966-2-1). The visible brightness is the luminance Y (the ybar-weighted sum), compared with
//   the Sun's surface by the same rule at the Sun's effective temperature (orbit/sun_facts.h kSunTeffK).
#pragma once

#include <array>
#include <cmath>
#include <vector>

namespace blackhole {

// ---- the gas (aSigned: the spin a/M, negative when the gas orbits against it; this step draws 0) ----
double orbitEnergy(double r, double aSigned);       // BPT (2.12), E/mu
double orbitAngMom(double r, double aSigned);       // BPT (2.13), L/mu
double orbitOmega(double r, double aSigned);        // BPT (2.16)
double discInner(double aSigned);                   // r_ms (PT (15k) = BPT (2.21))
double discEfficiency(double aSigned);              // 1 - E(r_ms)
double ptF(double r, double aSigned);               // PT's f (15n), M = 1 (0 at and inside r_ms)
double discFlux(double r, double aSigned);          // F per face for Mdot = 1, M = 1: f / (4 pi r) (PT (11b), (15d))
double luminetFlux(double r, double logFactor);     // L79 (15) for Mdot = 1, M = 1, with the factor before Log given (sqrt(3)/2 correct, sqrt(3)/3 as printed)
double discLuminosity(double aSigned, double rOut); // Int_{r_ms}^{rOut} 4 pi r F E dr by quadrature (rOut = infinity allowed), Mdot = 1
double fluxMaxRadius(double aSigned);               // where F is largest (golden-section search to 1e-10)

// ---- in numbers (SI) ----
constexpr double kKappaEs = 0.04;                   // m^2/kg: YN footnote 1, 0.4 cm^2/g
constexpr double kThinMin = 0.015, kThinMax = 0.3;  // L / L_Edd where the thin-disc model applies (YN 4.2.1, AF 6)
double stefanBoltzmann();                           // W m^-2 K^-4
double eddingtonLuminosity(double massSun);         // W: 4 pi G M c / kappa_es (YN footnote 1)
double eddingtonRate(double massSun);               // kg/s: YN's Mdot_Edd = 10 L_Edd / c^2 (only to check YN's printed number)
struct DiscPhys {                                   // a disc in numbers
    double massSun = 1e8, lEdd = 0.1, aSigned = 0;  // mass, L / L_Edd, spin
    double mdot() const;                            // kg/s: L = (1 - E(r_ms)) Mdot c^2 = lEdd L_Edd
    double fluxScale() const;                       // W/m^2 per unit of discFlux: Mdot c^2 / r_g^2
    double flux(double r) const;                    // W/m^2, per face
    double temperature(double r) const;             // K: (F / sigma)^(1/4)
    double hottest(double* where = nullptr) const;  // the largest temperature and its radius (M)
};
double afFluxCoefficient();                         // AF's [7 x 10^26 erg cm^-2 s^-1] computed: 3 L_Edd(1 M_sun) c^4 / (8 pi (G M_sun)^2), in erg cm^-2 s^-1

// ---- the light ----
constexpr double kTmin = 300, kTmax = 1e9;          // K: the colour table's range (below: no visible light to speak of)
std::array<double, 3> blackBodyXYZ(double T);       // W m^-2 sr^-1 under xbar, ybar, zbar (360-830 nm, 1 nm): tools/star_colour.py's sum
std::array<double, 3> blackBodyLinear(double T);    // linear sRGB of the same, in units of the Sun's surface luminance Y (so Y(Sun) = 1)
double visibleVsSun(double T);                      // Y(T) / Y(Sun's surface)
double sunTeff();                                   // orbit/sun_facts.h kSunTeffK

// ---- rays and the disc ----
// The ray of a hovering camera at rO seen at psi (from the outward radial direction): where it ends, and u = 1/r along it. Reference: Dormand-Prince on Binet's equation.
struct RayEnd2 { double phiEnd = 0; bool captured = false; };
RayEnd2 rayEndAngle(double rO, double psi);
// u at the increasing swept angles phis[0..n) (each below the end): one pass
void rayU(double rO, double psi, const double* phis, int n, double* u);
double rayRadius(double rO, double psi, double phi);
// the first crossing angle of the disc's plane: A = e_r . N, B = t . N; in (0, pi] (A = 0: pi, the camera is in the plane)
double firstCrossing(double A, double B);
// the photon's g from gas at r for a ray of axial angular momentum Lz (a = 0)
double discShift(double r, double Lz);

struct DiscGeom { double rIn = 6, rOut = 20; };
constexpr int kDiscOrders = 4;                      // crossings looked at: the direct image, the underside's, and two thin rings (the cut-off; see DiscTable)
struct DiscHit {
    int order = -1;                                  // -1: no crossing on the disc (then ended tells where the ray went)
    double r = 0, g = 0, Lz = 0, b = 0;
    bool captured = false;                           // where the ray ends if it misses the disc
};
// the reference for one ray: camera at rO, look at psi, A = e_r . N, B = t . N, Lz from the caller's geometry (b (t x e_r) . N)
DiscHit discRay(double rO, double psi, double A, double B, double LzOverB, const DiscGeom& d);

// The table the graphics card looks up (plan section 6, way (c)): u = 1/r along every ray, against the pixel's angle from the shadow's edge and the swept angle.
// Rows: two halves, side 0 outside the shadow (x = th - alpha_sh > 0, th the angle from the hole's direction), side 1 inside (x < 0); row i of a side at
// s = ln|x| + |x| / kDiscXc = s0 + i ds, from |x| = 1e-9 to the side's largest |x| (pi - alpha_sh outside, alpha_sh inside): even in ln|x| close to the edge,
// where the rays wind, and even in |x| far from it. Columns j = 0..nT-1 at phi = (j / (nT-1)) Phi(row), Phi the swept angle where the ray ends.
// A 4-point cubic (LensTable's) in s and in the column, rows and columns clamped at the ends.
// The cut-off: kDiscOrders crossings (phi_0 + k pi, k < 4) are looked at; a ray with a 5th lies within about e^-(4 pi - 2 pi) of the shadow's edge in angle
// (each half turn about e^-pi, Gralla & Lupsasca 2020b (72)): the tests measure the 4th image's width in pixels.
constexpr double kDiscXc = 0.1;
class DiscTable {
public:
    explicit DiscTable(double rO, int nS = 768, int nT = 256);
    double rO() const { return rO_; }
    double alphaSh() const { return alpha_; }
    int rows() const { return nS_; }
    int cols() const { return nT_; }
    static double sOf(double ax) { return std::log(ax) + ax / kDiscXc; }   // |x| -> s
    double s0(int side) const { return s0_[side]; }
    double ds(int side) const { return ds_[side]; }
    // values for the card: [side][row][col] u, then Phi per row (side 0 rows then side 1 rows)
    const std::vector<float>& uValues() const { return uf_; }
    const std::vector<float>& phiValues() const { return pf_; }
    // the same lookup the card does, in double; x = th - alpha_sh (radians)
    double phiEnd(double x) const;
    bool lookup(double x, double phi, double& u) const;   // false: phi beyond the row's Phi
    double fillMs() const { return ms_; }
private:
    double rO_, alpha_;
    int nS_, nT_;
    double s0_[2], ds_[2];
    std::vector<double> u_, p_;
    std::vector<float> uf_, pf_;
    double ms_ = 0;
    double rowValue(const std::vector<double>& v, int side, double s, int col, int ncol) const;
};

// A hit seen through the table (as the card sees it, in double)
DiscHit discRayTable(const DiscTable& t, double x, double A, double B, double LzOverB, const DiscGeom& d);

}  // namespace blackhole
