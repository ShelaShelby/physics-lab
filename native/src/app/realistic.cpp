#include "app/realistic.h"

#include <cstring>

#include "atom/atom_tools.h"
#include "orbit/real_errors.h"

namespace realistic {

namespace {

std::string commas(long v) { std::string s = std::to_string(v); for (int i = static_cast<int>(s.size()) - 3; i > 0; i -= 3) s.insert(static_cast<std::size_t>(i), ","); return s; }
const char* onOff(bool b) { return b ? "on" : "off"; }
bool readOnOff(const std::string& v, bool& out) { if (v != "on" && v != "off") return false; out = v == "on"; return true; }
bool pick2(const std::string& v, const char* yes, const char* no, bool& out) {   // a control of two named values
    if (v == yes) { out = true; return true; }
    if (v == no) { out = false; return true; }
    return false;
}

}  // namespace

const std::vector<Row>& table() {
    static const std::vector<Row> t = {
        // ---- Orbit Lab ----
        {"orbit", {"view", "3D", "space has three dimensions; 2D shows the Solar System from above, flattened."}},
        {"orbit", {"sizes", "True scale", "the Sun and the planets as big as they really are, compared with their distances."}},
        {"orbit", {"physics", "Real", "the Sun and the planets pull on each other (Newton's gravity), started from NASA/JPL Horizons' positions. Simple moves each planet alone on a fixed orbit."}},
        // 0.29: on is the realistic choice (with the accurate integrator it brings every inner planet closer to JPL: orbit/real_errors.h, tests/test_jpl.cpp)
        {"orbit", {"gr", "on", "Einstein's correction to the Sun's gravity is real: with it, Mercury stays within " + commas(orbit::kRealErrKmRelOn[0]) + " km of NASA/JPL Horizons over "
                                "1800 to 2100 (" + commas(orbit::kRealErrKmRelOff[0]) + " km without it)."}},
        // 0.85.0 (outside review, R1): the true dimming with distance (the owner's decision of 25 Nov 2026)
        {"orbit", {"light", "True sunlight", "the sunlight on each body falls with the square of its distance from the Sun; Even lights every body as if it were at Earth's distance."}},
        {"orbit", {"wobble", "off", "the Sun's path round the centre of mass at its real size (less than 2.3 Sun radii)."}},
        {"orbit", {"moons", "on", "they are there in the real Solar System."}},
        {"orbit", {"dwarfs", "on", "they are there in the real Solar System."}},
        {"orbit", {"comets", "on", "they are there in the real Solar System."}},
        {"orbit", {"belts", "on", "they are there in the real Solar System."}},
        {"orbit", {"far", "on", "the nearest stars, the star clusters, the Voyager probes and the centre of the Galaxy are really there."}},
        // 0.77.0 (outside review): was "real stars do not get brighter as you zoom out", but Full brightens too (orbit_view.cpp: 0.45 to 1 of the stars' level, Soft 0.16 to 0.76)
        // 0.86.0: with True sunlight (the realistic Lighting above) the stars are on the camera's exposure (orbit/star_light.h); Full and Soft are then the same
        {"orbit", {"sky", "Full", "with True sunlight each star is drawn by its true light on the camera's exposure, as the planets are. With Even lighting both choices squeeze the stars' brightness range and brighten as you zoom out, which real stars do not."}},
        {"orbit", {"cons", "Off", "constellation lines and names are drawn by people; they are not in the sky."}},
        // ---- Atom Lab (the keys and values of ui/atom_panel.cpp atomChoices; "picture" is the View selector). 0.86.0: the values from atom/atom_tools.h realisticValue ----
        {"atom", {"picture", atom::realisticValue("picture"), "the electron is a cloud of probability in three dimensions. The Orbit picture is Bohr's 1913 model (electrons on circles), which quantum mechanics replaced."}},
        {"atom", {"ostyle", atom::realisticValue("ostyle"), "a real nucleus, with quarks when you zoom in; Bohr 1913 is his simple drawing."}},
        {"atom", {"scale", atom::realisticValue("scale"), "every atom at the same scale, so their sizes compare truly; Auto stretches each one to fill the picture."}},
        {"atom", {"qstyle", atom::realisticValue("qstyle"), "a quark has no definite place inside a proton: Accurate draws where its quarks and gluons carry the momentum, fast ones near the centre, from measured data; Simple's balls and tubes are a model."}},   // 0.44
        {"atom", {"detail", atom::realisticValue("detail"), "a real proton or neutron holds short-lived quark-antiquark pairs as well as its three valence quarks and the gluons."}},
        {"atom", {"style", atom::realisticValue("style"), "the measured charge density of the nucleus (what electron scattering sees); Nucleons is a model."}},
        {"atom", {"cstyle", atom::realisticValue("cstyle"), "the soft dots add up into a smooth density, as the probability is; Classic shows separate hard dots."}},
        {"atom", {"cdens", atom::realisticValue("cdens"), "the most dots show the same probability most smoothly (the physics is the same)."}},
        {"atom", {"phase", atom::realisticValue("phase"), "the wave has a phase, not only a sign; the wheel shows every phase, Sign shows only + and −."}},
        {"atom", {"dirac", atom::realisticValue("dirac"), "real spectral lines are split by relativity (fine structure); the Dirac levels show it."}},
        {"atom", {"elec", atom::realisticValue("elec"), "a real atom has all its electrons (the neutral atom, Dirac-Hartree-Fock); Hydrogen-like removes every electron but one."}},   // 0.40: was missing, so Most realistic left Electrons on One
        {"atom", {"rscale", atom::realisticValue("rscale"), "every electron at its real distance from the nucleus; Shells enlarged spreads the inner shells out so they can be seen."}},   // 0.46 (review A1): was missing, so Most realistic left the shells enlarged
        {"atom", {"hrel", atom::realisticValue("hrel"), "a real one-electron ion obeys Dirac's equation: its cloud is the Dirac state (n, l, j) at every Z, not the non-relativistic orbital."}},   // 0.49.3
        {"atom", {"vdots", atom::realisticValue("vdots"), "each electron gets its true share of the dots, so the outer electron is 1 in Z of them; Easier to see draws it denser so it and its jumps stand out."}},   // 0.54.1
        {"atom", {"sdots", atom::realisticValue("sdots"), "the Slice gives each electron its true share of the dots; Outer electron alone shows one electron's cloud without the core."}},   // 0.91.0
        {"atom", {"mrel", atom::realisticValue("mrel"), "real electrons obey Dirac's equation: heavy atoms have smaller s and p shells and larger d and f than the non-relativistic picture."}},   // 0.41
        // ---- Molecule Lab ----
        {"molecule", {"style", "Density", "the electron density calculated from quantum mechanics (B3LYP); Ball-and-stick and Space-filling are models."}},
        {"molecule", {"dots", "100,000", "the most dots show the same density most smoothly (the physics is the same)."}},
        {"molecule", {"iso", "0.0016", "it matched measured molecular surfaces best in Alibakhshi and Schäfer’s 2024 study of 104 molecules, named under these buttons (1.6 % error)."}},
        {"molecule", {"vibsize", "Real size", "the swing as big as it really is in the lowest vibrational level."}},
        {"molecule", {"anharm", "on", "a real bond is not a perfect spring: its rungs close up as it stretches, and at the top it breaks."}},   // 0.53.1
        // 0.97.0 (outside review of 28 Nov 2026, second, part 1): gas cooled without a catalyst keeps its warm mix of the two spin forms for days (molecule/rotation.h)
        {"molecule", {"opara", "Kept as when warm", "without a catalyst the two spin forms turn into each other only slowly (in liquid hydrogen the extra ortho takes about 5 days to halve), so a gas cooled from warm keeps its warm mix. In balance needs a catalyst."}},
        // ---- Black Hole Lab (0.55.0) ----
        // 0.77.0 (outside review): was "calculated exactly for a hole that does not spin". Since 0.55.9 a spinning hole's rays are closed forms too (blackhole/spin_sky.h kSpinFineFill, kerr_closed.h)
        {"blackhole", {"bend", "On", "gravity really bends light: this is what a camera held there would see (beside a spinning hole, one carried round with the turning space). Every light path is "
                                     "calculated exactly from Einstein's equations, by closed formulas for a hole that spins or not, on a fine table of rays the picture is read from."}},
        // 0.77.0 (outside review): the moving gas's clump and patches (0.79.0: streaks) are models (their place, size, strength and start are chosen, not measured): best left off
        {"blackhole", {"gasclump", "off", "a model: its orbit, size, brightness and start are chosen, not measured. No telescope has seen the face of such a disc."}},
        {"blackhole", {"gaspattern", "off", "a model from computer simulations: where the streaks are and their strength are chosen, not measured."}},
        {"blackhole", {"look", "Real", "the disc as the standard thin-disc model gives it, with the colour and brightness shifts of the gas's speed and gravity; Film copies the film's stated choices, made for the story, not for accuracy."}},   // 0.55.3
        {"blackhole", {"sky", "Our sky", "a real black hole has a sky of stars behind it; ours, seen from Earth, is the only whole sky we have a map of."}},
        {"blackhole", {"skybeside", "True brightness", "the stars at their measured brightness on the disc's own scale: beside a bright disc they are far too faint to show, as they would be."}},   // 0.55.4
        {"blackhole", {"light", "What an eye sees", "the picture an eye would see, its visible light; All light shows the light of all wavelengths, mostly ultraviolet here, which no eye sees."}},   // 0.58.0
        {"blackhole", {"glare", "As an eye sees it", "a light this bright scatters inside the eye and veils its surroundings; drawn for an average eye by the CIE's glare equation."}},   // 0.55.4
    };
    return t;
}

const Pick* find(const char* lab, const std::string& key) {
    for (const Row& r : table()) if (std::strcmp(r.lab, lab) == 0 && key == r.p.key) return &r.p;
    return nullptr;
}

// ---- Orbit Lab (the labels of ui/shell_orbit.cpp's controls; "view" is the toolbar's 2D / 3D) ----
std::string get(const orbit::ViewSettings& v, const std::string& k) {
    if (k == "view") return v.view3D ? "3D" : "2D";
    if (k == "sizes") return v.trueScale ? "True scale" : "Easy to see";
    if (k == "physics") return v.realPlanets ? "Real" : "Simple";
    if (k == "gr") return onOff(v.relativity);
    if (k == "light") return v.trueLight ? "True sunlight" : "Even (not realistic)";
    if (k == "wobble") return onOff(v.exaggerateWobble);
    if (k == "moons") return onOff(v.moons);
    if (k == "dwarfs") return onOff(v.dwarfs);
    if (k == "comets") return onOff(v.comets);
    if (k == "belts") return onOff(v.belts);
    if (k == "far") return onOff(v.far);
    if (k == "sky") return v.sky == orbit::SkyMode::Full ? "Full" : v.sky == orbit::SkyMode::Soft ? "Soft" : "Off";
    if (k == "cons") return v.cons == orbit::ConsMode::Off ? "Off" : v.cons == orbit::ConsMode::Lines ? "Lines" : "Lines and names";
    return "";
}
bool set(orbit::ViewSettings& v, const std::string& k, const std::string& x) {
    if (k == "view") return pick2(x, "3D", "2D", v.view3D);
    if (k == "sizes") return pick2(x, "True scale", "Easy to see", v.trueScale);
    if (k == "physics") return pick2(x, "Real", "Simple", v.realPlanets);
    if (k == "gr") return readOnOff(x, v.relativity);
    if (k == "light") return pick2(x, "True sunlight", "Even (not realistic)", v.trueLight);
    if (k == "wobble") return readOnOff(x, v.exaggerateWobble);
    if (k == "moons") return readOnOff(x, v.moons);
    if (k == "dwarfs") return readOnOff(x, v.dwarfs);
    if (k == "comets") return readOnOff(x, v.comets);
    if (k == "belts") return readOnOff(x, v.belts);
    if (k == "far") return readOnOff(x, v.far);
    if (k == "sky") {
        if (x == "Soft") v.sky = orbit::SkyMode::Soft; else if (x == "Full") v.sky = orbit::SkyMode::Full; else if (x == "Off") v.sky = orbit::SkyMode::Off; else return false;
        return true;
    }
    if (k == "cons") {
        if (x == "Off") v.cons = orbit::ConsMode::Off; else if (x == "Lines") v.cons = orbit::ConsMode::Lines; else if (x == "Lines and names") v.cons = orbit::ConsMode::LinesAndNames; else return false;
        return true;
    }
    return false;
}

// ---- Atom Lab (the keys of ui/atom_panel.cpp atomChoices; "picture" is the view) ----
namespace {
std::string* atomField(atom::AtomParams& a, const std::string& k) {
    if (k == "picture") return &a.view;
    if (k == "ostyle") return &a.ostyle;
    if (k == "scale") return &a.scale;
    if (k == "qstyle") return &a.qstyle;
    if (k == "detail") return &a.detail;
    if (k == "style") return &a.style;
    if (k == "cstyle") return &a.cstyle;
    if (k == "cdens") return &a.cdens;
    if (k == "phase") return &a.phase;
    if (k == "dirac") return &a.dirac;
    if (k == "elec") return &a.elec;
    if (k == "rscale") return &a.rscale;
    if (k == "hrel") return &a.hrel;
    if (k == "vdots") return &a.vdots;
    if (k == "sdots") return &a.sdots;
    if (k == "mrel") return &a.mrel;
    return nullptr;
}
}  // namespace
std::string get(const atom::AtomParams& a, const std::string& k) {
    const std::string* f = atomField(const_cast<atom::AtomParams&>(a), k);
    return f ? *f : std::string();
}
bool set(atom::AtomParams& a, const std::string& k, const std::string& x) {
    std::string* f = atomField(a, k);
    if (!f || x.empty()) return false;
    *f = x;
    return true;
}

// ---- Molecule Lab (the labels of ui/shell_molecule.cpp's controls) ----
std::string get(const MoleculeChoices& m, const std::string& k) {
    if (k == "style") return m.set.style == molecule::Style::Realistic ? "Density" : m.set.style == molecule::Style::SpaceFill ? "Space-filling" : "Ball-and-stick";
    if (k == "dots") return commas(m.set.realDots);
    if (k == "iso") return molecule::kRealIsoText[m.set.realIso < 0 || m.set.realIso > 3 ? 2 : m.set.realIso];
    if (k == "vibsize") return m.vibSize == 0 ? "Real size" : "3 times bigger";
    if (k == "anharm") return onOff(m.vibAnharm);
    if (k == "opara") return m.set.spinMix == 1 ? "In balance" : "Kept as when warm";
    return "";
}
bool set(MoleculeChoices& m, const std::string& k, const std::string& x) {
    if (k == "style") {
        if (x == "Density") { m.set.style = molecule::Style::Realistic; m.set.realUsed = true; }
        else if (x == "Space-filling") m.set.style = molecule::Style::SpaceFill;
        else if (x == "Ball-and-stick") m.set.style = molecule::Style::BallStick;
        else return false;
        return true;
    }
    if (k == "dots") { for (int n : molecule::kRealDots) if (x == commas(n)) { m.set.realDots = n; return true; } return false; }
    if (k == "iso") { for (int i = 0; i < 4; i++) if (x == molecule::kRealIsoText[i]) { m.set.realIso = i; return true; } return false; }
    if (k == "vibsize") { if (x == "Real size") m.vibSize = 0; else if (x == "3 times bigger") m.vibSize = 1; else return false; return true; }
    if (k == "anharm") return readOnOff(x, m.vibAnharm);
    if (k == "opara") { if (x == "Kept as when warm") m.set.spinMix = 0; else if (x == "In balance") m.set.spinMix = 1; else return false; return true; }
    return false;
}

// ---- Black Hole Lab (the labels of ui/shell_blackhole.cpp's controls) ----
std::string get(const blackhole::Settings& s, const std::string& k) {
    if (k == "bend") return s.bend ? "On" : "Off";
    if (k == "gasclump") return onOff(s.gasClump);
    if (k == "gaspattern") return onOff(s.gasPattern);
    if (k == "look") return s.look == blackhole::Look::Real ? "Real" : "Film";
    if (k == "sky") return s.skyOurs ? "Our sky" : "Black";
    if (k == "skybeside") return s.trueSky ? "True brightness" : "Shown";
    if (k == "light") return s.allLight ? "All light" : "What an eye sees";
    if (k == "glare") return s.glare ? "As an eye sees it" : "Off";
    return "";
}
bool set(blackhole::Settings& s, const std::string& k, const std::string& x) {
    if (k == "bend") return pick2(x, "On", "Off", s.bend);
    if (k == "gasclump") return readOnOff(x, s.gasClump);
    if (k == "gaspattern") return readOnOff(x, s.gasPattern);
    if (k == "look") { if (x == "Real") s.look = blackhole::Look::Real; else if (x == "Film") s.look = blackhole::Look::Film; else return false; return true; }
    if (k == "sky") return pick2(x, "Our sky", "Black", s.skyOurs);
    if (k == "skybeside") return pick2(x, "True brightness", "Shown", s.trueSky);
    if (k == "light") return pick2(x, "All light", "What an eye sees", s.allLight);
    if (k == "glare") return pick2(x, "As an eye sees it", "Off", s.glare);
    return false;
}

namespace {
template <class S> void applyLab(const char* lab, S& s) { for (const Row& r : table()) if (std::strcmp(r.lab, lab) == 0) set(s, r.p.key, r.p.value); }
}  // namespace
void apply(orbit::ViewSettings& v) { applyLab("orbit", v); }
void apply(atom::AtomParams& a) { applyLab("atom", a); }
void apply(MoleculeChoices& m) { applyLab("molecule", m); }
void apply(blackhole::Settings& s) { applyLab("blackhole", s); }

orbit::ViewSettings freshOrbit() { orbit::ViewSettings v; apply(v); return v; }
atom::AtomParams freshAtom() { atom::AtomParams a = atom::AtomParams::appDefaults(); apply(a); return a; }
MoleculeChoices freshMolecule() { MoleculeChoices m; apply(m); return m; }
blackhole::Settings freshBlackHole() { blackhole::Settings s = blackhole::settingsDefaults(); apply(s); return s; }

}  // namespace realistic
