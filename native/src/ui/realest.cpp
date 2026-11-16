#include "ui/realest.h"

#include <cstring>
#include <vector>

#include "atom/atom_tools.h"
#include "molecule/molecule.h"
#include "molecule/molecule_view.h"
#include "orbit/real_errors.h"
#include "ui/orbit_panel.h"
#include "ui/shell_widgets.h"

namespace ui::real {

namespace {

std::string commas(long v) { std::string s = std::to_string(v); for (int i = static_cast<int>(s.size()) - 3; i > 0; i -= 3) s.insert(static_cast<std::size_t>(i), ","); return s; }

struct Row { const char* lab; Pick p; };
const std::vector<Row>& table() {
    static const std::vector<Row> t = {
        // ---- Orbit Lab ----
        {"orbit", {"view", "3D", "space has three dimensions; 2D shows the Solar System from above, flattened."}},
        {"orbit", {"sizes", "True scale", "the Sun and the planets as big as they really are, compared with their distances."}},
        {"orbit", {"physics", "Real", "the Sun and the planets pull on each other (Newton's gravity), started from NASA/JPL Horizons' positions. Simple moves each planet alone on a fixed orbit."}},
        // 0.29: on is the realistic choice (with the accurate integrator it brings every inner planet closer to JPL: orbit/real_errors.h, tests/test_jpl.cpp)
        {"orbit", {"gr", "on", "Einstein's correction to the Sun's gravity is real: with it, Mercury stays within " + commas(orbit::kRealErrKmRelOn[0]) + " km of NASA/JPL Horizons over "
                                "1800 to 2100 (" + commas(orbit::kRealErrKmRelOff[0]) + " km without it)."}},
        {"orbit", {"wobble", "off", "the Sun's path round the centre of mass at its real size (less than 2.3 Sun radii)."}},
        {"orbit", {"moons", "on", "they are there in the real Solar System."}},
        {"orbit", {"dwarfs", "on", "they are there in the real Solar System."}},
        {"orbit", {"comets", "on", "they are there in the real Solar System."}},
        {"orbit", {"belts", "on", "they are there in the real Solar System."}},
        {"orbit", {"far", "on", "the nearest stars, the Voyager probes and the centre of the Galaxy are really there."}},
        {"orbit", {"sky", "Full", "real stars do not get brighter as you zoom out; Soft dims them close up."}},
        {"orbit", {"cons", "Off", "constellation lines and names are drawn by people; they are not in the sky."}},
        // ---- Atom Lab (the keys and values of ui/atom_panel.cpp atomChoices; "picture" is the View selector) ----
        {"atom", {"picture", "Cloud 3D", "the electron is a cloud of probability in three dimensions. The Orbit picture is Bohr's 1913 model (electrons on circles), which quantum mechanics replaced."}},
        {"atom", {"ostyle", "Modern", "a real nucleus, with quarks when you zoom in; Bohr 1913 is his simple drawing."}},
        {"atom", {"scale", "Fixed", "every atom at the same scale, so their sizes compare truly; Auto stretches each one to fill the picture."}},
        {"atom", {"qstyle", "Accurate", "a quark has no definite place inside a proton: Accurate draws where its quarks and gluons carry the momentum, fast ones near the centre, from measured data; Simple's balls and tubes are a model."}},   // 0.44
        {"atom", {"detail", "+ Sea", "a real proton or neutron holds short-lived quark-antiquark pairs as well as its three valence quarks and the gluons."}},
        {"atom", {"style", "Density", "the measured charge density of the nucleus (what electron scattering sees); Nucleons is a model."}},
        {"atom", {"cstyle", "Glow", "the soft dots add up into a smooth density, as the probability is; Classic shows separate hard dots."}},
        {"atom", {"cdens", "Ultra", "the most dots show the same probability most smoothly (the physics is the same)."}},
        {"atom", {"phase", "Wheel", "the wave has a phase, not only a sign; the wheel shows every phase, Sign shows only + and −."}},
        {"atom", {"dirac", "On", "real spectral lines are split by relativity (fine structure); the Dirac levels show it."}},
        {"atom", {"elec", "All", "a real atom has all its electrons (the neutral atom, Dirac-Hartree-Fock); Hydrogen-like removes every electron but one."}},   // 0.40: was missing, so Most realistic left Electrons on One
        {"atom", {"rscale", "True scale", "every electron at its real distance from the nucleus; Shells enlarged spreads the inner shells out so they can be seen."}},   // 0.46 (review A1): was missing, so Most realistic left the shells enlarged
        {"atom", {"hrel", "Dirac", "a real one-electron ion obeys Dirac's equation: its cloud is the Dirac state (n, l, j) at every Z, not the non-relativistic orbital."}},   // 0.49.3
        {"atom", {"vdots", "True share", "each electron gets its true share of the dots, so the outer electron is 1 in Z of them; Easier to see draws it denser so it and its jumps stand out."}},   // 0.54.1
        {"atom", {"mrel", "Relativistic", "real electrons obey Dirac's equation: heavy atoms have smaller s and p shells and larger d and f than the non-relativistic picture."}},   // 0.41
        // ---- Molecule Lab ----
        {"molecule", {"style", "Density", "the electron density calculated from quantum mechanics (B3LYP); Ball-and-stick and Space-filling are models."}},
        {"molecule", {"dots", "100,000", "the most dots show the same density most smoothly (the physics is the same)."}},
        {"molecule", {"iso", "0.0016", "it matched measured molecular surfaces best in Alibakhshi and Schäfer’s 2024 study of 104 molecules, named under these buttons (1.6 % error)."}},
        {"molecule", {"vibsize", "Real size", "the swing as big as it really is in the lowest vibrational level."}},
        {"molecule", {"anharm", "on", "a real bond is not a perfect spring: its rungs close up as it stretches, and at the top it breaks."}},   // 0.53.1
        // ---- Black Hole Lab (0.55.0) ----
        {"blackhole", {"bend", "On", "gravity really bends light: this is what a camera hovering there would see (calculated exactly for a hole that does not spin)."}},
        {"blackhole", {"look", "Real", "the disc as the standard thin-disc model gives it, with the colour and brightness shifts of the gas's speed and gravity; Film copies the film's stated choices, made for the story, not for accuracy."}},   // 0.55.3
        {"blackhole", {"sky", "Our sky", "a real black hole has a sky of stars behind it; ours, seen from Earth, is the only whole sky we have a map of."}},
    };
    return t;
}

}  // namespace

const Pick* find(const char* lab, const std::string& key) {
    for (const Row& r : table()) if (std::strcmp(r.lab, lab) == 0 && key == r.p.key) return &r.p;
    return nullptr;
}
void mark(const char* lab, const std::string& key, const std::string& value, int part) {
    const Pick* p = find(lab, key);
    if (p && value == p->value) sh::markReal(part, p->why);
}

void markLabels(const char* lab, const std::string& key, const std::initializer_list<const char*>& labels) {
    int i = 0;
    for (const char* l : labels) mark(lab, key, l, i++);
}
void markCheck(const char* lab, const std::string& key) { mark(lab, key, "on", 1); mark(lab, key, "off", 0); }

void applyOrbit(AppState& app) {
    orbit::ViewSettings& s = app.set;
    s.view3D = true; s.trueScale = true; s.realPlanets = true; s.relativity = true; s.exaggerateWobble = false;
    s.moons = s.dwarfs = s.comets = s.belts = s.far = true;
    s.sky = orbit::SkyMode::Full; s.cons = orbit::ConsMode::Off;
}
void applyAtom(AppState& app) {
    atom::AtomLab& L = app.atom;
    atom::pressTool(L, atom::Tool::Cloud3D);
    for (const char* k : {"ostyle", "scale", "qstyle", "detail", "style", "cstyle", "cdens", "phase", "dirac", "elec", "rscale", "mrel", "hrel", "vdots"}) L.setParam(k, find("atom", k)->value);
}
void applyMolecule(AppState& app) {
    molecule::MoleculeLab& L = app.molecule;
    L.setStyle(molecule::Style::Realistic);
    L.set.realDots = molecule::kRealDots[3];
    L.set.realIso = 1;                         // kRealIso[1] = 0.0016
    L.vibSize = 0;
    L.vibAnharm = true;                        // 0.53.1 (shown in Ball-and-stick and Space-filling)
}
void applyBlackHole(AppState& app) {
    app.blackhole.bend = true;
    app.blackhole.skyOurs = true;
    app.blackhole.look = blackhole::Look::Real;                          // 0.55.3
}
void applyLab(AppState& app) {
    if (app.lab == 0) applyOrbit(app);
    else if (app.lab == molecule::kLabIndex) applyMolecule(app);
    else if (app.lab == blackhole::kLabIndex) applyBlackHole(app);
    else applyAtom(app);
}
const char* buttonCaption() { return "Sets every choice closest to reality at once. A dot marks that choice in each setting (a ring by a box: best left unticked); point at it to see why."; }

}  // namespace ui::real
