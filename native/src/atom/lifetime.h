// How long a state lives and how likely each jump is (0.10.8 stage 3), from the NIST Atomic Spectra Database (asd.h). No drawing here, so it can be tested.
//
// Where every number comes from: ASD's transition probability A (the Einstein A coefficient: the chance per second that the upper level makes that one jump to the lower level on its own) for each line, with the
// accuracy code ASD gives it (read from ASD's lines help page, 2026-10-04; the table is in accMeaning) and ASD's transition type (blank = allowed electric dipole E1; M1, E2 ... = forbidden). The lab adds only
//   lifetime  tau = 1 / (sum of A over every allowed way down)        (the average time before the state jumps down by itself; the definition of the mean life of a decaying state)
//   branching ratio of one way = A / (sum of A)                        (the share of all the jumps that go that way)
// No rate is calculated: where ASD has no A for a jump the lab offers, it says so and shows no number (it does not fill the gap with a hydrogenic formula).
//
// "Every allowed way down" means: every electric-dipole jump (the rule of dirac.h's diracAllowed: l changes by 1 and j by 0 or 1, which for one electron is ASD's "opposite parity, J changes by at most 1") to a level of
// LOWER n. ASD's data kept here has lines between levels n <= 6 within 0.0001 nm to 10,000 nm, so jumps between levels of the same n (radio waves, centimetres) are not in it and are not counted; the card says so.
// A level counts as having a complete set when ASD has an A value for every one of these ways; then (and only then) the lab gives its lifetime. When some way has no A the sum is only part of the total: the lab then gives
// only an upper limit on the lifetime, and says so.
//
// The levels are those of a hydrogen-like ion of nuclear charge Z (the ion the lab models, ASD's "H I", "He II", ... "Fe XXVI"), with the fine structure resolved: (n, l, j). The lab's orbitals have no j, so the card shows
// every j of the shell on screen.
#pragma once

#include <string>
#include <vector>

#include "asd.h"

namespace atom {

struct Level { int n, l, j2; };                         // principal number, orbital quantum number, 2 j (j2 = 2 l + 1 or 2 l - 1, at least 1)
bool levelExists(const Level& L);
std::string shellName(int n, int l);                    // "2p"
std::string levelName(const Level& L);                  // "2p 3/2"

enum class LifeStatus { Data, NoData };                 // NoData: ASD has no hydrogen-like ion for this Z (Z = 111 .. 118)
struct Way { Level to; bool haveA; double A; std::string acc; };    // one allowed jump down: ASD's A in 1/s (when haveA) and its accuracy code ("" when ASD gives none)
struct Life {
    LifeStatus status = LifeStatus::Data;
    Level level{1, 0, 1};
    std::vector<Way> ways;      // every allowed jump to a lower n, sorted by n, l, j of the lower level
    int withA = 0;              // how many of them ASD gives an A for
    double sumA = 0;            // the sum of those A, 1/s
    bool complete = false;      // ASD has an A for every way (and there is at least one way)
    bool ground = false;        // n = 1: the lowest level, nothing below it
    bool metastable = false;    // n > 1 and no allowed way down at all (2s)
    double tauS = 0;            // 1 / sumA when complete, else NaN
    double tauMaxS = 0;         // when some ways have no A but others do: 1 / sumA, an UPPER limit on the lifetime; else NaN
};
Life lifeOf(int Z, const Level& L);

// One jump from the upper level to the lower one, and what the rules and ASD say about it.
struct Jump {
    Level upper, lower;
    bool allowed = false;           // allowed electric dipole by the rules
    std::vector<std::string> why;   // when not allowed: every rule that forbids it, in words (parity, l, j)
    bool ionData = true;            // ASD has this ion
    bool haveA = false;             // ASD gives an A for this jump (allowed or forbidden type)
    double A = 0;                   // 1/s
    std::string acc;                // ASD's accuracy code ("" none)
    std::string asdType;            // ASD's type for this line when it is listed as forbidden ("M1", "E2", "M1+E2", "UT"); "" otherwise
    bool asdListed = false;         // ASD has a line for these two levels
    double share = 0;               // A / sum of A of the ways ASD gives for the upper level (valid when allowed and haveA)
    bool shareComplete = false;     // that sum is the whole sum (every way has an A)
};
// Every pair of fine-structure levels of two shells: (nUp, lUp) and (nLow, lLow), the one with the higher n is the upper. Empty when the two have the same n (ASD keeps no such line here).
std::vector<Jump> jumpsBetween(int Z, int nUp, int lUp, int nLow, int lLow);

// ---- the words (shown by the panel, checked by the tests) ----
std::string accMeaning(const std::string& code);        // "AAA: uncertainty at most 0.3 %" (ASD's table); "" -> "no rating given by ASD"
std::string lifeText(double seconds);                   // "1.596 ns": the unit (as, fs, ps, ns, µs, ms, s) chosen automatically
std::string rateText(double A);                         // "6.265e+8 s⁻¹"
std::string lifeExplain();                              // what a lifetime, A and a branching ratio are, in plain words, and where they come from
std::string lifeLine(int Z, const Life& L);             // one state: its lifetime (or why there is none) and ASD's accuracy
std::string jumpLine(const Jump& J);                    // one jump: allowed with A, accuracy and share; or not allowed and why; or allowed with no ASD value
std::string metastableText(int Z);                      // 2s: what the lab can and cannot say
extern const char* const kLifeNoEqualN;                 // the note that jumps between levels of one n are not counted

}  // namespace atom
