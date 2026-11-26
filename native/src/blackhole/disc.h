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
//   b sin theta0 sin alpha) for a far observer). This g is against a clock at rest far away; the held camera's own factor (kerr.h hoverShift; lab.h heldCameraShift)
//   multiplies it. 0.84.0: drawn (until 0.83.0 a readout only): the colour temperature seen is g f T.
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
    // 0.79.0: the true thickness (below): h (M) at r, radiation pressure only (hRad) and with the gas's pressure (height); 0 at and inside r_ms
    double heightRad(double r) const;
    double height(double r, double* gasShare = nullptr) const;   // gasShare: P_gas / P_tot there
};
// 0.79.0, THE DISC'S TRUE THICKNESS (PLAN_DISC.md section 2). Y. Zhu, S. W. Davis, R. Narayan, A. K. Kulkarni, R. F. Penna, J. E. McClintock, MNRAS 424, 2504
//   (2012), arXiv:1202.1530 ("Zhu") App. B, the one-zone vertical structure: (B1a) P_tot / h = rho Q h, (B1b) P_rad = a T^4 / 3 = F kappa Sigma / (4 c),
//   (B1c) W = 2 h alpha P_tot, (B1d) F = (3/4) Omega_k R_F W, R_F = (1 - 2/r + a^2/r^2) / (1 - 3/r + 2a/r^(3/2)), (B1e) P_tot = rho k T / mu + a T^4 / 3,
//   (B1f) Sigma = 2 h rho. Q = Omega_k^2 R_z (their (5), (7)), Omega_k = (GM/r^3)^(1/2), R_z = (u_phi^2 + a^2 (u_t - 1)) / r (their (6)).
//   Solved here (the algebra in disc.cpp): with A = P_tot h = 2 F / (3 alpha Omega_k R_F) and h_rad = kappa F / (2 c Q):  h^2 - h_rad h - k T / (mu Q) = 0,
//   T^4 = 3 F kappa A / (2 a c Q h^2). Radiation pressure alone: h = h_rad. Far out (F -> 3 GM Mdot / (8 pi R^3), Q -> GM / R^3) h_rad -> (3/4) (l / eta)
//   (kappa / kappa_es) GM/c^2, with L = l L_Edd = eta Mdot c^2 and L_Edd by kKappaEs (tested).
//   Zhu's App. B numbers: kappa = kappa_es = 0.3383 cm^2/g and mu = 0.615 m_H ("a fluid composition of 70 per cent Hydrogen, 30 per cent Helium by mass").
//   The feeding rate stays L / L_Edd with the lab's L_Edd (kKappaEs = 0.4 cm^2/g, YN): the gas's own opacity sets the thickness.
//   alpha = 0.1: a CHOICE (Zhu's Model B; their models span 0.03 to 0.22, Table 1). It enters only where the gas's pressure counts (far out): there h goes as
//   alpha^(-1/10) (the gas-pressure limit below), 6 % between 0.1 and 0.055.
//   SPIN (0.86.0, PLAN_REVIEW.md R12): Zhu's (6) prints R_z's spin term as a^2 (u_t - 1), Abramowicz, Lanza & Percival 1997 (27) as L*^2 = L^2 - a^2 (E - 1),
//   Abramowicz & Fragile 2013 (92) as L^2 - a^2 (E^2 - 1), E = -u_t (extracts: data/blackhole/arxiv/astro-ph_9611101_abramowicz_lanza_percival1997.extract.txt,
//   1104.5499_abramowicz_fragile2013_vertical.extract.txt). Only AF13 (92)'s form gives AF13 (40)'s vertical epicyclic frequency, so it is used:
//   R_z = (L^2 - a^2 (E^2 - 1)) / r, Q = (L^2 - a^2 (E^2 - 1)) / r^4 (M = 1), E and L of the circular orbit (BPT (2.12), (2.13), signed a).
//   The test (tests/test_blackhole.cpp testDiscHeightSpin): Q = (u^t)^2 omega_theta^2, omega_theta^2 = Omega_K^2 (1 - 4 a r^-3/2 + 3 a^2 r^-2) (AF13 (40),
//   Omega_K = 1 / (r^3/2 + a), BPT (2.16)), u^t = 1 / (E - Omega_K L) (u.u = -1 on a circular orbit): the restoring term per unit proper time squared is the
//   squared vertical frequency at infinity times (dt/dtau)^2. Within 1e-10 for a = 0, 0.5, 0.9, 0.99 from r_ms to 100 M. At a = 0 Q is 0.79.0's L^2 / r^4 to the bit.
//   (B1d)'s R_F takes the spin as Zhu prints it. The thickness is computed for every spin (heightOn). The picture draws it at a = 0 only (lab.h discThickDrawn):
//   the light paths round a spinning hole (spin_sky.h) store only where they cross the plane, not the path near it, so a surface above the plane cannot be
//   found there yet (ROADMAP). m_H: the hydrogen-1 atom, 1.00782503223 u (NIST, from AME2020,
//   as atom/atom_learn.cpp), u = 1.66053906892e-27 kg (CODATA 2022).
constexpr double kZhuKappa = 0.03383;               // m^2/kg (0.3383 cm^2/g), Zhu App. B
constexpr double kZhuMu = 0.615;                    // mean particle weight in m_H, Zhu App. B
constexpr double kDiscAlpha = 0.1;                  // CHOICE, labelled (Zhu Model B)
double zhuQ(double r, double aSigned = 0);         // Q for M = 1: Omega_k^2 R_z = (L^2 - a^2 (E^2 - 1)) / r^4 (AF13 (92)); a = 0: L^2 / r^4, L = BPT (2.13) = r / (r - 3)^(1/2)
double zhuRF(double r, double aSigned = 0);         // (B1d)'s R_F = (1 - 2/r + a^2/r^2) / (1 - 3/r + 2a/r^(3/2))
bool heightOn(double aSigned);                      // the thickness is modelled: every spin the lab reaches (|a| < 1); 0.79.0 to 0.85: a = 0 only
// the 1-D table the card reads (h in M, kHeightN values evenly in ln r from ln r_ms to ln(1.05 x 2000), linear between them): one table per disc (feeding
// rate and mass); the processor's reference reads the same floats
constexpr int kHeightN = 1024;
struct HeightTable { double ln0 = 0, dln = 1; std::vector<float> h; double at(double r) const; bool on() const { return !h.empty(); } };
HeightTable heightTable(const DiscPhys& p);
double afFluxCoefficient();                         // AF's [7 x 10^26 erg cm^-2 s^-1] computed: 3 L_Edd(1 M_sun) c^4 / (8 pi (G M_sun)^2), in erg cm^-2 s^-1

