#include "atom_sim.h"
#include "photon_pattern.h"
#include "quark_tomo.h"
#include "radii.h"
#include "dirac.h"
#include "dirac_cloud.h"
#include "asd.h"
#include "quadrupole.h"
#include "lifetime.h"
#include "levels.h"
#include "zeeman.h"
#include "stark.h"

#include <algorithm>
#include <atomic>
#include <thread>
#include <cstring>
#include <random>
#include <chrono>
#include <cmath>
#include <complex>
#include <cstdint>
#include <map>

#include "cloud_sample.h"
#include "atom_card.h"
#include "orbit/belts.h"
#include "orbit/format.h"
#include "phase_wheel.h"
#include "radial_graph.h"

namespace atom {

static std::uint64_t nextEpoch();   // a new number each call (for the GPU copies of the amplitude arrays, see AtomState::saEpoch)

static double roundHalfUp(double x) { const double r = std::floor(x + 0.5); return r == 0 && x < 0 ? -0.0 : r; }
static double clampZoom(double z) { return std::max(kZoomMin, std::min(7.0, z)); }
static constexpr double kMaxStepAngle = 2 * kPi / 300;   // the largest angle one leapfrog step of the Orbit view may turn, rad (see AtomLab::step)
static const char* const kGreek[5] = {"α", "β", "γ", "δ", "ε"};

AtomLab::AtomLab() {
    nowMs = [] {
        using namespace std::chrono;
        return duration<double, std::milli>(steady_clock::now().time_since_epoch()).count();
    };
    restart();
}

bool AtomLab::validChoice(const std::string& key, const std::string& v) {
    static const std::map<std::string, std::vector<std::string>> opts = {
        {"scale", {"Auto", "Fixed"}}, {"blook", {"Across", "Along"}}, {"dspin", {"Unpolarised", "M = ±1", "M = 0"}}, {"qstyle", {"Accurate", "Simple"}}, {"detail", {"Valence", "+ Gluons", "+ Sea"}},
        {"style", {"Density", "Nucleons"}}, {"view", {"Orbit", "Cloud", "Cloud 3D", "Slice", kChartView}}, {"cheight", {"Half-life", "Binding energy per nucleon"}}, {"ostyle", {"Modern", "Bohr 1913"}},
        {"orb", {"1s", "2s", "2p", "3s", "3p", "3d", "4s", "4p", "4d", "4f", "5s", "5p", "5d", "5f", "6s", "6p", "6d", "6f"}}, {"pAx", {"x", "y", "z", "m+1", "m-1"}}, {"dAx", {"z2", "xz", "yz", "xy", "x2y2", "m+1", "m-1", "m+2", "m-2"}},
        {"fAx", {"z3", "xz2", "yz2", "xyz", "zx2y2", "xx23y2", "y3x2y2", "m+1", "m-1", "m+2", "m-2", "m+3", "m-3"}},
        {"pAx2", {"x", "y", "z", "m+1", "m-1"}}, {"dAx2", {"z2", "xz", "yz", "xy", "x2y2", "m+1", "m-1", "m+2", "m-2"}},
        {"fAx2", {"z3", "xz2", "yz2", "xyz", "zx2y2", "xx23y2", "y3x2y2", "m+1", "m-1", "m+2", "m-2", "m+3", "m-3"}},
        {"orb2", {"1s", "2s", "2p", "3s", "3p", "3d", "4s", "4p", "4d", "4f", "5s", "5p", "5d", "5f", "6s", "6p", "6d", "6f"}}, {"super", {"Off", "On"}}, {"cutAxis", {"Vertical", "Horizontal"}},
        {"cstyle", {"Classic", "Glow"}}, {"outline", {"Off", "On"}}, {"phase", {"Sign", "Wheel"}}, {"cdens", {"Normal", "High", "Ultra"}}, {"dirac", {"Off", "On"}}, {"hrel", {"Auto", "Dirac", "Non-relativistic"}}, {"hj", {"l + 1/2", "l - 1/2"}}, {"elec", {"Hydrogen-like", "All"}}, {"mrel", {"Relativistic", "Non-relativistic"}}, {"show", {"Whole atom", "One orbital"}}, {"rscale", {"Shells enlarged", "True scale"}}, {"vdots", {"Easier to see", "True share"}}, {"sdots", {"True share", "Outer electron alone"}},
        {"jfrom", {"1s", "2s", "2p", "3s", "3p", "3d", "4s", "4p", "4d", "4f", "5s", "5p", "5d", "5f", "6s", "6p", "6d", "6f"}}, {"jto", {"1s", "2s", "2p", "3s", "3p", "3d", "4s", "4p", "4d", "4f", "5s", "5p", "5d", "5f", "6s", "6p", "6d", "6f"}}};
    const auto it = opts.find(key);
    return it != opts.end() && std::find(it->second.begin(), it->second.end(), v) != it->second.end();
}

std::string AtomLab::migrateChoice(const std::string& key, const std::string& v) {
    if (key == "qstyle") return v == "Cinematic" ? "Accurate" : v == "Classic" ? "Simple" : v;   // 0.43 (owner, 8 Nov 2026): two quark styles, Accurate and Simple
    if (key == "style") return v == "Fuzzy" || v == "Hard balls" ? "Nucleons" : v;                // 0.43: two nucleus styles, Density and Nucleons
    if (key == "elec") return v == "One" ? "Hydrogen-like" : v;                                  // 0.46 (review A3): "One" renamed; saved settings and old scripts still load
    return v;
}

static std::string lower(std::string t) { for (char& c : t) if (c >= 'A' && c <= 'Z') c = char(c - 'A' + 'a'); return t; }   // element names are plain ASCII

int AtomLab::capFor(const std::string& d) { return d == "Ultra" ? 4 * kCap : d == "High" ? 2 * kCap : kCap; }
int AtomLab::cloudCap() const {
    const int base = capFor(p.cdens);
    if (!manyOn() || manySub() >= 0) return base;             // one electron, or one orbital of the atom: as before
    return std::min(base * std::max(1, p.Z), kManyDotsCeiling * base);   // the neutral atom has Z electrons
}

void AtomLab::restart() {
    s = AtomState();
    init();
}

// ---------------------------------------------------------------- init
void AtomLab::init() {
    // circular orbit for level n: r = n²/Z Bohr radii, v = Z/n atomic units of speed
    const double a = random() * kPi * 2, r = double(p.n) * p.n / p.Z * aUnit(), v = double(p.Z) / p.n * kAuSpeed;
    if (s.Zprev != p.Z) { s.valN = 0; s.valL = 0; s.valJ2 = 0; s.valJ2Next = 0; s.lvl = 0; }               // 0.46: another element: its outer electron starts in the ground level
    s.t = 0; s.Zprev = p.Z; s.more = moreStored; s.pan = {}; s.panTo.reset(); s.dragMode = "rotate";
    s.x = r * std::cos(a); s.y = r * std::sin(a); s.vx = -v * std::sin(a); s.vy = v * std::cos(a); s.trail.clear();
    s.pts.assign(std::size_t(cloudCap()) * 3, 0.f); s.sg.assign(cloudCap(), 0); s.ph.assign(cloudCap(), 0.f); s.k = 0;
    owed_.clear(); owedLeft_ = 0;                              // 0.66.0: steps owed to the cloud before this start are not this cloud's
    s.sa.clear(); s.beatT = 0; s.saEpoch = nextEpoch(); s.ptsEpoch = nextEpoch(); s.saDirtyLo = 0; s.saDirtyHi = 0;
    if (superOn()) { s.sa.assign(std::size_t(cloudCap()) * 4, 0.f); sup_ = superState(); nA_ = normFactor(sup_.idA, p.Z); nB_ = normFactor(sup_.idB, p.Z); overlap_ = overlapAB(sup_.idA, sup_.idB) != 0.0; } else overlap_ = false;
    s.yaw = 0.6; s.pitch = 0.35; s.oyaw = 0; s.opitch = kPi / 2; s.spin = !reducedMotion;   // Cloud 3D turns by itself, unless motion is reduced
    s.fc = {}; s.focus = -1; s.zoomTo.reset(); s.layer = "Atom"; s.hits.clear();
    s.trans.reset(); s.photon.reset(); s.jp.clear(); s.jA.clear(); s.jB.clear(); s.jph.clear(); s.jsg.clear(); s.jn = 0; s.jb = AtomState::JumpBuild(); s.landT = 99; s.lines.clear(); s.autoPhotons = false; s.autoSaid.clear(); s.wait = 0.3; s.boost = 0;
    s.last.clear(); s.msg.clear(); s.szCur.reset();
    s.crng = std::uint32_t(random() * 4294967296.0);       // the cloud's own generator (0.37: no burn-in; every dot is an exact sample from the start)
    s.sliceAxis = sliceAxisNow(); s.sliceOff = sliceOffNow(); s.sliceFresh = 0;
}

// ---------------------------------------------------------------- settings
void AtomLab::setParam(const std::string& key, double v) {
    if (key == "tilt") { setChartTilt(v); return; }              // the chart's tilt is part of the view, not of the atom: no new state, no re-init
    double lo = 0, hi = 0;
    if (key == "zoom") { lo = kZoomMin; hi = 7; }
    else if (key == "Z") { lo = 1; hi = 118; }
    else if (key == "N") { const auto r = nRange(p.Z); lo = r.first; hi = r.second; }
    else if (key == "cut") { lo = -1; hi = 1; }
    else if (key == "n") { lo = 1; hi = kLineMaxN; }
    else if (key == "speed") { lo = 1; hi = 30; }
    else if (key == "mix") { lo = 0; hi = 100; }
    else if (key == "bfield") { p.bfield = std::isfinite(v) ? std::max(0.0, std::min(100.0, v)) : 0; return; }   // 0.54.0: nothing else changes (no new state)
    else if (key == "zline") { const int n = static_cast<int>(zeeman::labLines(p.Z, p.N, zeemanKind()).size()); p.zline = v < 0 || n == 0 ? -1 : std::min(n - 1, static_cast<int>(v)); return; }
    // 0.93.0: the electric field and the Stark card's choices: nothing else changes (the parabolic state's cloud does not depend on the field's size)
    else if (key == "efield") { p.efield = std::isfinite(v) ? std::max(0.0, std::min(starkTopVm(), v)) : 0; return; }
    else if (key == "starkN") { p.starkN = std::isfinite(v) ? std::max(2, std::min(5, static_cast<int>(std::lround(v)))) : 3; p.efield = std::min(p.efield, starkTopVm());
                                p.sline = std::min(p.sline, static_cast<int>(starkLines().size()) - 1); return; }
    else if (key == "sline") { const int n = static_cast<int>(starkLines().size()); p.sline = !std::isfinite(v) || v < 0 ? 0 : std::min(n - 1, static_cast<int>(v)); return; }
    else return;
    v = std::max(lo, std::min(hi, v));
    if (key == "zoom") p.zoom = v;
    else if (key == "Z") p.Z = int(v);
    else if (key == "N") p.N = int(v);
    else if (key == "cut") p.cut = v;
    else if (key == "n") p.n = int(v);
    else if (key == "mix") p.mix = v;
    else p.speed = int(v);
    onParam(key);
}

void AtomLab::setParam(const std::string& key, const std::string& v) {
    if (key == "cheight") {                                      // what the chart's height means: part of the view, not of the atom (not saved, no re-init); nothing else about the picture changes
        if (!validChoice(key, v)) return;
        s.chartView.height = v == "Binding energy per nucleon" ? ChartHeightMode::Binding : ChartHeightMode::HalfLife;
        return;
    }
    if (key == "scale") p.scale = v;
    else if (key == "qstyle") p.qstyle = migrateChoice(key, v);
    else if (key == "detail") p.detail = v;
    else if (key == "style") p.style = migrateChoice(key, v);
    else if (key == "dspin") { if (validChoice(key, v)) p.dspin = v; }
    else if (key == "blook") { if (validChoice(key, v)) p.blook = v; return; }   // 0.54.0
    else if (key == "view") { if (validChoice(key, v)) p.view = v; }   // 0.50.3 (outside review): an unknown view was stored and drawn as Orbit
    else if (key == "ostyle") p.ostyle = v;
    else if (key == "orb") { if (orbIndex(v) >= 0) p.orb = v; }     // (0.39.1: an unknown shell was stored and then used as index -1 by fullOrb / stateIdOf)
    else if (key == "pAx") p.pAx = v;
    else if (key == "dAx") p.dAx = v;
    else if (key == "fAx") p.fAx = v;
    else if (key == "cutAxis") p.cutAxis = v;
    else if (key == "cstyle") p.cstyle = v;
    else if (key == "cdens") p.cdens = v;
    else if (key == "elec" || key == "show" || key == "rscale" || key == "mrel") {   // 0.39.1: these change the fitted size (one electron's orbital against the whole atom: about 50 times for iron), so while
        const double before = sizeM();                          // zoomed in the scale on screen is kept, as for a new element (it threw a view zoomed to the nucleus far out)
        (key == "elec" ? p.elec : key == "show" ? p.show : key == "mrel" ? p.mrel : p.rscale) = migrateChoice(key, v);
        if (p.zoom > 0 && p.scale != "Fixed" && before > 0 && !s.jumping) { s.selfSet = true; setParam("zoom", std::max(0.0, p.zoom + std::log10(sizeM() / before))); s.selfSet = false; }
    }
    else if (key == "outline") p.outline = v;
    else if (key == "dirac") p.dirac = v;
    else if (key == "hrel") p.hrel = v;     // 0.49.3
    else if (key == "hj") p.hj = v;
    else if (key == "vdots") { if (validChoice(key, v)) p.vdots = v; else return; }
    else if (key == "sdots") { if (validChoice(key, v)) p.sdots = v; else return; }   // 0.91.0   // 0.54.1
    else if (key == "pstate") {                                  // 0.93.0: "Off" or "n1 n2 m" (checked against the orbital's n when it is used: starkPar)
        int a = 0, b = 0, c = 0; char extra = 0;
        if (v == "Off" || std::sscanf(v.c_str(), "%d %d %d %c", &a, &b, &c, &extra) == 3) p.pstate = v; else return;
    }
    else if (key == "jfrom") p.jfrom = v;
    else if (key == "jto") p.jto = v;
    else if (key == "phase") p.phase = v;
    else if (key == "super") p.super = v;
    else if (key == "orb2") { if (orbIndex(v) >= 0) p.orb2 = v; }
    else if (key == "pAx2") p.pAx2 = v;
    else if (key == "dAx2") p.dAx2 = v;
    else if (key == "fAx2") p.fAx2 = v;
    else return;
    onParam(key);
}

void AtomLab::onParam(const std::string& key) {
    if (s.jumping) return;                                    // a photon jump changed the level itself: keep the state
    if (key == "Z") setParam("N", defaultN(p.Z));            // a new element: neutrons follow to its usual isotope
    if (key == "Z" || key == "elec" || key == "mrel") p.zline = -1;   // 0.54.0: another atom or model has other lines: back to the level's own
    if (key == "zoom") { if (!s.selfSet) s.zoomTo.reset(); return; }   // a manual zoom cancels an animated dive
    if (key == "N" || key == "Z") { if (s.focus >= p.Z + p.N) s.focus = -1; }   // keep the followed nucleon if it is still there
    if (key == "Z" && p.scale != "Fixed" && s.Zprev && s.Zprev != p.Z) {
        // auto-fit rescales with Z: counter it so what you are looking at stays the same size on screen. 0.39.1: by the ratio of the fitted sizes themselves (one electron: Zprev / Z;
        // a neutral many-electron atom is about the same size for every Z, and the old Zprev / Z threw a zoomed-in view far out, e.g. a factor 46 from He to U)
        const int Znew = p.Z; p.Z = s.Zprev; const double szOld = sizeM(); p.Z = Znew;
        const double d = std::log10(sizeM() / szOld);
        if (p.zoom > 0) { s.selfSet = true; setParam("zoom", std::max(0.0, p.zoom + d)); s.selfSet = false; }   // (5p: only while zoomed in; at or below the old smallest size nothing is countered)
        if (s.zoomTo) s.zoomTo = clampZoom(*s.zoomTo + d);
    }
    if (key == "mix") {                                       // orthogonal pair: the shares change the time dependence only, never the dots. An overlapping pair has no time dependence (one energy): the
        sup_.a = std::sqrt(1 - p.mix / 100); sup_.b = std::sqrt(p.mix / 100);   // mix changes the density itself, so the cloud is refilled below, like a new orbital
        if (!(superOn() && overlap_)) return;
    }
    if ((key == "orb2" || key == "pAx2" || key == "dAx2" || key == "fAx2") && p.super != "On") return;    // (nothing shown depends on them while the superposition is off)
    static const char* const reinit[] = {"Z", "n", "view", "orb", "pAx", "dAx", "fAx", "cdens", "super", "orb2", "pAx2", "dAx2", "fAx2", "elec", "show", "rscale", "mrel", "hrel", "hj", "vdots", "sdots", "pstate"};   // a new density refills the cloud
    if (key == "mix" || std::find(std::begin(reinit), std::end(reinit), key) != std::end(reinit)) {
        const AtomState keep = s;
        init();
        s.yaw = keep.yaw; s.pitch = keep.pitch; s.oyaw = keep.oyaw; s.opitch = keep.opitch; s.spin = keep.spin;
        s.pan = keep.pan; s.panTo = keep.panTo; s.dragMode = keep.dragMode; s.fc = keep.fc; s.focus = keep.focus;
        s.zoomTo = keep.zoomTo; s.autoPhotons = keep.autoPhotons;
        if (key != "Z") s.lines = keep.lines;                 // a new element has a new spectrum
    }
}

// ---------------------------------------------------------------- orbitals and sizes
int AtomLab::fullOrb(const std::string& shell) const {
    const int si = orbIndex(shell);
    const Orbital& o = orbitals()[si];
    if (o.l == 0) return si;
    static const char* const def[4] = {"", "z", "z2", "z3"};  // the default orientation of p, d and f
    const std::string& ori = o.l == 1 ? p.pAx : o.l == 2 ? p.dAx : p.fAx;
    const int i = orbIndex(shell + "|" + (ori.empty() ? def[o.l] : ori));
    return i >= 0 ? i : orbIndex(shell + "|" + def[o.l]);
}
int AtomLab::fullOrb() const { return fullOrb(p.orb); }
int AtomLab::complexM() const {
    const int l = orbitals()[orbIndex(p.orb)].l;
    int m = 0;
    if (l == 0 || !oriIsComplex(l == 1 ? p.pAx : l == 2 ? p.dAx : p.fAx, m) || std::abs(m) > l) return 0;
    return m;
}
int AtomLab::stateIdOf(const std::string& shell, const std::string& pAx, const std::string& dAx, const std::string& fAx) const {
    const int si = orbIndex(shell), l = orbitals()[si].l;
    int m = 0;
    if (l > 0 && oriIsComplex(l == 1 ? pAx : l == 2 ? dAx : fAx, m) && std::abs(m) <= l) return complexState(si, m);
    if (l == 0) return si;
    static const char* const def[4] = {"", "z", "z2", "z3"};
    const std::string& ori = l == 1 ? pAx : l == 2 ? dAx : fAx;
    const int i = orbIndex(shell + "|" + (ori.empty() ? def[l] : ori));
    return i >= 0 ? i : orbIndex(shell + "|" + def[l]);
}
Superposition AtomLab::superState() const {
    Superposition u;
    u.idA = stateId(); u.idB = stateIdOf(p.orb2, p.pAx2, p.dAx2, p.fAx2);
    u.a = std::sqrt(1 - p.mix / 100); u.b = std::sqrt(p.mix / 100);
    return u;
}
int AtomLab::sizeN() const {
    const int nA = orbitals()[orbIndex(p.orb)].n;
    return superOn() ? std::max(nA, orbitals()[orbIndex(p.orb2)].n) : nA;
}
int AtomLab::outlineStateId() const { return stateId(); }   // the outline belongs to the state ON SCREEN: for m = +-1.. its own complex id (fullOrb() would give the real orbital the Orientation falls back to: a dumbbell round a doughnut)
int AtomLab::stateId() const { const int m = complexM(); return m ? complexState(orbIndex(p.orb), m) : fullOrb(); }
bool AtomLab::diracCloudOn() const {                            // 0.49.3
    if (p.elec != "Hydrogen-like" || p.view == "Orbit" || chartOn() || p.super == "On") return false;
    return p.hrel == "Dirac" || (p.hrel == "Auto" && p.Z >= kRelativityZ);
}
// ---- 0.93.0: the electric field (atom/stark.h) ----
bool AtomLab::starkPar(int& n1, int& n2, int& m) const {
    if (p.pstate == "Off") return false;
    char extra = 0;
    if (std::sscanf(p.pstate.c_str(), "%d %d %d %c", &n1, &n2, &m, &extra) != 3) return false;
    const int fo = fullOrb();
    if (fo < 0) return false;
    return stark::valid(stark::Par{orbitals()[std::size_t(fo)].n, n1, n2, m});
}
bool AtomLab::starkCloudOn() const {
    int a = 0, b = 0, c = 0;
    return starkOk() && (p.view == "Cloud" || p.view == "Cloud 3D") && p.super != "On" && !diracCloudOn() && starkPar(a, b, c);
}
double AtomLab::starkFieldAu() const { return p.efield / stark::fieldUnitVm(); }
double AtomLab::starkTopVm() const { return stark::saddleFieldAu(std::max(2, std::min(5, p.starkN)), stark::reducedMassH()) * stark::fieldUnitVm(); }
std::vector<std::pair<int, int>> AtomLab::starkLines() const {
    std::vector<std::pair<int, int>> v;
    for (int lo = 1; lo < p.starkN; lo++) for (int up = lo + 1; up <= p.starkN; up++) v.push_back({up, lo});
    return v;
}
int AtomLab::diracJ2() const { const int l = orbitals()[std::size_t(fullOrb())].l; return l == 0 ? 1 : p.hj == "l - 1/2" ? 2 * l - 1 : 2 * l + 1; }
std::string AtomLab::diracText() const { const Orbital& o = orbitals()[std::size_t(fullOrb())]; return diracStateText(o.n, o.l, diracJ2()); }
std::string AtomLab::stateText() const { return atom::stateText(stateId()); }

// the size the picture is fitted to: Fixed uses one reference size for every atom and level, that of hydrogen n = 4: the circle n² = 16 a0 in the Orbit view, and the view radius of the
// n = 4 clouds, kExt[4] = 40 a0, in the cloud views. (It was 24 a0 = kExt[3], the n = 3 size, which cut off 4s, 4p, 4d and 4f: their 90 % outline reaches 30-36 a0; 0.10.6 stage 3, audit finding 8.)
// 0.46 (B1): levels 5 and 6 are bigger than that reference, so for one electron Fixed fits the bigger of the reference and the level (hydrogen n = 5, 6 only; every n <= 4 is as before).
static double fitSize(const AtomParams& p, double szM) { return p.scale == "Fixed" ? (p.view == "Orbit" ? 16 : kExt[4]) * kA0 : szM; }
static double fitSizeOne(const AtomParams& p, double szM) { return p.scale == "Fixed" ? std::max(fitSize(p, szM), szM) : szM; }
double AtomLab::sizeM() const {
    if (manyOn() && p.view == "Orbit") { const std::vector<Ring> g = manyRings(); return fitSize(p, (g.empty() ? 1.0 : drawnRadius(g.back().r)) * kA0); }   // 0.39.1: the outermost ring where the one-electron circle sits
    if (manyOn()) return fitSize(p, manyExtM());
    return fitSizeOne(p, p.view == "Orbit" ? double(p.n) * p.n / p.Z * aUnit() : kExt[sizeN()] / p.Z * aUnit());
}
double AtomLab::pxPerM(double m, std::optional<double> sz) const { return (p.view == "Orbit" ? 0.42 : 0.45) * m / (sz ? *sz : sizeM()); }
double AtomLab::nucRA0() const { return nuclearRadius(p.Z, p.N) / kFmPerA0; }   // measured charge radius (as a uniform ball) where there is one, else the rule: radii.h
double AtomLab::zoomForNucleus() const {
    const double m = fitSpan();
    // 0.46.5: the deuteron's charge reaches far past its rms radius (a long halo: the pair is more than 3 fm apart about 43 % of the time), so it is shown smaller: its ball at 0.2 of the
    // span (0.28 for every other nucleus), which also keeps it in the Density look (nucleons under 40 points), where its spin-state shapes are drawn
    return clampZoom(std::log10((p.Z + p.N == 2 ? 0.2 : 0.28) * m / (nucRA0() * kA0 * pxPerM(m))));
}
double AtomLab::zoomForNucleon() const {
    const double m = fitSpan();
    return clampZoom(std::log10(0.42 * m * kFmPerA0 / kNucleonR / (pxPerM(m) * kA0)));
}
double AtomLab::zoomForNucleonRadius(double points) const {
    return clampZoom(zoomForNucleon() + std::log10(points / (0.42 * fitSpan())));   // (the nucleon's radius grows as 10^zoom; at zoomForNucleon it is 0.42 of the span)
}
double AtomLab::sliceOffNow() const { return p.cut * 0.5 * (manyOn() ? manyExtM() / kA0 : kExt[sizeN()] / p.Z); }
double AtomLab::cutOffsetM() const { return sliceOffNow() * aUnit(); }
double AtomLab::lenScale() const { return manyOn() ? 1.0 : 1 + 1 / nuclearMassMe(p.Z, p.N); }   // m / mu = 1 + m / M

const char* const kSampledRadiusNote = " (an estimate from independent dots, usually within 1 %)";   // the figure: tests/test_atom.cpp, testSampledRadius

// ---------------------------------------------------------------- the cloud: exact, independent samples of |ψ|² (0.37)
// Until 0.36 the dots were one Metropolis chain: right on average, but neighbouring dots were close together, so the 12,000 on screen were worth only a few hundred independent ones and
// one lobe often looked denser than the other. Now every dot is drawn on its own from the exact density (cloud_sample.h: the radius by inverting its exact distribution function, the
// direction by rejection), so a picture of N dots has only the 1/sqrt(N) scatter of independent samples. In the Slice view the dots are made ON the cut plane, with the density there.
// ---------------------------------------------------------------- 0.39: many electrons (atom/hf.h)
const hf::Atom* AtomLab::manyAtom() const {                     // 0.41: Dirac-Hartree-Fock, or its non-relativistic limit; 0.46: an alkali atom with its outer electron in an excited orbital
    const bool rel = p.mrel != "Non-relativistic";
    if (rel && levelJumps(p.Z))                                  // 0.47.3: a level atom: the level's GRASP2018 cloud, the ground one too (levels.h)
        if (const GraspLevel* g = levelCloud(p.Z, s.lvl)) return &g->atom;
    if (alkaliJumps(p.Z) && rel && graspHas(p.Z)) {             // 0.47.2: every level of Li..Fr, the ground one too, from GRASP2018 (alkali.h)
        const AlkOrbital c = valenceNow();
        if (const hf::Atom* a = alkaliAtom(p.Z, c.n, c.l, true, valenceJ2())) return a;
    } else if (s.valN > 0 && alkaliJumps(p.Z)) if (const hf::Atom* a = alkaliAtom(p.Z, s.valN, s.valL, rel)) return a;
    return hf::find(p.Z, 0, rel);
}
bool AtomLab::manyOn() const { return p.elec == "All" && !chartOn() && manyAtom() != nullptr; }   // 0.39.1: every view of the atom (0.39: Cloud and Cloud 3D only)
int AtomLab::manySub() const {
    if (p.show != "One orbital" || p.view == "Orbit" || !manyAtom()) return -1;      // (the Orbit view shows the shells as rings: always the whole atom)
    return manySubOf(*manyAtom());
}
int AtomLab::manySubOf(const hf::Atom& a) const {
    if (p.show != "One orbital" || p.view == "Orbit") return -1;
    const Orbital& o = orbitals()[orbIndex(p.orb)];
    const int i = hf::subshellIndex(a, o.n, o.l);
    return i >= 0 && a.sub[std::size_t(i)].occ > 0 ? i : -1;
}

// ---- 0.46 (B3): the jumps of the one-valence-electron atoms (alkali.h) ----
static bool manyDot(const AtomLab& L, const hf::Atom& a, int one, int vi, const GraspLevel* gl, const hf::Plane* plane, int axis, orbit::Mulberry32& r, double& x, double& y, double& z, int& shell,
                    const std::vector<int>* vset = nullptr, bool alone = false);
static int valenceSub(const AtomLab& L, const hf::Atom& a);
static std::vector<int> levelValenceSet(const AtomLab& L, const hf::Atom& a);
bool AtomLab::valenceOn() const {                               // 0.47.2: relativistic: the GRASP clouds (Fr has only these); non-relativistic: the frozen-core orbitals of 0.46
    if (!manyOn() || !alkaliJumps(p.Z) || alkaliOrbitals(p.Z).empty()) return false;
    const bool rel = p.mrel != "Non-relativistic";
    if (rel && graspHas(p.Z)) return true;
    return valenceOrbital(p.Z, alkaliOrbitals(p.Z)[0].n, alkaliOrbitals(p.Z)[0].l, rel) != nullptr;
}
int AtomLab::valenceJ2() const { return s.valN == 0 ? 1 : s.valJ2 > 0 ? s.valJ2 : graspDefaultJ2(p.Z, s.valN, s.valL); }
AlkOrbital AtomLab::valenceNow() const {
    if (s.valN > 0) return {s.valN, s.valL};
    const std::vector<AlkOrbital> o = alkaliOrbitals(p.Z);
    return o.empty() ? AlkOrbital{} : o[0];
}
std::vector<AlkOrbital> AtomLab::valenceTargets(int dir) const {
    std::vector<AlkOrbital> out;
    if (!valenceOn()) return out;
    const AlkOrbital c = valenceNow();
    const double e0 = alkaliTermCm(p.Z, c.n, c.l);
    const bool rel = p.mrel != "Non-relativistic";
    for (const AlkOrbital& o : alkaliOrbitals(p.Z)) {
        if (o.n == c.n && o.l == c.l) continue;
        const double e = alkaliTermCm(p.Z, o.n, o.l);
        if ((dir < 0 ? e >= e0 : e <= e0) || alkaliLinesBetween(p.Z, c.n, c.l, o.n, o.l).empty() || !alkaliAtom(p.Z, o.n, o.l, rel)) continue;
        out.push_back(o);
    }
    return out;
}
static int valIndex(int Z, int n, int l) { const std::vector<AlkOrbital> o = alkaliOrbitals(Z); for (int i = 0; i < int(o.size()); i++) if (o[std::size_t(i)].n == n && o[std::size_t(i)].l == l) return i; return -1; }
static std::string valName(int n, int l) { return std::to_string(n) + "spdfg"[l]; }
void AtomLab::setValence(int n, int l) {
    const std::vector<AlkOrbital> o = alkaliOrbitals(p.Z);
    if (o.empty() || valIndex(p.Z, n, l) < 0) return;
    const bool ground = o[0].n == n && o[0].l == l;
    s.valN = ground ? 0 : n; s.valL = ground ? 0 : l; s.valJ2 = 0;
    s.k = 0; s.sliceFresh = 0; s.ptsEpoch = nextEpoch();         // the cloud is made again for the new atom (the dots of the old one would be wrong)
}
std::vector<AlkOrbital> AtomLab::valenceChoices() const {
    std::vector<AlkOrbital> out;
    if (!valenceOn()) return out;
    const bool rel = p.mrel != "Non-relativistic";
    for (const AlkOrbital& o : alkaliOrbitals(p.Z)) if (alkaliAtom(p.Z, o.n, o.l, rel)) out.push_back(o);
    return out;
}
// 0.47: the owner's "tap a shell and the outer electron goes there". No photon and no line needed (up, down or between levels with no line): the lab places the electron to show its shape;
// the panel's caption says that a real atom changes level only by taking in or giving out energy. A jump or photon under way is finished or dropped first.
bool AtomLab::placeValence(int n, int l) {
    if (!valenceOn()) return false;
    const int to = valIndex(p.Z, n, l);
    if (to < 0 || !alkaliAtom(p.Z, n, l, p.mrel != "Non-relativistic")) return false;
    if (s.trans) finishTransition();
    s.photon.reset();
    const AlkOrbital c = valenceNow();
    if (c.n == n && c.l == l) return true;
    s.valJ2Next = 0;                                             // 0.47.2: a placed electron takes the term's default J (graspDefaultJ2: the highest; the readout names it)
    s.last = "outer electron placed: " + valName(c.n, c.l) + " → " + valName(n, l) + "  ·  no photon (the lab placed it; a real atom needs light or a collision for this)";
    beginTransition(valIndex(p.Z, c.n, c.l), to, kJumpPlace);
    return true;
}
double AtomLab::valenceShape(int l, double cz) {                 // angular() of the m = 0 real orbital on the unit sphere over its value at the pole: P_l(cos theta), squared (top 1 at the poles)
    static const int kM0[4] = {0, 3, 4, 9};                      // (atom_core.cpp kAng: s, pz, dz2, fz3)
    if (l <= 0 || l > 3) return 1;
    const double sz = std::sqrt(std::max(0.0, 1 - cz * cz));
    const double q = angular(kM0[l], sz, 0, cz) / angular(kM0[l], 0, 0, 1);
    return q * q;
}
void AtomLab::startValenceJump(const std::string& kind, int toIdx, double nm) {
    const std::vector<AlkOrbital> orbs = alkaliOrbitals(p.Z);
    const AlkOrbital c = valenceNow(), t = orbs[std::size_t(toIdx)];
    AlkLine L{};
    bool got = false;
    if (nm > 0) for (const AlkLine& q : alkaliLinesBetween(p.Z, c.n, c.l, t.n, t.l)) if (std::fabs(q.nm() - nm) < 1e-6) { L = q; got = true; break; }   // the clicked component
    const bool byJ = p.mrel != "Non-relativistic" && graspHas(p.Z);   // 0.47.2: the level has a J: only the lines from that J (from 3p 2P3/2 only D2)
    if (got && byJ) s.valJ2 = (L.nLo == c.n && L.lLo == c.l) ? L.j2Lo : L.j2Hi;   // a clicked component starts from its own level
    if (!got && !alkaliPickLine(p.Z, c.n, c.l, t.n, t.l, random(), L, byJ ? valenceJ2() : 0)) return;
    s.valJ2Next = (L.nLo == t.n && L.lLo == t.l) ? L.j2Lo : L.j2Hi;
    const int from = valIndex(p.Z, c.n, c.l);
    const double lam = L.nm(), E = kHcEVnm / lam;
    const std::string col = lamColorCss(lam);
    const auto jt = [](int j2) { return std::to_string(j2) + "/2"; };
    const std::string comp = valName(L.nLo, L.lLo) + " " + jt(L.j2Lo) + " – " + valName(L.nHi, L.lHi) + " " + jt(L.j2Hi);
    const Photon ph{kind, from, toIdx, E, lam, comp, col, random() * kPi * 2, 0, 1.6, jumpDuration(kJumpEmit), false};
    s.photon = ph;                                               // 0.49.2: an emitted photon leaves during its jump, which starts now
    if (std::none_of(s.lines.begin(), s.lines.end(), [&](const SeenLine& q) { return std::fabs(q.lam - lam) < 0.005; })) s.lines.push_back({lam, col});
    s.last = "last photon: " + std::string(kind == "emit" ? "emitted" : "absorbed") + "  ·  outer electron " + valName(c.n, c.l) + " → " + valName(t.n, t.l) + "  ·  " + fmt::fixed(lam, 3) + " nm (" + vacuumBand(lam) +   // (0.46.1: one bracket, was "(vacuum) (UV) (...")
             "; " + comp + ", measured, NIST ASD" + (std::isfinite(L.aki) ? "; A = " + sci(L.aki) + " /s" : "") + ")  ·  " + fmtEV(E);
    if (kind == "emit") beginTransition(from, toIdx, kJumpEmit);
}
bool AtomLab::emitValenceLine(int nHi, int lHi, int nLo, int lLo, double nm) {
    if (!valenceOn() || s.trans || s.photon || valIndex(p.Z, nHi, lHi) < 0 || valIndex(p.Z, nLo, lLo) < 0 || alkaliLinesBetween(p.Z, nHi, lHi, nLo, lLo).empty()) return false;
    const AlkOrbital c = valenceNow();
    if (c.n != nHi || c.l != lHi) setValence(nHi, lHi);
    startValenceJump("emit", valIndex(p.Z, nLo, lLo), nm);
    return true;
}
// ---- 0.47.3: level jumps of the atoms with several outer electrons (levels.h; PLAN_JUMPS.md part 2) ----
bool AtomLab::levelOn() const { return manyOn() && p.mrel != "Non-relativistic" && levelJumps(p.Z); }
int AtomLab::zeemanKind() const { return levelOn() ? 2 : valenceOn() ? 1 : !manyOn() ? 0 : -1; }
int AtomLab::zeemanLine() const {
    const int k = zeemanKind();
    if (k < 0) return -1;
    const int n = static_cast<int>(zeeman::labLines(p.Z, p.N, k).size());
    if (p.zline >= 0 && p.zline < n) return p.zline;
    int nNow = p.n, lNow = -1;                                   // the one-electron atom: the shell shown (Orbit view: n only)
    if (k == 0 && p.view != "Orbit" && orbIndex(p.orb) >= 0) { nNow = orbitals()[std::size_t(orbIndex(p.orb))].n; lNow = orbitals()[std::size_t(orbIndex(p.orb))].l; }
    if (k == 1) { nNow = valenceNow().n; lNow = valenceNow().l; }
    return zeeman::defaultLine(p.Z, p.N, k, levelNow(), nNow, lNow);
}
std::vector<int> AtomLab::levelTargets(int dir) const {
    std::vector<int> out;
    if (!levelOn()) return out;
    const std::vector<JLevel>& lv = jumpLevels(p.Z);
    const std::vector<JLine>& ln = jumpLines(p.Z);
    const double e0 = lv[std::size_t(s.lvl)].cm;
    for (const int k : linesAt(p.Z, s.lvl)) {
        if (!jumpOffered(p.Z, k)) continue;
        const JLine& L = ln[std::size_t(k)];
        const int o = L.lo == s.lvl ? L.hi : L.lo;
        const double e = lv[std::size_t(o)].cm;
        if ((dir < 0 ? e < e0 : e > e0) && std::find(out.begin(), out.end(), o) == out.end()) out.push_back(o);
    }
    return out;
}
bool AtomLab::placeLevel(int i) {
    if (!levelOn() || !levelCloud(p.Z, i)) return false;
    if (s.trans) finishTransition();
    s.photon.reset();
    if (i == s.lvl) return true;
    s.last = "atom placed: " + levelName(p.Z, s.lvl) + " → " + levelName(p.Z, i) + "  ·  no photon (the lab placed it; a real atom needs light or a collision for this)";
    beginTransition(s.lvl, i, kJumpPlace);
    return true;
}
void AtomLab::startLevelJump(const std::string& kind, int to, int line) {
    const std::vector<JLine>& ln = jumpLines(p.Z);
    if (line < 0) { const std::vector<int> b = linesBetween(p.Z, s.lvl, to); if (b.empty()) return; line = b[0]; }
    const JLine& L = ln[std::size_t(line)];
    const int from = s.lvl;
    const double lam = L.nm(), E = kHcEVnm / lam;
    const std::string col = lamColorCss(lam);
    const std::string comp = levelName(p.Z, L.hi) + " – " + levelName(p.Z, L.lo);
    const Photon ph{kind, from, to, E, lam, comp, col, random() * kPi * 2, 0, 1.6, jumpDuration(kJumpEmit), false};
    s.photon = ph;                                               // 0.49.2: an emitted photon leaves during its jump, which starts now
    if (std::none_of(s.lines.begin(), s.lines.end(), [&](const SeenLine& q) { return std::fabs(q.lam - lam) < 0.005; })) s.lines.push_back({lam, col});
    s.last = "last photon: " + std::string(kind == "emit" ? "emitted" : "absorbed") + "  ·  " + levelName(p.Z, from) + " → " + levelName(p.Z, to) + "  ·  " + fmt::fixed(lam, 3) + " nm (" + vacuumBand(lam) +
             "; " + std::string(L.observed() ? "measured" : "from the measured levels (Ritz)") + ", NIST ASD; " + rateText(L) + ")  ·  " + fmtEV(E);
    if (kind == "emit") beginTransition(from, to, kJumpEmit);
}
bool AtomLab::emitLevelLine(int line) {
    if (!levelOn() || s.trans || s.photon || !jumpOffered(p.Z, line)) return false;
    const JLine& L = jumpLines(p.Z)[std::size_t(line)];
    if (s.lvl != L.hi) { s.lvl = L.hi; s.k = 0; s.sliceFresh = 0; s.ptsEpoch = nextEpoch(); }
    startLevelJump("emit", L.lo, line);
    return true;
}
bool AtomLab::absorbLevelLine(int line) {
    if (!levelOn() || s.trans || s.photon || !jumpOffered(p.Z, line)) return false;
    const JLine& L = jumpLines(p.Z)[std::size_t(line)];
    if (s.lvl != L.lo) { s.lvl = L.lo; s.k = 0; s.sliceFresh = 0; s.ptsEpoch = nextEpoch(); }
    startLevelJump("absorb", L.hi, line);
    return true;
}
void AtomLab::setRound(bool on) { if (s.round == on) return; s.round = on; s.k = 0; s.sliceFresh = 0; s.ptsEpoch = nextEpoch(); }
std::string AtomLab::levelShapeNote() const {
    const GraspLevel* g = levelCloud(p.Z, s.lvl);
    if (!g) return "";
    if (g->j2 == 0) return "J = 0: this level is round in every state (no direction to line up)";
    if (g->j2 == 1) return "J = 1/2: this level is round in every M_J state";
    if (s.round) return "drawn round: the free atom, every M_J state alike (the average over directions keeps only the round part D_0); \"Lined up\" shows one M_J state";
    return "drawn lined up: the state M_J = J, as atoms lined up by a magnetic field or polarized light are (the shape along z); a free atom with no field is the round average (\"Round\")";
}

// The new dots of a valence jump: independent samples of the atom with the electron in the target orbital (the whole atom, or the shown orbital in that atom), coloured as the cloud is.
// Core dots are new samples of the same core density, so the core looks the same throughout; the outer electron's part changes shape. Its own generator, as buildJump.
// 0.89.0: built in fixed chunks of kManyJumpChunk dots. Chunk c has its own random stream, seeded from the jump's seed and c (manyJumpChunkSeed), so a dot depends only on its
// index: the same dots for any number of threads and any split over frames (tested exactly for 1, 2, 8 and 16 threads). The chunks run on up to manyJumpThreads threads.
// The sampler's lazy caches (hf.cpp prep, AtomLab::enlargedRadius, manyExtM) are filled on this thread first; inside the threads they are only read.
int AtomLab::manyJumpThreadsOverride = 0;
std::uint32_t AtomLab::manyJumpChunkSeed(std::uint32_t base, long chunk) {     // splitmix32-style mixing of the jump's seed and the chunk number
    std::uint32_t z = base + 0x9E3779B9u * (std::uint32_t(chunk) + 1u);
    z = (z ^ (z >> 16)) * 0x85EBCA6Bu;
    z = (z ^ (z >> 13)) * 0xC2B2AE35u;
    return z ^ (z >> 16);
}
int AtomLab::manyJumpThreads() {
    if (manyJumpThreadsOverride > 0) return manyJumpThreadsOverride;
    return std::clamp(static_cast<int>(std::thread::hardware_concurrency()) - 1, 1, 16);   // (one core left for the frame's other work)
}
void AtomLab::buildManyJump(long upTo) {
    AtomState::JumpBuild& b = s.jb;
    upTo = std::min(s.jn, ((std::min(upTo, s.jn) + kManyJumpChunk - 1) / kManyJumpChunk) * kManyJumpChunk);   // (whole chunks only; the last may be short)
    if (b.done >= upTo) return;
    const hf::Atom* a = nullptr;
    if (levelOn()) { if (const GraspLevel* g = levelCloud(p.Z, b.to)) a = &g->atom; }   // 0.47.3
    else {
        const std::vector<AlkOrbital> orbs = alkaliOrbitals(p.Z);
        const AlkOrbital t = orbs[std::size_t(b.to)];
        a = alkaliAtom(p.Z, t.n, t.l, p.mrel != "Non-relativistic", s.valJ2Next > 0 ? s.valJ2Next : -1);
    }
    if (!a) { b.done = upTo; return; }
    const int one = manySubOf(*a), vi = valenceSub(*this, *a);
    const GraspLevel* gl = graspOfAtom(a);
    const std::vector<int> vset = levelValenceSet(*this, *a);
    const bool slice = sliceDots();
    const int axis = sliceAxisNow();
    const double off = slice ? sliceOffNow() : 0;
    hf::Plane pl;
    if (slice) pl = hf::planeOf(*a, off);
    for (const hf::Subshell& sh : a->sub) (void)hf::radialCdf(sh, 1.0);     // (fills every subshell's table: sampleRadius, radialR, totalRadial only read them now)
    if (p.rscale == "Shells enlarged") (void)enlargedRadius(*a, 1.0);
    const long c0 = b.done / kManyJumpChunk, c1 = (upTo + kManyJumpChunk - 1) / kManyJumpChunk;
    const auto chunk = [&](long c) {
        orbit::Mulberry32 r(manyJumpChunkSeed(b.rng, c));
        const long i1 = std::min(upTo, (c + 1) * kManyJumpChunk);
        for (long i = c * kManyJumpChunk; i < i1; i++) {
            double x = 0, y = 0, z = 1e4;
            int shell = -1;
            bool ok = false;
            for (int k = 0; k < 4096 && !ok; k++) ok = manyDot(*this, *a, one, vi, gl, slice ? &pl : nullptr, axis, r, x, y, z, shell, &vset);
            signed char sg = 1; float ph = 0;
            if (ok) manyColour(*a, one, shell, x, y, z, sg, ph);
            else { x = 0; y = axis == 1 ? off + 1e4 : 0; z = axis == 2 ? off + 1e4 : 0; }
            s.jp[std::size_t(3 * i)] = float(x * kA0); s.jp[std::size_t(3 * i + 1)] = float(y * kA0); s.jp[std::size_t(3 * i + 2)] = float(z * kA0);
            s.jph[std::size_t(i)] = ph; s.jsg[std::size_t(i)] = sg;
        }
    };
    const int nth = static_cast<int>(std::min<long>(manyJumpThreads(), c1 - c0));
    if (nth <= 1) { for (long c = c0; c < c1; c++) chunk(c); }
    else {
        std::atomic<long> next{c0};
        const auto work = [&] { for (long c = next++; c < c1; c = next++) chunk(c); };
        std::vector<std::thread> pool;
        pool.reserve(std::size_t(nth - 1));
        for (int t = 1; t < nth; t++) pool.emplace_back(work);
        work();
        for (std::thread& t : pool) t.join();
    }
    b.done = upTo;
}
double AtomLab::manyExtM() const {                              // 0.39: 1.7 times the radius that holds 90 % of the most extended orbital shown (the one-electron views use kExt, which is about
    double r90 = 0;                                              // 1.7 times the 90 % radius of 1s: 4.5 against 2.66 a0)
    const hf::Atom& a = *manyAtom();
    const int only = manySub();
    static const hf::Atom* cA = nullptr;                         // 0.39.1: kept for the atom and orbital it was worked out for (it was worked out again every frame: about 1,000 exact
    static int cOnly = -2;                                       // evaluations of the distribution function, 60 halvings per shell)
    static double cExt = 0;
    if (cA == &a && cOnly == only) return cExt;
    for (int i = 0; i < int(a.sub.size()); i++) {
        const hf::Subshell& sh = a.sub[std::size_t(i)];
        if (!sh.occ || (only >= 0 && i != only)) continue;
        double lo = 0, hi = 200;
        for (int it = 0; it < 60; it++) { const double mid = 0.5 * (lo + hi); (hf::radialCdf(sh, mid) < 0.9 ? lo : hi) = mid; }
        r90 = std::max(r90, hi);
    }
    cA = &a; cOnly = only; cExt = 1.7 * r90 * kA0;
    return cExt;
}
std::vector<AtomLab::Ring> AtomLab::manyRings() const {         // 0.39.1: the shells K, L, M ... of the atom for the Orbit view (kept per atom)
    static const hf::Atom* cA = nullptr;
    static std::vector<Ring> cR;
    const hf::Atom* a = manyAtom();
    if (!a) return {};
    if (a == cA) return cR;
    std::vector<Ring> v;
    for (const hf::Subshell& sh : a->sub) {
        if (!sh.occ) continue;
        if (int(v.size()) < sh.n) v.resize(std::size_t(sh.n));
        Ring& g = v[std::size_t(sh.n - 1)];
        g.n = sh.n; g.electrons += sh.occ; g.r += sh.occ * hf::meanRadius(sh);
    }
    cR.clear();
    for (Ring& g : v) if (g.electrons) { g.r /= g.electrons; cR.push_back(g); }
    cA = a;
    return cR;
}
// 0.39: the whole atom's dots are almost all near the nucleus (iron: 24 of 26 electrons within about 1 Bohr radius, its 4s pair out to 6), so at true scale the inner shells are one
// bright point. "Shells enlarged" draws each dot at radius R asinh(r / r0) / asinh(R / r0): R = the 90 % radius of the outermost shell (so the outer edge stays at its true size),
// r0 = the 90 % radius of 1s; the mapping only grows, so the order of the shells and every direction are kept. The scale bar is then true only at the edge (the info line says so).
double AtomLab::drawnRadius(double r) const { return shellsEnlarged() ? enlargedRadius(*manyAtom(), r) : r; }
double AtomLab::enlargedRadius(const hf::Atom& a, double r) {
    const auto r90 = [](const hf::Subshell& sh) { double lo = 0, hi = 200; for (int it = 0; it < 60; it++) { const double mid = 0.5 * (lo + hi); (hf::radialCdf(sh, mid) < 0.9 ? lo : hi) = mid; } return hi; };
    static const hf::Atom* cached = nullptr;
    static double R = 1, r0 = 1;
    if (cached != &a) {
        cached = &a; R = 0; r0 = 1e9;
        for (const hf::Subshell& sh : a.sub) if (sh.occ) { const double v = r90(sh); R = std::max(R, v); if (sh.n == 1) r0 = v; }
    }
    return R * std::asinh(r / r0) / std::asinh(R / r0);
}
double AtomLab::cloudExtM() const { return manyOn() ? manyExtM() : kExt[sizeN()] / p.Z * aUnit(); }
double AtomLab::shellPhase(int n) {                              // K red, L ... spread over the wheel so that neighbouring shells differ most: 0, 3/7, 6/7, 2/7, 5/7, 1/7, 4/7 of a turn
    static const int step[8] = {0, 0, 3, 6, 2, 5, 1, 4};
    return 2 * kPi * (step[std::max(1, std::min(7, n))] + 0.5) / 7.0 - kPi;
}
// One dot of the many-electron cloud: the whole atom (a subshell by its share of the electrons, its Hartree-Fock radius, a uniform direction: exact, hf.cpp), or the chosen orbital
// (its HF radius and the direction of the one-electron orbital of the same l and orientation: the angular part is the same function).
// 0.47: the subshell of the outer electron (an alkali atom's one singly filled subshell) when valence jumps are on, else -1
static int valenceSub(const AtomLab& L, const hf::Atom& a) {
    if (!L.valenceOn()) return -1;
    for (std::size_t i = a.sub.size(); i-- > 0;) if (a.sub[i].occ == 1) return int(i);
    return -1;
}
// 0.47.3: a level atom's outer electrons: the subshells (natural orbitals, not the lumped ones) outside the closed shells of its ground level (every subshell of the ground level but
// its outer s shell), holding more than 0.02 electron. Their kValenceDotShare of the dots, as the alkalis' outer electron (the readout says so; 0.54.1: none with True share).
static std::vector<int> levelValenceSet(const AtomLab& L, const hf::Atom& a) {
    std::vector<int> v;
    if (!L.levelOn()) return v;
    const GraspLevel* g0 = levelCloud(a.Z, 0);
    if (!g0) return v;
    int ns = 0;                                                  // the ground level's outer s shell
    for (const hf::Subshell& sh : g0->atom.sub) if (sh.l == 0 && !sh.lumped && sh.focc > 1.5) ns = std::max(ns, sh.n);
    for (int i = 0; i < int(a.sub.size()); i++) {
        const hf::Subshell& sh = a.sub[std::size_t(i)];
        if (sh.lumped || sh.focc < 0.02) continue;
        const bool core = !(sh.l == 0 && sh.n == ns) && std::any_of(g0->atom.sub.begin(), g0->atom.sub.end(), [&](const hf::Subshell& c) { return c.n == sh.n && c.l == sh.l && !c.lumped && c.focc > 1.5; });
        if (!core) v.push_back(i);
    }
    return v;
}
// 0.47: a direction drawn from |Y_l0|^2 (the real m = 0 orbital pointed along z): cos theta uniform, kept with odds P_l(cos theta)^2 (about 2l + 1 tries), the azimuth uniform
static void valenceDir(int l, orbit::Mulberry32& r, double& ux, double& uy, double& uz) {
    double cz = 2 * r.next() - 1;
    for (int t = 0; t < 10000 && l > 0 && !(r.next() < AtomLab::valenceShape(l, cz)); t++) cz = 2 * r.next() - 1;
    const double ph = 2 * kPi * r.next(), sz = std::sqrt(std::max(0.0, 1 - cz * cz));
    ux = sz * std::cos(ph); uy = sz * std::sin(ph); uz = cz;
}
static bool manyDot(const AtomLab& L, const hf::Atom& a, int one, int vi, const GraspLevel* gl, const hf::Plane* plane, int axis, orbit::Mulberry32& r, double& x, double& y, double& z, int& shell,
                    const std::vector<int>* vset, bool alone) {
    if (plane && alone && one < 0 && (vi >= 0 || (vset && !vset->empty()))) {
        // 0.91.0, the Slice's "Outer electron alone": every dot is the outer electron's (an alkali's one outer subshell, or a level atom's outer subshells picked by their mass
        // on the plane, so the kept dots have their summed density there). Its radius on the plane exactly as samplePlane draws it; its shape as the whole-atom Slice keeps it
        // (the aligned state's w / bound at its radius; an alkali's frozen-core orbital its m = 0 shape P_l^2). The core's dots are left out.
        int pick = vi;
        if (pick < 0) {
            double tot = 0;
            for (const int i : *vset) tot += plane->mass[std::size_t(i)];
            if (!(tot > 0)) return false;
            double u = r.next() * tot;
            pick = vset->back();
            for (const int i : *vset) { u -= plane->mass[std::size_t(i)]; if (u < 0) { pick = i; break; } }
        }
        double rad = 0;
        if (!hf::samplePlane(a, *plane, pick, r, rad, shell) || !(rad > 0)) return false;
        const double off = plane->off, rho = std::sqrt(std::max(0.0, rad * rad - off * off)), ph = 2 * kPi * r.next();
        const double u = rho * std::cos(ph), v = rho * std::sin(ph);
        if (axis == 2) { x = u; y = v; z = off; } else { x = u; y = off; z = v; }
        if (gl) {
            if (gl->ks.empty() || L.s.round) return true;
            double bound = 1;
            const double w = graspShape(*gl, rad, z / rad, bound);
            return r.next() * bound <= w;
        }
        const int l = a.sub[std::size_t(shell)].l;
        return l == 0 || r.next() < AtomLab::valenceShape(l, z / rad);
    }
    if (plane) {                                                 // 0.39.1, the Slice view: ON the plane (hf.h, samplePlane: exact for the whole atom; one orbital: its plane envelope, then the angular
        double rad = 0;                                          // part kept with probability |P(u)|^2 / M, as tryStateOnPlane does for one electron)
        if (!hf::samplePlane(a, *plane, one, r, rad, shell) || !(rad > 0)) return false;
        const double off = plane->off, rho = std::sqrt(std::max(0.0, rad * rad - off * off)), ph = 2 * kPi * r.next();
        const double u = rho * std::cos(ph), v = rho * std::sin(ph);
        if (axis == 2) { x = u; y = v; z = off; } else { x = u; y = off; z = v; }   // (the same axes as cloud_sample.cpp, onPlane)
        if (one < 0 && gl) {                                     // 0.47.2: the whole atom's aligned-state density on the plane: a dot kept with odds w / bound at its own radius (exact)
            if (gl->ks.empty() || L.s.round) return true;       // (0.47.3, D6: the round free-atom view keeps D_0 only)
            double bound = 1;
            const double w = graspShape(*gl, rad, z / rad, bound);
            return r.next() * bound <= w;
        }
        if (one < 0) {
            // 0.47: the outer electron with its m = 0 shape: its density on the plane is (round density) x (2l+1) P_l(cos theta)^2, whose top is 2l+1 times the round one. A dot of the
            // outer electron is kept with odds P_l^2, a core dot with odds 1 / (2l+1): the kept dots then have the density (core + outer x (2l+1) P_l^2) / (2l+1), the exact oriented sum.
            if (vi < 0 || a.sub[std::size_t(vi)].l == 0) return true;
            const int l = a.sub[std::size_t(vi)].l;
            return r.next() < (shell == vi ? AtomLab::valenceShape(l, z / rad) : 1.0 / (2 * l + 1));
        }
        const int id = L.stateId();
        const Orbital& o = orbitals()[stateOrbital(id)];
        const double ux = x / rad, uy = y / rad, uz = z / rad;
        const double a2 = isComplexState(id) ? std::norm(angularComplex(o.l, stateM(id), ux, uy, uz)) : [&] { const double q = angular(o.ang, ux, uy, uz); return q * q; }();
        return r.next() * angularBound(id) <= a2;
    }
    if (one < 0 && gl) {
        // 0.47.2: GRASP level: the outer electron's extra share of dots as before (its natural orbital's radius); every dot's direction from the aligned state's density at its radius
        // (the k > 0 part cannot be split per electron; at the outer electron's radii it is its shape, inside the round core it fades). J = 1/2: every direction alike.
        // 0.47.3: a level atom's outer electrons (vset: several subshells) take that share together, each by its occupation; D6: the round view draws every direction alike
        double rad = 0;
        if (vi >= 0 && r.next() < L.extraOuterShare()) { shell = vi; rad = hf::sampleRadius(a.sub[std::size_t(vi)], r); }
        else if (vset && !vset->empty() && r.next() < L.extraOuterShare()) {
            double tot = 0;
            for (const int i : *vset) tot += a.sub[std::size_t(i)].focc;
            double u = r.next() * tot;
            shell = vset->back();
            for (const int i : *vset) { u -= a.sub[std::size_t(i)].focc; if (u < 0) { shell = i; break; } }
            rad = hf::sampleRadius(a.sub[std::size_t(shell)], r);
        }
        else { hf::sampleAtom(a, r, x, y, z, shell); if (gl->ks.empty() || L.s.round) return true; rad = std::sqrt(x * x + y * y + z * z); }
        double ux, uy, uz;
        if (L.s.round) { const double cz = 2 * r.next() - 1, ph = 2 * kPi * r.next(), sz = std::sqrt(std::max(0.0, 1 - cz * cz)); ux = sz * std::cos(ph); uy = sz * std::sin(ph); uz = cz; }
        else graspDirection(*gl, rad, r, ux, uy, uz);            // (no k > 0 term: every direction alike)
        x = rad * ux; y = rad * uy; z = rad * uz;
        return true;
    }
    if (one < 0) {
        // 0.46.1 (outside review: the outer electron was almost invisible in the whole atom, so its jumps hardly showed): with valence jumps on, kValenceDotShare of the dots are the outer electron's
        // (its own exact radial density; until 0.47 every direction alike), the rest the whole atom's sum as before. The readout says so; the Slice view keeps the true shares.
        // 0.54.1: with "Outer electron's dots: True share" (extraOuterShare 0) every dot is the whole atom's sum: the outer electron has its true share, 1 in Z.
        // 0.47: its direction is drawn from the real m = 0 orbital (pz, dz2, fz3; s round), as One orbital shows it (valenceDir). The core is round, as before.
        if (vi >= 0 && r.next() < L.extraOuterShare()) {
            shell = vi;
            const double rad = hf::sampleRadius(a.sub[std::size_t(vi)], r);
            valenceDir(a.sub[std::size_t(vi)].l, r, x, y, z);
            x *= rad; y *= rad; z *= rad;
            return true;
        }
        hf::sampleAtom(a, r, x, y, z, shell);
        if (vi >= 0 && shell == vi && a.sub[std::size_t(vi)].l > 0) {   // the outer electron's own share of the whole-atom dots: the same shape (the radius kept; direction and radius are independent)
            const double rad = std::sqrt(x * x + y * y + z * z);
            valenceDir(a.sub[std::size_t(vi)].l, r, x, y, z);
            x *= rad; y *= rad; z *= rad;
        }
        return true;
    }
    shell = one;
    const double rad = hf::sampleRadius(a.sub[std::size_t(one)], r);
    double ux, uy, uz;
    sampleDirection(L.stateId(), r, ux, uy, uz);
    x = rad * ux; y = rad * uy; z = rad * uz;
    return true;
}

bool AtomLab::tryStatePoint(int id, orbit::Mulberry32& r, double& x, double& y, double& z, int diracJ2) const {
    if (diracJ2 > 0) {                                           // 0.49.3: the Dirac state of id's n and l with this j (dirac_cloud.h), lined up (m = j) or round
        const Orbital& o = orbitals()[std::size_t(stateOrbital(id))];
        if (sliceDots()) return tryDiracOnPlane(p.Z, o.n, o.l, diracJ2, s.round, sliceAxisNow(), sliceOffNow(), r, x, y, z);
        sampleDirac(p.Z, o.n, o.l, diracJ2, s.round, r, x, y, z);
        return true;
    }
    CloudPoint q;
    if (sliceDots()) { if (!tryStateOnPlane(id, p.Z, sliceAxisNow(), sliceOffNow(), r, q)) return false; }
    else q = sampleState(id, p.Z, r);
    x = q.x; y = q.y; z = q.z;
    return true;
}

// The superposition: an orthogonal pair is shown from the time-independent mixture q = (|A|^2 + |B|^2) / 2 (A, B normalised) and thinned on screen to the density at the beat phase
// (superShown: exact, see the note at recordDot below). In space each state holds probability 1, so the mixture picks A or B with odds 1/2 each and samples it. On a plane the two states hold
// different amounts, so each attempt picks a state with odds in proportion to its envelope's mass on the plane (planeMass) and tries one point of it: an accepted point then has the density
// |A|^2 + |B|^2 on the plane (a mixture of envelopes, each thinned to its own state). An overlapping pair (one energy, static) is |a A + b B|^2 <= (a^2 + b^2)(|A|^2 + |B|^2) = 2 q
// (Cauchy-Schwarz): a point of q is kept with probability |a A + b B|^2 / (|A|^2 + |B|^2).
bool AtomLab::tryDot(orbit::Mulberry32& r, double& x, double& y, double& z) {
    if (manyNow_) return manyDot(*this, *manyNow_, manySubNow_, valSubNow_, gNow_, sliceDots() ? &manyPlane_ : nullptr, sliceAxisNow(), r, x, y, z, manyShell_, &valSetNow_, aloneNow_);
    if (batchNow_ && diracJ2Now_ > 0) {                           // 0.65.0: the Dirac state and the cut found once per batch (tryStatePoint's numbers, the same dots)
        if (sliceDots()) return tryDiracOnPlane(p.Z, diracNNow_, diracLNow_, diracJ2Now_, s.round, axisNow_, offNow_, r, x, y, z);
        sampleDirac(p.Z, diracNNow_, diracLNow_, diracJ2Now_, s.round, r, x, y, z);
        return true;
    }
    if (!superOn()) return tryStatePoint(stateId(), r, x, y, z, diracCloudOn() ? diracJ2() : 0);
    double wA = 0.5;
    if (sliceDots()) {
        const double mA = planeMass(sup_.idA, p.Z, sliceAxisNow(), sliceOffNow()), mB = planeMass(sup_.idB, p.Z, sliceAxisNow(), sliceOffNow());
        if (!(mA + mB > 0)) return false;
        wA = mA / (mA + mB);
    }
    const int id = r.next() < wA ? sup_.idA : sup_.idB;
    if (!tryStatePoint(id, r, x, y, z)) return false;
    return !overlap_ || overlapKeep(r, x, y, z);
}
// an overlapping pair's point of q kept with probability |a A + b B|^2 / (|A|^2 + |B|^2) (one more random number; see tryDot)
bool AtomLab::overlapKeep(orbit::Mulberry32& r, double x, double y, double z) const {
    const auto amp = [&](int s_, double nf) { return nf * (isComplexState(s_) ? psiComplex(s_, p.Z, x, y, z) : std::complex<double>(psi(s_, p.Z, x, y, z), 0)); };
    const std::complex<double> A = amp(sup_.idA, nA_), B = amp(sup_.idB, nB_);
    const double qq = std::norm(A) + std::norm(B);
    return !(!(qq > 0) || r.next() * qq > std::norm(sup_.a * A + sup_.b * B));
}

// Quantum beats: the dots are samples of the time-INDEPENDENT mixture q = (|A|^2 + |B|^2) / 2 (A and B normalised). The density at beat phase theta is |a A + b B e^(-i theta)|^2, and
// |a A + b B e^(-i theta)|^2 <= (a|A| + b|B|)^2 <= (a^2 + b^2)(|A|^2 + |B|^2) = 2 q (Cauchy-Schwarz), so a dot at x is shown with probability w / 2, w = |psi(x, theta)|^2 / q:
// the shown dots are then exact samples of the CURRENT density, whatever the time, and no dot carries any memory of the past. (A fixed random number per dot decides, so
// a dot appears and disappears smoothly as the density at its place rises and falls.) An overlapping pair is sampled from its own density, and every dot is shown.
// 0.46: a many-electron dot's sign and colour phase (and, for the whole atom, the enlarged-shells mapping of its place), for atom a and its shown orbital `sub` (-1: the whole atom; shell:
// the subshell the dot came from). recordDot uses it for the cloud, buildManyJump for the new dots of a valence jump (another atom), so both are coloured the same way.
void AtomLab::manyColour(const hf::Atom& a, int sub, int shell, double& x, double& y, double& z, signed char& sg, float& ph) const {
        if (sub < 0) {                                           // the whole atom: the colour of the dot's shell (no sign: a sum of densities has none)
            sg = 1;
            ph = float(shellPhase(shell >= 0 ? a.sub[std::size_t(shell)].n : 1));
            const double rr = std::sqrt(x * x + y * y + z * z);
            if (rr > 0 && p.rscale == "Shells enlarged") {     // (0.39.1: no mode checks per dot)
                const double k = enlargedRadius(a, rr) / rr;
                if (!sliceDots()) { x *= k; y *= k; z *= k; }
                else if (sliceAxisNow() == 2) { x *= k; y *= k; } else { x *= k; z *= k; }   // Slice: spread within the plane only, so the dot stays on it
            }
        } else {                                                       // one orbital: the sign of its wave, radial (HF) times angular, as in the one-electron lab
            const hf::Subshell& sh = a.sub[std::size_t(sub)];
            const double rr = std::sqrt(x * x + y * y + z * z), rad = hf::radialR(sh, rr);
            const int id = stateId();
            if (isComplexState(id)) {                                  // 0.39.1: a complex orbital (m = +-1 ...) has a phase, not a sign: the wheel colour is its argument, as in the one-electron lab
                const std::complex<double> v = rad * angularComplex(orbitals()[stateOrbital(id)].l, stateM(id), x, y, z);   // (0.39 gave only 0 or pi, from the real part's sign)
                sg = v.real() >= 0 ? 1 : -1;
                ph = float(std::arg(v));
            } else {
                const double v = rad * angular(orbitals()[id].ang, x, y, z);
                sg = v >= 0 ? 1 : -1;
                ph = v >= 0 ? 0.f : float(kPi);
            }
        }
}

void AtomLab::recordDot(double x, double y, double z) {
    const long i = s.k % (batchNow_ ? capNow_ : std::min<long>(cloudCap(), long(s.sg.size())));   // (the arrays are sized by restart(); never write outside them)
    if (manyNow_) {
        double xx = x, yy = y, zz = z;
        manyColour(*manyNow_, manySubNow_, manyShell_, xx, yy, zz, s.sg[i], s.ph[i]);
        x = xx; y = yy; z = zz;
    } else if (starkNow_) {                                       // 0.93.0: the parabolic state's wave (complex where m is not 0: its phase is m phi)
        const std::complex<double> v = stark::psiPar(*static_cast<const stark::ParSampler*>(starkNow_), x, y, z);
        s.sg[i] = v.real() >= 0 ? 1 : -1;
        s.ph[i] = float(std::arg(v));
    } else if (superOn()) {
        const auto amp = [&](int s_, double nf) { return nf * (isComplexState(s_) ? psiComplex(s_, p.Z, x, y, z) : std::complex<double>(psi(s_, p.Z, x, y, z), 0)); };
        const std::complex<double> A = amp(sup_.idA, nA_), B = amp(sup_.idB, nB_);
        if (std::size_t(4 * i + 3) < s.sa.size()) { s.sa[4 * i] = float(A.real()); s.sa[4 * i + 1] = float(A.imag()); s.sa[4 * i + 2] = float(B.real()); s.sa[4 * i + 3] = float(B.imag()); }
        if (s.saDirtyHi <= s.saDirtyLo) { s.saDirtyLo = i; s.saDirtyHi = i + 1; } else { s.saDirtyLo = std::min(s.saDirtyLo, i); s.saDirtyHi = std::max(s.saDirtyHi, i + 1); }   // (for a GPU copy)
        s.sg[i] = 1; s.ph[i] = 0.f;
    } else if (diracCloudOn()) {                                 // 0.49.3: a Dirac wave has four components: no one sign or phase to colour, so every dot has the first colour
        s.sg[i] = 1; s.ph[i] = 0.f;
    } else {
        const int id = idNow_ >= 0 ? idNow_ : stateId();
        if (isComplexState(id)) {
            const std::complex<double> v = psiComplex(id, p.Z, x, y, z);
            s.sg[i] = v.real() >= 0 ? 1 : -1;                  // (the sign of the real part: only used where a sign is wanted)
            s.ph[i] = float(std::arg(v));                      // the phase of the wave here: the wheel colour
        } else {
            const double pv = psi(id, p.Z, x, y, z);
            s.sg[i] = pv >= 0 ? 1 : -1;                        // the sign of the wave here: the phase colour
            s.ph[i] = pv >= 0 ? 0.f : float(kPi);              // the same as a phase: 0 or pi
        }
    }
    const double u = batchNow_ ? unitNow_ : aUnit();
    s.pts[3 * i] = float(x * u); s.pts[3 * i + 1] = float(y * u); s.pts[3 * i + 2] = float(z * u);
    s.k++;
}

void AtomLab::syncSlicePlane() {
    if (sliceDots() && (s.sliceAxis != sliceAxisNow() || s.sliceOff != sliceOffNow())) { s.sliceAxis = sliceAxisNow(); s.sliceOff = sliceOffNow(); s.sliceFresh = 0; }
}

long AtomLab::addDots(long want) {
    cut_ = false;
    if (want <= 0 || s.sg.empty()) return 0;
    manyNow_ = manyOn() ? manyAtom() : nullptr;                  // 0.39.1: once per batch (each call searched the tables and compared orbital names: about 0.2 s per 48,000 dots)
    struct Batch { bool& on; ~Batch() { on = false; } } batch{batchNow_};   // 0.65.0: the ring's size and the length unit once per batch (recordDot)
    capNow_ = std::min<long>(cloudCap(), long(s.sg.size())); unitNow_ = aUnit(); batchNow_ = true;
    diracJ2Now_ = !manyNow_ && !superOn() && diracCloudOn() ? diracJ2() : 0;  // (0.65.0: a Dirac state looked up once per batch, not per attempt)
    if (diracJ2Now_ > 0) { const Orbital& o = orbitals()[std::size_t(stateOrbital(stateId()))]; diracNNow_ = o.n; diracLNow_ = o.l; axisNow_ = sliceAxisNow(); offNow_ = sliceOffNow(); }
    if (!manyNow_ && starkCloudOn()) return addDotsStark(want);  // 0.93.0: a parabolic state's cloud (every point kept)
    manySubNow_ = manyNow_ ? manySub() : -1;
    valSubNow_ = manyNow_ ? valenceSub(*this, *manyNow_) : -1;   // 0.47 (the outer electron's subshell, for its shape)
    gNow_ = manyNow_ && (valenceOn() || levelOn()) ? graspOfAtom(manyNow_) : nullptr;   // 0.47.2 (the aligned state's shape; 0.47.3: and a level atom's)
    valSetNow_ = manyNow_ ? levelValenceSet(*this, *manyNow_) : std::vector<int>();
    aloneNow_ = manyNow_ && sliceAlone();
    if (manyNow_ && sliceDots()) manyPlane_ = hf::planeOf(*manyNow_, sliceOffNow());   // (once per batch: about 3,000 exponentials, for the atom and plane of this moment)
    const bool one = !manyNow_ && !superOn() && !diracCloudOn();
    idNow_ = one ? stateId() : -1;                               // 0.57.0: the state looked up once per batch (it searched the orbitals by name for every dot: a third of the Slice's time)
    orbit::Mulberry32 r(s.crng);
    long got = 0;
    if (one && sliceDots()) {                                    // 0.57.0: the Slice's one-electron dots: the plane's part of an attempt worked out once (the same dots, bit for bit)
        const PlaneSampler ps = planeSampler(idNow_, p.Z, sliceAxisNow(), sliceOffNow());
        if (gpuSlice && sliceCard && sliceCard->ready() && ps.g0 > 0 && ps.tailPoly().size() <= 16) {   // 0.57.0: on the card (its start-up check passed)
            got = addDotsCard(ps, want);
            idNow_ = -1;
            return got;
        }
        cancelSliceJob();                                        // (the processor again: a batch in flight is forgotten; it used no numbers)
        for (long tries = 0; got < want && tries < 64 * want + 64; tries++) {
            if ((tries & 127) == 127 && pastDeadline()) { cut_ = true; break; }   // 0.66.0 (between attempts: the next call goes on with the same numbers)
            CloudPoint q;
            if (ps.tryPoint(r, q)) { recordDot(q.x, q.y, q.z); got++; }
        }
    } else if (superOn() && sliceDots()) {                       // 0.64.0: the Slice of a superposition: the planes' parts worked out once per batch, as for one state (the same dots, bit for bit)
        const int ax = sliceAxisNow();
        const double off = sliceOffNow(), mA = planeMass(sup_.idA, p.Z, ax, off), mB = planeMass(sup_.idB, p.Z, ax, off);
        if (mA + mB > 0) {                                       // (else every attempt is rejected, using no numbers, as tryDot)
            const double wA = mA / (mA + mB);
            const PlaneSampler pa = planeSampler(sup_.idA, p.Z, ax, off), pb = planeSampler(sup_.idB, p.Z, ax, off);
            // 0.64.0: on the card (the Slice's program, its start-up check passed with a superposition's states): an orthogonal pair only (an overlapping pair takes
            // one more number for a kept point, so its attempts have no fixed numbering); every attempt then takes 4 numbers (3 where the radius comes out 0)
            if (!overlap_ && gpuSlice && sliceCard && sliceCard->ready() && pa.g0 > 0 && pb.g0 > 0 && pa.tailPoly().size() <= 16 && pb.tailPoly().size() <= 16) {
                got = addDotsCardSuper(pa, pb, wA, want);
                idNow_ = -1;
                return got;
            }
            cancelSliceJob();
            for (long tries = 0; got < want && tries < 64 * want + 64; tries++) {
                if ((tries & 127) == 127 && pastDeadline()) { cut_ = true; break; }   // 0.66.0
                CloudPoint q;
                const PlaneSampler& ps = r.next() < wA ? pa : pb;
                if (!ps.tryPoint(r, q) || (overlap_ && !overlapKeep(r, q.x, q.y, q.z))) continue;
                recordDot(q.x, q.y, q.z); got++;
            }
        }
    } else {
    if (diracJ2Now_ > 0 && sliceDots() && gpuSlice && sliceCard && sliceCard->ready()) {   // 0.91.0: on the card (the Slice's Dirac program, its start-up check passed)
        got = addDotsCardDirac(want);
        idNow_ = -1;
        return got;
    }
    if (sliceJob_.job || sliceJob_.job2) cancelSliceJob();
    if (diracJ2Now_ > 0 && sliceDots()) {                        // 0.65.0: the Slice of a Dirac state: the same attempts and numbers, four radii side by side (dirac_cloud.h)
        const auto rec = [this](double x, double y, double z) { recordDot(x, y, z); };
        if (deadline_ <= 0)
            got = diracPlaneDots(p.Z, diracNNow_, diracLNow_, diracJ2Now_, s.round, axisNow_, offNow_, r, want, 64 * want + 64, rec);
        else                                                     // 0.66.0: 128 dots at a time, the budget checked between (an attempt's numbers are taken only by it: the same dots)
            while (got < want) {
                if (got > 0 && pastDeadline()) { cut_ = true; break; }
                const long w = std::min<long>(128, want - got), g = diracPlaneDots(p.Z, diracNNow_, diracLNow_, diracJ2Now_, s.round, axisNow_, offNow_, r, w, 64 * w + 64, rec);
                got += g;
                if (g < w) break;                                // (the attempts ran out: a plane with almost nothing)
            }
    } else
    for (long tries = 0; got < want && tries < 64 * want + 64; tries++) {     // (an attempt on a plane can be rejected; the cap only matters where the plane holds almost nothing)
        if ((tries & 127) == 127 && pastDeadline()) { cut_ = true; break; }   // 0.66.0
        double x, y, z;
        if (tryDot(r, x, y, z)) { recordDot(x, y, z); got++; }
    }
    }
    idNow_ = -1;
    s.crng = r.seed;
    s.sliceFresh += got;
    return got;
}

// 0.57.0: the Slice's one-electron dots made by the card (render/atom_card.h, shaders.h kSliceDotsCS), the frame never waiting for it. A batch of attempts
// is started from the generator's state of the moment for the plane of the moment; a later step takes its kept dots in the order of the attempts, as the
// processor's loop above would have made them from the same state (the generator is a counter: attempt t takes numbers 3t .. 3t + 2), so the cloud gets
// the same dots up to single-precision rounding (the start-up check and the tests: atom_card.h compareSlice). The difference the eye can meet: the dots
// of a batch arrive one step (a frame) after the plane moved, as the outline's contour does. A batch made for another state or cloud, or after the
// generator was used for something else, is dropped; dots made for a plane that has moved on are kept but not counted as the new plane's.
long AtomLab::addDotsCard(const PlaneSampler& ps, long want) {
    static std::vector<float> res;
    long got = 0, need = want;
    if (sliceJob_.idB >= 0) cancelSliceJob();                        // (a superposition's batch: not this state's)
    if (sliceJob_.job) {
        const render::JobState st = sliceCard->pollSlice(sliceJob_.job, res);
        if (st == render::JobState::Pending) return 0;
        const SliceJob j = sliceJob_;
        sliceJob_ = SliceJob{};
        if (st == render::JobState::Done && j.id == idNow_ && j.Z == p.Z && j.epoch == s.ptsEpoch && j.seed == s.crng && int(res.size()) >= 4 * j.tries) {
            std::uint64_t numbers = 3ull * std::uint64_t(j.tries);
            long kept = 0;
            for (int t = 0; t < j.tries; t++) {
                const float w = res[std::size_t(4 * t + 3)];
                if (w < -0.5f) { numbers = 3ull * std::uint64_t(t) + 2; break; }   // a radius of 0: the processor takes 2 numbers there; the rest is made again
                if (w > 0.5f) {
                    kept++;
                    recordDot(res[std::size_t(4 * t)], res[std::size_t(4 * t + 1)], res[std::size_t(4 * t + 2)]);
                    if (++got == j.want) { numbers = 3ull * std::uint64_t(t + 1); break; }
                }
            }
            s.crng = mulberryAfter(j.seed, numbers);
            sliceRate_ = std::max(0.002, double(std::max<long>(kept, 1)) / double(std::max<std::uint64_t>(1, numbers / 3)));
            sliceCardDots_ += got;
            if (j.axis == s.sliceAxis && j.off == s.sliceOff) { s.sliceFresh += got; need = want - got; }   // (dots of an older plane: not the new plane's)
        }
    }
    if (need > 0) {                                                  // the next batch, for the plane of the moment
        const int tries = int(std::min(262144.0, std::max(64.0, double(need) / sliceRate_ * 1.3 + 64)));
        const int job = sliceCard->beginSlice(sliceJobOf(ps, s.crng, tries));
        if (job) sliceJob_ = SliceJob{job, s.crng, tries, need, idNow_, p.Z, s.sliceAxis, s.sliceOff, s.ptsEpoch};
    }
    return got;
}

// 0.91.0: the Slice of a Dirac state made by the card (render/atom_card_types.h DiracSliceJob, shaders.h kDiracSliceCS), as addDotsCard: attempt t takes
// the numbers stride t .. (stride 3 with the keep test, else 2), so a batch's kept dots, in the order of the attempts, are diracPlaneDots's from the same
// state up to single-precision rounding (the start-up check and the tests: atom_card.h compareDirac). They arrive one step after the plane moved.
long AtomLab::addDotsCardDirac(long want) {
    static std::vector<float> res;
    long got = 0, need = want;
    const int key = 1 + ((diracNNow_ * 8 + diracLNow_) * 16 + diracJ2Now_) * 2 + (s.round ? 1 : 0);
    if (sliceJob_.job && sliceJob_.dirac != key) cancelSliceJob();    // (another kind of batch)
    DiracCase dc{p.Z, diracNNow_, diracLNow_, diracJ2Now_, s.round, axisNow_, offNow_, s.crng, 0};
    if (sliceJob_.job) {
        const render::JobState st = sliceCard->pollSlice(sliceJob_.job, res);
        if (st == render::JobState::Pending) return 0;
        const SliceJob j = sliceJob_;
        sliceJob_ = SliceJob{};
        const std::uint64_t stride = (s.round || (diracJ2Now_ - 1) / 2 == 0) ? 2 : 3;
        if (st == render::JobState::Done && j.dirac == key && j.Z == p.Z && j.epoch == s.ptsEpoch && j.seed == s.crng && int(res.size()) >= 4 * j.tries) {
            std::uint64_t numbers = stride * std::uint64_t(j.tries);
            long kept = 0;
            for (int t = 0; t < j.tries; t++) {
                const float w = res[std::size_t(4 * t + 3)];
                if (w < -0.5f) {                                     // a radius of 0: the processor keeps that dot and takes 2 numbers there; the rest is made again
                    recordDot(res[std::size_t(4 * t)], res[std::size_t(4 * t + 1)], res[std::size_t(4 * t + 2)]); got++;
                    numbers = stride * std::uint64_t(t) + 2; break;
                }
                if (w > 0.5f) {
                    kept++;
                    recordDot(res[std::size_t(4 * t)], res[std::size_t(4 * t + 1)], res[std::size_t(4 * t + 2)]);
                    if (++got == j.want) { numbers = stride * std::uint64_t(t + 1); break; }
                }
            }
            s.crng = mulberryAfter(j.seed, numbers);
            sliceRate_ = std::max(0.002, double(std::max<long>(kept, 1)) / double(std::max<std::uint64_t>(1, numbers / stride)));
            sliceCardDots_ += got;
            if (j.axis == s.sliceAxis && j.off == s.sliceOff) { s.sliceFresh += got; need = want - got; }   // (dots of an older plane: not the new plane's)
        }
    }
    if (need > 0) {                                                  // the next batch, for the plane of the moment
        dc.seed = s.crng;
        dc.tries = int(std::min(262144.0, std::max(64.0, double(need) / sliceRate_ * 1.3 + 64)));
        const render::DiracSliceJob dj = diracJobOf(dc);
        const int job = dj.tries > 0 ? sliceCard->beginDirac(dj) : 0;
        if (job) { sliceJob_ = SliceJob{job, s.crng, dc.tries, need, -1, p.Z, s.sliceAxis, s.sliceOff, s.ptsEpoch}; sliceJob_.dirac = key; }
    }
    return got;
}

// 0.93.0: a parabolic state's cloud (atom/stark.h samplePar: every point kept, 4 numbers each). The card's batch (render/atom_card_types.h StarkJob,
// shaders.h kStarkCS) where the Slice's start-up check passed (it checks this program too): point t takes the numbers 4t .. 4t + 3, so a batch's points are
// the processor's from the same state up to single-precision rounding (atom_card.h compareStark). They arrive one step after they were asked for.
namespace {
const stark::ParSampler& starkSampler(int n, int n1, int n2, int m) {
    static stark::ParSampler ps;
    static int key = -1;
    const int k = ((n * 16 + n1) * 16 + n2) * 32 + (m + 16);
    if (k != key) { ps = stark::parSampler(stark::Par{n, n1, n2, m}); key = k; }
    return ps;
}
}  // namespace
long AtomLab::addDotsStark(long want) {
    int n1 = 0, n2 = 0, m = 0;
    starkPar(n1, n2, m);
    const int n = orbitals()[std::size_t(fullOrb())].n;
    const stark::ParSampler& ps = starkSampler(n, n1, n2, m);
    struct Now { const void*& v; ~Now() { v = nullptr; } } now{starkNow_};
    starkNow_ = &ps;
    if (gpuSlice && sliceCard && sliceCard->ready()) return addDotsCardStark(want);
    if (sliceJob_.job || sliceJob_.job2) cancelSliceJob();
    orbit::Mulberry32 r(s.crng);
    long got = 0;
    for (; got < want; got++) {
        if ((got & 127) == 127 && pastDeadline()) { cut_ = true; break; }   // (between points: the next call goes on with the same numbers)
        double x = 0, y = 0, z = 0;
        stark::samplePar(ps, r, x, y, z);
        recordDot(x, y, z);
    }
    s.crng = r.seed;
    return got;
}
long AtomLab::addDotsCardStark(long want) {
    static std::vector<float> res;
    const stark::ParSampler& ps = *static_cast<const stark::ParSampler*>(starkNow_);
    const int key = -(1 + ((ps.s.n * 16 + ps.s.n1) * 16 + ps.s.n2) * 32 + (ps.s.m + 16));   // (below 0: never a Dirac key, which is above 0)
    if (sliceJob_.job && sliceJob_.dirac != key) cancelSliceJob();     // (another kind of batch)
    long got = 0;
    if (sliceJob_.job) {
        const render::JobState st = sliceCard->pollSlice(sliceJob_.job, res);
        if (st == render::JobState::Pending) return 0;
        const SliceJob j = sliceJob_;
        sliceJob_ = SliceJob{};
        if (st == render::JobState::Done && j.dirac == key && j.epoch == s.ptsEpoch && j.seed == s.crng && int(res.size()) >= 4 * j.tries) {
            const long take = std::min<long>(j.tries, j.want);
            for (long t = 0; t < take; t++) recordDot(res[std::size_t(4 * t)], res[std::size_t(4 * t + 1)], res[std::size_t(4 * t + 2)]);
            got = take;
            s.crng = mulberryAfter(j.seed, 4ull * std::uint64_t(take));
            sliceCardDots_ += got;
        }
    }
    const long need = want - got;
    if (need > 0) {
        StarkCase c{ps.s.n, ps.s.n1, ps.s.n2, ps.s.m, s.crng, static_cast<int>(std::min<long>(need, 262144))};
        const render::StarkJob sj = starkJobOf(c);
        const int job = sj.tries > 0 ? sliceCard->beginStark(sj) : 0;
        if (job) { sliceJob_ = SliceJob{job, s.crng, c.tries, need, -1, p.Z, s.sliceAxis, s.sliceOff, s.ptsEpoch}; sliceJob_.dirac = key; }
    }
    return got;
}

void AtomLab::cancelSliceJob() {
    if (sliceCard) { if (sliceJob_.job) sliceCard->cancelSlice(sliceJob_.job); if (sliceJob_.job2) sliceCard->cancelSlice(sliceJob_.job2); }
    sliceJob_ = SliceJob{};
}

// 0.64.0: the Slice of a superposition (an orthogonal pair) from the card: the processor's attempt t takes the number 4t to choose the state (A with odds wA:
// the planes' masses) and 4t + 1 .. 4t + 3 for that state's attempt (tryDot). The card makes both states' attempts for every t with that numbering (two
// batches of the Slice's own program, stride 4, first 1); here the choice is made from the same number in double precision, exactly as the processor makes
// it, and the chosen state's answer is taken. The same dots as the processor's up to single-precision rounding (the start-up check has both states' cases).
long AtomLab::addDotsCardSuper(const PlaneSampler& pa, const PlaneSampler& pb, double wA, long want) {
    static std::vector<float> resA, resB;
    long got = 0, need = want;
    if ((sliceJob_.job || sliceJob_.job2) && sliceJob_.idB < 0) cancelSliceJob();   // (one state's batch: not this superposition's)
    const auto pollOne = [&](int& job, std::vector<float>& res) {    // true while the batch is still on the card
        if (!job) return false;
        const render::JobState st = sliceCard->pollSlice(job, res);
        if (st == render::JobState::Pending) return true;
        job = 0;
        if (st == render::JobState::Failed) res.clear();
        return false;
    };
    const bool waitA = pollOne(sliceJob_.job, resA), waitB = pollOne(sliceJob_.job2, resB);
    if (waitA || waitB) return 0;
    if (sliceJob_.tries > 0) {                                       // both batches back
        const SliceJob j = sliceJob_;
        sliceJob_ = SliceJob{};
        if (j.id == sup_.idA && j.idB == sup_.idB && j.Z == p.Z && j.epoch == s.ptsEpoch && j.seed == s.crng && int(resA.size()) >= 4 * j.tries && int(resB.size()) >= 4 * j.tries) {
            std::uint64_t numbers = 4ull * std::uint64_t(j.tries);
            long kept = 0;
            for (int t = 0; t < j.tries; t++) {
                orbit::Mulberry32 ch(mulberryAfter(j.seed, 4ull * std::uint64_t(t)));
                const float* g = (ch.next() < j.wA ? resA.data() : resB.data()) + std::size_t(4 * t);
                if (g[3] < -0.5f) { numbers = 4ull * std::uint64_t(t) + 3; break; }   // a radius of 0: the processor takes 3 numbers there; the rest is made again
                if (g[3] > 0.5f) {
                    kept++;
                    recordDot(g[0], g[1], g[2]);
                    if (++got == j.want) { numbers = 4ull * std::uint64_t(t + 1); break; }
                }
            }
            s.crng = mulberryAfter(j.seed, numbers);
            sliceRate_ = std::max(0.002, double(std::max<long>(kept, 1)) / double(std::max<std::uint64_t>(1, numbers / 4)));
            sliceCardDots_ += got;
            if (j.axis == s.sliceAxis && j.off == s.sliceOff) { s.sliceFresh += got; need = want - got; }
        }
    }
    if (need > 0) {                                                  // the next two batches, for the plane of the moment
        const int tries = int(std::min(262144.0, std::max(64.0, double(need) / sliceRate_ * 1.3 + 64)));
        const int ja = sliceCard->beginSlice(sliceJobOf(pa, s.crng, tries, 4, 1));
        const int jb = ja ? sliceCard->beginSlice(sliceJobOf(pb, s.crng, tries, 4, 1)) : 0;
        if (ja && jb) sliceJob_ = SliceJob{ja, s.crng, tries, need, sup_.idA, p.Z, s.sliceAxis, s.sliceOff, s.ptsEpoch, jb, sup_.idB, wA};
        else if (ja) sliceCard->cancelSlice(ja);
    }
    return got;
}

// ---- the shown dots of a cloud jump ----
double AtomLab::jumpShareTo() const {                            // 0.49.3: see kJumpTauS (atom_sim.h)
    if (!s.trans) return 0;
    const Transition& t = *s.trans;
    if (!(t.dur > 0) || t.t >= t.dur) return 1;
    const double x = std::max(0.0, t.t);
    // the photon is the exponential cut at T and renormalised (the part beyond T is 1 / kJumpDotsEnd of it), so the shares start at exactly 0 and end at exactly 1
    const double whole = -std::expm1(-t.dur / kJumpTauS);
    if (t.how == kJumpEmit) return -std::expm1(-x / kJumpTauS) / whole;                                  // the lower level's share: (1 - exp(-t / tau)) / (1 - exp(-T / tau))
    if (t.how == kJumpAbsorb) return (std::exp((x - t.dur) / kJumpTauS) - std::exp(-t.dur / kJumpTauS)) / whole;   // the upper level's share: the time reverse
    const double u = x / t.dur, e = u * u * (3 - 2 * u), sn = std::sin(kPi / 2 * e);   // a placement: the cross-fade of 0.46 (theta = pi/2 smoothstep)
    return sn * sn;
}
double AtomLab::jumpU() const { return s.trans ? 2 / kPi * std::asin(std::sqrt(std::clamp(jumpShareTo(), 0.0, 1.0))) : 0; }
static double jumpUniform(int pop, long i) {                     // a fixed uniform number for each dot (splitmix64 of its index and population)
    std::uint64_t z = (std::uint64_t(i) + (std::uint64_t(pop) << 40)) * 0x9E3779B97F4A7C15ULL + 0x632BE59BD9B4E019ULL;
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL; z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL; z ^= z >> 31;
    return double(z >> 11) * (1.0 / 9007199254740992.0);
}
double jumpUniformOf(int pop, long i) { return jumpUniform(pop, i); }   // (atom_card.cpp: the card's check)
bool AtomLab::jumpShown(int pop, long i) const {
    if (!s.trans || i < 0 || i >= s.jn) return false;
    if (!jumpReady()) const_cast<AtomLab*>(this)->completeJump();
    // 0.49.3: every jump is a mixture of the two levels (kJumpTauS): an old dot is shown with odds 1 - share, a new one with odds share; no interference (0.46-0.49.2 drew the one-electron jump
    // as a coherent superposition sloshing at a slowed beat, the classical-light picture: not what one photon does)
    const double sh = jumpShareTo();
    return jumpUniform(pop, i) < (pop ? sh : 1 - sh);
}     // (anything that asks about a dot gets the finished jump)
double AtomLab::jumpPhase(int pop, long i) const {
    if (!jumpReady()) const_cast<AtomLab*>(this)->completeJump();
    if (s.jb.many) return pop ? s.jph[std::size_t(i)] : s.ph[std::size_t(i)];
    const std::vector<float>& a = pop ? s.jB : s.jA;               // 0.49.3: each dot keeps the phase of its own level's wave (the old level at an old dot, the new one at a new dot)
    const std::complex<double> own = pop ? std::complex<double>(a[4 * i + 2], a[4 * i + 3]) : std::complex<double>(a[4 * i], a[4 * i + 1]);
    return std::arg(own);
}
int AtomLab::jumpGroup(int pop, long i) const {
    if (!jumpShown(pop, i)) return 255;
    const double ph = jumpPhase(pop, i);
    if (s.jb.many) return usesWheel() ? phaseBin(ph) : ((pop ? s.jsg[std::size_t(i)] : s.sg[std::size_t(i)]) > 0 ? 0 : 1);   // (0.46: as plainGroups colours the cloud)
    return p.phase == "Wheel" ? phaseBin(ph) : (std::cos(ph) >= 0 ? 0 : 1);
}
// ---- 0.13 stage 2: the same colour groups from the GPU (render/group_compute.h; the kernel is a port of superGroup and jumpGroups, shaders.h kGroupsCS) ----
static std::uint64_t nextEpoch() { static std::uint64_t n = 0; return ++n; }
render::SuperGroupParams AtomLab::superGpuParams(double theta) const {
    render::SuperGroupParams q;
    q.a = float(sup_.a); q.b = float(sup_.b);
    q.emRe = float(std::cos(theta)); q.emIm = float(-std::sin(theta));             // exp(-i theta)
    q.overlap = overlap_ ? 1 : 0;
    return q;
}
render::JumpGroupParams AtomLab::jumpGpuParams() const {
    const double sh = jumpShareTo();
    render::JumpGroupParams q;
    q.shareA = float(1 - sh); q.shareB = float(sh);
    q.wheel = p.phase == "Wheel" ? 1 : 0;
    return q;
}
bool AtomLab::superGroupsGpu(render::IGroupCompute& gc, long cnt, double theta, unsigned char* out) {
    if (!gc.ok() || !superOn() || cnt <= 0 || std::size_t(4 * cnt) > s.sa.size()) return false;
    const long lo = s.saDirtyLo, hi = s.saDirtyHi;
    s.saDirtyLo = s.saDirtyHi = 0;                                                  // taken: the GPU copy is current after this call
    return gc.superGroups(s.sa.data(), cnt, s.saEpoch, lo, hi, superGpuParams(theta), out);
}
bool AtomLab::jumpGroupsGpu(render::IGroupCompute& gc, unsigned char* out) {
    const long jn = s.jn;
    if (!gc.ok() || !s.trans || jn <= 0 || s.jb.many) return false;   // (0.46: a valence jump's groups come from the CPU)
    if (!jumpReady()) completeJump();
    return gc.jumpGroups(s.jA.data(), s.jB.data(), jn, s.jumpEpoch, jumpGpuParams(), out);
}
// 0.57.0: the card's half of jumpGroups for a jump with every electron: each dot's group when shown (it does not change during the jump: its shell's colour,
// the old dots then the new), and the two populations' shares as 53-bit integers (a dot is shown when its fixed number z >> 11 is below: the same test as
// jumpUniform(pop, i) < share, exactly)
void AtomLab::jumpFixedGroups(std::vector<unsigned char>& g) const {
    const long jn = s.jn;
    g.assign(std::size_t(2 * jn), 255);
    if (!s.trans || jn <= 0 || !s.jb.many) return;
    if (!jumpReady()) const_cast<AtomLab*>(this)->completeJump();
    const bool wheelM = usesWheel();
    for (int pop = 0; pop < 2; pop++) {
        const float* ph = pop ? s.jph.data() : s.ph.data();
        const signed char* sg = pop ? s.jsg.data() : s.sg.data();
        for (long i = 0; i < jn; i++) g[std::size_t(pop * jn + i)] = (unsigned char)(wheelM ? phaseBin(ph[i]) : (sg[i] > 0 ? 0 : 1));
    }
}
void AtomLab::jumpThresholds(std::uint64_t& t0, std::uint64_t& t1) const {
    const double sh = jumpShareTo();
    t0 = render::jumpShareThreshold(1 - sh); t1 = render::jumpShareThreshold(sh);
}
// Every dot of the jump, in one go (the picture asks for this each frame): the same numbers as jumpGroup(pop, i) one by one, bit for bit (tested), but cos, sin and the complex exponentials
// of the jump's progress, and the jump's energy step (levelEV, which looks up the nucleus's mass), are worked out once here and not once per dot (0.10.6 stage 8; the benchmark's scene 11 spent about 26 of 30 ms here).
void AtomLab::jumpGroups(unsigned char* grp) const {
    const long jn = s.jn;
    if (!s.trans || jn <= 0) { for (long i = 0; i < 2 * jn; i++) grp[i] = 255; return; }
    if (!jumpReady()) const_cast<AtomLab*>(this)->completeJump();
    const double sh = jumpShareTo();
    if (s.jb.many) {                                             // 0.57.0: as jumpGroup(pop, i) dot by dot, bit for bit (tested), with the share and the mode worked out once, not once per dot
        const bool wheelM = usesWheel();                         // (dot by dot it was 40 ms a frame for 480,000 dots: the share's exponentials and the checks, again for every dot)
        for (int pop = 0; pop < 2; pop++) {
            const float* ph = pop ? s.jph.data() : s.ph.data();
            const signed char* sg = pop ? s.jsg.data() : s.sg.data();
            unsigned char* g = grp + (pop ? jn : 0);
            const double f = pop ? sh : 1 - sh;
            for (long i = 0; i < jn; i++) g[i] = !(jumpUniform(pop, i) < f) ? 255 : (unsigned char)(wheelM ? phaseBin(ph[i]) : (sg[i] > 0 ? 0 : 1));
        }
        return;
    }
    const bool wheel = p.phase == "Wheel";
    for (int pop = 0; pop < 2; pop++) {
        const float* a = (pop ? s.jB : s.jA).data();
        unsigned char* g = grp + (pop ? jn : 0);
        const double f = pop ? sh : 1 - sh;
        for (long i = 0; i < jn; i++) {
            if (!(jumpUniform(pop, i) < f)) { g[i] = 255; continue; }
            const std::complex<double> own = pop ? std::complex<double>(a[4 * i + 2], a[4 * i + 3]) : std::complex<double>(a[4 * i], a[4 * i + 1]);   // the dot's own level's wave
            g[i] = (unsigned char)(wheel ? phaseBin(std::arg(own)) : (std::cos(std::arg(own)) >= 0 ? 0 : 1));
        }
    }
}
double AtomLab::outlineAlpha() const {                          // the outline belongs to one orbital: it fades out as the cloud leaves it, and back in after landing
    if (reducedMotion) return 1;
    const auto sm = [](double u) { u = std::max(0.0, std::min(1.0, u)); return u * u * (3 - 2 * u); };
    if (s.trans && p.view != "Orbit") return 1 - sm(s.trans->t / 0.25);
    return s.landT < 0.4 ? sm(s.landT / 0.4) : 1;
}

double AtomLab::beatRealPeriodS() const { return beatPeriodS(p.Z, p.N, superState()); }
double AtomLab::beatSlowdown() const { const double T = beatRealPeriodS(); return std::isinf(T) ? 0 : kBeatScreenS * 5.0 / p.speed / T; }
double AtomLab::beatTheta() const {
    const double dE = beatEV(p.Z, p.N, superState());
    if (!superOn() || dE == 0) return 0;                       // equal energies: no beats, the density never changes
    return (dE > 0 ? 1 : -1) * 2 * kPi * s.beatT / kBeatScreenS;
}
double AtomLab::superPhase(long i, double theta) const {
    if (std::size_t(4 * i + 3) >= s.sa.size()) return 0;
    const std::complex<double> A(s.sa[4 * i], s.sa[4 * i + 1]), B(s.sa[4 * i + 2], s.sa[4 * i + 3]);
    return std::arg(sup_.a * A + sup_.b * B * std::exp(std::complex<double>(0, -theta)));
}
bool AtomLab::superShown(long i, double theta) const {
    if (std::size_t(4 * i + 3) >= s.sa.size()) return false;
    const std::complex<double> A(s.sa[4 * i], s.sa[4 * i + 1]), B(s.sa[4 * i + 2], s.sa[4 * i + 3]);
    if (overlap_) return std::norm(sup_.a * A + sup_.b * B) > 0;               // sampled from the renormalised density itself: nothing is thinned
    const double q = 0.5 * (std::norm(A) + std::norm(B));
    if (q <= 0) return false;
    const double w = std::norm(sup_.a * A + sup_.b * B * std::exp(std::complex<double>(0, -theta))) / q;
    std::uint64_t z = std::uint64_t(i) * 0x9E3779B97F4A7C15ULL + 0x632BE59BD9B4E019ULL;          // splitmix64: a fixed uniform number for this dot
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL; z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL; z ^= z >> 31;
    return double(z >> 11) * (1.0 / 9007199254740992.0) < w / 2;
}
int AtomLab::superGroup(long i, double theta) const {
    if (!superShown(i, theta)) return 255;
    return phaseBin(superPhase(i, theta));
}
std::string AtomLab::beatKindText() const {
    const Superposition u = superState();
    const Orbital &oa = orbitals()[stateOrbital(u.idA)], &ob = orbitals()[stateOrbital(u.idB)];
    if (oa.n == ob.n) return "Same n: " + std::string(oa.n == 1 ? "" : "the same energy in this model, so NO beats (the density does not change). ") +
                             "Fine structure and the Lamb shift, which make 2s and 2p differ a little in real hydrogen, are not modelled." +
                             (overlap_ ? std::string(" These two states overlap, so the mixture is renormalised (it is one fixed state).") : std::string());
    double d[3];
    if ((oa.l + ob.l) % 2 == 1) {
        if (dipoleAxes(u, d)) {
            const bool alongZ = d[2] > 0.5 * std::max(d[0], d[1]) && d[2] > 1e-3 && std::max(d[0], d[1]) < 1e-3;
            return alongZ ? "Opposite parity: the cloud's centre sloshes back and forth along z: an oscillating dipole."
                          : "Opposite parity: the cloud's centre moves (an oscillating or turning dipole across the z axis).";
        }
        return "Opposite parity, but light cannot connect these two states (no dipole), so the centre does not move: the cloud only changes shape.";
    }
    return "Same parity: the centre stays put and the cloud breathes (it changes shape and size).";
}
std::string AtomLab::beatSlowText() const {
    const double T = beatRealPeriodS();
    if (std::isinf(T)) return "no beats: both states have the same energy";
    return "real beat period " + fmtT(T * 1e18) + ", slowed by " + sci(beatSlowdown());
}

// ---------------------------------------------------------------- photon jumps
int AtomLab::curLevel() const { return p.view == "Orbit" ? p.n : fullOrb(); }
int AtomLab::lvlN(int l) const { return p.view == "Orbit" ? l : orbitals()[l].n; }
std::string AtomLab::lvl(int l) const { return p.view == "Orbit" ? "n=" + std::to_string(l) : orbitals()[l].txt; }

std::vector<int> AtomLab::targets(int dir) const {          // dir −1: lower levels (emit), +1: higher levels (absorb)
    if (levelOn()) return levelTargets(dir);                     // 0.47.3: a level atom (to: a level index, levels.h)
    if (manyOn()) { std::vector<int> o; for (const AlkOrbital& t : valenceTargets(dir)) o.push_back(valIndex(p.Z, t.n, t.l)); return o; }   // 0.46: the alkali atoms' outer electron (else none)
    const int cur = curLevel(), n0 = lvlN(cur);
    std::vector<int> out;
    if (superOn()) return out;                                 // a superposition has no single level to jump from
    if (diracCloudOn()) {                                      // 0.49.3: a Dirac state jumps to any shell with l changed by 1 (every l' has a j' the rule allows); startJump picks j'
        const Orbital& c = orbitals()[std::size_t(cur)];
        for (int n = 1; n <= kLineMaxN; n++)
            for (const int l : {c.l - 1, c.l + 1})
                if (l >= 0 && l < n && l <= 3 && n != c.n && (dir < 0 ? n < n0 : n > n0)) out.push_back(orbIndex(std::to_string(n) + "spdf"[l]));
        return out;
    }
    if (p.view != "Orbit" && complexM() != 0) return out;      // a complex state: the dipole rule depends on the light's polarisation, which this lab does not model
    if (p.view == "Orbit") {
        for (int t = 1; t <= kLineMaxN; t++) if (t != cur && (dir < 0 ? t < n0 : t > n0)) out.push_back(t);
        return out;
    }
    // the dipole rule: l changes by 1, and only orientations the light can connect
    const Orbital& c = orbitals()[cur];
    const auto& all = orbitals();
    for (int k = 0; k < int(all.size()); k++) {
        const Orbital& o = all[k];
        if (std::abs(o.l - c.l) != 1 || o.n == c.n || (dir < 0 ? o.n > n0 : o.n < n0)) continue;
        if (dipoleOK(cur, k)) out.push_back(k);
    }
    return out;
}

// photon energy (eV) and wavelength (nm), with the nucleus's real mass (hydrogen's Hα is 656.47 nm in vacuum)
AtomLab::PhotonInfo AtomLab::photonInfo(int from, int to) const {
    const int nf = lvlN(from), nt = lvlN(to), hi = std::max(nf, nt), lo = std::min(nf, nt);
    const double E = rydM(p.Z, p.N) * p.Z * p.Z * (1.0 / (lo * lo) - 1.0 / (hi * hi)), lam = kHcEVnm / E;
    static const char* const series[6] = {"", "Lyman", "Balmer", "Paschen", "Brackett", "Pfund"};
    return {E, lam, p.Z == 1 ? std::string(series[lo]) + "-" + kGreek[hi - lo - 1] : std::string()};
}

template <class T> static T pick(const std::vector<T>& a) { return a[std::size_t(std::floor(random() * a.size()))]; }

// 0.46.1 (outside review): a decay goes to each lower level with the odds of its rate, not with equal odds (H 3p: 1s about 88 %, 2s about 12 %). One electron: NIST ASD's hydrogen rates
// (asd.h hydrogenTermRate; times Z^4 for an ion, which leaves the odds the same) times the share of the target's orientation (atom_core.h dipoleAngularSq; the shares of one shell add up to 1);
// the Orbit view's circles have no l: the n -> n' rate summed over l and l', each l of n weighted by (2l + 1) / n^2 (filled in proportion to its states). Alkali atoms: NIST ASD's own lines of the
// atom, sum over the components of g_upper A divided by the sum of g_upper (the term's J levels filled in proportion to 2J + 1).
double AtomLab::emitRate(int to) const {
    if (levelOn()) {                                            // 0.47.3: the A of the NIST line to that level (0.47.4: else this app's computed A, D4); a line without either is not used by the decay picker when another way down has one (PLAN_JUMPS.md section 5)
        const auto rateTo = [&](int t) { double a = 0; bool got = false; for (const int k : linesBetween(p.Z, s.lvl, t)) { const JLine& L = jumpLines(p.Z)[std::size_t(k)]; if (L.hi == s.lvl && std::isfinite(L.rate())) { a += L.rate(); got = true; } } return got ? a : std::nan(""); };
        const double r = rateTo(to);
        if (std::isfinite(r)) return r;
        for (const int t : levelTargets(-1)) if (std::isfinite(rateTo(t))) return 0.0;
        return std::nan("");
    }
    if (manyOn()) {
        if (!valenceOn()) return std::nan("");
        const std::vector<AlkOrbital> orbs = alkaliOrbitals(p.Z);
        if (to < 0 || to >= int(orbs.size())) return std::nan("");
        const AlkOrbital c = valenceNow(), t = orbs[std::size_t(to)];
        std::map<int, double> perJ;
        for (const AlkLine& L : alkaliLinesBetween(p.Z, c.n, c.l, t.n, t.l)) if (L.nHi == c.n && L.lHi == c.l && std::isfinite(L.aki)) perJ[L.j2Hi] += L.aki;
        double num = 0, den = 0;
        for (const auto& j : perJ) { num += (j.first + 1) * j.second; den += j.first + 1; }
        return den > 0 ? num / den : std::nan("");
    }
    if (p.view == "Orbit") {
        const int n = p.n, m = to;
        double sum = 0;
        for (int l = 0; l < n; l++)
            for (const int l2 : {l - 1, l + 1}) {
                if (l2 < 0 || l2 >= m) continue;
                const double r = hydrogenTermRate(n, l, m, l2);
                if (!std::isfinite(r)) return std::nan("");
                sum += (2 * l + 1) * r / (double(n) * n);
            }
        return sum;
    }
    const int cur = curLevel();
    const Orbital& c = orbitals()[std::size_t(cur)];
    const Orbital& o = orbitals()[std::size_t(to)];
    const double r = hydrogenTermRate(c.n, c.l, o.n, o.l);
    if (diracCloudOn()) return r;                               // 0.49.3: summed over j' and every m', the rate from (n l j) to the shell (n' l') is the term's own, the same for both j (the line-strength sum rule)
    return r * dipoleAngularSq(cur, to) * (2 * c.l + 1) / std::max(c.l, o.l);
}
int AtomLab::pickEmit(const std::vector<int>& list, double* share) const {
    std::vector<double> w;
    double tot = 0;
    bool ok = true;
    for (const int k : list) { const double r = emitRate(k); ok = ok && std::isfinite(r) && r >= 0; w.push_back(r); tot += ok ? r : 0; }
    if (!ok || !(tot > 0)) { const int k = pick(list); if (share) *share = std::nan(""); return k; }
    const double u = random() * tot;
    double acc = 0;
    for (std::size_t i = 0; i < list.size(); i++) { acc += w[i]; if (u < acc) { if (share) *share = w[i] / tot; return list[i]; } }
    if (share) *share = w.back() / tot;
    return list.back();
}

void AtomLab::startJump(const std::string& kind, int to) {   // emit: the jump and the photon start together; absorb: the photon flies in, the jump starts when its front arrives
    if (levelOn()) { startLevelJump(kind, to); return; }        // 0.47.3
    if (manyOn()) { if (valenceOn()) startValenceJump(kind, to); return; }   // 0.39: photon jumps were the one-electron lab's only; 0.46: the alkali atoms' outer electron (to: an alkaliOrbitals index)
    const int from = curLevel();
    PhotonInfo info = photonInfo(from, to);
    std::string dText;
    pendingJ2_ = 0;
    if (diracCloudOn()) {                                        // 0.49.3: the target's j by the odds of the line strengths (dirac_cloud.h), and the Dirac line with the Lamb shift (dirac.h)
        const Orbital& c = orbitals()[std::size_t(from)];
        const Orbital& o = orbitals()[std::size_t(to)];
        const int j2 = diracJ2();
        std::vector<int> js; std::vector<double> w; double tot = 0;
        for (const int jp : {2 * o.l - 1, 2 * o.l + 1}) { const double S = jp >= 1 ? lineStrength(c.l, j2, o.l, jp) : 0; if (S > 0) { js.push_back(jp); w.push_back(S); tot += S; } }
        double u = random() * tot; int jp = js.back();
        for (std::size_t k = 0; k < js.size(); k++) { if (u < w[k]) { jp = js[k]; break; } u -= w[k]; }
        pendingJ2_ = jp;
        const double E = std::fabs(diracQedEV(p.Z, p.N, o.n, o.l, jp) - diracQedEV(p.Z, p.N, c.n, c.l, j2));
        info.E = E; info.lam = kHcEVnm / E;
        dText = diracStateText(c.n, c.l, j2) + " → " + diracStateText(o.n, o.l, jp) + (js.size() > 1 ? " (" + numText(100 * (jp == js[0] ? w[0] : w.back()) / tot, 3) + " % of this shell's line strength)" : "");
    }
    const std::string col = lamColorCss(info.lam);
    Photon ph{kind, from, to, info.E, info.lam, info.name, col, random() * kPi * 2, 0, 1.6, jumpDuration(kJumpEmit), false};
    if (p.view != "Orbit" && dipoleUnit(kind == "emit" ? stateId() : to, kind == "emit" ? to : stateId(), ph.u)) {   // 0.78.0: the dipole pattern (the Orbit view knows no m)
        // (an absorbed photon comes in from a direction drawn the same way: the pattern is the same both ways). Its own generator, seeded from the angle already
        // drawn, so the lab's random sequence (recorded by the tests) is the one it was before 0.78.0.
        std::uint64_t seed = 0;
        std::memcpy(&seed, &ph.ang, sizeof seed);
        std::mt19937_64 gen(seed);
        drawDipoleDirection(ph.u, [&gen] { return double(gen() >> 11) * (1.0 / 9007199254740992.0); }, ph.dir);
        ph.has3d = true;
    }
    s.photon = ph;                                               // 0.49.2: an emitted photon leaves during its jump, which starts now
    if (std::none_of(s.lines.begin(), s.lines.end(), [&](const SeenLine& L) { return std::fabs(L.lam - info.lam) < 0.05; }))
        s.lines.push_back({info.lam, col});
    s.last = "last photon: " + std::string(kind == "emit" ? "emitted" : "absorbed") + "  ·  " + (info.name.empty() ? "" : info.name + "  ·  ") +
             (dText.empty() ? lvl(from) + " → " + lvl(to) : dText + "  ·  Dirac" + (lambKnown(p.Z, std::min(orbitals()[std::size_t(from)].n, orbitals()[std::size_t(to)].n)) ? " + Lamb shift" : "")) +
             "  ·  " + fmtLam(info.lam) + "  ·  " + fmtEV(info.E);
    if (kind == "emit") beginTransition(from, to, kJumpEmit);
}

// Build the jump's dots from b.done up to upTo: for each index, the amplitudes of both states at the old dot, and a NEW dot, an exact independent sample of the target state (on the cut
// plane in the Slice view), with the amplitudes there. The jump has its own generator (b.rng), so the dots are the same whether they are built over the frames or at once.
// (The jump is between real orbitals: targets() gives none for a complex state.)
void AtomLab::buildJump(long upTo) {
    AtomState::JumpBuild& b = s.jb;
    upTo = std::min(upTo, s.jn);
    orbit::Mulberry32 r(b.rng);
    for (; b.done < upTo; b.done++) {
        const long i = b.done;
        const double u = aUnit(), x = s.pts[3 * i] / u, y = s.pts[3 * i + 1] / u, z = s.pts[3 * i + 2] / u;      // the old dot
        if (b.toJ2 > 0) { s.jA[4 * i] = 1; s.jA[4 * i + 2] = 1; }                                          // 0.49.3: a Dirac jump: one colour (no single phase), as recordDot
        else { s.jA[4 * i] = float(b.nA * psi(b.from, p.Z, x, y, z)); s.jA[4 * i + 2] = float(b.nB * psi(b.to, p.Z, x, y, z)); }
        double nx = 0, ny = 0, nz = 0;                                                                     // the new dot
        bool ok = false;
        for (int t = 0; t < 4096 && !ok; t++) ok = tryStatePoint(b.to, r, nx, ny, nz, b.toJ2);
        if (!ok) {                                                     // the cut plane holds (almost) none of the target state: the new dot is put far off the plane, where the Slice view does not show it
            nx = 0; ny = sliceAxisNow() == 1 ? sliceOffNow() + 1e4 : 0; nz = sliceAxisNow() == 2 ? sliceOffNow() + 1e4 : 0;
        }
        s.jp[3 * i] = float(nx * u); s.jp[3 * i + 1] = float(ny * u); s.jp[3 * i + 2] = float(nz * u);
        if (b.toJ2 > 0) { s.jB[4 * i] = 1; s.jB[4 * i + 2] = 1; }
        else { s.jB[4 * i] = float(b.nA * psi(b.from, p.Z, nx, ny, nz)); s.jB[4 * i + 2] = float(b.nB * psi(b.to, p.Z, nx, ny, nz)); }
    }
    b.rng = r.seed;
}

// A jump. Orbit view: the electron glides from one circle to the other (Bohr's model has no path: to help the eye). Cloud views: two fixed populations of dots (the cloud on screen, and a new
// one of the target orbital), each shown with its level's share (jumpShareTo, 0.49.3). Reduced motion: no glide, no cross-fade: the jump is over at once.
double AtomLab::jumpDuration(int how) const { return reducedMotion ? 0 : how == kJumpPlace ? 1.2 : kJumpTauS * std::log(kJumpDotsEnd); }   // 0.49.3: a photon's jump is tau ln(48000) = 2.16 s (was 1.2 s; 0.8 s in Orbit)
void AtomLab::beginTransition(int from, int to, int how) {
    const double dur = jumpDuration(how);
    if (manyOn()) {                                             // 0.46: a valence jump. Orbit view: the rings change when it lands. Cloud views: the dots cross-fade to the new atom's (buildManyJump)
        if (p.view != "Orbit") {
            const std::vector<AlkOrbital> orbs = valenceOn() ? alkaliOrbitals(p.Z) : std::vector<AlkOrbital>();   // (0.47.3: a level atom has no one outer orbital to follow)
            const AlkOrbital c = valenceOn() ? orbs[std::size_t(from)] : AlkOrbital{}, t = valenceOn() ? orbs[std::size_t(to)] : AlkOrbital{};
            if (valenceOn() && manySub() >= 0 && orbitals()[orbIndex(p.orb)].n == c.n && orbitals()[orbIndex(p.orb)].l == c.l) {   // One orbital showing the outer electron: it follows the electron
                s.jumping = true; setParam("orb", valName(t.n, t.l)); if (t.l == 1) setParam("pAx", std::string("z")); else if (t.l == 2) setParam("dAx", std::string("z2")); else if (t.l == 3) setParam("fAx", std::string("z3")); s.jumping = false;
            }
            const long jn = std::min<long>(s.k, std::min<long>(cloudCap(), long(s.sg.size())));
            s.jumpEpoch = nextEpoch();
            s.jn = jn; s.jp.assign(std::size_t(3 * jn), 0.f); s.jph.assign(std::size_t(jn), 0.f); s.jsg.assign(std::size_t(jn), 1); s.jA.clear(); s.jB.clear();
            s.jb = AtomState::JumpBuild(); s.jb.from = from; s.jb.to = to; s.jb.many = true;
            orbit::Mulberry32 cr(s.crng);
            s.jb.rng = std::uint32_t(cr.next() * 4294967296.0) ^ 0x5A5A5A5Au;
            s.crng = cr.seed;
        } else { s.jb = AtomState::JumpBuild(); s.jb.from = from; s.jb.to = to; s.jb.many = true; s.jn = 0; }
        s.trans = Transition{from, to, 0, dur, how};
        if (dur == 0) finishTransition();
        return;
    }
    if (p.view == "Orbit") {
        s.gAng = std::atan2(s.y, s.x);
    } else {
        const long jn = std::min<long>(s.k, std::min<long>(cloudCap(), long(s.sg.size())));
        s.jumpEpoch = nextEpoch();
        s.jn = jn; s.jp.assign(std::size_t(3 * jn), 0.f); s.jA.assign(std::size_t(4 * jn), 0.f); s.jB.assign(std::size_t(4 * jn), 0.f);
        // The dots are built by buildJump(), a part of them each frame (an old dot's amplitudes, and a new dot of the target state with its amplitudes, for the same index). Nothing is sampled
        // during a jump (step() does not run the cloud), so the old dots stay as they are. The jump's generator is seeded from the cloud's, once, here.
        AtomState::JumpBuild& b = s.jb;
        b = AtomState::JumpBuild();
        b.nA = normFactor(from, p.Z); b.nB = normFactor(to, p.Z); b.from = from; b.to = to;
        if (diracCloudOn() && how != kJumpPlace) { b.fromJ2 = diracJ2(); b.toJ2 = pendingJ2_ > 0 ? pendingJ2_ : 1; }   // 0.49.3: a Dirac jump (startJump picked the target's j)
        orbit::Mulberry32 cr(s.crng);
        b.rng = std::uint32_t(cr.next() * 4294967296.0) ^ 0xA5A5A5A5u;
        s.crng = cr.seed;
        buildJump(0);
    }
    s.trans = Transition{from, to, 0, dur, how};
    if (dur == 0) finishTransition();
}

// The jump is over: the electron is on its new circle (Orbit), or the new dots become the cloud (cloud views). (0.49.2: an emitted photon is already on its way: it left during the jump.)
void AtomLab::finishTransition() {
    if (s.jb.many) {                                            // 0.46: the valence jump has landed: the outer electron is in the new orbital; the new dots are the cloud
        completeJump();
        if (levelOn() || (levelJumps(p.Z) && p.mrel != "Non-relativistic")) s.lvl = s.trans->to;   // 0.47.3: a level atom is in its new level
        else {
        const AlkOrbital t = alkaliOrbitals(p.Z)[std::size_t(s.trans->to)];
        const bool ground = alkaliOrbitals(p.Z)[0].n == t.n && alkaliOrbitals(p.Z)[0].l == t.l;
        s.valN = ground ? 0 : t.n; s.valL = ground ? 0 : t.l; s.valJ2 = ground ? 0 : s.valJ2Next;
        }
        for (long i = 0; i < s.jn; i++) { s.pts[3 * i] = s.jp[3 * i]; s.pts[3 * i + 1] = s.jp[3 * i + 1]; s.pts[3 * i + 2] = s.jp[3 * i + 2]; s.ph[i] = s.jph[std::size_t(i)]; s.sg[i] = s.jsg[std::size_t(i)]; }
        if (s.jn > 0) { s.k = s.jn; s.sliceFresh = s.jn; }
        s.ptsEpoch = nextEpoch();
        s.jp.clear(); s.jph.clear(); s.jsg.clear(); s.jn = 0; s.jb = AtomState::JumpBuild(); s.landT = 0; s.boost = 0.6;
        s.trans.reset();
        return;
    }
    if (p.view == "Orbit") {
        const int to = s.trans->to;
        const double a = s.gAng, r = double(to) * to / p.Z * aUnit(), v = double(p.Z) / to * kAuSpeed;
        s.x = r * std::cos(a); s.y = r * std::sin(a); s.vx = -v * std::sin(a); s.vy = v * std::cos(a);
        s.jumping = true; setParam("n", to); s.jumping = false; s.flash = 0;                      // the landing flash
    } else {
        completeJump();                                         // (a jump that is ended before it was built, by reduced motion or a long frame, is built now)
        const Orbital& o = orbitals()[s.trans->to];
        for (long i = 0; i < s.jn; i++) {                       // the new dots are the cloud now (the old ones were hidden at the end already)
            s.pts[3 * i] = s.jp[3 * i]; s.pts[3 * i + 1] = s.jp[3 * i + 1]; s.pts[3 * i + 2] = s.jp[3 * i + 2];
            const bool pos = s.jB[4 * i + 2] >= 0;
            s.sg[i] = pos ? 1 : -1; s.ph[i] = pos ? 0.f : float(kPi);
        }
        if (s.jn > 0) { s.k = s.jn; s.sliceFresh = s.jn; }      // (Slice: the new dots were made on the plane of the moment)
        s.ptsEpoch = nextEpoch();
        const int toJ2 = s.jb.toJ2;
        s.jumping = true;
        if (o.l > 0) setParam(o.l == 1 ? "pAx" : o.l == 2 ? "dAx" : "fAx", o.ori);
        setParam("orb", o.shell);
        if (toJ2 > 0 && o.l > 0) p.hj = toJ2 == 2 * o.l + 1 ? "l + 1/2" : "l - 1/2";   // 0.49.3: the Dirac cloud lands in the j the photon's line went to
        s.jumping = false;
        s.boost = 0.6;
        s.jp.clear(); s.jA.clear(); s.jB.clear(); s.jn = 0; s.jb = AtomState::JumpBuild();
        s.landT = 0;
    }
    s.trans.reset();
}

// Orbit view, during a jump: the electron's radius glides (smoothstep) from the old circle to the new one, turning at the rates of the two circles blended; no Coulomb stepping.
void AtomLab::glideOrbit(double dt) {
    const Transition& t = *s.trans;
    const double u0 = t.dur > 0 ? std::min(1.0, t.t / t.dur) : 1, e = u0 * u0 * (3 - 2 * u0);
    const double r1 = double(t.from) * t.from / p.Z * aUnit(), r2 = double(t.to) * t.to / p.Z * aUnit(), r = r1 + e * (r2 - r1);
    s.gAng += dt * p.speed * ((1 - e) / std::pow(double(t.from), 3) + e / std::pow(double(t.to), 3));   // rad per step of a circular orbit n: dt x speed / n^3 (any Z)
    const double v = (1 - e) * double(p.Z) / t.from * kAuSpeed + e * double(p.Z) / t.to * kAuSpeed;
    s.x = r * std::cos(s.gAng); s.y = r * std::sin(s.gAng); s.vx = -v * std::sin(s.gAng); s.vy = v * std::cos(s.gAng);
    s.t += dt * p.speed / (double(p.Z) * p.Z) * tUnit();
    s.trail.push_back({s.x, s.y});
    if (s.trail.size() > 500) s.trail.pop_front();
}

AtomLab::NoJump AtomLab::whyNoJump(int dir) const {          // (only asked when targets(dir) is empty)
    if (levelOn()) {                                            // 0.47.3
        const std::string nm = levelName(p.Z, s.lvl);
        return {dir < 0 ? (s.lvl == 0 ? "The atom is in its ground level, " + nm + ": nothing to emit. Try Absorb." : "From " + nm + " no lower level with a cloud is joined by a measured line (NIST). Try Absorb.")
                        : "From " + nm + " no higher level with a cloud is joined by a measured line (NIST). Try Emit.", 5000};
    }
    if (manyOn()) {                                             // 0.46
        if (!noJumpText(p.Z).empty()) return {noJumpText(p.Z), 6000};   // 0.47.3 (D5)
        if (levelJumps(p.Z) && p.mrel == "Non-relativistic") return {"This atom's jumps are drawn in the relativistic model (its clouds are relativistic GRASP2018 calculations). Set Model: Relativistic.", 6000};
        if (!valenceOn()) return {cloudsNotComputedText(), 7000};   // 0.52.0: one sentence for the message and the Photons section, with no promise of more atoms
        const AlkOrbital c = valenceNow();
        return {dir < 0 ? "The outer electron is in " + valName(c.n, c.l) + ", the ground level: nothing to emit. Try Absorb."
                        : "From " + valName(c.n, c.l) + " no higher level of this lab's set is joined by a measured line (NIST). Try Emit.", 5000};
    }
    const int cur = curLevel();
    if (superOn()) return {"A superposition has no single level to jump from. Switch Superposition off to emit or absorb a photon.", 5500};
    if (p.view != "Orbit" && complexM() != 0)
        return {stateText() + " is a complex state. Photon jumps from it are not in the lab: which levels light can reach depends on the light's polarisation, which the lab does not model. Pick a real orientation (px, pz, ...) to emit or absorb.", 6500};
    return {dir < 0 ? (lvlN(cur) == 1 ? lvl(cur) + " is the lowest level: nothing to emit. Try Absorb."
                                      : p.view != "Orbit" && orbitals()[std::size_t(cur)].n == 2 && orbitals()[std::size_t(cur)].l == 0
                                            // 0.46.1 (outside review): it does decay, by two photons at once (lifetime.h twoPhoton2sLifetimeS); the lab draws no two-photon decay
                                            ? lvl(cur) + " has no one-photon electric jump down (l must change by 1, and 1s has the same l). Really it decays by giving off two photons at once, about once per " +   // 0.46.4: and by one M1 photon, the faster way in heavy ions (lifetime.cpp metastableText)
                                                  fmtLife(twoPhoton2sLifetimeS(p.Z)) + (p.Z >= kRelativityZ ? " (non-relativistic estimate)" : "") + ", or by one magnetic-dipole photon, which is the faster way in heavy ions (Lifetimes card); the lab draws neither. Try Absorb."
                                            : lvl(cur) + " has no one-photon jump down (selection rule: l must change by 1).")
                    : lvl(cur) + " is the highest level in this sim: nothing more to absorb.", 4500};
}

bool AtomLab::emitLine(int hi, int lo) {
    if (hi <= lo || lo < 1 || hi > kLineMaxN || chartOn() || manyOn() || superOn() || s.trans || s.photon) return false;
    if (p.view == "Orbit") {
        if (p.n != hi) setParam("n", double(hi));
        if (p.n != hi) return false;
        startJump("emit", lo);
        return true;
    }
    // 0.46 (B2): the route with the largest NIST transition probability (asd.h strongestRoute; Balmer: nd -> 2p), not always p -> s; the orientation along z (m = 0), which light connects to the m = 0 orbital below
    const LineRoute rt = strongestRoute(hi, lo);
    const int lHi = rt.lHi >= 0 ? rt.lHi : 1, lLo = rt.lHi >= 0 ? rt.lLo : 0;
    setParam("orb", std::to_string(hi) + "spdf"[lHi]);
    if (lHi == 1) setParam("pAx", std::string("z")); else if (lHi == 2) setParam("dAx", std::string("z2")); else if (lHi == 3) setParam("fAx", std::string("z3"));
    for (const int k : targets(-1))
        if (orbitals()[std::size_t(k)].n == lo && orbitals()[std::size_t(k)].l == lLo) {
            startJump("emit", k);
            if (rt.lHi >= 0) {
                const auto nm = [](int n, int l) { return std::to_string(n) + "spdf"[l]; };
                s.last += "  ·  drawn: " + nm(hi, lHi) + " → " + nm(lo, lLo) + ", the strongest route (NIST: A = " + sci(rt.aki) + " /s" +
                          (rt.routes > 1 ? "; next " + nm(hi, rt.lHi2) + " → " + nm(lo, rt.lLo2) + " " + sci(rt.second) + " /s, of " + std::to_string(rt.routes) + " routes)" : ")");
            }
            return true;
        }
    return false;
}

void AtomLab::manualJump(int dir) {
    if (s.trans || s.photon) return;
    if (chartOn()) { s.msg = "The chart of nuclides shows nuclei, not the electron. Switch to Orbit, Cloud or Slice to emit or absorb a photon."; s.msgUntil = nowMs() + 4500; return; }
    const std::vector<int> list = targets(dir);
    if (list.empty()) {
        const NoJump w = whyNoJump(dir);
        s.msg = w.text;
        s.msgUntil = nowMs() + w.ms;
        return;
    }
    if (dir > 0) { startJump("absorb", pick(list)); return; }   // (absorbing: which line depends on the light shone in, which the lab does not model; equal odds)
    double sh = 0;
    startJump("emit", pickEmit(list, &sh));
    if (std::isfinite(sh) && list.size() > 1) s.last += "  ·  this way down: " + numText(100 * sh, 2) + " % of decays (NIST rates)";
}

// photon flight, morph progress, auto mode (runs inside step, so Pause freezes it)
void AtomLab::advance(double dt) {
    const bool hadTrans = s.trans.has_value();
    s.landT += dt;
    if (s.photon) {
        s.photon->t += dt;
        Photon& ph = *s.photon;
        if (ph.kind == "absorb" && !ph.landed && ph.t >= ph.dur) {   // the front of an absorbed photon reaches the atom: its jump starts in this step
            ph.landed = true;
            const double over = ph.t - ph.dur;
            beginTransition(ph.from, ph.to, kJumpAbsorb);
            if (s.trans) s.trans->t = over;
        }
        if (s.photon && s.photon->kind == "emit" && s.photon->t >= s.photon->dur + s.photon->jdur) s.photon.reset();   // its tail has left the picture
    }
    if (hadTrans && s.trans) {
        s.trans->t += dt;
        if (s.trans->t >= s.trans->dur) finishTransition();
    }
    if (s.photon && s.photon->kind == "absorb" && s.photon->landed && !s.trans) s.photon.reset();   // its tail is taken in as its jump ends (the jump's own clock decides: no rounding gap)
    if (s.flash) { *s.flash += dt; if (*s.flash > 0.7) s.flash.reset(); }
    if (s.boost > 0) s.boost -= dt;
    if (s.autoPhotons && !chartOn() && (!manyOn() || valenceOn() || levelOn()) && !s.trans && !s.photon && (s.wait -= dt) <= 0) {   // (0.39.1: no automatic photons with many electrons)
        const std::vector<int> down = targets(-1), up = targets(1);
        const bool goUp = down.empty() || (!up.empty() && random() < 0.25);
        const std::vector<int>& list = goUp ? up : down;
        if (!list.empty()) {
            if (goUp) startJump("absorb", pick(list));
            else {   // 0.46.1: the decay with NIST's odds
                double sh = 0;
                startJump("emit", pickEmit(list, &sh));
                if (std::isfinite(sh) && list.size() > 1) s.last += "  ·  this way down: " + numText(100 * sh, 2) + " % of decays (NIST rates)";
            }
            s.autoSaid.clear();
        }
        else {                                                  // nothing to do in either direction: say why, once per cause (not at every look), with the buttons' words
            const NoJump w = whyNoJump(-1);                     // (for a superposition or a complex state the reason is the same both ways; for the rest, see the report)
            const std::string key = w.text;
            if (s.autoSaid != key) { s.autoSaid = key; s.msg = w.text; s.msgUntil = nowMs() + w.ms; }
        }
        s.wait = 0.5 + random() * 1.3;
    }
}

// ---------------------------------------------------------------- radioactive decay
// Every unstable nucleus decays on its own, at random, with its real half-life and real odds.
void AtomLab::decTick(double dt) {
    const std::string key = std::to_string(p.Z) + "," + std::to_string(p.N);
    auto nucName = [](int Z, int N) { return std::string(symbol(Z)) + "-" + std::to_string(Z + N); };
    if (s.decKey != key) {                                     // a new nucleus: draw its (random) lifetime now, from the exponential law
        if (s.decNext != key || chain.empty()) chain = {ChainStep{nucName(p.Z, p.N), "", 0, std::nullopt}};   // picked by hand: a new history
        s.decNext.clear(); chainVer++;
        s.decKey = key; s.decAge = 0; s.decHeld = false;
        s.dec = decInfo(p.Z, p.N);
        s.decZ = p.Z; s.decN = p.N;
        // A nucleus is timed only if its half-life is a measured or an approximate value (0.12 stage 2, the same rule as the chain walk): the lab's decay table also holds numbers for the 369 nuclides whose half-life
    // NUBASE2020 only extrapolates from systematics and for 11 that have only a limit; the clock used to time those as if they were measured, and no longer does.
    const ChainTiming tm = s.dec ? chainTiming(s.decZ, s.decN) : ChainTiming{ChainTime::None, 0, HeightKind::Unknown};
    s.decAt = tm.kind == ChainTime::None ? INFINITY : decDrawLifetime(tm.halfLife, random());   // (decay_chain.h: the same two draws as the chain walk on the chart)
    }
    if (paused || hold || chartOn() || s.decHeld || std::isinf(s.decAt)) return;   // (the chart: the chosen nucleus must stay where the user put it, so its decay clock waits)
    s.decAge += dt;
    if (s.decAge < s.decAt) return;
    // which way it decays, with the measured odds
    const std::string m = s.dec->br[decDrawBranch(*s.dec, random())].first;   // (the listed odds, normalised to add up to 1: decay_chain.h)
    const auto mv = decMove(m);
    const std::string from = nucName(p.Z, p.N);
    const double lived = s.decAt, now = nowMs();
    const double ang = random() * kPi * 2;
    DecayFx fx{now, {}};
    if (mv) {
        const std::vector<Shot> shots = decShots(m);
        for (std::size_t i = 0; i < shots.size(); i++)
            fx.shots.push_back({shots[i], ang + i * 2 * kPi / std::max<std::size_t>(1, shots.size()) + (random() - 0.5) * 0.6});
    } else {
        fx.shots = {{{"fragment", "#d9a13a", 12}, ang}, {{"fragment", "#d9a13a", 11}, ang + kPi}};
    }
    s.decFx = fx;
    s.decays++;                                                // PLAch.grant('decay')
    auto addStep = [&](std::optional<std::string> to, const std::string& how) {
        chain.push_back({"", how, lived, to});
        if (chain.size() > 41) chain.erase(chain.begin() + 1, chain.begin() + (chain.size() - 40));
        chainVer++;
    };
    if (!mv) {                                                 // fission: the pieces differ every time, so the lab does not follow one
        addStep(std::nullopt, decShort(m));
        s.decLog.push_back({now, "☢ " + from + " split in two (" + decWord(m) + ") after " + fmtLife(lived) + ". Tap “☢ Build another” to watch a fresh one."});
        s.decHeld = true;
    } else {
        const int Z2 = p.Z + mv->first, N2 = p.N + mv->second;
        std::string shotTxt;
        for (const Shot& q : decShots(m)) shotTxt += (shotTxt.empty() ? "" : " + ") + q.label;
        if (Z2 < 1 || N2 < 0 || Z2 > 118 || N2 < nRange(Z2).first || N2 > nRange(Z2).second) {
            addStep(std::nullopt, decShort(m));
            s.decLog.push_back({now, "☢ " + from + " decayed (" + decWord(m) + ") after " + fmtLife(lived) + ". What is left is outside what this lab can build."});
            s.decHeld = true;
        } else {
            const std::string to = nucName(Z2, N2);
            s.decLog.push_back({now, "☢ " + from + " → " + to + " + " + shotTxt + "  ·  " + decWord(m) + ", after " + fmtLife(lived)});
            addStep(to, decShort(m));
            s.decNext = std::to_string(Z2) + "," + std::to_string(N2);
            if (Z2 != p.Z) setParam("Z", Z2);
            setParam("N", N2);                                 // the lab follows the new nucleus: the chain goes on from there
        }
    }
    if (s.decLog.size() > 6) s.decLog.erase(s.decLog.begin(), s.decLog.end() - 6);
}

static std::string pctText(double v) {
    return v >= 0.9995 ? std::string("100 %") : v >= 1e-4 ? numText(v * 100, 2) + " %" : sci(v * 100) + " %";
}
std::string AtomLab::decLine() const {                        // the readout line about decay
    if (!s.dec) return "";
    const DecayInfo& d = *s.dec;
    std::string top;
    for (std::size_t i = 0; i < d.br.size() && i < 3; i++) top += (i ? ", " : "") + decWord(d.br[i].first) + " " + pctText(d.br[i].second);
    const std::string unk = d.unk ? " (odds not measured, shown as equal)" : "";
    const ChainTiming tm = chainTiming(s.decZ, s.decN);
    const std::string more = decUnquantified(s.decZ, s.decN).empty() ? "" : "  ·  odds not complete (NUBASE2020 lists modes with no intensity)";
    if (tm.kind == ChainTime::None) return "☢ unstable: decays by " + top + unk + more + "  ·  " + chainHalfLifeWords(s.decZ, s.decN) + ", so the lab does not time it and it will not decay here by itself";
    const double q = -std::expm1(-kLn2 * 60 / tm.halfLife);       // chance of decaying within a minute (0.39.2: expm1; 1 - exp(-x) rounded a tiny x, uranium-238 said 3.3e-14 % for 2.95e-14 %)
    const std::string qs = q > 0.9999 ? "certain" : q >= 1e-4 ? numText(q * 100, 2) + " %" : q > 0 ? sci(q * 100) + " %" : "0 %";
    return "☢ unstable: " + chainHalfLifeWords(s.decZ, s.decN) + "  ·  decays by " + top + unk + more + "  ·  chance it decays in the next minute: " + qs +
           (s.decHeld ? "  ·  this one is gone: tap ☢ Build another" : paused ? "  ·  paused: its clock is stopped" : "");
}

// ---------------------------------------------------------------- every frame / every step
void AtomLab::tick(double dt) {                               // runs every frame, even when paused: smooth dive and pan
    decTick(dt);                                               // radioactive decay: only while not paused
    if (chartOn()) {                                           // the decay chain walk (0.12 stage 1): it follows the chosen nuclide; the screen time between steps is counted only while the app runs
        if (s.walk.startZ != p.Z || s.walk.startN != p.N) walkStart(s.walk, p.Z, p.N);
        if (!paused) {
            walkTime(s.walk, dt);
            if (s.walkAuto && !s.walk.done && s.walk.sinceStep >= kWalkPaceS) walkOnce();
        }
    }
    const bool rmv = reducedMotion;                            // reduced motion: camera moves jump to the end instead of gliding
    if (s.zoomTo) {
        const double d = rmv ? 0 : *s.zoomTo - p.zoom;
        s.selfSet = true;
        if (std::fabs(d) < 0.004) { setParam("zoom", *s.zoomTo); s.zoomTo.reset(); }
        else setParam("zoom", p.zoom + d * (1 - std::exp(-4 * dt)));
        s.selfSet = false;
    }
    // the auto-fit size glides to the new level
    double sz;
    if (manyOn() && p.view == "Orbit") { const std::vector<Ring> g = manyRings(); sz = (g.empty() ? 1.0 : drawnRadius(g.back().r)) * kA0; }   // 0.39.1: the outermost shell ring (it fitted the one-electron circle)
    else if (p.view == "Orbit") { const int lv = s.trans ? std::max(s.trans->from, s.trans->to) : p.n;   // during a jump the picture fits the bigger of the two circles
        const double nn = p.ostyle == "Bohr 1913" ? std::max(3, lv) : lv; sz = nn * nn / p.Z * aUnit(); }
    else sz = s.trans ? kExt[std::max(orbitals()[s.trans->from].n, orbitals()[s.trans->to].n)] / p.Z * aUnit() : cloudExtM();   // (0.39: the many-electron cloud has its own size)
    sz = fitSize(p, sz);
    s.szCur = !s.szCur || rmv ? sz : *s.szCur + (sz - *s.szCur) * (1 - std::exp(-4 * dt));
    const Vec3d tg = s.focus >= 0 && s.focus < int(s.nuc.size()) ? Vec3d{s.nuc[s.focus].x, s.nuc[s.focus].y, s.nuc[s.focus].z} : Vec3d{};
    const double k = rmv ? 1 : 1 - std::exp(-5 * dt), m = fitSpan();
    const double px = pxPerM(m, s.szCur) * std::pow(10, p.zoom);   // points per metre
    if (s.panTo) {
        const double k2 = rmv ? 1 : 1 - std::exp(-6 * dt);
        s.pan.x += (s.panTo->x - s.pan.x) * k2; s.pan.y += (s.panTo->y - s.pan.y) * k2;
        if (std::hypot(s.pan.x - s.panTo->x, s.pan.y - s.panTo->y) * px < 0.5) { s.pan.x = s.panTo->x; s.pan.y = s.panTo->y; s.panTo.reset(); }
    }
    if (!s.panTo) {                                            // zooming in after moving the picture must never push it off screen
        const double lim = 0.45 * m / px;
        if (std::isfinite(lim)) { s.pan.x = std::max(-lim, std::min(lim, s.pan.x)); s.pan.y = std::max(-lim, std::min(lim, s.pan.y)); }
        else { s.pan.x = 0; s.pan.y = 0; }
    }
    s.fc.x += (tg.x - s.fc.x) * k; s.fc.y += (tg.y - s.fc.y) * k; s.fc.z += (tg.z - s.fc.z) * k;
    // Paused (and a lab that starts paused under Reduced motion): the cloud still fills once, so the picture is never empty. Nothing moves; the dots are the same kind of samples.
    if (paused && !s.trans) syncSlicePlane();
    if (paused && sliceDots() && !s.trans && s.sliceFresh < cloudCap()) oweOrMake(true);   // (0.66.0: owed on the frame's budget, as a step's)
    else if (paused && p.view != "Orbit" && !chartOn() && !s.trans && s.k < cloudCap())
        addDots(std::min<long>(4000, cloudCap() - s.k));
}

long AtomLab::stepWant() const {
    if (sliceDots() && s.sliceFresh < cloudCap()) return std::min<long>(cloudCap() - s.sliceFresh, kSliceDotsPerFrame);
    return (s.boost > 0 ? 48 : p.view == "Slice" ? 64 : 16) * (cloudCap() / kCap);
}

double AtomLab::workMs() const {
    if (workClock) return workClock();
    return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

// 0.66.0: a step's (or a paused frame's) Slice dots: owed while the frame has a budget (makeOwedDots makes them, in order), else made now as before.
// A paused frame only fills a plane that moved (its old rule). At most 240 steps (a second) are owed: beyond that a step's dots are left out (only while
// the processor cannot keep up at all; a standing plane's dots are then replaced a little more slowly).
void AtomLab::oweOrMake(bool pausedFrame) {
    if (dotBudgetMs <= 0) {
        if (!pausedFrame) addDots(stepWant());
        else if (s.sliceFresh < cloudCap()) addDots(std::min<long>(cloudCap() - s.sliceFresh, kSliceDotsPerFrame));
        return;
    }
    if (owed_.size() < 240) owed_.push_back(pausedFrame ? 1 : 0);
}

void AtomLab::makeOwedDots() {
    if (dotBudgetMs <= 0 || owed_.empty()) return;
    struct End { double& d; ~End() { d = 0; } } end{deadline_};
    deadline_ = workMs() + dotBudgetMs;
    while (!owed_.empty()) {
        if (owedLeft_ == 0) {                                    // the oldest owed step starts: its dots by the rule of the moment, as the step would have
            owedLeft_ = owed_.front() ? (sliceDots() && s.sliceFresh < cloudCap() ? std::min<long>(cloudCap() - s.sliceFresh, kSliceDotsPerFrame) : 0) : stepWant();
            if (owedLeft_ <= 0) { owedLeft_ = 0; owed_.pop_front(); continue; }
        }
        const long got = addDots(owedLeft_);
        if (cut_) { owedLeft_ = std::max<long>(0, owedLeft_ - got); if (owedLeft_ == 0) owed_.pop_front(); break; }
        owedLeft_ = 0; owed_.pop_front();                        // (done, or fewer: the attempts ran out or the card's batch is on its way, as a step)
        if (pastDeadline()) break;
    }
}

void AtomLab::step(double dt) {
    advance(dt);
    if (chartOn()) return;                                      // the chart shows no electron: nothing to move
    if (superOn()) s.beatT += dt * p.speed / 5.0;                // the beat clock: 4 s per beat at Playback speed 5, faster or slower with the speed (step() does not run while paused)
    if (p.view != "Orbit") {
        if (s.trans) {
            // a cloud jump: the two populations of dots are fixed, nothing is sampled (the dots are being built: a quarter of them each frame, so the start of a jump is not one long frame)
            if (!jumpReady()) { const long up = std::min(s.jn, s.jb.done + std::max<long>(2000, (s.jn + 3) / 4)); if (s.jb.many) buildManyJump(up); else buildJump(up); }
        } else {
            // Slice (0.37): the dots lie ON the cut plane. When the plane moves (a drag, the Cut slider, the axis), new dots are made on the new plane, at most kSliceDotsPerFrame a
            // frame, and replace the oldest: the picture never empties during a drag, and a few frames after it stops every dot lies on the plane again (8 frames at Normal).
            syncSlicePlane();
            if (sliceDots()) oweOrMake(false);                   // 0.66.0: every Slice by one rule (owed on the frame's budget, else made now)
            else addDots(stepWant());
        }
        if (p.view == "Cloud 3D" && s.spin) s.yaw += 0.0015;   // slow auto-rotation
        return;
    }
    if (manyOn()) return;   // Orbit with every electron: still shell rings (0.50: they no longer turn; the turning was only for the eye); no one-electron orbit
    if (s.trans) { glideOrbit(dt); return; }      // Orbit, during a jump: the electron glides between the circles
    // Orbit: the electron on a Coulomb orbit, leapfrog (kick–drift–kick).
    // Time is slowed: h = dt·speed/Z² atomic time units per step (a nucleus with Z protons whirls the
    // electron Z² times faster, so it is slowed by Z² to keep the laps watchable).
    const double tu = tUnit(), au = aUnit(), h = dt * p.speed / (double(p.Z) * p.Z) * tu;
    const double kC = p.Z * au * au * au / (tu * tu);   // Z·e²/(4πε₀·μ) in these units (0.46.4: the reduced mass μ): a = −kC·r/|r|³
    auto acc = [&](double x, double y, double& ax, double& ay) { const double r3 = std::pow(std::hypot(x, y), 3); ax = -kC * x / r3; ay = -kC * y / r3; };
    // Sub-steps (0.10.6 stage 3, audit finding 14). A circular orbit n turns dt·speed/n³ radians per step (any Z). A leapfrog step of angle φ makes the radius wobble by about φ²/2 (measured:
    // n = 1 at speed 5, φ = 0.0208 rad, 302 steps per orbit: +2.2e-4; at speed 10: +8.7e-4; at speed 30, 50 steps per orbit: +7.8e-3). So one step is split into equal sub-steps so that none turns more than
    // kMaxStepAngle = 2π/300 rad (300 steps per orbit), the accuracy the default speed 5 has always had: the radius then stays within about 2.2e-4 and the energy within about 1e-4 at every speed. At speed 5 and
    // below nothing is split (n = 1 at speed 5 is 0.02083 rad, just under the limit), so the default motion is bit for bit the old one; only n = 1 above speed 5 is split (6 sub-steps at 30). The same
    // time passes and the electron turns at the same rate: nothing in the look or the speed changes, and the trail still gets one point per frame.
    const double stepAngle = dt * p.speed / (double(p.n) * p.n * p.n);
    const int sub = std::max(1, int(std::ceil(stepAngle / kMaxStepAngle)));   // no step turns more than kMaxStepAngle
    const double hs = h / sub;
    for (int k = 0; k < sub; k++) {
        double ax, ay;
        acc(s.x, s.y, ax, ay);
        s.vx += ax * hs / 2; s.vy += ay * hs / 2;
        s.x += s.vx * hs; s.y += s.vy * hs;
        acc(s.x, s.y, ax, ay);
        s.vx += ax * hs / 2; s.vy += ay * hs / 2;
    }
    s.t += h;
    s.trail.push_back({s.x, s.y});
    if (s.trail.size() > 500) s.trail.pop_front();
}

void AtomLab::ensureNucleus() {
    const std::string key = std::to_string(p.Z) + "," + std::to_string(p.N) + "," + std::to_string(s.nucSeed) + "," + p.dspin;
    if (s.nucKey != key) { s.nucAxis[0] = 0; s.nucAxis[1] = 0; s.nucAxis[2] = 1; s.nuc = buildNucleus(p.Z, p.N, s.nucAxis, deuteronSpinIndex(p.dspin)); s.nucKey = key; }
}

// ---------------------------------------------------------------- actions
void AtomLab::zoomAtom() { s.focus = -1; s.zoomTo = 0; s.panTo = Vec3d{}; }
void AtomLab::zoomNucleus() { s.focus = -1; s.zoomTo = zoomForNucleus(); s.panTo = Vec3d{}; }
void AtomLab::zoomNucleon() { if (s.focus < 0) s.focus = 0; s.zoomTo = zoomForNucleon(); s.panTo = Vec3d{}; }
bool AtomLab::showCentre() const { return p.zoom > 0.001 || (std::fabs(s.pan.x) + std::fabs(s.pan.y)) / kA0 > 0.002; }
bool AtomLab::showResetView() const {
    return p.view == "Cloud 3D" || (p.view == "Orbit" && (std::fabs(s.oyaw) > 0.02 || std::fabs(s.opitch - kPi / 2) > 0.02));
}
void AtomLab::resetView() {
    if (p.view == "Orbit") { s.oyaw = 0; s.opitch = kPi / 2; }
    else { s.yaw = 0.6; s.pitch = 0.35; }
}

// the ⓘ button, or a click on a nucleon to dive into it
const ChainGraph& AtomLab::decayGraph() const {
    if (chainZ_ != p.Z || chainN_ != p.N) { chainGraph_ = decayChain(p.Z, p.N); chainZ_ = p.Z; chainN_ = p.N; }
    return chainGraph_;
}
void AtomLab::walkRestart() { walkStart(s.walk, p.Z, p.N); }
void AtomLab::walkOnce() {
    if (s.walk.startZ != p.Z || s.walk.startN != p.N) walkStart(s.walk, p.Z, p.N);
    walkStep(s.walk, [] { return random(); });
}
const ChartScene& AtomLab::chartScene() const { chartScene_.build(viewW, viewH, s.chartView); return chartScene_; }
void AtomLab::setChartTilt(double t) {
    s.chartView.tilt = std::max(0.0, std::min(1.0, t));
    if (s.chartView.tilt == 0) { s.chartView.yawDeg = kChartYawDeg; s.chartView.pitchDeg = kChartPitchDeg; }   // 0.40: back to 2D: the next 3D starts from the first view
    chartScene_.build(viewW, viewH, s.chartView);
    s.chartView = chartScene_.clamped(s.chartView);
}
void AtomLab::chartZoomAt(double factor, double px, double py) {
    ChartViewState v = s.chartView;
    const double z = std::max(kChartZoomMin, std::min(kChartZoomMax, v.zoom * factor)), k = z / v.zoom;     // k: the factor actually applied (the limits cut it)
    // scaling the whole picture about (px, py) by k: final' = q + k (final - q), which is the same view with zoom z and the pan below
    v.panX = (1 - k) * (px - viewW / 2) + k * v.panX;
    v.panY = (1 - k) * (py - viewH / 2) + k * v.panY;
    v.zoom = z;
    chartScene_.build(viewW, viewH, s.chartView);
    s.chartView = chartScene_.clamped(v);
}
void AtomLab::chartTurn(double dx, double dy) {
    ChartViewState v = s.chartView;
    v.yawDeg -= dx * 0.4;                                       // 0.4 degrees a point: a drag across a 900-point picture turns it once round
    v.pitchDeg = std::max(kChartPitchMinDeg, std::min(kChartPitchMaxDeg, v.pitchDeg - dy * 0.3));
    chartScene_.build(viewW, viewH, v);
    s.chartView = chartScene_.clamped(v);
}
void AtomLab::chartPan(double dx, double dy) {
    if (s.chartView.zoom <= 1) return;                          // the whole chart is fitted (or smaller): nothing to move
    ChartViewState v = s.chartView;
    v.panX += dx; v.panY += dy;
    chartScene_.build(viewW, viewH, s.chartView);
    s.chartView = chartScene_.clamped(v);
}

void AtomLab::chooseNuclide(int Z, int N) {
    if (!chartHas(Z, N)) return;
    if (p.Z != Z) setParam("Z", double(Z));                     // (a new element: the neutrons first follow to its usual isotope)
    if (p.N != N) setParam("N", double(N));
}
bool AtomLab::chartStep(int dZ, int dN) {
    int Z = p.Z, N = p.N;
    if (!chartMove(Z, N, dZ, dN)) return false;
    chooseNuclide(Z, N);
    return true;
}

void AtomLab::click(double x, double y) {
    if (chartOn()) {                                           // the chart: a click on a tile chooses that nuclide
        int Z, N;
        if (s.chartView.flat() ? chartTileAt(chartLayout(viewW, viewH), x, y, Z, N) : chartScene().pick(x, y, Z, N)) chooseNuclide(Z, N);
        return;
    }
    if (s.infoBox && x >= s.infoBox->x && x <= s.infoBox->x + s.infoBox->w && y >= s.infoBox->y && y <= s.infoBox->y + s.infoBox->h) {
        s.more = !moreShown();
        moreStored = s.more;
        return;
    }
    const Hit* best = nullptr;
    for (const Hit& h : s.hits)
        if (std::hypot(x - h.X, y - h.Y) <= h.r && (!best || h.dep < best->dep)) best = &h;
    if (best) { s.focus = best->i; s.zoomTo = std::max(p.zoom, zoomForNucleon()); }
}

// Orbit: turn the atom. Cloud 3D: rotate. Slice: slide the cut. Cloud (side view): move.
// Shift, right or middle drag, or the ✥ button, always moves the picture.
void AtomLab::drag(double dx, double dy, bool forceMove) {
    if (chartOn()) {                                           // the chart: in 3D a drag turns it (0.40); flat, or with Shift / the right button, it moves the picture (only while zoomed in)
        if (s.chartView.tilt > 0 && !forceMove && s.dragMode != "move") chartTurn(dx, dy); else chartPan(dx, dy);
        return;
    }
    const std::string act = forceMove || s.dragMode == "move" || p.view == "Cloud" ? "move"
                          : p.view == "Orbit" ? "turn" : p.view == "Cloud 3D" ? "rotate" : p.view == "Slice" ? "cut" : "";
    const double m = fitSpan();
    if (act == "rotate") { s.yaw += dx * 0.01; s.pitch = std::max(-1.5, std::min(1.5, s.pitch + dy * 0.01)); return; }
    if (act == "turn") { s.oyaw += dx * 0.01; s.opitch = std::max(0.02, std::min(kPi - 0.02, s.opitch + dy * 0.01)); return; }
    if (act == "cut") {
        if (!s.cutF || std::fabs(*s.cutF - p.cut) > 0.011) s.cutF = p.cut;   // keep fractions between drag events; re-sync if the slider moved it
        s.cutF = std::max(-1.0, std::min(1.0, *s.cutF - dy / (0.45 * m)));
        setParam("cut", roundHalfUp(*s.cutF * 100) / 100);
        return;
    }
    if (act.empty()) return;
    const double px = pxPerM(m, s.szCur) * std::pow(10, p.zoom), lim = 0.45 * m / px;
    s.panTo.reset();
    s.pan.x = std::max(-lim, std::min(lim, s.pan.x + dx / px));
    s.pan.y = std::max(-lim, std::min(lim, s.pan.y + dy / px));
}

// ---------------------------------------------------------------- the readout
// the electron's energy in eV from its position and speed (kinetic + Coulomb, in atomic units)
// The orbit is integrated in atomic units for an infinitely heavy nucleus (½v² − Z/r in Hartree), whose circular orbit n has −Z²/(2n²) Hartree = −kRydInf Z²/n². The lab's one energy
// for an atom is levelEV (the reduced-mass level −rydM(Z, N) Z²/n², also used by Superposition and the spectrum), so the number is multiplied by rydM / kRydInf: for a circle it is then
// levelEV (to the integrator's error). Without it hydrogen read −13.61 eV here and −13.60 eV in the spectrum. (Non-relativistic, one electron, no fine structure: same model as levelEV.)
double AtomLab::orbitEnergyEV() const {
    const double r = std::hypot(s.x, s.y) / aUnit(), v2 = (s.vx * s.vx + s.vy * s.vy) / (kAuSpeed * kAuSpeed);   // (0.46.4: r in units of a_mu, so this is the reduced-mass energy exactly)
    return (0.5 * v2 - p.Z / r) * kHartreeEV * (rydM(p.Z, p.N) / kRydInf);
}

std::string AtomLab::info1() {
    const std::string head = std::string(name(p.Z)) + "-" + std::to_string(p.Z + p.N) + " (" + ionLabel(p.Z) + ")";
    if (superOn()) {
        const Superposition u = superState();
        const double eA = levelEV(p.Z, p.N, orbitals()[stateOrbital(u.idA)].n), eB = levelEV(p.Z, p.N, orbitals()[stateOrbital(u.idB)].n);
        const std::string shares = std::to_string(int(std::lround(100 - p.mix))) + " % / " + std::to_string(int(std::lround(p.mix))) + " %";
        // orthogonal states: the shares a^2 and b^2 are exactly the probabilities. Overlapping states: they are only the amplitudes of the mixture, which is renormalised, so the line says that instead
        const std::string split = overlap_ ? "(amplitudes " + shares + "; the states overlap by " + std::to_string(int(std::lround(100 * std::abs(overlapAB(u.idA, u.idB))))) + " %, so the mixture is renormalised)" : "(" + shares + ")";
        return head + "  ·  superposition " + atom::stateText(u.idA) + " + " + atom::stateText(u.idB) + " " + split +
               "  ·  E −" + fmtEV(-eA) + " and −" + fmtEV(-eB) + "  ·  " + beatSlowText();
    }
    if (p.view != "Orbit") {
        const Orbital& o = orbitals()[orbIndex(p.orb)];
        const long cnt = std::min<long>(s.k, cloudCap());
        double sum = 0;
        for (long i = 0; i < cnt; i++) sum += std::hypot(double(s.pts[3 * i]), double(s.pts[3 * i + 1]), double(s.pts[3 * i + 2]));
        // 0.37: the dots are independent exact samples, so their mean radius scatters only by sigma_r / sqrt(N) (0.5 % for 1s at N = 12,000; until 0.36 they were one correlated chain, worst
        // 17.7 % off in 96 runs). The note's figure is measured in tests/test_atom.cpp (testSampledRadius). It is shown only with the full ring, and not in the Slice view, whose dots lie on one plane.
        const std::string got = cnt >= kCap ? fmtLen(sum / cnt * 1e10) + kSampledRadiusNote : std::string("…");
        if (diracCloudOn()) {                                    // 0.49.3: the Dirac cloud (dirac_cloud.h): its state, its level (Dirac + Lamb shift where known), its exact <r>
            const int j2 = diracJ2(), kap = diracKappa(o.l, j2);
            const std::string th = fmtLen(diracMeanRadius(p.Z, o.n, kap) * lenScale() * kA0Ang);
            const std::string en = "E −" + fmtEV(-diracQedEV(p.Z, p.N, o.n, o.l, j2)) + (lambKnown(p.Z, o.n) ? " (Dirac + Lamb shift)" : " (Dirac)");
            const std::string st = diracText() + (j2 == 1 ? std::string(" (round in every m)") : s.round ? std::string(", round (every m)") : ", lined up (m = j)");
            if (sliceDots()) return head + "  ·  " + st + "  ·  " + en + "  ·  avg r: theory " + th + " (a slice's dots are not the whole cloud)";
            return head + "  ·  " + st + "  ·  " + en + "  ·  avg r: sampled " + got + ", theory " + th;
        }
        int n1 = 0, n2 = 0, mq = 0;
        if (starkCloudOn() && starkPar(n1, n2, mq)) {              // 0.93.0: the parabolic state's cloud (stark.h): its name, its level in the field, its exact <r>
            const stark::Par sp{o.n, n1, n2, mq};
            const std::string th = fmtLen(stark::meanR(starkSampler(o.n, n1, n2, mq)) * lenScale() * kA0Ang);
            const double mu = stark::reducedMassH(), Eh = stark::energyAu(sp, starkFieldAu(), mu) * kHartreeEV;
            const std::string st = "parabolic state " + stark::name(sp) + " of n = " + std::to_string(o.n);
            return head + "  ·  " + st + "  ·  E −" + fmtEV(-Eh) + " in the field (to second order)  ·  avg r: sampled " + got + ", theory " + th;
        }
        const std::string th = fmtLen(meanRadius(o.n, o.l, p.Z) * lenScale() * kA0Ang);   // exact <r> for hydrogen-like atoms: the same function the radial graph uses
        const double E = levelEV(p.Z, p.N, o.n);   // the same reduced-mass level as the spectrum and Superposition (finding 3 of the 0.10.6 audit)
        // 0.46 (review A4): from kRelativityZ the non-relativistic level is far off (U91+ 1s: -115 keV against Dirac's -132 keV), so the headline gives the Dirac energy (point nucleus,
        // reduced mass: dirac.h diracEV) of the orbital's j = l -+ 1/2 levels first and labels the lab's own non-relativistic value
        std::string en = "E −" + fmtEV(-E);
        if (p.Z >= kRelativityZ) {
            const double dHi = diracQedEV(p.Z, p.N, o.n, o.l, 2 * o.l + 1);   // 0.49.3: with the Lamb shift where it is known (n <= 2, dirac.h)
            en = "E −" + fmtEV(-dHi);
            if (o.l > 0) en += " / −" + fmtEV(-diracQedEV(p.Z, p.N, o.n, o.l, 2 * o.l - 1));
            en += std::string(lambKnown(p.Z, o.n) ? " (Dirac + Lamb shift" : " (Dirac") + (o.l > 0 ? ", j = " + std::to_string(2 * o.l + 1) + "/2 / " + std::to_string(2 * o.l - 1) + "/2" : std::string()) + "; non-relativistic −" + fmtEV(-E) + ", the cloud's model)";
        }
        if (sliceDots()) return head + "  ·  " + stateText() + " orbital  ·  " + en + "  ·  avg r: theory " + th + " (a slice's dots are not the whole cloud)";
        return head + "  ·  " + stateText() + " orbital  ·  " + en + "  ·  avg r: sampled " + got + ", theory " + th;
    }
    const double r = std::hypot(s.x / kA0, s.y / kA0) * kA0Ang, E = orbitEnergyEV();
    const double T = 2 * kPi * std::pow(p.n, 3) / (double(p.Z) * p.Z) * kAuTimeAs * lenScale();   // the orbit's period, as (0.46.4: reduced mass)
    return head + "  ·  n=" + std::to_string(p.n) + "  ·  r " + fmtLen(r) + "  ·  E " + (E < 0 ? "−" : "") + fmtEV(std::fabs(E)) + "  ·  period " + fmtT(T) +
           "  ·  the circle is Bohr's 1913 model beside a modern nucleus";   // 0.46 (review A11)
}

// 0.39.1: the nucleus lines of the readout (its shape at the Nucleus zoom, what the Density look shows), shared by the one-electron and the many-electron readouts: the many-electron
// one returned before them, so zooming to the nucleus with Electrons All lost them
static void nucleusLines(const AtomLab& L, std::vector<std::string>& lines, int part) {   // part 1: the shape; 2: the Density look; 3: both
    if ((part & 1) && L.s.layer == "Nucleus" && L.p.Z + L.p.N >= 12) {
        const Shape sh = nucShape(L.p.Z, L.p.N);   // 0.10.9 stage 2: a measured quadrupole moment where it can be used, else B(E2), else an estimate: the line names which (one of three labels, never one vague word)
        // 0.46.4 (outside review): Pb-208, Ca-40 ... are drawn round because FRDM(2012) calculates them round; the label said "(from a measured B(E2))"
        const bool roundByFrdm = sh.src == ShapeSource::BE2 && sh.beta == 0 && nucFrdmKnown(L.p.Z, L.p.N) && std::fabs(nucFrdmBeta(L.p.Z, L.p.N)) < kDeformedCoreBeta;
        const bool pastLimit = sh.src == ShapeSource::BE2 && !roundByFrdm && !std::isfinite(nucBE2ShapeBeta(L.p.Z, L.p.N));   // C-12: its B(E2) needs |beta2| > 0.6
        lines.push_back("shape: " + std::string(nucShapeWord(sh.beta)) + " " + (roundByFrdm ? std::string("(FRDM 2012 calculates it round; its measured B(E2) is a vibration about that)")
                                                                               : pastLimit ? std::string("(its measured B(E2) needs more deformation than the lab's shape allows: see the card)")
                                                                                           : shapeSourceLabel(sh.src, L.p.Z)));
    }
    if ((part & 2) && L.p.style == "Density" && L.p.Z == 0 && L.p.N == 1 && L.s.layer != "Atom")   // 0.46 (review A5)
        lines.push_back("a neutron has no net charge, so the Density look draws no charge cloud: the neutron is drawn as a ball (its charge is spread as a positive core and a negative edge at rest, "
                        "a model-dependent picture; More info)");
    if ((part & 2) && L.p.style == "Density" && L.s.densityDrawn) {   // 0.22: what the Density look shows (atom_view draws it while the nucleons are small on screen)
        const RadiusRow* rr = chargeRadius(L.p.Z, L.p.N);
        // 0.46.5: the edge of what is drawn (it said "about 2.4 fm" for every nucleus, right only for the Fermi shape of A > 16); the deuteron's halo and spin state; C-12 and O-16 measured
        const int A = L.p.Z + L.p.N;
        double ha = 0, hal = 0;
        const bool ho = nucleusHoCharge(L.p.Z, L.p.N, ha, hal);
        std::string shape;
        if (A == 2)
            shape = "no edge: a dense middle and a long thin halo (from the deuteron's wave function, CD-Bonn: the neutron and the proton are more than 3 fm apart " +
                    fmt::fixed(100 * deuteronFarShare(3.0), 0) + " % of the time)  ·  spin " +
                    (L.p.dspin == "M = ±1" ? std::string("M = ±1: the charge a little longer along the spin axis (the axis across the view); where the two nucleon centres are most often found is a dumbbell, blurred here by the proton's own size")
                     : L.p.dspin == "M = 0" ? std::string("M = 0: the charge a little flatter along the spin axis (across the view); where the two nucleon centres are most often found is a doughnut round it, blurred here by the proton's own size")
                                            : std::string("unpolarised: the three spin states mixed, so round (Controls › Nucleus › Deuteron spin shows each state)"));
        else {
            shape = "a soft edge " + fmt::fixed(nucEdgeFm(L.p.Z, L.p.N), 2) + " fm thick (90 % to 10 %), no surface";
            if (ho) shape += L.p.Z == 8 ? "  ·  the measured shape (de Vries 1987): slightly lower in the very middle" : "  ·  the measured shape (de Vries 1987): flat in the middle";
            else if (A >= 3 && A <= 16) shape += "  ·  drawn as a Gaussian (no measured profile saved for this light nucleus)";
        }
        lines.push_back("nucleus: its charge density, as electron scattering measures it (no nucleons drawn)  ·  " +
                        (rr && std::isfinite(rr->rms) ? "rms charge radius " + fmt::fixed(rr->rms, 3) + (L.p.Z == 1 && L.p.N <= 1 ? " fm (CODATA 2022)" : " fm (IAEA)") : std::string("no measured radius: the lab's rule size")) +
                        "  ·  " + shape + (A >= 2 ? "  ·  zoom in further for one snapshot of the nucleons" : ""));
    }
}

// 0.54.0: the readout's lines while the magnetic field is on: the g of the level now (or each J level of the term now), LS and measured, and the gap
std::vector<std::string> AtomLab::fieldInfo() const {
    std::vector<std::string> out;
    const int k = zeemanKind();
    if (!(p.bfield > 0) || k < 0 || p.view == kChartView) return out;
    zeeman::Term t;
    std::vector<int> js;
    std::vector<double> meas;
    if (k == 2) {
        t = zeeman::termLevel(p.Z, s.lvl);
        const JLevel& L = jumpLevels(p.Z)[std::size_t(s.lvl)];
        js = {L.j2}; meas = {zeeman::measuredGLevel(p.Z, L.conf, L.term, L.j2)};
    } else {
        int n = p.n, l = -1;
        if (k == 1) { n = valenceNow().n; l = valenceNow().l; }
        else if (p.view != "Orbit" && orbIndex(p.orb) >= 0) { n = orbitals()[std::size_t(orbIndex(p.orb))].n; l = orbitals()[std::size_t(orbIndex(p.orb))].l; }
        if (l < 0) return out;                                   // the Orbit view of the one-electron atom: no l chosen
        t = k == 1 ? zeeman::termAlkali(p.Z, n, l) : zeeman::termOneElectron(p.Z, p.N, n, l);
        js = t.J2;
        for (const int j : js) meas.push_back(k == 1 ? zeeman::measuredGAlkali(p.Z, n, l, j) : NAN);
    }
    char b[200];
    std::snprintf(b, sizeof b, "in the field (%.4g T): ", p.bfield);
    if (t.mode == zeeman::Mode::None) { out.push_back(b + t.why); return out; }
    for (std::size_t i = 0; i < js.size(); i++) {
        const std::string J = js[i] % 2 ? std::to_string(js[i]) + "/2" : std::to_string(js[i] / 2);
        const double gls = zeeman::landeLS(t.L, t.S2, js[i], zeeman::gsCodata());
        std::string line = std::string(b) + t.name + " J=" + J + ": g (LS) = " + fmt::fixed(gls, 5);
        if (std::isfinite(meas[i])) {
 char g[32]; std::snprintf(g, sizeof g, "%.7g", meas[i]);
            line += ", g (measured, NIST ASD) = " + std::string(g) + ", gap " + (meas[i] - gls >= 0 ? "+" : "−") + fmt::fixed(std::fabs(meas[i] - gls), 5);
        } else line += ", g (measured): NIST lists none";
        out.push_back(line);
    }
    return out;
}
std::vector<std::string> AtomLab::info() {
    std::vector<std::string> ls = infoBody();
    for (const std::string& t : fieldInfo()) ls.push_back(t);
    return ls;
}
std::vector<std::string> AtomLab::infoBody() {
    const bool msgOn = !s.msg.empty() && nowMs() < s.msgUntil;
    if (p.view == "Orbit" && p.ostyle == "Bohr 1913" && !manyOn()) {   // Bohr 1913: only what Bohr knew (0.39.1: with every electron, the shell readout below)
        const double r = std::hypot(s.x / kA0, s.y / kA0) * kA0Ang, E = orbitEnergyEV();
        std::vector<std::string> ls = {"Bohr model, 1913  ·  " + std::string(name(p.Z)) + "  ·  nucleus +" + std::to_string(p.Z) + "  ·  n=" + std::to_string(p.n) +
                                       "  ·  r " + fmtLen(r) + "  ·  E " + (E < 0 ? "−" : "") + fmtEV(std::fabs(E))};
        if (!s.last.empty()) ls.push_back(s.last);
        if (msgOn) ls.push_back(s.msg);
        if (moreShown()) {
            ls.push_back("Bohr’s rules: the electron circles only on the allowed orbits (n = 1, 2, 3 …) and does not give off light while it stays on one.");
            ls.push_back("A jump to another orbit takes or gives exactly one photon, with the energy difference. The nucleus is drawn as one ball, not to scale: in 1913 nobody knew about neutrons (1932), protons as parts (1919) or quarks.");
        }
        return ls;
    }
    if (manyOn()) {                                              // 0.39: the neutral atom (atom/hf.h; 0.41: Dirac-Hartree-Fock): what it is, what is measured, what the colours mean, the model's limits
        const hf::Atom& a = *manyAtom();
        std::vector<std::string> ls;
        const hf::NistIe* ni = hf::nistIe(p.Z, 0);
        ls.push_back(std::string(name(p.Z)) + "-" + std::to_string(p.Z + p.N) + "  ·  neutral atom, " + std::to_string(a.electrons()) + " electrons  ·  " + hf::configText(a));
        { const std::string dl = decLine(); if (!dl.empty()) ls.push_back(dl); }   // (0.39.1: the nucleus's decay, as in the one-electron readout)
        if (ni) ls.push_back("first ionization energy " + ni->ie + " eV (NIST, " + hf::nistKind(*ni) + ")  ·  ground level " + hf::levelText(ni->level) + " (NIST)");
        if (levelOn()) {                                         // 0.47.3: a level atom (levels.h)
            const JLevel& L = jumpLevels(p.Z)[std::size_t(s.lvl)];
            const GraspLevel* gl = levelCloud(p.Z, s.lvl);
            ls.push_back("level: " + levelName(p.Z, s.lvl) + (s.lvl > 0 ? " (excited, " + fmtEV(L.cm * kHcEVnm * 1e-7) + " above the ground level, NIST" + (L.calc ? ": its calculated value" : "") + ")" : " (ground level)") +
                         (sliceDots() ? sliceShareText(a) : outerDenserText(a)) + "  ·  every electron from a correlated calculation (GRASP2018)  ·  levels and lines: NIST ASD");
            if (p.view != "Orbit" && manySub() < 0) ls.push_back(levelShapeNote());
            if (gl) {
                const double q = gl->qualityPercent();
                ls.push_back(std::isfinite(q) ? "cloud quality: its calculated energy is " + std::string(q >= 0 ? "+" : "−") + fmt::fixed(std::fabs(q), 2) + " % from NIST's (the energies shown are NIST's)"
                                              : "cloud quality: the ground level (the reference of every level's calculated energy)");
            }
            if (s.trans && s.jb.many) ls.push_back("jumping: " + levelName(p.Z, s.trans->from) + " → " + levelName(p.Z, s.trans->to) + " (drawn as a cross-fade of the two clouds)");
            if (!sliceDots() && moreShown())
                ls.push_back(p.vdots == "True share" ? "the outer electrons' dots (outside the closed shells): their true share, " + trueShareText(a) + ", so their cloud and its jumps are faint against the core; "
                                                       "\"Outer electron's dots: Easier to see\" (Controls) draws them denser"
                                                     : "the outer electrons' dots (outside the closed shells): " + numText(100 * kValenceDotShare, 2) + " % of the dots more than their true share, so they and their jumps can be seen; Slice shows the true shares");
            if (moreShown()) ls.push_back(levelModelNote());
        }
        if (valenceOn()) {                                       // 0.46 (B3)
            const AlkOrbital c = valenceNow();
            const double e = alkaliTermCm(p.Z, c.n, c.l);
            const GraspLevel* gl = p.mrel != "Non-relativistic" ? graspOfAtom(&a) : nullptr;   // 0.47.2: the GRASP2018 cloud of this level
            const std::string lvName = gl ? valName(c.n, c.l) + " " + "2" + std::string(1, "SPDF"[c.l]) + std::to_string(gl->j2) + "/2" : valName(c.n, c.l);
            ls.push_back("outer electron: " + lvName + (s.valN > 0 ? " (excited, " + fmtEV((gl ? gl->nistCm : e) * kHcEVnm * 1e-7) + " above the ground level, NIST)" : " (ground level)") +
                         (sliceDots() ? sliceShareText(a) : p.vdots == "True share" ? "  ·  its dots at their true share, " + trueShareText(a) + ", so its cloud and its jumps are faint"   // 0.54.1
                                                              : "  ·  its dots drawn " + numText(kValenceDotShare * a.electrons() + 1 - kValenceDotShare, 2) + "× denser than its true share") +   // 0.48 (review): the factor, Na 3.5 to Cs 14.5
                         (gl ? std::string("  ·  every electron from a correlated calculation (GRASP2018)") : std::string("  ·  the core kept fixed (a model: frozen-core Dirac-Hartree-Fock)")) +
                         "  ·  place it with \"Put the outer electron in\" (Photons) or let Emit / Absorb make it jump; levels and lines measured (NIST ASD)");
            if (gl) {                                            // 0.47.2: the shape is the aligned state M_J = J; the cloud's quality figure
                const std::string Jt = std::to_string(gl->j2) + "/2";
                if (p.view != "Orbit" && manySub() < 0)
                    ls.push_back(gl->j2 == 1 ? "a J = 1/2 level is round in every M_J state, so " + lvName + " is drawn round"
                                             : s.round ? "drawn round: the free atom, an even mix of all M_J (its average keeps only the round part); \"Lined up\" shows the state M_J = " + Jt
                                             : "the shape drawn is the aligned state M_J = " + Jt + " (as in a magnetic field along z); a free atom is an even mix of all M_J, and its average is round");
                const double q = gl->qualityPercent();
                ls.push_back(std::isfinite(q) ? "cloud quality: its calculated energy is " + std::string(q >= 0 ? "+" : "−") + fmt::fixed(std::fabs(q), 2) + " % from NIST's (the energies shown are NIST's)"
                                              : "cloud quality: the ground level (the reference of every level's calculated energy)");
            } else if (c.l > 0 && c.l <= 3 && p.view != "Orbit" && manySub() < 0)   // 0.47: the shape (manyDot, valenceDir)
                ls.push_back("the outer electron is drawn as its m = 0 orbital (" + valName(c.n, c.l) + std::string(c.l == 1 ? "z" : c.l == 2 ? "z²" : "z³") +
                             ") pointed along z so you can see its shape; a free atom has no preferred direction, so its true average picture is round");
            if (!sliceDots() && moreShown()) {   // 0.46.1: the outer electron's dots are drawn denser (manyDot; the main line says so in short)
                const int ne = a.electrons();
                if (p.vdots == "True share")     // 0.54.1
                    ls.push_back("the outer electron's dots: its true share, 1 of " + std::to_string(ne) + " electrons (" + numText(100.0 / ne, 2) + " % of the dots), so its cloud and its jumps are faint against the core; "
                                 "\"Outer electron's dots: Easier to see\" (Controls) draws it denser");
                else
                    ls.push_back("the outer electron's dots: " + numText(100 * (kValenceDotShare + (1 - kValenceDotShare) / ne), 2) + " % of the dots (really 1 of " + std::to_string(ne) +
                                 " electrons, " + numText(100.0 / ne, 2) + " %): drawn denser so it and its jumps can be seen; Slice shows the true shares");
            }
            if (s.trans && s.jb.many) ls.push_back("jumping: outer electron " + valName(alkaliOrbitals(p.Z)[std::size_t(s.trans->from)].n, alkaliOrbitals(p.Z)[std::size_t(s.trans->from)].l) + " → " +
                                                   valName(alkaliOrbitals(p.Z)[std::size_t(s.trans->to)].n, alkaliOrbitals(p.Z)[std::size_t(s.trans->to)].l) + " (drawn as a cross-fade of the two clouds)");
            if (moreShown() && gl) ls.push_back(graspModelNote());
            else if (moreShown()) ls.push_back(alkaliModelNote() + ". The excited orbitals are computed for this app (tools/dhf/valence.py), the method checked against Safronova, Johnson and Derevianko, PRA 60, 4476 (1999)");
        }
        if (ni && s.valN == 0 && s.lvl == 0 && !hf::configMatchesNist(a, *ni))
            ls.push_back("NIST's ground configuration is " + hf::shellsText(ni->shells) + "; the one drawn is " + hf::configText(a));
        if (!ni) ls.push_back("ground configuration " + a.shells + ", level J = " + a.level + " (predicted: " + a.source + ")");
        const int one = manySub();
        if (p.view == "Orbit") {                                 // 0.39.1: the shells as rings
            std::string sh;
            for (const Ring& g : manyRings()) sh += std::string(sh.empty() ? "" : " ") + hf::shellLetter(g.n) + " " + std::to_string(g.electrons);
            ls.push_back("rings = the shells (" + sh + "), each at the average distance of its electrons (" + manyModel() + ")  ·  a picture of the shells, not orbits: real electrons are clouds");
            if (shellsEnlarged() && s.layer == "Atom") ls.push_back("radius NOT to scale: the rings are spread out (the cloud's enlarged-shells mapping) so the inner ones can be seen; Radius: True scale puts each ring at its real average distance");
        } else if (one < 0) {
            if (p.view == "Slice") ls.push_back("cut plane: " + std::string(p.cutAxis == "Horizontal" ? "z" : "y") + " = " + fmt::fixed(cutOffsetM() * 1e10, 2) + " Å  ·  every dot lies on the plane, spread like the whole atom's electron density there");
            {   std::string letters; int top = 0;               // 0.41: this atom's shells (it listed K to Q for every atom)
                for (const hf::Subshell& sh : a.sub) top = std::max(top, sh.n);
                for (int n = 1; n <= top; n++) letters += std::string(n > 1 ? ", " : "") + hf::shellLetter(n);
                // 0.46.1 (outside review: the readout ran to about 10 lines and covered the picture): the legend in the picture names the shells, so this line waits for More info
                if (moreShown()) ls.push_back("colours = the shells " + letters + " (inner to outer); each dot is a place where an electron may be found, not an electron"); }
            // 0.46.6 (outside review: zoomed into uranium's nucleus it still spoke of the shells): the nucleus is drawn at its true size; only the electron dots are spread
            if (shellsEnlarged() && s.layer != "Atom") ls.push_back("the nucleus is drawn at its true size; the electron dots round it are spread out (Shells enlarged)");
            else if (shellsEnlarged()) ls.push_back(moreShown() ? "radius NOT to scale: the inner shells are spread out so they can be seen (the outer edge is true); Radius: True scale shows real distances"
                                                           : "radius NOT to scale (inner shells spread out; Radius: True scale shows real distances)");
        }
        else {
            if (p.view == "Slice") ls.push_back("cut plane: " + std::string(p.cutAxis == "Horizontal" ? "z" : "y") + " = " + fmt::fixed(cutOffsetM() * 1e10, 2) + " Å  ·  every dot lies on the plane");
            const hf::Subshell& sh = a.sub[std::size_t(one)];
            ls.push_back(std::to_string(sh.n) + "spdf"[sh.l] + " orbital of " + lower(name(p.Z)) + " (" + std::to_string(sh.occ) + (sh.occ == 1 ? " electron" : " electrons") +
                         (sh.l > 0 && a.relativistic ? ": " + hf::jText(sh) : std::string()) + ")  ·  average distance " +
                         fmtLen(hf::meanRadius(sh) * kA0Ang) + "  ·  " +
                         (usesWheel() ? "colour = the phase of the wave (see the colour ring)"   // 0.39.1: a complex orientation or Phase Wheel (it said "two colours" there too)
                                      : sh.n - sh.l - 1 == 0 && sh.l == 0 ? "one colour: no node" : "two colours = the + and − halves of the wave"));
        }
        if (!s.last.empty()) ls.push_back(s.last);
        if (msgOn) ls.push_back(s.msg);
        nucleusLines(*this, ls, 3);
        if (a.relativistic && graspOfAtom(&a))   // 0.47.2: a GRASP2018 cloud is correlated (the details are in its own More info line above)
            ls.push_back("model: GRASP2018 multiconfiguration Dirac-Hartree-Fock and CI with the Breit interaction and QED (core-valence correlation; More info: the details)");
        else if (a.relativistic)   // (0.46.1: the details wait for More info)
            ls.push_back(moreShown() ? "model: Dirac-Hartree-Fock (relativistic; each electron in the average field of the others; no electron correlation, the largest missing effect: 2 % in neon's <r²>; no Breit or QED terms)"
                                     : "model: Dirac-Hartree-Fock (relativistic, no electron correlation; More info: the limits)");
        else
            ls.push_back(std::string("model: Hartree-Fock, the non-relativistic limit (for comparison)") +
                         (p.Z >= 55 ? "  ·  for an atom this heavy relativity shrinks the s and p shells and widens d and f: Model Relativistic shows the real sizes" : ""));
        if (moreShown()) {
            ls.push_back(std::string("orbitals: computed for this app (tools/dhf), checked against ") + (a.relativistic ? (p.Z <= 109 ? "Visscher & Dyall, At. Data Nucl. Data Tables 67, 207 (1997)" : "Visscher & Dyall, At. Data Nucl. Data Tables 67, 207 (1997) for Z = 1 to 109 (their table ends at meitnerium, so not this element)")
                                                                                                    : (p.Z <= 103 ? "the Hartree-Fock orbitals of T. Koga et al. (1999, 2000)" : "the Hartree-Fock orbitals of T. Koga et al. (1999, 2000) for He to Lr (their tables end at lawrencium, so not this element)")) + "  ·  configurations: " + a.source + "  ·  energies and levels: NIST Atomic Spectra Database 5.12");
            ls.push_back("this nucleus (" + std::string(name(p.Z)) + "-" + std::to_string(p.Z + p.N) + ") in real life: " + isoInfo(p.Z, p.N) + "  ·  zoomed to: " + s.layer);
        }
        return ls;
    }
    // 0.46.7 (outside review): zoomed into one nucleon, the electron's cloud is far outside the view, so the first line no longer gives its orbital and radius
    const bool inNucleon = s.layer == "Nucleon" || s.layer == "Quarks";
    std::vector<std::string> lines = {inNucleon ? std::string(name(p.Z)) + "-" + std::to_string(p.Z + p.N) + " (" + ionLabel(p.Z) + ")  ·  zoomed into one nucleon: the electron's cloud is far outside this view"
                                                : info1()}, more;   // 'more' lines show after a tap on ⓘ (on by default on wide screens)
    { const std::string dl = decLine(); if (!dl.empty()) lines.push_back(dl); }
    if (s.trans) lines.push_back("jumping: " + lvl(s.trans->from) + " → " + lvl(s.trans->to));
    if (!s.last.empty()) lines.push_back(s.last);
    if (msgOn) lines.push_back(s.msg);
    if (p.view != "Orbit" && p.zoom >= 2) {
        const Orbital& o = orbitals()[orbIndex(p.orb)];
        if (o.l > 0) more.push_back("electron density at the nucleus: 0 (this orbital has a node there)");
        else {
            const double P = 4.0 / 3 * std::pow(p.Z, 3) * std::pow(nucRA0() / lenScale(), 3) / std::pow(o.n, 3);   // ≈ |ψ(0)|² × the nucleus's volume
            // P is |psi(0)|² of the NON-relativistic wave times the nucleus's volume. For heavy atoms relativity makes the density at the nucleus of an s electron larger (the Dirac wave is larger there);
            // this lab does not compute that and gives no correction factor.
            more.push_back("chance the electron is inside the nucleus at any instant: ≈ " + sci(P) + " (1 in " + sci(1 / P) + ")  ·  non-relativistic estimate: for heavy atoms relativity makes the real value larger, and this lab does not work it out");
        }
    }
    if (p.view == "Slice") lines.push_back("cut plane: " + std::string(p.cutAxis == "Horizontal" ? "z" : "y") + " = " + fmt::fixed(cutOffsetM() * 1e10, 2) + " Å");
    if (superOn()) { lines.push_back(beatKindText()); lines.push_back("colour = the phase of the wave ψ relative to the first orbital (see the colour ring); it turns once per beat"); }
    else if (diracCloudOn()) lines.push_back("one colour: a Dirac wave has four components, so it has no one sign or phase to draw");   // 0.49.3
    else if (p.view != "Orbit") lines.push_back(usesWheel() ? "colour = the phase of the wave ψ (see the colour ring); it is not a place"
                                                : orbitals()[orbIndex(p.orb)].n == 1 ? "one colour: ψ has the same sign everywhere (1s has no node)"   // 0.22: a hydrogen-like orbital has n − 1 nodes (Griffiths, Introduction to Quantum Mechanics, 4.2), so only 1s never changes sign
                                                : "two colours = the + and − halves of the wave ψ (zero where they meet)");
    if (p.view != "Orbit" && p.outline == "On") {
        lines.push_back("thin line: the 90% outline (the electron is found inside it 90% of the time)");
        more.push_back(std::string(p.view == "Slice" ? "90% outline: the line where the cut plane crosses the 90% surface. " : p.view == "Cloud" ? "90% outline: the shadow of the 90% region seen from the side. " : "90% outline: faint lines across the 90% surface, turning with the cloud. ") + "One electron, non-relativistic, hydrogen-like orbital with nuclear charge " + std::to_string(p.Z) + ", the same model as the cloud. It is the boundary of the smallest region that holds 90% of the probability, which follows the orbital's real shape; atoms with more electrons have different shapes.");
    }
    nucleusLines(*this, lines, 1);
    // 0.22 stage 6: always shown, one short line: the model, and (Z > 1) that the picture is the one-electron ion, while the melting point and the card are for the ordinary element
    // 0.46.1 (outside review): from kRelativityZ the energy over the picture is Dirac's (0.46, A4), so the model line no longer calls the whole readout non-relativistic there
    const bool diracHead = p.Z >= kRelativityZ && p.view != "Orbit";   // (the Orbit view's energy is the Bohr circle's, non-relativistic)
    const std::string modelHead = diracCloudOn() ? "model: one electron, Dirac equation (the cloud and the energy)"   // 0.49.3
                                : diracHead ? "model: one electron; the cloud is non-relativistic, the energy above is the Dirac value" : "model: one electron, non-relativistic";
    lines.push_back(p.Z == 1 ? modelHead
                             : modelHead + "  ·  the picture is " + ionLabel(p.Z) + " (" + std::to_string(p.Z - 1) + " electrons removed); the card is for ordinary " + lower(name(p.Z)));
    more.push_back((p.Z == 1 ? std::string("") : "ordinary " + lower(name(p.Z)) + ": ") + tempLine(p.Z));
    more.push_back("this nucleus (" + std::string(name(p.Z)) + "-" + std::to_string(p.Z + p.N) + ") in real life: " + isoInfo(p.Z, p.N) + "  ·  zoomed to: " + s.layer);
    {   // the model, said at EVERY Z: the cloud is non-relativistic. The Dirac comparison is the lab's own formula (see kRelativityZ in atom_core.h)
        const double zx = p.Z * kAlpha, zx2 = zx * zx, sq = std::sqrt(1 - zx2), pct = 100 * zx2 / ((1 + sq) * (1 + sq));   // 100 ((1 - sq) / ((Z alpha)^2 / 2) - 1) = 100 (Z alpha)^2 / (1 + sq)^2, written without the subtractions
        if (diracCloudOn())   // 0.49.3: the Dirac cloud (dirac_cloud.h)
            more.push_back("model: one electron, Dirac equation, point nucleus (Cloud model: " + p.hrel + "). The cloud is the exact Dirac state " + diracText() + ": its large and small components, " +
                           numText(100 * diracSmallShare(p.Z, orbitals()[std::size_t(fullOrb())].n, diracKappa(orbitals()[std::size_t(fullOrb())].l, diracJ2())), 3) + " % of the electron in the small one; "
                           "drawn in one colour (a four-component wave has no one phase). Lined up is the state m = j (as in a magnetic field along z), Round the free atom, every m alike. "
                           "Not in the wave: the Lamb shift and the size of the nucleus (they are in the energy where known); lengths in the reduced-mass Bohr radius. "
                           "For a point nucleus the Dirac 1s level lies " + numText(pct, 2) + " % deeper than the non-relativistic one.");
        else if (!diracHead)
            more.push_back("model: one electron, non-relativistic (no relativity, fine structure, Lamb shift or spin). For a point nucleus the Dirac equation puts the 1s level " + numText(pct, 2) + " % deeper than this lab's.");
        else   // 0.46.1: the headline energy is Dirac's here
            more.push_back("model: one electron. The cloud, the orbitals and the photon jumps are non-relativistic (no fine structure, Lamb shift or spin); the energy over the picture is the Dirac level for a point nucleus, "
                           "which for 1s lies " + numText(pct, 2) + " % deeper than the non-relativistic one.");
    }
    if (p.dirac == "On") more.push_back(diracCloudOn()   // 0.49.3: the spectrum's setting and the cloud's model are separate settings
        ? std::string("relativity setting (spectrum): ON. The spectrum cards show Dirac energies and lines with the nucleus's recoil, and the Lamb shift for n = 1 and 2; the cloud, its energy and its photons are Dirac too (Cloud model).")
        : std::string("relativity setting (spectrum): ON. The spectrum cards show Dirac energies and lines with the nucleus's recoil, and the Lamb shift for n = 1 and 2. The cloud and the photons of its jumps are non-relativistic here (Cloud model"
                      ": Dirac draws the Dirac cloud)" + std::string(diracHead ? "; the energy over the picture is the Dirac value from cobalt, Z = 27, on." : ".")));
    if (p.Z >= kRelativityZ) {   // relativity: the 1s electron of a heavy nucleus moves at a large fraction of light speed
        const double x = p.Z * kAlpha, model = -levelEV(p.Z, p.N, 1), real = kMecEV * x * x / (1 + std::sqrt(1 - x * x));   // Dirac 1s: mc²(1 − √(1−(Zα)²)) = mc² (Zα)² / (1 + √(1−(Zα)²)), without the subtraction (0.10.8 stage 2)
        lines.push_back("1s electron speed: "   /* 0.22 stage 6: always shown (not only under More info): from Z = 27 the non-relativistic model is noticeably off */ + fmt::fixed(100 * x, 0) + "% of light speed  ·  Dirac 1s energy −" + fmtEV(real) + " vs −" + fmtEV(model) + " non-relativistic");   // (0.46.1: was "real ... in this lab", but the lab's headline is Dirac's from Z = 27)
    }
    nucleusLines(*this, lines, 2);                               // (0.39.1: the Density look line, in its old place)
    if (s.layer != "Atom" && !s.densityDrawn) more.push_back(std::string("the nucleons as drawn are one possible snapshot: really they overlap and are spread through the nucleus as waves; the fastest move at about 0.27 c (Fermi momentum about 265 MeV/c at nuclear density 0.164 per fm³)"));
    if (s.layer != "Atom" && !s.densityDrawn && p.Z + p.N > 1) more.push_back(nucleonBallNote());   // 0.46 (review A10)
    if (s.layer != "Atom" && p.Z + p.N >= 12 && nucShape(p.Z, p.N).beta != 0)   // 0.46.5 (data/qcd/nucleus/star_2024_2401.06625.pdf)
        more.push_back("the shape drawn is the nucleus's own (intrinsic) shape, held still with its axis across the view. A real nucleus in its ground state turns: slow measurements see it in all "
                       "directions at once (\"a coherent superposition of wavefunctions in all orientations\"), so its average charge is round; only very fast collisions catch one "
                       "orientation (STAR, Nature 635, 67 (2024)" + (p.Z == 92 && p.N == 146 ? ", whose collisions give uranium-238 beta2 = 0.297 ± 0.015; drawn: " + fmt::fixed(nucShape(p.Z, p.N).beta, 3) + ")" : std::string(")")));
    // 0.42: saturation density 0.164 +- 0.007 fm^-3 (Drischler, Hebeler, Schwenk, PRC 93, 054314 (2016), their empirical range; the Pb-208 Fermi fit of de Vries et al. as quoted by Loizides et al. gives 0.160);
    // k_F = (3 pi^2 n / 2)^(1/3) = 1.344 fm^-1, p_F = hbar c k_F = 265 MeV/c, v = p c / E = 265 / sqrt(265^2 + 939^2) = 0.27 c (free Fermi gas; the lab's own arithmetic). Was "move at about a quarter of light speed". (0.43: the text said 0.16 per fm^3, the arithmetic used 0.164)
    if (s.layer != "Atom") more.push_back("nucleon edge: drawn as the proton's charge density ∝ e^(−r / 0.243 fm) (no sharp surface; rms radius 0.8408 fm, CODATA 2022); a neutron is drawn with the same profile, but its charge is not zero everywhere: ⟨r²⟩ = −0.1155 fm² (PDG 2024; −0.106 from chiral EFT, Filin 2020: not yet settled)");
    // 0.43: the drawn skin and its open question (radii.h neutronSkinFm): Klos et al., PRC 76, 014311 (2007), arXiv:nucl-ex/0702016 (data/qcd/nucleus): the fit of all antiprotonic data allows a slope
    // between 0.9 and 1.3 fm (the point-proton analysis gave 0.90 +- 0.15); PREX-II 0.283 +- 0.071 fm (Adhikari et al. 2021).
    if (s.layer != "Atom" && !s.densityDrawn && p.Z + p.N > 16 && p.N > p.Z)
        more.push_back("neutron skin drawn: the neutrons reach " + fmt::fixed(neutronSkinFm(p.Z, p.N), 2) + " fm farther out than the protons (rms), the antiprotonic-atom fit 0.90 (N − Z)/A − 0.03 fm (Trzcińska et al. 2001); "
                       "a later analysis of the same data allows 0.9 to 1.3 in place of 0.90 (Kłos et al. 2007), and PREX-II measured 0.28 ± 0.07 fm for lead-208, against 0.16 from the fit");
    if (s.layer == "Nucleon" || s.layer == "Quarks") {
        const int i = s.focus >= 0 ? s.focus : (p.Z + p.N == 1 ? 0 : -1);
        const Nucleon* q = i >= 0 && i < int(s.nuc.size()) ? &s.nuc[i] : nullptr;
        lines.push_back(q ? std::string(q->isP ? "proton: up + up + down  ·  charge +⅔ +⅔ −⅓ = +1" : "neutron: up + down + down  ·  charge +⅔ −⅓ −⅓ = 0") +
                                (p.qstyle == "Accurate" ? "  ·  the dots show where its quarks are found, not quarks (no quark size found: below 4.3×10⁻¹⁹ m)"
                                                        : "  ·  quarks are points (no size found: below 4.3×10⁻¹⁹ m), balls not to scale")
                          : std::string("click a nucleon to dive into it"));
        // 0.42: CODATA 2022 masses, radius and magnetic moments (atom_core.h); PDG 2024 neutron <r^2>; Kelly, PRC 66, 065203 (2002): rest-frame densities, "positive interior density is balanced by a negative
        // surface lobe" (model dependent); Miller, PRL 99, 112001 (2007): the model-independent TRANSVERSE density of the neutron is negative at the centre. PDG 2024 quark model review: mu_n / mu_p = -2/3 predicted.
        if (q && q->isP) more.push_back("proton: 938.272 MeV/c², charge radius 0.8408 fm (CODATA 2022; older electron scattering gave about 0.88 fm), magnetic moment +2.793 nuclear magnetons");
        else if (q) more.push_back("neutron: 939.565 MeV/c², magnetic moment −1.913 nuclear magnetons (the simple quark model predicts −⅔ of the proton's: measured −0.685). Its charge: "
                                   "⟨r²⟩ = −0.1155 fm² (PDG 2024; chiral EFT gives −0.106, not yet settled); seen at rest, a positive core and a negative edge (a model-dependent picture), "
                                   "but the frame-independent transverse density has a negative centre: there is no single true picture of where its charge is");
        // 0.44: the Accurate dots (quark_tomo.h; native/data/qcd/tomography/README.txt, sources there). Momentum shares at 2 GeV (ABM11 NLO, atom_tests): u_v 0.297, d_v 0.115,
        // gluons 0.405, u pairs 0.062, d pairs 0.077, s + c 0.045. Valence sqrt<b^2>: u 0.626, d 0.636 fm (Diehl and Kroll table 15: 0.627, 0.638); the 0.43 clouds from BBBA05 + Atac
        // give <b^2> 0.419 and 0.436 against 0.392 and 0.405 fm^2: 7 % more. Gluons: sqrt 0.28 = 0.53 fm at x ~ 0.1, sqrt 0.35 = 0.59 fm at small x (FSW 2005); the lattice A_g tripole
        // (m_A = 1.641 GeV, Pefkou et al. 2022) gives <b^2> = 12 / m_A^2 = 0.174 fm^2, rms 0.42 fm, against 0.30 fm^2 weighted by momentum here. Sea: COMPASS 0.58 fm, H1 0.64 fm.
        if (p.qstyle == "Accurate") {
            more.push_back("the dots: where the nucleon's momentum is, by kind of parton and by momentum fraction x (nucleon tomography), seen along the direction of a very fast nucleon's motion: "
                           "no frame-free 3D picture exists. Shares (ABM11 parton densities at 2 GeV): valence up quarks 30 %, valence down 11 %, gluons 40 %, sea pairs 14 %, strange and charm 4.5 % (not drawn)");
            // 0.45 (review, finding 1): the sizes of the clouds the dots draw are momentum weighted (quark_tomo.h pkDotsMeanB2); 0.44 gave the number-counted 0.63 / 0.64 fm as if they were these
            more.push_back("fast quarks sit near the centre, slow ones spread out: the widths of the valence quarks at each x are the fit of Diehl and Kroll 2013 to all the measured form factors. "
                           "The dot clouds, which follow the momentum, have an rms distance from the centre of " + fmt::fixed(std::sqrt(pkDotsMeanB2(Pk::Uv)), 2) + " fm for the up quarks and " +
                           fmt::fixed(std::sqrt(pkDotsMeanB2(Pk::Dv)), 2) + " fm for the down quark; counting every quark the same, whatever its momentum, 0.63 and 0.64 fm (the independent "
                           "form factor fits of Bradford et al. 2006 and Atac et al. 2021 give about 7 % more in b²). The soft glow under the dots is the same density");
            if (p.detail != "Valence") more.push_back("gluons: their spread is from J/ψ photoproduction (Frankfurt, Strikman and Weiss 2005), measured for x ≈ 0.001 to 0.1: rms distance from the centre about 0.59 fm at small x, 0.53 fm at "
                                                      "x ≈ 0.1. Faster gluons (a third of their momentum) are extrapolated: drawn closer to the centre, as (1 − x)², a choice. Lattice QCD's gluon form factor A_g gives "
                                                      "a narrower 0.42 fm (Pefkou, Hackett and Shanahan 2022): not yet settled");
            if (p.detail == "+ Sea") more.push_back("the sea's spread is measured by deeply virtual Compton scattering only at small x: 0.64 fm at x ≈ 0.001 (H1 2009), 0.58 fm at x ≈ 0.03 (COMPASS 2019). "
                                                    "Most of the sea's momentum (about 80 %) is at larger x, where the spread is extrapolated: drawn closer to the centre as (1 − x)², a choice. "
                                                    "Sea quarks and their antiquarks are given the same spread (no difference has been measured)");
        } else
            more.push_back("the three balls and their tubes are a model for learning: a nucleon is one quantum state, its quarks have no definite places, and they are relativistic "
                           "(their energy inside, about a third of the proton's mass, is far above their own 2 to 5 MeV)");
        // Quark energy 32(4)(4) % of the proton mass at the physical point: Yang et al. 2018, text (its abstract rounds 33 %); masses: PDG 2024.
        if (p.detail == "+ Sea") more.push_back("sea: more anti-down than anti-up quarks, d̄/ū = 1.3 to 1.6 (SeaQuest 2023, 0.13 < x < 0.45; E866: ∫(d̄ − ū) dx = 0.118 ± 0.012); strange pairs are left out: "
                                                "how many there are is still debated (ATLAS 2017: about as many as light antiquarks; HERMES 2014: fewer)");
        if (p.detail != "Valence") more.push_back("spin: the quarks' spins give only about a third of the proton's spin (ΔΣ = 0.26 to 0.36, COMPASS 2016; lattice 0.38, Alexandrou 2020); gluon spin about 0.2 for x > 0.05 (DSSV 2014); the gluon spin at x < 0.05 has not been measured, and the rest is the quarks' and gluons' orbital motion");
    }
    if (moreShown()) lines.insert(lines.end(), more.begin(), more.end());
    return lines;
}

// 0.52.1: the level atoms' main line said only "the outer electrons' dots drawn denser": now what is drawn and why, with the factor. The outer electrons (levelValenceSet)
// get kValenceDotShare of the dots plus their true share of the rest (manyDot): (k + (1 - k) x) / x times their true share x = n_outer / Z.
double AtomLab::denserFactor(double nOuter, int electrons) {
    const double x = nOuter / electrons;
    return x > 0 ? (kValenceDotShare + (1 - kValenceDotShare) * x) / x : 1;
}
std::string AtomLab::trueShareText(const hf::Atom& a) const {
    double n = 0;
    if (valenceOn()) n = 1;
    else for (const int i : levelValenceSet(*this, a)) n += a.sub[std::size_t(i)].focc;
    return numText(n, 3) + " in " + std::to_string(a.electrons());
}
bool AtomLab::sliceAloneChoice() const { return manyOn() && manySub() < 0 && (valenceOn() || levelOn()) && sliceDots(); }
long AtomLab::sliceOuterSample(int n, unsigned seed, std::vector<double>& radii) const {
    radii.clear();
    const hf::Atom* a = manyOn() ? manyAtom() : nullptr;
    if (!a || !sliceAloneChoice() || n <= 0) return -1;
    const int vi = valenceSub(*this, *a);
    const GraspLevel* gl = graspOfAtom(a);
    const std::vector<int> vset = levelValenceSet(*this, *a);
    const hf::Plane pl = hf::planeOf(*a, sliceOffNow());
    orbit::Mulberry32 r(seed);
    long notOuter = 0;
    for (int i = 0; i < n; i++) {
        double x, y, z; int shell = -1; bool ok = false;
        for (int k = 0; k < 4096 && !ok; k++) ok = manyDot(*this, *a, -1, vi, gl, &pl, sliceAxisNow(), r, x, y, z, shell, &vset, sliceAlone());
        if (!ok) continue;
        radii.push_back(std::sqrt(x * x + y * y + z * z));
        if (!(shell >= 0 && (shell == vi || std::find(vset.begin(), vset.end(), shell) != vset.end()))) notOuter++;
    }
    return notOuter;
}
bool AtomLab::outerDotsChoice() const { return manyOn() && manySub() < 0 && (valenceOn() || levelOn()) && (p.view == "Cloud" || p.view == "Cloud 3D"); }
double AtomLab::outerDotFraction(int n, unsigned seed) const {
    const hf::Atom* a = manyOn() ? manyAtom() : nullptr;        // (as addDots sets up a batch)
    if (!a || n <= 0) return NAN;
    const int one = manySub(), vi = valenceSub(*this, *a);
    const GraspLevel* gl = valenceOn() || levelOn() ? graspOfAtom(a) : nullptr;
    const std::vector<int> vset = levelValenceSet(*this, *a);
    orbit::Mulberry32 r(seed);
    int outer = 0, drawn = 0;
    for (int i = 0; i < n; i++) {
        double x, y, z; int shell = -1; bool ok = false;
        for (int k = 0; k < 4096 && !ok; k++) ok = manyDot(*this, *a, one, vi, gl, nullptr, 2, r, x, y, z, shell, &vset);
        if (!ok) continue;
        drawn++;
        if (shell >= 0 && (shell == vi || std::find(vset.begin(), vset.end(), shell) != vset.end())) outer++;
    }
    return drawn > 0 ? double(outer) / drawn : NAN;
}
// 0.91.0: the Slice's words for its "Outer electron's dots" choice (empty where the choice does not apply)
std::string AtomLab::sliceShareText(const hf::Atom& a) const {
    if (!sliceAloneChoice()) return std::string();
    if (sliceAlone()) return "  ·  Slice: only the outer electron's dots, all of them: its cloud alone, not the atom's (the core's electrons are left out)";
    return "  ·  Slice: every electron its true share of the dots (the outer electron " + trueShareText(a) + "), so its shape is faint; \"Outer electron alone\" (Controls) shows its cloud alone";
}
std::string AtomLab::outerDenserText(const hf::Atom& a) const {
    if (p.vdots == "True share") return "  ·  the outer electrons' dots at their true share, " + trueShareText(a) + ", so their cloud and its jumps are faint against the core";   // 0.54.1
    double n = 0;
    for (const int i : levelValenceSet(*this, a)) n += a.sub[std::size_t(i)].focc;
    return "  ·  the outer electrons' dots are drawn " + numText(denserFactor(n, a.electrons()), 2) + "× denser than their true share, so their cloud and its jumps stand out against the core";
}
std::string AtomLab::hint() const {
    if (chartOn())
        return "The chart of nuclides: every known nuclide is one square, neutrons across and protons up. Hover a square for its details, click it to choose that nuclide (the other views then show it), "
               "or use the arrow keys to move one proton or one neutron. The colour is how the nuclide decays; the lines are the magic numbers. The 3D switch turns the flat chart into a landscape whose height is either the half-life or the binding energy per nucleon (chosen scales: see the legend); in 3D a drag turns it, the wheel zooms, Shift-drag pans.";
    const double x = p.Z * kAlpha;
    std::string note;
    if (p.Z >= kRelativityZ) {   // 0.46.1 (outside review): the hint said "this lab uses non-relativistic physics" even where the default model (Dirac-Hartree-Fock) and the energy readout (Dirac) are relativistic
        const std::string v = fmt::fixed(100 * x, 0) + "% of light speed";
        if (!manyOn()) note += " Heavy atoms: the " + std::string(p.view == "Orbit" ? "orbit is non-relativistic" : "cloud is the non-relativistic one-electron orbital") + ", but a 1s electron here moves at " + v + ", so the true energy differs. " +
                               (p.view == "Orbit" ? "The Cloud and Slice views give the Dirac-equation energy for a point nucleus." : "The readout gives the Dirac-equation energy for a point nucleus.");
        else if (p.mrel != "Non-relativistic") note += " Heavy atoms: a 1s electron here moves at " + v + ", so relativity changes the shells (s and p smaller, d and f larger). The Dirac-Hartree-Fock model includes this; Model: Non-relativistic shows the difference.";
        else note += " Heavy atoms: this model is non-relativistic, but a 1s electron here moves at " + v + ", so the real shells differ in size (s and p smaller, d and f larger). Model: Relativistic shows them.";
    }
    if (p.zoom >= 4 && p.qstyle == "Accurate") {
        note += " A quark has no definite place inside a nucleon, so the Accurate style draws a probability cloud, as for the electrons: each dot is a piece of the nucleon's momentum, "
                "carried there by an up (warm) or a down (cool) quark, not a quark. Fast quarks sit near the centre and slow ones spread out (nucleon tomography: Diehl and Kroll's fit to the "
                "measured form factors, with ABM11 parton densities), seen along the direction of a very fast nucleon's motion, the one view in which such a density needs no model. "
                "No colour charge is drawn: a nucleon is a colour singlet, so no quark has one definite colour, and the dots are coloured by flavour. No size has been found "
                "for a quark (ZEUS: below 4.3×10⁻¹⁹ m).";
        if (p.detail == "Valence") note += " Valence shows only the valence quarks: the gluons and the sea are hidden, not absent (they carry more than half of the momentum).";
        else note += " Grey dots are the gluons, about 40 % of the momentum at the 2 GeV scale of the ABM11 densities used (the share grows slowly with the probing energy, towards about half); their spread is from J/ψ data.";
        if (p.detail == "+ Sea") note += " Rings are the sea's antiquarks, dots of the same colour their sea quarks; how far out they sit is measured only for slow pairs (COMPASS, H1) and extrapolated for the rest. "
                                         "Strange and charm pairs are left out. The real sea has more anti-down than anti-up quarks (More info).";
    } else if (p.zoom >= 4)
        // 0.45 (review, finding 2): Bissey et al. 2007 give the quarks' distance from the centre (a radius), not their separation; the Simple quarks are drawn 0.18 to 0.42 fm from the centre
        note += " The Simple style is a model for learning: three balls joined by a Y of flux tubes, 0.38 fm in radius (lattice QCD). Lattice QCD finds the Y only when three fixed heavy quarks are "
                "more than about 0.5 fm from the centre; at about 0.4 fm it finds a filled triangle and at 0.3 fm or less a round bubble. The balls here stay about 0.2 to 0.4 fm from the centre, "
                "where the real field would be a bubble or a triangle: the Y is a cartoon. The light quarks of a real nucleon have no fixed places. No size has been found for a quark (ZEUS: below 4.3×10⁻¹⁹ m): the balls "
                "are drawn about 150 times too big.";
    // quark size: ZEUS, PLB 757, 468 (2016), arXiv:1604.01280: effective quark radius < 0.43e-16 cm = 4.3e-19 m (95 % C.L., quark form-factor model). The Simple balls are 0.075 x 0.84075 fm = 6.3e-17 m in
    // radius: 147 times the ZEUS limit. Colour singlet: PDG 2024 quark model review. Field shapes: Bissey et al., PRD 76, 114512 (2007), arXiv:hep-lat/0606016 (bubble up to ~0.3 fm, filled triangle ~0.4 fm,
    // Y beyond 0.5 fm; quenched lattice, static quarks). Sea: anti-d > anti-u (E866, hep-ex/0103030; SeaQuest, arXiv:2103.04024). See docs/QUARKS.md.
    if (p.scale == "Fixed") note += manyOn() ? " Fixed scale: every atom is drawn at the same scale, so the outer sizes of neutral atoms compare truly."   // 0.39.1: the one-electron text spoke of one electron pulled in
                                             : " Fixed scale: every atom and level is drawn at the same scale, so heavier nuclei pull the electron in close. Zoom in to follow it.";
    if (p.Z >= 104) note += " Superheavy elements are made a few atoms at a time; their half-lives are still being measured.";
    if (p.Z + p.N > 45 && p.zoom >= 4) note += " Quarks are drawn for the 45 nearest nucleons only (big nuclei have hundreds).";
    const double size = manyOn() ? (p.view == "Orbit" ? (manyRings().empty() ? 1.0 : manyRings().back().r) : manyExtM() / 1.7 / kA0) : p.view == "Orbit" ? double(p.n) * p.n / p.Z : std::pow(orbitals()[orbIndex(p.orb)].n, 2) / p.Z;   // orbit/orbital size in a0 (0.39.1: the neutral atom's 90 % radius with many electrons)
    const std::string ratio = fmt::localeNumber(std::strtod(fmt::toPrecision(size * lenScale() / nucRA0(), 2).c_str(), nullptr));
    const std::string find = "The nucleus is ~" + ratio + "× smaller than this " + (manyOn() ? "atom" : p.view == "Orbit" ? "orbit" : "orbital") +
                             ": scroll, pinch or use the zoom slider to find it. Zoom into the nucleus and click a nucleon to dive inside it.";
    const std::string drag = p.view == "Orbit" ? (manyOn() ? " Drag to turn the atom and see the rings from another angle." : " Drag to turn the atom and see the orbit from another angle.") + std::string(" Shift-drag or right-drag (or the ✥ button) moves the picture.")
                           : p.view == "Cloud" ? " Drag to move around."
                           : p.view == "Cloud 3D" ? " Shift-drag or right-drag (or the ✥ button) moves the picture instead of rotating it."
                           : p.view == "Slice" ? " Drag up or down to slide the cut through the cloud. Shift-drag or right-drag (or the ✥ button) moves the picture." : "";
    if (manyOn() && p.view == "Orbit")                          // 0.39.1: the shells as rings
        return "Every electron of the neutral atom, shell by shell: each ring is a shell (K, L, M …) at the average distance of its electrons (" + manyModel() + " orbitals), with its electrons spaced on it. "
               "This is a picture of the shells, not of real paths: an electron is a cloud (see Cloud and Cloud 3D). " + find + drag + note;
    if (manyOn() && p.view == "Slice")
        return "A flat cut through the atom, seen face-on: the dots are places on the cut plane itself where " + std::string(manySub() < 0 ? "one of the atom's electrons" : "an electron of its " + p.orb + " orbital") +
               " may be found, spread like the electron density on that plane (" + manyModel() + " orbitals); " + std::string(manySub() < 0 ? "the colours are the shells. " : usesWheel() ? "the colours are the phase of the wave. " : "the colours are the signs of the wave. ") +
               "Move the cut position to sweep the plane through the atom. " + find + drag + note;
    if (p.view == "Slice")
        return "A flat cut through the cloud, seen face-on: the dots are electron positions on the cut plane itself, spread like the probability on that plane (they are made there, not picked out of the 3D cloud). " + std::string(usesWheel() ? "The colours are the phase of the wave (ψ), shown by the colour ring; where the dots thin out to nothing is a node, a place the electron is never found." : "Blue and orange are opposite signs of the wave (ψ); where they meet is a node, a place the electron is never found.") + " Move the cut position to sweep the plane through the cloud; new dots are made on the new plane (during a drag the newest replace the oldest, so the picture briefly mixes the last few positions). If no dots appear, the whole plane is a node. " + find + drag + note;
    if (manyOn()) return std::string(p.view == "Cloud 3D" ? "Drag to rotate; brighter dots are nearer. " : "") +   // 0.39.1 (and for One orbital: it described the whole atom there too)
                         (manySub() < 0 ? std::string("Each dot is one place where an electron of the neutral atom may be found, sampled from the whole atom's electron density (" + manyModel() + " orbitals); the colours are the shells. ")
                                        : "Each dot is one place where an electron of the atom's " + std::string(p.orb) + " orbital may be found (its " + manyModel() + " radius, the angular shape of the orbital); " +
                                          (usesWheel() ? "the colours are the phase of the wave (see the colour ring). " : p.orb == "1s" ? "one colour: the wave has the same sign everywhere (1s has no node). "
                                                                                                : "the two colours are the + and − signs of the wave. ")) + find + drag + note;
    if (p.view == "Cloud 3D") return "Drag to rotate; brighter dots are nearer. " + find + drag + note;
    if (p.view == "Cloud") return "Each dot is one possible position of the electron, sampled from |ψ|² (side view). There is no path, only probabilities. " + find + drag + note;
    std::string slow = fmt::toExponential(double(p.Z) * p.Z / (p.speed * kAuTimeAs * 1e-18 * lenScale()), 0);
    const std::size_t e = slow.find("e+");
    if (e != std::string::npos) slow.replace(e, 2, "×10^");
    return "Orbit and nucleus are both to scale. " + find + " Time is slowed ~" + slow + "× so you can watch it" +
           (p.Z > 1 ? " (the real electron here is " + fmt::localeNumber(double(p.Z) * p.Z) + "× faster than in hydrogen)" : std::string()) + "." + drag + note;
}

double AtomLab::ringElectronAngle(const Ring& g, int k) const {
    // 0.50 (review item 6): still. The electrons sit evenly spaced on their shell's ring; each shell starts at its own angle (0.9 n rad) so the dots of
    // neighbouring shells do not line up. (Until 0.49.4 the rings turned, K once in 8 s: the view's one non-physical motion.)
    return 0.9 * g.n + 2 * kPi * k / std::max(g.electrons, 1);
}

}  // namespace atom
