// The tables each lab hands to the Data panel (0.51): the app's own data (every value with the source it already has in the labs) laid out as tables, each with
// worked analyses a physics course does with them (Kepler's third law, the Rydberg constant, quantum defects, a frequency scale factor, a Birge-Sponer plot).
// No graphics and no AppState: the panel passes what is selected. tests/test_analysis.cpp checks the analyses against independent values.
#pragma once

#include <string>
#include <vector>

#include "analysis/table.h"
#include "orbit/vec3.h"

namespace analysis {

struct LabContext {
    int lab = 0;                   // 0 Orbit, 1 Atom, 2 Molecule (AppState::lab)
    int Z = 1, N = 0;              // the Atom Lab's nucleus
    std::string molecule = "H2O";  // the Molecule Lab's molecule id
    double T = 298.15;             // 0.53: the Molecule Lab's temperature (K): the rotational shares and line strengths
    int rotIso = 4;                // 0.53: HCl's isotope (molecule::rot::Iso; 4 the natural mix)
    int spinMix = 0;               // 0.97.0: ortho and para, 0 kept as when warm, 1 in balance (molecule::rot::SpinMix)
    double fieldT = 0;             // 0.54.0: the Atom Lab's magnetic field (T; 0: off, no Zeeman tables)
    int zKind = -1, zLine = -1;    //         which levels it acts on (AtomLab::zeemanKind) and the line shown split (index into zeeman::labLines)
};
struct TableRef { std::string id, title; };
std::vector<TableRef> labTables(const LabContext& c);     // the tables on offer for this lab and selection (titles name the selection: "NIST lines of He II")
bool makeTable(const std::string& id, const LabContext& c, Table& out);   // false: no such table for this selection

// A recording of the selected body as the Orbit Lab runs (the panel collects the samples). A table like the others.
// 0.51.1: each sample also has the osculating two-body quantities about the Sun (GM = GM☉ of IAU 2009): the specific energy, angular momentum, a, e and the longitude of
// perihelion ϖ = Ω + ω (ecliptic and equinox J2000, the app's frame), so energy conservation and the turning of the perihelion can be measured.
struct RecSample {
    double tDays = 0;      // since the recording started
    double rAu = 0;        // distance from the Sun
    double vKmS = 0;       // speed relative to the Sun
    double energy = 0;     // v²/2 − GM☉/r, km²/s²
    double h = 0;          // |r × v|, km²/s
    double aAu = 0;        // −GM☉ / (2 energy) (NaN when not bound)
    double e = 0;          // |e vector|
    double varpiDeg = 0;   // Ω + ω, degrees in 0..360 (orbitRecording unwraps it)
};
RecSample recSample(double tDays, const orbit::Vec3& posM, const orbit::Vec3& velMS);   // position and velocity relative to the Sun, m and m/s
Table orbitRecording(const std::string& body, const std::vector<RecSample>& s, bool real, bool relativity);
// The first-order general-relativity turning of a perihelion, arcseconds per Julian century: 6π GM☉ / (c² a (1 − e²)) per orbit (Einstein 1915), times the orbits a century holds
double grPrecessionArcsecPerCentury(double aAu, double e);

// The ionisation limits of the alkali atoms, cm^-1 (NIST ASD, the "Limit" row of the saved level files data/jumps/asd/<X>I_levels.tsv.gz); NaN for other Z
double alkaliLimitCm(int Z, double* unc = nullptr);
// The Rydberg constant for a nucleus or ion core of mass M (in electron masses): R_M = R_inf / (1 + m_e / M), cm^-1 (R_inf: CODATA 2022)
double rydbergCm(double massMe);
constexpr double kRinfCm = 109737.31568157;               // R_inf, cm^-1: CODATA 2022 10 973 731.568 157(12) m^-1 (the calculator's Rinf, the same source)

}  // namespace analysis
