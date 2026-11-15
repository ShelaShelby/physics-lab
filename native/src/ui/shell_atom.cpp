// 0.15 stage 5a: the Atom Lab's toolbar and the two small panels under its carets (Nucleus: the level; Cloud: which cloud view). What each button does is in atom/atom_tools.* (tested without the app).
// Play / pause is in the element chip (stage 4; Space too), not on the toolbar. View options, marks, photons, the element chip's periodic table, the card and the legend come in 5b.
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <map>

#include "atom/asd.h"
#include "atom/atom_core.h"
#include "atom/atom_learn.h"
#include "atom/alkali.h"
#include "atom/atom_tools.h"
#include "atom/dirac.h"
#include "atom/levels.h"
#include "atom/zeeman.h"
#include "orbit/format.h"
#include "ui/atom_panel.h"
#include "ui/fonts.h"
#include "ui/realest.h"
#include "ui/style_draw.h"
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
    button("tool.atom", "Orbit: the electron as a particle on an orbit (a model: a real electron is a cloud). Zoom to the nucleus in Controls › View", Icon::Atom, Tool::Atom);
    v.back().on = L.p.view == "Orbit";                    // (5l: the Nucleus button left the toolbar (it is in Controls › View); Atom stays lit at every level of the Orbit view, as the view is still Orbit)
    button("tool.cloud", "Cloud: the electron as a probability cloud, seen from the side (it cannot be turned)", Icon::Cloud, Tool::Cloud);
    button("tool.cloud3d", "Cloud 3D: the electron cloud in three dimensions; drag to turn it", Icon::Cloud3D, Tool::Cloud3D);
    button("tool.slice", "Slice: a flat cut through the cloud, seen face-on", Icon::Slice, Tool::Slice);
    button("tool.chart", "Chart of nuclides: every nuclide on one map", Icon::Chart, Tool::Chart);
    for (ToolItem& t : v) {                               // 0.22: the five views carry their name next to the icon (the icons alone were hard to tell apart)
        const std::string id = t.id;
        t.word = id == "tool.atom" ? "Orbit" : id == "tool.cloud" ? "Cloud" : id == "tool.cloud3d" ? "Cloud 3D" : id == "tool.slice" ? "Slice" : id == "tool.chart" ? "Chart" : "";
    }
    v.push_back(separator());
    button("tool.reset", L.chartOn() ? "Reset view: the turn, zoom and position of the chart (key R)" : "Reset view: turn the picture back (key R)", Icon::Reset, Tool::ResetView);
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
        pCaption(p, "Click an element to load its most common (or longest-lived) isotope. Dashed edges: no stable isotope (bismuth, whose half-life is over 10¹⁵ years, counts as practically stable).");   // night hunt 0.39.3: Bi-209 decays (NUBASE2020) yet has a solid edge (hasStable: over 10^15 years)
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
        for (std::size_t k = i; k < j; k++) real::mark("atom", key, opts[k].first, static_cast<int>(k - i));   // 0.25: the dot on the choice closest to reality
        if (pSegmented(p, id.c_str(), labels, &pick, &names) && pick >= 0 && pick != sel) app.atom.setParam(key, opts[i + static_cast<std::size_t>(pick)].first);
        i = j;
    }
    if (!note.empty()) pCaption(p, displayable(note));
}
// 0.47: "Put the outer electron in": one button per shell of this atom that has an orbital (AtomLab::valenceChoices), the current one chosen; a tap places the electron there with the
// jump's cross-fade and no photon (AtomLab::placeValence). Each button is noted as "atom.place=<shell>" for the automatic checks. Packed into rows as choiceRow does.
static void placeRow(AppState& app, Panel& p) {
    const Metrics& m = F().m;
    atom::AtomLab& L = app.atom;
    const std::vector<atom::AlkOrbital> sh = L.valenceChoices();
    if (sh.empty()) return;
    const atom::AlkOrbital cur = L.valenceNow();
    pHeading(p, "Put the outer electron in:");
    std::vector<std::string> words;
    for (const atom::AlkOrbital& o : sh) words.push_back(std::to_string(o.n) + "spdfg"[o.l]);
    int rowNo = 0;
    for (std::size_t i = 0; i < sh.size();) {
        std::size_t j = i;
        float need = 0;
        while (j < sh.size() && j - i < 8) { const float w = width(Type::Body, false, words[j]) + 2.0f * m.s8; if (j > i && need + w > p.w) break; need += w; j++; }
        std::vector<std::string> labels(words.begin() + static_cast<long>(i), words.begin() + static_cast<long>(j)), names;
        int sel = -1;
        for (std::size_t k = i; k < j; k++) { names.push_back("atom.place=" + words[k]); if (sh[k].n == cur.n && sh[k].l == cur.l) sel = static_cast<int>(k - i); }
        int pick = sel;
        const std::string id = "atom.place.row" + std::to_string(rowNo++);
        if (pSegmented(p, id.c_str(), labels, &pick, &names) && pick >= 0 && pick != sel) L.placeValence(sh[i + static_cast<std::size_t>(pick)].n, sh[i + static_cast<std::size_t>(pick)].l);
        i = j;
    }
    pCaption(p, "In a real atom the electron only moves when it takes in or gives out energy (light or a collision). Here you place it to see its shape.");
}
// 0.47.3 (PLAN_JUMPS.md part 2; owner decisions D2, D4, D6): the jump panel of a level atom (atom/levels.h). The levels with a cloud grouped configuration -> term -> J (one row of J
// buttons per term, "atom.level=<index>"; a tap places the atom there, no photon); the jumps from the level now, strongest first (g A), each with its rate and grade or "rate not
// known" ("atom.line=<index>": a tap makes that jump); the lined-up / round switch ("atom.shape"); the other measured levels in a fold, with their NIST energy and "no cloud".
// D6 (owner decision, 9 Nov 2026): the lined-up state M_J = J (default) or the round free atom (the average over M_J), for every atom drawn from GRASP2018 clouds
static void shapeRow(AppState& app, Panel& p) {
    atom::AtomLab& L = app.atom;
    std::vector<std::string> labels = {"Lined up (M_J = J)", "Round (free atom)"}, names = {"atom.shape=lined", "atom.shape=round"};
    int pick = L.s.round ? 1 : 0;
    const int was = pick;
    if (pSegmented(p, "atom.shape", labels, &pick, &names) && pick != was) L.setRound(pick == 1);
}
static std::string jPretty(int j2) { return j2 % 2 ? std::to_string(j2) + "/2" : std::to_string(j2 / 2); }
static void levelRows(AppState& app, Panel& p) {
    atom::AtomLab& L = app.atom;
    const int Z = L.p.Z;
    const std::vector<atom::JLevel>& lv = atom::jumpLevels(Z);
    const std::vector<int> cl = atom::cloudLevelList(Z);
    pHeading(p, "Levels (configuration → term → J)");
    pCaption(p, "Tap one to put the atom in that level.");
    std::vector<std::string> confs;
    for (const int i : cl) if (std::find(confs.begin(), confs.end(), lv[std::size_t(i)].conf) == confs.end()) confs.push_back(lv[std::size_t(i)].conf);
    int rowNo = 0;
    for (const std::string& c : confs) {
        std::vector<std::string> terms;
        for (const int i : cl) if (lv[std::size_t(i)].conf == c && std::find(terms.begin(), terms.end(), lv[std::size_t(i)].term) == terms.end()) terms.push_back(lv[std::size_t(i)].term);
        for (const std::string& t : terms) {
            std::vector<int> js;
            for (const int i : cl) if (lv[std::size_t(i)].conf == c && lv[std::size_t(i)].term == t) js.push_back(i);
            std::sort(js.begin(), js.end(), [&](int a, int b) { return lv[std::size_t(a)].j2 < lv[std::size_t(b)].j2; });
            std::vector<std::string> labels, names;
            int sel = -1;
            for (std::size_t k = 0; k < js.size(); k++) {
                labels.push_back((k == 0 ? atom::confPretty(c) + " " + atom::termPretty(t) + " J=" : "J=") + jPretty(lv[std::size_t(js[k])].j2));
                names.push_back("atom.level=" + std::to_string(js[k]));
                if (js[k] == L.levelNow()) sel = static_cast<int>(k);
            }
            int pick = sel;
            const std::string id = "atom.level.row" + std::to_string(rowNo++);
            if (pSegmented(p, id.c_str(), labels, &pick, &names) && pick >= 0 && pick != sel) L.placeLevel(js[static_cast<std::size_t>(pick)]);
        }
    }
    pCaption(p, "In a real atom the electrons only change level when the atom takes in or gives out energy (light or a collision). Here you place it to see its shape.");
    // the jumps from the level now
    const std::vector<atom::JLine>& ln = atom::jumpLines(Z);
    std::vector<int> ks;
    for (const int k : atom::linesAt(Z, L.levelNow())) if (atom::jumpOffered(Z, k)) ks.push_back(k);
    const auto gA = [&](int k) { const atom::JLine& q = ln[std::size_t(k)]; return std::isfinite(q.rate()) ? (lv[std::size_t(q.hi)].j2 + 1) * q.rate() : -1.0; };   // (NIST's A, else this app's, D4)
    std::stable_sort(ks.begin(), ks.end(), [&](int a, int b) { return gA(a) > gA(b); });
    if (ks.empty()) pCaption(p, "No measured line joins this level to another level with a cloud.");
    else { pHeading(p, "Jumps from this level"); pCaption(p, displayable("From " + atom::levelName(Z, L.levelNow()) + ", strongest first:")); }
    for (const int k : ks) {
        const atom::JLine& q = ln[std::size_t(k)];
        const bool down = q.hi == L.levelNow();
        const int other = down ? q.lo : q.hi;
        char nm[32];
        std::snprintf(nm, sizeof nm, "%.3f nm", q.nm());
        const std::string label = std::string(down ? "↓ " : "↑ ") + nm + " → " + atom::levelName(Z, other);
        const std::string id = "atom.line=" + std::to_string(k);
        if (pButton(p, id.c_str(), displayable(label))) { if (down) L.emitLevelLine(k); else L.absorbLevelLine(k); }
        pCaption(p, displayable(std::string(down ? "emit: " : "absorb: ") + atom::rateText(q)));
    }
    if (!ks.empty()) pCaption(p, "A: the jump's rate (transition probability), NIST's value with its accuracy grade; where NIST has none, this app's own GRASP2018 value, marked \"computed\"; \"rate not known\": neither.");
    shapeRow(app, p);
}
// The measured levels without a cloud (D2): in a fold, with NIST's energy and the reason
static std::vector<std::string> levelListNotes(int Z) {
    std::vector<std::string> out;
    const std::vector<atom::JLevel>& lv = atom::jumpLevels(Z);
    int n = 0;
    for (const atom::JLevel& L : lv) n += L.cloud && atom::levelCloud(Z, static_cast<int>(&L - lv.data())) ? 1 : 0;
    out.push_back("NIST lists " + std::to_string(lv.size()) + " named levels of this atom; " + std::to_string(n) + " have a cloud computed (the levels of the 30 strongest lines with a known rate and the lowest level of each of the 10 lowest configurations). The others, with NIST's energy:");
    for (int i = 0; i < static_cast<int>(lv.size()); i++) {
        const atom::JLevel& L = lv[std::size_t(i)];
        if (atom::levelCloud(Z, i)) continue;
        out.push_back(atom::levelName(Z, i) + "  " + L.cmText + " cm⁻¹" + (L.calc ? " (calculated)" : "") + "  ·  no cloud computed for this level" + (L.d2 && !L.why.empty() ? ": " + L.why : ""));
    }
    return out;
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
    if (pRow(p, (std::string("atom.notes.") + key).c_str(), open ? "Hide notes" : "Show notes")) open = !open;
    if (shellLogOn()) g_noteState[key] = open ? "open" : "closed";
    if (!open) return;
    for (const std::string& n : notes) { pCaption(p, displayable(n)); if (shellLogOn()) auditText("ctlnote", px(Type::Caption), displayable(n)); }
}

