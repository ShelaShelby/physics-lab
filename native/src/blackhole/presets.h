// Black Hole Lab (0.54.3): the two real holes of the presets, their measured mass and distance, and the sizes that follow. Owner's decision (15 Nov 2026):
// presets set mass and distance only; spin starts at 0 ("spin not measured").
//
// Sagittarius A*: GRAVITY Collaboration, A&A 657, L12 (2022), arXiv:2112.07478, Table B.1 ("this paper": M = 4.297 +- 0.012 x 10^6 M_sun, R0 = 8277 +- 9 pc;
//   the same in the abstract and section 4, statistical errors). The Event Horizon Telescope's Sagittarius A* paper I (ApJL 930, L12 (2022), arXiv:2311.08680)
//   Table 1 quotes these as its "Previous Estimate" (VLTI) with a systematic part added: 8277 +- 9 +- 33 pc, (4.297 +- 0.013) x 10^6 M_sun, theta_g = 5.125
//   +- 0.009 +- 0.020 microarcsec, and its measured ring: diameter 51.8 +- 2.3 microarcsec.
// M87*: Event Horizon Telescope, ApJL 875, L1 (2019), arXiv:1906.11238, Table 1: M = (6.5 +- 0.7) x 10^9 M_sun, D = (16.8 +- 0.8) Mpc ("Prior Estimate"),
//   theta_g = GM/(D c^2) = 3.8 +- 0.4 microarcsec, ring diameter 42 +- 3 microarcsec.
// Derived here: r_g = G M / c^2 with the Sun's GM of JPL DE440 and c (orbit/units.h); the horizon of the non-spinning hole r+ = 2 r_g (BPT (2.6), a = 0);
// the shadow's angular diameter from Earth 2 alpha_sh(D / r_g) (Synge, Perlick & Tsupko (7); at these distances 2 sqrt(27) r_g / D to 1e-10).
// The ring is not the shadow's edge ("does not necessarily coincide", M87 paper I section 6): the app shows the two side by side, not as a test.
// The parsec: the distance at which 1 au subtends one second of arc, 648000/pi au (the app's sky_data.inc uses 1 pc = 3.26156 ly, the same).
// Where else these numbers live: src/orbit/sky_data.inc (Sagittarius A* at 26996 ly = 8277 pc) and src/orbit/far_masses.h (4.30 million Suns, rounded);
// tests/test_blackhole.cpp checks that they agree with the values here.
#pragma once

namespace blackhole {

struct Preset {
    const char* key;
    const char* name;
    double massSun, massErr;          // solar masses
    double distPc, distErr;           // parsec
    double ringMuas, ringErr;         // the measured ring's diameter, microarcsec (1.3 mm)
    double thetaGMuas, thetaGErr;     // the published GM/(D c^2), microarcsec
    const char* source;
};
const Preset* presets(int& count);
const Preset* preset(const char* key);   // "sgra", "m87"; nullptr if unknown

double gravRadiusM(double massSun);       // r_g = G M / c^2, metres
double parsecM();                         // metres
double horizonKm(const Preset& p);        // 2 r_g (a = 0)
double horizonAU(const Preset& p);
double thetaGMuas(const Preset& p);       // r_g / D, microarcsec
double shadowDiameterMuas(const Preset& p);   // 2 alpha_sh seen from Earth, microarcsec

}  // namespace blackhole
