// 0.18 stage 1: the Molecule Lab's bottom bar (where the other labs have their toolbar: the style, labels, spin and Reset view) and its Controls panel
// (the molecule list, the same options, the clicked atom or bond with the measured values and their sources, the data sheet and what the picture does not show).
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

#include "imgui.h"
#include "molecule/molecule.h"
#include "molecule/molecule_view.h"
#include "ui/fonts.h"
#include "ui/look.h"
#include "ui/style_draw.h"
#include "ui/shell_internal.h"
#include "ui/shell_test.h"

namespace ui::shellimpl {

using namespace sh;
using app::shell::Metrics;
using app::shell::Rect;

namespace {

std::string fixed(double v, int decimals) { char b[48]; std::snprintf(b, sizeof b, "%.*f", std::max(0, decimals), v); return b; }
// a listed value: "0.958 Å (CCCBDB 1979Hoy/Bun:1, re)"; derived: "109.89° (derived: ...)"
std::string listed(const molecule::Value& v) {
    const std::string unit = v.kind == 'r' ? " Å" : "°";
    std::string s = fixed(v.value, v.decimals) + unit;
    if (v.derived) return s + " DERIVED (not in CCCBDB: " + v.note + ")";
    s += std::string(" (CCCBDB ") + v.ref + (v.note[0] ? std::string(", ") + v.note : std::string()) + ")";
    if (v.symmetry) s += ", fixed by symmetry";
    return s;
}
std::string built(double v, char kind) { return fixed(v, kind == 'r' ? 6 : 4) + (kind == 'r' ? " Å" : "°"); }
const char* orderName(int o) { return o == 2 ? "double bond" : o == 3 ? "triple bond" : o == 4 ? "aromatic bond" : "single bond"; }
std::string massText(const molecule::Element& e) {
    char b[96];
    if (e.massUncU > 0) std::snprintf(b, sizeof b, "%d%s: %.11g u (± %.1g u)", e.A, e.sym, e.massU, e.massUncU);
    else std::snprintf(b, sizeof b, "%d%s: %.0f u exactly (the definition of u)", e.A, e.sym, e.massU);
    return b;
}

}  // namespace

Rect moleculeBar(AppState& app, const app::shell::Frame& fr, const Metrics& m) {
    molecule::MoleculeLab& L = app.molecule;
    const double W = fr.window.w, H = fr.window.h;
    const auto widthOf = [&](const std::string& s) { return sh::width(Type::Body, false, s) + 2.0 * m.s12; };
    const char* names[2] = {"Ball-and-stick", "Space-filling"};
    const double sw0 = widthOf(names[0]), sw1 = widthOf(names[1]), lw = widthOf("Labels"), pw = widthOf("Spin"), rw = widthOf("Reset view");
    const double total = 2.0 * m.s8 + sw0 + sw1 + m.s4 + lw + pw + rw + 3.0 * m.s12;
    const double freeW = fr.freeW > 0 ? fr.freeW : W;
    const double x = std::max<double>(m.s12, std::min(freeW / 2 - total / 2, freeW - m.s12 - total));
    const Rect r{x, H - m.s12 - m.toolbarH, total, static_cast<double>(m.toolbarH)};
    sh::Panel p;
    if (sh::beginPanel("##moleculeBar", r, p, sh::Shape::Pill, false)) {
        float bx = p.origin.x + m.s8;
        const float by = p.origin.y + (m.toolbarH - m.control) / 2.0f, ch = static_cast<float>(m.control);
        for (int i = 0; i < 2; i++) {
            const bool on = static_cast<int>(L.set.style) == i;
            if (sh::toolToggle(i ? "molecule.bar.fill" : "molecule.bar.ball", ImVec2(bx, by), ImVec2(static_cast<float>(i ? sw1 : sw0), ch), names[i], on)) L.setStyle(static_cast<molecule::Style>(i));
            if (ImGui::IsItemHovered()) ui::SetTooltip("%s", i ? "Space-filling: balls of the van der Waals radius (a convention; a molecule has no hard edge)" : "Ball-and-stick: ball sizes are only a drawing scale");
            bx += static_cast<float>((i ? sw1 : sw0) + m.s4);
        }
        bx += static_cast<float>(m.s12 - m.s4);
        if (sh::toolToggle("molecule.bar.labels", ImVec2(bx, by), ImVec2(static_cast<float>(lw), ch), "Labels", L.set.labels)) L.set.labels = !L.set.labels;
        if (ImGui::IsItemHovered()) ui::SetTooltip("Element labels on the atoms");
        bx += static_cast<float>(lw + m.s12);
        const bool reduced = ui::look().reduced;
        if (sh::toolToggle("molecule.bar.spin", ImVec2(bx, by), ImVec2(static_cast<float>(pw), ch), "Spin", L.set.spin && !reduced) && !reduced) L.set.spin = !L.set.spin;
        if (ImGui::IsItemHovered()) ui::SetTooltip("%s", reduced ? "Auto-spin is off while Motion is Reduced (Settings)" : "Turn the molecule slowly by itself");
        bx += static_cast<float>(pw + m.s12);
        if (sh::button("molecule.bar.reset", ImVec2(bx, by), ImVec2(static_cast<float>(rw), ch), "Reset view", Face::Quiet)) L.resetView();   // (0.19: quiet, as the other toolbars' unlit buttons, so it is not mistaken for a lit toggle)
        if (ImGui::IsItemHovered()) ui::SetTooltip("Back to the first view (turn, zoom and position)");
        sh::endPanel(p);
    }
    auditNote(std::string("molecule ") + L.mol().id + " style=" + (L.set.style == molecule::Style::SpaceFill ? "fill" : L.set.style == molecule::Style::Realistic ? "real" : "ball") + " labels=" + (L.set.labels ? "1" : "0") +
              " spin=" + (L.set.spin ? "1" : "0") + " sel=" + std::to_string(L.sel.atom) + "," + std::to_string(L.sel.bond));
    return r;
}

// 0.20: Build mode's panel: the view (Sketch | 3D shape), the element to add, the edit buttons, the built molecule's formula, mass, status and name,
// the clicked atom or bond of the ESTIMATED shape, and what the estimate leaves out.
static void buildControls(AppState& app, Panel& p, const SectionFn& section) {
    namespace mb = molecule::build;
    molecule::MoleculeLab& L = app.molecule;
    const Metrics& m = sh::F().m;
    const mb::Graph& g = L.ed.graph();
    if (section("build", "Build", true)) {
        int v = L.buildView;
        const std::vector<std::string> vn{"molecule.view.sketch", "molecule.view.3d"};
        if (pSegmented(p, "molecule.view", {"Sketch", "3D shape (estimated)"}, &v, &vn)) L.setBuildView(v);
        if (L.buildView == 0) {
            pHeading(p, "Element (a click on empty space adds it)");
            int e = L.element;
            const std::vector<std::string> en{"molecule.el.H", "molecule.el.C", "molecule.el.N", "molecule.el.O", "molecule.el.Cl"};
            if (pSegmented(p, "molecule.el", {"H", "C", "N", "O", "Cl"}, &e, &en)) L.element = e;
            const float half = (p.w - static_cast<float>(m.s8)) / 2, ch = static_cast<float>(m.control);
            const auto pair = [&](const char* id0, const char* l0, bool on0, const char* id1, const char* l1, bool on1, int& hit) {
                const ImVec2 o = here(p);
                hit = 0;
                if (sh::button(id0, o, ImVec2(half, ch), l0, on0 ? Face::Secondary : Face::Quiet)) hit = on0 ? 1 : 0;
                if (sh::button(id1, ImVec2(o.x + half + static_cast<float>(m.s8), o.y), ImVec2(half, ch), l1, on1 ? Face::Secondary : Face::Quiet)) hit = on1 ? 2 : hit;
                p.y += ch + static_cast<float>(m.s4);
            };
            int hit = 0;
            pair("molecule.undo", "Undo", L.ed.canUndo(), "molecule.redo", "Redo", L.ed.canRedo(), hit);
            if (hit == 1) L.ed.undo(); else if (hit == 2) L.ed.redo();
            pair("molecule.delete", "Delete atom", L.ed.picked >= 0, "molecule.clear", "Clear", !g.atoms.empty(), hit);
            if (hit == 1) L.deletePicked(); else if (hit == 2) L.ed.clear();
            pCaption(p, "Click empty space to add the element. Drag an atom next to another one with an open valence (the small open stubs) and let go: it snaps and bonds. "
                        "Click a bond: single, double, triple. Click an atom to pick it; Delete removes it. Ctrl+Z undoes, Ctrl+Y redoes.");
        } else pCaption(p, "Drag to turn the shape, the wheel zooms; Shift, right or middle drag moves it. Click an atom or a bond for its estimated values.");
    }
    if (section("built", "Built molecule", true)) {
        const int open = mb::openValences(g), pc = mb::pieces(g);
        pStat(p, "Formula (Hill order)", g.atoms.empty() ? std::string("none yet") : mb::hill(g));
        if (!g.atoms.empty()) {
            char b[96];
            std::snprintf(b, sizeof b, "%.8f u (± %.1g u)", mb::massU(g), mb::massUncU(g));
            pStat(p, "Mass (most abundant isotopes)", b);
        }
        pStat(p, "Status", g.atoms.empty() ? std::string("empty") : pc > 1 ? std::to_string(pc) + " separate pieces" : open > 0 ? std::to_string(open) + (open == 1 ? " open valence" : " open valences") : std::string("complete"));
        pStat(p, "Name", L.knownIdx >= 0 ? std::string(mb::known(L.knownIdx).name) : std::string("no name in our list"));
        if (L.knownIdx >= 0 && mb::known(L.knownIdx).viewer >= 0 && pButton(p, "molecule.showmeasured", "Show measured shape")) L.showMeasured();
        if (L.knownIdx >= 0 && std::string(mb::known(L.knownIdx).id) == "C6H6")
            pCaption(p, "Built as a Kekulé ring: alternating single and double bonds, so the estimate has alternating lengths (1.536 and 1.339 Å). Real benzene has six equal C-C bonds, "
                        "1.397 Å measured (CCCBDB 1966Herzberg): see the measured shape.", false);
        if (!L.ed.message.empty()) { pHeading(p, "Last message"); pBody(p, L.ed.message); }
        pCaption(p, "Mass: NIST, Atomic Weights and Isotopic Compositions. Names are conventions; only 23 molecules have one here.");
    }
    if (L.buildView == 1 && section("selected", "Clicked atom or bond (estimated)", true)) {
        const molecule::Molecule& mo = L.built;
        if (!L.builtOk) pBody(p, "No 3D shape yet. " + L.builtWhy);
        else if (L.sel.atom >= 0 && L.sel.atom < static_cast<int>(mo.atoms.size())) {
            const int a = L.sel.atom;
            const molecule::Element& e = molecule::element(mo.atoms[a].el);
            pStat(p, "Atom", molecule::atomLabel(mo, a) + "  ·  " + e.name);
            pStat(p, "Lone pairs, steric number", std::to_string(mb::lonePairs(g, a)) + ", " + std::to_string(mb::steric(g, a)));
            for (const molecule::Bond& b : mo.bonds) {
                if (b.a != a && b.b != a) continue;
                const mb::LengthRef* lr = mb::bondLength(mo.atoms[b.a].el, mo.atoms[b.b].el, b.order);
                pHeading(p, molecule::atomLabel(mo, b.a) + "–" + molecule::atomLabel(mo, b.b) + " " + orderName(b.order));
                if (lr) pBody(p, fixed(lr->angstrom, 3) + " Å, estimated, not measured (CCCBDB " + lr->ref + ": the " + mb::bondName(lr->e0, lr->e1, lr->order) + " bond of " + lr->molecule + ")");
            }
            const double ideal = mb::idealAngle(mb::steric(g, a));
            if (std::isfinite(ideal) && mb::steric(g, a) > 1) pBody(p, "Angles at this atom: " + fixed(ideal, ideal == 180 || ideal == 120 ? 0 : 4) + "°, estimated, not measured (ideal VSEPR for steric number " + std::to_string(mb::steric(g, a)) + ")");
        } else if (L.sel.bond >= 0 && L.sel.bond < static_cast<int>(mo.bonds.size())) {
            const molecule::Bond& b = mo.bonds[L.sel.bond];
            pStat(p, "Bond", molecule::atomLabel(mo, b.a) + "–" + molecule::atomLabel(mo, b.b) + ", " + orderName(b.order));
            if (const mb::LengthRef* lr = mb::bondLength(mo.atoms[b.a].el, mo.atoms[b.b].el, b.order))
                pBody(p, fixed(lr->angstrom, 3) + " Å, estimated, not measured (CCCBDB " + lr->ref + ": the " + mb::bondName(lr->e0, lr->e1, lr->order) + " bond of " + lr->molecule + ")");
            pBody(p, "From the 3D shape " + built(molecule::dist(mo.atoms[b.a], mo.atoms[b.b]), 'r'), true);
        } else pCaption(p, L.realOn() ? "Nothing chosen. Click a nucleus (a small bright point) in the picture." : "Nothing chosen. Click an atom or a bond in the picture.");
    }
    if (section("limits", "What the estimate does not show", true)) {
        pCaption(p, "Estimated, not measured: the shape uses the ideal VSEPR angles (Gillespie and Nyholm: 180°, 120°, 109.47°) and one CCCBDB bond length per bond type, then a simple spring model so rings close.", false);
        pCaption(p, "VSEPR (ideal angles) is a simplified model; lone pairs make real angles smaller (water is 104.48° measured, 109.47° here).", false);
        pCaption(p, "Each bond length comes from one reference molecule; real lengths vary by 1-3 % with the surroundings. A bond type with no measured length in our sources is refused.", false);
        pCaption(p, "No energies and no stability check: a molecule the builder accepts is not necessarily a stable real one. Valences H 1, C 4, N 3, O 2, Cl 1 are a convention: "
                    "molecules that need formal charges (ozone, for example) cannot be built.", false);
    }
    std::string orders;
    for (const mb::SBond& b : g.bonds) orders += (orders.empty() ? "" : ",") + std::to_string(b.order);
    auditNote("molbuild view=" + std::string(L.buildView ? "3d" : "sketch") + " atoms=" + std::to_string(g.atoms.size()) + " bonds=" + std::to_string(g.bonds.size()) + " orders=" + orders + " hill=" + mb::hill(g) +
              " open=" + std::to_string(mb::openValences(g)) + " shape=" + (L.builtOk ? "estimated" : "none") + " name=" + (L.knownIdx >= 0 ? mb::known(L.knownIdx).id : "-") + " msg=" + L.ed.message);
}

// 0.21 stage 1: the Realistic style: what is shown (cloud, surface), the dots, the isovalue, the colours with their scale, the computed vs the measured dipole and the honest notes
static void realisticControls(molecule::MoleculeLab& L, Panel& p, const SectionFn& section) {
    namespace dn = molecule::dens;
    const molecule::Molecule& mo = L.mol();
    const bool ok = L.real.ensure(mo.id, L.set.realDots, L.set.realIso);
    if (!section("realistic", "Realistic: electron density", true)) return;
    if (!ok) { pCaption(p, "No computed density for this molecule.", false); return; }
    const dn::Data& d = *L.real.data;
    int show = L.set.realShow;
    const std::vector<std::string> shn{"molecule.real.both", "molecule.real.cloud", "molecule.real.surface"};
    if (pSegmented(p, "molecule.real.show", {"Cloud + surface", "Cloud", "Surface"}, &show, &shn)) L.set.realShow = show;
    if (L.set.realShow != 2) {
        pHeading(p, "Cloud: dots");
        int di = 2;
        for (int i = 0; i < 4; i++) if (molecule::kRealDots[i] == L.set.realDots) di = i;
        const std::vector<std::string> dnm{"molecule.real.dots20k", "molecule.real.dots40k", "molecule.real.dots60k", "molecule.real.dots100k"};
        if (pSegmented(p, "molecule.real.dots", {"20,000", "40,000", "60,000", "100,000"}, &di, &dnm)) L.set.realDots = molecule::kRealDots[di];
        pCaption(p, "Each dot is drawn at random from the computed density (a fixed seed: the same cloud every time). Dots show where the electrons are likely to be, not electrons as little balls.");
    }
    if (L.set.realShow != 1) {
        pHeading(p, "Surface: density cut-off (e/bohr³)");
        int iso = L.set.realIso;
        const std::vector<std::string> inm{"molecule.real.iso0010", "molecule.real.iso0016", "molecule.real.iso0020", "molecule.real.iso0100"};
        if (pSegmented(p, "molecule.real.iso", {"0.001", "0.0016", "0.002", "0.01"}, &iso, &inm)) L.set.realIso = iso;
        pCaption(p, "0.002: the usual convention (Bader). 0.0016 matched measured molecular surfaces best in a 2024 study of 104 molecules (1.6 % error; 0.002 gave 3.2 %, 0.001 gave 7 %). 0.001: a larger, looser cut-off. 0.01: a tight one, close to the nuclei. A molecule has no hard edge: the surface is a chosen cut-off of the density.");
        pHeading(p, "Surface colour");
        int col = L.set.realPot ? 0 : 1;
        const std::vector<std::string> cnm{"molecule.real.pot", "molecule.real.plain"};
        if (pSegmented(p, "molecule.real.col", {"Charge (potential)", "Plain"}, &col, &cnm)) L.set.realPot = col == 0;
        if (L.set.realPot) {                              // the colour bar: red (negative) .. white (0) .. blue (positive), -range..+range
            const ImVec2 a = here(p);
            const float bw = p.w, bh = 12 * static_cast<float>(ImGui::GetFontSize() / 16), half = bw / 2;
            const auto u32 = [](render::Color c) { return IM_COL32(static_cast<int>(c.r * 255 + 0.5f), static_cast<int>(c.g * 255 + 0.5f), static_cast<int>(c.b * 255 + 0.5f), 255); };
            const ImU32 cr = u32(molecule::realPotColour(-1)), cw = u32(molecule::realPotColour(0)), cb = u32(molecule::realPotColour(1));
            p.dl->AddRectFilledMultiColor(a, ImVec2(a.x + half, a.y + bh), cr, cw, cw, cr);
            p.dl->AddRectFilledMultiColor(ImVec2(a.x + half, a.y), ImVec2(a.x + bw, a.y + bh), cw, cb, cb, cw);
            p.dl->AddRect(a, ImVec2(a.x + bw, a.y + bh), cMuted(0.5f));
            pGap(p, static_cast<int>(bh + 6));
            const double r = L.real.potRange;
            char b[200];
            std::snprintf(b, sizeof b, "Red −%.3f au (−%.0f kJ/mol per e) · white 0 · blue +%.3f au (+%.0f kJ/mol per e)", r, r * dn::kHartreeKJmol, r, r * dn::kHartreeKJmol);
            pCaption(p, b, false);
            pCaption(p, "The electrostatic potential of the molecule's own nuclei and electrons on the surface: red where a positive test charge would be attracted (negative), blue where it would be pushed away. The scale is symmetric and set by this molecule's largest |value| on this surface, so colours are not comparable between molecules. 1 au = 1 hartree per elementary charge = 2625.50 kJ/mol per e (CODATA 2022: E_h × N_A).");
        }
    }
    const double mu = std::sqrt(d.dip[0] * d.dip[0] + d.dip[1] * d.dip[1] + d.dip[2] * d.dip[2]) * dn::kAuDebye;
    char b[160];
    std::snprintf(b, sizeof b, "%.3f D (calculated)", mu);
    pStat(p, "Dipole moment", b);
    if (const dn::MeasuredDipole* md = dn::measuredDipole(mo.id)) {
        std::snprintf(b, sizeof b, "%.3f D (NIST CCCBDB, %s)", md->debye, md->ref);
        pStat(p, "Measured", b);
    }
    std::snprintf(b, sizeof b, "%d (the stored grid adds up to %.2f)", d.electrons, L.real.gridElectrons);
    pStat(p, "Electrons", b);
    std::string calc = "Calculated, not measured: Kohn-Sham density functional theory, B3LYP / " + std::string(d.basis == "aug-cc-pvtz" ? "aug-cc-pVTZ" : d.basis == "aug-cc-pvdz" ? "aug-cc-pVDZ" : d.basis.c_str()) +
                       " (PySCF 2.14), at the measured (CCCBDB) shape the other styles show. " + (d.spin2S ? "O2 is a triplet: the total density of both spins (UKS). " : "") +
                       (mo.id == "O3" ? "Ozone: a closed-shell approximation (ozone has multi-reference character, so this density is less certain). " : "") +
                       "The potential is the molecule's own at its equilibrium shape; vibrations are ignored.";
    pCaption(p, calc, false);
    pCaption(p, "Real pictures of molecules by atomic force microscopy (Gross et al., IBM Research Zurich, 2009: a tip ending in a CO molecule over a flat pentacene molecule) show forces, not the electron density.");
    pCaption(p, "Drawing: the surface is cut into triangles (marching tetrahedra on a 0.5 bohr grid) and drawn translucent over the cloud, far to near (no depth buffer: the cloud and the surface are not sorted together). Bonds are not drawn in this style.");
}

void moleculeControls(AppState& app, Panel& p, const SectionFn& section) {
    molecule::MoleculeLab& L = app.molecule;
    if (section("mode", "Mode", true)) {                  // 0.20: Viewer (the 15 measured molecules) or Build
        int md = L.buildMode ? 1 : 0;
        const std::vector<std::string> mn{"molecule.mode.viewer", "molecule.mode.build"};
        if (pSegmented(p, "molecule.mode", {"Viewer", "Build"}, &md, &mn)) L.setBuild(md == 1);
    }
    if (L.buildMode) { buildControls(app, p, section); return; }
    const molecule::Molecule& mo = L.mol();
    if (section("molecules", "Molecule", true)) {
        for (int i = 0; i < molecule::count(); i++) {
            const molecule::Molecule& c = molecule::get(i);
            const std::string id = "molecule.pick." + c.id;
            if (pRow(p, id.c_str(), c.formula + "  ·  " + c.name, i == L.set.mol)) L.choose(i);
        }
    }
    if (section("style", "Style", true)) {
        int cur = static_cast<int>(L.set.style);
        const std::vector<std::string> sn{"molecule.style.ball", "molecule.style.fill", "molecule.style.real"};
        if (pSegmented(p, "molecule.style", {"Ball-and-stick", "Space-filling", "Realistic"}, &cur, &sn)) L.setStyle(static_cast<molecule::Style>(cur));
        bool lab = L.set.labels;
        if (pCheck(p, "molecule.labels", "Element labels", &lab)) L.set.labels = lab;
        const bool reduced = ui::look().reduced;
        bool sp = L.set.spin && !reduced;
        if (pCheck(p, "molecule.spin", "Auto-spin", &sp) && !reduced) L.set.spin = sp;
        if (reduced) pCaption(p, "Auto-spin is off while Motion is Reduced (Settings).");
        if (pButton(p, "molecule.reset", "Reset view")) L.resetView();
        pCaption(p, "Drag to turn the molecule, the wheel zooms; Shift, right or middle drag moves it. Click an atom or a bond for its measured values.");
    }
    if (L.realOn()) realisticControls(L, p, section);
    if (section("selected", L.realOn() ? "Clicked atom" : "Clicked atom or bond", true)) {
        const int n = static_cast<int>(mo.atoms.size());
        if (L.sel.atom >= 0 && L.sel.atom < n) {
            const int a = L.sel.atom;
            const molecule::Element& e = molecule::element(mo.atoms[a].el);
            pStat(p, "Atom", molecule::atomLabel(mo, a) + "  ·  " + e.name + " (Z = " + std::to_string(e.Z) + ")");
            pStat(p, "Isotope (most abundant)", massText(e));
            pCaption(p, "Mass: NIST, Atomic Weights and Isotopic Compositions.");
            for (const molecule::Bond& b : mo.bonds) {
                if (b.a != a && b.b != a) continue;
                const molecule::Value* v = molecule::lengthValue(mo, b.a, b.b);
                pHeading(p, molecule::atomLabel(mo, b.a) + "–" + molecule::atomLabel(mo, b.b) + " " + orderName(b.order));
                if (v) pBody(p, "Measured " + listed(*v));
                pBody(p, "From the 3D shape " + built(molecule::dist(mo.atoms[b.a], mo.atoms[b.b]), 'r'), true);
            }
            for (int c = 0; c < n; c++)                       // angles at a bonded centre that include this atom
                for (int x = 0; x < n; x++)
                    for (int y = x + 1; y < n; y++) {
                        if (x == c || y == c) continue;
                        if (a != c && a != x && a != y) continue;
                        bool bx = false, by = false;
                        for (const molecule::Bond& b : mo.bonds) { bx |= (b.a == x && b.b == c) || (b.b == x && b.a == c); by |= (b.a == y && b.b == c) || (b.b == y && b.a == c); }
                        if (!bx || !by) continue;
                        const molecule::Value* v = molecule::angleValue(mo, x, c, y);
                        if (!v) continue;
                        pHeading(p, molecule::atomLabel(mo, x) + "–" + molecule::atomLabel(mo, c) + "–" + molecule::atomLabel(mo, y) + " angle");
                        pBody(p, (v->derived ? "" : "Measured ") + listed(*v));
                        pBody(p, "From the 3D shape " + built(molecule::angleDeg(mo.atoms[x], mo.atoms[c], mo.atoms[y]), 'a'), true);
                    }
            for (const molecule::Value& v : mo.values)
                if (v.kind == 'd' && (a == v.i0 || a == v.i1 || a == v.i2 || a == v.i3)) {
                    pHeading(p, molecule::atomLabel(mo, v.i0) + "–" + molecule::atomLabel(mo, v.i1) + "–" + molecule::atomLabel(mo, v.i2) + "–" + molecule::atomLabel(mo, v.i3) + " dihedral");
                    pBody(p, fixed(v.value, v.decimals) + "° (CCCBDB " + v.ref + ", " + v.note + ")");
                    pBody(p, "From the 3D shape " + built(molecule::dihedralDeg(mo.atoms[v.i0], mo.atoms[v.i1], mo.atoms[v.i2], mo.atoms[v.i3]), 'a'), true);
                }
            pCaption(p, "Uncertainty: the CCCBDB values used here came without one; the listed digits are all that is known.");
        } else if (L.sel.bond >= 0 && L.sel.bond < static_cast<int>(mo.bonds.size())) {
            const molecule::Bond& b = mo.bonds[L.sel.bond];
            pStat(p, "Bond", molecule::atomLabel(mo, b.a) + "–" + molecule::atomLabel(mo, b.b) + ", " + orderName(b.order));
            if (const molecule::Value* v = molecule::lengthValue(mo, b.a, b.b)) pBody(p, "Measured " + listed(*v));
            pBody(p, "From the 3D shape " + built(molecule::dist(mo.atoms[b.a], mo.atoms[b.b]), 'r'), true);
            pCaption(p, "Bond order from the usual structural formula: a drawing convention, not a measurement. Uncertainty: not given with the CCCBDB values used here.");
        } else pCaption(p, L.realOn() ? "Nothing chosen. Click a nucleus (a small bright point) in the picture." : "Nothing chosen. Click an atom or a bond in the picture.");
    }
    if (section("sheet", "Data sheet", true)) {
        pStat(p, "Formula", mo.formula + " (Hill order: " + mo.hill + ")");
        pStat(p, "Name", mo.name);
        char b[96];
        std::snprintf(b, sizeof b, "%.8f u (± %.1g u)", molecule::massU(mo), molecule::massUncU(mo));
        pStat(p, "Mass (most abundant isotopes)", b);
        pStat(p, "Atoms, bonds", std::to_string(mo.atoms.size()) + ", " + std::to_string(mo.bonds.size()));
        pStat(p, "Point group", "not in the CCCBDB data used (see the shape below)");
        std::string rc;
        double I[3];
        molecule::principalMoments(mo, I);
        if (mo.linear) { std::snprintf(b, sizeof b, "B %.6g (from the shape %.6g)", mo.rotB, molecule::rotConstCm(I[2])); rc = b; }
        else { std::snprintf(b, sizeof b, "A %.6g, B %.6g, C %.6g", mo.rotA, mo.rotB, mo.rotC); rc = b; std::snprintf(b, sizeof b, " (from the shape %.4g, %.4g, %.4g)", molecule::rotConstCm(I[0]), molecule::rotConstCm(I[1]), molecule::rotConstCm(I[2])); rc += b; }
        pStat(p, "Rotational constants, cm⁻¹", rc);
        if (mo.id == "NH3") pCaption(p, "Note: the A and B worked out from this shape (9.97 cm⁻¹) differ from the listed 9.4443 by 5.6 %. CCCBDB's own page lists 9.4443 (Herzberg 1966) next to the same geometry, so the doubt is in the listed value; it is shown unchanged until a second source is checked.", false);
        pCaption(p, "Shape: " + mo.shapeNote, false);
        if (mo.derivedAngle) pCaption(p, "H-C-O angle derived: CCCBDB lists no H-C-O angle, so it is worked out from H-C-H under the stated assumption.", false);
        pCaption(p, std::string("Source: NIST CCCBDB (Computational Chemistry Comparison and Benchmark Database), SRD 101, Release 22, experimental geometry and rotational constants (") + mo.rotRef +
                    " for the constants); masses: NIST Atomic Weights and Isotopic Compositions.");
    }
    if (section("honest", "What the picture does not show", true)) {
        pCaption(p, "The shape is a fixed average: an equilibrium structure (CCCBDB calls most values \"re\", acetylene's an \"rm structure\"). Real molecules vibrate and rotate all the time.", false);
        pCaption(p, "Ball sizes in Ball-and-stick are only a drawing scale. Space-filling balls use van der Waals radii (PubChem): a convention, because a molecule has no hard edge; its electrons thin out gradually.", false);
        pCaption(p, "Colours are the CPK convention (PubChem). Double, triple and aromatic bonds come from the usual structural formula, also a convention. The view has no perspective.", false);
        pCaption(p, "Realistic shows a CALCULATED electron density (quantum chemistry), not a measurement and not a photo; see its own section above.", false);
    }
}

}  // namespace ui::shellimpl