// The View selector (stage 5f): the seven views in one compact control (two rows of segmented buttons, the current one highlighted). Each part does what the old row / toolbar button
// does (atom::pressTool). The parts are noted by the ids the plain list used ("nucleus.nucleus", "cloud.slice" ...).
static void viewSelector(Panel& p, atom::AtomLab& L) {
    // 0.22 stage 6: two separate choices, each with its own caption: the picture (the same five as the toolbar, with the same names) and how far in it is zoomed. Before, "Atom" meant
    // the Orbit picture in the toolbar but the atom-sized zoom here, and the two rows mixed pictures and zoom levels.
    struct V { const char* label; const char* id; const char* shortName; Tool tool; };
    static const V row1[] = {{"Orbit", "view.atom", "orbit", Tool::Atom}, {"Cloud", "cloud.cloud", "cloud", Tool::Cloud}, {"Cloud 3D", "cloud.cloud3d", "cloud3d", Tool::Cloud3D}, {"Slice", "cloud.slice", "slice", Tool::Slice}, {"Chart", "view.chart", "chart", Tool::Chart}};
    static const V row2[] = {{"Atom", "zoom.atom", "atom", Tool::ZoomAtom}, {"Nucleus", "nucleus.nucleus", "nucleus", Tool::Nucleus}, {"Nucleon", "nucleus.nucleon", "nucleon", Tool::Nucleon}};
    const char* cur = L.chartOn() ? "chart" : L.p.view == "Slice" ? "slice" : L.p.view == "Cloud 3D" ? "cloud3d" : L.p.view == "Cloud" ? "cloud" : "orbit";
    const std::string& ly = L.s.layer;                    // (the zoom level in every picture, not only Orbit)
    const char* zoom = ly == "Nucleon" || ly == "Quarks" ? "nucleon" : ly == "Nucleus" ? "nucleus" : "atom";
    const auto row = [&](const V* v, int n, const char* id, const char* now) {
        std::vector<std::string> labels, names;
        int sel = -1;
        for (int i = 0; i < n; i++) { labels.push_back(v[i].label); names.push_back(v[i].id); if (std::string(v[i].shortName) == now) sel = i; }
        int pick = sel;
        if (shellLogOn()) { const ImVec2 o = here(p); auditRect(id, o.x, o.y, p.w, static_cast<float>(F().m.row)); }
        for (int i = 0; i < n; i++) real::mark("atom", "picture", v[i].label, i);
        if (pSegmented(p, id, labels, &pick, &names) && pick >= 0 && pick != sel) atom::pressTool(L, v[pick].tool);
    };
    pCaption(p, "Picture");
    row(row1, 5, "atom.viewsel1", cur);
    if (!L.chartOn()) { pCaption(p, "Zoom to"); row(row2, 3, "atom.viewsel2", zoom); }
    if (shellLogOn()) auditNote(std::string("viewsel ") + cur + " zoom=" + zoom);
}

