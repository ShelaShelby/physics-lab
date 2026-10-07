// 0.15 stage 5a: the Atom Lab toolbar's buttons -> lab state (included by test_atom.cpp). WRITTEN BEFORE atom/atom_tools.cpp existed.
// References: the names of the views are the literal words of INVENTORY.md section 2 ("View: Orbit / Cloud / Cloud 3D / Slice / Chart of nuclides"); the old controls are called directly
// (the bar under the picture: zoomAtom, zoomNucleus, zoomNucleon, centre, toggleDrag, resetView; the panel's "Reset tilt, zoom and position": chartReset) and the button must leave the SAME state.
#include "atom/atom_tools.h"

static std::string tlSnap(const AtomLab& L) {
    char b[400];
    std::snprintf(b, sizeof b, "view=%s zoomTo=%d/%.6g focus=%d panTo=%d drag=%s yaw=%.6g pitch=%.6g oyaw=%.6g opitch=%.6g tilt=%.6g",
                  L.p.view.c_str(), L.s.zoomTo ? 1 : 0, L.s.zoomTo ? *L.s.zoomTo : 0.0, L.s.focus, L.s.panTo ? 1 : 0, L.s.dragMode.c_str(), L.s.yaw, L.s.pitch, L.s.oyaw, L.s.opitch, L.s.chartView.tilt);
    return b;
}
static AtomLab tlLab(const char* view) {
    AtomLab L;
    L.viewW = 1000; L.viewH = 700;
    if (std::string(view) != "Orbit") L.setParam("view", std::string(view));
    return L;
}

