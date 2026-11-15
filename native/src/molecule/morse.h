// Molecule Lab (0.53.1): bonds that stretch unevenly and break, for the four diatomics H2, N2, O2, HCl. No graphics in here (molecule_view.h moves the atoms,
// ui/shell_molecule.cpp has the controls and the Bond energy card), so tests/test_rotation.cpp checks it without a window.
//
// MEASURED and MODEL, kept apart:
//   - Measured: the ladder G(v) of vib::anharmonic (Huber & Herzberg 1979 via the NIST WebBook), shown as measured only up to the highest level its constants
//     were fitted to (validTo: H2 3, HCl 5). N2's and O2's pages do not say how far their constants hold; each page quotes the measured first spacing Delta G(1/2)
//     (N2 footnote 109, Bendtsen 1974; O2 footnote 69, Babcock & Herzberg 1948), so for them only v = 0 and 1 are shown as measured.
//     The bond length re: the X row of the same pages. The dissociation energy D0 (from the lowest level): H2 Huber & Herzberg's footnote 125 on the saved page
//     (36118.3 cm^-1, "an upper limit ... the true value is probably close to the upper limit", Herzberg 1970); N2, O2, HCl: Darwent, NSRDS-NBS 31 (1970), the
//     recommended values, saved in data/molecules/vib/dissociation/. HCl's band origins nu0(v-0), v = 1..5: Rank, Eastman, Rao & Wiggins, JOSA 52, 1 (1962), saved.
//   - Model: the Morse curve V(r) = De (1 - exp(-a (r - re)))^2 fixed by the measured we and wexe alone: De = we^2 / (4 wexe), a = 2 pi c we sqrt(mu / (2 De))
//     (the curve's harmonic frequency is then we). Its levels are exactly we (v + 1/2) - wexe (v + 1/2)^2 and end at v_max = floor(we / (2 wexe) - 1/2).
//     The levels above the measured ones are the Morse ones, labelled model.
//   - The motion: the classical motion of the reduced mass in the Morse curve at the level's energy E (closed form, eps = E / De, s = sqrt(eps),
//     omega0 = 2 pi c we):  bound (E < De):   r(t) = re + ln((1 - s cos(omega t)) / (1 - eps)) / a,   omega = omega0 sqrt(1 - eps);
//                          unbound (E > De): r(t) = re + ln((s cosh(Omega t) - 1) / (eps - 1)) / a, Omega = omega0 sqrt(eps - 1)   (t = 0: the inner turning point).
//     Turning points re - ln(1 + s) / a and re - ln(1 - s) / a; the time-averaged length re + (ln((1 + sqrt(1 - eps)) / 2) - ln(1 - eps)) / a (the mean of
//     ln(1 - s cos theta) over a turn is ln((1 + sqrt(1 - s^2)) / 2)); the period 2 pi / omega, which tends to 1 / (c we) at small E.
//     A classical swing at a level's energy, not the quantum wave of that level (which is spread between and a little beyond the turning points).
//   - Morse against measured: De - G_Morse(0) against the measured D0; the Pekeris alpha_e = 6 sqrt(wexe Be^3) / we - 6 Be^2 / we against the measured alpha_e
//     (O2: the page gives B0, so Be = B0 + alpha_e / 2). Shown side by side, used for nothing else.
// Constants: CODATA 2022 (h, c, u, N_A; exact or as in atom_core.h).
#pragma once

#include <string>
#include <vector>

namespace molecule::morse {

struct Curve {
    double we = 0, wexe = 0;    // cm^-1 (the measured constants the curve is fixed by)
    double De = 0;              // cm^-1, the well's depth: we^2 / (4 wexe)
    double a = 0;               // 1/angstrom
    double re = 0;              // angstrom
    double muU = 0;             // reduced mass, u
};
bool has(const std::string& id);                                   // H2, N2, O2, HCl
Curve make(double we, double wexe, double re, double muU);
Curve curve(const std::string& id);                                // from the lab's measured constants (vib::anharmonic), re of the saved page, mu of rot::rotor(id)
double reMeasured(const std::string& id);                          // angstrom, the page's re (NaN: none)

double V(const Curve& c, double rA);                               // cm^-1 above the bottom
double level(const Curve& c, int v);                               // Morse: we (v + 1/2) - wexe (v + 1/2)^2
int vMax(const Curve& c);                                          // the last bound level: floor(we / (2 wexe) - 1/2)

// The levels the lab shows: measured up to measuredTop, the Morse ones above
int measuredTop(const std::string& id);                            // H2 3, HCl 5, N2 and O2 1 (see above)
bool isMeasured(const std::string& id, int v);
double levelShown(const std::string& id, int v);                   // cm^-1: vib::termG for a measured level, the Morse level above (v <= vMax)
double dissociatedE(const Curve& c);                               // the energy used one step above the last bound level: De plus the last spacing

// Measured dissociation energy D0 (from the lowest level), cm^-1; NaN: none saved
double d0Measured(const std::string& id);
const char* d0Source(const std::string& id);
double d0Morse(const Curve& c);                                    // De - level(c, 0)
constexpr double kKJmolToCm = 1000 / (6.02214076e23 * 6.62607015e-34 * 299792458.0 * 100);   // 1 kJ/mol in cm^-1 (CODATA 2022, exact): 83.5935
// The rotation link: Pekeris' alpha_e of a Morse curve, and the measured one
double alphaPekeris(double we, double wexe, double Be);
double alphaPekeris(const std::string& id);                        // with the lab's we, wexe and Be (O2: B0 + alpha_e / 2)
double alphaMeasured(const std::string& id);

// The motion at energy E (cm^-1); t in real seconds from the inner turning point
struct Motion {
    double E = 0;
    bool bound = true;
    double rIn = 0, rOut = 0;   // angstrom (unbound: rOut is infinite)
    double rMean = 0;           // the time-averaged bond length (bound only)
    double periodS = 0;         // s (bound only)
};
Motion motion(const Curve& c, double E);
double bondAt(const Curve& c, double E, double t);                 // angstrom
double speedAt(const Curve& c, double E, double t);                // dr/dt, angstrom/s

// HCl's measured band origins nu0(v-0) = G(v) - G(0), v = 1..5 (Rank et al. 1962); empty for the others
std::vector<double> bandOrigins(const std::string& id);

// Birge-Sponer: the spacings Delta G(v + 1/2) against v + 1, a straight-line fit, and the area under it from v + 1 = 1/2 to where it reaches 0 (the dissociation
// energy from the lowest level; exact for a Morse ladder). The spacings: HCl's measured band origins; the others the measured ladder's constants within its range.
struct BirgeSponer {
    std::vector<double> x, dG;  // v + 1, cm^-1
    bool measuredOrigins = false;
    bool ok = false;            // at least two spacings
    double intercept = 0, slope = 0, xZero = 0, area = 0;
};
BirgeSponer birgeSponer(const std::string& id);

}  // namespace molecule::morse
