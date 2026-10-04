// Every nucleus at its measured size (0.10.9 stage 1): measured rms charge radii from the IAEA Nuclear Data Section's table of nuclear charge radii, baked in at build time
// (src/atom/radii_data.inc, made by tools/make_radii_data.py from data/radii/charge_radii.csv; the README there has the URL, the date and the credit). No drawing here, so it can be tested.
//
// What the number is: the rms charge radius sqrt(<r^2>) of the PROTONS' electric charge (neutrons carry no charge, so it says nothing direct about where the neutrons are).
// How it becomes a drawn size (an ASSUMPTION, stated in the app): the lab's usual nucleus size is a uniform ball, r = 1.2 A^(1/3) fm. A uniform ball of radius R has rms radius sqrt(3/5) R
// (<r^2> = integral of r^2 over the ball / volume = 3/5 R^2, a calculus result), so a measured rms radius is drawn as the uniform ball of radius R = sqrt(5/3) x rms. Without this step the measurement
// and the rule would be compared in different quantities (the rule's R is an edge, the measurement an rms): the rms of the rule's ball is sqrt(3/5) x 1.2 A^(1/3) = 0.9295 A^(1/3) fm.
// Where there is no value the rule stays, and the card says so. Nothing is interpolated. The PRELIMINARY columns of the file are NOT used (the file marks them preliminary; 48 isotopes have only such a value:
// they keep the rule and the card names the preliminary value). The neutron row (a negative number) is not in the table. Hydrogen-1 (a single proton) keeps the lab's own proton radius (CODATA 2022).
#pragma once

#include <string>
#include <vector>

namespace atom {

struct RadiusRow { short Z, N; double rms, unc, prelim, prelimUnc; };   // fm; NaN where the file leaves the cell empty
const RadiusRow* radiusRows(int& count);                      // the whole table, sorted by Z then N
const RadiusRow* chargeRadius(int Z, int N);                  // the isotope's row, or nullptr when the file has none
bool sizedByMeasurement(int Z, int N);                        // the lab draws it at a measured size: a (non-preliminary) value exists and A > 1

constexpr double kSharpOverRms = 1.2909944487358056;          // sqrt(5/3): the radius of the uniform ball whose rms radius is 1
double ruleRms(int A);                                        // the usual rule's ball (1.2 A^(1/3) fm) expressed as an rms charge radius: sqrt(3/5) x nuclearR(A), fm
double nuclearRadius(int Z, int N);                           // fm: the drawn (uniform-ball) radius: sqrt(5/3) x the measured rms where sizedByMeasurement, else the usual rule nuclearR(A)

// The Woods-Saxon half-density radius (diffuseness a = 0.54 fm, the lab's) whose PROTON density has the charge radius rms once the proton's own size (0.84 fm) is added: <r^2> of the density = rms^2 - 0.84^2.
// Solved numerically (bisection). It only STARTS the sampling of a nucleus; buildNucleus then scales the snapshot so its protons have the measured rms exactly (atom_core.cpp).
double wsHalfDensityRadiusForChargeRms(double rms);

std::vector<std::string> nuclearSizeLines(int Z, int N);      // the words of the "Nuclear size (measured)" card for this isotope
std::string chargeRadiusExplain();                            // what a charge radius is, and the limits of this picture
inline constexpr const char* kRadiiCredit = "Nuclear charge radii: the IAEA Nuclear Data Section's table of nuclear charge radii (www-nds.iaea.org/radii/charge_radii.csv, retrieved 2026-10-04), the compilation of I. Angeli and K. P. Marinova, "
                                            "Atomic Data and Nuclear Data Tables 99 (2013) 69, \"Table of experimental nuclear ground state charge radii: An update\" (the file itself carries no citation text and does not say which edition it is).";

}  // namespace atom