// ---- the light ----
constexpr double kTmin = 300, kTmax = 1e9;          // K: the colour table's range (below: no visible light to speak of)
std::array<double, 3> blackBodyXYZ(double T);       // W m^-2 sr^-1 under xbar, ybar, zbar (360-830 nm, 1 nm): tools/star_colour.py's sum
std::array<double, 3> blackBodyLinear(double T);    // linear sRGB of the same, in units of the Sun's surface luminance Y (so Y(Sun) = 1)
double visibleVsSun(double T);                      // Y(T) / Y(Sun's surface)
double sunTeff();                                   // orbit/sun_facts.h kSunTeffK
// the same two from a table made once (4096 temperatures evenly in log T from kTmin to kTmax; Y interpolated in log Y, the channels over Y linearly): for many pixels
// (lab.h discLook). Tested against the full sums.
double visibleVsSunFast(double T);
std::array<double, 3> blackBodyLinearFast(double T);
// 0.58.0, "All light" (lab.h Settings::allLight): the total light of all wavelengths. A black body at T (each face of the disc: sigma T^4 = F, PT (11b)) has
// the intensity sigma T^4 / pi in every direction; seen with the shift g it is a black body at g T (Riazuelo (53)), so the observed total intensity is
// g^4 sigma T^4 / pi = g^4 F / pi (L79 p. 234: "the observed bolometric flux F0 = Fs/(1+z)^4"; C75 (A9) is the g of a spinning hole, the same law: I_nu / nu^3 is
// the same along a ray). bolometricVsSun: that intensity against the Sun's surface's own, sigma T_sun^4 / pi: (T / T_sun)^4.
// allLightLinear: the eye's colour of the black body at T (blackBodyLinearFast) scaled so that its luminance on the screen (the IEC 61966-2-1 row
// 0.2126 R + 0.7152 G + 0.0722 B of linear sRGB) is bolometricVsSun(T): brightness of all light, colour as the eye sees it. Black below kTmin (no colour).
constexpr double kLumR = 0.2126, kLumG = 0.7152, kLumB = 0.0722;   // IEC 61966-2-1: Y of linear sRGB
double bolometricVsSun(double T);
std::array<double, 3> allLightLinear(double T);

