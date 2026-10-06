// 0.15 stage 5a: the Atom Lab's toolbar and the two small panels under its carets (Nucleus: the level; Cloud: which cloud view). What each button does is in atom/atom_tools.* (tested without the app).
// Play / pause is in the element chip (stage 4; Space too), not on the toolbar. View options, marks, photons, the element chip's periodic table, the card and the legend come in 5b.
#include <algorithm>
#include <cmath>

#include "atom/atom_core.h"
#include "atom/atom_learn.h"
#include "atom/atom_tools.h"
#include "ui/atom_panel.h"
#include "ui/fonts.h"
#include "ui/shell_internal.h"
#include "ui/shell_test.h"

namespace ui::shellimpl {

using namespace sh;
using atom::Tool;
using app::shell::Metrics;

std::vector<ToolItem> atomTools(AppState& app) {
    atom::AtomLab& L = app.atom;
    std::vector<ToolItem> v;
    const auto button = [&](const char* id, const char* tip, Icon icon, Tool t) {
        ToolItem it; it.kind = ToolItem::Button; it.id = id; it.tip = tip; it.icon = icon; it.on = atom::toolOn(L, t);
        it.act = [&L, t] { atom::pressTool(L, t); };
        v.push_back(it);
    };
    button("tool.atom", "Atom: the electron and the whole atom (Orbit view)", Icon::Atom, Tool::Atom);
    button("tool.nucleus", "Nucleus: zoom to the nucleus (Nucleon, one proton or neutron: Controls › View)", Icon::Nucleus, Tool::Nucleus);
    button("tool.cloud", "Cloud: the electron as a probability cloud (Cloud 3D and Slice: Controls › View)", Icon::Cloud, Tool::Cloud);
    button("tool.chart", "Chart of nuclides: every nuclide on one map", Icon::Chart, Tool::Chart);
    v.push_back(separator());
    button("tool.reset", L.chartOn() ? "Reset view: tilt, zoom and position of the chart" : "Reset view: turn the picture back", Icon::Reset, Tool::ResetView);
    if (atom::toolShown(L, Tool::Centre)) button("tool.centre", "Centre: move the picture back to the middle", Icon::Target, Tool::Centre);
    if (atom::toolShown(L, Tool::DragMode)) {
        const bool move = L.s.dragMode == "move";
        button("tool.drag", move ? "Drag moves the picture (click: drag rotates)" : L.p.view == "Slice" ? "Drag slides the cut (click: drag moves the picture)" : "Drag rotates (click: drag moves the picture)", Icon::Move, Tool::DragMode);
    }
    return v;                                             // (stage 5c.1: the Controls panel opens from its handle on the picture's right edge, not from the toolbar)
}

double atomPanelWidthEm(PanelId id) {
    switch (id) {
    case PanelId::Chip: return 38;    // the element dropdown: 18 columns of cells of at least 28 u (18 x 28 + 17 x 2 + 2 x 16 padding = 570 u = 38 em of Body)
    default: return 0;
    }
}

// ◀ slider ▶ on one row (the Protons and Neutrons rows). The slider keeps its own value while it is dragged; a change is reported as the new whole number.
static bool stepperRow(Panel& p, const char* id, int v, int lo, int hi, int* out, bool* dec, bool* inc) {
    const Metrics& m = F().m;
    const float bw = static_cast<float>(m.control), gap = static_cast<float>(m.s4), y = p.y;
    const ImVec2 o = here(p);
    const std::string base = id;
    *dec = iconButton((base + ".dec").c_str(), o, ImVec2(bw, bw), Icon::CaretLeft, "One fewer");
    *inc = iconButton((base + ".inc").c_str(), ImVec2(o.x + p.w - bw, o.y), ImVec2(bw, bw), Icon::CaretRight, "One more");
    float fv = static_cast<float>(v);
    const float sw = p.w - 2 * (bw + gap);
    bool changed = false;
    if (slider((base + ".slider").c_str(), ImVec2(o.x + bw + gap, o.y + (bw - static_cast<float>(m.row)) / 2.0f), sw, &fv, static_cast<float>(lo), static_cast<float>(hi))) { *out = static_cast<int>(std::lround(fv)); changed = *out != v; }
    p.y = y + bw + static_cast<float>(m.s8);
    return changed;
}

bool buildAtomPanel(PanelId id, AppState& app, Panel& p, bool& /*closeAfter*/) {
    atom::AtomLab& L = app.atom;
    const Metrics& m = F().m;
    switch (id) {
    case PanelId::Chip: {
        if (app.lab != 1) return false;
        pTitle(p, "Element");
        // the table: 18 columns, cells at least 28 u wide, the chosen one ringed; same colours and dashed edges (no stable isotope) as the old table
        const float u = static_cast<float>(F().dpi), gap = 2 * u;
        PtGeom g;
        g.o = here(p);
        g.gap = gap; g.cw = (p.w - 17 * gap) / 18; g.ch = std::round(g.cw * 0.92f); g.rowGap = static_cast<float>(m.s8);
        g.s = u; g.corner = 3 * u; g.symPx = px(Type::Body); g.markPx = px(Type::Caption); g.boldAll = false; g.note = "pt.";
        const int z = periodicTableCells(g, L.p.Z, cText(), cAccent(), cMuted());
        if (z) atom::pickElement(L, z);
        if (shellLogOn()) for (int e = 1; e <= 118; e++) { const auto [r, c] = atom::ptPos(e); const ImVec2 a = g.cellPos(r, c); auditRect("pt." + std::to_string(e), a.x, a.y, g.cw, g.ch); }   // (for the automatic checks only)
        p.y += (g.cellPos(10, 1).y + g.ch) - g.o.y + static_cast<float>(m.s8);
        pCaption(p, "Click an element to load its most common (or longest-lived) isotope. Dashed edges: no stable isotope.");
        pCaption(p, displayable(atom::isotopeLine(L.p.Z, L.p.N)));
        pGap(p, m.s8);
        // protons
        pBody(p, "Protons (Z): " + std::to_string(L.p.Z) + " · " + atom::name(L.p.Z), true);
        {
            int nz = L.p.Z; bool dec = false, inc = false;
            if (stepperRow(p, "chip.z", L.p.Z, 1, 118, &nz, &dec, &inc)) atom::pickElement(L, nz);
            if (dec) atom::stepProtons(L, -1);
            if (inc) atom::stepProtons(L, 1);
        }
        // neutrons
        const auto nr = atom::nRange(L.p.Z);
        pBody(p, "Neutrons: " + std::to_string(L.p.N), true);
        {
            int nn = L.p.N; bool dec = false, inc = false;
            if (stepperRow(p, "chip.n", L.p.N, nr.first, nr.second, &nn, &dec, &inc)) L.setParam("N", double(nn));
            if (dec) atom::stepNeutrons(L, -1);
            if (inc) atom::stepNeutrons(L, 1);
        }
        if (pButton(p, "chip.usual", "usual (N = " + std::to_string(atom::defaultN(L.p.Z)) + ")")) atom::usualNeutrons(L);
        // famous isotopes
        pGap(p, m.s8);
        pHeading(p, "Famous isotopes");
        const auto& fam = atom::famous();
        for (int i = 0; i < static_cast<int>(fam.size()); i++) {
            const std::string label = std::string(atom::name(fam[i].Z)) + "-" + std::to_string(fam[i].Z + fam[i].N) + " · " + fam[i].why;
            if (pRow(p, ("fam." + std::to_string(i)).c_str(), displayable(label), fam[i].Z == L.p.Z && fam[i].N == L.p.N)) atom::pickFamous(L, i);
        }
        return true;
    }
    default: return false;
    }
}


// ---- the Controls panel (stage 5c): what the Nucleus ▾ and Cloud ▾ panels held, in a "View" section. The other Atom controls are still in the Old panel (they move in 5d and 5e). ----
void atomControls(AppState& app, Panel& p, const SectionFn& section) {
    atom::AtomLab& L = app.atom;
    const Metrics& m = F().m;
    if (section("view", "View", true)) {
        pHeading(p, "Nucleus");
        if (pRow(p, "nucleus.nucleus", "Nucleus", atom::toolOn(L, Tool::Nucleus) && !atom::toolOn(L, Tool::Nucleon))) { atom::pressTool(L, Tool::Nucleus); }
        if (pRow(p, "nucleus.nucleon", "Nucleon (one proton or neutron)", atom::toolOn(L, Tool::Nucleon))) { atom::pressTool(L, Tool::Nucleon); }
        pCaption(p, "Zoom in to a nucleon and click one to dive inside it.");
        if (atom::toolShown(L, Tool::BuildAnother) && pButton(p, "nucleus.build", "Build another")) atom::pressTool(L, Tool::BuildAnother);
        if (atom::toolShown(L, Tool::Reshuffle) && pButton(p, "nucleus.reshuffle", "Reshuffle nucleons")) atom::pressTool(L, Tool::Reshuffle);
        pGap(p, m.s8);
        pHeading(p, "Cloud");
        if (pRow(p, "cloud.cloud", "Cloud (side view)", L.p.view == "Cloud")) { atom::pressTool(L, Tool::Cloud); }
        if (pRow(p, "cloud.cloud3d", "Cloud 3D (turn it with the mouse)", L.p.view == "Cloud 3D")) { atom::pressTool(L, Tool::Cloud3D); }
        if (pRow(p, "cloud.slice", "Slice (cross-section of the cloud)", L.p.view == "Slice")) { atom::pressTool(L, Tool::Slice); }
        pCaption(p, "Each dot is one possible place to find the electron (one electron, non-relativistic model).");
    }
}

}  // namespace ui::shellimpl
