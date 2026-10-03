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
// width, height: the picture's size in points.
// Every scene starts from the Atom Lab's default settings (AtomParams{}), then applies its own.
void benchSetup(AtomLab& L, int which, double width, double height);
// "view Orbit, Orbit style Modern, quark style Cinematic, ..." as the lab is now: put in the result so a wrong run is visible.
std::string benchSettingsText(const AtomLab& L);

}  // namespace atom
