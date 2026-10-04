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
#include <cstring>
#include <map>
#include <set>
#include <complex>
#include <string>
#include <vector>

#include "atom/atom_core.h"
#include "atom/atom_learn.h"
#include "atom/asd.h"
#include "atom/dirac.h"
#include "atom/lifetime.h"
#include "atom/atom_sim.h"
#include "atom/outline.h"
#include "atom/phase_wheel.h"
#include "atom/radii.h"
#include "atom/quadrupole.h"
#include "atom/radial_graph.h"
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
// 0.10.6 stage 6 (finding 5): the lab's nuclear mass is now Z m_p + N m_n - B/c^2 (B from the lab's binding table, which is AME2020) instead of A u - Z m_e. Written here again from the NIST masses typed above; a
// nuclide the table does not hold keeps the old form. The saved web numbers (made with the web's A u - Z m_e) are rescaled by the ratio of this to the web's formula.
static double nuclearMassNew(int Z, int N) {
    const Binding b = massBinding(Z, N);
    if (!b.real) return (Z + N) / nist::meInU - Z;
    return Z * nist::mpOverMe + N * (nist::mnU / nist::meInU) - b.B * 1e6 / nist::mecEV;
}
static double rydMNew(int Z, int N) { const double M = nuclearMassNew(Z, N); return nist::rydEV * M / (M + 1); }
static double rydMRatio(int Z, int N) { return rydMNew(Z, N) / rydMBy(webval::ryd, webval::meu, webval::mpOverMe, Z, N); }

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
        const auto got = buildNucleusByRule(r.Z, r.N);   // 0.10.9 stage 1: buildNucleus now sizes a nucleus by its measured charge radius where there is one (9 of these 10 have one); the web's saved positions are the web's RULE layout, which buildNucleusByRule still is
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
    const double rm = rydMNew(Z, N);
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
        const double rm = rydMNew(lab.p.Z, lab.p.N);
        line.replace(k + key.size(), e - k - key.size(), fmtEV(rm * lab.p.Z * lab.p.Z));
        if (energyOnly) *energyOnly += "|" + line.substr(k, e - k);
    }
    return line;
}

// 0.10.6 stage 5: three parts of the readout changed on purpose (words, not physics), so the saved web lines are adapted, each by ONE exact rule, and everything else stays word for word:
//  1. a new line "model: one electron, non-relativistic ..." (all Z): taken out of the lab's lines before comparing (its content is tested in testRelativityText);
//  2. the "chance the electron is inside the nucleus" line gets a fixed suffix: cut off (exactly this text) before comparing;
//  3. "avg r: sampled X, theory T": X is shown only with the full ring of 12,000 dots (kCap) and then with kSampledNote after it; before that the web's number is replaced by the placeholder "…".
static const std::string kModelPrefix = "model: one electron, non-relativistic";
static const std::string kNucPrefix = "chance the electron is inside the nucleus";
static const std::string kNucSuffix = "  ·  non-relativistic estimate: for heavy atoms relativity makes the real value larger, and this lab does not work it out";
static const std::string kSampledNote = " (an estimate, usually within 10 %)";
static std::vector<std::string> stripNewParts(const std::vector<std::string>& in, bool& ok) {
    std::vector<std::string> out;
    ok = true;
    for (const std::string& l : in) {
        if (l.compare(0, kModelPrefix.size(), kModelPrefix) == 0) continue;
        if (l.compare(0, kNucPrefix.size(), kNucPrefix) == 0) {
            if (l.size() < kNucSuffix.size() || l.compare(l.size() - kNucSuffix.size(), kNucSuffix.size(), kNucSuffix) != 0) { ok = false; out.push_back(l); }
            else out.push_back(l.substr(0, l.size() - kNucSuffix.size()));
            continue;
        }
        out.push_back(l);
    }
    return out;
}
static std::string adaptSampled(std::string web, const AtomLab& lab) {
    const std::string key = "avg r: sampled ", th = ", theory";
    const std::size_t a = web.find(key);
    if (a == std::string::npos) return web;
    const std::size_t b = web.find(th, a);
    const std::string x = web.substr(a + key.size(), b - a - key.size());
    web.replace(a + key.size(), x.size(), lab.s.k >= kCap ? x + kSampledNote : std::string("…"));
    return web;
}

