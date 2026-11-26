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
    case Tool::ZoomAtom: L.zoomAtom(); break;
    }
}

bool toolOn(const AtomLab& L, Tool t) {
    const bool orbit = L.p.view == "Orbit";
    const bool deep = L.s.layer == "Nucleus" || L.s.layer == "Nucleon" || L.s.layer == "Quarks";   // (set when the picture is drawn)
    switch (t) {
    case Tool::Atom: return orbit && !deep;
    case Tool::Nucleus: return orbit && deep;
    case Tool::Nucleon: return orbit && (L.s.layer == "Nucleon" || L.s.layer == "Quarks");
    case Tool::Cloud: return L.p.view == "Cloud";            // (5l: only the flat Cloud view; Cloud 3D and Slice have their own buttons, so one view lights one button)
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

// ---- 0.86.0 (R5): the made-up learning pictures and the realistic choices (atom_tools.h) ----
namespace {
struct MadeUp { const char* key; const char* value; const char* what; };
const MadeUp kMadeUp[] = {
    {"picture", "Orbit", "the Orbit picture draws each electron as a ball gliding on a circle, as in Bohr's 1913 model, which quantum mechanics replaced. A real electron "
                         "has no path: Cloud 3D shows where it can be found."},
    {"qstyle", "Simple", "three balls joined by flux tubes. A quark has no definite place inside a proton: Accurate draws where its quarks and gluons carry the momentum, "
                         "from measured data."},
    {"style", "Nucleons", "the nucleons' colours and glow, one snapshot of them, each with the proton's charge profile. Real nucleons overlap and move: Density draws the "
                          "measured charge density."},
};
// the realistic values, in the order Most realistic sets them (0.25 to 0.85: ui/realest.cpp applyAtom's list)
const std::pair<const char*, const char*> kRealistic[] = {
    {"picture", "Cloud 3D"}, {"ostyle", "Modern"}, {"scale", "Fixed"}, {"qstyle", "Accurate"}, {"detail", "+ Sea"}, {"style", "Density"}, {"cstyle", "Glow"}, {"cdens", "Ultra"},
    {"phase", "Wheel"}, {"dirac", "On"}, {"elec", "All"}, {"rscale", "True scale"}, {"mrel", "Relativistic"}, {"hrel", "Dirac"}, {"vdots", "True share"},
};
}  // namespace
bool madeUpChoice(const std::string& key, const std::string& value) {
    for (const MadeUp& m : kMadeUp) if (key == m.key && value == m.value) return true;
    return false;
}
std::string madeUpCaption(const std::string& key) {
    for (const MadeUp& m : kMadeUp) if (key == m.key) return std::string(kMadeUpLabel) + ": " + m.what;
    return std::string();
}
const char* madeUpValue(const std::string& key) {
    for (const MadeUp& m : kMadeUp) if (key == m.key) return m.value;
    return nullptr;
}
std::vector<std::string> madeUpOn(const AtomLab& L) {
    std::vector<std::string> o;
    if (madeUpChoice("picture", L.p.view)) o.push_back("picture=" + L.p.view);
    if (madeUpChoice("qstyle", L.p.qstyle)) o.push_back("qstyle=" + L.p.qstyle);
    if (madeUpChoice("style", L.p.style)) o.push_back("style=" + L.p.style);
    return o;
}
const char* realisticValue(const std::string& key) {
    for (const auto& r : kRealistic) if (key == r.first) return r.second;
    return nullptr;
}
void applyRealistic(AtomLab& L) {
    pressTool(L, Tool::Cloud3D);
    for (const auto& r : kRealistic) if (std::string(r.first) != "picture") L.setParam(r.first, std::string(r.second));
}

}  // namespace atom
