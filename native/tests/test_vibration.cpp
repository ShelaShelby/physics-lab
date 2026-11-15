// Molecule Lab vibrations (0.23 stage 1) tests, written before the code. Independent references only:
//  - counting: a molecule of N atoms has 3N - 6 vibrations, 3N - 5 if it is linear;
//  - conservation laws: a vibration moves neither the centre of mass (momentum) nor turns the molecule (angular momentum about the centre of mass, Eckart);
//  - the selection rules of group theory (an infrared band needs a change of the dipole): which modes of H2, N2, O2, CO2, C2H2, CH4, C2H4 and C6H6 are
//    IR-inactive, and the degeneracies symmetry forces (the textbook symmetry species, e.g. Herzberg, Infrared and Raman Spectra, 1945; from knowledge, the
//    book was not opened in this session). These do not depend on the calculation: any correct calculation must give them;
//  - the diatomic's turning point sqrt(hbar / (mu omega)) from its reduced mass (CODATA 2022 hbar, c, u; worked out here);
//  - the well-known spectral regions: a stretch of a bond to hydrogen lies above 2,500 cm^-1, and no fundamental of these molecules above 4,500 cm^-1;
//  - water and CO2: the symmetric stretch below the antisymmetric one.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <map>
#include <string>
#include <vector>

#include "molecule/molecule.h"
#include "molecule/vibration.h"

using namespace molecule;

static int g_failed = 0, g_checks = 0;
static void check(bool ok, const std::string& what) {
    g_checks++;
    if (!ok) { g_failed++; std::printf("FAIL: %s\n", what.c_str()); }
}
static std::string f(double v) { char b[64]; std::snprintf(b, sizeof b, "%.6g", v); return b; }
static const double kHbar = 1.054571817e-34, kC = 299792458.0, kU = 1.66053906892e-27, kPi = 3.14159265358979323846;   // CODATA 2022

static const Molecule& byId(const std::string& id) { return get(findId(id)); }

static void testCountsAndConservation() {
    for (int i = 0; i < count(); i++) {
        const Molecule& m = get(i);
        const auto& md = vib::modes(m.id);
        const int n = static_cast<int>(m.atoms.size()), want = 3 * n - (m.linear || n == 2 ? 5 : 6);
        check(static_cast<int>(md.size()) == want, m.id + ": " + std::to_string(want) + " vibrations (has " + std::to_string(md.size()) + ")");
        check(vib::expectedCount(m) == want, m.id + ": expectedCount");
        for (std::size_t a = 0; a < md.size(); a++) {
            check(md[a].L.size() == static_cast<std::size_t>(3 * n), m.id + ": a mode has 3 numbers per atom");
            if (md[a].L.size() != static_cast<std::size_t>(3 * n)) continue;
            for (std::size_t b = a; b < md.size(); b++) {           // mass-weighted orthonormal
                double s = 0;
                for (int k = 0; k < 3 * n; k++) s += md[a].L[k] * md[b].L[k];
                check(std::fabs(s - (a == b ? 1 : 0)) < 1e-6, m.id + ": modes " + std::to_string(a + 1) + " and " + std::to_string(b + 1) + " orthonormal (" + f(s) + ")");
            }
            if (a) check(md[a].nuCalc >= md[a - 1].nuCalc, m.id + ": ascending wavenumbers");
            check(md[a].nuCalc > 100 && md[a].nuCalc < 4500, m.id + ": mode " + std::to_string(a + 1) + " between 100 and 4,500 cm^-1 (" + f(md[a].nuCalc) + ")");
            // momentum: sum m_i d_i = 0; angular momentum: sum m_i r_i x d_i = 0 (r about the centre of mass, which is the origin of the app's frame)
            const std::vector<double> d = vib::turningPoint(m, md[a]);
            double p[3] = {0, 0, 0}, J[3] = {0, 0, 0}, M = 0, big = 0;
            for (int i2 = 0; i2 < n; i2++) {
                const double mi = element(m.atoms[i2].el).massU, r[3] = {m.atoms[i2].x, m.atoms[i2].y, m.atoms[i2].z}, *q = &d[3 * i2];
                for (int k = 0; k < 3; k++) p[k] += mi * q[k];
                J[0] += mi * (r[1] * q[2] - r[2] * q[1]); J[1] += mi * (r[2] * q[0] - r[0] * q[2]); J[2] += mi * (r[0] * q[1] - r[1] * q[0]);
                M += mi; big = std::max(big, std::sqrt(q[0] * q[0] + q[1] * q[1] + q[2] * q[2]));
            }
            check(std::hypot(p[0], p[1], p[2]) / M < 1e-7, m.id + ": mode " + std::to_string(a + 1) + " keeps the centre of mass still (" + f(std::hypot(p[0], p[1], p[2]) / M) + " A)");
            check(std::hypot(J[0], J[1], J[2]) / M < 1e-6, m.id + ": mode " + std::to_string(a + 1) + " does not turn the molecule (" + f(std::hypot(J[0], J[1], J[2]) / M) + ")");
            check(big > 0 && std::isfinite(big) && std::fabs(vib::largestMoveA(m, md[a]) - big) < 1e-12, m.id + ": the zero-point swing is a real length (" + f(big) + " A)");
            const double ds = vib::drawnScale(m, md[a], true);
            check(ds >= 1 && ds <= vib::kExaggerate && (ds * big <= vib::kMaxDrawnA + 1e-12 || ds == 1), m.id + ": 3 times bigger, capped at 0.3 A (" + f(ds) + ")");
        }
    }
    check(vib::modes("nope").empty(), "an unknown molecule has no modes");
}

