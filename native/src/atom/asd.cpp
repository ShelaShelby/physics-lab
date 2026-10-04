#include "asd.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <limits>

#include "atom_core.h"

namespace atom {

namespace {
#include "asd_data.inc"      // kAsdLines, kAsdSets, kAsdSelIon, kAsdSelAtom, kAsdRanges (generated, see the header)
const AsdSpectrum& spectrum(int Z, int kind) {
    static const AsdSpectrum none = {"", AsdStatus::NoSpectrum, 0, nullptr, 0};
    static AsdSpectrum cache[118][2];
    static bool built = false;
    if (!built) {
        for (int z = 0; z < 118; z++)
            for (int k = 0; k < 2; k++) {
                const AsdSet& s = kAsdSets[z][k];
                cache[z][k] = {s.spectrum, static_cast<AsdStatus>(s.status), s.returned, s.count ? kAsdLines + s.first : nullptr, s.count};
            }
        built = true;
    }
    return Z >= 1 && Z <= 118 ? cache[Z - 1][kind] : none;
}
}  // namespace

const double kAsdIonLowNm = kAsdRanges[0], kAsdIonHighNm = kAsdRanges[1], kAsdAtomLowNm = kAsdRanges[2], kAsdAtomHighNm = kAsdRanges[3];
const char* const kAsdSelectIon = kAsdSelIon;
const char* const kAsdSelectAtom = kAsdSelAtom;

const AsdSpectrum& asdIon(int Z) { return spectrum(Z, 0); }
const AsdSpectrum& asdAtom(int Z) { return spectrum(Z, 1); }
int asdTotalLines() { return int(sizeof(kAsdLines) / sizeof(kAsdLines[0])); }

double asdIntensity(const AsdLine& l) {
    const char* p = l.intens;
    while (*p == ' ' || *p == '(' || *p == '[') p++;                   // ASD sometimes writes the number in brackets, e.g. "(51000)"; the text itself is kept as it was
    char* end = nullptr;
    const double v = std::strtod(p, &end);
    return end == p ? std::numeric_limits<double>::quiet_NaN() : v;
}

std::vector<IonCompare> compareIon(int Z, int N) {
    std::vector<IonCompare> out;
    const AsdSpectrum& sp = asdIon(Z);
    for (const SpecLine& s : specLines(Z, N)) {
        if (s.hi == 0 || s.hi > 6) continue;
        IonCompare c{s.lo, s.hi, s.lam, 1e300, 0, 0, 0};
        for (int i = 0; i < sp.count; i++) {
            const AsdLine& l = sp.lines[i];
            if (!l.resolved || l.ni != s.lo || l.nk != s.hi) continue;
            const double w = std::isnan(l.ritz) ? l.nm : l.ritz;          // the Ritz wavelength separates the components; ASD's observed one often does not
            c.minNm = std::min(c.minNm, w); c.maxNm = std::max(c.maxNm, w); c.components++;
        }
        if (!c.components) continue;
        c.offsetNm = c.labNm < c.minNm ? c.labNm - c.minNm : c.labNm > c.maxNm ? c.labNm - c.maxNm : 0;
        out.push_back(c);
    }
    return out;
}

const char* const kAsdMeasuredNote =
    "NIST lines are wavelengths measured in a laboratory (observed) or worked out from measured energy levels (Ritz), each with its own uncertainty; all are vacuum wavelengths. "
    "Where NIST gives an intensity it is only a qualitative relative value: it has meaning within one spectrum, not between spectra, and it depends on the light source; a line NIST gives no intensity for is drawn short and equal, with no brightness meant. "
    "Only a selection is stored in the app, not every line NIST has.";
const char* const kAsdCredit =
    "Data: NIST Atomic Spectra Database, version 5.12 (the newest listed on ASD's version history page when it was read): Kramida, A., Ralchenko, Yu., Reader, J. and NIST ASD Team (2024), https://physics.nist.gov/asd, "
    "National Institute of Standards and Technology, Gaithersburg, MD, DOI https://doi.org/10.18434/T4W30F. Lines fetched 2026-10-04.";

std::string asdIonTitle(int Z) { return "(a) The ion the lab models: " + ionLabel(Z) + " (NIST: " + asdIon(Z).name + ")"; }
std::string asdIonNote(int Z) {
    return "One electron round a nucleus of charge " + std::to_string(Z) + ": the lab's own model. Top row: the lab's lines; bottom row: NIST's lines for this ion. They should nearly coincide. What is left is mostly fine structure "
           "and relativity, which the lab leaves out (one electron, non-relativistic, point nucleus), and it grows as Z gets bigger. This comparison checks the lab.";
}
std::string asdAtomTitle(int Z) { return "(b) The neutral atom of the element: " + std::string(asdAtom(Z).name); }
std::string asdAtomNote(int Z) {
    if (Z == 1) return "Hydrogen's neutral atom has one electron, so it is the same system as (a): here the two comparisons are the same physics (the lines shown in (b) are NIST's strongest observed ones).";
    return "This is what \"the spectrum of " + std::string(name(Z)) + "\" means: the neutral atom, with " + std::to_string(Z) + " electrons. The lab has ONE electron, so its lines are not expected to match these. "
           "This comparison shows the limit of the lab; it is not an error in either.";
}
std::string asdMissingText(const AsdSpectrum& s) {
    if (s.status == AsdStatus::NoSpectrum) return std::string("NIST ASD has no data for ") + s.name + ".";
    if (s.status == AsdStatus::NoLines) return std::string("NIST ASD has ") + s.name + ", but no line in the range that passes the selection, so none is drawn.";
    return "";
}

}  // namespace atom