// ---- the Controls panel (stage 5c): what the Nucleus ▾ and Cloud ▾ panels held, in a "View" section. Stage 5d: the old panel's view options, in the sections Orbital, Cloud, Orbit,
// Scale and motion, Nucleus and Chart, each shown when the old panel showed its rows (atom_panel.cpp, atomPanelContents). Marks and photons come in 5e. ----

// 0.40: the spectrum strip in the Photons section (the owner wanted the spectrum in the main view, not only on the data sheet). Electrons Hydrogen-like: the lab's own lines
// (one electron round the nucleus, reduced mass: atom::specLines); a line between levels the lab has (n <= 4) is drawn full height and a click makes that jump (AtomLab::emitLine).
// Electrons All: the MEASURED lines of the neutral atom (NIST ASD, atom::asdAtom), shown, not clickable; 0.46: for Li, Na, K, Rb, Cs (AtomLab::valenceOn) the outer electron's measured
// E1 lines (atom::alkaliLines, NIST ASD), brighter = larger A, and a click makes that jump with that fine-structure component (AtomLab::emitValenceLine). Log scale of the wavelength.
// 0.40: how many dots the whole atom's cloud keeps, and per electron (the 0.40 research: the noise of a shell falls as 1 / sqrt(its dots))
static std::string manyDotsWords(const atom::AtomLab& L) {
    const int n = L.cloudCap(), Z = std::max(1, L.p.Z);
    std::string t = fmt::commas(n) + " dots, about " + fmt::commas(n / Z) + " per electron (a real electron is a smooth cloud: more dots come closer to it). "
                    "The brightness is scaled so that every atom's cloud is equally bright.";
    if (n >= atom::AtomLab::kManyDotsCeiling * atom::AtomLab::capFor(L.p.cdens)) t += " That is the most this density setting keeps.";
    return t;
}

