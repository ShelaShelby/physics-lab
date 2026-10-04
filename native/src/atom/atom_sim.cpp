#include "atom_sim.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <complex>
#include <cstdint>
#include <map>

#include "orbit/belts.h"
#include "orbit/format.h"
#include "phase_wheel.h"
#include "radial_graph.h"

namespace atom {

static double jsRound(double x) { const double r = std::floor(x + 0.5); return r == 0 && x < 0 ? -0.0 : r; }
static double clampZoom(double z) { return std::max(0.0, std::min(7.0, z)); }
static const char* const kLetter = "spdf";
static constexpr double kMaxStepAngle = 2 * kPi / 300;   // the largest angle one leapfrog step of the Orbit view may turn, rad (see AtomLab::step)
static const char* const kGreek[4] = {"α", "β", "γ", "δ"};

AtomLab::AtomLab() {
    nowMs = [] {
        using namespace std::chrono;
        return duration<double, std::milli>(steady_clock::now().time_since_epoch()).count();
    };
    restart();
}

bool AtomLab::validChoice(const std::string& key, const std::string& v) {
    static const std::map<std::string, std::vector<std::string>> opts = {
        {"scale", {"Auto", "Fixed"}}, {"qstyle", {"Cinematic", "Accurate", "Classic"}}, {"detail", {"Valence", "+ Gluons", "+ Sea"}},
        {"style", {"Fuzzy", "Hard balls"}}, {"view", {"Orbit", "Cloud", "Cloud 3D", "Slice"}}, {"ostyle", {"Modern", "Bohr 1913"}},
        {"orb", {"1s", "2s", "2p", "3s", "3p", "3d", "4s", "4p", "4d", "4f"}}, {"pAx", {"x", "y", "z", "m+1", "m-1"}}, {"dAx", {"z2", "xz", "yz", "xy", "x2y2", "m+1", "m-1", "m+2", "m-2"}},
        {"fAx", {"z3", "xz2", "yz2", "xyz", "zx2y2", "xx23y2", "y3x2y2", "m+1", "m-1", "m+2", "m-2", "m+3", "m-3"}},
        {"pAx2", {"x", "y", "z", "m+1", "m-1"}}, {"dAx2", {"z2", "xz", "yz", "xy", "x2y2", "m+1", "m-1", "m+2", "m-2"}},
        {"fAx2", {"z3", "xz2", "yz2", "xyz", "zx2y2", "xx23y2", "y3x2y2", "m+1", "m-1", "m+2", "m-2", "m+3", "m-3"}},
        {"orb2", {"1s", "2s", "2p", "3s", "3p", "3d", "4s", "4p", "4d", "4f"}}, {"super", {"Off", "On"}}, {"cutAxis", {"Vertical", "Horizontal"}},
        {"cstyle", {"Classic", "Glow"}}, {"outline", {"Off", "On"}}, {"phase", {"Sign", "Wheel"}}, {"cdens", {"Normal", "High", "Ultra"}}, {"dirac", {"Off", "On"}},
        {"jfrom", {"1s", "2s", "2p", "3s", "3p", "3d", "4s", "4p", "4d", "4f"}}, {"jto", {"1s", "2s", "2p", "3s", "3p", "3d", "4s", "4p", "4d", "4f"}}};
    const auto it = opts.find(key);
    return it != opts.end() && std::find(it->second.begin(), it->second.end(), v) != it->second.end();
}

int AtomLab::capFor(const std::string& d) { return d == "Ultra" ? 4 * kCap : d == "High" ? 2 * kCap : kCap; }

void AtomLab::restart() {
    s = AtomState();
    init();
}

// ---------------------------------------------------------------- init (the web's init(p))
void AtomLab::init() {
    // circular orbit for level n: r = n²/Z Bohr radii, v = Z/n atomic units of speed
    const double a = random() * kPi * 2, r = double(p.n) * p.n / p.Z * kA0, v = double(p.Z) / p.n * kAuSpeed;
    const Orbital& o = orbitals()[orbIndex(p.orb)];
    const double k = double(o.n) / p.Z;
    s.t = 0; s.Zprev = p.Z; s.more = moreStored; s.pan = {}; s.panTo.reset(); s.dragMode = "rotate";
    s.x = r * std::cos(a); s.y = r * std::sin(a); s.vx = -v * std::sin(a); s.vy = v * std::cos(a); s.trail.clear();
    s.pts.assign(std::size_t(cloudCap()) * 3, 0.f); s.sg.assign(cloudCap(), 0); s.ph.assign(cloudCap(), 0.f); s.k = 0;
    s.sa.clear(); s.beatT = 0;
    if (superOn()) { s.sa.assign(std::size_t(cloudCap()) * 4, 0.f); sup_ = superState(); nA_ = normFactor(sup_.idA, p.Z); nB_ = normFactor(sup_.idB, p.Z); overlap_ = overlapAB(sup_.idA, sup_.idB) != 0.0; } else overlap_ = false; s.w = {0.7 * k, 0.4 * k, 1.1 * k};
    s.yaw = 0.6; s.pitch = 0.35; s.oyaw = 0; s.opitch = kPi / 2; s.spin = !reducedMotion;   // Cloud 3D turns by itself, unless motion is reduced
    s.fc = {}; s.focus = -1; s.zoomTo.reset(); s.layer = "Atom"; s.hits.clear();
    s.trans.reset(); s.photon.reset(); s.photonAfter.reset(); s.jp.clear(); s.jA.clear(); s.jB.clear(); s.jn = 0; s.jb = AtomState::JumpBuild(); s.landT = 99; s.lines.clear(); s.autoPhotons = false; s.wait = 0.3; s.boost = 0;
    s.last.clear(); s.msg.clear(); s.szCur.reset();
    for (int i = 0; i < 800; i++) mcmc(false);   // burn-in: let the walker forget where it started
}

// ---------------------------------------------------------------- settings
void AtomLab::setParam(const std::string& key, double v) {
    double lo = 0, hi = 0;
    if (key == "zoom") { lo = 0; hi = 7; }
    else if (key == "Z") { lo = 1; hi = 118; }
    else if (key == "N") { const auto r = nRange(p.Z); lo = r.first; hi = r.second; }
    else if (key == "cut") { lo = -1; hi = 1; }
    else if (key == "n") { lo = 1; hi = 4; }
    else if (key == "speed") { lo = 1; hi = 30; }
    else if (key == "mix") { lo = 0; hi = 100; }
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
    if (key == "scale") p.scale = v;
    else if (key == "qstyle") p.qstyle = v;
    else if (key == "detail") p.detail = v;
    else if (key == "style") p.style = v;
    else if (key == "view") p.view = v;
    else if (key == "ostyle") p.ostyle = v;
    else if (key == "orb") p.orb = v;
    else if (key == "pAx") p.pAx = v;
    else if (key == "dAx") p.dAx = v;
    else if (key == "fAx") p.fAx = v;
    else if (key == "cutAxis") p.cutAxis = v;
    else if (key == "cstyle") p.cstyle = v;
    else if (key == "cdens") p.cdens = v;
    else if (key == "outline") p.outline = v;
    else if (key == "dirac") p.dirac = v;
    else if (key == "jfrom") p.jfrom = v;
    else if (key == "jto") p.jto = v;
    else if (key == "phase") p.phase = v;
    else if (key == "super") p.super = v;
    else if (key == "orb2") p.orb2 = v;
    else if (key == "pAx2") p.pAx2 = v;
    else if (key == "dAx2") p.dAx2 = v;
    else if (key == "fAx2") p.fAx2 = v;
    else return;
    onParam(key);
}

void AtomLab::onParam(const std::string& key) {
    if (s.jumping) return;                                    // a photon jump changed the level itself: keep the state
    if (key == "Z") setParam("N", defaultN(p.Z));            // a new element: neutrons follow to its usual isotope
    if (key == "zoom") { if (!s.selfSet) s.zoomTo.reset(); return; }   // a manual zoom cancels an animated dive
    if (key == "N" || key == "Z") { if (s.focus >= p.Z + p.N) s.focus = -1; }   // keep the followed nucleon if it is still there
    if (key == "Z" && p.scale != "Fixed" && s.Zprev && s.Zprev != p.Z) {
        // auto-fit rescales with Z: counter it so what you are looking at stays the same size on screen
        const double d = std::log10(double(s.Zprev) / p.Z);
        if (p.zoom > 0) { s.selfSet = true; setParam("zoom", std::max(0.0, p.zoom + d)); s.selfSet = false; }
        if (s.zoomTo) s.zoomTo = clampZoom(*s.zoomTo + d);
    }
    if (key == "mix") {                                       // orthogonal pair: the shares change the time dependence only, never the dots. An overlapping pair has no time dependence (one energy): the
        sup_.a = std::sqrt(1 - p.mix / 100); sup_.b = std::sqrt(p.mix / 100);   // mix changes the density itself, so the cloud is refilled below, like a new orbital
        if (!(superOn() && overlap_)) return;
    }
    if ((key == "orb2" || key == "pAx2" || key == "dAx2" || key == "fAx2") && p.super != "On") return;    // (nothing shown depends on them while the superposition is off)
    static const char* const reinit[] = {"Z", "n", "view", "orb", "pAx", "dAx", "fAx", "cdens", "super", "orb2", "pAx2", "dAx2", "fAx2"};   // a new density refills the cloud
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
std::string AtomLab::stateText() const { return atom::stateText(stateId()); }

// the size the picture is fitted to: Fixed uses one reference size for every atom and level, that of hydrogen n = 4: the circle n² = 16 a0 in the Orbit view, and the view radius of the
// n = 4 clouds, kExt[4] = 40 a0, in the cloud views. (It was 24 a0 = kExt[3], the n = 3 size, which cut off 4s, 4p, 4d and 4f: their 90 % outline reaches 30-36 a0; 0.10.6 stage 3, audit finding 8.)
static double fitSize(const AtomParams& p, double szM) { return p.scale == "Fixed" ? (p.view == "Orbit" ? 16 : kExt[4]) * kA0 : szM; }
double AtomLab::sizeM() const {
    return fitSize(p, p.view == "Orbit" ? double(p.n) * p.n / p.Z * kA0 : kExt[sizeN()] / p.Z * kA0);
}
double AtomLab::pxPerM(double m, std::optional<double> sz) const { return (p.view == "Orbit" ? 0.42 : 0.45) * m / (sz ? *sz : sizeM()); }
double AtomLab::nucRA0() const { return nuclearR(p.Z + p.N) / kFmPerA0; }
double AtomLab::zoomForNucleus() const {
    const double m = std::min(viewW, viewH);
    return clampZoom(std::log10(0.28 * m / (nucRA0() * kA0 * pxPerM(m))));
}
double AtomLab::zoomForNucleon() const {
    const double m = std::min(viewW, viewH);
    return clampZoom(std::log10(0.42 * m * kFmPerA0 / kNucleonR / (pxPerM(m) * kA0)));
}
double AtomLab::cutOffsetM() const { return p.cut * 0.5 * kExt[sizeN()] / p.Z * kA0; }

// ---------------------------------------------------------------- the cloud: Metropolis sampling of |ψ|²
// One step: try a random move (a Gaussian step in each direction) and accept it with the odds
// |ψ_new|² / |ψ_old|². The dots that pile up are spread like the electron's probability.
void AtomLab::walk(int orb, Vec3d& w, bool record) {
    const int n = orbitals()[stateOrbital(orb)].n;
    const double sg = 0.4 * n * (n >= 4 ? 1.8 : 1) / p.Z;     // bigger steps for the big n = 4 clouds, so the dots spread through them quickly
    auto gauss = [] { const double u = random(); return std::sqrt(-2 * std::log(1 - u)) * std::cos(2 * kPi * random()); };
    const double nx = w.x + sg * gauss(), ny = w.y + sg * gauss(), nz = w.z + sg * gauss();
    if (isComplexState(orb)) {                                 // a complex state: the odds are |psi|^2 (same steps and random numbers as for a real orbital)
        const double pOld = density(orb, p.Z, w.x, w.y, w.z), pNew = density(orb, p.Z, nx, ny, nz);
        if (random() * pOld < pNew || pOld == 0) { w.x = nx; w.y = ny; w.z = nz; }
        if (record) {
            const long i = s.k % std::min<long>(cloudCap(), long(s.sg.size()));
            const std::complex<double> v = psiComplex(orb, p.Z, w.x, w.y, w.z);
            s.sg[i] = v.real() >= 0 ? 1 : -1;                  // (the sign of the real part: only used where a sign is wanted)
            s.ph[i] = float(std::arg(v));                      // the phase of the wave here: the wheel colour
            s.pts[3 * i] = float(w.x * kA0); s.pts[3 * i + 1] = float(w.y * kA0); s.pts[3 * i + 2] = float(w.z * kA0);
            s.k++;
        }
        return;
    }
    const double fo = psi(orb, p.Z, w.x, w.y, w.z), pOld = fo * fo;
    const double fn = psi(orb, p.Z, nx, ny, nz);
    if (random() * pOld < fn * fn || pOld == 0) { w.x = nx; w.y = ny; w.z = nz; }
    if (record) {
        const long i = s.k % std::min<long>(cloudCap(), long(s.sg.size()));   // (the arrays are sized by restart(); never write outside them)
        const double pv = psi(orb, p.Z, w.x, w.y, w.z);
        s.sg[i] = pv >= 0 ? 1 : -1;                            // the sign of the wave here: the phase colour
        s.ph[i] = pv >= 0 ? 0.f : float(kPi);                  // the same as a phase: 0 or pi
        s.pts[3 * i] = float(w.x * kA0); s.pts[3 * i + 1] = float(w.y * kA0); s.pts[3 * i + 2] = float(w.z * kA0);
        s.k++;
    }
}

// ---------------------------------------------------------------- quantum beats: a superposition of two orbitals
// The dots are samples of the time-INDEPENDENT mixture q = (|A|^2 + |B|^2) / 2 (A and B normalised). The density at beat phase theta is |a A + b B e^(-i theta)|^2, and
// |a A + b B e^(-i theta)|^2 <= (a|A| + b|B|)^2 <= (a^2 + b^2)(|A|^2 + |B|^2) = 2 q (Cauchy-Schwarz), so a dot at x is shown with probability w / 2, w = |psi(x, theta)|^2 / q:
// the shown dots are then exact samples of the CURRENT density, whatever the time, and no dot carries any memory of the past. (A fixed random number per dot decides, so
// a dot appears and disappears smoothly as the density at its place rises and falls.)
void AtomLab::walkSuper(Vec3d& w, bool record) {
    const int nmax = std::max(orbitals()[stateOrbital(sup_.idA)].n, orbitals()[stateOrbital(sup_.idB)].n);
    const double sg = 0.4 * nmax * (nmax >= 4 ? 1.8 : 1) / p.Z;
    auto gauss = [] { const double u = random(); return std::sqrt(-2 * std::log(1 - u)) * std::cos(2 * kPi * random()); };
    const auto amps = [&](double x, double y, double z, std::complex<double>& A, std::complex<double>& B) {
        A = nA_ * (isComplexState(sup_.idA) ? psiComplex(sup_.idA, p.Z, x, y, z) : std::complex<double>(psi(sup_.idA, p.Z, x, y, z), 0));
        B = nB_ * (isComplexState(sup_.idB) ? psiComplex(sup_.idB, p.Z, x, y, z) : std::complex<double>(psi(sup_.idB, p.Z, x, y, z), 0));
    };
    const double nx = w.x + sg * gauss(), ny = w.y + sg * gauss(), nz = w.z + sg * gauss();
    std::complex<double> A0, B0, A1, B1;
    amps(w.x, w.y, w.z, A0, B0); amps(nx, ny, nz, A1, B1);
    // orthogonal pair: the time-independent mixture (|A|^2 + |B|^2) / 2, thinned below. Overlapping pair (one energy, static): the renormalised density |a A + b B|^2 itself (the constant norm does not matter
    // to the Metropolis odds), and every dot is shown.
    const double pOld = overlap_ ? std::norm(sup_.a * A0 + sup_.b * B0) : std::norm(A0) + std::norm(B0), pNew = overlap_ ? std::norm(sup_.a * A1 + sup_.b * B1) : std::norm(A1) + std::norm(B1);
    if (random() * pOld < pNew || pOld == 0) { w.x = nx; w.y = ny; w.z = nz; }
    if (record) {
        const long i = s.k % std::min<long>(cloudCap(), long(s.sg.size()));
        std::complex<double> A, B;
        amps(w.x, w.y, w.z, A, B);
        if (std::size_t(4 * i + 3) < s.sa.size()) { s.sa[4 * i] = float(A.real()); s.sa[4 * i + 1] = float(A.imag()); s.sa[4 * i + 2] = float(B.real()); s.sa[4 * i + 3] = float(B.imag()); }
        s.sg[i] = 1; s.ph[i] = 0.f;
        s.pts[3 * i] = float(w.x * kA0); s.pts[3 * i + 1] = float(w.y * kA0); s.pts[3 * i + 2] = float(w.z * kA0);
        s.k++;
    }
}

// ---- the shown dots of a cloud jump ----
double AtomLab::jumpU() const {
    if (!s.trans) return 0;
    const double u = s.trans->dur > 0 ? std::max(0.0, std::min(1.0, s.trans->t / s.trans->dur)) : 1;
    return u * u * (3 - 2 * u);
}
double AtomLab::jumpPhi() const {                                // the beat phase, slowed as in a superposition (4 s per beat at Playback speed 5)
    if (!s.trans || p.view == "Orbit") return 0;
    const double dE = levelEV(p.Z, p.N, orbitals()[s.trans->to].n) - levelEV(p.Z, p.N, orbitals()[s.trans->from].n);
    return (dE > 0 ? 1 : -1) * 2 * kPi * (s.trans->t * p.speed / 5.0) / kBeatScreenS;
}
static double jumpUniform(int pop, long i) {                     // a fixed uniform number for each dot (splitmix64 of its index and population)
    std::uint64_t z = (std::uint64_t(i) + (std::uint64_t(pop) << 40)) * 0x9E3779B97F4A7C15ULL + 0x632BE59BD9B4E019ULL;
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL; z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL; z ^= z >> 31;
    return double(z >> 11) * (1.0 / 9007199254740992.0);
}
bool AtomLab::jumpShown(int pop, long i) const {
    if (!s.trans || i < 0 || i >= s.jn) return false;
    if (!jumpReady()) const_cast<AtomLab*>(this)->completeJump();     // (anything that asks about a dot gets the finished jump)
    const std::vector<float>& a = pop ? s.jB : s.jA;
    const std::complex<double> A(a[4 * i], a[4 * i + 1]), B(a[4 * i + 2], a[4 * i + 3]);
    double fA, fB;
    jumpFractions(A, B, kPi / 2 * jumpU(), jumpPhi(), fA, fB);
    return jumpUniform(pop, i) < (pop ? fB : fA);
}
double AtomLab::jumpPhase(int pop, long i) const {
    if (!jumpReady()) const_cast<AtomLab*>(this)->completeJump();
    const std::vector<float>& a = pop ? s.jB : s.jA;
    const std::complex<double> A(a[4 * i], a[4 * i + 1]), B(a[4 * i + 2], a[4 * i + 3]);
    const double th = kPi / 2 * jumpU(), phi = jumpPhi();
    return std::arg(std::exp(std::complex<double>(0, phi * jumpU())) * (std::cos(th) * A + std::sin(th) * B * std::exp(std::complex<double>(0, -phi))));   // gauge: ends on B itself
}
int AtomLab::jumpGroup(int pop, long i) const {
    if (!jumpShown(pop, i)) return 255;
    const double ph = jumpPhase(pop, i);
    return p.phase == "Wheel" ? phaseBin(ph) : (std::cos(ph) >= 0 ? 0 : 1);
}
// Every dot of the jump, in one go (the picture asks for this each frame): the same numbers as jumpGroup(pop, i) one by one, bit for bit (tested), but cos, sin and the complex exponentials
// of the jump's progress, and the jump's energy step (levelEV, which looks up the nucleus's mass), are worked out once here and not once per dot (0.10.6 stage 8; the benchmark's scene 11 spent about 26 of 30 ms here).
void AtomLab::jumpGroups(unsigned char* grp) const {
    const long jn = s.jn;
    if (!s.trans || jn <= 0) { for (long i = 0; i < 2 * jn; i++) grp[i] = 255; return; }
    if (!jumpReady()) const_cast<AtomLab*>(this)->completeJump();
    const double u = jumpU(), phi = jumpPhi(), th = kPi / 2 * u;
    const JumpTerms T = jumpTerms(th, phi, u);
    const bool wheel = p.phase == "Wheel";
    for (int pop = 0; pop < 2; pop++) {
        const float* a = (pop ? s.jB : s.jA).data();
        unsigned char* g = grp + (pop ? jn : 0);
        for (long i = 0; i < jn; i++) {
            const std::complex<double> A(a[4 * i], a[4 * i + 1]), B(a[4 * i + 2], a[4 * i + 3]);
            double fA, fB;
            jumpFractions(A, B, T, fA, fB);
            if (!(jumpUniform(pop, i) < (pop ? fB : fA))) { g[i] = 255; continue; }
            const double ph = jumpPhaseOf(A, B, T);
            g[i] = (unsigned char)(wheel ? phaseBin(ph) : (std::cos(ph) >= 0 ? 0 : 1));
        }
    }
}
double AtomLab::outlineAlpha() const {                          // the outline belongs to one orbital: it fades out as the cloud leaves it, and back in after landing
    if (reducedMotion || webJumps) return 1;
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
    const int cur = curLevel(), n0 = lvlN(cur);
    std::vector<int> out;
    if (superOn()) return out;                                 // a superposition has no single level to jump from
    if (p.view != "Orbit" && complexM() != 0) return out;      // a complex state: the dipole rule depends on the light's polarisation, which this lab does not model
    if (p.view == "Orbit") {
        for (int t = 1; t <= 4; t++) if (t != cur && (dir < 0 ? t < n0 : t > n0)) out.push_back(t);
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
    static const char* const series[5] = {"", "Lyman", "Balmer", "Paschen", "Brackett"};
    return {E, lam, p.Z == 1 ? std::string(series[lo]) + "-" + kGreek[hi - lo - 1] : std::string()};
}

template <class T> static T pick(const std::vector<T>& a) { return a[std::size_t(std::floor(random() * a.size()))]; }

void AtomLab::startJump(const std::string& kind, int to) {   // emit: the jump runs first, then the photon appears; absorb: the photon flies in first, the jump starts when it arrives
    const int from = curLevel();
    const PhotonInfo info = photonInfo(from, to);
    const std::string col = lamColorCss(info.lam);
    const Photon ph{kind, from, to, info.E, info.lam, info.name, col, random() * kPi * 2, 0, 1.6};
    if (webJumps || kind != "emit") s.photon = ph;              // (the web's emit: the photon leaves at once, with the jump)
    else s.photonAfter = ph;                                    // the smooth emit: it appears at the atom exactly when the jump ends
    if (std::none_of(s.lines.begin(), s.lines.end(), [&](const SeenLine& L) { return std::fabs(L.lam - info.lam) < 0.05; }))
        s.lines.push_back({info.lam, col});
    s.last = "last photon: " + std::string(kind == "emit" ? "emitted" : "absorbed") + "  ·  " + (info.name.empty() ? "" : info.name + "  ·  ") +
             lvl(from) + " → " + lvl(to) + "  ·  " + fmtLam(info.lam) + "  ·  " + fmtEV(info.E);
    if (kind == "emit") beginTransition(from, to);
}

// Build the jump's dots from b.done up to upTo. Each new dot is ONE step of the walk(), written out here with exactly its arithmetic and its 7 random numbers (from the chain's own copy of the
// generator, see beginTransition), but the wave function at the walker's place is carried from step to step: psi(to) at the point the walker stands on is the same number the next step starts
// with and the same number the new dot's amplitude needs (the function is pure, so the same arguments give the same bits), which is 2 evaluations per dot instead of 4.
// (The jump is between real orbitals: targets() gives none for a complex state, so walk()'s complex branch is not needed.)
void AtomLab::buildJump(long upTo) {
    AtomState::JumpBuild& b = s.jb;
    upTo = std::min(upTo, s.jn);
    const int n = orbitals()[stateOrbital(b.to)].n;
    const double sg = 0.4 * n * (n >= 4 ? 1.8 : 1) / p.Z;
    orbit::Mulberry32 r(b.rng);
    const auto gauss = [&r] { const double u = r.next(); return std::sqrt(-2 * std::log(1 - u)) * std::cos(2 * kPi * r.next()); };
    const auto chainStep = [&] {                               // = walk(to, w, false), with b.fo = psi(to, w) carried
        const double nx = b.w.x + sg * gauss(), ny = b.w.y + sg * gauss(), nz = b.w.z + sg * gauss();
        const double pOld = b.fo * b.fo;
        const double fn = psi(b.to, p.Z, nx, ny, nz);
        if (r.next() * pOld < fn * fn || pOld == 0) { b.w.x = nx; b.w.y = ny; b.w.z = nz; b.fo = fn; }
    };
    if (!b.settled) { for (int i = 0; i < 800; i++) chainStep(); b.settled = true; }
    for (; b.done < upTo; b.done++) {
        const long i = b.done;
        const double x = s.pts[3 * i] / kA0, y = s.pts[3 * i + 1] / kA0, z = s.pts[3 * i + 2] / kA0;      // the old dot
        s.jA[4 * i] = float(b.nA * psi(b.from, p.Z, x, y, z)); s.jA[4 * i + 2] = float(b.nB * psi(b.to, p.Z, x, y, z));
        chainStep();                                                                                       // the new dot
        s.jp[3 * i] = float(b.w.x * kA0); s.jp[3 * i + 1] = float(b.w.y * kA0); s.jp[3 * i + 2] = float(b.w.z * kA0);
        s.jB[4 * i] = float(b.nA * psi(b.from, p.Z, b.w.x, b.w.y, b.w.z)); s.jB[4 * i + 2] = float(b.nB * b.fo);
    }
    b.rng = r.seed;
    if (b.done >= s.jn) s.w2 = b.w;                            // the chain's end is where the new cloud's walker goes on from
}

// A jump. Orbit view: the electron glides from one circle to the other (0.8 s). Cloud views: two fixed populations of dots (the cloud on screen, and a new one of the target orbital),
// shown with the exact fractions of jumpFractions() while theta goes 0 -> pi/2 over 1.2 s (atom_core.h). Reduced motion: no glide, no cross-fade: the jump is over at once.
void AtomLab::beginTransition(int from, int to) {
    if (webJumps) {
        if (p.view == "Orbit") {                               // the web's Bohr picture: an instant leap to the new circular orbit
            const double a = std::atan2(s.y, s.x), r = double(to) * to / p.Z * kA0, v = double(p.Z) / to * kAuSpeed;
            s.x = r * std::cos(a); s.y = r * std::sin(a); s.vx = -v * std::sin(a); s.vy = v * std::cos(a);
            s.jumping = true; setParam("n", to); s.jumping = false; s.flash = 0;
        } else {                                               // the web's quantum picture: the cloud morphs, new dots replacing old ones with growing odds
            const double k = double(orbitals()[to].n) / p.Z;
            s.w2 = {0.7 * k, 0.4 * k, 1.1 * k};
            for (int i = 0; i < 800; i++) walk(to, s.w2, false);
            s.trans = Transition{from, to, 0, 1.2};
        }
        return;
    }
    const double dur = reducedMotion ? 0 : (p.view == "Orbit" ? 0.8 : 1.2);
    if (p.view == "Orbit") {
        s.gAng = std::atan2(s.y, s.x);
    } else {
        const long jn = std::min<long>(s.k, std::min<long>(cloudCap(), long(s.sg.size())));
        s.jn = jn; s.jp.assign(std::size_t(3 * jn), 0.f); s.jA.assign(std::size_t(4 * jn), 0.f); s.jB.assign(std::size_t(4 * jn), 0.f);
        // The dots are built by buildJump(), a part of them each frame (an old dot's amplitudes, and one step of the chain of new dots with its amplitudes, for the same index). Nothing is sampled during a
        // jump (step() does not run the cloud), so the old dots stay as they are and the random numbers are used by this chain only: the generator is moved past all of them now, exactly as the
        // old code used them, 800 steps and one per dot, 7 numbers each (3 pairs for the steps, 1 for the odds), so whatever uses random numbers after this gets the numbers it always got.
        const double k = double(orbitals()[to].n) / p.Z;
        s.w2 = {0.7 * k, 0.4 * k, 1.1 * k};
        AtomState::JumpBuild& b = s.jb;
        b = AtomState::JumpBuild();
        b.nA = normFactor(from, p.Z); b.nB = normFactor(to, p.Z); b.from = from; b.to = to;
        b.rng = randomState(); b.w = s.w2; b.fo = psi(to, p.Z, b.w.x, b.w.y, b.w.z);
        skipRandom(7ull * (800 + std::uint64_t(jn)));
        buildJump(0);                                           // the 800 steps that settle the chain (about 0.2 ms)
    }
    s.trans = Transition{from, to, 0, dur};
    if (dur == 0) finishTransition(0);
}

// The jump is over: the electron is on its new circle (Orbit), or the new dots become the cloud (cloud views); the emitted photon appears now.
void AtomLab::finishTransition(double overshoot) {
    if (p.view == "Orbit") {
        const int to = s.trans->to;
        const double a = s.gAng, r = double(to) * to / p.Z * kA0, v = double(p.Z) / to * kAuSpeed;
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
        if (s.jn > 0) s.k = s.jn;
        s.jumping = true;
        if (o.l > 0) setParam(o.l == 1 ? "pAx" : o.l == 2 ? "dAx" : "fAx", o.ori);
        setParam("orb", o.shell);
        s.jumping = false;
        s.w = s.w2; s.boost = 0.6;
        s.jp.clear(); s.jA.clear(); s.jB.clear(); s.jn = 0; s.jb = AtomState::JumpBuild();
        s.landT = 0;
    }
    s.trans.reset();
    if (s.photonAfter) { s.photon = *s.photonAfter; s.photon->t = overshoot; s.photonAfter.reset(); }
}

// Orbit view, during a jump: the electron's radius glides (smoothstep) from the old circle to the new one, turning at the rates of the two circles blended; no Coulomb stepping.
void AtomLab::glideOrbit(double dt) {
    const Transition& t = *s.trans;
    const double u0 = t.dur > 0 ? std::min(1.0, t.t / t.dur) : 1, e = u0 * u0 * (3 - 2 * u0);
    const double r1 = double(t.from) * t.from / p.Z * kA0, r2 = double(t.to) * t.to / p.Z * kA0, r = r1 + e * (r2 - r1);
    s.gAng += dt * p.speed * ((1 - e) / std::pow(double(t.from), 3) + e / std::pow(double(t.to), 3));   // rad per step of a circular orbit n: dt x speed / n^3 (any Z)
    const double v = (1 - e) * double(p.Z) / t.from * kAuSpeed + e * double(p.Z) / t.to * kAuSpeed;
    s.x = r * std::cos(s.gAng); s.y = r * std::sin(s.gAng); s.vx = -v * std::sin(s.gAng); s.vy = v * std::cos(s.gAng);
    s.t += dt * p.speed / (double(p.Z) * p.Z) * kAuTime;
    s.trail.push_back({s.x, s.y});
    if (s.trail.size() > 500) s.trail.pop_front();
}

void AtomLab::manualJump(int dir) {
    if (s.trans || s.photon) return;
    const std::vector<int> list = targets(dir);
    const int cur = curLevel();
    if (list.empty() && superOn()) {
        s.msg = "A superposition has no single level to jump from. Switch Superposition off to emit or absorb a photon.";
        s.msgUntil = nowMs() + 5500;
        return;
    }
    if (list.empty() && p.view != "Orbit" && complexM() != 0) {
        s.msg = stateText() + " is a complex state. Photon jumps from it are not in the lab: which levels light can reach depends on the light's polarisation, which the lab does not model. Pick a real orientation (px, pz, ...) to emit or absorb.";
        s.msgUntil = nowMs() + 6500;
        return;
    }
    if (list.empty()) {
        s.msg = dir < 0 ? (lvlN(cur) == 1 ? lvl(cur) + " is the lowest level: nothing to emit. Try Absorb."
                                           : lvl(cur) + " can't emit a photon (selection rule: l must change by 1), so it stays excited.")
                        : lvl(cur) + " is the highest level in this sim: nothing more to absorb.";
        s.msgUntil = nowMs() + 4500;
        return;
    }
    startJump(dir < 0 ? "emit" : "absorb", pick(list));
}

// photon flight, morph progress, auto mode (runs inside step, so Pause freezes it)
void AtomLab::advance(double dt) {
    const bool hadTrans = s.trans.has_value();
    s.landT += dt;
    if (s.photon) {
        s.photon->t += dt;
        if (s.photon->t >= s.photon->dur) {
            const Photon ph = *s.photon;
            s.photon.reset();
            if (ph.kind == "absorb") {
                beginTransition(ph.from, ph.to);                // the jump starts exactly when the photon arrives
                if (!webJumps && s.trans) s.trans->t = ph.t - ph.dur;
            }
        }
    }
    if (webJumps) {
        if (s.trans) {
            s.trans->t += dt;
            if (s.trans->t >= s.trans->dur) {
                const Orbital& o = orbitals()[s.trans->to];
                s.jumping = true;
                if (o.l > 0) setParam(o.l == 1 ? "pAx" : o.l == 2 ? "dAx" : "fAx", o.ori);
                setParam("orb", o.shell);
                s.jumping = false;
                s.w = s.w2; s.trans.reset(); s.boost = 0.6;
            }
        }
    } else if (hadTrans && s.trans) {
        s.trans->t += dt;
        if (s.trans->t >= s.trans->dur) finishTransition(s.trans->t - s.trans->dur);
    }
    if (s.flash) { *s.flash += dt; if (*s.flash > 0.7) s.flash.reset(); }
    if (s.boost > 0) s.boost -= dt;
    if (s.autoPhotons && !s.trans && !s.photon && (s.wait -= dt) <= 0) {
        const std::vector<int> down = targets(-1), up = targets(1);
        const bool goUp = down.empty() || (!up.empty() && random() < 0.25);
        const std::vector<int>& list = goUp ? up : down;
        if (!list.empty()) startJump(goUp ? "absorb" : "emit", pick(list));
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
        s.decAt = !s.dec || !s.dec->T ? INFINITY : -*s.dec->T / kLn2 * std::log(1 - random());
    }
    if (paused || hold || s.decHeld || std::isinf(s.decAt)) return;
    s.decAge += dt;
    if (s.decAge < s.decAt) return;
    // which way it decays, with the measured odds
    double r = random();
    std::string m = s.dec->br.back().first;
    for (const auto& [mm, v] : s.dec->br) { if (r < v) { m = mm; break; } r -= v; }
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
    return v >= 0.9995 ? std::string("100 %") : v >= 1e-4 ? jsNum(v * 100, 2) + " %" : sci(v * 100) + " %";
}
std::string AtomLab::decLine() const {                        // the readout line about decay
    if (!s.dec) return "";
    const DecayInfo& d = *s.dec;
    std::string top;
    for (std::size_t i = 0; i < d.br.size() && i < 3; i++) top += (i ? ", " : "") + decWord(d.br[i].first) + " " + pctText(d.br[i].second);
    const std::string unk = d.unk ? " (odds not measured, shown as equal)" : "";
    if (!d.T) return "☢ unstable: decays by " + top + unk + "  ·  no half-life measured, so the lab cannot time it";
    const double q = 1 - std::exp(-kLn2 * 60 / *d.T);       // chance of decaying within a minute
    const std::string qs = q > 0.9999 ? "certain" : q >= 1e-4 ? jsNum(q * 100, 2) + " %" : q > 0 ? sci(q * 100) + " %" : "0 %";
    return "☢ unstable: half-life " + fmtLife(*d.T) + "  ·  decays by " + top + unk + "  ·  chance it decays in the next minute: " + qs +
           (s.decHeld ? "  ·  this one is gone: tap ☢ Build another" : paused ? "  ·  paused: its clock is stopped" : "");
}

// ---------------------------------------------------------------- every frame / every step
void AtomLab::tick(double dt) {                               // runs every frame, even when paused: smooth dive and pan
    decTick(dt);                                               // radioactive decay: only while not paused
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
    if (p.view == "Orbit") { const int lv = s.trans && !webJumps ? std::max(s.trans->from, s.trans->to) : p.n;   // during a jump the picture fits the bigger of the two circles
        const double nn = p.ostyle == "Bohr 1913" ? std::max(3, lv) : lv; sz = nn * nn / p.Z * kA0; }
    else sz = kExt[s.trans ? (webJumps ? orbitals()[s.trans->to].n : std::max(orbitals()[s.trans->from].n, orbitals()[s.trans->to].n)) : sizeN()] / p.Z * kA0;
    sz = fitSize(p, sz);
    s.szCur = !s.szCur || rmv ? sz : *s.szCur + (sz - *s.szCur) * (1 - std::exp(-4 * dt));
    const Vec3d tg = s.focus >= 0 && s.focus < int(s.nuc.size()) ? Vec3d{s.nuc[s.focus].x, s.nuc[s.focus].y, s.nuc[s.focus].z} : Vec3d{};
    const double k = rmv ? 1 : 1 - std::exp(-5 * dt), m = std::min(viewW, viewH);
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
}

void AtomLab::step(double dt) {
    advance(dt);
    if (superOn()) s.beatT += dt * p.speed / 5.0;                // the beat clock: 4 s per beat at Playback speed 5, faster or slower with the speed (step() does not run while paused)
    if (p.view != "Orbit") {
        if (s.trans && !webJumps) {
            // a cloud jump: the two populations of dots are fixed, nothing is sampled (the dots are being built: a quarter of them each frame, so the start of a jump is not one long frame)
            if (!jumpReady()) buildJump(std::min(s.jn, s.jb.done + std::max<long>(2000, (s.jn + 3) / 4)));
        } else if (s.trans) {                                  // the web's morph: new dots come from the target orbital with growing odds
            const double u0 = s.trans->t / s.trans->dur, u = u0 * u0 * (3 - 2 * u0);
            for (int i = 0, n = 48 * cloudCap() / kCap; i < n; i++) {     // more dots to fill: more steps per frame (the dots themselves are sampled the same way)
                if (random() < u) walk(s.trans->to, s.w2, true);
                else walk(s.trans->from, s.w, true);
            }
        } else {
            const int n = (s.boost > 0 ? 48 : p.view == "Slice" ? 64 : 16) * (cloudCap() / kCap);
            for (int i = 0; i < n; i++) mcmc(true);
        }
        if (p.view == "Cloud 3D" && s.spin) s.yaw += 0.0015;   // slow auto-rotation
        return;
    }
    if (s.trans && !webJumps) { glideOrbit(dt); return; }      // Orbit, during a jump: the electron glides between the circles
    // Orbit: the electron on a Coulomb orbit, leapfrog (kick–drift–kick).
    // Time is slowed: h = dt·speed/Z² atomic time units per step (a nucleus with Z protons whirls the
    // electron Z² times faster, so it is slowed by Z² to keep the laps watchable).
    const double h = dt * p.speed / (double(p.Z) * p.Z) * kAuTime;
    const double kC = p.Z * kA0 * kA0 * kA0 / (kAuTime * kAuTime);   // Z·e²/(4πε₀·mₑ) in these units: a = −kC·r/|r|³
    auto acc = [&](double x, double y, double& ax, double& ay) { const double r3 = std::pow(std::hypot(x, y), 3); ax = -kC * x / r3; ay = -kC * y / r3; };
    // Sub-steps (0.10.6 stage 3, audit finding 14). A circular orbit n turns dt·speed/n³ radians per step (any Z). A leapfrog step of angle φ makes the radius wobble by about φ²/2 (measured:
    // n = 1 at speed 5, φ = 0.0208 rad, 302 steps per orbit: +2.2e-4; at speed 10: +8.7e-4; at speed 30, 50 steps per orbit: +7.8e-3). So one step is split into equal sub-steps so that none turns more than
    // kMaxStepAngle = 2π/300 rad (300 steps per orbit), the accuracy the default speed 5 has always had: the radius then stays within about 2.2e-4 and the energy within about 1e-4 at every speed. At speed 5 and
    // below nothing is split (n = 1 at speed 5 is 0.02083 rad, just under the limit), so the default motion is bit for bit the old one; only n = 1 above speed 5 is split (6 sub-steps at 30). The same
    // time passes and the electron turns at the same rate: nothing in the look or the speed changes, and the trail still gets one point per frame.
    const double stepAngle = dt * p.speed / (double(p.n) * p.n * p.n);
    const int sub = webJumps ? 1 : std::max(1, int(std::ceil(stepAngle / kMaxStepAngle)));   // (webJumps, tests only: the web's own single leapfrog step, which the web-compared runs follow)
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
    const std::string key = std::to_string(p.Z) + "," + std::to_string(p.N) + "," + std::to_string(s.nucSeed);
    if (s.nucKey != key) { s.nuc = buildNucleus(p.Z, p.N); s.nucKey = key; }
    // (the web also wipes its quark caches here; those belong to the drawing)
}

// ---------------------------------------------------------------- actions
void AtomLab::zoomAtom() { s.focus = -1; s.zoomTo = 0; s.panTo = Vec3d{}; }
void AtomLab::zoomNucleus() { s.focus = -1; s.zoomTo = zoomForNucleus(); s.panTo = Vec3d{}; }
void AtomLab::zoomNucleon() { if (s.focus < 0) s.focus = 0; s.zoomTo = zoomForNucleon(); s.panTo = Vec3d{}; }
bool AtomLab::showCentre() const { return p.zoom > 0.001 || (std::fabs(s.pan.x) + std::fabs(s.pan.y)) / kA0 > 0.002; }
std::string AtomLab::dragText() const { return s.dragMode == "move" ? "✥ Drag: move" : p.view == "Slice" ? "✥ Drag: slide the cut" : "✥ Drag: rotate"; }
bool AtomLab::showResetView() const {
    return p.view == "Cloud 3D" || (p.view == "Orbit" && (std::fabs(s.oyaw) > 0.02 || std::fabs(s.opitch - kPi / 2) > 0.02));
}
void AtomLab::resetView() {
    if (p.view == "Orbit") { s.oyaw = 0; s.opitch = kPi / 2; }
    else { s.yaw = 0.6; s.pitch = 0.35; }
}

// the ⓘ button, or a click on a nucleon to dive into it
void AtomLab::click(double x, double y) {
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
    const std::string act = forceMove || s.dragMode == "move" || p.view == "Cloud" ? "move"
                          : p.view == "Orbit" ? "turn" : p.view == "Cloud 3D" ? "rotate" : p.view == "Slice" ? "cut" : "";
    const double m = std::min(viewW, viewH);
    if (act == "rotate") { s.yaw += dx * 0.01; s.pitch = std::max(-1.5, std::min(1.5, s.pitch + dy * 0.01)); return; }
    if (act == "turn") { s.oyaw += dx * 0.01; s.opitch = std::max(0.02, std::min(kPi - 0.02, s.opitch + dy * 0.01)); return; }
    if (act == "cut") {
        if (!s.cutF || std::fabs(*s.cutF - p.cut) > 0.011) s.cutF = p.cut;   // keep fractions between drag events; re-sync if the slider moved it
        s.cutF = std::max(-1.0, std::min(1.0, *s.cutF - dy / (0.45 * m)));
        setParam("cut", jsRound(*s.cutF * 100) / 100);
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
    const double r = std::hypot(s.x, s.y) / kA0, v2 = (s.vx * s.vx + s.vy * s.vy) / (kAuSpeed * kAuSpeed);
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
        // The dots are ONE correlated Metropolis chain, so their mean radius is a rough estimate: measured over 96 runs at the full ring of kCap = 12,000 dots (12 orbitals, 8 seeds), 99 % were within 10 %
        // of the exact value below, the worst 17.7 % (0.10.6 stage 5). It is shown only with the full ring (it was 300 dots before), and says so.
        const std::string got = cnt >= kCap ? fmtLen(sum / cnt * 1e10) + " (an estimate, usually within 10 %)" : "…";
        const std::string th = fmtLen(meanRadius(o.n, o.l, p.Z) * kA0Ang);   // exact <r> for hydrogen-like atoms: the same function the radial graph uses
        const double E = levelEV(p.Z, p.N, o.n);   // the same reduced-mass level as the spectrum and Superposition (finding 3 of the 0.10.6 audit)
        return head + "  ·  " + stateText() + " orbital  ·  E −" + fmtEV(-E) + "  ·  avg r: sampled " + got + ", theory " + th;
    }
    const double r = std::hypot(s.x / kA0, s.y / kA0) * kA0Ang, E = orbitEnergyEV();
    const double T = 2 * kPi * std::pow(p.n, 3) / (double(p.Z) * p.Z) * kAuTimeAs;   // the orbit's period, as
    return head + "  ·  n=" + std::to_string(p.n) + "  ·  r " + fmtLen(r) + "  ·  E " + (E < 0 ? "−" : "") + fmtEV(std::fabs(E)) + "  ·  period " + fmtT(T);
}

std::vector<std::string> AtomLab::info() {
    const bool msgOn = !s.msg.empty() && nowMs() < s.msgUntil;
    if (p.view == "Orbit" && p.ostyle == "Bohr 1913") {        // Bohr 1913: only what Bohr knew
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
    std::vector<std::string> lines = {info1()}, more;           // 'more' lines show after a tap on ⓘ (on by default on wide screens)
    { const std::string dl = decLine(); if (!dl.empty()) lines.push_back(dl); }
    if (s.trans) lines.push_back("jumping: " + lvl(s.trans->from) + " → " + lvl(s.trans->to));
    if (!s.last.empty()) lines.push_back(s.last);
    if (msgOn) lines.push_back(s.msg);
    if (p.view != "Orbit" && p.zoom >= 2) {
        const Orbital& o = orbitals()[orbIndex(p.orb)];
        if (o.l > 0) more.push_back("electron density at the nucleus: 0 (this orbital has a node there)");
        else {
            const double P = 4.0 / 3 * std::pow(p.Z, 3) * std::pow(nucRA0(), 3) / std::pow(o.n, 3);   // ≈ |ψ(0)|² × the nucleus's volume
            // P is |psi(0)|² of the NON-relativistic wave times the nucleus's volume. For heavy atoms relativity makes the density at the nucleus of an s electron larger (the Dirac wave is larger there);
            // this lab does not compute that and gives no correction factor.
            more.push_back("chance the electron is inside the nucleus at any instant: ≈ " + sci(P) + " (1 in " + sci(1 / P) + ")  ·  non-relativistic estimate: for heavy atoms relativity makes the real value larger, and this lab does not work it out");
        }
    }
    if (p.view == "Slice") lines.push_back("cut plane: " + std::string(p.cutAxis == "Horizontal" ? "z" : "y") + " = " + fmt::fixed(cutOffsetM() * 1e10, 2) + " Å");
    if (superOn()) { lines.push_back(beatKindText()); lines.push_back("colour = the phase of the wave ψ relative to the first orbital (see the colour ring); it turns once per beat"); }
    else if (p.view != "Orbit") lines.push_back(usesWheel() ? "colour = the phase of the wave ψ (see the colour ring); it is not a place" : "two colours = the + and − halves of the wave ψ (zero where they meet)");
    if (p.view != "Orbit" && p.outline == "On") {
        lines.push_back("thin line: the 90% outline (the electron is found inside it 90% of the time)");
        more.push_back(std::string(p.view == "Slice" ? "90% outline: the line where the cut plane crosses the 90% surface. " : p.view == "Cloud" ? "90% outline: the shadow of the 90% region seen from the side. " : "90% outline: faint lines across the 90% surface, turning with the cloud. ") + "One electron, non-relativistic, hydrogen-like orbital with nuclear charge " + std::to_string(p.Z) + ", the same model as the cloud. It is the boundary of the smallest region that holds 90% of the probability, which follows the orbital's real shape; atoms with more electrons have different shapes.");
    }
    if (s.layer == "Nucleus" && p.Z + p.N >= 12) {
        const double b = nucBeta(p.Z, p.N);
        lines.push_back("shape: " + std::string(nucShapeWord(b)) + (nucMeasured(p.Z, p.N) ? "" : p.Z >= 100 ? " (a guess)" : " (estimated)"));
    }
    more.push_back(tempLine(p.Z));
    more.push_back("layer: " + s.layer + "  ·  this nucleus in real life: " + isoInfo(p.Z, p.N));
    {   // the model, said at EVERY Z: non-relativistic. The Dirac comparison is the lab's own formula (see kRelativityZ in atom_core.h)
        const double zx = p.Z * kAlpha, zx2 = zx * zx, sq = std::sqrt(1 - zx2), pct = 100 * zx2 / ((1 + sq) * (1 + sq));   // 100 ((1 - sq) / ((Z alpha)^2 / 2) - 1) = 100 (Z alpha)^2 / (1 + sq)^2, written without the subtractions (0.10.8 stage 2)
        more.push_back("model: one electron, non-relativistic (no relativity, fine structure, Lamb shift or spin). For a point nucleus the Dirac equation puts the 1s level " + jsNum(pct, 2) + " % deeper than this lab's.");
    }
    if (p.dirac == "On") more.push_back("relativity setting: ON. The spectrum cards show Dirac energies and lines; this readout's energy, the photons of your jumps and the clouds are still non-relativistic.");
    if (p.Z >= kRelativityZ) {   // relativity: the 1s electron of a heavy nucleus moves at a large fraction of light speed
        const double x = p.Z * kAlpha, model = -levelEV(p.Z, p.N, 1), real = kMecEV * x * x / (1 + std::sqrt(1 - x * x));   // Dirac 1s: mc²(1 − √(1−(Zα)²)) = mc² (Zα)² / (1 + √(1−(Zα)²)), without the subtraction (0.10.8 stage 2)
        more.push_back("1s electron speed: " + fmt::fixed(100 * x, 0) + "% of light speed  ·  real 1s energy −" + fmtEV(real) + " vs −" + fmtEV(model) + " in this lab");
    }
    if (p.style != "Hard balls" && s.layer != "Atom") more.push_back("nucleon edge: charge density ∝ e^(−r / 0.24 fm), so there is no sharp surface (rms radius 0.84 fm)");
    if (s.layer == "Nucleon" || s.layer == "Quarks") {
        const int i = s.focus >= 0 ? s.focus : (p.Z + p.N == 1 ? 0 : -1);
        const Nucleon* q = i >= 0 && i < int(s.nuc.size()) ? &s.nuc[i] : nullptr;
        lines.push_back(q ? std::string(q->isP ? "proton: up + up + down  ·  charge +⅔ +⅔ −⅓ = +1" : "neutron: up + down + down  ·  charge +⅔ −⅓ −⅓ = 0") +
                                "  ·  quarks are points (<0.001 fm), dots not to scale"
                          : std::string("click a nucleon to dive into it"));
    }
    if (moreShown()) lines.insert(lines.end(), more.begin(), more.end());
    return lines;
}

std::string AtomLab::hint() const {
    const double x = p.Z * kAlpha;
    std::string note;
    if (p.Z >= kRelativityZ) note += " Heavy atoms: this lab uses non-relativistic physics, but a 1s electron here moves at " + fmt::fixed(100 * x, 0) +
                           "% of light speed, so the true energy differs. The readout gives the Dirac-equation value for a point nucleus.";
    if (p.zoom >= 4 && p.qstyle != "Classic")
        note += " Quark colours (red, green, blue) are a charge of the strong force, not the up/down flavour: gluons carry colour between quarks, so each keeps changing colour while the three always add up to colourless. The glowing haze is the gluon field, an artistic version of what lattice-QCD calculations show; the quarks themselves are points.";
    if (p.scale == "Fixed") note += " Fixed scale: every atom and level is drawn at the same scale, so heavier nuclei pull the electron in close. Zoom in to follow it.";
    if (p.Z >= 104) note += " Superheavy elements are made a few atoms at a time; their half-lives are still being measured.";
    if (p.Z + p.N > 45 && p.zoom >= 4) note += " Quarks are drawn for the 45 nearest nucleons only (big nuclei have hundreds).";
    const double size = p.view == "Orbit" ? double(p.n) * p.n / p.Z : std::pow(orbitals()[orbIndex(p.orb)].n, 2) / p.Z;   // orbit/orbital size in a0
    const std::string ratio = fmt::jsLocale(std::strtod(fmt::jsToPrecision(size / nucRA0(), 2).c_str(), nullptr));
    const std::string find = "The nucleus is ~" + ratio + "× smaller than this " + (p.view == "Orbit" ? "orbit" : "orbital") +
                             ": scroll, pinch or use the zoom slider to find it. Zoom into the nucleus and click a nucleon to dive inside it.";
    const std::string drag = p.view == "Orbit" ? " Drag to turn the atom and see the orbit from another angle. Shift-drag or right-drag (or the ✥ button) moves the picture."
                           : p.view == "Cloud" ? " Drag to move around."
                           : p.view == "Cloud 3D" ? " Shift-drag or right-drag (or the ✥ button) moves the picture instead of rotating it."
                           : p.view == "Slice" ? " Drag up or down to slide the cut through the cloud. Shift-drag or right-drag (or the ✥ button) moves the picture." : "";
    if (p.view == "Slice")
        return "A thin flat cut through the 3D cloud, seen face-on: only electron positions inside the slab are shown. " + std::string(usesWheel() ? "The colours are the phase of the wave (ψ), shown by the colour ring; where the dots thin out to nothing is a node, a place the electron is never found." : "Blue and orange are opposite signs of the wave (ψ); where they meet is a node, a place the electron is never found.") + " Move the cut position to sweep the plane through the cloud. " + find + drag + note;
    if (p.view == "Cloud 3D") return "Drag to rotate; brighter dots are nearer. " + find + drag + note;
    if (p.view == "Cloud") return "Each dot is one possible position of the electron, sampled from |ψ|² (side view). There is no path, only probabilities. " + find + drag + note;
    std::string slow = fmt::jsToExponential(double(p.Z) * p.Z / (p.speed * kAuTimeAs * 1e-18), 0);
    const std::size_t e = slow.find("e+");
    if (e != std::string::npos) slow.replace(e, 2, "×10^");
    return "Orbit and nucleus are both to scale. " + find + " Time is slowed ~" + slow + "× so you can watch it" +
           (p.Z > 1 ? " (the real electron here is " + fmt::jsLocale(double(p.Z) * p.Z) + "× faster than in hydrogen)" : std::string()) + "." + drag + note;
}

}  // namespace atom
