// 0.15 stage 5a: the Atom Lab's toolbar and the two small panels under its carets (Nucleus: the level; Cloud: which cloud view). What each button does is in atom/atom_tools.* (tested without the app).
// Play / pause is in the element chip (stage 4; Space too), not on the toolbar. View options, marks, photons, the element chip's periodic table, the card and the legend come in 5b.
#include "atom/atom_tools.h"
#include "ui/shell_internal.h"

namespace ui::shellimpl {

using namespace sh;
using atom::Tool;

std::vector<ToolItem> atomTools(AppState& app) {
    atom::AtomLab& L = app.atom;
    std::vector<ToolItem> v;
    const auto button = [&](const char* id, const char* tip, Icon icon, Tool t) {
        ToolItem it; it.kind = ToolItem::Button; it.id = id; it.tip = tip; it.icon = icon; it.on = atom::toolOn(L, t);
        it.act = [&L, t] { atom::pressTool(L, t); };
        v.push_back(it);
    };
    const auto split = [&](const char* id, const char* tip, Icon icon, Tool t, PanelId panel) {   // the button acts; its caret opens the small panel
        ToolItem it; it.kind = ToolItem::Split; it.id = id; it.tip = tip; it.icon = icon; it.on = atom::toolOn(L, t); it.panel = panel;
        it.act = [&L, t] { atom::pressTool(L, t); };
        v.push_back(it);
    };
    button("tool.atom", "Atom: the electron and the whole atom (Orbit view)", Icon::Atom, Tool::Atom);
    split("tool.nucleus", "Nucleus: zoom to the nucleus; the arrow: Nucleus or Nucleon", Icon::Nucleus, Tool::Nucleus, PanelId::Nucleus);
    split("tool.cloud", "Cloud: the electron as a probability cloud; the arrow: Cloud, Cloud 3D or Slice", Icon::Cloud, Tool::Cloud, PanelId::Cloud);
    button("tool.chart", "Chart of nuclides: every nuclide on one map", Icon::Chart, Tool::Chart);
    v.push_back(separator());
    button("tool.reset", L.chartOn() ? "Reset view: tilt, zoom and position of the chart" : "Reset view: turn the picture back", Icon::Reset, Tool::ResetView);
    if (atom::toolShown(L, Tool::Centre)) button("tool.centre", "Centre: move the picture back to the middle", Icon::Target, Tool::Centre);
    if (atom::toolShown(L, Tool::DragMode)) {
        const bool move = L.s.dragMode == "move";
        button("tool.drag", move ? "Drag moves the picture (click: drag rotates)" : L.p.view == "Slice" ? "Drag slides the cut (click: drag moves the picture)" : "Drag rotates (click: drag moves the picture)", Icon::Move, Tool::DragMode);
    }
    return v;
}

double atomPanelWidthEm(PanelId id) {
    switch (id) {
    case PanelId::Nucleus: return 18;
    case PanelId::Cloud: return 18;
    default: return 0;
    }
}

bool buildAtomPanel(PanelId id, AppState& app, Panel& p, bool& closeAfter) {
    atom::AtomLab& L = app.atom;
    switch (id) {
    case PanelId::Nucleus: {
        pTitle(p, "Nucleus");
        if (pRow(p, "nucleus.nucleus", "Nucleus", atom::toolOn(L, Tool::Nucleus) && !atom::toolOn(L, Tool::Nucleon))) { atom::pressTool(L, Tool::Nucleus); closeAfter = true; }
        if (pRow(p, "nucleus.nucleon", "Nucleon (one proton or neutron)", atom::toolOn(L, Tool::Nucleon))) { atom::pressTool(L, Tool::Nucleon); closeAfter = true; }
        pCaption(p, "Zoom in to a nucleon and click one to dive inside it.");
        return true;
    }
    case PanelId::Cloud: {
        pTitle(p, "Cloud");
        if (pRow(p, "cloud.cloud", "Cloud (side view)", L.p.view == "Cloud")) { atom::pressTool(L, Tool::Cloud); closeAfter = true; }
        if (pRow(p, "cloud.cloud3d", "Cloud 3D (turn it with the mouse)", L.p.view == "Cloud 3D")) { atom::pressTool(L, Tool::Cloud3D); closeAfter = true; }
        if (pRow(p, "cloud.slice", "Slice (cross-section of the cloud)", L.p.view == "Slice")) { atom::pressTool(L, Tool::Slice); closeAfter = true; }
        pCaption(p, "Each dot is one possible place to find the electron (one electron, non-relativistic model).");
        return true;
    }
    default: return false;
    }
}

}  // namespace ui::shellimpl
