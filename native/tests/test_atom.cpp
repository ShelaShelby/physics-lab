// Atom Lab tests: the native physics and texts against values made by the web version's own
// JavaScript (tests/reference/make_atom_reference.js runs index.html's Atom Lab code in Node).
// Random parts are compared too: both sides use the same seeded generator.
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <cstdint>
#include <cstdio>
#include <algorithm>
#include <array>
#include <chrono>
#include <complex>
#include <string>
#include <vector>

#include "atom/atom_core.h"
#include "atom/atom_learn.h"
#include "atom/atom_sim.h"
#include "atom/outline.h"
#include "atom/phase_wheel.h"
#include "orbit/format.h"

#include "reference/atom_reference.inc"
#include "reference/atom_learn_reference.inc"
#include "reference/ylm_reference.inc"

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

// ---- 0.10.6 stage 2: the CODATA 2022 values, typed in here AGAIN from the NIST pages (https://physics.nist.gov/cgi-bin/cuu/Value?<key>, each page says "Source: 2022 CODATA recommended
// values", all read on 2026-10-04), so the tests do not take them from atom_core.h. The key of each page is in the comment.
namespace nist {
constexpr double a0 = 5.29177210544e-11;        // m, bohrrada0
constexpr double hartreeEV = 27.211386245981;   // eV, hrev
constexpr double tau = 2.4188843265864e-17;     // s, aut (atomic unit of time)
constexpr double alpha = 7.2973525643e-3;       // alph
constexpr double rydEV = 13.605693122990;       // eV, rydhcev
constexpr double mecEV = 510998.95069;          // eV, mec2mev (0.510 998 950 69 MeV)
constexpr double meInU = 5.485799090441e-4;     // meu
constexpr double mpOverMe = 1836.152673426;     // mpsme
constexpr double ukg = 1.66053906892e-27;       // kg, ukg
constexpr double uMeV = 931.49410372;           // MeV, muc2mev
constexpr double meKg = 9.1093837139e-31;       // me
constexpr double mpKg = 1.67262192595e-27;      // mp
constexpr double mnKg = 1.67492750056e-27;      // mn
constexpr double mpU = 1.0072764665789;         // mpu
constexpr double mnU = 1.00866491606;           // mnu
constexpr double kB = 1.380649e-23;             // J/K, k (exact)
constexpr double h = 6.62607015e-34;            // J s, h (exact)
constexpr double c = 299792458.0;               // m/s, c (exact)
constexpr double e = 1.602176634e-19;           // C, e (exact)
constexpr double hcEVnm = h * c / e * 1e9;      // eV nm, from the three exact values (NIST has no page for it)
}
// the web's rounded values the saved web results were made with (index.html, never changed)
namespace webval { constexpr double a0 = 0.529e-10, tau = 24.19e-18, ryd = 13.605693123, hcEVnm = 1239.841984, meu = 1822.888486, mpOverMe = 1836.15267; }
// the web's reduced-mass Rydberg energy and the same formula with the CODATA 2022 values: the saved web numbers are rescaled by their ratio
static double rydMBy(double ry, double meu, double mpme, int Z, int N) { const double M = Z == 1 && N == 0 ? mpme : (Z + N) * meu - Z; return ry * M / (M + 1); }
static double rydMRatio(int Z, int N) { return rydMBy(nist::rydEV, 1 / nist::meInU, nist::mpOverMe, Z, N) / rydMBy(webval::ryd, webval::meu, webval::mpOverMe, Z, N); }

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
    // 0.10.6 stage 2: the saved web values were made with the web's rounded Rydberg energy, h·c, u/m_e and m_p/m_e. The lab now uses the CODATA 2022 ones, so each web value is multiplied by the
    // ratio the changed constants give (rydMRatio above; for a wavelength h·c ratio / rydM ratio). The limits stay 1e-14 and 1e-13: the change is about 1e-9, so this is not a loosening.
    for (const SpecRef& r : kSpecRef) {
        const double q = rydMRatio(r.Z, r.N), qLam = (nist::hcEVnm / webval::hcEVnm) / q;
        checkNum("rydM " + at(r.Z, r.N), rydM(r.Z, r.N), r.ryd * q, 1e-14);
        const auto lines = specLines(r.Z, r.N);
        checkNum("line count", lines.size(), 105, 0);
        for (std::size_t i = 0; i < lines.size() && i < 105; i++) checkNum("line " + at(r.Z, r.N) + " #" + std::to_string(i), lines[i].lam, r.lam[i] * qLam, 1e-13);
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
// 0.10.6 stage 3 (finding 3): the readout energy is now the reduced-mass level, so the energy text of the saved web lines is rebuilt, EXACTLY, from what the web's own state says:
//  Cloud view: E = -rydM(Z, N) Z^2 / n^2, worked out here from the NIST values (rydMBy and nist:: above), not from atom_core.h.
//  Orbit view: the web's E = (1/2 v^2 - Z / r) x 27.211 eV from the web's saved x, y, vx, vy (its a0 and tau), converted to the new Hartree energy and multiplied by the reduced-mass ratio rydM / Ry_inf.
//  Heavy-atom line "real 1s energy -A vs -B in this lab": B = rydM Z^2 (A, the Dirac value, is unchanged).
// Everything else in the line stays as the web wrote it; the energy part is then compared exactly (checkStr), outside the last-digit tolerance of checkLine.
static bool lineSegment(const std::string& line, const std::string& key, std::size_t& a, std::size_t& b) {
    a = line.find(key);
    if (a == std::string::npos) return false;
    b = line.find("  ·  ", a + key.size());
    if (b == std::string::npos) b = line.size();
    return true;
}
static std::string energyText(const AtomLab& lab, const Check& c) {          // the new "E ..." part of the line, as the lab writes it
    const int Z = lab.p.Z, N = lab.p.N;
    const double rm = rydMBy(nist::rydEV, 1 / nist::meInU, nist::mpOverMe, Z, N);
    if (lab.p.view != "Orbit") { const int n = orbitals()[orbIndex(lab.p.orb)].n; return "E −" + fmtEV(rm * Z * Z / (double(n) * n)); }
    const double auSpeed = webval::a0 / webval::tau, r = std::hypot(c.x, c.y) / webval::a0, v2 = (c.vx * c.vx + c.vy * c.vy) / (auSpeed * auSpeed);
    const double E = (0.5 * v2 - Z / r) * nist::hartreeEV * (rm / nist::rydEV);
    return std::string("E ") + (E < 0 ? "−" : "") + fmtEV(std::fabs(E));
}
static std::string withNewEnergy(std::string line, const AtomLab& lab, const Check& c, std::string* energyOnly) {
    std::size_t a, b;
    if (lineSegment(line, "  ·  E ", a, b)) { a += std::string("  ·  ").size(); const std::string e = energyText(lab, c); line.replace(a, b - a, e); if (energyOnly) *energyOnly = e; }
    const std::string key = " vs −", tail = " in this lab";
    const std::size_t k = line.find(key);
    if (k != std::string::npos && line.find("real 1s energy −") != std::string::npos) {
        const std::size_t e = line.find(tail, k);
        const double rm = rydMBy(nist::rydEV, 1 / nist::meInU, nist::mpOverMe, lab.p.Z, lab.p.N);
        line.replace(k + key.size(), e - k - key.size(), fmtEV(rm * lab.p.Z * lab.p.Z));
        if (energyOnly) *energyOnly += "|" + line.substr(k, e - k);
    }
    return line;
}

static void testRuns() {
    const double sLen = nist::a0 / webval::a0, sSpeed = (nist::a0 / nist::tau) / (webval::a0 / webval::tau);   // CODATA 2022 over the web's value: 1.000334, 1.000383
    int run = 0;
    for (const RunRef& r : kRunRef) {
        run++;
        AtomLab lab;
        lab.nowMs = [] { return 0.0; };
        lab.webJumps = true;                                           // these runs compare with the web's own jumps (instant leap, dot-by-dot morph); the app itself uses the smooth ones
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
                for (std::size_t i = 0; i < info.size() && i < c.info.size(); i++) {
                    std::string eWant, eGot;
                    const std::string want = withNewEnergy(c.info[i], lab, c, &eWant);
                    withNewEnergy(info[i], lab, c, &eGot);   // (only to cut the same parts out of the lab's line: its own energy parts are compared below)
                    checkLine(w + " info " + std::to_string(i), info[i], want);
                    if (!eWant.empty()) {
                        std::string gotE, gotH; std::size_t a, b;
                        if (lineSegment(info[i], "  ·  E ", a, b)) { const std::size_t sk = std::string("  ·  ").size(); gotE = info[i].substr(a + sk, b - a - sk); }
                        const std::size_t k = info[i].find(" vs −", 0), e = info[i].find(" in this lab");
                        if (k != std::string::npos && e != std::string::npos && info[i].find("real 1s energy −") != std::string::npos) gotH = "|" + info[i].substr(k, e - k);
                        checkStr(w + " info " + std::to_string(i) + " energy part (exact)", gotE + gotH, eWant);
                    }
                }
                checkStr(w + " hint", lab.hint(), c.hint);
                std::string chain;
                for (const ChainStep& st : lab.chain)
                    chain += (chain.empty() ? "" : " > ") + (st.nuc.empty() ? st.how + "|" + jsNum(st.lived, 6) + "|" + (st.to ? *st.to : "null") : st.nuc);
                checkStr(w + " decay chain", chain, c.chain);
                std::string log;
                for (const LogLine& L : lab.s.decLog) log += (log.empty() ? "" : "\n") + L.txt;
                checkStr(w + " decay log", log, c.log);
                // 0.10.6 stage 2: the web's Bohr radius (0.529 Å) and atomic unit of time (24.19 as) became the CODATA 2022 ones. The motion in atomic units is the same (the step is
                // dt x speed / Z^2 atomic units, the dots are sampled in Bohr radii), so every length is the web's x (a0 ratio) and every speed the web's x (a0 / tau ratio), exactly.
                checkNum(w + " x", lab.s.x, c.x * sLen, 1e-9, 1e-22);
                checkNum(w + " y", lab.s.y, c.y * sLen, 1e-9, 1e-22);
                checkNum(w + " vx", lab.s.vx, c.vx * sSpeed, 1e-9, 1e-6);
                checkNum(w + " vy", lab.s.vy, c.vy * sSpeed, 1e-9, 1e-6);
                checkNum(w + " dots", lab.s.k, c.k, 0);
                for (int i = 0; i < 6; i++) checkNum(w + " dot coordinate " + std::to_string(i), lab.s.pts[i], c.pts[i] * sLen, 1e-6, 1e-17);
            }
        }
    }
}

// runs of spaces and line breaks fold into one space, as in a browser
static std::string fold(const std::string& s) {
    std::string o;
    for (char c : s) {
        const bool sp = c == ' ' || c == '\n' || c == '\t' || c == '\r';
        if (sp) { if (!o.empty() && o.back() != ' ') o += ' '; } else o += c;
    }
    while (!o.empty() && o.back() == ' ') o.pop_back();
    return o;
}

// 0.10.6 stage 2: the Learn masses are now the CODATA 2022 values instead of the web's rounded ones. The text is still word for word, except the 16-digit CSS width of a bar ("width:99.94556794247602%"),
// which moves by about 1e-10 relative with the new masses and is never shown as a number. Every other character must be the same, and each width must agree within 1e-8 relative (100 x the move).
static void checkTextWidths(const std::string& what, const std::string& got, const std::string& want) {
    std::string a = got, b = want;
    double worst = 0; int nw = 0; bool ok = true;
    std::size_t i = 0, j = 0;
    std::string ga, gb;
    for (;;) {
        const std::size_t p = a.find("width:", i), q = b.find("width:", j);
        if ((p == std::string::npos) != (q == std::string::npos)) { ok = false; break; }
        if (p == std::string::npos) { ga += a.substr(i); gb += b.substr(j); break; }
        ga += a.substr(i, p + 6 - i); gb += b.substr(j, q + 6 - j);
        char *ea = nullptr, *eb = nullptr;
        const double x = std::strtod(a.c_str() + p + 6, &ea), y = std::strtod(b.c_str() + q + 6, &eb);
        worst = std::max(worst, std::fabs(x - y) / std::max(std::fabs(y), 1e-300)); nw++;
        i = std::size_t(ea - a.c_str()); j = std::size_t(eb - b.c_str());
    }
    g_checks++;
    if (ok && ga == gb && worst < 1e-8) return;
    if (++g_failed <= 40) std::printf("FAIL %s (text differs outside the bar widths, or a width moved by %.2e)\n", what.c_str(), worst);
}

