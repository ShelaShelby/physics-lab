// Drawing the Atom Lab picture: the electron's orbit or cloud, the nucleus at its real size, the
// nucleons and their quarks, photons, the spectral-line strip, decays, the scale bar, the readout.
// Ported from the web's SIMS['Atom Lab'].draw() and its helpers (drawQuarks, drawQuarksField,
// decDraw) and the engine's readout. Clouds, glows and gluon fields are instanced draws.
#pragma once

#include <string>
#include <vector>

#include "app/prefs.h"
#include "atom_sim.h"
#include "chart_draw.h"
#include "outline.h"
#include "render/renderer2d.h"

struct ImDrawList;

namespace atom {

// The page colours the web reads from its CSS (light or dark theme).
struct Theme { render::Color fg, muted, accent, card, line, onacc; bool light; };
Theme theme(bool light);
Theme theme(const app::ThemeColors& c);          // the colours of the theme picked with 🎨

// 0.15 stage 5e: what the last frame drew for the marks, in the picture's own units (Bohr radii, counts), not in pixels: the automatic checks compare the old and the new UI with it
// (their pictures are different sizes). Filled by AtomView::draw; reading it costs nothing.
struct MarksDrawn {
    std::string outline = "none";          // which 90 % outline was drawn: none / side (Cloud) / slice / wire (Cloud 3D)
    int outlineOrb = -1;                   // its state (outlineStateId)
    double outlineMass = 0, outlineReach = 0;   // the probability inside it and how far it reaches, a0 (outline.h)
    std::size_t outlineSegs = 0;           // how many line pieces were drawn
    int phaseGroups = 0;                   // the cloud's colour groups (Sign: the two signs; Wheel: the phase steps); 0 = no cloud drawn
    bool wheel = false;
    double info[4] = {0, 0, 0, 0};          // the info text's plate at the top left (x, y, w, h in points); w = 0: none
    double centre[2] = {-1, -1};            // 0.15 stage 5h: the picture's drawing centre (points)
    double label[4] = {0, 0, 0, 0};         // 0.15 stage 5h: the nucleus label's box (x, y, w, h in points); w = 0: none drawn
    double scale[4] = {0, 0, 0, 0};         // 0.15 stage 5q: the scale bar with its label (x, y, w, h in points); for the automatic checks
    double infoPx = 0;                      // 0.15 stage 5h: the font size the info text asks for, in pixels (points x scale x text size)
    double ring[4] = {0, 0, 0, 0};          // the phase ring with its labels (x, y, w, h in points) when drawn; w = 0: not drawn
    int chainNodes = -1, chainEdges = -1;  // the decay chain drawn on the chart (-1: none)
};
const MarksDrawn& marksDrawn();

class AtomView {
public:
    // Draws one frame of a picture w × h points at (originX, originY) in the window; uiScale turns
    // points into window pixels (for the text, which Dear ImGui draws). Also sets lab.s.layer, the
    // nucleon hit targets and the ⓘ button's place, as the web's draw() does.
    void draw(render::Renderer2D& r, ImDrawList* text, double originX, double originY, double uiScale,
              AtomLab& lab, double w, double h, const app::ThemeColors& colours, double textScale = 1);

private:
    std::string quarkKey_;                 // which nucleus the quark bookkeeping below belongs to
    std::vector<int> qset_;                // nucleons that show their quarks (web: s.qset), in the order they were added
    std::vector<float> qf_;                // how far each nucleon's quarks have faded in (web: s.qf)
    double qNow_ = 0;
    ChartCache chartCache_;                // the chart's tiles / blocks and ground on the GPU (0.13 stage 4 part B); only used while lab.gpuChart is on
    SliceContour olSlice_;                 // the 90 % outline in the Slice view: kept until the orbital, the plane or the cut changes; with the GPU (0.13 stage 3) made by a compute job and a frame behind the cut (outline.h)
    std::vector<render::Renderer2D::Blob> blobs_;   // the blobs of one frame, kept so their buffer is not allocated again every frame (0.13 stage 4 part C)
    double cloudExpo_ = 0;                 // the cloud's exposure (Glow), smoothed over frames; 0 = not set yet
};

}  // namespace atom
