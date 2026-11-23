#include "app/bench_scenes.h"

#include "app/bench.h"
#include "atom/atom_bench.h"

namespace app {

const std::vector<BenchSceneText>& benchSceneTexts() {
    static const std::vector<BenchSceneText> v = [] {
        std::vector<BenchSceneText> v;
        const auto add = [&](bool hard, const char* id, BenchKind kind, int which, const char* name, const char* what, const char* twin = "") {
            v.push_back({id, name, what, kind, which, twin, hard});
        };
        // ---- Hard scenes: the default run, in the order they run ----
        add(true, "orbit.heaviest", BenchKind::OrbitHeaviest, -1, "Orbit Lab, the heaviest view",
            "3D, belts and Oort cloud (6,000 rocks), every extra, the full night sky with constellation lines and names, far-away things, full orbit trails, true scale, "
            "the fastest time (1 s = about 50 years), zoom x1, Simple planet physics. The rocks' places come from the graphics card where it can (Vulkan).");
        add(true, "orbit.saturn", BenchKind::OrbitSaturn, -1, "Orbit Lab, Saturn close up, the camera turning",
            "Saturn followed, true scale, 3D, zoom 4.1 (log10, the control goes to 5.5; the rings and part of the planet fill the picture), the camera turning 0.5 rad a "
            "second and tilting, time stopped. The rings from Cassini's profile, per pixel on the graphics card (Vulkan) once its check passed; the planet's map and its shadow on the rings.");
        add(true, "orbit.earth", BenchKind::OrbitEarth, -1, "Orbit Lab, Earth close up with its air and the Moon's shadow, the camera turning",
            "12 Aug 2026 17:46 UTC: the app's own Moon (ELP2000-82B) puts its shadow axis 0.90 Earth radii from Earth's centre (found with the app's geometry, app_tests "
            "checks it). Earth followed, true scale, 3D, the closest zoom (5.5 asked; 0.75.0: the camera stays 1.5 Earth radii from its middle, zoom 5.14), the camera aimed at the shadow over Greenland and turning to and fro, time stopped.");
        add(true, "orbit.real_fast", BenchKind::OrbitRealFast, -1, "Orbit Lab, Real planet physics with relativity at the fastest time",
            "Planet physics Real (the n-body integration, general relativity 1PN on), moons on, 3D, every extra, zoom x1, the fastest time (1 s = about 50 years): "
            "the integrator walks about 50 years a second and makes its stretches ahead on a thread of its own.");
        add(true, "orbit.sky", BenchKind::OrbitSky, -1, "Orbit Lab, the full night sky with constellations, the camera turning",
            "3D, Stars Full (the Gaia map and 2,874 catalogue stars), constellation lines and names, the extras off so the sky is the picture (it fills the picture "
            "at every zoom; zoom 0, x1), the camera turning 0.5 rad a second and tilting, time stopped.");
        add(true, "orbit.journey", BenchKind::OrbitJourney, -1, "Orbit Lab, the Journey tour through all its scales",
            "The Journey (Earth, the Moon, Earth's path, the planets, Voyager 1, the Oort cloud, Alpha Centauri, the Pleiades, the galactic centre: zoom 4.6 down to -9) "
            "run faster so the whole trip fits the counted time; every zoom from the start to the end is drawn.");
        add(true, "atom.nucleus_zoom", BenchKind::Atom, atom::kBenchFullZoomScene, "Atom Lab, the nucleus at full zoom, the camera turning",
            "lawrencium-266, Orbit view, Accurate quarks, + Sea, Nucleons, the largest zoom (x10,000,000; log10 7), the camera turning 0.15 rad every frame as a fast drag does.");
        add(true, "atom.nucleon", BenchKind::Atom, 2, "Atom Lab, one nucleon",
            "uranium-238, Accurate quarks, + Sea detail, zoomed all the way into the first nucleon: its quarks and the sea, drawn every frame.");
        add(true, "atom.slice_drag", BenchKind::Atom, 14, "Atom Lab, the Slice view with the cut dragged",
            "uranium-238, Slice view, 4f, Classic, 12,000 dots, 90 % outline On: the cut moves 0.01 every frame from -0.90 to +0.90 and back, so the cut's dots and the "
            "outline's contour are made again every frame (the contour on the graphics card where it can (Vulkan): new pictures are the contours that came back).");
        add(true, "atom.slice_super_drag", BenchKind::Atom, atom::kBenchSliceSuper, "Atom Lab, the Slice of a superposition with the cut dragged",
            "mendelevium-258 (one electron), Slice view, the superposition 2p + 5f (50 %), Relativity (Dirac levels) On, phase colours Wheel, Classic, Ultra: the cut moves "
            "0.01 every frame from -0.90 to +0.90 and back, as atom.slice_drag. The Slice's card program makes both states' attempts; the processor picks "
            "the state from the same random number.");
        // 0.64.0: the heavy views the benchmark missed (each tried at the zooms -0.5, 0, 0.5, 1.0 on the session's box; the slowest kept: REPORT 0.64.0)
        add(true, "atom.slice_all_drag", BenchKind::Atom, atom::kBenchSliceAll, "Atom Lab, the Slice with every electron, the cut dragged",
            "oganesson-294, every electron, Whole atom, Slice view, Classic, Ultra (240,000 dots), zoom -0.5: the cut moves 0.01 every frame as atom.slice_drag.");
        add(true, "atom.slice_dirac_drag", BenchKind::Atom, atom::kBenchSliceDirac, "Atom Lab, the Slice of a Dirac state, the cut dragged",
            "uranium-238 (one electron), 4f, Relativity Dirac, Slice view, Classic, Ultra (48,000 dots), zoom 0: the cut moves 0.01 every frame as atom.slice_drag.");
        add(true, "atom.side_all_glow", BenchKind::Atom, atom::kBenchSideAll, "Atom Lab, the side view with every electron, Glow",
            "oganesson-294, every electron, Whole atom, Shells enlarged, Cloud (the side view), Glow, Ultra (240,000 dots), zoom -0.5, zoomed in to +0.5 and back as a wheel does.");
        add(true, "atom.side_all_classic", BenchKind::Atom, atom::kBenchSideAllClassic, "Atom Lab, the side view with every electron, Classic",
            "oganesson-294, every electron, Whole atom, Shells enlarged, Cloud (the side view), Classic, Ultra (240,000 dots), zoom -0.5, zoomed in to +0.5 and back as a wheel does.");
        add(true, "atom.slice_glow_ultra", BenchKind::Atom, atom::kBenchSliceGlowUltra, "Atom Lab, the Slice of one state at Ultra, Glow, the cut dragged",
            "uranium-238, 4f, Slice view, Glow, Ultra (48,000 dots), zoom -0.5: the cut moves 0.01 every frame as atom.slice_drag.");
        add(true, "atom.slice_classic_ultra", BenchKind::Atom, atom::kBenchSliceClassicUltra, "Atom Lab, the Slice of one state at Ultra, Classic, the cut dragged",
            "uranium-238, 4f, Slice view, Classic, Ultra (48,000 dots), zoom -0.5: the cut moves 0.01 every frame as atom.slice_drag.");
        add(true, "atom.all_enlarged", BenchKind::Atom, atom::kBenchAllEnlarged, "Atom Lab, every electron, Shells enlarged, Glow, Ultra, the camera turning",
            "The heaviest atom with every electron (oganesson-294, 118 electrons; its decay clock held), Cloud 3D, Whole atom, Radius Shells enlarged, Glow, Cloud density "
            "Ultra (240,000 dots: 48,000 per electron, at most 240,000), zoom 0.5 (log10; the cloud fills the picture), the camera turning 0.8 rad a second.");
        add(true, "atom.all_true_zoomed", BenchKind::Atom, atom::kBenchAllTrueZoomed, "Atom Lab, every electron at True scale, zoomed in on the inner shells",
            "oganesson-294, Cloud 3D, Whole atom, True scale, Glow, Ultra (240,000 dots), zoom 2.5 (log10): the inner shells fill the picture and most dots lie "
            "beyond it, the camera turning 0.8 rad a second.");
        add(true, "atom.all_one_orbital", BenchKind::Atom, atom::kBenchAllOneOrbital, "Atom Lab, every electron, One orbital, the camera turning",
            "oganesson-294, Cloud 3D, Electrons All, Show One orbital (5f: every orbital keeps the same 48,000 dots at Ultra and the view fits it, so the one with the "
            "most lobes is taken), Glow, Ultra, zoom 0.3, the camera turning 0.8 rad a second.");
        add(true, "atom.all_classic", BenchKind::Atom, atom::kBenchAllClassic, "Atom Lab, every electron, Classic dots with phase colours Wheel",
            "oganesson-294, Cloud 3D, Whole atom, Shells enlarged, Classic dots, Ultra (240,000 dots), zoom 0.5, turning 0.8 rad a second. Phase colours Wheel is set, "
            "but the whole atom's colours are its shells (the Phase choice applies to one orbital): no phase picture exists for every electron.");
        add(true, "atom.jump_all", BenchKind::Atom, atom::kBenchJumpAll, "Atom Lab, a jump with every electron held in flight",
            "The heaviest atom with level clouds (radium-226), Cloud 3D, every electron, Shells enlarged, Glow, Ultra: a jump between two measured levels held half way "
            "(the two levels' clouds cross-faded every frame), zoom 0 (the cloud fills the picture), the camera turning 0.8 rad a second.");
        add(true, "atom.nucleus_density", BenchKind::Atom, atom::kBenchNucleusDensity, "Atom Lab, the biggest nucleus in the Density look, still and then turning",
            "oganesson-294, Orbit view, the nucleus's Density look, zoomed until a nucleon is 36 points across, just inside the Density look in any window size (0.68.1; 0.56.1-0.68.0 used a fixed zoom that left the look in a big window): the first half of the counted time "
            "standing still (is its density grid made again while nothing changes?), the second half turning 0.8 rad a second.");
        add(true, "molecule.density", BenchKind::MoleculeDensity, -1, "Molecule Lab, the Density style of the biggest molecule, the camera turning",
            "benzene (12 atoms, the most of the molecules with a density file), Density style, 100,000 dots (the most), cloud and surface both shown, the surface coloured "
            "by the potential, zoom x1.6 (the surface fills the picture), the camera turning 0.8 rad a second: the surface's triangles are sorted again as it turns.");
        add(true, "molecule.vib_density", BenchKind::MoleculeVibDensity, -1, "Molecule Lab, the vibrating density playing",
            "water (the only molecule with a vibrating density file, H2O_vib.pld, 1.7 MB), Density style, its first mode playing, 100,000 dots, cloud and surface, "
            "zoom x1.6: the density is blended and its surface made again for every new phase of the swing.");
        add(true, "molecule.spacefill_vib", BenchKind::MoleculeSpaceVib, -1, "Molecule Lab, Space-filling with a vibration, the camera turning",
            "benzene (12 atoms, the most of the lab's molecules), Space-filling, zoom x4 (its atoms fill the picture), its first vibration playing (3 times bigger), the "
            "camera turning 0.8 rad a second.");
        add(true, "blackhole.real_move", BenchKind::BlackHole, -1, "Black Hole Lab, the spinning hole's sky and disc while the view moves",
            "A feeding hole spinning at 0.9, seen from 20 GM/c\xC2\xB2, 10\xC2\xB0 above its disc, a 90\xC2\xB0 view, our sky, Real look with the glare: the camera turns, tilts "
            "and comes closer (0.65 s each, with rests), then the same at spin 0 going back. The sky and disc per pixel on the graphics card where its check passed.");
        add(true, "blackhole.film_tilt", BenchKind::BlackHoleFilmTilt, -1, "Black Hole Lab, the Film look tilted through the disc's plane, then at rest",
            "The Film look at the paper's view (74.1 GM/c\xC2\xB2, 86.6\xC2\xB0 from the spin axis) with a 30\xC2\xB0 view (the disc fills it): the camera tilts 6\xC2\xB0 down through the "
            "disc's plane and back, three times, then rests. The picture at rest is compared with the last moving one: the pixels that change between disc and not-disc and the largest patch.");
        add(true, "blackhole.extreme", BenchKind::BlackHoleExtreme, -1, "Black Hole Lab, the top spin, the closest distance and the widest view",
            "A feeding hole, Real look: the spin dragged from 0 to 0.99 (the control's top) and back, then at 0.99 the camera at 2.2 GM/c\xC2\xB2 (the closest the control allows) "
            "with a 360\xC2\xB0 view (the widest), turning and tilting.");
        add(true, "blackhole.all_light", BenchKind::BlackHoleAllLight, -1, "Black Hole Lab, Light: All light on the spinning hole while the view moves",
            "A feeding hole spinning at 0.9 with its disc out to 2000 GM/c\xC2\xB2, Real look, Light: All light, from 20 GM/c\xC2\xB2 with a 90\xC2\xB0 view (the hardest of "
            "five views tried, REPORT 0.58.0): the camera turns round the axis and tilts through the disc's plane and back, as a drag does. The sky and disc per pixel on "
            "the graphics card where its check passed.");
        add(true, "blackhole.moving_gas", BenchKind::BlackHoleMovingGas, -1, "Black Hole Lab, the moving gas at the fastest rate while the view turns",
            "A feeding hole spinning at 0.9, Real look, the moving gas on (the clump and the patches, the light's travel time per pixel) at the fastest rate (10^7), "
            "from 10 GM/c\xC2\xB2 with a 60\xC2\xB0 view, 10\xC2\xB0 above the disc (the hardest of five views tried, 2.2 to 20 GM/c\xC2\xB2): the camera turns slowly "
            "round the axis. A new picture every frame; the sky and disc per pixel on the graphics card where its check passed.");
        add(true, "ui.guide_scroll", BenchKind::UiGuideScroll, -1, "Pages, the longest Learn guide scrolled",
            "The longest Learn guide (the Orbit Lab's timeline: 7,157 px of scrolling at 1280 x 800) open in the reading view over its lab, scrolled from top to bottom "
            "at a steady rate in the counted time (and back up in the warm-up).");
        add(true, "ui.data_plot", BenchKind::UiDataPlot, -1, "Panels, the Data panel's largest table plotted with a fit",
            "The Data panel open with the largest table the labs offer (the Atom Lab's binding energy per nucleon, 2,549 measured nuclides) plotted with a fit (its first "
            "worked analysis with one, else a straight line) and its band, over the Atom Lab turning 0.8 rad a second.");
        add(true, "ui.settings_open", BenchKind::UiSettingsOpen, -1, "Panels, Settings over the heaviest Orbit view, Pick a body open and scrolled",
            "The heaviest Orbit view (orbit.heaviest) with the Settings page open over it, and the Controls panel's Pick a body list open and scrolled down and up.");
        add(true, "orbit.alphacen", BenchKind::OrbitAlphaCen, -1, "Orbit Lab, Alpha Centauri from the whole system to Proxima's planets, turning",
            "System Alpha Centauri, 3D, Stars Full with constellation lines and names, orbit paths, Proxima followed, the fastest time: zoom -3.6 (the whole system) "
            "through Proxima's planets to 4.0 (Proxima fills the picture) and back every 8 s, the camera turning 0.5 rad a second and tilting. The orbits are the card's lines where its check passed.");
        add(true, "orbit.flyby", BenchKind::OrbitFlyby, -1, "Orbit Lab, a fly-by of Saturn from the closest zoom, through the ring plane",
            "Saturn followed, true scale, 3D, the closest zoom (the camera 1.5 Saturn radii from its middle, zoom 4.17), every extra on, Stars Full with constellations, "
            "time stopped; the camera turning 0.5 rad a second and tilting 1 to 53 degrees above the ecliptic, through the ring plane: rings, lines and moons pass "
            "close to it and behind it (cut at the near plane, on the card).");
        add(true, "orbit.earth_clouds", BenchKind::OrbitEarthClouds, -1, "Orbit Lab, Earth close up with its clouds, the camera turning round it",
            "0.77.0: Earth followed, true scale, 3D, the closest zoom (5.5 asked, 5.14 drawn), its map, clouds (one map, the planet shader reads them from the map's alpha) "
            "and air on the graphics card, the camera turning 0.5 rad a second all round it and tilting, time stopped.");
        // ---- All scenes: the easy ones and the old processor twins ----
        add(false, "orbit.rocks8", BenchKind::OrbitRocks8, -1, "Orbit Lab, 8 times the rocks",
            "orbit.heaviest with 8 times the rocks in the belts and the Oort cloud (48,000, the same kinds of orbits, a fixed seed); the rocks do not pull on each other.");
        add(false, "atom.nucleus_whole", BenchKind::Atom, 15, "Atom Lab, the whole nucleus of oganesson-294",
            "oganesson-294, Orbit view, the whole nucleus in view, Accurate quarks, + Sea, Nucleons; nothing moves.");
        add(false, "atom.phase_wheel", BenchKind::Atom, 5, "Atom Lab, phase colours Wheel, Cloud 3D, 48,000 dots",
            "uranium-238, Cloud 3D, 4f, Classic, Ultra, Phase colours Wheel; nothing moves.");
        add(false, "atom.jump_held", BenchKind::Atom, 8, "Atom Lab, a one-electron jump held half way",
            "uranium-238 (one electron), Cloud 3D, 4f, Classic, Ultra: an emitted jump held half way.");
        add(false, "atom.superposition", BenchKind::Atom, 12, "Atom Lab, the superposition 2p z + 3d z2 (beats)",
            "uranium-238 (one electron), Cloud 3D, Classic, Ultra, 2p z with 3d z2, 50 % each: the dots' colours change every frame.");
        add(false, "atom.outline_first", BenchKind::Atom, 13, "Atom Lab, the first use of an outline",
            "uranium-238, Cloud 3D, 4f, Normal: the 90 % outline switched on with its cache emptied every 120 frames.");
        add(false, "atom.chart_chain", BenchKind::Atom, 19, "Atom Lab, the chart of nuclides tilted, with the decay chain",
            "The chart as a landscape at full tilt, 3,557 blocks, uranium-238's decay chain walked five steps; nothing moves.");
        add(false, "atom.uranium_true", BenchKind::Atom, atom::kBenchGlowAllScene, "Atom Lab, uranium's whole atom at True scale (auto-fit)",
            "uranium-238, every electron, Whole atom, True scale, Glow, Ultra (240,000 dots), auto-fit: the cloud is a dot of a few pixels (moved here in 0.56.1).");
        add(false, "atom.phase_wheel_cpu", BenchKind::Atom, 20, "Atom Lab, phase colours Wheel, GPU colours OFF", "atom.phase_wheel with the processor colouring every dot.", "atom.phase_wheel");
        add(false, "atom.jump_held_cpu", BenchKind::Atom, 22, "Atom Lab, a jump held half way, GPU superposition OFF", "atom.jump_held with the processor working out the colour groups.", "atom.jump_held");
        add(false, "atom.superposition_cpu", BenchKind::Atom, 25, "Atom Lab, the superposition, GPU superposition OFF", "atom.superposition with the processor working out the colour groups.", "atom.superposition");
        add(false, "atom.outline_first_cpu", BenchKind::Atom, 26, "Atom Lab, the first outline, GPU outline OFF", "atom.outline_first with the processor making the outline's 64\xC2\xB3 grid.", "atom.outline_first");
        add(false, "atom.slice_drag_cpu", BenchKind::Atom, 27, "Atom Lab, the Slice view dragged, GPU outline OFF", "atom.slice_drag with the processor making the cut's contour values.", "atom.slice_drag");
        add(false, "orbit.heaviest_cpu", BenchKind::OrbitHeaviestCpuRocks, -1, "Orbit Lab, the heaviest view, GPU rocks OFF", "orbit.heaviest with the processor placing the 6,000 rocks.", "orbit.heaviest");
        add(false, "orbit.rocks8_cpu", BenchKind::OrbitRocks8CpuRocks, -1, "Orbit Lab, 8 times the rocks, GPU rocks OFF", "orbit.rocks8 with the processor placing the 48,000 rocks.", "orbit.rocks8");
        add(false, "atom.uranium_true_cpu", BenchKind::Atom, atom::kBenchGlowAllCpuScene, "Atom Lab, uranium's whole atom, GPU cloud OFF", "atom.uranium_true with the processor turning, sorting and sending every dot.", "atom.uranium_true");
        return v;
    }();
    return v;
}

int benchSceneIndex(const std::string& id) {
    const std::vector<BenchSceneText>& v = benchSceneTexts();
    for (std::size_t i = 0; i < v.size(); i++) if (v[i].id == id) return int(i);
    return -1;
}

double benchPlannedS() {
    return kBenchHardScenes * (kBenchWarmupS + kBenchMeasureS) + kBenchResponseActions * kBenchResponsePlanS + kBenchIdleEntries * kBenchIdlePlanS;
}

// the default run takes about four minutes (the owner's decision in 0.56.1's job): 24 scenes of 6.5 s, 33 waits of about 1.6 s, 6 labs left alone about 4 s.
// 0.64.0: 32 scenes (the six heavy views the benchmark missed, CLAUDE.md: a heavy view goes to the Hard list), so about four and three-quarter minutes (REPORT 0.64.0)
// 0.72.0: 33 scenes (blackhole.moving_gas, CLAUDE.md: a new drawing adds its scene): 291.3 s planned, over 0.64.0's 4.85 minutes by 0.3 s; the limit is 4.9
// minutes now (nothing shortened, REPORT 0.72.0)
// 0.73.0: 34 scenes (orbit.alphacen, CLAUDE.md: a new drawing adds its scene): 297.8 s planned; the limit is 5.0 minutes now (nothing shortened, REPORT 0.73.0)
// 0.75.0: 35 scenes (orbit.flyby, CLAUDE.md: a new drawing adds its scene): 304.3 s planned; the limit is 5.1 minutes now (nothing shortened, REPORT 0.75.0)
// 0.77.0: 36 scenes (orbit.earth_clouds, CLAUDE.md: a new drawing adds its scene): 310.8 s planned; the limit is 5.2 minutes now (nothing shortened, REPORT 0.77.0)
static_assert(kBenchHardScenes * (kBenchWarmupS + kBenchMeasureS) + kBenchResponseActions * kBenchResponsePlanS + kBenchIdleEntries * kBenchIdlePlanS <= 5.2 * 60,
              "the default benchmark run is planned at about four minutes");
static_assert(kBenchHardScenes * (kBenchWarmupS + kBenchMeasureS) + kBenchResponseActions * kBenchResponsePlanS + kBenchIdleEntries * kBenchIdlePlanS >= 3.5 * 60,
              "the default benchmark run is planned at about four minutes");

}  // namespace app