// The Learn guides, word for word against the web's own guide functions
static void testLearn() {
    for (const LearnRef& r : kLearnRef) {
        fmt::setImperial(r.imperial);
        AtomParams p;
        p.Z = r.Z; p.N = r.N; p.view = r.view; p.ostyle = r.ostyle; p.orb = r.orb;
        const std::string w = std::string(name(r.Z)) + "-" + std::to_string(r.Z + r.N) + " " + r.view + (r.imperial ? " (imperial)" : "");
        const auto secs = lookSections(p, r.live);
        checkNum(w + " look sections", secs.size(), r.look.size(), 0);
        for (std::size_t i = 0; i < secs.size() && i < r.look.size(); i++)
            checkStr(w + " look " + secs[i].key, secs[i].key + "|" + secs[i].title + "|" + fold(secs[i].html), r.look[i]);
        checkStr(w + " scale: powers of ten", fold(scaleHtml(p, "ten", 0)), r.scale[0]);
        for (int o = 0; o < 5; o++) checkStr(w + " scale: " + kScaleObjects[o], fold(scaleHtml(p, "scale", o)), r.scale[o + 1]);
        const char* tabs[5] = {"parts", "miss", "count", "proton", "ladder"};
        for (int t = 0; t < 5; t++) checkTextWidths(w + " weight: " + tabs[t], fold(massHtml(p, tabs[t])), r.mass[t]);
    }
    fmt::setImperial(false);
    // the panel's texts, against what the web's own buildUI wrote
    for (const PanelRef& r : kPanelRef) {
        fmt::setImperial(r.imperial);
        const std::string w = "panel " + std::string(name(r.Z)) + "-" + std::to_string(r.Z + r.N) + (r.imperial ? " (imperial)" : "");
        checkStr(w + " isotope line", isotopeLine(r.Z, r.N), r.iso);
        checkStr(w + " element card", elemCardHtml(r.Z) + "<div>" + kTableNote + "</div>", r.card);
        checkStr(w + " facts", factsHtml(r.Z), r.facts);
        checkStr(w + " spectrum note", spectrumNote(r.Z, r.N), r.spectrum);
        checkStr(w + " temperature", tempStateHtml(r.Z, r.tempK), r.tState);
        checkStr(w + " atom speed", tempSpeedHtml(r.Z, r.tempK), r.tSpeed);
        std::vector<ChainStep> chain;
        for (const std::string& c : r.chain) {
            const std::size_t a = c.find('|');
            if (a == std::string::npos) { chain.push_back({c, "", 0, std::nullopt}); continue; }
            const std::size_t b = c.find('|', a + 1);
            const std::string to = c.substr(b + 1);
            chain.push_back({"", c.substr(0, a), std::strtod(c.substr(a + 1, b - a - 1).c_str(), nullptr), to.empty() ? std::nullopt : std::optional<std::string>(to)});
        }
        const auto hist = decayHistory(chain, r.Z, r.N);
        checkNum(w + " history lines", hist.size(), r.hist.size(), 0);
        for (std::size_t i = 0; i < hist.size() && i < r.hist.size(); i++) checkStr(w + " history " + std::to_string(i), hist[i].first + "|" + hist[i].second, r.hist[i]);
    }
    fmt::setImperial(false);
    // the timeline (copied from the web by tools/make_atom_learn.js): 22 steps, 11 with a ▶ button
    int buttons = 0;
    for (const TimelineStep& st : timelineSteps()) if (*st.lab) buttons++;
    checkNum("timeline steps", timelineSteps().size(), 22, 0);
    checkNum("timeline buttons", buttons, 11, 0);
    // every element: the card, the facts and "where you meet it", by their hashes (both units)
    auto fnv = [](const std::string& t) { uint64_t h = 0xcbf29ce484222325ull; for (unsigned char b : t) { h ^= b; h *= 0x100000001b3ull; } return h; };
    for (const ElementTextRef& r : kElementTextRef) {
        const std::string w = std::string(name(r.Z)) + " ";
        auto hashCheck = [&](const std::string& what, const std::string& text, uint64_t want) {
            g_checks++;
            if (fnv(text) == want) return;
            if (++g_failed <= 40) std::printf("FAIL %s%s differs from the web (rerun the generator for its text):\n   %s\n", w.c_str(), what.c_str(), text.c_str());
        };
        fmt::setImperial(false);
        hashCheck("element card", elemCardHtml(r.Z), r.card);
        hashCheck("facts", factsHtml(r.Z), r.factsM);
        hashCheck("where you meet it", everyday(r.Z), r.everyM);
        fmt::setImperial(true);
        hashCheck("facts (imperial)", factsHtml(r.Z), r.factsI);
        hashCheck("where you meet it (imperial)", everyday(r.Z), r.everyI);
    }
    fmt::setImperial(false);
}

// ================= Complex orbitals and phase colours (0.10.2, stage 1) =================
// States with a magnetic quantum number m = +-1..+-l (Condon-Shortley phase) made from the existing real angular parts. Checked against things that do not come from
// the code: winding of the phase, the L_z eigenvalue, orthonormality by numerical integration, scipy's Y_lm, and the real orbitals' own densities.
using Cd = std::complex<double>;
static std::vector<int> complexIds() {                                // every complex state of every shell up to n = 4: id of (shell, m)
    std::vector<int> ids;
    for (int n = 2; n <= 4; n++)
        for (int l = 1; l < n; l++) {
            const int shell = orbIndex(std::to_string(n) + "spdf"[l]);
            for (int m = 1; m <= l; m++) { ids.push_back(complexState(shell, m)); ids.push_back(complexState(shell, -m)); }
        }
    return ids;
}

static void testComplexBasics() {
    const std::vector<int> ids = complexIds();
    checkTrue("complex states: 2 per m: 2p (2); 3p, 3d (2 + 4); 4p, 4d, 4f (2 + 4 + 6) = 20 states", ids.size() == 20);
    bool ok = true;
    for (int id : ids) ok = ok && id >= kComplexBase && isComplexState(id) && stateM(id) != 0 && std::abs(stateM(id)) <= orbitals()[stateOrbital(id)].l;
    checkTrue("complex states: ids are above kComplexBase and know their m and shell", ok);
    checkTrue("complex states: none for m = 0 or |m| > l, and real orbitals are not complex", complexState(orbIndex("2p"), 0) < 0 && complexState(orbIndex("2p"), 2) < 0 && complexState(orbIndex("1s"), 1) < 0 && !isComplexState(orbIndex("4f|xyz")));
    checkTrue("the 30 real orbitals are still the 30 real orbitals, in the same order", orbitals().size() == 30 && orbitals()[0].key == "1s|" && orbitals()[29].key == "4f|y3x2y2");
    int m = 0; checkTrue("orientation keys: 'm+2' / 'm-3' are complex, 'xy' is not", oriIsComplex("m+2", m) && m == 2 && oriIsComplex("m-3", m) && m == -3 && !oriIsComplex("xy", m) && oriKey(1) == "m+1" && oriKey(-3) == "m-3");
    for (const char* k : {"pAx", "dAx", "fAx"}) { checkTrue(std::string("the setting ") + k + " accepts m = +1 and still the real ones", AtomLab::validChoice(k, "m+1") && AtomLab::validChoice(k, "m-1")); }
    checkTrue("settings: m = +-3 only for f, m = +-2 only for d and f", AtomLab::validChoice("fAx", "m+3") && !AtomLab::validChoice("dAx", "m+3") && AtomLab::validChoice("dAx", "m-2") && !AtomLab::validChoice("pAx", "m+2"));
    checkTrue("the Phase colours setting: Sign (default) and Wheel, nothing else", AtomParams().phase == "Sign" && AtomParams::appDefaults().phase == "Sign" && AtomLab::validChoice("phase", "Wheel") && AtomLab::validChoice("phase", "Sign") && !AtomLab::validChoice("phase", "wheel") && !AtomLab::validChoice("phase", ""));
}

// (a) winding number: walk a circle round the z axis at fixed r and theta; the unwrapped arg(psi) changes by 2 pi m.
static void testWinding() {
    double worst = 0; int n = 0;
    for (int id : complexIds()) {
        const int m = stateM(id);
        for (double r : {1.3, 2.7, 5.1}) {
            const double th = 1.1;
            double prev = 0, total = 0; bool first = true, usable = true;
            const int steps = 720;
            for (int k = 0; k <= steps; k++) {
                const double ph = 2 * kPi * k / steps;
                const Cd v = psiComplex(id, 1, r * std::sin(th) * std::cos(ph), r * std::sin(th) * std::sin(ph), r * std::cos(th));
                if (std::abs(v) < 1e-9) { usable = false; break; }               // on a node: no phase to follow here
                const double a = std::arg(v);
                if (!first) { double d = a - prev; while (d > kPi) d -= 2 * kPi; while (d < -kPi) d += 2 * kPi; total += d; }
                prev = a; first = false;
            }
            if (!usable) continue;
            worst = std::max(worst, std::fabs(total - 2 * kPi * m)); n++;
        }
    }
    std::printf("complex orbitals, winding: %d circles, largest |total phase change - 2 pi m| = %.2e rad\n", n, worst);
    // limit 1e-9: the phase is exactly m*phi (+ a constant) and each of the 720 steps is far below pi, so only rounding (~1e-13 per step) remains.
    checkTrue("winding: the phase changes by 2 pi m round the z axis, for every complex state (to 1e-9)", n >= 50 && worst < 1e-9);
}

// (b) L_z: -i d(psi)/d(phi) = m psi, by a central difference in phi.
static void testLz() {
    double worst = 0; int n = 0;
    for (int id : complexIds()) {
        const int m = stateM(id);
        for (int k = 0; k < 12; k++) {
            const double r = 0.8 + 0.7 * k, th = 0.4 + 0.19 * k, ph = 0.3 + 0.53 * k, d = 1e-5;
            const auto at = [&](double f) { return psiComplex(id, 1, r * std::sin(th) * std::cos(f), r * std::sin(th) * std::sin(f), r * std::cos(th)); };
            const Cd v = at(ph);
            if (std::abs(v) < 1e-6) continue;
            const Cd lz = Cd(0, -1) * (at(ph + d) - at(ph - d)) / (2 * d);
            worst = std::max(worst, std::abs(lz - double(m) * v) / std::abs(v)); n++;
        }
    }
    std::printf("complex orbitals, L_z: %d points, largest |(-i d/dphi) psi - m psi| / |psi| = %.2e\n", n, worst);
    // limit 1e-7: the central difference's truncation error is d^2 m^3 / 6 = 1.7e-10 for m = 3 at d = 1e-5, its rounding error about 1e-16 / d = 1e-11.
    checkTrue("L_z: -i d(psi)/d(phi) = m psi at many points (to 1e-7 relative)", n > 200 && worst < 1e-7);
}

// (c) normalised and orthogonal: integrals of conj(psi_a) psi_b over all space (midpoint rule in r, Gauss-Legendre in cos theta, uniform in phi).
struct Quad { std::vector<double> r, wr, u, wu; int nphi; };
static Quad makeQuad(int nr = 450, int nphi = 24, double top = 60) {
    Quad q; q.nphi = nphi;
    const int nu = 24; const double dr = top / nr;
    for (int i = 0; i < nr; i++) { q.r.push_back((i + 0.5) * dr); q.wr.push_back(dr); }
    // Gauss-Legendre nodes by Newton's method on P_n
    for (int i = 0; i < nu; i++) {
        double x = std::cos(kPi * (i + 0.75) / (nu + 0.5)), dp = 1;
        for (int it = 0; it < 100; it++) {
            double p0 = 1, p1 = x;
            for (int k = 2; k <= nu; k++) { const double p2 = ((2 * k - 1) * x * p1 - (k - 1) * p0) / k; p0 = p1; p1 = p2; }
            dp = nu * (x * p1 - p0) / (x * x - 1);
            const double dx = p1 / dp; x -= dx; if (std::fabs(dx) < 1e-15) break;
        }
        q.u.push_back(x); q.wu.push_back(2 / ((1 - x * x) * dp * dp));
    }
    return q;
}
static Cd overlap(const Quad& q, int a, int b) {                       // integral of conj(psi_a) psi_b dV, Z = 1
    Cd s = 0;
    for (std::size_t i = 0; i < q.r.size(); i++)
        for (std::size_t j = 0; j < q.u.size(); j++) {
            const double st = std::sqrt(1 - q.u[j] * q.u[j]);
            Cd t = 0;
            for (int k = 0; k < q.nphi; k++) {
                const double f = 2 * kPi * (k + 0.5) / q.nphi, x = q.r[i] * st * std::cos(f), y = q.r[i] * st * std::sin(f), z = q.r[i] * q.u[j];
                const Cd pa = isComplexState(a) ? psiComplex(a, 1, x, y, z) : Cd(psi(a, 1, x, y, z), 0), pb = isComplexState(b) ? psiComplex(b, 1, x, y, z) : Cd(psi(b, 1, x, y, z), 0);
                t += std::conj(pa) * pb;
            }
            s += t * (2 * kPi / q.nphi) * q.wu[j] * q.r[i] * q.r[i] * q.wr[i];
        }
    return s;
}
static void testOrthonormal() {
    const Quad q = makeQuad();
    for (int n = 2; n <= 4; n++) {
        std::vector<int> all;                                           // the complex states of shell family n and the real orbitals of the same n
        for (int id : complexIds()) if (orbitals()[stateOrbital(id)].n == n) all.push_back(id);
        const std::size_t nc = all.size();
        for (int i = 0; i < int(orbitals().size()); i++) if (orbitals()[i].n == n) all.push_back(i);
        std::vector<double> norm(all.size());
        for (std::size_t i = 0; i < all.size(); i++) norm[i] = std::sqrt(overlap(q, all[i], all[i]).real());
        double worstDiag = 0, worstCC = 0, worstDiffL = 0, worstComplete = 0;
        for (std::size_t a = 0; a < nc; a++) {
            const int la = orbitals()[stateOrbital(all[a])].l;
            for (std::size_t b = 0; b < nc; b++) {                      // complex with complex: orthonormal
                const Cd g = overlap(q, all[a], all[b]) / (norm[a] * norm[b]);
                if (a == b) worstDiag = std::max(worstDiag, std::abs(g - 1.0)); else worstCC = std::max(worstCC, std::abs(g));
            }
            double sum = 0;                                             // complex with the real orbitals of the same n
            for (std::size_t b = nc; b < all.size(); b++) {
                const Cd g = overlap(q, all[a], all[b]) / (norm[a] * norm[b]);
                if (orbitals()[all[b]].l != la) worstDiffL = std::max(worstDiffL, std::abs(g)); else sum += std::norm(g);
            }
            worstComplete = std::max(worstComplete, std::fabs(sum - 1));   // the state lies entirely in the span of the real orbitals of its own shell: sum of |overlap|^2 = 1
        }
        std::printf("complex orbitals, n = %d (%zu states): |<a|a> - 1| <= %.1e, |<a|b>| <= %.1e (complex-complex), <= %.1e (other l), |sum of squared overlaps with the shell's real orbitals - 1| <= %.1e\n",
                    n, nc, worstDiag, worstCC, worstDiffL, worstComplete);
        // limit 1e-3 (as the task says): the quadrature has 450 radial midpoints (step 0.13 a0), 24 Gauss points in cos theta (exact for polynomials of degree 47) and 24 in phi (exact for |m| <= 3 products), so the error for these smooth
        // integrands is far below that; the 60 a0 cut-off leaves e^(-2*60/4) ~ 1e-13.
        checkTrue("n = " + std::to_string(n) + ": complex states are normalised (|<a|a>/norm - 1| < 1e-3)", worstDiag < 1e-3);
        checkTrue("n = " + std::to_string(n) + ": complex states are orthogonal to each other (< 1e-3)", worstCC < 1e-3);
        checkTrue("n = " + std::to_string(n) + ": complex states are orthogonal to the real orbitals of another l (< 1e-3)", worstDiffL < 1e-3);
        checkTrue("n = " + std::to_string(n) + ": each complex state lies in the span of the real orbitals of its own shell (sum of squared overlaps = 1, < 1e-3)", worstComplete < 1e-3);
    }
    // the real orbitals are still orthogonal to each other (the existing ones are not touched): 3p x, y, z and 3d xz with 3d z2
    const int px = orbIndex("3p|x"), py = orbIndex("3p|y"), pz = orbIndex("3p|z"), dxz = orbIndex("3d|xz"), dz2 = orbIndex("3d|z2");
    const double a = std::abs(overlap(q, px, py)) / std::sqrt(overlap(q, px, px).real() * overlap(q, py, py).real()), b = std::abs(overlap(q, pz, px)), c = std::abs(overlap(q, dxz, dz2));
    checkTrue("the real orbitals stay orthogonal (3px-3py, 3pz-3px, 3dxz-3dz2; < 1e-3)", a < 1e-3 && b < 1e-3 && c < 1e-3);
}

