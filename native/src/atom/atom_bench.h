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
constexpr int kBenchFirstRisk = 5, kBenchChartScene = 16, kBenchLandscapeScene = 17, kBenchSceneCount = 18;
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
