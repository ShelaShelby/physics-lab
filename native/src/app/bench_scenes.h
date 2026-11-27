// The benchmark's scene list as text (0.16 stage 4; 0.56.1: two lists and stable ids): which scenes there are, in which order, and what they test.
// No graphics in here, so the texts and the lists can be tested. How a scene is set up is in ui/benchmark.cpp and atom/atom_bench.cpp.
//
// 0.56.1 (the owner, 16 Nov 2026: keep only scenes that are really hard to simulate):
//   "Hard scenes" (the default run): only what can fall below a fast screen's rate or make the user wait; each moves as a user does (a picture that stands
//     still is drawn once and kept, and measures nothing).
//   "All scenes": the Hard ones, then the easy ones and the old processor twins. Nothing is deleted.
// Every scene has a stable short id (lab.what), printed in the report, so runs of different versions can be compared when the numbers shift. An id is never
// reused for something else; a scene that moves between the lists keeps its id.
#pragma once

#include <string>
#include <vector>

namespace app {

enum class BenchKind {
    OrbitHeaviest, OrbitRocks8, OrbitHeaviestCpuRocks, OrbitRocks8CpuRocks,      // (up to 0.56.0)
    OrbitSaturn, OrbitEarth, OrbitRealFast, OrbitSky, OrbitJourney,               // 0.56.1
    OrbitEarthClouds,                                                            // 0.77.0
    Atom,                                                                        // atom/atom_bench.h, scene `atomWhich`
    MoleculeDensity, MoleculeVibDensity, MoleculeSpaceVib,                       // 0.56.1
    BlackHole, BlackHoleFilmTilt, BlackHoleExtreme, BlackHoleAllLight, BlackHoleMovingGas, BlackHoleDiscStreaks, BlackHoleCircleSpin,                              // 0.56.1: the first is 0.55.0's scene (blackhole.real_move)
    UiGuideScroll, UiDataPlot, UiSettingsOpen,                                   // 0.56.1: the pages and panels the 0.50.3 review suspected
    OrbitAlphaCen,                                                               // 0.73.0: the Alpha Centauri system
    OrbitFlyby,                                                                  // 0.75.0: Saturn from the closest the camera may come, through the ring plane
    OrbitCometTails,                                                             // 0.78.0: Hale-Bopp at perihelion, its calculated tails
    BlackHoleDolly, BlackHoleOverPoles, BlackHoleEverything, BlackHoleEdgeOn,      // 0.83.0
    OrbitPorkchop,                                                               // 0.92.0: the Data panel's porkchop plot, Earth to Mars, the dates dragged
    OrbitSaturnRealFast, MoleculeLiveHot, UiSettingsOverBlackHole, UiResizeBlackHole,  // 0.83.0
    BlackHoleCloseSpin,                                                          // 0.84.0: the camera's own shift, ray by ray, at its strongest
    OrbitTrueLight, OrbitSunEdge, OrbitOortRegion,                               // 0.85.0: the outside review's R1-R4 (PLAN_REVIEW.md)
    OrbitTrueLightStars                                                          // 0.86.0: the stars on the camera's exposure
};

struct BenchSceneText {
    std::string id;         // stable: "orbit.saturn"
    std::string name;       // "Orbit Lab, Saturn close up, the camera turning"
    std::string what;       // what it tests today, in two or three lines (the chosen zoom, distance and field of view, so a run can be repeated by hand)
    BenchKind kind = BenchKind::Atom;
    int atomWhich = -1;     // kind Atom: the scene of atom/atom_bench.h
    std::string twinOf;     // the id of the scene this one repeats with the processor doing the work ("" not a twin)
    bool hard = false;      // in the default run
};

constexpr int kBenchHardScenes = 29;      // 0.56.1: the default run (0.92.0: 29, orbit.porkchop in, orbit.sun_edge and orbit.oort_region to All scenes: bench_0.85.2's 1 % low 524.3 and 406.2 fps, both over 1.5 x 255 Hz; 0.86.0: 30, orbit.true_light_stars in place of blackhole.film_soft, removed with Film's see-through disc; 0.85.0: 30, orbit.true_light, orbit.sun_edge, orbit.oort_region; 0.84.0: 27, blackhole.close_spin; 0.83.0: 26, the 17 kept and 9 new, 23 moved to All scenes; 0.82.0: 40, blackhole.circle_spin; 0.81.0: 39, blackhole.film_soft; 0.79.0: 38, blackhole.disc_streaks; 0.58.0: 25, blackhole.all_light; 0.63.0: 26, atom.slice_super_drag; 0.64.0: 32, six heavy views; 0.72.0: 33, blackhole.moving_gas; 0.73.0: 34, orbit.alphacen; 0.75.0: 35, orbit.flyby; 0.77.0: 36, orbit.earth_clouds; 0.78.0: 37, orbit.comet_tails)
constexpr int kBenchAllScenes = 71;       // ... and with "All scenes"

const std::vector<BenchSceneText>& benchSceneTexts();   // the Hard scenes first, in the order they run, then the others of All
int benchSceneIndex(const std::string& id);             // -1: no such scene

// 0.56.1: the whole default run's planned length (s): the Hard scenes, the waits (Part 2) and the labs left alone (Part 3). A wait or a rest that takes
// longer than planned makes the run longer (each is capped: kBenchResponseCapS, kBenchIdleSettleCapS).
constexpr int kBenchResponseActions = 47;      // 0.92.0: + a new porkchop grid x 1 (response.porkchop_grid: done once, the grid is made the same way each time);       // 0.87.0: + the returning light at a new spin x 1 (done once: each spin's table is made once; 3 tries would pass the fixed limit, 295.8 s); 0.85.0: + the sunspot table x 3; 4 lab switches x 3, and 3 each of: element, density, molecule, spin and distance, Real to Film, Film to Real, the streaks (0.79.0), a tilt at spin (0.82.0), a wheel step at spin (0.83.0), date
constexpr double kBenchResponsePlanS = 1.6;    // a wait's planned time with the pause after it
constexpr int kBenchIdleEntries = 6;           // the four labs, and the Black Hole Lab spinning and with the glare
constexpr double kBenchIdlePlanS = 4.0;        // the 3 s left alone and the picture finished before
double benchPlannedS();
constexpr double kBenchLimitS = 4.9 * 60;     // 0.83.0: the run's limit, fixed by the owner (24 Nov 2026): a new scene replaces one or the 1.5 x refresh rule makes room. 0.85.0: raised once to 4.9 minutes by the owner (25 Nov 2026) for the three scenes of the outside review

}  // namespace app