// (d) Y_lm against scipy: our angular part = (a positive constant for each l, |m|) x r^l x Y_lm, so the ratio must be positive, real and the same in every direction.
static void testAgainstScipy() {
    double worstConst = 0, worstPhase = 0; int n = 0; bool equalPlusMinus = true;
    for (const YRef& y : kYRef) {
        double first = 0;
        for (int k = 0; k < 6; k++) {
            const double th = kYDir[k][0], ph = kYDir[k][1], r = 1.7;
            const Cd mine = angularComplex(y.l, y.m, r * std::sin(th) * std::cos(ph), r * std::sin(th) * std::sin(ph), r * std::cos(th));
            const Cd ref = std::pow(r, y.l) * Cd(y.re[k], y.im[k]);
            if (std::abs(ref) < 1e-9) continue;                           // (a node of this state at that direction)
            const Cd ratio = mine / ref;
            worstPhase = std::max(worstPhase, std::fabs(ratio.imag()) / std::abs(ratio));   // 0 when the phase convention matches (the sign too: ratio > 0)
            if (ratio.real() <= 0) worstPhase = 1;
            if (first == 0) first = ratio.real(); else worstConst = std::max(worstConst, std::fabs(ratio.real() / first - 1));
            n++;
        }
        if (y.m < 0) {                                                     // the +m and -m states have the same scale
            for (const YRef& z : kYRef) if (z.l == y.l && z.m == -y.m) {
                const Cd a = angularComplex(y.l, y.m, 1.0, 0.3, 0.5), b = angularComplex(z.l, z.m, 1.0, 0.3, 0.5);
                equalPlusMinus = equalPlusMinus && std::fabs(std::abs(a) / std::abs(b) - 1) < 1e-12;
            }
        }
    }
    std::printf("complex orbitals vs scipy %s: %d values, ratio non-constant by at most %.1e, phase off by at most %.1e rad\n", "1.17.1 sph_harm_y", n, worstConst, worstPhase);
    // limit 1e-12: scipy's values are given to 17 digits and our formula is a short polynomial, so only rounding remains.
    checkTrue("Y_lm: our angular parts are scipy's Y_lm times a positive constant (same phase, Condon-Shortley), for l = 1..3, m = +-1..+-l", n > 60 && worstConst < 1e-12 && worstPhase < 1e-12);
    checkTrue("Y_lm: the +m and -m states have the same size (Y_l,-m = (-1)^m conj(Y_lm))", equalPlusMinus);
}

// (e) |psi(m = +-1)|^2 = (|psi_x|^2 + |psi_y|^2) / 2 for p; and for every |m| the same with the real pair the state is made of.
static void testDensityFromReal() {
    double worst = 0;
    for (int n = 2; n <= 4; n++)
        for (int k = 0; k < 40; k++) {
            const double x = 0.4 + 0.37 * k, y = -1.1 + 0.29 * k, z = 0.9 - 0.31 * k;
            const int pShell = orbIndex(std::to_string(n) + "p"), px = orbIndex(std::to_string(n) + "p|x"), py = orbIndex(std::to_string(n) + "p|y");
            const double want = (std::pow(psi(px, 1, x, y, z), 2) + std::pow(psi(py, 1, x, y, z), 2)) / 2;
            for (int m : {1, -1}) worst = std::max(worst, std::fabs(std::norm(psiComplex(complexState(pShell, m), 1, x, y, z)) - want) / (std::fabs(want) + 1e-300));
        }
    // d and f: |psi(+-m)|^2 = (Re^2 + (c Im)^2) / 2 with the pair (Re, Im, c): d1 (xz, yz, 1), d2 (x2-y2, xy, 2), f1 (xz2, yz2, 1), f2 (zx2y2, xyz, 2), f3 (xx23y2, y3x2y2, 1)
    struct Pair { const char* shell; int m; const char* re; const char* im; double c; } pairs[] = {
        {"3d", 1, "3d|xz", "3d|yz", 1}, {"3d", 2, "3d|x2y2", "3d|xy", 2}, {"4d", 1, "4d|xz", "4d|yz", 1}, {"4d", 2, "4d|x2y2", "4d|xy", 2},
        {"4f", 1, "4f|xz2", "4f|yz2", 1}, {"4f", 2, "4f|zx2y2", "4f|xyz", 2}, {"4f", 3, "4f|xx23y2", "4f|y3x2y2", 1}};
    double worstD = 0;
    for (const Pair& pr : pairs)
        for (int k = 0; k < 40; k++) {
            const double x = 0.5 + 0.41 * k, y = -0.7 + 0.23 * k, z = 1.3 - 0.37 * k;
            const double want = (std::pow(psi(pr.re, 1, x, y, z), 2) + std::pow(pr.c * psi(pr.im, 1, x, y, z), 2)) / 2;
            for (int s : {1, -1}) worstD = std::max(worstD, std::fabs(std::norm(psiComplex(complexState(orbIndex(pr.shell), s * pr.m), 1, x, y, z)) - want) / (std::fabs(want) + 1e-300));
        }
    std::printf("complex orbitals, density from the real ones: p: %.1e, d and f: %.1e (largest relative difference)\n", worst, worstD);
    checkTrue("|psi(m = +-1)|^2 = (|psi_x|^2 + |psi_y|^2) / 2 for 2p, 3p, 4p (to 1e-12)", worst < 1e-12);
    checkTrue("|psi(+-m)|^2 = (Re^2 + (c Im)^2) / 2 for the d and f states (to 1e-12)", worstD < 1e-12);
}

// (f) the real orbitals' dots are unchanged bit for bit, with Phase colours = Sign: the positions and signs of 4,800 dots (300 steps, seed 12345) hashed (FNV-1a 64) were
// recorded from the code BEFORE this change (version 0.10.1). (The 17,818 existing checks also compare whole runs with the web version's.)
// 0.10.6 stage 2: the dots are stored in METRES, and the Bohr radius changed (0.529 Å -> CODATA 2022), so every stored float changed and the six hashes below were recorded again from the new code.
// What did NOT change is checked directly: three dots of each run (the 1st, 1,001st and 4,800th) in Bohr radii as the 0.10.5 code made them (pts / its 0.529e-10 m, printed with 9 digits) are compared
// with the new dots in Bohr radii (pts / kA0) to 1e-6 (a float has 6e-8). The two builds also agreed on all 288,000 stored values to 1.2e-7 (largest relative difference, measured once).
static const double kOldDots[6][3][3] = {{{-8.49509131, 4.58798511, -0.172708282}, {-6.44851532, -6.55992851, -6.97008981}, {1.90480114, -0.842173815, -1.21785222}},
      {{-4.65779379, 4.7618688, -4.54753542}, {-2.61313786, -5.34231817, -9.28414245}, {-5.28331002, -0.380771737, -4.61095193}},
      {{3.57307231, 6.31028783, 6.82481534}, {-6.70778965, -6.43479966, -15.6323054}, {4.5692939, -3.5280141, -7.98969271}},
      {{2.59999174, 0.154706657, 0.266091743}, {-5.94455697, -1.49512373, -5.18749452}, {2.37630384, -0.308817178, -0.0588654883}},
      {{-5.36542875, -8.61586375, -11.5972785}, {-11.7242227, -12.4388013, -24.8153651}, {-12.508972, -14.3252507, -13.5947617}},
      {{5.01336003, 2.34338462, 11.8061452}, {-5.89072322, 2.22509471, -10.7500954}, {10.365039, -4.37580081, 8.63580788}}};
static uint64_t fnv(const void* d, std::size_t n, uint64_t h = 1469598103934665603ULL) { const unsigned char* p = static_cast<const unsigned char*>(d); for (std::size_t i = 0; i < n; i++) { h ^= p[i]; h *= 1099511628211ULL; } return h; }
static void testRealDotsUnchanged() {
    struct C { const char* orb; const char* key; const char* ori; int Z; uint64_t hash; } cs[] = {
{"2p", "pAx", "x", 1, 0xc93e0afcd9cce69aULL},   // 4800 dots
    {"2p", "pAx", "z", 1, 0x0023e5319ddd8eefULL},   // 4800 dots
    {"3d", "dAx", "xz", 1, 0xf8b6a385a90a4162ULL},   // 4800 dots
    {"3d", "dAx", "x2y2", 2, 0xb633d809a5b2079fULL},   // 4800 dots
    {"4f", "fAx", "xyz", 1, 0xe0f89b80a9f6b154ULL},   // 4800 dots
    {"3s", "", "", 1, 0x514506a3ffd7ba46ULL},   // 4800 dots
    };
    for (const C& c : cs) {
        seedRandom(12345);
        AtomLab lab; lab.nowMs = [] { return 0.0; };
        lab.p = AtomParams(); lab.paused = false; lab.restart();
        lab.setParam("view", std::string("Cloud")); lab.setParam("Z", double(c.Z)); lab.setParam("orb", std::string(c.orb));
        if (*c.key) lab.setParam(c.key, std::string(c.ori));
        for (int i = 0; i < 300; i++) lab.step(1.0 / 240);
        uint64_t h = fnv(lab.s.pts.data(), lab.s.pts.size() * sizeof(float));
        h = fnv(lab.s.sg.data(), lab.s.sg.size(), h);
        checkTrue(std::string("real orbital dots unchanged (positions and signs) from 0.10.1: ") + c.orb + " " + c.ori + " Z=" + std::to_string(c.Z), h == c.hash);
        {
            const int ci = int(&c - cs); const long idx[3] = {0, 1000, 4799}; double worst = 0;
            for (int d = 0; d < 3; d++) for (int a = 0; a < 3; a++) worst = std::max(worst, std::fabs(lab.s.pts[3 * idx[d] + a] / kA0 / kOldDots[ci][d][a] - 1));
            checkTrue(std::string("real orbital dots in Bohr radii equal the 0.10.5 ones (1e-6): ") + c.orb + " " + c.ori, worst < 1e-6);
        }
        // the new phase of each dot of a real orbital is 0 where the sign is + and pi where it is -
        bool ok = lab.s.ph.size() == lab.s.sg.size();
        for (std::size_t i = 0; ok && i < std::size_t(lab.s.k) && i < lab.s.sg.size(); i++) ok = lab.s.ph[i] == (lab.s.sg[i] > 0 ? 0.f : float(kPi));
        checkTrue(std::string("real orbital dots carry phase 0 or pi matching their sign: ") + c.orb + " " + c.ori, ok);
    }
}

// complex clouds: the dots carry arg(psi) of the state at their place; for 2p m = +-1 (no radial node, no theta sign change) that is m phi + (pi for +m)
static void testComplexDots() {
    for (int m : {1, -1}) {
        seedRandom(777);
        AtomLab lab; lab.nowMs = [] { return 0.0; };
        lab.p = AtomParams(); lab.paused = false; lab.restart();
        lab.setParam("view", std::string("Cloud")); lab.setParam("orb", std::string("2p")); lab.setParam("pAx", oriKey(m));
        for (int i = 0; i < 300; i++) lab.step(1.0 / 240);
        const long cnt = std::min<long>(lab.s.k, long(lab.s.ph.size()));
        double worst = 0, worstArg = 0; long usable = 0;
        double sxx = 0, syy = 0, szz = 0;
        for (long i = 0; i < cnt; i++) {
            const double x = lab.s.pts[3 * i] / kA0, y = lab.s.pts[3 * i + 1] / kA0, z = lab.s.pts[3 * i + 2] / kA0;
            sxx += x * x; syy += y * y; szz += z * z;
            const double phi = std::atan2(y, x), want = m * phi + (m > 0 ? kPi : 0);
            double d = std::fmod(lab.s.ph[i] - want, 2 * kPi); if (d > kPi) d -= 2 * kPi; if (d < -kPi) d += 2 * kPi;
            worst = std::max(worst, std::fabs(d)); usable++;
            const Cd v = psiComplex(complexState(orbIndex("2p"), m), 1, x, y, z);
            double e = std::fmod(lab.s.ph[i] - std::arg(v), 2 * kPi); if (e > kPi) e -= 2 * kPi; if (e < -kPi) e += 2 * kPi;
            worstArg = std::max(worstArg, std::fabs(e));
        }
        std::printf("complex 2p m = %+d cloud: %ld dots, phase vs m phi (+pi): %.1e rad, vs arg(psi) at the dot: %.1e rad; <x^2>:<y^2>:<z^2> = %.2f : %.2f : %.2f\n", m, usable, worst, worstArg, sxx / usable, syy / usable, szz / usable);
        // limit 1e-4 rad: the dot positions are stored as 32-bit numbers (relative 6e-8), which moves the angle by about 1e-7.
        checkTrue("complex 2p dots carry arg(psi): m phi (+ pi for m > 0), m = " + std::to_string(m), usable > 3000 && worst < 1e-4 && worstArg < 1e-4);
        // the cloud is a doughnut round the z axis (|psi|^2 = f(r) sin^2(theta) for m = +-1): <x^2> = <y^2> (round the axis), both larger than <z^2> (flat)
        // (2p m = +-1: exact ratios <x^2> : <y^2> : <z^2> = 1 : 1 : 0.5 for the density sin^2(theta) f(r); the dots are a Metropolis sample, so the limits are loose: 15 % on x:y, z below 0.7 of x)
        checkTrue("complex 2p cloud: round the z axis (<x^2> = <y^2> within 15 %) and flat (<z^2> < 0.7 <x^2>)", std::fabs(sxx / syy - 1) < 0.15 && szz < 0.7 * sxx);
    }
}

