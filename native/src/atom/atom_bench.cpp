#include "atom/atom_bench.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <utility>
#include <string>

#include "atom/hf.h"
#include "atom/levels.h"
#include "atom/outline.h"

namespace atom {

namespace {
// the scene a CPU-comparison scene repeats: 20 and 21 are 5 and 7 (stage 1), 22 to 25 are 8, 10, 11, 12 (stage 2); every other scene is itself
int baseScene(int which) {
    if (which == kBenchWheelCpuScene) return 5;
    if (which == kBenchWheel2DCpuScene) return 7;
    if (which == kBenchJumpCpuScene) return 8;
    if (which >= kBenchSuperCpuFirst && which < kBenchOutlineCpuFirst) return 10 + (which - kBenchSuperCpuFirst);
    if (which >= kBenchOutlineCpuFirst && which < kBenchSceneCount) return 13 + (which - kBenchOutlineCpuFirst);
    if (which >= kBenchChartCpuFirst && which < kBenchFullZoomScene) return 16 + (which - kBenchChartCpuFirst);   // 28, 29: scenes 19 and 20 (which 16, 17)
    if (which == kBenchGlowAllCpuScene) return kBenchGlowAllScene;
    return which;
}
void fresh(AtomLab& L, int Z, int N, double w, double h) {
    seedRandom(20260101);                                // the same random numbers every run
    L.p = AtomParams::appDefaults();                                // the Atom Lab's own defaults (what "Defaults" gives): no setting of the user's can leak in (Orbit style Bohr 1913 hid the nucleons)
    L.p.elec = "Hydrogen-like";                          // 0.46: the default became All; the old scenes are one electron, so their numbers stay comparable (the new cloud scenes set All)
    L.p.hrel = "Non-relativistic";                       // 0.49.3: the old scenes are the non-relativistic one-electron cloud (Auto would draw uranium as a Dirac cloud), so their numbers stay comparable
    L.p.super = "Off";                                   // no superposition in the benchmark (its dots would be thinned and coloured every frame): the old numbers stay comparable
    L.p.outline = "Off";                                 // the 90 % outline is never drawn in the benchmark, so the old numbers stay comparable
    L.moreStored.reset();                                // the ⓘ More / Less choice changes the readout in the picture
    L.chain.clear(); L.paused = false; L.hold = false;
    L.gpuColours = true;                                 // the default (0.13 stage 1); the scenes that compare with the CPU way switch it off themselves
    L.gpuOutline = true;                                 // the default (0.13 stage 3: the outline's grid and the Slice's contour values from compute shaders, where the PC has OpenGL 4.3)
    L.gpuChart = true;                                   // the default (0.13 stage 4 part B: the chart's tiles and blocks in a GPU buffer)
    L.gpuCloud = true;                                   // the default (0.46: the Glow cloud from a GPU buffer)
    L.gpuCompute = true;                                 // the default (0.13 stage 2: the superposition and jump colour groups from a compute shader, where the PC has OpenGL 4.3)
    L.viewW = w; L.viewH = h;
    L.setParam("Z", double(Z)); L.setParam("N", double(N));
    L.restart();
    L.s.nucSeed = 1;
    L.s.spin = false;
}
void nucleus(AtomLab& L, const char* qstyle) {
    L.setParam("view", std::string("Orbit")); L.setParam("ostyle", std::string("Modern")); L.setParam("qstyle", std::string(qstyle)); L.setParam("detail", std::string("+ Sea")); L.setParam("style", std::string("Nucleons"));
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

int heaviestAll() {
    static const int z = [] { for (int Z = 118; Z > 1; Z--) if (hf::find(Z, 0, true)) return Z; return 2; }();
    return z;
}
int heaviestLevelAtom() {
    static const int z = [] { for (int Z = 118; Z > 1; Z--) if (levelJumps(Z)) return Z; return 2; }();
    return z;
}
double benchZoom(int which) {
    // 0.56.1: chosen from four zooms tried in the lab's own range (REPORT 0.56.1): the heaviest a user reaches with the cloud filling the picture
    switch (which) {
        case kBenchAllEnlarged: return 0.5;        // tried 0, 0.5, 1.0, 1.5: 0 leaves the cloud a third of the picture; 0.5 fills it and is the heaviest
        case kBenchAllTrueZoomed: return 2.5;      // tried 0.8 to 4.0: below 2.5 the inner shells are a dot; from 3.0 the view passes into the nucleus
        case kBenchAllOneOrbital: return 0.3;      // tried 0, 0.3, 0.6, 0.9
        case kBenchAllClassic: return 0.5;         // tried 0, 0.5, 1.0, 1.5: 0 does not fill the picture
        case kBenchJumpAll: return 0.0;            // tried 0, 0.3, 0.6, 0.9 (Shells enlarged: at True scale the cloud is a dot at all four)
        case kBenchNucleusDensity: return 0.3;     // relative to zoomForNucleus(); tried 0, 0.3, 0.6, 0.9: from 0.6 the Density look gives way to the nucleons
        case kBenchSliceAll: return -0.5;          // 0.64.0: each tried at -0.5, 0, 0.5, 1.0 (1 % low here, REPORT 0.64.0): the slowest kept
        case kBenchSliceDirac: return 0.0;
        case kBenchSideAll: case kBenchSideAllClassic: return -0.5;   // (zoomed from there to 0 and back while it runs: benchFrame)
        case kBenchSliceGlowUltra: case kBenchSliceClassicUltra: return -0.5;
        case kBenchSliceSuper: return 0.0;         // 0.63.0: tried -0.5, 0, 0.5, 1.0, 1.5 (1 % low 2.70, 2.70, 2.95, 3.35, 2.86 fps here): 0 and -0.5 the slowest
        default: return 0.0;
    }
}

namespace {
// One orbital keeps the same number of dots whichever orbital it is (cloudCap), and the view fits the orbital, so every one has all its dots on screen: the scene
// takes the orbital of the lab's list (1s .. 4f) with the most lobes that the atom fills (the largest l, then the largest n), the most work per dot for phase colours
std::string oneOrbitalPick(int Z) {
    const hf::Atom* a = hf::find(Z, 0, true);
    std::string best = "1s"; int bl = -1, bn = -1;
    for (const Orbital& o : orbitals()) {
        const int i = a ? hf::subshellIndex(*a, o.n, o.l) : -1;
        if (i < 0 || a->sub[std::size_t(i)].occ <= 0) continue;
        if (o.l > bl || (o.l == bl && o.n > bn)) { bl = o.l; bn = o.n; best = o.shell; }
    }
    return best;
}
void fill(AtomLab& L, int most = 2000000) { for (int i = 0; i < most && L.s.k < L.cloudCap(); i++) L.step(1.0 / 240); }
// the every-electron scenes of 0.56.1
void manyScene(AtomLab& L, int which, double w, double h) {
    const int Z = which == kBenchJumpAll ? heaviestLevelAtom() : heaviestAll();
    fresh(L, Z, defaultN(Z), w, h);
    L.p.elec = "All"; L.p.mrel = "Relativistic"; L.p.show = "Whole atom"; L.p.cdens = "Ultra"; L.p.cstyle = "Glow";
    L.p.rscale = which == kBenchAllTrueZoomed ? "True scale" : "Shells enlarged";   // (the jump's cloud at True scale is a dot of a few pixels at every zoom tried: REPORT 0.56.1)
    if (which == kBenchAllClassic) { L.p.cstyle = "Classic"; L.p.phase = "Wheel"; }
    if (which == kBenchAllOneOrbital) { L.p.show = "One orbital"; L.p.orb = oneOrbitalPick(Z); }
    if (which == kBenchJumpAll) {                       // the atom put in the upper level of its first line on offer; the jump starts in benchFrame
        const std::vector<JLine>& ln = jumpLines(Z);
        for (int k = 0; k < int(ln.size()); k++) if (jumpOffered(Z, k)) { L.s.lvl = ln[std::size_t(k)].hi; break; }
    }
    L.setParam("view", std::string("Cloud 3D"));
    L.setParam("zoom", benchZoom(which));
    L.resetView();
    L.setParam("zoom", benchZoom(which));
    fill(L);
}
}  // namespace

void benchSetup(AtomLab& L, int which, double w, double h) {
    if (which >= kBenchAllEnlarged && which <= kBenchJumpAll) { manyScene(L, which, w, h); still(L); return; }
    if (which >= kBenchSliceAll && which <= kBenchSliceClassicUltra) {   // 0.64.0: the heavy views the benchmark missed (atom_bench.h)
        const bool many = which == kBenchSliceAll || which == kBenchSideAll || which == kBenchSideAllClassic;
        const int Z = many ? heaviestAll() : 92;
        fresh(L, Z, many ? defaultN(Z) : 146, w, h);
        if (many) { L.p.elec = "All"; L.p.mrel = "Relativistic"; L.p.show = "Whole atom"; L.p.rscale = "Shells enlarged"; }
        else L.p.orb = "4f";
        if (which == kBenchSliceDirac) L.p.hrel = "Dirac";
        L.p.cdens = "Ultra";
        L.p.cstyle = which == kBenchSideAll || which == kBenchSliceGlowUltra ? "Glow" : "Classic";
        L.setParam("view", std::string(which == kBenchSideAll || which == kBenchSideAllClassic ? "Cloud" : "Slice"));
        L.resetView();
        L.setParam("zoom", benchZoom(which));
        fill(L);
        still(L);
        return;
    }
    if (which == kBenchSliceSuper) {                    // 0.63.0: the owner's view at 5 fps (the Slice of a superposition is the processor's work on both APIs)
        fresh(L, 101, 157, w, h);                        // mendelevium-258
        L.p.cstyle = "Classic"; L.p.cdens = "Ultra";
        L.setParam("dirac", std::string("On")); L.setParam("phase", std::string("Wheel"));
        L.setParam("view", std::string("Slice")); L.setParam("orb", std::string("2p")); L.setParam("orb2", std::string("5f"));
        L.setParam("mix", 50.0); L.setParam("super", std::string("On"));
        L.resetView();
        L.setParam("zoom", benchZoom(which));
        for (int i = 0; i < 40000 && L.s.k < L.cloudCap(); i++) L.step(1.0 / 240);
        still(L);
        return;
    }
    if (which == kBenchNucleusDensity) {                // 0.56.1: the biggest nucleus in the Density look (the 0.50.3 review suspected its grid is made again while nothing changes)
        fresh(L, 118, 176, w, h);
        L.setParam("view", std::string("Orbit")); L.setParam("ostyle", std::string("Modern")); L.setParam("style", std::string("Density"));
        L.ensureNucleus();
        L.setParam("zoom", L.zoomForNucleus() + benchZoom(which));
        still(L);
        return;
    }
    const int base0 = baseScene(which);
    if (base0 == kBenchChartScene || base0 == kBenchLandscapeScene || base0 == kBenchBindingScene || base0 == kBenchChainScene) {   // the chart of nuclides: the whole map (flat) or the landscape (full tilt), U-238 chosen (the marker is drawn), nothing to wait for
        fresh(L, 92, 146, w, h);
        L.chartReset();
        L.gpuChart = which < kBenchChartCpuFirst;        // scenes 33 and 34 (which 28, 29) are scenes 19 and 20 with the chart drawn the old way
        L.setParam("view", std::string(kChartView));
        L.s.chartView.height = which == kBenchBindingScene ? ChartHeightMode::Binding : ChartHeightMode::HalfLife;   // (the scene says what the height means: no choice of the user's leaks in)
        if (base0 != kBenchChartScene) L.setChartTilt(1);
        L.s.showChain = base0 == kBenchChainScene;       // the three older chart scenes draw no chain, so they cost what they cost in 0.11; the benchmark's scene 22 (which 19) draws it
        if (base0 == kBenchChainScene) {                 // the chain of uranium-238, walked five steps with a fixed seed (the same trail and the same walker every run); the walk does not move by itself
            seedRandom(20241004);
            L.walkRestart();
            for (int i = 0; i < 5; i++) L.walkOnce();
            L.s.walkAuto = false;
        }
        still(L);
        return;
    }
    if (base0 == kBenchGlowAllScene) {                   // 0.46: the whole uranium atom, every electron, Glow, Ultra, True scale; which 32: the same with the CPU building every dot (GPU cloud Off)
        fresh(L, 92, 146, w, h);
        L.gpuCloud = which == kBenchGlowAllScene;
        L.p.elec = "All"; L.p.show = "Whole atom"; L.p.rscale = "True scale"; L.p.cstyle = "Glow"; L.p.cdens = "Ultra";
        L.setParam("view", std::string("Cloud 3D")); L.setParam("zoom", 0.0);
        L.resetView();
        for (int i = 0; i < 400000 && L.s.k < L.cloudCap(); i++) L.step(1.0 / 240);
        still(L);
        return;
    }
    if (which == kBenchFullZoomScene) {                  // 0.16 stage 3: the owner's slow case, the nucleus at the largest zoom with the camera turning (benchFrame turns it)
        fresh(L, 103, 163, w, h);                        // lawrencium-266
        nucleus(L, "Accurate");
        L.setParam("zoom", 7.0);                         // the largest zoom (x10,000,000; AtomLab::setParam clamps the zoom to kZoomMin..7)
        still(L);
        return;
    }
    if (which >= kBenchFirstRisk) {                      // the speed-risk scenes: U-238 like the benchmark's scene 3 (scene 18, which 15: oganesson-294), every setting named, nothing animated by itself
        const bool cpuColours = which == kBenchWheelCpuScene || which == kBenchWheel2DCpuScene;   // scenes 23 and 24 are scene 8 and scene 10 again with the 0.12 way of colouring
        const bool cpuCompute = which >= kBenchJumpCpuScene && which < kBenchOutlineCpuFirst;
        const bool cpuOutline = which >= kBenchOutlineCpuFirst && which < kBenchChartCpuFirst;               // scenes 29 and 30 are scenes 16 and 17 again with the CPU making the outline (0.13 stage 3)                  // scenes 25 to 28 are scenes 11, 13, 14, 15 again with the CPU working out the colour groups (0.13 stage 2)
        const int sc = baseScene(which);
        const bool og = sc == 15;
        fresh(L, og ? 118 : 92, og ? 176 : 146, w, h);
        L.gpuColours = !cpuColours;
        L.gpuCompute = !cpuCompute;
        L.gpuOutline = !cpuOutline;
        if (sc <= 7) { L.setParam("phase", std::string(sc == 6 ? "Sign" : "Wheel")); cloud3D(L, "Classic", "Ultra", sc == 7 ? "Cloud" : "Cloud 3D"); }
        else if (sc <= 9) cloud3D(L, "Classic", "Ultra");                        // the jump itself starts in benchFrame
        else if (sc == 10) superScene(L, "2p", "z", "2p", "x");
        else if (sc == 11) superScene(L, "2p", "x", "2p", "m+1");
        else if (sc == 12) superScene(L, "2p", "z", "3d", "z2");
        else if (sc == 13) { cloud3D(L, "Classic", "Normal"); clearOutlineCache(); }   // the outline is switched on at the first counted frame, in benchFrame
        else if (sc == 14) { L.setParam("outline", std::string("On")); cloud3D(L, "Classic", "Normal", "Slice"); }
        else { nucleus(L, "Accurate"); }
        still(L);
        return;
    }
    if (which == 4) {                                    // stress: the same cloud as the benchmark's scene 3 (which 1), drawn as Glow with 48,000 dots
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
    if (which == 0) { nucleus(L, "Accurate"); still(L); }
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
        {"Risk 10: phase colours (Wheel), Classic dots, Cloud 3D, 48,000 dots", "uranium-238, Cloud 3D, 4f, cloud style Classic, density Ultra (48,000 dots), Phase colours Wheel (24 colours). Up to 0.12 the drawing went over all the dots once for each colour, in each of three depth bands (72 passes). From 0.13 each dot carries its colour number and the graphics card picks the colour (GPU colours On, the default). Scene 23 is this scene with the old way (GPU colours Off). Fixed camera"},
        {"Risk 10 baseline: two colours (Sign), Classic dots, Cloud 3D, 48,000 dots", "the same as the scene before with Phase colours Sign (2 colours). The difference between the two was the cost of the 24 colours up to 0.12. From 0.13 both run with GPU colours On (the two Sign colours are picked on the graphics card too)"},
        {"Risk 10: phase colours (Wheel), Classic dots, Cloud 2D (side view), 48,000 dots", "uranium-238, Cloud (the 2D side view), 4f, Classic, Ultra, Phase colours Wheel: the flat view. Up to 0.12 the same colour-by-colour scan; from 0.13 GPU colours On. Scene 24 is this scene with the old way (GPU colours Off)"},
        {"Risk 11: cloud jump in flight (held half way), Cloud 3D, Classic dots, 48,000 dots", "uranium-238, Cloud 3D, 4f, Classic, Ultra: an emitted jump to a lower orbital is started and then held half way for the whole run, so every frame draws a jump in flight (the sum over two sets of 48,000 dots). From 0.13 stage 2, on a PC with Vulkan or OpenGL 4.3, the colour group of every dot (shown or not, and its colour) is worked out by a compute shader instead of a loop on the processor (the report says 'Vulkan: GPU superposition on' or 'OpenGL 4.3: GPU superposition on', or 'CPU path' on a PC without either); scene 25 is this scene with the old CPU way."},
        {"Risk 11: cloud jumps one after another, Cloud 3D, Classic dots, 48,000 dots", "as the scene before, but the jump is let go: emit, then absorb back to 4f, again and again (the emitted photon is dropped). Each start builds 48,000 new dots in one frame: look at the worst frame. From 0.13 stage 2, on a PC with Vulkan or OpenGL 4.3, the colour group of every dot (shown or not, and its colour) is worked out by a compute shader instead of a loop on the processor (the report says 'Vulkan: GPU superposition on' or 'OpenGL 4.3: GPU superposition on', or 'CPU path' on a PC without either)."},
        {"Risk 11: superposition, orthogonal pair 2p z + 2p x (one energy), Cloud 3D, 48,000 dots", "uranium-238, Cloud 3D, Classic, Ultra, Superposition On, 2p (z) with 2p (x), 50 % each: the two real p orbitals are orthogonal, so the dots are thinned every frame. From 0.13 stage 2, on a PC with Vulkan or OpenGL 4.3, the colour group of every dot (shown or not, and its colour) is worked out by a compute shader instead of a loop on the processor (the report says 'Vulkan: GPU superposition on' or 'OpenGL 4.3: GPU superposition on', or 'CPU path' on a PC without either); scene 26 is this scene with the old CPU way."},
        {"Risk 11: superposition, overlapping pair 2p x + 2p m+1 (one energy), Cloud 3D, 48,000 dots", "the same with 2p (x) and the complex 2p (m = +1): these overlap, so the dots are sampled from the combined density (stage 4 of 0.10.6) and nothing is thinned. From 0.13 stage 2, on a PC with Vulkan or OpenGL 4.3, the colour group of every dot (shown or not, and its colour) is worked out by a compute shader instead of a loop on the processor (the report says 'Vulkan: GPU superposition on' or 'OpenGL 4.3: GPU superposition on', or 'CPU path' on a PC without either); scene 27 is this scene with the old CPU way."},
        {"Risk 11: superposition, orthogonal pair 2p z + 3d z2 (two energies, it beats), Cloud 3D, 48,000 dots", "2p (z) with 3d (z2), 50 % each: orthogonal, and the energies differ, so the density beats and every dot's colour and thinning change with the beat phase. From 0.13 stage 2, on a PC with Vulkan or OpenGL 4.3, the colour group of every dot (shown or not, and its colour) is worked out by a compute shader instead of a loop on the processor (the report says 'Vulkan: GPU superposition on' or 'OpenGL 4.3: GPU superposition on', or 'CPU path' on a PC without either); scene 28 is this scene with the old CPU way."},
        {"Risk 12: first use of an outline (cache emptied every 120 frames), Cloud 3D, 4f, 12,000 dots", "uranium-238, Cloud 3D, 4f, Classic, Normal (scene 3 of the benchmark). The outline is switched on at the first counted frame and the outline cache is emptied every 120 frames after that, so the 64 x 64 x 64 density pass runs again each time: look at the worst frame. From 0.13 stage 3, on a PC with Vulkan or OpenGL 4.3, the 64 x 64 x 64 density pass and the 90 % level are made by compute shaders on the graphics card and the picture does not wait for them: the outline appears a frame or two later, so the worst frame should lose its hitch (about 20 ms on the PC of the first measurement); the report says 'Vulkan: GPU outline on' or 'OpenGL 4.3: GPU outline on', or 'CPU path' on a PC without either. Scene 29 is this scene with the old CPU way."},
        {"Risk 12: outline with the cut dragged, Slice view, 4f, 12,000 dots", "uranium-238, Slice view, 4f, Classic, Normal, 90 % outline On: the cut moves 0.01 every frame between -0.90 and +0.90 (as a drag does), so the outline's slice (40,000 density values) is redone every frame. From 0.37 the dots lie on the cut plane, so each move also makes new dots on it (at most 1,500 a frame, replacing the oldest). From 0.13 stage 3, on a PC with Vulkan or OpenGL 4.3, the cut's 40,000 density values are made by a compute shader without the picture waiting (the contour is a frame behind the cut while it moves); scene 30 is this scene with the old CPU way (about 1.7 of its 2 ms was the density values on the CPU of the machine where this was measured)"},
        {"Risk 12: nucleus view, oganesson-294 (294 nucleons), whole nucleus, Accurate quarks, + Sea detail", "oganesson-294 (118 protons, 176 neutrons), the whole nucleus in view, Accurate quarks, + Sea detail, Nucleons style (scene 2 of the benchmark with the biggest nucleus)"},
        {"Chart of nuclides: the whole chart, 3,557 squares", "the Atom Lab's Chart of nuclides view (0.11 stage 2): every nuclide of the lab's table as one square (3,557), the 14 magic-number bands, the legend and the text box, the chosen nuclide (uranium-238) marked. "
                                                              "Nothing moves. Keep the pointer off the picture while it runs (a hovered square changes the text box). Compare it with scene 2 (the Atom Lab's own Orbit view of uranium-238, drawn by the same frame code) and look at the GPU time: "
                                                              "this scene is all rectangles in one batch, so the CPU time to build them is what it measures"},
        {"Chart of nuclides, tilted landscape (full tilt), 3,557 blocks", "the Chart of nuclides view at full tilt (0.11 stage 3): every nuclide a block whose height is the log of its half-life (stable ones on a plateau), drawn far to near with its top and two faces, "
                                                                          "the ground and the magic-number bands, the legend, uranium-238 chosen. Nothing moves. It is the same data as the flat chart scene just before it, so the DIFFERENCE between the two is the cost of the third dimension: "
                                                                          "about three times the triangles, and the per-frame loop over the blocks. Look at the CPU frame time first (the GPU is expected near idle); keep the pointer off the picture"},
        {"Chart of nuclides, tilted landscape, BINDING-ENERGY height (full tilt), 3,557 blocks", "the same view as the scene before it (the tilted landscape, half-life height) with the height meaning the binding energy per nucleon (0.11 stage 4): the same 3,557 blocks drawn the same way, "
                                                                          "but 2,549 solid blocks (the measured values) and 1,008 flat pale squares (estimates only), and the peak note. The three chart scenes are flat, tilted with half-life height and tilted with binding height, "
                                                                          "so the DIFFERENCE between the last two is only the other height table (built once) and the notes: it should be about nothing. Look at the CPU frame time; keep the pointer off the picture"},
        {"Chart of nuclides, tilted landscape with the DECAY CHAIN of uranium-238 drawn and walked, 3,557 blocks", "the same view as the tilted landscape with half-life height (0.12 stage 1) with the decay chain of the chosen nuclide, uranium-238, drawn over it: 43 trail segments between 28 nuclides (the likeliest way thick, the others thinner or faint) with their mode labels, "
                                                                          "and the walker after five steps of a walk with a fixed random seed (its path, the squares it has been through and its disc). The chain is built once; the walk is not advancing during the scene. Scenes 19 to 21 draw no chain. "
                                                                          "Compare this scene with the tilted-landscape scene before it: the DIFFERENCE is the cost of the trail, the labels and the walker. Keep the pointer off the picture while it runs."},
        {"Phase colours (Wheel), Cloud 3D, 48,000 dots, GPU colours OFF (the 0.12 way, to compare with scene 8)", "exactly scene 8 (uranium-238, Cloud 3D, 4f, Classic, Ultra, Phase colours Wheel, the same dots and camera) with GPU colours Off: the CPU goes over all the dots once for each colour in each of three depth bands, as 0.12 did, and every dot carries its colour. "
                                                   "The picture is the same; the DIFFERENCE between this scene and scene 8 is what moving the colours to the GPU is worth on this PC"},
        {"Phase colours (Wheel), Cloud 2D (side view), 48,000 dots, GPU colours OFF (the 0.12 way, to compare with scene 10)", "exactly scene 10 (uranium-238, Cloud, 4f, Classic, Ultra, Wheel) with GPU colours Off: the 0.12 colour-by-colour scan. The DIFFERENCE between this scene and scene 10 is what moving the colours to the GPU is worth on this PC"},
        {"Cloud jump in flight (held half way), Cloud 3D, 48,000 dots, GPU superposition OFF (the CPU path, to compare with scene 11)", "exactly scene 11 (uranium-238, Cloud 3D, 4f, Classic, Ultra, a jump held half way) with GPU superposition Off: the processor works out every dot's colour group, as before 0.13 stage 2. The DIFFERENCE between this scene and scene 11 is what moving the jump's per-dot work to the graphics card is worth on this PC; on a PC without Vulkan or OpenGL 4.3 the two scenes run the same way"},
        {"Superposition 2p z + 2p x (orthogonal), Cloud 3D, 48,000 dots, GPU superposition OFF (the CPU path, to compare with scene 13)", "exactly scene 13 with GPU superposition Off: the processor thins and colours every dot every frame. The DIFFERENCE between this scene and scene 13 is what the compute shader is worth on this PC"},
        {"Superposition 2p x + 2p m+1 (overlapping), Cloud 3D, 48,000 dots, GPU superposition OFF (the CPU path, to compare with scene 14)", "exactly scene 14 (the slowest scene of 0.12) with GPU superposition Off. The DIFFERENCE between this scene and scene 14 is what the compute shader is worth on this PC"},
        {"Superposition 2p z + 3d z2 (beats), Cloud 3D, 48,000 dots, GPU superposition OFF (the CPU path, to compare with scene 15)", "exactly scene 15 with GPU superposition Off: the processor works out every dot's phase, odds and colour every frame. The DIFFERENCE between this scene and scene 15 is what the compute shader is worth on this PC"},
        {"First use of an outline (cache emptied every 120 frames), Cloud 3D, 4f, 12,000 dots, GPU outline OFF (the CPU path, to compare with scene 16)", "exactly scene 16 with GPU outline Off: the processor makes the outline's 64 x 64 x 64 density grid and its 90 % level on the frame that first needs them (about 30 ms on the machine where this was measured: 13 ms for the density, 17 ms to sort the cells), as before 0.13 stage 3. The DIFFERENCE in the worst frame between this scene and scene 16 is what the compute shaders are worth on this PC; on a PC without Vulkan or OpenGL 4.3 the two scenes run the same way"},
        {"Outline with the cut dragged, Slice view, 4f, 12,000 dots, GPU outline OFF (the CPU path, to compare with scene 17)", "exactly scene 17 with GPU outline Off: the processor makes the cut's 40,000 density values every frame. The DIFFERENCE between this scene and scene 17 is what the compute shader is worth on this PC"},
        {"The chart of nuclides, flat, GPU chart cache OFF (the 0.13 stage 3 way, to compare with scene 19)", "exactly scene 19 (the whole chart, flat, uranium-238 chosen, nothing hovered) with the GPU chart cache Off: the 3,557 squares are turned into vertices and sent to the graphics card again every frame, as up to 0.13 stage 3. Scene 19 is the same with the squares kept in a GPU buffer. The DIFFERENCE between the two is what the cache is worth on this PC. Keep the pointer off the picture while it runs"},
        {"The chart as a landscape, full tilt, GPU chart cache OFF (the 0.13 stage 3 way, to compare with scene 20)", "exactly scene 20 (the landscape at full tilt, uranium-238 chosen) with the GPU chart cache Off: the 3,557 blocks and their sides are turned into vertices and sent again every frame, as up to 0.13 stage 3. Scene 20 is the same with the blocks kept in a GPU buffer. The DIFFERENCE between the two is what the cache is worth on this PC. Keep the pointer off the picture while it runs"},
        {"Nucleus, full zoom, camera moving: lawrencium-266, Accurate quarks, + Sea", "lawrencium-266 (103 protons, 163 neutrons), Orbit view, Accurate quarks, + Sea detail, Nucleons style, zoom at the maximum (x10,000,000: the picture is a small part of one nucleon), "
         "the camera turning 0.15 rad every frame as a fast drag does (15 points of mouse movement a frame). The owner's slow case of 0.16, which scenes 4 and 5 missed because neither goes to the maximum zoom. "
         "From 0.16 stage 3 the nucleons, blobs, rings and lines that lie wholly outside the picture are not made at all"},
        {"Whole atom, every electron, Glow, Ultra, True scale: uranium-238, GPU cloud", "uranium-238, Cloud 3D, Electrons All, Whole atom, True scale, cloud style Glow, density Ultra (240,000 dots), fixed camera. From 0.46 the dots stay in a buffer on the graphics card: only the dots that changed are sent, the graphics card turns and lights them, and the brightness grid is summed there too"},
        {"Whole atom, every electron, Glow, Ultra, True scale: uranium-238, GPU cloud OFF", "the same scene with GPU cloud Off: the processor turns, sorts and sends every dot each frame, as up to 0.45. The DIFFERENCE between the two is what the GPU cloud is worth on this PC"},
        {"Every electron, Shells enlarged, Glow, Ultra, the heaviest atom", "0.56.1: the texts are app/bench_scenes.cpp's"},
        {"Every electron, True scale zoomed in on the inner shells, Glow, Ultra", "0.56.1"},
        {"Every electron, One orbital, Ultra", "0.56.1"},
        {"Every electron, Classic with phase colours Wheel, Ultra", "0.56.1"},
        {"A jump with every electron held half way, the heaviest level atom, Ultra", "0.56.1"},
        {"The biggest nucleus in the Density look", "0.56.1"},
        {"The Slice of a superposition dragged, mendelevium-258", "0.63.0"},
        {"The Slice with every electron dragged, the heaviest atom", "0.64.0"},
        {"The Slice of a Dirac state dragged, uranium 4f", "0.64.0"},
        {"The side view with every electron, Glow", "0.64.0"},
        {"The side view with every electron, Classic", "0.64.0"},
        {"The Slice of one state dragged, Glow, Ultra", "0.64.0"},
        {"The Slice of one state dragged, Classic, Ultra", "0.64.0"},
    };
    return v[std::max(kBenchFirstRisk, std::min(which, kBenchAtomScenes - 1)) - kBenchFirstRisk];
}

void benchFrame(AtomLab& L, int which, int sceneFrame, int measureFrame) {
    if (which == kBenchJumpAll) {                        // 0.56.1: a jump between two levels held half way in flight (the emitted photon is not part of it)
        if (L.s.trans) { L.s.trans->t = std::min(L.s.trans->t, 0.5 * L.s.trans->dur); return; }
        if (L.s.photon) { L.s.photon.reset(); return; }
        const std::vector<JLine>& ln = jumpLines(L.p.Z);
        for (int k = 0; k < int(ln.size()); k++) if (jumpOffered(L.p.Z, k)) { L.emitLevelLine(k); break; }
        return;
    }
    which = baseScene(which);                            // a CPU-comparison scene does what its original does
    if (which == 8 || which == 9) {
        if (L.s.trans) { if (which == 8) L.s.trans->t = std::min(L.s.trans->t, 0.6); return; }       // 8: held half way (the jump lasts 1.2 s)
        if (L.s.photon && L.s.photon->kind == "emit") L.s.photon.reset();                               // the emitted photon is not part of the measurement
        if (L.s.photon) return;                                                                         // an absorbed photon is flying in
        const int cur = L.curLevel(), start = orbIndex("4f|z3");
        if (cur == start || which == 8) { const std::vector<int> t = L.targets(-1); if (!t.empty()) L.startJump("emit", t.front()); }
        else {
            const std::vector<int> up = L.targets(1);
            int to = up.empty() ? -1 : up.front();
            for (int k : up) if (orbitals()[k].shell == "4f") { to = k; break; }
            if (to >= 0) L.startJump("absorb", to);
        }
    } else if (which == kBenchFullZoomScene) {
        L.s.oyaw += 0.15;                                // turning as a fast drag does: 15 points of mouse movement a frame (the Orbit view turns 0.01 rad per point, AtomLab "turn")
    } else if (which == 13) {
        if (measureFrame < 0) return;
        if (measureFrame == 0) L.setParam("outline", std::string("On"));
        if (measureFrame % 120 == 0) clearOutlineCache();
    } else if (which == 14 || which == kBenchSliceSuper || which == kBenchSliceAll || which == kBenchSliceDirac || which == kBenchSliceGlowUltra || which == kBenchSliceClassicUltra) {   // (0.63.0: the superposition's Slice dragged the same way)
        const int k = sceneFrame % 360, tri = k <= 180 ? k : 360 - k;                                   // up from -0.90 to +0.90 and back, 0.01 a frame
        L.setParam("cut", std::round((-0.9 + 0.01 * tri) * 100) / 100);
    } else if (which == kBenchSideAll || which == kBenchSideAllClassic) {   // 0.64.0: the side view zoomed in and out as a user's wheel does, -0.5 to 0 and back
        const int k = sceneFrame % 200, tri = k <= 100 ? k : 200 - k;
        L.setParam("zoom", -0.5 + 0.005 * tri);
    }
}

std::string benchShortText(const AtomLab& L) {
    std::string t = std::string("Atom Lab: ") + symbol(L.p.Z) + "-" + std::to_string(L.p.Z + L.p.N) + ", " + L.p.view;
    if (L.chartOn()) return t + ", tilt " + std::to_string(int(std::lround(L.s.chartView.tilt * 100))) + " %";
    char z[32]; std::snprintf(z, sizeof z, ", zoom %.2f", L.p.zoom);
    if (L.p.view == "Orbit") return t + ", " + L.p.style + (L.p.style == "Nucleons" ? ", " + L.p.qstyle + ", " + L.p.detail : std::string()) + z;
    t += L.manyOn() ? ", every electron, " + L.p.show + (L.manySub() >= 0 ? " " + L.p.orb : std::string()) + ", " + L.p.rscale : ", one electron, " + L.p.orb;
    t += ", " + L.p.cstyle + ", " + std::to_string(L.cloudCap()) + " dots" + (L.p.phase == "Wheel" ? ", phase Wheel" : "") + z;
    if (L.p.outline == "On") t += ", outline On";
    if (L.p.super == "On") t += ", superposition " + L.p.orb + " + " + L.p.orb2;
    for (const auto& sw : {std::make_pair(L.gpuColours, "GPU colours"), std::make_pair(L.gpuCompute, "GPU superposition"), std::make_pair(L.gpuOutline, "GPU outline"), std::make_pair(L.gpuCloud, "GPU cloud")})
        if (!sw.first) t += std::string(", ") + sw.second + " Off";
    return t;
}

std::string benchSettingsText(const AtomLab& L) {
    if (L.chartOn()) return "view " + L.p.view + ", " + std::to_string(chartTiles().size()) + " squares, tilt " + std::to_string(int(std::lround(L.s.chartView.tilt * 100))) + " %, GPU chart cache " + (L.gpuChart ? "On" : "Off") + ", height " + (L.s.chartView.height == ChartHeightMode::Binding ? "binding energy per nucleon" : "half-life") + ", zoom " + std::to_string(int(std::lround(L.s.chartView.zoom * 100))) + " %, chosen Z " + std::to_string(L.p.Z) + " N " + std::to_string(L.p.N) + ", decay chain " + (L.s.showChain ? "drawn, walked " + std::to_string(L.s.walk.steps.size()) + " steps" : std::string("not drawn"));
    std::string t = "view " + L.p.view + ", Orbit style " + L.p.ostyle + ", quark style " + L.p.qstyle + ", detail " + L.p.detail + ", " + L.p.style + " nucleons";
    if (L.p.view != "Orbit") {                           // the cloud views: what the cost depends on
        t += ", orbital " + L.p.orb + ", cloud style " + L.p.cstyle + ", " + std::to_string(L.cloudCap()) + " dots, phase " + L.p.phase + ", GPU colours " + (L.gpuColours ? "On" : "Off") + ", GPU superposition " + (L.gpuCompute ? "On" : "Off") + ", GPU outline " + (L.gpuOutline ? "On" : "Off") + ", electrons " + L.p.elec + (L.manyOn() ? ", " + L.p.show + ", " + L.p.rscale : std::string()) + ", GPU cloud " + (L.gpuCloud ? "On" : "Off");   // (0.46: electrons and GPU cloud)
        if (L.p.super == "On") t += ", superposition " + L.p.orb + " + " + L.p.orb2 + " (" + std::to_string(int(L.p.mix)) + " %)";
        if (L.p.outline == "On") t += ", 90 % outline On";
    }
    return t;
}

}  // namespace atom