static void spectrumStrip(AppState& app, Panel& p) {
    atom::AtomLab& L = app.atom;
    const Metrics& m = F().m;
    const bool val = L.manyOn() && L.valenceOn(), many = L.manyOn() && !val;
    pCaption(p, val ? "Outer electron's lines, measured (NIST ASD, log scale): click one and the outer electron makes that jump"
              : many && L.levelOn() ? "Spectrum of the neutral atom, measured (NIST ASD); its jumps are the buttons above"   // 0.47.3
              : many ? "Spectrum of the neutral atom, measured (NIST ASD); the one-electron jumps need Electrons: Hydrogen-like"
                     : "Spectrum (log scale): click a full-height line and the electron makes that jump", true);
    struct Ln { double lam; int hi, lo; float alpha; bool click; };
    std::vector<Ln> ls;
    double lo = 0, hi = 0;
    if (val) {
        double amax = 0;
        for (const atom::AlkLine& q : atom::alkaliLines(L.p.Z)) if (std::isfinite(q.aki)) amax = std::max(amax, q.aki);
        for (const atom::AlkLine& q : atom::alkaliLines(L.p.Z)) {
            const double nm = q.nm();
            if (!(nm >= atom::kAsdAtomLowNm && nm <= atom::kAsdAtomHighNm)) continue;
            const float a = std::isfinite(q.aki) && amax > 0 ? float(0.35 + 0.65 * std::log10(std::max(1.0, q.aki)) / std::log10(amax)) : 0.5f;   // (brighter = larger A; no A in the ASD: middle)
            ls.push_back({nm, q.nHi * 10 + q.lHi, q.nLo * 10 + q.lLo, a, true});
        }
        lo = atom::kAsdAtomLowNm; hi = atom::kAsdAtomHighNm;
    } else if (!many) {
        for (const atom::SpecLine& l : atom::specLines(L.p.Z, L.p.N)) {
            if (l.hi == 0 || l.lo > 3 || l.hi > 12) continue;                    // (the series limits, and lines past the third series or above n = 12, crowd the strip)
            const bool c = l.hi <= atom::AtomLab::kLineMaxN;
            ls.push_back({l.lam, l.hi, l.lo, c ? 1.0f : 0.45f, c});
        }
        lo = 1e300; hi = 0;
        for (const Ln& l : ls) { lo = std::min(lo, l.lam); hi = std::max(hi, l.lam); }
        lo *= 0.9; hi *= 1.1;
    } else {
        const atom::AsdSpectrum& A = atom::asdAtom(L.p.Z);
        double imax = 0;
        for (int i = 0; i < A.count; i++) { const double v = atom::asdIntensity(A.lines[i]); if (std::isfinite(v)) imax = std::max(imax, v); }
        for (int i = 0; i < A.count; i++) {
            const double v = atom::asdIntensity(A.lines[i]);
            ls.push_back({A.lines[i].nm, 0, 0, std::isfinite(v) && imax > 0 ? float(0.35 + 0.65 * std::log1p(v) / std::log1p(imax)) : 0.6f, false});   // (brighter = a larger ASD relative intensity; none given: middle)
        }
        lo = atom::kAsdAtomLowNm; hi = atom::kAsdAtomHighNm;
    }
    const ImVec2 o = here(p);
    const float H = 30.0f * static_cast<float>(m.s8) / 8.0f, W = p.w;
    if (many && ls.empty()) { const std::string why = atom::asdMissingText(atom::asdAtom(L.p.Z)); pCaption(p, why.empty() ? "No lines in the range." : why, true); return; }   // 0.41: the reason (it said \"No lines in the range\" for elements with no NIST data)
    if (ls.empty() || !(hi > lo) || lo <= 0) { pCaption(p, "No lines in the range.", true); return; }
    const double la = std::log(lo), lb = std::log(hi);
    const auto X = [&](double lam) { return o.x + float((std::log(lam) - la) / (lb - la)) * W; };
    ImDrawList* dl = p.dl;
    dl->AddRectFilled(o, ImVec2(o.x + W, o.y + H), cFace(), 4);
    if (atom::kVisibleHiNm > lo && atom::kVisibleLoNm < hi)                   // the visible band, in its colours, faint
        for (float x = std::max(o.x, X(atom::kVisibleLoNm)); x < std::min(o.x + W, X(atom::kVisibleHiNm)); x += 1) {
            const double lam = std::exp(la + (x - o.x) / W * (lb - la));
            const atom::Rgb c = atom::lamColor(lam);
            dl->AddRectFilled(ImVec2(x, o.y), ImVec2(x + 1, o.y + H), IM_COL32(c.r, c.g, c.b, 60));
        }
    const ImVec2 mp = ImGui::GetMousePos();
    const bool over = mp.x >= o.x && mp.x <= o.x + W && mp.y >= o.y && mp.y <= o.y + H;
    int near = -1; float best = 6;
    for (int i = 0; i < int(ls.size()); i++) {
        const Ln& l = ls[std::size_t(i)];
        const float x = X(l.lam);
        const bool vis = atom::isVisibleNm(l.lam);
        const atom::Rgb c = atom::lamColor(l.lam);
        const ImU32 col = vis ? IM_COL32(c.r, c.g, c.b, int(255 * l.alpha)) : cText(l.alpha);
        const float top = l.click || many ? o.y + 2 : o.y + H * 0.45f;
        dl->AddRectFilled(ImVec2(x - (l.click ? 1.0f : 0.5f), top), ImVec2(x + (l.click ? 1.0f : 0.5f), o.y + H - 2), col);
        if (l.click) {
            char nm[40];
            if (val) std::snprintf(nm, sizeof nm, "atom.vline.%.3f", l.lam); else std::snprintf(nm, sizeof nm, "atom.line.%d-%d", l.hi, l.lo);
            noteRect(nm, x - 3, o.y, 6, H);
        }
        if (over && std::fabs(mp.x - x) < best) { best = std::fabs(mp.x - x); near = i; }
    }
    for (const atom::SeenLine& sl : L.s.lines) {                              // ▼ the lines your jumps made
        if (many || sl.lam < lo || sl.lam > hi) continue;
        const float x = X(sl.lam);
        dl->AddTriangleFilled(ImVec2(x - 4, o.y - 1), ImVec2(x + 4, o.y - 1), ImVec2(x, o.y + 5), cAccent());
    }
    if (near >= 0) {
        const Ln& l = ls[std::size_t(near)];
        static const char* const series[6] = {"", "Lyman", "Balmer", "Paschen", "Brackett", "Pfund"};   // (0.46: to Pfund; lo = 4 read past the old array)
        static const char* const greek[5] = {"α", "β", "γ", "δ", "ε"};
        std::string t;
        static const char kL[] = "spdf";
        const auto vn = [&](int c) { return std::to_string(c / 10) + kL[std::min(3, c % 10)]; };
        if (val) t = vn(l.hi) + " → " + vn(l.lo) + ", " + atom::fmtLam(l.lam) + " (" + atom::vacuumBand(l.lam) + "; measured, NIST ASD) · click: the outer electron makes this jump";
        else if (many) t = std::string(atom::asdAtom(L.p.Z).name) + ", " + atom::fmtLam(l.lam) + atom::bandTag(l.lam) + " (measured, NIST ASD)";
        else {
            t = (L.p.Z == 1 && l.hi - l.lo <= 5 && l.lo <= 5 ? std::string(series[l.lo]) + "-" + greek[l.hi - l.lo - 1] + ": " : std::string()) + "n = " + std::to_string(l.hi) + " → " + std::to_string(l.lo) + ", " + atom::fmtLam(l.lam) + atom::bandTag(l.lam);
            t += l.click ? " · click: the electron makes this jump" : " · the lab's levels go up to n = 6";
        }
        ui::SetTooltip("%s", displayable(t).c_str());
        if (l.click && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            if (val) L.emitValenceLine(l.hi / 10, l.hi % 10, l.lo / 10, l.lo % 10, l.lam); else if (!many) L.emitLine(l.hi, l.lo);
        }
    }
    if (shellLogOn()) {                                                     // (for the automatic checks: what the strip shows and the lines your jumps made)
        std::string t = std::string("specstrip mode=") + (val ? "valence" : many ? "atom" : "lab") + " lines=" + std::to_string(ls.size()) + " seen=";
        char b[24]; for (const atom::SeenLine& sl : L.s.lines) { std::snprintf(b, sizeof b, "%.3f,", sl.lam); t += b; }
        auditNote(t);
    }
    text(dl, Type::Caption, false, ImVec2(o.x, o.y + H + 2), cMuted(), displayable(atom::fmtLam(lo)));
    const std::string r = displayable(atom::fmtLam(hi));
    text(dl, Type::Caption, false, ImVec2(o.x + W - width(Type::Caption, false, r), o.y + H + 2), cMuted(), r);
    p.y += H + 2 + px(Type::Caption) + m.s8;
}