// the colour map: against scikit-image 0.26.0 (skimage.color.lab2rgb, D65, clipped to [0, 1]) for the same L*, C and hue; and it must be cyclic
static void testPhaseWheel() {
    struct Ref { bool light; int k; float rgb[3]; } ref[] = {
        {false, 0, {1.0f, 0.5345799f, 0.6996457f}}, {false, 1, {1.0f, 0.5809391f, 0.4366234f}}, {false, 2, {0.8014520f, 0.6817521f, 0.3069459f}}, {false, 3, {0.5022609f, 0.7556829f, 0.4189309f}},
        {false, 4, {0.0f, 0.7859955f, 0.6860444f}}, {false, 5, {0.0f, 0.7740075f, 0.9507833f}}, {false, 6, {0.3489737f, 0.7115779f, 1.0f}}, {false, 7, {0.8262902f, 0.6063740f, 0.9570877f}},
        {true, 0, {0.8252200f, 0.2892619f, 0.4946457f}}, {true, 1, {0.7853675f, 0.3583448f, 0.2163298f}}, {true, 2, {0.5808809f, 0.4771553f, 0.0f}}, {true, 3, {0.2601518f, 0.5491062f, 0.1877637f}},
        {true, 4, {0.0f, 0.5759984f, 0.4803519f}}, {true, 5, {0.0f, 0.5678036f, 0.7585976f}}, {true, 6, {0.0f, 0.5093147f, 0.8776893f}}, {true, 7, {0.6184071f, 0.3892127f, 0.7642639f}}};
    double worst = 0;
    for (const Ref& r : ref) {
        const PhaseRgb c = phaseRgb(2 * kPi * r.k / 8, r.light);
        worst = std::max({worst, double(std::fabs(c.r - r.rgb[0])), double(std::fabs(c.g - r.rgb[1])), double(std::fabs(c.b - r.rgb[2]))});
    }
    // cyclic, and the four marked phases (0, pi/2, pi, 3 pi/2) have clearly different colours
    double cyc = 0, minApart = 9;
    for (bool light : {false, true})
        for (int k = 0; k < 40; k++) {
            const double a = 0.37 * k - 5;
            const PhaseRgb x = phaseRgb(a, light), y = phaseRgb(a + 2 * kPi, light);
            cyc = std::max({cyc, double(std::fabs(x.r - y.r)), double(std::fabs(x.g - y.g)), double(std::fabs(x.b - y.b))});
        }
    for (bool light : {false, true})
        for (int i = 0; i < 4; i++)
            for (int j = i + 1; j < 4; j++) {
                const PhaseRgb x = phaseRgb(i * kPi / 2, light), y = phaseRgb(j * kPi / 2, light);
                minApart = std::min(minApart, std::sqrt(std::pow(x.r - y.r, 2) + std::pow(x.g - y.g, 2) + std::pow(x.b - y.b, 2)));
            }
    bool bins = true;
    for (int b = 0; b < kPhaseBins; b++) bins = bins && phaseBin(phaseOfBin(b)) == b && phaseBin(phaseOfBin(b) + 2 * kPi) == b && phaseBin(phaseOfBin(b) - 2 * kPi) == b;
    bins = bins && phaseBin(kPi) == kPhaseBins / 2 && phaseBin(-kPi) == kPhaseBins / 2 && phaseBin(0.0) == 0 && phaseBin(-1e-9) == 0;
    std::printf("phase wheel: largest difference from scikit-image %.1e, cyclic to %.1e, nearest two of the four marked colours %.2f apart (sRGB)\n", worst, cyc, minApart);
    // limit 2e-3: scikit-image inverts the sRGB matrix to more digits than the 4-digit matrix written here (about 1e-4 per channel), and both clip the same way.
    checkTrue("phase wheel: colours equal scikit-image's CIELAB circle (2e-3)", worst < 2e-3);
    checkTrue("phase wheel: cyclic (phase and phase + 2 pi give the same colour)", cyc < 1e-9);
    // limit 0.25: the four marked hues are 90 degrees apart on a circle of chroma 52-58 (colour differences of 70-80 in L*a*b* units), far more than a visible step.
    checkTrue("phase wheel: the four marked phases have clearly different colours (> 0.25 apart in sRGB)", minApart > 0.25);
    checkTrue("phase wheel: the 24 colour bins are centred on their phases, wrap round, and pi is in bin 12", bins);
}

// the 90 % outline works for a complex state too: it holds 90 % of the probability (the grid's own count), is round about the z axis, and the density is as the cloud's
static void testComplexOutline() {
    const int id = complexState(orbIndex("2p"), 1), real = orbIndex("2p|z");
    const Outline& O = outlineOf(id);
    const double norm = orbitalNorm(id);
    // the norm by another way: the numerical integral of |psi|^2 (Z = 1) over the sphere shell by shell: radial integral of r^2 R^2 times the angular mean of |A|^2 times 4 pi
    double s = 0;
    for (double r = 0.005; r < 40; r += 0.01) s += std::pow(radial(orbitals()[orbIndex("2p")], r), 2) * r * r * r * r * 0.01;     // r^(2l+2) with l = 1
    // |A|^2 for m = +1: (x^2 + y^2) / 2 averaged over the sphere = r^2 / 3 (mean of sin^2(theta) = 2/3, halved): angular mean 1/3
    const double expect = s * 4 * kPi / 3;
    std::printf("complex 2p m = +1 outline: holds %.3f of the probability, reaches %.2f a0; norm %.6g, by hand %.6g\n", O.mass, O.rmax, norm, expect);
    // limit 0.02 on the mass: the grid has 64 voxels per side, and the real orbitals' own outlines are held to the same reason (their mass is within the same few percent of 0.9)
    checkTrue("complex 2p outline holds 90 % of the probability (0.88 to 0.92, from the 64^3 grid)", O.mass > 0.88 && O.mass < 0.92);
    checkTrue("complex 2p norm equals the by-hand radial x angular integral (1e-3)", std::fabs(norm / expect - 1) < 1e-3);
    checkTrue("complex 2p outline: a doughnut round z: its side view reaches farther across (x) than up (z)", outlineOf(real).rmax > 0 && O.rmax > 0);
    double ax = 0, az = 0;
    for (const OutSeg2& g : O.side) { ax = std::max({ax, double(std::fabs(g.x0)), double(std::fabs(g.x1))}); az = std::max({az, double(std::fabs(g.y0)), double(std::fabs(g.y1))}); }
    checkTrue("complex 2p m = +1: the region is wider across than tall (|psi|^2 ~ sin^2(theta) f(r))", ax > az);
}

// ================= Quantum beats: a superposition of two orbitals (0.10.2, stage 2) =================
// psi(t) = a psiA e^(-i EA t/hbar) + b psiB e^(-i EB t/hbar), shown relative to state A (multiplied by e^(+i EA t/hbar)), so the colours turn only at the beat frequency.
static Superposition mkSup(const char* shellA, const char* oriA, const char* shellB, const char* oriB, double mixB) {
    const auto id = [](const char* shell, const char* ori) { int m = 0; return oriIsComplex(ori, m) ? complexState(orbIndex(shell), m) : orbIndex(std::string(shell) + "|" + ori); };
    Superposition s; s.idA = id(shellA, oriA); s.idB = id(shellB, oriB); s.a = std::sqrt(1 - mixB); s.b = std::sqrt(mixB);
    return s;
}
// <z> of the superposition at phase theta = dE t / hbar, by quadrature (axisymmetric states: one phi point)
static double expectZ(const Quad& q, const Superposition& su, int Z, double theta) {
    double s = 0;
    for (std::size_t i = 0; i < q.r.size(); i++)
        for (std::size_t j = 0; j < q.u.size(); j++) {
            const double st = std::sqrt(1 - q.u[j] * q.u[j]), x = q.r[i] * st, z = q.r[i] * q.u[j];
            s += std::norm(psiSuper(su, Z, theta, x, 0, z)) * z * 2 * kPi * q.wu[j] * q.r[i] * q.r[i] * q.wr[i];
        }
    return s;
}

static void testBeatPeriodAndAmplitude() {
    const Superposition su = mkSup("1s", "", "2p", "z", 0.5);
    const Quad q = makeQuad(1500, 1, 40);
    // independent hydrogen functions written here: R10 = 2 e^-r, R21 = r e^(-r/2) / (2 sqrt 6), Y00 = 1/sqrt(4 pi), Y10 = sqrt(3/(4 pi)) cos(theta)
    double radial = 0;
    // own numerical integration of <1s|z|2p0>: radial part by the midpoint rule, angular part by Gauss-Legendre in cos(theta) (the pieces are the textbook ones above)
    const int nr = 200000; const double top = 60, dr = top / nr;
    for (int i = 0; i < nr; i++) { const double r = (i + 0.5) * dr; radial += 2 * std::exp(-r) * (r * std::exp(-r / 2) / (2 * std::sqrt(6.0))) * r * r * r * dr; }
    double ang = 0;
    for (std::size_t j = 0; j < q.u.size(); j++) ang += std::sqrt(1 / (4 * kPi)) * std::sqrt(3 / (4 * kPi)) * q.u[j] * q.u[j] * 2 * kPi * q.wu[j];
    const double mOwn = radial * ang, mClosed = 128 * std::sqrt(2.0) / 243;
    // the lab's amplitude of <z>(t) = 2 a b M cos(theta): at theta = 0 (Z = 1)
    const double zLab = expectZ(q, su, 1, 0), zQuarter = expectZ(q, su, 1, kPi / 2);
    std::printf("beats 1s + 2p(m=0), a = b: matrix element by my own integral %.6f a0, closed form 128 sqrt2/243 = %.6f; the lab's <z>(0) = %.6f (2ab M = %.6f), <z> at a quarter period %.1e\n", mOwn, mClosed, zLab, 2 * 0.5 * mClosed, zQuarter);
    // limit 1e-4 as asked: the midpoint rule with 200,000 steps and 24 Gauss points are far better than that.
    checkTrue("beats: <1s|z|2p0> by my own numerical integration equals 128 sqrt2 / 243 a0 = 0.74494 a0 (Fitzpatrick, 2P-1S transitions; 1e-4)", std::fabs(mOwn / mClosed - 1) < 1e-4);
    checkTrue("beats: the amplitude of <z>(t) of 1s + 2p0 (a = b) is 2ab x 128 sqrt2/243 a0 (1e-4)", std::fabs(zLab / (2 * 0.5 * mClosed) - 1) < 1e-4);
    checkTrue("beats: <z> is zero a quarter period later (|<z>| < 1e-4 a0)", std::fabs(zQuarter) < 1e-4);
    // amplitude with other mixes: 2 a b M for a^2 = 0.8 / 0.2
    const double z2 = expectZ(q, mkSup("1s", "", "2p", "z", 0.2), 1, 0);
    checkTrue("beats: the amplitude follows 2ab: mix 20 % gives 2 sqrt(0.2 x 0.8) M (1e-4)", std::fabs(z2 / (2 * std::sqrt(0.2 * 0.8) * mClosed) - 1) < 1e-4);
    // the period: found from the zero of <z>(t) in real seconds (a quarter period), against h / (E2 - E1) written here from the CODATA 2022 values of the nist:: block above (NIST pages h, e, rydhcev, mpsme, read 2026-10-04)
    const double hJs = nist::h, eV = nist::e, RyInf = nist::rydEV, mpMe = nist::mpOverMe;
    const double RyRed = RyInf * mpMe / (mpMe + 1);                       // reduced-mass Rydberg energy for hydrogen (proton), eV
    const double Tref = hJs / ((RyRed * eV) * (1.0 - 0.25));              // E2 - E1 = Ry (1 - 1/4)
    double lo = 0, hi = 0.5 * Tref;                                        // <z> goes from + to - between t = 0 and T/2: bisection for the first zero
    for (int it = 0; it < 60; it++) {
        const double mid = 0.5 * (lo + hi);
        const double th = beatEV(1, 0, su) * mid / (kPlanckEVs / (2 * kPi));
        (expectZ(q, su, 1, th) > 0 ? lo : hi) = mid;
    }
    const double Tmeas = 4 * 0.5 * (lo + hi);
    std::printf("beats 1s + 2p, hydrogen: period from the zero of <z>(t) %.9e s, h/(E2-E1) from CODATA %.9e s (relative difference %.1e); beatPeriodS %.9e\n", Tmeas, Tref, std::fabs(Tmeas / Tref - 1), beatPeriodS(1, 0, su));
    // limit 1e-6 as asked (the lab's constants are the CODATA 2022 ones since 0.10.6 stage 2, so the real difference is far smaller).
    checkTrue("beats: the period of <z>(t) is h / (E2 - E1) (CODATA 2022 h, Rydberg energy, proton mass; 1e-6)", std::fabs(Tmeas / Tref - 1) < 1e-6 && std::fabs(beatPeriodS(1, 0, su) / Tref - 1) < 1e-6);
    checkTrue("beats: about 4e-16 s for hydrogen 1s + 2p (the owner's h / 10.204 eV)", Tmeas > 4.0e-16 && Tmeas < 4.1e-16);
    // Z = 2: a quarter of the period, up to the reduced-mass difference between hydrogen and helium
    const double T2 = beatPeriodS(2, 2, su);
    std::printf("beats Z = 2 (helium-4 ion): period %.6e s, ratio to Z = 1: %.6f (1/4 = 0.25)\n", T2, T2 / beatPeriodS(1, 0, su));
    // limit 1e-3: the reduced-mass factor differs between a proton and a helium-4 nucleus by 4.1e-4 (mu/m_e = M/(M+1)): 1/4 x (1 + 4.1e-4)
    checkTrue("beats: for Z = 2 the period is a quarter of Z = 1 (Z^2 scaling; 1e-3 for the reduced-mass difference)", std::fabs(T2 / beatPeriodS(1, 0, su) * 4 - 1) < 1e-3);
}

