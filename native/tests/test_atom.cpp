// Atom Lab tests: the native physics and texts against values made by the web version's own
// JavaScript (tests/reference/make_atom_reference.js runs index.html's Atom Lab code in Node).
// Random parts are compared too: both sides use the same seeded generator.
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include "atom/atom_core.h"
#include "atom/atom_sim.h"
#include "orbit/format.h"

#include "reference/atom_reference.inc"

using namespace atom;

static int g_checks = 0, g_failed = 0;

static void checkStr(const std::string& what, const std::string& got, const std::string& want) {
    g_checks++;
    if (got == want) return;
    if (++g_failed <= 40) std::printf("FAIL %s\n   got:  %s\n   want: %s\n", what.c_str(), got.c_str(), want.c_str());
}
// Text that may differ only by one in the last shown digit of a number, where the value sits exactly
// on a rounding tie (e.g. an orbit radius of exactly 0.575 pm): the last bit of sin/cos differs
// between compilers, so the web's own result on such a tie depends on the platform too.
static bool sameButLastDigit(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) return false;
    int diffs = 0;
    for (std::size_t i = 0; i < a.size(); i++) {
        if (a[i] == b[i]) continue;
        if (!std::isdigit(static_cast<unsigned char>(a[i])) || !std::isdigit(static_cast<unsigned char>(b[i])) || std::abs(a[i] - b[i]) != 1) return false;
        if (i + 1 < a.size() && std::isdigit(static_cast<unsigned char>(a[i + 1]))) return false;   // only a number's last digit
        diffs++;
    }
    return diffs == 1;
}
static int g_ties = 0;
static void checkLine(const std::string& what, const std::string& got, const std::string& want) {
    if (got != want && sameButLastDigit(got, want)) { g_checks++; g_ties++; std::printf("note %s: rounding tie, %s vs %s\n", what.c_str(), got.c_str(), want.c_str()); return; }
    checkStr(what, got, want);
}
static void checkNum(const std::string& what, double got, double want, double relTol, double absTol = 0) {
    g_checks++;
    const bool same = (std::isnan(got) && std::isnan(want)) || got == want || std::fabs(got - want) <= std::max(absTol, relTol * std::fabs(want));
    if (same) return;
    if (++g_failed <= 40) std::printf("FAIL %s: got %.17g, want %.17g\n", what.c_str(), got, want);
}
static void checkTrue(const std::string& what, bool ok) { checkStr(what, ok ? "true" : "false", "true"); }
static std::string at(int Z, int N) { return std::to_string(Z) + "," + std::to_string(N); }

static void testElements() {
    for (const ElementRef& e : kElementRef) {
        const std::string w = "Z=" + std::to_string(e.Z);
        checkStr(w + " name", name(e.Z), e.name);
        checkStr(w + " symbol", symbol(e.Z), e.symbol);
        checkNum(w + " defaultN", defaultN(e.Z), e.defaultN, 0);
        checkNum(w + " N range lo", nRange(e.Z).first, e.nLo, 0);
        checkNum(w + " N range hi", nRange(e.Z).second, e.nHi, 0);
        checkTrue(w + " hasStable", hasStable(e.Z) == e.stable);
        checkStr(w + " category", categoryName(e.Z), e.cat);
        checkNum(w + " row", ptPos(e.Z).first, e.row, 0);
        checkNum(w + " col", ptPos(e.Z).second, e.col, 0);
        const auto st = tempState(e.Z, kTRoom);
        checkStr(w + " state", st ? *st : "", e.state);
        fmt::setImperial(false);
        checkStr(w + " tempLine", tempLine(e.Z), e.tempM);
        fmt::setImperial(true);
        checkStr(w + " tempLine imperial", tempLine(e.Z), e.tempI);
        fmt::setImperial(false);
        checkStr(w + " ion", ionLabel(e.Z), e.ion);
    }
}

