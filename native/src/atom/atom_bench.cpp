#include "atom/atom_bench.h"

#include <algorithm>
#include <cmath>
#include <string>

#include "atom/outline.h"

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
void cloud3D(AtomLab& L, const char* style, const char* density, const char* view = "Cloud 3D") {
    L.p.cstyle = style; L.p.cdens = density;     // set explicitly, so the old scene 3 stays comparable; set directly (the next setParam rebuilds the cloud to this size) so the random numbers are used exactly as before
    L.setParam("view", std::string(view)); L.setParam("orb", std::string("4f")); L.setParam("zoom", 0.0);
    L.resetView();
    for (int i = 0; i < 40000 && L.s.k < L.cloudCap(); i++) L.step(1.0 / 240);   // fill the cloud, the same way every time
}
// A superposition of two orbitals, Cloud 3D, Classic dots, all 48,000 dots, 50 / 50, the cloud filled the same way every time.
void superScene(AtomLab& L, const char* a, const char* axA, const char* b, const char* axB) {
    L.p.cstyle = "Classic"; L.p.cdens = "Ultra";
    L.setParam("view", std::string("Cloud 3D")); L.setParam("zoom", 0.0);
    L.setParam("orb", std::string(a)); L.setParam(std::string(a) == "2p" ? "pAx" : "dAx", std::string(axA));
    L.setParam("orb2", std::string(b)); L.setParam(std::string(b) == "2p" ? "pAx2" : "dAx2", std::string(axB));
    L.setParam("mix", 50.0); L.setParam("super", std::string("On"));
    L.resetView();
    for (int i = 0; i < 40000 && L.s.k < L.cloudCap(); i++) L.step(1.0 / 240);
}
void still(AtomLab& L) { L.s.zoomTo.reset(); L.s.panTo.reset(); L.s.pan = Vec3d{}; }   // no camera glide to wait for
}  // namespace