static void testBeatNormStationary() {
    const Quad q = makeQuad(450, 24, 60);
    double worst = 0;
    const Superposition su = mkSup("2p", "m+1", "3d", "m+2", 0.3);
    for (double th : {0.0, 1.0, 2.5, kPi}) {
        double s = 0;
        for (std::size_t i = 0; i < q.r.size(); i++)
            for (std::size_t j = 0; j < q.u.size(); j++) {
                const double st = std::sqrt(1 - q.u[j] * q.u[j]);
                for (int k = 0; k < q.nphi; k++) {
                    const double f = 2 * kPi * (k + 0.5) / q.nphi;
                    s += std::norm(psiSuper(su, 1, th, q.r[i] * st * std::cos(f), q.r[i] * st * std::sin(f), q.r[i] * q.u[j])) * (2 * kPi / q.nphi) * q.wu[j] * q.r[i] * q.r[i] * q.wr[i];
                }
            }
        worst = std::max(worst, std::fabs(s - 1));
    }
    std::printf("beats: integral of |psi(t)|^2 for 2p(m=+1) + 3d(m=+2), mix 30 %%, at 4 times: largest |norm - 1| = %.1e\n", worst);
    checkTrue("beats: the superposition stays normalised in time (integral of |psi|^2 = 1, 1e-3)", worst < 1e-3);
    // a single orbital (mix 0 or 100 %): the density does not change with time; 2s + 2p: same energy, so no beats at all
    bool still = true;
    for (const Superposition& one : {mkSup("1s", "", "2p", "z", 0.0), mkSup("1s", "", "2p", "z", 1.0)})
        for (int k = 0; k < 20; k++) { const double x = 0.3 + 0.4 * k, y = 0.2 * k - 1, z = 1.1 - 0.17 * k; still = still && std::fabs(std::norm(psiSuper(one, 1, 0.0, x, y, z)) / std::norm(psiSuper(one, 1, 2.2, x, y, z)) - 1) < 1e-12; }
    checkTrue("single orbital (mix 0 % or 100 %): the density does not change with time (1e-12)", still);
    const Superposition same = mkSup("2s", "", "2p", "z", 0.5);
    bool still2 = beatEV(1, 0, same) == 0 && std::isinf(beatPeriodS(1, 0, same));
    for (int k = 0; k < 20; k++) { const double x = 0.3 + 0.4 * k, y = 0.2 * k - 1, z = 1.1 - 0.17 * k; still2 = still2 && std::norm(psiSuperT(same, 1, 0, 0.0, x, y, z)) == std::norm(psiSuperT(same, 1, 0, 3.7e-16, x, y, z)); }
    checkTrue("2s + 2p: same n, same energy in this model: no beats, the density does not change (exactly)", still2);
    // <z> of a single orbital is constant too
    const Quad qz = makeQuad(900, 1, 40);
    const double z0 = expectZ(qz, mkSup("1s", "", "2p", "z", 0.0), 1, 0.0), z1 = expectZ(qz, mkSup("1s", "", "2p", "z", 0.0), 1, 1.7);
    checkTrue("single orbital: <z> does not change with time", std::fabs(z0 - z1) < 1e-12);
    // the dipole check used for the text: 1s + 2p(z) moves along z; 1s + 2s does not move; 1s + 2p(m=+1) moves in the x-y plane
    double d[3];
    checkTrue("dipole: 1s + 2pz couples along z only", dipoleAxes(mkSup("1s", "", "2p", "z", 0.5), d) && std::fabs(d[2]) > 0.1 && d[0] < 1e-3 && d[1] < 1e-3);   // (1e-3: the 1,500-point sphere sum is exact to about 1e-4)
    checkTrue("dipole: 1s + 2s has none, 1s + 3d has none, 2p + 3d does", !dipoleAxes(mkSup("1s", "", "2s", "", 0.5), d) && !dipoleAxes(mkSup("1s", "", "3d", "z2", 0.5), d) && dipoleAxes(mkSup("2p", "z", "3d", "z2", 0.5), d));
    checkTrue("dipole: 1s + 2p(m=+1) moves in the x-y plane, not along z", dipoleAxes(mkSup("1s", "", "2p", "m+1", 0.5), d) && d[2] < 1e-3 && d[0] > 0.1 && d[1] > 0.1);
}

// the lab: dots (seeded) follow the CURRENT density. One dot per independent run (the chain's own dots are correlated), a fresh lab each.
static void superSetup(AtomLab& lab, const char* a, const char* b, double mixPct, int Z = 1) {
    lab.nowMs = [] { return 0.0; };
    lab.p = AtomParams(); lab.paused = false; lab.restart();
    lab.setParam("view", std::string("Cloud")); lab.setParam("Z", double(Z));
    lab.setParam("orb", std::string(a)); lab.setParam("orb2", std::string(b)); lab.setParam("mix", mixPct); lab.setParam("super", std::string("On"));
}
static void testSuperDots() {
    // the rate of new dots (the measurement behind the design): dots recorded per physics step and per second at 240 steps/s, and how long the ring buffer remembers
    for (const char* dens : {"Normal", "High", "Ultra"}) {
        AtomLab lab; lab.nowMs = [] { return 0.0; }; lab.p = AtomParams(); lab.restart(); lab.setParam("view", std::string("Cloud")); lab.setParam("cdens", std::string(dens));
        const long k0 = lab.s.k; for (int i = 0; i < 60; i++) lab.step(1.0 / 240);
        const double perStep = double(lab.s.k - k0) / 60, perSec = perStep * 240;
        std::printf("dot rate, density %s: %.0f dots per physics step, %.0f dots/s, the ring of %d dots remembers %.2f s (a 4 s beat needs well under 0.4 s)\n", dens, perStep, perSec, lab.cloudCap(), lab.cloudCap() / perSec);
    }
    // Several full rings of 1s + 2p(z), a = b, seeded. The dots are a Metropolis chain, so neighbours are correlated: only every 400th dot is used (slots with different fixed random
    // numbers; the measured correlation of z falls like exp(-lag / 60), 0.75 at lag 16), after checking the correlation of z at that lag. The lab's own thinning (superShown) decides which are on screen.
    seedRandom(2468);
    const double M = 128 * std::sqrt(2.0) / 243;
    const double thetas[4] = {0.0, 1.0, 2.2, 3.1};
    double sum[4] = {0, 0, 0, 0}, sum2[4] = {0, 0, 0, 0}; long nShown[4] = {0, 0, 0, 0};
    const int nb = 20; const double rmax = 12;
    std::vector<double> obs[4]; for (auto& o : obs) o.assign(nb, 0);
    double worstPhase = 0, rho1 = 0, rho16 = 0, rho64 = 0, rhoSum = 0; long usedDots = 0, nPhase = 0;
    const int nLab = 170, stride = 400;
    for (int k = 0; k < nLab; k++) {
        AtomLab lab; superSetup(lab, "1s", "2p", 50);
        for (int i = 0; i < 800; i++) lab.step(1.0 / 240);           // fills the 12,000-dot ring
        const long cap = std::min<long>(lab.s.k, long(lab.s.sg.size()));
        const Superposition su = lab.superState();
        const double nA = normFactor(su.idA, 1), nB = normFactor(su.idB, 1);
        {                                                              // autocorrelation of z along the chain at lags 1, 16, 400 (400: averaged over all the labs)
            double m = 0, v = 0; for (long i = 0; i < cap; i++) m += lab.s.pts[3 * i + 2] / kA0; m /= cap;
            for (long i = 0; i < cap; i++) { const double d = lab.s.pts[3 * i + 2] / kA0 - m; v += d * d; }
            const auto rho = [&](long lag) { double c = 0; for (long i = 0; i + lag < cap; i++) c += (lab.s.pts[3 * i + 2] / kA0 - m) * (lab.s.pts[3 * (i + lag) + 2] / kA0 - m); return c / v; };
            if (k == 0) { rho1 = rho(1); rho16 = rho(16); } rhoSum += rho(400);
        }
        for (long i = (long(k) * 7919) % stride; i < cap; i += stride) {      // (a different start slot in every lab, so every dot has its own fixed random number)
            const double x = lab.s.pts[3 * i] / kA0, y = lab.s.pts[3 * i + 1] / kA0, z = lab.s.pts[3 * i + 2] / kA0, r = std::sqrt(x * x + y * y + z * z);
            usedDots++;
            for (int t = 0; t < 4; t++) {
                if (lab.superShown(i, thetas[t])) {
                    sum[t] += z; sum2[t] += z * z; nShown[t]++;
                    if (r < rmax) obs[t][std::size_t(r / rmax * nb)] += 1;
                }
                if (k < 12) {                                          // the colour phase against the exact phase from the primitives (stored 32-bit values, so not exact to rounding)
                    const Cd A = nA * psi(su.idA, 1, x, y, z), B = nB * psi(su.idB, 1, x, y, z);
                    const Cd exact = su.a * A + su.b * B * std::exp(Cd(0, -thetas[t]));
                    double d2 = std::fmod(lab.superPhase(i, thetas[t]) - std::arg(exact), 2 * kPi); if (d2 > kPi) d2 -= 2 * kPi; if (d2 < -kPi) d2 += 2 * kPi;
                    worstPhase = std::max(worstPhase, std::fabs(d2)); nPhase++;
                }
            }
        }
    }
    rho64 = rhoSum / nLab;
    std::printf("dot chain: autocorrelation of z at lag 1: %.2f, 16: %.2f, 400: %.3f (stride %d); %ld dots used\n", rho1, rho16, rho64, stride, usedDots);
    checkTrue("the dot chain is nearly uncorrelated at the stride used (|autocorrelation of z| < 0.05 at lag 400, averaged over the labs)", std::fabs(rho64) < 0.05);
    // limit 1e-4 rad: the stored position is a 32-bit float (relative 6e-8) and the stored amplitudes are 32-bit too, so the phase moves by about 1e-6
    std::printf("dot colour phase vs the exact arg(psi(x, t)) relative to state A: %ld dot-times, largest difference %.1e rad\n", nPhase, worstPhase);
    checkTrue("a dot's colour phase equals the exact arg(psi(x, t)) relative to state A at 4 times (1e-4 rad)", nPhase > 1000 && worstPhase < 1e-4);
    double worstZ = 0, worstChi = 0;
    for (int t = 0; t < 4; t++) {
        const double mean = sum[t] / nShown[t], sd = std::sqrt(sum2[t] / nShown[t] - mean * mean), se = sd / std::sqrt(double(nShown[t])), exact = 2 * 0.5 * M * std::cos(thetas[t]);
        worstZ = std::max(worstZ, std::fabs(mean - exact) / se);
        // the exact radial density a^2 P1s(r) + b^2 P2p(r) (the cross term has no radial part), 20 bins on 0 to 12 a0
        std::vector<double> ex(nb, 0); double tot = 0, nin = 0;
        for (int b = 0; b < nb; b++) {
            double P = 0; const int sub = 200;
            for (int j = 0; j < sub; j++) { const double r = (b + (j + 0.5) / sub) * rmax / nb; P += (0.5 * 4 * r * r * std::exp(-2 * r) + 0.5 * r * r * r * r * std::exp(-r) / 24) * rmax / nb / sub; }
            ex[std::size_t(b)] = P; tot += P; nin += obs[t][std::size_t(b)];
        }
        double chi = 0;
        for (int b = 0; b < nb; b++) { const double e = nin * ex[std::size_t(b)] / tot; chi += (obs[t][std::size_t(b)] - e) * (obs[t][std::size_t(b)] - e) / e; }
        worstChi = std::max(worstChi, chi);
        std::printf("dots at theta = %.1f: %ld of %ld shown, mean z %.4f +- %.4f, exact <z> %.4f; radial histogram (20 bins) chi-square %.1f\n", thetas[t], nShown[t], usedDots, mean, se, exact, chi);
    }
    // limits: 4 standard errors as asked; chi-square with 19 degrees of freedom: its 99.99 % point is 48, so a good match stays below it
    checkTrue("shown dots: the mean z follows the exact <z>(t) within 4 standard errors at 4 times", worstZ < 4);
    checkTrue("shown dots: the radial histogram matches the exact radial density (chi-square below 48, 19 dof) at 4 times", worstChi < 48);
    // the cost of colouring and thinning every dot of a High-density cloud each frame (this machine, software, one core: not your PC)
    AtomLab big; superSetup(big, "1s", "2p", 50); big.setParam("cdens", std::string("High")); superSetup(big, "1s", "2p", 50); big.setParam("cdens", std::string("High"));
    for (int i = 0; i < 800; i++) big.step(1.0 / 240);
    const long cap = std::min<long>(big.s.k, long(big.s.sg.size()));
    const auto t0 = std::chrono::steady_clock::now();
    long keepCount = 0; const int frames = 20;
    for (int f = 0; f < frames; f++) for (long i = 0; i < cap; i++) keepCount += big.superGroup(i, 0.1 * f) < 255;
    const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count() / frames;
    std::printf("cost of thinning and colouring %ld dots (High) per frame on this machine (software, one core): %.2f ms; %.0f%% shown\n", cap, ms, 100.0 * keepCount / (double(cap) * frames));
}

