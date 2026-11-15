// See levels.h.
#include "atom/levels.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <map>

#include "atom/atom_core.h"

namespace atom {

namespace {
#include "atom/level_data.inc"

const LvlAtomRow* atomRow(int Z) {
    for (const LvlAtomRow& a : kLvlAtoms) if (a.Z == Z) return &a;
    return nullptr;
}
}  // namespace

double JLine::nm() const { return std::isfinite(obsNm) ? obsNm : ritzNm; }
bool JLine::observed() const { return std::isfinite(obsNm); }
double JLine::rate() const { return std::isfinite(aki) ? aki : akiCalc; }

const std::vector<JLevel>& jumpLevels(int Z) {
    static std::map<int, std::vector<JLevel>> cache;
    const auto it = cache.find(Z);
    if (it != cache.end()) return it->second;
    std::vector<JLevel>& v = cache[Z];
    if (const LvlAtomRow* a = atomRow(Z))
        for (int i = a->first; i < a->first + a->count; i++) {
            const LvlRow& r = kLvlLevels[i];
            JLevel L;
            L.conf = r.conf; L.term = r.term; L.j2 = r.j2; L.odd = r.odd != 0; L.cm = r.cm; L.cmText = r.cmText;
            L.cloud = (r.flags & 1) != 0; L.calc = (r.flags & 2) != 0; L.measured = (r.flags & 4) != 0; L.d2 = (r.flags & 8) != 0; L.why = r.why;
            v.push_back(L);
        }
    return v;
}
const std::vector<JLine>& jumpLines(int Z) {
    static std::map<int, std::vector<JLine>> cache;
    const auto it = cache.find(Z);
    if (it != cache.end()) return it->second;
    std::vector<JLine>& v = cache[Z];
    if (const LvlAtomRow* a = atomRow(Z))
        for (int i = a->firstLine; i < a->firstLine + a->lineCount; i++) {
            const LvlLineRow& r = kLvlLines[i];
            JLine L;
            L.lo = r.lo; L.hi = r.hi; L.obsNm = r.obsNm; L.ritzNm = r.ritzNm; L.aki = r.aki; L.acc = r.acc; L.akiCalc = r.akiCalc; L.gaugePct = r.gaugePct; L.Z = Z;
            v.push_back(L);
        }
    return v;
}
double ionLimitCm(int Z) { const LvlAtomRow* a = atomRow(Z); return a ? a->limitCm : NAN; }
double rateMedianPct(int Z) { const LvlAtomRow* a = atomRow(Z); return a ? a->rateMedPct : NAN; }
int rateCompared(int Z) { const LvlAtomRow* a = atomRow(Z); return a ? a->rateN : 0; }

const GraspLevel* levelCloud(int Z, int i) {
    const std::vector<JLevel>& lv = jumpLevels(Z);
    if (i < 0 || i >= int(lv.size()) || !lv[std::size_t(i)].cloud) return nullptr;
    const JLevel& L = lv[std::size_t(i)];
    for (const GraspLevel& g : graspAll(Z)) if (!g.testOnly && g.j2 == L.j2 && g.conf == L.conf && g.term == L.term) return &g;
    return nullptr;
}
bool levelJumps(int Z) {
    static std::map<int, bool> cache;
    const auto it = cache.find(Z);
    if (it != cache.end()) return it->second;
    bool any = false;                                            // (at least one jump to offer: else the atom keeps the "not computed yet" sentence)
    if (!jumpLevels(Z).empty() && levelCloud(Z, 0) != nullptr) for (int k = 0; k < int(jumpLines(Z).size()) && !any; k++) any = jumpOffered(Z, k);
    return cache[Z] = any;
}
std::vector<int> cloudLevelList(int Z) {
    std::vector<int> v;
    const std::vector<JLevel>& lv = jumpLevels(Z);
    for (int i = 0; i < int(lv.size()); i++) if (levelCloud(Z, i)) v.push_back(i);
    std::stable_sort(v.begin(), v.end(), [&](int a, int b) { return lv[std::size_t(a)].cm < lv[std::size_t(b)].cm; });
    return v;
}
std::vector<int> linesAt(int Z, int i) {
    std::vector<int> v;
    const std::vector<JLine>& ln = jumpLines(Z);
    for (int k = 0; k < int(ln.size()); k++) if (ln[std::size_t(k)].lo == i || ln[std::size_t(k)].hi == i) v.push_back(k);
    return v;
}
std::vector<int> linesBetween(int Z, int a, int b) {
    std::vector<int> v;
    const std::vector<JLine>& ln = jumpLines(Z);
    for (int k = 0; k < int(ln.size()); k++) {
        const JLine& L = ln[std::size_t(k)];
        if ((L.lo == a && L.hi == b) || (L.lo == b && L.hi == a)) v.push_back(k);
    }
    return v;
}
bool jumpOffered(int Z, int line) {
    const std::vector<JLine>& ln = jumpLines(Z);
    if (line < 0 || line >= int(ln.size())) return false;
    const std::vector<JLevel>& lv = jumpLevels(Z);
    const JLine& L = ln[std::size_t(line)];
    const JLevel &a = lv[std::size_t(L.lo)], &b = lv[std::size_t(L.hi)];
    return (a.measured || L.lo == 0) && b.measured && levelCloud(Z, L.lo) && levelCloud(Z, L.hi);
}

static const char* const kSupDigit[10] = {"⁰", "¹", "²", "³", "⁴", "⁵", "⁶", "⁷", "⁸", "⁹"};
std::string termPretty(const std::string& t) {
    // "3P*" -> "³P°"; ASD's other forms (jK "2[3/2]*", jj "(7/2,5/2)*") are kept, with ° for the star
    std::string out;
    std::size_t i = 0;
    if (!t.empty() && std::isdigit(static_cast<unsigned char>(t[0])) && t.find('[') == std::string::npos)
        for (; i < t.size() && std::isdigit(static_cast<unsigned char>(t[i])); i++) out += kSupDigit[t[i] - '0'];
    for (; i < t.size(); i++) out += t[i] == '*' ? std::string("°") : std::string(1, t[i]);
    return out;
}
std::string confPretty(const std::string& c) {
    // ASD's "2p6.3s2" -> "2p⁶3s²": the electron count after each subshell letter as a superscript; dots dropped
    std::string out;
    for (std::size_t i = 0; i < c.size(); i++) {
        const char ch = c[i];
        if (ch == '.') continue;
        if (std::isdigit(static_cast<unsigned char>(ch)) && i > 0 && std::isalpha(static_cast<unsigned char>(c[i - 1])) && std::string("spdfgh").find(c[i - 1]) != std::string::npos) {
            std::size_t j = i;
            while (j < c.size() && std::isdigit(static_cast<unsigned char>(c[j]))) out += kSupDigit[c[j++] - '0'];
            i = j - 1;
        } else out += ch;
    }
    return out;
}
static std::string jText(int j2) { return j2 % 2 ? std::to_string(j2) + "/2" : std::to_string(j2 / 2); }
std::string levelName(int Z, int i) {
    const std::vector<JLevel>& lv = jumpLevels(Z);
    if (i < 0 || i >= int(lv.size())) return "";
    const JLevel& L = lv[std::size_t(i)];
    return confPretty(L.conf) + " " + termPretty(L.term) + " J=" + jText(L.j2);
}
std::string gradeText(const std::string& acc) {
    // NIST ASD lines help (data/jumps/sources/asd_lines_help.html): AAA <= 0.3 %, AA <= 1 %, A+ <= 2 %, A <= 3 %, B+ <= 7 %, B <= 10 %, C+ <= 18 %, C <= 25 %, D+ <= 40 %,
    // D <= 50 %, E > 50 %; a prime: split from a multiplet assuming pure LS coupling (may be less accurate)
    static const std::map<std::string, std::string> kGrade = {{"AAA", "within 0.3 %"}, {"AA", "within 1 %"}, {"A+", "within 2 %"}, {"A", "within 3 %"}, {"B+", "within 7 %"},
                                                             {"B", "within 10 %"}, {"C+", "within 18 %"}, {"C", "within 25 %"}, {"D+", "within 40 %"}, {"D", "within 50 %"}, {"E", "more than 50 % uncertain"}};
    if (acc.empty()) return "";
    const bool prime = acc.back() == '\'';
    const std::string g = prime ? acc.substr(0, acc.size() - 1) : acc;
    const auto it = kGrade.find(g);
    if (it == kGrade.end()) return "grade " + acc;
    return "grade " + g + ": " + it->second + (prime ? "; split from its multiplet assuming LS coupling" : "");
}
std::string rateText(const JLine& L) {
    const bool own = !std::isfinite(L.aki) && std::isfinite(L.akiCalc);
    if (!std::isfinite(L.aki) && !own) return "rate not known (no measured or evaluated value in NIST, and none computed)";
    char b[32];
    std::snprintf(b, sizeof b, own ? "%.2g" : "%.3g", own ? L.akiCalc : L.aki);
    std::string s = b;
    const std::size_t e = s.find('e');
    if (e != std::string::npos) {
        const int x = std::atoi(s.c_str() + e + 1);
        std::string sup = x < 0 ? "⁻" : "";
        for (const char ch : std::to_string(std::abs(x))) sup += kSupDigit[ch - '0'];
        s = s.substr(0, e) + " × 10" + sup;
    }
    if (own) {
        // D4 (owner decision 9 Nov 2026): the app's own value where NIST has none, marked as such; the length and velocity forms of the same calculation differ by gaugePct
        char gp[160];
        const int Z = L.Z;
        if (Z > 0 && rateCompared(Z) > 0)
            std::snprintf(gp, sizeof gp, "; on the %d lines NIST also has, half of this app's values are within %.0f %% of NIST's; this one's two forms differ by %.0f %%",
                          rateCompared(Z), rateMedianPct(Z), std::fabs(L.gaugePct));
        else std::snprintf(gp, sizeof gp, "; its two forms differ by %.0f %%", std::fabs(L.gaugePct));
        return "A = " + s + " /s, computed by this app (GRASP2018), not measured" + gp;
    }
    const std::string g = gradeText(L.acc);
    return "A = " + s + " /s (NIST ASD" + (g.empty() ? "" : ", " + g) + ")";
}

JumpStatus jumpStatus(int Z) {
    for (const JumpStatusRow& r : kJumpStatus) if (r.Z == Z) return JumpStatus(r.code);
    return JumpStatus::None;
}
std::string noJumpText(int Z) {
    // PLAN_JUMPS.md section 7 (owner decision D5)
    switch (jumpStatus(Z)) {
        case JumpStatus::Measured: return "";
        case JumpStatus::LevelsOnly: return "Measured levels exist, but NIST has not matched its lines to them yet; no jumps are shown.";
        case JumpStatus::NotInNist: return "Measured (Blaise & Wyart 1992 and later), but the tables are not in NIST and could not be reached; no jumps are shown.";
        case JumpStatus::FewLines:
            return Z == 100 ? "A few jumps were measured with lasers (Sewtz et al. 2003; Warbinek et al., Nature 2024); they are not in NIST; no jumps are shown."
                            : "One jump and Rydberg levels were measured with lasers (Laatiaoui et al., Nature 2016; Chhetri et al. 2018); they are not in NIST; no jumps are shown.";
        case JumpStatus::None: return "No jumps: nobody has measured light from this atom yet.";
    }
    return "";
}

std::string cloudsNotComputedText() {
    static const std::string text = [] {
        std::string alk, lev;
        for (int Z = 1; Z <= 118; Z++) {
            if (alkaliJumps(Z)) alk += std::string(alk.empty() ? "" : ", ") + symbol(Z);
            else if (levelJumps(Z)) lev += std::string(lev.empty() ? "" : ", ") + symbol(Z);
        }
        return "NIST has measured jumps of this atom, but the clouds of its levels were not computed for this app; with every electron shown only " + alk + ", " + lev +
               " have jumps. Electrons: Hydrogen-like shows jumps for any element.";
    }();
    return text;
}

std::string levelModelNote() {
    return "every electron: this level's natural orbitals with their occupations (they add up to the electron count), computed for this app with GRASP2018 (Froese Fischer, Gaigalas, "
           "Joensson, Bieron, Comput. Phys. Commun. 237, 184 (2019)): Dirac-Fock orbitals for each outer orbital; one set of correlation orbitals for the whole atom (MCDHF with single "
           "and double excitations of the two outer electrons and at most one hole in the outermost closed shell, three layers), shared by all its levels; then CI per J and parity with "
           "the Breit interaction and QED (helium: its own correlation orbitals per J and parity). "
           "The shape is the M_J = J state's density, sum of D_k(r) P_k(cos theta), k even up to 2J. Energies, wavelengths and rates: NIST ASD";
}

}  // namespace atom
