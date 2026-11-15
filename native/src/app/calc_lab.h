// The calculator's lab formulas (0.49): ready-made lines that put the selected thing's numbers into a textbook formula, which the calculator then works
// out like any typed line (so the formula is always shown and can be changed). Plain numbers in, lines out: no lab code here, tests/test_calc.cpp checks
// every line against an independent value. ui/calc_panel.cpp collects the numbers from the labs.
#pragma once

#include <string>
#include <vector>

namespace calc {

struct Recipe {
    std::string label;      // what it works out, in plain words
    std::string line;       // the calculator line, e.g. "(1.495978707e+11 m) / c to min"
    std::string note;       // what the formula leaves out ("" when nothing worth saying)
};

std::string num(double v);  // a number for a line: 10 significant digits, plain ASCII ("1.495978707e+11"); the calculator reads it back

// A body of the Solar System (Orbit Lab). r: distance from the Sun (m), v: speed round the Sun (m/s), radius (m, 0 = unknown), mass (kg, 0 = unknown),
// a: semi-major axis of its orbit round the Sun (m, 0 = not bound or unknown).
std::vector<Recipe> orbitRecipes(const std::string& name, double r, double v, double radius, double mass, double a);
// An atom with one electron (Atom Lab): Z protons, the nucleus's mass (kg, 0 = unknown), the electron's level n (1 or more);
// bindingMeV and A: the nucleus's total binding energy and mass number (bindingMeV < 0 = unknown).
std::vector<Recipe> atomRecipes(const std::string& name, int Z, double nucleusKg, int n, double bindingMeV, int A);
// A molecule (Molecule Lab): its mass in u, and the wavenumber (cm^-1) of the vibration on show (0 = none).
std::vector<Recipe> moleculeRecipes(const std::string& name, double massU, double wavenumber);
// Lines that need no selection.
std::vector<Recipe> generalRecipes();
// 0.50.1: a few quantum-mechanics lines (the Rabi frequency with its ω note, a spectroscopy conversion, a Clebsch-Gordan coefficient).
std::vector<Recipe> qmRecipes();

}  // namespace calc