static void testAtomTools() {
    // the five views of the old View row: each is reachable by a button, and gives exactly that word
    struct V { atom::Tool t; const char* word; };
    const V views[] = {{atom::Tool::Atom, "Orbit"}, {atom::Tool::Cloud, "Cloud"}, {atom::Tool::Cloud3D, "Cloud 3D"}, {atom::Tool::Slice, "Slice"}, {atom::Tool::Chart, "Chart of nuclides"}};
    for (const char* from : {"Orbit", "Cloud", "Cloud 3D", "Slice", "Chart of nuclides"})
        for (const V& v : views) {
            AtomLab L = tlLab(from);
            atom::pressTool(L, v.t);
            checkTrue(std::string("toolbar: from ") + from + " the button for " + v.word + " gives that view", L.p.view == v.word);
            AtomLab ref = tlLab(from);
            ref.setParam("view", std::string(v.word));            // the old View row
            if (v.t == atom::Tool::Atom) ref.zoomAtom();          // (the Atom button is the view and the zoom out)
            checkTrue(std::string("toolbar: ") + from + " -> " + v.word + " leaves the state the old View row left", tlSnap(L) == tlSnap(ref));
        }
    // the zoom buttons = the bar's three buttons, called directly
    {
        AtomLab L = tlLab("Orbit"), ref = tlLab("Orbit");
        atom::pressTool(L, atom::Tool::Nucleus); ref.zoomNucleus();
        checkTrue("toolbar: Nucleus = the bar's Nucleus", tlSnap(L) == tlSnap(ref));
        checkTrue("toolbar: Nucleus asks for a zoom in (the nucleus is much smaller than the atom)", L.s.zoomTo && *L.s.zoomTo > 3.0);
        AtomLab n = tlLab("Orbit"), rn = tlLab("Orbit");
        atom::pressTool(n, atom::Tool::Nucleon); rn.zoomNucleon();
        checkTrue("toolbar: Nucleon = the bar's Nucleon", tlSnap(n) == tlSnap(rn));
        checkTrue("toolbar: Nucleon zooms in further than Nucleus and picks a nucleon", *n.s.zoomTo > *L.s.zoomTo && n.s.focus >= 0);
        AtomLab a = tlLab("Orbit");
        a.setParam("zoom", 5.0); a.s.focus = 2; a.s.zoomTo.reset();
        atom::pressTool(a, atom::Tool::Atom);
        checkTrue("toolbar: Atom zooms all the way out and lets go of the nucleon", a.s.zoomTo && *a.s.zoomTo == 0 && a.s.focus == -1);
    }
    {   // the chart has no zoom: from the chart the zoom buttons go to the Orbit view (else nothing would be seen); the cloud views keep their view
        AtomLab c = tlLab("Chart of nuclides");
        atom::pressTool(c, atom::Tool::Nucleus);
        checkTrue("toolbar: Nucleus from the chart goes to the Orbit view", c.p.view == "Orbit" && c.s.zoomTo);
        AtomLab k = tlLab("Cloud 3D"), rk = tlLab("Cloud 3D");
        atom::pressTool(k, atom::Tool::Nucleus); rk.zoomNucleus();
        checkTrue("toolbar: Nucleus in Cloud 3D = the old bar (the view stays)", k.p.view == "Cloud 3D" && tlSnap(k) == tlSnap(rk));
    }
    {   // Reset view
        AtomLab L = tlLab("Cloud 3D"), ref = tlLab("Cloud 3D");
        L.s.yaw = ref.s.yaw = 1.9; L.s.pitch = ref.s.pitch = -0.4;
        atom::pressTool(L, atom::Tool::ResetView); ref.resetView();
        checkTrue("toolbar: Reset view in Cloud 3D = the old button", tlSnap(L) == tlSnap(ref));
        checkTrue("toolbar: Reset view in Cloud 3D gives the start angles (0.6, 0.35)", L.s.yaw == 0.6 && L.s.pitch == 0.35);
        AtomLab o = tlLab("Orbit");
        o.s.oyaw = 0.8; o.s.opitch = 1.0;
        atom::pressTool(o, atom::Tool::ResetView);
        checkTrue("toolbar: Reset view in Orbit turns the orbit back flat", o.s.oyaw == 0 && std::fabs(o.s.opitch - 3.14159265358979 / 2) < 1e-9);
        AtomLab f = tlLab("Orbit"), rf = tlLab("Orbit");
        atom::pressTool(f, atom::Tool::ResetView);
        checkTrue("toolbar: Reset view in an untouched Orbit view changes nothing (the old button was not shown)", tlSnap(f) == tlSnap(rf));
        AtomLab s = tlLab("Cloud"), rs = tlLab("Cloud");
        s.s.yaw = rs.s.yaw = 2.2;
        atom::pressTool(s, atom::Tool::ResetView);
        checkTrue("toolbar: Reset view in the Cloud side view leaves the 3D angles alone (the old button was not shown)", tlSnap(s) == tlSnap(rs));
        AtomLab ch = tlLab("Chart of nuclides"), rch = tlLab("Chart of nuclides");
        ch.setChartTilt(0.7); rch.setChartTilt(0.7);
        atom::pressTool(ch, atom::Tool::ResetView); rch.chartReset();
        checkTrue("toolbar: Reset view on the chart = Reset tilt, zoom and position", tlSnap(ch) == tlSnap(rch) && ch.s.chartView.tilt != 0.7);
    }
    {   // Centre and Drag
        AtomLab L = tlLab("Orbit");
        checkTrue("toolbar: Centre is not shown while the picture is centred and not zoomed (as the old bar)", !atom::toolShown(L, atom::Tool::Centre) && !L.showCentre());
        L.setParam("zoom", 3.0);
        checkTrue("toolbar: Centre is shown once zoomed, as the old bar", atom::toolShown(L, atom::Tool::Centre) && L.showCentre());
        L.s.panTo.reset();
        atom::pressTool(L, atom::Tool::Centre);
        checkTrue("toolbar: Centre sends the picture back to the middle", L.s.panTo && L.s.panTo->x == 0 && L.s.panTo->y == 0);
        checkTrue("toolbar: Drag mode starts at rotate", L.s.dragMode == "rotate");
        atom::pressTool(L, atom::Tool::DragMode);
        checkTrue("toolbar: Drag mode toggles to move", L.s.dragMode == "move");
        atom::pressTool(L, atom::Tool::DragMode);
        checkTrue("toolbar: Drag mode toggles back to rotate", L.s.dragMode == "rotate");
        for (const char* v : {"Orbit", "Cloud 3D", "Slice"}) { AtomLab d = tlLab(v); checkTrue(std::string("toolbar: Drag is shown in ") + v + " (as the old bar)", atom::toolShown(d, atom::Tool::DragMode)); }
        for (const char* v : {"Cloud", "Chart of nuclides"}) { AtomLab d = tlLab(v); checkTrue(std::string("toolbar: Drag is not shown in ") + v + " (as the old bar)", !atom::toolShown(d, atom::Tool::DragMode)); }
        AtomLab r = tlLab("Cloud 3D");
        checkTrue("toolbar: Reset view is always shown", atom::toolShown(r, atom::Tool::ResetView) && atom::toolShown(tlLab("Chart of nuclides"), atom::Tool::ResetView));
    }
    {   // which button is lit
        AtomLab L = tlLab("Orbit");
        checkTrue("toolbar: in the Orbit view at the atom level Atom is lit, Nucleus, Cloud and Chart are not",
                  atom::toolOn(L, atom::Tool::Atom) && !atom::toolOn(L, atom::Tool::Nucleus) && !atom::toolOn(L, atom::Tool::Cloud) && !atom::toolOn(L, atom::Tool::Chart));
        L.s.layer = "Nucleus";
        checkTrue("toolbar: at the nucleus level Nucleus is lit and Atom is not", atom::toolOn(L, atom::Tool::Nucleus) && !atom::toolOn(L, atom::Tool::Atom) && !atom::toolOn(L, atom::Tool::Nucleon));
        L.s.layer = "Nucleon";
        checkTrue("toolbar: at the nucleon level Nucleus and Nucleon are lit", atom::toolOn(L, atom::Tool::Nucleus) && atom::toolOn(L, atom::Tool::Nucleon));
        // 0.15 stage 5l (changed on purpose, owner's bug: Cloud 3D lit "Cloud" as well): each cloud view lights ITS button only
        for (const char* v : {"Cloud", "Cloud 3D", "Slice"}) {
            AtomLab c = tlLab(v);
            const int lit = atom::toolOn(c, atom::Tool::Cloud) + atom::toolOn(c, atom::Tool::Cloud3D) + atom::toolOn(c, atom::Tool::Slice);
            const bool mine = std::string(v) == "Cloud" ? atom::toolOn(c, atom::Tool::Cloud) : std::string(v) == "Cloud 3D" ? atom::toolOn(c, atom::Tool::Cloud3D) : atom::toolOn(c, atom::Tool::Slice);
            checkTrue(std::string("toolbar: in ") + v + " exactly its own button is lit (and Atom, Chart are not)", lit == 1 && mine && !atom::toolOn(c, atom::Tool::Atom) && !atom::toolOn(c, atom::Tool::Chart));
        }
        AtomLab ch = tlLab("Chart of nuclides");
        checkTrue("toolbar: on the chart only Chart is lit", atom::toolOn(ch, atom::Tool::Chart) && !atom::toolOn(ch, atom::Tool::Atom) && !atom::toolOn(ch, atom::Tool::Cloud) && !atom::toolOn(ch, atom::Tool::Nucleus));
    }
}
