// Molecule Lab (0.23 stage 1): the molecules' vibrations. No graphics in here (molecule_view.cpp draws the moved atoms, ui/shell_molecule.cpp has the panel), so
// tests/test_vibration.cpp can check it without a window.
//
// What is CALCULATED and what is not:
//   - The normal modes (which way each atom moves), their harmonic wavenumbers and their infrared intensities are CALCULATED: Kohn-Sham DFT, B3LYP / aug-cc-pVTZ (aug-cc-pVDZ
//     put the bends of acetylene, HCN and benzene 8-20 % low; benzene: finite-difference Hessian with its symmetry, restart-proof) (PySCF 2.14), analytic Hessian and dipole derivatives, at the app's measured (NIST CCCBDB) shape, by tools/make_vib_data.py (data/molecules/vib/README.txt).
//     Harmonic wavenumbers are known to come out a few per cent above the measured fundamentals (anharmonicity and the method), and the measured shape is not the
//     B3LYP minimum; the app says "calculated" next to every one.
//   - The MEASURED fundamentals, their symmetry, CCCBDB's description and (where listed) the measured IR intensity: NIST CCCBDB, SRD 101, the experimental pages
//     saved in data/molecules/vib/cccbdb/ (read 4 Nov 2026), paired with the calculated modes by tools/make_vib_measured.py (symmetry first, then the scaled wavenumber).
//     CCCBDB's scaling factors (the "Scale factor uncertainty" and "vsfx" pages, saved there too): B3LYP/aug-cc-pVTZ 0.9675, 1 sigma 0.019 (137 molecules, 1447
//     frequencies); B3LYP/aug-cc-pVDZ 0.9704, 1 sigma 0.026 (182 molecules, 1418 frequencies).
//   - The picture: each atom swings along its mode between the classical turning points of the lowest vibrational level, |Q| = sqrt(hbar / omega) in the
//     mass-weighted normal coordinate Q (the size of the zero-point spread), or 3 times that, labelled. A real molecule in its lowest level does not swing like a
//     ball on a spring: its atoms are smeared out (a wave), and all its modes are there at once. Shown one mode at a time, slowed down kSlowDown times, every mode
//     at its true speed relative to the others.
// Constants: CODATA 2022 (hbar, c, u) as in atom_core.h.
#pragma once

#include <string>
#include <vector>

#include "molecule/molecule.h"

