// 0.18 stage 1: the Molecule Lab's bottom bar (where the other labs have their toolbar: the style, labels, spin and Reset view) and its Controls panel
// (the molecule list, the same options, the clicked atom or bond with the measured values and their sources, the data sheet and what the picture does not show).
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

#include "imgui.h"
#include "molecule/molecule.h"
#include "molecule/molecule_view.h"
#include "molecule/rotation.h"
#include "ui/fonts.h"
#include "ui/look.h"
#include "ui/style_draw.h"
#include "molecule/morse.h"
#include "ui/realest.h"
#include "ui/shell_internal.h"
#include "ui/shell_test.h"

// 0.48 (review B): what kind of shape CCCBDB gives for this molecule, from the notes on its measured lengths ("re", "rm structure" or none)
static std::string shapeKindText(const molecule::Molecule& mo) {
    bool eq = false, rm = false, none = false;
    for (const molecule::Value& v : mo.values) {
        if (v.kind != 'r') continue;
        const std::string n = v.note ? v.note : "";
        if (n.rfind("re", 0) == 0 || n == "equilibrium") eq = true; else if (n.rfind("rm", 0) == 0) rm = true; else none = true;
    }
    return eq && !rm && !none ? "an equilibrium structure (re)" : rm && !eq && !none ? "an rm structure, CCCBDB's term" : "CCCBDB gives no structure type";
}

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
const char* orderName(int o) { return o == 2 ? "double bond" : o == 3 ? "triple bond" : o == 4 ? "1.5 bond (delocalised)" : "single bond"; }
std::string massText(const molecule::Element& e) {
    char b[96];
    if (e.massUncU > 0) std::snprintf(b, sizeof b, "%d%s: %.11g u (± %.1g u)", e.A, e.sym, e.massU, e.massUncU);
    else std::snprintf(b, sizeof b, "%d%s: %.0f u exactly (the definition of u)", e.A, e.sym, e.massU);
    return b;
}

}  // namespace

Rect moleculeBar(AppState& app, const app::shell::Frame& fr, const Metrics& m) {
    // 0.22: the bar is a shortcut, not a copy of the panel: the three styles of the panel (Realistic only in the Viewer, as in the panel) and Reset view.
    // Element labels and Auto-spin are in the panel's Style section only.
    molecule::MoleculeLab& L = app.molecule;
    const double W = fr.window.w, H = fr.window.h;
    const auto widthOf = [&](const std::string& s) { return sh::width(Type::Body, false, s) + 2.0 * m.s12; };
    const char* full[3] = {"Ball-and-stick", "Space-filling", "Density"};   // (0.22: "Density" was "Realistic": a computed density, not a photo)
    const char* compact[3] = {"Ball-stick", "Space-fill", "Density"};     // 0.39.2: when the whole bar does not fit left of the Controls panel (Extra large text at 880 x 560 ran it under the panel)
    const char* ids[3] = {"molecule.bar.ball", "molecule.bar.fill", "molecule.bar.real"};
    const char* tips[3] = {"Ball-and-stick: ball sizes are only a drawing scale", "Space-filling: balls of the van der Waals radius (a convention; a molecule has no hard edge)",
                           "Electron density (computed): where the electrons are likely to be, from quantum chemistry; not a photo"};
    const int styles = 3;
    const double freeW = fr.freeW > 0 ? fr.freeW : W;
    const char* const* names = full;
    const char* resetName = "Reset view";
    double sw[3], total = 0, rw = 0;
    const auto measure = [&] {
        total = 2.0 * m.s8;
        for (int i = 0; i < styles; i++) { sw[i] = widthOf(names[i]); total += sw[i] + m.s4; }
        rw = widthOf(resetName);
        total += m.s12 - m.s4 + rw;
    };
    measure();
    if (total > freeW - 2.0 * m.s12) { names = compact; resetName = "Reset"; measure(); }
    const double x = std::max<double>(m.s12, std::min(freeW / 2 - total / 2, freeW - m.s12 - total));
    const Rect r{x, H - m.s12 - m.toolbarH, total, static_cast<double>(m.toolbarH)};
    sh::Panel p;
    if (sh::beginPanel("##moleculeBar", r, p, sh::Shape::Pill, false)) {
        float bx = p.origin.x + m.s8;
        const float by = p.origin.y + (m.toolbarH - m.control) / 2.0f, ch = static_cast<float>(m.control);
        for (int i = 0; i < styles; i++) {
            const bool on = static_cast<int>(L.set.style) == i;
            if (sh::toolToggle(ids[i], ImVec2(bx, by), ImVec2(static_cast<float>(sw[i]), ch), names[i], on)) L.setStyle(static_cast<molecule::Style>(i));
            if (ImGui::IsItemHovered()) ui::SetTooltip("%s", tips[i]);
            bx += static_cast<float>(sw[i] + m.s4);
        }
        bx += static_cast<float>(m.s12 - m.s4);
        if (sh::button("molecule.bar.reset", ImVec2(bx, by), ImVec2(static_cast<float>(rw), ch), resetName, Face::Quiet)) L.resetView();   // (0.19: quiet, as the other toolbars' unlit buttons, so it is not mistaken for a lit toggle)
        if (ImGui::IsItemHovered()) ui::SetTooltip("Back to the first view (turn, zoom and position; key R)");
        sh::endPanel(p);
    }
    auditNote(std::string("molecule ") + L.mol().id + " style=" + (L.set.style == molecule::Style::SpaceFill ? "fill" : L.set.style == molecule::Style::Realistic ? "real" : "ball") + " labels=" + (L.set.labels ? "1" : "0") +
              " spin=" + (L.set.spin ? "1" : "0") + " sel=" + std::to_string(L.sel.atom) + "," + std::to_string(L.sel.bond));
    if (shellLogOn()) {                                   // 0.40, for the automatic checks: each ball on the screen (points): x, y, r and the depth of its centre in points (bigger = nearer)
        static std::vector<molecule::Proj> pr;
        molecule::project(L.shown(), L.set.cam, L.set.style, L.cx(), L.cy(), L.scale(), pr);
        std::string t = "molatoms";
        char b[96];
        for (const molecule::Proj& q : pr) { std::snprintf(b, sizeof b, " %.2f,%.2f,%.2f,%.2f", q.x, q.y, q.r, q.depth * L.scale()); t += b; }
        auditNote(t);
    }
    return r;
}

// 0.21 stage 1: the Realistic style: what is shown (cloud, surface), the dots, the isovalue, the colours with their scale, the computed vs the measured dipole and the honest notes
static void realisticControls(molecule::MoleculeLab& L, Panel& p, const SectionFn& section) {
    namespace dn = molecule::dens;
    const molecule::Molecule& mo = L.mol();
    const bool ok = L.ensureReal();          // (0.57.1: with the swing's moment, as the picture asks)
    if (!section("realistic", "Electron density (computed)", true)) return;
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
        real::markLabels("molecule", "dots", {"20,000", "40,000", "60,000", "100,000"});
        if (pSegmented(p, "molecule.real.dots", {"20,000", "40,000", "60,000", "100,000"}, &di, &dnm)) L.set.realDots = molecule::kRealDots[di];
        pCaption(p, "Each dot is drawn at random from the computed density (a fixed seed: the same cloud every time). Dots show where the electrons are likely to be, not electrons as little balls.");
    }
    if (L.set.realShow != 1) {
        pHeading(p, "Surface: density cut-off (e/bohr³)");
        int iso = L.set.realIso;
        const std::vector<std::string> inm{"molecule.real.iso0010", "molecule.real.iso0016", "molecule.real.iso0020", "molecule.real.iso0100"};
        real::markLabels("molecule", "iso", {"0.001", "0.0016", "0.002", "0.01"});
        if (pSegmented(p, "molecule.real.iso", {"0.001", "0.0016", "0.002", "0.01"}, &iso, &inm)) L.set.realIso = iso;
        pCaption(p, "0.0016 matched measured molecular surfaces best (Alibakhshi and Schäfer, Nature Communications 2024, 104 molecules: 1.6 % error; 0.002 gave 3.2 %, 0.001 gave 7.0 %). 0.002 and 0.001: older, common cut-offs (0.001 is the larger, looser one). 0.01: a tight one, close to the nuclei. A molecule has no hard edge: the surface is a chosen cut-off of the density.");
        // 0.46.3 (outside review): it said "0.002: the usual convention (Bader)" and cited "a 2024 study" without naming it. The paper: A. Alibakhshi, L. V. Schäfer, Nat. Commun. 15, 6086 (2024),
        // doi:10.1038/s41467-024-50408-8 (abstract and record saved: data/molecules/density/alibakhshi_schaefer_2024_europepmc_record.json; the 3.17 % (0.002) and 6.98 % (0.001) were read in its
        // full text on PMC, PMC11271626, on 2026-11-08; the full text could not be saved). Bader's own 1987 paper (JACS 109, 7968) was not read, so no cut-off is credited to it here.
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
                       "The potential is the molecule's own at the shape shown" + std::string(molecule::dens::vibDensity(mo.id, 0) ? " (and, while a vibration plays, at the atoms' places then: Vibrations)." : "; vibrations are not shown in this style for this molecule yet.");
    pCaption(p, calc, false);
    pCaption(p, "Real pictures of molecules by atomic force microscopy (Gross et al., IBM Research Zurich, 2009: a tip ending in a CO molecule over a flat pentacene molecule) show forces, not the electron density.");
    pCaption(p, "Drawing: the surface is cut into triangles (marching tetrahedra on a 0.5 bohr grid) and drawn translucent over the cloud, far to near (no depth buffer: the cloud and the surface are not sorted together). It is lit from the viewer, so parts that are mirror images by symmetry get the same colour; when the molecule is turned towards you, the near side of the surface lies over a different part of the axis than the far side, so a symmetric molecule (CO₂, water) can look lopsided for a moment. Bonds are not drawn in this style: the real density has no sticks, because bonding moves only a small part of it; it is highest at the nuclei.");
}