// 0.54.0: the "Magnetic field" section (atom/zeeman.h): the field on a log slider (0 = off, the default; 1 mT to 100 T) and typed, where the light is seen from, and the
// line shown split on the data sheet's Spectrum tab (atom.zline.*). With the field at 0 nothing else in the lab changes.
static void fieldSection(AppState& app, Panel& p) {
    namespace z = atom::zeeman;
    atom::AtomLab& L = app.atom;
    const atom::AtomParams& P = L.p;
    const Metrics& m = F().m;
    const int kind = L.zeemanKind();
    if (kind < 0) { pCaption(p, "With every electron shown, the field acts on the levels of the atoms that have jumps (Photons); this atom has none to split."); return; }
    char b[96];
    if (P.bfield > 0) std::snprintf(b, sizeof b, "%.4g T", P.bfield); else std::snprintf(b, sizeof b, "Off (0 T)");
    pStat(p, "Field", b);
    float v = P.bfield > 0 ? static_cast<float>(std::max(1.0, (std::log10(P.bfield) + 3) / 5 * 1000)) : 0.0f;
    if (pSlider(p, "atom.bfield", &v, 0.0f, 1000.0f)) {
        const double nb = v < 0.5f ? 0.0 : std::pow(10.0, -3 + 5 * static_cast<double>(v) / 1000);
        const double q = nb > 0 ? std::pow(10.0, std::floor(std::log10(nb)) - 2) : 1;   // three significant digits
        L.setParam("bfield", nb > 0 ? std::round(nb / q) * q : 0.0);
    }
    pCaption(p, "Log scale: 1 mT to 100 T; all the way left is off. Or type a field in tesla:");
    static char typed[32] = "";
    const ImVec2 o = here(p);
    ImGui::SetCursorScreenPos(o);
    if (inputField("##atom.bfield.text", typed, sizeof typed, "e.g. 1.5", p.w)) {
        char* end = nullptr;
        const double t = std::strtod(typed, &end);
        if (end != typed && std::isfinite(t) && t >= 0) L.setParam("bfield", t);
    }
    p.y += m.control + m.s4;
    choiceRow(app, p, "blook", "Look", ui::atomChoices("blook"), P.blook);
    const std::vector<z::LineRef>& lines = z::labLines(P.Z, P.N, kind);
    const int k = L.zeemanLine();
    if (k < 0) { pCaption(p, "No line of this atom's levels to split."); return; }
    const z::Line& ln = lines[std::size_t(k)].line;
    pStat(p, "Line shown split", displayable(ln.name));
    {
        const float w = (p.w - 2 * static_cast<float>(m.s8)) / 4;
        const ImVec2 q = here(p);
        if (button("atom.zline.prev", q, ImVec2(w, static_cast<float>(m.control)), "−", Face::Quiet, false)) L.setParam("zline", static_cast<double>(k > 0 ? k - 1 : static_cast<int>(lines.size()) - 1));
        noteItem("atom.zline.prev");
        if (button("atom.zline.next", ImVec2(q.x + w + m.s8, q.y), ImVec2(w, static_cast<float>(m.control)), "+", Face::Quiet, false)) L.setParam("zline", static_cast<double>((k + 1) % static_cast<int>(lines.size())));
        noteItem("atom.zline.next");
        if (button("atom.zline.auto", ImVec2(q.x + 2 * (w + m.s8), q.y), ImVec2(2 * w, static_cast<float>(m.control)), "The level's own", Face::Quiet, P.zline < 0)) L.setParam("zline", -1.0);
        noteItem("atom.zline.auto");
        p.y += m.control + m.s4;
    }
    if (P.bfield > 0) {
        pCaption(p, displayable(z::regimeText(ln, P.bfield)));
        pCaption(p, "The split line, its polarisations and its two levels' M sublevels: data sheet, Spectrum tab; tables: Data panel.");
    } else pCaption(p, "Turn the field up to split the line (data sheet, Spectrum tab).");
    notesFold(p, "field", {z::usedText(ln), z::limitsText(P.Z, P.N, kind),
                           "Strong-field note: the lab follows each state from its zero-field level (states of equal M never cross); in a strong field, pairs of levels with no line at zero field "
                           "(J changing by 2) also get some strength from the mixing: those components are not drawn."});
    if (shellLogOn()) {
        const std::vector<z::Component> c = z::components(ln, P.bfield, z::gsCodata());
        int seen = 0;
        for (const z::Component& x : c) seen += z::seenStrength(x, P.blook == "Along") > 1e-12 ? 1 : 0;
        std::snprintf(b, sizeof b, "zeeman kind=%d line=%d B=%.6g look=%s comps=%d seen=%d", kind, k, P.bfield, P.blook.c_str(), static_cast<int>(c.size()), seen);
        auditNote(std::string(b) + " name=" + ln.name);
    }
}

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
        // (0.22 stage 6: the keys moved into the notes; the two captions of the view rows took their room)
        notesFold(p, "view", {std::string("Keys: + − zoom  ·  R reset view  ·  1 2 3 4 switch lab") + (L.chartOn() ? "  ·  arrows walk the chart" : ""),
                              "Zoom in to a nucleon and click one to dive inside it.",
                              L.manyOn() ? "Each dot is one possible place to find one of the atom's electrons (" + L.manyModel() + " orbitals). " + manyDotsWords(L)   // 0.39.1: it said "one electron" with every electron shown
                                         : "Each dot is one possible place to find the electron (one electron, non-relativistic model)."});
    }
    const atom::AtomParams& P = L.p;
    const bool orbit = P.view == "Orbit", chart = L.chartOn();
    if (!chart && section("orbital", "Orbital", true)) {                 // old: Orbital and Orientation in the cloud views (not the chart); Energy level in the Orbit view
        // 0.39: many electrons (atom/hf.h): One = one electron round the bare nucleus (exact, as always); All = the neutral atom from Hartree-Fock orbitals (0.39.1: in every view, the Orbit view as shell rings)
        const bool many = L.manyOn();
        {   // 0.46.6 (outside review: hydrogen's All said "relativistic Dirac-Hartree-Fock" above a readout that says one electron, non-relativistic): hydrogen has its own words
            ui::AtomChoices ec = ui::atomChoices("elec");
            if (P.Z == 1 && !many)
                for (auto& o : ec) if (o.first == "All") o.second = "All · hydrogen has one electron, so the whole atom is the one-electron cloud: the exact, non-relativistic solution "
                                                                     "(the same as Hydrogen-like). Every other element is drawn from Dirac-Hartree-Fock orbitals";
            choiceRow(app, p, "elec", "Electrons", ec, P.elec);
        }
        if (P.elec == "All" && many) choiceRow(app, p, "mrel", "Model", ui::atomChoices("mrel"), P.mrel);
        if (P.elec == "Hydrogen-like" && !many && !orbit) choiceRow(app, p, "hrel", "Cloud model", ui::atomChoices("hrel"), P.hrel);   // 0.49.3 (outside review): the Dirac cloud   // 0.41: relativistic (real) or its non-relativistic limit, to compare
        if (orbit) {
            if (many) choiceRow(app, p, "rscale", "Radius", ui::atomChoices("rscale"), P.rscale);
            else numberRow(app, p, "n", "Energy level (n)", std::to_string(P.n), P.n, 1, atom::AtomLab::kLineMaxN, 1);
        } else {
            if (many) choiceRow(app, p, "show", "Show", ui::atomChoices("show"), P.show);
            if (many && P.show == "Whole atom") choiceRow(app, p, "rscale", "Radius", ui::atomChoices("rscale"), P.rscale);
            if (L.outerDotsChoice()) choiceRow(app, p, "vdots", "Outer electron's dots", ui::atomChoices("vdots"), P.vdots);   // 0.54.1
            if (!many || P.show == "One orbital") {
                ui::AtomChoices oc = ui::atomChoices("orb");
                if (L.diracCloudOn()) for (auto& o : oc) o.second = o.first;   // 0.49.3: the shape words (dumbbell ...) are the non-relativistic orbitals'; a Dirac state's shape is set by j and Lined up / Round
                choiceRow(app, p, "orb", P.super == "On" && !many ? "First orbital" : "Orbital", oc, P.orb);   // 0.39.1: no second orbital with many electrons
                if (L.diracCloudOn()) {                           // 0.49.3: a Dirac state is (n, l, j), lined up (m = j) or round: no real orientation
                    if (atom::orbitals()[std::size_t(atom::orbIndex(P.orb))].l > 0) choiceRow(app, p, "hj", "j", ui::atomChoices("hj"), P.hj);
                    if (L.diracJ2() > 1) shapeRow(app, p);
                    else pCaption(p, "j = 1/2: round in every m.");
                } else orientationRow(app, p, P.orb, "pAx", "dAx", "fAx", "Orientation");
                if (many && L.manySub() < 0) {                   // 0.41: the list ends at 4f, so it named orbitals (gold's 6s) that cannot be picked
                    std::string can, cannot;
                    for (const atom::hf::Subshell& sh : L.manyAtom()->sub)
                        (sh.n <= 6 && sh.l <= 3 ? can : cannot) += (sh.n <= 6 && sh.l <= 3 ? can : cannot).empty() ? std::to_string(sh.n) + "spdf"[sh.l] : " " + std::to_string(sh.n) + "spdf"[sh.l];
                    pCaption(p, "This atom has no electron in that orbital: the whole atom is shown. Pick one of its filled orbitals (" + can + ")." +
                                (cannot.empty() ? std::string() : " The list goes up to 6f (s, p, d and f): its other orbitals (" + cannot + ") are shown only in the whole atom."));
                }
            }
        }
        notesFold(p, "orbital", L.manyOn() ? std::vector<std::string>{ui::atomNote("many")} : std::vector<std::string>{ui::atomNote("energy")});
    }
    if (!orbit && !chart && section("cloud", "Cloud", true)) {                      // old: cloud style, density and superposition whenever the view is not Orbit; the cut in Slice
        choiceRow(app, p, "cstyle", "Cloud style", ui::atomChoices("cstyle"), P.cstyle);
        choiceRow(app, p, "cdens", "Cloud density", ui::atomChoices("cdens"), P.cdens);
        if (L.manyOn()) pCaption(p, "Superposition is a state of one electron: set Electrons to Hydrogen-like to mix two orbitals.");   // 0.39.1: the row did nothing with many electrons
        else choiceRow(app, p, "super", "Superposition", ui::atomChoices("super"), P.super);
        if (P.super == "On" && !L.manyOn()) {
            choiceRow(app, p, "orb2", "Second orbital", ui::atomChoices("orb2"), P.orb2);
            orientationRow(app, p, P.orb2, "pAx2", "dAx2", "fAx2", "Second orientation");
            numberRow(app, p, "mix", "Mix (share of the second orbital)", fmt::localeNumber(std::round(P.mix)) + " %", P.mix, 0, 100, 1);
            numberRow(app, p, "speed", "Playback speed (beat speed)", std::to_string(P.speed), P.speed, 1, 30, 1);
            notesFold(p, "super", {ui::atomNote("super")});
        }
        if (P.view == "Slice") {
            choiceRow(app, p, "cutAxis", "Cut direction", ui::atomChoices("cutAxis"), P.cutAxis);
            numberRow(app, p, "cut", "Cut position (0 = through the nucleus)", fmt::shortestNumber(P.cut), P.cut, -1, 1, 0.01);
        }
    }
    if (orbit && section("orbit", "Orbit", true)) choiceRow(app, p, "ostyle", "Orbit style", ui::atomChoices("ostyle"), P.ostyle);
    if (!chart && section("scale", "Scale and motion", true)) {          // old: View & zoom (not on the chart) and the Orbit view's Playback speed
        if (L.showSpin()) { bool spin = L.s.spin; if (pCheck(p, "atom.spin", "Spin (the cloud turns by itself)", &spin)) L.toggleSpin(); pGap(p, m.s4); }
        choiceRow(app, p, "scale", "Scale", ui::atomChoices("scale"), P.scale);
        numberRow(app, p, "zoom", "Zoom (log scale)", "×" + fmt::localeNumber(atom::zoomFactorShown(P.zoom)), P.zoom, atom::kZoomMin, 7, 0.01);
        if (orbit) numberRow(app, p, "speed", "Playback speed", std::to_string(P.speed), P.speed, 1, 30, 1);
    }
    if (!chart && section("nucleus", "Nucleus", true)) {                 // 0.16 stage 2: always here (it was hidden below zoom 3 / 4, too deep to find); the picture shows the nucleus when zoomed in close
        choiceRow(app, p, "qstyle", "Quark style", ui::atomChoices("qstyle"), P.qstyle);
        choiceRow(app, p, "detail", "Nucleon detail", ui::atomChoices("detail"), P.detail);
        choiceRow(app, p, "style", "Nucleus look", ui::atomChoices("style"), P.style);
        if (P.Z == 1 && P.N == 1) choiceRow(app, p, "dspin", "Deuteron spin", ui::atomChoices("dspin"), P.dspin);   // 0.46.5: only the deuteron (its spin states have different shapes)
        pCaption(p, "These show in the picture when zoomed in close.", true);
    }
    if (chart && section("chart", "Chart", true)) {                       // old: the landscape's tilt and height, on the chart only
        {   // 0.40: 2D | 3D (was a Tilt slider); in 3D a drag turns the chart (Shift or the right button moves it)
            int cur = L.s.chartView.tilt > 0 ? 1 : 0;
            static const std::vector<std::string> names = {"atom.chart2d", "atom.chart3d"};
            if (pSegmented(p, "atom.chartdim", {"2D", "3D"}, &cur, &names)) L.setParam("tilt", cur ? 1.0 : 0.0);
            if (cur) pCaption(p, "Drag to turn the chart round and to tip it (never below its floor). Shift or the right button moves it.", true);
        }
        choiceRow(app, p, "cheight", "Height means", ui::atomChoices("cheight"), L.s.chartView.height == atom::ChartHeightMode::Binding ? std::string("Binding energy per nucleon") : std::string("Half-life"));
        if (pButton(p, "atom.chartreset", "Reset view, zoom and position")) L.chartReset();
        notesFold(p, "chart", {ui::atomNote("chart")});
    }
    // ---- stage 5e: the marks (the 90 % outline and the phase colours when the view is not Orbit; the decay chain on the chart) and the photons, as in the old panel ----
    if ((!orbit || chart) && section("marks", "Marks", true)) {
        if (!orbit && !chart && L.manyOn()) {                              // 0.39.1: with many electrons the outline (a one-electron orbital's) and the phase colours (the whole atom is coloured by shell) do not apply
            pCaption(p, L.manySub() < 0 ? "The whole atom is coloured by shell; the 90% outline and the phase colours belong to one electron (Electrons: Hydrogen-like)."
                                        : "The 90% outline belongs to one electron (Electrons: Hydrogen-like); this orbital shows the sign of its wave.");
        } else if (!orbit && !chart) {                                     // old: 90% outline and Phase colours in the cloud views (0.22 stage 6: not on the chart, which has no cloud)
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
                    if (pButton(p, "atom.walkauto", displayable(L.s.walkAuto ? "Pause auto-walk" : "Auto-walk (a step every " + atom::numText(atom::kWalkPaceS, 2) + " s)"))) L.s.walkAuto = !L.s.walkAuto;
                }
                if (pButton(p, "atom.walkrestart", "Restart walk")) L.walkRestart();
                for (std::size_t i = 0; i < w.steps.size(); i++) {
                    pBody(p, "Step " + std::to_string(i + 1) + ": " + displayable(atom::chainStepWords(w.steps[i])));
                    pCaption(p, displayable(atom::chainStepTime(w.steps[i])));
                }
                if (!w.steps.empty()) pCaption(p, displayable("Drawn lifetimes added up: " + atom::fmtLife(w.drawnS) + ". Screen time since the walk began: " + atom::numText(w.screenS, 3) + " s. These are not the same thing: the screen is not real time."));
                pCaption(p, displayable(w.done ? atom::chainWalkEndWords(w) : "The walker is at " + atom::chainNuclide(w.Z, w.N) + "."));
                pCaption(p, displayable(atom::chainTimeNote()));
            }
        }
    }
    if (section("photons", "Photons", true)) {
        if (!chart && L.manyOn() && !L.valenceOn() && !L.levelOn()) {     // 0.39.1: the jumps are the one-electron lab's levels; with every electron shown the buttons did nothing (Absorb flew a photon in that never landed)
            // 0.47.3 (D5): the buttons shown greyed (they do nothing here) with the reason: nothing measured, levels only, not in NIST, or clouds not computed yet
            const std::string why = !atom::noJumpText(P.Z).empty() ? atom::noJumpText(P.Z)
                                  : atom::levelJumps(P.Z) && P.mrel == "Non-relativistic" ? "This atom's jumps are drawn in the relativistic model (its clouds are relativistic calculations): set Model to Relativistic."
                                  : atom::cloudsNotComputedText();
            pCaption(p, displayable(why));
            const float half = (p.w - static_cast<float>(m.s8)) / 2;
            const ImVec2 o = here(p);
            disabledButton("atom.emit.off", o, ImVec2(half, static_cast<float>(m.control)), displayable("↓ Emit photon"));
            disabledButton("atom.absorb.off", ImVec2(o.x + half + m.s8, o.y), ImVec2(half, static_cast<float>(m.control)), displayable("↑ Absorb photon"));
            p.y += m.control + m.s4;
            if (shellLogOn()) auditNote("nojump " + why);
        } else if (!chart) {                                               // old: no photons on the chart (it shows nuclei, not the electron)
            if (L.manyOn() && L.valenceOn()) placeRow(app, p);               // 0.47: above Emit / Absorb
            if (L.manyOn() && L.valenceOn() && P.mrel != "Non-relativistic" && atom::graspHas(P.Z)) shapeRow(app, p);   // 0.47.3 (D6)
            if (L.levelOn()) levelRows(app, p);                              // 0.47.3
            const float half = (p.w - static_cast<float>(m.s8)) / 2;
            const ImVec2 o = here(p);
            if (button("atom.emit", o, ImVec2(half, static_cast<float>(m.control)), displayable("↓ Emit photon"))) L.emit();
            if (button("atom.absorb", ImVec2(o.x + half + m.s8, o.y), ImVec2(half, static_cast<float>(m.control)), displayable("↑ Absorb photon"))) L.absorb();
            p.y += m.control + m.s4;
            if (pButton(p, "atom.auto", displayable(L.autoText()), Face::Secondary, L.s.autoPhotons)) L.toggleAuto();
            notesFold(p, "jump", {ui::atomNote(L.levelOn() ? "jumpLevel" : L.manyOn() ? (P.mrel != "Non-relativistic" && atom::graspHas(P.Z) ? "jumpValenceRel" : "jumpValence") : orbit ? "jumpOrbit" : "jumpCloud")});   // 0.49.2: the relativistic note; the many-electron notes also in Orbit
            if (L.levelOn()) notesFold(p, "levels", levelListNotes(P.Z));    // 0.47.3 (D2): every other measured level
        }
        if (!chart) spectrumStrip(app, p);                                  // 0.40
        // 0.16 stage 1a: the Dirac levels switch (old: the "Light spectrum" section); the lab's own spectrum picture is on the data sheet's Spectrum tab
        choiceRow(app, p, "dirac", displayable(atom::kDiracLabel), ui::atomChoices("dirac"), P.dirac);
        // Check a jump (old: the "Lifetimes and jumps (NIST)" card): the same rows, the same result lines (ui::atomJumpCheck, atom/lifetime.h, NIST ASD)
        choiceRow(app, p, "jfrom", "Look up a jump (NIST): from (the upper shell)", ui::atomChoices("jfrom"), P.jfrom);
        choiceRow(app, p, "jto", "to (the lower shell)", ui::atomChoices("jto"), P.jto);
        const std::vector<std::string> lines = ui::atomJumpCheck(P.Z, P.jfrom, P.jto);
        for (const std::string& line : lines) pCaption(p, displayable(line));
        if (shellLogOn()) { std::string all = "jumpcheck"; for (const std::string& line : lines) all += " | " + line; auditNote(all); }
    }
    if (!chart && section("field", "Magnetic field", false)) fieldSection(app, p);   // 0.54.0 (closed at first: the field is off by default)
}

}  // namespace ui::shellimpl
