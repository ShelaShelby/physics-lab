// 0.73.0: the Alpha Centauri system (orbit/alphacen.h). Every number is re-read in the saved paper (sources repo data/alphacen/; names in alphacen.h
// and native/data/alphacen/README.txt) and named where it is used; About, "Where the facts come from", lists them.
#include "alphacen.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "card_text.h"
#include "format.h"
#include "sky.h"
#include "sun_facts.h"
#include "units.h"

namespace orbit::acen {

using namespace units;

namespace {
constexpr double d2r = kPi / 180;
constexpr double kPcAU = 648000.0 / kPi;                         // au per parsec (IAU 2015 B2: 1 pc = 648000/pi au)
constexpr double kObliquityArcsec = 84381.448;                   // the lab's J2000 obliquity (orbit/iau_rotation.h, moon_orientation.h)
// the A-B barycentre, ICRS J2019.5: A21 Table 9 (219.85892215, -60.83163195 deg; proper motion -3639.95, +700.40 mas/yr)
constexpr double kBaryRa = 219.85892215, kBaryDec = -60.83163195, kBaryPmRa = -3639.95, kBaryPmDec = 700.40;
// Proxima, Gaia DR3 at epoch 2016.0 as Libralato et al. 2025 (ARES I) Table 2 gives it: 217.392321472, -62.676075117 deg, parallax 768.067 +- 0.050 mas
constexpr double kProxRa = 217.392321472, kProxDec = -62.676075117, kProxPlxMas = 768.067;
constexpr double kEpoch2016Jd = 2457389.0;                       // J2016.0 (TDB)
// Proxima about A-B: A21 Table 10 (a kau, e, i, Omega, omega deg, periastron passage +278 kyr from "present" = A21's epoch 2019.5)
constexpr double kProxAkau = 8.2, kProxE = 0.497, kProxIDeg = 124.9, kProxNodeDeg = 165.0, kProxPeriDeg = 151.0, kProxPeriFromNowKyr = 278.0, kPresentYear = 2019.5;

Vec3 radec(double raDeg, double decDeg) {
    const double a = raDeg * d2r, d = decDeg * d2r;
    return {std::cos(d) * std::cos(a), std::cos(d) * std::sin(a), std::sin(d)};
}
// the scene's axes in ICRS (at the A-B barycentre's J2019.5 direction): west, north, towards the Sun
struct Axes { Vec3 W, N, S; };
const Axes& axes() {
    static const Axes ax = [] {
        const double a = kBaryRa * d2r, d = kBaryDec * d2r;
        const Vec3 r = radec(kBaryRa, kBaryDec), E{-std::sin(a), std::cos(a), 0}, N{-std::sin(d) * std::cos(a), -std::sin(d) * std::sin(a), std::cos(d)};
        return Axes{E * -1.0, N, r * -1.0};
    }();
    return ax;
}
Vec3 eqToEcl(const Vec3& v) {
    const double e = kObliquityArcsec / 3600 * d2r, c = std::cos(e), s = std::sin(e);
    return {v.x, c * v.y + s * v.z, -s * v.y + c * v.z};
}
Vec3 eclToEq(const Vec3& v) {
    const double e = kObliquityArcsec / 3600 * d2r, c = std::cos(e), s = std::sin(e);
    return {v.x, c * v.y - s * v.z, s * v.y + c * v.z};
}
// a sky-frame orbit (the binary convention: x north, y east, z away from us, as keplerState's Rz(Omega) Rx(i) Rz(omega) gives it) -> the scene
Vec3 skyToScene(const Vec3& v) { return {-v.y, v.x, -v.z}; }
// galactic Cartesian axes (X the galactic centre, Z the north galactic pole) -> the scene, through the lab's own galactic directions (sky.cpp, ecliptic)
Vec3 galToScene(const Vec3& g) {
    static const Vec3 gx = [] { return skyData().galCentre; }();
    static const Vec3 gz = [] { const Vec3 p = skyData().galPole, c = skyData().galCentre; const Vec3 q = p - c * dot(p, c); return q * (1 / length(q)); }();
    static const Vec3 gxo = [] { const Vec3 x = gx - gz * dot(gx, gz); return x * (1 / length(x)); }();
    static const Vec3 gy = cross(gz, gxo);
    const Vec3 ecl = gxo * g.x + gy * g.y + gz * g.z;
    return equatorialToScene(eclToEq(ecl));
}
// Proxima on A21's orbit (no shift), scene frame
State proximaOnA21(double jdTDB) {
    const double a = kProxAkau * 1000 * kAU, P = 2 * kPi * std::sqrt(a * a * a / proximaGM()) / kYear;
    const double T0 = kPresentYear + kProxPeriFromNowKyr * 1000;
    const double M = 360.0 * (yearOfJd(jdTDB) - T0) / P;
    const State g = keplerState(a, kProxE, kProxIDeg, kProxNodeDeg, kProxPeriDeg, M, proximaGM());
    return {galToScene(g.pos), galToScene(g.vel)};
}
Vec3 proximaMeasured2016() {
    // the barycentre moved from J2019.5 to J2016.0 by its proper motion (mas/yr; RA's on the sky, so / cos dec)
    const double dt = 2016.0 - 2019.5;
    const double ra = kBaryRa + kBaryPmRa * dt / 3.6e6 / std::cos(kBaryDec * d2r), de = kBaryDec + kBaryPmDec * dt / 3.6e6;
    const Vec3 ab = radec(ra, de) * (kPcAU / (kAbPlxMas / 1000) * kAU), px = radec(kProxRa, kProxDec) * (kPcAU / (kProxPlxMas / 1000) * kAU);
    return equatorialToScene(px - ab);
}
Vec3 proximaShift() {
    static const Vec3 s = proximaMeasured2016() - proximaOnA21(kEpoch2016Jd).pos;
    return s;
}
}  // namespace

const char* thingName(int k) {
    static const char* const n[ThingCount] = {"A-B centre of mass", "Alpha Centauri A", "Alpha Centauri B", "Proxima Centauri", "Proxima b", "Proxima d",
                                              "Proxima c (disputed)", "Candidate around A (unconfirmed)", "Sun"};
    return k >= 0 && k < ThingCount ? n[k] : "";
}

double yearOfJd(double jd) { return 2000.0 + (jd - 2451545.0) / 365.25; }
double abSemiMajorAU() { return kAbAArcsec / (kAbPlxMas / 1000); }
double abGM() { return kGMSun * (kMassA + kMassB); }
double abPeriodYears() { const double a = abSemiMajorAU() * kAU; return 2 * kPi * std::sqrt(a * a * a / abGM()) / kYear; }

Vec3 bAboutA(double jdTDB) {
    // mean anomaly from A21's T0 and P (the elements were fitted with them); keplerState about GM of A + B
    const double M = 360.0 * (yearOfJd(jdTDB) - kAbT0Year) / kAbPYear;
    return skyToScene(keplerState(abSemiMajorAU() * kAU, kAbE, kAbIDeg, kAbNodeDeg, kAbPeriDeg, M, abGM()).pos);
}

Stars stars(double jdTDB) {
    const double M = 360.0 * (yearOfJd(jdTDB) - kAbT0Year) / kAbPYear;
    const State rel = keplerState(abSemiMajorAU() * kAU, kAbE, kAbIDeg, kAbNodeDeg, kAbPeriDeg, M, abGM());
    const Vec3 r = skyToScene(rel.pos), v = skyToScene(rel.vel);
    const double fA = kMassB / (kMassA + kMassB), fB = kMassA / (kMassA + kMassB);
    const State p = proximaState(jdTDB);
    return {r * -fA, r * fB, p.pos, v * -fA, v * fB, p.vel};
}

Vec3 Ellipse::at(double E) const { return F + P * (a * (std::cos(E) - e)) + Q * (b * std::sin(E)); }

namespace {
Ellipse relEllipse(double scale) {   // B's relative orbit times scale (A: -mB/(mA+mB), B: +mA/(mA+mB)) about the origin
    Ellipse o;
    const double a = abSemiMajorAU() * kAU;
    const Vec3 p = skyToScene(keplerState(1, 0, kAbIDeg, kAbNodeDeg, kAbPeriDeg, 0, 1).pos), q = skyToScene(keplerState(1, 0, kAbIDeg, kAbNodeDeg, kAbPeriDeg, 90, 1).pos);
    const double s = scale < 0 ? -1 : 1;
    o.P = p * s; o.Q = q * s; o.a = a * std::fabs(scale); o.e = kAbE; o.b = o.a * std::sqrt(1 - kAbE * kAbE); o.periodYears = abPeriodYears();
    return o;
}
}  // namespace
Ellipse orbitOfA() { return relEllipse(-kMassB / (kMassA + kMassB)); }
Ellipse orbitOfB() { return relEllipse(kMassA / (kMassA + kMassB)); }

double proximaGM() { return kGMSun * (kMassA + kMassB + kMassProxima); }
State proximaState(double jdTDB) { State s = proximaOnA21(jdTDB); s.pos = s.pos + proximaShift(); return s; }
double proximaOrbitMissAU() { return length(proximaShift()) / kAU; }
Ellipse proximaOrbit() {
    Ellipse o;
    const double a = kProxAkau * 1000 * kAU;
    o.F = proximaShift();
    o.P = galToScene(keplerState(1, 0, kProxIDeg, kProxNodeDeg, kProxPeriDeg, 0, 1).pos);
    o.Q = galToScene(keplerState(1, 0, kProxIDeg, kProxNodeDeg, kProxPeriDeg, 90, 1).pos);
    o.a = a; o.e = kProxE; o.b = a * std::sqrt(1 - kProxE * kProxE);
    o.periodYears = 2 * kPi * std::sqrt(a * a * a / proximaGM()) / kYear;
    return o;
}

const AbSolution* abSolutions(int& n) {
    static const AbSolution k[3] = {
        {"Akeson et al. 2021", 17.4930, 0.0096, 0.51947, 79.2430, 205.073, 231.519, 79.762, 0.019, 1955.564, 750.81, 0.38, 1.0788, 0.0029, 0.9092, 0.0025},
        {"Kervella et al. 2016", 17.592, 0.013, 0.5208, 79.320, 205.064, 232.006, 79.929, 0.013, 1955.604, 747.17, 0.61, 1.1055, 0.0039, 0.9373, 0.0033},
        {"Pourbaix & Boffin 2016", 17.66, 0.026, 0.524, 79.32, 204.75, 232.3, 79.91, 0.013, 1955.66, 743.0, 1.3, 1.133, 0.0050, 0.972, 0.0045}};
    n = 3;
    return k;
}

const PlanetData& planet(int k) {
    // b, d: N25 Table 3 (circular fits; T0 the inferior conjunction, BJD; m sin i, and the mass if in the star's equator plane, i = 47 deg; S, Teq at A = 0.3).
    // c: Damasso et al. 2020 as Kervella et al. 2020 quote it: P 1900 +96 -82 d (Table 1), a 1.489 +- 0.049 au (Table 2), m sin i 5.7 +- 1.9 M_E (Table 2),
    // inferior conjunction BJD 2455892 +101 -102 (Table 1); Kervella 2020's Gaia astrometry: i = 152 +- 14 deg, true mass 12 +12 -5 M_E (if it exists).
    static const PlanetData p[3] = {
        {"Proxima b", 11.18465, 0.04848, 1.055, 0.055, 2460548.59, 1.44, 0.21, 0.641, 218, "confirmed"},
        {"Proxima d", 5.12338, 0.02881, 0.260, 0.038, 2460557.55, 0.357, 0.072, 1.814, 282, "confirmed (NIRPS 2025)"},
        {"Proxima c", 1900, 1.489, 5.7, 1.9, 2455892, 0, 0, 0, 0, "disputed"}};
    return p[std::clamp(k, 0, 2)];
}

Vec3 planetOffset(int k, double jdTDB) {
    const PlanetData& d = planet(k);
    const double a = d.aAU * kAU, P = d.periodDays * kDay, GM = 4 * kPi * kPi * a * a * a / (P * P);   // the GM that makes N25's a and P one Kepler circle
    // the argument of latitude from the ascending node: 270 deg at the inferior conjunction (z away from us smallest), growing by 360 deg per period
    const double u = 270.0 + 360.0 * (jdTDB - d.t0Bjd) / d.periodDays;
    return skyToScene(keplerState(a, 0, kPlanetIncDeg, kPlanetNodeDeg, 0, u, GM).pos);
}

Ellipse planetOrbit(int k, const Vec3& proxima) {
    Ellipse o;
    o.F = proxima;
    o.P = skyToScene(keplerState(1, 0, kPlanetIncDeg, kPlanetNodeDeg, 0, 0, 1).pos);
    o.Q = skyToScene(keplerState(1, 0, kPlanetIncDeg, kPlanetNodeDeg, 0, 90, 1).pos);
    o.a = o.b = planet(k).aAU * kAU; o.e = 0; o.periodYears = planet(k).periodDays / 365.25;
    return o;
}

double rvSemiAmplitude(double P, double m, double M) {
    return std::cbrt(2 * kPi * kGMSun / P) * m / std::pow(M + m, 2.0 / 3.0);
}
double keplerAAU(double P, double Mt) { return std::cbrt(kGMSun * Mt * P * P / (4 * kPi * kPi)) / kAU; }

const StarFacts& starFacts(int k) {
    static const StarFacts s[3] = {
        {"Alpha Centauri A", "Rigil Kentaurus", "G2 V", "Pourbaix & Boffin 2016", 1.0788, 0.0029, "Akeson et al. 2021", 1.2175, 0.0055, "Akeson et al. 2021",
         5795, 19, "Kervella et al. 2017", 1.5059, 0.0019, "Akeson et al. 2021", -0.01, 0.71},
        {"Alpha Centauri B", "Toliman", "K1 V", "Pourbaix & Boffin 2016", 0.9092, 0.0025, "Akeson et al. 2021", 0.8591, 0.0036, "Akeson et al. 2021",
         5231, 21, "Kervella et al. 2017", 0.4981, 0.0007, "Akeson et al. 2021", 1.35, 0.90},
        // Proxima: M5.5 V (Bessell 1991 as N25 Table 2 quotes it); mass 0.1221 +- 0.0022 (Kervella 2017 Table 1, Mann 2015's relation; the lensing mass
        // 0.150 +0.062 -0.051, Zurlo et al. 2018, is shown beside it on the card); radius 0.154 +- 0.001 (Rabus et al. 2019 Table 1, interferometry);
        // Teff 2900 +- 100 K (Pavlenko et al. 2017 as N25 Table 2 quotes it); L 0.00151 +- 0.00008 (Ribas et al. 2017 as N25 quotes it)
        {"Proxima Centauri", "Proxima Centauri", "M5.5 V", "Bessell 1991, as NIRPS 2025", 0.1221, 0.0022, "Kervella et al. 2017", 0.154, 0.001, "Rabus et al. 2019",
         2900, 100, "Pavlenko et al. 2017", 0.00151, 0.00008, "Ribas et al. 2017", 11.13, 1.807}};
    return s[std::clamp(k, 0, 2)];
}

void habitableZoneAU(int k, double& in, double& out) {
    const StarFacts& f = starFacts(k);
    in = sunfacts::hzEdgeAU(f.lum, f.teff, sunfacts::Hz::Runaway);
    out = sunfacts::hzEdgeAU(f.lum, f.teff, sunfacts::Hz::MaxGreenhouse);
}

void eclipticToScene(double M[9]) {
    const Axes& ax = axes();
    const Vec3 r[3] = {eqToEcl(ax.W), eqToEcl(ax.N), eqToEcl(ax.S)};
    for (int i = 0; i < 3; i++) { M[3 * i] = r[i].x; M[3 * i + 1] = r[i].y; M[3 * i + 2] = r[i].z; }
}
Vec3 equatorialToScene(const Vec3& v) { const Axes& ax = axes(); return {dot(v, ax.W), dot(v, ax.N), dot(v, ax.S)}; }
Vec3 sceneToEquatorial(const Vec3& v) { const Axes& ax = axes(); return ax.W * v.x + ax.N * v.y + ax.S * v.z; }

// V = M_V + 5 log10(d / 10 pc), M_V(Sun) = 4.81 (Willmer 2018, ApJS 236, 47, Table 3, Johnson V, Vega system; data/photometry)
double sunVFromAB() { return 4.81 + 5 * std::log10(1000.0 / kAbPlxMas / 10); }
double sunVFromProxima() { return 4.81 + 5 * std::log10(1000.0 / kProxPlxMas / 10); }
Vec3 sunDirectionEquatorial() {                          // the Sun's star of Alpha Centauri's sky (acenSky), back in ICRS
    const Sky& s = acenSky();
    for (const auto& nm : s.names) if (nm.second == "Sun") return sceneToEquatorial(s.stars[std::size_t(nm.first)].dir);
    return Vec3{};
}

#include "sky_dist.inc"   // kSkyStarPc: each catalogue star's distance (HYG v4.1; tools/acen_sky.py)

// The night sky seen from the A-B centre of mass (0.73.0, step 6 of PLAN_ALPHACEN.md): every catalogue star moved by the parallax of the 1.3319 pc step
// (A21's distance; its direction A21 Table 9), its V changed by 5 log10(new distance / old) (the inverse square law); a star without a distance stays as
// it is (infinitely far); A and B themselves (now under 0.05 pc away) are left out; our Sun added at the antipode, V 0.43 (Willmer 2018's M_V 4.81 at
// A21's distance), its colour from Willmer 2018's M_B - M_V = 5.44 - 4.81 = 0.63. Then the whole sky is turned into the scene's frame. Made once (2,874
// stars: under a millisecond, not a wait).
const Sky& acenSky() {
    static const Sky S = [] {
        Sky s = skyData();
        double M[9];
        eclipticToScene(M);
        const auto turn = [&](const Vec3& v) { return Vec3{M[0] * v.x + M[1] * v.y + M[2] * v.z, M[3] * v.x + M[4] * v.y + M[5] * v.z, M[6] * v.x + M[7] * v.y + M[8] * v.z}; };
        const Vec3 here = eqToEcl(radec(kBaryRa, kBaryDec)) * (1000.0 / kAbPlxMas);   // Alpha Centauri from the Sun, pc, ecliptic
        const std::size_t n = std::min(s.stars.size(), sizeof(kSkyStarPc) / sizeof(kSkyStarPc[0]));
        for (std::size_t i = 0; i < s.stars.size(); i++) {
            SkyStar& st = s.stars[i];
            const double d = i < n ? kSkyStarPc[i] : 0;
            if (d > 0) {
                const Vec3 v = st.dir * d - here;
                const double d2 = length(v);
                if (d2 < 0.05) st.mag = 99;                      // A and B themselves: not in their own sky (drawSky skips V > 30)
                else { st.dir = v * (1 / d2); st.mag = static_cast<float>(st.mag + 5 * std::log10(d2 / d)); }
            }
            st.dir = turn(st.dir);
        }
        for (auto& c : s.cons) c.second = turn(c.second);
        s.galCentre = turn(s.galCentre);
        s.galPole = turn(s.galPole);
        const double sunBV = 5.44 - 4.81;                       // Willmer 2018 Table 3 (M_B, M_V)
        const int bucket = std::clamp(static_cast<int>(std::lround((sunBV + 0.3) / 2.3 * (kSkyColourBuckets - 1))), 0, kSkyColourBuckets - 1);   // (sky.cpp's rule)
        s.stars.push_back({Vec3{0, 0, 1}, static_cast<float>(sunVFromAB()), bucket});
        s.names.push_back({static_cast<int>(s.stars.size() - 1), "Sun"});
        return s;
    }();
    return S;
}

}  // namespace orbit::acen