// 0.23: the vibrations: one mode at a time (the list, the IR stick spectrum, play / pause, the swing's size) and the honest notes; Ball-and-stick and Space-filling only
static std::string wn(double nu) { char b[32]; std::snprintf(b, sizeof b, "%.0f cm⁻¹", nu); return b; }
// 0.29 (stage 3): live, every mode at once at a temperature (molecule/vibration.h says what is simplified; the caption says it too)
static void liveControls(molecule::MoleculeLab& L, Panel& p, const molecule::Molecule& mo) {
    namespace vb = molecule::vib;
    const Metrics& m = F().m;
    const auto& md = vb::modes(mo.id);
    char b[200];
    pGap(p, m.s4);
    std::snprintf(b, sizeof b, "Temperature: %.0f K (%.0f °C)", L.liveK, L.liveK - 273.15);
    pBody(p, b);
    float k = static_cast<float>(L.liveK);
    if (pSlider(p, "molecule.live.temp", &k, 0.0f, 3000.0f)) { L.liveK = std::round(k); L.moveLive(); }
    {
        const float third = (p.w - 2 * static_cast<float>(m.s8)) / 3;
        const ImVec2 o = here(p);
        if (button("molecule.live.0", o, ImVec2(third, static_cast<float>(m.control)), "0 K", Face::Quiet, false)) { L.liveK = 0; L.moveLive(); }
        if (button("molecule.live.room", ImVec2(o.x + third + m.s8, o.y), ImVec2(third, static_cast<float>(m.control)), "Room", Face::Quiet, false)) { L.liveK = 298.15; L.moveLive(); }
        if (button("molecule.live.hot", ImVec2(o.x + 2 * (third + m.s8), o.y), ImVec2(third, static_cast<float>(m.control)), "1,500 K", Face::Quiet, false)) { L.liveK = 1500; L.moveLive(); }
        p.y += m.control + m.s4;
    }
    int low = 0;
    for (int j = 1; j < static_cast<int>(md.size()); j++) if (vb::nuShown(mo.id, j) < vb::nuShown(mo.id, low)) low = j;
    std::snprintf(b, sizeof b, "%.3g kJ/mol above the lowest level", vb::thermalEnergyKJmol(mo.id, L.liveK));
    pStat(p, "Vibration energy", b);
    std::snprintf(b, sizeof b, "%s: %.3g quanta on average", wn(vb::nuShown(mo.id, low)).c_str(), vb::meanQuanta(vb::nuShown(mo.id, low), L.liveK));
    pStat(p, "Lowest mode", b);
    std::snprintf(b, sizeof b, "%.3f Å (root mean square)", vb::liveRmsA(mo, L.liveK));
    pStat(p, "Biggest atom swing", b);
    {
        const float half = (p.w - static_cast<float>(m.s8)) / 2;
        const ImVec2 o = here(p);
        if (button("molecule.vib.play", o, ImVec2(half, static_cast<float>(m.control)), L.vibPlaying ? "Pause" : "Play", Face::Secondary, false)) L.vibPlaying = !L.vibPlaying;
        if (button("molecule.live.restart", ImVec2(o.x + half + m.s8, o.y), ImVec2(half, static_cast<float>(m.control)), "Restart", Face::Quiet, false)) { L.liveT = 0; L.moveLive(); }
        p.y += m.control + m.s4;
    }
    int sz = L.vibSize;
    const std::vector<std::string> szn{"molecule.vib.real", "molecule.vib.big"};
    if (pSegmented(p, "molecule.vib.size", {"Real size", "3 times bigger"}, &sz, &szn)) { L.vibSize = sz; L.moveLive(); }
    if (L.vibSize == 1 && vb::liveDrawnScale(mo, L.liveK, true) < vb::kExaggerate) { std::snprintf(b, sizeof b, "Drawn %.1f times, so the typical swing stays under %.1f Å.", vb::liveDrawnScale(mo, L.liveK, true), vb::kMaxDrawnA); pCaption(p, b); }
    pCaption(p, "Every mode moves at once, each at its true speed (slowed down) and with its size at this temperature: each is a quantum oscillator holding "
                "n = 1 / (e^(hcν/kT) − 1) quanta on average, so its spread is the zero-point one at 0 K and grows as it warms. Simplified: each mode keeps one size "
                "(a real one varies from moment to moment), the starting phases are fixed, the springs are perfect (bonds never break, no anharmonicity), the molecule "
                "does not turn, and real atoms are smeared out, not points.");
    if (ui::look().reduced) pCaption(p, "Motion is Reduced (Settings): one moment is held.");
}

// 0.53.1: the anharmonic bond of a diatomic (molecule/morse.h): the tick box, the level and the readout. The Bond energy card (moleculeBondCard) draws the curve.
static std::string ang(double r) { char b[32]; std::snprintf(b, sizeof b, "%.3f Å", r); return b; }
static std::string cmK(double v) {   // 12,345 cm⁻¹
    char b[48]; const long n = std::lround(v);
    if (n >= 1000) std::snprintf(b, sizeof b, "%ld,%03ld cm⁻¹", n / 1000, n % 1000); else std::snprintf(b, sizeof b, "%ld cm⁻¹", n);
    return b;
}
static void anharmonicControls(molecule::MoleculeLab& L, Panel& p, const molecule::Molecule& mo) {
    namespace ms = molecule::morse;
    const Metrics& m = F().m;
    pGap(p, m.s4);
    real::markCheck("molecule", "anharm");
    bool a = L.vibAnharm;
    if (pCheck(p, "molecule.vib.anharm", "Anharmonic (real bond)", &a)) { L.vibAnharm = a; L.vibT = 0; L.moveVib(); }
    noteItem("molecule.vib.anharm");
    if (!L.anharmOn()) { pCaption(p, "Tick it for the bond as it really is: its rungs close up as it stretches, and at the top it breaks.", true); return; }
    const ms::Curve c = ms::curve(mo.id);
    const int vm = ms::vMax(c), top = L.anharmTop();
    L.vibLevel = std::clamp(L.vibLevel, 0, top);
    const int v = L.vibLevel;
    const bool apart = v > vm, meas = ms::isMeasured(mo.id, v);
    char b[260];
    if (apart) std::snprintf(b, sizeof b, "above v = %d: dissociated", vm);
    else std::snprintf(b, sizeof b, "v = %d of 0 – %d, %s", v, vm, meas ? "measured" : "model (Morse)");
    pStat(p, "Level", b);
    {
        const float w = (p.w - 2 * static_cast<float>(m.s8)) / 4;
        const ImVec2 o = here(p);
        if (button("molecule.vib.vdown", o, ImVec2(w, static_cast<float>(m.control)), "−", Face::Quiet, false) && v > 0) { L.vibLevel = v - 1; L.vibT = 0; L.moveVib(); }
        noteItem("molecule.vib.vdown");
        if (button("molecule.vib.vup", ImVec2(o.x + w + m.s8, o.y), ImVec2(w, static_cast<float>(m.control)), "+", Face::Quiet, false) && v < top) { L.vibLevel = v + 1; L.vibT = 0; L.moveVib(); }
        noteItem("molecule.vib.vup");
        if (button("molecule.vib.vtop", ImVec2(o.x + 2 * (w + m.s8), o.y), ImVec2(2 * w, static_cast<float>(m.control)), "Last bound", Face::Quiet, v == vm)) { L.vibLevel = vm; L.vibT = 0; L.moveVib(); }
        noteItem("molecule.vib.vtop");
        p.y += m.control + m.s4;
    }
    const double E = L.anharmE();
    const ms::Motion mt = ms::motion(c, E);
    if (apart) {
        pStat(p, "Energy", cmK(E) + " (above the well's depth De " + cmK(c.De) + ")");
        pBody(p, std::string("Dissociated: with more energy than the well holds the atoms fly apart and do not come back. Reset brings the molecule back."), true);
        pStat(p, "Bond now", L.anharmR >= c.re + molecule::MoleculeLab::kApartA - 1e-9 ? "more than " + ang(c.re + molecule::MoleculeLab::kApartA) + " (drawn there)" : ang(L.anharmR));
    } else {
        std::snprintf(b, sizeof b, "G(%d) = %s, %s", v, cmK(E).c_str(), meas ? "measured" : "Morse model");
        pStat(p, "Energy", b);
        pStat(p, "Turning points", ang(mt.rIn) + " and " + ang(mt.rOut));
        std::snprintf(b, sizeof b, "%s (re %.4f Å)", ang(mt.rMean).c_str(), c.re);
        pStat(p, "Mean bond length", b);
        std::snprintf(b, sizeof b, "%.1f fs real, %.2f s on screen", mt.periodS * 1e15, mt.periodS * molecule::vib::slowDown());
        pStat(p, "One swing", b);
    }
    pCaption(p, std::string("A classical swing in the Morse curve at the level's energy (the Bond energy card). Measured levels: Huber & Herzberg's constants, v ≤ ") + std::to_string(ms::measuredTop(mo.id)) +
                "; above them the Morse model, fixed by ωe and ωexe alone. A real molecule in level v is a wave spread between (and a little beyond) the turning points, not a point swinging.", true);
}