static int inactive(const std::string& id) { int k = 0; for (const vib::Mode& m : vib::modes(id)) if (!vib::irActive(m)) k++; return k; }
static std::vector<int> sets(const std::string& id) {              // the sizes of the degenerate sets, in order
    std::vector<int> v;
    const auto& md = vib::modes(id);
    for (int k = 0; k < static_cast<int>(md.size()); k++) if (vib::degenerateFirst(id, k) == k) v.push_back(vib::degeneracy(id, k));
    return v;
}
static std::string str(const std::vector<int>& v) { std::string s; for (int x : v) s += std::to_string(x); return s; }

static void testSelectionRules() {
    for (const char* id : {"H2", "N2", "O2"}) check(inactive(id) == 1, std::string(id) + ": a homonuclear diatomic has no IR band");
    check(inactive("HCl") == 0, "HCl: its stretch is IR-active");
    for (int i = 0; i < count(); i++) for (const vib::Mode& md : vib::modes(get(i).id))   // forbidden is forbidden: well below the cut-off, not near it
        check(md.irKmMol < 1e-6 || md.irKmMol > 5e-4, get(i).id + ": an intensity is clearly zero (forbidden) or clearly not (" + f(md.irKmMol) + " km/mol)");
    check(inactive("CO2") == 1, "CO2 (D-inf-h): only the symmetric stretch is IR-inactive (" + std::to_string(inactive("CO2")) + ")");
    check(inactive("C2H2") == 4, "C2H2 (D-inf-h): 2 sigma-g+ and the pi-g pair are IR-inactive (" + std::to_string(inactive("C2H2")) + ")");
    check(inactive("CH4") == 3, "CH4 (Td): only the two T2 sets absorb: 3 of 9 modes inactive (" + std::to_string(inactive("CH4")) + ")");
    check(inactive("C2H4") == 7, "C2H4 (D2h): only B1u, B2u, B3u absorb: 7 of 12 modes inactive (" + std::to_string(inactive("C2H4")) + ")");
    check(inactive("C6H6") == 23, "C6H6 (D6h): only A2u and the three E1u pairs absorb: 23 of 30 modes inactive (" + std::to_string(inactive("C6H6")) + ")");
    for (const char* id : {"H2O", "O3", "NH3", "HCN", "H2CO", "CH3OH"}) check(inactive(id) == 0, std::string(id) + ": every mode is IR-active (" + std::to_string(inactive(id)) + ")");
    // degeneracies forced by symmetry
    std::vector<int> co2 = sets("CO2"), ch4 = sets("CH4"), nh3 = sets("NH3"), c6 = sets("C6H6"), hcn = sets("HCN"), c2h2 = sets("C2H2");
    std::sort(co2.begin(), co2.end()); std::sort(ch4.begin(), ch4.end()); std::sort(nh3.begin(), nh3.end()); std::sort(c6.begin(), c6.end()); std::sort(hcn.begin(), hcn.end()); std::sort(c2h2.begin(), c2h2.end());
    check(str(co2) == "112", "CO2: the bend is a degenerate pair (" + str(co2) + ")");
    check(str(hcn) == "112", "HCN: the bend is a degenerate pair (" + str(hcn) + ")");
    check(str(c2h2) == "11122", "C2H2: two bending pairs (" + str(c2h2) + ")");
    check(str(ch4) == "1233", "CH4: A1, E, T2, T2 (" + str(ch4) + ")");
    check(str(nh3) == "1122", "NH3: two A1 and two E (" + str(nh3) + ")");
    check(c6.size() == 20 && std::count(c6.begin(), c6.end(), 2) == 10, "C6H6: 20 fundamentals, 10 of them degenerate pairs (" + str(c6) + ")");
    for (const char* id : {"H2O", "O3", "H2CO", "C2H4", "CH3OH"}) { const auto s = sets(id); check(std::count(s.begin(), s.end(), 1) == static_cast<long>(s.size()), std::string(id) + ": no degenerate modes"); }
}