// ---- the cards ----
namespace orbit::acen {
namespace {
std::string pm(double v, double e, int d) { return fmt::fixed(v, d) + " ± " + fmt::fixed(e, d); }
}  // namespace

void acenCard(int k, double jd, Card& c) {
    // one fact a line (the new UI's card wraps them); every number with its source (alphacen.h; About, "Where the facts come from")
    c = Card{};
    const Stars st = stars(jd);
    const auto distAU = [](const Vec3& a, const Vec3& b) { return length(a - b) / kAU; };
    const std::string age = "Age: disputed, 5.3 ± 0.3 billion years (Joyce & Chaboyer 2018) or 7.2–7.8 (Thévenin et al. 2026)";
    const auto starLines = [&](int s) {
        const StarFacts& f = starFacts(s);
        c.lines.push_back("Mass: " + pm(f.mass, f.massErr, 4) + " Suns (" + f.massSrc + ")");
        if (s == 2) c.lines.push_back("Weighed directly by bent light (lensing): 0.150 +0.062 −0.051 Suns (Zurlo et al. 2018); the mass above is from its brightness by a relation made on binary stars (Mann et al. 2015)");
        c.lines.push_back("Radius: " + pm(f.radius, f.radiusErr, s == 2 ? 3 : 4) + " Suns (" + f.radiusSrc + ")");
        c.lines.push_back("Surface: " + fmt::commas(f.teff) + " ± " + fmt::commas(f.teffErr) + " K (" + f.teffSrc + ")");
        c.lines.push_back("Light: " + pm(f.lum, f.lumErr, s == 2 ? 5 : 4) + " Suns (" + f.lumSrc + ")");
        double in = 0, out = 0;
        habitableZoneAU(s, in, out);
        c.lines.push_back("Habitable zone: " + fmt::fixed(in, s == 2 ? 4 : 3) + " – " + fmt::fixed(out, s == 2 ? 4 : 3) + " au (Kopparapu et al. 2014, the green band)");
        c.lines.push_back("Type: " + std::string(f.type) + " (" + f.typeSrc + ")");
        if (s < 2) c.lines.push_back("Name: " + std::string(f.iau) + " (the HYG v4.1 catalogue)");
        c.lines.push_back(age);
    };
    switch (k) {
    case Bary:
        c.title = "Alpha Centauri A-B · centre of mass";
        c.lines = {"Distance from the Sun: 4.344 light years (parallax 750.81 ± 0.38 mas, Akeson et al. 2021)",
                   "A to B now: " + fmt::fixed(distAU(st.A, st.B), 2) + " au (closest " + fmt::fixed(abSemiMajorAU() * (1 - kAbE), 2) + ", farthest " + fmt::fixed(abSemiMajorAU() * (1 + kAbE), 2) + ")",
                   "One turn: " + fmt::fixed(kAbPYear, 3) + " years (Akeson et al. 2021)",
                   "The published orbits disagree by about 5 % in mass, more than their error bars (Pourbaix & Boffin 2016: 1.133 and 0.972 Suns; Kervella et al. 2016: 1.1055 and 0.9373)",
                   "Shown “now”: no light-travel time (the light we see left 4.3 years ago)"};
        return;
    case StarA: case StarB:
        c.title = std::string(starFacts(k - 1).name) + " · " + starFacts(k - 1).type + " star";
        starLines(k - 1);
        c.lines.push_back("Distance to " + std::string(k == StarA ? "B" : "A") + " now: " + fmt::fixed(distAU(st.A, st.B), 2) + " au");
        c.lines.push_back("Orbit: a " + fmt::fixed(abSemiMajorAU(), 3) + " au, e " + fmt::fixed(kAbE, 5) + ", " + fmt::fixed(kAbPYear, 3) + " years (Akeson et al. 2021)");
        return;
    case Proxima: {
        const Ellipse o = proximaOrbit();
        c.title = "Proxima Centauri · M5.5 V red dwarf";
        starLines(2);
        c.lines.push_back("Distance to A-B now: " + fmt::commas(distAU(st.P, Vec3{})) + " au (its measured place)");
        c.lines.push_back("Orbit about A-B: a " + fmt::commas(o.a / kAU) + " au, e " + fmt::fixed(o.e, 3) + ", one turn in " + fmt::commas(o.periodYears / 1000) +
                          " thousand years; bound at 8.3 sigma (Akeson et al. 2021)");
        c.lines.push_back("The orbit's tilt and turn are uncertain by tens of degrees; drawn through its measured place (the published orbit puts it " + fmt::commas(proximaOrbitMissAU()) + " au from there)");
        c.lines.push_back("Flares: a naked-eye superflare in March 2016; at least five superflares a year (Howard et al. 2018)");
        return;
    }
    case PlanetB: case PlanetD: {
        const PlanetData& p = planet(k - PlanetB);
        c.title = std::string(p.name) + " · planet, " + p.status;
        double in = 0, out = 0;
        habitableZoneAU(2, in, out);
        c.lines = {"Mass: at least " + pm(p.msiniEarth, p.msiniErr, 3) + " Earths (Suárez Mascareño et al. 2025)",
                   "Mass if in the star's equator plane (47°): " + pm(p.mass47, p.mass47Err, k == PlanetB ? 2 : 3) + " Earths (the same paper)",
                   "Size: unknown (it does not pass in front of the star); the dot is not to scale",
                   "Orbit: " + fmt::fixed(p.aAU, 5) + " au, " + fmt::fixed(p.periodDays, 5) + " days (Suárez Mascareño et al. 2025)",
                   "Tilt: assumed, the star's equator (47°, Klein et al. 2021); the node on our sky's east-west line, assumed",
                   "Starlight: " + fmt::fixed(p.fluxEarth, 3) + " of Earth's; " + fmt::commas(p.teqK) + " K if it reflects 30 % (an estimate)",
                   p.aAU >= in && p.aAU <= out ? std::string("Inside Proxima's habitable zone") : std::string("Closer to Proxima than its habitable zone")};
        return;
    }
    case PlanetC: {
        const PlanetData& p = planet(2);
        c.title = "Proxima c · disputed planet";
        c.lines = {"Status: disputed",
                   "Proposed in 2020 (Damasso et al.); Artigau et al. 2022 tie the signal to small errors of the older speed measurements; NIRPS 2025 (Suárez Mascareño et al.) finds no significant signal",
                   "Mass: at least " + pm(p.msiniEarth, p.msiniErr, 1) + " Earths (as Kervella et al. 2020 quote it)",
                   "Orbit: " + fmt::fixed(p.aAU, 3) + " au, " + fmt::commas(p.periodDays) + " days (as Kervella et al. 2020 quote it)",
                   "If real, Gaia's astrometry hints at a tilt of 152 ± 14° (Kervella et al. 2020)",
                   "Drawn dashed, in the assumed plane of b and d"};
        return;
    }
    case CandidateA:
        c.title = "Candidate around A · unconfirmed";
        c.lines = {"Status: unconfirmed, seen once",
                   "JWST, 10 Aug 2024: a point 1.51\" from A (Beichman et al. 2025); maybe the same as a 2019 VLT point (Wagner et al. 2021)",
                   "Not seen in February and April 2025",
                   "If real: a giant of about 1.0–1.1 Jupiter radii, 90–150 Earth masses, about 225 K (Beichman et al. 2025)",
                   "The band: its possible orbit sizes, 1.58–2.23 au (Beichman et al. 2025, Table 4), drawn face-on as we see it"};
        return;
    case Sun:
        c.title = "The Sun · seen from Alpha Centauri";
        c.lines = {"Distance: 4.344 light years",
                   "In their sky: a star of V " + fmt::fixed(sunVFromAB(), 2) + " (" + fmt::fixed(sunVFromProxima(), 2) + " from Proxima), in Cassiopeia (from Willmer 2018's M_V 4.81)"};
        return;
    default: return;
    }
}
}  // namespace orbit::acen