void benchSetup(AtomLab& L, int which, double w, double h) {
    if (which >= kBenchFirstRisk) {                      // the speed-risk scenes: U-238 like scene 3 (the nucleus scene: oganesson-294), every setting named, nothing animated by itself
        const bool og = which == 15;
        fresh(L, og ? 118 : 92, og ? 176 : 146, w, h);
        if (which <= 7) { L.setParam("phase", std::string(which == 6 ? "Sign" : "Wheel")); cloud3D(L, "Classic", "Ultra", which == 7 ? "Cloud" : "Cloud 3D"); }
        else if (which <= 9) cloud3D(L, "Classic", "Ultra");                        // the jump itself starts in benchFrame
        else if (which == 10) superScene(L, "2p", "z", "2p", "x");
        else if (which == 11) superScene(L, "2p", "x", "2p", "m+1");
        else if (which == 12) superScene(L, "2p", "z", "3d", "z2");
        else if (which == 13) { cloud3D(L, "Classic", "Normal"); clearOutlineCache(); }   // the outline is switched on at the first counted frame, in benchFrame
        else if (which == 14) { L.setParam("outline", std::string("On")); cloud3D(L, "Classic", "Normal", "Slice"); }
        else { nucleus(L, "Cinematic"); }
        still(L);
        return;
    }
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

const BenchInfo& benchSceneInfo(int which) {
    static const BenchInfo v[] = {
        {"Risk 10: phase colours (Wheel), Classic dots, Cloud 3D, 48,000 dots", "uranium-238, Cloud 3D, 4f, cloud style Classic, density Ultra (48,000 dots), Phase colours Wheel (24 colours): the drawing goes over all the dots once for each colour, in each of three depth bands. Fixed camera"},
        {"Risk 10 baseline: two colours (Sign), Classic dots, Cloud 3D, 48,000 dots", "the same as the scene before with Phase colours Sign (2 colours). The difference between the two is the cost of the 24 colours"},
        {"Risk 10: phase colours (Wheel), Classic dots, Cloud 2D (side view), 48,000 dots", "uranium-238, Cloud (the 2D side view), 4f, Classic, Ultra, Phase colours Wheel: the same colour-by-colour scan in the flat view"},
        {"Risk 11: cloud jump in flight (held half way), Cloud 3D, Classic dots, 48,000 dots", "uranium-238, Cloud 3D, 4f, Classic, Ultra: an emitted jump to a lower orbital is started and then held half way for the whole run, so every frame draws a jump in flight (the sum over two sets of 48,000 dots)"},
        {"Risk 11: cloud jumps one after another, Cloud 3D, Classic dots, 48,000 dots", "as the scene before, but the jump is let go: emit, then absorb back to 4f, again and again (the emitted photon is dropped). Each start builds 48,000 new dots in one frame: look at the worst frame"},
        {"Risk 11: superposition, orthogonal pair 2p z + 2p x (one energy), Cloud 3D, 48,000 dots", "uranium-238, Cloud 3D, Classic, Ultra, Superposition On, 2p (z) with 2p (x), 50 % each: the two real p orbitals are orthogonal, so the dots are thinned every frame"},
        {"Risk 11: superposition, overlapping pair 2p x + 2p m+1 (one energy), Cloud 3D, 48,000 dots", "the same with 2p (x) and the complex 2p (m = +1): these overlap, so the dots are sampled from the combined density (stage 4 of 0.10.6) and nothing is thinned"},
        {"Risk 11: superposition, orthogonal pair 2p z + 3d z2 (two energies, it beats), Cloud 3D, 48,000 dots", "2p (z) with 3d (z2), 50 % each: orthogonal, and the energies differ, so the density beats and every dot's colour and thinning change with the beat phase"},
        {"Risk 12: first use of an outline (cache emptied every 120 frames), Cloud 3D, 4f, 12,000 dots", "uranium-238, Cloud 3D, 4f, Classic, Normal (scene 3 of the benchmark). The outline is switched on at the first counted frame and the outline cache is emptied every 120 frames after that, so the 64 x 64 x 64 density pass runs again each time: look at the worst frame"},
        {"Risk 12: outline with the cut dragged, Slice view, 4f, 12,000 dots", "uranium-238, Slice view, 4f, Classic, Normal, 90 % outline On: the cut moves 0.01 every frame between -0.90 and +0.90 (as a drag does), so the outline's slice (40,000 density values) is redone every frame"},
        {"Risk 12: nucleus view, oganesson-294 (294 nucleons), whole nucleus, Cinematic quarks, + Sea detail", "oganesson-294 (118 protons, 176 neutrons), the whole nucleus in view, Cinematic quarks, + Sea detail, Fuzzy nucleons (scene 2 of the benchmark with the biggest nucleus)"},
    };
    return v[std::max(kBenchFirstRisk, std::min(which, kBenchSceneCount - 1)) - kBenchFirstRisk];
}

void benchFrame(AtomLab& L, int which, int sceneFrame, int measureFrame) {
    if (which == 8 || which == 9) {
        if (L.s.trans) { if (which == 8) L.s.trans->t = std::min(L.s.trans->t, 0.6); return; }       // 8: held half way (the jump lasts 1.2 s)
        if (L.s.photon && L.s.photon->kind == "emit") L.s.photon.reset();                               // the emitted photon is not part of the measurement
        if (L.s.photon || L.s.photonAfter) return;                                                      // an absorbed photon is flying in
        const int cur = L.curLevel(), start = orbIndex("4f|z3");
        if (cur == start || which == 8) { const std::vector<int> t = L.targets(-1); if (!t.empty()) L.startJump("emit", t.front()); }
        else {
            const std::vector<int> up = L.targets(1);
            int to = up.empty() ? -1 : up.front();
            for (int k : up) if (orbitals()[k].shell == "4f") { to = k; break; }
            if (to >= 0) L.startJump("absorb", to);
        }
    } else if (which == 13) {
        if (measureFrame < 0) return;
        if (measureFrame == 0) L.setParam("outline", std::string("On"));
        if (measureFrame % 120 == 0) clearOutlineCache();
    } else if (which == 14) {
        const int k = sceneFrame % 360, tri = k <= 180 ? k : 360 - k;                                   // up from -0.90 to +0.90 and back, 0.01 a frame
        L.setParam("cut", std::round((-0.9 + 0.01 * tri) * 100) / 100);
    }
}

std::string benchSettingsText(const AtomLab& L) {
    std::string t = "view " + L.p.view + ", Orbit style " + L.p.ostyle + ", quark style " + L.p.qstyle + ", detail " + L.p.detail + ", " + L.p.style + " nucleons";
    if (L.p.view != "Orbit") {                           // the cloud views: what the cost depends on
        t += ", orbital " + L.p.orb + ", cloud style " + L.p.cstyle + ", " + std::to_string(L.cloudCap()) + " dots, phase " + L.p.phase;
        if (L.p.super == "On") t += ", superposition " + L.p.orb + " + " + L.p.orb2 + " (" + std::to_string(int(L.p.mix)) + " %)";
        if (L.p.outline == "On") t += ", 90 % outline On";
    }
    return t;
}

}  // namespace atom
