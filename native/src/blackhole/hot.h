// Black Hole Lab (0.80.0): Sagittarius A*'s real gas, the hot, thick, see-through flow (native/PLAN_DISC.md section 3). Physics only, double precision, CPU.
// The reference a later GPU table will be checked against (0.80.0 draws nothing: LIMITS below).
//
// THE MODEL. A. E. Broderick, A. Loeb, MNRAS 367, 905 (2006), arXiv:astro-ph/0509237 ("BL06"), section 2.2, (4) and Table 1 (data/blackhole/arxiv, read in
//   the saved text): n_e = n0e (rho/M)^-1.1 exp(-z^2 / 2 rho^2), T_e = Te0 (r/M)^-0.84, n_nth = n0nth (rho/M)^pnth exp(-z^2 / 2 rho^2), rho the cylindrical
//   radius. Table 1: a = 0: 3e7 cm^-3, 1.7e11 K, 8e4 cm^-3, -2.9; a = 0.5: 3e7, 1.4e11, 5e4, -2.8; a = 0.998: 1e7, 1.5e11, 1e5, -2.8. "In all models, the
//   non-thermal electrons have a spectral index of 1.25 and a minimum Lorentz factor of 100. The magnetic field strength is set to be a fixed fraction (30%)
//   of equipartition relative to the protons ... the field is taken to be toroidal ... free fall inside of the ISCO (6M, 4.233M and 1.237M for a = 0, 0.5
//   and 0.998, respectively), and in Keplerian rotation otherwise." The fit: BL06 Fig. 1 (the spectrum, viewed from 45 degrees, against Yuan et al. 2004's
//   data). Their mass and distance: "4 x 10^6 M_sun and distance 8 kpc, appropriate for Sgr A*" (Broderick & Loeb 2005, MNRAS 363, 353, Fig. 7's caption).
// THE FIELD. Equipartition as written by A. E. Broderick, V. L. Fish, S. S. Doeleman, A. Loeb, ApJ 735, 110 (2011), arXiv:1011.2770, (4):
//   B^2 / 8 pi = beta^-1 n_e m_p c^2 r_S / (12 r), r_S = 2M. BL06's "30%" has two readings: 0.3 of that energy (beta^-1 = 0.3) or 0.3 of that field
//   (beta^-1 = 0.09). The 1.3 mm flux decides (tests/test_blackhole.cpp testHotSpectrum; REPORT 0.80.0): kReading below.
// THE LIGHT. Synchrotron. A. Pandya, Z. Zhang, M. Chandra, C. F. Gammie, ApJ 822, 34 (2016), arXiv:1602.08749 (read on the PDF's pages 10 and 12):
//   thermal (24), the fit of Leung et al. 2011: j = n_e sqrt(2) pi e^2 nu_s / (3 K_2(1/Theta_e) c) (X^1/2 + 2^(11/12) X^1/6)^2 exp(-X^1/3), X = nu / nu_s,
//   nu_s = (2/9) nu_c Theta_e^2 sin(theta), nu_c = e B / (2 pi m_e c). (The page prints K_2(1/Theta_2); the thermal distribution's normalisation is
//   K_2(1/Theta_e), as in Leung et al.: REPORT 0.80.0.) Its absorption by Kirchhoff's law (25): alpha = j / B_nu(T_e).
//   Power law (26) and its absorption (27); (27) prints 3^(p/2) and Gamma((3p+12)/12). Both coefficients are checked in the tests against a direct
//   integral of the one-electron synchrotron spectrum over the power-law electrons (Rybicki & Lightman's method), which decided the factor (REPORT 0.80.0).
// THE TRANSFER. I_nu / nu^3 is the same along a ray (BL06 (3): N_nu ~ I_nu / nu^3). With g = nu_obs / nu_gas (nu_obs at rest far away; the hovering camera's
//   own factor is a readout only, as for the disc: disc.h "THE SHIFT"), the affine length lambda with p_t = -1 and the gas's proper length lambda / g:
//   dI_obs / dlambda = g^2 j(nu_obs / g) - (alpha(nu_obs / g) / g) I_obs. Each step adds g^3 (j / alpha) (1 - exp(-dtau)) exp(-tau), tau the optical depth
//   between the step and the camera (exact for a step of constant j and alpha).
// THE RAY (a = 0): the lab's ray (disc.h "WHERE A RAY MEETS THE DISC"): it moves in the plane of e_r and t, u = 1/r obeys d^2u/dphi^2 = 3u^2 - u, its point at
//   swept angle phi is r (cos phi e_r + sin phi t), its height z = r (A cos phi + B sin phi) (A = e_r . N, B = t . N, N the spin axis), its covariant momentum
//   p_t = -1, p_phi = Lz, p_r = b (du/dphi) / (1 - 2u) (dr/dlambda = b du/dphi for the light going to the camera).
// THE GAS'S MOTION (a = 0): outside rho = 6 M, Keplerian in rho: Omega = rho^-3/2, u^t from u.u = -1 at (r, theta). Inside, the ISCO's E and L kept on an
//   equatorial plunge at the same r (BL06's free fall), its dr/dt and dphi/dt normalised at (r, theta). The field's direction: toroidal in the gas's frame,
//   b^mu ~ (g_phiphi u^phi, 0, 0, -g_tt u^t), so cos(angle) = p.b / (|b| (-p.u)).
// LIMITS: a fitted, smooth, self-similar model; no outflow or jet; no structure or flicker; one still moment; spin 0, 0.5 and 0.998 only (Table 1).
// 0.80.0 DRAWS NOTHING: the model as printed passes both measurement checks under no sourced reading (THE CHECKS below; REPORT 0.80.0, ROADMAP).
//   The GPU table, the benchmark scene blackhole.hot_flow and the guide text wait for a model that passes both.
#pragma once

