// The Atom Lab scenes of the benchmark (the same ones every run: fixed random numbers, fixed camera, no animation to wait for).
#pragma once

#include "atom/atom_sim.h"

namespace atom {

// 0: uranium-238, the whole nucleus in view, Cinematic quarks, + Sea detail
// 1: uranium-238, Cloud 3D, the 4f orbital, all kCap dots
// 2: uranium-238, Accurate quarks, + Sea detail, zoomed all the way into the first nucleon
// 3: stress: oganesson-294 (118 protons, 176 neutrons), Accurate quarks, + Sea detail, zoomed 0.6 (about 4 times) closer than the whole nucleus, so every nucleon is large
// width, height: the picture's size in points.
void benchSetup(AtomLab& L, int which, double width, double height);

}  // namespace atom