static void testDiatomicTurningPoint() {
    for (const char* id : {"H2", "N2", "O2", "HCl"}) {
        const Molecule& m = byId(id);
        check(!vib::modes(id).empty(), std::string(id) + ": has its mode");
        if (vib::modes(id).empty()) continue;
        const vib::Mode& md = vib::modes(id)[0];
        const double m1 = element(m.atoms[0].el).massU, m2 = element(m.atoms[1].el).massU, mu = m1 * m2 / (m1 + m2) * kU, w = 2 * kPi * kC * md.nuCalc * 100;
        const double want = std::sqrt(kHbar / (mu * w)) * 1e10;    // angstrom: the change of the bond length at the turning point
        const std::vector<double> d = vib::turningPoint(m, md);
        const double ux = m.atoms[1].x - m.atoms[0].x, uy = m.atoms[1].y - m.atoms[0].y, uz = m.atoms[1].z - m.atoms[0].z, r = std::sqrt(ux * ux + uy * uy + uz * uz);
        const double dr = std::fabs(((d[3] - d[0]) * ux + (d[4] - d[1]) * uy + (d[5] - d[2]) * uz) / r);
        check(std::fabs(dr - want) < 1e-9 * want + 1e-12, std::string(id) + ": the bond stretches by sqrt(hbar/(mu omega)) = " + f(want) + " A at the turning point (" + f(dr) + ")");
    }
}

static void testDisplacedAndTime() {
    const Molecule& w = byId("H2O");
    if (vib::modes("H2O").empty()) { check(false, "H2O has modes"); return; }
    const vib::Mode& md = vib::modes("H2O")[0];
    Molecule out;
    vib::displaced(w, md, 0, 3, out);
    bool same = out.atoms.size() == w.atoms.size();
    for (std::size_t i = 0; same && i < w.atoms.size(); i++) same = out.atoms[i].x == w.atoms[i].x && out.atoms[i].y == w.atoms[i].y && out.atoms[i].z == w.atoms[i].z;
    check(same && out.bonds.size() == w.bonds.size(), "phase 0: the equilibrium shape, bonds kept");
    const std::vector<double> d = vib::turningPoint(w, md);
    vib::displaced(w, md, -1, 3, out);
    check(std::fabs(out.atoms[1].x - (w.atoms[1].x - 3 * d[3])) < 1e-12, "phase -1 at 3 times: moved by -3 d");
    check(std::fabs(vib::periodS(1000) - 1 / (kC * 1e5)) < 1e-25, "the period of 1000 cm^-1 is 1/(c x 100000 m^-1) = 33.36 fs (" + f(vib::periodS(1000)) + ")");
    check(std::fabs(vib::screenPeriodS(1000) - 2) < 1e-12 && std::fabs(vib::screenPeriodS(4000) - 0.5) < 1e-12, "on screen 1000 cm^-1 takes 2 s and 4000 cm^-1 0.5 s");
    check(std::fabs(vib::slowDown() - 2 * kC * 1e5) < 1e-3 * vib::slowDown(), "slowed down 2 s / 33.36 fs = 6.0e13 times (" + f(vib::slowDown()) + ")");
}

