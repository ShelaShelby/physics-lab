#include "lifetime.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <limits>

#include "atom_core.h"
#include "dirac.h"

namespace atom {

namespace {
constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();

struct Parsed { Level lo, hi; const AsdLine* line; };
// the resolved ASD lines of the hydrogen-like ion of element Z whose two levels read as (n, l, j), parsed once
const std::vector<Parsed>& parsedLines(int Z) {
    static std::vector<Parsed> cache[119];
    static bool built[119] = {};
    static const std::vector<Parsed> none;
    if (Z < 1 || Z > 118) return none;
    if (!built[Z]) {
        const AsdSpectrum& sp = asdIon(Z);
        for (int i = 0; i < sp.count; i++) {
            AsdTransition t;
            if (!sp.lines[i].resolved || !asdParseTransition(sp.lines[i].transition, t)) continue;
            cache[Z].push_back({{t.nLo, t.lLo, t.jLo2}, {t.nHi, t.lHi, t.jHi2}, &sp.lines[i]});
        }
        built[Z] = true;
    }
    return cache[Z];
}
bool same(const Level& a, const Level& b) { return a.n == b.n && a.l == b.l && a.j2 == b.j2; }
const AsdLine* findLine(int Z, const Level& lo, const Level& hi) {
    for (const Parsed& p : parsedLines(Z)) if (same(p.lo, lo) && same(p.hi, hi)) return p.line;
    return nullptr;
}
std::string trimmed(std::string s) {                          // "12.00" -> "12", "2.200" -> "2.2", "4.000e+5" -> "4e+5"
    const std::size_t e = s.find('e');
    std::string mant = e == std::string::npos ? s : s.substr(0, e), tail = e == std::string::npos ? "" : s.substr(e);
    if (mant.find('.') != std::string::npos) { while (!mant.empty() && mant.back() == '0') mant.pop_back(); if (!mant.empty() && mant.back() == '.') mant.pop_back(); }
    return mant + tail;
}
int accRank(const std::string& code) {                        // 0 best ... 10 worst, 11 = no rating
    static const char* const order[] = {"AAA", "AA", "A+", "A", "B+", "B", "C+", "C", "D+", "D", "E"};
    std::string c = code;
    while (!c.empty() && (c.back() == '\'' || static_cast<unsigned char>(c.back()) >= 0x80)) c.pop_back();   // a prime (' or the UTF-8 prime) does not change the rank
    for (int i = 0; i < 11; i++) if (c == order[i]) return i;
    return 11;
}
}  // namespace

bool levelExists(const Level& L) { return L.n >= 1 && L.l >= 0 && L.l < L.n && L.j2 >= 1 && (L.j2 == 2 * L.l + 1 || L.j2 == 2 * L.l - 1); }
std::string shellName(int n, int l) { return std::to_string(n) + "spdfghik"[l]; }
std::string levelName(const Level& L) { return shellName(L.n, L.l) + " " + std::to_string(L.j2) + "/2"; }

Life lifeOf(int Z, const Level& L) {
    Life out;
    out.level = L; out.ground = L.n == 1; out.tauS = kNaN; out.tauMaxS = kNaN;
    if (asdIon(Z).status != AsdStatus::Lines) { out.status = LifeStatus::NoData; return out; }
    for (int n = 1; n < L.n; n++)                                    // every allowed jump to a lower n (the rule of dirac.h)
        for (int l = 0; l < n; l++)
            for (int j2 = std::max(1, 2 * l - 1); j2 <= 2 * l + 1; j2 += 2) {
                if (!levelExists({n, l, j2}) || !diracAllowed(l, j2, L.l, L.j2)) continue;
                Way w{{n, l, j2}, false, 0, ""};
                const AsdLine* a = findLine(Z, w.to, L);
                if (a && a->type[0] == '\0' && !std::isnan(a->aki)) { w.haveA = true; w.A = a->aki; w.acc = a->acc; out.withA++; out.sumA += a->aki; }
                out.ways.push_back(w);
            }
    out.complete = !out.ways.empty() && out.withA == int(out.ways.size());
    out.metastable = !out.ground && out.ways.empty();
    if (out.complete) out.tauS = 1 / out.sumA;
    else if (out.withA > 0) out.tauMaxS = 1 / out.sumA;
    return out;
}

std::vector<Jump> jumpsBetween(int Z, int nUp, int lUp, int nLow, int lLow) {
    std::vector<Jump> out;
    if (nUp < nLow) { std::swap(nUp, nLow); std::swap(lUp, lLow); }
    if (nUp == nLow || lUp < 0 || lUp >= nUp || lLow < 0 || lLow >= nLow) return out;
    const bool data = asdIon(Z).status == AsdStatus::Lines;
    for (int ju = 2 * lUp + 1; ju >= std::max(1, 2 * lUp - 1); ju -= 2)
        for (int jl = 2 * lLow + 1; jl >= std::max(1, 2 * lLow - 1); jl -= 2) {
            Jump j;
            j.upper = {nUp, lUp, ju}; j.lower = {nLow, lLow, jl}; j.ionData = data;
            j.allowed = diracAllowed(lLow, jl, lUp, ju);
            if (!j.allowed) {
                if ((lUp - lLow) % 2 == 0) j.why.push_back("the parity does not change (l goes from " + std::to_string(lUp) + " to " + std::to_string(lLow) + ", so (−1)^l is the same), and an electric-dipole jump must change it");
                if (std::abs(lUp - lLow) != 1) j.why.push_back("l must change by exactly 1 (here it changes by " + std::to_string(std::abs(lUp - lLow)) + ")");
                if (std::abs(ju - jl) > 2) j.why.push_back("j must change by 0 or 1 (here it changes by " + std::to_string(std::abs(ju - jl) / 2) + ")");
            }
            if (data) {
                const AsdLine* a = findLine(Z, j.lower, j.upper);
                if (a) {
                    j.asdListed = true;
                    j.asdType = a->type;
                    if (!std::isnan(a->aki)) { j.haveA = true; j.A = a->aki; j.acc = a->acc; }
                }
                if (j.allowed && j.haveA && j.asdType.empty()) {
                    const Life life = lifeOf(Z, j.upper);
                    if (life.sumA > 0) { j.share = j.A / life.sumA; j.shareComplete = life.complete; }
                }
            }
            out.push_back(j);
        }
    return out;
}

// ---- the words ----
std::string accMeaning(const std::string& code) {
    // ASD's table (lines help page, "Accuracy", read 2026-10-04): AAA <= 0.3 %, AA <= 1 %, A+ <= 2 %, A <= 3 %, B+ <= 7 %, B <= 10 %, C+ <= 18 %, C <= 25 %, D+ <= 40 %, D <= 50 %, E > 50 %
    static const char* const limit[] = {"0.3 %", "1 %", "2 %", "3 %", "7 %", "10 %", "18 %", "25 %", "40 %", "50 %"};
    const int r = accRank(code);
    if (r == 11) return "no rating given by ASD";
    std::string base = code;
    while (!base.empty() && (base.back() == '\'' || static_cast<unsigned char>(base.back()) >= 0x80)) base.pop_back();
    std::string t = base + ": " + (r == 10 ? std::string("uncertainty above 50 %") : std::string("uncertainty at most ") + limit[r]);
    if (base.size() != code.size()) t += "; a multiplet was split into its lines assuming pure LS coupling, which may lower the accuracy";
    return t;
}

std::string lifeText(double s) {
    struct U { double v; const char* name; };
    static const U units[] = {{1e-18, " as"}, {1e-15, " fs"}, {1e-12, " ps"}, {1e-9, " ns"}, {1e-6, " µs"}, {1e-3, " ms"}, {1, " s"}};
    int k = 0;
    if (s >= 1) k = 6; else if (s >= 1e-3) k = 5; else if (s >= 1e-6) k = 4; else if (s >= 1e-9) k = 3; else if (s >= 1e-12) k = 2; else if (s >= 1e-15) k = 1;
    return trimmed(jsNum(s / units[k].v, 4)) + units[k].name;
}
std::string rateText(double A) { return lrnSci3(A, 4) + " s⁻¹"; }         // "6.265×10⁸ s⁻¹"

std::string lifeExplain() {
    return "The lifetime of a state is the average time before it jumps down by itself (spontaneously): 1 divided by the sum of the rates A of every allowed way down. A is the rate of one jump: the chance per second that the state makes that jump. "
           "The branching ratio of a jump is its share of all the jumps from that state: its A divided by the sum. The rates are NIST ASD's own values with the accuracy rating ASD gives each; the lab only adds them up and takes the shares, and gives no number where ASD has none.";
}
const char* const kLifeNoEqualN = "Jumps between levels of the same n (the fine-structure and Lamb-shift gaps: radio waves) are not in the NIST data kept here, so they are not counted and no rate is given for them.";

std::string lifeLine(int Z, const Life& L) {
    const std::string name = levelName(L.level);
    if (L.status == LifeStatus::NoData) return "NIST ASD has no data for " + std::string(asdIon(Z).name) + ": no lifetime is given.";
    if (L.ground) return name + ": the lowest level, nothing below it: it does not jump down by itself.";
    if (L.metastable) return name + ": no allowed way down to a lower level (see the note below): the lab gives no lifetime.";
    int worst = 0; std::string worstCode;
    for (const Way& w : L.ways) if (w.haveA && accRank(w.acc) >= worst) { worst = accRank(w.acc); worstCode = w.acc; }
    const std::string ways = std::to_string(L.ways.size()) + (L.ways.size() == 1 ? " way" : " ways") + " down";
    if (L.complete) return name + ": lives " + lifeText(L.tauS) + " on average (1 / the sum of A over " + ways + "). Accuracy of ASD's rates (the worst of them): " + accMeaning(worstCode) + ". From NIST ASD.";
    if (L.withA == 0) return name + ": NIST ASD gives no A for any of its " + ways + ", so no lifetime is given.";
    return name + ": lifetime at most " + lifeText(L.tauMaxS) + " (an upper limit): NIST ASD gives A for " + std::to_string(L.withA) + " of its " + std::to_string(L.ways.size()) + " ways down, so the sum is only part of the total.";
}

std::string jumpLine(const Jump& J) {
    const std::string head = levelName(J.upper) + " → " + levelName(J.lower) + ": ";
    if (!J.ionData) return head + (J.allowed ? "allowed by the rules, but " : "not allowed, and ") + "NIST ASD has no data for this ion.";
    auto percent = [](double x) { return trimmed(jsNum(100 * x, 3)) + " %"; };
    if (J.allowed) {
        if (!J.haveA) return head + "allowed (electric dipole), but NIST ASD has no transition probability for this jump, so no rate or share is given.";
        return head + "allowed (electric dipole). A = " + rateText(J.A) + " (accuracy " + accMeaning(J.acc) + "); " + percent(J.share) +
               (J.shareComplete ? " of all the jumps down from " : " of the jumps ASD gives a rate for, from ") + levelName(J.upper) + (J.shareComplete ? "" : " (it has no rate for some of its ways, so the share is not the full one)") + ". From NIST ASD.";
    }
    std::string t = head + "NOT allowed as an electric-dipole jump: ";
    for (std::size_t i = 0; i < J.why.size(); i++) t += (i ? "; and " : "") + J.why[i];
    t += ". ";
    if (J.asdListed && !J.asdType.empty()) {
        t += "NIST ASD lists it as a forbidden transition of type " + J.asdType;
        t += J.haveA ? ": A = " + rateText(J.A) + " (accuracy " + accMeaning(J.acc) + "), a different, forbidden process, not counted in the lifetime above." : ", but gives no A for it.";
    } else if (J.asdListed) {                                       // ASD marks the line as allowed (blank type) although the rule forbids it: said, not used
        t += "NIST ASD lists these two levels as an ALLOWED line (type blank)" + std::string(J.haveA ? " with A = " + rateText(J.A) : "") + ", which disagrees with the rule above; the lab follows the rule, treats the jump as not allowed and does not use that value.";
    } else t += "NIST ASD lists no line for it.";
    return t;
}

std::string metastableText(int Z) {
    std::string t = "Metastable: no electric-dipole jump takes 2s down to a lower level (l and the parity would not change), so the lab has no allowed way down to count and gives no lifetime. A real 2s state still decays: it can emit two photons at once, "
                    "but NIST ASD does not list that process, so the lab does not model it and gives no number for it.";
    const AsdLine* m = findLine(Z, {1, 0, 1}, {2, 0, 1});          // ASD's forbidden one-photon line 2s -> 1s, when this ion has one
    if (m && m->type[0] != '\0' && !std::isnan(m->aki)) t += " (ASD does list a one-photon jump 2s → 1s of type " + std::string(m->type) + ", A = " + rateText(m->aki) + ", accuracy " + accMeaning(m->acc) + ": a different process, shown in the jump check, not a lifetime.)";
    return t;
}

}  // namespace atom