// ================= Smooth photon jumps (0.10.2, stage 3) =================
// Cloud views: psi = e^(i phi u) (cos(theta) A + sin(theta) B e^(-i phi)), theta = pi/2 smoothstep(t / 1.2 s), phi = the slowed beat phase. Two fixed populations of dots (the old
// cloud, and a new one of the target orbital); a dot is shown with the exact fractions of jumpFractions().
static double smoothstep(double u) { u = std::max(0.0, std::min(1.0, u)); return u * u * (3 - 2 * u); }

static void testJumpFractions() {
    uint64_t st = 88172645463325252ULL;
    const auto rnd = [&] { st ^= st << 13; st ^= st >> 7; st ^= st << 17; return double(st >> 11) / 9007199254740992.0; };
    double worstSum = 0, worstRange = 0, worstEnd = 0; int n = 0;
    for (int k = 0; k < 20000; k++) {
        const Cd A(rnd() * 2 - 1, rnd() * 2 - 1), B(rnd() * 2 - 1, rnd() * 2 - 1);
        const double th = rnd() * kPi / 2, ph = rnd() * 2 * kPi;
        double fA, fB; jumpFractions(A, B, th, ph, fA, fB);
        const double rho = std::norm(std::cos(th) * A + std::sin(th) * B * std::exp(Cd(0, -ph)));
        worstSum = std::max(worstSum, std::fabs(std::norm(A) * fA + std::norm(B) * fB - rho) / (std::norm(A) + std::norm(B)));
        worstRange = std::max({worstRange, -fA, -fB, fA - 1, fB - 1});
        jumpFractions(A, B, 0, ph, fA, fB); worstEnd = std::max({worstEnd, std::fabs(fA - 1), std::fabs(fB)});
        jumpFractions(A, B, kPi / 2, ph, fA, fB); worstEnd = std::max({worstEnd, std::fabs(fA), std::fabs(fB - 1)});
        n++;
    }
    std::printf("jump fractions, %d random cases: |A|^2 fA + |B|^2 fB - |psi|^2 <= %.1e (relative to |A|^2 + |B|^2); outside [0, 1] by at most %.1e; endpoints off by %.1e\n", n, worstSum, worstRange, worstEnd);
    checkTrue("jump: the shown fractions of the two dot populations give exactly the density |psi|^2 (1e-12)", worstSum < 1e-12);
    checkTrue("jump: the fractions stay between 0 and 1 (Cauchy-Schwarz: |psi|^2 <= |A|^2 + |B|^2)", worstRange < 1e-12);
    // limit 1e-9: cos(pi/2) is 6e-17 in floating point, not 0, so the cross term at theta = pi/2 is ~1e-16 |A||B|, and divided by |A|^2 for the random test amplitudes (down to ~1e-4)
    // that is a few 1e-12. At theta = 0 exactly (the jump's first moment) the dots test below shows it is exact; the jump ends at theta = pi/2 by swapping in the new dots.
    checkTrue("jump: at the start only the old dots are shown (fA = 1, fB = 0) and at the end only the new ones (1e-9)", worstEnd < 1e-9);
}

static void jumpSetup(AtomLab& lab, const char* from, const char* fromOri, int Z = 1) {
    lab.nowMs = [] { return 0.0; };
    lab.p = AtomParams(); lab.paused = false; lab.restart();
    lab.setParam("view", std::string("Cloud")); lab.setParam("Z", double(Z));
    lab.setParam("orb", std::string(from));
    if (*fromOri) lab.setParam(orbitals()[orbIndex(from)].l == 1 ? "pAx" : orbitals()[orbIndex(from)].l == 2 ? "dAx" : "fAx", std::string(fromOri));
}
static void runSteps(AtomLab& lab, int n) { for (int i = 0; i < n; i++) lab.step(1.0 / 240); }

// norm along the jump, and the exact <r>: continuity per 1/60 s frame
static void testJumpNormAndContinuity() {
    AtomLab lab; jumpSetup(lab, "1s", "");
    runSteps(lab, 100);
    const int to = orbIndex("2p|z");
    lab.startJump("absorb", to);
    runSteps(lab, 400);                                                   // the photon flies in (1.6 s), then the jump starts
    checkTrue("jump: the cloud jump starts when the photon arrives", lab.s.trans.has_value() && lab.jumpCloud() && lab.s.trans->to == to);
    // own numbers: <r> of 1s and 2p by numerical integration of the textbook radial functions R10 = 2 e^-r, R21 = r e^(-r/2)/(2 sqrt 6)
    double rA = 0, rB = 0; const double dr = 1e-4;
    for (int i = 0; i < 400000; i++) { const double r = (i + 0.5) * dr; rA += std::pow(2 * std::exp(-r), 2) * r * r * r * dr; rB += std::pow(r * std::exp(-r / 2) / (2 * std::sqrt(6.0)), 2) * r * r * r * dr; }
    const double gap = rB - rA;
    // the norm at 21 times along the jump, and <r> = cos^2 rA + sin^2 rB against the full 2D integral (the cross term has no radial part)
    const Quad q = makeQuad(900, 1, 40);
    double worstNorm = 0, worstCross = 0, worstFrame = 0, prev = rA;
    for (int k = 0; k <= 72; k++) {
        lab.s.trans->t = std::min(1.2 - 1e-9, k / 60.0);                  // frame k at 60 frames a second
        const double u = lab.jumpU(), th = kPi / 2 * u, phi = lab.jumpPhi();
        const double rExact = std::cos(th) * std::cos(th) * rA + std::sin(th) * std::sin(th) * rB;
        worstFrame = std::max(worstFrame, std::fabs(rExact - prev)); prev = rExact;
        if (k % 6 == 0) {
            Superposition su; su.idA = orbIndex("1s"); su.idB = to; su.a = std::cos(th); su.b = std::sin(th);
            double nrm = 0, mr = 0;
            for (std::size_t i = 0; i < q.r.size(); i++)
                for (std::size_t j = 0; j < q.u.size(); j++) {
                    const double st = std::sqrt(1 - q.u[j] * q.u[j]), w = 2 * kPi * q.wu[j] * q.r[i] * q.r[i] * q.wr[i];
                    const double d = std::norm(psiSuper(su, 1, phi, q.r[i] * st, 0, q.r[i] * q.u[j]));
                    nrm += d * w; mr += d * w * q.r[i];
                }
            worstNorm = std::max(worstNorm, std::fabs(nrm - 1)); worstCross = std::max(worstCross, std::fabs(mr - rExact));
        }
    }
    std::printf("jump 1s -> 2p(z): <r> of 1s %.5f, of 2p %.5f a0 (by my own integral); norm along the jump: largest |norm - 1| = %.1e; <r> from the full integral vs cos^2 rA + sin^2 rB: %.1e; "
                "largest change of the exact <r> per 1/60 s frame: %.4f of the gap %.4f (limit 0.1)\n", rA, rB, worstNorm, worstCross, worstFrame / gap, gap);
    checkTrue("jump: <r> of 1s is 1.5 and of 2p is 5 a0 (own integral of the textbook radial functions, 1e-6)", std::fabs(rA - 1.5) < 1e-6 && std::fabs(rB - 5) < 1e-6);
    checkTrue("jump: the wave stays normalised all along the jump (integral of |psi|^2 = 1, 1e-3)", worstNorm < 1e-3);
    checkTrue("jump: <r> follows cos^2 rA + sin^2 rB (the cross term has no radial part, 1e-3 a0)", worstCross < 1e-3);
    checkTrue("jump: the exact <r> changes by less than a tenth of the endpoint gap per 1/60 s frame", worstFrame < 0.1 * gap);
    checkTrue("jump: the smooth step starts and ends flat (u = 0 at the start, 1 at the end; theta from 0 to pi/2)", smoothstep(0) == 0 && smoothstep(1) == 1);
}

// dots: endpoints and <r> in the middle, with independent dots (every 400th dot of full rings, a different start slot in every lab, so every dot has its own fixed random number)
static void testJumpDots() {
    seedRandom(13579);
    const int nLab = 120, stride = 400, nb = 20; const double rmax = 14;
    const double ths[5] = {0.0, 0.25, 0.5, 0.75, 1.0};
    struct Acc { double sumR = 0, sumR2 = 0; long n = 0; std::vector<double> hist; };
    std::vector<Acc> acc(5); for (auto& a : acc) a.hist.assign(nb, 0);
    long oldDotsSame = 0, oldDotsTotal = 0; bool noNewAtStart = true, noOldAtEnd = true; double rhoSum = 0;
    for (int k = 0; k < nLab; k++) {
        AtomLab lab; jumpSetup(lab, "1s", "");
        runSteps(lab, 800);                                               // the ring is full
        const long cap = std::min<long>(lab.s.k, long(lab.s.sg.size()));
        lab.startJump("absorb", orbIndex("2p|z"));
        runSteps(lab, 378);
        std::vector<float> before;
        for (int guard = 0; guard < 20 && !lab.s.trans; guard++) { before.assign(lab.s.pts.begin(), lab.s.pts.begin() + 3 * cap); lab.step(1.0 / 240); }   // the photon arrives: the jump begins (the ring is not sampled in that step)
        const long jn = lab.jumpDots();
        if (k == 0) rhoSum = double(jn);
        for (int t = 0; t < 5; t++) {
            lab.s.trans->t = t == 4 ? 1.2 - 1e-9 : ths[t] * 1.2;
            if (t == 0) lab.s.trans->t = 0;
            for (int pop = 0; pop < 2; pop++) {
                const float* P = pop ? lab.s.jp.data() : lab.s.pts.data();
                for (long i = (long(k) * 7919) % stride; i < jn; i += stride) {
                    if (!lab.jumpShown(pop, i)) continue;
                    const double x = P[3 * i] / kA0, y = P[3 * i + 1] / kA0, z = P[3 * i + 2] / kA0, r = std::sqrt(x * x + y * y + z * z);
                    acc[t].sumR += r; acc[t].sumR2 += r * r; acc[t].n++;
                    if (r < rmax) acc[t].hist[std::size_t(r / rmax * nb)] += 1;
                }
            }
        }
        // at the start every old dot is shown, none of the new ones, and the old dots are exactly the ones that were on screen; at the end the other way round
        lab.s.trans->t = 0;
        for (long i = 0; i < jn; i++) { oldDotsTotal++; if (lab.jumpShown(0, i) && !lab.jumpShown(1, i) && lab.s.pts[3 * i] == before[3 * i] && lab.s.pts[3 * i + 2] == before[3 * i + 2]) oldDotsSame++; else noNewAtStart = false; }
        lab.s.trans->t = 1.2 - 1e-9;
        for (long i = 0; i < jn; i += 7) if (lab.jumpShown(0, i) || !lab.jumpShown(1, i)) noOldAtEnd = false;
    }
    (void)rhoSum;
    // exact radial densities: P1s = 4 r^2 e^-2r, P2p = r^4 e^-r / 24; at theta the radial density is cos^2 P1s + sin^2 P2p (no cross term)
    const double rA = 1.5, rB = 5.0;
    const auto radialExact = [&](double th, std::vector<double>& ex) {
        ex.assign(nb, 0); double tot = 0;
        for (int b = 0; b < nb; b++) {
            double P = 0; const int sub = 200;
            for (int j = 0; j < sub; j++) { const double r = (b + (j + 0.5) / sub) * rmax / nb; P += (std::cos(th) * std::cos(th) * 4 * r * r * std::exp(-2 * r) + std::sin(th) * std::sin(th) * std::pow(r, 4) * std::exp(-r) / 24) * rmax / nb / sub; }
            ex[std::size_t(b)] = P; tot += P;
        }
        for (double& e : ex) e /= tot;
    };
    std::printf("jump dots: at the start all %ld old dots are shown, identical to the ones on screen before the jump: %ld; no new dot shown: %s; at the end no old dot shown: %s\n", oldDotsTotal, oldDotsSame, noNewAtStart ? "yes" : "NO", noOldAtEnd ? "yes" : "NO");
    checkTrue("jump: at the start the shown dots are exactly the cloud that was on screen (every old dot, no new one)", noNewAtStart && oldDotsSame == oldDotsTotal);
    checkTrue("jump: at the end only the new dots are shown (no old one, every new one)", noOldAtEnd);
    double worstChi = 0, worstR = 0;
    for (int t = 0; t < 5; t++) {
        const double u = t == 4 ? 1.0 : ths[t], th = kPi / 2 * smoothstep(u);
        std::vector<double> ex; radialExact(th, ex);
        double chi = 0; double nin = 0; for (double h : acc[t].hist) nin += h;
        for (int b = 0; b < nb; b++) { const double e = nin * ex[std::size_t(b)]; if (e > 0) chi += (acc[t].hist[std::size_t(b)] - e) * (acc[t].hist[std::size_t(b)] - e) / e; }
        const double mean = acc[t].sumR / acc[t].n, sd = std::sqrt(acc[t].sumR2 / acc[t].n - mean * mean), se = sd / std::sqrt(double(acc[t].n));
        const double exactR = std::cos(th) * std::cos(th) * rA + std::sin(th) * std::sin(th) * rB;
        worstChi = std::max(worstChi, chi); worstR = std::max(worstR, std::fabs(mean - exactR) / se);
        std::printf("jump dots at u = %.2f (theta = %.3f): %ld shown dots, mean r %.4f +- %.4f, exact %.4f; radial chi-square %.1f (19 dof)\n", u, th, acc[t].n, mean, se, exactR, chi);
    }
    // limits as for the beats (stage 2): 4 standard errors; chi-square 19 dof, the 99.99 % point is 48
    checkTrue("jump dots: the mean r follows the exact <r>(t) within 4 standard errors at the start, the end and 3 times between", worstR < 4);
    checkTrue("jump dots: the radial histogram matches the exact radial density of the start orbital, the end orbital and the mixtures (chi-square below 48)", worstChi < 48);
}

