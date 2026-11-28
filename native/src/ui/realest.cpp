#include "ui/realest.h"

#include "app/realistic.h"

#include <cstring>
#include <vector>

#include "atom/atom_tools.h"
#include "molecule/molecule.h"
#include "molecule/molecule_view.h"
#include "ui/orbit_panel.h"
#include "ui/shell_widgets.h"

namespace ui::real {

// 0.96.0: the list itself is app/realistic.h (below the UI), so each lab's fresh settings come from the same rows
const Pick* find(const char* lab, const std::string& key) {
    static thread_local Pick out;
    const realistic::Pick* p = realistic::find(lab, key);
    if (!p) return nullptr;
    out = Pick{p->key, p->value.c_str(), p->why};
    return &out;
}
void mark(const char* lab, const std::string& key, const std::string& value, int part) {
    const realistic::Pick* p = realistic::find(lab, key);
    if (p && value == p->value) sh::markReal(part, p->why);
}

void markLabels(const char* lab, const std::string& key, const std::initializer_list<const char*>& labels) {
    int i = 0;
    for (const char* l : labels) mark(lab, key, l, i++);
}
void markCheck(const char* lab, const std::string& key) { mark(lab, key, "on", 1); mark(lab, key, "off", 0); }

void applyOrbit(AppState& app) { realistic::apply(app.set); }
void applyAtom(AppState& app) { atom::applyRealistic(app.atom); }   // 0.86.0: the values live in atom/atom_tools.h (tested there: no made-up picture, R5); setParam remakes the cloud
void applyMolecule(AppState& app) {
    molecule::MoleculeLab& L = app.molecule;
    realistic::MoleculeChoices m{L.set, L.vibSize, L.vibAnharm};
    realistic::apply(m);
    L.setStyle(m.set.style);
    L.set.realDots = m.set.realDots;
    L.set.realIso = m.set.realIso;
    L.vibSize = m.vibSize;
    L.vibAnharm = m.vibAnharm;                 // 0.53.1 (shown in Ball-and-stick and Space-filling)
}
void applyBlackHole(AppState& app) { realistic::apply(app.blackhole); }
void applyLab(AppState& app) {
    if (app.lab == 0) applyOrbit(app);
    else if (app.lab == molecule::kLabIndex) applyMolecule(app);
    else if (app.lab == blackhole::kLabIndex) applyBlackHole(app);
    else applyAtom(app);
}
void applyAll(AppState& app) { applyOrbit(app); applyAtom(app); applyMolecule(app); applyBlackHole(app); }
const char* buttonCaption() { return "Sets every choice closest to reality at once. A dot marks that choice in each setting (a ring by a box: best left unticked); point at it to see why."; }

}  // namespace ui::real
