#include "asd.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <limits>

#include "atom_core.h"
#include "dirac.h"

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

bool asdParseTransition(const char* text, AsdTransition& out) {
    const std::string s(text ? text : "");
    const std::size_t d = s.find(" - ");
    if (d == std::string::npos) return false;
    auto side = [](const std::string& t, int& n, int& l, int& j2) {              // "2p 2P* 3/2": configuration, term, J
        const std::size_t a = t.find(' '), b = a == std::string::npos ? a : t.find(' ', a + 1);
        if (a == std::string::npos || b == std::string::npos) return false;
        const std::string conf = t.substr(0, a), J = t.substr(b + 1);
        std::size_t i = 0;
        while (i < conf.size() && std::isdigit(static_cast<unsigned char>(conf[i]))) i++;
        if (i == 0 || i + 1 != conf.size()) return false;                         // a leading n and ONE letter (a configuration such as 2p; not "2p2" or "6")
        const std::size_t p = std::string("spdfghiklm").find(conf[i]);
        const std::size_t sl = J.find("/2");
        if (p == std::string::npos || sl == std::string::npos || sl + 2 != J.size() || sl == 0) return false;
        for (std::size_t k = 0; k < sl; k++) if (!std::isdigit(static_cast<unsigned char>(J[k]))) return false;
        n = std::atoi(conf.c_str()); l = int(p); j2 = std::atoi(J.c_str());
        return true;
    };
    return side(s.substr(0, d), out.nLo, out.lLo, out.jLo2) && side(s.substr(d + 3), out.nHi, out.lHi, out.jHi2);
}

std::vector<FineCompare> compareIonDirac(int Z, int N) {
    std::vector<FineCompare> out;
    const AsdSpectrum& sp = asdIon(Z);
    const std::vector<DiracLine> lines = diracLines(Z, N);
    const std::vector<SpecLine> plain = specLines(Z, N);
    for (int lo = 1; lo <= 5; lo++)
        for (int hi = lo + 1; hi <= 6; hi++) {
            FineCompare c{lo, hi, 0, 0, 0, 1e300, 0, 1e300, 0, 0, 0};
            for (const DiracLine& l : lines) if (l.lo == lo && l.hi == hi) { c.labLines++; c.labMinNm = std::min(c.labMinNm, l.lam); c.labMaxNm = std::max(c.labMaxNm, l.lam); }
            double plainNm = 0;
            for (const SpecLine& l : plain) if (l.lo == lo && l.hi == hi) plainNm = l.lam;
            for (int i = 0; i < sp.count; i++) {
                const AsdLine& a = sp.lines[i];
                AsdTransition t;
                if (!a.resolved || !asdParseTransition(a.transition, t) || t.nLo != lo || t.nHi != hi || std::abs(t.lLo - t.lHi) != 1) continue;
                const double w = std::isnan(a.ritz) ? a.nm : a.ritz;
                const DiracLine* m = nullptr;
                for (const DiracLine& l : lines) if (l.lo == lo && l.hi == hi && l.jLo2 == t.jLo2 && l.jHi2 == t.jHi2) m = &l;
                if (!m) { c.asdUnmatched++; continue; }
                c.asdComponents++; c.asdMinNm = std::min(c.asdMinNm, w); c.asdMaxNm = std::max(c.asdMaxNm, w);
                const double rd = (m->lam - w) / w, rp = (plainNm - w) / w;
                if (std::fabs(rd) > std::fabs(c.worstDiracRel)) c.worstDiracRel = rd;
                if (std::fabs(rp) > std::fabs(c.worstPlainRel)) c.worstPlainRel = rp;
            }
            if (c.asdComponents + c.asdUnmatched > 0 && c.labLines > 0) out.push_back(c);
        }
    return out;
}

std::string asdFineRow(const FineCompare& c) {
    auto pct = [](double r) { return jsNum(std::fabs(r) * 100, 2) + " % " + (r > 0 ? "longer" : "shorter"); };
    return "n = " + std::to_string(c.hi) + " → " + std::to_string(c.lo) + ": the lab has " + std::to_string(c.labLines) + " Dirac line" + (c.labLines > 1 ? "s" : "") + " (" + jsNum(c.labMinNm, 7) + (c.labLines > 1 ? " to " + jsNum(c.labMaxNm, 7) : "") +
           " nm), NIST has " + std::to_string(c.asdComponents) + " component" + (c.asdComponents > 1 ? "s" : "") + " (" + jsNum(c.asdMinNm, 7) + (c.asdMinNm != c.asdMaxNm ? " to " + jsNum(c.asdMaxNm, 7) : "") +
           " nm). Worst miss with Dirac: " + pct(c.worstDiracRel) + "; without it: " + pct(c.worstPlainRel) + ".";
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
std::string asdIonNoteDirac(int Z) {
    return "One electron round a nucleus of charge " + std::to_string(Z) + ": the lab's own model, here with the Dirac levels. Top row: the lab's lines, split by j; bottom row: NIST's lines for this ion. They should nearly coincide, much better than without "
           "the Dirac levels. What is left is the Lamb shift and the other QED effects and the size of the nucleus, which the lab leaves out (one electron, point nucleus), and it grows as Z gets bigger. This comparison checks the lab.";
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
