#include "atom_sim.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <map>

#include "orbit/format.h"

namespace atom {

static double jsRound(double x) { const double r = std::floor(x + 0.5); return r == 0 && x < 0 ? -0.0 : r; }
static double clampZoom(double z) { return std::max(0.0, std::min(7.0, z)); }
static const char* const kLetter = "spdf";
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
        {"orb", {"1s", "2s", "2p", "3s", "3p", "3d", "4s", "4p", "4d", "4f"}}, {"pAx", {"x", "y", "z"}}, {"dAx", {"z2", "xz", "yz", "xy", "x2y2"}},
        {"fAx", {"z3", "xz2", "yz2", "xyz", "zx2y2", "xx23y2", "y3x2y2"}}, {"cutAxis", {"Vertical", "Horizontal"}}};
    const auto it = opts.find(key);
    return it != opts.end() && std::find(it->second.begin(), it->second.end(), v) != it->second.end();
}

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
    s.pts.assign(kCap * 3, 0.f); s.sg.assign(kCap, 0); s.k = 0; s.w = {0.7 * k, 0.4 * k, 1.1 * k};
    s.yaw = 0.6; s.pitch = 0.35; s.oyaw = 0; s.opitch = kPi / 2; s.spin = !reducedMotion;   // Cloud 3D turns by itself, unless motion is reduced
    s.fc = {}; s.focus = -1; s.zoomTo.reset(); s.layer = "Atom"; s.hits.clear();
    s.trans.reset(); s.photon.reset(); s.lines.clear(); s.autoPhotons = false; s.wait = 0.3; s.boost = 0;
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
    else return;
    v = std::max(lo, std::min(hi, v));
    if (key == "zoom") p.zoom = v;
    else if (key == "Z") p.Z = int(v);
    else if (key == "N") p.N = int(v);
    else if (key == "cut") p.cut = v;
    else if (key == "n") p.n = int(v);
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
    static const char* const reinit[] = {"Z", "n", "view", "orb", "pAx", "dAx", "fAx"};
    if (std::find(std::begin(reinit), std::end(reinit), key) != std::end(reinit)) {
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

// the size the picture is fitted to: Fixed uses one reference size (hydrogen n = 4 / 3s) for every atom and level
static double fitSize(const AtomParams& p, double szM) { return p.scale == "Fixed" ? (p.view == "Orbit" ? 16 : 24) * kA0 : szM; }
double AtomLab::sizeM() const {
    return fitSize(p, p.view == "Orbit" ? double(p.n) * p.n / p.Z * kA0 : kExt[orbitals()[orbIndex(p.orb)].n] / p.Z * kA0);
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
double AtomLab::cutOffsetM() const { return p.cut * 0.5 * kExt[orbitals()[orbIndex(p.orb)].n] / p.Z * kA0; }

// ---------------------------------------------------------------- the cloud: Metropolis sampling of |ψ|²
// One step: try a random move (a Gaussian step in each direction) and accept it with the odds
// |ψ_new|² / |ψ_old|². The dots that pile up are spread like the electron's probability.
void AtomLab::walk(int orb, Vec3d& w, bool record) {
    const int n = orbitals()[orb].n;
    const double sg = 0.4 * n * (n >= 4 ? 1.8 : 1) / p.Z;     // bigger steps for the big n = 4 clouds, so the dots spread through them quickly
    auto gauss = [] { const double u = random(); return std::sqrt(-2 * std::log(1 - u)) * std::cos(2 * kPi * random()); };
    const double nx = w.x + sg * gauss(), ny = w.y + sg * gauss(), nz = w.z + sg * gauss();
    const double fo = psi(orb, p.Z, w.x, w.y, w.z), pOld = fo * fo;
    const double fn = psi(orb, p.Z, nx, ny, nz);
    if (random() * pOld < fn * fn || pOld == 0) { w.x = nx; w.y = ny; w.z = nz; }
    if (record) {
        const long i = s.k % kCap;
        s.sg[i] = psi(orb, p.Z, w.x, w.y, w.z) >= 0 ? 1 : -1;   // the sign of the wave here: the phase colour
        s.pts[3 * i] = float(w.x * kA0); s.pts[3 * i + 1] = float(w.y * kA0); s.pts[3 * i + 2] = float(w.z * kA0);
        s.k++;
    }
}

// ---------------------------------------------------------------- photon jumps
int AtomLab::curLevel() const { return p.view == "Orbit" ? p.n : fullOrb(); }
int AtomLab::lvlN(int l) const { return p.view == "Orbit" ? l : orbitals()[l].n; }
std::string AtomLab::lvl(int l) const { return p.view == "Orbit" ? "n=" + std::to_string(l) : orbitals()[l].txt; }

std::vector<int> AtomLab::targets(int dir) const {          // dir −1: lower levels (emit), +1: higher levels (absorb)
    const int cur = curLevel(), n0 = lvlN(cur);
    std::vector<int> out;
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

void AtomLab::startJump(const std::string& kind, int to) {   // emit: the photon leaves now; absorb: it arrives first
    const int from = curLevel();
    const PhotonInfo info = photonInfo(from, to);
    const std::string col = lamColorCss(info.lam);
    s.photon = Photon{kind, from, to, info.E, info.lam, info.name, col, random() * kPi * 2, 0, 1.6};
    if (std::none_of(s.lines.begin(), s.lines.end(), [&](const SeenLine& L) { return std::fabs(L.lam - info.lam) < 0.05; }))
        s.lines.push_back({info.lam, col});
    s.last = "last photon: " + std::string(kind == "emit" ? "emitted" : "absorbed") + "  ·  " + (info.name.empty() ? "" : info.name + "  ·  ") +
             lvl(from) + " → " + lvl(to) + "  ·  " + fmtLam(info.lam) + "  ·  " + fmtEV(info.E);
    if (kind == "emit") beginTransition(from, to);
}

void AtomLab::beginTransition(int from, int to) {
    if (p.view == "Orbit") {                                   // Bohr picture: an instant leap to the new circular orbit
        const double a = std::atan2(s.y, s.x), r = double(to) * to / p.Z * kA0, v = double(p.Z) / to * kAuSpeed;
        s.x = r * std::cos(a); s.y = r * std::sin(a); s.vx = -v * std::sin(a); s.vy = v * std::cos(a);
        s.jumping = true; setParam("n", to); s.jumping = false; s.flash = 0;
    } else {                                                   // quantum picture: the cloud morphs from one orbital to the other
        const double k = double(orbitals()[to].n) / p.Z;
        s.w2 = {0.7 * k, 0.4 * k, 1.1 * k};
        for (int i = 0; i < 800; i++) walk(to, s.w2, false);
        s.trans = Transition{from, to, 0, 1.2};
    }
}

void AtomLab::manualJump(int dir) {
    if (s.trans || s.photon) return;
    const std::vector<int> list = targets(dir);
    const int cur = curLevel();
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
    if (s.photon) {
        s.photon->t += dt;
        if (s.photon->t >= s.photon->dur) {
            const Photon ph = *s.photon;
            s.photon.reset();
            if (ph.kind == "absorb") beginTransition(ph.from, ph.to);
        }
    }
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
    if (p.view == "Orbit") { const double nn = p.ostyle == "Bohr 1913" ? std::max(3, p.n) : p.n; sz = nn * nn / p.Z * kA0; }
    else sz = kExt[orbitals()[s.trans ? s.trans->to : orbIndex(p.orb)].n] / p.Z * kA0;
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
    if (p.view != "Orbit") {
        if (s.trans) {                                         // morph: new dots come from the target orbital with growing odds
            const double u0 = s.trans->t / s.trans->dur, u = u0 * u0 * (3 - 2 * u0);
            for (int i = 0; i < 48; i++) {
                if (random() < u) walk(s.trans->to, s.w2, true);
                else walk(s.trans->from, s.w, true);
            }
        } else {
            const int n = s.boost > 0 ? 48 : p.view == "Slice" ? 64 : 16;
            for (int i = 0; i < n; i++) mcmc(true);
        }
        if (p.view == "Cloud 3D" && s.spin) s.yaw += 0.0015;   // slow auto-rotation
        return;
    }
    // Orbit: the electron on a Coulomb orbit, leapfrog (kick–drift–kick).
    // Time is slowed: h = dt·speed/Z² atomic time units per step (a nucleus with Z protons whirls the
    // electron Z² times faster, so it is slowed by Z² to keep the laps watchable).
    const double h = dt * p.speed / (double(p.Z) * p.Z) * kAuTime;
    const double kC = p.Z * kA0 * kA0 * kA0 / (kAuTime * kAuTime);   // Z·e²/(4πε₀·mₑ) in these units: a = −kC·r/|r|³
    auto acc = [&](double x, double y, double& ax, double& ay) { const double r3 = std::pow(std::hypot(x, y), 3); ax = -kC * x / r3; ay = -kC * y / r3; };
    double ax, ay;
    acc(s.x, s.y, ax, ay);
    s.vx += ax * h / 2; s.vy += ay * h / 2;
    s.x += s.vx * h; s.y += s.vy * h;
    acc(s.x, s.y, ax, ay);
    s.vx += ax * h / 2; s.vy += ay * h / 2;
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
static double orbitEnergyEV(const AtomState& s, int Z) {
    const double r = std::hypot(s.x, s.y) / kA0, v2 = (s.vx * s.vx + s.vy * s.vy) / (kAuSpeed * kAuSpeed);
    return (0.5 * v2 - Z / r) * kHartreeEV;
}

std::string AtomLab::info1() {
    const std::string head = std::string(name(p.Z)) + "-" + std::to_string(p.Z + p.N) + " (" + ionLabel(p.Z) + ")";
    if (p.view != "Orbit") {
        const Orbital& o = orbitals()[orbIndex(p.orb)];
        const long cnt = std::min<long>(s.k, kCap);
        double sum = 0;
        for (long i = 0; i < cnt; i++) sum += std::hypot(double(s.pts[3 * i]), double(s.pts[3 * i + 1]), double(s.pts[3 * i + 2]));
        const std::string got = cnt > 300 ? fmtLen(sum / cnt * 1e10) : "…";
        const std::string th = fmtLen((3.0 * o.n * o.n - o.l * (o.l + 1)) / 2 / p.Z * kA0Ang);   // exact <r> for hydrogen-like atoms
        const double E = -0.5 * kHartreeEV * p.Z * p.Z / (o.n * o.n);
        return head + "  ·  " + lvl(fullOrb()) + " orbital  ·  E −" + fmtEV(-E) + "  ·  avg r: sampled " + got + ", theory " + th;
    }
    const double r = std::hypot(s.x / kA0, s.y / kA0) * kA0Ang, E = orbitEnergyEV(s, p.Z);
    const double T = 2 * kPi * std::pow(p.n, 3) / (double(p.Z) * p.Z) * kAuTimeAs;   // the orbit's period, as
    return head + "  ·  n=" + std::to_string(p.n) + "  ·  r " + fmtLen(r) + "  ·  E " + (E < 0 ? "−" : "") + fmtEV(std::fabs(E)) + "  ·  period " + fmtT(T);
}

std::vector<std::string> AtomLab::info() {
    const bool msgOn = !s.msg.empty() && nowMs() < s.msgUntil;
    if (p.view == "Orbit" && p.ostyle == "Bohr 1913") {        // Bohr 1913: only what Bohr knew
        const double r = std::hypot(s.x / kA0, s.y / kA0) * kA0Ang, E = orbitEnergyEV(s, p.Z);
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
            more.push_back("chance the electron is inside the nucleus at any instant: ≈ " + sci(P) + " (1 in " + sci(1 / P) + ")");
        }
    }
    if (p.view == "Slice") lines.push_back("cut plane: " + std::string(p.cutAxis == "Horizontal" ? "z" : "y") + " = " + fmt::fixed(cutOffsetM() * 1e10, 2) + " Å");
    if (p.view != "Orbit") lines.push_back("two colours = the + and − halves of the wave ψ (zero where they meet)");
    if (s.layer == "Nucleus" && p.Z + p.N >= 12) {
        const double b = nucBeta(p.Z, p.N);
        lines.push_back("shape: " + std::string(nucShapeWord(b)) + (nucMeasured(p.Z, p.N) ? "" : p.Z >= 100 ? " (a guess)" : " (estimated)"));
    }
    more.push_back(tempLine(p.Z));
    more.push_back("layer: " + s.layer + "  ·  this nucleus in real life: " + isoInfo(p.Z, p.N));
    if (p.Z >= 27) {   // relativity: the 1s electron of a heavy nucleus moves at a large fraction of light speed
        const double x = p.Z / 137.036, model = 0.5 * kHartreeEV * p.Z * p.Z, real = 510998.95 * (1 - std::sqrt(1 - x * x));   // Dirac 1s: mc²(√(1−(Zα)²) − 1)
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
    const double x = p.Z / 137.036;
    std::string note;
    if (p.Z >= 27) note += " Heavy atoms: this lab uses non-relativistic physics, but a 1s electron here moves at " + fmt::fixed(100 * x, 0) +
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
        return "A thin flat cut through the 3D cloud, seen face-on: only electron positions inside the slab are shown. Blue and orange are opposite signs of the wave (ψ); where they meet is a node, a place the electron is never found. Move the cut position to sweep the plane through the cloud. " + find + drag + note;
    if (p.view == "Cloud 3D") return "Drag to rotate; brighter dots are nearer. " + find + drag + note;
    if (p.view == "Cloud") return "Each dot is one possible position of the electron, sampled from |ψ|² (side view). There is no path, only probabilities. " + find + drag + note;
    std::string slow = fmt::jsToExponential(double(p.Z) * p.Z / (p.speed * kAuTimeAs * 1e-18), 0);
    const std::size_t e = slow.find("e+");
    if (e != std::string::npos) slow.replace(e, 2, "×10^");
    return "Orbit and nucleus are both to scale. " + find + " Time is slowed ~" + slow + "× so you can watch it" +
           (p.Z > 1 ? " (the real electron here is " + fmt::jsLocale(double(p.Z) * p.Z) + "× faster than in hydrogen)" : std::string()) + "." + drag + note;
}

}  // namespace atom
