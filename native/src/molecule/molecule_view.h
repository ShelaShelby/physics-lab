// Molecule Lab (0.18 stage 1): the lab's state on screen (the chosen molecule, style, view, the clicked atom or bond) and the picture.
// The picture is an orthographic 3D view drawn back to front (painter's order, no depth buffer): every ball and stick is one "blob" of the renderer's instanced blob batch,
// so the whole molecule is ONE instanced draw call (ball: a lit sphere-like gradient disc; stick: a line blob). Nothing is allocated per frame once the buffers have grown.
#pragma once

#include <algorithm>
#include <cmath>
#include <vector>

#include "app/prefs.h"
#include "molecule/molecule.h"
#include "render/renderer2d.h"

struct ImDrawList;

namespace molecule {

constexpr double kSpinRadPerS = 0.5;       // design: auto-spin, 0.5 rad a second about the screen's vertical axis

struct MoleculeLab {
    Settings set;
    Pick sel;                                // the clicked atom or bond (none: -1)
    double viewW = 1, viewH = 1;             // the picture, points
    double freeX = 0, freeY = 0, freeW = -1, freeH = -1;   // the part the bars and the panel leave free (points; set by the shell); the molecule is centred in it
    const Molecule& mol() const { return get(set.mol); }
    double cx() const { return freeW > 0 ? freeX + freeW / 2 : viewW / 2; }
    double cy() const { return freeH > 0 ? freeY + freeH / 2 : viewH / 2; }
    double scale() const { return fitScale(mol(), freeW > 0 ? freeW : viewW, freeH > 0 ? freeH : viewH) * set.cam.zoom; }
    void choose(int i) { if (i != set.mol) { set.mol = std::clamp(i, 0, count() - 1); sel = Pick{}; } }
    void click(double x, double y) { sel = pick(mol(), set.cam, set.style, cx(), cy(), scale(), x, y); }
    void drag(double dx, double dy, bool panMode) { if (panMode) pan(set.cam, dx, dy, scale()); else rotate(set.cam, dx, dy); }
    void wheel(double notches) { zoomBy(set.cam, std::pow(1.15, notches)); }
    void resetView() { set.cam = defaultCamera(); }
    void update(double dt, bool reducedMotion, bool paused) { if (set.spin && !reducedMotion && !paused && dt > 0 && dt < 0.25) spin(set.cam, kSpinRadPerS * dt); }
};

struct ViewStats { int atoms = 0, sticks = 0, blobs = 0, labels = 0; double cpuMs = 0; };

class MoleculeView {
public:
    void draw(render::Renderer2D& r, ImDrawList* text, double uiScale, MoleculeLab& lab, double w, double h, const app::ThemeColors& c);
    const ViewStats& stats() const { return stats_; }
    const std::vector<Proj>& projected() const { return proj_; }   // the atoms on screen last frame (points; for the automatic checks)

private:
    struct Item { double depth; int kind, index, half; };   // kind 0 atom, 1 bond half (half 0 at bond.a, 1 at bond.b)
    std::vector<Proj> proj_;
    std::vector<Item> items_;
    std::vector<render::Renderer2D::Blob> blobs_;
    ViewStats stats_;
};

}  // namespace molecule
