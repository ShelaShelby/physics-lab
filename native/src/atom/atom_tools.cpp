#include "atom/atom_tools.h"

#include "atom/chart.h"

namespace atom {

namespace {
void setView(AtomLab& L, const char* view) {
    if (L.p.view != view) L.setParam("view", std::string(view));     // the old View row: one entry point for every change
}
}  // namespace

void pressTool(AtomLab& L, Tool t) {
    switch (t) {
    case Tool::Atom: setView(L, "Orbit"); L.zoomAtom(); break;
    case Tool::Nucleus: if (L.chartOn()) setView(L, "Orbit"); L.zoomNucleus(); break;
    case Tool::Nucleon: if (L.chartOn()) setView(L, "Orbit"); L.zoomNucleon(); break;
    case Tool::Cloud: setView(L, "Cloud"); break;
    case Tool::Cloud3D: setView(L, "Cloud 3D"); break;
    case Tool::Slice: setView(L, "Slice"); break;
    case Tool::Chart: setView(L, kChartView); break;
    case Tool::ResetView: if (L.chartOn()) L.chartReset(); else if (L.showResetView()) L.resetView(); break;
    case Tool::Centre: L.centre(); break;
    case Tool::DragMode: L.toggleDrag(); break;
    case Tool::BuildAnother: L.buildAnother(); break;
    case Tool::Reshuffle: L.reshuffle(); break;
    }
}

bool toolOn(const AtomLab& L, Tool t) {
    const bool orbit = L.p.view == "Orbit";
    const bool deep = L.s.layer == "Nucleus" || L.s.layer == "Nucleon" || L.s.layer == "Quarks";   // (set when the picture is drawn)
    switch (t) {
    case Tool::Atom: return orbit && !deep;
    case Tool::Nucleus: return orbit && deep;
    case Tool::Nucleon: return orbit && (L.s.layer == "Nucleon" || L.s.layer == "Quarks");
    case Tool::Cloud: return L.p.view == "Cloud" || L.p.view == "Cloud 3D" || L.p.view == "Slice";
    case Tool::Cloud3D: return L.p.view == "Cloud 3D";
    case Tool::Slice: return L.p.view == "Slice";
    case Tool::Chart: return L.chartOn();
    case Tool::DragMode: return L.s.dragMode == "move";
    default: return false;
    }
}

bool toolShown(const AtomLab& L, Tool t) {
    switch (t) {
    case Tool::Centre: return L.showCentre();
    case Tool::DragMode: return L.showDrag();
    case Tool::BuildAnother: return toolOn(L, Tool::Nucleus) && L.showBuildAnother();   // only in the nucleus view, and when the old panel offered it
    case Tool::Reshuffle: return toolOn(L, Tool::Nucleus) && L.showReshuffle();
    default: return true;
    }
}

static std::string plural(int n, const char* one, const char* many) { return std::to_string(n) + " " + (n == 1 ? one : many); }

ChipText elementChipText(int Z, int N) {
    ChipText t;
    t.title = std::string(symbol(Z)) + " · " + name(Z) + "-" + std::to_string(Z + N);
    t.sub = plural(Z, "proton", "protons") + " · " + plural(N, "neutron", "neutrons");
    return t;
}
void pickElement(AtomLab& L, int Z) { L.setParam("Z", double(Z)); }   // (setParam("Z") sets the neutrons to the usual count itself)
void stepProtons(AtomLab& L, int d) { L.setParam("Z", double(L.p.Z + d)); }
void stepNeutrons(AtomLab& L, int d) { L.setParam("N", double(L.p.N + d)); }
void usualNeutrons(AtomLab& L) { L.setParam("N", double(defaultN(L.p.Z))); }
void pickFamous(AtomLab& L, int i) {
    const auto& fam = famous();
    if (i < 0 || i >= static_cast<int>(fam.size())) return;
    L.setParam("Z", double(fam[i].Z)); L.setParam("N", double(fam[i].N));
}

}  // namespace atom
