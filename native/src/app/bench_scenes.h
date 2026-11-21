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
    Atom,                                                                        // atom/atom_bench.h, scene `atomWhich`
    MoleculeDensity, MoleculeVibDensity, MoleculeSpaceVib,                       // 0.56.1
    BlackHole, BlackHoleFilmTilt, BlackHoleExtreme, BlackHoleAllLight, BlackHoleMovingGas,                              // 0.56.1: the first is 0.55.0's scene (blackhole.real_move)
    UiGuideScroll, UiDataPlot, UiSettingsOpen                                    // 0.56.1: the pages and panels the 0.50.3 review suspected
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

constexpr int kBenchHardScenes = 33;      // 0.56.1: the default run (0.58.0: 25, blackhole.all_light; 0.63.0: 26, atom.slice_super_drag; 0.64.0: 32, six heavy views; 0.72.0: 33, blackhole.moving_gas)
constexpr int kBenchAllScenes = 49;       // ... and with "All scenes"

const std::vector<BenchSceneText>& benchSceneTexts();   // the Hard scenes first, in the order they run, then the others of All
int benchSceneIndex(const std::string& id);             // -1: no such scene

// 0.56.1: the whole default run's planned length (s): the Hard scenes, the waits (Part 2) and the labs left alone (Part 3). A wait or a rest that takes
// longer than planned makes the run longer (each is capped: kBenchResponseCapS, kBenchIdleSettleCapS).
constexpr int kBenchResponseActions = 33;      // 4 lab switches x 3, and 3 each of: element, density, molecule, spin and distance, Real to Film, Film to Real, date
constexpr double kBenchResponsePlanS = 1.6;    // a wait's planned time with the pause after it
constexpr int kBenchIdleEntries = 6;           // the four labs, and the Black Hole Lab spinning and with the glare
constexpr double kBenchIdlePlanS = 4.0;        // the 3 s left alone and the picture finished before
double benchPlannedS();

}  // namespace app
