// The REAL measured spectrum next to the lab's (0.10.7 stage 2): lines from the NIST Atomic Spectra Database (ASD), baked in at build time (src/atom/asd_data.inc, made by
// tools/make_asd_data.py from data/asd; the README there has the exact query, the date and NIST's citation). No drawing here, so it can be tested.
//
// Two different things are kept apart everywhere:
//   (a) the ION the lab models: one electron round a nucleus of charge Z (H I, He II, ..., Fe XXVI). The lab's lines should land almost on these; what is left is mostly fine
//       structure and relativity, which the lab leaves out. This comparison VALIDATES the lab.
//   (b) the NEUTRAL ATOM of the same element (H I, He I, ... Fe I), which is what "the spectrum of iron" means. For anything past hydrogen it has Z electrons and looks nothing like the
//       lab's one-electron lines. This comparison shows the LIMIT of the lab.
// Wavelengths are VACUUM wavelengths in nm (as the lab's). A line's wavelength is ASD's observed one when it gives it, else its Ritz one (calculated from measured energy levels).
#pragma once

#include <cmath>
#include <string>
#include <vector>

namespace atom {

struct AsdLine {
    double nm;                 // vacuum wavelength, nm
    int observed;              // 1: ASD's observed wavelength; 0: its Ritz (calculated) wavelength
    double unc;                // ASD's uncertainty of that wavelength, nm (NaN: ASD gives none)
    double ritz;               // ASD's Ritz wavelength (calculated from the measured energy levels), nm (NaN when ASD gives none): the lab is compared with this one, which separates the fine-structure components
    const char* intens;        // ASD's relative intensity exactly as written ("" when ASD gives none: then no brightness is shown or implied)
    int ni, nk;                // principal quantum numbers of the lower and the upper level (0 when the configuration has no leading number)
    int resolved;              // 1: ASD gives J of both levels (a fine-structure component); 0: an unresolved line of the whole n -> n' group
    const char* transition;    // "1s 2S 1/2 - 2p 2P* 3/2": configuration, term and J of the lower level, then of the upper one; just "-" when ASD gives no classification for the line
    // 0.10.8 stage 3 (no refetch: already in data/asd/asd_lines.tsv):
    double aki;                // ASD's transition probability A of the upper level to the lower one, 1/s (NaN: ASD gives none for this line)
    const char* acc;           // ASD's accuracy code for it as written: "AAA" (<= 0.3 %), "AA" (<= 1 %), "A+", "A", "B+", ..., "E" (> 50 %); a prime ' means a multiplet was split with pure LS coupling; "" when ASD gives none
    const char* type;          // ASD's transition type: "" = allowed electric dipole (E1); "M1", "E2", "M1+E2", "UT" = forbidden (UT: unspecified)
};
struct AsdSet {
    const char* spectrum;      // ASD's name: "He II", "Fe I"
    int status;                // 0 lines kept; 1 ASD has no such spectrum; 2 ASD has it but no line in the range (or none that passes the selection)
    int returned;              // lines ASD returned in the asked range, before the selection
    int first, count;          // the kept lines: kAsdLines[first .. first + count)
};
enum class AsdStatus { Lines = 0, NoSpectrum = 1, NoLines = 2 };
struct AsdSpectrum { const char* name; AsdStatus status; int returned; const AsdLine* lines; int count; };

const AsdSpectrum& asdIon(int Z);          // (a) the hydrogen-like ion of element Z (1..118)
const AsdSpectrum& asdAtom(int Z);         // (b) the neutral atom of element Z
double asdIntensity(const AsdLine& l);     // the number in ASD's intensity text (brackets before it ignored), NaN when ASD gives none (letters after it are ASD's own flags, kept in the text)
int asdTotalLines();                       // all kept lines
extern const char* const kAsdSelectIon;    // the selection, in words (also in the README and in About)
extern const char* const kAsdSelectAtom;
extern const double kAsdIonLowNm, kAsdIonHighNm, kAsdAtomLowNm, kAsdAtomHighNm;   // the wavelength ranges asked for

// (a): the lab's line for each transition between levels n <= 6 against the measured components of it
struct IonCompare {
    int lo, hi;                // the lab's transition: n = hi -> lo
    double labNm;              // the lab's wavelength (specLines)
    double minNm, maxNm;       // the shortest and longest ASD component of that transition
    int components;            // how many ASD lines (fine-structure components) there are
    double offsetNm;           // 0 when the lab's value lies between the components; else lab - nearest end (positive: the lab's line is longer than every component)
};
std::vector<IonCompare> compareIon(int Z, int N);

// ---- with the Dirac levels on (0.10.8 stage 1, dirac.h): the lab's lines split by j, so they are compared with ASD's fine-structure components one by one ----
// A line of ASD's transition text, "1s 2S 1/2 - 2p 2P* 3/2", read as numbers: n, l and 2j of the lower and the upper level. False when ASD gives no classification, an unresolved group ("1s 2S 1/2 - 6")
// or a J that is not a half-integer.
struct AsdTransition { int nLo, lLo, jLo2, nHi, lHi, jHi2; };
bool asdParseTransition(const char* text, AsdTransition& out);
// One transition n = hi -> lo between levels n <= 6: the lab's Dirac lines against the ASD electric-dipole components (l changes by 1) that have the same n and j on both sides. Every ASD component is
// compared with the lab's line of the SAME j pair (a component is matched by its quantum numbers, not by being the nearest line).
struct FineCompare {
    int lo, hi;
    int labLines;               // the lab's Dirac lines of this transition (lines of equal energy count once)
    int asdComponents;          // ASD components matched to a lab line
    int asdUnmatched;           // ASD components with no lab line of the same j pair (0 for every ion of the data: the rule leaves out no line ASD has)
    double labMinNm, labMaxNm;  // the lab's Dirac lines, shortest and longest
    double asdMinNm, asdMaxNm;  // the matched ASD components (Ritz wavelength where there is one, else the observed one), shortest and longest
    double worstDiracRel;       // the largest |lab - ASD| / ASD over the matched components, with its sign (positive: the lab's line is longer)
    double worstPlainRel;       // the same for the lab WITHOUT the Dirac levels (its one line per transition) against the same components
};
std::vector<FineCompare> compareIonDirac(int Z, int N);
std::string asdFineRow(const FineCompare& c);          // one row of text for the card
std::string asdIonNoteDirac(int Z);                    // the note of block (a) when the Dirac levels are on

// the words of the card (used by the panel and checked by the tests)
std::string asdIonTitle(int Z);            // "(a) The ion the lab models: He⁺ (NIST: He II)"
std::string asdIonNote(int Z);
std::string asdAtomTitle(int Z);           // "(b) The neutral atom of the element: Fe I"
std::string asdAtomNote(int Z);
std::string asdMissingText(const AsdSpectrum& s);          // what to say when there is no line to draw
extern const char* const kAsdMeasuredNote;                 // measured values with their own uncertainties; intensities relative
extern const char* const kAsdCredit;                       // the citation NIST asks for

}  // namespace atom
