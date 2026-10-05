// The radial probability graph of the Atom Lab: how likely the electron is to be found at each distance from the nucleus. No drawing here, so it can be tested.
//
// Convention: P(r) is the radial probability DENSITY, P(r) = r^2 |R_nl(r)|^2, so that P(r) dr is the probability of finding the electron between r and r + dr from the nucleus
// (any direction), and the area under the whole curve is 1. Units: r in Bohr radii, P in 1 / a0. Same model as the cloud (atom_core.h): ONE electron, non-relativistic, point nucleus,
// hydrogen-like orbitals with the nuclear charge Z. The radial part depends on n, l and Z only, not on m or the orientation, so every orientation of a shell has the same curve.
#pragma once

#include <string>
#include <vector>

namespace atom {

constexpr int kRadialPoints = 400;         // the curve is drawn from kRadialPoints + 1 points (worked out when the state or Z changes, not every frame)
constexpr double kRadialCover = 0.999;     // the picture's right edge: the distance inside which this share of the probability lies

struct RadialGraph {
    int n = 0, l = 0, Z = 1;
    std::vector<double> r;                 // distance from the nucleus, a0: 0 to rEnd, equally spaced
    std::vector<double> P;                 // P(r), 1 / a0
    double rEnd = 0;                       // a0
    double peak = 0;                       // the most likely distance: where P is highest, a0
    std::vector<double> maxima;            // every local maximum of P, a0, nearest the nucleus first (there are n - l)
    double mean = 0;                       // the average distance <r> = a0 (3 n^2 - l (l + 1)) / (2 Z), the number the readout shows as "theory"
    double reach90 = 0;                    // how far the 90 % outline of this state reaches from the nucleus, a0 (outline.h rmax / Z)
    bool reach90Pending = false;           // 0.13 stage 3: the outline is still being made on the GPU, so reach90 is not known yet (0): the panel shows no 90 % mark and calls completeReach90 each frame
};

double meanRadius(int n, int l, int Z);                       // <r>, a0: the one formula used by the graph and by the readout
double radialDensity(int state, int Z, double r);             // P(r), 1 / a0; `state` is a real orbital's index or a complex state's id (only its n and l matter)
RadialGraph radialGraph(int state, int Z);                    // `state` as above (the 90 % reach is that state's own outline; if it is not made yet this waits for the CPU to make it: about 30 ms the first time)
// 0.13 stage 3: the same without that wait when the GPU can make the outline: `useGpu` (the lab's "GPU outline" switch) hands the outline to the compute object and leaves reach90Pending set until it is ready.
// With no compute object it is the same as radialGraph(state, Z).
RadialGraph radialGraphNoWait(int state, int Z, bool useGpu);
bool completeReach90(RadialGraph& g, int state, int Z, bool useGpu);   // fills reach90 once the outline is ready; true when it is (or was never pending)

// the words around the graph (used by the panel and checked by the tests)
double radialTickStep(double rEnd);                           // a round axis step (1, 2 or 5 times a power of ten, a0) that gives 3 to 8 ticks up to rEnd
extern const char* const kRadialNote;
extern const char* const kRadialAxisNote;
std::string radialMarkText(const char* what, double rA0);    // "most likely distance 1 a0 = 0.529 Å"

}  // namespace atom
