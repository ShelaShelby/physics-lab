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
            if (sh::button(i ? "molecule.bar.fill" : "molecule.bar.ball", ImVec2(bx, by), ImVec2(static_cast<float>(i ? sw1 : sw0), ch), names[i], Face::Quiet, on)) L.set.style = static_cast<molecule::Style>(i);
            if (ImGui::IsItemHovered()) ui::SetTooltip("%s", i ? "Space-filling: balls of the van der Waals radius (a convention; a molecule has no hard edge)" : "Ball-and-stick: ball sizes are only a drawing scale");
            bx += static_cast<float>((i ? sw1 : sw0) + m.s4);
        }
        bx += static_cast<float>(m.s12 - m.s4);
        if (sh::button("molecule.bar.labels", ImVec2(bx, by), ImVec2(static_cast<float>(lw), ch), "Labels", Face::Quiet, L.set.labels)) L.set.labels = !L.set.labels;
        if (ImGui::IsItemHovered()) ui::SetTooltip("Element labels on the atoms");
        bx += static_cast<float>(lw + m.s12);
        const bool reduced = ui::look().reduced;
        if (sh::button("molecule.bar.spin", ImVec2(bx, by), ImVec2(static_cast<float>(pw), ch), "Spin", Face::Quiet, L.set.spin && !reduced) && !reduced) L.set.spin = !L.set.spin;
        if (ImGui::IsItemHovered()) ui::SetTooltip("%s", reduced ? "Auto-spin is off while Motion is Reduced (Settings)" : "Turn the molecule slowly by itself");
        bx += static_cast<float>(pw + m.s12);
        if (sh::button("molecule.bar.reset", ImVec2(bx, by), ImVec2(static_cast<float>(rw), ch), "Reset view", Face::Secondary)) L.resetView();
        if (ImGui::IsItemHovered()) ui::SetTooltip("Back to the first view (turn, zoom and position)");
        sh::endPanel(p);
    }
    auditNote(std::string("molecule ") + L.mol().id + " style=" + (L.set.style == molecule::Style::SpaceFill ? "fill" : "ball") + " labels=" + (L.set.labels ? "1" : "0") +
              " spin=" + (L.set.spin ? "1" : "0") + " sel=" + std::to_string(L.sel.atom) + "," + std::to_string(L.sel.bond));
    return r;
}

void moleculeControls(AppState& app, Panel& p, const SectionFn& section) {
    molecule::MoleculeLab& L = app.molecule;
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
        if (pSegmented(p, "molecule.style", {"Ball-and-stick", "Space-filling"}, &cur)) L.set.style = static_cast<molecule::Style>(cur);
        bool lab = L.set.labels;
        if (pCheck(p, "molecule.labels", "Element labels", &lab)) L.set.labels = lab;
        const bool reduced = ui::look().reduced;
        bool sp = L.set.spin && !reduced;
        if (pCheck(p, "molecule.spin", "Auto-spin", &sp) && !reduced) L.set.spin = sp;
        if (reduced) pCaption(p, "Auto-spin is off while Motion is Reduced (Settings).");
        if (pButton(p, "molecule.reset", "Reset view")) L.resetView();
        pCaption(p, "Drag to turn the molecule, the wheel zooms; Shift, right or middle drag moves it. Click an atom or a bond for its measured values.");
    }
    if (section("selected", "Clicked atom or bond", true)) {
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
        } else pCaption(p, "Nothing chosen. Click an atom or a bond in the picture.");
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
        if (mo.id == "NH3") pCaption(p, "Note: the A and B worked out from this shape (9.97 cm⁻¹) differ from the listed 9.4443 by 5.6 %; reported for checking against CCCBDB.", false);
        pCaption(p, "Shape: " + mo.shapeNote, false);
        if (mo.derivedAngle) pCaption(p, "H-C-O angle derived: CCCBDB lists no H-C-O angle, so it is worked out from H-C-H under the stated assumption.", false);
        pCaption(p, std::string("Source: NIST CCCBDB (Computational Chemistry Comparison and Benchmark Database), SRD 101, Release 22, experimental geometry and rotational constants (") + mo.rotRef +
                    " for the constants); masses: NIST Atomic Weights and Isotopic Compositions.");
    }
    if (section("honest", "What the picture does not show", true)) {
        pCaption(p, "The shape is a fixed average: an equilibrium structure (CCCBDB calls most values \"re\", acetylene's an \"rm structure\"). Real molecules vibrate and rotate all the time.", false);
        pCaption(p, "Ball sizes in Ball-and-stick are only a drawing scale. Space-filling balls use van der Waals radii (PubChem): a convention, because a molecule has no hard edge; its electrons thin out gradually.", false);
        pCaption(p, "Colours are the CPK convention (PubChem). Double, triple and aromatic bonds come from the usual structural formula, also a convention. The view has no perspective.", false);
    }
}

}  // namespace ui::shellimpl
