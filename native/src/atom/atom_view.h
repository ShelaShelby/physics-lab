// Drawing the Atom Lab picture: the electron's orbit or cloud, the nucleus at its real size, the
// nucleons and their quarks, photons, the spectral-line strip, decays, the scale bar, the readout.
// Ported from the web's SIMS['Atom Lab'].draw() and its helpers (drawQuarks, drawQuarksField,
// decDraw) and the engine's readout. Clouds, glows and gluon fields are instanced draws.
#pragma once

#include <string>
#include <vector>

#include "app/prefs.h"
#include "atom_sim.h"
#include "outline.h"
#include "render/renderer2d.h"

struct ImDrawList;

namespace atom {

// The page colours the web reads from its CSS (light or dark theme).
struct Theme { render::Color fg, muted, accent, card, line, onacc; bool light; };
Theme theme(bool light);
Theme theme(const app::ThemeColors& c);          // the colours of the theme picked with 🎨

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
    int olOrb_ = -1; bool olHz_ = false; double olOff_ = 0; std::vector<OutSeg2> olSlice_;   // the 90 % outline in the Slice view: kept until the orbital or the cut changes
    double cloudExpo_ = 0;                 // the cloud's exposure (Glow), smoothed over frames; 0 = not set yet
};

}  // namespace atom