static void testNuclides() {
    for (const NuclideRef& n : kNuclideRef) {
        checkStr("isoLine " + at(n.Z, n.N), isoLine(n.Z, n.N), n.line);
        checkStr("isoInfo " + at(n.Z, n.N), isoInfo(n.Z, n.N), n.info);
    }
    for (const ModeRef& m : kModeRef) {
        const auto mv = decMove(m.m);
        checkTrue(std::string("decMove ") + m.m, bool(mv) == m.moves && (!mv || (mv->first == m.dZ && mv->second == m.dN)));
        checkStr(std::string("decWord ") + m.m, decWord(m.m), m.word);
        checkStr(std::string("decShort ") + m.m, decShort(m.m), m.shortWord);
        std::string shots;
        for (const Shot& s : decShots(m.m)) shots += (shots.empty() ? "" : " ") + s.label + "/" + s.col + "/" + fmt::jsString(s.size);
        checkStr(std::string("decShots ") + m.m, shots, m.shots);
    }
    for (const NucNumRef& r : kNucNumRef) {
        const Binding b = massBinding(r.Z, r.N);
        checkNum("binding " + at(r.Z, r.N), b.B, r.B, 1e-12);
        checkTrue("binding measured " + at(r.Z, r.N), b.real == r.real);
        checkNum("beta2 " + at(r.Z, r.N), nucBeta(r.Z, r.N), r.beta, 1e-12, 1e-15);
        checkTrue("shape measured " + at(r.Z, r.N), nucMeasured(r.Z, r.N) == r.measured);
        checkNum("nuclear radius " + at(r.Z, r.N), nuclearR(r.Z + r.N), r.R, 1e-14);
    }
}

static void testFormats() {
    for (const FmtRef& f : kFmtRef) {
        const std::string fn = f.f;
        const double x = f.x;
        std::string got;
        if (fn == "fmtHalf") got = fmtHalf(x);
        else if (fn == "fmtHalf2") got = fmtHalf(x, 2);
        else if (fn == "fmtLife") got = fmtLife(x);
        else if (fn == "sci") got = sci(x);
        else if (fn == "metres") got = metres(x);
        else if (fn == "fmtLam") got = fmtLam(x);
        else if (fn == "fmtLen") got = fmtLen(x);
        else if (fn == "fmtT") got = fmtT(x);
        else if (fn == "fmtEV") got = fmtEV(x);
        else if (fn == "lrnSci") got = lrnSci(x);
        else if (fn == "lrnSci3") got = lrnSci3(x);
        else if (fn == "lrnSci3_2") got = lrnSci3(x, 2);
        else if (fn == "lrnLenM") { fmt::setImperial(false); got = lrnLen(x); }
        else if (fn == "lrnLenI") { fmt::setImperial(true); got = lrnLen(x); fmt::setImperial(false); }
        else if (fn == "supDigits") got = supDigits(long(x));
        else if (fn == "nuclideLabel") got = nuclideLabel(int(x) / 1000, int(x) % 1000);
        else if (fn == "fK") got = fK(x);
        else if (fn == "fC") got = fC(x);
        else if (fn == "fF") got = fF(x);
        else got = "(no such format " + fn + ")";
        checkStr(fn + "(" + fmt::jsString(x) + ")", got, f.text);
    }
}

static void testOrbitals() {
    const auto& orbs = orbitals();
    checkNum("orbital count", orbs.size(), sizeof kOrbKeys / sizeof kOrbKeys[0], 0);
    for (std::size_t i = 0; i < orbs.size() && i < sizeof kOrbKeys / sizeof kOrbKeys[0]; i++) {
        checkStr("orbital key " + std::to_string(i), orbs[i].key, kOrbKeys[i]);
        checkStr("orbital name " + std::to_string(i), orbs[i].txt, kOrbTxt[i]);
    }
    for (const PsiRef& r : kPsiRef)
        checkNum(std::string("psi ") + r.key + " Z=" + std::to_string(r.Z), psi(r.key, r.Z, r.x, r.y, r.z), r.v, 1e-12, 1e-300);
    for (const LagRef& r : kLagRef) {
        const auto c = lagCoef(r.k, r.a);
        for (int i = 0; i < 4; i++) checkNum("lagCoef", i < int(c.size()) ? c[i] : 0, r.c[i], 1e-14);
    }
    const std::size_t n = orbs.size();
    for (std::size_t a = 0; a < n; a++)
        for (std::size_t b = 0; b < n; b++)
            checkTrue("dipole " + orbs[a].key + " > " + orbs[b].key, dipoleOK(int(a), int(b)) == (kDipoleRef[a * n + b] == '1'));
}

static void testLight() {
    for (const SpecRef& r : kSpecRef) {
        checkNum("rydM " + at(r.Z, r.N), rydM(r.Z, r.N), r.ryd, 1e-14);
        const auto lines = specLines(r.Z, r.N);
        checkNum("line count", lines.size(), 105, 0);
        for (std::size_t i = 0; i < lines.size() && i < 105; i++) checkNum("line " + at(r.Z, r.N) + " #" + std::to_string(i), lines[i].lam, r.lam[i], 1e-13);
    }
    for (const ColRef& c : kColRef) checkStr("lamColor " + fmt::jsString(c.lam), lamColorCss(c.lam), c.css);
}