// The Bond energy card: the Morse curve against bond length, the levels (measured solid, model dashed), the current level with its turning points, D0 and De, and the
// Morse-against-measured rows (D0, and the Pekeris alpha_e of the rotation)
void moleculeBondCard(AppState& app, Panel& p) {
    namespace ms = molecule::morse;
    molecule::MoleculeLab& L = app.molecule;
    const molecule::Molecule& mo = L.mol();
    if (!ms::has(mo.id)) return;
    const Metrics& m = F().m;
    const ms::Curve c = ms::curve(mo.id);
    const int vm = ms::vMax(c), shown = L.vibLevel;
    const double d0 = ms::d0Measured(mo.id), g0 = ms::levelShown(mo.id, 0), Eshown = L.anharmE();
    pCaption(p, molecule::prettyFormula(mo.formula) + ": energy against bond length", true);
    const ImVec2 o = here(p);
    const float labW = width(Type::Caption, false, "De (Morse)") + m.s4, W = p.w - labW, H = static_cast<float>(m.em(11));
    const double rLo = c.re - std::log1p(std::sqrt(1.12)) / c.a, rHi = c.re + 5.5 / c.a, eTop = 1.12 * c.De;
    const auto X = [&](double r) { return o.x + static_cast<float>((r - rLo) / (rHi - rLo)) * W; };
    const auto Y = [&](double e) { return o.y + H - static_cast<float>(e / eTop) * H; };
    // axes
    p.dl->AddLine(ImVec2(o.x, o.y + H), ImVec2(o.x + W, o.y + H), cMuted(0.7f), 1.0f);
    p.dl->AddLine(ImVec2(o.x, o.y), ImVec2(o.x, o.y + H), cMuted(0.7f), 1.0f);
    for (double r = std::ceil(rLo * 2) / 2; r <= rHi; r += 0.5) {
        const float x = X(r);
        p.dl->AddLine(ImVec2(x, o.y + H), ImVec2(x, o.y + H + 3), cMuted(), 1.0f);
        char t[16]; std::snprintf(t, sizeof t, "%.1f", r);
        text(p.dl, Type::Caption, false, ImVec2(x - width(Type::Caption, false, t) / 2, o.y + H + 4), cMuted(), t);
    }
    // the levels between their turning points (measured solid, model dashed)
    const auto dashed = [&](float x0, float x1, float y, ImU32 col, float th) { for (float x = x0; x < x1; x += 6) p.dl->AddLine(ImVec2(x, y), ImVec2(std::min(x1, x + 3.5f), y), col, th); };
    for (int v = 0; v <= vm; v++) {
        const double E = ms::levelShown(mo.id, v);
        const ms::Motion mt = ms::motion(c, E);
        const float x0 = X(mt.rIn), x1 = std::min(X(mt.rOut), o.x + W), y = Y(E);
        const ImU32 col = cText(vm > 40 ? 0.35f : 0.55f);
        if (v == shown) continue;
        if (ms::isMeasured(mo.id, v)) p.dl->AddLine(ImVec2(x0, y), ImVec2(x1, y), col, 1.2f); else dashed(x0, x1, y, col, 1.0f);
    }
    // De (the curve's asymptote) and the measured dissociation (D0 above v = 0)
    dashed(o.x, o.x + W, Y(c.De), cMuted(0.8f), 1.0f);
    text(p.dl, Type::Caption, false, ImVec2(o.x + W + m.s4, Y(c.De) - lineH(Type::Caption) / 2), cMuted(), "De (Morse)");
    if (std::isfinite(d0)) {
        const float y = Y(g0 + d0);
        p.dl->AddLine(ImVec2(o.x, y), ImVec2(o.x + W, y), cText(0.95f), 1.8f);
        text(p.dl, Type::Caption, false, ImVec2(o.x + W + m.s4, y - lineH(Type::Caption) / 2), cText(), "D₀ meas.");
    }
    // the curve
    {
        std::vector<ImVec2> pts;
        for (int i = 0; i <= 160; i++) { const double r = rLo + (rHi - rLo) * i / 160.0; pts.push_back(ImVec2(X(r), Y(std::min(ms::V(c, r), eTop)))); }
        p.dl->AddPolyline(pts.data(), static_cast<int>(pts.size()), cText(0.9f), 0, 2.0f);
    }
    // the current level, lit, with its turning points (dissociated: from the inner wall out to the edge)
    {
        const ms::Motion mt = ms::motion(c, Eshown);
        const float y = Y(std::min(Eshown, eTop)), x0 = X(mt.rIn), x1 = mt.bound ? X(mt.rOut) : o.x + W;
        p.dl->AddLine(ImVec2(x0, y), ImVec2(x1, y), cAccent(), 2.5f);
        p.dl->AddCircleFilled(ImVec2(x0, y), 4.0f, cAccent());
        if (mt.bound) p.dl->AddCircleFilled(ImVec2(x1, y), 4.0f, cAccent());
        if (L.anharmOn()) { const float xr = X(std::min(L.anharmR, rHi)); p.dl->AddCircle(ImVec2(xr, y), 6.0f, cAccent(), 16, 1.5f); }
        const std::string lab = shown > vm ? "dissociated" : "v = " + std::to_string(shown);
        text(p.dl, Type::Caption, true, ImVec2(std::min(x1 + m.s4, o.x + W - width(Type::Caption, true, lab)), y - lineH(Type::Caption) - 1), cAccent(), lab);
    }
    p.y += H + lineH(Type::Caption) + m.s4;
    pCaption(p, "Bond length in Å. Solid: measured levels (v ≤ " + std::to_string(ms::measuredTop(mo.id)) + "); dashed: Morse model, to the last bound level v = " + std::to_string(vm) +
                ". Lit: the level shown; ring: the bond now.", true);
    // Morse against measured
    pGap(p, m.s4);
    char b[200];
    const double dm = ms::d0Morse(c);
    pStat(p, "De (Morse, ωe² / 4ωexe)", cmK(c.De));
    if (std::isfinite(d0)) {
        pStat(p, "D₀ measured", cmK(d0) + std::string(" (") + ms::d0Source(mo.id) + ")");
        std::snprintf(b, sizeof b, "%s, %+.1f %% against measured", cmK(dm).c_str(), (dm / d0 - 1) * 100);
        pStat(p, "D₀ Morse = De − G(0)", b);
        pCaption(p, ms::morseGapWords(mo.id), true);                  // 0.86.0: the gap computed (molecule/morse.h morseGap)
    } else {
        pStat(p, "D₀ Morse = De − G(0)", cmK(dm) + " (model, no measured value saved)");
    }
    const double ap = ms::alphaPekeris(mo.id), am = ms::alphaMeasured(mo.id);
    std::snprintf(b, sizeof b, "%.5f against measured %.5f cm⁻¹ (%+.1f %%)", ap, am, (ap / am - 1) * 100);
    pStat(p, "αe Morse (Pekeris)", b);
    pCaption(p, std::string("Pekeris: αe = 6√(ωexe Be³)/ωe − 6Be²/ωe, B's fall per quantum if the bond were a Morse curve. The Rotation section uses the measured αe.") +
                (mo.id == "O2" ? " O₂: Be = B₀ + αe/2 (the table gives B₀)." : ""), true);
    if (shellLogOn()) { char t[160]; std::snprintf(t, sizeof t, "bondcard mol=%s lev=%d vmax=%d", mo.id.c_str(), shown, vm); auditNote(t); }
}