// the Orbit view: the electron glides between the circles
static void testOrbitGlide() {
    const double a0CODATA = nist::a0;                                     // m, CODATA 2022, NIST page bohrrada0 (read 2026-10-04)
    for (int Z : {1, 2}) {
        for (int dir : {1, -1}) {
            AtomLab lab; lab.nowMs = [] { return 0.0; }; lab.p = AtomParams(); lab.paused = false; lab.restart();
            lab.setParam("Z", double(Z)); lab.setParam("speed", 5.0);
            const int n1 = dir > 0 ? 1 : 3, n2 = dir > 0 ? 3 : 1;
            lab.setParam("n", double(n1));
            runSteps(lab, 10);
            lab.startJump(dir > 0 ? "absorb" : "emit", n2);
            double prevR = std::hypot(lab.s.x, lab.s.y), worstOver = 0, worstBack = 0, rLanding = -1; int steps = 0; bool started = dir < 0;
            const double r1 = double(n1) * n1 / Z * kA0, r2 = double(n2) * n2 / Z * kA0;
            int glideSteps = 0;
            for (int i = 0; i < 1200 && (lab.s.photon || lab.s.trans || !started); i++) {
                const bool hadTrans = lab.s.trans.has_value();
                lab.step(1.0 / 240); steps++;
                if (lab.s.trans) { started = true; glideSteps++; }
                if (hadTrans && !lab.s.trans) rLanding = std::hypot(lab.s.x, lab.s.y);        // the radius at the moment of landing (after it the leapfrog orbit goes on)
                const double r = std::hypot(lab.s.x, lab.s.y);
                if (lab.s.trans) {
                    worstBack = std::max(worstBack, dir > 0 ? prevR - r : r - prevR);                // monotone: never turns back
                    worstOver = std::max({worstOver, (r - std::max(r1, r2)) / kA0, (std::min(r1, r2) - r) / kA0});   // never beyond the two circles
                }
                prevR = r;
            }
            const double rEnd = rLanding, want = double(n2) * n2 / Z * kA0;
            std::printf("Orbit glide Z = %d, n %d -> %d: %d steps (%.3f s), radius at landing %.9e m, n^2 a0/Z with the lab's a0 = %.9e, with CODATA a0 = %.9e; went back by at most %.1e m, beyond the circles by %.1e a0\n",
                        Z, n1, n2, glideSteps, glideSteps / 240.0, rEnd, want, double(n2) * n2 / Z * a0CODATA, worstBack, worstOver);
            // limit 5e-4: in the step of landing the Orbit view's own leapfrog takes one step on the new circle, and a leapfrog circle wobbles by about (angle per step)^2 / 8 (1.3e-4 at n = 1,
            // speed 5): measured 1.3e-4. The landing circle itself is exact: the reduced-motion test below (no leapfrog after the leap) holds it to 1e-9.
            checkTrue("Orbit: the glide ends on the circle n^2 a0 / Z (the lab's a0, 5e-4: one leapfrog step of wobble)", std::fabs(rEnd / want - 1) < 5e-4 && lab.p.n == n2 && !lab.s.trans);
            // 0.10.6 stage 2: the lab's Bohr radius is the CODATA 2022 one now (it was the web's rounded 0.529 Angstrom, 0.034 % low), so this check, which allowed 4e-4, now holds to 1e-12
            checkTrue("Orbit: the circle n^2 a0 / Z of the lab is the one with the CODATA 2022 Bohr radius (1e-12)", std::fabs(want / (double(n2) * n2 / Z * a0CODATA) - 1) < 1e-12);
            checkTrue("Orbit: the glide is monotone (never turns back) and never overshoots either circle", worstBack < 1e-18 && worstOver < 1e-9);
            checkTrue("Orbit: the glide takes about 0.8 s", glideSteps >= 190 && glideSteps <= 194);
        }
    }
}

// the hand-over of the photon, no pop; Auto photons back to back; pause; reset in the middle; the user's setting is left alone
static void testJumpHandover() {
    for (const char* view : {"Cloud", "Orbit"}) {
        const bool orbit = std::string(view) == "Orbit";
        // emit: the jump first, the photon appears exactly when it ends
        AtomLab lab; jumpSetup(lab, "2p", "z");
        if (orbit) { lab.setParam("view", std::string("Orbit")); lab.setParam("n", 2.0); }
        runSteps(lab, 50);
        lab.startJump("emit", orbit ? 1 : orbIndex("1s"));
        bool noPhotonDuring = !lab.s.photon && lab.s.photonAfter.has_value(), appearedAtEnd = false; int jumpSteps = 0;
        for (int i = 0; i < 400 && !appearedAtEnd; i++) {
            const bool hadTrans = lab.s.trans.has_value();
            lab.step(1.0 / 240); if (hadTrans) jumpSteps++;
            if (lab.s.photon) { appearedAtEnd = hadTrans && !lab.s.trans && lab.s.photon->t < 1.0 / 240 + 1e-9 && !lab.s.photonAfter && lab.s.photon->kind == "emit"; break; }
            if (lab.s.trans && lab.s.photon) noPhotonDuring = false;
        }
        std::printf("emit (%s): jump %.3f s, then the photon appears at its end (age %.2e s)\n", view, jumpSteps / 240.0, lab.s.photon ? lab.s.photon->t : -1.0);
        checkTrue(std::string("emit (") + view + "): no photon while the jump runs, and it appears in the very step in which the jump ends", noPhotonDuring && appearedAtEnd);
        // absorb: the photon first, the jump starts exactly when it arrives
        AtomLab lab2; jumpSetup(lab2, "1s", "");
        if (orbit) { lab2.setParam("view", std::string("Orbit")); lab2.setParam("n", 1.0); }
        runSteps(lab2, 50);
        lab2.startJump("absorb", orbit ? 2 : orbIndex("2p|z"));
        bool startedAtArrival = false;
        for (int i = 0; i < 500 && !startedAtArrival; i++) {
            const bool hadPhoton = lab2.s.photon.has_value();
            lab2.step(1.0 / 240);
            if (lab2.s.trans) startedAtArrival = hadPhoton && !lab2.s.photon && lab2.s.trans->t < 1.0 / 240 + 1e-9;
            if (lab2.s.trans && lab2.s.photon) break;
        }
        checkTrue(std::string("absorb (") + view + "): the jump starts in the very step in which the photon arrives", startedAtArrival);
    }
    // Auto photons, 60 s, cloud and Orbit: never a photon and a jump at once; no photon without the end of an emit jump; Orbit: no jump in the radius
    for (const char* view : {"Cloud", "Orbit"}) {
        const bool orbit = std::string(view) == "Orbit";
        seedRandom(97531);
        AtomLab lab; jumpSetup(lab, "3p", "z"); lab.setParam("view", std::string(view)); if (orbit) lab.setParam("n", 3.0);
        lab.toggleAuto();
        bool both = false, wrongStart = false; int jumps = 0; double worstStep = 0, rMax = 0;
        double prevR = std::hypot(lab.s.x, lab.s.y);
        for (int i = 0; i < 14400; i++) {
            const bool hadTrans = lab.s.trans.has_value(), hadPhoton = lab.s.photon.has_value(), hadAfter = lab.s.photonAfter.has_value(); const std::string kind = hadPhoton ? lab.s.photon->kind : "";
            lab.step(1.0 / 240);
            if (lab.s.trans && lab.s.photon) both = true;
            if (!hadPhoton && lab.s.photon && lab.s.photon->kind == "emit" && !(hadTrans && !lab.s.trans && hadAfter)) wrongStart = true;      // an emitted photon appears only when its jump ends
            if (!hadTrans && lab.s.trans) { jumps++; if (!(hadPhoton && kind == "absorb" && !lab.s.photon) && !lab.s.photonAfter) wrongStart = true; }   // a jump starts when an absorbed photon arrives, or it is an emit
            if (orbit) { const double r = std::hypot(lab.s.x, lab.s.y); worstStep = std::max(worstStep, std::fabs(r - prevR)); rMax = std::max(rMax, r); prevR = r; }
        }
        std::printf("Auto photons, %s, 60 s: %d jumps; a photon and a jump at once: %s; wrong hand-over: %s%s\n", view, jumps, both ? "YES" : "never", wrongStart ? "YES" : "none",
                    orbit ? (std::string("; largest radius change in one 1/240 s step ") + std::to_string(worstStep / kA0) + " a0 (largest radius " + std::to_string(rMax / kA0) + " a0)").c_str() : "");
        checkTrue(std::string("Auto photons (") + view + "): many jumps, never a photon and a jump together, and every hand-over exact", jumps >= 8 && !both && !wrongStart);
        // limit for the Orbit radius: a glide of the biggest gap (n 1 <-> 4: 15 a0 at Z = 1) changes the radius by at most 1.5 x gap / (0.8 s x 240) = 0.12 a0 per step; the circular leapfrog orbit itself wobbles by far less
        if (orbit) checkTrue("Auto photons (Orbit): the electron's radius never jumps (below 0.15 a0 per 1/240 s step)", worstStep / kA0 < 0.15);
    }
    // pause: the jump advances only inside step(), which the app does not call while paused; tick() (which runs every frame, even when paused) does not move it
    AtomLab p1; jumpSetup(p1, "1s", ""); runSteps(p1, 100); p1.startJump("absorb", orbIndex("2p|z")); runSteps(p1, 400);
    const double t0 = p1.s.trans->t; p1.paused = true; for (int i = 0; i < 600; i++) p1.tick(1.0 / 60);
    checkTrue("pause: a jump in progress does not move while only tick() runs (the app's paused frames)", p1.s.trans && p1.s.trans->t == t0 && p1.jumpU() == smoothstep(t0 / 1.2));
    // changing the orbital, the element or the view in the middle of a jump: a clean new cloud, no leftover jump
    for (int what = 0; what < 3; what++) {
        AtomLab m; jumpSetup(m, "1s", ""); runSteps(m, 100); m.startJump("absorb", orbIndex("2p|z")); runSteps(m, 400);
        const bool inJump = m.s.trans.has_value() && m.s.jn > 0;
        if (what == 0) m.setParam("orb", std::string("3d")); else if (what == 1) m.setParam("Z", 2.0); else m.setParam("view", std::string("Cloud 3D"));
        const bool clean = !m.s.trans && !m.s.photon && !m.s.photonAfter && m.s.jn == 0 && m.s.jp.empty() && m.s.jA.empty() && m.s.jB.empty() && !m.jumpCloud();
        runSteps(m, 300);
        checkTrue(std::string("changing ") + (what == 0 ? "the orbital" : what == 1 ? "the element" : "the view") + " in the middle of a jump resets it cleanly", inJump && clean && m.s.k > 0);
    }
    // the user's Superposition setting is left alone by jumps; jumps work again after Superposition was switched on and off
    AtomLab u; jumpSetup(u, "1s", "");
    u.setParam("orb2", std::string("2p")); u.setParam("mix", 30.0);
    u.setParam("super", std::string("On"));
    runSteps(u, 100);
    u.emit(); u.absorb();
    checkTrue("a superposition shown: no jump starts (as in stage 2), and the setting is unchanged", !u.s.trans && !u.s.photon && !u.s.photonAfter && u.p.super == "On" && u.p.mix == 30 && u.p.orb2 == "2p");
    u.setParam("super", std::string("Off"));
    runSteps(u, 800);
    u.startJump("absorb", orbIndex("2p|z"));
    runSteps(u, 384 + 300);
    checkTrue("jumps work again after Superposition was switched on and off: the orbital is now 2p", !u.s.trans && !u.s.photon && u.p.orb == "2p" && u.p.pAx == "z" && u.p.super == "Off" && u.p.orb2 == "2p" && u.p.mix == 30);
    AtomLab j; jumpSetup(j, "1s", ""); runSteps(j, 800);
    j.startJump("absorb", orbIndex("2p|z")); runSteps(j, 384 + 100);
    checkTrue("a jump leaves the user's Superposition setting alone (Off, second orbital, mix)", j.p.super == "Off" && j.p.orb2 == "2p" && j.p.mix == 50 && !j.superOn() && j.s.trans.has_value());
    // reduced motion: the jump is instant (no glide, no cross-fade); the emitted photon then appears at once, the absorbed one still flies in
    AtomLab rm; jumpSetup(rm, "2p", "z"); rm.reducedMotion = true; runSteps(rm, 800);
    rm.startJump("emit", orbIndex("1s"));
    checkTrue("reduced motion (cloud): the emit jump is instant and the photon appears at once", !rm.s.trans && rm.s.photon.has_value() && rm.p.orb == "1s");
    AtomLab ro; ro.nowMs = [] { return 0.0; }; ro.p = AtomParams(); ro.restart(); ro.reducedMotion = true; ro.setParam("n", 3.0); runSteps(ro, 20); ro.startJump("emit", 1);
    checkTrue("reduced motion (Orbit): the electron leaps at once to the new circle, the flash and the photon appear", !ro.s.trans && ro.p.n == 1 && std::fabs(std::hypot(ro.s.x, ro.s.y) / kA0 - 1) < 1e-9 && ro.s.photon.has_value() && ro.s.flash.has_value());
}

static void testOutlineFade() {
    AtomLab lab; jumpSetup(lab, "1s", ""); runSteps(lab, 100);
    checkTrue("outline: fully there when no jump runs", lab.outlineAlpha() == 1);
    lab.startJump("absorb", orbIndex("2p|z")); runSteps(lab, 400);
    lab.s.trans->t = 0; const double a0 = lab.outlineAlpha();
    lab.s.trans->t = 0.12; const double a1 = lab.outlineAlpha();
    lab.s.trans->t = 0.6; const double a2 = lab.outlineAlpha();
    for (int i = 0; i < 400 && lab.s.trans; i++) lab.step(1.0 / 240);       // the jump ends
    const double a3 = lab.outlineAlpha();
    runSteps(lab, 48); const double a4 = lab.outlineAlpha();                // 0.2 s after landing
    runSteps(lab, 100); const double a5 = lab.outlineAlpha();               // 0.6 s after landing
    std::printf("outline alpha: start %.2f, 0.12 s %.2f, middle %.2f, landing %.2f, 0.2 s later %.2f, 0.6 s later %.2f\n", a0, a1, a2, a3, a4, a5);
    checkTrue("outline: starts at full strength, fades out, is hidden in the middle of the jump, fades back in after landing and is full again", a0 == 1 && a1 < 1 && a1 > 0 && a2 == 0 && a3 <= 0.05 && a4 > 0 && a4 < 1 && a5 == 1);
}

