#include "app/bench_scenes.h"

#include <cctype>
#include <cstdlib>

#include "atom/atom_bench.h"

namespace app {

namespace {

// atom/atom_bench.cpp still numbers its texts the way the benchmark did before 0.16 stage 4 (its tests check them): the old number -> the number now (0: no longer in the benchmark)
int shownNumber(int old) {
    static const int m[36] = {0, 1, 0, 0, 5, 0, 2, 0, 6, 0, 0, 7, 0, 0, 0, 8, 9, 10, 3, 0, 0, 0, 11, 12, 0, 13, 0, 0, 14, 15, 16, 17, 18, 0, 0, 4};
    return old >= 0 && old < 36 ? m[old] : 0;
}

// "scene 23" -> "scene 12": the first number after every "scene" / "scenes" / "Scene"
std::string renumber(const std::string& s) {
    std::string o;
    std::size_t i = 0;
    while (i < s.size()) {
        const std::size_t k = s.find("cene", i);
        if (k == std::string::npos) { o += s.substr(i); break; }
        std::size_t j = k + 4;
        while (j < s.size() && (s[j] == 's' || s[j] == ' ')) j++;
        if (j < s.size() && std::isdigit(static_cast<unsigned char>(s[j]))) {
            std::size_t e = j;
            while (e < s.size() && std::isdigit(static_cast<unsigned char>(s[e]))) e++;
            o += s.substr(i, j - i) + std::to_string(shownNumber(std::atoi(s.substr(j, e - j).c_str())));
            i = e;
        } else { o += s.substr(i, k + 4 - i); i = k + 4; }
    }
    return o;
}

void replaceOnce(std::string& s, const std::string& from, const std::string& to) {
    const std::size_t p = s.find(from);
    if (p != std::string::npos) s.replace(p, from.size(), to);
}

// "Risk 10: " and "Stress: " are not part of a title any more
std::string noPrefix(std::string n) {
    if (n.rfind("Risk ", 0) == 0 || n.rfind("Stress: ", 0) == 0) { const std::size_t p = n.find(": "); if (p != std::string::npos) n.erase(0, p + 2); }
    if (!n.empty()) n[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(n[0])));
    return n;
}

// the texts of atom_bench.cpp that point at a scene which is no longer in the benchmark (written with the OLD numbers, renumbered afterwards)
std::string fixedWhat(int which, std::string w) {
    if (which == 13) replaceOnce(w, "(scene 3 of the benchmark)", "(the 4f cloud of the Cloud 3D scene of older versions)");
    if (which == 15) replaceOnce(w, "(scene 2 of the benchmark with the biggest nucleus)", "(the nucleus view of older versions, with the biggest nucleus)");
    if (which == 19) {
        replaceOnce(w, "Scenes 19 to 21 draw no chain.", "The chart scenes of older versions drew no chain.");
        replaceOnce(w, "Compare this scene with the tilted-landscape scene before it: the DIFFERENCE is", "Compared with the tilted-landscape scene of older versions, the DIFFERENCE is");
    }
    if (which == 30) replaceOnce(w, "which scenes 4 and 5 missed", "which the nucleus-view and one-nucleon scenes (3 and 5) miss");
    return w;
}

}  // namespace

const std::vector<BenchSceneText>& benchSceneTexts() {
    static const std::vector<BenchSceneText> v = [] {
        std::vector<BenchSceneText> v;
        const auto add = [&](BenchKind kind, int which, int twinOf, std::string name, std::string what) {
            v.push_back({std::to_string(v.size() + 1) + ". " + name, what, kind, which, twinOf});
        };
        const auto atomScene = [&](int which, int twinOf) {
            const atom::BenchInfo& in = atom::benchSceneInfo(which);
            add(BenchKind::Atom, which, twinOf, renumber(noPrefix(in.name)), renumber(fixedWhat(which, in.what)));
        };
        add(BenchKind::OrbitHeaviest, -1, 0, "Orbit Lab, heaviest view",
            "3D view, belts and Oort cloud on, all Solar System extras on, full night sky with constellation lines and names, "
            "far-away things on, full orbit trails, true scale, fastest time speed (1 s = about 50 years), zoom x1, fixed camera. "
            "From 0.13 stage 4 the rocks' places are worked out on the graphics card when the PC has OpenGL 4.3 (scene 17 is this scene with the processor doing it)");
        add(BenchKind::OrbitRocks8, -1, 0, "Orbit Lab, 8 times the rocks",
            "scene 1 (3D, everything on, true scale, zoom x1, fastest time) with 8 times as many rocks in the asteroid belt, the Trojans, the Kuiper belt and the Oort cloud "
            "(48,000 instead of 6,000; the same kinds of orbits, a fixed random seed). The rocks do not pull on each other, so the physics is unchanged. "
            "From 0.13 stage 4 their places are worked out on the graphics card when the PC has OpenGL 4.3 (scene 18 is this scene with the processor doing it)");
        atomScene(15, 0);    // old 18: the nucleus view, oganesson-294, the whole nucleus
        atomScene(30, 0);    // old 35: the nucleus at full zoom, the camera moving
        add(BenchKind::Atom, 2, 0, "Atom Lab, one nucleon", "uranium-238, Accurate quarks, + Sea detail, zoomed all the way into one nucleon (the first one)");   // old 4 (atom_bench.cpp has no text for it)
        atomScene(5, 0);     // old 8: phase colours Wheel, Cloud 3D, 48,000 dots
        atomScene(8, 0);     // old 11: a cloud jump in flight
        atomScene(12, 0);    // old 15: superposition 2p z + 3d z2 (beats)
        atomScene(13, 0);    // old 16: first use of an outline
        atomScene(14, 0);    // old 17: the outline with the cut dragged
        atomScene(19, 0);    // old 22: the chart tilted, with the decay chain
        // "Compare old CPU paths": the twins of 6, 7, 8, 9, 10, 1 and 2
        atomScene(20, 6); atomScene(22, 7); atomScene(25, 8); atomScene(26, 9); atomScene(27, 10);
        add(BenchKind::OrbitHeaviestCpuRocks, -1, 1, "Orbit Lab, heaviest view, GPU rocks OFF (the CPU path, to compare with scene 1)",
            "scene 1 (3D view, belts and Oort cloud, everything on, true scale, fastest time, zoom x1) with the GPU rocks switch Off: the processor places the 6,000 rocks, as before 0.13 stage 4. Scene 1 is the same with it On");
        add(BenchKind::OrbitRocks8CpuRocks, -1, 2, "Orbit Lab, 8 times the rocks, GPU rocks OFF (the CPU path, to compare with scene 2)",
            "scene 2 (the same view with 48,000 rocks) with the GPU rocks switch Off: the processor places all 48,000 rocks. Scene 2 is the same with it On");
        return v;
    }();
    return v;
}

}  // namespace app