static void vibrationControls(molecule::MoleculeLab& L, Panel& p, const SectionFn& section) {
    namespace vb = molecule::vib;
    if (shellLogOn()) { char b[260]; std::snprintf(b, sizeof b, "vib mode=%d on=%d phase=%.3f size=%d playing=%d live=%d liveK=%.2f liveT=%.3f anhbox=%d anh=%d lev=%d diss=%d r=%.4f", L.vibMode, L.vibOn() ? 1 : 0, L.vibPhase, L.vibSize, L.vibPlaying ? 1 : 0, L.liveOn() ? 1 : 0, L.liveK, L.liveT,
                                          molecule::morse::has(L.mol().id) && L.set.style != molecule::Style::Realistic && L.vibOn() ? 1 : 0, L.anharmOn() ? 1 : 0, L.vibLevel, L.anharmOn() && L.vibLevel > L.anharmTop() - 1 ? 1 : 0, L.anharmOn() ? L.anharmR : 0.0); auditNote(b); }   // (for the automatic checks)
    if (!section("vib", "Vibrations", true)) return;
    const molecule::Molecule& mo = molecule::get(L.set.mol);
    const auto& md = vb::modes(mo.id);
    const bool dens = L.set.style == molecule::Style::Realistic;
    if (dens && !molecule::dens::vibDensity(mo.id, 0)) { pCaption(p, "In the Density style only water vibrates so far (a first test of the electron cloud moving with its atoms); this molecule's vibrations are in Ball-and-stick and Space-filling."); return; }
    if (md.empty()) { pCaption(p, "No calculated vibrations for this molecule."); return; }
    const Metrics& m = F().m;
    const bool on = L.vibOn();
    if (pRow(p, "molecule.vib.still", "Standing still", !on && !L.liveOn() && !L.rotOn())) { L.setVib(-1); L.rotTurning = false; }
    if (!dens && pRow(p, "molecule.vib.live", "All at once, warm (live)", L.liveOn())) L.setLive();
    std::vector<int> rows;                                         // (0.23 stage 3: in the order of the wavenumber SHOWN, the measured one: benzene listed 992 before 975)
    for (int k = 0; k < static_cast<int>(md.size()); k++) if (vb::degenerateFirst(mo.id, k) == k) rows.push_back(k);
    std::stable_sort(rows.begin(), rows.end(), [&](int a, int b) { return vb::nuShown(mo.id, a) < vb::nuShown(mo.id, b); });
    for (const int k : rows) {
        const int dg = vb::degeneracy(mo.id, k);
        std::string label = wn(vb::nuShown(mo.id, k)) + "  ·  " + vb::character(mo, md[k]).shortWords;
        if (dg > 1) label += "  ·  ×" + std::to_string(dg);
        if (!vb::irActive(md[k])) label += "  ·  no IR";
        const bool sel = on && vb::degenerateFirst(mo.id, L.vibMode) == k;
        if (pRow(p, ("molecule.vib." + std::to_string(k)).c_str(), label, sel)) L.setVib(k);
    }
    // the infrared stick spectrum: wavenumber falling to the right (as chemists draw it), each degenerate set one stick, its height its calculated intensity
    {
        pGap(p, m.s4);
        const bool measIr = vb::irMeasuredAll(mo.id);
        pHeading(p, measIr ? "Infrared spectrum (measured, NIST CCCBDB)" : "Infrared spectrum (heights calculated)");   // (0.34: the longer heading was cut off at the panel's edge)
        const ImVec2 o = here(p);
        const float W = p.w, Hh = static_cast<float>(5 * m.row / 2), base = o.y + Hh, lo = 300, hi = 4500;
        const auto X = [&](double nu) { return o.x + static_cast<float>((hi - nu) / (hi - lo)) * W; };
        double top = 0;
        const auto height = [&](int k) {                         // a set's band: the measured intensity (CCCBDB lists the whole degenerate band), or the calculated ones added up
            if (measIr) { const vb::Measured* e = vb::measured(mo.id, k); return e && std::isfinite(e->ir) ? e->ir : 0.0; }
            double a = 0; for (int j = k; j < k + vb::degeneracy(mo.id, k); j++) a += md[j].irKmMol; return a; };
        for (int k = 0; k < static_cast<int>(md.size()); k++) if (vb::degenerateFirst(mo.id, k) == k) top = std::max(top, height(k));
        p.dl->AddLine(ImVec2(o.x, base), ImVec2(o.x + W, base), cMuted(0.7f), 1.0f);
        for (int t = 1000; t <= 4000; t += 1000) {
            const float x = X(t);
            p.dl->AddLine(ImVec2(x, base), ImVec2(x, base + 3), cMuted(), 1.0f);
            const std::string s = std::to_string(t);
            text(p.dl, Type::Caption, false, ImVec2(x - width(Type::Caption, false, s) / 2, base + 4), cMuted(), s);
        }
        for (int k = 0; k < static_cast<int>(md.size()); k++) {
            if (vb::degenerateFirst(mo.id, k) != k) continue;
            const double a = height(k);
            const bool sel = on && vb::degenerateFirst(mo.id, L.vibMode) == k;
            const float x = X(vb::nuShown(mo.id, k));
            if (vb::irActive(md[k]) && top > 0) p.dl->AddLine(ImVec2(x, base), ImVec2(x, base - std::max(2.0f, static_cast<float>(a / top) * (Hh - 4))), sel ? cAccent() : cText(0.8f), sel ? 3.0f : 2.0f);
            else p.dl->AddCircle(ImVec2(x, base - 3), 3.0f, sel ? cAccent() : cMuted(), 12, 1.2f);
        }
        ImGui::SetCursorScreenPos(o);
        if (ImGui::InvisibleButton("molecule.vib.plot", ImVec2(W, Hh + lineH(Type::Caption)))) {   // a click picks the nearest stick
            const float mx = ImGui::GetIO().MousePos.x;
            int best = -1; float bd = 10;
            for (int k = 0; k < static_cast<int>(md.size()); k++) if (vb::degenerateFirst(mo.id, k) == k && std::fabs(X(vb::nuShown(mo.id, k)) - mx) < bd) { bd = std::fabs(X(vb::nuShown(mo.id, k)) - mx); best = k; }
            if (best >= 0) L.setVib(best);
        }
        noteItem("molecule.vib.plot");
        p.y += Hh + lineH(Type::Caption) + m.s4;
        char b[240];
        if (top > 0) std::snprintf(b, sizeof b, "Wavenumber in cm⁻¹ (measured). The tallest stick is %.0f km/mol (%s); a circle is a mode with no infrared band (its swing does not change the dipole).", top, measIr ? "measured" : "calculated");
        else std::snprintf(b, sizeof b, "Wavenumber in cm⁻¹ (measured). No vibration of this molecule absorbs infrared light: a circle is a mode whose swing does not change the dipole.");   // 0.34: was "the tallest stick is 0 km/mol"
        pCaption(p, b);
    }
    if (L.liveOn()) liveControls(L, p, mo);
    if (on && molecule::morse::has(mo.id)) anharmonicControls(L, p, mo);
    if (on) {
        const vb::Mode& v = md[L.vibMode];
        char b[200];
        pBody(p, "This mode " + vb::character(mo, v).words + ".");
        const vb::Measured* e = vb::measured(mo.id, L.vibMode);
        const double nu = vb::nuShown(mo.id, L.vibMode);
        const bool fermi = e && mo.id == "CO2" && e->mode == 1;   // 0.46.6 (outside review): CO2's nu1 is never seen at 1333 cm^-1 (Fermi resonance)
        if (e) {
            pStat(p, fermi ? "Listed" : "Measured", wn(e->fund) + " (NIST CCCBDB, " + e->ref + ")");
            if (fermi) pCaption(p, "No spectrum shows a line at 1333 cm⁻¹. This stretch and the bend's overtone (two bend quanta, 2 × 667 = 1334 cm⁻¹) have almost the same energy, so they mix "
                                   "(Fermi resonance) and push apart: the gas shows two Raman lines, at 1285 and 1388 cm⁻¹, each part stretch and part bend (McCluskey & Stoker, arXiv:physics/0601182). "
                                   "1333 cm⁻¹ is the stretch without that mixing. The swing drawn is the pure stretch.", true);
            pCaption(p, std::string("CCCBDB: mode ") + std::to_string(e->mode) + ", symmetry " + e->sym + (e->desc[0] ? std::string(", ") + e->desc : std::string()) + (std::isfinite(e->harm) ? "; harmonic " + wn(e->harm) : std::string()) + ".", true);
        }
        if (const vb::Anharmonic* an = vb::anharmonic(mo.id)) {   // 0.34: the measured ladder of a diatomic
            const double r1 = vb::termG(*an, 1) - vb::termG(*an, 0), r2 = vb::termG(*an, 2) - vb::termG(*an, 1), r3 = vb::termG(*an, 3) - vb::termG(*an, 2);
            std::snprintf(b, sizeof b, "%.0f, %.0f, %.0f cm⁻¹", r1, r2, r3);
            pStat(p, "Rungs 0→1, 1→2, 2→3", b);
            char c[600];
            std::snprintf(c, sizeof c, "From the measured constants ωe %.1f and ωexe %.2f cm⁻¹ (Huber & Herzberg 1979, NIST WebBook)%s%s. Each step up the ladder is a little "
                          "smaller than the one below: the bond gets softer as it stretches, until it breaks. %s",
                          an->we, an->wexe, an->note[0] ? "; " : "", an->note, L.anharmOn() ? "The swing drawn is the anharmonic one (above)." : "The swing drawn here is harmonic (equal steps); tick Anharmonic for the real bond.");
            pCaption(p, c, true);
        }
        if (v.nuCalc > 0) { std::snprintf(b, sizeof b, "%.0f cm⁻¹ harmonic (× %.4f = %.0f)", v.nuCalc, vb::scaleOf(mo.id), v.nuCalc * vb::scaleOf(mo.id)); pStat(p, "Calculated", b); }
        else { std::snprintf(b, sizeof b, "%.0f i cm⁻¹: unstable at the measured shape", -v.nuCalc); pStat(p, "Calculated", b);
               pCaption(p, "At the measured shape the calculation finds no restoring force for this twist (the shape is not the calculation's own minimum). The swing uses the measured wavenumber; its pattern is the calculated one."); }
        if (e && mo.id == "NH3" && e->mode == 2) pCaption(p, "Measured as two levels, 932.5 and 968.3 cm⁻¹ (NIST WebBook, Shimanouchi 1972): the nitrogen tunnels through the plane of the three hydrogens (the umbrella turns inside out), which splits the level. A harmonic calculation has one level: compare it with the nearer one.");
        if (e && mo.id == "CH3OH" && e->mode == 12) pCaption(p, "A hindered rotation of the O–H about the C–O bond, far from a harmonic swing (NIST CCCBDB lists it among the modes with big anharmonicity). Its calculated wavenumber is not trustworthy: it lies BELOW the measured one, which anharmonicity does not explain, because the calculation is done at the measured shape, not at its own minimum, and a twist is very sensitive to that. The swing uses the measured wavenumber's speed and the calculated pattern.");
        if (!L.anharmOn()) { std::snprintf(b, sizeof b, "%.1f fs real, %.2f s on screen", vb::periodS(nu) * 1e15, vb::screenPeriodS(nu)); pStat(p, "One swing", b); }
        if (e && std::isfinite(e->ir)) {
            if (std::isfinite(e->irUnc)) std::snprintf(b, sizeof b, "%.1f ± %.1f km/mol measured (%s)", e->ir, e->irUnc, e->irRef); else std::snprintf(b, sizeof b, "%.1f km/mol measured (%s)", e->ir, e->irRef);
            pStat(p, "Infrared absorption", b);
            std::snprintf(b, sizeof b, "%.4g km/mol calculated%s", v.irKmMol * vb::degeneracy(mo.id, L.vibMode), vb::degeneracy(mo.id, L.vibMode) > 1 ? " (the whole band)" : "");
            pCaption(p, b, true);
        } else {
        if (vb::irActive(v) && v.irKmMol < 0.1) std::snprintf(b, sizeof b, "%.4f km/mol (calculated; very weak)", v.irKmMol);
        else if (vb::irActive(v)) std::snprintf(b, sizeof b, "%.1f km/mol (calculated)", v.irKmMol); else std::snprintf(b, sizeof b, "none: the dipole does not change");
        pStat(p, "Infrared absorption", b);
        }
        if (!L.anharmOn()) { std::snprintf(b, sizeof b, "%.3f Å (zero-point turning point)", vb::largestMoveA(mo, v)); pStat(p, "Biggest atom swing", b); }
        {
            const float half = (p.w - static_cast<float>(m.s8)) / 2;
            const ImVec2 o = here(p);
            if (button("molecule.vib.play", o, ImVec2(half, static_cast<float>(m.control)), L.vibPlaying ? "Pause" : "Play", Face::Secondary, false)) L.vibPlaying = !L.vibPlaying;
            const bool apart = L.anharmOn() && L.vibLevel >= L.anharmTop();
            if (button("molecule.vib.step", ImVec2(o.x + half + m.s8, o.y), ImVec2(half, static_cast<float>(m.control)), apart ? "Reset" : "Restart swing", Face::Quiet, false)) { L.vibT = 0; L.moveVib(); }
            p.y += m.control + m.s4;
        }
        int sz = L.vibSize;
        const std::vector<std::string> szn{"molecule.vib.real", "molecule.vib.big"};
        real::markLabels("molecule", "vibsize", {"Real size", "3 times bigger"});
        if (L.anharmOn()) pCaption(p, "The anharmonic swing is always drawn at its real size.");
        else if (pSegmented(p, "molecule.vib.size", {"Real size", "3 times bigger"}, &sz, &szn)) L.vibSize = sz;
        if (!L.anharmOn() && L.vibSize == 1 && vb::drawnScale(mo, v, true) < vb::kExaggerate) { std::snprintf(b, sizeof b, "This swing is already big: drawn %.1f times, so no atom moves more than %.1f Å.", vb::drawnScale(mo, v, true), vb::kMaxDrawnA); pCaption(p, b); }
        if (ui::look().reduced) pCaption(p, L.anharmOn() ? "Motion is Reduced (Settings): the atoms are held at the outer turning point." : "Motion is Reduced (Settings): the atoms are held at one end of the swing.");
        if (vb::degeneracy(mo.id, L.vibMode) > 1) pCaption(p, std::to_string(vb::degeneracy(mo.id, L.vibMode)) + " modes share this wavenumber; one of them is shown (any mixture of them is an equally good mode).");
    }
    char sd[96]; std::snprintf(sd, sizeof sd, "%.0e", vb::slowDown());
    std::string slow = sd; { const auto e = slow.find("e+"); if (e != std::string::npos) slow = slow.substr(0, e) + " × 10" + std::string(slow.substr(e + 2) == "13" ? "¹³" : slow.substr(e + 2) == "14" ? "¹⁴" : "^" + slow.substr(e + 2)); }
    pCaption(p, "Slowed down " + slow + " times, every mode at its true speed relative to the others. The swing is as big as the spread of the lowest vibrational level (or 3 times that, as chosen). "
                "A real molecule does not swing like a ball on a spring: in its lowest level its atoms are smeared out (a wave), and every mode is present at once.", true);
    pCaption(p, "Measured: the wavenumbers, the symmetry and (where CCCBDB lists one) the infrared intensity, from NIST CCCBDB. Calculated: which way each atom moves, and the intensities CCCBDB does not list: "
                "B3LYP / " + std::string(vb::basisOf(mo.id)) + " (PySCF 2.14), harmonic, at the measured shape. CCCBDB's factor for this method, " + "0.968 ± 0.019, takes a calculated harmonic wavenumber to the measured band.", true);
    if (dens) pCaption(p, "The cloud and the surface at each moment are the calculated electron density for the atoms where they are then (the electrons follow the nuclei at once, the Born–Oppenheimer picture): "
                          "calculated at both ends of the \"3 times bigger\" swing (B3LYP / aug-cc-pVTZ, as the still density) and blended smoothly between them. The colour scale stays the still molecule's.", true);
    if (mo.id == "O3") pCaption(p, "Ozone: the calculation is less certain (closed-shell B3LYP for a molecule with multi-reference character); it gives the symmetric stretch almost no infrared band, the measurement 1.0 km/mol.", true);
    // 0.46.6 (outside review): name the bands that do the greenhouse work. The warm Earth (about 288 K) gives off most light per cm^-1 near 565 cm^-1 (18 um); Planck's law
    // B(nu, 288 K) relative to 667 cm^-1: 1595 -> 0.13, 2349 -> 0.009, 3657 -> 0.0001 (computed from CODATA h, c, k)
    if (mo.id == "CO2") pCaption(p, "Why this matters: the warm Earth (about 288 K) gives off its infrared light mostly at long wavelengths, the most per cm⁻¹ near 565 cm⁻¹ (18 µm; Planck's law). "
                                    "Carbon dioxide's bend, 667 cm⁻¹ (15 µm), lies right there: it is the band that makes CO₂ a greenhouse gas. The tallest stick, 2349 cm⁻¹ (4.3 µm), lies where Earth "
                                    "gives off only about 1 % as much per cm⁻¹, so it matters little for the greenhouse effect.", true);
    if (mo.id == "H2O") pCaption(p, "Why this matters: the warm Earth (about 288 K) gives off its infrared light mostly at long wavelengths, the most per cm⁻¹ near 565 cm⁻¹ (18 µm; Planck's law). "
                                    "Water vapour's greenhouse work is done by its bend, 1595 cm⁻¹ (6.3 µm), and above all by its pure-rotation band in the far infrared (the molecule turning, "
                                    "not vibrating, so it is not in this list). The O–H stretches near 2.7 µm lie where Earth gives off almost nothing (about 10,000 times less per cm⁻¹ than at 15 µm); they absorb some sunlight instead.", true);
    if (mo.id == "CO2" || mo.id == "H2O" || mo.id == "N2" || mo.id == "O2")
        pCaption(p, "N₂ and O₂, most of the air, have no allowed infrared band: their one stretch does not change a dipole. They still absorb a little when two molecules collide "
                    "(collision-induced absorption), which matters in dense atmospheres such as Titan's.", true);
}