namespace molecule::vib {

struct Mode {
    double nuCalc;               // harmonic wavenumber, cm^-1 (calculated)
    double irKmMol;              // harmonic infrared intensity, km/mol (calculated)
    std::vector<double> L;       // mass-weighted unit vector, 3 per atom (x, y, z, the app's frame), sum of squares 1
    double nuSwing = 0;          // the wavenumber the swing's size uses: nuCalc, or the measured fundamental where the calculation is imaginary (methanol's torsion)
};
// A measured mode (NIST CCCBDB), as listed; a degenerate one (E, T, Pi) is `count` calculated modes from `first`
struct Measured {
    const char* id; int mode; const char* sym;
    double fund, harm;           // cm^-1; harm NaN: not listed
    const char* ref;
    double ir, irUnc;            // km/mol; NaN: not listed
    const char* irRef; const char* desc;
    int first, count;
};
const Measured* measured(const std::string& id, int k);  // the measured mode calculated mode k belongs to (nullptr: none)
// 0.34: the MEASURED anharmonic ladder of the diatomics' ground state X: G(v) = we (v + 1/2) - wexe (v + 1/2)^2 + weye (v + 1/2)^3 + weze (v + 1/2)^4 (cm^-1,
// zero at the bottom of the potential, without the small constant Y00). Constants: K. P. Huber & G. Herzberg, "Constants of Diatomic Molecules" (1979), as the NIST
// Chemistry WebBook (SRD 69) gives them, pages saved in data/molecules/vib/webbook_diatomic/ (read 6 Nov 2026). The live view stays harmonic.
struct Anharmonic { const char* id; double we, wexe, weye, weze; int validTo; const char* note; };   // validTo: the highest v the constants are fitted to (0: not stated)
const Anharmonic* anharmonic(const std::string& id);      // nullptr: not a diatomic in the table
double termG(const Anharmonic& a, int v);                 // G(v), cm^-1
const char* basisOf(const std::string& id);               // "aug-cc-pVTZ"
double scaleOf(const std::string& id);                    // CCCBDB's factor for B3LYP/aug-cc-pVTZ: 0.9675
double scaleSigmaOf(const std::string& id);               // its 1-sigma uncertainty: 0.019
double nuShown(const std::string& id, int k);             // the measured fundamental, or the calculated wavenumber where none is listed
bool irMeasuredAll(const std::string& id);                // every IR-active mode of this molecule has a measured intensity (the spectrum then shows the measured ones)

const std::vector<Mode>& modes(const std::string& id);   // ascending wavenumber; empty for an unknown id
int expectedCount(const Molecule& m);                     // 3N - 6, or 3N - 5 for a linear molecule
// how many modes share mode k's wavenumber, to kDegenerateCm: 1, 2 or 3. A degenerate set comes out split by up to 0.3 cm^-1 (benzene: the numerical grid
// and differences); the closest two modes that are NOT degenerate are 2.2 cm^-1 apart (benzene, 1026.0 and 1028.3), so 1 cm^-1 tells them apart
constexpr double kDegenerateCm = 1.0;
int degeneracy(const std::string& id, int k);
int degenerateFirst(const std::string& id, int k);        // the first mode of k's degenerate set
constexpr double kIrActive = 1e-4;                        // km/mol: below this a mode counts as IR-inactive. The modes symmetry forbids come out below 1e-6 (rounding of the
                                                          // differences); allowed but weak ones above: ethylene's CH2 rock 0.0096, ozone's symmetric stretch 0.0053 (tests/test_vibration.cpp)
bool irActive(const Mode& m);

// The Cartesian displacement of every atom (angstrom) at the turning point Q = sqrt(hbar / omega), omega = 2 pi c nuSwing: d_i = L_i Q / sqrt(m_i)
std::vector<double> turningPoint(const Molecule& m, const Mode& md);
double largestMoveA(const Molecule& m, const Mode& md);   // the biggest |d_i| of turningPoint (angstrom)
// The molecule at phase s (-1..1) of the swing, times `scale` (1 = the real zero-point size): atoms = equilibrium + s * scale * d
void displaced(const Molecule& eq, const Mode& md, double s, double scale, Molecule& out);

constexpr double kExaggerate = 3;                         // the picture's "3 times" choice ...
constexpr double kMaxDrawnA = 0.3;                        // ... but no atom drawn more than 0.3 A from its place (a slow twist's real swing is already 0.4 A)
double drawnScale(const Molecule& m, const Mode& md, bool bigger);   // 1 (Real size), or kExaggerate capped so the biggest swing drawn is at most kMaxDrawnA (never below 1)
// The slow-down: a mode of 1000 cm^-1 takes kScreenSAt1000 seconds per swing on screen, so every mode keeps its true speed relative to the others
constexpr double kScreenSAt1000 = 2.0;
double periodS(double nu);                                // the real period, s: 1 / (c nu)
double screenPeriodS(double nu);                          // kScreenSAt1000 * 1000 / nu
double slowDown();                                        // kScreenSAt1000 / periodS(1000)

// 0.29 (stage 3, live): every mode at once. Each mode is a quantum harmonic oscillator at temperature T (K) and wavenumber nu (the one shown: measured where
// CCCBDB lists it): it holds n = 1 / (exp(hc nu / kT) - 1) quanta on average (Bose-Einstein), so <Q^2> = (hbar / 2 omega) (2n + 1) (= coth(hc nu / 2kT)); at 0 K the
// zero-point spread, in the classical limit kT / omega^2. The picture moves each mode as Q = A cos(omega t + phi) with A^2 / 2 = <Q^2>, so each mode has exactly the
// thermal energy hc nu (n + 1/2). Simplified, and the panel says so: a real molecule's mode sizes vary from moment to moment (a Gaussian spread, here a fixed size each)
// and its atoms are smeared out, not points; the phases are fixed, spread evenly (golden ratio), not random; no anharmonicity (bonds never break), no rotation.
double meanQuanta(double nu, double T);                   // n, as above (0 at T <= 0)
double thermalFactor(double nu, double T);                // 2n + 1 = coth(hc nu / 2kT): <Q^2> over its zero-point value
double thermalEnergyKJmol(const std::string& id, double T);   // sum over the modes of hc nu n, per mole: the vibrational energy above the lowest level
// The molecule at real time t (s) with every mode moving, times `scale` (1 = real size). The zero-point size here uses the shown wavenumber, so at 0 K a mode's
// amplitude is turningPoint's times sqrt(nuSwing / nuShown) (within a few per cent).
void liveDisplaced(const Molecule& eq, double T, double t, double scale, Molecule& out);
double liveRmsA(const Molecule& m, double T);             // the largest root-mean-square distance of an atom from its place (angstrom)
double liveDrawnScale(const Molecule& m, double T, bool bigger);   // 1, or up to kExaggerate with twice liveRmsA at most kMaxDrawnA

// What the mode mostly does, worked out from its calculated shape: how much of the atoms' relative motion along the bonds is stretching (along the bond) against
// bending (across it); the words name the bond type that stretches most and, for two bonds of one type, whether they stretch together or in turn.
struct Character { double stretchShare; std::string words, shortWords; };   // shortWords: for a list row ("O–H stretch, in turn", "bend")
Character character(const Molecule& m, const Mode& md);

}  // namespace molecule::vib