static void testCharacter() {
    const auto& w = vib::modes("H2O");
    const Molecule& h2o = byId("H2O");
    if (w.size() != 3 || vib::modes("CO2").size() != 4) { check(false, "H2O and CO2 have their modes"); return; }
    check(vib::character(h2o, w[0]).stretchShare < 0.3, "H2O: the lowest mode is a bend (" + f(vib::character(h2o, w[0]).stretchShare) + ")");
    check(vib::character(h2o, w[1]).stretchShare > 0.7 && vib::character(h2o, w[2]).stretchShare > 0.7, "H2O: the two highest modes are stretches");
    check(vib::character(h2o, w[1]).words.find("together") != std::string::npos, "H2O: the lower stretch is the symmetric one (" + vib::character(h2o, w[1]).words + ")");
    check(vib::character(h2o, w[2]).words.find("in turn") != std::string::npos, "H2O: the higher stretch is the antisymmetric one (" + vib::character(h2o, w[2]).words + ")");
    const auto& c = vib::modes("CO2");
    const Molecule& co2 = byId("CO2");
    check(vib::character(co2, c[0]).stretchShare < 0.3, "CO2: the degenerate pair is the bend");
    check(vib::character(co2, c[2]).words.find("together") != std::string::npos && !vib::irActive(c[2]), "CO2: the IR-inactive mode is the symmetric stretch (" + vib::character(co2, c[2]).words + ")");
    check(vib::character(co2, c[3]).words.find("in turn") != std::string::npos && vib::irActive(c[3]), "CO2: the strong band is the antisymmetric stretch");
    // a stretch of a bond to hydrogen lies above 2,500 cm^-1; the words name the bond
    for (int i = 0; i < count(); i++) {
        const Molecule& m = get(i);
        for (const vib::Mode& md : vib::modes(m.id)) {
            const vib::Character ch = vib::character(m, md);
            const bool hStretch = ch.stretchShare > 0.8 && ch.words.find("–H") != std::string::npos;
            if (hStretch) check(md.nuCalc > 2500, m.id + ": a stretch of a bond to H above 2,500 cm^-1 (" + f(md.nuCalc) + ", " + ch.words + ")");
            check(!ch.words.empty(), m.id + ": every mode has words");
            // 0.39.3: a torsion turns one part of the molecule about a bond: no bond angle changes, so it is not called a bend. Reference: the measured
            // mode's own description in the source (Shimanouchi's "torsion"; methanol is the one molecule here with one); planar molecules keep
            // "out of the plane" for their wags and twists
            const int k = static_cast<int>(&md - vib::modes(m.id).data());
            const vib::Measured* ms = vib::measured(m.id, k);
            const bool torsion = ms && std::string(ms->desc).find("torsion") != std::string::npos;
            check(torsion == (ch.shortWords.find("twist") != std::string::npos), m.id + " mode " + std::to_string(k) + ": called a twist exactly when the source says torsion (" + ch.shortWords + ")");
        }
    }
}