// 0.53: rotation (molecule/rotation.h). The section: the temperature (the lab's one), the level shown, HCl's isotope, what the card's spectrum shows, the turning picture.
static std::string cm1(double v, int dec) { char b[48]; std::snprintf(b, sizeof b, "%.*f cm⁻¹", dec, v); return b; }
static std::string pct(double v) {   // a share: 12.3 %, 0.42 %, 3.1e-07
    char b[48];
    if (v >= 1e-3) std::snprintf(b, sizeof b, "%.1f %%", v * 100); else if (v >= 1e-6) std::snprintf(b, sizeof b, "%.2g %%", v * 100); else std::snprintf(b, sizeof b, "%.1e", v);
    return b;
}
static const char* qName(const molecule::rot::Rotor& r) { return r.nLevels ? "N" : "J"; }
// 0.97.0 (outside review of 28 Nov 2026, second, part 1): the words that name the ortho and para case shown (molecule/rotation.h SpinMix)
static std::string spinMixText(const molecule::rot::Rotor& r) {
    namespace rot = molecule::rot;
    if (!rot::hasSpinForms(r)) return "";
    char b[400];
    if (r.mix == rot::SpinMix::Kept) {
        std::snprintf(b, sizeof b, "Shown: ortho and para kept as when warm (no catalyst). Even J keep %.3g %% of the molecules and odd J %.3g %% at every temperature, each spread over its own levels.",
                      100 * rot::formShare(r, 0), 100 * rot::formShare(r, 1));
        std::string t = b;
        if (r.id == "H2") {
            std::snprintf(b, sizeof b, " Without a catalyst the change is slow: in liquid hydrogen the extra ortho takes about %.0f days to halve (Woolley, Scott & Brickwedde 1948).", rot::liquidOrthoHalfHours(0.75) / 24);
            t += b;
        }
        return t;
    }
    return std::string("Shown: ortho and para in balance (with a catalyst): every level in one Boltzmann sum.") +
           (r.id == "H2" ? " The para share matches Woolley, Scott & Brickwedde's 1948 table within 0.005 percentage points." : "");
}
static bool hasLines(const molecule::rot::Rotor& r) { return r.dipole; }
static molecule::rot::Spectrum specOf(const molecule::MoleculeLab& L, const molecule::rot::Rotor& r) {
    namespace rot = molecule::rot;
    return L.rotSpec == 1 && rot::hasSpectrum(r, rot::Spectrum::Fundamental) ? rot::Spectrum::Fundamental : rot::Spectrum::PureRotation;
}
static void rotationControls(molecule::MoleculeLab& L, Panel& p, const SectionFn& section) {
    namespace rot = molecule::rot;
    const molecule::Molecule& mo = molecule::get(L.set.mol);
    if (shellLogOn()) { char b[200]; std::snprintf(b, sizeof b, "rot linear=%d turning=%d on=%d J=%d iso=%d spec=%d card=%d angle=%.4f T=%.2f", rot::hasRotation(mo) ? 1 : 0, L.rotTurning ? 1 : 0, L.rotOn() ? 1 : 0, rot::hasRotation(mo) ? L.rotLevel() : -1, L.rotIso, L.rotSpec, L.rotCard ? 1 : 0, L.rotAngle, L.liveK); auditNote(b); }
    if (!section("rot", "Rotation", true)) return;
    if (!rot::hasRotation(mo)) { pCaption(p, "Rotation of non-linear molecules is not shown yet."); return; }
    const Metrics& m = F().m;
    const rot::Rotor r = L.rotor();
    const char* q = qName(r);
    const int J = L.rotLevel(), best = rot::mostPopulated(r, L.liveK);
    char b[240];
    std::snprintf(b, sizeof b, "Temperature: %.0f K (%.0f °C)", L.liveK, L.liveK - 273.15);
    pBody(p, b);
    float k = static_cast<float>(L.liveK);
    if (pSlider(p, "molecule.rot.temp", &k, 0.0f, 3000.0f)) { L.liveK = std::round(k); if (L.liveOn()) L.moveLive(); }
    if (L.rotJ < 0) std::snprintf(b, sizeof b, "%s = %d, the most populated at %.0f K", q, J, L.liveK);
    else std::snprintf(b, sizeof b, "%s = %d (the most populated at %.0f K: %d)", q, J, L.liveK, best);
    pStat(p, "Level shown", b);
    {
        const float w = (p.w - 2 * static_cast<float>(m.s8)) / 4;
        const ImVec2 o = here(p);
        const int step = (r.gEven == 0 || r.gOdd == 0) ? 2 : 1;     // (O2 and CO2: only every other level exists)
        if (button("molecule.rot.jdown", o, ImVec2(w, static_cast<float>(m.control)), "−", Face::Quiet, false) && J - step >= rot::lowestJ(r)) L.rotJ = J - step;
        noteItem("molecule.rot.jdown");
        if (button("molecule.rot.jup", ImVec2(o.x + w + m.s8, o.y), ImVec2(w, static_cast<float>(m.control)), "+", Face::Quiet, false) && J + step <= 60) L.rotJ = J + step;
        noteItem("molecule.rot.jup");
        if (button("molecule.rot.jauto", ImVec2(o.x + 2 * (w + m.s8), o.y), ImVec2(2 * w, static_cast<float>(m.control)), "Most populated", Face::Quiet, L.rotJ < 0)) L.rotJ = -1;
        noteItem("molecule.rot.jauto");
        p.y += m.control + m.s4;
    }
    std::snprintf(b, sizeof b, "%s above %s = 0; %s of the molecules", cm1(rot::term(r, 0, J), 2).c_str(), q, pct(rot::share(r, J, L.liveK)).c_str());
    pStat(p, "Energy, share", b);
    if (rot::hasSpinForms(r)) {   // 0.97.0: ortho and para (H2, N2, C2H2)
        pHeading(p, "Ortho and para");
        int mx = L.set.spinMix;
        const std::vector<std::string> on{"molecule.rot.opara.kept", "molecule.rot.opara.balance"};
        real::markLabels("molecule", "opara", {"Kept as when warm", "In balance"});
        if (pSegmented(p, "molecule.rot.opara", {"Kept as when warm", "In balance"}, &mx, &on)) { L.set.spinMix = mx; L.rotJ = -1; }
        pCaption(p, "Kept as when warm: no catalyst, the two spin forms stay as they were at room temperature. In balance: with a catalyst. " + spinMixText(L.rotor()), true);
    }
    if (mo.id == "HCl") {
        int iso = L.rotIso;
        std::vector<std::string> lab, nm;
        for (int i = 0; i < rot::kIsoCount; i++) { lab.push_back(rot::isoShort(i)); nm.push_back("molecule.rot.iso." + std::to_string(i)); }
        if (pSegmented(p, "molecule.rot.iso", lab, &iso, &nm)) { L.rotIso = iso; L.rotLine = -1; }
        int sp = L.rotSpec;
        const std::vector<std::string> sn{"molecule.rot.spec.pure", "molecule.rot.spec.band"};
        if (pSegmented(p, "molecule.rot.spec", {"Pure rotation", "Fundamental band"}, &sp, &sn)) { L.rotSpec = sp; L.rotLine = -1; }
        if (L.rotIso != rot::kH35Cl) pCaption(p, L.rotIso == rot::kNatural ? "Natural mix: H³⁵Cl and H³⁷Cl, weighted by chlorine's abundances (NIST: 75.76 % and 24.24 %); the levels and the turning are H³⁵Cl's."
                                                                          : "This isotope's constants are scaled from H³⁵Cl's measured ones by the Born–Oppenheimer mass rules, not measured.", true);
    }
    {
        const float half = (p.w - static_cast<float>(m.s8)) / 2;
        const ImVec2 o = here(p);
        if (button("molecule.rot.turn", o, ImVec2(half, static_cast<float>(m.control)), L.rotTurning ? "Stop turning" : "Turn", Face::Secondary, L.rotTurning)) L.setTurning(!L.rotTurning);
        noteItem("molecule.rot.turn");
        if (button("molecule.rot.card", ImVec2(o.x + half + m.s8, o.y), ImVec2(half, static_cast<float>(m.control)), "Rotation card", Face::Secondary, L.rotCard)) L.rotCard = !L.rotCard;
        noteItem("molecule.rot.card");
        p.y += m.control + m.s4;
    }
    if (L.rotTurning && L.set.style == molecule::Style::Realistic) pCaption(p, "The turning is shown in Ball-and-stick and Space-filling.");
    if (J == 0) pCaption(p, std::string(q) + " = 0 has no angular momentum: the molecule does not turn.");
    else {
        const double w = rot::omegaRadS(r, J);
        std::snprintf(b, sizeof b, "%.2f ps real, %.1f s on screen", 2 * 3.14159265358979323846 / w * 1e12, rot::screenTurnS(r, J));
        pStat(p, "One turn", b);
    }
    char sd[64]; std::snprintf(sd, sizeof sd, "%.0e", rot::kSlowDown);
    pCaption(p, std::string("A quantum rotor in a level of fixed ") + q + " has no turning picture (it points every way at once); the picture turns at the classical rate of that energy, ω = √(" + q + "(" + q + "+1)) ħ / I, "
                "slowed down 5 × 10¹² times.", true);
    if (ui::look().reduced && L.rotTurning) pCaption(p, "Motion is Reduced (Settings): the turning is held.");
    std::string about;
    if (r.D[0] > 0 && mo.id == "HCl")
        about = std::string("Levels from the measured Be, αe, γe, De and D's change with v (Huber & Herzberg 1979, NIST WebBook, footnotes 29 and 30): F = B_v ") + q + "(" + q + "+1) − D_v[" + q + "(" + q + "+1)]², "
                "B_v and D_v as series in v + ½." + (r.scaled ? " This isotope's constants are all scaled from H³⁵Cl by the Born–Oppenheimer mass rules (none measured here)." : "");
    else if (r.D[0] > 0) about = std::string("Levels from the measured Be, αe and D (Huber & Herzberg 1979, NIST WebBook): F = B") + q + "(" + q + "+1) − D[" + q + "(" + q + "+1)]².";
    else about = "Levels of a rigid rotor from the measured ground-state B (NIST CCCBDB): no centrifugal term.";
    if (r.gEven != r.gOdd) {
        char w[320];
        // 0.97.0 (outside review, part 5): was "(the Pauli principle)". Spin-0 nuclei are bosons: the exchange rule (Herzberg 1950, ch. III)
        if (r.gEven == 0 || r.gOdd == 0) std::snprintf(w, sizeof w, " Only %s %s exist: the two identical nuclei have spin 0, so they are bosons, and the whole state must stay the same when they are swapped. That removes every other level.", r.gEven == 0 ? "odd" : "even", q);
        else std::snprintf(w, sizeof w, " Nuclear spin weights: even %s ×%d, odd ×%d (ortho and para).", q, r.gEven, r.gOdd);
        about += w;
    }
    if (r.nLevels) about += " O₂'s ground state is a triplet: each N level is split into three by about 2 cm⁻¹ (not shown).";
    if (!r.dipole) about += mo.id == "CO2" || mo.id == "C2H2" ? " No permanent dipole, so no pure-rotation lines (its infrared bands, from bending and stretching, are not shown here yet)." : " No permanent dipole, so no pure-rotation lines.";
    pCaption(p, about, true);
}