// 0.10.9 stage 1: the nucleus is drawn at its MEASURED size, so two lines of the saved web runs move on purpose: "The nucleus is ~X× smaller than this orbit/orbital" and "chance the electron is inside the nucleus ...".
// The web's saved text is kept for everything else, word for word. For an isotope with a measured charge radius these two numbers are rebuilt here from the CSV value (typed in kRunRad from data/radii/charge_radii.csv):
//   R = sqrt(5/3) x rms charge radius (a uniform ball), in Bohr radii; ratio = (orbit or orbital size n^2 / Z in Bohr radii) / R; chance = 4/3 Z^3 R^3 / n^3 (the lab's own formula, 0.10.6: |psi(0)|^2 x the nucleus' volume).
struct RunRad { int Z, N; double rms; };
static const RunRad kRunRad[] = {{2, 1, 1.9661}, {2, 2, 1.6755}, {3, 4, 2.4440}, {26, 30, 3.7377}, {82, 126, 5.5012}, {83, 129, 5.5489}, {92, 141, 5.8203}, {96, 152, 5.8687}};
static int g_runMoved = 0;
static bool runRadiusA0(const AtomLab& lab, double& Ra0) {
    if (!sizedByMeasurement(lab.p.Z, lab.p.N)) return false;
    for (const RunRad& r : kRunRad) if (r.Z == lab.p.Z && r.N == lab.p.N) { Ra0 = 1.2909944487358056 * r.rms * 1e-15 / nist::a0; return true; }
    checkTrue("a saved run uses a measured isotope that kRunRad does not list: " + at(lab.p.Z, lab.p.N), false);
    return false;
}
static std::string adaptNucleusHint(std::string web, const AtomLab& lab) {
    double R;
    if (!runRadiusA0(lab, R)) return web;
    const std::string a = "The nucleus is ~", b = "× smaller than this";
    const std::size_t i = web.find(a), j = web.find(b);
    if (i == std::string::npos || j == std::string::npos) return web;
    const double size = lab.p.view == "Orbit" ? double(lab.p.n) * lab.p.n / lab.p.Z : std::pow(orbitals()[orbIndex(lab.p.orb)].n, 2) / double(lab.p.Z);
    const std::string now = fmt::jsLocale(std::strtod(fmt::jsToPrecision(size / R, 2).c_str(), nullptr)), was = web.substr(i + a.size(), j - i - a.size());
    if (now != was) { g_runMoved++; std::printf("saved web reference moved: %s hint \"~%s× smaller\" -> \"~%s×\"\n", at(lab.p.Z, lab.p.N).c_str(), was.c_str(), now.c_str()); }
    return web.substr(0, i + a.size()) + now + web.substr(j);
}
static std::string adaptNucleusChance(std::string web, const AtomLab& lab) {
    double R;
    if (!runRadiusA0(lab, R)) return web;
    const std::string a = "chance the electron is inside the nucleus at any instant: ≈ ";
    if (web.compare(0, a.size(), a) != 0) return web;
    const int n = orbitals()[orbIndex(lab.p.orb)].n;
    const double P = 4.0 / 3 * std::pow(lab.p.Z, 3) * R * R * R / std::pow(n, 3);
    const std::string now = sci(P) + " (1 in " + sci(1 / P) + ")", was = web.substr(a.size());
    if (now != was) { g_runMoved++; std::printf("saved web reference moved: %s chance \"%s\" -> \"%s\"\n", at(lab.p.Z, lab.p.N).c_str(), was.c_str(), now.c_str()); }
    return a + now;
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
                bool partsOk = true;
                const auto info = stripNewParts(lab.info(), partsOk);
                checkTrue(w + " the nucleus line ends with the stage 5 caveat", partsOk);
                checkNum(w + " info lines", info.size(), c.info.size(), 0);
                for (std::size_t i = 0; i < info.size() && i < c.info.size(); i++) {
                    std::string eWant, eGot;
                    const std::string want = withNewEnergy(adaptNucleusChance(adaptSampled(c.info[i], lab), lab), lab, c, &eWant);
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
                checkStr(w + " hint", lab.hint(), adaptNucleusHint(c.hint, lab));
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
        // 0.10.6 stage 5: the web warned "the true lines are shifted and split noticeably" from Z = 20; the lab now warns from Z = 27 on, like the readout and the hint (finding 6): for 20 <= Z < 27 the web's warning is dropped
        std::string spectrumWant = r.spectrum;
        if (r.Z >= 20 && r.Z < 27) { const std::size_t a = spectrumWant.find(" (for "), b = spectrumWant.find("so these are approximate).", a); if (a != std::string::npos && b != std::string::npos) spectrumWant.replace(a, b + 26 - a, "."); }
        checkStr(w + " spectrum note", spectrumNote(r.Z, r.N), spectrumWant);
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
// ---- 0.10.6 stage 8 / 9: the cloud jump gives the dots it gave before the speed work ----
// Stage 8 compared 64-bit fingerprints of the raw bytes of the results, recorded on Linux. That is not portable: the walker is a chain of 48,800 steps made with log, cos and sqrt, which the C
// libraries of GCC and MSVC may round differently in the last bit, so a double's bytes (the walker's end, `w2` / `w`) differ between the compilers although nothing is wrong (the Windows run of
// stage 8 failed in exactly the 7 checks that hashed the walker's bytes; the checks of the float-derived colour groups, the random numbers and the dot count passed there). So now:
//   1. the OLD way of making the jump's dots (the code of stage 7, written out below from it) is run in the same test, in the same binary, on the same seeded jump, and must agree with the new
//      way bit for bit: both are computed by the same compiler and library, so exact equality is right here;
//   2. numbers recorded on Linux are kept, compared with a stated tolerance (like the stage 2 dot tests), so a real change of the dots is caught across versions too.
struct JumpCase { const char *orb, *cdens, *kind, *to; bool reduced; int seed; };
static atom::AtomLab jumpLabFor(const JumpCase& c) {
    using namespace atom;
    seedRandom(uint32_t(c.seed));
    AtomLab lab; lab.nowMs = [] { return 0.0; };
    lab.p = AtomParams(); lab.reducedMotion = c.reduced; lab.restart();
    lab.setParam("Z", 6.0); lab.setParam("N", 6.0); lab.setParam("view", std::string("Cloud 3D")); lab.setParam("cdens", std::string(c.cdens)); lab.setParam("orb", std::string(c.orb));
    for (int i = 0; i < 40000 && lab.s.k < lab.cloudCap(); i++) lab.step(1.0 / 240);
    return lab;
}
// The old build of the jump's dots: stage 7's beginTransition and walk() for a real orbital, written out (4 evaluations of psi per new dot, the global generator, all in one go).
struct OldJump { std::vector<float> jp, jA, jB; atom::Vec3d w2; };
static OldJump oldJumpBuild(const std::vector<float>& pts, long jn, int from, int to, int Z) {
    using namespace atom;
    OldJump o; o.jp.assign(std::size_t(3 * jn), 0.f); o.jA.assign(std::size_t(4 * jn), 0.f); o.jB.assign(std::size_t(4 * jn), 0.f);
    const double nA = normFactor(from, Z), nB = normFactor(to, Z);
    for (long i = 0; i < jn; i++) {
        const double x = pts[3 * i] / kA0, y = pts[3 * i + 1] / kA0, z = pts[3 * i + 2] / kA0;
        o.jA[4 * i] = float(nA * psi(from, Z, x, y, z)); o.jA[4 * i + 2] = float(nB * psi(to, Z, x, y, z));
    }
    const double k = double(orbitals()[to].n) / Z;
    o.w2 = {0.7 * k, 0.4 * k, 1.1 * k};
    const int n = orbitals()[stateOrbital(to)].n;
    const double sg = 0.4 * n * (n >= 4 ? 1.8 : 1) / Z;
    const auto walk = [&](Vec3d& w) {                                          // walk(to, w, false)
        auto gauss = [] { const double u = atom::random(); return std::sqrt(-2 * std::log(1 - u)) * std::cos(2 * kPi * atom::random()); };
        const double nx = w.x + sg * gauss(), ny = w.y + sg * gauss(), nz = w.z + sg * gauss();
        const double fo = psi(to, Z, w.x, w.y, w.z), pOld = fo * fo;
        const double fn = psi(to, Z, nx, ny, nz);
        if (atom::random() * pOld < fn * fn || pOld == 0) { w.x = nx; w.y = ny; w.z = nz; }
    };
    for (int i = 0; i < 800; i++) walk(o.w2);
    for (long i = 0; i < jn; i++) {
        walk(o.w2);
        o.jp[3 * i] = float(o.w2.x * kA0); o.jp[3 * i + 1] = float(o.w2.y * kA0); o.jp[3 * i + 2] = float(o.w2.z * kA0);
        o.jB[4 * i] = float(nA * psi(from, Z, o.w2.x, o.w2.y, o.w2.z)); o.jB[4 * i + 2] = float(nB * psi(to, Z, o.w2.x, o.w2.y, o.w2.z));
    }
    return o;
}
// Start the jump in `lab` and give back what the old way makes from the same state: the random generator is put back to where the jump began and the old build runs on it. Afterwards the
// generator stands where the old build left it (it must be where the new build left it: the same count of numbers), and `rngNew` is where the new build left it.
struct JumpPair { OldJump old; std::uint32_t rngNew = 0, rngOld = 0; long jn = 0; int from = 0, to = 0; };
static JumpPair startAndReference(atom::AtomLab& lab, const JumpCase& c) {
    using namespace atom;
    JumpPair r;
    r.to = orbIndex(c.to); r.from = lab.curLevel();
    r.jn = std::min<long>(lab.s.k, std::min<long>(lab.cloudCap(), long(lab.s.sg.size())));
    std::vector<float> pts(lab.s.pts.begin(), lab.s.pts.begin() + 3 * r.jn);
    std::uint32_t before = randomState();
    const bool emit = std::string(c.kind) == "emit";
    lab.startJump(c.kind, r.to);
    if (!emit)                                                                // the photon flies in first (the cloud goes on sampling meanwhile); the jump begins when it arrives:
        for (int guard = 0; guard < 600 && !lab.s.trans; guard++) {           // the old dots, the generator and the count are those of the moment before the arrival step
            before = randomState(); pts.assign(lab.s.pts.begin(), lab.s.pts.begin() + 3 * std::min<long>(lab.cloudCap(), long(lab.s.sg.size())));
            r.jn = std::min<long>(lab.s.k, std::min<long>(lab.cloudCap(), long(lab.s.sg.size())));
            lab.step(1.0 / 240);
        }
    r.rngNew = randomState();
    seedRandom(before);
    if (emit) (void)atom::random();                                           // startJump's own number (the photon's angle) comes first
    r.old = oldJumpBuild(pts, r.jn, r.from, r.to, lab.p.Z);
    r.rngOld = randomState();
    seedRandom(r.rngNew);                                                     // leave the generator where the new build left it
    return r;
}
// Everything the checks below look at, as plain numbers.
struct JumpSummary { double jn = 0, w2[3] = {0, 0, 0}, sumJp = 0, sumJA = 0, sumJB = 0, shownSign03 = 0, shownSign07 = 0, wheelSum07 = 0, k = 0, w[3] = {0, 0, 0}, sumPts = 0, sumPh = 0, sumSg = 0, rng[3] = {0, 0, 0}; };
static JumpSummary jumpSummary(const JumpCase& c) {
    using namespace atom;
    AtomLab lab = jumpLabFor(c);
    lab.startJump(c.kind, orbIndex(c.to));
    JumpSummary r;
    for (int guard = 0; guard < 600 && !(lab.s.trans && lab.s.jn > 0) && !c.reduced; guard++) lab.step(1.0 / 240);   // an absorbed photon: the jump begins when it arrives
    if (!c.reduced) {
        for (int i = 0; i < 40; i++) lab.step(1.0 / 240);                                                      // some frames into the jump: built over the frames, as the app does
        r.jn = double(lab.s.jn);
        r.w2[0] = lab.s.w2.x; r.w2[1] = lab.s.w2.y; r.w2[2] = lab.s.w2.z;
        for (float f : lab.s.jp) r.sumJp += std::fabs(double(f));
        for (float f : lab.s.jA) r.sumJA += std::fabs(double(f));
        for (float f : lab.s.jB) r.sumJB += std::fabs(double(f));
        std::vector<unsigned char> g(std::size_t(2 * lab.s.jn));
        lab.p.phase = "Sign";
        for (double t : {0.3, 0.7}) { lab.s.trans->t = t; lab.jumpGroups(g.data()); double n = 0; for (unsigned char v : g) n += v != 255; (t < 0.5 ? r.shownSign03 : r.shownSign07) = n; }
        lab.p.phase = "Wheel";
        lab.s.trans->t = 0.7; lab.jumpGroups(g.data());
        for (unsigned char v : g) if (v != 255) r.wheelSum07 += v;
    }
    for (int i = 0; i < 700 && (lab.s.trans || lab.s.photon || lab.s.photonAfter); i++) lab.step(1.0 / 240);      // the jump ends, the cloud goes on for a while
    for (int i = 0; i < 50; i++) lab.step(1.0 / 240);
    const std::size_t cap = std::size_t(std::min<long>(lab.cloudCap(), long(lab.s.sg.size())));
    r.k = double(lab.s.k); r.w[0] = lab.s.w.x; r.w[1] = lab.s.w.y; r.w[2] = lab.s.w.z;
    for (std::size_t i = 0; i < 3 * cap; i++) r.sumPts += std::fabs(double(lab.s.pts[i]));
    for (std::size_t i = 0; i < cap; i++) { r.sumPh += double(lab.s.ph[i]); r.sumSg += double(lab.s.sg[i]); }
    for (double& q : r.rng) q = atom::random();
    return r;
}

static void testJumpSameDots() {
    const JumpCase cases[] = {{"4f", "Ultra", "emit", "3d|z2", false, 2026}, {"2p", "Normal", "absorb", "3d|z2", false, 77}, {"3d", "Normal", "emit", "2p|z", true, 5}, {"2s", "Normal", "emit", "1s", false, 9}};
    // Recorded on Linux (GCC, Release) from the code of stage 7 (the same numbers came out of stage 8's code: the stage 8 fingerprints matched). Tolerances, and why:
    //  - sums of absolute values of float arrays (positions, amplitudes, picture dots, phases): relative 1e-6. A float holds 6e-8; a last-bit difference of the C library (1e-16 relative in a double,
    //    perhaps 1e-14 after 48,800 chained steps) moves a stored float by one unit of its last place at most rarely, which is about 1e-7 of a sum of 10^4 values at the very most; 1e-6 is 10 times that.
    //  - the walker (doubles after 48,800 steps): relative 1e-9 (the accumulated library difference is about 1e-14).
    //  - how many dots are shown at a moment of the jump: within 0.05 % of all dots (+ 1): a dot at the very edge of its odds could flip with a last-bit difference; the sums of colour numbers likewise.
    //  - the count of dots k, and the next random numbers: exactly (integers; the generator uses only integer maths and an exact division).
    // (No Windows run of these numbers exists before the CI run of stage 9, see REPORT.md; they are what the Linux code gives.)
    const JumpSummary want[] = {
        {48000, {-0.61042645026652043, 0.18372748262565081, 1.6411758933862042}, 6.0625978228732691e-06, 6031.6557989177954, 11212.080984488653, 48189, 48033, 445747, 94272, {0.11361336951312254, -1.6703970121316665, -0.52004410793344713}, 6.2572345950363523e-06, 59297.56298661232, 10250, {0.21281560184434056, 0.25574702024459839, 0.53744121524505317}},
        {12000, {0.87155394968079025, 1.4187888667326771, 0.02980526777541892}, 1.6482404722913135e-06, 7868.2822019484593, 3264.3803650895343, 11546, 11499, 149535, 14448, {-0.029836201632667078, -0.40120695930633449, -1.8850526524151678}, 1.6525631670415527e-06, 16562.476930618286, 1456, {0.152442317456007, 0.86131891421973705, 0.58514888910576701}},
        {0, {0, 0, 0}, 0, 0, 0, 0, 0, 0, 23568, {-0.073387654444096589, 0.08689996727725588, -0.49041880346852601}, 8.0405526679301871e-07, 22999.600456953049, -2642, {0.88974490761756897, 0.38691916177049279, 0.74242525990121067}},
        {12000, {-0.24404311097619288, -0.12356757358303777, 0.016061441752741386}, 2.321701230241769e-07, 7991.6644537394195, 38289.438420659302, 11642, 11877, 96672, 23568, {0.2475660059532431, 0.34024883493234381, 0.074693734791195537}, 2.5181023221036696e-07, 0, 12000, {0.9405794651247561, 0.65091568906791508, 0.5389178313780576}},
    };
    for (std::size_t ci = 0; ci < 4; ci++) {
        const JumpCase& c = cases[ci];
        const std::string name = std::string("jump: ") + c.orb + " " + c.kind + " to " + c.to + ", " + c.cdens + (c.reduced ? ", reduced motion" : "") + ": ";
        // ---- 1. the old way against the new way, in this run: exactly equal ----
        {
            atom::AtomLab lab = jumpLabFor(c);
            const JumpPair pr = startAndReference(lab, c);
            checkTrue(name + "the new build and the old build use exactly the same count of random numbers (the generator ends on the same state)", pr.rngNew == pr.rngOld);
            if (!c.reduced) {
                lab.completeJump();
                checkTrue(name + "new dots, old way = new way, bit for bit (same run, same compiler)", lab.s.jp == pr.old.jp && lab.s.jn == pr.jn);
                checkTrue(name + "amplitudes at the old and at the new dots, old way = new way, bit for bit", lab.s.jA == pr.old.jA && lab.s.jB == pr.old.jB);
                checkTrue(name + "the walker the new cloud goes on from, old way = new way, bit for bit", lab.s.w2.x == pr.old.w2.x && lab.s.w2.y == pr.old.w2.y && lab.s.w2.z == pr.old.w2.z);
            } else {                                                              // no glide: the jump is over at once, the new dots are the cloud now
                bool same = lab.s.k == pr.jn && pr.jn > 0 && lab.s.w.x == pr.old.w2.x && lab.s.w.y == pr.old.w2.y && lab.s.w.z == pr.old.w2.z;
                for (long i = 0; same && i < 3 * pr.jn; i++) same = lab.s.pts[std::size_t(i)] == pr.old.jp[std::size_t(i)];
                for (long i = 0; same && i < pr.jn; i++) same = lab.s.sg[std::size_t(i)] == (pr.old.jB[std::size_t(4 * i + 2)] >= 0 ? 1 : -1);
                checkTrue(name + "the cloud after an instant jump = the old way's new dots, signs and walker, bit for bit", same);
            }
            // the whole run to the end, the jump built over the frames against built at once: the same cloud after landing, bit for bit
            atom::AtomLab a = jumpLabFor(c), b = jumpLabFor(c);                   // (same seed: the same start; the generator is global, so each one is run on its own, from the same state)
            const std::uint32_t s0 = atom::randomState();
            auto landed = [&](atom::AtomLab& L, bool atOnce) {
                atom::seedRandom(s0);
                L.startJump(c.kind, atom::orbIndex(c.to));
                for (int g = 0; g < 600 && !L.s.trans && !c.reduced; g++) L.step(1.0 / 240);
                if (atOnce) L.completeJump();
                for (int i = 0; i < 700 && (L.s.trans || L.s.photon || L.s.photonAfter); i++) L.step(1.0 / 240);
                for (int i = 0; i < 50; i++) L.step(1.0 / 240);
            };
            landed(a, false); const std::uint32_t ra = atom::randomState();
            landed(b, true); const std::uint32_t rb = atom::randomState();
            checkTrue(name + "the cloud after landing is the same built over the frames or at once (dots, signs, phases, walker, count, generator), bit for bit",
                      a.s.pts == b.s.pts && a.s.sg == b.s.sg && a.s.ph == b.s.ph && a.s.k == b.s.k && a.s.w.x == b.s.w.x && a.s.w.y == b.s.w.y && a.s.w.z == b.s.w.z && ra == rb);
        }
        // ---- 2. against the numbers recorded on Linux, with the tolerances above ----
        const JumpSummary got = jumpSummary(c), &w = want[ci];
        const double nDots = std::max(1.0, w.jn * 2), shownTol = 0.0005 * nDots + 1;
        checkNum(name + "recorded: dot count", got.jn, w.jn, 0);
        for (int a = 0; a < 3; a++) checkNum(name + "recorded: the walker at the end of the build, coordinate " + std::to_string(a), got.w2[a], w.w2[a], 1e-9, 1e-12);
        checkNum(name + "recorded: sum of |new dot coordinates|", got.sumJp, w.sumJp, 1e-6);
        checkNum(name + "recorded: sum of |amplitudes at the old dots|", got.sumJA, w.sumJA, 1e-6);
        checkNum(name + "recorded: sum of |amplitudes at the new dots|", got.sumJB, w.sumJB, 1e-6);
        checkNum(name + "recorded: dots shown at 0.3 of the jump (Sign)", got.shownSign03, w.shownSign03, 0, shownTol);
        checkNum(name + "recorded: dots shown at 0.7 of the jump (Sign)", got.shownSign07, w.shownSign07, 0, shownTol);
        checkNum(name + "recorded: sum of the phase colour numbers at 0.7 (Wheel)", got.wheelSum07, w.wheelSum07, 1e-3, 1);
        checkNum(name + "recorded: dots in the cloud after landing", got.k, w.k, 0);
        for (int a = 0; a < 3; a++) checkNum(name + "recorded: the walker after landing, coordinate " + std::to_string(a), got.w[a], w.w[a], 1e-9, 1e-12);
        checkNum(name + "recorded: sum of |dot coordinates| after landing", got.sumPts, w.sumPts, 1e-6);
        checkNum(name + "recorded: sum of the phases after landing", got.sumPh, w.sumPh, 1e-6);
        checkNum(name + "recorded: sum of the signs after landing", got.sumSg, w.sumSg, 0, 0.0005 * (w.jn > 0 ? w.jn : 12000) + 1);
        for (int a = 0; a < 3; a++) checkNum(name + "recorded: the next random number " + std::to_string(a), got.rng[a], w.rng[a], 0);
    }
}

// The new parts of the jump's speed work, each against the old way of getting the same numbers.
static void testJumpBuild() {
    using namespace atom;
    // random numbers: skipping n is the same as drawing n
    for (std::uint64_t n : {0ull, 1ull, 7ull, 5600ull, 343000ull}) {
        seedRandom(4242);
        const std::uint32_t s0 = randomState();
        for (std::uint64_t i = 0; i < n; i++) (void)atom::random();
        const std::uint32_t drawn = randomState();
        seedRandom(4242); skipRandom(n);
        checkTrue("random numbers: skipRandom(" + std::to_string(n) + ") ends on the state n draws end on", randomState() == drawn && (n == 0 || drawn != s0));
        seedRandom(4242); for (std::uint64_t i = 0; i < n; i++) (void)atom::random();
        const double a = atom::random();
        seedRandom(4242); skipRandom(n);
        checkTrue("random numbers: and the next number is the same", a == atom::random());
    }
    // the fractions with the terms worked out once are the same numbers, bit for bit
    {
        seedRandom(99);
        long same = 0, total = 0;
        for (int k = 0; k < 20000; k++) {
            const std::complex<double> A(atom::random() - 0.5, atom::random() - 0.5), B(atom::random() - 0.5, atom::random() - 0.5);
            const double u = atom::random(), th = kPi / 2 * u, phi = (atom::random() - 0.5) * 12;
            double fA, fB, gA, gB;
            jumpFractions(A, B, th, phi, fA, fB);
            jumpFractions(A, B, jumpTerms(th, phi, u), gA, gB);
            total++; if (fA == gA && fB == gB) same++;
        }
        checkTrue("jumpFractions with per-frame terms: 20,000 random cases, the same bits as the old call", same == total);
        long samePh = 0;                                                         // the phase of a dot: the old per-dot expression (AtomLab::jumpPhase) against jumpPhaseOf
        for (int k = 0; k < 20000; k++) {
            const std::complex<double> A(atom::random() - 0.5, atom::random() - 0.5), B(atom::random() - 0.5, atom::random() - 0.5);
            const double u = atom::random(), th = kPi / 2 * u, phi = (atom::random() - 0.5) * 12;
            const double old = std::arg(std::exp(std::complex<double>(0, phi * u)) * (std::cos(th) * A + std::sin(th) * B * std::exp(std::complex<double>(0, -phi))));
            if (old == jumpPhaseOf(A, B, jumpTerms(th, phi, u))) samePh++;
        }
        checkTrue("jumpPhaseOf with per-frame terms: 20,000 random cases, the same bits as the old per-dot expression", samePh == 20000);
    }
    // every dot's group in one call is the same number as the call for each dot, in both colour modes and at several moments of the jump
    for (const char* phase : {"Sign", "Wheel"}) {
        AtomLab lab; lab.nowMs = [] { return 0.0; };
        seedRandom(31);
        lab.p = AtomParams(); lab.restart();
        lab.setParam("Z", 6.0); lab.setParam("N", 6.0); lab.setParam("view", std::string("Cloud 3D")); lab.setParam("cdens", std::string("High")); lab.setParam("orb", std::string("3d")); lab.setParam("phase", std::string(phase));
        for (int i = 0; i < 40000 && lab.s.k < lab.cloudCap(); i++) lab.step(1.0 / 240);
        lab.startJump("emit", orbIndex("2p|z"));
        lab.completeJump();
        const long jn = lab.jumpDots();
        long same = 0, shown = 0;
        for (double t : {0.0, 0.05, 0.3, 0.6, 0.9, 1.19}) {
            lab.s.trans->t = t;
            std::vector<unsigned char> g(std::size_t(2 * jn), 7);
            lab.jumpGroups(g.data());
            for (long i = 0; i < jn; i++) { same += g[std::size_t(i)] == (unsigned char)lab.jumpGroup(0, i); same += g[std::size_t(jn + i)] == (unsigned char)lab.jumpGroup(1, i); shown += g[std::size_t(i)] != 255; }
        }
        checkTrue(std::string("jumpGroups = jumpGroup for every dot (") + phase + ", " + std::to_string(jn) + " dots x 2 populations x 6 moments)", same == 12 * jn && shown > 0);
    }
    // the build over a few frames: the start frame builds no dots, a few frames finish it, nothing is read half built, and the result is the one built all at once
    for (const char* dens : {"Normal", "Ultra"}) {
        AtomLab A, B;
        for (AtomLab* L : {&A, &B}) {
            seedRandom(8); L->nowMs = [] { return 0.0; };
            L->p = AtomParams(); L->restart();
            L->setParam("Z", 3.0); L->setParam("N", 4.0); L->setParam("view", std::string("Cloud 3D")); L->setParam("cdens", std::string(dens)); L->setParam("orb", std::string("4f"));
            for (int i = 0; i < 40000 && L->s.k < L->cloudCap(); i++) L->step(1.0 / 240);
            L->startJump("emit", L->targets(-1).front());
        }
        const std::string w = std::string("jump build, ") + dens + ": ";
        checkTrue(w + "right after the start the jump is not built (the start frame does not carry it) and is not drawn yet", A.s.trans && !A.jumpReady() && !A.jumpDrawn() && A.jumpCloud() && A.s.jn == A.cloudCap());
        int frames = 0;
        while (!A.jumpReady() && frames < 100) { A.step(1.0 / 240); frames++; }
        checkTrue(w + "built by the frames alone, in 4 or 5 of them (" + std::to_string(frames) + ")", A.jumpReady() && frames >= 3 && frames <= 5 && A.jumpDrawn());
        B.completeJump();                                                          // the other lab: all at once, at once
        for (int i = 0; i < frames; i++) B.step(1.0 / 240);                         // the same number of frames, so the clocks agree
        checkTrue(w + "the dots, amplitudes and walker are the same built over frames or all at once, bit for bit",
                  A.s.jp == B.s.jp && A.s.jA == B.s.jA && A.s.jB == B.s.jB && A.s.w2.x == B.s.w2.x && A.s.w2.y == B.s.w2.y && A.s.w2.z == B.s.w2.z && A.s.jb.rng == B.s.jb.rng);
        checkTrue(w + "asking about a dot of a half-built jump gives the finished jump", [&] {
            AtomLab C; seedRandom(8); C.nowMs = [] { return 0.0; }; C.p = AtomParams(); C.restart();
            C.setParam("Z", 3.0); C.setParam("N", 4.0); C.setParam("view", std::string("Cloud 3D")); C.setParam("cdens", std::string(dens)); C.setParam("orb", std::string("4f"));
            for (int i = 0; i < 40000 && C.s.k < C.cloudCap(); i++) C.step(1.0 / 240);
            C.startJump("emit", C.targets(-1).front());
            C.step(1.0 / 240);                                                        // one chunk built, the rest not
            const bool part = !C.jumpReady();
            (void)C.jumpShown(1, 5);
            return part && C.jumpReady() && C.s.jp == A.s.jp && C.s.jB == A.s.jB;
        }());
    }
    // a jump that is over before it is built (a very long frame) is built first, so the landing is the same cloud
    {
        AtomLab A, B;
        for (AtomLab* L : {&A, &B}) {
            seedRandom(12); L->nowMs = [] { return 0.0; }; L->p = AtomParams(); L->restart();
            L->setParam("view", std::string("Cloud 3D")); L->setParam("orb", std::string("2p"));
            for (int i = 0; i < 40000 && L->s.k < L->cloudCap(); i++) L->step(1.0 / 240);
            L->startJump("emit", orbIndex("1s"));
        }
        seedRandom(555); A.step(5.0);                                                 // one frame of 5 s: the jump is over at once (the cloud goes on after the landing: the same numbers for both)
        B.completeJump(); seedRandom(555); B.step(5.0);
        checkTrue("jump build: a jump ended by one very long frame lands on the same cloud as one built first", !A.s.trans && !B.s.trans && A.s.pts == B.s.pts && A.s.sg == B.s.sg && A.s.k == B.s.k && A.s.jn == 0 && A.s.jb.done == 0);
    }
}

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

// ================= 0.10.6 stage 4: the outline of a complex state, and the norm of a superposition =================
// Finding 1 (audit): the view drew the 90 % outline of the real orbital the Orientation falls back to (fullOrb), not of the state on screen. The view now asks AtomLab::outlineStateId().
// The drawing itself cannot be tested here; this checks one level down what the view would ask for, and that the answer has a different shape from the fallback's.
static void testOutlineIsTheState(double margin) {
    int n = 0; double minDiff = 1e9; std::string which; bool allRight = true, axisOk = true;
    for (int id : complexIds()) {
        const Orbital& o = orbitals()[stateOrbital(id)];
        AtomLab lab; lab.nowMs = [] { return 0.0; }; lab.p = AtomParams(); lab.restart();
        lab.setParam("view", std::string("Cloud")); lab.setParam("orb", o.shell);
        lab.setParam(o.l == 1 ? "pAx" : o.l == 2 ? "dAx" : "fAx", oriKey(stateM(id)));
        const int fb = lab.fullOrb();                                       // what the view used to ask for: the real fallback
        allRight = allRight && lab.outlineStateId() == id && lab.stateId() == id && fb != id && !isComplexState(fb) && orbitals()[fb].n == o.n && orbitals()[fb].l == o.l;
        const Outline &c = outlineOf(lab.outlineStateId()), &f = outlineOf(fb);
        auto ext = [](const Outline& O, double& ax, double& az) { ax = az = 0; for (const OutSeg2& g : O.side) { ax = std::max({ax, double(std::fabs(g.x0)), double(std::fabs(g.x1))}); az = std::max({az, double(std::fabs(g.y0)), double(std::fabs(g.y1))}); } };
        double ax1, az1, ax2, az2; ext(c, ax1, az1); ext(f, ax2, az2);
        // the shape differs: the largest relative difference of the 90 % radius, the side view's width and its height
        const double d = std::max({std::fabs(c.rmax / f.rmax - 1), std::fabs(ax1 / ax2 - 1), std::fabs(az1 / az2 - 1)});
        if (d < minDiff) { minDiff = d; which = stateText(id); }
        axisOk = axisOk && density(id, 1, 0, 0, 3) < 1e-12 * density(fb, 1, 0, 0, 3);   // every m != 0 state is zero on the z axis; the m = 0 fallback is not
        n++;
    }
    std::printf("outline of complex states: %d states; the 90 %% shape differs from the real fallback's by at least %.4f (%s); margin %.3f\n", n, minDiff, which.c_str(), margin);
    checkTrue("outline: for every complex state the view asks for that state's outline, not the real fallback's (20 states)", n == 20 && allRight);
    checkTrue("outline: every complex state is zero on the z axis, its fallback is not", axisOk);
    checkTrue("outline: the complex state's 90 % shape (radius, side width or height) differs from the fallback's by more than the margin", minDiff > margin);
}

// Finding 2 (audit): the integral of the drawn density |psi|^2 over all space is 1 for ANY pair, also when the states overlap (same state twice, a real orbital with an m state of its shell).
// For a = cos, b = sin: integral |a A + b B e^(-i theta)|^2 = a^2 + b^2 + 2 a b Re(<A|B> e^(-i theta)), which is 1 only if <A|B> = 0, so psiSuper divides by that.
static void testSuperNorm(double tol) {
    struct P { const char* sa; const char* oa; const char* sb; const char* ob; bool overlap; };
    const P ps[] = {{"2p", "z", "2p", "z", true}, {"1s", "", "1s", "", true}, {"2p", "x", "2p", "m+1", true}, {"2p", "y", "2p", "m-1", true}, {"3d", "xz", "3d", "m+1", true}, {"3d", "x2y2", "3d", "m+2", true},
        {"4f", "xz2", "4f", "m+1", true}, {"3d", "m+2", "3d", "m+2", true},
        {"2p", "m+1", "2p", "m-1", false}, {"2p", "x", "2p", "y", false}, {"1s", "", "2p", "z", false}, {"1s", "", "2s", "", false}, {"2s", "", "2p", "z", false}, {"2p", "m+1", "3d", "m+2", false},
        {"3d", "z2", "3d", "xy", false}, {"3d", "xz", "3d", "m+2", false}, {"4p", "z", "4f", "z3", false}, {"2p", "x", "3p", "x", false}};
    const Quad q = makeQuad(450, 16, 50);
    double worst = 0, worstOverlap = 0, worstOrtho = 0; std::string wp;
    for (const P& pr : ps)
        for (double mix : {0.0, 0.5, 1.0})
            for (double th : {0.0, 2.5}) {
                const Superposition su = mkSup(pr.sa, pr.oa, pr.sb, pr.ob, mix);
                double sum = 0;
                for (std::size_t i = 0; i < q.r.size(); i++)
                    for (std::size_t j = 0; j < q.u.size(); j++) {
                        const double st = std::sqrt(1 - q.u[j] * q.u[j]);
                        for (int k = 0; k < q.nphi; k++) {
                            const double f = 2 * kPi * (k + 0.5) / q.nphi;
                            sum += std::norm(psiSuper(su, 1, th, q.r[i] * st * std::cos(f), q.r[i] * st * std::sin(f), q.r[i] * q.u[j])) * (2 * kPi / q.nphi) * q.wu[j] * q.r[i] * q.r[i] * q.wr[i];
                        }
                    }
                const double err = std::fabs(sum - 1);
                if (err > worst) { worst = err; wp = std::string(pr.sa) + pr.oa + " + " + pr.sb + pr.ob + " mix " + std::to_string(int(mix * 100)); }
                (pr.overlap ? worstOverlap : worstOrtho) = std::max(pr.overlap ? worstOverlap : worstOrtho, err);
            }
    std::printf("superposition norm: integral of |psi|^2 over %zu pairs x mix 0, 50, 100 %% x 2 phases: largest |norm - 1| = %.5e (overlapping pairs %.5e, orthogonal %.5e; worst: %s)\n", sizeof ps / sizeof ps[0], worst, worstOverlap, worstOrtho, wp.c_str());
    checkTrue("superposition: the drawn density integrates to 1 for every pair, overlapping or not, at mix 0, 50 and 100 % (guard = measured + 1 %)", worst <= tol);
    // the overlap itself, from the maths: the same state 1; 2px with 2p m=+1 is -1/sqrt 2 (m=+1 = -(px + i py)/sqrt 2); orthogonal pairs exactly 0
    checkNum("overlap: <2pz|2pz> = 1", std::abs(overlapAB(orbIndex("2p|z"), orbIndex("2p|z"))), 1, 1e-12);
    checkNum("overlap: |<2px|2p m=+1>| = 1/sqrt 2", std::abs(overlapAB(orbIndex("2p|x"), complexState(orbIndex("2p"), 1))), 1 / std::sqrt(2.0), 1e-9);
    checkNum("overlap: |<2px|2p m=-1>| = 1/sqrt 2", std::abs(overlapAB(orbIndex("2p|x"), complexState(orbIndex("2p"), -1))), 1 / std::sqrt(2.0), 1e-9);
    checkTrue("overlap: exactly 0 for orthogonal pairs (m=+1 and m=-1, px and py, 1s and 2p, 2s and 2p, different n)", overlapAB(complexState(orbIndex("2p"), 1), complexState(orbIndex("2p"), -1)) == 0.0 && overlapAB(orbIndex("2p|x"), orbIndex("2p|y")) == 0.0
              && overlapAB(orbIndex("1s"), orbIndex("2p|z")) == 0.0 && overlapAB(orbIndex("2s"), orbIndex("2p|z")) == 0.0 && overlapAB(orbIndex("2p|x"), orbIndex("3p|x")) == 0.0);
}

// The dots of an overlapping pair follow the renormalised density and ALL of them are shown (no thinning): for 2px + 2p(m=+1) at mix 0, 50 and 100 % the mean x^2, y^2, z^2 of the dots equal the
// integrals of x^2 |psi|^2 etc. of the lab's own psiSuper (quadrature), within the Metropolis chain's sampling error (the 8 labs are independent, every dot of a lab is used).
static void testOverlapDots(double tol) {
    const Quad q = makeQuad(450, 16, 50);
    double worst = 0; bool allShown = true; std::string line;
    for (double mix : {0.0, 0.5, 1.0}) {
        const Superposition su = mkSup("2p", "x", "2p", "m+1", mix);
        double ex[3] = {0, 0, 0}, nrm = 0;
        for (std::size_t i = 0; i < q.r.size(); i++)
            for (std::size_t j = 0; j < q.u.size(); j++) {
                const double st = std::sqrt(1 - q.u[j] * q.u[j]);
                for (int k = 0; k < q.nphi; k++) {
                    const double f = 2 * kPi * (k + 0.5) / q.nphi, x = q.r[i] * st * std::cos(f), y = q.r[i] * st * std::sin(f), z = q.r[i] * q.u[j];
                    const double w = std::norm(psiSuper(su, 1, 0, x, y, z)) * (2 * kPi / q.nphi) * q.wu[j] * q.r[i] * q.r[i] * q.wr[i];
                    ex[0] += w * x * x; ex[1] += w * y * y; ex[2] += w * z * z; nrm += w;
                }
            }
        double got[3] = {0, 0, 0}; long cnt = 0;
        for (int lab_ = 0; lab_ < 8; lab_++) {
            seedRandom(9000 + lab_ + int(mix * 10));
            AtomLab lab; lab.nowMs = [] { return 0.0; }; lab.p = AtomParams(); lab.restart();
            lab.setParam("view", std::string("Cloud")); lab.setParam("orb", std::string("2p")); lab.setParam("pAx", std::string("x"));
            lab.setParam("super", std::string("On")); lab.setParam("orb2", std::string("2p")); lab.setParam("pAx2", std::string("m+1")); lab.setParam("mix", mix * 100);
            for (int i = 0; i < 800; i++) lab.step(1.0 / 240);
            const long c = std::min<long>(lab.s.k, long(lab.s.sg.size()));
            for (long i = 0; i < c; i++) {
                allShown = allShown && lab.superGroup(i, lab.beatTheta()) != 255;
                for (int a = 0; a < 3; a++) { const double v = lab.s.pts[3 * i + a] / kA0; got[a] += v * v; }
            }
            cnt += c;
        }
        for (int a = 0; a < 3; a++) { got[a] /= cnt; worst = std::max(worst, std::fabs(got[a] / (ex[a] / nrm) - 1)); }
        line += " mix " + std::to_string(int(mix * 100)) + ": <x2,y2,z2> dots " + fmt::fixed(got[0], 2) + "," + fmt::fixed(got[1], 2) + "," + fmt::fixed(got[2], 2) + " vs integral " + fmt::fixed(ex[0] / nrm, 2) + "," + fmt::fixed(ex[1] / nrm, 2) + "," + fmt::fixed(ex[2] / nrm, 2) + ";";
    }
    std::printf("overlapping pair 2px + 2p(m=+1), dots against the integral of psiSuper:%s largest relative difference %.5f\n", line.c_str(), worst);
    checkTrue("overlapping pair: every dot is shown (no thinning)", allShown);
    checkTrue("overlapping pair: the dots' mean x^2, y^2, z^2 equal those of the renormalised density at mix 0, 50 and 100 % (guard = measured + 1 %)", worst <= tol);
}

// The readout must not claim a split that is not drawn: orthogonal pairs keep "(50 % / 50 %)"; an overlapping pair says the mixture is renormalised.
static void testSuperReadout() {
    auto mk = [](const char* a, const char* ao, const char* b, const char* bo) {
        AtomLab lab; lab.nowMs = [] { return 0.0; }; lab.p = AtomParams(); lab.restart();
        lab.setParam("view", std::string("Cloud")); lab.setParam("orb", std::string(a)); if (*ao) lab.setParam(orbitals()[orbIndex(a)].l == 1 ? "pAx" : "dAx", std::string(ao));
        lab.setParam("super", std::string("On")); lab.setParam("orb2", std::string(b)); if (*bo) lab.setParam(orbitals()[orbIndex(b)].l == 1 ? "pAx2" : "dAx2", std::string(bo));
        return lab.info1();
    };
    const std::string ortho = mk("1s", "", "2p", "z"), overlap = mk("2p", "x", "2p", "m+1"), same = mk("2p", "z", "2p", "z");
    checkTrue("readout: an orthogonal pair keeps its exact split (50 % / 50 %)", ortho.find("(50 % / 50 %)") != std::string::npos && ortho.find("overlap") == std::string::npos);
    checkTrue("readout: an overlapping pair does not claim (50 % / 50 %) and says the states overlap and the mixture is renormalised", overlap.find("(50 % / 50 %)") == std::string::npos && overlap.find("overlap") != std::string::npos && overlap.find("renormalised") != std::string::npos);
    checkTrue("readout: the same state twice says so (overlap 100 %)", same.find("(50 % / 50 %)") == std::string::npos && same.find("overlap") != std::string::npos && same.find("100 %") != std::string::npos);
}

// ================= 0.10.6 stage 5: wrong and misleading text =================
// Finding 6: ONE relativity line (Z = 27) in the readout, the hint and the spectrum note; "non-relativistic" in the readout at every Z; the nucleus line carries its caveat.
static void testRelativityText() {
    int agree = 0; bool nonrel = true, caveat = true, model = true, threshold = true;
    for (int Z : {1, 19, 20, 26, 27, 92, 118}) {
        AtomLab lab; lab.nowMs = [] { return 0.0; }; lab.p = AtomParams(); lab.restart();
        lab.setParam("Z", double(Z)); lab.setParam("view", std::string("Cloud")); lab.setParam("zoom", 2.0);
        std::string all; for (const std::string& l : lab.info()) all += l + "\n";
        const bool warnRead = all.find("1s electron speed:") != std::string::npos, warnHint = lab.hint().find("Heavy atoms: this lab uses non-relativistic physics") != std::string::npos,
                   warnSpec = spectrumNote(Z, lab.p.N).find("so these are approximate") != std::string::npos, want = Z >= 27;
        if (warnRead == warnHint && warnHint == warnSpec && warnSpec == want) agree++;
        else std::printf("  relativity warning at Z = %d: readout %d, hint %d, spectrum note %d (want %d)\n", Z, warnRead, warnHint, warnSpec, want);
        nonrel = nonrel && all.find("non-relativistic") != std::string::npos;
        // the nucleus line (s orbital, zoomed in) has its caveat
        const std::size_t k = all.find(kNucPrefix);
        caveat = caveat && k != std::string::npos && all.find(kNucSuffix, k) != std::string::npos;
        // the model line: the Dirac 1s level is deeper than the non-relativistic one by this much (independent: NIST alpha typed in the nist:: block)
        const double x = Z * nist::alpha, sq = std::sqrt(1 - x * x), pct = 100 * x * x / ((1 + sq) * (1 + sq));   // = 100 ((1 - sq) / ((Z alpha)^2 / 2) - 1) without the subtractions (0.10.8 stage 2)
        model = model && all.find(kModelPrefix + " (no relativity, fine structure, Lamb shift or spin). For a point nucleus the Dirac equation puts the 1s level " + jsNum(pct, 2) + " % deeper than this lab's.") != std::string::npos;
    }
    checkTrue("relativity: the readout, the hint and the spectrum note warn at exactly the same Z (none below 27, all three from 27) for Z = 1, 19, 20, 26, 27, 92, 118", agree == 7);
    checkTrue("relativity: the word non-relativistic is in the readout at every Z (1, 19, 20, 26, 27, 92, 118)", nonrel);
    checkTrue("relativity: the model line gives the Dirac-minus-lab 1s difference at every Z, from the lab's own formula", model);
    checkTrue("relativity: the chance-inside-the-nucleus line says it is a non-relativistic estimate that this lab does not correct", caveat);
    // the line: the correction to the 1s energy is 0.917 % at Z = 26 and 0.990 % at Z = 27 (about 1 %); (Z alpha)^2 = 0.0388 and v/c = 0.197 at 27
    auto corr = [](int Z) { const double x = Z * nist::alpha, sq = std::sqrt(1 - x * x); return 100 * x * x / ((1 + sq) * (1 + sq)); };   // cancellation-free (0.10.8 stage 2)
    threshold = std::fabs(corr(26) - 0.917) < 0.001 && std::fabs(corr(27) - 0.990) < 0.001 && corr(19) < 0.5 && kRelativityZ == 27 && std::fabs(27 * nist::alpha - 0.197) < 0.001;
    checkTrue("relativity: the threshold Z = 27 is where the 1s energy is 0.99 % off (0.917 % at 26), Z alpha = 0.197, (Z alpha)^2 = 0.039", threshold);
}

// Finding 7: the two Learn steps. The text lives in the generator's edit list (tools/native_edits.js, applied by tools/make_atom_learn.js); this checks what the app finally says.
static void testTimelineText() {
    const TimelineStep *pauli = nullptr, *stod = nullptr;
    for (const TimelineStep& st : timelineSteps()) { if (std::string(st.y) == "1925" && std::string(st.t) == "Why electrons form shells") pauli = &st; if (std::string(st.y) == "2013") stod = &st; }
    checkTrue("timeline: the Pauli and the 2013 steps are there", pauli && stod);
    if (!pauli || !stod) return;
    std::string zs, ns; for (const auto& kv : pauli->set) { if (kv.first == "Z") zs = kv.second; if (kv.first == "N") ns = kv.second; }
    checkTrue("timeline: the Pauli button says it opens a neon nucleus with ONE electron (not 'See neon')", std::string(pauli->lab) == "Open a neon nucleus with one electron" && zs == "10" && ns == "10");
    checkTrue("timeline: the Pauli text says the lab draws one electron, so it cannot show a full shell (Ne⁹⁺)", std::string(pauli->d).find("The lab draws only one electron, so it cannot show a full shell like this: the button opens a neon nucleus with a single electron (Ne⁹⁺).") != std::string::npos);
    const std::string d = stod->d;
    checkTrue("timeline: the 2013 step no longer calls it a direct picture of the quantum cloud, or the same kind of shape", d.find("direct picture") == std::string::npos && d.find("same kind of shape") == std::string::npos && std::string(stod->t).find("Photographing") == std::string::npos);
    checkTrue("timeline: the 2013 step says a pattern recorded on a detector, not a picture of a cloud like those in the lab, which are worked out from theory",
              d.find("It is a pattern recorded on a detector, evidence for the electron’s wave, not a picture of a cloud like the ones in this lab: those are worked out from the theory.") != std::string::npos && std::string(stod->t) == "An electron wave, seen on a detector");
    checkTrue("timeline: the 2013 button says the cloud it opens is worked out from theory", std::string(stod->lab) == "Open a hydrogen cloud (worked out from theory)");
}

// Finding 13: one visible range (380-780 nm, both ends included) for the spectrum card, the colour code and the photon tag.
static void testVisibleRange() {
    bool ok = true;
    for (double nm : {379.0, 380.0, 381.0, 749.0, 750.0, 751.0, 779.0, 780.0, 781.0}) {
        const bool vis = nm >= 380 && nm <= 780;                                  // the range, written here again
        const std::string css = lamColorCss(nm), tag = bandTag(nm);
        const bool card = isVisibleNm(nm), colour = css.compare(0, 4, "rgb(") == 0, noTag = tag.empty();
        const Rgb c = lamColor(nm); const bool standIn = (c.r == 0xb0 && c.g == 0x6c && c.b == 0xff) || (c.r == 0xd2 && c.g == 0x4a && c.b == 0x3c);
        const bool here = card == vis && colour == vis && noTag == vis && standIn == !vis && (vis || tag == (nm < 380 ? " (UV)" : " (IR)"));
        if (!here) std::printf("  visible range at %.0f nm: card %d, colour %d, tag '%s', stand-in %d (want visible %d)\n", nm, card, colour, tag.c_str(), standIn, vis);
        ok = ok && here;
    }
    checkTrue("visible range: the spectrum card, the line colour and the (UV)/(IR) tag agree at 379, 380, 381, 749, 750, 751, 779, 780, 781 nm", ok);
    // the card's visible strip: the lines it marks are exactly the lines with a real colour and no tag, for every element
    bool same = true; long nVis = 0;
    for (int Z = 1; Z <= 118; Z++) {
        const auto strip = visibleLines(Z, defaultN(Z));
        long inColour = 0;
        for (const SpecLine& l : specLines(Z, defaultN(Z))) if (l.hi != 0 && lamColorCss(l.lam).compare(0, 4, "rgb(") == 0 && std::string(bandTag(l.lam)).empty()) inColour++;
        same = same && long(strip.size()) == inColour; nVis += inColour;
        for (const SpecLine& l : strip) same = same && l.lam >= 380 && l.lam <= 780;
    }
    checkTrue("visible range: for all 118 elements the lines the card marks as visible are exactly those the colour code draws in real colour", same && nVis > 0);
}

// Finding 15: "avg r: sampled" only with the full ring of kCap dots, with its honest note; and the sampled average against the exact <r> = a0 (3 n^2 - l (l + 1)) / (2 Z).
static void testSampledRadius(double guard) {
    struct C { const char* orb; const char* key; const char* ori; } cs[] = {{"1s", "", ""}, {"2s", "", ""}, {"2p", "pAx", "z"}, {"2p", "pAx", "m+1"}, {"3s", "", ""}, {"3p", "pAx", "z"}, {"3d", "dAx", "z2"}, {"3d", "dAx", "m+2"},
                                                                  {"4s", "", ""}, {"4d", "dAx", "x2y2"}, {"4f", "fAx", "z3"}, {"4f", "fAx", "m+3"}};
    double worst = 0, sumd = 0; long n = 0, within10 = 0; bool placeholderOk = true, numberOk = true, noteOk = true;
    for (const C& c : cs)
        for (int seed = 1; seed <= 8; seed++) {
            seedRandom(5000 + seed);
            AtomLab lab; lab.nowMs = [] { return 0.0; }; lab.p = AtomParams(); lab.restart();
            lab.setParam("view", std::string("Cloud")); lab.setParam("orb", std::string(c.orb)); if (*c.key) lab.setParam(c.key, std::string(c.ori));
            const Orbital& o = orbitals()[orbIndex(c.orb)];
            const double theory = (3.0 * o.n * o.n - o.l * (o.l + 1)) / 2 * nist::a0;               // <r> for Z = 1, with the NIST Bohr radius typed above
            for (int i = 0; i < 100; i++) lab.step(1.0 / 240);                                       // 1,600 dots: more than the old 300, fewer than kCap
            placeholderOk = placeholderOk && lab.s.k > 300 && lab.s.k < kCap && lab.info1().find("sampled …, theory") != std::string::npos;
            for (int i = 0; i < 700; i++) lab.step(1.0 / 240);                                       // more than kCap dots: the ring is full
            const long cnt = std::min<long>(lab.s.k, lab.cloudCap());
            double sum = 0; for (long i = 0; i < cnt; i++) sum += std::hypot(double(lab.s.pts[3 * i]), double(lab.s.pts[3 * i + 1]), double(lab.s.pts[3 * i + 2]));
            const double d = std::fabs(sum / cnt / theory - 1);
            worst = std::max(worst, d); sumd += d; n++; if (d < 0.10) within10++;
            const std::string line = lab.info1();
            numberOk = numberOk && line.find("sampled …") == std::string::npos && line.find("avg r: sampled ") != std::string::npos;
            noteOk = noteOk && line.find(kSampledNote + ", theory") != std::string::npos;
        }
    std::printf("sampled <r> at the full ring (12,000 dots), %ld labs (12 orbitals x 8 seeds): mean |difference| %.4f, worst %.5f, within 10 %%: %ld of %ld\n", n, sumd / n, worst, within10, n);
    checkTrue("sampled <r>: below the full ring of kCap dots the readout shows the placeholder, not a number", placeholderOk);
    checkTrue("sampled <r>: with the full ring the readout shows the number, with the note 'an estimate, usually within 10 %'", numberOk && noteOk);
    checkTrue("sampled <r>: at kCap dots the sampled average is within the guard of the exact a0 (3n^2 - l(l+1))/2Z (guard = measured worst + 1 %)", worst <= guard);
    checkTrue("sampled <r>: the stated 'usually within 10 %' holds for at least 95 % of the runs", within10 >= 0.95 * n);
}

// ================= 0.10.6 stage 6: the nuclear mass from a source, an independent spectrum check, four basic checks =================
// Finding 5: the nuclear mass. Reference: AME2020 atomic masses, https://www-nds.iaea.org/amdc/ame2020/mass.mas20.txt (read 2026-10-04; "ATOMIC MASS" column, micro-u, typed in below), with the NIST m_e in u
// (nist::meInU): M_nucleus = atomic mass / m_e - Z, in electron masses (the electrons' binding energy, which this neglects, is far below the lab's own rounding of the table to 1 keV per nucleon).
struct AmeRef { int Z, N; const char* name; double microU; };
static const AmeRef kAme[] = {{1, 0, "H-1", 1007825.031898}, {1, 1, "H-2", 2014101.777844}, {1, 2, "H-3", 3016049.28132}, {2, 2, "He-4", 4002603.25413}, {6, 6, "C-12", 12000000.0}, {26, 30, "Fe-56", 55934935.537}, {92, 146, "U-238", 238050786.936}};
static double ameNuclearMe(const AmeRef& a) { return a.microU * 1e-6 / nist::meInU - a.Z; }
static void testNuclearMass(double guard) {
    double worst = 0, worstOld = 0; std::string line;
    for (const AmeRef& a : kAme) {
        const double ref = ameNuclearMe(a), lab = nuclearMassMe(a.Z, a.N), old = (a.Z + a.N) / nist::meInU - a.Z - (a.Z == 1 && a.N == 0 ? (a.Z + a.N) / nist::meInU - a.Z - nist::mpOverMe : 0);
        worst = std::max(worst, std::fabs(lab / ref - 1)); worstOld = std::max(worstOld, std::fabs(old / ref - 1));
        line += std::string(" ") + a.name + " " + fmt::fixed(100 * (lab / ref - 1), 6) + " % (old form " + fmt::fixed(100 * (old / ref - 1), 4) + " %);";
    }
    std::printf("nuclear mass against AME2020 (relative difference):%s worst new %.3e, worst old form %.3e\n", line.c_str(), worst, worstOld);
    checkTrue("nuclear mass: the lab's M (electron masses) equals AME2020's atomic mass / m_e - Z for H-1, H-2, H-3, He-4, C-12, Fe-56, U-238 (guard = measured + 1 %)", worst <= guard);
    checkTrue("nuclear mass: the old form A u - Z m_e was off by more than 1e-3 for H-2 and H-3 (so this test would catch it)", std::fabs(((1 + 1) / nist::meInU - 1) / ameNuclearMe(kAme[1]) - 1) > 1e-3);
    // the deuterium D-alpha line by hand from the AME mass (Rydberg formula with the NIST values) against the lab's
    const double M = ameNuclearMe(kAme[1]), hand = nist::hcEVnm / (nist::rydEV * M / (M + 1) * (1.0 / 4 - 1.0 / 9));
    double lab = 0; for (const SpecLine& l : specLines(1, 1)) if (l.lo == 2 && l.hi == 3) lab = l.lam;
    std::printf("D-alpha: by hand from the AME2020 deuteron mass %.6f nm, lab %.6f nm (relative %.2e)\n", hand, lab, lab / hand - 1);
    checkNum("D-alpha: the lab's wavelength equals the hand value from the AME2020 deuteron mass (1e-9)", lab, hand, 1e-9);
    // the lines that moved: deuterium's D-alpha is shorter than before by about 1.3 pm (the old A u - Z m_e mass was 0.7 % low)
    const double oldM = 2 / nist::meInU - 1, oldLam = nist::hcEVnm / (nist::rydEV * oldM / (oldM + 1) * (1.0 / 4 - 1.0 / 9));
    checkTrue("D-alpha: it moved by about -1.3 pm against the old mass form (between -1.0 and -1.6 pm)", (lab - oldLam) * 1e3 < -1.0 && (lab - oldLam) * 1e3 > -1.6);
}

// Finding 16: the spectrum against two things the lab did not make. (a) The Rydberg formula written out here from the NIST values, with the nucleus's mass from AME2020 (He-4: the alpha particle).
// (b) NIST ASD, https://physics.nist.gov/cgi-bin/ASD/lines1.pl (read 2026-10-04): query spectra = "H I" and "He II", low_w = 90 / 20, upp_w = 2000 nm, unit = 1 (nm), format = 3 (tab-delimited),
// show_av = 3 (VACUUM wavelengths), show_obs_wl = 1, show_calc_wl = 1, allowed_out = 1, forbid_out = 1. The column ritz_wl_vac(nm) of the fine-structure COMPONENTS (J_i and J_k given) of each line is typed
// in below as [lowest, highest]. The lab is vacuum, one electron, non-relativistic, level-averaged (no fine structure, Lamb shift or spin), so its single wavelength can differ from the ASD's components by the
// size of what it leaves out: relativistic corrections are of the order (Z alpha)^2 of the level energies (5.3e-5 for hydrogen, 2.1e-4 for He+). The check is therefore that the lab's value lies inside the
// components' range widened by (Z alpha)^2 of the wavelength on each side. Measured: the lab lies outside the bare range by 8.9e-6 (H Lyman-alpha), 1.0e-6 (H-beta), 3.8e-5 (He+ Lyman-alpha); inside for H-alpha and He II 468.7 nm (the allowed distance is 5.3e-5 and 2.1e-4).
static double handLine(double M, int Z, int lo, int hi) { return nist::hcEVnm / (nist::rydEV * M / (M + 1) * Z * Z * (1.0 / (lo * lo) - 1.0 / (hi * hi))); }
static void testSpectrumIndependent() {
    struct Asd { int Z, N, lo, hi; const char* name; double minNm, maxNm; };
    const Asd as[] = {{1, 0, 1, 2, "H Lyman-alpha", 121.566823731, 121.5673644608}, {1, 0, 2, 3, "H-alpha", 656.4522556, 656.4722349}, {1, 0, 2, 4, "H-beta", 486.26365622, 486.273282785},
                      {2, 2, 1, 2, "He II 30.4 nm", 30.3780409484, 30.3785814724}, {2, 2, 3, 4, "He II 468.7 nm", 468.66881681, 468.722934627}};
    const double M[2] = {nist::mpOverMe, ameNuclearMe(kAme[3])};                 // the proton; the alpha particle from AME2020 (He-4 atomic mass - 2 m_e)
    bool handOk = true, asdOk = true; double worstHand = 0;
    for (const Asd& a : as) {
        double lab = 0; for (const SpecLine& l : specLines(a.Z, a.N)) if (l.lo == a.lo && l.hi == a.hi) lab = l.lam;
        const double hand = handLine(M[a.Z == 1 ? 0 : 1], a.Z, a.lo, a.hi), tol = std::pow(a.Z * nist::alpha, 2) * lab;
        const double outside = lab < a.minNm ? a.minNm - lab : lab > a.maxNm ? lab - a.maxNm : 0;
        worstHand = std::max(worstHand, std::fabs(lab / hand - 1));
        handOk = handOk && std::fabs(lab / hand - 1) < 1e-9; asdOk = asdOk && outside <= tol;
        std::printf("  %-14s lab %.6f nm, hand %.6f nm, ASD components %.6f..%.6f nm: lab outside by %.1e of the wavelength (allowed %.1e)\n", a.name, lab, hand, a.minNm, a.maxNm, outside / lab, tol / lab);
    }
    checkTrue("spectrum: H Lyman-alpha, H-alpha, H-beta and two He II lines equal the Rydberg formula written out from the NIST values (1e-9; worst " + fmt::fixed(worstHand, 12) + ")", handOk);
    checkTrue("spectrum: the same five lines lie inside NIST ASD's fine-structure components widened by (Z alpha)^2 (the size of what the lab does not model)", asdOk);
}

// Finding 17: four basic checks.
static void testBasicOrbitChecks() {
    // (a) the 90 % outline holds 90 % of the probability for all 30 real orbitals: 0.9 <= mass (by construction) and <= the measured worst 0.903 (probe) + 1 %
    double lo = 1, hi = 0; for (int i = 0; i < int(orbitals().size()); i++) { const double m = outlineOf(i).mass; lo = std::min(lo, m); hi = std::max(hi, m); }
    std::printf("90 %% outline mass over the 30 real orbitals: %.5f to %.5f\n", lo, hi);
    checkTrue("outline: all 30 real orbitals hold 90 % of the probability (0.9 up to the measured worst + 1 %: 0.912)", lo >= 0.9 && hi <= 0.912);
    // (b) the 1s norm: the integral of |e^-r|^2 over space is pi (a0^3)
    const double n1 = orbitalNorm(orbIndex("1s"));
    std::printf("1s normalisation integral %.9f, pi %.9f (relative %.2e)\n", n1, kPi, n1 / kPi - 1);
    checkNum("orbital: the 1s normalisation integral is pi (guard = measured 5.83e-10 + 1 %)", n1, kPi, 5.9e-10);
    // (c) radial nodes: the radial part R(q) has n - l - 1 sign changes for q > 0 (counted on a fine grid) for every orbital
    bool nodesOk = true; int checked = 0;
    for (const Orbital& o : orbitals()) {
        int changes = 0; double prev = radial(o, 1e-6);
        for (double q = 0.001; q < 80; q += 0.001) { const double v = radial(o, q); if ((v > 0) != (prev > 0) && v != 0) changes++; prev = v; }
        nodesOk = nodesOk && changes == o.n - o.l - 1; checked++;
    }
    checkTrue("orbital: every one of the 30 real orbitals has n - l - 1 radial nodes", nodesOk && checked == 30);
}
// (d) the sampled <r> against a0 (3n^2 - l(l+1)) / (2Z) for Z other than 1 (stage 5 tests Z = 1): the same dots, 3 and 26 times closer to the nucleus.
static void testSampledRadiusZ(double guard) {
    struct C { const char* orb; const char* key; const char* ori; } cs[] = {{"1s", "", ""}, {"2p", "pAx", "z"}, {"3d", "dAx", "z2"}, {"3s", "", ""}, {"4f", "fAx", "xyz"}, {"4s", "", ""}};
    double worst = 0; long n = 0;
    for (int Z : {3, 26})
        for (const C& c : cs)
            for (int seed = 1; seed <= 4; seed++) {
                seedRandom(7000 + seed);
                AtomLab lab; lab.nowMs = [] { return 0.0; }; lab.p = AtomParams(); lab.restart();
                lab.setParam("Z", double(Z)); lab.setParam("view", std::string("Cloud")); lab.setParam("orb", std::string(c.orb)); if (*c.key) lab.setParam(c.key, std::string(c.ori));
                for (int i = 0; i < 800; i++) lab.step(1.0 / 240);
                const Orbital& o = orbitals()[orbIndex(c.orb)];
                const long cnt = std::min<long>(lab.s.k, lab.cloudCap()); double sum = 0;
                for (long i = 0; i < cnt; i++) sum += std::hypot(double(lab.s.pts[3 * i]), double(lab.s.pts[3 * i + 1]), double(lab.s.pts[3 * i + 2]));
                worst = std::max(worst, std::fabs(sum / cnt / ((3.0 * o.n * o.n - o.l * (o.l + 1)) / (2.0 * Z) * nist::a0) - 1)); n++;
            }
    std::printf("sampled <r> for Z = 3 and 26 (%ld labs): worst difference from a0 (3n^2 - l(l+1)) / 2Z %.5f\n", n, worst);
    checkTrue("sampled <r>: for Z = 3 and 26 the dots' mean radius is within the guard of a0 (3n^2 - l(l+1)) / 2Z (guard = measured worst + 1 %)", worst <= guard);
}

// ================= 0.10.7 stage 1: the radial probability graph =================
// References, none of them from the lab's own radial(): (1) the textbook closed forms of the first hydrogen radial functions R10 .. R32 (Griffiths, Introduction to Quantum Mechanics, ch. 4, the table
// of the first few R_nl; a0 = 1), typed here and CHECKED below to be normalised and orthogonal, so a wrong coefficient shows; (2) composite Simpson quadrature; (3) the exact peaks: 1s at a0/Z and every
// l = n - 1 orbital at n^2 a0/Z (P is then r^(2n) e^(-2r/n a0 / Z), its maximum is at n^2 a0 / Z); (4) <r> = a0 (3n^2 - l(l+1)) / 2Z; (5) the closed-form share of the 1s probability inside q = Z r:
// 1 - e^(-2q)(1 + 2q + 2q^2) (by parts, d/dq of it is 4 q^2 e^(-2q)); (6) the dots themselves.
namespace rg {
static const char* kShells[10] = {"1s", "2s", "2p", "3s", "3p", "3d", "4s", "4p", "4d", "4f"};
static double Rref(int idx, double q) {      // idx: 0 1s, 1 2s, 2 2p, 3 3s, 4 3p, 5 3d  (a0 = 1)
    switch (idx) {
        case 0: return 2 * std::exp(-q);
        case 1: return (1 / std::sqrt(2.0)) * (1 - q / 2) * std::exp(-q / 2);
        case 2: return (1 / std::sqrt(24.0)) * q * std::exp(-q / 2);
        case 3: return (2 / std::sqrt(27.0)) * (1 - 2 * q / 3 + 2 * q * q / 27) * std::exp(-q / 3);
        case 4: return (8 / (27 * std::sqrt(6.0))) * (1 - q / 6) * q * std::exp(-q / 3);
        default: return (4 / (81 * std::sqrt(30.0))) * q * q * std::exp(-q / 3);
    }
}
template <class F> static double simpson(F f, double a, double b, int n) {   // n even
    const double h = (b - a) / n; double s = f(a) + f(b);
    for (int i = 1; i < n; i++) s += f(a + i * h) * (i % 2 ? 4 : 2);
    return s * h / 3;
}
static double meanFormula(const Orbital& o, int Z) { return (3.0 * o.n * o.n - o.l * (o.l + 1)) / (2.0 * Z); }
}  // namespace rg

static void testRadialGraph(double tolRef, double tolArea, double tolCurve, double tolCover, double tolMean, double tolPeak, double tolR90s) {
    // ---- the closed forms are themselves right: normalised, and R10, R20, R30 mutually orthogonal (same l) ----
    double worstN = 0, worstO = 0;
    for (int i = 0; i < 6; i++) worstN = std::max(worstN, std::fabs(rg::simpson([&](double q) { return q * q * rg::Rref(i, q) * rg::Rref(i, q); }, 0, 400, 40000) - 1));
    for (int a : {0, 1, 3}) for (int b : {0, 1, 3}) if (a < b) worstO = std::max(worstO, std::fabs(rg::simpson([&](double q) { return q * q * rg::Rref(a, q) * rg::Rref(b, q); }, 0, 400, 40000)));
    std::printf("radial graph: the typed textbook R10..R32: worst |norm - 1| %.4e, worst |overlap| %.4e\n", worstN, worstO);
    checkTrue("radial graph: the typed textbook radial functions are normalised and orthogonal (so the reference is right)", worstN <= 2.70e-9 && worstO <= 9.53e-10);   // guards = measured (2.6662e-9, 9.4268e-10: Simpson, h = 0.01) + 1 %

    // ---- the lab's P(r) against those closed forms (n <= 3), for Z = 1, 2, 26 ----
    double worstRef = 0;
    for (int i = 0; i < 6; i++)
        for (int Z : {1, 2, 26})
            for (int k = 1; k <= 60; k++) {
                const double r = k * 0.25 / Z, q = Z * r, want = Z * q * q * rg::Rref(i, q) * rg::Rref(i, q);   // P(r) dr = q^2 R(q)^2 dq, r = q / Z
                const double got = radialDensity(orbIndex(rg::kShells[i]), Z, r);
                if (want > 1e-12) worstRef = std::max(worstRef, std::fabs(got / want - 1));
            }
    std::printf("radial graph: P(r) against the textbook closed forms, 6 shells x Z 1, 2, 26: worst relative difference %.3e\n", worstRef);
    checkTrue("radial graph: P(r) = r^2 R^2 equals the textbook closed forms for 1s 2s 2p 3s 3p 3d at Z = 1, 2, 26 (guard from the measured value)", worstRef <= tolRef);

    // ---- area 1, <r>, peaks, number of maxima, for all ten shells at Z = 1, 2, 26 ----
    double worstArea = 0, worstMean = 0, worstPeak = 0, worstPeakIdx = 0; bool countsOk = true, gridCountOk = true, highest = true, minimaOk = true, markOk = true, diffOk = true;
    double worstCurve = 0, worstCover = 0;
    for (int s = 0; s < 10; s++)
        for (int Z : {1, 2, 26}) {
            const int st = orbIndex(rg::kShells[s]); const Orbital& o = orbitals()[st];
            const RadialGraph g = radialGraph(st, Z);
            const auto Pf = [&](double r) { return radialDensity(st, Z, r); };
            const double L = 60.0 * o.n * o.n / Z;
            worstArea = std::max(worstArea, std::fabs(rg::simpson(Pf, 0, L, 20000) - 1));
            worstMean = std::max(worstMean, std::fabs(rg::simpson([&](double r) { return r * Pf(r); }, 0, L, 20000) / rg::meanFormula(o, Z) - 1));
            diffOk = diffOk && g.mean == meanRadius(o.n, o.l, Z) && std::fabs(meanRadius(o.n, o.l, Z) / rg::meanFormula(o, Z) - 1) < 1e-14;
            // the stored curve: its own trapezoid area against Simpson of the exact function over the same range, and the share it covers
            double tr = 0; for (std::size_t i = 0; i + 1 < g.r.size(); i++) tr += 0.5 * (g.P[i] + g.P[i + 1]) * (g.r[i + 1] - g.r[i]);
            const double ex = rg::simpson(Pf, 0, g.rEnd, 20000);
            worstCurve = std::max(worstCurve, std::fabs(tr - ex)); worstCover = std::max(worstCover, std::fabs(ex - kRadialCover));
            // peaks
            if (o.n == 1) worstPeak = std::max(worstPeak, std::fabs(g.peak * Z - 1));
            if (o.l == o.n - 1) worstPeak = std::max(worstPeak, std::fabs(g.peak * Z / (double(o.n) * o.n) - 1));
            std::size_t am = 0; for (std::size_t i = 0; i < g.P.size(); i++) if (g.P[i] > g.P[am]) am = i;
            worstPeakIdx = std::max(worstPeakIdx, std::fabs(g.r[am] - g.peak) / (g.r[1] - g.r[0]));
            highest = highest && Pf(g.peak) >= g.P[am] * (1 - 1e-12) && g.P.size() == std::size_t(kRadialPoints + 1) && g.r.front() == 0;
            // maxima: n - l (exact roots), and n - l on the stored curve; the n - l - 1 nodes between them are zeros of P
            countsOk = countsOk && int(g.maxima.size()) == o.n - o.l;
            int gm = 0, gmin = 0;
            for (std::size_t i = 1; i + 1 < g.P.size(); i++) { if (g.P[i] > g.P[i - 1] && g.P[i] >= g.P[i + 1]) gm++; if (g.P[i] < g.P[i - 1] && g.P[i] <= g.P[i + 1]) { gmin++; minimaOk = minimaOk && g.P[i] < 0.02 * g.P[am]; } }
            gridCountOk = gridCountOk && gm == o.n - o.l && gmin == o.n - o.l - 1;
            // the 90 % mark is the state's own outline reach; at Z it is the Z = 1 one over Z
            for (int a = 0; a < int(orbitals().size()); a++) {
                if (orbitals()[a].shell != o.shell) continue;
                const RadialGraph ga = radialGraph(a, Z);
                markOk = markOk && std::fabs(ga.reach90 * Z / outlineOf(a).rmax - 1) < 1e-12 && ga.P == g.P && ga.maxima == g.maxima && ga.reach90 > 0;
            }
        }
    std::printf("radial graph, 10 shells x Z 1, 2, 26: worst |area - 1| %.4e; worst |<r>(quadrature) / formula - 1| %.4e; worst peak error vs exact %.3e (relative); highest point of the curve within %.2f grid steps of the peak;\n"
                "   stored curve (trapezoid, %d points) against Simpson of the exact P over the same range: worst difference %.4e; the share inside the right edge differs from %.3f by at most %.4e\n",
                worstArea, worstMean, worstPeak, worstPeakIdx, kRadialPoints + 1, worstCurve, kRadialCover, worstCover);
    checkTrue("radial graph: the area under P(r) is 1 for all ten shells at Z = 1, 2, 26 (Simpson, 20,000 intervals; guard = measured + 1 %)", worstArea <= tolArea);
    checkTrue("radial graph: the stored curve's area equals the exact P's over the same range (trapezoid, 401 points; guard = measured + 1 %)", worstCurve <= tolCurve);
    checkTrue("radial graph: the picture's right edge holds 99.9 % of the probability (guard = measured + 1 %)", worstCover <= tolCover);
    checkTrue("radial graph: <r> from the quadrature of r P(r) equals a0 (3n^2 - l(l+1)) / 2Z for all ten shells at Z = 1, 2, 26 (guard = measured + 1 %)", worstMean <= tolMean);
    checkTrue("radial graph: the graph's <r> is meanRadius() (the readout's number) and equals the formula to 1e-14", diffOk);
    checkTrue("radial graph: the most likely distance is exactly a0/Z for 1s and n^2 a0/Z for every l = n - 1 orbital (relative guard: the bisection's, above rounding)", worstPeak <= tolPeak);
    checkTrue("radial graph: the peak is the highest point of the 401-point curve (to a step) and the curve starts at r = 0", highest && worstPeakIdx <= 1.0);
    checkTrue("radial graph: P has n - l maxima (exact roots), the stored curve shows n - l maxima and n - l - 1 nodes where P falls to about 0", countsOk && gridCountOk && minimaOk);
    checkTrue("radial graph: the 90 % mark is the outline's own rmax / Z for every orientation, and the curve does not depend on the orientation", markOk);

    // ---- the 90 % mark against an independent number: 1s is a ball, its outline radius is the radius holding 90 %: bisection of the closed-form share ----
    double lo = 0, hi = 20; for (int i = 0; i < 100; i++) { const double q = 0.5 * (lo + hi); (1 - std::exp(-2 * q) * (1 + 2 * q + 2 * q * q) < 0.9 ? lo : hi) = q; }
    const double q90 = 0.5 * (lo + hi), got1 = radialGraph(orbIndex("1s"), 1).reach90, h = 2 * 1.15 * 4.5 / kOutlineGrid;
    std::printf("radial graph: 1s radius holding 90 %% (closed form) %.6f a0; the outline's reach %.6f a0 (grid step %.4f a0): difference %.5f a0 (%.5f of a step)\n", q90, got1, h, q90 - got1, (q90 - got1) / h);
    checkTrue("radial graph: for 1s the 90 % mark agrees with the closed-form 90 % radius to a small part of a grid step of the outline (guard = measured 0.05966 of a step + 1 %)", std::fabs(got1 - q90) <= tolR90s * h);
    checkNum("radial graph: the 90 % mark scales as 1/Z (1s, Z = 26)", radialGraph(orbIndex("1s"), 26).reach90 * 26, got1, 1e-12);

    // ---- complex states: the same curve as the shell ----
    bool sameM = true;
    for (const char* sh : {"2p", "3p", "3d", "4d", "4f"}) {
        const int base = orbIndex(sh), l = orbitals()[base].l;
        const RadialGraph g0 = radialGraph(base, 2);
        for (int m = -l; m <= l; m++) { if (!m) continue; const RadialGraph gm = radialGraph(complexState(base, m), 2); sameM = sameM && gm.P == g0.P && gm.peak == g0.peak && gm.mean == g0.mean && gm.reach90 > 0 && std::fabs(gm.reach90 * 2 / outlineOf(complexState(base, m)).rmax - 1) < 1e-12; }
    }
    checkTrue("radial graph: every m = +-1 .. +-l state of a shell gives the same curve, peak and <r>, and each one's 90 % mark is its own outline's rmax / Z (the radial part does not depend on m)", sameM);

    // ---- the tick step is a round number and gives a few ticks ----
    bool ticksOk = true;
    for (int s = 0; s < 10; s++) for (int Z : {1, 2, 26}) {
        const RadialGraph g = radialGraph(orbIndex(rg::kShells[s]), Z);
        const double st = radialTickStep(g.rEnd), e = std::pow(10.0, std::floor(std::log10(st))), m = st / e, nt = std::floor(g.rEnd / st + 1e-9);
        ticksOk = ticksOk && (std::fabs(m - 1) < 1e-9 || std::fabs(m - 2) < 1e-9 || std::fabs(m - 5) < 1e-9) && nt >= 3 && nt <= 8;
    }
    checkTrue("radial graph: the axis step is 1, 2 or 5 times a power of ten and gives 3 to 8 ticks", ticksOk);

    // ---- the words ----
    const std::string note = kRadialNote, axis = kRadialAxisNote, mk = radialMarkText("average distance", 1.5);
    checkTrue("radial graph: the note says what P(r) dr is, that the area is 1, that every orientation has the same curve, and the model's limits (one electron, non-relativistic, point nucleus)",
              note.find("between r and r + dr") != std::string::npos && note.find("area") != std::string::npos && note.find("orientation") != std::string::npos &&
              note.find("one electron") != std::string::npos && note.find("non-relativistic") != std::string::npos && note.find("point nucleus") != std::string::npos);
    checkTrue("radial graph: the axis note names both units, Bohr radii and Å, and says where the right edge is (99.9 %)", axis.find("a₀") != std::string::npos && axis.find("Å") != std::string::npos && axis.find("99.9 %") != std::string::npos);
    checkStr("radial graph: a mark's text gives the value in a0 and in Å (CODATA a0 = 0.529177 Å)", mk, "average distance 1.5 a₀ = " + fmtLen(1.5 * kA0Ang));

    // ---- when it is shown ----
    {
        AtomLab lab; lab.nowMs = [] { return 0.0; }; lab.p = AtomParams(); lab.restart();
        lab.setParam("view", std::string("Cloud"));
        const bool cloud = lab.radialMode() == RadialMode::Shown;
        lab.setParam("view", std::string("Orbit")); const bool orbit = lab.radialMode() == RadialMode::Hidden;
        lab.setParam("view", std::string("Slice")); const bool slice = lab.radialMode() == RadialMode::Shown;
        lab.setParam("view", std::string("Cloud 3D")); const bool c3 = lab.radialMode() == RadialMode::Shown;
        lab.setParam("view", std::string("Cloud")); lab.setParam("super", std::string("On")); const bool sup = lab.radialMode() == RadialMode::Superposition;
        lab.setParam("super", std::string("Off")); lab.setParam("orb", std::string("2p")); lab.emit(); for (int i = 0; i < 4 && lab.radialMode() != RadialMode::Jumping; i++) lab.step(1.0 / 60);
        const bool jump = lab.radialMode() == RadialMode::Jumping;
        checkTrue("radial graph: shown in Cloud, Cloud 3D and Slice; hidden in Orbit; replaced by a note in a superposition and during a photon jump", cloud && orbit && slice && c3 && sup && jump);
    }
}

// The dots: the distance distribution of the dots (12,000 of them) against the graph's own cumulative curve, as the Kolmogorov distance (largest gap between the two cumulative curves).
static void testRadialDots(double guard) {
    struct C { const char* orb; const char* key; const char* ori; int Z; } cs[] = {{"1s", "", "", 1}, {"2s", "", "", 1}, {"2p", "pAx", "z", 1}, {"2p", "pAx", "m+1", 1}, {"3s", "", "", 1}, {"3p", "pAx", "z", 1},
        {"3d", "dAx", "z2", 1}, {"3d", "dAx", "m+2", 1}, {"4s", "", "", 1}, {"4d", "dAx", "x2y2", 1}, {"4f", "fAx", "z3", 1}, {"4f", "fAx", "m+3", 1}, {"1s", "", "", 3}, {"3d", "dAx", "z2", 3}, {"4f", "fAx", "m+3", 26}};
    double worst = 0, sum = 0; long n = 0;
    for (const C& c : cs)
        for (int seed = 1; seed <= 4; seed++) {
            seedRandom(8000 + seed);
            AtomLab lab; lab.nowMs = [] { return 0.0; }; lab.p = AtomParams(); lab.restart();
            lab.setParam("Z", double(c.Z)); lab.setParam("view", std::string("Cloud")); lab.setParam("orb", std::string(c.orb)); if (*c.key) lab.setParam(c.key, std::string(c.ori));
            for (int i = 0; i < 800; i++) lab.step(1.0 / 240);
            const long cnt = std::min<long>(lab.s.k, lab.cloudCap());
            std::vector<double> rr(cnt); for (long i = 0; i < cnt; i++) rr[i] = std::hypot(double(lab.s.pts[3 * i]), double(lab.s.pts[3 * i + 1]), double(lab.s.pts[3 * i + 2])) / nist::a0;
            std::sort(rr.begin(), rr.end());
            const RadialGraph g = radialGraph(lab.stateId(), c.Z);
            double cdf = 0, d = 0; std::size_t j = 0;
            for (std::size_t i = 1; i < g.r.size(); i++) {
                cdf += 0.5 * (g.P[i - 1] + g.P[i]) * (g.r[i] - g.r[i - 1]);
                while (j < rr.size() && rr[j] <= g.r[i]) j++;
                d = std::max(d, std::fabs(double(j) / double(cnt) - cdf));
            }
            worst = std::max(worst, d); sum += d; n++;
        }
    std::printf("radial graph against the dots (%ld labs, 12,000 dots each): Kolmogorov distance mean %.4f, worst %.6f\n", n, sum / n, worst);
    checkTrue("radial graph: the distance distribution of the dots matches the graph's cumulative curve (Kolmogorov distance; guard = measured worst + 1 %)", worst <= guard);
}

// The readout's "theory" is the graph's <r>: the same number, to the digit shown, for every shell and several Z.
static void testRadialReadout() {
    bool same = true; int k = 0;
    for (const char* sh : rg::kShells)
        for (int Z : {1, 2, 26}) {
            AtomLab lab; lab.nowMs = [] { return 0.0; }; lab.p = AtomParams(); lab.restart();
            lab.setParam("Z", double(Z)); lab.setParam("view", std::string("Cloud")); lab.setParam("orb", std::string(sh));
            const RadialGraph g = radialGraph(lab.stateId(), Z);
            same = same && lab.info1().find("theory " + fmtLen(g.mean * kA0Ang)) != std::string::npos; k++;
        }
    checkTrue("radial graph: the readout's 'theory' avg r is the graph's <r>, for 10 shells x Z 1, 2, 26 (" + std::to_string(k) + " cases)", same);
}

// ================= 0.10.7 stage 2: the real spectrum (NIST ASD) next to the lab's =================
// The data was fetched on 2026-10-04 by tools/make_asd_data.py (the query is in data/asd/README.txt). The numbers typed below are read from data/asd/manifest.tsv and asd_lines.tsv (the files the fetch wrote, NOT
// from asd_data.inc), so the test checks that parsing, building and compiling kept what the fetch produced.
// The Dirac level formula (point nucleus, reduced mass): E(n, j) = mu c^2 [ (1 + (Z alpha)^2 / (n - d)^2)^(-1/2) - 1 ], d = (j + 1/2) - sqrt((j + 1/2)^2 - (Z alpha)^2). It is the standard textbook formula (Bethe &
// Salpeter; Griffiths ch. 6 gives its expansion), written from knowledge, not looked up here; the comparison with ASD below is its check.
// 0.10.8 stage 2: written WITHOUT subtracting nearly equal numbers (it was the plain textbook form, 1e-10 off at the worst; harmless for the guards below, but a reference should be accurate):
// d = (Z alpha)^2 / ((j + 1/2) + sqrt(...)) and (1 + t)^(-1/2) - 1 = -t / (s (1 + s)), s = sqrt(1 + t).
static double diracE(double mu, int Z, int n, double j) {      // eV, binding energy negative; mu in eV (m c^2 times the reduced-mass factor)
    const double za = Z * nist::alpha, d = za * za / ((j + 0.5) + std::sqrt((j + 0.5) * (j + 0.5) - za * za)), t = za * za / ((n - d) * (n - d)), sq = std::sqrt(1 + t);
    return -mu * t / (sq * (1 + sq));
}
static const AsdLine* asdFind(const AsdSpectrum& sp, const char* transitionStart) {
    for (int i = 0; i < sp.count; i++) if (std::string(sp.lines[i].transition).rfind(transitionStart, 0) == 0) return &sp.lines[i];
    return nullptr;
}

static void testAsdData() {
    // ---- the data parses and is what the fetch produced ----
    int total = 0, ionTotal = 0, atomTotal = 0, ionOk = 0, ionNone = 0, atomOk = 0, atomNone = 0, atomEmpty = 0;
    int flagsOnly = 0, ionObs = 0, ionRitzOnly = 0, atomObs = 0, ionResolved = 0, atomResolved = 0, ionIntens = 0, atomIntens = 0;
    bool positive = true, rangeOk = true, sorted = true, namesOk = true, unResolvedOk = true, uncOk = true, intensOk = true;
    double ionMin = 1e30, ionMax = 0, atomMin = 1e30, atomMax = 0;
    for (int Z = 1; Z <= 118; Z++)
        for (int kind = 0; kind < 2; kind++) {
            const AsdSpectrum& sp = kind == 0 ? asdIon(Z) : asdAtom(Z);
            (kind == 0 ? ionTotal : atomTotal) += sp.count; total += sp.count;
            if (kind == 0) { ionOk += sp.status == AsdStatus::Lines; ionNone += sp.status == AsdStatus::NoSpectrum; }
            else { atomOk += sp.status == AsdStatus::Lines; atomNone += sp.status == AsdStatus::NoSpectrum; atomEmpty += sp.status == AsdStatus::NoLines; }
            namesOk = namesOk && std::string(sp.name).rfind(std::string(symbol(Z)) + " ", 0) == 0 && (sp.status == AsdStatus::Lines) == (sp.count > 0);
            for (int i = 0; i < sp.count; i++) {
                const AsdLine& l = sp.lines[i];
                positive = positive && l.nm > 0 && l.ritz != 0;
                if (kind == 0) { rangeOk = rangeOk && l.nm >= kAsdIonLowNm && l.nm <= kAsdIonHighNm; ionMin = std::min(ionMin, l.nm); ionMax = std::max(ionMax, l.nm); ionObs += l.observed; ionRitzOnly += !l.observed; ionResolved += l.resolved; ionIntens += !std::isnan(asdIntensity(l)); }
                else { rangeOk = rangeOk && l.nm >= kAsdAtomLowNm && l.nm <= kAsdAtomHighNm && l.observed; atomMin = std::min(atomMin, l.nm); atomMax = std::max(atomMax, l.nm); atomObs += l.observed; atomResolved += l.resolved; atomIntens += !std::isnan(asdIntensity(l)); }
                if (i > 0) sorted = sorted && (kind == 1 ? sp.lines[i - 1].nm <= l.nm : true);
                unResolvedOk = unResolvedOk && (l.resolved == 0 || l.resolved == 1) && l.ni >= 0 && l.nk >= 0 && (std::string(l.transition).find(" - ") != std::string::npos || std::string(l.transition) == "-");
                uncOk = uncOk && (std::isnan(l.unc) || l.unc >= 0);
                if (!std::string(l.intens).empty() && std::isnan(asdIntensity(l))) flagsOnly++;
                intensOk = intensOk && (std::string(l.intens).empty() ? std::isnan(asdIntensity(l)) : true) && (std::isnan(asdIntensity(l)) || asdIntensity(l) > 0) && (kind == 0 || !std::isnan(asdIntensity(l)));
            }
        }
    std::printf("ASD data: %d lines kept (ion %d, atom %d); ions with lines %d, not in ASD %d; atoms with lines %d, not in ASD %d, none in range %d; wavelengths ion %.7g..%.7g nm, atom %.7g..%.7g nm\n",
                total, ionTotal, atomTotal, ionOk, ionNone, atomOk, atomNone, atomEmpty, ionMin, ionMax, atomMin, atomMax);
    checkTrue("ASD data: the totals are what the fetch produced (7,915 lines: 5,029 of the ions and 2,886 of the atoms; asdTotalLines agrees)", total == 7915 && ionTotal == 5029 && atomTotal == 2886 && asdTotalLines() == 7915);
    checkTrue("ASD data: 110 ions and 98 atoms have lines; 8 ions (Z 111-118) and 19 atoms (Z 100-118 except Fr) are not in ASD; Fr I has no line in the range", ionOk == 110 && ionNone == 8 && atomOk == 98 && atomNone == 19 && atomEmpty == 1);
    checkTrue("ASD data: counts per spectrum as fetched: H I ion 169 of 441, He II 140, Fe XXVI 95, U XCII 13, Fm C 13, H I atom 30 of 262, Fe I atom 30 of 9,548, Ne I atom 30 of 1,264",
              asdIon(1).count == 169 && asdIon(1).returned == 441 && asdIon(2).count == 140 && asdIon(26).count == 95 && asdIon(92).count == 13 && asdIon(100).count == 13 &&
              asdAtom(1).count == 30 && asdAtom(1).returned == 262 && asdAtom(26).count == 30 && asdAtom(26).returned == 9548 && asdAtom(10).count == 30 && asdAtom(10).returned == 1264);
    checkTrue("ASD data: every wavelength is positive, in the range asked (ion 1e-4 to 1e4 nm, atom 100 to 2500 nm), the names start with the element's symbol, status matches the line count", positive && rangeOk && namesOk);
    checkNum("ASD data: shortest and longest ion line are the fetched ones (0.0075649 and 9569.4 nm)", ionMin, 0.0075649, 1e-12); checkNum("ASD data: longest ion line", ionMax, 9569.4, 1e-12);
    checkNum("ASD data: shortest atom line (102.5728 nm)", atomMin, 102.5728, 1e-12); checkNum("ASD data: longest atom line (2486.733 nm)", atomMax, 2486.733, 1e-12);
    checkTrue("ASD data: observed and Ritz flags survive: ion lines 210 observed and 4,819 Ritz only; every atom line (2,886) is observed", ionObs == 210 && ionRitzOnly == 4819 && atomObs == 2886);
    checkTrue("ASD data: resolved components (both J given): ion 5,014 of 5,029, atom 2,164 of 2,886; numeric intensities: ion 107, atom all 2,886; 17 ion lines have only ASD flags in the intensity cell (no number: drawn without brightness)", ionResolved == 5014 && atomResolved == 2164 && ionIntens == 107 && atomIntens == 2886 && flagsOnly == 17);
    checkTrue("ASD data: atom lines are sorted by wavelength, uncertainties are NaN or not negative, a transition reads 'lower - upper' (or just '-' when ASD classifies none), an empty intensity is NaN, a given number is positive", sorted && unResolvedOk && uncOk && intensOk);
    // spot values typed from asd_lines.tsv
    const AsdLine *h1 = asdFind(asdIon(1), "1s 2S 1/2 - 2p 2P* 3/2"), *h2 = asdFind(asdIon(1), "1s 2S 1/2 - 2p 2P* 1/2"), *he = asdFind(asdIon(2), "1s 2S 1/2 - 2p 2P* 3/2"), *fe = asdFind(asdIon(26), "1s 2S 1/2 - 2p 2P* 3/2");
    checkTrue("ASD data: H I Lyman-alpha is there with ASD's observed 121.56699 nm +- 0.00020 and Ritz 121.5668237310 / 121.5673644608 nm", h1 && h2 && h1->observed == 1 && std::fabs(h1->nm - 121.56699) < 1e-9 && std::fabs(h1->unc - 0.00020) < 1e-12 && std::fabs(h1->ritz - 121.5668237310) < 1e-9 && std::fabs(h2->ritz - 121.5673644608) < 1e-9);
    checkTrue("ASD data: He II 1s-2p Ritz 30.3780409484 nm (also the stage 6 value), and it is a Ritz-only line; Fe XXVI 1s-2p3/2 0.177801471 nm Ritz-only with 1e-8 nm uncertainty",
              he && !he->observed && std::fabs(he->nm - 30.3780409484) < 1e-9 && fe && !fe->observed && std::fabs(fe->nm - 0.177801471) < 1e-12 && std::fabs(fe->unc - 1e-8) < 1e-12);
    checkTrue("ASD data: a Ritz-only ion line carries no intensity text (nothing invented), and the 1s - 6 H I line carries ASD's 19000", std::string(he->intens).empty() && std::isnan(asdIntensity(*he)));
    bool h6 = false; for (int i = 0; i < asdIon(1).count; i++) { const AsdLine& l = asdIon(1).lines[i]; if (std::string(l.transition) == "1s 2S 1/2 - 6" && asdIntensity(l) == 19000 && !l.resolved) h6 = true; }
    checkTrue("ASD data: the unresolved H I line '1s 2S 1/2 - 6' keeps ASD's intensity 19000 and is marked not resolved", h6);

    // ---- (a) the lab against the measured ion: fine structure and relativity are what is left ----
    // Tolerance: the lab is level-averaged and non-relativistic, so its line can differ from the components by the relativistic corrections, of the order (Z alpha)^2 of the level energies (5.3e-5 for hydrogen, 2.1e-4 for
    // He+): the lab's value must lie in the components' range widened by (Z alpha)^2 of the wavelength (the same justification as stage 6, now for every transition between n <= 6, not five lines).
    double worstH = 0, worstHe = 0; bool allIn = true; int cmpN = 0;
    for (int Z : {1, 2}) {
        const auto cmp = compareIon(Z, defaultN(Z));
        cmpN += int(cmp.size());
        for (const IonCompare& c : cmp) {
            const double rel = std::fabs(c.offsetNm) / c.labNm, tol = std::pow(Z * nist::alpha, 2);
            (Z == 1 ? worstH : worstHe) = std::max(Z == 1 ? worstH : worstHe, rel / tol);
            allIn = allIn && rel <= tol && c.components >= 1 && c.minNm <= c.maxNm && c.lo < c.hi && c.hi <= 6;
        }
    }
    std::printf("ASD ion vs lab: H I and He II, %d transitions between n <= 6; the lab lies outside the components by at most %.3f (H I) and %.3f (He II) of the allowed (Z alpha)^2 of the wavelength\n", cmpN, worstH, worstHe);
    checkTrue("ASD ion vs lab: for all 15 transitions among n <= 6 of H I and of He II (30 compared) the lab's wavelength lies inside ASD's components widened by (Z alpha)^2 of the wavelength", cmpN == 30 && allIn);
    // the heavy ions: the lab's line is LONGER than the measured one, by about the Dirac correction
    {
        const int Z = 26; const auto cmp = compareIon(Z, defaultN(Z));
        const double M = nuclearMassMe(Z, defaultN(Z)), mu = nist::mecEV * M / (M + 1);
        const AsdLine *a3 = asdFind(asdIon(Z), "1s 2S 1/2 - 2p 2P* 3/2"), *a1 = asdFind(asdIon(Z), "1s 2S 1/2 - 2p 2P* 1/2");
        double lab = 0; for (const IonCompare& c : cmp) if (c.lo == 1 && c.hi == 2) lab = c.labNm;
        const double d3 = nist::hcEVnm / (diracE(mu, Z, 2, 1.5) - diracE(mu, Z, 1, 0.5)), d1 = nist::hcEVnm / (diracE(mu, Z, 2, 0.5) - diracE(mu, Z, 1, 0.5));
        const double asd3 = a3->ritz, asd1 = a1->ritz;
        std::printf("Fe XXVI Lyman-alpha: lab %.9f nm; Dirac 2p3/2 %.9f, 2p1/2 %.9f; ASD Ritz 2p3/2 %.9f, 2p1/2 %.9f nm. lab - ASD: %.4e (2p3/2), %.4e (2p1/2) of the wavelength; lab - Dirac: %.4e, %.4e; ASD - Dirac: %.3e, %.3e\n",
                    lab, d3, d1, asd3, asd1, (lab - asd3) / lab, (lab - asd1) / lab, (lab - d3) / lab, (lab - d1) / lab, (asd3 - d3) / lab, (asd1 - d1) / lab);
        checkTrue("ASD ion vs lab, Fe XXVI Lyman-alpha: the lab's non-relativistic line is longer than both measured components (the relativistic correction shortens it)", lab > asd3 && lab > asd1);
        double offL = 0, mxL = 0, mnL = 0; for (const IonCompare& c : cmp) if (c.lo == 1 && c.hi == 2) { offL = c.offsetNm; mxL = c.maxNm; mnL = c.minNm; }
        checkTrue("ASD ion vs lab, Fe XXVI Lyman-alpha: compareIon reports the lab as LONGER (positive offset = lab minus the longest component) and the components are the two ASD Ritz values",
                  offL > 0 && std::fabs(offL - (lab - mxL)) < 1e-12 && std::fabs(mxL - asd1) < 1e-12 && std::fabs(mnL - asd3) < 1e-12);
        const double rel3 = (lab - asd3) / (lab - d3), rel1 = (lab - asd1) / (lab - d1);
        // ASD lies between the lab and Dirac: the Dirac formula has no QED (Lamb shift) and no finite nuclear size, which lift the 1s level and so move the line back towards the lab's. Measured share of the lab's
        // error that Dirac accounts for: 0.9508 (2p3/2) and 0.9318 (2p1/2); the guard is the larger remainder 0.0682 + 1 %.
        checkTrue("ASD ion vs lab, Fe XXVI Lyman-alpha: (lab - ASD) / (lab - Dirac) is within 0.0689 of 1 for both components (measured remainder 0.0682, + 1 %); the rest is what neither the lab nor Dirac has (QED, nuclear size)", std::fabs(rel3 - 1) <= 0.0689 && std::fabs(rel1 - 1) <= 0.0689 && rel3 < 1 && rel1 < 1);
        // every ion ASD has (Z = 1 .. 100 with the 1s-2p3/2 line): the lab's line is longer than ASD's, ASD's is longer than Dirac's, and Dirac accounts for 80 % to 100 % of the gap
        double lo = 1e9, hi = -1e9; bool order = true; int nZ = 0;
        for (int z = 1; z <= 118; z++) {
            const AsdLine* l3 = asdFind(asdIon(z), "1s 2S 1/2 - 2p 2P* 3/2"); if (!l3 || std::isnan(l3->ritz)) continue;
            double lb = 0; for (const IonCompare& c : compareIon(z, defaultN(z))) if (c.lo == 1 && c.hi == 2) lb = c.labNm;
            const double M2 = nuclearMassMe(z, defaultN(z)), mu2 = nist::mecEV * M2 / (M2 + 1), dz = nist::hcEVnm / (diracE(mu2, z, 2, 1.5) - diracE(mu2, z, 1, 0.5));
            const double r = (lb - l3->ritz) / (lb - dz); lo = std::min(lo, r); hi = std::max(hi, r); order = order && lb > l3->ritz && l3->ritz > dz; nZ++;
        }
        std::printf("Lyman-alpha 1s-2p3/2, %d ions: (lab - ASD) / (lab - Dirac) from %.4f to %.4f\n", nZ, lo, hi);
        // ---- the words: (a) validates, (b) teaches the limit, and the missing data are said ----
        auto has = [](const std::string& s, const char* w) { return s.find(w) != std::string::npos; };
        const std::string ta = asdIonTitle(26), na = asdIonNote(26), tb = asdAtomTitle(26), nb = asdAtomNote(26), nb1 = asdAtomNote(1), mOg = asdMissingText(asdIon(118)), mFr = asdMissingText(asdAtom(87)), mOk = asdMissingText(asdIon(26));
        checkTrue("ASD words: (a) names the ion the lab models (Fe XXVI), says one electron, non-relativistic, point nucleus, fine structure and relativity are left out, and that it checks the lab",
                  has(ta, "(a)") && has(ta, "ion the lab models") && has(ta, "Fe XXVI") && has(na, "One electron") && has(na, "non-relativistic") && has(na, "point nucleus") && has(na, "fine structure") && has(na, "relativity") && has(na, "checks the lab"));
        checkTrue("ASD words: (b) names the neutral atom (Fe I), says it has 26 electrons while the lab has ONE, that the lines are not expected to match and that it shows the limit of the lab",
                  has(tb, "(b)") && has(tb, "neutral atom") && has(tb, "Fe I") && has(nb, "26 electrons") && has(nb, "ONE electron") && has(nb, "not expected to match") && has(nb, "limit of the lab"));
        checkTrue("ASD words: for hydrogen (b) says the neutral atom is the same system as (a)", has(nb1, "same system as (a)"));
        checkTrue("ASD words: no data is said on screen: Og CXVIII 'has no data for Og CXVIII', Fr I 'but no line in the range', a spectrum with lines gives no message",
                  has(mOg, "has no data for Og CXVIII") && has(mFr, "Fr I") && has(mFr, "no line in the range") && mOk.empty());
        const std::string mn = kAsdMeasuredNote, cr = kAsdCredit;
        checkTrue("ASD words: the note says measured or Ritz with uncertainties, vacuum wavelengths, qualitative relative intensities that mean something only within one spectrum and depend on the source, no brightness meant where there is none, and that only a selection is stored",
                  has(mn, "measured") && has(mn, "Ritz") && has(mn, "uncertainty") && has(mn, "vacuum") && has(mn, "qualitative") && has(mn, "within one spectrum") && has(mn, "light source") && has(mn, "no brightness") && has(mn, "selection"));
        checkTrue("ASD words: the credit follows NIST's request (authors, title, version 5.12, 2024, https://physics.nist.gov/asd, DOI 10.18434/T4W30F) and gives the fetch date",
                  has(cr, "Kramida, A., Ralchenko, Yu., Reader, J. and NIST ASD Team (2024)") && has(cr, "version 5.12") && has(cr, "https://physics.nist.gov/asd") && has(cr, "10.18434/T4W30F") && has(cr, "2026-10-04"));
        checkTrue("ASD words: the selections are stated (ion: n <= 6; atom: the 30 strongest observed lines with an intensity in 100-2500 nm)", has(kAsdSelectIon, "n <= 6") && has(kAsdSelectAtom, "30 strongest") && has(kAsdSelectAtom, "100-2500 nm"));
        checkTrue("ASD ion vs lab: for every ion ASD has (Lyman-alpha 1s-2p3/2) the order is lab > ASD > Dirac in wavelength, and Dirac accounts for between 0.793 and 1 of the lab's error (measured 0.8017 for H to 0.9780; lowest - 1 %)", nZ == 110 && order && lo >= 0.793 && hi < 1);
    }
}

// ================= 0.10.8 stage 1: relativity as an option (Dirac levels) =================
// References that do NOT come from the lab's own functions:
//  (1) the CODATA 2022 constants typed in namespace nist above (alpha, m_e c^2, the Rydberg energy, h c);
//  (2) the textbook arrangement of the Sommerfeld / Dirac formula, E = m c^2 [ (1 + (Z alpha / (n - (j + 1/2) + sqrt((j + 1/2)^2 - (Z alpha)^2)))^2)^(-1/2) - 1 ], worked out here in DOUBLE-DOUBLE arithmetic (about 32
//      digits, built below from error-free transformations and std::fma): the plain subtractions of the textbook form are then harmless (they cost at most 11 digits of 32). The lab uses cancellation-free
//      rearrangements instead (dirac.cpp). Same formula, so (1) and (2) check the arithmetic, not the physics. 0.10.8 stage 2: this block was first written with long double, which is 80 bits on Linux and only 64 on MSVC,
//      so on Windows the reference was the inaccurate side and six checks failed. **DO NOT USE long double (or any extended precision) IN A TEST REFERENCE: it is not portable.** The 80-digit values typed below
//      (Python decimal) pin the double-double code itself;
//  (3) its expansion to the order (Z alpha)^4 (Bethe & Salpeter; Griffiths ch. 6): E = -(m c^2 (Z alpha)^2 / 2 n^2) [1 + ((Z alpha)^2 / n^2)(n / (j + 1/2) - 3/4)]: a different formula, whose miss must scale as (Z alpha)^6;
//  (4) NIST ASD (data/asd, 0.10.7 stage 2): MEASURED wavelengths, matched to the lab's lines by the n and j of both levels, read from ASD's own transition text.
// Both formulas are the standard textbook ones, written from knowledge, not looked up in this session (only physics.nist.gov was reachable); (4) is what checks them against nature.
// ---- double-double arithmetic (a value is hi + lo, |lo| <= half an ulp of hi): the portable high-precision reference. Only +, -, *, / and sqrt of doubles and std::fma, which are exactly rounded on every IEEE
// platform; no extended precision. (Knuth's two-sum; the product's error from fma; Newton steps for / and sqrt.)
struct DD { double hi, lo; };
static DD ddNorm(double a, double b) { const double s = a + b; return {s, b - (s - a)}; }                       // |a| >= |b|
static DD ddTwoSum(double a, double b) { const double s = a + b, v = s - a; return {s, (a - (s - v)) + (b - v)}; }
static DD ddTwoProd(double a, double b) { const double p = a * b; return {p, std::fma(a, b, -p)}; }
static DD ddAdd(DD a, DD b) { DD s = ddTwoSum(a.hi, b.hi); const DD t = ddTwoSum(a.lo, b.lo); s.lo += t.hi; s = ddNorm(s.hi, s.lo); s.lo += t.lo; return ddNorm(s.hi, s.lo); }
static DD ddNeg(DD a) { return {-a.hi, -a.lo}; }
static DD ddSub(DD a, DD b) { return ddAdd(a, ddNeg(b)); }
static DD ddMul(DD a, DD b) { DD p = ddTwoProd(a.hi, b.hi); p.lo += a.hi * b.lo + a.lo * b.hi; return ddNorm(p.hi, p.lo); }
static DD ddDiv(DD a, DD b) {
    const double q1 = a.hi / b.hi; DD r = ddSub(a, ddMul(b, DD{q1, 0}));
    const double q2 = r.hi / b.hi; r = ddSub(r, ddMul(b, DD{q2, 0}));
    const double q3 = r.hi / b.hi;
    return ddAdd(ddNorm(q1, q2), DD{q3, 0});
}
static DD ddSqrt(DD a) { const double s = std::sqrt(a.hi); const DD r = ddSub(a, ddTwoProd(s, s)); return ddAdd(DD{s, 0}, DD{r.hi / (2 * s), 0}); }
static double ddRel(DD got, DD want) { return std::fabs((got.hi - want.hi) + (got.lo - want.lo)) / std::fabs(want.hi); }
// the textbook formula with plain subtractions, in double-double, from the same double x = Z * alpha the lab forms:
//   delta = (j + 1/2) - sqrt((j + 1/2)^2 - x^2), k = n - delta, E = m c^2 ((1 + x^2 / k^2)^(-1/2) - 1), correction c = E + m c^2 x^2 / (2 n^2)
struct DiracDD { DD E, c, delta; };
static DiracDD ddDirac(int Z, int n, int twoJ) {
    const double x = Z * nist::alpha, jh = (twoJ + 1) / 2.0;
    const DD x2 = ddTwoProd(x, x), delta = ddSub(DD{jh, 0}, ddSqrt(ddSub(DD{jh * jh, 0}, x2))), k = ddSub(DD{double(n), 0}, delta);
    const DD f = ddDiv(DD{1, 0}, ddSqrt(ddAdd(DD{1, 0}, ddDiv(x2, ddMul(k, k)))));
    const DD E = ddMul(DD{nist::mecEV, 0}, ddSub(f, DD{1, 0}));
    const DD c = ddAdd(E, ddDiv(ddMul(DD{nist::mecEV, 0}, x2), DD{2.0 * n * n, 0}));
    return {E, c, delta};
}
static double refDiracInfEV(int Z, int n, int twoJ) { return ddDirac(Z, n, twoJ).E.hi; }
static double refCorrEV(int Z, int n, int twoJ) { return ddDirac(Z, n, twoJ).c.hi; }
// typed 80-digit values (Python decimal, 80 digits, from the same double Z alpha and m c^2), as {hi, lo}: Z, n, 2j, E (eV), the correction c (eV), delta
struct DiracTyped { int Z, n, twoJ; DD E, c, delta; };
static const DiracTyped kDiracTyped[] = {
    {1, 1, 1, {-13.605874258016533, -5.969003669744587e-16}, {-0.00018113521962715416, -3.783553996857771e-21}, {2.6626031696629847e-05, 2.678459506123533e-22}},
    {1, 2, 1, {-3.4014798855307187, 4.520228029309583e-17}, {-5.660483149211426e-05, 1.871567248221069e-21}, {2.6626031696629847e-05, 2.678459506123533e-22}},
    {1, 2, 3, {-3.401434601424379, -1.6954613731282566e-17}, {-1.1320725152316283e-05, 2.5521239038358817e-22}, {1.3312882920136858e-05, 8.51972088924689e-24}},
    {1, 3, 5, {-1.5117459164951206, 6.853153541232245e-17}, {-2.2361843531711144e-06, -1.0230414748475084e-22}, {8.875238869593463e-06, -1.719576036269966e-22}},
    {1, 6, 11, {-0.3779360598389038, 2.2515491269053562e-17}, {-1.3976121196891403e-07, 5.097490222003399e-24}, {4.437614511676528e-06, 2.0789311433837705e-22}},
    {92, 1, 1, {-132279.93376052065, -6.296308638660601e-12}, {-17121.34716916764, -1.763957495721877e-12}, {0.2588653725842363, 9.632853740197007e-18}},
    {92, 2, 1, {-34215.48418576323, 1.7159236195463466e-12}, {-5425.837537924971, 1.20527299962243e-13}, {0.2588653725842363, 9.632853740197007e-18}},
    {92, 2, 3, {-29649.837103282563, -1.3856411128590466e-12}, {-860.1904554443101, -2.5179651681133387e-14}, {0.11604656746651436, -4.190429857505444e-18}},
    {92, 3, 5, {-12959.73819605728, -4.92793156578785e-14}, {-164.33968590694485, -4.320951067065598e-16}, {0.07608472490143743, -4.275308759882891e-18}},
    {92, 6, 11, {-3208.9251872264167, 1.1499724852477167e-14}, {-10.075559688832845, 6.188910780668959e-16}, {0.03767825960769684, 2.7536361417673256e-18}}
};
static double refSeriesEV(int Z, int n, int twoJ) {                 // the (Z alpha)^4 expansion
    const double x = Z * nist::alpha, jh = (twoJ + 1) / 2.0;
    return -(nist::mecEV * x * x / (2.0 * n * n)) * (1 + x * x / (double(n) * n) * (n / jh - 0.75));
}
// the lab's hybrid level written again by hand: reduced-mass main term (the lab's nuclear mass, typed again above) + the infinite-mass relativistic correction
static double refHybridEV(int Z, int N, int n, int twoJ) {
    const double M = nuclearMassNew(Z, N), ry = nist::rydEV * M / (M + 1);
    return -ry * Z * Z / (double(n) * n) + refCorrEV(Z, n, twoJ);
}
// the lines of ASD for ion Z that are electric-dipole (n changes, l changes by 1), as {nLo, nHi, 2jLo, 2jHi} -> Ritz wavelengths
struct AsdKey { int a, b, c, d; bool operator<(const AsdKey& o) const { return std::array<int, 4>{a, b, c, d} < std::array<int, 4>{o.a, o.b, o.c, o.d}; } };
static std::map<AsdKey, std::vector<double>> asdE1(int Z) {
    std::map<AsdKey, std::vector<double>> out;
    const AsdSpectrum& sp = asdIon(Z);
    for (int i = 0; i < sp.count; i++) {
        AsdTransition t;
        if (!asdParseTransition(sp.lines[i].transition, t) || t.nLo >= t.nHi || std::abs(t.lLo - t.lHi) != 1 || !sp.lines[i].resolved) continue;
        out[{t.nLo, t.nHi, t.jLo2, t.jHi2}].push_back(std::isnan(sp.lines[i].ritz) ? sp.lines[i].nm : sp.lines[i].ritz);   // ASD's Ritz wavelength, else its observed one (as compareIon)
    }
    return out;
}
// the hand-made wavelength of one line, nm: with the Dirac levels (hybrid) or without
static double refLam(int Z, int N, int lo, int jl, int hi, int jh, bool dirac) {
    return nist::hcEVnm / ((dirac ? refHybridEV(Z, N, hi, jh) - refHybridEV(Z, N, lo, jl) : -(nist::rydEV * nuclearMassNew(Z, N) / (nuclearMassNew(Z, N) + 1)) * Z * Z * (1.0 / (double(hi) * hi) - 1.0 / (double(lo) * lo))));
}
// the worst relative miss of the lab's lines against ASD's E1 components, matched by (n, j) of both levels; positive = the lab's line is longer
struct Miss { double worst = 0; int comps = 0, unmatched = 0; };
static Miss worstMiss(int Z, int N, bool dirac) {
    Miss m;
    for (const auto& kv : asdE1(Z))
        for (double w : kv.second) {
            const AsdKey& k = kv.first;
            if (k.b > 6) continue;
            const double lam = refLam(Z, N, k.a, k.c, k.b, k.d, dirac), r = (lam - w) / w;
            if (std::fabs(r) > std::fabs(m.worst)) m.worst = r;
            m.comps++;
        }
    return m;
}

static void testDirac() {
    // Guards = the worst MEASURED value + 1 % (Linux GCC Release; improvement factors: measured - 1 %), from the first run of these tests:
    //  scaling of Dirac - non-relativistic with (Z alpha)^2: 0.50676 -> 0.5118 (the next-order coefficient is 1/2); log-log slope off 2 by at most 1.5412e-2 -> 1.557e-2; miss of the (Z alpha)^4 expansion 0.00782 -> 0.00790;
    //  ASD, worst relative miss of any n <= 6 component, with Dirac / without, and the improvement factor: H I 3.3169e-6 -> 3.351e-6 / 2.6431e-5 -> 2.670e-5, x7.969 -> 7.88; He II 1.0931e-5 -> 1.104e-5 / 1.0573e-4 -> 1.068e-4,
    //  x9.672 -> 9.57; Fe XXVI 5.7231e-4 -> 5.781e-4 / 1.8239e-2 -> 1.843e-2, x31.87 -> 31.5; what is left, Lyman-alpha of 110 ions, 220 components: (ASD - Dirac) / (non-relativistic - Dirac) at most 0.27164 -> 0.2744.
    constexpr double GUARD_ROUND_E = 2e-15, GUARD_ROUND_HYB = 1e-15;   // rounding level against the double-double reference: measured worst 6.66e-16 (3 units of the last digit) and 0
    constexpr double GUARD_RATIO = 0.5118, GUARD_SLOPE = 1.557e-2, GUARD_SERIES = 0.00790, GUARD_LEFT = 0.2744;
    constexpr double GUARD_H_ON = 3.351e-6, GUARD_H_OFF = 2.670e-5, GUARD_H_RATIO = 7.88, GUARD_HE_ON = 1.104e-5, GUARD_HE_OFF = 1.068e-4, GUARD_HE_RATIO = 9.57, GUARD_FE_ON = 5.781e-4, GUARD_FE_OFF = 1.843e-2, GUARD_FE_RATIO = 31.5;
    const double a = nist::alpha, mc = nist::mecEV;
    // ---- (1) the level against hand-made values ----
    {   // hydrogen 1s, infinite mass: j = 1/2, k = sqrt(1 - alpha^2), so E = m c^2 (sqrt(1 - alpha^2) - 1) = - m c^2 alpha^2 / (1 + sqrt(1 - alpha^2)) in closed form (the second form has no subtraction;
        // the first, in plain double, is off by 2.5e-12: it subtracts 1 from 0.99997338)
        const double want = -nist::mecEV * a * a / (1 + std::sqrt(1 - a * a));
        checkNum("Dirac H 1s, infinite mass, = - m c^2 alpha^2 / (1 + sqrt(1 - alpha^2)) worked out here from CODATA (the cancellation-free form of m c^2 (sqrt(1 - alpha^2) - 1))", diracInfiniteEV(1, 1, 1), want, 1e-15);
        std::printf("Dirac H 1s infinite mass: %.12f eV (the non-relativistic %.12f eV)\n", diracInfiniteEV(1, 1, 1), -0.5 * mc * a * a);
        const double x26 = 26 * a, wantFe = -nist::mecEV * x26 * x26 / (1 + std::sqrt(1 - x26 * x26));
        checkNum("Dirac Fe XXVI 1s, infinite mass, = - m c^2 (26 alpha)^2 / (1 + sqrt(1 - (26 alpha)^2)) worked out here", diracInfiniteEV(26, 1, 1), wantFe, 1e-15);
        std::printf("Dirac Fe 1s infinite mass: %.6f eV (non-relativistic %.6f eV)\n", diracInfiniteEV(26, 1, 1), -0.5 * mc * 26 * a * 26 * a);
    }
    double worstFormula = 0; bool allFinite = true;
    for (int Z : {1, 2, 6, 26, 54, 92, 118})
        for (int n = 1; n <= 6; n++)
            for (int j2 = 1; j2 <= 2 * n - 1; j2 += 2) {
                const double got = diracInfiniteEV(Z, n, j2), want = refDiracInfEV(Z, n, j2);
                worstFormula = std::max(worstFormula, std::fabs(got - want) / std::fabs(want)); allFinite = allFinite && std::isfinite(got) && got < 0;
            }
    std::printf("Dirac level, infinite mass, vs the textbook arrangement in double-double (Z up to 118, n <= 6, every j): worst relative difference %.3e\n", worstFormula);
    checkTrue("Dirac level: every level is finite and bound for Z = 1 .. 118 (Z alpha < 1)", allFinite);
    checkTrue("Dirac level agrees with the textbook arrangement of the formula (double-double) to 2e-15 for Z = 1, 2, 6, 26, 54, 92, 118, n <= 6, every j (rounding level: 2.3 units of the last digit; the measured worst is printed above)", worstFormula < GUARD_ROUND_E);
    // ---- the lab's level: reduced-mass main term + the infinite-mass relativistic correction ----
    double worstHyb = 0;
    for (int Z : {1, 2, 26}) for (int n = 1; n <= 4; n++) for (int j2 = 1; j2 <= 2 * n - 1; j2 += 2) worstHyb = std::max(worstHyb, std::fabs(diracEV(Z, defaultN(Z), n, j2) - refHybridEV(Z, defaultN(Z), n, j2)) / std::fabs(refHybridEV(Z, defaultN(Z), n, j2)));
    checkTrue("Dirac level of the lab = the hand-made hybrid (reduced-mass term from the NIST masses + the infinite-mass Dirac correction) to the rounding level (H, He+, Fe, n <= 4)", worstHyb < GUARD_ROUND_HYB);
    std::printf("Dirac level of the lab vs the hand-made hybrid: worst relative difference %.3e\n", worstHyb);
    // ---- the reduced mass: what the hybrid does and how far it is from the plain m -> mu substitution (itself only a leading-order recoil term, not an exact one) ----
    {
        const int Z = 1, N = 0; const double M = nuclearMassNew(Z, N), mu = M / (M + 1);
        const double corr = refCorrEV(1, 2, 3) - refCorrEV(1, 1, 1);   // the infinite-mass relativistic correction to the Lyman-alpha (2p3/2) photon energy
        const double hyb = diracEV(Z, N, 2, 3) - diracEV(Z, N, 1, 1);                                                      // the lab's photon energy
        const double sub = mu * (nist::rydEV * 0.75 + corr);                                                               // m -> mu in the whole formula: the correction is scaled by mu too
        const double E = nist::hcEVnm / refLam(Z, N, 1, 1, 2, 3, true);
        std::printf("Reduced mass, H Lyman-alpha 2p3/2: photon %.9f eV; hybrid minus (m -> mu everywhere) %.3e eV = %.3e of the photon energy; the whole relativistic correction is %.3e eV = %.3e of it\n", E, hyb - sub, (hyb - sub) / E, corr, corr / E);
        checkNum("Reduced mass, H Lyman-alpha 2p3/2: the hybrid differs from the m -> mu photon energy by exactly the correction times (1 - mu), as derived", hyb - sub, corr * (1 - mu), 1e-6);
        checkTrue("Reduced mass, H Lyman-alpha 2p3/2: that difference is below 1e-7 of the photon energy (measured, stated in the report), far below the Dirac correction itself", std::fabs(hyb - sub) / E < 1e-7 && std::fabs(hyb - sub) < 1e-2 * std::fabs(corr));
    }
    // ---- the splitting 2p3/2 - 2p1/2 of hydrogen against m c^2 alpha^4 / 32, from the constants ----
    {
        const double lead = mc * std::pow(a, 4) / 32, got = diracEV(1, 0, 2, 3) - diracEV(1, 0, 2, 1), gotInf = diracInfiniteEV(1, 2, 3) - diracInfiniteEV(1, 2, 1);
        std::printf("H n = 2 fine structure: Dirac %.9e eV (%.6f GHz), m c^2 alpha^4 / 32 = %.9e eV, ratio - 1 = %.3e (alpha^2 = %.3e)\n", got, got / nist::h * nist::e * 1e-9, lead, got / lead - 1, a * a);
        checkNum("H 2p3/2 - 2p1/2 Dirac splitting = m c^2 alpha^4 / 32 within alpha^2 (the next order)", got, lead, a * a);
        checkNum("H 2p3/2 - 2p1/2 splitting: the lab's level (reduced-mass main term) gives the same as the infinite-mass Dirac levels, because only the main term has the reduced mass", got, gotInf, 1e-9);
        checkTrue("H 2p3/2 is less bound than 2p1/2 (the splitting is positive, j = 3/2 above j = 1/2)", got > 0);
        checkNum("Dirac theory: 2s1/2 and 2p1/2 have exactly the same energy (no Lamb shift): the level depends on n and j only", diracEV(1, 0, 2, 1), diracEV(1, 0, 2, 1), 0);
        {   // NIST's own 2p3/2 - 2p1/2 gap for hydrogen, from ASD's two Lyman-alpha Ritz wavelengths (ASD's Ritz wavelengths are made from its measured energy levels): h c (1 / lambda(2p3/2) - 1 / lambda(2p1/2))
            const auto m = asdE1(1); const double l3 = m.at({1, 2, 1, 3}).front(), l1 = m.at({1, 2, 1, 1}).front();
            const double nist2 = nist::hcEVnm * (1 / l3 - 1 / l1), over = nist2 / got - 1;
            std::printf("H n = 2 gap from ASD's Lyman-alpha Ritz wavelengths: %.9e eV (%.6f GHz); Dirac %.9e eV; NIST / Dirac - 1 = %.4e\n", nist2, nist2 / nist::h * nist::e * 1e-9, got, over);
            checkTrue("H 2p3/2 - 2p1/2: NIST's measured gap is LARGER than the Dirac one, by 0.1772 % (measured; the guard is + 1 % of that): the part of the gap Dirac theory does not have (QED), small and one-sided", over > 0 && over < 1.789e-3);
        }
        checkNum("The splitting from the expansion to (Z alpha)^4: m c^2 alpha^4 / 32 is the leading difference of the series formula", refSeriesEV(1, 2, 3) - refSeriesEV(1, 2, 1), lead, 1e-9);
    }
    // ---- reduces to the non-relativistic level as Z alpha -> 0, at the order (Z alpha)^2 ----
    // Infinite nuclear mass first (pure Dirac against pure Bohr): the relative difference of the 1s level is (Z alpha)^2 / 4 at the lowest order (n = 1, j = 1/2 in the expansion (3)), and its next term is
    // of the order (Z alpha)^4, so the ratio to (Z alpha)^2 / 4 minus 1, divided by (Z alpha)^2, stays a constant. The lab's level carries the reduced mass on top, which is NOT a (Z alpha)^2 effect (5.4e-4
    // for hydrogen), so it is checked separately: the lab's relative difference is the infinite-mass one divided by mu / m.
    {
        double worstRatio = 0, worstSlope = 0, worstSeries = 0, worstMass = 0, prevR = 0, prevZ = 0;
        std::printf("Dirac vs non-relativistic 1s, infinite mass, (E_Dirac - E_NR) / E_NR over (Z alpha)^2 / 4 (1 at the lowest order):");
        for (int Z : {1, 2, 3, 4, 6, 8, 10, 14, 20}) {
            const double x2 = Z * a * Z * a, nr = -0.5 * mc * x2, r = refCorrEV(Z, 1, 1) / nr;   // (E_Dirac - E_NR) / E_NR from the double-double reference: the plain difference of the two energies would lose 5 digits
            worstRatio = std::max(worstRatio, std::fabs(r / (x2 / 4) - 1) / x2);       // the miss of the leading order, in units of (Z alpha)^2: about constant (3/8 in the expansion's next term)
            if (prevR != 0) worstSlope = std::max(worstSlope, std::fabs(std::log(r / prevR) / std::log(double(Z) / prevZ) - 2));
            prevR = r; prevZ = Z;
            std::printf(" Z=%d:%.5f", Z, r / (x2 / 4));
            const double M = nuclearMassMe(Z, defaultN(Z)), lab = (diracEV(Z, defaultN(Z), 1, 1) - levelEV(Z, defaultN(Z), 1)) / levelEV(Z, defaultN(Z), 1);
            worstMass = std::max(worstMass, std::fabs(lab / r * (M / (M + 1)) - 1));
        }
        std::printf("\n");
        for (int Z : {1, 2, 4, 8, 16}) {      // against the expansion: the miss is of the order (Z alpha)^4 of the energy
            const double x2 = Z * a * Z * a, miss = std::fabs(diracInfiniteEV(Z, 2, 3) - refSeriesEV(Z, 2, 3)) / std::fabs(refSeriesEV(Z, 2, 3));
            worstSeries = std::max(worstSeries, miss / (x2 * x2));
        }
        std::printf("Dirac vs non-relativistic: worst (ratio to (Z alpha)^2 / 4 minus 1) over (Z alpha)^2: %.5f; worst deviation of the log-log slope from 2: %.4e; lab's relative difference vs the infinite-mass one times m / mu: %.2e; miss of the (Z alpha)^4 expansion in units of (Z alpha)^4: %.5f\n", worstRatio, worstSlope, worstMass, worstSeries);
        checkTrue("Dirac - non-relativistic (1s, infinite mass) scales like (Z alpha)^2 for Z = 1 .. 20: (ratio to (Z alpha)^2 / 4 minus 1) / (Z alpha)^2 stays below its measured value + 1 %", worstRatio < GUARD_RATIO && worstRatio > 0.3);
        checkTrue("Dirac - non-relativistic: the log-log slope of the relative difference between neighbouring Z (1 .. 20) is 2 to within its measured deviation + 1 %", worstSlope < GUARD_SLOPE);
        checkTrue("The lab's relative difference (reduced-mass level) is the infinite-mass one times M / (M + 1) inverse, to 1e-8 for Z = 1 .. 20: the reduced mass is a separate, mass-ratio-sized factor", worstMass < 1e-8);
        checkTrue("Dirac 2p3/2 against the (Z alpha)^4 expansion: the miss is a constant (below its measured value + 1 %) times (Z alpha)^4 of the energy for Z = 1 .. 16, a different formula agreeing to the order it is exact", worstSeries < GUARD_SERIES);
    }
    // ---- which jumps are allowed ----
    {
        checkTrue("selection rule: 2p1/2 -> 1s1/2, 2p3/2 -> 1s1/2, 2s1/2 -> 2p3/2 type jumps follow l changes by 1 and j by 0 or 1", diracAllowed(0, 1, 1, 1) && diracAllowed(0, 1, 1, 3) && diracAllowed(1, 3, 2, 3) && diracAllowed(1, 3, 2, 5) && diracAllowed(1, 1, 2, 3));
        checkTrue("selection rule: d5/2 -> p1/2 (j changes by 2), s -> d (l changes by 2), p -> p (l unchanged) and an impossible j are not allowed", !diracAllowed(1, 1, 2, 5) && !diracAllowed(0, 1, 2, 3) && !diracAllowed(1, 1, 1, 3) && !diracAllowed(0, 3, 1, 3) && !diracAllowed(1, 5, 2, 5));
        auto count = [](int lo, int hi) { int n = 0; for (const DiracLine& l : diracLines(1, 0)) if (l.lo == lo && l.hi == hi) n++; return n; };
        checkTrue("lines counted by hand: Lyman-alpha 2 (np1/2, np3/2 -> 1s1/2), Lyman-beta 2, Balmer-alpha 5 (of the 7 textbook components two pairs coincide in Dirac theory), Balmer-beta 5", count(1, 2) == 2 && count(1, 3) == 2 && count(2, 3) == 5 && count(2, 4) == 5);
        // 3s1/2 -> 2p1/2 and 3p1/2 -> 2s1/2 have the same energies in Dirac theory: one line, not two
        int h1 = 0; for (const DiracLine& l : diracLines(1, 0)) if (l.lo == 2 && l.hi == 3 && l.jLo2 == 1 && l.jHi2 == 1) h1++;
        checkTrue("Dirac theory: 3s1/2 -> 2p1/2 and 3p1/2 -> 2s1/2 have equal energies, so they are ONE line", h1 == 1);
        // against NIST: for H I and He II the set of lines (n, j, n', j') of ASD's E1 components is exactly the set the rule gives
        for (int Z : {1, 2}) {
            std::set<std::array<int, 4>> mine, asd;
            for (const DiracLine& l : diracLines(Z, defaultN(Z))) if (l.hi >= 1 && l.hi <= 6) mine.insert({l.lo, l.hi, l.jLo2, l.jHi2});
            for (const auto& kv : asdE1(Z)) if (kv.first.b <= 6) asd.insert({kv.first.a, kv.first.b, kv.first.c, kv.first.d});
            std::printf("selection rule vs ASD, Z = %d: the rule gives %zu lines (n <= 6), ASD has %zu E1 (n, j, n', j') sets\n", Z, mine.size(), asd.size());
            checkTrue(std::string("selection rule vs NIST ASD, Z = ") + std::to_string(Z) + ": the lines the rule gives for n <= 6 are exactly the (n, j, n', j') sets of ASD's electric-dipole components", mine == asd);
        }
        // every ion ASD has: no ASD component lies outside the rule (ASD may lack some Ritz lines, the rule never lacks one ASD has)
        int zsChecked = 0; bool subset = true;
        for (int Z = 1; Z <= 118; Z++) {
            if (asdIon(Z).status != AsdStatus::Lines) continue;
            std::set<std::array<int, 4>> mine;
            for (const DiracLine& l : diracLines(Z, defaultN(Z))) if (l.hi >= 1 && l.hi <= 6) mine.insert({l.lo, l.hi, l.jLo2, l.jHi2});
            for (const auto& kv : asdE1(Z)) if (kv.first.b <= 6 && !mine.count({kv.first.a, kv.first.b, kv.first.c, kv.first.d})) subset = false;
            zsChecked++;
        }
        checkTrue("selection rule vs NIST ASD: for every ion ASD has, every electric-dipole component (n <= 6) is a line the rule allows (110 ions)", subset && zsChecked == 110);
    }
    // ---- against NIST: the lab's lines with Dirac closer to the measured ones than without ----
    {
        struct Case { int Z; const char* name; double guardOn, guardOff; double ratioMin; };
        const Case cases[3] = {{1, "H I", GUARD_H_ON, GUARD_H_OFF, GUARD_H_RATIO}, {2, "He II", GUARD_HE_ON, GUARD_HE_OFF, GUARD_HE_RATIO}, {26, "Fe XXVI", GUARD_FE_ON, GUARD_FE_OFF, GUARD_FE_RATIO}};
        for (const Case& c : cases) {
            const Miss off = worstMiss(c.Z, defaultN(c.Z), false), on = worstMiss(c.Z, defaultN(c.Z), true);
            std::printf("vs ASD, %s, %d components (n <= 6): worst relative miss without Dirac %.4e, with Dirac %.4e (improvement x%.2f; the lab's line is %s)\n", c.name, on.comps, off.worst, on.worst, std::fabs(off.worst / on.worst), on.worst > 0 ? "longer" : "shorter");
            checkTrue(std::string("Dirac vs ASD, ") + c.name + ": with Dirac the lab's lines land closer to every measured component set than without (worst miss smaller)", std::fabs(on.worst) < std::fabs(off.worst) && on.comps > 20);
            checkTrue(std::string("Dirac vs ASD, ") + c.name + ": the worst miss with Dirac is below its measured value + 1 % and the improvement factor above its measured value - 1 %", std::fabs(on.worst) < c.guardOn && std::fabs(off.worst) < c.guardOff && std::fabs(off.worst / on.worst) > c.ratioMin);
        }
    }
    // ---- what is left is the Lamb shift (and QED, nuclear size): ASD lies between Dirac and the non-relativistic lab, for Lyman-alpha of every ion ----
    {
        double lo = 1e9, hi = -1e9; bool order = true; int nZ = 0;
        for (int Z = 1; Z <= 118; Z++) {
            const auto m = asdE1(Z); if (m.empty()) continue;
            for (int j2 : {1, 3}) {
                const auto it = m.find({1, 2, 1, j2}); if (it == m.end()) continue;
                for (double w : it->second) {
                    const int N = defaultN(Z); const double nr = refLam(Z, N, 1, 1, 2, j2, false), d = refLam(Z, N, 1, 1, 2, j2, true), share = (w - d) / (nr - d);
                    lo = std::min(lo, share); hi = std::max(hi, share); order = order && nr > w && w > d; nZ++;
                }
            }
        }
        std::printf("Lamb-shift side, Lyman-alpha, both components of %d (ion, component) pairs: (ASD - Dirac) / (non-relativistic - Dirac) from %.5f to %.5f\n", nZ, lo, hi);
        checkTrue("What is left, Lyman-alpha (1s-2p1/2 and 1s-2p3/2) of every ion ASD has: non-relativistic lab > ASD > Dirac in wavelength, i.e. ASD lies between, as the Lamb shift (it lifts 1s) and the nuclear size would put it", order && nZ >= 200);
        checkTrue("What is left is small: ASD is at most 0.2744 of the way from Dirac to the non-relativistic line (measured max 0.27164, + 1 %), and always beyond Dirac", hi < GUARD_LEFT && lo > 0);
    }
    // ---- the lab's functions: the comparison, the lines, the rows, the words ----
    {
        bool same = true; double maxD = 0;
        for (int Z : {1, 2, 26}) {
            const auto cmp = compareIonDirac(Z, defaultN(Z));
            for (const FineCompare& c : cmp) {
                double worst = 0; int comps = 0;
                for (const auto& kv : asdE1(Z)) {
                    if (kv.first.a != c.lo || kv.first.b != c.hi) continue;
                    for (double w : kv.second) { const double r = (refLam(Z, defaultN(Z), c.lo, kv.first.c, c.hi, kv.first.d, true) - w) / w; if (std::fabs(r) > std::fabs(worst)) worst = r; comps++; }
                }
                maxD = std::max(maxD, std::fabs(c.worstDiracRel - worst)); same = same && c.asdComponents == comps && c.asdUnmatched == 0 && c.lo < c.hi && c.hi <= 6 && c.labLines >= 1;
            }
            same = same && !cmp.empty();
        }
        std::printf("compareIonDirac vs the hand-made comparison: worst difference of a relative miss %.3e (the misses themselves are 1e-6 to 1e-2)\n", maxD);
        checkTrue("compareIonDirac gives the component counts and, to 1e-14 (rounding; measured 3.2e-16 since 0.10.8 stage 2, it was 1.1e-13 while the reference was long double; the misses themselves are 1e-6 and up), the worst miss of the hand-made comparison (H I, He II, Fe XXVI, every transition n <= 6)", same && maxD < 1e-14);
        const auto L = diracLines(1, 0);
        bool shape = !L.empty(), limits = true;
        for (const DiracLine& l : L) { shape = shape && l.lam > 0 && l.lo >= 1 && l.lo <= 5 && (l.hi == 0 ? l.jHi2 == 0 : l.hi > l.lo && l.hi <= l.lo + 20); if (l.hi == 0) limits = limits && l.lo >= 1; }
        int lim2 = 0; for (const DiracLine& l : L) if (l.hi == 0 && l.lo == 2) lim2++;
        checkTrue("diracLines: every line has a positive wavelength and sensible levels; the n = 2 series limit is split in two (j = 1/2 and 3/2)", shape && lim2 == 2);
        // the wavelength of Lyman-alpha 2p3/2 in the lab with Dirac equals the hand-made one
        double got = 0; for (const DiracLine& l : L) if (l.lo == 1 && l.hi == 2 && l.jHi2 == 3) got = l.lam;
        checkNum("diracLines: H Lyman-alpha 2p3/2 wavelength = the hand-made one", got, refLam(1, 0, 1, 1, 2, 3, true), 1e-12);
        // visible lines and the lines of the spectrum come from ONE list
        const auto V = visibleLinesFor(1, 0, true); bool visOk = !V.empty();
        for (const SpecLine& l : V) visOk = visOk && isVisibleNm(l.lam) && l.hi != 0;
        checkTrue("visibleLinesFor(Dirac) lists the Dirac lines inside visible light (Balmer and above), none a series limit", visOk);
    }
    {   // OFF must be the old lab, number for number: the same list, bit for bit, every element
        bool listSame = true, visSame = true, noteSame = true;
        for (int Z = 1; Z <= 118; Z++) {
            const int N = defaultN(Z);
            const auto A = specLines(Z, N), B = specLinesFor(Z, N, false), V1 = visibleLines(Z, N), V2 = visibleLinesFor(Z, N, false);
            listSame = listSame && A.size() == B.size() && std::memcmp(A.data(), B.data(), A.size() * sizeof(SpecLine)) == 0;
            visSame = visSame && V1.size() == V2.size() && (V1.empty() || std::memcmp(V1.data(), V2.data(), V1.size() * sizeof(SpecLine)) == 0);
            noteSame = noteSame && spectrumNote(Z, N) == spectrumNote(Z, N, false);
        }
        checkTrue("setting OFF: specLinesFor, visibleLinesFor and the spectrum note are the old ones, bit for bit, for all 118 elements (compared in this run, no stored hashes)", listSame && visSame && noteSame);
        checkTrue("setting OFF is the default, in the web-like struct and in the app's defaults, and only Off and On are valid choices", AtomParams().dirac == "Off" && AtomParams::appDefaults().dirac == "Off" && AtomLab::validChoice("dirac", "Off") && AtomLab::validChoice("dirac", "On") && !AtomLab::validChoice("dirac", "on") && !AtomLab::validChoice("dirac", ""));
        // a lab with the setting set to Off by hand reads out and runs exactly as one never told: same readout lines, same dots
        AtomLab x, y; x.nowMs = [] { return 0.0; }; y.nowMs = [] { return 0.0; };
        x.p = AtomParams(); y.p = AtomParams(); x.restart(); y.restart();
        seedRandom(77); x.setParam("view", std::string("Cloud")); x.setParam("Z", 26.0);
        seedRandom(77); y.setParam("view", std::string("Cloud")); y.setParam("dirac", std::string("Off")); y.setParam("Z", 26.0);
        const auto rx = x.info(), ry = y.info();
        checkTrue("setting OFF set by hand: the readout lines and the cloud dots are identical to a lab that never saw the setting", rx == ry && x.s.k == y.s.k && x.s.pts == y.s.pts);
        y.setParam("dirac", std::string("On"));
        checkTrue("setting ON changes nothing about the cloud or the readout's energy line: the pictures stay non-relativistic (the readout gains a note only)", x.s.pts == y.s.pts && y.info().size() >= rx.size());
    }
    {   // the words
        auto has = [](const std::string& s, const char* w) { return s.find(w) != std::string::npos; };
        const std::string lab = kDiracLabel, ex = diracExplain(), on = diracOnNote(26), off = diracOffNote(), sn = spectrumNote(26, defaultN(26), true), an = asdIonNoteDirac(26), same = spectrumNote(26, defaultN(26));
        checkTrue("Dirac words: the label is short and honest (Dirac levels), and the one-line explanation says what changes (levels and lines in the spectrum cards) and what does not (pictures, readouts, Lamb shift, nuclear size)",
                  has(lab, "Dirac") && lab.size() < 40 && has(ex, "levels") && has(ex, "lines") && has(ex, "spectrum") && has(ex, "pictures") && has(ex, "non-relativistic") && has(ex, "Lamb shift") && has(ex, "size of the nucleus") && ex.size() < 400);
        checkTrue("Dirac words (on): says the energies and lines are Dirac while the shapes are still non-relativistic, one electron, point nucleus; levels depend on j; 2s and 2p (j = 1/2) equal; the Lamb shift, QED and nuclear size are not in it and why NIST still differs",
                  has(on, "Dirac equation") && has(on, "one electron") && has(on, "point nucleus") && has(on, "clouds") && has(on, "non-relativistic") && has(on, "j") && has(on, "2s") && has(on, "2p") && has(on, "exactly the same energy") && has(on, "Lamb shift") && has(on, "QED") && has(on, "size of the nucleus") && has(on, "NIST") && has(on, "reduced mass"));
        checkTrue("Dirac words (on): the reduced-mass treatment is said plainly: the main energy has the reduced mass, the relativistic correction is for an infinitely heavy nucleus, recoil beyond that is not included", has(on, "infinitely heavy") && has(on, "recoil"));
        checkTrue("Dirac words: with the setting off the note says only that the setting is off and non-relativistic is what is shown", has(off, "non-relativistic") && !has(off, "Lamb shift is"));
        checkTrue("Dirac words: the spectrum note (on) says fine structure IS included and the Lamb shift, QED and nuclear size are NOT, and that the marks of your jumps and photon readouts keep the non-relativistic value; the old note (off) is unchanged and says fine structure is not included",
                  has(sn, "Dirac") && has(sn, "Lamb shift") && has(sn, "size of the nucleus") && has(sn, "non-relativistic") && !has(sn, "Not included: fine structure") && has(same, "Not included: fine structure"));
        checkTrue("Dirac words: the real-spectrum note (on) says the top row is the lab's Dirac lines, what is left is the Lamb shift, QED and nuclear size, and that this checks the lab", has(an, "Dirac") && has(an, "Lamb shift") && has(an, "size of the nucleus") && has(an, "checks the lab"));
        const auto rows = diracLevelRows(1, 0); std::string all; for (const std::string& r : rows) all += r + "\n";
        checkTrue("Dirac level rows (hydrogen): n = 1, 2 and 3 are listed, 2s and 2p share j = 1/2, and the 2p 3/2 - 2p 1/2 gap is given in eV-type units", rows.size() >= 6 && has(all, "n = 1") && has(all, "n = 2") && has(all, "n = 3") && has(all, "2s, 2p") && has(all, "gap") && has(all, "GHz"));
    }
}

// ================= 0.10.8 stage 2: the precision of the Dirac formula =================
// Stage 1's Windows build failed because its reference was computed with long double, which is 80 bits on Linux and 64 on MSVC: the plain subtraction sqrt(1 - alpha^2) - 1 (the result is 2.7e-5, so 11 of the 16
// digits of the sqrt are thrown away) was then 2.5e-12 off, and the LAB's value, which had no such subtraction, looked wrong. Here the references are well conditioned and portable: double-double arithmetic (the
// classes above) of the textbook formula, pinned by 80-digit values typed from Python's decimal module. Written from the same double Z alpha the lab forms, so what is measured is the arithmetic of the lab's
// rearrangements, not the rounding of the input. The "old" forms below are written out here exactly as they were (the naive textbook form; stage 1's expm1/log1p energy and its E + m c^2 (Z alpha)^2 / 2 n^2 correction).
// The lab's rewritten quantities (rounding level, units of the last digit 2.2e-16; measured worst: delta 1.77e-16, energy 4.56e-16, correction 1.29e-15, readout 2.98e-16): about 2-4 times the measured.
// The old forms' errors (plain IEEE arithmetic, so the same on every platform; the guard is measured - 1 %, rounded down): Z = 1: delta 6.01e-11, energy 1.45e-10, stage 1 correction 2.07e-10;
// Z = 92: stage 1 correction 3.65e-14, naive energy 1.46e-14.
constexpr double GUARD_NOW_D = 5e-16, GUARD_NOW_E = 1e-15, GUARD_NOW_C = 5e-15, GUARD_NOW_R = 1e-15;
constexpr double GUARD_NAIVE_D = 5.95e-11, GUARD_NAIVE_E = 1.43e-10, GUARD_NAIVE_C = 2.04e-10, GUARD_NAIVE_C92 = 3.61e-14, GUARD_NAIVE_E92 = 1.44e-14;
static void testDiracPrecision() {
    // guards: rounding-level ones are in units of the last digit (2.2e-16), not measured + 1 %, because the last bit may differ between compilers; the deterministic ones (the naive forms' errors, plain IEEE operations) are
    // measured - 1 % (they must stay at least that bad, or the test no longer shows the cancellation): see the numbers printed below and the report.
    {   // the double-double arithmetic itself
        const DD r2 = ddSqrt(DD{2, 0}), e1 = ddSub(ddMul(r2, r2), DD{2, 0}), e2 = ddSub(ddMul(ddDiv(DD{1, 0}, DD{3, 0}), DD{3, 0}), DD{1, 0});
        std::printf("double-double: sqrt(2)^2 - 2 = %.1e, (1/3) 3 - 1 = %.1e\n", e1.hi, e2.hi);
        checkTrue("double-double: sqrt(2)^2 = 2 and (1/3) 3 = 1 to 1e-30 (about 32 digits), so it can serve as the reference", std::fabs(e1.hi) < 1e-30 && std::fabs(e2.hi) < 1e-30);
    }
    double worstDD = 0, tE = 0, tC = 0, tD = 0;
    for (const DiracTyped& r : kDiracTyped) {
        const DiracDD d = ddDirac(r.Z, r.n, r.twoJ);
        worstDD = std::max({worstDD, ddRel(d.E, r.E), ddRel(d.c, r.c), ddRel(d.delta, r.delta)});
        tE = std::max(tE, ddRel(DD{diracInfiniteEV(r.Z, r.n, r.twoJ), 0}, r.E)); tC = std::max(tC, ddRel(DD{diracCorrectionEV(r.Z, r.n, r.twoJ), 0}, r.c)); tD = std::max(tD, ddRel(DD{diracDelta(r.Z, r.twoJ), 0}, r.delta));
    }
    std::printf("typed 80-digit values (Z = 1 and 92, 5 levels each): double-double worst %.2e; the lab: energy %.2e, correction %.2e, delta %.2e (relative)\n", worstDD, tE, tC, tD);
    // The textbook form has the plain subtractions, so even 32 digits leave less at Z = 1, n = 6: measured worst 3.64e-20 (the correction, whose size is 4e-7 of the energy it is the difference of), + 1 %. Still 4 orders
    // of magnitude finer than a double (the lab is at 1e-15), so it is a valid reference; the energy itself is good to 1.3e-26.
    checkTrue("double-double reference = the 80-digit values typed from Python decimal, to 3.7e-20 relative (measured 3.64e-20 + 1 %; E, the correction and delta, Z = 1 and 92, five levels each)", worstDD < 3.7e-20);
    checkTrue("the lab's energy, correction and delta agree with the typed 80-digit values to 5e-15 (a few units of the last digit; Z = 1 and 92, five levels each)", tE < 5e-15 && tC < 5e-15 && tD < 5e-15);
    // ---- digits gained, against the double-double reference: Z = 1 and 92, n = 1 .. 6, every j ----
    struct Worst { double naive = 0, old = 0, now = 0; };
    Worst wE[2], wD[2], wC[2];
    const int zs[2] = {1, 92};
    for (int zi = 0; zi < 2; zi++) {
        const int Z = zs[zi];
        for (int n = 1; n <= 6; n++)
            for (int j2 = 1; j2 <= 2 * n - 1; j2 += 2) {
                const double x = Z * nist::alpha, x2 = x * x, jh = (j2 + 1) / 2.0, m = nist::mecEV;
                const DiracDD ref = ddDirac(Z, n, j2);
                const double dNaive = jh - std::sqrt(jh * jh - x2), dStable = x2 / (jh + std::sqrt(jh * jh - x2));
                const double eNaive = m * (1 / std::sqrt(1 + x2 / ((n - dNaive) * (n - dNaive))) - 1);                 // the textbook form with the plain subtractions
                const double eOld = m * std::expm1(-0.5 * std::log1p(x2 / ((n - dStable) * (n - dStable))));            // stage 1's energy (already without the subtractions)
                const double cOld = eOld + 0.5 * m * x2 / (double(n) * n), cNaive = eNaive + 0.5 * m * x2 / (double(n) * n);   // stage 1's correction: the difference of two energies
                auto up = [](Worst& w, double naive, double old, double now, DD ref) { w.naive = std::max(w.naive, ddRel(DD{naive, 0}, ref)); w.old = std::max(w.old, ddRel(DD{old, 0}, ref)); w.now = std::max(w.now, ddRel(DD{now, 0}, ref)); };
                up(wD[zi], dNaive, dNaive, diracDelta(Z, j2), ref.delta);
                up(wE[zi], eNaive, eOld, diracInfiniteEV(Z, n, j2), ref.E);
                up(wC[zi], cNaive, cOld, diracCorrectionEV(Z, n, j2), ref.c);
            }
    }
    auto digits = [](double was, double now) { return std::log10(was / std::max(now, 1e-18)); };
    std::printf("digits against the double-double reference (worst relative error over n = 1..6 and every j; digits gained = log10(old / now)):\n");
    for (int zi = 0; zi < 2; zi++) {
        std::printf("  Z = %2d  delta: naive %.2e -> now %.2e (%.1f digits)   energy: naive %.2e, stage 1 %.2e -> now %.2e (%.1f digits over naive)   correction: naive-based %.2e, stage 1 %.2e -> now %.2e (%.1f digits over stage 1)\n",
                    zs[zi], wD[zi].naive, wD[zi].now, digits(wD[zi].naive, wD[zi].now), wE[zi].naive, wE[zi].old, wE[zi].now, digits(wE[zi].naive, wE[zi].now), wC[zi].naive, wC[zi].old, wC[zi].now, digits(wC[zi].old, wC[zi].now));
    }
    checkTrue("precision: the lab's delta, energy and correction are within GUARD_NOW_D / GUARD_NOW_E / GUARD_NOW_C of the double-double reference for Z = 1 and 92, n = 1..6, every j (rounding level, units of the last digit)",
              wD[0].now < GUARD_NOW_D && wD[1].now < GUARD_NOW_D && wE[0].now < GUARD_NOW_E && wE[1].now < GUARD_NOW_E && wC[0].now < GUARD_NOW_C && wC[1].now < GUARD_NOW_C);
    checkTrue("precision: the naive forms really are bad at Z = 1 (so this test shows the cancellation): delta off by at least GUARD_NAIVE_D, the energy by at least GUARD_NAIVE_E, stage 1's correction by at least GUARD_NAIVE_C (measured - 1 %)",
              wD[0].naive >= GUARD_NAIVE_D && wE[0].naive >= GUARD_NAIVE_E && wC[0].old >= GUARD_NAIVE_C);
    checkTrue("precision: at Z = 92 the stage 1 correction was off by at least GUARD_NAIVE_C92 and the naive energy by at least GUARD_NAIVE_E92 (measured - 1 %); the new ones are not", wC[1].old >= GUARD_NAIVE_C92 && wE[1].naive >= GUARD_NAIVE_E92);
    checkTrue("precision: the rewrites gain digits at Z = 1: at least 4 for the correction over stage 1, at least 5 for the energy over the naive form", digits(wC[0].old, wC[0].now) >= 4 && digits(wE[0].naive, wE[0].now) >= 5);
    // ---- the readout's two heavy-atom formulas, 1 - sqrt(1 - (Z alpha)^2): the percentage and the 1s energy ----
    {
        double worstOld = 0, worstNew = 0, worstOldE = 0, worstNewE = 0; bool same = true;
        for (int Z = 1; Z <= 118; Z++) {
            const double zx = Z * nist::alpha, zx2 = zx * zx, sq = std::sqrt(1 - zx2);
            const double oldPct = 100 * ((1 - sq) / (0.5 * zx2) - 1), newPct = 100 * zx2 / ((1 + sq) * (1 + sq)), oldE = nist::mecEV * (1 - sq), newE = nist::mecEV * zx2 / (1 + sq);
            const DD y = ddTwoProd(zx, zx), s = ddSqrt(ddSub(DD{1, 0}, y)), num = ddSub(DD{1, 0}, s);
            const DD refPct = ddMul(DD{100, 0}, ddSub(ddDiv(num, ddMul(DD{0.5, 0}, y)), DD{1, 0})), refE = ddMul(DD{nist::mecEV, 0}, num);
            worstOld = std::max(worstOld, ddRel(DD{oldPct, 0}, refPct)); worstNew = std::max(worstNew, ddRel(DD{newPct, 0}, refPct));
            worstOldE = std::max(worstOldE, ddRel(DD{oldE, 0}, refE)); worstNewE = std::max(worstNewE, ddRel(DD{newE, 0}, refE));
            same = same && jsNum(oldPct, 2) == jsNum(newPct, 2) && fmtEV(oldE) == fmtEV(newE);      // what the readout SHOWS (2 decimals; the 1s energy in eV text)
        }
        std::printf("readout, Z = 1..118: the percentage off by %.2e (old) -> %.2e (now), the 1s energy off by %.2e (old) -> %.2e (now); the shown text identical for all 118: %s\n", worstOld, worstNew, worstOldE, worstNewE, same ? "yes" : "NO");
        checkTrue("readout: the cancellation-free percentage and 1s energy are within GUARD_NOW_R of the double-double reference for Z = 1 .. 118 (rounding level)", worstNew < GUARD_NOW_R && worstNewE < GUARD_NOW_R);
        checkTrue("readout: the text the readout shows (the percentage to 2 decimals, the 1s energy) is the same with the old and the new arithmetic for all 118 elements: nothing the app shows changed", same);
    }
}

// ================= 0.10.8 stage 3: real lifetimes and selection rules (from NIST ASD) =================
// References that do not come from the lab's lifetime code: (1) numbers typed from data/asd/asd_lines.tsv (the file the fetch wrote, NOT the compiled table) so the whole pipeline is checked: the A values, their accuracy
// codes and transition types; (2) 1 / (sum of A) worked out here from those typed numbers; (3) ASD's own words for the selection rule (ASD's lines help, read 2026-10-04: an allowed E1 line joins levels of opposite
// parity whose J differs by at most 1, J = 0 <-> 0 excluded; its Type column is blank for E1 and M1, E2, M1+E2, UT for forbidden ones); (4) the ASD data itself, scanned here by a separate loop (grouping by the upper level).
// No long double and no hashes of doubles (0.10.6 stage 9, 0.10.8 stage 2).
struct AsdE1 { Level lo, hi; double A; std::string acc, type; };
static std::vector<AsdE1> asdAllLines(int Z) {             // every resolved ASD line of the ion whose two levels read as (n, l, j), lower n < upper n
    std::vector<AsdE1> out; const AsdSpectrum& sp = asdIon(Z);
    for (int i = 0; i < sp.count; i++) {
        AsdTransition t;
        if (!asdParseTransition(sp.lines[i].transition, t) || t.nLo >= t.nHi) continue;
        out.push_back({{t.nLo, t.lLo, t.jLo2}, {t.nHi, t.lHi, t.jHi2}, sp.lines[i].aki, sp.lines[i].acc, sp.lines[i].type});
    }
    return out;
}
// ASD's rule in ASD's words (opposite parity, J changes by at most 1, not 0 <-> 0 (never needed: j is at least 1/2)), for two real levels
static bool asdRuleAllows(Level lo, Level hi) { return (lo.l + hi.l) % 2 == 1 && std::abs(lo.j2 - hi.j2) <= 2; }
static std::vector<Level> allLevelsUpTo(int nMax) { std::vector<Level> v; for (int n = 1; n <= nMax; n++) for (int l = 0; l < n; l++) for (int j2 = 2 * l - 1; j2 <= 2 * l + 1; j2 += 2) if (j2 >= 1) v.push_back({n, l, j2}); return v; }

// Guards = the worst MEASURED value + 1 % (Linux GCC Release), for A(Z) / (Z^4 A(H)) from ASD's own numbers, the three transitions 2p3/2 -> 1s, 2p1/2 -> 1s, 3d5/2 -> 2p3/2:
//  worst |r - 1| for Z <= 10: 6.489e-4, 1.229e-3, 8.749e-4 -> 6.554e-4, 1.242e-3, 8.837e-4 (the reduced mass and the like);
//  worst |r - 1| / (Z alpha)^2 for Z >= 10 (ions up to Z = 110): 0.3614, 0.2308, 0.1452 -> 0.3651, 0.2331, 0.1467 (the relativistic correction, of the order (Z alpha)^2; r falls to 0.77 at Z = 110 for 2p3/2).
static constexpr double GUARD_Z4_LOW[3] = {6.554e-4, 1.242e-3, 8.837e-4}, GUARD_Z4_REL[3] = {0.3651, 0.2331, 0.1467};
static void testLifetimes() {
    // ---- 1. what the file says (typed from asd_lines.tsv: the Aki, Acc and Type columns) ----
    {
        int ionN = 0, withA = 0, accAA = 0, accAAA = 0, accNone = 0, accNoneWithA = 0, t1 = 0, tM1 = 0, tE2 = 0, tUT = 0, tMix = 0;
        for (int Z = 1; Z <= 118; Z++) { const AsdSpectrum& sp = asdIon(Z); for (int i = 0; i < sp.count; i++) { const AsdLine& l = sp.lines[i]; ionN++; withA += !std::isnan(l.aki); const std::string a = l.acc, t = l.type;
            accAA += a == "AA"; accAAA += a == "AAA"; accNone += a.empty(); accNoneWithA += a.empty() && !std::isnan(l.aki); t1 += t.empty(); tM1 += t == "M1"; tE2 += t == "E2"; tUT += t == "UT"; tMix += t == "M1+E2"; } }
        std::printf("ASD ion lines: %d, with A %d; accuracy AA %d, AAA %d, none %d (of which with an A value: %d); types E1 %d, M1 %d, E2 %d, UT %d, M1+E2 %d\n", ionN, withA, accAA, accAAA, accNone, accNoneWithA, t1, tM1, tE2, tUT, tMix);
        checkTrue("ASD data (counted in asd_lines.tsv): 5,029 ion lines, 4,925 with an A value; accuracy AA 4,762, AAA 159, none 108 (4 of them have an A value: ASD gave a rate and no rating); types E1 5,011, M1 11, E2 5, UT 1, M1+E2 1",
                  ionN == 5029 && withA == 4925 && accAA == 4762 && accAAA == 159 && accNone == 108 && accNoneWithA == 4 && t1 == 5011 && tM1 == 11 && tE2 == 5 && tUT == 1 && tMix == 1);
        const std::vector<AsdE1> hl = asdAllLines(1), hel = asdAllLines(2);
        auto find = [](const std::vector<AsdE1>& v, Level lo, Level hi) -> const AsdE1* { for (const AsdE1& e : v) if (e.lo.n == lo.n && e.lo.l == lo.l && e.lo.j2 == lo.j2 && e.hi.n == hi.n && e.hi.l == hi.l && e.hi.j2 == hi.j2) return &e; return nullptr; };
        const AsdE1 *a3 = find(hl, {1, 0, 1}, {2, 1, 3}), *a1 = find(hl, {1, 0, 1}, {2, 1, 1}), *m1 = find(hl, {1, 0, 1}, {2, 0, 1}), *e2 = find(hl, {1, 0, 1}, {3, 2, 5}), *he = find(hel, {1, 0, 1}, {2, 1, 3});
        checkTrue("ASD data, read from asd_lines.tsv: H I Lyman-alpha 1s - 2p3/2 A = 6.2647e8 /s (AAA), 1s - 2p1/2 6.2648e8 /s (AAA); He II 1s - 2p3/2 1.0029e10 /s (AA)",
                  a3 && a1 && he && a3->A == 6.2647e8 && a3->acc == "AAA" && a1->A == 6.2648e8 && a1->acc == "AAA" && he->A == 1.0029e10 && he->acc == "AA" && a3->type.empty());
        checkTrue("ASD data, read from asd_lines.tsv: H I 1s - 2s is listed as M1 with A = 2.495e-6 /s (AAA); 1s - 3d5/2 as E2 with A = 5.937e2 /s (AAA)", m1 && e2 && m1->type == "M1" && m1->A == 2.495e-6 && m1->acc == "AAA" && e2->type == "E2" && e2->A == 5.937e2 && e2->acc == "AAA");
    }
    // ---- 2. the lifetime of a state: 1 / (sum of A of every allowed way down) ----
    {
        // numbers typed from asd_lines.tsv (H I): A in /s, all AAA
        const double p3 = 6.2647e8, p1 = 6.2648e8, p3_1s = 1.6725e8, p3_2s = 2.2448e7, p1_1s = 1.6725e8, p1_2s = 2.2449e7, d3_p1 = 5.3877e7, d3_p3 = 1.0775e7, d5_p3 = 6.4651e7, s3_p1 = 2.1046e6, s3_p3 = 4.2097e6;
        const Life l2p3 = lifeOf(1, {2, 1, 3}), l2p1 = lifeOf(1, {2, 1, 1}), l3p3 = lifeOf(1, {3, 1, 3}), l3p1 = lifeOf(1, {3, 1, 1}), l3d3 = lifeOf(1, {3, 2, 3}), l3d5 = lifeOf(1, {3, 2, 5}), l3s = lifeOf(1, {3, 0, 1});
        std::printf("H I lifetimes from ASD's A: 2p3/2 %.5e s (A %.4e), 2p1/2 %.5e s, 3p3/2 %.5e s, 3p1/2 %.5e s, 3d3/2 %.5e s, 3d5/2 %.5e s, 3s %.5e s\n", l2p3.tauS, p3, l2p1.tauS, l3p3.tauS, l3p1.tauS, l3d3.tauS, l3d5.tauS, l3s.tauS);
        checkNum("H 2p3/2 lifetime = 1 / A(Lyman-alpha) with A read from the data file (6.2647e8 /s)", l2p3.tauS, 1 / p3, 1e-12);
        checkNum("H 2p1/2 lifetime = 1 / 6.2648e8 /s", l2p1.tauS, 1 / p1, 1e-12);
        checkTrue("H 2p: one way down (to 1s 1/2), ASD's rate complete, accuracy AAA, the lifetime is 1.596 ns", l2p3.ways.size() == 1 && l2p3.complete && l2p3.ways[0].acc == "AAA" && std::fabs(l2p3.tauS - 1.5963e-9) < 1e-12);
        checkNum("H 3p3/2 lifetime = 1 / (A to 1s + A to 2s) from the data file", l3p3.tauS, 1 / (p3_1s + p3_2s), 1e-12);
        checkNum("H 3p1/2 lifetime = 1 / (A to 1s + A to 2s) from the data file", l3p1.tauS, 1 / (p1_1s + p1_2s), 1e-12);
        checkNum("H 3d3/2 lifetime = 1 / (A to 2p1/2 + A to 2p3/2) from the data file", l3d3.tauS, 1 / (d3_p1 + d3_p3), 1e-12);
        checkNum("H 3d5/2 lifetime = 1 / A(to 2p3/2) from the data file (one way down)", l3d5.tauS, 1 / d5_p3, 1e-12);
        checkNum("H 3s lifetime = 1 / (A to 2p1/2 + A to 2p3/2) from the data file", l3s.tauS, 1 / (s3_p1 + s3_p3), 1e-12);
        checkTrue("H 3d3/2 has two ways (2p 1/2 and 2p 3/2); the branching ratios are A / sum and sum to 1", l3d3.ways.size() == 2 && std::fabs(l3d3.ways[0].A / l3d3.sumA - d3_p1 / (d3_p1 + d3_p3)) < 1e-12 && std::fabs(l3d3.ways[0].A / l3d3.sumA + l3d3.ways[1].A / l3d3.sumA - 1) < 1e-15);
        // a state the lab offers but ASD cannot complete, or has no data for: said, never filled in
        const Life none = lifeOf(118, {2, 1, 3});
        checkTrue("an element ASD has no hydrogen-like ion for (Og, Z = 118): status 'no data', no lifetime number", none.status == LifeStatus::NoData && std::isnan(none.tauS));
    }
    // ---- 3. metastable 2s: no electric-dipole way down; no number is invented ----
    {
        const Life s2 = lifeOf(1, {2, 0, 1});
        checkTrue("H 2s1/2 has NO allowed way down to a lower n (l would not change, parity would not change): it is metastable, and the lab gives no lifetime number", s2.ways.empty() && s2.metastable && std::isnan(s2.tauS) && std::isnan(s2.tauMaxS));
        const std::string t = metastableText(1);
        auto has = [](const std::string& x, const char* w) { return x.find(w) != std::string::npos; };
        checkTrue("metastable text: says no electric-dipole jump is allowed, that a real 2s state still decays (two photons at once), that ASD does not list that process and the lab does not model it, and gives no lifetime", has(t, "electric-dipole") && has(t, "two photons") && has(t, "does not") && has(t, "ASD") && !has(t, " ns") && !has(t, " ms"));
        const std::vector<Jump> j = jumpsBetween(1, 2, 0, 1, 0);      // 2s -> 1s
        bool m1 = false; for (const Jump& x : j) m1 = m1 || (x.asdType == "M1" && x.haveA && x.A == 2.495e-6 && x.acc == "AAA" && !x.allowed);
        checkTrue("2s -> 1s in H: not allowed; ASD lists it only as magnetic dipole (M1) with A = 2.495e-6 /s (AAA), and the lab shows that rate with its type, not as an allowed jump", j.size() == 1 && m1);
        // no state with no allowed way ever gets a lifetime number
        bool never = true; int metas = 0;
        for (int Z = 1; Z <= 110; Z++) for (const Level& L : allLevelsUpTo(6)) { const Life x = lifeOf(Z, L); if (x.ways.empty()) { metas++; never = never && (x.metastable || x.ground) && std::isnan(x.tauS) && std::isnan(x.tauMaxS); } }
        checkTrue("every state with no allowed way down to a lower n (the ground state 1s, and the metastable 2s) has no lifetime number, for all 110 ions", never && metas > 0);
        std::printf("states with no allowed way down to a lower n, over 110 ions and n <= 6: %d (1s and 2s of each ion)\n", metas);
    }
    // ---- 4. the selection rules against ASD (extends 0.10.8 stage 1's 90-of-90 for H I and He II to every ion and to the forbidden lines) ----
    {
        int e1 = 0, forb = 0, e1Bad = 0, forbBad = 0, reasonBad = 0, typeBad = 0, typeOddZ = 0;
        std::set<std::string> forbTypes;
        for (int Z = 1; Z <= 118; Z++) {
            if (asdIon(Z).status != AsdStatus::Lines) continue;
            for (const AsdE1& e : asdAllLines(Z)) {
                const bool lab = diracAllowed(e.lo.l, e.lo.j2, e.hi.l, e.hi.j2);
                const bool oddCs = Z == 55 && e.lo.n == 1 && e.hi.n == 2 && e.hi.l == 0;   // ASD's one anomaly, see below
                if (e.type.empty()) { e1++; e1Bad += !lab && !oddCs; typeOddZ += !lab && oddCs; }
                else { forb++; forbBad += lab; forbTypes.insert(e.type); }
                const std::vector<Jump> js = jumpsBetween(Z, e.hi.n, e.hi.l, e.lo.n, e.lo.l);
                bool found = false; for (const Jump& x : js) if (x.upper.j2 == e.hi.j2 && x.lower.j2 == e.lo.j2) { found = true; typeBad += (x.allowed != e.type.empty()) && !oddCs; reasonBad += !x.allowed && x.why.empty(); }
                typeBad += !found;
            }
        }
        std::printf("selection rules vs ASD, all ions with data: %d lines ASD calls allowed (E1), the lab calls %d of them not allowed (and %d more: the Cs LV 1s-2s line, see below); %d lines ASD lists as forbidden (", e1, e1Bad, typeOddZ, forb);
        for (const std::string& t : forbTypes) std::printf("%s ", t.c_str());
        std::printf("), the lab calls %d of them allowed\n", forbBad);
        // ONE exception, found by this test and reported (REPORT.md): ASD lists the line 1s 2S 1/2 - 2s 2S 1/2 of Cs LV (Z = 55) with a BLANK Type (= allowed E1) and A = 7.591e11 /s, but l does not change (nor the parity), so it
        // cannot be electric dipole; the same line is typed M1 for Z = 56 (9.155e11) and UT for Z = 38 (4.068e10). The lab follows the rule and refuses it, says so on screen and does not use ASD's value; the test names it.
        checkTrue("selection rules vs ASD: EVERY ASD line of every ion that is allowed E1 (Type blank, both levels read as n, l, j) is a jump the lab calls allowed, with ONE exception, ASD's own Cs LV 1s-2s line (4,770 lines checked)", e1 > 4000 && e1Bad == 0 && typeOddZ == 1);
        checkTrue("selection rules vs ASD: the lab calls allowed NOTHING that ASD lists only as forbidden (M1, E2, M1+E2, UT): all 16 forbidden lines that read as levels (E2, M1, UT) are refused", forb == 16 && forbBad == 0);
        {   const std::vector<Jump> cs = jumpsBetween(55, 2, 0, 1, 0); const std::string line = cs.empty() ? "" : jumpLine(cs.front());
            checkTrue("the Cs LV 1s-2s anomaly is said on screen: not allowed, ASD marks it allowed, the lab follows the rule and does not use ASD's value as a lifetime", cs.size() == 1 && !cs[0].allowed && cs[0].asdListed && cs[0].asdType.empty() && line.find("disagrees with the rule") != std::string::npos && line.find("does not use that value") != std::string::npos && lifeOf(55, {2, 0, 1}).metastable); }
        checkTrue("jumpsBetween agrees with ASD's Type for every line (allowed exactly when the Type is blank), and every refused jump carries at least one reason", typeBad == 0 && reasonBad == 0);
        // ASD's own wording of the rule (opposite parity, J changes by at most 1) against the lab's rule (l changes by 1, j by at most 1) for EVERY pair of levels n <= 6: the same
        int pairs = 0, differ = 0;
        for (const Level& lo : allLevelsUpTo(6)) for (const Level& hi : allLevelsUpTo(6)) { if (hi.n <= lo.n) continue; pairs++; differ += asdRuleAllows(lo, hi) != diracAllowed(lo.l, lo.j2, hi.l, hi.j2); }
        std::printf("rule in ASD's words (parity + J) against the lab's rule (l + j), all %d pairs of levels n <= 6 with n' > n: %d differ\n", pairs, differ);
        checkTrue("ASD's wording of the rule (levels of opposite parity, J changes by at most 1) and the lab's (l changes by 1, j by at most 1) allow exactly the same jumps between levels n <= 6 (505 pairs; for one electron they are the same rule)", pairs == 505 && differ == 0);
        // the reasons name the rule that fails
        auto why = [](int Z, int nu, int lu, int ju, int nl, int ll, int jl) { std::string out; for (const Jump& x : jumpsBetween(Z, nu, lu, nl, ll)) if (x.upper.j2 == ju && x.lower.j2 == jl) for (const std::string& w : x.why) out += w + " | "; return out; };
        auto has = [](const std::string& x, const char* w) { return x.find(w) != std::string::npos; };
        const std::string ss = why(1, 2, 0, 1, 1, 0, 1), dp = why(1, 3, 2, 5, 2, 1, 1), sd = why(1, 3, 2, 3, 1, 0, 1);
        checkTrue("reasons: 2s -> 1s (l and parity unchanged) names the parity rule and the l rule and not the j rule", has(ss, "parity") && has(ss, "l") && !has(ss, "j changes"));
        checkTrue("reasons: 3d5/2 -> 2p1/2 (parity changes, l changes by 1, but j changes by 2) names ONLY the j rule", has(dp, "j") && !has(dp, "parity") && !has(dp, "l must"));
        checkTrue("reasons: 3d3/2 -> 1s (l changes by 2, parity unchanged) names the parity rule and the l rule", has(sd, "parity") && has(sd, "l"));
    }
    // ---- 5. branching ratios: which states have a complete set from ASD ----
    {
        int statesAll = 0, completeAll = 0, mismatch = 0, sumBad = 0, tauBad = 0, hComplete = 0, heComplete = 0, feComplete = 0, hStates = 0;
        double worstSum = 0, worstTau = 0;
        for (int Z = 1; Z <= 110; Z++) {
            if (asdIon(Z).status != AsdStatus::Lines) continue;
            const std::vector<AsdE1> lines = asdAllLines(Z);
            for (const Level& up : allLevelsUpTo(6)) {
                if (up.n < 2) continue;
                // the test's own scan: every level below (lower n, opposite parity, J within 1) must have an E1 line with an A value
                int need = 0, have = 0; double sum = 0;
                for (const Level& lo : allLevelsUpTo(up.n - 1)) {
                    if (!asdRuleAllows(lo, up)) continue;
                    need++;
                    for (const AsdE1& e : lines) if (e.type.empty() && e.lo.n == lo.n && e.lo.l == lo.l && e.lo.j2 == lo.j2 && e.hi.n == up.n && e.hi.l == up.l && e.hi.j2 == up.j2 && !std::isnan(e.A)) { have++; sum += e.A; break; }
                }
                const bool complete = need > 0 && have == need;
                const Life L = lifeOf(Z, up);
                statesAll++; completeAll += complete;
                if (Z == 1) { hStates++; hComplete += complete; } if (Z == 2) heComplete += complete; if (Z == 26) feComplete += complete;
                mismatch += (L.complete != complete) || (int(L.ways.size()) != need) || (L.withA != have);
                if (complete) {
                    double shares = 0; for (const Way& w : L.ways) shares += w.A / L.sumA;
                    worstSum = std::max(worstSum, std::fabs(shares - 1)); sumBad += std::fabs(shares - 1) > 1e-15;
                    worstTau = std::max(worstTau, std::fabs(L.tauS * sum - 1)); tauBad += std::fabs(L.tauS * sum - 1) > 1e-12;
                }
            }
        }
        std::printf("branching ratios: %d states (n = 2..6, every l and j) of 110 ions; ASD has the complete set of A for %d of them (H I %d of %d, He II %d, Fe XXVI %d); the shares sum to 1 within %.1e; tau * (sum A) = 1 within %.1e\n", statesAll, completeAll, hComplete, hStates, heComplete, feComplete, worstSum, worstTau);
        checkTrue("lifeOf agrees with the test's own scan of the ASD lines for every state n = 2..6 of every ion (number of ways, ways with an A value, complete or not)", mismatch == 0);
        // WHICH states have a complete set (measured): a state (n, l, j) of an ion has one when ASD gives an A for every allowed jump to a lower n. Of the 3,850 states n = 2..6 of the 110 ions, 1,531 do. Hydrogen: 34 of the 35
        // (all but 2s, which has no allowed way down at all); He II 34; Fe XXVI 26. ASD has A for every way down of every state with n <= 3 in most ions and is patchy above (n = 6: A for a fifth of the ways).
        checkTrue("the branching ratios of every state with a complete ASD set add up to 1 (to 1e-15) and tau = 1 / sum A (to 1e-12), for all 1,531 of them (the set is defined above: ASD has an A for every allowed jump to a lower n)", sumBad == 0 && tauBad == 0 && completeAll == 1531);
        checkTrue("hydrogen: ASD has a complete set for 34 of the 35 states n = 2..6 (every l and j); the 35th is 2s, which has no allowed way down", hStates == 35 && hComplete == 34 && !lifeOf(1, {2, 0, 1}).complete && lifeOf(1, {2, 0, 1}).ways.empty());
    }
    // ---- 6. A scales as Z^4 for hydrogen-like ions: from ASD's own numbers ----
    {
        struct T { Level lo, hi; const char* name; } trs[3] = {{{1, 0, 1}, {2, 1, 3}, "2p3/2 -> 1s"}, {{1, 0, 1}, {2, 1, 1}, "2p1/2 -> 1s"}, {{2, 1, 3}, {3, 2, 5}, "3d5/2 -> 2p3/2"}};
        double lowDev[3] = {0, 0, 0}, relMax[3] = {0, 0, 0}; int cnt[3] = {0, 0, 0};
        for (int k = 0; k < 3; k++) {
            double A1 = 0; for (const AsdE1& e : asdAllLines(1)) if (e.type.empty() && e.lo.n == trs[k].lo.n && e.lo.l == trs[k].lo.l && e.lo.j2 == trs[k].lo.j2 && e.hi.n == trs[k].hi.n && e.hi.l == trs[k].hi.l && e.hi.j2 == trs[k].hi.j2) A1 = e.A;
            for (int Z = 2; Z <= 110; Z++) for (const AsdE1& e : asdAllLines(Z)) {
                if (!e.type.empty() || std::isnan(e.A) || e.lo.n != trs[k].lo.n || e.lo.l != trs[k].lo.l || e.lo.j2 != trs[k].lo.j2 || e.hi.n != trs[k].hi.n || e.hi.l != trs[k].hi.l || e.hi.j2 != trs[k].hi.j2) continue;
                const double r = e.A / (std::pow(double(Z), 4) * A1), x2 = std::pow(Z * nist::alpha, 2);
                cnt[k]++;
                if (Z <= 10) lowDev[k] = std::max(lowDev[k], std::fabs(r - 1));
                if (Z >= 10) relMax[k] = std::max(relMax[k], std::fabs(r - 1) / x2);
            }
            std::printf("A scaling, %s: A(Z) / (Z^4 A(H)) over %d ions: worst |r - 1| for Z <= 10 is %.3e; worst |r - 1| / (Z alpha)^2 for Z >= 10 is %.4f\n", trs[k].name, cnt[k], lowDev[k], relMax[k]);
        }
        checkTrue("A of the same transition scales as Z^4 across the ASD ions (Z = 2..110, 3 transitions: 109, 109 and 104 ions): for Z <= 10 within GUARD_Z4_LOW of Z^4 times hydrogen's A (measured worst + 1 %)", cnt[0] >= 100 && cnt[1] >= 100 && cnt[2] >= 100 && lowDev[0] < GUARD_Z4_LOW[0] && lowDev[1] < GUARD_Z4_LOW[1] && lowDev[2] < GUARD_Z4_LOW[2]);
        checkTrue("and for Z >= 10 the departure from Z^4 is a relativistic correction of the order (Z alpha)^2: |r - 1| / (Z alpha)^2 stays below GUARD_Z4_REL (measured worst + 1 %) for all three transitions", relMax[0] < GUARD_Z4_REL[0] && relMax[1] < GUARD_Z4_REL[1] && relMax[2] < GUARD_Z4_REL[2]);
    }
    // ---- 7. the words ----
    {
        auto has = [](const std::string& x, const char* w) { return x.find(w) != std::string::npos; };
        checkTrue("lifetime units are chosen automatically: 1.5963e-9 s = 1.596 ns, 2.2e-12 s in ps, 3.3e-7 s in ns, 4.5e-5 s in microseconds, 2.0e-2 s in ms, 12 s in s",
                  lifeText(1.5963e-9) == "1.596 ns" && has(lifeText(2.2e-12), " ps") && has(lifeText(3.3e-7), " ns") && has(lifeText(4.5e-5), " µs") && has(lifeText(2.0e-2), " ms") && lifeText(12) == "12 s");
        const std::string e = lifeExplain();
        checkTrue("the app explains what a lifetime is in plain words: the average time before the state jumps down by itself, the inverse of the sum of the rates, A is the rate (chance per second) of one jump, the branching ratio is the share of all jumps",
                  has(e, "average time") && has(e, "by itself") && has(e, "sum") && has(e, "per second") && has(e, "share") && has(e, "NIST"));
        checkTrue("accuracy codes carry ASD's meaning (read from its help page 2026-10-04): AAA = 0.3 %, AA = 1 %, B+ = 7 %, E = more than 50 %, a prime = a split multiplet, none = no rating given",
                  has(accMeaning("AAA"), "0.3 %") && has(accMeaning("AA"), "1 %") && has(accMeaning("B+"), "7 %") && has(accMeaning("E"), "50 %") && has(accMeaning("A'"), "multiplet") && has(accMeaning(""), "no rating"));
        const Life x = lifeOf(1, {2, 1, 3}); const std::string line = lifeLine(1, x);
        checkTrue("the state line gives the level, the lifetime in ns, ASD's accuracy and says it is from ASD (2p3/2 of hydrogen: 1.596 ns, AAA)", has(line, "2p 3/2") && has(line, "1.596 ns") && has(line, "AAA") && has(line, "NIST"));
        const std::vector<Jump> ok = jumpsBetween(1, 2, 1, 1, 0); const std::string jl = jumpLine(ok.front());
        checkTrue("the jump line gives A, ASD's accuracy and the branching share (2p3/2 -> 1s: 100 %)", ok.size() == 2 && has(jl, "6.265×10⁸") && has(jl, "AAA") && has(jl, "100 %"));
        // a transition ASD gives no A for, where the lab offers it: said on screen
        int noA = 0; std::string sample;
        for (int Z = 1; Z <= 110; Z++) for (const Level& up : allLevelsUpTo(6)) for (const Jump& j : jumpsBetween(Z, up.n, up.l, 1, 0)) if (j.allowed && !j.haveA && j.ionData) { if (!noA) sample = jumpLine(j); noA++; }
        std::printf("allowed jumps to 1s for which ASD has no A (all ions, n <= 6): %d; the lab says: %s\n", noA, sample.c_str());
        checkTrue("an allowed jump ASD has no A for says so on screen and shows no number", noA == 0 || (has(sample, "no transition probability") && !has(sample, "s⁻¹")));
    }
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------------------------
// 0.10.9 stage 1: every nucleus at its measured size (measured rms charge radii, IAEA Nuclear Data Section, data/radii).
// REFERENCES here are NOT the lab's own output: (1) numbers typed from data/radii/charge_radii.csv, the file the fetch wrote (fetched 2026-10-04; counted with a separate script, not with the lab); (2) hand
// arithmetic with the stated relation (a uniform ball of radius R has rms radius sqrt(3/5) R, so R = sqrt(5/3) x rms: 2.1631 fm for helium-4 is 1.2910 x 1.6755); (3) the Woods-Saxon density's mean square
// radius by the test's own Simpson integral (not the lab's routine) and the textbook closed form 3/5 R^2 + 7/5 pi^2 a^2; (4) the statistics of the nucleon positions the lab actually draws.
struct RadRef { int Z, N; double rms, unc; };
static const RadRef kRadRef[] = {                                                         // typed from the CSV: z, n, radius_val, radius_unc
    {1, 0, 0.8783, 0.0086}, {1, 1, 2.1421, 0.0088}, {1, 2, 1.7591, 0.0363}, {2, 1, 1.9661, 0.0030}, {2, 2, 1.6755, 0.0028}, {2, 4, 2.0660, 0.0111}, {2, 6, 1.9239, 0.0306},
    {3, 3, 2.5890, 0.0390}, {3, 4, 2.4440, 0.0420}, {3, 5, 2.3390, 0.0440}, {3, 6, 2.2450, 0.0460}, {3, 8, 2.4820, 0.0430}, {6, 6, 2.4702, 0.0022}, {8, 8, 2.6991, 0.0052},
    {14, 14, 3.1224, 0.0024}, {20, 20, 3.4776, 0.0019}, {20, 28, 3.4771, 0.0020}, {26, 30, 3.7377, 0.0016}, {50, 82, 4.7093, 0.0076}, {82, 126, 5.5012, 0.0013}, {92, 146, 5.8571, 0.0033},
    {95, 148, 5.9048, 0.0035}, {19, 28, 3.4534, 0.0138}, {4, 6, 2.3550, 0.0170}};
// rows per element Z = 1 .. 96 that have a radius_val, counted from the CSV (z = 0, the neutron, left out): 90 elements, 908 isotopes
static const int kRadPerElement[96] = {3, 4, 5, 4, 2, 3, 2, 3, 1, 11, 12, 3, 1, 3, 1, 3, 2, 14, 10, 11, 5, 7, 1, 4, 7, 4, 1, 5, 2, 5, 2, 5, 1, 6, 2, 24, 23, 24, 16, 14, 7, 15, 0, 7, 1, 6, 6, 18,
                                       24, 25, 2, 13, 1, 24, 29, 28, 4, 7, 1, 16, 0, 17, 23, 11, 11, 18, 14, 12, 19, 24, 19, 12, 1, 5, 2, 7, 10, 20, 17, 26, 19, 32, 11, 15, 0, 15, 16, 19, 0, 5, 0, 5, 0, 6, 2, 5};
// the mean square radius of the Woods-Saxon density 1 / (1 + exp((r - R) / a)): the test's own Simpson integral of r^4 f and r^2 f (20,000 intervals out to R + 40 a)
static double wsMeanSquareOwn(double R, double a) {
    const int n = 20000;
    const double top = R + 40 * a, h = top / n;
    double num = 0, den = 0;
    for (int i = 0; i <= n; i++) {
        const double r = i * h, f = 1 / (1 + std::exp((r - R) / a)), w = (i == 0 || i == n) ? 1 : (i % 2 ? 4 : 2);
        num += w * r * r * r * r * f; den += w * r * r * f;
    }
    return num / den;
}
// what the lab really draws: the rms charge radius of one nucleus' protons, averaged over many builds. Positions are about the centre of mass (the nucleus is centred on it); a charge radius is the proton
// positions' rms there plus the proton's own size (r_p^2 = 0.84^2 fm^2, the lab's nucleon radius). minSep = the smallest distance between two nucleons in any build, in fm.
struct LayoutStat { double chargeRms, minSep; };
static LayoutStat layoutStat(int Z, int N, int builds, bool byRule, uint32_t seed) {
    double sum = 0, minSep = 1e9; long np = 0;
    seedRandom(seed);
    for (int b = 0; b < builds; b++) {
        const std::vector<Nucleon> v = byRule ? buildNucleusByRule(Z, N) : buildNucleus(Z, N);
        double cx = 0, cy = 0, cz = 0;
        for (const Nucleon& q : v) { cx += q.x; cy += q.y; cz += q.z; }
        cx /= v.size(); cy /= v.size(); cz /= v.size();
        for (std::size_t i = 0; i < v.size(); i++) {
            if (v[i].isP) { const double x = (v[i].x - cx) / kFmLab, y = (v[i].y - cy) / kFmLab, z = (v[i].z - cz) / kFmLab; sum += x * x + y * y + z * z; np++; }
            for (std::size_t j = i + 1; j < v.size(); j++) {
                const double dx = (v[i].x - v[j].x) / kFmLab, dy = (v[i].y - v[j].y) / kFmLab, dz = (v[i].z - v[j].z) / kFmLab;
                minSep = std::min(minSep, std::sqrt(dx * dx + dy * dy + dz * dz));
            }
        }
    }
    return {std::sqrt(sum / np + kNucleonR * kNucleonR), minSep};
}
static std::string joinLines(const std::vector<std::string>& v) { std::string s; for (const std::string& l : v) s += l + "\n"; return s; }
static bool has(const std::string& s, const std::string& w) { return s.find(w) != std::string::npos; }

static void testNuclearSize(double guardLayout, double guardMinSepFloor) {
    // ---- 1. the data parses; counts as fetched ----
    int n = 0;
    const RadiusRow* rows = radiusRows(n);
    int withValue = 0, prelimOnly = 0, both = 0, perElement[119] = {0}, badOrder = 0, badSane = 0, badUnc = 0, zeroZ = 0, badA = 0;
    double lo = 1e9, hi = -1e9, uhi = 0;
    for (int i = 0; i < n; i++) {
        const RadiusRow& r = rows[i];
        if (r.Z < 1) zeroZ++;
        if (r.N < 0) badA++;
        if (i > 0 && !(rows[i - 1].Z < r.Z || (rows[i - 1].Z == r.Z && rows[i - 1].N < r.N))) badOrder++;
        const bool v = !std::isnan(r.rms), p = !std::isnan(r.prelim);
        if (v) { withValue++; perElement[r.Z]++; lo = std::min(lo, r.rms); hi = std::max(hi, r.rms); uhi = std::max(uhi, r.unc); if (!(r.rms > 0) || !(r.unc > 0)) badUnc++; }
        else if (p) prelimOnly++;
        if (v && p) both++;
        if (!v && !p) badSane++;
    }
    checkTrue("radii: 956 rows (the CSV's 957 minus the neutron row z = 0): 908 with a value, 48 preliminary only, 99 with both", n == 956 && withValue == 908 && prelimOnly == 48 && both == 99);
    checkTrue("radii: no row for z = 0 (the neutron's negative number is a mean square in fm^2, not a radius), none with N < 0, sorted and unique, every row has a value or a preliminary one", zeroZ == 0 && badA == 0 && badOrder == 0 && badSane == 0 && chargeRadius(0, 1) == nullptr);
    int bad = 0;
    for (int z = 1; z <= 96; z++) if (perElement[z] != kRadPerElement[z - 1]) bad++;
    checkTrue("radii: the number of isotopes with a value per element Z = 1 .. 96 equals the count made from the CSV (90 elements)", bad == 0);
    int elements = 0; for (int z = 1; z <= 118; z++) if (perElement[z]) elements++;
    checkTrue("radii: 90 elements have a value; none above Z = 96", elements == 90 && perElement[97] + perElement[98] + perElement[110] + perElement[118] == 0);
    // every value in a sane range: the CSV's smallest is 0.8783 (hydrogen), the largest 5.9048 (Am-243); guard = those + 1 % (labelled)
    checkTrue("radii: every value in [0.8783 x 0.99, 5.9048 x 1.01] fm (the CSV's extremes -/+ 1 %: guard), every uncertainty > 0 and at most 0.3139 x 1.01 fm", lo >= 0.8783 * 0.99 && hi <= 5.9048 * 1.01 && badUnc == 0 && uhi <= 0.3139 * 1.01);
    std::printf("radii: %d rows, %d with a value (%.4f .. %.4f fm, uncertainty up to %.4f), %d preliminary only\n", n, withValue, lo, hi, uhi, prelimOnly);
    // ---- 2. spot values, typed from the CSV ----
    for (const RadRef& r : kRadRef) {
        const RadiusRow* row = chargeRadius(r.Z, r.N);
        const std::string w = "radii " + at(r.Z, r.N);
        checkTrue(w + " is in the table", row != nullptr);
        if (!row) continue;
        checkNum(w + " rms", row->rms, r.rms, 1e-12);
        checkNum(w + " uncertainty", row->unc, r.unc, 1e-9);
    }
    const RadiusRow* k47 = chargeRadius(19, 28); const RadiusRow* be12 = chargeRadius(4, 8); const RadiusRow* li10 = chargeRadius(3, 7);
    checkTrue("radii: K-47 has a value 3.4534 and a preliminary 3.4419 with uncertainty 0.0000 (as the CSV has it); Be-12 has ONLY a preliminary 2.5030 +- 0.0150; Li-10 has no row",
              k47 && k47->prelim == 3.4419 && k47->prelimUnc == 0 && be12 && std::isnan(be12->rms) && be12->prelim == 2.5030 && be12->prelimUnc == 0.0150 && li10 == nullptr);
    // ---- 3. the size drawn: R = sqrt(5/3) x the measured rms where there is a value (a uniform ball), the usual rule where there is none; the preliminary column and H-1 are not used ----
    checkNum("a uniform ball has rms radius sqrt(3/5) R: sqrt(5/3) = 1.2909944487", kSharpOverRms, 1.2909944487358056, 1e-15);
    checkNum("helium-4: drawn radius = 1.2910 x 1.6755 = 2.1631 fm (hand arithmetic)", nuclearRadius(2, 2), 2.1631, 3e-5);
    checkNum("lead-208: drawn radius = 1.2910 x 5.5012 = 7.1020 fm (hand arithmetic)", nuclearRadius(82, 126), 7.1020, 3e-5);
    checkNum("lead-208: the usual rule 1.2 x 208^(1/3) = 7.1100 fm, which is an rms charge radius of 5.5074 fm (hand arithmetic)", ruleRms(208), 5.5074, 2e-5);
    for (const RadRef& r : kRadRef) {
        if (r.Z + r.N == 1) continue;
        checkNum("drawn radius of " + at(r.Z, r.N) + " = sqrt(5/3) x measured", nuclearRadius(r.Z, r.N), 1.2909944487358056 * r.rms, 1e-12);
    }
    checkNum("K-47: the value is used, not the preliminary one", nuclearRadius(19, 28), 1.2909944487358056 * 3.4534, 1e-12);
    checkNum("Be-12 (only a preliminary value): the usual rule", nuclearRadius(4, 8), nuclearR(12), 1e-14);
    checkNum("Li-10 (no row): the usual rule", nuclearRadius(3, 7), nuclearR(10), 1e-14);
    checkNum("Og-294 (no row): the usual rule", nuclearRadius(118, 176), nuclearR(294), 1e-14);
    checkNum("H-1: the proton keeps the lab's own proton radius (CODATA 2022 0.84075 fm, drawn 0.84), not the table's 0.8783", nuclearRadius(1, 0), kNucleonR, 0);
    checkTrue("sizedByMeasurement: yes for He-4 and Pb-208, no for H-1, Be-12 (preliminary only), Li-10, Og-294", sizedByMeasurement(2, 2) && sizedByMeasurement(82, 126) && !sizedByMeasurement(1, 0) && !sizedByMeasurement(4, 8) && !sizedByMeasurement(3, 7) && !sizedByMeasurement(118, 176));
    // the usual rule against the measurements (rule rms = sqrt(3/5) x 1.2 A^(1/3)); the percentages are hand arithmetic from the CSV values
    auto dev = [](int Z, int N) { const double m = chargeRadius(Z, N)->rms, r = ruleRms(Z + N); return 100 * (m - r) / r; };
    checkNum("deviation from the rule: H-2 +82.91 %", dev(1, 1), 82.91, 0, 0.01);
    checkNum("deviation from the rule: Li-6 +53.28 %", dev(3, 3), 53.28, 0, 0.01);
    checkNum("deviation from the rule: He-3 +46.66 %", dev(2, 1), 46.66, 0, 0.01);
    checkNum("deviation from the rule: C-12 +16.08 %", dev(6, 6), 16.08, 0, 0.01);
    checkNum("deviation from the rule: Fe-56 +5.10 %", dev(26, 30), 5.10, 0, 0.01);
    checkNum("deviation from the rule: U-238 +1.68 %", dev(92, 146), 1.68, 0, 0.01);
    checkNum("deviation from the rule: Pb-208 -0.11 %", dev(82, 126), -0.11, 0, 0.01);
    checkNum("deviation from the rule: Sn-132 -0.50 %", dev(50, 82), -0.50, 0, 0.01);
    double worstUp = -1e9, worstDown = 1e9; int upZ = 0, upN = 0, dnZ = 0, dnN = 0;
    for (int i = 0; i < n; i++) {
        if (std::isnan(rows[i].rms) || rows[i].Z + rows[i].N == 1) continue;
        const double d = dev(rows[i].Z, rows[i].N);
        if (d > worstUp) { worstUp = d; upZ = rows[i].Z; upN = rows[i].N; }
        if (d < worstDown) { worstDown = d; dnZ = rows[i].Z; dnN = rows[i].N; }
    }
    checkTrue("the biggest deviations from the rule are H-2 (+82.9 %) and, below it, Sn-132 (-0.50 %): the measurement is never more than 0.5 % under the rule", upZ == 1 && upN == 1 && dnZ == 50 && dnN == 82 && worstDown > -0.51);
    // known cases, from the data itself
    checkTrue("He-4 (1.6755) is smaller than He-3 (1.9661), as measured; lithium falls from Li-6 to Li-9 (2.2450) and rises at Li-11 (2.4820)",
              chargeRadius(2, 2)->rms < chargeRadius(2, 1)->rms && chargeRadius(3, 3)->rms > chargeRadius(3, 4)->rms && chargeRadius(3, 4)->rms > chargeRadius(3, 5)->rms &&
              chargeRadius(3, 5)->rms > chargeRadius(3, 6)->rms && chargeRadius(3, 8)->rms > chargeRadius(3, 6)->rms);
    checkTrue("Li-11 against Li-7: +0.0380 fm, with an uncertainty of sqrt(0.043^2 + 0.042^2) = 0.0601 fm: NOT distinguishable by charge radius (the app must not call it much larger)",
              std::fabs((chargeRadius(3, 8)->rms - chargeRadius(3, 4)->rms) - 0.038) < 1e-9 && chargeRadius(3, 8)->rms - chargeRadius(3, 4)->rms < std::hypot(chargeRadius(3, 8)->unc, chargeRadius(3, 4)->unc));
    checkTrue("a strongly deformed rare earth, Gd-158, is bigger than the rule by a few percent (+0.6 %: the charge radius carries no shape)", dev(64, 94) > 0 && dev(64, 94) < 5);
    // the density the drawn size implies (a uniform ball of A nucleons): between the measured extremes -/+ 1 % (guard); the lowest is Ne-17
    double rlo = 1e9, rhi = 0; int rloZ = 0;
    for (int i = 0; i < n; i++) {
        if (std::isnan(rows[i].rms) || rows[i].Z + rows[i].N < 12) continue;
        const int A = rows[i].Z + rows[i].N; const double R = nuclearRadius(rows[i].Z, rows[i].N), rho = 3.0 * A / (4 * kPi * R * R * R);
        if (rho < rlo) { rlo = rho; rloZ = rows[i].Z; }
        rhi = std::max(rhi, rho);
    }
    std::printf("radii: nucleon density of the drawn uniform ball, A >= 12: %.5f (Z = %d) .. %.5f per fm^3\n", rlo, rloZ, rhi);
    checkTrue("density of the drawn ball for A >= 12 is between 0.06705 x 0.99 and 0.14023 x 1.01 per fm^3 (the CSV's extremes -/+ 1 %: guard; the lowest is Ne-17)", rlo >= 0.06705 * 0.99 && rhi <= 0.14023 * 1.01 && rloZ == 10);
    // ---- 4. how many isotopes the slider offers are measured ----
    int offered = 0, measured = 0, fallback = 0, prelimOnlyOffered = 0;
    for (int z = 1; z <= 118; z++) {
        const auto rg = nRange(z);
        for (int nn = rg.first; nn <= rg.second; nn++) {
            offered++;
            const RadiusRow* r = chargeRadius(z, nn);
            if (sizedByMeasurement(z, nn)) measured++; else { fallback++; if (r && std::isnan(r->rms)) prelimOnlyOffered++; }
        }
    }
    std::printf("radii: the slider offers %d isotopes: %d sized by a measurement, %d by the rule (%d of those have only a preliminary value)\n", offered, measured, fallback, prelimOnlyOffered);
    checkTrue("the slider's isotopes: every measured isotope the table has (907 with A > 1) is offered or not, but measured + fallback = offered", measured + fallback == offered && measured > 0 && fallback > 0);
    // ---- 5. the layout the lab draws ----
    // the half-density radius that gives a measured charge radius: the lab's solver against the test's own integral. proton density rms^2 + r_p^2 = measured^2
    for (const double rms : {2.7, 3.1224, 3.7377, 5.5012, 5.8571}) {
        const double a = 0.54, R = wsHalfDensityRadiusForChargeRms(rms);
        checkNum("Woods-Saxon radius for a charge rms of " + fmt::fixed(rms, 4) + " fm: the test's own Simpson integral gives back sqrt(rms^2 - r_p^2)", std::sqrt(wsMeanSquareOwn(R, a)), std::sqrt(rms * rms - kNucleonR * kNucleonR), 1e-5);
        checkNum("... and the textbook closed form 3/5 R^2 + 7/5 pi^2 a^2 agrees (it drops exp(-R/a) terms)", 0.6 * R * R + 1.4 * kPi * kPi * a * a, rms * rms - kNucleonR * kNucleonR, 2e-3);
    }
    // the deuteron is two nucleons at +-d/2: the proton's own distance to the centre is d/2, so d/2 = sqrt(2.1421^2 - 0.84^2) = 1.9705 fm (hand arithmetic); by the rule it stays 1.6
    for (const bool rule : {false, true}) {
        seedRandom(5);
        const auto v = rule ? buildNucleusByRule(1, 1) : buildNucleus(1, 1);
        const double d = std::hypot(std::hypot(v[0].x - v[1].x, v[0].y - v[1].y), v[0].z - v[1].z) / kFmLab;
        checkNum(rule ? "deuteron by the rule: half separation 1.6 fm" : "deuteron by its measured charge radius: half separation 1.9705 fm", d / 2, rule ? 1.6 : 1.9705, rule ? 1e-9 : 3e-5);
    }
    // unmeasured, ROUND isotopes are built exactly as the rule builds them (same seed, same positions); measured ones are not. (0.10.9 stage 2: this list had Og-294, whose estimated beta2 is 0.206, and passed with seed 9 by
    // luck; a deformed nucleus is now drawn with the volume-keeping surface of quadrupole.h, so its layout differs on purpose: Og-294 moved to the check below, which needs it to differ for every one of 4 seeds.)
    for (const auto& zn : {std::pair<int, int>{3, 7}, {4, 8}, {1, 0}, {50, 40}}) {
        checkTrue("no measured radius and no deformation (beta2 = 0): " + at(zn.first, zn.second), nucBeta(zn.first, zn.second) == 0 && !sizedByMeasurement(zn.first, zn.second));
        seedRandom(9); const auto a = buildNucleus(zn.first, zn.second); seedRandom(9); const auto b = buildNucleusByRule(zn.first, zn.second);
        bool same = a.size() == b.size();
        for (std::size_t i = 0; same && i < a.size(); i++) same = a[i].x == b[i].x && a[i].y == b[i].y && a[i].z == b[i].z && a[i].isP == b[i].isP;
        checkTrue("no measurement: buildNucleus = the rule's layout, position for position: " + at(zn.first, zn.second), same);
    }
    {
        int differs = 0;
        for (uint32_t sd = 1; sd <= 4; sd++) {
            seedRandom(sd); const auto a = buildNucleus(118, 176); seedRandom(sd); const auto b = buildNucleusByRule(118, 176);
            bool same = a.size() == b.size();
            for (std::size_t i = 0; same && i < a.size(); i++) same = a[i].x == b[i].x;
            differs += !same;
        }
        checkTrue("no measured radius but a deformed nucleus (Og-294, estimated beta2 = 0.206): the layout differs from the web's rule layout for all 4 seeds (the volume is kept now)", nucBeta(118, 176) > 0.2 && differs == 4);
    }
    {
        seedRandom(9); const auto a = buildNucleus(2, 1); seedRandom(9); const auto b = buildNucleusByRule(2, 1);
        checkTrue("a measurement: He-3 is built differently from the rule", a.size() == b.size() && (a[0].x != b[0].x || a[0].y != b[0].y || a[0].z != b[0].z));
    }
    // the protons' rms charge radius of what is drawn against the measured value. buildNucleus scales every snapshot so that this is exact (see atom_core.cpp), so the guard is rounding-level, NOT a statistical one
    // (the test averages over many builds anyway). The same check on the rule's layout must NOT hold: it is the check that tells the two apart. minSep: the closest two nucleons in any build.
    struct Lay { int Z, N, builds; };
    const Lay lays[] = {{2, 1, 400}, {2, 2, 400}, {3, 3, 300}, {4, 5, 300}, {6, 6, 200}, {8, 8, 150}, {20, 20, 60}, {26, 30, 40}, {82, 126, 20}};
    double worst = 0, worstRule = 0, lowSep = 1e9; std::string wl;
    for (const Lay& L : lays) {
        const double m = chargeRadius(L.Z, L.N)->rms;
        const LayoutStat st = layoutStat(L.Z, L.N, L.builds, false, 20260 + L.Z), sr = layoutStat(L.Z, L.N, L.builds, true, 20260 + L.Z);
        const double d = std::fabs(st.chargeRms / m - 1), dr = std::fabs(sr.chargeRms / m - 1);
        std::printf("layout %-8s beta2 %+.3f measured %.4f fm: drawn %.4f (%+.2e), by the rule %.4f (%+.2f %%), closest pair %.3f fm (rule %.3f)\n", at(L.Z, L.N).c_str(), nucBeta(L.Z, L.N), m, st.chargeRms, st.chargeRms / m - 1, sr.chargeRms, 100 * (sr.chargeRms / m - 1), st.minSep, sr.minSep);
        if (d > worst) { worst = d; wl = at(L.Z, L.N); }
        worstRule = std::max(worstRule, dr);
        lowSep = std::min(lowSep, st.minSep);
    }
    std::printf("layout: worst difference of the drawn rms charge radius from the measured one: %.3e (%s); the rule's layout was off by up to %.4f; closest pair of any build %.4f fm\n", worst, wl.c_str(), worstRule, lowSep);
    checkTrue("the drawn protons' rms charge radius equals the measured value for 9 nuclei, deformed or not (exact by construction: guard 1e-9)", worst <= guardLayout);
    checkTrue("... and the rule's layout does NOT (off by more than 5 % for some of them): the check can tell the two apart", worstRule > 0.05);
    checkTrue("no two nucleons closer than the guard (what the density rule guarantees: hard core 1.0 fm, relaxed to 0.55 fm only when crowded, then the snapshot is scaled by about 1: so never below 0.55 x the scale)", lowSep >= guardMinSepFloor);
    // the lab itself: the size it frames, zooms to and uses in its texts
    for (const auto& zn : {std::pair<int, int>{2, 2}, {82, 126}, {1, 0}, {4, 8}, {118, 176}}) {
        AtomLab lab; lab.p.Z = zn.first; lab.p.N = zn.second;
        checkNum("the lab's nucleus radius (Bohr radii x fm per Bohr radius) = nuclearRadius: " + at(zn.first, zn.second), lab.nucRA0() * kFmPerA0, nuclearRadius(zn.first, zn.second), 1e-12);
    }
    // ---- 6. the words ----
    const std::string he4 = joinLines(nuclearSizeLines(2, 2)), li9 = joinLines(nuclearSizeLines(3, 6)), c12 = joinLines(nuclearSizeLines(6, 6)), be12t = joinLines(nuclearSizeLines(4, 8)), h1 = joinLines(nuclearSizeLines(1, 0)), og = joinLines(nuclearSizeLines(118, 176));
    checkTrue("card, He-4: the measured 1.6755 +- 0.0028 fm, the drawn 2.1631 fm, the rule's 1.9049 fm and its rms 1.4755 fm, the difference +0.2000 fm (+13.6 %)",
              has(he4, "1.6755") && has(he4, "0.0028") && has(he4, "2.1631") && has(he4, "1.9049") && has(he4, "1.4755") && has(he4, "+0.2000") && has(he4, "13.6 %"));
    checkTrue("card, He-4: says helium-4 is smaller than helium-3 (1.9661 fm) and gives the binding per nucleon (AME2020) of both", has(he4, "smaller than helium-3") && has(he4, "1.9661") && has(he4, "MeV per nucleon"));
    checkTrue("card, Li-9: lists the measured lithium radii falling to Li-9 and rising at Li-11, says Li-10 has none, and that Li-11 against Li-7 is inside the uncertainty",
              has(li9, "2.5890") && has(li9, "2.2450") && has(li9, "2.4820") && has(li9, "Li-10") && has(li9, "no measured") && has(li9, "uncertainty") && !has(li9, "much larger"));
    checkTrue("card, C-12: no He/Li series text", !has(c12, "lithium") && !has(c12, "smaller than helium-3"));
    checkTrue("card, an isotope with no value: \"no measured radius for this isotope; size from the usual rule\" and the rule's radius, Og-294: 1.2 x 294^(1/3) = 7.9793 fm",
              has(og, "no measured radius for this isotope; size from the usual rule") && has(og, "7.9793"));
    checkTrue("card, Be-12 (only preliminary): says so, gives 2.5030 +- 0.0150 and that it is not used", has(be12t, "preliminary") && has(be12t, "2.5030") && has(be12t, "0.0150") && has(be12t, "not used") && has(be12t, "no measured radius for this isotope; size from the usual rule"));
    checkTrue("card, H-1: one proton, 0.84 fm, says the table's 0.8783 +- 0.0086 is not the CODATA 2022 proton radius 0.84075 and is not used", has(h1, "0.84") && has(h1, "0.8783") && has(h1, "CODATA 2022") && has(h1, "not used"));
    const std::string ex = chargeRadiusExplain();
    checkTrue("explanation: what a charge radius is (the size of the protons' charge, rms), neutrons carry no charge, matter extends further, which is why a halo cannot be drawn from this table, a size not a shape, the uniform-ball assumption, static, no vibration or rotation",
              has(ex, "charge radius") && has(ex, "rms") && has(ex, "protons") && has(ex, "Neutrons carry no electric charge") && has(ex, "matter") && has(ex, "halo") && has(ex, "cannot be drawn") && has(ex, "a size, not a shape") &&
              has(ex, "uniform ball") && has(ex, "sqrt(5/3)") && has(ex, "vibrate and rotate"));
    checkTrue("the credit names IAEA, Angeli and Marinova and the file's own limits", has(kRadiiCredit, "IAEA") && has(kRadiiCredit, "Angeli") && has(kRadiiCredit, "Marinova") && has(kRadiiCredit, "charge_radii.csv"));
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------------------------
// 0.10.9 stage 2: the nucleus's shape from measured electric quadrupole moments (IAEA recommended values, data/quadrupole).
// REFERENCES are NOT the lab's own output: (1) numbers typed from data/quadrupole/elec_mom_recomm.csv (fetched 2026-10-04; counted with a separate script) and data/radii/charge_radii.csv; (2) hand arithmetic of the
// textbook relation between the spectroscopic and the intrinsic moment for strong coupling K = I, Q = Q0 I (2I - 1) / ((I + 1) (2I + 3)), so Q0 = Q x 10, 5, 7/2, 14/5, 15/7, 11/6 for I = 1, 3/2, 2, 5/2, 7/2, 9/2, and
// the first-order Q0 = 3 Z R^2 beta / sqrt(5 pi); (3) a SEPARATE Python implementation (plain Simpson integration over the angle, not the lab's closed forms) for the exact shape parameters typed below;
// (4) the test's own integrals of the drawn surface.
struct QuadRef { int Z, A, spin2; char kind; double Qb, uncB; const char* method; const char* nsr; int stable; };
static const QuadRef kQuadRef[] = {                                                      // typed from the CSV: z, column 2 (A), spin, value, uncertainty, method, nsr, half-life
    {1, 2, 2, 'S', +0.0028578, 3e-7, "MB,R", "1958Qu02", 1}, {3, 6, 2, 'S', -0.000806, 6e-6, "MB", "2005Bo45", 1}, {3, 7, 3, 'S', -0.0400, 3e-4, "MB", "2008Py02", 1},
    {5, 11, 3, 'S', +0.04059, 1e-4, "AB", "", 1}, {11, 23, 3, 'S', +0.104, 1e-3, "O", "", 1}, {13, 27, 5, 'S', +0.1466, 1e-3, "AB", "", 1}, {19, 39, 3, 'S', +0.603, 6e-3, "AB", "", 1},
    {19, 40, 8, 'S', -0.750, 8e-3, "AB", "", 0}, {27, 59, 7, 'S', +0.42, 3e-2, "AB", "", 1}, {55, 124, 2, 'S', -0.69, 4e-2, "ABLS", "", 0}, {55, 126, 2, 'S', -0.64, 3e-2, "ABLS", "", 0},
    {62, 147, 7, 'S', -0.26, 3e-2, "Mu-X", "", 0}, {63, 153, 5, 'S', +2.41, 2e-2, "Mu-X", "", 1}, {67, 165, 7, 'S', +3.58, 2e-2, "Pi-X", "", 1}, {68, 167, 7, 'S', +3.57, 3e-2, "Mu-X", "", 1},
    {71, 175, 7, 'S', +3.49, 2e-2, "Mu-X", "", 1}, {83, 209, 9, 'S', -0.516, 1.5e-2, "AB", "", 0}, {92, 235, 7, 'S', +4.936, 6e-3, "Mu-X", "", 0}, {5, 12, 2, 'U', 0.0132, 3e-4, "b-NMR", "", 0},
    {3, 11, 3, 'T', -0.0333, 5e-4, "b-NMR", "2008Ne11", 0}, {19, 41, 3, 'S', +0.734, 7e-3, "AB", "2018Py01", 1}};
// ground-state rows kept per element Z = 1 .. 102 (the file's 599 minus the K-41 duplicate), counted from the CSV
static const int kQuadPerElement[102] = {1, 0, 5, 1, 8, 1, 4, 3, 4, 2, 9, 4, 7, 1, 1, 2, 3, 5, 4, 7, 6, 3, 2, 1, 13, 0, 5, 1, 21, 6, 17, 2, 4, 2, 6, 8, 20, 9, 7, 5, 7, 2, 1, 3, 0, 1, 4, 8, 20, 8, 5, 3, 6, 10,
                                         24, 8, 5, 0, 3, 7, 4, 7, 19, 2, 11, 9, 12, 9, 12, 10, 15, 4, 5, 0, 7, 4, 13, 4, 12, 5, 3, 7, 15, 8, 18, 7, 20, 6, 3, 1, 2, 2, 1, 1, 3, 0, 0, 0, 1, 0, 0, 1};
// the measured rms charge radius (radii CSV), Q0 / (Z <r^2>) with <r^2> = rms^2 - 0.84^2 (the protons' point mean square radius), the exact beta2 of the uniformly charged body that has that moment (separate Python
// implementation) and the first-order beta2 = sqrt(5 pi) Q0 / (3 Z R^2), R^2 = (5/3) <r^2> (hand formula)
struct ShapeRef { int Z, A; double rms, ratio, beta, first; };
static const ShapeRef kShapeRef[] = {{67, 165, 5.2022, 0.434410959, +0.32474953, +0.3443426}, {68, 167, 5.2560, 0.417905897, +0.31242822, +0.3312596}, {71, 175, 5.3700, 0.374429877, +0.28018744, +0.2967976},
                                      {63, 153, 5.1115, 0.421335135, +0.31498403, +0.3339778}, {92, 235, 5.8337, 0.344978011, +0.25848001, +0.2734522}, {27, 59, 3.7875, 0.244387227, +0.18464940, +0.1937173},
                                      {83, 209, 5.5211, -0.038276560, -0.03069289, -0.0303405}, {62, 147, 4.9892, -0.037153656, -0.02978184, -0.0294504}, {55, 126, 4.7872, -0.523884804, -0.57887637, -0.4152654},
                                      {13, 27, 3.0610, 0.364438408, +0.27281351, +0.2888777}, {11, 23, 2.9936, 0.572583488, +0.43068437, +0.4538672}, {64, 155, 5.1319, 0.387107540, +0.28956095, +0.3068468}};
// a symmetric 3 x 3 matrix's eigenvalue of the largest size, by Jacobi rotations
static double bigEigen(double a[3][3]) {
    for (int sweep = 0; sweep < 50; sweep++) {
        double off = 0;
        for (int p = 0; p < 3; p++) for (int q = p + 1; q < 3; q++) off += a[p][q] * a[p][q];
        if (off < 1e-24) break;
        for (int p = 0; p < 3; p++) for (int q = p + 1; q < 3; q++) {
            if (std::fabs(a[p][q]) < 1e-300) continue;
            const double th = (a[q][q] - a[p][p]) / (2 * a[p][q]), t = (th >= 0 ? 1 : -1) / (std::fabs(th) + std::sqrt(th * th + 1)), c = 1 / std::sqrt(t * t + 1), s = t * c;
            for (int k = 0; k < 3; k++) { const double akp = a[k][p], akq = a[k][q]; a[k][p] = c * akp - s * akq; a[k][q] = s * akp + c * akq; }
            for (int k = 0; k < 3; k++) { const double apk = a[p][k], aqk = a[q][k]; a[p][k] = c * apk - s * aqk; a[q][k] = s * apk + c * aqk; }
        }
    }
    double e = a[0][0];
    for (int k = 1; k < 3; k++) if (std::fabs(a[k][k]) > std::fabs(e)) e = a[k][k];
    return e;
}
// what the lab draws: the protons' traceless quadrupole tensor sum(3 x_i x_j - r^2 delta_ij) / sum(r^2) of one build (rotation does not change its eigenvalues, so the random tilt does not matter). For an axial body
// its eigenvalues are (ratio, -ratio/2, -ratio/2) with ratio = Q0 / (Z <r^2>): the one of the largest size is positive for a prolate nucleus and negative for an oblate one. Mean and spread over the builds.
struct CloudQ { double mean, sd; };
static CloudQ cloudQuadrupole(int Z, int N, int builds, uint32_t seed) {
    seedRandom(seed);
    double s1 = 0, s2 = 0;
    for (int b = 0; b < builds; b++) {
        const std::vector<Nucleon> v = buildNucleus(Z, N);
        double cx = 0, cy = 0, cz = 0;
        for (const Nucleon& q : v) { cx += q.x; cy += q.y; cz += q.z; }
        cx /= v.size(); cy /= v.size(); cz /= v.size();
        double T[3][3] = {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}}, r2s = 0;
        for (const Nucleon& q : v) {
            if (!q.isP) continue;
            const double x[3] = {(q.x - cx) / kFmLab, (q.y - cy) / kFmLab, (q.z - cz) / kFmLab}, r2 = x[0] * x[0] + x[1] * x[1] + x[2] * x[2];
            r2s += r2;
            for (int i = 0; i < 3; i++) for (int j = 0; j < 3; j++) T[i][j] += 3 * x[i] * x[j] - (i == j ? r2 : 0);
        }
        for (int i = 0; i < 3; i++) for (int j = 0; j < 3; j++) T[i][j] /= r2s;
        const double e = bigEigen(T);
        s1 += e; s2 += e * e;
    }
    const double m = s1 / builds;
    return {m, std::sqrt(std::max(0.0, s2 / builds - m * m))};
}
// the volume, the mean square radius and <3z^2 - r^2> / <r^2> of the uniform body whose surface is r(u) = R_V f(u) (u = cos theta, f = shapeRadiusFactor), by the test's own Simpson integrals over u:
// V = (2 pi / 3) integral r^3 du;  <r^2> = (3/5) integral r^5 du / integral r^3 du;  <3z^2 - r^2> / <r^2> = integral r^5 (3u^2 - 1) du / integral r^5 du
struct BodyOwn { double volume, ms, ratio; };
static BodyOwn bodyOwn(double beta, double RV) {
    const int n = 20000;
    const double h = 2.0 / n;
    double v3 = 0, v5 = 0, q5 = 0;
    for (int i = 0; i <= n; i++) {
        const double u = -1 + i * h, w = (i == 0 || i == n) ? 1 : (i % 2 ? 4 : 2), r = RV * shapeRadiusFactor(beta, u), r3 = r * r * r, r5 = r3 * r * r;
        v3 += w * r3; v5 += w * r5; q5 += w * r5 * (3 * u * u - 1);
    }
    return {2 * kPi / 3 * v3 * h / 3, 0.6 * v5 / v3, q5 / v5};
}

static void testQuadrupole(double guardMinSepDeformed) {
    // ---- 1. the data parses; counts as fetched ----
    int n = 0, fileRows = 0, isoRows = 0;
    const QuadRow* rows = quadRows(n);
    quadFileCounts(fileRows, isoRows);
    int kinds[256] = {0}, perEl[120] = {0}, badOrder = 0, asA = 0, asN = 0, stable = 0, stableS = 0, stableN = 0, spinLow = 0, evenEven = 0, firmBad = 0;
    for (int i = 0; i < n; i++) {
        const QuadRow& r = rows[i];
        kinds[(unsigned char)r.kind]++; perEl[r.Z]++;
        if (i > 0 && !(rows[i - 1].Z < r.Z || (rows[i - 1].Z == r.Z && rows[i - 1].A < r.A))) badOrder++;
        if (nucCode(r.Z, r.A - r.Z)) asA++;
        if (nucCode(r.Z, r.A)) asN++;
        if (r.stable) { stable++; const std::string* c = nucCode(r.Z, r.A - r.Z); if (c && *c == "S") stableS++; const std::string* d = nucCode(r.Z, r.A); if (d && *d == "S") stableN++; }
        if (r.kind == 'S' && r.spin2 >= 0 && r.spin2 < 2) spinLow++;
        if (r.Z % 2 == 0 && (r.A - r.Z) % 2 == 0) evenEven++;
        if (r.spinFirm && r.spin2 < 0) firmBad++;
    }
    checkTrue("quadrupole: the file has 1,155 rows: 599 ground states (598 here: the duplicate K-41 row is left out) and 556 isomer rows (not in the table)", fileRows == 1155 && isoRows == 556 && n == 598 && fileRows - isoRows == 599);
    checkTrue("quadrupole: 598 ground-state rows: 546 signed, 41 unsigned (no sign in the file), 4 tentative sign, 7 not a single clean number (counted from the CSV)", kinds['S'] == 546 && kinds['U'] == 41 && kinds['T'] == 4 && kinds['X'] == 7);
    checkTrue("quadrupole: sorted and unique by (Z, A); ground rows per element Z = 1 .. 102 as counted from the CSV", badOrder == 0 && std::equal(kQuadPerElement, kQuadPerElement + 102, perEl + 1));
    checkTrue("quadrupole: no even-even ground row and none of spin 0 or 1/2 (quantum mechanics: such a nucleus has no spectroscopic quadrupole moment)", evenEven == 0 && spinLow == 0 && firmBad == 0);
    // what the second column is: the MASS NUMBER A. Read as A, all 598 rows are nuclides of the lab's NUBASE-derived table; read as a neutron count only 53 are, and the 77 'stable' rows are the stable isotopes (76: Eu-151
    // is in the table with 1.45e26 s) only as A
    checkTrue("quadrupole: the file's second column is the mass number: (Z, A - Z) is a known nuclide for 598 of 598 rows, (Z, column) for only 53; of 77 'stable' rows 76 are stable isotopes read as A and 0 read as N",
              asA == 598 && asN == 53 && stable == 77 && stableS == 76 && stableN == 0);
    std::printf("quadrupole: %d ground rows (signed %d, unsigned %d, tentative %d, not clean %d), %d isomer rows; column read as A: %d of %d are nuclides, as N: %d; stable rows %d: %d on stable isotopes as A, %d as N\n",
                n, kinds['S'], kinds['U'], kinds['T'], kinds['X'], isoRows, asA, n, asN, stable, stableS, stableN);
    // ---- 2. spot values typed from the CSV (units: barns) ----
    for (const QuadRef& r : kQuadRef) {
        const QuadRow* q = quadRow(r.Z, r.A - r.Z);
        const std::string w = "quadrupole " + std::string(symbol(r.Z)) + "-" + std::to_string(r.A);
        checkTrue(w + " is in the table", q != nullptr);
        if (!q) continue;
        checkTrue(w + ": spin, kind, stable, method as in the CSV", q->spin2 == r.spin2 && q->kind == r.kind && q->stable == r.stable && std::string(q->method) == r.method && (!*r.nsr || std::string(q->nsr) == r.nsr));
        checkNum(w + " Q (barns)", q->Qb, r.Qb, 1e-12);
        checkNum(w + " uncertainty (barns)", q->uncB, r.uncB, 1e-9);
    }
    checkTrue("quadrupole: the value text and spin text are the file's own (Ho-165 \"+3.58(2)\" \"7/2-\"; Cu-78 spin \"(6-)\" is not firm; B-12 \"0.0132(3)\" has no sign; At-218's row is not clean)",
              std::string(quadRow(67, 98)->raw) == "+3.58(2)" && std::string(quadRow(67, 98)->spin) == "7/2-" && quadRow(29, 49)->spinFirm == 0 && std::string(quadRow(5, 7)->raw) == "0.0132(3)" && quadRow(85, 133)->kind == 'X');
    checkNum("1 barn = 100 fm^2 exactly (10^-28 m^2 over 10^-30 m^2)", kBarnFm2, 100, 0);
    // ---- 3. spectroscopic -> intrinsic, K = I (hand arithmetic) ----
    checkNum("K = I: spin 1: Q0 = 10 Q", intrinsicQ0(1.0, 2), 10, 1e-14);
    checkNum("K = I: spin 3/2: Q0 = 5 Q", intrinsicQ0(1.0, 3), 5, 1e-14);
    checkNum("K = I: spin 2: Q0 = 7/2 Q", intrinsicQ0(1.0, 4), 3.5, 1e-14);
    checkNum("K = I: spin 5/2: Q0 = 14/5 Q", intrinsicQ0(1.0, 5), 2.8, 1e-14);
    checkNum("K = I: spin 7/2: Q0 = 15/7 Q", intrinsicQ0(1.0, 7), 15.0 / 7, 1e-14);
    checkNum("K = I: spin 9/2: Q0 = 11/6 Q", intrinsicQ0(1.0, 9), 11.0 / 6, 1e-14);
    checkTrue("K = I: spin 0 and spin 1/2 have no conversion (the spectroscopic moment is exactly zero whatever the shape): not a number", std::isnan(intrinsicQ0(1.0, 0)) && std::isnan(intrinsicQ0(1.0, 1)));
    checkNum("Ho-165 (spin 7/2, Q = +3.58 b): Q0 = 3.58 x 15/7 = +7.6714 b", intrinsicQ0(3.58, 7), 7.671428571428571, 1e-14);
    checkNum("deuteron (spin 1, Q = +0.0028578 b): Q0 = +0.028578 b", intrinsicQ0(0.0028578, 2), 0.028578, 1e-14);
    checkNum("Bi-209 (spin 9/2, Q = -0.516 b): Q0 = -0.516 x 11/6 = -0.9460 b", intrinsicQ0(-0.516, 9), -0.946, 1e-14);
    // ---- 4. the shape parameter: the point-proton mean square radius, the exact beta2 for the drawn body, the first-order formula as a hand check ----
    for (const ShapeRef& r : kShapeRef) {
        const int N = r.A - r.Z;
        const std::string w = std::string(symbol(r.Z)) + "-" + std::to_string(r.A);
        const double ms = r.rms * r.rms - 0.84 * 0.84;
        checkNum(w + ": the point-proton mean square radius = measured rms^2 - 0.84^2", pointProtonMs(r.Z, N), ms, 1e-12);
        const QuadRow* q = quadRow(r.Z, N);
        const double Q0fm2 = intrinsicQ0(q->Qb, q->spin2) * kBarnFm2;
        checkNum(w + ": ratio Q0 / (Z <r^2>) (separate Python)", Q0fm2 / (r.Z * ms), r.ratio, 1e-7);
        const Shape s = nucShape(r.Z, N);
        checkTrue(w + ": the shape comes from the measured moment", s.src == ShapeSource::MeasuredMoment && s.moment == MomentUse::Used);
        checkNum(w + ": beta2 equals the separate Python solution (exact for the drawn body)", s.beta, r.beta, 0, 3e-6);
        checkTrue(w + ": the sign of beta2 is the sign of the measured moment (positive: prolate, negative: oblate)", (s.beta > 0) == (q->Qb > 0));
        const double first = std::sqrt(5 * kPi) * Q0fm2 / (3 * r.Z * (5.0 / 3) * ms);
        checkNum(w + ": the first-order hand formula sqrt(5 pi) Q0 / (3 Z R^2) with R^2 = (5/3) <r^2>", first, r.first, 1e-6);
        if (std::fabs(r.beta) > 0.2 && std::fabs(r.beta) < 0.45) checkTrue(w + ": the exact value is 4 to 9 % below the first-order one for 0.2 < |beta2| < 0.45 (the second-order term)", std::fabs(s.beta) < std::fabs(first) * 0.96 && std::fabs(s.beta) > std::fabs(first) * 0.90);
    }
    // known cases: strongly deformed rare earths and actinides are clearly prolate; the most oblate case the rule accepts is clearly oblate (Cs-126: a moment from a few nucleons; no benchmark was read, this is the data's own)
    int prolate = 0;
    for (const auto& za : {std::pair<int, int>{67, 165}, {68, 167}, {71, 175}, {63, 153}, {64, 155}, {70, 173}, {92, 235}}) {
        const Shape s = nucShape(za.first, za.second - za.first);
        if (s.src == ShapeSource::MeasuredMoment && s.beta > 0.25 && s.beta < 0.40) prolate++;
    }
    checkTrue("known cases: Ho-165, Er-167, Lu-175, Eu-153, Gd-155, Yb-173, U-235 are all prolate with 0.25 < beta2 < 0.40", prolate == 7);
    checkTrue("the most oblate case the rule accepts, Cs-126, is clearly oblate (beta2 = -0.579); near a closed shell Bi-209 and Sm-147 are almost round (-0.031, -0.030)",
              nucShape(55, 71).beta < -0.5 && std::fabs(nucShape(83, 126).beta) < 0.04 && nucShape(83, 126).beta < 0 && std::fabs(nucShape(62, 85).beta) < 0.04 && nucShape(62, 85).beta < 0);
    // ---- 5. the drawn body: volume, mean square radius and quadrupole by the test's own integrals ----
    double worstVol = 0, worstMs = 0, minRV = 1e9;
    for (const double beta : {-0.6, -0.45, -0.3, -0.03, 0.0, 0.03, 0.2, 0.3247, 0.45, 0.6}) {
        const double Rpt = 6.0, RV = volumeEquivalentRadius(Rpt, beta);
        const BodyOwn b = bodyOwn(beta, RV);
        worstVol = std::max(worstVol, std::fabs(b.volume / (4 * kPi / 3 * RV * RV * RV) - 1));
        worstMs = std::max(worstMs, std::fabs(b.ms / (0.6 * Rpt * Rpt) - 1));
        minRV = std::min(minRV, RV / Rpt);
        checkNum("the drawn body keeps its volume when deformed: beta2 = " + fmt::fixed(beta, 4) + ": V = 4 pi/3 R_V^3 (own Simpson)", b.volume, 4 * kPi / 3 * RV * RV * RV, 1e-9);
        checkNum("... and its mean square radius is the measured point-proton one: beta2 = " + fmt::fixed(beta, 4), b.ms, 0.6 * Rpt * Rpt, 1e-9);
        if (beta != 0) checkNum("... and <3z^2 - r^2> / <r^2> is the lab's shapeMomentRatio: beta2 = " + fmt::fixed(beta, 4), b.ratio, shapeMomentRatio(beta), 1e-9);
        else checkNum("a sphere has no quadrupole moment", b.ratio, 0, 0, 1e-12);
    }
    std::printf("drawn body (own integrals): worst volume error %.2e, worst mean-square-radius error %.2e; smallest R_V / R_pt = %.4f (beta2 = +-0.6)\n", worstVol, worstMs, minRV);
    checkNum("the first-order relation is the small-beta limit: <3z^2 - r^2> / <r^2> = 4 c beta (c = sqrt(5/(16 pi))) for beta = 0.001", shapeMomentRatio(0.001), 4 * 0.31539156525252005 * 0.001, 1e-3);
    checkNum("Q0 of the measured moments is reproduced by the drawn body: Ho-165, Q0 = Z <r^2> x (own integral of the drawn surface)", 67 * (5.2022 * 5.2022 - 0.84 * 0.84) * bodyOwn(nucShape(67, 98).beta, volumeEquivalentRadius(std::sqrt(5.0 / 3 * (5.2022 * 5.2022 - 0.84 * 0.84)), nucShape(67, 98).beta)).ratio,
             intrinsicQ0(3.58, 7) * kBarnFm2, 1e-6);
    // ---- 6. which source the nucleus uses, and why not the measured moment ----
    struct Why { int Z, A; ShapeSource src; MomentUse use; const char* what; };
    const Why whys[] = {
        {67, 165, ShapeSource::MeasuredMoment, MomentUse::Used, "Ho-165: a measured, signed moment, firm spin"},
        {64, 158, ShapeSource::BE2, MomentUse::NoRow, "Gd-158: even-even, no moment possible; the lab's B(E2) table has it"},
        {82, 208, ShapeSource::Estimate, MomentUse::NoRow, "Pb-208: even-even and doubly magic: no moment, an estimate (round)"},
        {50, 120, ShapeSource::Estimate, MomentUse::NoRow, "Sn-120: even-even: no moment, an estimate"},
        {3, 7, ShapeSource::Estimate, MomentUse::TooLarge, "Li-7: a real moment, but K = I would need |beta2| = 1.0 > 0.6: not used"},
        {19, 41, ShapeSource::Estimate, MomentUse::TooLarge, "K-41: |beta2| about 1.4: not used"},
        {5, 12, ShapeSource::Estimate, MomentUse::SignNotGiven, "B-12: |Q| = 0.0132 b but the file gives no sign"},
        {3, 11, ShapeSource::Estimate, MomentUse::SignNotGiven, "Li-11: sign only tentative (-)"},
        {29, 78, ShapeSource::Estimate, MomentUse::SpinNotFirm, "Cu-78: spin only tentative (6-)"},
        {85, 218, ShapeSource::Estimate, MomentUse::RowNotUsable, "At-218: the row is not a clean number"},
        {91, 231, ShapeSource::Estimate, MomentUse::RowNotUsable, "Pa-231: [-1.72(5)] 'not measured'"}};
    for (const Why& w : whys) {
        const Shape s = nucShape(w.Z, w.A - w.Z);
        checkTrue(std::string("source and reason: ") + w.what, s.src == w.src && s.moment == w.use);
    }
    checkTrue("a nucleus without a measured moment keeps what the lab had: the same beta2 as nucBeta (Gd-158: B(E2); Pb-208: round; Sn-120: the estimate)", nucShape(64, 94).beta == nucBeta(64, 94) && nucShape(82, 126).beta == nucBeta(82, 126) && nucShape(50, 70).beta == nucBeta(50, 70));
    int used = 0, tooLarge = 0, noSign = 0, spinNo = 0, notClean = 0, qualifying = 0;
    for (int i = 0; i < n; i++) {
        const Shape s = nucShape(rows[i].Z, rows[i].A - rows[i].Z);
        used += s.moment == MomentUse::Used; tooLarge += s.moment == MomentUse::TooLarge; noSign += s.moment == MomentUse::SignNotGiven; spinNo += s.moment == MomentUse::SpinNotFirm; notClean += s.moment == MomentUse::RowNotUsable;
    }
    qualifying = used + tooLarge;
    checkTrue("of the 598 ground rows 512 qualify (signed, firm spin >= 1): 498 give a shape, 14 need |beta2| > 0.6; 45 have no firm sign, 34 no firm spin, 7 are not clean numbers (counted by the separate Python)", qualifying == 512 && used == 498 && tooLarge == 14 && noSign == 45 && spinNo == 34 && notClean == 7);
    int offered = 0, srcQ = 0, srcB = 0, srcE = 0, estEvenEven = 0;
    for (int z = 1; z <= 118; z++) {
        const auto rg = nRange(z);
        for (int nn = rg.first; nn <= rg.second; nn++) {
            offered++;
            const Shape s = nucShape(z, nn);
            srcQ += s.src == ShapeSource::MeasuredMoment; srcB += s.src == ShapeSource::BE2; srcE += s.src == ShapeSource::Estimate;
            if (s.src == ShapeSource::Estimate && z % 2 == 0 && nn % 2 == 0) estEvenEven++;
        }
    }
    std::printf("shape sources of the slider's %d isotopes: measured moment %d, B(E2) %d, estimate %d (of which even-even, spin 0: %d)\n", offered, srcQ, srcB, srcE, estEvenEven);
    checkTrue("the slider's 4,252 isotopes: 498 shapes from a measured moment, 11 from B(E2), 3,743 estimates (1,057 of them even-even); counted separately in Python", offered == 4252 && srcQ == 498 && srcB == 11 && srcE == 3743 && estEvenEven == 1057);
    // ---- 7. the cloud the lab draws ----
    // (a) EXACT: buildNucleusAxial is buildNucleus without its last step, the random tilt, so the symmetry axis is z. The protons' <x^2> = <y^2> and <z^2> about the centre of mass are scaled to those of the model, so
    // (2 <z^2> - <x^2> - <y^2>) / (<x^2> + <y^2> + <z^2>) must be Q0 / (Z <r^2>) of the DATA (typed ratios, separate Python), the trace the point-proton <r^2>, and <x^2> = <y^2>: by construction, so the guard is rounding-level.
    struct Cloud { int Z, A; double ratio; };
    const Cloud clouds[] = {{67, 165, 0.434410959236}, {68, 167, 0.417905896953}, {63, 153, 0.421335134532}, {92, 235, 0.344978011370}, {27, 59, 0.244387227285}, {13, 27, 0.364438407721}, {55, 126, -0.523884804060}, {64, 155, 0.387107539569},
                           {3, 6, -0.044797779987}, {3, 8, 0.768748492144}, {7, 12, 0.373674093554}, {7, 14, 0.500103853759}};   // the last four (A <= 16) go through the Gaussian branch of the layout
    double worstRatio = 0, worstTrace = 0, worstXY = 0, minSepAxial = 1e9;
    for (const Cloud& c : clouds) {
        const int N = c.A - c.Z;
        double ratioSum = 0, minSep = 1e9; const int builds = 40;
        seedRandom(4242 + c.Z);
        for (int b = 0; b < builds; b++) {
            const std::vector<Nucleon> v = buildNucleusAxial(c.Z, N);
            double cx = 0, cy = 0, cz = 0;
            for (const Nucleon& q : v) { cx += q.x; cy += q.y; cz += q.z; }
            cx /= v.size(); cy /= v.size(); cz /= v.size();
            double sxx = 0, syy = 0, szz = 0; int np = 0;
            for (const Nucleon& q : v) if (q.isP) { sxx += (q.x - cx) * (q.x - cx) / (kFmLab * kFmLab); syy += (q.y - cy) * (q.y - cy) / (kFmLab * kFmLab); szz += (q.z - cz) * (q.z - cz) / (kFmLab * kFmLab); np++; }
            sxx /= np; syy /= np; szz /= np;
            const double tr = sxx + syy + szz, ratio = (2 * szz - sxx - syy) / tr;
            worstRatio = std::max(worstRatio, std::fabs(ratio - c.ratio)); worstTrace = std::max(worstTrace, std::fabs(tr / pointProtonMs(c.Z, N) - 1)); worstXY = std::max(worstXY, std::fabs(sxx - syy) / tr);
            ratioSum += ratio;
            for (std::size_t i = 0; i < v.size(); i++) for (std::size_t j = i + 1; j < v.size(); j++) {
                const double dx = (v[i].x - v[j].x) / kFmLab, dy = (v[i].y - v[j].y) / kFmLab, dz = (v[i].z - v[j].z) / kFmLab;
                minSep = std::min(minSep, std::sqrt(dx * dx + dy * dy + dz * dz));
            }
        }
        minSepAxial = std::min(minSepAxial, minSep);
        std::printf("cloud %s-%d (untilted, %d builds): <3z^2 - r^2>/<r^2> of the drawn protons %+.9f, from the data %+.9f; closest pair %.3f fm\n", symbol(c.Z), c.A, builds, ratioSum / builds, c.ratio, minSep);
    }
    std::printf("cloud: worst |drawn - data| ratio %.2e, trace %.2e, |<x^2> - <y^2>| %.2e; closest pair %.4f fm\n", worstRatio, worstTrace, worstXY, minSepAxial);
    checkTrue("the drawn cloud has EXACTLY the measured moment: protons' <3z^2 - r^2> / <r^2> = Q0 / (Z <r^2>) of the data for 12 nuclei (4 through the Gaussian branch, A <= 16) (guard 1e-9: exact by construction)", worstRatio <= 1e-9);
    checkTrue("... with the point-proton <r^2> as its trace (the stage 1 radius: 1e-9) and <x^2> = <y^2> (an axial body: 1e-9)", worstTrace <= 1e-9 && worstXY <= 1e-9);
    checkTrue("... and no two nucleons closer than the guard 0.606 fm (the hard core 1.0 fm, relaxed to 0.55 fm only when crowded, then scaled by the deformation; the smallest measured is printed)", minSepAxial >= guardMinSepDeformed);
    // (b) the tilted cloud the screen shows: only the SIGN is a safe statistic for a random snapshot (the largest eigenvalue of a noisy tensor of a few protons is biased), so the sign only
    for (std::size_t ci = 0; ci < 8; ci++) {   // the 8 with Z >= 13 and |ratio| >= 0.24; the light ones have a few protons, a noise of about 0.5 and (Li-6) a ratio of -0.045
        const Cloud& c = clouds[ci];
        const CloudQ q = cloudQuadrupole(c.Z, c.A - c.Z, 120, 777 + c.Z);
        std::printf("cloud %s-%d (tilted, as drawn): largest eigenvalue %+.4f (sd of one build %.3f), model %+.4f\n", symbol(c.Z), c.A, q.mean, q.sd, c.ratio);
        checkTrue(std::string("the drawn cloud's sign: ") + symbol(c.Z) + "-" + std::to_string(c.A) + (c.ratio > 0 ? " is prolate (positive)" : " is oblate (negative)"), (q.mean > 0) == (c.ratio > 0));
    }
    // the stage 1 radius still holds with the deformation: the protons' rms charge radius of the drawn nucleus is the measured one (9 nuclei, several deformed, in testNuclearSize); here the strongly deformed ones
    for (const auto& za : {std::pair<int, int>{67, 165}, {63, 153}, {92, 235}}) {
        const double m = chargeRadius(za.first, za.second - za.first)->rms;
        const LayoutStat st = layoutStat(za.first, za.second - za.first, 30, false, 31337 + za.first);
        checkNum(std::string("stage 1 radius kept after deformation: ") + symbol(za.first) + "-" + std::to_string(za.second) + ": the drawn rms charge radius = the measured one", st.chargeRms, m, 1e-9);
    }
    // the lab's readout line "shape: ..." at the Nucleus layer names the source, one of three labels
    auto shapeLine = [](int Z, int N) {
        AtomLab lab; lab.p.Z = Z; lab.p.N = N; lab.s.layer = "Nucleus";
        for (const std::string& l : lab.info()) if (l.compare(0, 7, "shape: ") == 0) return l;
        return std::string("(no shape line)");
    };
    checkStr("readout, Ho-165: a measured moment", shapeLine(67, 98), "shape: stretched like a rugby ball (from a measured quadrupole moment)");
    checkStr("readout, Cs-126: a measured moment, oblate", shapeLine(55, 71), "shape: flattened like a pancake (from a measured quadrupole moment)");
    checkStr("readout, Gd-158: B(E2)", shapeLine(64, 94), "shape: stretched like a rugby ball (from B(E2))");
    checkStr("readout, Pb-208: doubly magic, an estimate (was labelled as measured before 0.10.9 stage 2)", shapeLine(82, 126), "shape: nearly round (an estimate)");
    checkStr("readout, Sn-120: an estimate", shapeLine(50, 70), "shape: nearly round (an estimate)");
    checkStr("readout, Og-294: an estimate, a guess", shapeLine(118, 176), "shape: stretched like a rugby ball (an estimate: a guess)");
    // ---- 8. the words ----
    const std::string ho = joinLines(nuclearShapeLines(67, 98)), cs = joinLines(nuclearShapeLines(55, 71)), li7 = joinLines(nuclearShapeLines(3, 4)), b12 = joinLines(nuclearShapeLines(5, 7)), pb = joinLines(nuclearShapeLines(82, 126)),
                    gd = joinLines(nuclearShapeLines(64, 94)), sn = joinLines(nuclearShapeLines(50, 70)), h1 = joinLines(nuclearShapeLines(1, 0)), bi = joinLines(nuclearShapeLines(83, 126)), cu = joinLines(nuclearShapeLines(29, 49)),
                    li10 = joinLines(nuclearShapeLines(3, 7));
    checkTrue("card, Ho-165: names the source 'measured quadrupole moment', the moment +3.5800 b +- 0.0200 b with spin 7/2, the conversion Q0 = +7.6714 b and that it is a model assumption (K = I), prolate and beta2 = +0.3247",
              has(ho, "measured quadrupole moment") && has(ho, "+3.5800 b") && has(ho, "0.0200 b") && has(ho, "7/2") && has(ho, "+7.6714 b") && has(ho, "Model assumption, not a measurement") && has(ho, "K = I") &&
              has(ho, "prolate (stretched like a rugby ball)") && has(ho, "+0.3247"));
    checkTrue("card, Cs-126: oblate (flattened like a pancake), a negative moment, beta2 = -0.5789", has(cs, "oblate (flattened like a pancake)") && has(cs, "-0.6400 b") && has(cs, "-0.5789"));
    checkTrue("card, Li-7: the moment is real (-0.0400 b) but the rule would need |beta2| = 1.0, above the limit 0.6: not used; the source is an estimate", has(li7, "-0.0400 b") && has(li7, "above the limit") && has(li7, "Not used") && has(li7, "estimate"));
    checkTrue("card, B-12: the table has |Q| = 0.0132 b but no sign: prolate or oblate cannot be told, not used", has(b12, "0.0132 b") && has(b12, "no sign") && has(b12, "not used"));
    checkTrue("card, Li-11: the sign is only tentative: not used", has(joinLines(nuclearShapeLines(3, 8)), "tentative") && has(joinLines(nuclearShapeLines(3, 8)), "not used"));
    checkTrue("card, Cu-78: the spin is only tentative (6-): not used", has(cu, "tentative") && has(cu, "(6-)") && has(cu, "not used"));
    checkTrue("card, Pb-208 and Sn-120 (even-even: spin 0): a spin-0 nucleus has NO spectroscopic quadrupole moment whatever its shape, so this table cannot say whether it is round; 'round' is not implied",
              has(pb, "spin 0") && has(pb, "whatever its shape") && has(pb, "cannot say") && has(sn, "spin 0") && has(sn, "whatever its shape") && has(sn, "cannot say") && has(sn, "estimate"));
    checkTrue("card, Pb-208: doubly magic is round by the shell model's expectation, labelled an estimate, not a measurement", has(pb, "estimate") && has(pb, "doubly magic") && has(pb, "not a measurement"));
    checkTrue("card, Gd-158: the source is B(E2)", has(gd, "B(E2)") && has(gd, "spin 0"));
    checkTrue("card, an odd nucleus with no row (Li-10): no row in the table; if the spin is 0 or 1/2 the moment is exactly zero whatever the shape; the lab has no spin table", has(li10, "no row") && has(li10, "spin is 0 or 1/2") && has(li10, "whatever the shape") && has(li10, "no spin table"));
    checkTrue("card, H-1: one proton: spin 1/2, no quadrupole moment possible", has(h1, "spin 1/2") && has(h1, "whatever") && has(h1, "round"));
    checkTrue("card, Bi-209: a measured moment of -0.5160 b is used and the nucleus is nearly round (oblate, beta2 = -0.0307)", has(bi, "-0.5160 b") && has(bi, "-0.0307") && has(bi, "oblate"));
    const std::string ex = shapeExplain();
    checkTrue("explanation: spectroscopic vs intrinsic, zero for spin 0 and 1/2, the strong-coupling K = I model assumption (poor for light nuclei and near closed shells), the 0.6 limit, a uniform charged body with a volume that is kept, "
              "a diffuse cloud edge, a static picture", has(ex, "spectroscopic") && has(ex, "intrinsic") && has(ex, "spin 0 and 1/2") && has(ex, "strong coupling") && has(ex, "K = I") && has(ex, "model assumption") &&
              has(ex, "near closed shells") && has(ex, "0.6") && has(ex, "uniform") && has(ex, "volume") && has(ex, "diffuse") && has(ex, "vibrate and rotate"));
    checkTrue("the credit names the IAEA file, its INDC report and the NSR keys, and says the page with the citation terms could not be read", has(kQuadCredit, "elec_mom_recomm.csv") && has(kQuadCredit, "indc-nds-0833") && has(kQuadCredit, "NSR") && has(kQuadCredit, "could not be read"));
}

int main() {
    testConstants();
    testOneEnergy();
    // guards = the worst error MEASURED after the fix (Linux GCC, 32 runs of 60 s) + 1 %: radius 2.16990e-4 -> 2.192e-4, energy 4.70644e-8 -> 4.754e-8, turning rate 1.45367e-4 -> 1.469e-4. The same runs
    // before the fix (0.10.6 stage 2 code, one leapfrog step per frame): radius 7.783e-3 (n = 1, speed 30), energy 5.964e-5, rate 5.165e-3, so this fails without the sub-steps.
    testJumpSameDots();
    testJumpBuild();
    testOrbitAccuracy(2.192e-4, 4.754e-8, 1.469e-4);
    testFixedExtent();
    // guards: outline margin 0.10 = about half the smallest MEASURED difference (0.1867, 4f m=+1 against its fallback 4fz3); norm 6.69e-4 = measured 6.62289e-4 (the midpoint quadrature's own error on the orthogonal
    // pair 4pz + 4fz3; the overlapping pairs reach 1.19e-4) + 1 %; dots 0.0591 = measured 0.05844 (Metropolis sampling error of the 8 seeded labs) + 1 %.
    // BEFORE the fix (same tests): outline asked for the fallback in 20 of 20 states (difference 0), norm error 1.00009 (the same state twice at mix 50: the integral is 2), dots 0.944 off (and the dots thinned instead of all shown).
    testOutlineIsTheState(0.10);
    testSuperNorm(6.69e-4);
    testOverlapDots(0.0591);
    testSuperReadout();
    testRelativityText();
    testTimelineText();
    testVisibleRange();
    // guard 0.1788 = the worst MEASURED difference of the sampled <r> from the exact value at the full ring, 0.17702 (96 seeded labs: 12 orbitals x 8 seeds), + 1 %; 95 of 96 were within 10 %, mean 0.0311.
    // BEFORE the fix (same test): the sampled value was shown from 300 dots on (1,600 dots here: a number instead of the placeholder), with no note.
    testSampledRadius(0.1788);
    // guards = the worst MEASURED difference + 1 %. Mass against AME2020: 3.899e-7 (Fe-56; the lab's table is rounded to 1 keV per nucleon and includes the electrons' binding) -> 3.94e-7; BEFORE the fix 7.003e-3 (H-2, the old
    // form A u - Z m_e), so this fails without the new mass. 1s norm integral: pi to 5.83e-10 -> 5.9e-10. Sampled <r> at Z = 3 and 26 (48 labs): worst 0.07226 -> 0.0730.
    testNuclearMass(3.94e-7);
    testSpectrumIndependent();
    testBasicOrbitChecks();
    testSampledRadiusZ(0.0730);
    // guards = the worst MEASURED value + 1 % (Linux GCC Release): area error 2.2069e-8 -> 2.23e-8 (Simpson, 20,000 intervals); stored curve against Simpson 3.8113e-7 -> 3.85e-7; right edge 9.1995e-9 -> 9.30e-9;
    // <r> by quadrature 4.5654e-10 -> 4.62e-10; 1s outline reach 0.05966 of a grid step from the closed-form 90 % radius -> 0.0603; dots: Kolmogorov distance of 60 seeded labs, worst 0.105906 -> 0.1070.
    // Two guards are NOT measured + 1 %: the closed-form comparison (measured 1.976e-14) and the peaks (measured 0, the bisection lands on the exact value) are rounding-level, where a 1 % guard would depend on the
    // compiler's last bit, so they use 1e-12 (50 times the first, far below any real error).
    testRadialGraph(1e-12, 2.23e-8, 3.85e-7, 9.30e-9, 4.62e-10, 1e-12, 0.0603);
    testRadialDots(0.1070);
    testRadialReadout();
    testAsdData();
    testDirac();
    testDiracPrecision();
    testLifetimes();
    // guards: layout 1e-9 (rounding level: the size is exact by construction); closest pair 0.673 fm = the smallest MEASURED (0.6801 fm, C-12, 9 nuclei) - 1 %, a floor
    testNuclearSize(1e-9, 0.673);
    // guard: closest two nucleons in the 12 deformed clouds = the smallest MEASURED (0.6125 fm, Li-8) - 1 %, a floor
    testQuadrupole(0.606);
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
