#pragma once
// 0.47.3 (PLAN_JUMPS.md part 2): jumps between the LEVELS of an atom, for atoms with more than one outer electron (0.47.3: He, Be, Mg, Zn, Cd with clouds; the level tables also
// hold Ca, Sr, Ba, Ra, Hg, Yb, whose clouds come in 0.47.4: two s electrons outside closed shells), and the jump status of all 118 atoms (owner decisions D1-D7, 9 Nov 2026, PLAN_JUMPS.md section 11).
//
//  - Levels and lines: NIST ASD as saved in data/jumps/asd (tools/make_level_data.py -> level_data.inc). A level is named configuration -> term -> J, as textbooks and
//    Grotrian diagrams do. Energies and wavelengths shown are NIST's only. Helium's NIST energies are calculated (Drake's theory, bracketed in ASD), matching the measured lines.
//  - D1: a level is "measured" when an observed classified line touches it; a jump is offered only between two measured levels.
//  - D2: the levels with a cloud: the ground level, both levels of each of the 30 strongest A-known lines, the lowest level of each of the 10 lowest configurations;
//    a D2 level outside the two-electron model (a hole in a closed shell) or above the ionization limit is listed with its NIST energy and "no cloud computed" and the reason.
//  - D4: a line's A is ASD's with its accuracy grade; where ASD has none, this app's own GRASP2018 value (rtransition, tools/grasp/rates_two.py: the line strength of the
//    computed levels in the length (Babushkin) form, turned into A with NIST's wavelength), marked "computed by this app (GRASP2018), not measured" (0.47.4); else "rate not known".
//  - The clouds: GRASP2018 MCDHF + RCI per level (tools/grasp/run_two.py, multipoles_two.py; data/jumps/grasp/<X>/), drawn as for the alkalis (alkali.h, GraspLevel).
#include <string>
#include <vector>

#include "atom/alkali.h"

namespace atom {

struct JLevel {
    std::string conf, term;        // ASD's text: "3s.3p", "3P*" (* = odd parity)
    int j2 = 0;                    // 2J
    bool odd = false;
    double cm = 0;                 // NIST energy above the ground level, cm-1
    std::string cmText;            // as ASD prints it
    bool cloud = false;            // a cloud is computed for it (D2 and the model covers it)
    bool calc = false;             // NIST's value is calculated (bracketed in ASD: helium)
    bool measured = false;         // D1: an observed line touches it
    bool d2 = false;               // in the D2 set
    std::string why;               // a D2 level without a cloud: why
};
struct JLine {
    int lo = 0, hi = 0; double obsNm = 0, ritzNm = 0, aki = 0; std::string acc;   // aki NaN: none in ASD
    int Z = 0;                          // the atom (0.47.4)
    double akiCalc = 0, gaugePct = 0;   // D4 (0.47.4): this app's GRASP2018 A (NaN: none); how far the velocity form's A is from it, %
    double nm() const; bool observed() const;
    double rate() const;                // NIST's A, else this app's computed one (NaN: neither)
};

const std::vector<JLevel>& jumpLevels(int Z);          // every named NIST level of a level atom (ground first; empty for other atoms)
const std::vector<JLine>& jumpLines(int Z);            // the E1 lines between two levels with clouds (NIST ASD)
double rateMedianPct(int Z);                           // D4: how far this app's computed A is from NIST's, median over the lines both have (%; NaN: none)
int rateCompared(int Z);                               //     and over how many lines
double ionLimitCm(int Z);                              // the first ionization limit, cm-1 (ASD's "Limit" row; NaN: none)
bool levelJumps(int Z);                                // this atom's jumps are level jumps with clouds (data embedded)
const GraspLevel* levelCloud(int Z, int i);            // the cloud of level i (nullptr: none)
std::vector<int> cloudLevelList(int Z);                // the levels with a cloud, by energy (the ground first)
std::vector<int> linesAt(int Z, int i);                // the lines (indices into jumpLines) that touch level i
std::vector<int> linesBetween(int Z, int a, int b);    // the line(s) joining levels a and b
bool jumpOffered(int Z, int line);                     // D1: both levels measured (and both have clouds)
std::string termPretty(const std::string& term);       // "3P*" -> "³P°"
std::string confPretty(const std::string& conf);       // "2p6.3s2" -> "2p⁶3s²"; "3s.3p" -> "3s3p"
std::string levelName(int Z, int i);                   // "3s3p ³P° J=1"
std::string gradeText(const std::string& acc);         // ASD's accuracy grade with its meaning: "B (within 10 %)"; "" when none
std::string rateText(const JLine& L);                  // "A = 1.2 × 10⁸ /s (NIST ASD, grade B: within 10 %)", the computed A (D4), or "rate not known (...)"

// The jump status of every atom (data/jumps/elements.tsv, PLAN_JUMPS.md sections 1 and 7)
enum class JumpStatus { Measured = 0, LevelsOnly = 1, NotInNist = 2, FewLines = 3, None = 4 };
JumpStatus jumpStatus(int Z);
std::string noJumpText(int Z);
// 0.52.0: the reason for an atom NIST has measured jumps of but whose level clouds were not computed (the element jumps are paused since 0.51.3, owner decision of
// 14 Nov 2026): names the atoms that have jumps with every electron, from the app's own data (alkaliJumps, levelJumps), and promises no more
std::string cloudsNotComputedText();
std::string levelModelNote();                          // one line for the readout: how the level clouds are computed                         // D5: the sentence for an atom with no jumps to offer ("" when NIST has measured jumps)
}  // namespace atom
