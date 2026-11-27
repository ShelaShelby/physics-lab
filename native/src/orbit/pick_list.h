// The Orbit Lab's "Pick a body" list (0.94.1: moved here from ui/orbit_actions.cpp so the unit tests read it; no app state, no Dear ImGui).
#pragma once

#include <string>
#include <vector>

#include "orbit/card_text.h"
#include "orbit/solar_system.h"

namespace orbit {

struct PickItem {
    std::string name;
    const char* color = "#ffffff";       // the dot's colour (the body's own: a planet's, its moons' use their planet's)
    Selection sel;
    std::string tip;                     // "Moon · moon of Earth" for a moon, else empty
    double beltZoom = 0;                 // belts only: the zoom that shows all of it
    int depth = 0;                       // 0.94.1: 1 for a moon (listed right after its planet or dwarf planet), else 0
    int moons = 0;                       // 0.94.1: a body's count of moons in the lab's own list (its moon items follow it)
};
struct PickGroup { const char* title; std::vector<PickItem> items; };

// The groups: Star and planets, Dwarf planets, Famous comets, Belts and the far edge, Far away (Alpha Centauri: its own four groups).
// 0.94.1: no "Moons" group. Each moon follows its own body (depth 1), in the lab's order (SolarSystem::bodies, then Body::moons).
std::vector<PickGroup> pickList(const SolarSystem& sys, bool alphaCen);

// 0.94.1: the rows the list shows of one group. `query` is already folded with `fold` (the app passes app/shell.h searchFold); `open(bodyId)` says whether
// a body is unfolded. No query: a body's moons show when it is open, or when one of them is selected. A query: a row shows when its name matches; a moon that
// matches shows under its body (shown open); a body that matches alone shows folded.
struct PickRow { int item = -1; bool open = false; };   // item: the index in the group's items
template <class Open, class Fold>
std::vector<PickRow> pickRows(const PickGroup& g, const std::string& query, const Selection& sel, Open open, Fold fold) {
    std::vector<PickRow> out;
    const auto hit = [&](const PickItem& it) { return query.empty() || fold(it.name).find(query) != std::string::npos; };
    const int n = static_cast<int>(g.items.size());
    for (int i = 0; i < n; i++) {
        const PickItem& it = g.items[static_cast<std::size_t>(i)];
        if (it.depth != 0) continue;
        std::vector<int> kids, shown;
        for (int j = i + 1; j < n && g.items[static_cast<std::size_t>(j)].depth == 1; j++) kids.push_back(j);
        bool show = true;
        if (query.empty()) {
            const bool moonSel = sel.kind == Selection::Moon && it.sel.kind == Selection::Body && sel.bodyId == it.sel.bodyId;
            if (!kids.empty() && (moonSel || open(it.sel.bodyId))) shown = kids;
        } else {
            for (int j : kids) if (hit(g.items[static_cast<std::size_t>(j)])) shown.push_back(j);
            show = hit(it) || !shown.empty();
        }
        if (!show) continue;
        out.push_back({i, !shown.empty()});
        for (int j : shown) out.push_back({j, false});
    }
    return out;
}

}  // namespace orbit
