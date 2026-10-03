// Published masses of the stars in the far-away table, for the small "mass" label under their names.
//
// Rules (CLAUDE.md accuracy bar): every number has a named source in the comment next to it. Where a source could not be reached, the star is LEFT OUT
// (see REPORT.md for the list). Nothing here is a guess. `how` says what kind of number it is, because they are not equally firm:
//   "orbit"    from the motion of the star (or the stars around it): a dynamical mass
//   "lensing"  from the bending of light by the star
//   "models"   worked out with stellar models from the star's light and distance: an estimate, not a measurement
// low and high are the range the source gives (value minus / plus its stated uncertainty).
//
// IMPORTANT about how these were read: arXiv, ADS and the journals' own sites were blocked in the session that wrote this file, so the numbers were read
// from search-engine excerpts of each paper's abstract or text (the paper is named so that anyone can check it). They have not been checked against
// the papers' own tables.
#pragma once

#include <cmath>
#include <cstring>
#include <string>

#include "format.h"

namespace orbit {

struct MassData {
    const char* key;      // the far-away thing it belongs to (kFar)
    const char* part;     // "" for one star, "A" / "B" for the stars of a pair
    double value, low, high;   // solar masses (Sagittarius A*: also solar masses, about 4.3 million)
    const char* how;
    const char* source;
};

static const MassData kFarMass[] = {
    // Alpha Centauri A and B: D. Pourbaix & H. M. J. Boffin, "Parallax and masses of alpha Centauri revisited" (2016), arXiv:1601.01636.
    // The abstract says "1.13 and 0.97 M_sun for A and B"; the paper's own uncertainties were not read, so low and high are only the rounding of the
    // two digits quoted (+-0.005), NOT the paper's error bar.
    {"acen", "A", 1.13, 1.125, 1.135, "orbit", "Pourbaix & Boffin 2016, arXiv:1601.01636"},
    {"acen", "B", 0.97, 0.965, 0.975, "orbit", "Pourbaix & Boffin 2016, arXiv:1601.01636"},
    // Proxima Centauri: Zurlo et al. 2018, "The gravitational mass of Proxima Centauri measured with SPHERE from a microlensing event", arXiv:1807.01318:
    // 0.150 (+0.062, -0.051) M_sun, the only direct (gravitational) measurement. (Mann et al. 2015 give 0.12 +- 0.02 from a mass-luminosity relation:
    // a model-based estimate, so it is not shown.)
    {"prox", "", 0.150, 0.150 - 0.051, 0.150 + 0.062, "lensing", "Zurlo et al. 2018, arXiv:1807.01318"},
    // Sirius A and B: H. E. Bond et al., "The Sirius System and its Astrophysical Puzzles: Hubble Space Telescope and Ground-Based Astrometry" (2017),
    // arXiv:1703.10625: dynamical masses 2.063 +- 0.023 and 1.018 +- 0.011 M_sun.
    {"sirius", "A", 2.063, 2.063 - 0.023, 2.063 + 0.023, "orbit", "Bond et al. 2017, arXiv:1703.10625"},
    {"sirius", "B", 1.018, 1.018 - 0.011, 1.018 + 0.011, "orbit", "Bond et al. 2017, arXiv:1703.10625"},
    // Procyon A and B: H. E. Bond et al., "Hubble Space Telescope Astrometry of the Procyon System" (2015), arXiv:1510.00485:
    // dynamical masses 1.478 +- 0.012 and 0.592 +- 0.006 M_sun.
    {"procyon", "A", 1.478, 1.478 - 0.012, 1.478 + 0.012, "orbit", "Bond et al. 2015, arXiv:1510.00485"},
    {"procyon", "B", 0.592, 0.592 - 0.006, 0.592 + 0.006, "orbit", "Bond et al. 2015, arXiv:1510.00485"},
    // Fomalhaut A: E. E. Mamajek, "On the Age and Binarity of Fomalhaut and its Giant Planet" (2012), ApJ 754, L20 (arXiv:1206.6353):
    // 1.92 +- 0.02 M_sun, taken from the star's place on stellar-model tracks (isochrones), so an estimate from models.
    {"fom", "", 1.92, 1.90, 1.94, "models", "Mamajek 2012, ApJ 754, L20"},
    // Arcturus: I. Ramirez & C. Allende Prieto, "Fundamental Parameters and Chemical Composition of Arcturus" (2011), ApJ 743, 135 (arXiv:1109.4425):
    // 1.08 +- 0.06 M_sun, from the star's measured gravity and stellar-evolution models: an estimate from models.
    {"arct", "", 1.08, 1.02, 1.14, "models", "Ramirez & Allende Prieto 2011, ApJ 743, 135"},
    // Sagittarius A*: GRAVITY Collaboration 2022, A&A 657, L12 (arXiv:2112.07478), the orbit of the star S2: 4.30 million M_sun, "precision about 0.25 %"
    // (read from an excerpt; the paper's separate statistical and systematic errors were not read, and the distance to the Galactic centre is a
    // main source of systematic error). low / high are 4.30 million -+ 0.25 %.
    {"sgra", "", 4.30e6, 4.30e6 * 0.9975, 4.30e6 * 1.0025, "orbit", "GRAVITY Collaboration 2022, arXiv:2112.07478"},
};
static constexpr int kFarMassCount = sizeof(kFarMass) / sizeof(kFarMass[0]);

// The text under the star's name, e.g. "Mass: A 2.06 · B 1.02 M☉" or "Mass: 0.15 (0.099 to 0.21) M☉, uncertain"; "" for a star left out.
inline std::string farMassText(const std::string& key) {
    std::string out;
    bool models = false, uncertain = false, million = false;
    for (const MassData& m : kFarMass) {
        if (key != m.key) continue;
        const bool wide = (m.high - m.low) / m.value > 0.2;      // more than +-10 %: say so
        uncertain = uncertain || wide;
        models = models || std::strcmp(m.how, "models") == 0;
        million = m.value > 1e5;
        const double scale = million ? 1e6 : 1;
        std::string v = fmt::precision(m.value / scale, million ? 2 : 3);
        if (wide) v += " (" + fmt::precision(m.low / scale, 2) + " to " + fmt::precision(m.high / scale, 2) + ")";
        if (!out.empty()) out += " · ";
        out += (*m.part ? std::string(m.part) + " " : std::string()) + v;
    }
    if (out.empty()) return out;
    return std::string("Mass: ") + (models ? "about " : "") + out + (million ? " million" : "") + " M☉" + (models ? " (from models)" : "") + (uncertain ? ", uncertain" : "");
}

}  // namespace orbit
