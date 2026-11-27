#include "orbit/pick_list.h"

#include "orbit/alphacen.h"
#include "orbit/sky.h"

namespace orbit {

std::vector<PickGroup> pickList(const SolarSystem& sys, bool alphaCen) {
    std::vector<PickGroup> g;
    if (alphaCen) {                                // 0.73.0: the Alpha Centauri scene's things (orbit/alphacen.h Thing)
        const auto it = [](int k, const char* col) { return PickItem{acen::thingName(k), col, {Selection::ACen, 0, k}, "", 0}; };
        PickGroup stars{"Alpha Centauri", {it(acen::StarA, "#fff1d8"), it(acen::StarB, "#ffd29a"), it(acen::Bary, "#9aa5b8"), it(acen::Proxima, "#ff8a70")}};
        PickGroup planets{"Proxima's planets", {it(acen::PlanetB, "#8fc7ff"), it(acen::PlanetD, "#e8d890"), it(acen::PlanetC, "#c9a6ff")}};
        PickGroup cand{"Unconfirmed", {it(acen::CandidateA, "#d1b0ff")}};
        PickGroup far{"Far away", {it(acen::Sun, "#f0b020")}};
        g.push_back(stars); g.push_back(planets); g.push_back(cand); g.push_back(far);
        return g;
    }
    PickGroup star{"Star and planets", {}}, dwarfs{"Dwarf planets", {}}, comets{"Famous comets", {}}, belts{"Belts and the far edge", {}}, far{"Far away", {}};
    star.items.push_back({"Sun", "#f0b020", {Selection::Star}, "", 0});
    star.items.push_back({"Barycentre", "#9aa5b8", {Selection::Bary}, "", 0});
    for (const Body& b : sys.bodies) {
        PickGroup& grp = b.type == BodyKind::Planet ? star : b.type == BodyKind::Dwarf ? dwarfs : comets;
        PickItem body{b.name, b.color, {Selection::Body, b.id}, "", 0};
        body.moons = static_cast<int>(b.moons.size());
        grp.items.push_back(body);
        for (std::size_t j = 0; j < b.moons.size(); j++) {   // 0.94.1: each moon under its own body (the owner's wish, 27 Nov 2026), not in a group of their own
            PickItem m{b.moons[j].data->name, b.color, {Selection::Moon, b.id, static_cast<int>(j)}, std::string(b.moons[j].data->name) + " · moon of " + b.name, 0};
            m.depth = 1;
            grp.items.push_back(m);
        }
    }
    static const struct { const char* key; const char* name; const char* color; double zoom; } B[] = {
        {"main", "Asteroid belt", "#8a7f72", -0.15}, {"trojan", "Jupiter’s Trojans", "#a08c6a", -0.55}, {"kuiper", "Kuiper belt", "#6f9fc0", -1.55}, {"oort", "Oort cloud", "#9aa5b8", -4.4}};
    for (const auto& b : B)
        for (std::size_t gi = 0; gi < sys.belts.size(); gi++)
            if (sys.belts[gi].key == b.key) belts.items.push_back({b.name, b.color, {Selection::Belt, 0, static_cast<int>(gi)}, "", b.zoom});
    static const struct { const char* key; const char* name; const char* color; } F[] = {
        {"sedna", "Sedna", "#b9a58c"}, {"vp113", "2012 VP113", "#b9a58c"}, {"tg387", "2015 TG387 (Leleakuhonua)", "#b9a58c"},   // 0.85.0 (R4)
        {"v1", "Voyager 1", "#ffd27a"}, {"v2", "Voyager 2", "#ffd27a"}, {"acen", "Alpha Centauri", "#ffe9b0"}, {"sirius", "Sirius", "#cfe0ff"},
        {"vega", "Vega", "#cfe0ff"}, {"pleia", "Pleiades", "#8fd0ff"}, {"m42", "Orion Nebula", "#8fd0ff"}, {"sgra", "Galactic centre", "#cfd6e6"}};
    for (const auto& f : F) far.items.push_back({f.name, f.color, {Selection::Far, 0, farIndexOf(f.key)}, "", 0});
    g.push_back(star); g.push_back(dwarfs); g.push_back(comets); g.push_back(belts); g.push_back(far);
    return g;
}

}  // namespace orbit