// The measured modes (NIST CCCBDB, a measurement independent of the calculation): every calculated mode belongs to one measured mode of the right degeneracy;
// the calculated harmonic wavenumber times CCCBDB's factor for its method (B3LYP/aug-cc-pVTZ 0.9675, sigma 0.019) is within 3 sigma of the measured fundamental;
// a measured infrared band is never on a mode the calculation (and symmetry) says cannot absorb.
static void testMeasured() {
    int pairs = 0;
    for (int i = 0; i < count(); i++) {
        const Molecule& m = get(i);
        const auto& md = vib::modes(m.id);
        for (int k = 0; k < static_cast<int>(md.size()); k++) {
            const vib::Measured* e = vib::measured(m.id, k);
            check(e != nullptr, m.id + ": mode " + std::to_string(k + 1) + " has a measured mode (CCCBDB)");
            if (!e) continue;
            check(e->count == vib::degeneracy(m.id, k) && e->first == vib::degenerateFirst(m.id, k), m.id + ": the measured " + e->sym + " mode covers one whole degenerate set");
            const std::string s = e->sym;
            const int want = s[0] == 'T' ? 3 : (s[0] == 'E' || s.rfind("Π", 0) == 0) ? 2 : 1;
            check(e->count == want, m.id + ": a " + s + " mode is " + std::to_string(want) + " calculated modes");
            if (k != e->first) continue;
            pairs++;
            // Two modes a harmonic calculation cannot fit, each named by NIST itself (typed again here):
            //  - methanol's torsion (CCCBDB mode 12, A"): on CCCBDB's "Big anharmonicity" list (|x_ii| > 10 % of omega; saved, cccbdb_biganharm1x_2026-11-04.html): not compared;
            //  - ammonia's umbrella (CCCBDB mode 2, 950 cm^-1): the NIST WebBook (Shimanouchi 1972; saved, webbook_NH3_vib_2026-11-04.html) lists it as two levels,
            //    932.5 and 968.3 cm^-1 (the inversion doublet: the nitrogen tunnels through the plane), where the other modes split by about 1 cm^-1: compared
            //    with the nearer of the two measured levels.
            const bool bigAnharm = m.id == "CH3OH" && e->mode == 12;
            if (bigAnharm) check(std::string(e->desc) == "torsion", "CH3OH mode 12 is the torsion");
            else if (m.id == "NH3" && e->mode == 2) {
                const double s = vib::scaleOf(m.id) * md[k].nuCalc, r = std::min(std::fabs(s / 932.5 - 1), std::fabs(s / 968.3 - 1));
                check(r < 3 * vib::scaleSigmaOf(m.id), "NH3 umbrella: " + f(s) + " cm^-1 within 3 sigma of the nearer level of the measured doublet 932.5 / 968.3 (" + f(100 * r) + " %)");
            } else if (md[k].nuCalc > 0) {
                const double r = vib::scaleOf(m.id) * md[k].nuCalc / e->fund - 1;
                check(std::fabs(r) < 3 * vib::scaleSigmaOf(m.id), m.id + " mode " + std::to_string(e->mode) + " (" + e->desc + "): " + f(vib::scaleOf(m.id)) + " x " + f(md[k].nuCalc) + " is within 3 sigma of the measured " + f(e->fund) + " cm^-1 (" + f(100 * r) + " %)");
            } else check(false, m.id + ": no imaginary wavenumber (" + f(md[k].nuCalc) + ")");
            if (std::isfinite(e->ir)) check(vib::irActive(md[k]), m.id + " mode " + std::to_string(e->mode) + ": a measured infrared band (" + f(e->ir) + " km/mol) is on a mode that can absorb");
            check(std::fabs(vib::nuShown(m.id, k) - e->fund) < 1e-9, m.id + ": the app shows the measured wavenumber");
        }
    }
    check(pairs == 79, "79 measured fundamentals for the 15 molecules (" + std::to_string(pairs) + ")");
    check(vib::irMeasuredAll("H2O") && !vib::irMeasuredAll("C6H6"), "water's spectrum is all measured, benzene's (no intensities listed) calculated");
}

