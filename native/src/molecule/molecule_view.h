// Molecule Lab (0.18 stage 1): the lab's state on screen (the chosen molecule, style, view, the clicked atom or bond) and the picture.
// The picture is an orthographic 3D view drawn back to front (painter's order, no depth buffer): every ball and stick is one "blob" of the renderer's instanced blob batch,
// so the whole molecule is ONE instanced draw call (ball: a lit sphere-like gradient disc; stick: a line blob). Nothing is allocated per frame once the buffers have grown.
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

#include "app/prefs.h"
#include "atom/cloud_dots.h"
#include "molecule/density.h"
#include "molecule/molecule.h"
#include "molecule/morse.h"
#include "molecule/rotation.h"
#include "molecule/vibration.h"
#include "render/renderer2d.h"

struct ImDrawList;

namespace molecule {

constexpr double kSpinRadPerS = 0.5;       // design: auto-spin, 0.5 rad a second about the screen's vertical axis

// 0.21 stage 1: the Realistic style's cached data for the molecule on screen: the Monte Carlo dots (rebuilt only when the molecule or the amount changes) and the density
// surface coloured by the potential (rebuilt only when the molecule or the isovalue changes). Nothing here is rebuilt in a normal frame.
struct RealCache {
    const dens::Data* data = nullptr;
    std::string dotsId; int dotsN = 0;
    std::vector<float> dots;                 // angstrom, xyz each
    const std::vector<float>* activeDots = &dots;   // (0.24: the dots to draw: the still ones, or the vibrating swing's at this moment)
    std::uint64_t dotsEpoch = 0, activeEpoch = 0;   // 0.57.1: a new number whenever a set of dots is made (the card keeps each set once)
    std::vector<double> cdf;                 // sampling work buffer
    std::string meshId; int meshIso = -1;
    dens::Mesh mesh;                         // angstrom; value = the potential (au) at each vertex
    double gridElectrons = 0;                // the stored grid's integral (shown next to the true count)
    double meshRadius = 0;                   // 0.24: the still surface's farthest point from the centre (angstrom): the Density style's zoom fits it
    double potRange = 0;                     // the symmetric colour scale: -potRange..+potRange au (max |V| on this surface)
    double dotsMs = 0, meshMs = 0;           // how long the last rebuilds took (for the report)
    unsigned revision = 0;                   // goes up whenever the mesh is rebuilt
    bool ensure(const std::string& id, int dotsN, int isoIdx, const Molecule& m, int vibMode = -1, double vibS = 0);   // false: no data for this molecule (or the molecule is not drawn in the stored frame)
    // 0.24: vibrating (vibMode >= 0 and a vibrating density for it): the surface from the density blended at vibS (dens::blendVib) every time vibS changes, the dots from
    // kVibLevels fixed moments of the swing (made once each, when first needed); the colour scale stays the still surface's
    static constexpr int kVibLevels = 17;
    int vibKeyMode = -1; double vibKeyS = 2; std::string vibKeyId; int vibKeyDots = 0, vibKeyIso = -1;
    std::vector<std::vector<float>> vibDots;     // per level (empty: not made yet)
    std::vector<std::uint64_t> vibEpoch;         // 0.57.1: each level's set number
    dens::Data vibNow;                           // the blended density (kept: no allocation per frame once grown)
    bool meshVib = false;                        // 0.57.1: mesh holds the vibrating surface (the still one is made again when the swing stops)
};

struct MoleculeLab {
    Settings set;
    Pick sel;                                // the clicked atom or bond (none: -1)
    double viewW = 1, viewH = 1;             // the picture, points
    double freeX = 0, freeY = 0, freeW = -1, freeH = -1;   // the part the bars and the panel leave free (points; set by the shell); the molecule is centred in it
    const Molecule& mol() const { return get(set.mol); }
    double cx() const { return freeW > 0 ? freeX + freeW / 2 : viewW / 2; }
    double cy() const { return freeH > 0 ? freeY + freeH / 2 : viewH / 2; }
    double scale() const {                   // (0.24: in the Density style the zoom fits the electron surface, which reaches beyond the atoms' van der Waals balls: it ran off the picture)
        const double fw = freeW > 0 ? freeW : viewW, fh = freeH > 0 ? freeH : viewH;
        if (set.style == Style::Realistic && real.meshRadius > 0) return 0.48 * std::max(1.0, std::min(fw, fh)) / real.meshRadius * set.cam.zoom;
        if (anharmOn()) return fitScale(anharmReach(), fw, fh) * set.cam.zoom;   // 0.53.1: the stretched bond's whole swing stays in the picture
        return fitScale(mol(), fw, fh) * set.cam.zoom;
    }
    void choose(int i) {
        if (i == set.mol) return;
        const Camera was = defaultCamera(mol().id);                 // 0.52.1: a view nobody has turned yet moves to the new molecule's own default view (water, methanol: defaultCamera)
        bool untouched = true;
        for (int k = 0; k < 9; k++) untouched = untouched && set.cam.R[k] == was.R[k];
        set.mol = std::clamp(i, 0, count() - 1); sel = Pick{}; vibMode = -1; vibLive = false; rotJ = -1; rotAngle = 0; rotLine = -1; vibLevel = 0;
        if (untouched) { const Camera now = defaultCamera(mol().id); for (int k = 0; k < 9; k++) set.cam.R[k] = now.R[k]; }
        clampPan(set.cam, radiusA(mol()));
    }
    // 0.23: the vibrations (molecule/vibration.h): one mode at a time, in Ball-and-stick and Space-filling (the Density style is the molecule standing still). Not saved.
    int vibMode = -1;                        // the mode shown (index into vib::modes(id)), -1: standing still
    bool vibPlaying = true;                  // the swing runs (its own Play / Pause: this lab has no clock)
    int vibSize = 1;                         // 0: the real zero-point size, 1: kExaggerate times bigger
    double vibT = 0, vibPhase = 0;           // screen time of the swing (s) and the phase shown (-1..1)
    Molecule vibShown;                       // the moved atoms (only while vibOn())
    bool vibOn() const { return (set.style != Style::Realistic || dens::vibDensity(get(set.mol).id, vibMode)) && vibMode >= 0 && vibMode < static_cast<int>(vib::modes(get(set.mol).id).size()); }
    // 0.29 (stage 3): live, every mode at once at a temperature (vib::liveDisplaced); Ball-and-stick and Space-filling only. Not saved.
    bool vibLive = false;
    double liveK = 298.15;                   // the temperature, K
    double liveT = 0;                        // screen time (s); the real time is liveT / vib::slowDown()
    bool liveOn() const { return vibLive && set.style != Style::Realistic && !vib::modes(get(set.mol).id).empty(); }
    void setLive() { vibMode = -1; vibLive = true; liveT = 0; rotTurning = false; if (liveOn()) moveLive(); }
    void moveLive() { vib::liveDisplaced(get(set.mol), liveK, liveT / vib::slowDown(), vib::liveDrawnScale(get(set.mol), liveK, vibSize == 1), vibShown); }
    const Molecule& shown() const { return vibOn() || liveOn() ? vibShown : rotOn() ? rotShown : mol(); }   // what the picture draws (mol() keeps the still shape: the zoom fits it, the panel's values are of it)
    void setVib(int k) { vibLive = false; vibMode = k; vibT = 0; vibPhase = 0; if (k >= 0) rotTurning = false; if (vibOn()) moveVib(); }
    // 0.53: rotation (molecule/rotation.h). A linear molecule turns about an axis through its centre of mass (the molecule's z axis, across its length) at the classical rate
    // of level J, omega = sqrt(J(J+1)) hbar / I, slowed down rot::kSlowDown times; J = 0 does not turn. Ball-and-stick and Space-filling; never with a vibration. Not saved.
    bool rotTurning = false;                 // the picture turns
    bool rotCard = false;                    // the Rotation card is open
    int rotJ = -1;                           // the level shown; -1: the most populated at liveK
    int rotIso = rot::kNatural;              // HCl's isotope
    int rotSpec = 1;                         // the card's spectrum: 0 pure rotation, 1 the fundamental band (where the molecule has it)
    int rotLine = -1;                        // the card's tapped line (index into its list), -1: none
    double rotAngle = 0;                     // radians
    Molecule rotShown;
    bool rotOn() const { return rotTurning && set.style != Style::Realistic && !vibOn() && !liveOn() && rot::hasRotation(mol()); }
    rot::Rotor rotor() const { return rot::rotor(mol().id, rotIso); }
    int rotLevel() const {                   // the level shown: the chosen one, or the most populated (always an allowed one)
        const rot::Rotor r = rotor();
        if (rotJ < 0) return rot::mostPopulated(r, liveK);
        return rot::weight(r, rotJ) > 0 ? rotJ : rotJ + 1;
    }
    void setTurning(bool on) { rotTurning = on; if (on) { vibMode = -1; vibLive = false; } moveRot(); }
    void moveRot() {                         // the molecule turned by rotAngle about its z axis (through the centre of mass: the atoms are placed with it at the origin)
        const Molecule& m = mol();
        rotShown = m;
        const double c = std::cos(rotAngle), s = std::sin(rotAngle);
        for (Atom& a : rotShown.atoms) { const double x = a.x, y = a.y; a.x = c * x - s * y; a.y = s * x + c * y; }
    }
    void moveVib() {
        if (anharmOn()) { moveAnharm(); return; }
        const vib::Mode& md = vib::modes(get(set.mol).id)[vibMode]; vib::displaced(get(set.mol), md, vibPhase, vib::drawnScale(get(set.mol), md, vibSize == 1), vibShown);
    }
    // 0.53.1: the anharmonic bond (molecule/morse.h). For H2, N2, O2 and HCl, with "Anharmonic" ticked, the swing is the classical motion in the Morse curve at the energy of
    // level vibLevel (0 .. one above the last bound level, which dissociates), at real time vibT / vib::slowDown() (the same slow-down as the harmonic swings), drawn at real
    // size. The two atoms keep their centre of mass; the bond's stretch r - re (re of Huber & Herzberg) is added to the shape's bond. Reset (vibT = 0) brings a dissociated
    // pair back. Not saved.
    bool vibAnharm = false;
    int vibLevel = 0;
    double anharmR = 0;                      // the bond length shown now (angstrom)
    bool anharmOn() const { return vibAnharm && vibMode >= 0 && set.style != Style::Realistic && morse::has(get(set.mol).id) && vibMode < static_cast<int>(vib::modes(get(set.mol).id).size()); }
    int anharmTop() const { return morse::vMax(morse::curve(get(set.mol).id)) + 1; }   // the level control's top: one above the last bound level
    double anharmE() const {                 // the energy of the level shown, cm^-1
        const morse::Curve c = morse::curve(get(set.mol).id);
        return vibLevel <= morse::vMax(c) ? morse::levelShown(get(set.mol).id, vibLevel) : morse::dissociatedE(c);
    }
    static constexpr double kApartA = 3.0;   // a dissociated pair is drawn at most this far beyond re (it stays in the picture; the readout says dissociated)
    // the molecule with its bond at length r (the centre of mass kept)
    Molecule stretched(double r) const {
        const Molecule& eq = get(set.mol);
        Molecule out = eq;
        if (eq.atoms.size() != 2) return out;
        const morse::Curve c = morse::curve(eq.id);
        const Atom &a0 = eq.atoms[0], &a1 = eq.atoms[1];
        double u[3] = {a1.x - a0.x, a1.y - a0.y, a1.z - a0.z};
        const double L = std::sqrt(u[0] * u[0] + u[1] * u[1] + u[2] * u[2]);
        if (!(L > 0)) return out;
        const double m0 = element(a0.el).massU, m1 = element(a1.el).massU, d = r - c.re;
        for (int k = 0; k < 3; k++) u[k] /= L;
        out.atoms[0].x -= u[0] * d * m1 / (m0 + m1); out.atoms[0].y -= u[1] * d * m1 / (m0 + m1); out.atoms[0].z -= u[2] * d * m1 / (m0 + m1);
        out.atoms[1].x += u[0] * d * m0 / (m0 + m1); out.atoms[1].y += u[1] * d * m0 / (m0 + m1); out.atoms[1].z += u[2] * d * m0 / (m0 + m1);
        return out;
    }
    Molecule anharmReach() const {           // the bond at its outer turning point (a dissociated pair: as far as it is drawn)
        const morse::Curve c = morse::curve(get(set.mol).id);
        const morse::Motion mo = morse::motion(c, anharmE());
        return stretched(mo.bound ? std::min(mo.rOut, c.re + kApartA) : c.re + kApartA);
    }
    void moveAnharm() {
        const Molecule& eq = get(set.mol);
        if (eq.atoms.size() != 2) { vibShown = eq; return; }
        const morse::Curve c = morse::curve(eq.id);
        const double E = anharmE();
        const morse::Motion mo = morse::motion(c, E);
        double r = held ? (mo.bound ? mo.rOut : c.re + kApartA) : morse::bondAt(c, E, vibT / vib::slowDown());
        if (!std::isfinite(r) || r > c.re + kApartA) r = c.re + kApartA;
        anharmR = r;
        vibShown = stretched(r);
        if (!mo.bound && r > morse::motion(c, morse::level(c, morse::vMax(c))).rOut) vibShown.bonds.clear();   // dissociated and beyond the last bound swing: no bond drawn
    }
    bool held = false;                       // Reduced motion: the anharmonic swing is held at its outer turning point
    RealCache real;                          // 0.21: the Realistic style's dots and surface
    // 0.57.1: the Density style's data for what is on screen, the swing's moment included: the picture and the panel ask the same (the panel asked for the still
    // molecule, which undid the swing every frame: the still surface and one moment's 100,000 dots were made again in every frame of a vibration)
    bool ensureReal() {
        const Molecule& m = shown();
        double vs = 0;                       // the swing's moment in units of the vibrating density's A ("3 times bigger" = the ends; Real size = 1 / A of them)
        const dens::VibDensityMode* vm = vibOn() ? dens::vibDensity(m.id, vibMode) : nullptr;
        if (vm) vs = vibPhase * vib::drawnScale(mol(), vib::modes(m.id)[vibMode], vibSize == 1) / vm->A;
        return real.ensure(m.id, set.realDots, set.realIso, m, vm ? vibMode : -1, vs);
    }
    bool realOn() const { return set.style == Style::Realistic; }
    void setStyle(Style st) { set.style = st; if (st == Style::Realistic) set.realUsed = true; }
    void click(double x, double y) {
        sel = pick(shown(), set.cam, set.style, cx(), cy(), scale(), x, y);
    }
    void drag(double dx, double dy, bool panMode) {
        if (panMode) { pan(set.cam, dx, dy, scale()); clampPan(set.cam, radiusA(mol())); } else rotate(set.cam, dx, dy);
    }
    void wheel(double notches) { zoomBy(set.cam, std::pow(1.15, notches)); }
    void resetView() { set.cam = defaultCamera(mol().id); }   // (0.52.1: the molecule's own default view)
    void update(double dt, bool reducedMotion, bool paused) {
        if (set.spin && !reducedMotion && !paused && dt > 0 && dt < 0.25) spin(set.cam, kSpinRadPerS * dt);
        if (vibOn()) {                           // 0.23: the swing, at its slowed-down true speed; Reduced motion holds it at a turning point
            held = reducedMotion;
            if (reducedMotion) vibPhase = 1;
            else {
                if (vibPlaying && !paused && dt > 0 && dt < 0.25) vibT += dt;
                vibPhase = std::sin(2 * 3.14159265358979323846 * vibT / vib::screenPeriodS(vib::nuShown(get(set.mol).id, vibMode)));   // (the measured wavenumber: the true speed)
            }
            moveVib();
        }
        if (rotOn()) {                           // 0.53: the turning, at its slowed-down classical rate; Reduced motion holds it
            const double T = rot::screenTurnS(rotor(), rotLevel());
            if (!reducedMotion && T > 0 && dt > 0 && dt < 0.25) rotAngle = std::fmod(rotAngle + 2 * 3.14159265358979323846 * dt / T, 2 * 3.14159265358979323846);
            moveRot();
        }
        if (liveOn()) {                          // 0.29: every mode at its slowed-down true speed; Reduced motion holds one moment
            if (!reducedMotion && vibPlaying && !paused && dt > 0 && dt < 0.25) liveT += dt;
            moveLive();
        }
    }
};

render::Color realPotColour(double t);       // 0.21: the surface's potential colours (t = V / range, -1 red .. 0 white .. +1 blue), also for the panel's colour bar

struct ViewStats { int atoms = 0, sticks = 0, blobs = 0, labels = 0, stubs = 0, dots = 0, triangles = 0; double cpuMs = 0; bool resorted = false, cloudCard = false; const char* mode = "viewer"; };   // (0.57.1 cloudCard: the cloud was drawn by the card)   // (mode: viewer / realistic; stubs: open valences drawn. 0.21: mode realistic; dots and surface triangles drawn; resorted: the triangles were sorted again this frame)

class MoleculeView {
public:
    void draw(render::Renderer2D& r, ImDrawList* text, double uiScale, MoleculeLab& lab, double w, double h, const app::ThemeColors& c);
    const ViewStats& stats() const { return stats_; }
    const std::vector<Proj>& projected() const { return proj_; }   // the atoms on screen last frame (points; for the automatic checks)
    bool gpuCloud = true;                    // 0.57.1: the Density cloud made and drawn on the card (once its start-up check passed); false: the processor. Not saved

private:
    void drawRealistic(render::Renderer2D& r, MoleculeLab& lab, const app::ThemeColors& c);                                    // 0.21: the computed electron density (cloud, surface, nuclei)
    void drawLabels(ImDrawList* text, double uiScale, const Molecule& m);
    // the Realistic picture's per-frame buffers (kept: nothing is allocated once they have grown)
    std::vector<atom::GlowDot> glow_;
    std::vector<float> sx_, sy_, sz_;                // the surface's vertices on screen (points) and their depth (angstrom, bigger = nearer)
    std::vector<render::Color> vc_;                  // each vertex's shaded colour (front face)
    std::vector<float> tz_;                          // each triangle's depth
    std::vector<int> order_;                         // the triangles far to near
    double expo_ = 0;                                // the cloud's exposure, smoothed over frames (as the Atom Lab's)
    std::vector<std::vector<atom::GlowDot>> near_;   // 0.57.1: the card's cloud: the dots that can reach a nucleus, and the nuclei (for the exposure's cap; kept until the frame's end)
    std::vector<std::array<double, 2>> cores_;
    double lastCam_[14] = {};                        // the view the triangles were last sorted for (R, pan, zoom, centre, scale)
    unsigned lastRev_ = ~0u;
    struct Item { double depth; int kind, index, half; };   // kind 0 atom, 1 bond half (half 0 at bond.a, 1 at bond.b)
    std::vector<Proj> proj_;
    std::vector<Item> items_;
    std::vector<render::Renderer2D::Blob> blobs_;
    std::vector<render::Renderer2D::Blob> spheres_;      // 0.40: Space-filling, the balls as true spheres (kind 9, Renderer2D::spheres)
    ViewStats stats_;
};

}  // namespace molecule