static void testNuclei() {
    for (const NucleusRef& r : kNucleusRef) {
        seedRandom(r.seed);
        const auto got = buildNucleus(r.Z, r.N);
        const std::string w = "nucleus " + at(r.Z, r.N);
        checkNum(w + " count", got.size(), r.n.size(), 0);
        for (std::size_t i = 0; i < got.size() && i < r.n.size(); i++) {
            const double f = kFmLab;   // built in fm and stored in metres: compare in fm
            checkNum(w + " x" + std::to_string(i), got[i].x / f, r.n[i].x, 1e-9, 1e-9);
            checkNum(w + " y" + std::to_string(i), got[i].y / f, r.n[i].y, 1e-9, 1e-9);
            checkNum(w + " z" + std::to_string(i), got[i].z / f, r.n[i].z, 1e-9, 1e-9);
            checkTrue(w + " kind" + std::to_string(i), got[i].isP == r.n[i].isP);
            checkNum(w + " phase" + std::to_string(i), got[i].ph, r.n[i].ph, 1e-15);
        }
    }
}

// The whole lab, run by the same script as the web, with the same random numbers.
static void testRuns() {
    int run = 0;
    for (const RunRef& r : kRunRef) {
        run++;
        AtomLab lab;
        lab.nowMs = [] { return 0.0; };
        std::size_t ci = 0;
        for (const Op& op : r.ops) {
            const std::string o = op.op;
            if (o == "seed") { seedRandom(uint32_t(op.v)); lab.p = AtomParams(); lab.paused = false; lab.chain.clear(); lab.restart(); }
            else if (o == "set") { if (*op.sv) lab.setParam(op.key, std::string(op.sv)); else lab.setParam(op.key, op.v); }
            else if (o == "tick") lab.tick(op.v);
            else if (o == "steps") for (int i = 0; i < int(op.v); i++) lab.step(1.0 / 240);
            else if (o == "emit") lab.emit();
            else if (o == "absorb") lab.absorb();
            else if (o == "auto") lab.toggleAuto();
            else if (o == "pause") lab.paused = true;
            else if (o == "resume") lab.paused = false;
            else if (o == "check") {
                if (ci >= r.checks.size()) break;
                const Check& c = r.checks[ci++];
                const std::string w = "run " + std::to_string(run) + " check " + std::to_string(ci);
                const auto info = lab.info();
                checkNum(w + " info lines", info.size(), c.info.size(), 0);
                for (std::size_t i = 0; i < info.size() && i < c.info.size(); i++) checkLine(w + " info " + std::to_string(i), info[i], c.info[i]);
                checkStr(w + " hint", lab.hint(), c.hint);
                std::string chain;
                for (const ChainStep& st : lab.chain)
                    chain += (chain.empty() ? "" : " > ") + (st.nuc.empty() ? st.how + "|" + jsNum(st.lived, 6) + "|" + (st.to ? *st.to : "null") : st.nuc);
                checkStr(w + " decay chain", chain, c.chain);
                std::string log;
                for (const LogLine& L : lab.s.decLog) log += (log.empty() ? "" : "\n") + L.txt;
                checkStr(w + " decay log", log, c.log);
                checkNum(w + " x", lab.s.x, c.x, 1e-9, 1e-22);
                checkNum(w + " y", lab.s.y, c.y, 1e-9, 1e-22);
                checkNum(w + " vx", lab.s.vx, c.vx, 1e-9, 1e-6);
                checkNum(w + " vy", lab.s.vy, c.vy, 1e-9, 1e-6);
                checkNum(w + " dots", lab.s.k, c.k, 0);
                for (int i = 0; i < 6; i++) checkNum(w + " dot coordinate " + std::to_string(i), lab.s.pts[i], c.pts[i], 1e-6, 1e-17);
            }
        }
    }
}

int main() {
    testElements();
    testNuclides();
    testFormats();
    testOrbitals();
    testLight();
    testNuclei();
    testRuns();
    std::printf("%d atom checks, %d failed (%d on an exact rounding tie, off by one in the last digit).\n", g_checks, g_failed, g_ties);
    return g_failed ? 1 : 0;
}
