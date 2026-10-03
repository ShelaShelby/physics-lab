#include "atom/atom_bench.h"

#include <string>

namespace atom {

namespace {
void fresh(AtomLab& L, int Z, int N, double w, double h) {
    seedRandom(20260101);                                // the same random numbers every run
    L.p = AtomParams::appDefaults();                                // the Atom Lab's own defaults (what "Defaults" gives): no setting of the user's can leak in (Orbit style Bohr 1913 hid the nucleons)
    L.p.super = "Off";                                   // no superposition in the benchmark (its dots would be thinned and coloured every frame): the old numbers stay comparable
    L.p.outline = "Off";                                 // the 90 % outline is never drawn in the benchmark, so the old numbers stay comparable
    L.moreStored.reset();                                // the ⓘ More / Less choice changes the readout in the picture
    L.chain.clear(); L.paused = false; L.hold = false;
    L.viewW = w; L.viewH = h;
    L.setParam("Z", double(Z)); L.setParam("N", double(N));
    L.restart();
    L.s.nucSeed = 1;
    L.s.spin = false;
}
void nucleus(AtomLab& L, const char* qstyle) {
    L.setParam("view", std::string("Orbit")); L.setParam("ostyle", std::string("Modern")); L.setParam("qstyle", std::string(qstyle)); L.setParam("detail", std::string("+ Sea")); L.setParam("style", std::string("Fuzzy"));
    L.ensureNucleus();
    L.setParam("zoom", L.zoomForNucleus());
}
// Cloud 3D, the 4f orbital, the cloud filled the same way every time. style / density: the cloud's look and number of dots.
void cloud3D(AtomLab& L, const char* style, const char* density) {
    L.p.cstyle = style; L.p.cdens = density;     // set explicitly, so the old scene 3 stays comparable; set directly (the next setParam rebuilds the cloud to this size) so the random numbers are used exactly as before
    L.setParam("view", std::string("Cloud 3D")); L.setParam("orb", std::string("4f")); L.setParam("zoom", 0.0);
    L.resetView();
    for (int i = 0; i < 40000 && L.s.k < L.cloudCap(); i++) L.step(1.0 / 240);   // fill the cloud, the same way every time
}
void still(AtomLab& L) { L.s.zoomTo.reset(); L.s.panTo.reset(); L.s.pan = Vec3d{}; }   // no camera glide to wait for
}  // namespace

void benchSetup(AtomLab& L, int which, double w, double h) {
    if (which == 4) {                                    // stress: the same cloud as scene 2, drawn as Glow with 48,000 dots
        fresh(L, 92, 146, w, h);
        cloud3D(L, "Glow", "Ultra");
        still(L);
        return;
    }
    if (which == 3) {                                    // stress: the biggest nucleus, every nucleon drawn large
        fresh(L, 118, 176, w, h);
        nucleus(L, "Accurate");
        L.setParam("zoom", L.zoomForNucleus() + 0.6);
        still(L);
        return;
    }
    fresh(L, 92, 146, w, h);
    if (which == 0) { nucleus(L, "Cinematic"); still(L); }
    else if (which == 1) {
        cloud3D(L, "Classic", "Normal");                 // exactly the cloud of the older versions
        still(L);
    } else {
        nucleus(L, "Accurate");
        L.s.focus = 0;
        L.setParam("zoom", L.zoomForNucleon());
        still(L);
    }
}

std::string benchSettingsText(const AtomLab& L) {
    return "view " + L.p.view + ", Orbit style " + L.p.ostyle + ", quark style " + L.p.qstyle + ", detail " + L.p.detail + ", " + L.p.style + " nucleons";
}

}  // namespace atom
