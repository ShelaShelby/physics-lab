// 0.25: the Molecule Lab's "What am I even looking at?" guide (the roadmap's small item; the Orbit and Atom Labs have one). Plain words; the text follows the chosen style and
// molecule. Every fact in it is one the lab already shows with its source (molecule/molecule.h, molecule/density.h, molecule/vibration.h); it adds no new numbers.
#pragma once

#include <string>
#include <vector>

namespace molecule { struct Molecule; enum class Style; }

namespace ui {

struct AppState;

struct MolLearnSection { std::string key, title, html; };
// the guide's sections for this molecule and style (no graphics: tests/check_025.py reads them through the app)
std::vector<MolLearnSection> moleculeLookSections(const molecule::Molecule& m, molecule::Style style, bool densityVibrates, int vibModes);
void moleculeLearnWindow(AppState& app);   // the open guide (app.molLearn == 1), if any

}  // namespace ui
