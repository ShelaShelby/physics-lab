// 0.73.0: the Alpha Centauri system in the Orbit Lab (native/PLAN_ALPHACEN.md, the spec; every number re-read in the saved paper, sources repo
// data/alphacen/, native/data/alphacen/README.txt). The positions only: the drawing is orbit/orbit_view.cpp (alphacen_view.inc), the cards are
// orbit/alphacen.cpp acenCard.
//
// THE SCENE: the whole system "now" (no light-travel time: what we see left it 4.3 years ago, and light needs up to 0.19 years from Proxima to A-B; neither
// is applied, the cards say so). Origin: the centre of mass of A and B. Axes (a right-handed frame, metres): x towards the WEST on our sky, y towards the
// north, z towards the Sun. Seen from +z (from the Sun's side), x is right and y is up: the 2D view shows the system as we see it from Earth, north up and
// east to the left. (PLAN_ALPHACEN.md sect. 11 wrote "x East": that frame is left-handed; this is its right-handed form, z still towards the Sun.)
// Time: the lab's clock in TDB (SolarSystem::julianDayTDB). A21's epochs are decimal Julian years, N25's BJD (TDB): the lab's TDB differs from them by
// the light-time across the Solar System (at most 8.3 minutes), far below their stated errors (2.88 h, 3.8 h).
#pragma once

#include <string>
#include <vector>

#include "kepler.h"
#include "vec3.h"