// 0.29 (Molecule Lab stage 3, live): every mode at once, each a quantum harmonic oscillator at temperature T. References: the second radiation constant
// c2 = hc/k = 1.438776877 cm K (CODATA 2022), the Bose-Einstein occupation n = 1 / (exp(c2 nu / T) - 1), the oscillator's thermal energy hc nu (n + 1/2) and
// its classical limit kT (equipartition); the projection onto each mode uses only the mode vectors and the atoms' masses.
static void testLive() {
    const double c2 = 1.438776877;                       // cm K
    const double nRef = 1 / (std::exp(c2 * 1000 / 298.15) - 1);
    check(std::fabs(vib::meanQuanta(1000, 298.15) / nRef - 1) < 1e-8, "live: 1000 cm^-1 at 298.15 K holds " + f(vib::meanQuanta(1000, 298.15)) + " quanta (Bose-Einstein " + f(nRef) + ")");
    check(vib::meanQuanta(1000, 0) == 0 && vib::thermalFactor(1000, 0) == 1, "live: at 0 K no quanta, the zero-point size");
    {   // classical limit: omega^2 <Q^2> -> kT, i.e. (2n + 1) * (hc nu / 2) -> kT
        const double T = 1e6, x = c2 * 1000 / T;
        check(std::fabs(vib::thermalFactor(1000, T) * x / 2 - 1) < 1e-6, "live: at 10^6 K each mode holds kT (equipartition): " + f(vib::thermalFactor(1000, T) * x / 2));
    }
    const double kH = 6.62607015e-34, kK = 1.380649e-23, kNA = 6.02214076e23;
    for (int i = 0; i < count(); i++) {
        const Molecule& m = get(i);
        const auto& md = vib::modes(m.id);
        if (md.empty()) continue;
        Molecule a, b;
        for (double T : {0.0, 298.15, 1500.0}) {
            double worstQ = 0, worstE = 0, worstCm = 0;
            for (double t : {0.0, 1.7e-14, 3.3e-13, 2.9e-12}) {
                const double dt = 1e-19;
                vib::liveDisplaced(m, T, t, 1, a); vib::liveDisplaced(m, T, t + dt, 1, b);
                double cm[3] = {0, 0, 0}, M = 0;
                for (std::size_t j = 0; j < m.atoms.size(); j++) {
                    const double mj = element(m.atoms[j].el).massU;
                    cm[0] += mj * (a.atoms[j].x - m.atoms[j].x); cm[1] += mj * (a.atoms[j].y - m.atoms[j].y); cm[2] += mj * (a.atoms[j].z - m.atoms[j].z); M += mj;
                }
                worstCm = std::max(worstCm, std::hypot(cm[0], cm[1], cm[2]) / M);
                for (std::size_t k = 0; k < md.size(); k++) {
                    double Q = 0, Qb = 0;                     // the mode's coordinate (kg^1/2 m) at t and t + dt
                    for (std::size_t j = 0; j < m.atoms.size(); j++) {
                        const double sm = std::sqrt(element(m.atoms[j].el).massU * kU);
                        Q += sm * 1e-10 * (md[k].L[3 * j] * (a.atoms[j].x - m.atoms[j].x) + md[k].L[3 * j + 1] * (a.atoms[j].y - m.atoms[j].y) + md[k].L[3 * j + 2] * (a.atoms[j].z - m.atoms[j].z));
                        Qb += sm * 1e-10 * (md[k].L[3 * j] * (b.atoms[j].x - m.atoms[j].x) + md[k].L[3 * j + 1] * (b.atoms[j].y - m.atoms[j].y) + md[k].L[3 * j + 2] * (b.atoms[j].z - m.atoms[j].z));
                    }
                    const double nu = vib::nuShown(m.id, static_cast<int>(k)), w = 2 * kPi * kC * nu * 100;
                    const double n = T > 0 ? 1 / (std::exp(c2 * nu / T) - 1) : 0;
                    const double A = std::sqrt(kHbar / w * (2 * n + 1));             // <Q^2> = A^2 / 2 = (hbar / 2 w) coth(hc nu / 2kT)
                    worstQ = std::max(worstQ, (std::fabs(Q) - A) / A);               // never beyond its amplitude
                    const double v = (Qb - Q) / dt, E = 0.5 * (v * v + w * w * Q * Q), Eref = kH * kC * nu * 100 * (n + 0.5);
                    worstE = std::max(worstE, std::fabs(E / Eref - 1));
                }
            }
            check(worstCm < 1e-7, m.id + " live at " + f(T) + " K: the centre of mass stays still (" + f(worstCm) + " A)");
            check(worstQ < 1e-5, m.id + " live at " + f(T) + " K: no mode goes beyond its thermal amplitude (" + f(worstQ) + ")");
            check(worstE < 1e-4, m.id + " live at " + f(T) + " K: every mode holds hc nu (n + 1/2), the oscillator's thermal energy (worst " + f(worstE) + ")");
        }
    }
    {   // the thermal energy above the lowest level, kJ/mol, against the oscillator sum done here (to 1e-8: c2 above has 10 digits, and n carries x = c2 nu / T ~ 5 times its error); and its classical limit N kT - sum hc nu / 2
        const auto& md = vib::modes("H2O");
        double e = 0, zp = 0;
        for (std::size_t k = 0; k < md.size(); k++) { const double nu = vib::nuShown("H2O", static_cast<int>(k)); e += kH * kC * nu * 100 / (std::exp(c2 * nu / 1000) - 1); zp += kH * kC * nu * 100 / 2; }
        check(std::fabs(vib::thermalEnergyKJmol("H2O", 1000) / (e * kNA / 1000) - 1) < 1e-8, "live: water's vibrational energy at 1000 K " + f(vib::thermalEnergyKJmol("H2O", 1000)) + " kJ/mol (sum here " + f(e * kNA / 1000) + ")");
        const double T = 1e7, cl = (3 * kK * T - zp) * kNA / 1000;     // (the next term is sum hc nu x / 12, x = c2 nu / T: 3e-8 of it at 10^7 K)
        check(std::fabs(vib::thermalEnergyKJmol("H2O", T) / cl - 1) < 1e-6, "live: ... and at 10^7 K, 3 kT minus the zero-point energy (equipartition)");
    }
    {   // the picture's size: real, or bigger up to 3 times with the typical swing (2 x the largest rms) at most 0.3 A
        const Molecule& w = byId("H2O");
        const double r0 = vib::liveRmsA(w, 0), s = vib::liveDrawnScale(w, 298.15, true);
        check(r0 > 0.02 && r0 < 0.2, "live: water's largest rms swing at 0 K is a few hundredths of an angstrom (" + f(r0) + ")");
        check(vib::liveRmsA(w, 3000) > vib::liveRmsA(w, 298.15) && vib::liveRmsA(w, 298.15) >= r0, "live: warmer swings more");
        check(vib::liveDrawnScale(w, 298.15, false) == 1 && s >= 1 && s <= vib::kExaggerate && (s * 2 * vib::liveRmsA(w, 298.15) <= vib::kMaxDrawnA + 1e-12 || s == 1), "live: bigger is capped (" + f(s) + ")");
    }
}

