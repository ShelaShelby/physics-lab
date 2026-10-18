// Molecule Lab (0.18 stage 1): the lab's state on screen (the chosen molecule, style, view, the clicked atom or bond) and the picture.
// The picture is an orthographic 3D view drawn back to front (painter's order, no depth buffer): every ball and stick is one "blob" of the renderer's instanced blob batch,
// so the whole molecule is ONE instanced draw call (ball: a lit sphere-like gradient disc; stick: a line blob). Nothing is allocated per frame once the buffers have grown.
#pragma once

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include "app/prefs.h"
#include "atom/cloud_dots.h"
#include "molecule/builder.h"
#include "molecule/density.h"
#include "molecule/molecule.h"
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
    std::vector<double> cdf;                 // sampling work buffer
    std::string meshId; int meshIso = -1;
    dens::Mesh mesh;                         // angstrom; value = the potential (au) at each vertex
    double gridElectrons = 0;                // the stored grid's integral (shown next to the true count)
    double potRange = 0;                     // the symmetric colour scale: -potRange..+potRange au (max |V| on this surface)
    double dotsMs = 0, meshMs = 0;           // how long the last rebuilds took (for the report)
    unsigned revision = 0;                   // goes up whenever the mesh is rebuilt
    bool ensure(const std::string& id, int dotsN, int isoIdx, const Molecule& m);   // false: no data for this molecule (or the molecule is not drawn in the stored frame)
};

struct MoleculeLab {
    Settings set;
    Pick sel;                                // the clicked atom or bond (none: -1)
    double viewW = 1, viewH = 1;             // the picture, points
    double freeX = 0, freeY = 0, freeW = -1, freeH = -1;   // the part the bars and the panel leave free (points; set by the shell); the molecule is centred in it
    // 0.20 (stage 2 of the lab): the builder. Build mode has two views: the Sketch (flat, where editing happens) and the 3D shape (ESTIMATED, see molecule/builder.h)
    bool buildMode = false;
    int buildView = 0;                       // 0 Sketch, 1 3D shape
    int element = C;                         // what a click on empty sketch space adds
    bool buildUsed = false;                  // Build mode was opened: only then is the molbuild settings line written (older settings files stay byte for byte)
    build::Editor ed;
    Molecule built;                          // the estimated shape of the sketch (valid while builtOk)
    bool builtOk = false;
    std::string builtWhy = "Nothing to build yet: add atoms in the Sketch.";   // why there is no shape ("2 open valences" ...)
    int knownIdx = -1;                       // build::known() index of the sketch, -1: no name in our list
    unsigned seenRev = ~0u;
    build::Graph builtFrom;                  // the graph the shape was built from (its atoms' positions do not matter)
    int pressAtom = -1;                      // the sketch atom under the press that started a drag
    static constexpr double kSketchPt = 46;  // points per sketch unit (a drawing choice)

