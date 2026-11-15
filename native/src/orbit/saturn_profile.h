// 0.51.2: Saturn's rings drawn from Cassini's measured profile (normal optical depth every 10 km of radius), not from four bands with the middle of a range.
// Source: Cassini UVIS stellar occultation of beta Centauri, 18 Aug 2008 (NASA PDS Ring-Moon Systems Node, COUVIS_8001,
// UVIS_HSP_2008_231_BETCEN_I_TAU10KM; Colwell et al. 2010, Icarus 205, 1); tools/make_ring_profile.py, src/orbit/saturn_profile.inc.
// What it is and is not (the app says so): one cut through the rings at one longitude in ultraviolet light (110-190 nm), used here for visible light
// (the ring particles, centimetres to metres, are far bigger than either wavelength); the rings' brightness per region still uses the NSSDC fact sheet's
// middle albedo (saturn_rings.h), and their colour is the one drawn tint. Structure finer than 10 km, the azimuthal wakes and the F ring's eccentricity are not in it.
#pragma once

#include <vector>

namespace orbit::rings {

int saturnProfileCount();
double saturnProfileKm(int i);      // ring-plane radius of bin i, km (its middle)
double saturnProfileTau(int i);     // normal optical depth of bin i (0 where the data are below 0, the maximum detectable where above it)
double saturnAlbedoAt(double rKm);  // the NSSDC fact sheet's middle albedo of the ring region at this radius (saturn_rings.h; D ring: C's; beyond A: the F ring's)

// The profile averaged over bins at least wKm wide (adjacent bins whose blocked share and light differ by under tol are merged): each bin's blocked share
// (1 - exp(-tau / mu), averaged over the 10 km bins in it) and light (albedo / 0.5 x shine(tau, mu, mu0, sunlitFace), averaged): area averages, so a
// bin drawn wide shows the same total light as the fine structure in it.
struct ProfileBin { double innerKm, outerKm, blocked, shine; };
std::vector<ProfileBin> saturnProfileBins(double wKm, double mu, double mu0, bool sunlitFace, double tol = 0.004);

// The rings' shadow on the ball: n equal bins from the C ring's inner edge to the A ring's outer edge, each with the optical depth that gives the
// bin's mean transmission along the Sun's slanted path: tau_eff = -mu0 ln(mean exp(-tau / mu0)). Out: inner, outer (planet radii), tau_eff, 3 floats a bin.
int saturnShadowBands(double mu0, double eqKm, int n, float* out);

}  // namespace orbit::rings
