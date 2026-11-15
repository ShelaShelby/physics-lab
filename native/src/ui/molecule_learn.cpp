#include "molecule_learn.h"

#include <cstdlib>

#include "imgui.h"
#include "molecule/morse.h"
#include "molecule/density.h"
#include "molecule/molecule.h"
#include "molecule/molecule_view.h"
#include "molecule/vibration.h"
#include "html_view.h"
#include "orbit_panel.h"
#include "rich_text.h"
#include "style_draw.h"
#include "ui/reading.h"

namespace ui {

namespace {

std::string p(const std::string& s) { return "<p class=\"lrnP\">" + s + "</p>"; }
std::string muted(const std::string& s) { return "<p class=\"lrnP lrnMuted\">" + s + "</p>"; }

}  // namespace

std::vector<MolLearnSection> moleculeLookSections(const molecule::Molecule& m, molecule::Style style, bool densityVibrates, int vibModes) {
    using molecule::Style;
    std::vector<MolLearnSection> v;
    const std::string nm = "<b>" + m.name + "</b> (" + molecule::prettyFormula(m.formula) + ")";
    const int n = static_cast<int>(m.atoms.size());

    v.push_back({"big", "🔎 The big picture",
        p("On screen is one molecule of " + nm + ": " + std::to_string(n) + (n == 1 ? " atom" : " atoms") + " held together by the electrons they share. "
          "Every atom sits where measurements put it: the bond lengths and angles are the measured ones, from NIST's Computational Chemistry Comparison and Benchmark "
          "Database (CCCBDB), not guessed.") +
        p("The molecule stands still in its measured shape unless you play a vibration. A real one never stands still: even at absolute zero its atoms jiggle a little, "
          "and in a gas it also tumbles and flies about.")});

    std::string see;
    if (style == Style::BallStick)
        see = p("<b>Ball-and-stick.</b> Each ball is an atom, each stick a bond (a double or triple bond is drawn as two or three sticks; benzene's ring bonds, shared evenly round the ring, as one). The distances between the balls' "
                "centres are real; the balls themselves are drawn small (a fifth of the atom's van der Waals radius) so the bonds can be seen. "
                "An atom has no hard surface, so no ball size would be \"the real one\".");
    else if (style == Style::SpaceFill)
        see = p("<b>Space-filling.</b> Each atom is a sphere as big as its van der Waals radius (PubChem): roughly how close another molecule can come before they push "
                "each other away. It shows the space the molecule takes up. The spheres' edges are a convention: a real molecule has no hard edge; its electron cloud "
                "just gets thinner and thinner.");
    else
        see = p("<b>Density.</b> This is the molecule's electron cloud, calculated with quantum mechanics (density functional theory, B3LYP / aug-cc-pVTZ, with the "
                "PySCF program) for the measured shape. It is calculated, not measured, but it is the closest picture of a real molecule in the lab.") +
              p("<b>The dots</b> are drawn at random from that density, so where they crowd, the electrons are most likely to be found. Each dot is a place an electron "
                "may be, not an electron. <b>The surface</b> is where the density drops to the cut-off you choose (0.002 electrons per cubic bohr is the usual "
                "convention): a way to draw an \"outside\" for something that has none. <b>The small balls</b> inside mark the nuclei.");
    v.push_back({"see", "👀 What is on the picture?", see + muted("Change the style in Controls › Style, and this page changes with it.")});

    std::string col = p("<b>Atom colours</b> are the usual chemists' convention (CPK colours, as PubChem gives them): hydrogen white, carbon grey, nitrogen blue, "
                        "oxygen red, chlorine green. Atoms have no colour of their own: they are far smaller than the wavelength of light.");
    if (style == Style::Realistic)
        col += p("<b>The surface colours</b> (with Surface colour: Charge (potential)) show the electric potential the molecule makes at its surface: <b>red</b> where it is negative "
                 "(electrons crowd there, so a positive charge nearby would be pulled in), <b>blue</b> where it is positive (the nuclei show through), white in between. "
                 "This is why water's oxygen side is red and its hydrogens blue: it is what makes water a polar molecule.");
    v.push_back({"col", "🎨 What do the colours mean?", col});

    std::string vib;
    if (vibModes > 0) {
        vib = p(nm + " has " + std::to_string(vibModes) + (vibModes == 1 ? " way" : " ways") + " to vibrate (its normal modes: " +
                (m.linear || n == 2 ? "3 × atoms − 5 for a straight molecule" : "3 × atoms − 6") + "). Pick one in Controls › Vibrations to watch it, one at a time, or <b>All at once, warm</b> to see every mode moving together at a temperature you choose: "
                "the colder the molecule, the closer each mode is to its lowest level (it never stands still, even at 0 K); warmer, the slow modes swing more first.") +
              p("The swings are slowed down many trillions of times, but every mode keeps its true speed compared with the others. <b>Real size</b> is as big as the "
                "swing really is in the molecule's lowest energy level; <b>3 times bigger</b> makes it easier to see.") +
              p("<b>The wavenumbers</b> (cm⁻¹) say how fast each mode vibrates; they are the measured ones (NIST CCCBDB). Modes that change the molecule's electric "
                "balance absorb infrared light: the bars of the small spectrum show how strongly. That is how chemists recognise molecules, and why water and carbon "
                "dioxide warm the Earth.");
        if (style == Style::Realistic)
            vib += p(densityVibrates ? "In the Density style the electron cloud moves with the atoms: at each moment it is the calculated density for the atoms where they "
                                       "are then (the electrons follow the much heavier nuclei at once)."
                                     : "In the Density style only water's cloud vibrates so far; switch to Ball-and-stick or Space-filling to watch this molecule's atoms move.");
        if (molecule::morse::has(m.id))   // 0.53.1
            vib += p("<b>A real bond is not a perfect spring.</b> Tick <b>Anharmonic (real bond)</b> under the stretch: the swing then follows a Morse curve, steep when the atoms "
                     "push together and flattening out as they pull apart, so each level's swing is lopsided and its middle lies beyond the resting length. Step the level up: "
                     "the rungs close up, the swing grows and slows, and above the last bound level the atoms fly apart (Reset brings them back). The lowest levels are the "
                     "measured ones; the levels above are the model's, and the <b>Bond energy</b> card shows how far the model's dissociation energy is from the measured one.");
        vib += muted("A real molecule does not swing like a ball on a spring: in its lowest level its atoms are smeared out like a wave, and every mode is there at once.");
    } else
        vib = p("This molecule has no vibrations in the lab.");
    v.push_back({"vib", "〰️ Vibrations", vib});

    // 0.53: rotation (molecule/rotation.h)
    std::string rotx;
    if (m.linear)
        rotx = p("A molecule also turns. Its energy of turning comes in steps, the <b>levels</b> J = 0, 1, 2 ..., with F(J) = B J(J+1): the steps grow as the molecule spins faster. "
                 "<b>The ladder</b> in Controls › Rotation › Rotation card shows them to scale, and beside each one the <b>share</b> of the molecules in it at the lab's temperature: "
                 "the weight g, times 2J+1 (the ways the turning axis can point), times e^(−energy/kT). Warmer, the higher levels fill. ") +
               p("<b>The weights</b> come from the nuclei's spins. In H₂ the two protons' spins can point together (ortho, odd J, 3 ways) or opposite (para, even J, 1 way), "
                 "so hot hydrogen is 3 parts ortho to 1 para. In O₂ and CO₂ the oxygen nuclei have no spin, and the Pauli principle leaves only every other level: O₂ has no even N, CO₂ no odd J.") +
               p("<b>The two branches.</b> Light can change J by exactly one. A molecule with an electric dipole (HCl, HCN) absorbs far-infrared light as it climbs J → J+1: lines 2B apart. "
                 "When HCl also starts to vibrate (v 0 → 1, near 2886 cm⁻¹), J goes up (the <b>R branch</b>, to the right) or down (the <b>P branch</b>, to the left), and there is a gap in the middle "
                 "where J would not change. From the lines' spacing the Data panel works out HCl's bond length.") +
               muted("Limits: the lines are sticks (no widths), their heights are the transitions' own strengths (no Herman–Wallis correction, not the absorbance), there are no hot bands (v 1 → 2), "
                     "the constants go as far as Huber & Herzberg's γe and the change of D with v (the terms beyond, given by Rank, Rao et al. 1965, are left out: the far lines are off by up to "
                     "0.09 cm⁻¹ at R(15) against the measured ones), and the isotopes other than H³⁵Cl have no measured constants here: all of theirs, γe and D's change included, are scaled "
                     "from H³⁵Cl by the Born–Oppenheimer mass rules. A quantum rotor in one level has no turning picture: the picture turns at the classical rate of that energy.");
    else
        rotx = p("Rotation of non-linear molecules is not shown yet: they turn about three axes at different rates, and their levels need a harder model (symmetric and asymmetric tops).");
    v.push_back({"rot", "🔄 Rotation", rotx});

    v.push_back({"real", "📏 What is real, and what is not",
        p("<b>Real (measured):</b> where the atoms are (bond lengths and angles), the vibrations' wavenumbers, and, where listed, how strongly they absorb infrared light. "
          "<b>Calculated:</b> the electron cloud and its surface colours, and which way each atom moves in a vibration. <b>Drawing conventions:</b> the ball and stick "
          "sizes, the atom colours, the surface's cut-off, and the slowed-down motion.") +
        p("Where one choice of a setting is closer to reality than the others, a small <b>dot</b> marks it (point at it to see why), and <b>Most realistic</b>, at the top of Controls, "
          "picks them all at once.")});

    v.push_back({"move", "🧭 How do I move around?",
        p("<b>Drag</b> turns the molecule. <b>Right-drag</b> or <b>Shift-drag</b> moves it. The <b>mouse wheel</b> (or <b>+</b> and <b>−</b>) zooms. <b>R</b> puts the view back. "
          "<b>Click an atom</b> to see what it is and its measured bonds and angles; <b>click a bond</b> for its measured length. Pick another molecule in "
          "Controls › Molecule, or type its name in search.")});
    return v;
}

void moleculeLearnWindow(AppState& app) {
    if (app.molLearn != 1) return;
    molecule::MoleculeLab& L = app.molecule;
    const molecule::Molecule& m = L.mol();
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowSize(ImVec2(std::min(vp->WorkSize.x * 0.6f, 44 * ImGui::GetFontSize()), vp->WorkSize.y * 0.85f), ImGuiCond_Appearing);
    ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x + vp->WorkSize.x * 0.4f, vp->WorkPos.y + vp->WorkSize.y * 0.5f), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    bool open = true;
    if (pageBegin("What am I even looking at?###mollearn", &open)) {
        if (app.molLearnJump == "top") { ImGui::SetScrollY(0); app.molLearnJump.clear(); }
        if (const char* sc = std::getenv("PL_LEARN_SCROLL")) ImGui::SetScrollY(float(std::atof(sc)));   // for automatic checks
        ImGui::PushTextWrapPos(0);
        richText("<p class=\"lrnP\">One page, top to bottom. It is about what is on screen now: change the molecule or the style and the text changes with it.</p>");
        const auto secs = moleculeLookSections(m, L.set.style, molecule::dens::vibDensity(m.id, 0) != nullptr, static_cast<int>(molecule::vib::modes(m.id).size()));
        for (std::size_t i = 0; i < secs.size(); i++) {
            ImGui::Spacing();
            ImGui::Separator();
            richHeading(secs[i].title);
            htmlView(secs[i].html);
        }
        ImGui::PopTextWrapPos();
        if (ImGui::IsKeyPressed(ImGuiKey_Escape) && ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) open = false;
    }
    pageEnd();
    if (!open) app.molLearn = 0;
}

}  // namespace ui