    bool sketchOn() const { return buildMode && buildView == 0; }
    bool showBuilt() const { return buildMode && buildView == 1 && builtOk; }
    const Molecule& mol() const { return showBuilt() ? built : get(set.mol); }
    double cx() const { return freeW > 0 ? freeX + freeW / 2 : viewW / 2; }
    double cy() const { return freeH > 0 ? freeY + freeH / 2 : viewH / 2; }
    double scale() const { return fitScale(mol(), freeW > 0 ? freeW : viewW, freeH > 0 ? freeH : viewH) * set.cam.zoom; }
    double sketchX(double x) const { return (x - cx()) / kSketchPt; }   // points -> sketch units
    double sketchY(double y) const { return (y - cy()) / kSketchPt; }
    void choose(int i) { if (i != set.mol) { set.mol = std::clamp(i, 0, count() - 1); sel = Pick{}; clampPan(set.cam, radiusA(mol())); } }
    RealCache real;                          // 0.21: the Realistic style's dots and surface
    bool realOn() const { return !buildMode && set.style == Style::Realistic; }   // (Realistic is offered in Viewer mode only: the builder's molecules have no computed density)
    void setStyle(Style st) { set.style = st; if (st == Style::Realistic) set.realUsed = true; }
    void click(double x, double y) {
        if (sketchOn()) {
            const double sx = sketchX(x), sy = sketchY(y);
            const int a = ed.hitAtom(sx, sy);
            if (a >= 0) { ed.picked = a; ed.message.clear(); return; }
            const int b = ed.hitBond(sx, sy, 0.2);
            if (b >= 0) { ed.cycleBond(b); return; }
            ed.addAtom(element, sx, sy);
            return;
        }
        if (buildMode && buildView == 1 && !builtOk) return;
        sel = pick(mol(), set.cam, set.style, cx(), cy(), scale(), x, y);
    }
    void press(double x, double y) { pressAtom = sketchOn() ? ed.hitAtom(sketchX(x), sketchY(y)) : -1; if (pressAtom >= 0) ed.beginDrag(pressAtom); }
    void drag(double dx, double dy, bool panMode) {
        if (sketchOn()) {
            if (pressAtom >= 0 && pressAtom < static_cast<int>(ed.graph().atoms.size()))
                ed.moveAtom(pressAtom, ed.graph().atoms[pressAtom].x + dx / kSketchPt, ed.graph().atoms[pressAtom].y + dy / kSketchPt);
            return;
        }
        if (panMode) { pan(set.cam, dx, dy, scale()); clampPan(set.cam, radiusA(mol())); } else rotate(set.cam, dx, dy);
    }
    void endDrag() { if (sketchOn() && pressAtom >= 0) ed.drop(pressAtom); pressAtom = -1; }
    void wheel(double notches) { if (!sketchOn()) zoomBy(set.cam, std::pow(1.15, notches)); }
    void resetView() { set.cam = defaultCamera(); }
    void update(double dt, bool reducedMotion, bool paused) { refresh(); if (set.spin && !reducedMotion && !paused && dt > 0 && dt < 0.25 && !sketchOn()) spin(set.cam, kSpinRadPerS * dt); }
    void setBuild(bool on) { if (on != buildMode) { buildMode = on; sel = Pick{}; if (on) buildUsed = true; refresh(); clampPan(set.cam, radiusA(mol())); } }
    void setBuildView(int v) { v = std::clamp(v, 0, 1); if (v != buildView) { buildView = v; sel = Pick{}; refresh(); clampPan(set.cam, radiusA(mol())); } }
    void deletePicked() { if (sketchOn() && ed.picked >= 0) ed.deleteAtom(ed.picked); }
    void showMeasured() { if (knownIdx >= 0 && build::known(knownIdx).viewer >= 0) { const int v = build::known(knownIdx).viewer; setBuild(false); choose(v); } }
    // the estimated shape follows the sketch: rebuilt only when its elements or bonds change (not when an atom is only moved), so nothing is allocated in a normal frame
    void refresh() {
        if (ed.revision() == seenRev) return;
        seenRev = ed.revision();
        const build::Graph& g = ed.graph();
        bool same = g.atoms.size() == builtFrom.atoms.size() && g.bonds.size() == builtFrom.bonds.size();
        for (std::size_t i = 0; same && i < g.atoms.size(); i++) same = g.atoms[i].el == builtFrom.atoms[i].el;
        for (std::size_t i = 0; same && i < g.bonds.size(); i++) same = g.bonds[i].a == builtFrom.bonds[i].a && g.bonds[i].b == builtFrom.bonds[i].b && g.bonds[i].order == builtFrom.bonds[i].order;
        if (same) return;
        builtFrom = g;
        sel = Pick{};
        knownIdx = build::matchKnown(g);
        builtOk = false;
        const int open = build::openValences(g), pc = build::pieces(g);
        if (g.atoms.empty()) builtWhy = "Nothing to build yet: add atoms in the Sketch.";
        else if (pc > 1) builtWhy = "The sketch is " + std::to_string(pc) + " separate pieces. A 3D shape is estimated only for one connected molecule: bond the pieces together in the Sketch.";
        else if (open > 0) builtWhy = std::to_string(open) + (open == 1 ? " open valence" : " open valences") + ". A 3D shape is estimated only for a complete molecule (every atom has all its bonds): add atoms or raise bond orders in the Sketch.";
        else builtOk = build::estimateShape(g, built, builtWhy);
    }
    build::BuildSettings buildSettings() const { build::BuildSettings b; b.build = buildMode; b.view = buildView; b.element = element; b.graph = ed.graph(); return b; }
    void loadBuild(const build::BuildSettings& b) { buildMode = b.build; buildView = b.view; element = b.element; ed.load(b.graph); seenRev = ~0u; builtFrom = build::Graph{}; builtFrom.atoms.push_back({-1, 0, 0}); refresh(); }
};

render::Color realPotColour(double t);       // 0.21: the surface's potential colours (t = V / range, -1 red .. 0 white .. +1 blue), also for the panel's colour bar

struct ViewStats { int atoms = 0, sticks = 0, blobs = 0, labels = 0, stubs = 0, dots = 0, triangles = 0; double cpuMs = 0; bool resorted = false; const char* mode = "viewer"; };   // (0.20: mode viewer / sketch / shape / noshape; stubs: open valences drawn. 0.21: mode realistic; dots and surface triangles drawn; resorted: the triangles were sorted again this frame)

class MoleculeView {
public:
    void draw(render::Renderer2D& r, ImDrawList* text, double uiScale, MoleculeLab& lab, double w, double h, const app::ThemeColors& c);
    const ViewStats& stats() const { return stats_; }
    const std::vector<Proj>& projected() const { return proj_; }   // the atoms on screen last frame (points; for the automatic checks)

private:
    void drawSketch(render::Renderer2D& r, ImDrawList* text, double uiScale, MoleculeLab& lab, const app::ThemeColors& c);   // 0.20: the builder's flat sketch
    void drawNote(ImDrawList* text, double uiScale, MoleculeLab& lab, const app::ThemeColors& c);                             // 0.20: the estimated shape's label, or why there is none
    void drawRealistic(render::Renderer2D& r, MoleculeLab& lab, const app::ThemeColors& c);                                    // 0.21: the computed electron density (cloud, surface, nuclei)
    void drawLabels(ImDrawList* text, double uiScale, const Molecule& m);
    // the Realistic picture's per-frame buffers (kept: nothing is allocated once they have grown)
    std::vector<atom::GlowDot> glow_;
    std::vector<float> sx_, sy_, sz_;                // the surface's vertices on screen (points) and their depth (angstrom, bigger = nearer)
    std::vector<render::Color> vc_;                  // each vertex's shaded colour (front face)
    std::vector<float> tz_;                          // each triangle's depth
    std::vector<int> order_;                         // the triangles far to near
    double expo_ = 0;                                // the cloud's exposure, smoothed over frames (as the Atom Lab's)
    double lastCam_[14] = {};                        // the view the triangles were last sorted for (R, pan, zoom, centre, scale)
    unsigned lastRev_ = ~0u;
    struct Item { double depth; int kind, index, half; };   // kind 0 atom, 1 bond half (half 0 at bond.a, 1 at bond.b)
    std::vector<Proj> proj_;
    std::vector<Item> items_;
    std::vector<render::Renderer2D::Blob> blobs_;
    ViewStats stats_;
};

}  // namespace molecule