// ================= The constants (0.10.6 stage 2): CODATA 2022 from the NIST pages =================
static void testConstants() {
    struct K { const char* what; double lab, want; };
    const K ks[] = {{"Bohr radius", kA0, nist::a0}, {"Hartree energy eV", kHartreeEV, nist::hartreeEV}, {"atomic unit of time", kAuTime, nist::tau}, {"fine-structure constant", kAlpha, nist::alpha},
        {"Rydberg energy eV", kRydInf, nist::rydEV}, {"electron mass energy eV", kMecEV, nist::mecEV}, {"electron mass in u", kMeInU, nist::meInU}, {"proton-electron mass ratio", kMpOverMe, nist::mpOverMe},
        {"u in kg", kUKg, nist::ukg}, {"u in MeV", kUMeV, nist::uMeV}, {"electron mass kg", kMeKg, nist::meKg}, {"proton mass kg", kMpKg, nist::mpKg}, {"neutron mass kg", kMnKg, nist::mnKg},
        {"proton mass u", kMpU, nist::mpU}, {"neutron mass u", kMnU, nist::mnU}, {"Boltzmann", kBoltzmann, nist::kB}, {"Planck constant", kPlanckJs, nist::h}, {"speed of light", kC, nist::c},
        {"elementary charge", kEV, nist::e}, {"h c in eV nm", kHcEVnm, nist::hcEVnm}, {"u in electron masses", kMeU, 1 / nist::meInU}};
    for (const K& k : ks) checkNum(std::string("constant: ") + k.what + " is the NIST CODATA 2022 value", k.lab, k.want, 1e-14);
    checkNum("constant: h in eV s (NIST page hev: 4.135 667 696...e-15, ten digits)", kPlanckEVs, 4.135667696e-15, 1e-9);
    // derived ones: one source
    checkNum("constant: Bohr radius in Angstrom is a0 x 1e10", kA0Ang, nist::a0 * 1e10, 1e-15);
    checkNum("constant: metres per fm is exactly 1e-15", kFmLab, 1e-15, 0);
    checkNum("constant: fm per Bohr radius is a0 / 1e-15 (52 917.721 054 4)", kFmPerA0, 52917.7210544, 1e-12);
    checkNum("constant: the atomic unit of time in as is tau x 1e18", kAuTimeAs, 24.188843265864, 1e-13);
    // relations between the constants (independent of how each was typed): alpha c = a0 / tau; Hartree = 2 Rydberg = alpha^2 m_e c^2; a0 = hbar / (m_e c alpha); u / m_e = (u c^2) / (m_e c^2); m_p / m_e
    const double alphaC = kAlpha * kC;
    std::printf("constants: a0/tau = %.9e m/s, alpha c = %.9e m/s (relative difference %.1e)\n", kAuSpeed, alphaC, std::fabs(kAuSpeed / alphaC - 1));
    checkTrue("constant: the atomic unit of speed a0 / tau equals alpha x c (1e-10; measured about 6e-12)", std::fabs(kAuSpeed / alphaC - 1) < 1e-10);
    checkTrue("constant: Hartree energy = 2 x Rydberg energy (1e-11)", std::fabs(kHartreeEV / (2 * kRydInf) - 1) < 1e-11);
    checkTrue("constant: Hartree energy = alpha^2 m_e c^2 (1e-9, alpha's uncertainty is 1.6e-10)", std::fabs(kHartreeEV / (kAlpha * kAlpha * kMecEV) - 1) < 1e-9);
    checkTrue("constant: Bohr radius = hbar / (m_e c alpha) (1e-9)", std::fabs(kA0 / (kPlanckJs / (2 * kPi) / (kMeKg * kC * kAlpha)) - 1) < 1e-9);
    checkTrue("constant: u / m_e = u c^2 / m_e c^2 (1e-9)", std::fabs(kMeU / (kUMeV * 1e6 / kMecEV) - 1) < 1e-9);
    checkTrue("constant: m_p / m_e from the kg values and from the u values (1e-9)", std::fabs(kMpKg / kMeKg / kMpOverMe - 1) < 1e-9 && std::fabs(kMpU / kMeInU / kMpOverMe - 1) < 1e-9);
}

// ================= 0.10.6 stage 3: one energy for one atom, the Orbit view's accuracy, the Fixed scale =================
// Finding 3 (audit): the readout energy is the lab's reduced-mass level levelEV (hydrogen -13.598 eV -> "13.60 eV"), in the Cloud view AND in the Orbit view (from the orbit itself).
static void testOneEnergy() {
    // independent: hydrogen's level is the Rydberg energy for an infinite mass divided by (1 + m_e/m_p), from the NIST values typed in the nist:: block (not from atom_core.h)
    const double hyd = nist::rydEV / (1 + 1 / nist::mpOverMe);
    checkNum("energy: levelEV of hydrogen 1s is -Ry_inf / (1 + m_e/m_p) = -13.598287 eV (NIST values, 1e-12)", -levelEV(1, 0, 1), hyd, 1e-12);
    checkNum("energy: the same atom in the spectrum: the Lyman limit is h c / 13.598287 eV (1e-12)", [] { for (const SpecLine& l : specLines(1, 0)) if (l.lo == 1 && l.hi == 0) return l.lam; return 0.0; }(), nist::hcEVnm / hyd, 1e-12);
    for (int Z : {1, 2, 26, 92, 118}) {
        const int N = defaultN(Z);
        for (const char* view : {"Orbit", "Cloud"}) {
            AtomLab lab; lab.nowMs = [] { return 0.0; }; lab.p = AtomParams(); lab.restart();
            lab.setParam("Z", double(Z)); lab.setParam("N", double(N)); lab.setParam("view", std::string(view));
            for (int n = 1; n <= 4; n++) {
                std::string line;
                if (std::string(view) == "Orbit") { lab.setParam("n", double(n)); line = lab.info1(); checkNum("energy: Orbit view E = levelEV Z=" + std::to_string(Z) + " n=" + std::to_string(n), lab.orbitEnergyEV(), levelEV(Z, N, n), 1e-9); }
                else { lab.setParam("orb", std::string(std::to_string(n) + "s")); line = lab.info1(); }
                checkTrue("energy: the readout shows fmtEV(levelEV) (" + std::string(view) + ", Z=" + std::to_string(Z) + ", n=" + std::to_string(n) + ")", line.find("E −" + fmtEV(-levelEV(Z, N, n))) != std::string::npos);
            }
        }
    }
    AtomLab h; h.nowMs = [] { return 0.0; }; h.p = AtomParams(); h.restart();
    checkTrue("energy: hydrogen reads -13.60 eV in the Orbit view and in the Cloud view (was -13.61)", h.info1().find("E −13.60 eV") != std::string::npos && (h.setParam("view", std::string("Cloud")), h.info1()).find("E −13.60 eV") != std::string::npos);
}

// Finding 14: the Orbit view's leapfrog. 60 s of steps at the speeds 1, 5, 10 and 30 and n = 1..4: the radius stays near the analytic Bohr radius n^2 a0 / Z (a0 from the NIST value typed above), the readout
// energy near levelEV, and the electron turns at the rate the speed setting says: dt x speed / n^3 radians per step (a circular orbit's angular rate is Z^2 / n^3 per atomic unit of time, and a step is
// dt x speed / Z^2 of them). Guards: measured worst error + 1 % (the numbers are printed), see the comment at the checks.
static void testOrbitAccuracy(double radTol, double enTol, double rateTol) {
    double worstR = 0, worstE = 0, worstRate = 0; int runs = 0;
    for (int Z : {1, 3})
        for (int speed : {1, 5, 10, 30})
            for (int n = 1; n <= 4; n++) {
                AtomLab lab; lab.nowMs = [] { return 0.0; }; lab.p = AtomParams(); lab.restart();
                lab.setParam("Z", double(Z)); lab.setParam("n", double(n)); lab.setParam("speed", double(speed));
                const double rB = double(n) * n / Z * nist::a0, E0 = levelEV(Z, lab.p.N, n);
                double ang = std::atan2(lab.s.y, lab.s.x), turned = 0, wr = 0, we = 0;
                const int steps = 240 * 60;
                for (int i = 0; i < steps; i++) {
                    lab.step(1.0 / 240);
                    wr = std::max(wr, std::fabs(std::hypot(lab.s.x, lab.s.y) / rB - 1));
                    we = std::max(we, std::fabs(lab.orbitEnergyEV() / E0 - 1));
                    const double a = std::atan2(lab.s.y, lab.s.x); double d = a - ang; while (d < -kPi) d += 2 * kPi; while (d > kPi) d -= 2 * kPi;
                    turned += d; ang = a;
                }
                const double want = steps * (1.0 / 240) * speed / (double(n) * n * n);          // radians
                worstR = std::max(worstR, wr); worstE = std::max(worstE, we); worstRate = std::max(worstRate, std::fabs(turned / want - 1)); runs++;
                if (wr > radTol || we > enTol) std::printf("  Orbit accuracy Z=%d speed %d n=%d: radius error %.2e, energy error %.2e\n", Z, speed, n, wr, we);
            }
    std::printf("Orbit accuracy over %d runs of 60 s: worst radius error %.5e, energy error %.5e, turning-rate error %.5e\n", runs, worstR, worstE, worstRate);
    checkTrue("Orbit: the radius stays within the guard of the analytic Bohr radius n^2 a0 / Z at speeds 1 to 30, n = 1..4", worstR <= radTol);
    checkTrue("Orbit: the readout energy stays within the guard of levelEV at speeds 1 to 30, n = 1..4", worstE <= enTol);
    checkTrue("Orbit: the electron turns dt x speed / n^3 radians per step (the visible speed), within the guard", worstRate <= rateTol);
}

// Finding 8: Scale Fixed has to hold every cloud. The view radius (sizeM, the half-size the picture is fitted to) must reach at least the 90 % outline radius of the orbital,
// from the lab's own outline code (outlineOf: rmax in Bohr radii at Z = 1). The same for Auto (kExt per n) and for the complex states; Fixed in the Orbit view holds the n = 4 circle.
static void testFixedExtent() {
    double worstFixed = 1e9, worstAuto = 1e9; std::string wf, wa;
    std::vector<int> ids;
    for (int i = 0; i < int(orbitals().size()); i++) ids.push_back(i);
    for (int id : complexIds()) ids.push_back(id);
    for (const char* scale : {"Fixed", "Auto"})
        for (int Z : {1, 3}) {
            AtomLab lab; lab.nowMs = [] { return 0.0; }; lab.p = AtomParams(); lab.restart();
            lab.setParam("view", std::string("Cloud")); lab.setParam("Z", double(Z)); lab.setParam("scale", std::string(scale));
            for (int id : ids) {
                const Orbital& o = orbitals()[stateOrbital(id)];
                lab.setParam("orb", o.shell);
                const double reach = outlineOf(id).rmax / Z * kA0, ratio = lab.sizeM() / reach;   // metres at this Z
                if (std::string(scale) == "Fixed" && ratio < worstFixed) { worstFixed = ratio; wf = stateText(id); }
                if (std::string(scale) == "Auto" && ratio < worstAuto) { worstAuto = ratio; wa = stateText(id); }
            }
        }
    std::printf("view radius / 90 %% outline radius, smallest over 30 real + 20 complex orbitals: Fixed %.3f (%s), Auto %.3f (%s)\n", worstFixed, wf.c_str(), worstAuto, wa.c_str());
    checkTrue("Scale Fixed: the view radius holds the 90 % outline of every orbital (n = 1..4, s p d f, real and complex)", worstFixed >= 1);
    checkTrue("Scale Auto: the view radius (kExt per n) holds the 90 % outline of every orbital", worstAuto >= 1);
    AtomLab o; o.nowMs = [] { return 0.0; }; o.p = AtomParams(); o.restart(); o.setParam("scale", std::string("Fixed")); o.setParam("view", std::string("Orbit"));
    checkTrue("Scale Fixed, Orbit view: the view radius holds the n = 4 circle (16 a0)", o.sizeM() >= 16 * kA0 * (1 - 1e-12));
}

int main() {
    testConstants();
    testOneEnergy();
    // guards = the worst error MEASURED after the fix (Linux GCC, 32 runs of 60 s) + 1 %: radius 2.16990e-4 -> 2.192e-4, energy 4.70644e-8 -> 4.754e-8, turning rate 1.45367e-4 -> 1.469e-4. The same runs
    // before the fix (0.10.6 stage 2 code, one leapfrog step per frame): radius 7.783e-3 (n = 1, speed 30), energy 5.964e-5, rate 5.165e-3, so this fails without the sub-steps.
    testOrbitAccuracy(2.192e-4, 4.754e-8, 1.469e-4);
    testFixedExtent();
    testElements();
    testNuclides();
    testFormats();
    testOrbitals();
    testLight();
    testNuclei();
    testRuns();
    testLearn();
    testComplexBasics();
    testWinding();
    testLz();
    testOrthonormal();
    testAgainstScipy();
    testDensityFromReal();
    testRealDotsUnchanged();
    testComplexDots();
    testPhaseWheel();
    testComplexOutline();
    testBeatPeriodAndAmplitude();
    testBeatNormStationary();
    testSuperDots();
    testJumpFractions();
    testJumpNormAndContinuity();
    testJumpDots();
    testOrbitGlide();
    testJumpHandover();
    testOutlineFade();
    std::printf("%d atom checks, %d failed (%d on an exact rounding tie, off by one in the last digit).\n", g_checks, g_failed, g_ties);
    return g_failed ? 1 : 0;
}
