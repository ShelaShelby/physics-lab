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
    default: return true;
    }
}

}  // namespace atom