// The Rotation card: the level ladder with a bar for each level's share, and the stick spectrum (hover or tap a line for its name, wavenumber and levels)
void moleculeRotationCard(AppState& app, Panel& p) {
    namespace rot = molecule::rot;
    molecule::MoleculeLab& L = app.molecule;
    const molecule::Molecule& mo = L.mol();
    const Metrics& m = F().m;
    const rot::Rotor r = L.rotor();
    const char* q = qName(r);
    char b[240];
    std::snprintf(b, sizeof b, "%s at %.0f K", r.label.c_str(), L.liveK);
    pCaption(p, b, true);
    if (rot::hasSpinForms(r)) pCaption(p, spinMixText(r), true);   // 0.97.0: the ortho and para case shown
    // the ladder: the levels to scale on the left, one row per level on the right (its share as a bar), joined by a faint line
    const int j0 = rot::lowestJ(r), best = rot::mostPopulated(r, L.liveK), shown = L.rotLevel();
    int top = std::max({best * 2 + 2, shown + 1, j0 + 5});
    while (top > j0 + 5 && rot::share(r, top, L.liveK) < 1e-3 && top > shown + 1) top--;
    top = std::min(top, j0 + 15);
    std::vector<int> js;
    for (int j = j0; j <= top; j++) js.push_back(j);
    double smax = 0; for (int j : js) smax = std::max(smax, rot::share(r, j, L.liveK));
    const ImVec2 o = here(p);
    const float rowH = lineH(Type::Caption) + 2, H = rowH * js.size(), lw = p.w * 0.22f, rx = o.x + lw + m.s12, labW = width(Type::Caption, false, std::string(q) + " 15 ×6") + m.s4,
                barX = rx + labW, barW = p.w - (barX - o.x) - width(Type::Caption, false, "100.0 %") - m.s8;
    const double fTop = rot::term(r, 0, top);
    for (std::size_t i = 0; i < js.size(); i++) {
        const int j = js[i];
        const bool allowed = rot::weight(r, j) > 0, sel = j == shown;
        const float yl = o.y + H - static_cast<float>(rot::term(r, 0, j) / (fTop > 0 ? fTop : 1)) * (H - 2) - 1;   // to scale
        const float yr = o.y + H - (i + 0.5f) * rowH;                                                               // the row
        const ImU32 col = sel ? cAccent() : allowed ? cText(0.85f) : cMuted(0.5f);
        p.dl->AddLine(ImVec2(o.x, yl), ImVec2(o.x + lw, yl), col, sel ? 2.5f : 1.2f);
        p.dl->AddLine(ImVec2(o.x + lw, yl), ImVec2(rx - 2, yr), sel ? cAccent(0.7f) : cMuted(0.45f), 1.0f);
        const bool showW = r.gEven > 0 && r.gOdd > 0 && r.gEven != r.gOdd;      // (H2, N2, C2H2: the nuclear-spin weight beside the level)
        text(p.dl, Type::Caption, sel, ImVec2(rx, yr - lineH(Type::Caption) / 2), col, std::string(q) + " " + std::to_string(j) + (showW ? " ×" + std::to_string(rot::weight(r, j)) : ""));
        const double s = rot::share(r, j, L.liveK);
        if (allowed) {
            const float w = smax > 0 ? static_cast<float>(s / smax) * barW : 0;
            p.dl->AddRectFilled(ImVec2(barX, yr - rowH * 0.32f), ImVec2(barX + std::max(1.0f, w), yr + rowH * 0.32f), sel ? cAccent(0.9f) : cText(0.45f));
            text(p.dl, Type::Caption, false, ImVec2(barX + std::max(1.0f, w) + m.s4, yr - lineH(Type::Caption) / 2), cMuted(), pct(s));
        } else text(p.dl, Type::Caption, false, ImVec2(barX, yr - lineH(Type::Caption) / 2), cMuted(0.7f), "none (forbidden)");
    }
    p.y += H + m.s4;
    std::snprintf(b, sizeof b, "Levels to scale (left); each one's share at %.0f K (right).", L.liveK);
    pCaption(p, b, true);
    // the spectrum
    pGap(p, m.s8);
    if (!hasLines(r)) {
        pBody(p, mo.id == "CO2" || mo.id == "C2H2" ? "No pure-rotation lines: the molecule has no permanent dipole (its infrared bands come from bending and stretching, not shown here yet)."
                                                   : "No pure-rotation lines: the molecule has no permanent dipole, so turning gives off and absorbs no light.", true);
        if (shellLogOn()) auditNote("rotcard lines=0 hover=-");
        return;
    }
    const rot::Spectrum sp = specOf(L, r);
    const int isoSel = mo.id == "HCl" ? L.rotIso : 0;
    {   // 0.96.0 (part D): the lines' heights, the transition's strength or the absorbance (rotation.h Measure)
        int ms = L.rotMeasure;
        const std::vector<std::string> mn{"molecule.rot.measure.strength", "molecule.rot.measure.absorbance"};
        if (pSegmented(p, "molecule.rot.measure", {"Line strength", "Absorbance"}, &ms, &mn)) L.rotMeasure = ms;
    }
    const rot::Measure meas = L.rotMeasure == 1 ? rot::Measure::Absorbance : rot::Measure::Strength;
    const std::vector<rot::Line> lines = rot::lines(mo.id, isoSel, sp, L.liveK, 2e-3, meas, L.spinMix());
    pHeading(p, sp == rot::Spectrum::Fundamental ? "Fundamental band v 0 → 1 (cm⁻¹)" : "Pure rotation, J → J+1 (cm⁻¹)");
    if (lines.empty()) { pCaption(p, "No line is strong enough to draw at this temperature."); return; }
    double lo = lines.front().nu, hi = lines.back().nu;
    const double pad = std::max(1.0, (hi - lo) * 0.04);
    lo -= pad; hi += pad;
    const ImVec2 so = here(p);
    const float W = p.w, Hh = static_cast<float>(5 * m.row / 2), base = so.y + Hh;
    const auto X = [&](double nu) { return so.x + static_cast<float>((nu - lo) / (hi - lo)) * W; };
    p.dl->AddLine(ImVec2(so.x, base), ImVec2(so.x + W, base), cMuted(0.7f), 1.0f);
    {   // ticks: 1, 2 or 5 times a power of ten, about five of them
        const double raw = (hi - lo) / 5, mag = std::pow(10.0, std::floor(std::log10(raw)));
        const double step = raw / mag < 1.5 ? mag : raw / mag < 3.5 ? 2 * mag : raw / mag < 7.5 ? 5 * mag : 10 * mag;
        for (double t = std::ceil(lo / step) * step; t <= hi; t += step) {
            const float x = X(t);
            p.dl->AddLine(ImVec2(x, base), ImVec2(x, base + 3), cMuted(), 1.0f);
            char tb[32]; std::snprintf(tb, sizeof tb, "%.0f", t);
            const float tw = width(Type::Caption, false, tb);
            if (x - tw / 2 >= so.x && x + tw / 2 <= so.x + W) text(p.dl, Type::Caption, false, ImVec2(x - tw / 2, base + 4), cMuted(), tb);
        }
    }
    if (sp == rot::Spectrum::Fundamental) {   // the band origin: no line there
        const float x0 = X(r.nu0);
        for (float y = so.y + 4; y < base; y += 5) p.dl->AddLine(ImVec2(x0, y), ImVec2(x0, std::min(base, y + 2)), cMuted(0.6f), 1.0f);
    }
    // the line under the pointer (within 6 points), else the tapped one
    int hov = -1;
    const bool over = hoveredRect(so, ImVec2(W, Hh + lineH(Type::Caption)));
    if (over) {
        const float mx = ImGui::GetIO().MousePos.x;
        float bd = 6;
        for (std::size_t i = 0; i < lines.size(); i++) { const float d = std::fabs(X(lines[i].nu) - mx); if (d < bd) { bd = d; hov = static_cast<int>(i); } }
    }
    if (L.rotLine >= static_cast<int>(lines.size())) L.rotLine = -1;
    const int act = hov >= 0 ? hov : L.rotLine;
    for (std::size_t i = 0; i < lines.size(); i++) {
        const rot::Line& l = lines[i];
        const float x = X(l.nu), h = std::max(2.0f, static_cast<float>(l.strength) * (Hh - 4));
        const bool a = static_cast<int>(i) == act;
        const ImU32 col = a ? cAccent() : l.iso == rot::kH37Cl ? cMuted(0.85f) : cText(0.85f);
        p.dl->AddLine(ImVec2(x, base), ImVec2(x, base - h), col, a ? 3.0f : 1.6f);
    }
    ImGui::SetCursorScreenPos(so);
    if (ImGui::InvisibleButton("molecule.rot.plot", ImVec2(W, Hh + lineH(Type::Caption)))) L.rotLine = hov;   // a tap picks the line under it (or none)
    noteItem("molecule.rot.plot");
    p.y += Hh + lineH(Type::Caption) + m.s4;
    if (act >= 0) {
        const rot::Line& l = lines[std::size_t(act)];
        if (l.branch == 'J') std::snprintf(b, sizeof b, "%s  ·  %.3f cm⁻¹  ·  from J = %d to J = %d in v = 0", l.name().c_str(), l.nu, l.jLo, l.jHi);
        else std::snprintf(b, sizeof b, "%s  ·  %.3f cm⁻¹  ·  from v = 0, J = %d to v = 1, J = %d", l.name().c_str(), l.nu, l.jLo, l.jHi);
        std::string t = b;
        if (mo.id == "HCl") t += std::string("  ·  ") + rot::isoName(l.iso);
        std::snprintf(b, sizeof b, meas == rot::Measure::Absorbance ? "  ·  absorbance %.2f" : "  ·  strength %.2f", l.strength);
        t += b;
        if (mo.id == "HCl" && l.iso == rot::kH35Cl && sp == rot::Spectrum::Fundamental) {
            const double meas = rot::measuredHClLine(l.branch, l.J);
            if (std::isfinite(meas)) { std::snprintf(b, sizeof b, "  ·  measured %.4f (Rank et al. 1962)", meas); t += b; }
        }
        pBody(p, t);
    } else pCaption(p, "Point at a line (or tap it) for its name, wavenumber and levels.");
    if (shellLogOn()) {   // (for the automatic checks: the strongest line's place on screen, so a check can point at it)
        std::size_t st = 0; for (std::size_t i = 0; i < lines.size(); i++) if (lines[i].strength > lines[st].strength) st = i;
        const auto nm = [](std::string s) { std::replace(s.begin(), s.end(), ' ', '_'); return s; };   // (0.96.0: "J 3 → 4" as one word in the note)
        char a[220]; std::snprintf(a, sizeof a, "rotcard lines=%zu hover=%s strong=%s x=%.1f y=%.1f meas=%d", lines.size(), act >= 0 ? nm(lines[std::size_t(act)].name()).c_str() : "-", nm(lines[st].name()).c_str(), X(lines[st].nu), base - 6, L.rotMeasure);
        auditNote(a);
    }
    std::string cap;
    if (sp == rot::Spectrum::Fundamental) cap = "R branch (J → J+1) right of the band origin (dashed), P branch (J → J−1) left; no line at the origin: J must change by one. ";
    else cap = "Each line is a jump J → J+1; the lines are 2B apart. ";
    if (mo.id == "HCl" && L.rotIso == rot::kNatural) cap += "Darker: H³⁵Cl; lighter: H³⁷Cl. ";
    if (meas == rot::Measure::Absorbance)   // 0.96.0 (part D)
        cap += "Heights: the absorbance, the line's area in a measured spectrum: the lower level's share × the Hönl–London factor × ν(1 − e^(−hcν/kT)) "
               "(HITRAN's line intensity; the last factor takes off the light the upper level gives back). Sticks: no line widths, no Herman–Wallis change of the "
               "strengths along the band, no hot bands (v = 1 → 2). Limits: Rotation guide.";
    else cap += "Heights: the lower level's share × the Hönl–London factor (the transition's own strength; Absorbance also weighs each line by ν(1 − e^(−hcν/kT))). Limits: Rotation guide.";
    pCaption(p, cap, true);
}