namespace blackhole {
namespace hot {

// cgs (CODATA 2018: c, h, k exact; e = 1.602176634e-19 C exact gives e in esu with c; m_e, m_p measured)
constexpr double cC = 2.99792458e10, cH = 6.62607015e-27, cK = 1.380649e-16;
constexpr double cE = 4.803204712570263e-10, cMe = 9.1093837015e-28, cMp = 1.67262192369e-24;

struct Model { double spin = 0, n0 = 0, T0 = 0, n0nth = 0, pnth = 0, rIsco = 6, unit = 1; };   // unit: the length the profiles are written in (M; 2 = r_S)
bool modelFor(double spin, Model& m);                     // BL06 Table 1: spin 0, 0.5, 0.998 only (within 1e-9); unit 1 (lengths in M, as BL06 prints)

constexpr double kSpectralIndex = 1.25;                   // BL06 sect. 2.2: I_nu ~ nu^-1.25 for the power-law electrons
constexpr double kP = 2 * kSpectralIndex + 1;             // the electrons' power, p = 2 alpha + 1 = 3.5
constexpr double kGammaMin = 100;                         // BL06 sect. 2.2
constexpr double kEquipShare = 0.3;                       // BL06 sect. 2.2 "(30%)"
constexpr double kRMax = 1000;                            // CHOICE: the flow out to 1000 GM/c^2 (the camera's farthest, lab.h kDistMax)
constexpr double kBl06MassSun = 4e6, kBl06DistPc = 8000;  // Broderick & Loeb 2005 Fig. 7: the mass and distance of BL06's fits

enum class BReading { Energy, Field };                    // 0.3 of the equipartition energy (beta^-1 = 0.3), or 0.3 of its field (beta^-1 = 0.09)
constexpr BReading kReading = BReading::Energy;           // the default of the functions below: Yuan, Quataert & Narayan 2003 sect. 2.2 (BL06's base) give
                                                          // the field as "a fixed fraction beta^-1 of the thermal energy density"

// THE CHECKS (tests/test_blackhole.cpp testHotSpectrum), the gate for drawing the flow. A reading is drawn only if its far observer's flux, at BL06's mass,
// distance and view (kBl06*, 45 degrees), passes both:
//   1.3 mm: Broderick et al. 2011 sect. 2: "typical 1.3 mm flux of ~3 Jy"; sect. 3.1: "Sgr A*'s flux varied by roughly 30%": 3 Jy x (1 +- 0.3).
//   2.2 um: GRAVITY Collaboration, A&A 638, A2 (2020), Table 1 (2017-2019 average, dereddened): from the 5 % percentile 0.28 to the 95 % percentile 6.9 mJy.
// 0.80.0 (REPORT): no sourced reading passes both, so the lab draws no flow (kDrawn). Lengths in M (BL06 (4) as printed): energy 4.54 Jy, 0.155 mJy;
//   field 2.13 Jy, 0.040 mJy. Lengths in r_S = 2M (Broderick et al. 2011 (2)-(5); Yuan et al. 2003 Fig. 2's profiles): energy 19.8 Jy, 2.77 mJy;
//   field 13.4 Jy, 0.70 mJy.
constexpr double k13mmJy = 3, k13mmShare = 0.3, k22LowMJy = 0.28, k22HighMJy = 6.9;
constexpr double kNu13mm = cC / 0.13, kNu22um = cC / 2.2e-4;
constexpr bool kDrawn = false;

struct Plasma { double ne = 0, Te = 0, nnth = 0, B = 0; };
Plasma plasma(const Model& m, double r, double theta, BReading rd);   // r in M
double bEquipartition(double ne, double r);               // Broderick et al. 2011 (4) with beta^-1 = 1, Gauss

double planckNu(double nu, double T);                     // B_nu, erg s^-1 cm^-2 Hz^-1 sr^-1
double jThermal(double ne, double Te, double B, double nu, double sinB);   // (24), erg s^-1 cm^-3 Hz^-1 sr^-1
double aThermal(double ne, double Te, double B, double nu, double sinB);   // (25), cm^-1
double jPower(double nnth, double B, double nu, double sinB);              // (26)
double aPower(double nnth, double B, double nu, double sinB);              // (27) with its coefficient corrected (hot.cpp, tested)
// the power-law electrons with their cut at gamma_min (j and alpha from the one-electron spectrum, tabulated once in nu / (nu_c sin)): the reference's;
// (26), (27) are its limit far above nu_c gamma_min^2 (tested)
double jPowerCut(double nnth, double B, double nu, double sinB);
double aPowerCut(double nnth, double B, double nu, double sinB);
// (the tests' independent reference) j and alpha of the power-law electrons by direct integration of the one-electron spectrum F(x) = x Int_x^inf K_5/3
double jPowerIntegral(double nnth, double B, double nu, double sinB);
double aPowerIntegral(double nnth, double B, double nu, double sinB);

void velocity0(double r, double theta, double& ut, double& ur, double& uph);   // contravariant, a = 0
struct Shift { double g = 0, sinB = 0; };
Shift shift0(double r, double theta, double pr, double Lz);                     // the light's g and the sine of its angle to the field

// One step of the transfer (THE TRANSFER above): the gas's j, alpha at nu_obs / g, the step's affine length dlam (cm); adds to I and to tau (the optical
// depth between the step and the camera, counted from the camera)
void addLight(double g, double j, double a, double dlam, double& I, double& tau);

// One ray of the lab's picture at spin 0: camera at rO (M), psi from the outward radial direction, A, B, Lz as above.
struct RayIn { double rO = 50, psi = 3.14159265358979, A = 0, B = 0, Lz = 0; };
struct Light { double I = 0, tau = 0; };                  // I_nu at nu_obs (cgs), tau the optical depth along the whole ray
struct Parts { bool thermal = true, power = true, absorb = true, cut = true; };   // cut: the power law with its gamma_min cut (else (26), (27))
Light rayLight(const Model& m, double massSun, double nuObs, const RayIn& in, double eps = 0.01, Parts parts = {}, BReading rd = kReading);
// The flux at Earth (Jy) for a far observer at inclination incl (degrees from the spin axis), D in parsec. Impact parameters 0..bMax (M).
double farFluxJy(const Model& m, double massSun, double distPc, double nu, double inclDeg, BReading rd = kReading, Parts parts = {},
                 int nb = 240, int nchi = 32, double bMax = 40, double eps = 0.01);

// The colour of I_nu ~ nu^-alpha under the CIE 1931 2-degree observer (render/cie1931_data.inc, 360-830 nm): chromaticity x, y
void powerLawColour(double alpha, double& x, double& y);

// The geometry of a pixel's ray: tilt (degrees from the spin axis), chi the azimuth on the screen: A, B, Lz / b
void viewGeometry(double tiltDeg, double chi, double& A, double& B, double& LzOverB);

}  // namespace hot
}  // namespace blackhole
