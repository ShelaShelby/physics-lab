// 0.15 stage 5a: the Atom Lab's toolbar and the two small panels under its carets (Nucleus: the level; Cloud: which cloud view). What each button does is in atom/atom_tools.* (tested without the app).
// Play / pause is in the element chip (stage 4; Space too), not on the toolbar. View options, marks, photons, the element chip's periodic table, the card and the legend come in 5b.
#include <algorithm>
#include <cmath>
#include <map>

#include "atom/atom_core.h"
#include "atom/atom_learn.h"
#include "atom/atom_tools.h"
#include "atom/dirac.h"
#include "orbit/format.h"
#include "ui/atom_panel.h"
#include "ui/fonts.h"
#include "ui/shell_internal.h"
#include "ui/shell_test.h"

namespace ui::shellimpl {

using namespace sh;
using atom::Tool;
using app::shell::Metrics;

std::vector<ToolItem> atomTools(AppState& app, bool all) {
    atom::AtomLab& L = app.atom;
    std::vector<ToolItem> v;
    const auto button = [&](const char* id, const char* tip, Icon icon, Tool t) {
        ToolItem it; it.kind = ToolItem::Button; it.id = id; it.tip = tip; it.icon = icon; it.on = atom::toolOn(L, t);
        it.act = [&L, t] { atom::pressTool(L, t); };
        v.push_back(it);
    };
    button("tool.atom", "Atom: the electron and the whole atom (Orbit view). The nucleus zoom is in Controls › View", Icon::Atom, Tool::Atom);
    v.back().on = L.p.view == "Orbit";                    // (5l: the Nucleus button left the toolbar (it is in Controls › View); Atom stays lit at every level of the Orbit view, as the view is still Orbit)
    button("tool.cloud", "Cloud: the electron as a probability cloud, seen from the side (it cannot be turned)", Icon::Cloud, Tool::Cloud);
    button("tool.cloud3d", "Cloud 3D: the electron cloud in three dimensions; drag to turn it", Icon::Cloud3D, Tool::Cloud3D);
    button("tool.slice", "Slice: a thin flat cut through the cloud, seen face-on", Icon::Slice, Tool::Slice);
    button("tool.chart", "Chart of nuclides: every nuclide on one map", Icon::Chart, Tool::Chart);
    v.push_back(separator());
    button("tool.reset", L.chartOn() ? "Reset view: tilt, zoom and position of the chart" : "Reset view: turn the picture back", Icon::Reset, Tool::ResetView);
    // The optional buttons have slots of their own at the right end: Drag first, Centre after it. A slot whose button is not shown stays empty, so a button that appears never moves one
    // that is already there (stage 5g).
    const bool drag = all || atom::toolShown(L, Tool::DragMode), centre = all || atom::toolShown(L, Tool::Centre);
    if (drag) {
        const bool move = L.s.dragMode == "move";
        button("tool.drag", move ? "Drag moves the picture (click: drag rotates)" : L.p.view == "Slice" ? "Drag slides the cut (click: drag moves the picture)" : "Drag rotates (click: drag moves the picture)", Icon::Move, Tool::DragMode);
    } else if (centre) v.push_back(ToolItem{});                                       // the empty slot (no id: nothing is drawn)
    if (centre) button("tool.centre", "Centre: move the picture back to the middle", Icon::Target, Tool::Centre);
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
        const int z = periodicTableCells(g, L.p.Z, cText(), lookM1() ? cText() : cAccent(), cMuted());
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


// ---- stage 5d: the old panel's setting rows, drawn with the new UI's controls. Same key, same values (atomChoices), same call (AtomLab::setParam) as the old row. ----
// A setting with a few values: its name, the values' words on segmented buttons (packed into rows that fit the panel), the chosen value's note under them. Each part is also noted as
// "atom.<key>=<value>" for the automatic checks.
static void choiceRow(AppState& app, Panel& p, const char* key, const std::string& label, const ui::AtomChoices& opts, const std::string& cur, std::size_t maxPerRow = 5) {
    const Metrics& m = F().m;
    pHeading(p, label);
    std::string note;
    std::vector<std::string> words(opts.size());
    for (std::size_t i = 0; i < opts.size(); i++) { const auto [t, sub] = ui::atomSplitLabel(opts[i].second); words[i] = displayable(t); if (opts[i].first == cur) note = sub; }
    int rowNo = 0;
    for (std::size_t i = 0; i < opts.size();) {
        std::size_t j = i;
        float need = 0;
        while (j < opts.size() && j - i < maxPerRow) { const float w = width(Type::Body, false, words[j]) + 2.0f * m.s8; if (j > i && need + w > p.w) break; need += w; j++; }
        std::vector<std::string> labels(words.begin() + static_cast<long>(i), words.begin() + static_cast<long>(j)), names;
        int sel = -1;
        for (std::size_t k = i; k < j; k++) { names.push_back(std::string("atom.") + key + "=" + opts[k].first); if (opts[k].first == cur) sel = static_cast<int>(k - i); }
        int pick = sel;
        const std::string id = std::string("atom.") + key + ".row" + std::to_string(rowNo++);
        if (pSegmented(p, id.c_str(), labels, &pick, &names) && pick >= 0 && pick != sel) app.atom.setParam(key, opts[i + static_cast<std::size_t>(pick)].first);
        i = j;
    }
    if (!note.empty()) pCaption(p, displayable(note));
}
// A number setting: its name with the value, then the slider ("atom.<key>"). Whole-number settings change only when the whole number does (the old SliderInt); the others are rounded to
// their step on every change (the old SliderFloat).
static void numberRow(AppState& app, Panel& p, const char* key, const std::string& label, const std::string& value, double v, double lo, double hi, double step) {
    pStat(p, displayable(label), displayable(value));
    float fv = static_cast<float>(v);
    const std::string id = std::string("atom.") + key;
    if (pSlider(p, id.c_str(), &fv, static_cast<float>(lo), static_cast<float>(hi))) {
        if (step >= 1) { const double nv = static_cast<double>(std::lround(fv)); if (nv != v) app.atom.setParam(key, nv); }
        else app.atom.setParam(key, std::round(static_cast<double>(fv) / step) * step);
    }
    pGap(p, F().m.s8);
}
static void orientationRow(AppState& app, Panel& p, const std::string& orb, const char* pKey, const char* dKey, const char* fKey, const std::string& label) {
    const atom::AtomParams& P = app.atom.p;
    const int l = atom::orbitals()[atom::orbIndex(orb)].l;
    const std::string& cur = l == 1 ? (std::string(pKey) == "pAx" ? P.pAx : P.pAx2) : l == 2 ? (std::string(dKey) == "dAx" ? P.dAx : P.dAx2) : (std::string(fKey) == "fAx" ? P.fAx : P.fAx2);
    if (l == 1) choiceRow(app, p, pKey, label, ui::atomChoices(pKey), cur, 3);
    if (l == 2) choiceRow(app, p, dKey, label, ui::atomChoices(dKey), cur, 3);
    if (l == 3) choiceRow(app, p, fKey, label, ui::atomChoices(fKey), cur, 3);
}

// 0.15 stage 5f: a long note sits behind a row "Notes about this choice (show)" like the Orbit panel's; closed by default, kept while the app runs. Same words as before.
static std::map<std::string, bool> g_noteOpen;
static std::map<std::string, std::string> g_noteState;      // (for the automatic checks: key -> open / closed, printed as one note)
static void notesFold(Panel& p, const char* key, const std::vector<std::string>& notes) {
    bool& open = g_noteOpen[key];
    if (pRow(p, (std::string("atom.notes.") + key).c_str(), open ? "Notes about this choice  (hide)" : "Notes about this choice  (show)")) open = !open;
    if (shellLogOn()) g_noteState[key] = open ? "open" : "closed";
    if (!open) return;
    for (const std::string& n : notes) { pCaption(p, displayable(n)); if (shellLogOn()) auditText("ctlnote", px(Type::Caption), displayable(n)); }
}

// The View selector (stage 5f): the seven views in one compact control (two rows of segmented buttons, the current one highlighted). Each part does what the old row / toolbar button
// does (atom::pressTool). The parts are noted by the ids the plain list used ("nucleus.nucleus", "cloud.slice" ...).
static void viewSelector(Panel& p, atom::AtomLab& L) {
    struct V { const char* label; const char* id; const char* shortName; Tool tool; };
    static const V row1[] = {{"Atom", "view.atom", "atom", Tool::Atom}, {"Nucleus", "nucleus.nucleus", "nucleus", Tool::Nucleus}, {"Nucleon", "nucleus.nucleon", "nucleon", Tool::Nucleon}, {"Chart", "view.chart", "chart", Tool::Chart}};
    static const V row2[] = {{"Cloud", "cloud.cloud", "cloud", Tool::Cloud}, {"Cloud 3D", "cloud.cloud3d", "cloud3d", Tool::Cloud3D}, {"Slice", "cloud.slice", "slice", Tool::Slice}};
    const char* cur = L.chartOn() ? "chart" : L.p.view == "Slice" ? "slice" : L.p.view == "Cloud 3D" ? "cloud3d" : L.p.view == "Cloud" ? "cloud"
                      : atom::toolOn(L, Tool::Nucleon) ? "nucleon" : atom::toolOn(L, Tool::Nucleus) ? "nucleus" : "atom";
    const auto row = [&](const V* v, int n, const char* id) {
        std::vector<std::string> labels, names;
        int sel = -1;
        for (int i = 0; i < n; i++) { labels.push_back(v[i].label); names.push_back(v[i].id); if (std::string(v[i].shortName) == cur) sel = i; }
        int pick = sel;
        if (shellLogOn()) { const ImVec2 o = here(p); auditRect(id, o.x, o.y, p.w, static_cast<float>(F().m.row)); }
        if (pSegmented(p, id, labels, &pick, &names) && pick >= 0 && pick != sel) atom::pressTool(L, v[pick].tool);
    };
    row(row1, 4, "atom.viewsel1");
    row(row2, 3, "atom.viewsel2");
    if (shellLogOn()) auditNote(std::string("viewsel ") + cur);
}

// ---- the Controls panel (stage 5c): what the Nucleus ▾ and Cloud ▾ panels held, in a "View" section. Stage 5d: the old panel's view options, in the sections Orbital, Cloud, Orbit,
// Scale and motion, Nucleus and Chart, each shown when the old panel showed its rows (atom_panel.cpp, atomPanelContents). Marks and photons come in 5e. ----
void atomControls(AppState& app, Panel& p, const SectionFn& section) {
    atom::AtomLab& L = app.atom;
    const Metrics& m = F().m;
    g_noteState.clear();
    struct NoteLine { ~NoteLine() { if (shellLogOn()) { std::string t = "atomnotes"; for (const auto& [k, v] : g_noteState) t += " " + k + "=" + v; auditNote(t); } } } noteLine;   // (for the automatic checks: which folds are drawn, open or closed)
    if (section("view", "View", true)) {
        viewSelector(p, L);
        if (atom::toolShown(L, Tool::BuildAnother) && pButton(p, "nucleus.build", "Build another")) atom::pressTool(L, Tool::BuildAnother);
        if (atom::toolShown(L, Tool::Reshuffle) && pButton(p, "nucleus.reshuffle", "Reshuffle nucleons")) atom::pressTool(L, Tool::Reshuffle);
        if (pButton(p, "atom.restart", "Restart atom")) L.restart();     // 0.16 stage 1a: the old panel's Reset (a fresh atom, the same settings)
        notesFold(p, "view", {"Zoom in to a nucleon and click one to dive inside it.", "Each dot is one possible place to find the electron (one electron, non-relativistic model)."});
    }
    const atom::AtomParams& P = L.p;
    const bool orbit = P.view == "Orbit", chart = L.chartOn();
    if (!chart && section("orbital", "Orbital", true)) {                 // old: Orbital and Orientation in the cloud views (not the chart); Energy level in the Orbit view
        if (!orbit) {
            choiceRow(app, p, "orb", P.super == "On" ? "First orbital" : "Orbital", ui::atomChoices("orb"), P.orb);
            orientationRow(app, p, P.orb, "pAx", "dAx", "fAx", "Orientation");
        } else numberRow(app, p, "n", "Energy level (n)", std::to_string(P.n), P.n, 1, 4, 1);
        notesFold(p, "orbital", {ui::atomNote("energy")});
    }
    if (!orbit && !chart && section("cloud", "Cloud", true)) {                      // old: cloud style, density and superposition whenever the view is not Orbit; the cut in Slice
        choiceRow(app, p, "cstyle", "Cloud style", ui::atomChoices("cstyle"), P.cstyle);
        choiceRow(app, p, "cdens", "Cloud density", ui::atomChoices("cdens"), P.cdens);
        choiceRow(app, p, "super", "Superposition", ui::atomChoices("super"), P.super);
        if (P.super == "On") {
            choiceRow(app, p, "orb2", "Second orbital", ui::atomChoices("orb2"), P.orb2);
            orientationRow(app, p, P.orb2, "pAx2", "dAx2", "fAx2", "Second orientation");
            numberRow(app, p, "mix", "Mix (share of the second orbital)", fmt::jsLocale(std::round(P.mix)) + " %", P.mix, 0, 100, 1);
            numberRow(app, p, "speed", "Playback speed (beat speed)", std::to_string(P.speed), P.speed, 1, 30, 1);
            notesFold(p, "super", {ui::atomNote("super")});
        }
        if (P.view == "Slice") {
            choiceRow(app, p, "cutAxis", "Cut direction", ui::atomChoices("cutAxis"), P.cutAxis);
            numberRow(app, p, "cut", "Cut position (0 = through the nucleus)", fmt::jsString(P.cut), P.cut, -1, 1, 0.01);
        }
    }
    if (orbit && section("orbit", "Orbit", true)) choiceRow(app, p, "ostyle", "Orbit style", ui::atomChoices("ostyle"), P.ostyle);
    if (!chart && section("scale", "Scale and motion", true)) {          // old: View & zoom (not on the chart) and the Orbit view's Playback speed
        if (L.showSpin()) { bool spin = L.s.spin; if (pCheck(p, "atom.spin", "Spin (the cloud turns by itself)", &spin)) L.toggleSpin(); pGap(p, m.s4); }
        choiceRow(app, p, "scale", "Scale", ui::atomChoices("scale"), P.scale);
        numberRow(app, p, "zoom", "Zoom (log scale)", "×" + fmt::jsLocale(atom::zoomFactorShown(P.zoom)), P.zoom, atom::kZoomMin, 7, 0.01);
        if (orbit) numberRow(app, p, "speed", "Playback speed", std::to_string(P.speed), P.speed, 1, 30, 1);
    }
    if (!chart && P.zoom >= 3 && section("nucleus", "Nucleus", true)) {  // old: quark style and nucleon detail from zoom 4, nucleon edge from zoom 3
        if (P.zoom >= 4) {
            choiceRow(app, p, "qstyle", "Quark style", ui::atomChoices("qstyle"), P.qstyle);
            choiceRow(app, p, "detail", "Nucleon detail", ui::atomChoices("detail"), P.detail);
        }
        choiceRow(app, p, "style", "Nucleon edge", ui::atomChoices("style"), P.style);
    }
    if (chart && section("chart", "Chart", true)) {                       // old: the landscape's tilt and height, on the chart only
        numberRow(app, p, "tilt", "Tilt (flat chart to landscape)", std::to_string(static_cast<int>(std::lround(L.s.chartView.tilt * 100))) + " %", L.s.chartView.tilt, 0, 1, 0.01);
        choiceRow(app, p, "cheight", "Height means", ui::atomChoices("cheight"), L.s.chartView.height == atom::ChartHeightMode::Binding ? std::string("Binding energy per nucleon") : std::string("Half-life"));
        if (pButton(p, "atom.chartreset", "Reset tilt, zoom and position")) L.chartReset();
        notesFold(p, "chart", {ui::atomNote("chart")});
    }
    // ---- stage 5e: the marks (the 90 % outline and the phase colours when the view is not Orbit; the decay chain on the chart) and the photons, as in the old panel ----
    if ((!orbit || chart) && section("marks", "Marks", true)) {
        if (!orbit) {                                                      // old: 90% outline and Phase colours whenever the view is not Orbit
            choiceRow(app, p, "outline", "90% outline", ui::atomChoices("outline"), P.outline);
            choiceRow(app, p, "phase", "Phase colours", ui::atomChoices("phase"), P.phase);
            notesFold(p, "phase", {ui::atomNote("phase")});
        }
        if (chart) {                                                       // old: the "Decay chain of the chosen nuclide" block, on the chart only
            pHeading(p, "Decay chain of the chosen nuclide");
            pCheck(p, "atom.chain", "Show the chain on the chart", &L.s.showChain);
            pGap(p, m.s4);
            const atom::ChainGraph& g = L.decayGraph();
            for (const std::string& line : atom::chainSummary(g)) pCaption(p, displayable(line));
            if (g.nodes[0].stop == atom::ChainStop::Continues) {
                notesFold(p, "chain", {ui::atomNote("chain")});
                const atom::ChainWalk& w = L.s.walk;
                if (!w.done) {                                             // (the old buttons are greyed out when the walk is over; here they are left out)
                    if (pButton(p, "atom.walk", "Walk one step")) L.walkOnce();
                    if (pButton(p, "atom.walkauto", displayable(L.s.walkAuto ? "Pause auto-walk" : "Auto-walk (a step every " + atom::jsNum(atom::kWalkPaceS, 2) + " s)"))) L.s.walkAuto = !L.s.walkAuto;
                }
                if (pButton(p, "atom.walkrestart", "Restart walk")) L.walkRestart();
                for (std::size_t i = 0; i < w.steps.size(); i++) {
                    pBody(p, "Step " + std::to_string(i + 1) + ": " + displayable(atom::chainStepWords(w.steps[i])));
                    pCaption(p, displayable(atom::chainStepTime(w.steps[i])));
                }
                if (!w.steps.empty()) pCaption(p, displayable("Drawn lifetimes added up: " + atom::fmtLife(w.drawnS) + ". Screen time since the walk began: " + atom::jsNum(w.screenS, 3) + " s. These are not the same thing: the screen is not real time."));
                pCaption(p, displayable(w.done ? atom::chainWalkEndWords(w) : "The walker is at " + atom::chainNuclide(w.Z, w.N) + "."));
                pCaption(p, displayable(atom::chainTimeNote()));
            }
        }
    }
    if (section("photons", "Photons", true)) {
        if (!chart) {                                                      // old: no photons on the chart (it shows nuclei, not the electron)
            const float half = (p.w - static_cast<float>(m.s8)) / 2;
            const ImVec2 o = here(p);
            if (button("atom.emit", o, ImVec2(half, static_cast<float>(m.control)), displayable("↓ Emit photon"))) L.emit();
            if (button("atom.absorb", ImVec2(o.x + half + m.s8, o.y), ImVec2(half, static_cast<float>(m.control)), displayable("↑ Absorb photon"))) L.absorb();
            p.y += m.control + m.s4;
            if (pButton(p, "atom.auto", displayable(L.autoText()), Face::Secondary, L.s.autoPhotons)) L.toggleAuto();
            notesFold(p, "jump", {ui::atomNote(orbit ? "jumpOrbit" : "jumpCloud")});
        }
        // 0.16 stage 1a: the Dirac levels switch (old: the "Light spectrum" section); the lab's own spectrum picture is on the data sheet's Spectrum tab
        choiceRow(app, p, "dirac", displayable(atom::kDiracLabel), ui::atomChoices("dirac"), P.dirac);
        // Check a jump (old: the "Lifetimes and jumps (NIST)" card): the same rows, the same result lines (ui::atomJumpCheck, atom/lifetime.h, NIST ASD)
        choiceRow(app, p, "jfrom", "Check a jump: from (the upper shell)", ui::atomChoices("jfrom"), P.jfrom);
        choiceRow(app, p, "jto", "to (the lower shell)", ui::atomChoices("jto"), P.jto);
        const std::vector<std::string> lines = ui::atomJumpCheck(P.Z, P.jfrom, P.jto);
        for (const std::string& line : lines) pCaption(p, displayable(line));
        if (shellLogOn()) { std::string all = "jumpcheck"; for (const std::string& line : lines) all += " | " + line; auditNote(all); }
    }
}

}  // namespace ui::shellimpl