void moleculeControls(AppState& app, Panel& p, const SectionFn& section) {
    molecule::MoleculeLab& L = app.molecule;
    const molecule::Molecule& mo = L.mol();
    if (section("molecules", "Molecule", true)) {
        for (int i = 0; i < molecule::count(); i++) {
            const molecule::Molecule& c = molecule::get(i);
            const std::string id = "molecule.pick." + c.id;
            if (pRow(p, id.c_str(), molecule::prettyFormula(c.formula) + "  ·  " + c.name, i == L.set.mol)) L.choose(i);
        }
    }
    if (section("style", "Style", true)) {
        int cur = static_cast<int>(L.set.style);
        const std::vector<std::string> sn{"molecule.style.ball", "molecule.style.fill", "molecule.style.real"};
        real::markLabels("molecule", "style", {"Ball-and-stick", "Space-filling", "Density"});
        if (pSegmented(p, "molecule.style", {"Ball-and-stick", "Space-filling", "Density"}, &cur, &sn)) L.setStyle(static_cast<molecule::Style>(cur));
        bool lab = L.set.labels;
        if (pCheck(p, "molecule.labels", "Element labels", &lab)) L.set.labels = lab;
        const bool reduced = ui::look().reduced;
        bool sp = L.set.spin && !reduced;
        if (pCheck(p, "molecule.spin", "Auto-spin", &sp) && !reduced) L.set.spin = sp;
        if (reduced) pCaption(p, "Auto-spin is off while Motion is Reduced (Settings).");
        // (0.22 stage 6: no Reset view here: it is on the bar under the picture, and key R)
        // 0.22: the shape is the equilibrium one, standing still. 0.46.6 (outside review: every molecule quoted water's O-H stretch): the example is the shown molecule's own
        // fastest measured vibration (NIST CCCBDB, vib_measured.inc), period 1 / (c nu): for water's 3756 cm^-1, 8.9 fs
        namespace vb = molecule::vib;
        double fastest = 0;
        for (int k = 0; k < static_cast<int>(vb::modes(mo.id).size()); k++) if (const vb::Measured* e = vb::measured(mo.id, k)) fastest = std::max(fastest, e->fund);
        char fv[160] = "";
        if (fastest > 0) std::snprintf(fv, sizeof fv, " (its fastest vibration, %.0f cm⁻¹ measured, swings once every %.0f femtoseconds)", fastest, 1e15 / (2.99792458e10 * fastest));
        pCaption(p, std::string(L.vibOn() ? "A vibration is shown (Vibrations, below); without one the molecule stands" : "Shown standing") + " still at its measured shape (NIST CCCBDB; " + shapeKindText(mo) + "). A real molecule never stops: it vibrates all the time" + fv + ", turns, and "
                    "even at absolute zero its atoms are smeared out a little by zero-point motion, the light hydrogens most. Colours and ball sizes are drawing conventions: atoms have no colour.");
        pCaption(p, std::string("Drag to turn the molecule, the wheel zooms; Shift, right or middle drag moves it. ") + (L.realOn() ? "Click an atom for its measured values (this style draws no bonds)." : "Click an atom or a bond for its measured values.") + " Keys: + − zoom, R reset view, 1 2 3 4 switch lab.");   // (0.22 stage 7: Density has no bonds to click)
    }
    vibrationControls(L, p, section);                     // 0.23
    rotationControls(L, p, section);                      // 0.53
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
        pStat(p, "Formula", molecule::prettyFormula(mo.formula) + " (Hill order: " + molecule::prettyFormula(mo.hill) + ")");
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
        pCaption(p, "The measured constants and the ones from the shape need not agree exactly: H2, N2 and HCl list the equilibrium value (Be), O2 the lowest vibrational level's (B0: NIST WebBook, Huber and Herzberg); "
                    "CCCBDB does not say which for the larger molecules, and a vibrating molecule's constants differ from its still shape's, here by up to a few per cent. This shape: " + shapeKindText(mo) + ".", false);   // 0.48 (review B)
        if (mo.id == "NH3") pCaption(p, "Ammonia's constants are from the JPL Molecular Spectroscopy catalogue (entry 17002, ground state): CCCBDB lists A = B = 9.4443, a slip for about 9.94 that its own geometry does not fit (9.97).", false);
        pCaption(p, "Shape: " + mo.shapeNote, false);
        if (mo.derivedAngle) pCaption(p, "H-C-O angle derived: CCCBDB lists no H-C-O angle, so it is worked out from H-C-H under the stated assumption.", false);
        pCaption(p, std::string("Source: NIST CCCBDB (Computational Chemistry Comparison and Benchmark Database), SRD 101, Release 22, experimental geometry and rotational constants (") + mo.rotRef +
                    " for the constants); masses: NIST Atomic Weights and Isotopic Compositions.");
    }
    if (section("honest", "What the picture does not show", true)) {
        pCaption(p, "The shape is fixed. CCCBDB marks it as an equilibrium (\"re\") structure for 6 of the 15 molecules (H2, N2, O2, HCl, water, methane) and acetylene's as an \"rm\" structure; for the other 8 it gives no type. Real molecules vibrate and rotate all the time.", false);
        pCaption(p, "Ball sizes in Ball-and-stick are only a drawing scale. Space-filling balls use van der Waals radii (PubChem): a convention, because a molecule has no hard edge; its electrons thin out gradually.", false);
        pCaption(p, "Colours are the CPK convention (PubChem). Double, triple and 1.5 bonds come from the usual structural formula, also a convention; a bond of order 1.5, shared over several atoms (benzene’s ring, ozone’s two equal bonds), is drawn as one solid and one dashed stick. The view has no perspective.", false);
        pCaption(p, "Density shows a CALCULATED electron density (quantum chemistry), not a measurement and not a photo; see its own section above.", false);
    }
}

}  // namespace ui::shellimpl
