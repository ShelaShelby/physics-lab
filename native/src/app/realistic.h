// 0.96.0: the "Most realistic" list (the owner's decision of 5 Nov 2026, ui/realest.h since 0.25) moved here, below the UI, so the labs' first-start
// settings come from it. The owner's decision of 28 Nov 2026 (DECISIONS.md): every lab opens in its Most realistic settings, at a first start, after
// Defaults, and for a key a saved file does not have. One list: the mark on a choice (ui::real::mark), the "Most realistic" button and the fresh
// settings all read it, so they cannot drift apart (tests/test_app.cpp testRealisticDefaults checks each key of each lab).
// The bare structs (orbit::ViewSettings{}, atom::AtomParams::appDefaults(), molecule::MoleculeLab(), blackhole::settingsDefaults()) keep their own
// values: the benchmark scenes and the tests start from them and set what they need.
#pragma once

#include <string>
#include <vector>

#include "atom/atom_sim.h"
#include "blackhole/lab.h"
#include "molecule/molecule.h"
#include "orbit/orbit_view.h"

namespace realistic {

struct Pick { const char* key; std::string value; std::string why; };
struct Row { const char* lab; Pick p; };
const std::vector<Row>& table();                                   // every lab's picks, in the order the button sets them
const Pick* find(const char* lab, const std::string& key);         // lab "orbit" / "atom" / "molecule" / "blackhole"; nullptr: no realistic choice

// One lab's settings as the list reads them: get returns the value shown for that key ("" for a key the lab does not have); set takes one of the
// control's values and returns false for a value it does not know (nothing is changed then).
struct MoleculeChoices { molecule::Settings set; int vibSize = 1; bool vibAnharm = false; };   // (vibSize and vibAnharm live on molecule::MoleculeLab, not saved)
std::string get(const orbit::ViewSettings& v, const std::string& key);
std::string get(const atom::AtomParams& a, const std::string& key);
std::string get(const MoleculeChoices& m, const std::string& key);
std::string get(const blackhole::Settings& s, const std::string& key);
bool set(orbit::ViewSettings& v, const std::string& key, const std::string& value);
bool set(atom::AtomParams& a, const std::string& key, const std::string& value);
bool set(MoleculeChoices& m, const std::string& key, const std::string& value);
bool set(blackhole::Settings& s, const std::string& key, const std::string& value);

// every pick of one lab set on the given settings (only the keys in the list change)
void apply(orbit::ViewSettings& v);
void apply(atom::AtomParams& a);
void apply(MoleculeChoices& m);
void apply(blackhole::Settings& s);

// what each lab opens with: the bare defaults with every pick set
orbit::ViewSettings freshOrbit();
atom::AtomParams freshAtom();          // AtomParams::appDefaults() with the picks (the view: Cloud 3D)
MoleculeChoices freshMolecule();
blackhole::Settings freshBlackHole();  // blackhole::settingsDefaults() with the picks

constexpr const char* kSwitchVersion = "0.96.0";   // the one-time switch of every lab to its picks (app::Prefs::realest holds it once done)

}  // namespace realistic