// ---- rays and the disc ----
// The ray of a hovering camera at rO seen at psi (from the outward radial direction): where it ends, and u = 1/r along it. Reference: Dormand-Prince on Binet's equation.
struct RayEnd2 { double phiEnd = 0; bool captured = false; };
RayEnd2 rayEndAngle(double rO, double psi);
// u at the increasing swept angles phis[0..n) (each below the end): one pass
void rayU(double rO, double psi, const double* phis, int n, double* u);
double rayRadius(double rO, double psi, double phi);
double rayTime(double rO, double psi, double phi);   // 0.72.0: the far time from the point at swept angle phi to the camera (M)
// the first crossing angle of the disc's plane: A = e_r . N, B = t . N; in (0, pi] (A = 0: pi, the camera is in the plane)
double firstCrossing(double A, double B);
// the photon's g from gas at r for a ray of axial angular momentum Lz (a = 0)
double discShift(double r, double Lz);

struct DiscGeom { double rIn = 6, rOut = 20; const HeightTable* height = nullptr; };   // 0.79.0: height: the surface at |z| = h (null: the plane)
constexpr int kDiscOrders = 4;                      // crossings looked at: the direct image, the underside's, and two thin rings (the cut-off; see DiscTable)
struct DiscHit {
    int order = -1;                                  // -1: no crossing on the disc (then ended tells where the ray went)
    double r = 0, g = 0, Lz = 0, b = 0;
    double psi = 0, z = 0;                           // 0.79.0: the swept angle where the ray meets the disc (the crossing's, or the surface's), its height there
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
    // 0.72.0 (the moving gas): the far time the light takes from each entry's point to the camera (M), as uValues (the last column carried on straight)
    const std::vector<float>& tValues() const { return tf_; }
    bool lookupT(double x, double phi, double& t) const;   // the same cubic as lookup
    // the same lookup the card does, in double; x = th - alpha_sh (radians)
    double phiEnd(double x) const;
    bool lookup(double x, double phi, double& u) const;   // false: phi beyond the row's Phi
    bool lookupD(double x, double phi, double& u, double& du) const;   // 0.79.0: u and du/dphi of the same cubic (the card's surface step)
    double fillMs() const { return ms_; }
private:
    double rO_, alpha_;
    int nS_, nT_;
    double s0_[2], ds_[2];
    std::vector<double> u_, p_, t_;
    std::vector<float> uf_, pf_, tf_;
    double ms_ = 0;
    double rowValue(const std::vector<double>& v, int side, double s, int col, int ncol) const;
};

// A hit seen through the table (as the card sees it, in double). 0.79.0: with d.height, the disc's surface at |z| = h(R) (surfaceStep)
DiscHit discRayTable(const DiscTable& t, double x, double A, double B, double LzOverB, const DiscGeom& d);
// 0.79.0, THE SURFACE (spin 0; the card does the same in single precision, render/shaders.h kLensMainPart). The ray moves in a plane; at swept angle psi its
//   point is r(psi) (cos psi e_r + sin psi t), its height above the disc's plane z = r (A cos psi + B sin psi) and its distance from the axis R = (r^2 - z^2)^(1/2).
//   At the crossing psi_k, A cos psi + B sin psi = 0, so before it (the ray followed back from the camera reaches the surface first) |z| = r C sin(delta),
//   delta = psi_k - psi, C = (A^2 + B^2)^(1/2). The surface |z| = h(R): delta = asin(h(R) / (r C)), solved by kSurfaceSteps fixed-point steps with
//   r(psi) taken on the line r_k + (dr/dpsi)_k (psi - psi_k) (the table's own slope at the crossing), started from h at the crossing's radius kept on the
//   disc (surfaceStep returns that delta; 0: no surface there); then kSurfaceExact more steps with r(psi) looked up in the table (a ray grazing the thick far
//   disc meets it far from the crossing, where r(psi) is no line); then u (and the travel time) looked up at psi_k - delta. delta is kept below kSurfaceMax.
//   Tested against the full path (disc.h rayU, double; tests/test_blackhole.cpp testDiscSurface) in pixels.
constexpr int kSurfaceSteps = 4, kSurfaceExact = 2, kSurfaceOrders = 2;   // (orders: the direct and the underside's images; the thin rings beyond at the plane)
constexpr double kSurfaceMax = 0.5;
double surfaceStep(double rk, double drk, double C, const HeightTable& h, const DiscGeom& d);

}  // namespace blackhole
