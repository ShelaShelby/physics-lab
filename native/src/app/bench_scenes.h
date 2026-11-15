// The benchmark's scene list as text (0.16 stage 4): which scenes there are, in which order, and what their names and descriptions say.
// No graphics in here, so the numbers in the texts can be tested. How a scene is set up is in ui/benchmark.cpp and atom/atom_bench.cpp.
#pragma once

#include <string>
#include <vector>

namespace app {

enum class BenchKind { OrbitHeaviest, OrbitRocks8, OrbitHeaviestCpuRocks, OrbitRocks8CpuRocks, Atom };

struct BenchSceneText {
    std::string name;       // with its number: "3. Atom Lab, ..."
    std::string what;       // the settings in words
    BenchKind kind = BenchKind::Atom;
    int atomWhich = -1;     // kind Atom: the scene of atom/atom_bench.h
    int twinOf = 0;         // the number of the scene this one repeats with the processor doing the work (0: not a twin)
};

constexpr int kBenchDefaultScenes = 12;   // scenes 1 to 12 are the default run (0.46: 12, the whole atom's Glow cloud)
constexpr int kBenchAllScenes = 20;       // 13 to 20 (the old CPU paths, twins of 6, 7, 8, 9, 10, 1, 2, 12) run only with "Compare old CPU paths"

const std::vector<BenchSceneText>& benchSceneTexts();

}  // namespace app
