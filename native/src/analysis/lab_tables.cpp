// The tables each lab hands to the Data panel (0.51): see lab_tables.h. Every value here is the labs' own, with the source the labs already name.
#include "analysis/lab_tables.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <map>

#include "atom/alkali.h"
#include "atom/ame.h"
#include "atom/asd.h"
#include "atom/atom_core.h"
#include "atom/levels.h"
#include "atom/zeeman.h"
#include "molecule/molecule.h"
#include "molecule/morse.h"
#include "molecule/rotation.h"
#include "molecule/vibration.h"
#include "orbit/solar_data.h"
#include "orbit/units.h"
#include "orbit/vec3.h"

namespace analysis {

namespace {
constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();

Column num(const std::string& name, const std::string& unit, const std::string& source) { Column c; c.name = name; c.unit = unit; c.source = source; return c; }
Column txt(const std::string& name, const std::string& source = "") { Column c; c.name = name; c.source = source; return c; }
std::string fmt(const char* f, double v) { char b[64]; std::snprintf(b, sizeof b, f, v); return b; }

const char* const kJplElements = "NASA/JPL: Keplerian Elements for Approximate Positions of the Major Planets (E. M. Standish), table for 1800 AD - 2050 AD";
const char* const kFactSheet = "NASA Planetary Fact Sheet (radius, mass in Earth masses; Earth = 5.9722 × 10^24 kg)";
const char* const kJplPhys = "JPL Planetary Physical Parameters, ssd.jpl.nasa.gov/planets/phys_par.html (equatorial gravity; the dwarf planets' sizes and masses)";
const char* const kIauGm = "IAU 2009 System of Astronomical Constants: GM of the Sun = 1.32712440041 × 10^20 m^3/s^2 (TDB-compatible)";
const char* const kSatSheets = "NASA planetary satellite fact sheets (mean distance, period, eccentricity, radius)";
const char* const kSbdb = "JPL Small-Body Database (each body's own orbit record and epoch, named in src/orbit/solar_data.cpp)";
const char* const kAsd = "NIST Atomic Spectra Database (version 5.12): Kramida, A., Ralchenko, Yu., Reader, J. and NIST ASD Team (2024); vacuum wavelengths as saved in data/asd";
const char* const kAme = "AME2020: M. Wang, W.J. Huang, F.G. Kondev, G. Audi and S. Naimi, Chinese Physics C 45, 030003 (2021)";
const char* const kNubase = "NUBASE2020: F.G. Kondev, M. Wang, W.J. Huang, S. Naimi and G. Audi, Chinese Physics C 45, 030001 (2021)";
const char* const kCodata = "CODATA 2022 (Mohr et al., arXiv:2409.03787): R_inf = 10 973 731.568 157(12) m^-1, the electron's mass";
const char* const kCccbdb = "NIST CCCBDB, NIST Standard Reference Database 101, Release 22 (measured fundamentals, harmonics, rotational constants and geometries)";
const char* const kHuber = "K. P. Huber and G. Herzberg, Constants of Diatomic Molecules (1979), via the NIST Chemistry WebBook (X state)";

void addSigma(Column& c) { c.sigma.assign(c.v.size(), kNaN); }
// a value to the last place its uncertainty reaches, with the uncertainty in brackets in the last digits: 41449.451(2)
std::string limitText(double v, double u) {
    if (!(u > 0) || !std::isfinite(u)) return fmt("%.3f", v);
    const int dec = std::max(0, std::min(10, -static_cast<int>(std::floor(std::log10(u)))));
    char b[64];
    std::snprintf(b, sizeof b, "%.*f(%.0f)", dec, v, u * std::pow(10.0, dec));
    return b;
}

// ---- Orbit Lab ----
Table planets() {
    Table t;
    t.id = "planets";
    t.title = "The planets: orbits and sizes";
    t.about = "Orbits from JPL's mean elements at J2000 (accurate to about 1 part in 10^4 between 1800 and 2050; the period is 360° over the rate of the mean longitude); sizes and masses as the planet cards show them.";
    t.sources = {kJplElements, kFactSheet, kJplPhys};
    Column name = txt("planet"), a = num("a", "au", "JPL elements, a at J2000"), e = num("e", "", "JPL elements, e at J2000"), inc = num("i", "deg", "JPL elements, I at J2000"),
           T = num("period", "yr", "JPL elements: 36000° / L-dot (deg per century), Julian years"), R = num("radius", "km", "NASA fact sheet, equatorial"),
           M = num("mass", "kg", "NASA fact sheet (Earth masses × 5.9722e24 kg)"), g = num("gravity", "m/s^2", "JPL, equatorial");
    for (std::size_t i = 0; i < orbit::kPlanetCount; i++) {
        const orbit::PlanetData& p = orbit::kPlanets[i];
        name.text.push_back(p.name);
        a.v.push_back(p.el[0]); e.v.push_back(p.el[2]); inc.v.push_back(p.el[4]);
        T.v.push_back(36000.0 / p.el[7]);
        R.v.push_back(p.radiusKm); M.v.push_back(p.massEarths * units::kEarthMassKg); g.v.push_back(p.gravityMS2);
    }
    t.cols = {name, a, e, inc, T, R, M, g};
    Preset k;
    k.label = "Kepler's third law: period against a";
    k.x = 1; k.y = 4; k.model = Model::Power;
    k.expect = "n = 3/2, and A = 2π √((1 au)³ / GM☉) = 1.00002 yr (GM☉ from the IAU 2009 system; Julian years). The giants' own mass shortens their period by about √(1 + m/M☉): Jupiter by 5 parts in 10^4.";
    t.presets.push_back(k);
    Preset m;
    m.label = "Mass against radius (log-log)";
    m.x = 5; m.y = 6; m.tx = Transform::Log10; m.ty = Transform::Log10; m.model = Model::Line;   // 0.51.1: a straight line on log-log axes (it was a power law on linear axes, which Jupiter alone decided)
    m.expect = "The slope is the power n in M ∝ Rⁿ (3 for a constant density). No single law holds: the four rocky planets and the four giants lie on two different lines, "
               "so a line through all eight mixes them; pick the rows by eye in the plot and compare.";
    t.presets.push_back(m);
    return t;
}
Table dwarfs() {
    Table t;
    t.id = "dwarfs";
    t.title = "Dwarf planets: orbits and sizes";
    t.about = "Each from its own JPL orbit record (a two-body orbit at its epoch); the period is 360° over the mean motion.";
    t.sources = {kSbdb, kJplPhys, kIauGm};
    Column name = txt("body"), a = num("a", "au", "JPL SBDB"), e = num("e", "", "JPL SBDB"), inc = num("i", "deg", "JPL SBDB"), T = num("period", "yr", "JPL SBDB: 360° / n"),
           D = num("diameter", "km", "JPL"), M = num("mass", "kg", "JPL");
    for (std::size_t i = 0; i < orbit::kDwarfCount; i++) {
        const orbit::DwarfData& d = orbit::kDwarfs[i];
        name.text.push_back(d.name);
        a.v.push_back(d.aAU); e.v.push_back(d.e); inc.v.push_back(d.inclDeg);
        T.v.push_back(360.0 / d.meanMotionDegPerDay / 365.25);
        D.v.push_back(d.diameterKm > 0 ? d.diameterKm : kNaN); M.v.push_back(d.massKg > 0 ? d.massKg : kNaN);
    }
    t.cols = {name, a, e, inc, T, D, M};
    Preset k;
    k.label = "Kepler's third law: period against a";
    k.x = 1; k.y = 4; k.model = Model::Power;
    k.expect = "n = 3/2 and A = 1.0000189 yr exactly, and that is circular, not a test: JPL's small-body records give the mean motion worked out from a by Kepler's third law "
               "with Gauss's constant k (2π/k = 365.2568983 days = 1.0000189 Julian years for a = 1 au). The planets' table is the real test: their periods are measured.";
    t.presets.push_back(k);
    return t;
}
Table moons() {
    Table t;
    t.id = "moons";
    t.title = "The big moons: orbits";
    t.about = "Mean distance from the planet's centre and the sidereal period, as the moon cards show them. Pick a planet's moons, then fit: Kepler's third law gives the planet's GM.";
    t.sources = {kSatSheets};
    Column pl = txt("planet"), name = txt("moon"), a = num("a", "km", "NASA fact sheet, mean distance"), T = num("period", "d", "NASA fact sheet, sidereal"), e = num("e", "", "NASA fact sheet"),
           R = num("radius", "km", "NASA fact sheet");
    for (std::size_t i = 0; i < orbit::kMoonCount; i++) {
        const orbit::MoonData& m = orbit::kMoons[i];
        pl.text.push_back(m.planet); name.text.push_back(m.name);
        a.v.push_back(m.aKm); T.v.push_back(m.periodDays); e.v.push_back(m.e); R.v.push_back(m.radiusKm);
    }
    t.cols = {pl, name, a, T, e, R};
    t.group = 0;
    const std::string gmJ = fmt("%.4g", 6.67430e-11 * 317.8 * units::kEarthMassKg);
    Preset k;
    k.label = "Jupiter's moons: Kepler's third law, n held at 3/2";
    k.x = 2; k.y = 3; k.model = Model::Power; k.group = "Jupiter"; k.hold = 1; k.holdValue = 1.5;
    k.expect = "Untick Hold n to see n come out 1.5001. With n held, GM = 4π² (1 km)³ / A² with A in days: Send, then type 4 π^2 (1 km)^3 / fit_Apow^2 to m^3/s^2 in the calculator. "
               "(With n free, A moves with n, and this recipe is off by 0.3 %.) Jupiter's card mass (NASA, 317.8 Earth masses) with G of CODATA 2022 gives " + gmJ + " m³/s².";
    t.presets.push_back(k);
    Preset k2;
    k2.label = "Jupiter's moons: T² against a³";
    k2.x = 2; k2.y = 3; k2.tx = Transform::Cube; k2.ty = Transform::Square; k2.model = Model::Proportional; k2.group = "Jupiter";
    k2.expect = "A straight line through the origin with slope 4π²/GM: Send, then type 4 π^2 / fit_b to m^3/s^2 in the calculator. The card mass gives " + gmJ +
                " m³/s². (Every point counts the same in T², so Callisto, the farthest, decides most of the slope.)";
    t.presets.push_back(k2);
    return t;
}
Table comets() {
    Table t;
    t.id = "comets";
    t.title = "Famous comets: orbits";
    t.about = "Each a fixed two-body orbit from one JPL record; a = q / (1 - e).";
    t.sources = {kSbdb, kIauGm};
    Column name = txt("comet"), q = num("q", "au", "JPL SBDB, perihelion distance"), e = num("e", "", "JPL SBDB"), a = num("a", "au", "computed: q / (1 - e)"),
           inc = num("i", "deg", "JPL SBDB"), T = num("period", "yr", "JPL SBDB"), D = num("nucleus", "km", "JPL SBDB diameter (NEOWISE: rough, none in SBDB)");
    for (std::size_t i = 0; i < orbit::kCometCount; i++) {
        const orbit::CometData& c = orbit::kComets[i];
        name.text.push_back(c.name);
        q.v.push_back(c.qAU); e.v.push_back(c.e); a.v.push_back(c.e < 1 ? c.qAU / (1 - c.e) : kNaN); inc.v.push_back(c.inclDeg);
        T.v.push_back(c.periodDays > 0 ? c.periodDays / 365.25 : kNaN); D.v.push_back(c.nucleusKm);
    }
    t.cols = {name, q, e, a, inc, T, D};
    Preset k;
    k.label = "Kepler's third law for comets";
    k.x = 3; k.y = 5; k.model = Model::Power;
    k.expect = "n = 3/2 and A = 1.0000189 yr exactly, and that is circular, not a test: JPL's small-body records give each comet's period worked out from a by Kepler's third law "
               "with Gauss's constant k (2π/k = 365.2568983 days for a = 1 au). The planets' table is the real test: their periods are measured.";
    t.presets.push_back(k);
    return t;
}

// ---- Atom Lab ----
Table ionLines(int Z, int N) {
    const atom::AsdSpectrum& s = atom::asdIon(Z);
    Table t;
    t.id = "ionlines";
    t.title = std::string("NIST lines of ") + s.name + " (one electron)";
    const double M = atom::nuclearMassMe(Z, N);
    const double RM = rydbergCm(M);
    t.about = std::string("The measured lines of the one-electron ion of ") + atom::name(Z) + ", in vacuum. The Bohr levels with the reduced mass give 1/λ = Z² R_M (1/n₁² − 1/n₂²); fine structure, "
              "the Lamb shift and the nucleus's size move the lines by about (Zα)² of that and more.";
    t.sources = {kAsd, kCodata};
    Column lam = num("λ", "nm", "NIST ASD (observed, else Ritz)"), lo = num("n₁", "", "NIST ASD, lower level"), hi = num("n₂", "", "NIST ASD, upper level"),
           x = num("1/n₁² − 1/n₂²", "", "computed"), k = num("1/λ", "cm^-1", "computed: 10^7 / λ(nm), its uncertainty from λ's"), A = num("A", "1/s", "NIST ASD transition probability"),
           tr = txt("transition", "NIST ASD");
    addSigma(lam);
    for (int i = 0; i < s.count; i++) {
        const atom::AsdLine& L = s.lines[i];
        if (L.ni <= 0 || L.nk <= L.ni) continue;
        lam.v.push_back(L.nm); lam.sigma.push_back(std::isfinite(L.unc) && L.unc > 0 ? L.unc : kNaN);
        lo.v.push_back(L.ni); hi.v.push_back(L.nk);
        x.v.push_back(1.0 / (L.ni * L.ni) - 1.0 / (static_cast<double>(L.nk) * L.nk));
        k.v.push_back(1e7 / L.nm);
        A.v.push_back(L.aki);
        tr.text.push_back(L.transition ? L.transition : "");
    }
    k.sigma.resize(k.v.size());
    for (std::size_t i = 0; i < k.v.size(); i++) k.sigma[i] = lam.sigma[i] * 1e7 / (lam.v[i] * lam.v[i]);
    if (tr.text.empty()) tr.text.clear();
    t.cols = {lam, lo, hi, x, k, A};
    if (!tr.text.empty()) t.cols.push_back(tr);
    Preset p;
    p.label = "The Rydberg formula: 1/λ against 1/n₁² − 1/n₂²";
    p.x = 3; p.y = 4; p.model = Model::Proportional; p.useSigma = false;
    p.expect = "slope = Z² R_M = " + fmt("%.2f", Z * Z * RM) + " cm⁻¹ (R_∞ of CODATA 2022 and this nucleus's mass, " + fmt("%.6g", M) + " mₑ). The fit counts every line the same: tick \"Use the uncertainties\" "
               "and χ²/dof becomes enormous, because the Bohr levels leave out fine structure, which is far bigger than NIST's measurement errors. For the same reason the slope's ± (from the scatter) is "
               "much smaller than its distance from Z² R_M: that distance is the fine structure and the Lamb shift (Dirac's levels move Lyman α by about a quarter of (Zα)² of itself), not a measurement error.";
    t.presets.push_back(p);
    return t;
}
Table atomLines(int Z) {
    const atom::AsdSpectrum& s = atom::asdAtom(Z);
    Table t;
    t.id = "atomlines";
    t.title = std::string("NIST lines of ") + s.name + " (the neutral atom)";
    t.about = "The measured lines of the neutral atom in the lab's range, vacuum wavelengths; the intensities are NIST's relative ones, as printed (no common scale between spectra).";
    t.sources = {kAsd};
    Column lam = num("λ", "nm", "NIST ASD (observed, else Ritz)"), k = num("1/λ", "cm^-1", "computed: 10^7 / λ(nm)"), A = num("A", "1/s", "NIST ASD"), I = num("intensity", "", "NIST ASD, relative"),
           tr = txt("transition", "NIST ASD");
    addSigma(lam);
    for (int i = 0; i < s.count; i++) {
        const atom::AsdLine& L = s.lines[i];
        lam.v.push_back(L.nm); lam.sigma.push_back(std::isfinite(L.unc) && L.unc > 0 ? L.unc : kNaN);
        k.v.push_back(1e7 / L.nm);
        A.v.push_back(L.aki); I.v.push_back(atom::asdIntensity(L));
        tr.text.push_back(L.transition ? L.transition : "");
    }
    t.cols = {lam, k, A, I, tr};
    return t;
}
Table isotopes(int Z) {
    Table t;
    t.id = "isotopes";
    t.title = std::string("Isotopes of ") + atom::name(Z);
    t.about = "Every known nuclide of this element: binding energy per nucleon from AME2020 (estimates marked: the evaluators' extrapolation, not a measurement) and the half-life from NUBASE2020.";
    t.sources = {kAme, kNubase};
    Column Nc = num("N", "", "neutrons"), A = num("A", "", "mass number"), B = num("B/A", "keV", "AME2020, with its uncertainty"), est = num("estimate", "", "1: AME2020's estimate (#), 0: measured"),
           T = num("half-life", "s", "NUBASE2020 (stable: empty)"), kind = txt("kind", "NUBASE2020");
    addSigma(B);
    for (const auto& [n, code] : atom::nuclides(Z)) {
        Nc.v.push_back(n); A.v.push_back(Z + n);
        const atom::AmeRow* r = atom::ame(Z, n);
        B.v.push_back(r ? r->bindPerA : kNaN); B.sigma.push_back(r && r->unc > 0 ? r->unc : kNaN); est.v.push_back(r ? r->est : kNaN);
        double hl = kNaN;
        std::string k;
        if (code == "S") k = "stable";
        else if (code == "U") k = "unbound";
        else if (code == "?") k = "half-life not known";
        else { hl = std::atof(code.c_str() + (code[0] == '~' ? 1 : 0)); k = code[0] == '~' ? "approximate" : "measured"; if (!(hl > 0)) { hl = kNaN; k = "half-life not known"; } }
        T.v.push_back(hl); kind.text.push_back(k);
    }
    t.cols = {Nc, A, B, est, T, kind};
    Preset p;
    p.label = "Half-life against N (log scale)";
    p.x = 0; p.y = 4; p.ty = Transform::Log10; p.fit = false;
    p.expect = "The longest-lived isotopes sit in the valley of stability; the half-life falls by orders of magnitude on each side.";
    t.presets.push_back(p);
    Preset b;
    b.label = "Binding energy per nucleon against N";
    b.x = 0; b.y = 2; b.fit = false;
    b.expect = "A gentle maximum near the stable isotopes; the odd-even zigzag is the pairing energy.";
    t.presets.push_back(b);
    return t;
}
Table bindingAll() {
    Table t;
    t.id = "binding";
    t.title = "Binding energy per nucleon, all measured nuclides";
    t.about = "Every nuclide whose AME2020 mass is measured (2,549; the evaluators' estimates are left out).";
    t.sources = {kAme};
    Column Zc = num("Z", "", "protons"), Nc = num("N", "", "neutrons"), A = num("A", "", "mass number"), B = num("B/A", "keV", "AME2020, with its uncertainty");
    addSigma(B);
    for (int z = 1; z <= 118; z++)
        for (int n = 0; n <= 200; n++) {
            const atom::AmeRow* r = atom::ame(z, n);
            if (!r || r->est) continue;
            Zc.v.push_back(z); Nc.v.push_back(n); A.v.push_back(z + n); B.v.push_back(r->bindPerA); B.sigma.push_back(r->unc > 0 ? r->unc : kNaN);
        }
    t.cols = {Zc, Nc, A, B};
    Preset p;
    p.label = "The binding-energy curve: B/A against A";
    p.x = 2; p.y = 3; p.fit = false;
    std::string top;
    for (const atom::AmeRow* r : atom::ameHighestN(3)) top += (top.empty() ? "" : ", ") + std::string(atom::symbol(r->Z)) + "-" + std::to_string(r->Z + r->N) + " " + fmt("%.1f", r->bindPerA) + " keV";
    p.expect = "The highest points are near iron and nickel (" + top + "): fusion of lighter nuclei and fission of heavier ones both release energy.";
    t.presets.push_back(p);
    return t;
}
Table quantumDefects(int Z) {
    Table t;
    t.id = "defects";
    t.title = std::string("Quantum defects of ") + atom::name(Z);
    double uL = 0;
    const double L = alkaliLimitCm(Z, &uL);
    const double Mcore = atom::nuclearMassMe(Z, atom::defaultN(Z)) + (Z - 1);   // the ion core: the nucleus and Z - 1 electrons (their binding, under 10^-4 of the mass, left out)
    const double RM = rydbergCm(Mcore);
    t.about = "The valence electron's NIST levels as a Rydberg series: E = I − R_M / n*², n* = n − δ. The quantum defect δ measures how far the electron gets into the core; Rydberg-Ritz: "
              "δ = δ₀ + δ₂ / n*². R_M = " + fmt("%.4f", RM) + " cm⁻¹ for this ion core; I = " + limitText(L, uL) + " cm⁻¹ (NIST ASD).";
    t.sources = {kAsd, kCodata};
    Column n = num("n", "", "NIST ASD configuration"), l = num("l", "", "NIST ASD term"), J = num("J", "", "NIST ASD"), E = num("E", "cm^-1", "NIST ASD level, with its uncertainty"),
           W = num("I − E", "cm^-1", "computed: the binding energy"), ns = num("n*", "", "computed: √(R_M / (I − E))"), d = num("δ", "", "computed: n − n*, with its uncertainty from E and I"),
           x = num("1/n*²", "", "computed");
    addSigma(E); addSigma(d);
    for (const atom::AlkLevel& lv : atom::alkaliLevels(Z)) {
        const double w = L - lv.cm;
        if (!(w > 0)) continue;
        const double nst = std::sqrt(RM / w);
        n.v.push_back(lv.n); l.v.push_back(lv.l); J.v.push_back(lv.j2 / 2.0);
        E.v.push_back(lv.cm); E.sigma.push_back(lv.unc > 0 ? lv.unc : kNaN);
        W.v.push_back(w); ns.v.push_back(nst); d.v.push_back(lv.n - nst);
        const double u = std::sqrt((lv.unc > 0 ? lv.unc * lv.unc : 0) + uL * uL);
        d.sigma.push_back(u > 0 ? nst / (2 * w) * u : kNaN);
        x.v.push_back(1 / (nst * nst));
    }
    t.cols = {n, l, J, E, W, ns, d, x};
    t.group = 1;
    Preset p;
    p.label = "The s series: δ against 1/n*² (Rydberg-Ritz)";
    p.x = 7; p.y = 6; p.group = "0"; p.model = Model::Line;
    p.expect = "A straight line: the intercept is δ₀, the s electron's defect at the series limit, the slope δ₂. The defects fall fast with l: f and higher are almost hydrogen (δ ≈ 0). "
               "Weighted by NIST's tiny level uncertainties, χ²/dof is huge and Q = 0: two Ritz terms do not describe the lowest levels to NIST's precision (higher terms matter at n = 3 to 5). "
               "With 3 levels there is 1 degree of freedom, so δ₀ and δ₂ here are the low-n values, not the series-limit ones of long Rydberg series.";
    t.presets.push_back(p);
    Preset q;
    q.label = "δ against l (every level)";
    q.x = 1; q.y = 6; q.fit = false;
    q.expect = "Big for s and p (the orbit dives into the core), near 0 from d or f on.";
    t.presets.push_back(q);
    return t;
}
Table levelTable(int Z) {
    Table t;
    t.id = "levels";
    t.title = std::string("NIST levels of ") + atom::name(Z);
    t.about = "The atom's levels as NIST lists them (configuration, term, J), energies above the ground level. Helium's are NIST's calculated values (Drake's theory), as ASD brackets them.";
    t.sources = {kAsd};
    Column conf = txt("configuration", "NIST ASD"), term = txt("term", "NIST ASD"), J = num("J", "", "NIST ASD"), E = num("E", "cm^-1", "NIST ASD"), eV = num("E", "eV", "computed: E × h c"),
           meas = num("measured", "", "1: an observed line touches it");
    for (const atom::JLevel& lv : atom::jumpLevels(Z)) {
        conf.text.push_back(atom::confPretty(lv.conf)); term.text.push_back(atom::termPretty(lv.term));
        J.v.push_back(lv.j2 / 2.0); E.v.push_back(lv.cm); eV.v.push_back(lv.cm * 1.239841984e-4); meas.v.push_back(lv.measured ? 1 : 0);
    }
    t.cols = {conf, term, J, E, eV, meas};
    return t;
}

// ---- 0.54.0: the Zeeman effect (atom/zeeman.h) of the line the Atom Lab shows split, at its field ----
const char* const kZeemanSrc = "CODATA 2022 (Mohr et al., arXiv:2409.03787): mu_B, h, c, g_e";
const char* const kZeemanModel = "this app's Zeeman model (atom/zeeman.h): one LS term's levels at NIST's energies, mu_B B (L_z + g_s S_z), diagonalised exactly";
const atom::zeeman::LineRef* zeemanLineOf(const LabContext& c) {
    if (c.lab != 1 || !(c.fieldT > 0) || c.zKind < 0 || c.zLine < 0) return nullptr;
    const std::vector<atom::zeeman::LineRef>& v = atom::zeeman::labLines(c.Z, c.N, c.zKind);
    return c.zLine < static_cast<int>(v.size()) ? &v[std::size_t(c.zLine)] : nullptr;
}
std::string zLevelSrc(int kind) { return kind == 0 ? "the lab's Dirac levels with the Lamb shift (Yerokhin & Shabaev 2015)" : "NIST ASD levels and line"; }
Table zeemanComponents(const LabContext& c) {
    namespace z = atom::zeeman;
    const z::LineRef& r = *zeemanLineOf(c);
    Table t;
    t.id = "zeeman";
    t.title = "Zeeman components of " + r.line.name + " at " + fmt("%.4g", c.fieldT) + " T";
    t.about = "The line split by the field: one row per component M_u → M_l, q = M_u − M_l (π: 0, σ±: ±1). Shifts from the field-free line; the Lorentz unit is μB B / hc = " +
              fmt("%.6g", c.fieldT * z::lorentzCmPerT()) + " cm⁻¹. Strengths are relative to the whole line (they sum to 1 in weak fields); seen across the field π counts fully and σ half, "
              "along it σ only. " + z::usedText(r.line) + " " + z::regimeText(r.line, c.fieldT);
    t.sources = {zLevelSrc(c.zKind), kZeemanSrc, kZeemanModel};
    Column mu = num("M_u", "", "upper level"), ml = num("M_l", "", "lower level"), pol = txt("polarisation", "q = M_u − M_l"), q = num("q", "", "M_u − M_l"),
           sc = num("shift", "cm^-1", "computed"), sn = num("shift", "nm", "computed (vacuum)"), sl = num("shift", "", "computed: in Lorentz units μB B / hc"),
           lam = num("λ", "nm", "computed: the line's wavelength plus the shift (vacuum)"), st = num("strength", "", "computed: relative to the line"),
           ac = num("seen across", "", "strength × sin²θ (π) or (1 + cos²θ)/2 (σ), θ = 90°"), al = num("seen along", "", "the same, θ = 0");
    for (const z::Component& k : z::components(r.line, c.fieldT, z::gsCodata())) {
        mu.v.push_back(k.M2u / 2.0); ml.v.push_back(k.M2l / 2.0); pol.text.push_back(z::polName(k.q)); q.v.push_back(k.q);
        sc.v.push_back(k.shiftCm); sn.v.push_back(z::shiftNm(r.line, k.shiftCm)); sl.v.push_back(k.shiftCm / (c.fieldT * z::lorentzCmPerT()));
        lam.v.push_back(r.line.nm0 + z::shiftNm(r.line, k.shiftCm)); st.v.push_back(k.strength); ac.v.push_back(z::seenStrength(k, false)); al.v.push_back(z::seenStrength(k, true));
    }
    t.cols = {mu, ml, pol, q, sc, sn, sl, lam, st, ac, al};
    Preset p;
    p.label = "Strength against shift";
    p.x = 6; p.y = 8; p.fit = false;
    p.expect = "In a weak field the pattern is symmetric about the line (π in the middle, σ+ and σ− outside), the shifts are g_u M_u − g_l M_l Lorentz units and the strengths the 3j-squared pattern; "
               "in a strong field the components gather near M_L + g_s M_S changes of 0 and ±1.";
    t.presets.push_back(p);
    return t;
}
// the sigma+ component with the largest shift (the singlet triplet's only one), against 20 fields from 0.01 to 100 T
Table zeemanShift(const LabContext& c) {
    namespace z = atom::zeeman;
    const z::LineRef& r = *zeemanLineOf(c);
    const double gs = z::gsCodata();
    std::vector<z::Component> now = z::components(r.line, c.fieldT, gs);
    int Mu = 0, Ml = 0;
    bool have = false;
    double best = -INFINITY;
    for (const z::Component& k : now) if (k.q == 1 && k.shiftCm > best) { best = k.shiftCm; Mu = k.M2u; Ml = k.M2l; have = true; }
    Table t;
    t.id = "zshift";
    t.title = "Shift against field: σ+ " + z::mText(Mu) + " → " + z::mText(Ml) + " of " + r.line.name;
    const bool singlet = r.line.up.S2 == 0 && r.line.lo.S2 == 0 && r.line.up.mode != z::Mode::None && r.line.lo.mode != z::Mode::None;
    t.about = "One component of the line at 20 fields, 0.01 T to 100 T, evenly on a log scale. Shift in cm⁻¹ and in m⁻¹ (the unit of μB/hc in CODATA's table). " + z::usedText(r.line) +
              (singlet ? "" : " For the e/m analysis choose a singlet line (¹P → ¹S, ¹D → ¹P ...): its σ shift is exactly proportional to B.");
    t.sources = {zLevelSrc(c.zKind), kZeemanSrc, kZeemanModel};
    Column B = num("B", "T", "chosen"), sc = num("shift", "cm^-1", "computed"), sm = num("shift", "m^-1", "computed"), sl = num("shift", "", "computed: in Lorentz units μB B / hc"),
           st = num("strength", "", "computed: relative to the line");
    if (have)
        for (int i = 0; i < 20; i++) {
            const double b = std::pow(10.0, -2 + 4 * i / 19.0);
            for (const z::Component& k : z::components(r.line, b, gs))
                if (k.M2u == Mu && k.M2l == Ml) { B.v.push_back(b); sc.v.push_back(k.shiftCm); sm.v.push_back(100 * k.shiftCm); sl.v.push_back(k.shiftCm / (b * z::lorentzCmPerT())); st.v.push_back(k.strength); }
        }
    t.cols = {B, sc, sm, sl, st};
    if (singlet) {
        // the slope is (g_u M_u - g_l M_l) mu_B / hc: in LS a singlet level's g is 1 exactly; where NIST lists a measured g the lab uses it (zeeman.h) and says so here
        bool measured = false;
        const auto gOf = [&](const z::Term& t, int J2) {
            for (std::size_t k = 0; k < t.J2.size(); k++)
                if (t.J2[k] == J2 && t.mode == z::Mode::Linear && k < t.gLin.size()) { if (t.gMeasured[k] && J2 > 0) { measured = true; return t.gLin[k]; } return 1.0; }
            return 1.0;
        };
        const double gU = gOf(r.line.up, r.line.upJ2), gL = gOf(r.line.lo, r.line.loJ2);
        const double slope = 100 * (gU * Mu / 2.0 - gL * Ml / 2.0) * z::lorentzCmPerT();
        Preset p;
        p.label = "e/m from the normal Zeeman triplet";
        p.x = 0; p.y = 2; p.model = Model::Line; p.useSigma = false;
        p.expect = "A straight line through 0: Δσ = e B / (4π m_e c), so e/m = 4π c × slope. Expected slope " + fmt("%.9g", slope) + " m⁻¹/T, e/m = " + fmt("%.9g", 4 * atom::kPi * atom::kC * slope) +
                   " C/kg" + (measured ? " (NIST's measured g of the levels, upper " + fmt("%.6g", gU) + ", is in it: the level is not pure LS, so the slope is not exactly e/(4π m c))" : "") +
                   "; CODATA 2022: μB/hc = 46.686 447 719(15) m⁻¹ T⁻¹, e/m_e = 1.758 820 008 38(55) × 10¹¹ C/kg. "
                   "The points are made by this app's own formula, so the fit checks the method, not nature: a real experiment measures the shift (Zeeman 1896; Lorentz's theory gave e/m before the electron was named).";
        t.presets.push_back(p);
    }
    return t;
}

// ---- Molecule Lab ----
Table modesOf(const std::string& id) {
    namespace vib = molecule::vib;
    const molecule::Molecule& mol = molecule::get(molecule::findId(id));
    Table t;
    t.id = "modes";
    t.title = "Vibrations of " + molecule::prettyFormula(mol.formula);
    const double sc = vib::scaleOf(id), ssc = vib::scaleSigmaOf(id);
    t.about = std::string("One row per vibration (a degenerate pair or triple once). Measured: NIST CCCBDB. Calculated: harmonic, B3LYP/") + vib::basisOf(id) +
              " at the measured shape, computed for this app; CCCBDB's scale factor for that method is " + fmt("%.4f", sc) + " ± " + fmt("%.3f", ssc) + ".";
    t.sources = {kCccbdb};
    Column k = num("mode", "", "CCCBDB numbering"), sym = txt("symmetry", "CCCBDB"), deg = num("degeneracy", "", ""), fund = num("measured ν", "cm^-1", "CCCBDB, fundamental"),
           harm = num("measured ω", "cm^-1", "CCCBDB, harmonic (where listed)"), calc = num("calculated ω", "cm^-1", "B3LYP harmonic, this app"),
           irc = num("calculated IR", "km/mol", "B3LYP, this app"), irm = num("measured IR", "km/mol", "CCCBDB, with its uncertainty");
    addSigma(irm);
    if (id == "CH3OH") t.about += " The torsion's calculated 118 cm⁻¹ is an artefact of the measured shape (177i there, 305 cm⁻¹ at the calculation's own minimum): leave it out of a fit.";
    const std::vector<vib::Mode>& ms = vib::modes(id);
    for (int i = 0; i < static_cast<int>(ms.size()); i++) {
        if (vib::degenerateFirst(id, i) != i) continue;
        const vib::Measured* me = vib::measured(id, i);
        k.v.push_back(me ? me->mode : i + 1);
        sym.text.push_back(me && me->sym ? me->sym : "");
        deg.v.push_back(vib::degeneracy(id, i));
        fund.v.push_back(me ? me->fund : kNaN); harm.v.push_back(me ? me->harm : kNaN);
        calc.v.push_back(ms[std::size_t(i)].nuCalc > 0 ? ms[std::size_t(i)].nuCalc : kNaN);
        irc.v.push_back(ms[std::size_t(i)].irKmMol);
        irm.v.push_back(me ? me->ir : kNaN); irm.sigma.push_back(me && me->irUnc > 0 ? me->irUnc : kNaN);
    }
    t.cols = {k, sym, deg, fund, harm, calc, irc, irm};
    Preset p;
    p.label = "Scale factor: measured ν against calculated ω";
    p.x = 5; p.y = 3; p.model = Model::Proportional;
    p.expect = "slope ≈ CCCBDB's " + fmt("%.4f", sc) + " ± " + fmt("%.3f", ssc) + " for B3LYP/" + vib::basisOf(id) + ": anharmonicity and the method's own error together.";
    t.presets.push_back(p);
    return t;
}
Table modesAll() {
    namespace vib = molecule::vib;
    Table t;
    t.id = "modesall";
    t.title = "Vibrations of all 15 molecules";
    t.about = "Every vibration with both a measured fundamental (CCCBDB) and a real calculated harmonic (B3LYP, this app); degenerate sets once. Methanol's torsion is left out: its calculated 118 cm⁻¹ is an artefact of computing it at the measured shape (a check gave 177i there and 305 cm⁻¹ at the calculation's own minimum).";
    t.sources = {kCccbdb};
    Column mo = txt("molecule"), k = num("mode", "", "CCCBDB numbering"), fund = num("measured ν", "cm^-1", "CCCBDB, fundamental"), calc = num("calculated ω", "cm^-1", "B3LYP harmonic, this app");
    for (int m = 0; m < molecule::count(); m++) {
        const molecule::Molecule& mol = molecule::get(m);
        const std::vector<vib::Mode>& ms = vib::modes(mol.id);
        for (int i = 0; i < static_cast<int>(ms.size()); i++) {
            if (vib::degenerateFirst(mol.id, i) != i) continue;
            const vib::Measured* me = vib::measured(mol.id, i);
            if (!me || !(me->fund > 0) || !(ms[std::size_t(i)].nuCalc > 0)) continue;
            if (mol.id == "CH3OH" && i == 0) continue;            // the torsion (the lowest mode): see the note above
            mo.text.push_back(molecule::prettyFormula(mol.formula)); k.v.push_back(me->mode); fund.v.push_back(me->fund); calc.v.push_back(ms[std::size_t(i)].nuCalc);
        }
    }
    t.cols = {mo, k, fund, calc};
    t.group = 0;
    Preset p;
    p.label = "Scale factor of B3LYP: measured ν against calculated ω";
    p.x = 3; p.y = 2; p.model = Model::Proportional;
    p.expect = "slope ≈ 0.9675 ± 0.019, CCCBDB's factor for B3LYP/aug-cc-pVTZ fitted to hundreds of molecules.";
    t.presets.push_back(p);
    return t;
}
Table ladder(const std::string& id) {   // 0.53.1: "Vibration levels": the measured ladder in its range, the Morse model's levels above, and the Birge-Sponer analysis
    namespace vib = molecule::vib;
    namespace mo = molecule::morse;
    const molecule::vib::Anharmonic* a = vib::anharmonic(id);
    Table t;
    t.id = "ladder";
    const molecule::Molecule& mol = molecule::get(molecule::findId(id));
    const std::string name = molecule::prettyFormula(mol.formula);
    t.title = "Vibration levels of " + name;
    const mo::Curve c = mo::curve(id);
    const int top = mo::measuredTop(id), vm = mo::vMax(c);
    t.about = "Measured (v = 0 to " + std::to_string(top) + "): G(v) = ωe(v+½) − ωexe(v+½)² + ωeye(v+½)³ + ωeze(v+½)⁴ from the X state's constants, " +
              (a->validTo > 0 ? std::string("the levels they were fitted to") : std::string("whose range the source does not state: only v = 0 and 1 are shown as measured (the page quotes the measured ΔG(½))")) +
              ". Model (v = " + std::to_string(top + 1) + " to " + std::to_string(vm) + ", the last bound level): the Morse curve fixed by ωe = " + fmt("%.4f", a->we) + " and ωexe = " + fmt("%.5f", a->wexe) +
              " cm⁻¹ alone, G(v) = ωe(v+½) − ωexe(v+½)², De = ωe²/4ωexe = " + fmt("%.0f", c.De) + " cm⁻¹. The spacing ΔG(v+½) = G(v+1) − G(v) is left empty where it would join a measured and a model level.";
    t.sources = {kHuber, "the Morse model: computed from ωe and ωexe"};
    const bool origins = !mo::bandOrigins(id).empty();
    if (!std::isnan(mo::d0Measured(id))) t.sources.push_back(std::string("D₀: ") + mo::d0Source(id));
    if (origins) t.sources.push_back("band origins: D. H. Rank, D. P. Eastman, B. S. Rao and T. A. Wiggins, J. Opt. Soc. Am. 52, 1 (1962)");
    Column v = num("v", "", ""), G = num("G(v)", "cm^-1", "measured constants, or the Morse model"), u = num("v + 1", "", ""),
           dG = num("ΔG(v+½)", "cm^-1", "computed: G(v+1) − G(v)"), kind = txt("kind"), meas = num("measured ΔG(v+½)", "cm^-1", "Rank et al. 1962, from the band origins");
    const std::vector<double> o = mo::bandOrigins(id);
    for (int i = 0; i <= vm; i++) {
        v.v.push_back(i); G.v.push_back(mo::levelShown(id, i)); u.v.push_back(i + 1);
        const bool joins = i < vm && mo::isMeasured(id, i) == mo::isMeasured(id, i + 1);
        dG.v.push_back(joins ? mo::levelShown(id, i + 1) - mo::levelShown(id, i) : kNaN);
        kind.text.push_back(mo::isMeasured(id, i) ? "measured" : "model (Morse)");
        if (origins) meas.v.push_back(i < static_cast<int>(o.size()) ? o[std::size_t(i)] - (i > 0 ? o[std::size_t(i) - 1] : 0.0) : kNaN);
    }
    t.cols = {v, G, u, dG, kind};
    if (origins) t.cols.push_back(meas);
    t.group = 4;
    Preset p;
    p.label = "Birge-Sponer: ΔG(v+½) against v + 1";
    p.x = 2; p.y = origins ? 5 : 3; p.model = Model::Line; p.group = "measured";
    const mo::BirgeSponer bs = mo::birgeSponer(id);
    const double d0 = mo::d0Measured(id);
    if (bs.ok)
        p.expect = std::string(origins ? "The measured spacings (Rank et al. 1962's band origins, v = 0 to 5)" : "The spacings of the measured ladder's constants (v = 0 to " + std::to_string(top) + ", not measured band origins)") +
                   " fall on a nearly straight line: about " + fmt("%.1f", bs.intercept) + " " + fmt("%.2f", bs.slope) + " (v+1) cm⁻¹. It reaches 0 at v + 1 = " + fmt("%.1f", bs.xZero) +
                   "; the area under it from v + 1 = ½ there, (a + b/2)² / (−2b), is the Birge-Sponer estimate of the dissociation energy: " + fmt("%.0f", bs.area) + " cm⁻¹, against the measured D₀ = " +
                   fmt("%.0f", d0) + " cm⁻¹ (" + fmt("%+.0f", (bs.area / d0 - 1) * 100) + " %). The straight line overestimates: near the top the real spacings shrink faster than a line.";
    else
        p.expect = "Only one measured spacing (ΔG(½)): no line can be drawn through it. Choose H₂ or HCl; the measured D₀ of " + name + " is " + fmt("%.0f", d0) + " cm⁻¹.";
    t.presets.push_back(p);
    return t;
}
Table rotation() {
    Table t;
    t.id = "rotation";
    t.title = "Rotational constants of the 15 molecules";
    t.about = "B from the molecule's measured shape (B = h / (8π² c I), I the moment of inertia about the middle axis; linear: the only one) next to CCCBDB's measured B. Where the shape and B belong to different "
              "vibrational states (an equilibrium re shape against the ground state's B0) they differ by the vibrational average.";
    t.sources = {kCccbdb, "CODATA 2022 (h, c, u)"};
    Column mo = txt("molecule"), m = num("mass", "u", "most abundant isotopes"), I = num("I_B", "u Å^2", "computed from the shape"), Bc = num("B from shape", "cm^-1", "computed"),
           Bm = num("B measured", "cm^-1", "CCCBDB");
    for (int k = 0; k < molecule::count(); k++) {
        const molecule::Molecule& mol = molecule::get(k);
        double In[3];
        molecule::principalMoments(mol, In);
        mo.text.push_back(molecule::prettyFormula(mol.formula)); m.v.push_back(molecule::massU(mol));
        I.v.push_back(In[1]); Bc.v.push_back(molecule::rotConstCm(In[1])); Bm.v.push_back(mol.rotB > 0 ? mol.rotB : kNaN);
    }
    t.cols = {mo, m, I, Bc, Bm};
    Preset p;
    p.label = "Measured B against B from the shape";
    p.x = 3; p.y = 4; p.model = Model::Proportional;
    p.expect = "slope near 1; the scatter shows how far the shapes and the constants belong to different vibrational states.";
    t.presets.push_back(p);
    return t;
}
// 0.53: rotation (molecule/rotation.h)
const char* const kRank = "D. H. Rank, D. P. Eastman, B. S. Rao and T. A. Wiggins, J. Opt. Soc. Am. 52, 1 (1962), Table III: H35Cl's 1-0 band, observed (vacuum)";
const char* const kNistMass = "NIST Atomic Weights and Isotopic Compositions (1H, 2H, 35Cl, 37Cl): masses and abundances";
std::string tempText(double T) { return fmt("%.0f K", T); }
Table rotLevels(const LabContext& c) {
    namespace rot = molecule::rot;
    const rot::Rotor r = rot::rotor(c.molecule, c.rotIso);
    Table t;
    t.id = "rotlevels";
    const char* q = r.nLevels ? "N" : "J";
    t.title = "Rotational levels of " + r.label + (c.molecule == "HCl" && c.rotIso == rot::kNatural ? " (the main isotope of the natural mix)" : "");
    t.about = std::string("F(") + q + ") = B " + q + "(" + q + "+1)" + (r.D[0] > 0 ? " − D [" + std::string(q) + "(" + q + "+1)]²" : "") + " in the ground vibrational level, B = " + fmt("%.6g", r.B[0]) +
              (r.D[0] > 0 ? " cm⁻¹, D = " + fmt("%.4g", r.D[0]) + " cm⁻¹" : " cm⁻¹ (rigid: no centrifugal term)") + (c.molecule == "HCl" ? " (B and D of v = 0 from Huber & Herzberg's series in v + ½, γe and D's change with v included)" : "") +
              (r.scaled ? " (scaled from H³⁵Cl by the Born-Oppenheimer mass rules, not measured)" : "") +
              ". The share at " + tempText(c.T) + " is g (2" + q + "+1) e^(−hcF/kT) over the sum of all of them (the partition function). g is the nuclear-spin weight." +
              (r.nLevels ? " O₂'s ground state is a triplet: each N level is split into three (J = N−1, N, N+1) by about 2 cm⁻¹, not shown." : "");
    t.sources = {r.source, molecule::rot::measured(c.molecule) ? kHuber : kCccbdb, "nuclear spins: NUBASE2020; the weights by the rule of Herzberg, Spectra of Diatomic Molecules (1950), ch. III", "CODATA 2022 (h, c, k)"};
    if (c.molecule == "HCl") t.sources.push_back(kNistMass);
    Column J = num(q, "", "the level"), F = num("F", "cm^-1", "computed from B and D"), g = num("weight g", "", "nuclear spin"), sh = num("share", "", std::string("computed at ") + tempText(c.T)),
           deg = num(std::string("2") + q + "+1", "", "the level's states");
    const int top = std::max(10, std::min(rot::partition(r, c.T).top, 60));
    for (int j = 0; j <= top; j++) {
        const double s = rot::share(r, j, c.T);
        if (j > 10 && s < 1e-6) break;
        J.v.push_back(j); F.v.push_back(rot::term(r, 0, j)); g.v.push_back(rot::weight(r, j)); deg.v.push_back(2 * j + 1); sh.v.push_back(s);
    }
    t.cols = {J, F, g, deg, sh};
    return t;
}
Table rotLines(const LabContext& c) {
    namespace rot = molecule::rot;
    const bool hcl = c.molecule == "HCl";
    const rot::Spectrum sp = hcl ? rot::Spectrum::Fundamental : rot::Spectrum::PureRotation;
    std::vector<rot::Line> L = rot::lines(c.molecule, hcl ? c.rotIso : 0, sp, c.T, hcl ? 0 : 1e-3);
    const rot::Rotor r = rot::rotor(c.molecule, c.rotIso);
    Table t;
    t.id = "rotlines";
    const bool mix = hcl && c.rotIso == rot::kNatural;
    t.title = hcl ? std::string("Lines of ") + (mix ? "HCl (natural mix)" : r.label) + ": the fundamental band" : "Pure-rotation lines of " + r.label;
    if (hcl) {   // the band's 20 strongest-region lines, R(0..9) and P(1..10): |m| <= 10
        std::vector<rot::Line> k;
        for (const rot::Line& l : L) if (std::abs(l.m) <= 10) k.push_back(l);
        L = k;
    }
    t.sources = {r.source, hcl ? kHuber : kCccbdb, "CODATA 2022 (h, c, k)"};
    if (hcl) { t.sources.push_back(kNistMass); t.sources.push_back(kRank); }
    Column name = txt("line"), iso = txt("isotope", "NIST abundances"), m = num("m", "", "R: J+1, P: −J"), nu = num("wavenumber", "cm^-1", "computed from the constants"),
           str = num("strength", "", std::string("relative, at ") + tempText(c.T)), meas = num("measured", "cm^-1", "Rank et al. 1962 (H³⁵Cl only)");
    if (!hcl) m.source = "J+1";
    for (const rot::Line& l : L) {
        name.text.push_back(l.name()); iso.text.push_back(rot::isoName(l.iso)); m.v.push_back(l.m); nu.v.push_back(l.nu); str.v.push_back(l.strength);
        meas.v.push_back(hcl && l.iso == rot::kH35Cl ? rot::measuredHClLine(l.branch, l.J) : kNaN);
    }
    t.about = hcl ? "R(J) = ν₀ + F₁(J+1) − F₀(J) and P(J) = ν₀ + F₁(J−1) − F₀(J), with ν₀ = G(1) − G(0) from the measured ladder and F_v from Be, αe, De. Strength: the lower level's share "
                    "times the Hönl-London factor ((J+1)/(2J+1) for R, J/(2J+1) for P), the strongest 1: the transition's own strength, not the absorbance. The lines up to |m| = 10 (the strong ones at room temperature). "
                    "F_v takes B_v and D_v as Huber & Herzberg's series in v + ½ (αe, γe and the next term; D and its change with v); the terms beyond (Rank, Rao et al. 1965) are left out, "
                    "so the far lines are off by up to 0.09 cm⁻¹ at R(15) against the measured ones (0.01 cm⁻¹ at R(10)). Not in the model: Herman-Wallis, hot bands." +
                    std::string(mix ? " Both chlorine isotopes, each weighted by its abundance (0.7576 and 0.2424)." : "") + (r.scaled ? " This isotope's constants are scaled from H³⁵Cl by the Born-Oppenheimer mass rules, not measured." : "")
                  : "J → J+1 at F(J+1) − F(J) = 2B(J+1) (a rigid rotor) in the ground vibrational level; strength: the lower level's share times (J+1)/(2J+1), the strongest 1 (no absorbance factor).";
    t.cols = hcl ? std::vector<Column>{name, iso, m, nu, str, meas} : std::vector<Column>{name, m, nu, str};
    if (mix) t.group = 1;
    if (hcl) {
        const rot::Rotor h = rot::rotor("HCl", rot::kH35Cl);
        const double kU = 1.66053906892e-27, kPi = 3.14159265358979323846, kHh = 6.62607015e-34, kCc = 299792458.0;
        const double r0 = std::sqrt(kHh / (8 * kPi * kPi * kCc * 100 * h.B[0]) / (h.muU * kU)) * 1e10;
        const double re = molecule::lengthValue(molecule::get(molecule::findId("HCl")), 0, 1) ? molecule::lengthValue(molecule::get(molecule::findId("HCl")), 0, 1)->value : kNaN;
        const bool own = c.rotIso == rot::kH35Cl || mix;
        Preset p;
        p.label = "HCl's bond length from its spectrum";
        p.x = 2; p.y = own ? 5 : 3; p.model = Model::Quadratic;
        if (mix) p.group = rot::isoName(rot::kH35Cl);
        p.expect = std::string("ν = ν₀ + (B₁ + B₀) m + (B₁ − B₀) m², so the fit's b and c give B₀ = (b − c) / 2, then r₀ = √(h / (8π² c μ B₀)) with μ = ") + fmt("%.6f", h.muU) + " u (H³⁵Cl, NIST masses). " +
                   (own ? "On the measured lines (Rank et al. 1962) B₀ comes out near 10.37 cm⁻¹ and r₀ near 1.288 Å. " : "These are the lab's own lines of a scaled isotope: choose H³⁵Cl or the natural mix to fit the measured ones. ") +
                   "The lab's measured bond length (NIST CCCBDB) is re = " + fmt("%.3f", re) + " Å. r₀ comes out a little longer: B₀ belongs to the lowest vibrational level, whose zero-point motion averages 1/r² over a well that is "
                   "softer outwards, so the average 1/r² is below 1/re² (B₀ = Be − αe/2 + γe/4 − …: r₀ = " + fmt("%.4f", r0) + " Å from Huber & Herzberg's constants). The quadratic also leaves out the centrifugal term −4D m³, "
                   "which pulls its B₀ down a little more: the cubic fit below takes it in.";
        t.presets.push_back(p);
        Preset q = p;
        q.label = "The same with the centrifugal term (cubic)";
        q.model = Model::Cubic;
        q.expect = "ν = ν₀ + (B₁ + B₀) m + (B₁ − B₀) m² − 4D m³ (D₁ ≈ D₀): B₀ = (b − c) / 2 again, now close to Huber & Herzberg's B₀ = " + fmt("%.4f", h.B[0]) + " cm⁻¹, so r₀ ≈ " + fmt("%.4f", r0) +
                   " Å; −d/4 is D (D₀ = " + fmt("%.3g", h.D[0]) + " cm⁻¹ from the table's series).";
        t.presets.push_back(q);
    } else {
        Preset p;
        p.label = "B from the line spacing";
        p.x = 1; p.y = 2; p.model = Model::Proportional;
        p.expect = "ν = 2B (J+1) = 2B m: the slope is 2B = " + fmt("%.5f", 2 * r.B[0]) + " cm⁻¹ (B from NIST CCCBDB).";
        t.presets.push_back(p);
    }
    return t;
}
}  // namespace

double alkaliLimitCm(int Z, double* unc) {
    // NIST ASD, the first "Limit" row of each saved file (data/jumps/asd/<X>I_levels.tsv.gz, read 2026-11-13): the ion's ground level
    struct Lim { int Z; double cm, unc; };
    static const Lim k[] = {{3, 43487.11420, 0.00018}, {11, 41449.451, 0.002}, {19, 35009.8140, 0.0007}, {37, 33690.81, 0.01}, {55, 31406.4677325, 0.0000014}, {87, 32848.872, 0.009}};
    for (const Lim& l : k) if (l.Z == Z) { if (unc) *unc = l.unc; return l.cm; }
    if (unc) *unc = kNaN;
    return kNaN;
}
double rydbergCm(double massMe) { return kRinfCm / (1 + 1 / massMe); }

std::vector<TableRef> labTables(const LabContext& c) {
    std::vector<TableRef> out;
    Table t;
    const auto add = [&](const std::string& id) { if (makeTable(id, c, t)) out.push_back({id, t.title}); };
    if (c.lab == 0) for (const char* id : {"planets", "dwarfs", "moons", "comets"}) add(id);
    else if (c.lab == 1) for (const char* id : {"ionlines", "atomlines", "defects", "levels", "isotopes", "binding", "zeeman", "zshift"}) add(id);
    else if (c.lab == 2) for (const char* id : {"modes", "modesall", "ladder", "rotation", "rotlevels", "rotlines"}) add(id);
    // (0.55.0: lab 3, the Black Hole Lab, has no tables yet; before, any lab that was not 0 or 1 got the Molecule Lab's)
    return out;
}

bool makeTable(const std::string& id, const LabContext& c, Table& out) {
    if (id == "planets") out = planets();
    else if (id == "dwarfs") out = dwarfs();
    else if (id == "moons") out = moons();
    else if (id == "comets") out = comets();
    else if (id == "ionlines") { if (c.Z < 1 || c.Z > 118 || atom::asdIon(c.Z).count == 0) return false; out = ionLines(c.Z, c.N); }
    else if (id == "atomlines") { if (c.Z < 1 || c.Z > 118 || atom::asdAtom(c.Z).count == 0) return false; out = atomLines(c.Z); }
    else if (id == "defects") { if (!std::isfinite(alkaliLimitCm(c.Z)) || atom::alkaliLevels(c.Z).empty()) return false; out = quantumDefects(c.Z); }
    else if (id == "levels") { if (c.Z < 1 || c.Z > 118 || atom::jumpLevels(c.Z).empty()) return false; out = levelTable(c.Z); }
    else if (id == "isotopes") { if (c.Z < 1 || c.Z > 118) return false; out = isotopes(c.Z); }
    else if (id == "binding") out = bindingAll();
    else if (id == "zeeman") { if (!zeemanLineOf(c)) return false; out = zeemanComponents(c); }
    else if (id == "zshift") { if (!zeemanLineOf(c)) return false; out = zeemanShift(c); }
    else if (id == "modes") { if (molecule::findId(c.molecule) < 0) return false; out = modesOf(c.molecule); }
    else if (id == "modesall") out = modesAll();
    else if (id == "ladder") { if (molecule::findId(c.molecule) < 0 || !molecule::vib::anharmonic(c.molecule)) return false; out = ladder(c.molecule); }
    else if (id == "rotation") out = rotation();
    else if (id == "rotlevels") { const int k = molecule::findId(c.molecule); if (k < 0 || !molecule::rot::hasRotation(molecule::get(k))) return false; out = rotLevels(c); }
    else if (id == "rotlines") {
        const int k = molecule::findId(c.molecule);
        if (k < 0 || !molecule::rot::hasRotation(molecule::get(k))) return false;
        if (!molecule::rot::rotor(c.molecule).dipole) return false;
        out = rotLines(c);
    }
    else return false;
    return out.rows() > 0;
}

RecSample recSample(double tDays, const orbit::Vec3& pos, const orbit::Vec3& vel) {
    const double GM = units::kGMSun;
    RecSample o;
    o.tDays = tDays;
    const double r = orbit::length(pos), v2 = orbit::dot(vel, vel);
    o.rAu = r / units::kAU;
    o.vKmS = std::sqrt(v2) / 1e3;
    const double en = v2 / 2 - GM / r;
    o.energy = en / 1e6;
    const orbit::Vec3 hv = orbit::cross(pos, vel);
    const double h = orbit::length(hv);
    o.h = h / 1e6;
    o.aAu = en < 0 ? -GM / (2 * en) / units::kAU : kNaN;
    const orbit::Vec3 ev = (pos * (v2 - GM / r) - vel * orbit::dot(pos, vel)) * (1.0 / GM);
    o.e = orbit::length(ev);
    // ϖ = Ω + ω: Ω from the node line z × h, ω from the node to the e vector in the orbit's plane (for an orbit in the ecliptic, Ω is undefined but ϖ is not: the e vector's longitude)
    const orbit::Vec3 nv = {-hv.y, hv.x, 0};
    const double nl = orbit::length(nv);
    double varpi;
    if (nl < 1e-12 * h) varpi = std::atan2(hv.z >= 0 ? ev.y : -ev.y, ev.x);
    else {
        const double Om = std::atan2(nv.y, nv.x);
        const orbit::Vec3 nh = nv * (1 / nl), hh = hv * (1 / h);
        const double om = std::atan2(orbit::dot(orbit::cross(nh, ev), hh), orbit::dot(nh, ev));
        varpi = Om + om;
    }
    const double deg = 180 / 3.14159265358979323846;
    o.varpiDeg = std::fmod(varpi * deg + 720.0, 360.0);
    return o;
}

double grPrecessionArcsecPerCentury(double aAu, double e) {
    const double GM = units::kGMSun, c = units::kLightSpeed, a = aAu * units::kAU, pi = 3.14159265358979323846;
    const double perOrbit = 6 * pi * GM / (c * c * a * (1 - e * e));                 // rad
    const double orbits = 100 * units::kYear / (2 * pi * std::sqrt(a * a * a / GM));
    return perOrbit * orbits * 180 / pi * 3600;
}

Table orbitRecording(const std::string& body, const std::vector<RecSample>& s, bool real, bool relativity) {
    Table t;
    t.id = "recording";
    t.title = "Recording: " + body;
    t.about = std::string("Sampled from the Orbit Lab while it ran (") +
              (real ? std::string("Real: the planets pull on each other") + (relativity ? ", with general relativity (1PN)" : ", Newton only") : "Simple: each body feels only the Sun, stepped by leapfrog") +
              "). Distance and speed relative to the Sun, and the osculating orbit about the Sun alone (GM☉): the specific energy E = v²/2 − GM☉/r, the angular momentum h = |r × v|, "
              "a, e and the longitude of perihelion ϖ = Ω + ω (ecliptic and equinox J2000).";
    t.sources = {real ? "this app's simulation (Gragg-Bulirsch-Stoer steps with the planets' pull), started from JPL Horizons vectors"
                      : "this app's simulation: leapfrog steps (at most 5.6 hours) about the Sun alone, started from JPL's mean elements", kIauGm,
                kJplElements, "G. M. Clemence, The Relativity Effect in Planetary Motions, Rev. Mod. Phys. 19, 361 (1947): Mercury's perihelion, about 532″ per century from the planets",
                "A. Einstein (1915): the perihelion turns by 6πGM/(c²a(1 − e²)) per orbit at first order; c exact (SI)"};
    Column tc = num("t", "d", "time since the recording started"), r = num("r", "au", "simulation"), v = num("v", "km/s", "simulation"),
           E = num("E", "km^2/s^2", "computed: v²/2 − GM☉/r"), h = num("h", "km^2/s", "computed: |r × v|"), a = num("a", "au", "computed: −GM☉ / 2E"),
           e = num("e", "", "computed: the eccentricity vector"), w = num("ϖ", "deg", "computed: Ω + ω, unwrapped (it may pass 360°)");
    double turns = 0, last = kNaN;
    for (const RecSample& x : s) {
        tc.v.push_back(x.tDays); r.v.push_back(x.rAu); v.v.push_back(x.vKmS); E.v.push_back(x.energy); h.v.push_back(x.h); a.v.push_back(x.aAu); e.v.push_back(x.e);
        double wd = x.varpiDeg + turns;
        if (std::isfinite(last)) { while (wd - last > 180) { wd -= 360; turns -= 360; } while (wd - last < -180) { wd += 360; turns += 360; } }
        w.v.push_back(wd);
        last = wd;
    }
    t.cols = {tc, r, v, E, h, a, e, w};
    Preset p;
    p.label = "Vis-viva: v² against 1/r";
    p.x = 1; p.y = 2; p.tx = Transform::Inverse; p.ty = Transform::Square; p.model = Model::Line;
    p.expect = "A straight line with slope 2GM☉ = " + fmt("%.2f", 2 * units::kGMSun / 1e6 / units::kAU) + " km²/s² · au and intercept −GM☉/a (GM☉ from the IAU). Record at least part of an orbit with some change in r. "
               "The points are the simulation's: in Simple the leapfrog's steps leave the slope off by about a part in a million for Mercury; in Real the planets' pull moves it more.";
    t.presets.push_back(p);
    Preset q;
    q.label = "Distance against time";
    q.x = 0; q.y = 1; q.fit = false;
    t.presets.push_back(q);
    Preset en;
    en.label = "Energy against time (conservation)";
    en.x = 0; en.y = 3; en.model = Model::Line;
    en.expect = "Simple: a flat line, E = −GM☉/2a; the slope is the integrator's drift (leapfrog keeps the energy to a small wobble, with no drift). Real: E about the Sun alone is "
                "not conserved (the planets exchange energy; only the whole system's total is), so it wanders, mostly with the other planets' periods.";
    t.presets.push_back(en);
    Preset pr;
    pr.label = "Perihelion: ϖ against time";
    pr.x = 0; pr.y = 7; pr.model = Model::Line;
    const double a0 = s.empty() ? 0 : s.front().aAu, e0 = s.empty() ? 0 : s.front().e;
    std::string jpl;
    for (std::size_t i = 0; i < orbit::kPlanetCount; i++)
        if (body == orbit::kPlanets[i].name) jpl = " JPL's mean elements give " + fmt("%.1f", orbit::kPlanets[i].el[9] * 3600) + "″ per century for " + body + " (ϖ-dot, 1800–2050, all causes).";
    pr.expect = "The slope in degrees per day: Send, then type fit_b to arcsec/yr and times 100 for a century. Simple: 0 (a Kepler ellipse does not turn). Real: the other planets turn it "
                "(Mercury: about 532″ per century, Clemence 1947); with general relativity on (Controls) it turns " +
                (std::isfinite(a0) && a0 > 0 && e0 < 1 ? fmt("%.2f", grPrecessionArcsecPerCentury(a0, e0)) + "″ per century faster for this orbit" : std::string("faster")) +
                " (6πGM☉/(c²a(1 − e²)) per orbit). ϖ also wobbles with the planets' periods: record many orbits, and run the same dates with relativity on and off to see the difference." + jpl;
    t.presets.push_back(pr);
    return t;
}

}  // namespace analysis