// 0.34: the measured anharmonic ladder of the four diatomics (vibration.h, anharmonic()). References, independent of the constants' table row: the measured
// first rung Delta G(1/2) that the same NIST WebBook pages give in their footnotes (N2 2329.9168 cm^-1, Bendtsen 1974; O2 1556.381, Babcock & Herzberg 1948),
// and NIST CCCBDB's measured fundamentals in vib_measured.inc (H2 4161.0, HCl 2886.0 (both Huber & Herzberg 1979), N2 2330.0 (Irikura 2007), O2 1556.0).
static void testAnharmonic() {
    const std::pair<const char*, double> kFoot[] = {{"N2", 2329.9168}, {"O2", 1556.381}};
    for (const auto& [id, dg] : kFoot) {
        const vib::Anharmonic* a = vib::anharmonic(id);
        check(a != nullptr, std::string(id) + ": anharmonic constants present");
        if (a) check(std::fabs(vib::termG(*a, 1) - vib::termG(*a, 0) - dg) < 0.01, std::string(id) + ": G(1) - G(0) is the measured Delta G(1/2) within 0.01 cm^-1 (" + f(vib::termG(*a, 1) - vib::termG(*a, 0)) + ")");
    }
    for (const char* id : {"H2", "N2", "O2", "HCl"}) {
        const vib::Anharmonic* a = vib::anharmonic(id);
        const vib::Measured* m = vib::measured(id, 0);
        check(a && m, std::string(id) + ": constants and a CCCBDB fundamental");
        if (!a || !m) continue;
        const double r1 = vib::termG(*a, 1) - vib::termG(*a, 0), r2 = vib::termG(*a, 2) - vib::termG(*a, 1), r3 = vib::termG(*a, 3) - vib::termG(*a, 2);
        std::printf("anharmonic %s: rungs %.2f, %.2f, %.2f cm^-1 (CCCBDB fundamental %.1f)\n", id, r1, r2, r3, m->fund);
        check(std::fabs(r1 / m->fund - 1) < 1e-3, std::string(id) + ": the first rung is CCCBDB's measured fundamental within 0.1 %");
        check(r1 > r2 && r2 > r3, std::string(id) + ": the rungs close up (anharmonic)");
        // the textbook identity G(1) - G(0) = we - 2 wexe + (13/4) weye + 5 weze
        check(std::fabs(r1 - (a->we - 2 * a->wexe + 3.25 * a->weye + 5 * a->weze)) < 1e-9, std::string(id) + ": G(1) - G(0) = we - 2 wexe + 13/4 weye + 5 weze");
    }
    check(vib::anharmonic("H2O") == nullptr, "H2O: no diatomic constants");
}

int main() {
    testMeasured();
    testCountsAndConservation();
    testSelectionRules();
    testDiatomicTurningPoint();
    testDisplacedAndTime();
    testCharacter();
    testLive();
    testAnharmonic();
    std::printf("vibration tests: %d checks, %d failed\n", g_checks, g_failed);
    return g_failed ? 1 : 0;
}