namespace orbit {
struct Card;
namespace acen {

// ---- the things in the scene (Selection::ACen's index) ----
enum Thing { Bary = 0, StarA, StarB, Proxima, PlanetB, PlanetD, PlanetC, CandidateA, Sun, ThingCount };
const char* thingName(int k);                  // "Alpha Centauri A", ..., "Proxima c (disputed)", "Candidate around A (unconfirmed)", "Sun"

// ---- the A-B orbit: Akeson et al. 2021 (A21), Table 8, "present work", epoch 2019.5 (B about A; omega is B's argument of periastron) ----
constexpr double kAbAArcsec = 17.4930, kAbE = 0.51947, kAbIDeg = 79.2430, kAbNodeDeg = 205.073, kAbPeriDeg = 231.519, kAbT0Year = 1955.564;
constexpr double kAbPYear = 79.762;            // the published period (Kepler's law with A21's a, parallax and masses gives it back: tests)
constexpr double kAbPlxMas = 750.81;           // A21 Table 8: the orbital parallax 750.81 +- 0.38 mas (1.3319 pc, 4.3441 ly)
constexpr double kMassA = 1.0788, kMassB = 0.9092;   // M_sun, A21 Table 8 (+- 0.0029, +- 0.0025)
constexpr double kMassProxima = 0.1221;        // M_sun, Kervella, Thevenin & Lovis 2017 Table 1 (Mann et al. 2015's mass-M_Ks relation); the planet papers use it
double abSemiMajorAU();                        // a" / parallax: 23.299 au
double abGM();                                 // G (mA + mB), m^3/s^2 (GM_sun x the masses)
double abPeriodYears();                        // from Kepler's law with abGM and a: 79.76 yr
double yearOfJd(double jdTDB);                 // decimal Julian year: 2000 + (JD - 2451545) / 365.25
Vec3 bAboutA(double jdTDB);                    // B - A, scene frame, m

struct Stars { Vec3 A, B, P, vA, vB, vP; };   // A, B and Proxima about the A-B centre of mass (m, m/s)
Stars stars(double jdTDB);

// an ellipse to draw: F the focus (m), P towards its periastron, Q in its plane (unit vectors), a, e, b (m); at(E) its point at eccentric anomaly E
struct Ellipse {
    Vec3 F, P, Q;
    double a = 0, e = 0, b = 0, periodYears = 0;
    Vec3 at(double E) const;
};
Ellipse orbitOfA();                            // A's and B's own ellipses about the centre of mass
Ellipse orbitOfB();

// ---- Proxima about A-B: A21 Table 10 (a 8.2 kau, e 0.497, i 124.9, Omega 165, omega 151 deg, periastron +278 kyr from "present", taken as A21's
// epoch 2019.5). The angles are in GALACTIC Cartesian axes (X towards the galactic centre, Z the north galactic pole; Kervella et al. 2017 sect. 3.2
// computed their Table 3 so, and A21's elements put Proxima 301 au from its measured place in those axes, 12,466 au in the sky's: tests). Proxima is
// put at its measured place (A21 Table 9's barycentre at 2016.0, Gaia DR3's Proxima as Libralato et al. 2025 Table 2, both parallaxes: 12,052 au) and
// moved on A21's orbit (two-body, GM of all three stars): the drawn ellipse is A21's moved by those 301 au, so it passes through Proxima.
double proximaGM();
State proximaState(double jdTDB);              // about the A-B centre of mass
Ellipse proximaOrbit();
double proximaOrbitMissAU();                   // how far A21's orbit puts Proxima from its measured place (au): the shift above

// ---- Proxima's planets (k: 0 b, 1 d, 2 c): circles about Proxima (e fixed 0 in the fits). b, d: Suarez Mascareno et al. 2025 (NIRPS, N25) Table 3;
// c (DISPUTED): Damasso et al. 2020 as Kervella et al. 2020 Tables 1-2 quote it. THE TILT IS NOT MEASURED: drawn in the star's equator plane (i = 47 deg,
// Klein et al. 2021; owner's choice 22 Nov 2026), the node on our sky's east-west line (assumed); the phase from T0, the inferior conjunction (N25 eq. 12).
struct PlanetData { const char* name; double periodDays, aAU, msiniEarth, msiniErr, t0Bjd; double mass47, mass47Err; double fluxEarth, teqK; const char* status; };
const PlanetData& planet(int k);
constexpr double kPlanetIncDeg = 47, kPlanetNodeDeg = 90;   // assumed (above)
Vec3 planetOffset(int k, double jdTDB);        // from Proxima, scene frame, m
Ellipse planetOrbit(int k, const Vec3& proxima);
// K = (2 pi G / P)^(1/3) m sin i / (M + m)^(2/3), m/s (P in s, masses in M_sun): a circular two-body orbit (PLAN_ALPHACEN.md sect. 12, test 3)
double rvSemiAmplitude(double periodS, double msiniSun, double mStarSun);
double keplerAAU(double periodS, double mTotalSun);  // Kepler's third law, au

// ---- the candidate around A (UNCONFIRMED, seen once by JWST in Aug 2024): Beichman et al. 2025 Table 4, its orbit families a = 1.58-2.23 au. Drawn as a
// faint dashed band about A in the plane of our sky (its orbit's tilt is not known; the card gives the two sightings, B25 Table 2).
constexpr double kCandInnerAU = 1.58, kCandOuterAU = 2.23;

// the three published A-B orbits side by side (the Data panel's table; the card names the disagreement): A21 Table 8 "present work", K16 as A21 Table 8
// quotes it (K16 Table 1), PB16 Table 1 (their own fit; A21 and K16 quote its Omega as 204.85, PB16's 2002 column)
struct AbSolution { const char* name; double aArcsec, aErr, e, iDeg, nodeDeg, periDeg, pYear, pErr, t0Year, plxMas, plxErr, mA, mAErr, mB, mBErr; };
const AbSolution* abSolutions(int& n);

// ---- the stars' facts (the cards) ----
struct StarFacts {
    const char* name; const char* iau; const char* type; const char* typeSrc;
    double mass, massErr; const char* massSrc;
    double radius, radiusErr; const char* radiusSrc;
    double teff, teffErr; const char* teffSrc;
    double lum, lumErr; const char* lumSrc;
    double vMag, bv;                           // V and B-V seen from Earth (HYG v4.1 rows; Proxima's V: Jao et al. 2014 as N25 Table 2 quotes it)
};
const StarFacts& starFacts(int k);             // 0 A, 1 B, 2 Proxima
// the habitable zone (Kopparapu et al. 2014, runaway to maximum greenhouse, sun_facts.h hzEdgeAU) of star k, au
void habitableZoneAU(int k, double& inner, double& outer);

// ---- the sky from Alpha Centauri ----
// the turn from the lab's ecliptic (J2000, obliquity 84381.448") into the scene's frame (rows: west, north, towards the Sun at the A-B barycentre's direction)
void eclipticToScene(double M[9]);
Vec3 equatorialToScene(const Vec3& v);
Vec3 sceneToEquatorial(const Vec3& v);
double sunVFromAB();                           // our Sun's V seen from A-B: M_V 4.81 (Willmer 2018 Table 3) at A21's distance: 0.43
double sunVFromProxima();                      // the same at Gaia DR3's distance of Proxima: 0.38
Vec3 sunDirectionEquatorial();                 // the Sun's direction in Alpha Centauri's sky (the sky's own Sun star), ICRS unit vector

// the night sky seen from Alpha Centauri, in the scene's frame (orbit/sky.h Sky: the same stars, lines and names): made once
}  // namespace acen
struct Sky;
namespace acen {
const Sky& acenSky();

// ---- the card of a thing (title and lines, as the Solar System's cards) ----
void acenCard(int k, double jdTDB, Card& out);

}  // namespace acen
}  // namespace orbit
