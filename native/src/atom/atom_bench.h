// The Atom Lab scenes of the benchmark (the same ones every run: fixed random numbers, fixed camera, no animation to wait for).
#pragma once

#include <string>

#include "atom/atom_sim.h"

namespace atom {

// 0: uranium-238, the whole nucleus in view, Cinematic quarks, + Sea detail
// 1: uranium-238, Cloud 3D, the 4f orbital, cloud style Classic, density Normal (12,000 dots, as in older versions)
// 2: uranium-238, Accurate quarks, + Sea detail, zoomed all the way into the first nucleon
// 4: stress: the same as 1, drawn as Glow with 48,000 dots (cloud style Glow, density Ultra)
// 3: stress: oganesson-294 (118 protons, 176 neutrons), Accurate quarks, + Sea detail, zoomed 0.6 (about 4 times) closer than the whole nucleus, so every nucleon is large
// 5 to 15: the speed-risk scenes of 0.10.6 stage 7 (audit findings 10, 11, 12), see benchSceneInfo(). They are for finding out where time goes on the owner's PC; nothing is optimised.
//   10 phase colours with Classic dots: 5 Cloud 3D Wheel, 6 Cloud 3D Sign (the baseline), 7 Cloud 2D Wheel (all 48,000 dots)
//   11 a cloud jump: 8 held half way, 9 one after another; a superposition: 10 orthogonal, 11 overlapping, 12 orthogonal with two energies (all 48,000 dots)
//   12 an outline: 13 first use (cache emptied again and again), 14 the cut dragged (Slice view), 15 the nucleus view (oganesson-294, whole nucleus)
//   16 the chart of nuclides (0.11 stage 2): the whole chart, every nuclide's square drawn, nothing hovered (keep the pointer off the picture)
//   17 the same chart as a landscape (0.11 stage 3): full tilt, 3,557 blocks with their sides, the painter's order, the ground and the legend
//   18 the landscape with the BINDING-ENERGY height (0.11 stage 4), full tilt: the same blocks and drawing as 17, with the other height table (and the peak note)
//   19 the landscape of 17 with the DECAY CHAIN of uranium-238 drawn and walked five steps (0.12 stage 1): the trail, its labels and the walker on top; scenes 16 to 18 (the benchmark's 19 to 21) draw no chain, so they stay comparable with 0.11
//   20 scene 8 again with GPU colours Off (0.13 stage 1): the 0.12 way, the CPU colours every dot (scene 8 is the same with GPU colours On); 21 scene 10 again with GPU colours Off
//   22 to 25 scenes 11, 13, 14 and 15 again with GPU superposition Off (0.13 stage 2): the CPU works out the colour groups, as before stage 2 (scenes 11, 13, 14, 15 are the same with it On)
// NUMBERS: `which` here is not the number the benchmark shows: the benchmark's scene number is which + 3 for which >= 5 (which 5 = "scene 8", which 19 = "scene 22", which 20 = "scene 23"),
// and which 0 to 4 are scenes 2, 3, 4, 5 and 7 (see ui/benchmark.cpp). Texts shown to the owner use the benchmark's numbers.
constexpr int kBenchFirstRisk = 5, kBenchChartScene = 16, kBenchLandscapeScene = 17, kBenchBindingScene = 18, kBenchChainScene = 19, kBenchWheelCpuScene = 20, kBenchWheel2DCpuScene = 21, kBenchJumpCpuScene = 22, kBenchSuperCpuFirst = 23, kBenchSceneCount = 26;   // 22: scene 11 with the CPU path; 23, 24, 25: scenes 13, 14, 15
struct BenchInfo { std::string name, what; };       // name: without a number (the benchmark adds it); what: the settings in words
const BenchInfo& benchSceneInfo(int which);          // which = kBenchFirstRisk .. kBenchSceneCount - 1
// Some scenes need something done every frame (a jump held half way, the cut moving, the outline's cache emptied): the benchmark calls this at the start of every frame
// after the scene is set up (and before the picture is drawn). sceneFrame: 0, 1, 2 ... from the set-up, the warm-up included; measureFrame: -1 while warming up, then 0, 1, 2 ... the counted frames.
// It does nothing for the scenes that need nothing (0 to 4, 5 to 7, 10 to 12, 15), so it can be called for any scene.
void benchFrame(AtomLab& L, int which, int sceneFrame, int measureFrame);
// width, height: the picture's size in points.
// Every scene starts from the Atom Lab's default settings (AtomParams{}), then applies its own.
void benchSetup(AtomLab& L, int which, double width, double height);
// "view Orbit, Orbit style Modern, quark style Cinematic, ..." as the lab is now: put in the result so a wrong run is visible.
std::string benchSettingsText(const AtomLab& L);

}  // namespace atom
